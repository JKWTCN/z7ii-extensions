/* SPDX-License-Identifier: MIT */
#ifdef Z72_RAW_NEF_SLOT_ENCODE
enum { NEF_SLOT_ENCODE=1, NEF_SLOT_GUARD=256 };
#else
enum { NEF_SLOT_ENCODE=0, NEF_SLOT_GUARD=0 };
#endif
#ifdef Z72_RAW_NEF_BURST_LIMIT
enum { NEF_FRAME_STEP=1 };
#else
enum { NEF_FRAME_STEP=2 };
#endif
#ifndef Z72_RAW_NEF_SLOT_BYTES
#define Z72_RAW_NEF_SLOT_BYTES (7u<<20)
#endif
enum {
    NEF_LOCK=0xb1809648u, NEF_QUEUE=0xb1d52d34u, NEF_DONE=0x42192c02u,
    NEF_CAPACITY=Z72_RAW_NEF_SLOT_BYTES, NEF_PIXELS=3826u*2158u,
    NEF_MIN_WRITE=(NEF_PIXELS*2u+7u)/8u, NEF_MAX_WRITE=(NEF_PIXELS*19u+7u)/8u,
    NEF_SCRATCH=(NEF_MAX_WRITE+127u)&~63u, NEF_UNAVAILABLE=11, NEF_OVERSIZE=12,
    NEF_OUTPUT_BUSY=13
};
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
enum { NEF_WAIT_MS=104, NEF_OUTPUT_WAIT_MS=104 };
#elif defined(Z72_RAW_NEF_LATE_COMPLETION)
enum { NEF_WAIT_MS=72 };
#else
enum { NEF_WAIT_MS=RAW_COPY_MS };
#endif
/* Only the page offset is used for buffer placement. The file writer tracks
 * the full 64-bit position for closing clips across the 4 GiB boundary. */
static u32 nef_offset,nef_complete,nef_complete_tick,nef_pending;
#ifdef Z72_RAW_PREFETCH_NEF
/* Published only for a direct input. The worker rechecks the ordinal and
 * pending/completion state after reading the capture tick. */
static u32 nef_prefetch_ordinal,nef_prefetch_tick;
#endif
#ifdef Z72_RAW_NEF_EVENT_WAIT
static u32 nef_event;

static int nef_wait_event(u32 timeout)
{
    u32 events=0;
    return CALL(0x10361117u,int,u32 *,u32,u32 *,u32)(&nef_event,1u,&events,timeout) && (events&1u);
}
#endif
static void *nef_owner;
static unsigned char *nef_scratch;
#ifdef Z72_RAW_NEF_ADAPTIVE_SLOTS
static u32 nef_slot_payload_limit=NEF_CAPACITY,nef_slot_pair_bytes;
#else
#define nef_slot_payload_limit NEF_CAPACITY
#endif
#ifdef Z72_RAW_NEF_DIRECT_OVERSIZE
/* The writer publishes consumed only after the synchronous file write. */
static u32 nef_scratch_until;

static unsigned char *nef_scratch_packet(u32 offset)
{
    return nef_scratch+(offset&(WRITE_ALIGNMENT-1u));
}

static int nef_write_from_scratch(u32 index,u32 bytes)
{
#ifdef Z72_RAW_NEF_BURST_LIMIT
    if (index+1u==Z72_RAW_NEF_BURST_LIMIT) return 1;
#else
    (void)index;
#endif
    return bytes>nef_slot_payload_limit;
}
#endif
static u32 nef_desc[27]={
    [0]=0x20001,[1]=3826,[2]=2158,[4]=0x11,[5]=0x200,
    [6]=0x119b59ac,[7]=0x119b5a9c,[8]=5744u*2158u,
    [13]=10,[14]=0xab,[17]=5744,[19]=5744,[20]=0x05040017
};

#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
static struct packet nef_source_header;
static struct {
    u32 token,result;
    struct z72_raw_status status;
#ifdef Z72_RAW_PREFETCH_NEF
    u32 private_input;
#endif
#ifdef Z72_RAW_PCM_2997_HANDOFF
    u32 late;
#endif
} nef_source_job;
static u32 nef_validate_completion(const struct packet *packet,u32 token,struct z72_raw_status *status);
#endif
#ifdef Z72_RAW_REC_SPLIT_SNAPSHOT
static u32 nef_submit_tick;
#endif

