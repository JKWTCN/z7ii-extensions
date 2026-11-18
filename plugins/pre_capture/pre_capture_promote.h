/* SPDX-License-Identifier: MIT */
/* Same native acquisition before/after full press. Only owned copies reach
 * this single worker; source callbacks keep their original real ACK/cleanup.
 */
#define PROMOTE_NOTICES (Z72_PRE_SEQUENCE ? 256u : 128u)
static z72_u32 promote_notices[PROMOTE_NOTICES][5],promote_queued,promote_delivered;
static z72_u32 promote_metadata_failed;
static void restore_destination(void);
static void exposure_reset(void);

static void promote_reset(void)
{
    for (z72_u32 i=0;i<16;++i) STORE(promote[i],0);
    promote_queued=promote_delivered=promote_metadata_failed=0;
    exposure_reset();
}
static z72_u32 promote_closed(z72_u32 id)
{
    if (promote_queued>=PROMOTE_NOTICES || LOAD(pre_notice_pending)!=2 ||
        pre_notice_payload[0]!=id) return 0;
    for (z72_u32 i=0;i<5;++i) promote_notices[promote_queued][i]=pre_notice_payload[i];
    ++promote_queued; STORE(pre_notice_pending,0); STORE(pre_notice_id,0);
    return 1;
}
static void promote_fault(z72_u32 reason)
{
    __atomic_fetch_or(&promote[1],reason,__ATOMIC_RELEASE);
    STORE(enabled,0); STORE(pre_failed,1);
}
#include "pre_capture_exposure.h"
static void promote_write_one(void)
{
    if (!exposure_seed()) return;
    if (save_failures || copy_failures || promote_queued>=PROMOTE_NOTICES || !ring_enter()) return;
    for (z72_u32 i=0;i<RING_SLOTS;++i) if (ring[i].state==2 && !promote_kind[i] &&
        (ring[i].generation!=(LOAD(control)&~7u) ||
        LOAD(trigger_tick)-ring[i].ready_tick>durations[duration_index])) {
        ring_release(i); ++expired_frames;
    }
    z72_u32 slot=ring_oldest();
    if (slot==RING_SLOTS) { ring_leave(); return; }
    /* Pin one slot, not the entire ring. Native I/O and parser operations
     * cannot hold this lock while either processor is delivering JPEGs. */
    ring[slot].state=4; --ring_count; ring_leave();
    z72_u32 bytes=ring[slot].size<STORE_HEADER_BYTES ? ring[slot].size : STORE_HEADER_BYTES;
    CALL(0x1034fa29u,void,void *,z72_u32)(store_header,bytes);
    z72_u32 result=ring_copy_bytes((z72_u32)store_header,ring[slot].address,bytes,0);
    CALL(0x1034fa29u,void,void *,z72_u32)(store_header,bytes);
    if (result || !photo_header(store_header,bytes,&photo_stamps[slot])) {
        ++store_stats[1]; result=3;
    }
    const struct photo_stamp *stamp=&photo_stamps[slot];
    z72_u32 index=0;
    if (!result) {
        z72_u32 valid=exposure_index(slot,&index);
        if (valid==2) { ring[slot].state=6; return; }
        if (!valid) { promote[12]=1; result=3; }
    }
    if (!result && !exposure_reserve(slot,index)) result=3;
    if (!result) result=store_native(slot);
    if (!result) {
        for (z72_u32 i=index+1;i<exposure_count;++i) if (exposure_used[i]==2) {
            ++sequence_stats[6]; break;
        }
        exposure_used[index]=2; exposure_stamps[index]=*stamp;
        ++cache_saves; ++store_plan[2];
        if (promote_kind[slot]) {
            ++fallback[1]; ++cycle_card_count;
            shot[6]=last_id;
        } else {
            ++cycle_pre_saves; ++shot[1];
            if (!cycle_first_file) cycle_first_file=last_id;
            cycle_last_file=saved_id=last_id; shot[2]=last_id;
        }
    } else {
        ++save_failures;
        promote_fault(promote[12] ? 16u : 4u);
    }
    /* Worker owns state4. Producers cannot evict it; retry release next step
     * if a source holds the short copy lock at this point. */
    ring[slot].state=result ? 5u : 6u;
}
static void promote_retire(void)
{
    if (!ring_enter()) return;
    for (z72_u32 i=0;i<RING_SLOTS;++i) {
        if (ring[i].state==6) ring_release_one(i);
        else if (ring[i].state==5) { ring[i].state=2; ++ring_count; }
    }
    ring_leave();
}
/* Only after true native stop/drain: rebuild closed-file metadata, then
 * forward each original completion payload once in the stored DCF order. */
