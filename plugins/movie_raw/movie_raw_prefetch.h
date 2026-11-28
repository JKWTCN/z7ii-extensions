/* SPDX-License-Identifier: MIT */
/* The raster worker is the sole submitter/reaper for this slot. The capture
 * task takes a verified copy by ordinal and returns it after the NEF call. */
enum {
    PREFETCH_OFF, PREFETCH_EMPTY, PREFETCH_SELECTING, PREFETCH_COPYING,
    PREFETCH_READY, PREFETCH_HELD, PREFETCH_RASTER_DRAIN, PREFETCH_NEF_DRAIN
};
/* One 30000/1001 frame period is 33.37 ms. */
enum { PREFETCH_AFTER_MS=34u };
static u32 prefetch_enabled,prefetch_state,prefetch_token,prefetch_started,prefetch_attempted;
static struct z72_raw_status prefetch_status;

static int prefetch_move(u32 from,u32 to)
{
    return __atomic_compare_exchange_n(&prefetch_state,&from,to,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}

#ifdef Z72_RAW_PREFETCH_PARALLEL
enum { PREFETCH_UNUSED=PREFETCH_NEF_DRAIN+1 };
static u32 prefetch_accepted;

static void prefetch_retire_ready(u32 through)
{
    if (load(&prefetch_state)!=PREFETCH_READY ||
        (load(&prefetch_token)>>RAW_TOKEN_SHIFT)-1u>through ||
        !prefetch_move(PREFETCH_READY,PREFETCH_SELECTING)) return;
    /* A later ordinal may have become READY before the claim. */
    const u32 ordinal=(load(&prefetch_token)>>RAW_TOKEN_SHIFT)-1u;
    publish(&prefetch_state,ordinal<=through ? PREFETCH_UNUSED : PREFETCH_READY);
}

/* The worker and the capture task consume the same direct-job authorization.
 * Winning this exchange lets the worker finish one copy even if the direct
 * frame is accepted immediately afterwards. It never consumes a newer job. */
static int prefetch_withdraw_direct(u32 ordinal)
{
    return __atomic_compare_exchange_n(&nef_prefetch_ordinal,&ordinal,0u,0,
                                       __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}

static void prefetch_reject(u32 ordinal)
{
    prefetch_withdraw_direct(ordinal);
}

static void prefetch_accept(u32 ordinal)
{
    if (!load(&prefetch_enabled)) return;
    prefetch_withdraw_direct(ordinal);
    publish(&prefetch_accepted,ordinal);
    prefetch_retire_ready(ordinal);
}
#endif

#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
/* The capture task offers at most once per target and never extends the
 * original deadline. Only the worker owns the raster descriptor. */
static u32 prefetch_output_token,prefetch_output_tick,prefetch_last_offer;
static u32 prefetch_session,prefetch_output_copy;

static int prefetch_withdraw_output(u32 token)
{
    return __atomic_compare_exchange_n(&prefetch_output_token,&token,0u,0,
                                       __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}

static void prefetch_offer_output_wait(u32 previous)
{
    const u32 bank=previous&RAW_TOKEN_SLOT,ordinal=previous>>RAW_TOKEN_SHIFT;
    if (!load(&prefetch_enabled) || load(&state)!=ARMED || load(&stop_requested) ||
        load(&card_error) || nef_capture_stopping() || !ordinal || !(previous&RAW_TOKEN_PAIR) ||
        !bank || bank>RAW_SLOT_COUNT || ordinal!=load(&prefetch_accepted)+1u) return;
    const u32 target=z72_raw_next_tail(previous,bank%RAW_SLOT_COUNT+1u);
    if (prefetch_last_offer==target || load(&prefetch_attempted)==ordinal) return;
    prefetch_last_offer=target;
    publish(&prefetch_output_token,0u);
    publish(&prefetch_output_tick,tick());
    publish(&prefetch_output_token,target);
    ++log.output_wait.offers;
}

static void prefetch_claim_output_wait(u32 token)
{
    const u32 offered=load(&prefetch_output_token);
    if (offered && (offered>>RAW_TOKEN_SHIFT)==(token>>RAW_TOKEN_SHIFT))
        prefetch_withdraw_output(offered);
}

static int prefetch_output_current(u32 token,u32 start,u32 session)
{
    return token && load(&prefetch_session)==session &&
           (token>>RAW_TOKEN_SHIFT)==load(&prefetch_accepted)+2u &&
           tick()-start<PREFETCH_AFTER_MS && load(&prefetch_output_tick)==start &&
           load(&prefetch_output_token)==token;
}

static int prefetch_claim_job(u32 offered,u32 direct,u32 capture,u32 session)
{
    if (!load(&prefetch_enabled) || load(&state)!=ARMED || load(&stop_requested) ||
        load(&card_error) || nef_capture_stopping() || load(&prefetch_session)!=session) return 0;
    if (offered)
        return prefetch_output_current(offered,capture,session) && prefetch_withdraw_output(offered);
    return tick()-capture<RAW_COPY_MS && load(&nef_prefetch_tick)==capture && prefetch_withdraw_direct(direct);
}
#endif

#ifdef Z72_RAW_MIXED_FRAMES
enum { PREFETCH_WRITER=PREFETCH_UNUSED+1 };
static u32 mixed_raster_sponsor,mixed_capture_tick,mixed_overlap_ms;

/* Measure the intersection of the native request intervals. Each raster
 * band has its own terminal callback. Neither clock is a device cycle count. */
static void mixed_raster_complete(u32 code,u32 context)
{
    const u32 sponsor=load(&mixed_raster_sponsor);
    if (sponsor && code==RASTER_SUCCESS) {
        const u32 capture=load(&mixed_capture_tick),now=tick()-capture;
        const u32 token=load(&nef_source_job.token),submit=load(&nef_submit_tick);
        const u32 nef_start=submit-capture,raster_start=load(&raster_submitted)-capture;
        const u32 callback=load(&nef_complete_tick)-capture;
        const u32 first=nef_start>raster_start ? nef_start : raster_start;
        const u32 last=callback>=nef_start && callback<now ? callback : now;
        if ((token>>RAW_TOKEN_SHIFT)==sponsor+1u && now<2u*RAW_COPY_MS && first<last &&
            token==load(&nef_source_job.token) && submit==load(&nef_submit_tick)) {
            publish(&mixed_overlap_ms,load(&mixed_overlap_ms)+last-first);
        }
    }
    raster_complete(code,context);
}
#endif

static void prefetch_start(void)
{
    /* Called before ARMED, with a newly allocated DMA-visible buffer. */
    publish(&prefetch_token,0);
    publish(&prefetch_attempted,0);
#ifdef Z72_RAW_PREFETCH_PARALLEL
    publish(&nef_prefetch_ordinal,0);
    publish(&prefetch_accepted,0);
#endif
#ifdef Z72_RAW_MIXED_FRAMES
    publish(&mixed_raster_sponsor,0);
    raster_message.callback=mixed_raster_complete;
#endif
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    publish(&prefetch_output_token,0);
    publish(&prefetch_output_copy,0);
    prefetch_last_offer=0;
    publish(&prefetch_session,load(&prefetch_session)+1u);
#endif
    log.prefetch.address=prefetched_address;
    publish(&prefetch_state,PREFETCH_EMPTY);
    publish(&prefetch_enabled,1);
}

static u32 prefetch_next_anchor(u32 previous)
{
    const u32 bank=previous&RAW_TOKEN_SLOT;
    if (!(previous&RAW_TOKEN_PAIR) || !bank || bank>RAW_SLOT_COUNT) return 0;
    const u32 next=bank%RAW_SLOT_COUNT,base=z72_raw_slot_address(next);
    if (WORD(PREVIEW+0x74u)!=next || WORD(PREVIEW+0x78u)!=base) return 0;
    const u32 cursor=WORD(PREVIEW+0x7cu);
    if (cursor<base+z72_uhd.stride || cursor>=base+clip_pixel_bytes() ||
        WORD(PREVIEW+0x74u)!=next || WORD(PREVIEW+0x78u)!=base ||
        load(&tail_token)!=previous) return 0;
    return z72_raw_next_tail(previous,next+1u);
}

static u32 prefetch_check(u32 token,u32 start,struct z72_raw_status *status)
{
    const u32 result=z72_raw_copy_status(&tail_token,token,start,status);
    /* A predicted anchor is valid only while the preceding real tail and
     * the live write cursor independently identify that exact handover. */
    if (result==RAW_WINDOW_EXPIRED && prefetch_next_anchor(status->observed_token)==token)
        return RAW_COPY_OK;
    return result;
}

static int prefetch_last_bank(u32 result,u32 token,const struct z72_raw_status *status)
{
    return result==RAW_UNSAFE_SLOT &&
           status->observed_token==z72_raw_next_tail(token,(token&RAW_TOKEN_SLOT)%RAW_SLOT_COUNT+1u) &&
           load(&nef_previous_tail)==token && nef_before_wrap(token,status);
}

static u32 prefetch_copy(u32 token,u32 start,struct z72_raw_status *status)
{
    const u32 slot=((token&RAW_TOKEN_SLOT)+RAW_SLOT_COUNT-2u)%RAW_SLOT_COUNT;
    const u32 source=z72_raw_slot_address(slot),dest=prefetched_address;
    if (!z72_raw_identity_span(source,clip_pixel_bytes())) return RAW_SOURCE_MAPPING;
    const u32 stride=z72_uhd.stride,height=z72_uhd.height;
    const int sliced=BYTE(RASTER_MODE)==1;
    const u32 band=sliced ? 0x40000u/stride : height;
    for (u32 row=0;row<height;row+=band) {
        const u32 result=prefetch_check(token,start,status);
        if (result!=RAW_COPY_OK) return result;
        const u32 rows=height-row<band ? height-row : band;
        const u32 copied=raster_copy_rows(dest+row*stride,source+row*stride,stride,rows,start,sliced);
        if (copied!=RAW_COPY_OK) return copied;
    }
    const u32 before=prefetch_check(token,start,status);
    if (before!=RAW_COPY_OK && !prefetch_last_bank(before,token,status)) return before;
    const unsigned char *pixels=(const unsigned char *)dest;
    if (!sample_matches(pixels,source,0) || !sample_matches(pixels,source,height/2u) ||
        !sample_matches(pixels,source,height-1u)) return RAW_SAMPLE_CHANGED;
    const u32 after=prefetch_check(token,start,status);
    if (after==RAW_COPY_OK) return before==RAW_COPY_OK ? RAW_COPY_OK : RAW_UNSAFE_SLOT;
    return prefetch_last_bank(after,token,status) ? RAW_COPY_OK : after;
}

static void prefetch_step(void)
{
    const u32 phase=load(&prefetch_state);
#ifdef Z72_RAW_PREFETCH_PARALLEL
    if (phase==PREFETCH_UNUSED) {
        ++log.prefetch.missed;
        publish(&prefetch_state,PREFETCH_EMPTY);
        return;
    }
    if (phase==PREFETCH_READY) {
        prefetch_retire_ready(load(&prefetch_accepted));
        return;
    }
#endif
    if (phase==PREFETCH_RASTER_DRAIN) {
        /* A timeout leaves the native request/DMA destination in use. */
        if (!load(&dma_pending_channel) || raster_reap(0)!=RAW_COPY_TIMEOUT)
            prefetch_move(PREFETCH_RASTER_DRAIN,PREFETCH_EMPTY);
        return;
    }
    if (phase==PREFETCH_NEF_DRAIN) {
        if (!load(&nef_pending)) prefetch_move(PREFETCH_NEF_DRAIN,PREFETCH_EMPTY);
        return;
    }
    if (!load(&prefetch_enabled) || load(&state)!=ARMED ||
#ifdef Z72_RAW_MIXED_FRAMES
        load(&card_error) ||
#endif
#ifndef Z72_RAW_PREFETCH_PARALLEL
        !load(&nef_pending) || load(&nef_complete) ||
#endif
        load(&stop_requested)) return;
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    if (load(&card_error)) return;
    const u32 session=load(&prefetch_session),offered=load(&prefetch_output_token);
    const u32 offered_tick=load(&prefetch_output_tick);
    const int output_wait=prefetch_output_current(offered,offered_tick,session);
    const u32 direct=output_wait ? (offered>>RAW_TOKEN_SHIFT)-2u : load(&nef_prefetch_ordinal);
    const u32 capture=output_wait ? offered_tick : load(&nef_prefetch_tick);
#else
    const u32 direct=load(&nef_prefetch_ordinal),capture=load(&nef_prefetch_tick);
#endif
#ifdef Z72_RAW_PREFETCH_PARALLEL
    /* The native IRQ/callback can precede capture-task handoff by several
     * ticks. The direct job remains eligible until accepted or rejected. */
    const u32 age=tick()-capture;
    if (
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
        !output_wait &&
#endif
        (direct<=load(&prefetch_accepted) ||
#ifndef Z72_RAW_MIXED_FRAMES
         age<PREFETCH_AFTER_MS ||
#endif
         age>=RAW_COPY_MS)) return;
#else
    if (!direct || tick()-capture<PREFETCH_AFTER_MS) return;
    if (phase==PREFETCH_READY && (load(&prefetch_token)>>RAW_TOKEN_SHIFT)-1u<=direct) {
        if (prefetch_move(PREFETCH_READY,PREFETCH_SELECTING)) {
            ++log.prefetch.missed;
            publish(&prefetch_state,PREFETCH_EMPTY);
        }
        return;
    }
#endif
    if (phase!=PREFETCH_EMPTY || load(&prefetch_attempted)==direct+1u ||
        !prefetch_move(PREFETCH_EMPTY,PREFETCH_SELECTING)) return;
    const u32 tail=load(&tail_token),anchor_ordinal=direct+2u;
    u32 token=0;
    if ((tail&RAW_TOKEN_PAIR) && (tail>>RAW_TOKEN_SHIFT)==anchor_ordinal) token=tail;
    else if ((tail>>RAW_TOKEN_SHIFT)==anchor_ordinal-1u) token=prefetch_next_anchor(tail);
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    else if (output_wait && tail==z72_raw_next_tail(offered,(offered&RAW_TOKEN_SLOT)%RAW_SLOT_COUNT+1u) &&
             load(&nef_previous_tail)==offered && load(&tail_token)==tail) token=offered;
    const int same_job=output_wait ? token==offered && prefetch_output_current(offered,capture,session) :
                       load(&nef_prefetch_ordinal)==direct && load(&nef_prefetch_tick)==capture;
#endif
    if (!token || !load(&prefetch_enabled) || load(&state)!=ARMED ||
#ifndef Z72_RAW_PREFETCH_PARALLEL
        !load(&nef_pending) || load(&nef_complete) ||
#endif
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
        !same_job || load(&card_error) ||
#else
        load(&nef_prefetch_ordinal)!=direct || load(&nef_prefetch_tick)!=capture ||
#endif
        load(&stop_requested) || nef_capture_stopping()) {
        publish(&prefetch_state,PREFETCH_EMPTY);
        return;
    }
    const u32 start=tick();
    struct z72_raw_status status={0};
    if (prefetch_check(token,start,&status)!=RAW_COPY_OK) {
        publish(&prefetch_state,PREFETCH_EMPTY);
        return;
    }
#ifdef Z72_RAW_PREFETCH_PARALLEL
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    if (!prefetch_claim_job(output_wait ? offered : 0u,direct,capture,session)) {
#else
    if (tick()-capture>=RAW_COPY_MS || load(&nef_prefetch_tick)!=capture ||
        !load(&prefetch_enabled) || load(&state)!=ARMED || load(&stop_requested) ||
#ifdef Z72_RAW_MIXED_FRAMES
        load(&card_error) || nef_capture_stopping() ||
#endif
        !prefetch_withdraw_direct(direct)) {
#endif
        publish(&prefetch_state,PREFETCH_EMPTY);
        return;
    }
#endif
    publish(&prefetch_attempted,direct+1u);
    publish(&prefetch_token,token);
    publish(&prefetch_started,start);
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    publish(&prefetch_output_copy,output_wait ? token : 0u);
    const u32 requests=log.raster_requests;
#endif
    ++log.prefetch.attempts;
    log.prefetch.last_ordinal=direct+1u;
#ifdef Z72_RAW_MIXED_FRAMES
    publish(&mixed_overlap_ms,0);
    publish(&mixed_capture_tick,capture);
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    publish(&mixed_raster_sponsor,output_wait ? 0u : direct);
#else
    publish(&mixed_raster_sponsor,direct);
#endif
#endif
    publish(&prefetch_state,PREFETCH_COPYING);
    const u32 result=prefetch_copy(token,start,&status);
#ifdef Z72_RAW_MIXED_FRAMES
    publish(&mixed_raster_sponsor,0);
    const u32 overlap=load(&mixed_overlap_ms);
    if (result==RAW_COPY_OK && overlap) {
        ++log.mixed.overlaps;
        if (overlap>log.mixed.max_overlap_ms) log.mixed.max_overlap_ms=overlap;
    }
#endif
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    if (output_wait && log.raster_requests!=requests) {
        ++log.output_wait.started;
        log.output_wait.last_ordinal=direct+1u;
        if (result==RAW_COPY_OK) ++log.output_wait.ready;
    }
#endif
    const u32 elapsed=tick()-start;
    log.prefetch.last_result=result;
    if (elapsed>log.prefetch.max_copy_ms) log.prefetch.max_copy_ms=elapsed;
    if (result==RAW_COPY_OK) {
        prefetch_status=status;
        ++log.prefetch.ready;
#ifdef Z72_RAW_PREFETCH_PARALLEL
        if (direct+1u<=load(&prefetch_accepted)) {
            ++log.prefetch.missed;
            publish(&prefetch_state,PREFETCH_EMPTY);
            return;
        }
#endif
        publish(&prefetch_state,PREFETCH_READY);
    } else {
        publish(&prefetch_state,load(&dma_pending_channel) ? PREFETCH_RASTER_DRAIN : PREFETCH_EMPTY);
    }
}

static void prefetch_worker(void *unused)
{
    (void)unused;
    for (;;) {
        prefetch_step();
        CALL(0x103dc539u,void,u32)(load(&prefetch_enabled) ? 1u : 20u);
    }
}

static u32 prefetch_ready_token(u32 ordinal)
{
    if (!load(&prefetch_enabled) || load(&state)!=ARMED) return 0;
#ifdef Z72_RAW_PREFETCH_PARALLEL
    if (ordinal<=load(&prefetch_accepted)) return 0;
#endif
    const u32 phase=load(&prefetch_state);
    if (phase!=PREFETCH_COPYING && phase!=PREFETCH_READY) return 0;
    const u32 token=load(&prefetch_token);
    return (token>>RAW_TOKEN_SHIFT)==ordinal+1u ? token : 0;
}

#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
static int prefetch_ready_output_wait(u32 token)
{
    return token && load(&prefetch_state)==PREFETCH_READY && load(&prefetch_output_copy)==token &&
           prefetch_ready_token((token>>RAW_TOKEN_SHIFT)-1u)==token;
}
#endif

static u32 prefetch_take(u32 ordinal,struct z72_raw_status *status,u32 *copy_token,u32 *start_tick)
{
    const u32 token=prefetch_ready_token(ordinal);
    if (!token) return 0;
    const u32 wait=tick();
    while (load(&prefetch_state)==PREFETCH_COPYING && prefetch_ready_token(ordinal)==token &&
           tick()-load(&prefetch_started)<RAW_COPY_MS)
        CALL(0x103dc539u,void,u32)(1u);
    const u32 elapsed=tick()-wait;
    if (elapsed>log.prefetch.max_wait_ms) log.prefetch.max_wait_ms=elapsed;
    if (!load(&prefetch_enabled) || load(&state)!=ARMED ||
        !prefetch_move(PREFETCH_READY,PREFETCH_HELD)) return 0;
    if (load(&prefetch_token)!=token) {
        publish(&prefetch_state,PREFETCH_READY);
        return 0;
    }
    *status=prefetch_status;
    *copy_token=token;
    *start_tick=load(&prefetch_started);
    ++log.prefetch.used;
    return prefetched_address;
}

static void prefetch_release(void)
{
    /* Called by the encoder owner after returning from its NEF request. */
    publish(&prefetch_state,load(&nef_pending) ? PREFETCH_NEF_DRAIN : PREFETCH_EMPTY);
}

#ifdef Z72_RAW_MIXED_FRAMES
/* Once a packet references this raster the capture task can publish other
 * frames, but only the card task may release this one after its write. */
static u32 prefetch_take_packed(u32 token,struct z72_raw_status *status,u32 *started)
{
    const u32 ordinal=(token>>RAW_TOKEN_SHIFT)-1u;
    if (!token || prefetch_ready_token(ordinal)!=token) return 0;
    u32 actual;
    const u32 address=prefetch_take(ordinal,status,&actual,started);
    if (!address) return 0;
    if (actual!=token) {
        publish(&prefetch_state,PREFETCH_READY);
        return 0;
    }
    publish(&prefetch_state,PREFETCH_WRITER);
    return address;
}

static u32 prefetch_packed_address(u32 ordinal)
{
    if (load(&prefetch_state)!=PREFETCH_WRITER ||
        (load(&prefetch_token)>>RAW_TOKEN_SHIFT)!=ordinal+1u) return 0;
    return prefetched_address;
}

static void prefetch_release_packed(u32 ordinal)
{
    if (prefetch_packed_address(ordinal)) prefetch_move(PREFETCH_WRITER,PREFETCH_EMPTY);
}
#endif

static int prefetch_quiesce(void)
{
    publish(&prefetch_enabled,0);
    const u32 phase=load(&prefetch_state);
    if (phase==PREFETCH_OFF) return 1;
#ifdef Z72_RAW_MIXED_FRAMES
    /* The card task is the only caller. On a failed earlier packet the
     * pending packed packet was never written and will never be consumed. */
    if (phase==PREFETCH_WRITER && load(&state)==FINISHED && load(&card_error)) {
        if (!prefetch_move(phase,PREFETCH_OFF)) return 0;
        raster_message.callback=raster_complete;
        return 1;
    }
#endif
#ifdef Z72_RAW_PREFETCH_PARALLEL
    if (phase==PREFETCH_READY) {
        prefetch_retire_ready(~0u);
        return 0;
    }
#endif
    if (phase==PREFETCH_READY || phase==PREFETCH_EMPTY) {
        const int closed=prefetch_move(phase,PREFETCH_OFF);
#ifdef Z72_RAW_MIXED_FRAMES
        if (closed) raster_message.callback=raster_complete;
#endif
        return closed;
    }
    return 0;
}