#ifdef Z72_RAW_NEF_QUIET_STOP
static int nef_input_stopping(int private_input)
{
#ifdef Z72_RAW_PREFETCH_NEF
    if (private_input) return 0; /* The verified REC-owned copy survives a stop. */
#else
    (void)private_input;
#endif
    return nef_capture_stopping();
}
#endif

static void nef_callback(u32 code,u32 context)
{
    (void)context;
    if (code==NEF_DONE || code==0x42192c12u) {
#ifdef Z72_RAW_NEF_EVENT_WAIT
        const int notify=(int)nef_event>0;
#endif
        nef_complete_tick=tick();
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
#ifdef Z72_RAW_PREFETCH_NEF
        const int private_input=load(&nef_source_job.private_input);
#else
        const int private_input=STAGED_NEF_INPUT;
#endif
#ifdef Z72_RAW_NEF_QUIET_STOP
        if (!STAGED_NEF_LIVE && nef_input_stopping(private_input)) nef_source_job.result=NEF_STOPPING;
        else
#endif
        nef_source_job.result=code==NEF_DONE ?
            (private_input ? RAW_COPY_OK :
             nef_validate_completion(&nef_source_header,nef_source_job.token,&nef_source_job.status)) : RAW_DMA_ERROR;
        log.nef_terminal.elapsed_ms=nef_complete_tick-nef_source_header.start_tick;
#ifdef Z72_RAW_REC_SPLIT_SNAPSHOT
        log.recording.submitted_ms=load(&nef_submit_tick);
        log.recording.callback_ms=nef_complete_tick;
#endif
        log.nef_terminal.code=code;
        log.nef_terminal.token=nef_source_job.status.observed_token;
        log.nef_terminal.slot=nef_source_job.status.live_index;
        log.nef_terminal.validation=nef_source_job.result;
        log.nef_terminal.ordinal=(nef_source_job.token>>RAW_TOKEN_SHIFT)-1u;
#endif
        publish(&nef_complete,code);
#ifdef Z72_RAW_NEF_EVENT_WAIT
        if (notify) CALL(0x103610cdu,void,u32 *,u32)(&nef_event,1u);
#endif
    }
}

static struct {
    u32 links[3];
    void (*callback)(u32,u32);
    u32 opcode,*desc,context;
} nef_request={{0},nef_callback,0x42192c42u,nef_desc,0};

static int nef_allocate(void)
{
#ifdef Z72_RAW_NEF_DIRECT_OVERSIZE
    const u32 alignment=WRITE_ALIGNMENT,span=NEF_SCRATCH+WRITE_ALIGNMENT;
#else
    const u32 alignment=64u,span=NEF_SCRATCH;
#endif
    const u32 request=span+alignment-1u;
    log.requested=request;
    const u32 heap=WORD(0xb1f7cbc0u);
    u32 info[4];
    if ((int)heap<=0 || CALL(0x10104788u,int,u32,u32 *)(heap,info)) return 0;
#ifdef Z72_RAW_NEF_ADAPTIVE_SLOTS
    log.nef_buffers.scratch_largest_block=info[3];
#endif
    if (info[1] || info[3]<request) return 0;
    nef_owner=CALL(0x1034fe31u,void *,u32,u32)(request,1);
    if (!nef_owner) return 0;
    nef_scratch=(void *)(((u32)nef_owner+alignment-1u)&~(alignment-1u));
    if (!z72_raw_identity_span((u32)nef_scratch,span)) return 0;
    invalidate_dma_pixels((u32)nef_scratch,span);
    return 1;
}

