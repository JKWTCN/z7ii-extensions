/* SPDX-License-Identifier: MIT */
/* C 1.70 input-tail FIFO and recorder notification shared by graphs 9 and 10. */
enum {
    SYNC_INPUT_SLOT=0xb1888168u, SYNC_INPUT_NATIVE=0x1022e2e1u,
    SYNC_GRAPH_SLOT=0xb186488cu, SYNC_GRAPH_NATIVE=0x103cdb15u,
    SYNC_VIDEO_QUEUE=0xb1f58e04u, SYNC_VIDEO_SUBMITTED=0xb1f58f3cu,
    SYNC_VIDEO_MUTEX=0xb1811778u, SYNC_PENDING=0xb1888164u,
    SYNC_SLOTS=256u, SYNC_MAGIC=0x31534d5au
};
static struct {
    u32 enabled, irq, first_ordinal, session;
    u32 tails[SYNC_SLOTS];
    unsigned long long input[SYNC_SLOTS], movie[SYNC_SLOTS];
    u32 video_cursor;
} mov_sync;

static int sync_ram(u32 address,u32 bytes)
{
    return !(address&3u) && address>=0xb1800000u && address<=0xb8000000u-bytes;
}

static int sync_same_source(u32 task,u32 ordinal)
{
    const u32 token=load(&mov_sync.tails[ordinal%SYNC_SLOTS]);
    const u32 slot=token&RAW_TOKEN_SLOT,desc=WORD(task+0x68u);
    return (token>>RAW_TOKEN_SHIFT)==ordinal && slot && slot<=RAW_SLOT_COUNT &&
           sync_ram(desc,0x38u) && WORD(desc+0x34u)==z72_raw_slot_address(slot-1u);
}

static void sync_associate(u32 task,u32 newest,u32 pending)
{
    const u32 newest_ordinal=newest>>RAW_TOKEN_SHIFT;
    if (!pending || pending>RAW_SLOT_COUNT || pending>newest_ordinal || !sync_ram(task,0x6cu) ||
        BYTE(task+0xdu)!=z72_mode->type || WORD(task+0x10u) || !WORD(task+0x30u) || !WORD(task+0x34u)) return;
    const u32 ordinal=newest_ordinal+1u-pending;
    const u32 seq=WORD(task);
    if (!sync_same_source(task,ordinal) ||
        load(&mov_sync.irq) || newest!=load(&tail_token) || pending!=WORD(SYNC_PENDING)) return;
    __atomic_store_n(&mov_sync.input[seq%SYNC_SLOTS],((unsigned long long)seq<<32)|ordinal,__ATOMIC_RELEASE);
}

static u32 sync_input(u32 event,u32 task)
{
    const u32 caller=(u32)__builtin_return_address(0)&~1u;
    if (event==0x041a0002u && caller==0x10238d16u && load(&mov_sync.enabled) &&
        load(&state)==ARMED && !nef_capture_stopping() && !load(&mov_sync.irq)) {
        const u32 token=load(&tail_token);
        sync_associate(task,token,WORD(SYNC_PENDING));
    }
    return CALL(SYNC_INPUT_NATIVE,u32,u32,u32)(event,task);
}

static u32 sync_video_position(u32 task)
{
    /* 10550C0C copies the YUV descriptor, then 10550C10 publishes the
     * new queue cursor. The graph notice also fires if enqueue fails. */
    if (!CALL(0x10360f2fu,int,u32,u32)(SYNC_VIDEO_MUTEX,0u)) {
        mov_sync.video_cursor=WORD(SYNC_VIDEO_QUEUE+0x1cu);
        return 0;
    }
    const u32 queue=SYNC_VIDEO_QUEUE, cursor=WORD(queue+0x1cu);
    const u32 previous=mov_sync.video_cursor;
    mov_sync.video_cursor=cursor;
    const u32 count=WORD(queue+4u),entries=WORD(queue+0x10u),desc=WORD(task+0xa8u);
    u32 position=0;
    if (cursor!=previous && cursor<count && count<=60u && WORD(queue+8u)==32u &&
        sync_ram(entries,count*8u) && sync_ram(desc,0x34u)) {
        const u32 committed=WORD(entries+8u*cursor+4u);
        if (sync_ram(committed,32u) && WORD(desc+0x2cu) && WORD(desc+0x30u) &&
            WORD(committed+0x14u)==WORD(desc+0x2cu) && WORD(committed+0x18u)==WORD(desc+0x30u)) {
            /* The recorder holds this same mutex across dequeue and the
             * successful H.264 submit count update at 10485972. */
            const u32 queued=CALL(0x10550d43u,u32,u32,u32)(queue,0u);
            if (queued<=count) position=WORD(SYNC_VIDEO_SUBMITTED)+queued;
        }
    }
    CALL(0x10360f13u,void,u32)(SYNC_VIDEO_MUTEX);
    return position;
}

static u32 sync_graph(u32 event,u32 task)
{
    const u32 caller=(u32)__builtin_return_address(0)&~1u;
    if (event==0x041b0080u && caller==0x10269f6eu && load(&mov_sync.enabled) &&
        sync_ram(task,0xacu) && BYTE(task+0xdu)==z72_mode->type && WORD(task+0x30u)) {
        const u32 seq=WORD(task),frame=sync_video_position(task);
        const unsigned long long match=__atomic_load_n(&mov_sync.input[seq%SYNC_SLOTS],__ATOMIC_ACQUIRE);
        const u32 ordinal=(u32)match;
        if (ordinal && (u32)(match>>32)==seq && frame && sync_same_source(task,ordinal))
            __atomic_store_n(&mov_sync.movie[ordinal%SYNC_SLOTS],
                ((unsigned long long)ordinal<<32)|frame,__ATOMIC_RELEASE);
    }
    return CALL(SYNC_GRAPH_NATIVE,u32,u32,u32)(event,task);
}

static void sync_uninstall(void)
{
    publish(&mov_sync.enabled,0);
    u32 own=(u32)sync_input;
    __atomic_compare_exchange_n(&WORD(SYNC_INPUT_SLOT),&own,SYNC_INPUT_NATIVE,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    own=(u32)sync_graph;
    __atomic_compare_exchange_n(&WORD(SYNC_GRAPH_SLOT),&own,SYNC_GRAPH_NATIVE,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
}

static void sync_install(int paired)
{
    if (!paired) return;
#ifndef Z72_RAW_MOV_SYNC_HD
    if (z72_mode!=&z72_uhd) return;
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
    if (load(&raw_mov_sync_choice)==RAW_MOV_SYNC_OFF) return;
#endif
    CALL(0x10361035u,void,void *,u32)(&mov_sync,sizeof(mov_sync));
    u32 input=SYNC_INPUT_NATIVE,graph=SYNC_GRAPH_NATIVE;
    if (!__atomic_compare_exchange_n(&WORD(SYNC_INPUT_SLOT),&input,(u32)sync_input,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) return;
    if (!__atomic_compare_exchange_n(&WORD(SYNC_GRAPH_SLOT),&graph,(u32)sync_graph,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) {
        sync_uninstall();
        return;
    }
    mov_sync.session=tick()|1u;
    mov_sync.video_cursor=WORD(SYNC_VIDEO_QUEUE+0x1cu);
    publish(&mov_sync.enabled,1);
}

static u32 sync_movie_frame(u32 ordinal)
{
    const unsigned long long row=__atomic_load_n(&mov_sync.movie[ordinal%SYNC_SLOTS],__ATOMIC_ACQUIRE);
    return (u32)(row>>32)==ordinal ? (u32)row : 0;
}
