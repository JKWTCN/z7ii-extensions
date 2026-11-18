/* SPDX-License-Identifier: MIT */
/* Native context+2858 / Nikon00a7 orders exposures across processors.
 * Missing sequence values reserve native gaps, canceled only if unopened. */
#define EXPOSURE_IDS 256u
static z72_u32 exposure_base,exposure_count,exposure_has_base;
static z72_u32 exposure_ids[EXPOSURE_IDS],exposure_used[EXPOSURE_IDS];
static struct photo_stamp exposure_stamps[EXPOSURE_IDS];

static void exposure_reset(void)
{
    exposure_base=exposure_count=exposure_has_base=0;
    for (z72_u32 i=0;i<EXPOSURE_IDS;++i) exposure_ids[i]=exposure_used[i]=0;
    for (z72_u32 i=0;i<8;++i) sequence_stats[i]=0;
}
static z72_u32 exposure_seed(void)
{
    if (exposure_has_base) return 1;
    if (!ring_enter()) return 0;
    z72_u32 generation=LOAD(control)&~7u,cutoff=LOAD(trigger_tick);
    for (z72_u32 i=0;i<RING_SLOTS;++i) if (ring[i].state==2 && !promote_kind[i] &&
        ring[i].generation==generation && cutoff-ring[i].ready_tick<=durations[duration_index] &&
        (!exposure_has_base || (int)(promote_exposure[i]-exposure_base)<0)) {
        exposure_base=promote_exposure[i]; exposure_has_base=1;
    }
    ring_leave();
    /* Snapshot scalars published with the verified native job identity.
     * A constructing/reused entry must never seed a partial exposure count. */
    for (z72_u32 i=0;i<BATCH_JOBS;++i) {
        z72_u32 request=LOAD(jobs[i].request),sequence=job_exposure[i];
        if (request==1) { exposure_has_base=0; return 0; }
        if (!request || LOAD(jobs[i].claimed)==2) continue;
        z72_u32 observed=jobs[i].observed_control,when=jobs[i].observed_tick;
        if (request!=LOAD(jobs[i].request) || sequence!=job_exposure[i] ||
            observed!=jobs[i].observed_control || when!=jobs[i].observed_tick) {
            exposure_has_base=0; return 0;
        }
        if ((observed&FROZEN) || (observed&~7u)!=generation ||
            cutoff-when>durations[duration_index]) continue;
        if (!exposure_has_base || (int)(sequence-exposure_base)<0) {
            exposure_base=sequence; exposure_has_base=1;
        }
    }
    /* With no pre-trigger source there is no safe live post lower bound.
     * Wait for true native/output drain, then choose among all owned copies.
     * Never silently discard an earlier post exposure arriving from the peer. */
    if (!exposure_has_base && LOAD(stream[0])==3 &&
        LOAD(batch_created)==LOAD(stream[2]) && LOAD(batch_seen)==LOAD(stream[2]) &&
        !LOAD(peer_active) && !LOAD(gate_active) && ring_enter()) {
        for (z72_u32 i=0;i<RING_SLOTS;++i) if (ring[i].state==2 &&
            (!exposure_has_base || (int)(promote_exposure[i]-exposure_base)<0)) {
            exposure_base=promote_exposure[i]; exposure_has_base=1;
        }
        ring_leave();
    }
    return exposure_has_base;
}
static z72_u32 exposure_stamp_before(const struct photo_stamp *a,const struct photo_stamp *b)
{
    return a->date<b->date || (a->date==b->date &&
        (a->clock<b->clock || (a->clock==b->clock && a->fraction<b->fraction)));
}
/*2: an older pre frame arriving before the already frozen prefix. */
static z72_u32 exposure_index(z72_u32 slot,z72_u32 *index)
{
    const struct photo_stamp *stamp=&photo_stamps[slot];
    if (!stamp->exposure_valid || stamp->exposure!=promote_exposure[slot]) {
        ++sequence_stats[3]; return 0;
    }
    if (!exposure_has_base) { exposure_base=stamp->exposure; exposure_has_base=1; }
    z72_u32 at=stamp->exposure-exposure_base;
    if ((int)at<0) {
        if (promote_kind[slot]) { ++sequence_stats[3]; return 0; }
        ++sequence_stats[1]; return 2;
    }
    if (at>=EXPOSURE_IDS) { ++sequence_stats[4]; return 0; }
    if (exposure_used[at]) { ++sequence_stats[3]; return 0; }
    for (z72_u32 i=0;i<exposure_count;++i) if (exposure_used[i]==2) {
        if ((i<at && exposure_stamp_before(stamp,&exposure_stamps[i])) ||
            (i>at && exposure_stamp_before(&exposure_stamps[i],stamp))) {
            ++sequence_stats[3]; return 0;
        }
    }
    *index=at; return 1;
}
static z72_u32 exposure_reserve(z72_u32 slot,z72_u32 at)
{
    const struct photo_stamp *stamp=&photo_stamps[slot];
    z72_u32 date[2]={0};
    CALL(0x10509aa7u,void,void *,z72_u32,z72_u32,z72_u32)(date,
        stamp->date/10000,(stamp->date/100)%100,stamp->date%100);
    CALL(0x10509ad3u,void,void *,z72_u32,z72_u32,z72_u32)(date,
        stamp->clock/3600,(stamp->clock/60)%60,stamp->clock%60);
    ((unsigned char *)date)[4]=(unsigned char)((stamp->clock&1u)*100u+stamp->fraction/10000000u);
    while (exposure_count<=at) {
        z72_u32 flags=0,ids[18]={0};
        _Alignas(8) z72_u32 plan[10]={1}; plan[4]=ring[slot].size;
        ++store_stats[2]; store_stats[9]=2;
        store_stats[3]=CALL(0x10507871u,z72_u32,z72_u32,void *,void *,void *,void *)(1,&flags,date,plan,ids);
        store_plan[6]=store_stats[3];
        if (store_stats[3]!=SDK_OK || ids[0]!=1 || ids[1]!=1 || (ids[2]&15u) ||
            !CALL(0x105094e5u,z72_u32,z72_u32)(ids[2])) return 0;
        z72_u32 previous=exposure_count ? exposure_ids[exposure_count-1] : 0;
        for (z72_u32 i=0;i<exposure_count;++i) if (exposure_ids[i]==ids[2]) return 0;
        /* Journal a successful new reservation before policy checks, so a
         * media/directory transition can cancel its unopened catalog entry. */
        exposure_ids[exposure_count++]=ids[2]; ++sequence_stats[5];
        if (previous && ((previous>>29)!=(ids[2]>>29) ||
            (ids[2]&0x1fffffe0u)<=(previous&0x1fffffe0u))) return 0;
    }
    store_ids[slot]=exposure_ids[at]; ++store_plan[1]; store_plan[0]=1;
    store_plan[5]=store_ids[slot]>>29; store_stats[4]=store_ids[slot];
    exposure_used[at]=1; return 1; /* Never cancel an attempted open/write. */
}
static void exposure_cancel_unused(void)
{
    for (z72_u32 i=0;i<exposure_count;++i) if (exposure_ids[i] && !exposure_used[i]) {
        /* Native105066f4 rollback uses this catalog-removal backend.
         * Void ABI; no file unlink, no number rollback, no user's file IDs. */
        CALL(0x105070fdu,void,z72_u32)(exposure_ids[i]);
        exposure_used[i]=3; ++sequence_stats[2];
    }
}
static void exposure_sort_notices(void)
{
    for (z72_u32 i=promote_delivered+1;i<promote_queued;++i) {
        z72_u32 at=i;
        while (at>promote_delivered && (promote_notices[at][0]&0x1fffffffu)<
            (promote_notices[at-1][0]&0x1fffffffu)) {
            for (z72_u32 j=0;j<5;++j) {
                z72_u32 tmp=promote_notices[at][j];
                promote_notices[at][j]=promote_notices[at-1][j]; promote_notices[at-1][j]=tmp;
            }
            --at;
        }
    }
}