#ifdef Z72_RAW_NEF_ADAPTIVE_SLOTS
static void nef_allocate_slot_pair(void)
{
    enum { RESERVE=0x80000u, SLOT_OVERHEAD=64u+2u*WRITE_ALIGNMENT, MIN_PAYLOAD=6u<<20 };
    const u32 heap=WORD(0xb1f7cbacu);
    u32 info[4];
    nef_slot_payload_limit=NEF_CAPACITY;
    nef_slot_pair_bytes=0;
    if ((int)heap<=0 || CALL(0x10104788u,int,u32,u32 *)(heap,info)) return;
    log.free_bytes[0]=info[2];
    log.largest_block[0]=info[3];
    if (info[1] || info[2]<=RESERVE) return;

    const u32 available=info[2]-RESERVE<info[3] ? info[2]-RESERVE : info[3];
    if (available<=WRITE_ALIGNMENT-1u) return;
    u32 stride=((available-(WRITE_ALIGNMENT-1u))/2u)&~(WRITE_ALIGNMENT-1u);
    if (stride<MIN_PAYLOAD+SLOT_OVERHEAD) return;
    const u32 max_stride=(NEF_CAPACITY+SLOT_OVERHEAD+WRITE_ALIGNMENT-1u)&~(WRITE_ALIGNMENT-1u);
    if (stride>max_stride) stride=max_stride;
    const u32 request=2u*stride+WRITE_ALIGNMENT-1u;
    void *owner=CALL(0x1034fe31u,void *,u32,u32)(request,0u);
    if (!owner) return;
    const u32 address=((u32)owner+WRITE_ALIGNMENT-1u)&~(WRITE_ALIGNMENT-1u);
    if (!z72_raw_identity_span(address,2u*stride)) {
        CALL(0x1034fe7fu,void,void *)(owner);
        return;
    }
    nef_slot_payload_limit=stride-SLOT_OVERHEAD;
    if (nef_slot_payload_limit>NEF_CAPACITY) nef_slot_payload_limit=NEF_CAPACITY;
    nef_slot_pair_bytes=request;
    /* Both slots are views into one DMA allocation. Slot 0 owns it. */
    frame_owners[0]=owner;
    frame_owners[1]=0;
    frame_slots[0]=(struct packet *)address;
    frame_slots[1]=(struct packet *)(address+stride);
    invalidate_dma_pixels(address,2u*stride);
    frame_capacity=2u;
}
#endif

#ifdef Z72_RAW_NEF_DMA_STAGE
static u32 nef_stage_dma(u32 dest,u32 source,u32 bytes,u32 start)
{
    const int sliced=BYTE(RASTER_MODE)==1;
    const u32 pitch=WRITE_ALIGNMENT, max_rows=sliced ? 0x40000u/pitch : bytes/pitch;
    u32 offset=0;
    while (bytes-offset>=pitch) {
        u32 rows=(bytes-offset)/pitch;
        if (rows>max_rows) rows=max_rows;
        u32 result=raster_copy_rows(dest+offset,source+offset,pitch,rows,start,sliced);
        if (result!=RAW_COPY_OK) return result;
        offset+=rows*pitch;
    }
    return bytes==offset ? RAW_COPY_OK :
        raster_copy_rows(dest+offset,source+offset,bytes-offset,1u,start,sliced);
}
#endif

#ifdef Z72_RAW_BANK_HANDOFF
/* The IRQ has reported the tail of the source, and the DMA cursor can
 * enter the next bank before its tail IRQ. The expected next token is a
 * private frame key. Only raw_irq publishes actual tokens to tail_token. */
static u32 early_anchor;
#ifdef Z72_RAW_TERMINAL_ROW
enum { NEF_WAIT_FOR_BANK=0x100u };
static u32 terminal_row_anchor(u32 source)
{
    const u32 bank=source&RAW_TOKEN_SLOT;
    if (!(source&RAW_TOKEN_PAIR) || !bank || bank>RAW_SLOT_COUNT ||
        z72_raw_completed_slot()!=bank || load(&tail_token)!=source) return 0;
    return z72_raw_next_tail(source,bank%RAW_SLOT_COUNT+1u);
}

static int at_terminal_row(u32 result,u32 token,const struct z72_raw_status *s)
{
    return result==RAW_WINDOW_EXPIRED && load(&early_anchor)==token &&
           (s->observed_token>>RAW_TOKEN_SHIFT)+1u==(token>>RAW_TOKEN_SHIFT) &&
           s->live_index+1u==(s->observed_token&RAW_TOKEN_SLOT) &&
           s->live_address==z72_raw_slot_address(s->live_index) &&
           s->live_tail==s->live_address+z72_uhd.tail_offset &&
           terminal_row_anchor(s->observed_token)==token;
}
#endif
static u32 next_bank_anchor(u32 source)
{
    const u32 bank=source&RAW_TOKEN_SLOT;
    if (!(source&RAW_TOKEN_PAIR) || !bank || bank>RAW_SLOT_COUNT) return 0;
    const u32 next=bank%RAW_SLOT_COUNT,base=z72_raw_slot_address(next);
    if (WORD(PREVIEW+0x74u)!=next || WORD(PREVIEW+0x78u)!=base) return 0;
    const u32 cursor=WORD(PREVIEW+0x7cu);
    if (cursor<base+z72_uhd.stride || cursor>=base+clip_pixel_bytes() ||
        WORD(PREVIEW+0x74u)!=next || WORD(PREVIEW+0x78u)!=base ||
        load(&tail_token)!=source) return 0;
    return z72_raw_next_tail(source,next+1u);
}
#endif

