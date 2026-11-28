/* SPDX-License-Identifier: MIT */
enum {
    RASTER_MODE=0xb1809694u, RASTER_LOCK=0xb18096acu, RASTER_GATE=0xb18096b0u,
    RASTER_QUEUE=0xb1d533d8u,
    RASTER_SUCCESS=0x42190c41u, RASTER_FAILURE=0x42190c48u
};

/* 0x1038A354 builds a byte-copy command from these pitches and addresses.
 * The task queue retains the message and its descriptor until the callback. */
static struct {
    unsigned short format,src_pitch,dst_pitch,width,rows,padding;
    u32 source,dest,options[5];
} raster;
_Static_assert(sizeof(raster)==40,"Native raster descriptor");
static u32 raster_event;

static void raster_complete(u32 code,u32 context)
{
    (void)context;
    /* The terminal callback at 0x10389D9E follows device cleanup.
     * Control notifications from 0x1038A4E0 are not transfer completions. */
    if (code==RASTER_SUCCESS || code==RASTER_FAILURE)
        CALL(0x103610cdu,void,u32 *,u32)(&raster_event,code==RASTER_SUCCESS ? 1u : 2u);
}

static struct {
    u32 queue_links[3];
    void (*callback)(u32,u32);
    u32 opcode;
    void *descriptor;
    u32 context;
} raster_message={{0},raster_complete,0x42190c01u,&raster,0};

static int raster_ready(void)
{
    if ((int)WORD(RASTER_QUEUE+0x20u)<=0 || !WORD(RASTER_MODE+0x0cu) ||
        (int)WORD(RASTER_LOCK)<=0) return 0;
    if ((int)raster_event<=0) CALL(0x10361083u,void,u32 *)(&raster_event);
    return (int)raster_event>0;
}

static u32 raster_wait(u32 timeout)
{
    u32 events=0;
    if (!CALL(0x10361117u,int,u32 *,u32,u32 *,u32)(&raster_event,3u,&events,timeout))
        return 0;
    return events&3u;
}

#ifndef Z72_RAW_DUAL_RECT
enum { RASTER_PENDING=1u<<RAW_DMA_CHANNELS };
static u32 raster_submitted;

static u32 raster_reap(u32 timeout)
{
    const u32 events=raster_wait(timeout);
    if (!events) return RAW_COPY_TIMEOUT;
    log.raster_result=(events&2u) ? RASTER_FAILURE : RASTER_SUCCESS;
    log.raster_observed_ms=tick()-raster_submitted;
    publish(&dma_pending_channel,0);
    return (events&2u) ? RAW_DMA_ERROR : RAW_COPY_OK;
}

static u32 raster_copy_rows(u32 dest,u32 source,u32 stride,u32 rows,u32 start,int sliced)
{
    if (!raster_ready()) return RAW_DMA_BUSY;
    /* The original sliced path gives priority clients a scheduling point. */
    if (sliced) {
        if ((int)WORD(RASTER_GATE)<=0 || !CALL(0x10360f2fu,int,u32,u32)(RASTER_GATE,1u))
            return RAW_DMA_BUSY;
        CALL(0x10360f13u,void,u32)(RASTER_GATE);
    }
    if (!CALL(0x10360f2fu,int,u32,u32)(RASTER_LOCK,1u)) return RAW_DMA_BUSY;
    u32 result=RAW_COPY_TIMEOUT;
#ifdef Z72_RAW_NEF_QUIET_STOP
    if (z72_mode==&z72_uhd && nef_capture_stopping()) result=NEF_STOPPING;
    else
#endif
    if (tick()-start<RAW_COPY_MS) {
        raster.format=1;
        raster.src_pitch=raster.dst_pitch=raster.width=(unsigned short)stride;
        raster.rows=(unsigned short)rows;
        raster.source=source;
        raster.dest=dest;
        raster_submitted=tick();
        ++log.raster_requests;
        log.raster_rows=rows;
        log.raster_result=log.raster_observed_ms=0;
        CALL(0x103610d3u,void,u32 *,u32)(&raster_event,3u);
        publish(&dma_pending_channel,RASTER_PENDING);
        CALL(0x10389f7du,void,void *)(&raster_message);
        u32 elapsed=tick()-start;
        result=raster_reap(elapsed<RAW_COPY_MS ? RAW_COPY_MS-elapsed : 0u);
    }
    /* Other native requests use their own descriptors. Our callback never
     * signals the native shared completion event after releasing this lock. */
    CALL(0x10360f13u,void,u32)(RASTER_LOCK);
    return result;
}
#endif
