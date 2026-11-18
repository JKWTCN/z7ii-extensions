/* SPDX-License-Identifier: MIT */
/* C 1.70 JPEG components -> separately owned native pool allocations.
 * Included only by the independent multi-frame experiment. No borrowed image
 * pointer survives its native output callback / slave acknowledgement.
 */
#define RING_SLOTS 12u
#define RING_BYTES (128u*1024u*1024u)
#define RING_FULL_BYTES (Z72_PRE_SEQUENCE ? 192u*1024u*1024u : RING_BYTES)
#define BURST_FRAMES 8u
#define BATCH_JOBS 16u
struct cached_jpeg {
    z72_u32 lease,address,capacity,size,ready_tick,generation,order,state;
};
struct capture_job {
    z72_u32 request,token,slot,context,order,claimed;
    z72_u32 observed_tick,observed_control;
};
static struct cached_jpeg ring[RING_SLOTS];
static struct capture_job jobs[BATCH_JOBS];
static z72_u32 promote_kind[RING_SLOTS];
static z72_u32 promote_exposure[RING_SLOTS],promote_max_capacity;
static z72_u32 job_exposure[BATCH_JOBS];
/* Pre evictions, late pre before base, unused reservations canceled,
 * exposure mismatch, mapping overflow, reservations, reordered writes, reserved. */
static z72_u32 sequence_stats[8];
static volatile z72_u32 ring_busy,ring_count,ring_bytes,ring_peak,ring_drops;
static volatile z72_u32 batch_expected,batch_seen,batch_created,job_sequence;
static z72_u32 batch_calls,pool_failures,copy_failures,map_failures,cache_saves;
static z72_u32 expired_frames,busy_drops,window_limited;
static z72_u32 duration_index=1,cycle_pre_saves,cycle_first_file,cycle_last_file;
static z72_u32 save_failures,quarantined_bytes;
static volatile z72_u32 pre_failed;
/* Last saved cycle: count, first/last observation, trigger; cumulative late
 * publications, post-trigger exclusions, window exclusions; saved window. */
static z72_u32 window_stats[8];
static const z72_u32 durations[]={300,500,700,1000};
static const char *const time_labels[]={"Pre-capture time: 0.3 s","Pre-capture time: 0.5 s",
    "Pre-capture time: 0.7 s","Pre-capture time: 1 s"};
static _Alignas(64) unsigned char ring_zeros[512],ring_signature[64];

/* These compare/exchanges are also used by the file task, peer receiver and
 * worker. Never spin on another task: a competing output is discarded with a
 * normal reply, not allowed to stall the native encoder or an input callback.
 */