#ifdef Z72_RAW_NEF_TOKEN_RECHECK
static u32 nef_source_status(u32 token,u32 start,struct z72_raw_status *status,u32 max_ms)
{
    const u32 retries=status->snapshot_retries;
    u32 result=z72_raw_copy_status_limit(&tail_token,token,start,status,max_ms);
#ifdef Z72_RAW_BANK_HANDOFF
    if (result==RAW_WINDOW_EXPIRED && load(&early_anchor)==token &&
        (status->observed_token>>RAW_TOKEN_SHIFT)+1u==(token>>RAW_TOKEN_SHIFT)) {
        const u32 next=(status->observed_token&RAW_TOKEN_SLOT)%RAW_SLOT_COUNT;
        if (status->live_index==next && status->live_address==z72_raw_slot_address(next) &&
            next_bank_anchor(status->observed_token)==token) result=RAW_COPY_OK;
        else if (load(&tail_token)!=status->observed_token)
            result=z72_raw_copy_status_limit(&tail_token,token,start,status,max_ms);
    }
#endif
    if (status->snapshot_retries!=retries) {
        ++timing_header[13];
        if (result==RAW_COPY_OK) ++timing_header[14];
    }
    return result;
}
#else
#define nef_source_status(token,start,status,max_ms) z72_raw_copy_status_limit(&tail_token,token,start,status,max_ms)
#endif

#ifdef Z72_RAW_NEF_SLOT_VALIDATE
enum { NEF_SAMPLE_LINES=3, NEF_SAMPLE_BYTES=64 };
static unsigned char nef_sample[NEF_SAMPLE_LINES][NEF_SAMPLE_BYTES];

static int nef_check_samples(u32 source,int verify)
{
#ifdef Z72_RAW_TERMINAL_ROW
    /* The reported tail points 32 bytes before the end of the raster.
     * Retain the complete last scanline until the next bank is witnessed. */
    static _Alignas(64) u32 last_row[5744u/sizeof(u32)];
    if (load(&early_anchor)) {
        const u32 address=source+(2158u-1u)*sizeof(last_row);
        CALL(0x1034fa29u,void,void *,u32)((void *)address,sizeof(last_row));
        if (verify) {
            for (u32 j=0;j<sizeof(last_row)/sizeof(u32);++j)
                if (WORD(address+4u*j)!=last_row[j]) return 0;
        } else CALL(0x10360facu,void,void *,const void *,u32)(last_row,(void *)address,sizeof(last_row));
    }
#endif
    static const u32 rows[NEF_SAMPLE_LINES]={0,1079,2157};
    for (u32 i=0;i<NEF_SAMPLE_LINES;++i) {
        const u32 address=source+rows[i]*5744u+5744u-128u;
        CALL(0x1034fa29u,void,void *,u32)((void *)address,NEF_SAMPLE_BYTES);
        for (u32 j=0;j<NEF_SAMPLE_BYTES;++j) {
            const unsigned char pixel=BYTE(address+j);
            if (verify) {
                if (pixel!=nef_sample[i][j]) return 0;
            } else nef_sample[i][j]=pixel;
        }
    }
    return 1;
}

static int nef_before_wrap(u32 token,const struct z72_raw_status *status)
{
    const u32 anchor=(token&RAW_TOKEN_SLOT)-1u;
    const u32 slot=(anchor+2u)%RAW_SLOT_COUNT;
    const u32 address=z72_raw_slot_address(slot);
    const u32 observed=status->observed_token;
    int valid_tail=(observed>>RAW_TOKEN_SHIFT)==(token>>RAW_TOKEN_SHIFT)+1u;
#ifdef Z72_RAW_PCM_2997_HANDOFF
    /* At 29.97p the second tail can arrive while DMA is still in the
     * preceding bank. Check both transitions before accepting that bank. */
    if (z72_clip_fps_num==30000u && (observed>>RAW_TOKEN_SHIFT)-(token>>RAW_TOKEN_SHIFT)==2u) {
        const u32 prior=load(&nef_previous_tail);
        valid_tail=(token&RAW_TOKEN_PAIR) &&
                   prior==z72_raw_next_tail(token,(anchor+1u)%RAW_SLOT_COUNT+1u) &&
                   observed==z72_raw_next_tail(prior,slot+1u);
    }
#endif
    return valid_tail && status->live_index==slot && status->live_address==address &&
           status->live_tail>=address && status->live_tail<=address+clip_pixel_bytes() &&
           WORD(PREVIEW+0x74u)==slot && WORD(PREVIEW+0x78u)==address &&
           load(&tail_token)==status->observed_token;
}