static z72_u32 promote_metadata(void)
{
    if (promote_delivered==promote_queued) return 1;
    z72_u32 id=promote_notices[promote_delivered][0];
    ++metadata_stats[0]; metadata_stats[2]=0;
    z72_u32 object=CALL(0x10567c09u,z72_u32,z72_u32)(id);
    if (object) {
        CALL(0x10567fffu,void,z72_u32)(object);
        CALL(0x1056788bu,void,z72_u32)(object);
        metadata_stats[2]=CALL(0x1055e1a1u,z72_u32,z72_u32)(id);
    }
    if (metadata_stats[2]!=1) {
        promote_metadata_failed=1;
        ++save_failures; promote_fault(4); return 0;
    }
    ++metadata_stats[1];
    CALL(IMAGE_NATIVE,void,z72_u32,const void *)(0x041f0002u,promote_notices[promote_delivered]);
    ++metadata_stats[4]; ++promote_delivered;
    return promote_delivered==promote_queued;
}
static z72_u32 promote_step(z72_u32 now)
{
    if (!LOAD(promote[0]) && LOAD(triggered) && LOAD(in_flight) && fallback[7]==1) {
        STORE(triggered,0); STORE(promote[0],1); promote[2]=LOAD(trigger_tick);
        ++fallback[0]; ++shot[0]; STORE(fallback[3],1); fallback[4]=LOAD(trigger_tick);
        continuation=cycle_card_count=0;
        cycle_pre_saves=cycle_first_file=cycle_last_file=0;
        store_plan[7]=LOAD(control)&~7u;
        if (ring_enter()) { ring_expire(LOAD(trigger_tick),LOAD(control)&~7u); ring_leave(); }
    }
    if (!LOAD(promote[0])) return 0;
    if ((LOAD(full_released) || !LOAD(full_latch)) &&
        (LOAD(promote[3]) || now-LOAD(promote[2])>=2000u))
        __atomic_fetch_or(&promote[1],1,__ATOMIC_RELEASE);
    if (promote_queued>=PROMOTE_NOTICES-(Z72_PRE_SEQUENCE ? RING_SLOTS+BATCH_JOBS : 0u))
        __atomic_fetch_or(&promote[1],2,__ATOMIC_RELEASE);
    /* Dropping a source during pressure need not stop a writable stream.
     * With no safe exposure base, however, writing can only start after
     * native drain. Stop when that queue first runs out of room. */
    if ((!exposure_has_base && LOAD(promote[10]) && !exposure_seed()) ||
        exposure_count>=EXPOSURE_IDS-RING_SLOTS-BATCH_JOBS)
        __atomic_fetch_or(&promote[1],2,__ATOMIC_RELEASE);
    if (!LOAD(enabled) || copy_failures || save_failures) __atomic_fetch_or(&promote[1],4,__ATOMIC_RELEASE);
    gate_pump();
    z72_u32 drained=stream_drain(now);
    if (LOAD(stream[3])) __atomic_fetch_or(&promote[1],4,__ATOMIC_RELEASE);
    promote_retire();
    if (!save_failures && !copy_failures) promote_write_one();
    gate_pump();
    /* Return a completed writer's lease immediately for the next post frame. */
    if (Z72_PRE_SEQUENCE || drained) promote_retire();
    if (!drained) return 1;
    if (save_failures || copy_failures) {
        exposure_cancel_unused(); exposure_sort_notices();
        /* No further capture or writes, but real closed files may still be
         * published after metadata succeeds. Unsaved owned bytes stay held. */
        if (promote_delivered<promote_queued && !promote_metadata_failed) {
            promote_metadata();
            if (promote_delivered<promote_queued && !promote_metadata_failed) return 1;
        }
    } else {
        if (!LOAD(ring_count) && !LOAD(ring_bytes) && !LOAD(ring_busy)) {
            exposure_cancel_unused(); exposure_sort_notices();
            if (!promote_metadata()) return 1;
            promote_queued=promote_delivered=0;
        }
        if (LOAD(ring_count) || LOAD(ring_busy) || LOAD(ring_bytes)) return 1;
    }
    STORE(in_flight,0); STORE(file_done,1); STORE(stream[0],0);
    STORE(fallback[3],0); STORE(promote[0],0);
    store_plan[0]=save_failures ? 2 : 3;
    destination_phase=3; restore_destination();
    STORE(cycle_done,2);
    return 0;
}