static z72_u32 ring_enter(void)
{
    z72_u32 empty=0;
    return __atomic_compare_exchange_n(&ring_busy,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void ring_leave(void) { STORE(ring_busy,0); }
static void ring_release_one(z72_u32 i)
{
    struct cached_jpeg *f=&ring[i];
    if (!f->lease || f->state==3 || (Z72_PRE_PROMOTE && f->state==4)) return;
    /* A failed DMA is quarantined; a live writer owns state4 until close. */
    CALL(0x10298c61u,z72_u32,z72_u32,z72_u32)(f->lease,0xf);
    ring_bytes-=f->capacity;
    if (f->state==2) --ring_count;
    for (z72_u32 j=0;j<sizeof(*f)/4;++j) ((volatile z72_u32 *)f)[j]=0;
    promote_kind[i]=0;
}
static void ring_release(z72_u32 i)
{
    ring_release_one(i);
}
static z72_u32 ring_oldest(void)
{
    z72_u32 oldest=RING_SLOTS;
    for (z72_u32 i=0;i<RING_SLOTS;++i) if (ring[i].state==2 &&
        (oldest==RING_SLOTS || ring[i].order<ring[oldest].order)) oldest=i;
    return oldest;
}
static void ring_expire(z72_u32 now,z72_u32 generation)
{
    for (z72_u32 i=0;i<RING_SLOTS;++i) if (ring[i].state==2 &&
        !(promote_kind[i] && (LOAD(control)&FROZEN) && ring[i].generation==generation) &&
        (ring[i].generation!=generation || now-ring[i].ready_tick>durations[duration_index])) {
        ring_release(i); ++expired_frames;
    }
}
static void ring_maintain(void)
{
    if (!LOAD(enabled) || LOAD(fallback[3]) || LOAD(cycle_done) || !ring_enter()) return;
    /* Sample time under the copy lock, before observing freeze. Newer JPEGs
     * cannot have a future ready_tick; a full press after this sample can only
     * advance the cutoff. Already frozen frames keep their trigger window.
     * Never wait for the next JPEG to return expired pool leases.
     */
    z72_u32 now=tick(),current=LOAD(control);
    if (!(current&FROZEN)) ring_expire(now,current&~7u);
    ring_leave();
}
static z72_u32 ring_reserve(z72_u32 bytes)
{
    z72_u32 slot=RING_SLOTS,budget=RING_BYTES;
    for (z72_u32 tries=0;tries<=RING_SLOTS;++tries) {
        slot=RING_SLOTS;
        for (z72_u32 i=0;i<RING_SLOTS;++i) if (!ring[i].lease) { slot=i; break; }
        /* Locked native availability query. Leave capacity for at least two
         * further chunks of this size; allocation may still legitimately fail.
         * Never write a pool limit, busy mask or a private pixel address.
         */
        budget=RING_BYTES;
        z72_u32 needed=bytes;
        /* Charge known pool capacity, not just the compressed JPEG length.
         * The native lease can be much larger than this frame's contents. */
        if (promote_max_capacity>needed) needed=promote_max_capacity;
        if (LOAD(control)&FROZEN)
            budget=RING_FULL_BYTES;
        else if (promote_max_capacity && promote_max_capacity<RING_BYTES/2)
            budget-=promote_max_capacity;
        if (needed<=budget && slot<RING_SLOTS && ring_bytes<=budget-needed &&
            CALL(0x10298d15u,z72_u32,z72_u32,z72_u32)(0xf,bytes)>=3) break;
        z72_u32 old=ring_oldest();
        if (LOAD(control)&FROZEN) {
            old=RING_SLOTS;
            for (z72_u32 i=0;i<RING_SLOTS;++i) if (ring[i].state==2 && !promote_kind[i] &&
                (old==RING_SLOTS || ring[i].order<ring[old].order)) old=i;
            if (old==RING_SLOTS) { ++pool_failures; return RING_SLOTS; }
            ++sequence_stats[0];
        } else
        if (old==RING_SLOTS || (LOAD(control)&FROZEN)) { ++pool_failures; return RING_SLOTS; }
        ring_release(old); ++ring_drops; ++window_limited;
    }
    if (slot==RING_SLOTS) return RING_SLOTS;
    struct cached_jpeg *f=&ring[slot];
    z72_u32 lease=CALL(0x10298bebu,z72_u32,z72_u32,z72_u32,z72_u32)((z72_u32)f,0xf,bytes);
    if (!lease) { ++pool_failures; return RING_SLOTS; }
    z72_u32 node=pre_capture_ram(lease,8) ? WORD(lease+4) : 0;
    if (!pre_capture_ram(node,12) || WORD(node+8)!=lease || !WORD(node) || WORD(node+4)<bytes ||
        WORD(node+4)>budget-ring_bytes || WORD(node)>0xffffffffu-WORD(node+4)) {
        /* The acquired descriptor is owned even on an unexpected layout. Use
         * its native destructor; never attempt to interpret/free pixel memory.
         */
        CALL(0x10298c61u,z72_u32,z72_u32,z72_u32)(lease,0xf);
        ++pool_failures; return RING_SLOTS;
    }
    f->lease=lease; f->address=WORD(node); f->capacity=WORD(node+4); f->state=1;
    if (f->capacity>promote_max_capacity) promote_max_capacity=f->capacity;
    ring_bytes+=f->capacity;
    if (ring_bytes>ring_peak) ring_peak=ring_bytes;
    return slot;
}

/* Same DMA wrapper and foreign mapping as the P: writer (105bf0da), with
 * checked return values. Serialize the mapping until DMA completes, using the
 * original FS mapping mutex. A peer address is never read with the master CPU.
 */
static z72_u32 ring_copy_bytes(z72_u32 dest,z72_u32 source,z72_u32 bytes,z72_u32 foreign)
{
    if (!bytes) return 0;
    CALL(0x103d5919u,void,z72_u32)(0xb1808a78u);
    z72_u32 mapped=foreign ? CALL(0x1038848bu,z72_u32,z72_u32,z72_u32,z72_u32)(3,source,bytes) : source;
    z72_u32 result=3;
    if (mapped==0xffffffffu) ++map_failures;
    else result=CALL(0x105bc399u,z72_u32,z72_u32,z72_u32,z72_u32)(dest,mapped,bytes);
    CALL(0x103d58fbu,void,z72_u32)(0xb1808a78u);
    return result;
}
static z72_u32 ring_copy_jpeg(z72_u32 record,z72_u32 mode,const struct capture_job *job)
{
    z72_u32 current=LOAD(control);
    z72_u32 observed=job->observed_tick,order=job->order;
    if (!LOAD(enabled) || (!(current&FROZEN) && !LOAD(half)) ||
        (current&~7u)!=(frame_control&~7u) ||
        (current&~7u)!=(job->observed_control&~7u) ||
        !__atomic_compare_exchange_n(&control,&current,current,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return 0;
    /* A verified image-pipeline notice, not SDK submission, admits a job.
     * Later JPEG delivery/release may drain a pre-trigger job after full press.
     * The control stamp excludes post-freeze notices even in the same ms.
     */
    z72_u32 post=(job->observed_control&FROZEN)!=0;
    if (post && !Z72_PRE_PROMOTE) { ++window_stats[5]; return 0; }
    if (!post && (current&FROZEN) && LOAD(trigger_tick)-observed>durations[duration_index]) {
        ++window_stats[6]; return 0;
    }
    if (!ring_enter()) {
        ++busy_drops; ++ring_drops; ++window_limited;
        if (post) {
            __atomic_fetch_add(&promote[10],1,__ATOMIC_RELAXED);
        }
        return 0;
    }
    z72_u32 total=WORD(record+0x18),parts[6][4],sum=0,n=0,result=0;
    if (WORD(record+0x1c) || total<8 || total>RING_BYTES-511u || WORD(record+0x34)>1) {
        result=3;
    }
    z72_u32 capture_context=WORD(record+8);
    z72_u32 job_index=(z72_u32)(job-jobs);
    if (job_index>=BATCH_JOBS || (!pre_capture_ram(capture_context,0x3400) &&
         !(record==(z72_u32)peer_record && capture_context==(z72_u32)peer_context)) ||
        WORD(capture_context+0x2858u)!=job_exposure[job_index]) {
        result=3;
    }
    for (z72_u32 i=0;!result && i<((Z72_PRE_QUALITY && mode==1) ? 4u : 6u);++i) {
        z72_u32 component=CALL(0x104ef019u,z72_u32,z72_u32,z72_u32)(mode,i);
        z72_u32 r=CALL(0x104eefc5u,z72_u32,z72_u32,z72_u32,z72_u32,void *)(mode,record,component,parts[n]);
        if (r==1) continue;
        if (r || !parts[n][0] || parts[n][1]<parts[n][2] || parts[n][2]>sizeof(ring_zeros)) {
            result=3; break;
        }
        /* 104ef81e loads {total,padding} from descriptor+4; 104ef82a
         * subtracts them. +0xc is metadata/capacity for the alternate object
         * writer, not the amount to subtract in the file assembler.
         */
        z72_u32 bytes=parts[n][1]-parts[n][2];
        if (bytes>total-sum || parts[n][2]>total-sum-bytes || parts[n][0]>0xffffffffu-bytes) {
            result=3; break;
        }
        sum+=bytes+parts[n][2]; ++n;
    }
    if (!result && sum!=total) {
        result=3;
    }
    z72_u32 slot=RING_SLOTS;
    if (!result) {
        /* Another output may have finished while this task was preempted
         * before ring_enter. Use current time to expire those newer entries.
         */
        ring_expire((LOAD(control)&FROZEN) ? LOAD(trigger_tick) : tick(),current&~7u);
        slot=ring_reserve((total+511u)&~511u);
        if (slot==RING_SLOTS) {
            ++ring_drops; ++window_limited;
            if (current&FROZEN) {
                if (post) __atomic_fetch_add(&promote[10],1,__ATOMIC_RELAXED);
            }
        }
    }
    if (!result && slot<RING_SLOTS) {
        struct cached_jpeg *f=&ring[slot];
        z72_u32 offset=0;
        CALL(0x1034f9edu,void,const void *,z72_u32)(ring_zeros,sizeof(ring_zeros));
        for (z72_u32 i=0;i<n && !result;++i) {
            z72_u32 bytes=parts[i][1]-parts[i][2],done=0;
            while (done<bytes && !result) {
                z72_u32 count=bytes-done;
                if (count>0x100000u) count=0x100000u;
                result=ring_copy_bytes(f->address+offset,parts[i][0]+done,count,WORD(record+0x34));
                done+=count; offset+=count;
            }
            if (!result && parts[i][2]) {
                result=ring_copy_bytes(f->address+offset,(z72_u32)ring_zeros,parts[i][2],0);
                offset+=parts[i][2];
            }
        }
        if (!result) {
            CALL(0x1034fa29u,void,void *,z72_u32)(ring_signature,sizeof(ring_signature));
            result=ring_copy_bytes((z72_u32)ring_signature,f->address,8,0);
            CALL(0x1034fa29u,void,void *,z72_u32)(ring_signature,sizeof(ring_signature));
            if (!result && (
                ring_signature[0]!=0xff || ring_signature[1]!=0xd8)) {
                result=3;
            }
        }
        if (result) {
            /* A native DMA timeout has no proven cancellation guarantee. Do
             * not return this destination block to a pool for later reuse.
             */
            f->state=3; quarantined_bytes+=f->capacity; ++copy_failures;
        } else {
            f->size=total; f->ready_tick=observed; f->generation=current&~7u;
            f->order=order;
            promote_exposure[slot]=WORD(WORD(record+8)+0x2858);
            promote_kind[slot]=post;
            if (post) {
                z72_u32 empty=0;
                __atomic_compare_exchange_n(&promote[3],&empty,observed,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
            }
            f->state=2; ++ring_count;
            if (current&FROZEN) ++window_stats[4];
        }
    }
    ring_leave(); return result;
}

static void batch_track(z72_u32 event,z72_u32 request)
{
    if (fallback[7]!=1 || (event!=0x041b0001u && event!=0x041b0090u && event!=0x041b0095u &&
        !(Z72_PRE_CONTINUOUS && !event)) ||
        !pre_capture_ram(request,0x4bdc)) return;
    z72_u32 owner=WORD(request+0xc),context=pre_capture_ram(owner,0x4f8) ? WORD(owner+0x4f4) : 0;
    if (!pre_capture_ram(context,0x3400) || (BYTE(context+0x27fa)!=1 && BYTE(context+0x27fa)!=2)) return;
    z72_u32 token=WORD(request+0x4bd8),slot=WORD(request+0x4bc8);
    for (z72_u32 i=0;i<BATCH_JOBS;++i) if (LOAD(jobs[i].request)==request &&
        LOAD(jobs[i].claimed)==2 && jobs[i].token==token && jobs[i].slot==slot && jobs[i].context==context)
    {
        if (event) return; /* A repeated notice is not a new output lifetime. */
        /* Only the validated local writer may reopen a completed identity.
         * Native address/token/sequence/context values can all be recycled;
         * its live one-JPEG record is the ownership proof, not these scalars.
         * A duplicate completed writer has count zero and cannot reach here.
         */
        break;
    }
    for (z72_u32 i=0;i<BATCH_JOBS;++i) if (LOAD(jobs[i].request)==request && LOAD(jobs[i].claimed)!=2) {
        /* The early 0001 notification may precede wrapper-token setup. Keep
         * the original ordering stamp, then refine identity before export.
         */
        if (!LOAD(jobs[i].claimed)) { jobs[i].token=token; jobs[i].slot=slot; jobs[i].context=context; }
        return;
    }
    /* Keep completed identities until the next source needs a retired slot.
     * Reserve atomically so preempting master/peer notifications cannot publish
     * two different jobs into the same partially initialized entry.
     */
    for (z72_u32 attempt=0;attempt<=Z72_PRE_CONTINUOUS;++attempt)
    for (z72_u32 i=0;i<BATCH_JOBS;++i) {
        z72_u32 empty=0;
        if (attempt) {
            empty=LOAD(jobs[i].request);
            if (!pre_capture_ram(empty,0x4bdc) || !stream_reusable(i)) continue;
        }
        if (!__atomic_compare_exchange_n(&jobs[i].request,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) continue;
        if (attempt) stream_recycled(i);
        jobs[i].token=token; jobs[i].slot=slot; jobs[i].context=context;
        /* This is an already validated local capture context. Never follow
         * the remote slave pointer from an IPC payload on this processor. */
        job_exposure[i]=WORD(context+0x2858u);
        jobs[i].order=__atomic_add_fetch(&job_sequence,1,__ATOMIC_RELAXED); jobs[i].claimed=0;
        z72_u32 stamp=LOAD(control);
        jobs[i].observed_tick=tick();
        if (!__atomic_compare_exchange_n(&control,&stamp,stamp,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) stamp|=FROZEN;
        jobs[i].observed_control=stamp;
        if (stamp&FROZEN) {
            z72_u32 empty=0;
            __atomic_compare_exchange_n(&promote[3],&empty,jobs[i].observed_tick,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
        }
        STORE(jobs[i].request,request);
        __atomic_fetch_add(&batch_created,1,__ATOMIC_RELEASE);
        return;
    }
    __atomic_fetch_or(&stream[3],0x40,__ATOMIC_RELAXED);
}
static z72_u32 batch_claim(z72_u32 request)
{
    for (z72_u32 i=0;i<BATCH_JOBS;++i) if (LOAD(jobs[i].request)==request) {
        z72_u32 empty=0;
        if (__atomic_compare_exchange_n(&jobs[i].claimed,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            return i+1;
        }
    }
    return 0;
}