#ifdef Z72_RAW_PCM_2997_HANDOFF
static int nef_late_bank(u32 result,u32 token,const struct z72_raw_status *status)
{
    return (result==RAW_UNSAFE_SLOT ||
            (result==RAW_WINDOW_EXPIRED && z72_clip_fps_num==30000u &&
             (status->observed_token>>RAW_TOKEN_SHIFT)-(token>>RAW_TOKEN_SHIFT)==2u)) &&
           nef_before_wrap(token,status);
}

static u32 nef_prepare_status(u32 token,u32 start,struct z72_raw_status *status)
{
    const u32 result=nef_source_status(token,start,status,RAW_COPY_MS);
#ifdef Z72_RAW_TERMINAL_ROW
    if (at_terminal_row(result,token,status)) return RAW_COPY_OK;
#endif
    if (z72_clip_fps_num==30000u && result==RAW_UNSAFE_SLOT && nef_before_wrap(token,status))
        return RAW_COPY_OK;
    if (result!=RAW_COPY_OK) log.handoff.prepare_cursor=status->live_tail;
    return result;
}
#else
#define nef_late_bank(result,token,status) ((result)==RAW_UNSAFE_SLOT && nef_before_wrap(token,status))
#define nef_prepare_status(token,start,status) nef_source_status(token,start,status,RAW_COPY_MS)
#endif

static u32 nef_validate_completion(const struct packet *packet,u32 token,struct z72_raw_status *status)
{
    u32 result=nef_source_status(token,packet->start_tick,status,NEF_WAIT_MS);
#ifdef Z72_RAW_TERMINAL_ROW
    if (at_terminal_row(result,token,status)) return NEF_WAIT_FOR_BANK;
#endif
#ifdef Z72_RAW_PCM_2997_HANDOFF
    /* A bank already seen just before the source cannot move backwards. */
    if (nef_source_job.late && !nef_late_bank(result,token,status))
        return result==RAW_COPY_OK ? RAW_UNSAFE_SLOT : result;
#endif
#ifdef Z72_RAW_BANK_HANDOFF
    if (load(&early_anchor)==token) {
        const int last_safe_bank=result!=RAW_COPY_OK;
        if (last_safe_bank && !nef_late_bank(result,token,status)) return result;
        if (!nef_check_samples(packet->address,1)) return RAW_SAMPLE_CHANGED;
        result=nef_source_status(token,packet->start_tick,status,NEF_WAIT_MS);
        if (result==RAW_COPY_OK) return last_safe_bank ? RAW_UNSAFE_SLOT : RAW_COPY_OK;
        return nef_late_bank(result,token,status) ? RAW_COPY_OK : result;
    }
#endif
    if (result!=RAW_UNSAFE_SLOT
#ifdef Z72_RAW_PCM_2997_HANDOFF
        && !nef_late_bank(result,token,status)
#endif
    ) return result;
    ++timing_header[13];
    result=nef_source_status(token,packet->start_tick,status,NEF_WAIT_MS);
#ifdef Z72_RAW_PCM_2997_HANDOFF
    if (nef_source_job.late && result==RAW_COPY_OK) return RAW_UNSAFE_SLOT;
#endif
    if (nef_late_bank(result,token,status)) {
        /* The active DMA bank is the
         * last bank before the source. Confirm it still owns the same span
         * after rereading active pixels from the source. */
        if (!nef_check_samples(packet->address,1)) return RAW_SAMPLE_CHANGED;
        result=nef_source_status(token,packet->start_tick,status,NEF_WAIT_MS);
        if (nef_late_bank(result,token,status)) result=RAW_COPY_OK;
        else if (result==RAW_COPY_OK) result=RAW_UNSAFE_SLOT;
    }
    if (result==RAW_COPY_OK) {
        ++timing_header[14];
#ifdef Z72_RAW_PCM_2997_HANDOFF
        if (nef_source_job.late) ++log.handoff.late_completions;
#endif
    }
    return result;
}
#endif

#ifdef Z72_RAW_PREFETCH_PARALLEL
static int nef_backup_reason(u32 result)
{
    return result==RAW_WINDOW_EXPIRED || result==RAW_UNSAFE_SLOT || result==RAW_SAMPLE_CHANGED ||
           result==RAW_SNAPSHOT_UNSTABLE || result==NEF_STOPPING;
}
#endif

static u32 nef_encode(struct packet *packet,u32 token,struct z72_raw_status *status,u32 source)
{
#ifdef Z72_RAW_PREFETCH_NEF
    const int private_input=source!=packet->address;
#else
    const int private_input=STAGED_NEF_INPUT;
#endif
    const u32 started=private_input ? tick() : packet->start_tick;
#ifdef Z72_RAW_NEF_DIRECT_OVERSIZE
    while (load(&consumed)<nef_scratch_until) {
#ifdef Z72_RAW_NEF_QUIET_STOP
        if (nef_input_stopping(private_input)) return NEF_STOPPING;
#endif
        log.nef.last_error_stage=4;
        if (load(&card_error) || tick()-started>=RAW_COPY_MS) return NEF_OUTPUT_BUSY;
        CALL(0x103dc539u,void,u32)(1u);
    }
    unsigned char *encoded;
#ifdef Z72_RAW_NEF_SLOT_ENCODE
    packet->reserved[5]=nef_offset;
    encoded=stream_packet(packet,load(&produced))+64u;
    volatile u32 *guard=(volatile u32 *)(encoded+nef_slot_payload_limit);
#else
    encoded=nef_scratch_packet(nef_offset)+64u;
#endif
#else
    unsigned char *const encoded=nef_scratch;
#endif
#ifdef Z72_RAW_NEF_EVENT_WAIT
    log.nef.last_error_stage=1; /* prepare or acquire encoder */
    if ((int)nef_event<=0) CALL(0x10361083u,void,u32 *)(&nef_event);
#endif
    if (!z72_raw_identity_span(source,clip_pixel_bytes())) return RAW_SOURCE_MAPPING;
    if ((int)WORD(NEF_QUEUE+0x20u)<=0 || (int)WORD(NEF_LOCK)<=0 || !WORD(0xb1809638u) ||
        !CALL(0x10360f2fu,int,u32,u32)(NEF_LOCK,1u)) return NEF_UNAVAILABLE;
#ifdef Z72_RAW_NEF_SLOT_VALIDATE
    u32 result=private_input ? RAW_COPY_OK : nef_prepare_status(token,started,status);
#ifdef Z72_RAW_PCM_2997_HANDOFF
    const u32 preceding=((token&RAW_TOKEN_SLOT)+1u)%RAW_SLOT_COUNT;
    const int late_initial=status->live_index==preceding;
#endif
    if (!private_input && result==RAW_COPY_OK) {
        nef_check_samples(packet->address,0);
        result=nef_prepare_status(token,packet->start_tick,status);
#ifdef Z72_RAW_PCM_2997_HANDOFF
        if (result==RAW_COPY_OK && late_initial && status->live_index!=preceding) result=RAW_UNSAFE_SLOT;
#endif
    }
#else
    u32 result=nef_source_status(token,started,status,RAW_COPY_MS);
#endif
    if (result==RAW_COPY_OK) {
#ifdef Z72_RAW_NEF_SLOT_ENCODE
        for (u32 i=0;i<NEF_SLOT_GUARD/sizeof(*guard);++i) guard[i]=0x5ac39867u;
        clean_bytes((void *)guard,NEF_SLOT_GUARD);
        nef_desc[8]=nef_slot_payload_limit;
#endif
        nef_desc[16]=source;
        nef_desc[18]=(u32)encoded;
        for (u32 i=22;i<27;++i) nef_desc[i]=0;
        publish(&nef_complete,0);
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
        nef_source_header.address=source;
#ifdef Z72_RAW_PREFETCH_NEF
        nef_source_header.start_tick=packet->start_tick;
#else
        nef_source_header.start_tick=started;
#endif
        nef_source_job.token=token;
        nef_source_job.status=*status;
#ifdef Z72_RAW_PREFETCH_NEF
        publish(&nef_source_job.private_input,(u32)private_input);
#endif
#ifdef Z72_RAW_PCM_2997_HANDOFF
        nef_source_job.late=!private_input && z72_clip_fps_num==30000u &&
                            status->live_index==preceding;
#endif
#endif
#ifdef Z72_RAW_NEF_EVENT_WAIT
        if ((int)nef_event>0) CALL(0x103610d3u,void,u32 *,u32)(&nef_event,1u);
        log.nef.last_error_stage=2; /* waiting for native encoder completion */
#endif
#ifdef Z72_RAW_NEF_QUIET_STOP
        if (nef_input_stopping(private_input)) {
            CALL(0x10360f13u,void,u32)(NEF_LOCK);
            return NEF_STOPPING;
        }
#endif
#ifdef Z72_RAW_PREFETCH_NEF
        publish(&nef_prefetch_ordinal,0u);
        if (!private_input) {
            publish(&nef_prefetch_tick,packet->start_tick);
            publish(&nef_prefetch_ordinal,packet->ordinal);
        }
#endif
        publish(&nef_pending,1);
#ifdef Z72_RAW_PCM_2997_HANDOFF
        if (nef_source_job.late) ++log.handoff.late_submits;
#endif
#ifdef Z72_RAW_TERMINAL_ROW
        if (!private_input && load(&early_anchor)==token && status->live_address==packet->address)
            ++log.terminal_row.submitted;
#endif
        const u32 start=tick();
#ifdef Z72_RAW_REC_SPLIT_SNAPSHOT
        publish(&nef_submit_tick,start);
#endif
#ifdef Z72_RAW_STAGED_NEF_TEST
        log.staged_nef.encoded_source=source;
#endif
        CALL(0x103b57bdu,void,void *)(&nef_request);
#ifdef Z72_RAW_NEF_EVENT_WAIT
        u32 code=0;
        if ((int)nef_event>0) {
            u32 elapsed=tick()-started;
            if (elapsed<NEF_WAIT_MS && nef_wait_event(NEF_WAIT_MS-elapsed)) {
                code=load(&nef_complete);
                ++log.nef.signaled;
            }
        } else {
            ++log.nef.polling;
#endif
        while (!load(&nef_complete) && tick()-started<NEF_WAIT_MS)
            CALL(0x103dc539u,void,u32)(1);
#ifdef Z72_RAW_NEF_EVENT_WAIT
            code=load(&nef_complete);
        }
#else
        const u32 code=load(&nef_complete);
#endif
#ifdef Z72_RAW_NEF_LATE_COMPLETION
        const u32 waited=tick()-started;
        log.nef_deadline.last_ms=waited;
        if (waited>log.nef_deadline.max_ms) log.nef_deadline.max_ms=waited;
#endif
        if (!code) result=RAW_COPY_TIMEOUT;
        else {
            publish(&nef_pending,0);
            packet->reserved[3]=nef_complete_tick-start;
#ifdef Z72_RAW_NEF_EVENT_WAIT
            log.nef.last_error_stage=3; /* validate source after completion */
#endif
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
            *status=nef_source_job.status;
            result=nef_source_job.result;
#elif defined(Z72_RAW_NEF_SLOT_VALIDATE)
            result=nef_validate_completion(packet,token,status);
#else
            result=nef_source_status(token,packet->start_tick,status,NEF_WAIT_MS);
#endif
            if (code!=NEF_DONE || nef_desc[23] || nef_desc[26]!=NEF_PIXELS) result=RAW_DMA_ERROR;
#ifdef Z72_RAW_NEF_SLOT_ENCODE
            if (code==0x42192c12u && nef_desc[23]==1u && !nef_desc[25] && !nef_desc[26]) result=NEF_OVERSIZE;
#ifdef Z72_RAW_PREFETCH_PARALLEL
            /* A rejected live source can coincide with a malformed hardware
             * result. Preserve the hardware failure before considering retry. */
            if (load(&prefetched_address) && nef_backup_reason(result)) {
                if (nef_desc[25]>nef_slot_payload_limit) result=NEF_OVERSIZE;
                else if (nef_desc[25]<NEF_MIN_WRITE) result=RAW_DMA_ERROR;
            }
#endif
            invalidate_dma_pixels((u32)guard,NEF_SLOT_GUARD);
            for (u32 i=0;i<NEF_SLOT_GUARD/sizeof(*guard);++i)
                if (guard[i]!=0x5ac39867u) result=RAW_DMA_ERROR;
#endif
#ifdef Z72_RAW_NEF_LATE_COMPLETION
            if (result==RAW_COPY_OK && waited>=RAW_COPY_MS) ++log.nef_deadline.accepted_late;
#endif
        }
    }
    CALL(0x10360f13u,void,u32)(NEF_LOCK);
#ifdef Z72_RAW_TERMINAL_ROW
    /* The callback must never sleep. Keep the output private until the
     * source has left the bank; only the capture task waits here. */
    const int first=!private_input && load(&early_anchor)==token && !load(&produced);
    if (result==NEF_WAIT_FOR_BANK || (first && result==RAW_COPY_OK)) {
        if (result==NEF_WAIT_FOR_BANK) ++log.terminal_row.deferred;
        const u32 i=(token>>RAW_TOKEN_SHIFT)%RAW_SLOT_COUNT;
        for (;;) {
            if (nef_capture_stopping()) { result=NEF_STOPPING; break; }
            if (tick()-packet->start_tick>=NEF_WAIT_MS) { result=RAW_COPY_TIMEOUT; break; }
            result=nef_validate_completion(packet,token,status);
            if (result==RAW_COPY_OK) {
                if (!first) break;
                if (load(&first_tails[i].token)==token) {
                    const u32 ms=load(&first_tails[i].ms);
                    if (load(&first_tails[i].token)==token) {
                        log.terminal_row.audio_ms=ms;
                        log.terminal_row.first_anchored=1;
                        break;
                    }
                }
            } else if (result!=NEF_WAIT_FOR_BANK) break;
            CALL(0x103dc539u,void,u32)(1u);
        }
        log.terminal_row.final_result=result;
    }
#endif
    if (result!=RAW_COPY_OK) return result;
    const u32 count=nef_desc[25];
#ifdef Z72_RAW_NEF_DIRECT_OVERSIZE
    if (count>log.nef_output.largest_frame) log.nef_output.largest_frame=count;
    if (count>(NEF_SLOT_ENCODE ? nef_slot_payload_limit : NEF_MAX_WRITE)) return NEF_OVERSIZE;
#else
    if (count>NEF_CAPACITY) return NEF_OVERSIZE;
#endif
    if (count<NEF_MIN_WRITE) return RAW_DMA_ERROR;
    /* Hardware has finished reading the live slot. The private scratch and
     * serialized packet can now be staged independently of sensor rotation. */
    packet->payload_bytes=count;
    packet->reserved[5]=nef_offset;
#ifdef Z72_RAW_NEF_SPILL
#ifdef Z72_RAW_NEF_EVENT_WAIT
    log.nef.last_error_stage=4; /* wait for an available output buffer */
#endif
    const u32 index=load(&produced),waiting=tick();
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
    const u32 budget=NEF_OUTPUT_WAIT_MS;
#else
    const u32 budget=80u;
#endif
    while (index-load(&consumed)>=frame_capacity) {
#ifdef Z72_RAW_NEF_QUIET_STOP
        if (nef_input_stopping(private_input)) return NEF_STOPPING;
#endif
        if (load(&card_error) || tick()-started>=budget) return NEF_OUTPUT_BUSY;
        CALL(0x103dc539u,void,u32)(1);
    }
    packet->reserved[6]=tick()-waiting;
    struct packet *slot=queued_packet(index);
    slot->reserved[5]=nef_offset;
    unsigned char *dest=stream_packet(slot,index)+64u;
#else
    unsigned char *dest=stream_packet(packet,0)+64u;
#endif
#ifdef Z72_RAW_NEF_QUIET_STOP
    if (!STAGED_NEF_LIVE && nef_input_stopping(private_input)) return NEF_STOPPING;
#endif
    const u32 staging=tick();
#ifdef Z72_RAW_NEF_DMA_STAGE
#ifdef Z72_RAW_NEF_EVENT_WAIT
    log.nef.last_error_stage=5; /* raster transfer from encoder buffer */
#endif
    const u32 bytes=(count+63u)&~63u;
#ifdef Z72_RAW_NEF_DIRECT_OVERSIZE
    if (NEF_SLOT_ENCODE) {
        ++log.nef_output.direct_frames;
    } else if (nef_write_from_scratch(index,count)) {
        dest=encoded;
        nef_scratch_until=index+1u;
        ++log.nef_output.direct_frames;
    } else
#endif
    {
        result=nef_stage_dma((u32)dest,(u32)encoded,bytes,staging);
        if (result!=RAW_COPY_OK) return result;
    }
    if (count!=bytes) {
        unsigned char *line=dest+(count&~63u);
        invalidate_dma_pixels((u32)line,64u);
        for (u32 i=count;i<bytes;++i) dest[i]=0;
        clean_bytes(line,64u);
    }
#else
    const u32 bytes=count;
    invalidate_dma_pixels((u32)encoded,count);
    CALL(0x10360f44u,void,void *,const void *,u32)(dest,encoded,count);
    clean_bytes(dest,count);
#endif
    packet->reserved[4]=tick()-staging;
#ifndef Z72_RAW_SIZE_SPLIT
    nef_offset+=64u+bytes;
#ifdef Z72_RAW_PCM_SEGMENTS
    if ((load(&produced)+1u)%Z72_RAW_NEF_BURST_LIMIT==0u) nef_offset=sizeof(header);
#endif
#endif
    return RAW_COPY_OK;
}
