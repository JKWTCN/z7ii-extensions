/* SPDX-License-Identifier: MIT */
/* Nikon Z 7II C 1.70 JPEG pre-capture. Included by the combined module. */
#define Z72_PRE_CONTINUOUS 1
#define Z72_PRE_QUALITY 0
#define Z72_PRE_JPEG_FIXED 1
#define Z72_PRE_PROMOTE 1
#define Z72_PRE_SEQUENCE 1
#define Z72_PRE_JPEG_MENU 1
#define Z72_PRE_NATIVE_CARD 0
#define Z72_PRE_CARD_STREAM 0

static z72_u32 jpeg_start_mode=2;
/* Active, stop reasons, full tick, first post notice/copy/save ticks,
 * pre/post saved, last saved ID, peak queue, post drops, files saved,
 * order fault, earlier-job waits, last/max write ms. */
static volatile z72_u32 promote[16];
#define STREAM_SLOT 0xb18695b8u
#define STREAM_NATIVE 0x103b9849u
/* State0 idle/1 running/2 stop requested/3 native ended; total capture units,
 * final expected outputs, fault bits, starts, stops, terminal events,
 * last progress, stop tick, record reuses, peak registered backlog, end tick,
 * validated local writers reopening a completed borrowed identity. */
static volatile z72_u32 stream[13];

#define LOAD(p) __atomic_load_n(&(p),__ATOMIC_ACQUIRE)
#define STORE(p,v) __atomic_store_n(&(p),(v),__ATOMIC_RELEASE)

/* EABI helper for constant-initialized, aligned native request buffers. */
void __aeabi_memclr8(void *pointer,z72_u32 size)
{
    volatile unsigned char *out=pointer;
    for (z72_u32 i=0;i<size;++i) out[i]=0;
}
#define INPUT_SLOT 0xb23f7df8u
#define INPUT_NATIVE 0x10228c95u
#define INPUT_CALLBACKS 0xb23f7d94u
#define FILE_SLOT 0xb23af288u
#define FILE_NATIVE 0x102c0e07u
#define CARD_SLOT 0xb23af25cu
#define CARD_NATIVE 0x102c109fu
#define FILE_STATE 0xb1864df8u
#define FILE_NOTIFY_NATIVE 0x103cd163u
#define PEER_SLOT 0xb290cc30u
#define PEER_NATIVE 0x104ee991u
#define CARD_PEER_SLOT 0xb29005f0u
#define CARD_PEER_NATIVE 0x104eea89u
#define IMAGE_SLOT 0xb1888a9cu
#define IMAGE_NATIVE 0x106d2f6du
#define PEER_FIELDS 24u
#define FALLBACK_FIELDS 8u
#define SHOT_FIELDS 8u
#define CARD_FIELDS 8u
#define READY_FIELDS 4u
#define CARD_SHOT 3u
#define CARD_BURST 8u
#define CARD_RECORDS (Z72_PRE_CARD_STREAM ? 256u : CARD_BURST)
#define GUI_SLOT 0xb1821b38u
#define GUI_NATIVE 0x104e6cb5u
#define GUI_CAPTURE_SLOT 0xb1821b5cu
#define GUI_CAPTURE_OTHER_SLOT 0xb1821b80u
#define GUI_CAPTURE_NATIVE 0x104e7177u
#define REVIEW_SLOT 0xb1821b34u
#define REVIEW_NATIVE 0x104e6ee9u
#define FROZEN 4u
#define SDK_OK 0x043a0080u
#define SDK_MISSING 0x043a0084u
#define CHOICE_PAGE 0xb2bb720cu
#define SETUP_PAGE 0xb2bbc6c8u
#define CHOICE_EVENTS 16u

static z72_u32 task_control[8];
static _Alignas(4) unsigned char task_descriptor[28];
static volatile z72_u32 enabled,half,half_latch,triggered,full_latch,control;
static volatile z72_u32 half_forwarded,half_consumed;
/* Last SW1 level, non-SW1 half events, in-flight generation holds,
 * owned outputs seen with native control flags, last such flag mask. */
static volatile z72_u32 enable_request; /* 0 none, 1 OFF, 2 ON. */
static volatile z72_u32 enable_servicing;
static volatile z72_u32 in_flight,file_done;
static volatile z72_u32 trigger_tick;
static volatile z72_u32 cycle_done,owned;
/* A release ends the physical press even if another down arrives while files
 * are draining. Keep that edge separate from the current switch level. */
static volatile z72_u32 full_released;
static z72_u32 continuation,cycle_card_count,generation_sequence;
/* Scalar identities only: native writers retain all image/job ownership. */
static volatile z72_u32 card_expected,card_finished,card_tracked;
static z72_u32 card_jobs[CARD_RECORDS][5]; /* request, owner, context, ID, 1 ready / 2 done */
/* 1: local writer join; 2: native peer writer's successful close notice. */
static volatile z72_u32 card_routes[CARD_RECORDS];
static z72_u32 card_close_stats[4];
static z72_u32 card_images[CARD_RECORDS][5];
static volatile z72_u32 card_image_count;
static z72_u32 card_burst_stats[4];
static z72_u32 gui_capture(z72_u32,z72_u32,z72_u32,z72_u32);
static volatile z72_u32 release_pending;
/* Kind 1: parsed native review RPC; kind 2: native GUI event 6.
 * Only scalar display selection is retained, never a borrowed RPC pointer. */
static volatile z72_u32 review_kind,review_ready,review_value;
static volatile z72_u32 fallback[FALLBACK_FIELDS];
static volatile z72_u32 shot[SHOT_FIELDS];
static volatile z72_u32 card[CARD_FIELDS];
/* Source-ready stamp, observation tick, delayed-close saves, rejected candidates. */
static volatile z72_u32 readiness[READY_FIELDS];

static z72_u32 task_result,backend_result,io_result,close_result,last_id;
static z72_u32 frame_control,frame_slot,frame_tick,session_control;
static z72_u32 native_request,native_id,native_size,file_calls,saved_id;
static z72_u32 waiting_since,last_attempt;
static z72_u32 admission[6],admission_tick,admission_waits,worker_steps;
static z72_u32 file_observed[11];
static z72_u32 file_format=0xffffffffu;
/* Destination phases: 0 none, 1 settling, 2 submitted/ready, 3 restore. */
static z72_u32 destination_before,destination_value,destination_phase;
static z72_u32 destination_target;
static z72_u32 destination_set_result,destination_restore_result,destination_tick,destination_waits;
static volatile z72_u32 peer[PEER_FIELDS],peer_active;
/* Retired hint, RAM match source (3 native permission), metadata invalidations,
 * shutter events held while draining, and timed-out batch count. */
static z72_u32 peer_recovery[5];
static z72_u32 drain_timed_out;
/* Native unpacker copies rounded wire/context allocations of 0x180/0x3400. */
static _Alignas(64) unsigned char peer_wire[0x180],peer_context[0x3400];
static _Alignas(64) z72_u32 peer_record[14],peer_headers[22],peer_extra[9];
static char destination[128];

static z72_u32 tick(void) { return CALL(0x1062e59du,z72_u32,void)(); }
static void delay(z72_u32 ms) { CALL(0x103dc539u,void,z72_u32)(ms); }
static z72_u32 pre_capture_ram(z72_u32 p,z72_u32 n)
{
    return !(p&3u) && n<=0x4700000u && p>=0xb1800000u && p<=0xb5f00000u-n;
}
/* Display snapshot +1a6/+1a7 belongs to OSD redraw/visibility. Native
 * 105535a8/105536c2 deliberately invert +1a6 in the comparison snapshot.
 * Keep these exclusions for a new press; an already owned continuation uses
 * the photo context and the native SDK admission checks instead. */
static z72_u32 view_blockers(void)
{
    z72_u32 output=WORD(0xb181d3a4u)&7u,scene=BYTE(0xb181da91u);
    z72_u32 m=BYTE(0xb1824f0cu),p=BYTE(0xb1828174u);
    return (!((output==1 && scene==0) || (output==4 && scene==2))) |
        (!!BYTE(0xb1821b18u)<<1) | (!!WORD(0xb25519b8u)<<2) |
        ((m>=1 && m<=3)<<3) | ((p>=1 && p<=3)<<4) |
        (!!BYTE(0xb202c016u)<<5) | (!!BYTE(0xb202c017u)<<6);
}
static z72_u32 photo_view(void) { return !view_blockers(); }
static z72_u32 continue_held(void)
{
    return !LOAD(full_released) && LOAD(full_latch);
}
static z72_u32 capture_view(z72_u32 current,z72_u32 normal)
{
    z72_u32 mask=view_blockers();
    z72_u32 continuing=normal && continuation && cycle_card_count &&
        (current&FROZEN) && (current&~7u)==session_control &&
        (Z72_PRE_NATIVE_CARD || (LOAD(half_latch) && LOAD(half))) && continue_held();
    if (!(mask&(continuing ? 0x1fu : 0x7fu))) return 1;
    return 0;
}
#include "pre_capture_af_sdk.h"

static z72_u32 capture_allowed(void)
{
    /* Both SDK request formats are explicitly JPEG; user quality is ignored. */
    return CALL(0x10496b8bu,z72_u32,void)()==1 &&
        CALL(0x1063c7d5u,z72_u32,z72_u32)(0x057a)==1 &&
        CALL(0x104d1e8bu,z72_u32,void)()==0x8019;
}

static z72_u32 stream_reusable(z72_u32 i);
static void stream_recycled(z72_u32 i);
#include "pre_capture_ring.h"
static volatile z72_u32 pre_notice_id,pre_notice_pending,pre_notice_payload[5];
static z72_u32 promote_closed(z72_u32 id);
/* Native metadata requests/success/result and pre notices held/delivered. */
static z72_u32 metadata_stats[5];
static void finish_pre_notice(void);
#include "pre_capture_store.h"

/* Keep the real formal-photo completion payload until the earlier reserved
 * photos have completed. Native 106d2f6c -> 106d2ebe updates the current
 * playback/catalog view; publishing older IDs after the formal ID regresses
 * that view. Never invent/replay a completion or retain its borrowed pointer.
 */
static volatile z72_u32 image_pending,image_payload[5];
static void finish_card(z72_u32 result);
static z72_u32 formal_image_id(z72_u32 id)
{
    if (id==native_card_id && id) return 1;
    for (z72_u32 i=0;i<CARD_RECORDS;++i) if (LOAD(card_jobs[i][3])==id && id) return 1;
    return 0;
}
static void image_notice(z72_u32 event,z72_u32 argument)
{
    if (event==0x041f0002u && LOAD(pre_notice_id) && pre_capture_ram(argument,20) &&
        WORD(argument)==LOAD(pre_notice_id)) {
        z72_u32 empty=0;
        if (__atomic_compare_exchange_n(&pre_notice_pending,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            for (z72_u32 i=0;i<5;++i) pre_notice_payload[i]=WORD(argument+4*i);
            ++metadata_stats[3]; STORE(pre_notice_pending,2); return;
        }
    }
    if (event==0x041f0002u && LOAD(enabled) && LOAD(in_flight) &&
        fallback[7]==CARD_SHOT && pre_capture_ram(argument,20) && formal_image_id(WORD(argument))) {
        z72_u32 pending=LOAD(image_pending);
        if ((pending==0 || pending==2) &&
            __atomic_compare_exchange_n(&image_pending,&pending,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            for (z72_u32 i=0;i<card_image_count;++i) if (card_images[i][0]==WORD(argument)) {
                STORE(image_pending,2); return;
            }
            if (card_image_count>=CARD_RECORDS) {
                ++card_burst_stats[3]; STORE(image_pending,2);
                CALL(IMAGE_NATIVE,void,z72_u32,z72_u32)(event,argument); return;
            }
            for (z72_u32 i=0;i<5;++i) {
                card_images[card_image_count][i]=WORD(argument+4*i);
                if (!card_image_count) image_payload[i]=WORD(argument+4*i);
            }
            ++card_image_count;
            /* Native 104ef466 emits this only after successful close/flush,
             * before the peer ACK. Peer photos have no local 0030/0031 join.
             * Match an ID recorded from live native metadata, never infer it
             * from neighboring filenames or the last peer diagnostic slot. */
            for (z72_u32 i=0;i<CARD_RECORDS;++i) if (LOAD(card_routes[i])==2 &&
                LOAD(card_jobs[i][3])==WORD(argument)) {
                z72_u32 ready=1;
                if (__atomic_compare_exchange_n(&card_jobs[i][4],&ready,2,0,
                        __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
                    ++card_close_stats[1]; finish_card(0);
                }
                break;
            }
            STORE(image_pending,2); return;
        }
    }
    CALL(IMAGE_NATIVE,void,z72_u32,z72_u32)(event,argument);
}
static void finish_pre_notice(void)
{
    z72_u32 pending=2;
    if (__atomic_compare_exchange_n(&pre_notice_pending,&pending,3,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
        CALL(IMAGE_NATIVE,void,z72_u32,const void *)(0x041f0002u,(const void *)pre_notice_payload);
        ++metadata_stats[4]; STORE(pre_notice_pending,0);
    }
    STORE(pre_notice_id,0);
}
static void finish_image_notice(void)
{
    z72_u32 pending=2;
    if (!__atomic_compare_exchange_n(&image_pending,&pending,3,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return;
    /* Writer callbacks may interleave. Publish native DCF IDs in increasing
     * order after the older reserved pre images, never delivery order. */
    for (z72_u32 i=0;i<card_image_count;++i) {
        z72_u32 first=i;
        for (z72_u32 j=i+1;j<card_image_count;++j)
            if ((card_images[j][0]&(Z72_PRE_QUALITY ? 0x1fffffffu : 0x1fffffe0u))<
                (card_images[first][0]&(Z72_PRE_QUALITY ? 0x1fffffffu : 0x1fffffe0u))) first=j;
        if (first!=i) for (z72_u32 j=0;j<5;++j) {
            z72_u32 tmp=card_images[i][j]; card_images[i][j]=card_images[first][j]; card_images[first][j]=tmp;
        }
        CALL(IMAGE_NATIVE,void,z72_u32,const void *)(0x041f0002u,(const void *)card_images[i]);
    }
    STORE(image_pending,0);
}

/* Preserve real physical up edges swallowed while the SDK owns capture.
 * Native 10228c94 is a synchronous callback dispatcher with no task-local
 * state. Its full/half-up callbacks only finish the original input lifecycle
 * (aa/1d/1c notifications); no synthetic down/exposure is sent here.
 * Do this after all outputs, pre writes and held completions have drained.
 */
static z72_u32 finish_input_releases(void)
{
    if (!LOAD(release_pending)) return 1;
    if (LOAD(in_flight) || LOAD(ring_busy) || LOAD(peer_active) ||
        LOAD(half) || LOAD(full_latch) || destination_phase ||
        BYTE(0xb1869364u) || BYTE(0xb1869365u) || BYTE(0xb186936bu) ||
        local_control_busy() ||
        WORD(0xb23f7e70u) || WORD(0xb18698ecu) || WORD(0xb18698f0u)) {
        return 0;
    }
    z72_u32 flags=__atomic_exchange_n(&release_pending,0,__ATOMIC_ACQ_REL);
    if (flags) {
        CALL(INPUT_NATIVE,void,z72_u32)(flags);
    }
    return 1;
}


static z72_u32 hold_review(void)
{
    return LOAD(enabled) && (LOAD(half_latch) || (LOAD(control)&FROZEN) ||
        LOAD(in_flight) || LOAD(triggered) || LOAD(cycle_done));
}
static void discard_review(void)
{
    STORE(review_kind,0);
    STORE(review_ready,0);
}
static void review_event(z72_u32 event,z72_u32 value)
{
    if (event==6 && hold_review()) {
        if (!LOAD(review_kind)) STORE(review_kind,2);
        return;
    }
    if (event==6) discard_review(); /* A newer native request supersedes ours. */
    CALL(REVIEW_NATIVE,void,z72_u32,z72_u32)(event,value);
}

static volatile z72_u32 gate_active,gate_fault,gate_ready[BATCH_JOBS];
static z72_u32 stream_reusable(z72_u32 i)
{
    z72_u32 ready=LOAD(gate_ready[i]);
    return LOAD(jobs[i].claimed)==2 && LOAD(gate_active)!=i+1 && ready!=1 && ready!=2;
}
static void stream_recycled(z72_u32 i)
{ STORE(gate_ready[i],0); }
static z72_u32 gate_stats[8];
static z72_u32 gate_notice(z72_u32 request);

static void finish_card(z72_u32 result)
{
    if (result) io_result=result;
    ++card[6];
    z72_u32 finished=__atomic_add_fetch(&card_finished,1,__ATOMIC_ACQ_REL);
    card_burst_stats[2]=finished;
    if (finished>=LOAD(card_expected)) STORE(file_done,1);
}
static z72_u32 card_native_busy(void)
{
    z72_u32 mask=!!BYTE(0xb1869364u) | (!!BYTE(0xb1869365u)<<1) |
        (!!BYTE(0xb186936bu)<<2) | (local_control_busy() ?
        ((!!WORD(0xb18693a8u)<<3) | (!!WORD(0xb18693acu)<<4)) : 0);
    if (mask) { ++card_close_stats[2]; card_close_stats[3]=mask; }
    return mask;
}
static z72_u32 track_card(z72_u32 request,z72_u32 owner,z72_u32 context,z72_u32 id)
{
    if (!id) return CARD_RECORDS;
    for (z72_u32 i=0;i<CARD_RECORDS;++i) {
        z72_u32 empty=0,current=LOAD(card_jobs[i][3]);
        if (current==id) return i;
        if (!current && __atomic_compare_exchange_n(&card_jobs[i][3],&empty,id,0,
                __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            card_jobs[i][0]=request; card_jobs[i][1]=owner; card_jobs[i][2]=context;
            STORE(card_jobs[i][4],1); __atomic_fetch_add(&card_tracked,1,__ATOMIC_RELAXED);
            empty=0; __atomic_compare_exchange_n(&native_card_id,&empty,id,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
            return i;
        }
        if (empty==id) return i; /* Another publisher claimed this identity. */
    }
    ++card_burst_stats[3]; return CARD_RECORDS;
}

/* 0x102c866a emits 0030 before scheduling the native card task; 0x102c86c8
 * clears its request slot after the task signals, then emits 0031. Read only
 * while that synchronous notification owns the request, never after forwarding.
 * A single card record must reach zero with its own writer result equal to 0.
 * request+4bb4 is not a positive success flag: real successful jobs keep it 0.
 */
static z72_u32 card_notice(z72_u32 event,z72_u32 request)
{
    if (fallback[7]!=CARD_SHOT || !LOAD(in_flight) || LOAD(file_done) ||
        !pre_capture_ram(request,0x4bdc)) return 0;
    z72_u32 owner=WORD(request+0xc),context=0;
    if (pre_capture_ram(owner,0x4f8)) context=WORD(owner+0x4f4);
    if (!pre_capture_ram(context,0x27fb)) return 0;
    if ((event==0x041b0090u || event==0x041b0095u) && BYTE(context+0x27fa)==0 &&
        !WORD(request+4) && WORD(owner+0x10)==1 && WORD(owner+0x14)==1) {
        /* Native 102c9604 walks owner+10 groups, group+4 count, group+8
         * IDs. This restricted JPEG-only path accepts exactly one card ID.
         * The peer proxy need not contain a local request+d8 writer record. */
        z72_u32 id=WORD(owner+0x18),format=CALL(0x105094dfu,z72_u32,z72_u32)(id);
        if ((format!=0 && format!=3) || !CALL(0x105094e5u,z72_u32,z72_u32)(id)) return 0;
        z72_u32 slot=track_card(request,owner,context,id);
        if (slot<CARD_RECORDS && !LOAD(card_routes[slot])) {
            STORE(card_routes[slot],2); ++card_close_stats[0];
        }
        return 0;
    }
    if (event==0x041b0030u && BYTE(context+0x27fa)==0 &&
        !WORD(request+4) && WORD(request+0x7c)==1 && !WORD(request+0x360)) {
        /* 102c1006..1048: entry=request+count*b8-38, record=entry+58. */
        z72_u32 slot=track_card(request,owner,context,WORD(request+0xd8));
        if (slot==CARD_RECORDS) return 0;
        STORE(card_routes[slot],1);
        card[1]=owner; card[2]=context; card[3]=1;
        STORE(card[0],request);
    } else if (event==0x041b0031u && BYTE(context+0x27fa)==0) {
        z72_u32 match=CARD_RECORDS;
        for (z72_u32 i=0;i<CARD_RECORDS;++i) if (LOAD(card_jobs[i][4])==1 && card_jobs[i][0]==request &&
            card_jobs[i][1]==owner && card_jobs[i][2]==context) { match=i; break; }
        if (match==CARD_RECORDS) return 0;
        /* For count=1, entry=request+0x80. Native 0x102c1050 stores
         * the writer return byte at entry+b4 before returning its lease.
         */
        card[4]=BYTE(request+0x134);
        z72_u32 result=(WORD(request+4)!=0) | ((WORD(request+0x7c)!=0)<<1) | ((card[4]!=0)<<2);
        card[5]=result;
        if (result&2u) return 0; /* Native join returns 2 and continues this request. */
        z72_u32 ready=1;
        if (!__atomic_compare_exchange_n(&card_jobs[match][4],&ready,2,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return 0;
        card[0]=request; card[1]=owner; card[2]=context;
        card[3]=2;
        return result ? 4 : 1;
    }
    return 0;
}

/* Native 0x103cdbb2 installs this file-pipeline notification callback.
 * Observe only while awaiting our frame, with a fixed first-event budget.
 * Only 0001/0030/0031/0032/0090/0093/0095 have verified request arguments; other payloads remain
 * opaque. No I/O, waits, object references or job mutations on this callback.
 */
static void pipeline_notify(z72_u32 event,z72_u32 argument)
{
    z72_u32 card_done=0;
    if (LOAD(in_flight)) {
        batch_track(event,argument);
        if (event==0x041b0030u || event==0x041b0031u ||
            event==0x041b0090u || event==0x041b0095u) card_done=card_notice(event,argument);
        if ((event==0x041b0095u || event==0x041b0090u) && pre_capture_ram(argument,0x4bdc)) {
            STORE(peer[9],0);
            peer[10]=WORD(argument+0x4bd8); peer[11]=WORD(argument+0x4bc8);
            peer[17]=argument;
            z72_u32 owner=WORD(argument+0xc);
            if (pre_capture_ram(owner,0x4f8)) {
                peer[12]=WORD(owner+0x4f4);
                /* Native master constructors store the wrapper at +4bd8;
                 * slave job lookup 0x101293f0 compares that same token.
                 * Context +2a8 contains Picture Control, not frame identity.
                 */
                STORE(peer[9],pre_capture_ram(peer[10],4) && WORD(peer[10])==argument &&
                    pre_capture_ram(peer[12],0x3400));
            }
        }

    }
    if (event==0x041b0093u && LOAD(in_flight) && fallback[7]==1 &&
        gate_notice(argument)) return;
    CALL(FILE_NOTIFY_NATIVE,void,z72_u32,z72_u32)(event,argument);
    if (card_done) {
        /* Original callback runs once before publication. No request reads
         * here: it may have retired or released work during that callback.
         */
        finish_card(card_done==1 ? 0 : 3);
    }
}

/* Input task, not the later GUI packet callback. Native registrations 0..3
 * are half-down, full-down, full-up and half-up. Resolve each pending callback
 * before filtering; forward unrelated events in the original ascending order.
 * No file I/O, waits, capture calls or menu rendering on this task.
 */
#include "pre_capture_jpeg_input.h"
static void input_dispatch(z72_u32 flags)
{
    for (z72_u32 i=0;i<16;++i) if (flags&(1u<<i)) {
        z72_u32 fn=WORD(INPUT_CALLBACKS+4*i),consume=0;
        consume=jpeg_shutter_event(fn,i);
        if (!consume) CALL(INPUT_NATIVE,void,z72_u32)(1u<<i);
    }
    if (flags&0xffff0000u) CALL(INPUT_NATIVE,void,z72_u32)(flags&0xffff0000u);
}

static void finish_frame(z72_u32 result)
{
    z72_u32 end=tick();
    if (result) __atomic_fetch_or(&io_result,result,__ATOMIC_RELAXED);
    /* One output cannot retire a capture session. Publish after
     * native source bookkeeping / slave acknowledgement has completed.
     */
    __atomic_fetch_add(&batch_seen,1,__ATOMIC_RELEASE);
    if (LOAD(stream[0])) STORE(stream[7],end);
}

/* Read back the first bytes of a closed PC file through the native file API.
 * Full JPEG decoding remains a separate hardware/desktop validation step.
 */
static z72_u32 check_jpeg(const char *path
)
{
    z72_u32 file[10],actual=0;
    _Alignas(64) unsigned char header[64];
    for (z72_u32 i=0;i<sizeof(header);++i) ((volatile unsigned char *)header)[i]=0;
    z72_u32 request[3]={1,(z72_u32)header,sizeof(header)};
    CALL(0x104d3c01u,void,z72_u32 *,const char *)(file,path);
    if (CALL(0x104d3c05u,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)!=SDK_OK) return 1;
    z72_u32 result=4;
    if (file[6]>=8 && !file[7]) {
        CALL(0x1034fa29u,void,void *,z72_u32)(header,sizeof(header));
        z72_u32 read=CALL(0x104d3d83u,z72_u32,z72_u32 *,void *,z72_u32,z72_u32 *)(
            file,request,8,&actual);
        CALL(0x1034fa29u,void,void *,z72_u32)(header,sizeof(header));
        result=read!=SDK_OK || actual!=8 ? 2 :
            header[0]!=0xff || header[1]!=0xd8 ? 3 : 0;
    }
    if (CALL(0x104d3c91u,z72_u32,z72_u32 *)(file)!=SDK_OK) result=5;
    return result; /* Basic signature/readback only, not a full JPEG decoder. */
}

static z72_u32 export_frame(z72_u32 record,z72_u32 mode,const struct capture_job *job)
{
    native_id=WORD(record); native_size=WORD(record+0x18);
    z72_u32 object=CALL(0x10567c09u,z72_u32,z72_u32)(native_id);
    if (!object) return 3;
    CALL(0x105681bfu,void,z72_u32,z72_u32,z72_u32,z72_u32)(object,0,native_size,0);
    z72_u32 result=ring_copy_jpeg(record,mode,job);
    CALL(0x105678b5u,z72_u32,z72_u32,z72_u32)(object,0);
    CALL(0x1056788bu,void,z72_u32)(object);
    return result;
}

/* Read small slave metadata through the same mapping/copy services used by
 * the native unpacker. The copier requires 8-byte alignment on all operands.
 * Never dereference a slave address with the master CPU.
 */
static z72_u32 peer_read(z72_u32 address,z72_u32 bytes,z72_u32 *out)
{
    _Alignas(8) z72_u32 scratch[6];
    ++peer[20];
    if ((address&3u) || !bytes || bytes>20 || address<0xc1000000u ||
        address>0xc60f0000u-24u) { peer[21]=address; return 0; }
    z72_u32 base=address&~7u,span=((address&7u)+bytes+7u)&~7u;
    z72_u32 mapped=CALL(0x103880e5u,z72_u32,z72_u32,z72_u32,z72_u32)(0,base,span);
    if (!mapped || mapped==0xffffffffu || (mapped&7u)) {
        peer[21]=address; return 0;
    }
    /* 10387f22 is a CPU LDRD copy, not a DMA/coherency operation. Invalidate
     * only our read-only mapped source, never clean stale foreign metadata. */
    CALL(0x1034fa29u,void,z72_u32,z72_u32)(mapped,span);
    ++peer_recovery[2];
    if (!CALL(0x10387f23u,z72_u32,void *,z72_u32,z72_u32)(scratch,mapped,span)) {
        peer[21]=address; return 0;
    }
    for (z72_u32 i=0;i<bytes/4;++i) out[i]=scratch[(address&7u)/4+i];
    return 1;
}

/* "slot" is the full native request sequence, not a 0..31 array index.
 * IPC preserves all bits; native event waits mask it with 0x1f locally. */
struct peer_identity { z72_u32 request,token,slot,context; };

/* Only one native RAM-output permission is outstanding at a time. Slave
 * 1012eee2 waits for class-3/op-25 before 1012e82a schedules 1012ad02.
 * Thus association comes from the permission we actually issued, not FIFO
 * arrival guesses or a snapshot of a private foreign task list.
 */
static z72_u32 gate_identity(z72_u32 i)
{
    struct capture_job *j=&jobs[i];
    z72_u32 request=LOAD(j->request),owner;
    if (!pre_capture_ram(request,0x4bdc) || !pre_capture_ram(j->token,4) ||
        WORD(j->token)!=request || WORD(request+0x4bd8)!=j->token ||
        WORD(request+0x4bc8)!=j->slot || WORD(request+4)) return 0;
    owner=WORD(request+0xc);
    return pre_capture_ram(owner,0x4f8) && WORD(owner+0x4f4)==j->context &&
        pre_capture_ram(j->context,0x3400) &&
        (BYTE(j->context+0x27fa)==1 || BYTE(j->context+0x27fa)==2);
}

static z72_u32 gate_notice(z72_u32 request)
{
    for (z72_u32 i=0;i<BATCH_JOBS;++i) if (LOAD(jobs[i].request)==request &&
        !LOAD(jobs[i].claimed) && gate_identity(i)) {
        z72_u32 empty=0;
        if (__atomic_compare_exchange_n(&gate_ready[i],&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))
            ++gate_stats[0];
        else ++gate_stats[5];
        return 1; /* Defer this permission, not the native task's ownership. */
    }
    /* An unknown permission invalidates exclusivity; never assign its later
     * output to our active job. Keep native handling, stop granting our queue. */
    STORE(gate_fault,1); ++gate_stats[3];
    return 0;
}

static void gate_pump(void)
{
    if (LOAD(gate_active) || LOAD(gate_fault)) return;
    z72_u32 next=BATCH_JOBS;
    for (z72_u32 i=0;i<BATCH_JOBS;++i) if (LOAD(gate_ready[i])==1 &&
        !LOAD(jobs[i].claimed) && (next==BATCH_JOBS || jobs[i].order<jobs[next].order)) next=i;
    if (next==BATCH_JOBS) return;
    if (!gate_identity(next)) { STORE(gate_fault,1); ++gate_stats[3]; return; }
    STORE(gate_ready[next],2);
    STORE(gate_active,next+1); /* Publish before the original callback sends IPC. */
    ++gate_stats[1]; gate_stats[7]=jobs[next].order;
    CALL(FILE_NOTIFY_NATIVE,void,z72_u32,z72_u32)(0x041b0093u,LOAD(jobs[next].request));
}

static z72_u32 peer_gate_matches(struct peer_identity *matched)
{
    z72_u32 active=LOAD(gate_active);
    if (!active || active>BATCH_JOBS || LOAD(gate_fault)) { ++gate_stats[4]; return 2; }
    z72_u32 i=active-1;
    if (LOAD(gate_ready[i])!=2 || LOAD(jobs[i].claimed) || !gate_identity(i)) {
        ++gate_stats[3]; STORE(gate_fault,1); return 16;
    }
    *matched=(struct peer_identity){LOAD(jobs[i].request),jobs[i].token,jobs[i].slot,jobs[i].context};
    peer[10]=matched->token; peer[11]=matched->slot;
    peer[12]=matched->context; peer[17]=matched->request;
    peer_recovery[1]=3;
    return 0;
}

/* Preserve the existing card-output association through the native active
 * lists searched by 101293f0. RAM output uses the native permission above. */
static z72_u32 card_peer_matches_once(struct peer_identity *matched)
{
    z72_u32 source=WORD((z72_u32)peer_wire+4);
    peer[19]=source;
    for (z72_u32 q=0;q<2;++q) {
        z72_u32 root=0xc16a58a8u+q*0x20u,list[3],check[3];
        if (!peer_read(root,12,list)) return 8;
        peer[22+q]=list[2];
        if (list[2]>32 || (list[2] && !list[0])) return 16;
        z72_u32 node=list[0];
        for (z72_u32 i=0;i<list[2];++i) {
            z72_u32 link[3],job,owner,context,identity[5],again[5];
            if (!peer_read(node,12,link)) return 8;
            /* The slave constructs/retires other wrappers concurrently. An
             * unrelated incomplete entry must not hide the synchronous sender
             * later in this list. Never follow a non-slave job pointer.
             */
            if (!peer_read(link[2],4,&job) || job<0xc1000000u || job>0xc60f0000u-0x4bdcu ||
                !peer_read(job+0x4bc8,20,identity)) goto next_peer;
            peer[13]=identity[4]; peer[14]=identity[0]; peer[18]=job;
            struct peer_identity candidate=*matched;
            if (identity[4]==candidate.token) {
                if (!peer_read(job+0xc,4,&owner) || !peer_read(owner+0x4f4,4,&context)) goto next_peer;
                peer[15]=context;
                if (identity[0]!=candidate.slot || context!=source) goto next_peer;
                if (!peer_read(root,12,check) || !peer_read(node,12,again)) return 8;
                for (z72_u32 j=0;j<3;++j)
                    if (list[j]!=check[j] || link[j]!=again[j]) return 16;
                if (!peer_read(link[2],4,check) || check[0]!=job) return 16;
                if (!peer_read(job+0x4bc8,20,again) || !peer_read(job+0xc,4,check) ||
                    !peer_read(owner+0x4f4,4,check+1)) return 8;
                if (again[0]!=identity[0] || again[4]!=identity[4] ||
                    check[0]!=owner || check[1]!=context ||
                    !pre_capture_ram(candidate.request,0x4bdc) || !pre_capture_ram(candidate.token,4) ||
                    WORD(candidate.request+0x4bd8)!=candidate.token ||
                    WORD(candidate.request+0x4bc8)!=candidate.slot || WORD(candidate.token)!=candidate.request) return 16;
                *matched=candidate;
                peer[10]=candidate.token; peer[11]=candidate.slot;
                peer[12]=candidate.context; peer[17]=candidate.request;
                return 0;
            }
next_peer:
            node=link[1];
            if (i+1<list[2] && (!node || node==list[0])) return 16;
        }
    }
    return 2;
}

static z72_u32 card_peer_matches(struct peer_identity *matched)
{
    z72_u32 result=2;
    /* No foreign lock, wait or write: retry a bounded metadata snapshot when
     * another job is inserted/retired during the scan. Unknown jobs still go
     * to the original dispatcher; never manufacture a completion or reply.
     */
    for (z72_u32 attempt=0;attempt<3;++attempt) {
        result=card_peer_matches_once(matched);
        if (!result) break;
    }
    return result;
}

/* Slave 0x10135974 sends class 6/op 7; master 0x104ee8cc unpacks it,
 * assembles a RAM file, then acknowledges class 6/op 8. This path bypasses
 * the master file task. Borrow only until our acknowledgement; the slave
 * retains its original leases and releases them itself after that reply.
 */
static void peer_dispatch(z72_u32 event,z72_u32 packet)
{
    if (!LOAD(in_flight) || fallback[7]==CARD_SHOT || LOAD(file_done) || event!=0x42070001u || !pre_capture_ram(packet,16)) {
        CALL(PEER_NATIVE,void,z72_u32,z72_u32)(event,packet); return;
    }
    ++peer[0]; peer[1]=WORD(packet); peer[16]=0;
    peer[2]=BYTE(packet+8); peer[3]=BYTE(packet+9);
    if (peer[1]!=7 || peer[2]!=1 ||
        (peer[3]!=0 && peer[3]!=3) ||
        !LOAD(batch_created) || LOAD(peer_active)) {
        peer[16]=1;
        CALL(PEER_NATIVE,void,z72_u32,z72_u32)(event,packet); return;
    }
    STORE(peer_active,1);
    peer_recovery[0]=peer_recovery[1]=0;
    for (z72_u32 i=0;i<sizeof(peer_wire);++i) peer_wire[i]=0;
    for (z72_u32 i=0;i<sizeof(peer_context);++i) peer_context[i]=0;
    for (z72_u32 i=0;i<14;++i) peer_record[i]=0;
    for (z72_u32 i=0;i<22;++i) peer_headers[i]=0;
    peer[4]=CALL(0x104ee77bu,z72_u32,z72_u32,void *,void *,void *,void *)(
        packet+8,peer_wire,peer_record,peer_context,peer_headers);
    peer[5]=peer_record[0];
    peer[19]=WORD((z72_u32)peer_wire+4); /* Diagnostic only, not sender identity. */
    z72_u32 reject=0;
    struct peer_identity matched={0};
    if (peer_record[2]!=(z72_u32)peer_context || peer_record[3]!=(z72_u32)peer_headers ||
        !peer_headers[0] || !peer_headers[10] ||
        peer_record[7] || peer_record[6]<4 ||
        peer_record[6]>128u*1024u*1024u || (peer_record[0]&15u)!=peer[3] ||
        BYTE((z72_u32)peer_context+0xe7) ||
        (BYTE((z72_u32)peer_context+0x27fa)!=1 && BYTE((z72_u32)peer_context+0x27fa)!=2)) reject|=4;
    if (!reject) reject=peer_gate_matches(&matched);
    peer[16]=reject;
    if (reject) {
        STORE(peer_active,0);
        CALL(PEER_NATIVE,void,z72_u32,z72_u32)(event,packet); return;
    }
    for (z72_u32 i=0;i<9;++i) peer_extra[i]=WORD((z72_u32)peer_wire+0x120+4*i);
    peer_record[4]=(z72_u32)peer_extra;
    /* Pipeline notifications can run during metadata copies/formatting. The
     * shared peer[] fields are diagnostics, never an ownership handoff. Claim
     * this callback's matched request before another native call can preempt.
     */
    z72_u32 claim=LOAD(gate_active),unclaimed=0;
    if (!claim || claim>BATCH_JOBS || LOAD(jobs[claim-1].request)!=matched.request ||
        !__atomic_compare_exchange_n(&jobs[claim-1].claimed,&unclaimed,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
        STORE(peer_active,0); CALL(PEER_NATIVE,void,z72_u32,z72_u32)(event,packet); return;
    }
    /* SDK control-state words can change after admission (native events
     * 04100078/79). They cannot revoke our issued, identity-verified output
     * permission; refusing that output strands the batch and shutdown. */
    CALL(0x104ef19bu,void,z72_u32,z72_u32)(peer[3],(z72_u32)peer_record);
    z72_u32 exported=export_frame((z72_u32)peer_record,peer[3],&jobs[claim-1]);
    peer[6]=exported;
    unsigned char result=(unsigned char)exported;
    STORE(jobs[claim-1].claimed,2);
    z72_u32 reply=CALL(0x10388667u,z72_u32,z72_u32,z72_u32,z72_u32,const void *)(6,8,1,&result);
    peer[7]=reply;
    ++peer[8];
    if (reply==1) {
        STORE(gate_ready[claim-1],3); ++gate_stats[2];
        STORE(peer_active,0); STORE(gate_active,0);
        /* A newly permitted callback may now overwrite diagnostic peer[]. */
        finish_frame(exported);
    } else {
        ++gate_stats[6]; STORE(gate_fault,1); STORE(enabled,0);
        STORE(peer_active,0);
    }
}

/* Slave 0x101359e6 sends class 5/op 3 and waits for op 4. The original
 * 0x104ee9c8 unpacks the same wire layout as RAM output and calls the native
 * card assembler 0x104ef0a2, preserving source flag 1. Keep that path intact
 * using bounded module-owned metadata so its result can be observed before
 * the acknowledgement lets the slave retire its job. No source lease release.
 */
static void card_peer_dispatch(z72_u32 event,z72_u32 packet)
{
    if (!LOAD(in_flight) || fallback[7]!=CARD_SHOT || LOAD(file_done) ||
        event!=0x42070001u || !pre_capture_ram(packet,16) || WORD(packet)!=3 || BYTE(packet+8)!=0 ||
        (BYTE(packet+9)!=0 && BYTE(packet+9)!=3) ||
        !LOAD(peer[9]) ||
        WORD(0xb18693a8u) || WORD(0xb18693acu) || LOAD(peer_active)) {
        CALL(CARD_PEER_NATIVE,void,z72_u32,z72_u32)(event,packet); return;
    }
    STORE(peer_active,1);
    struct peer_identity matched={LOAD(peer[17]),LOAD(peer[10]),LOAD(peer[11]),LOAD(peer[12])};
    for (z72_u32 i=0;i<sizeof(peer_wire);++i) peer_wire[i]=0;
    for (z72_u32 i=0;i<sizeof(peer_context);++i) peer_context[i]=0;
    for (z72_u32 i=0;i<14;++i) peer_record[i]=0;
    for (z72_u32 i=0;i<22;++i) peer_headers[i]=0;
    CALL(0x104ee77bu,z72_u32,z72_u32,void *,void *,void *,void *)(
        packet+8,peer_wire,peer_record,peer_context,peer_headers);
    z72_u32 mode=BYTE(packet+9),reject=0;
    if (peer_record[2]!=(z72_u32)peer_context || peer_record[3]!=(z72_u32)peer_headers ||
        !peer_headers[0] || !peer_headers[10] ||
        peer_record[7] || peer_record[6]<4 ||
        peer_record[6]>128u*1024u*1024u || (peer_record[0]&15u)!=mode ||
        peer_record[13]!=1 || BYTE((z72_u32)peer_context+0xe7) || BYTE((z72_u32)peer_context+0x27fa)!=0) reject=4;
    if (!reject) reject=card_peer_matches(&matched);
    if (reject) {
        STORE(peer_active,0);
        CALL(CARD_PEER_NATIVE,void,z72_u32,z72_u32)(event,packet); return;
    }
    for (z72_u32 i=0;i<9;++i) peer_extra[i]=WORD((z72_u32)peer_wire+0x120+4*i);
    peer_record[4]=(z72_u32)peer_extra;
    z72_u32 slot=track_card(matched.request,0,matched.context,peer_record[0]);
    if (slot==CARD_BURST || LOAD(card_jobs[slot][4])!=1) {
        STORE(peer_active,0);
        CALL(CARD_PEER_NATIVE,void,z72_u32,z72_u32)(event,packet); return;
    }
    z72_u32 result=CALL(0x104ef0a3u,z72_u32,z72_u32,z72_u32,z72_u32)(0,mode,(z72_u32)peer_record);
    card[0]=matched.request; card[1]=0; card[2]=matched.context; card[3]=3;
    card[4]=result; card[5]=result ? 4 : 0;
    unsigned char reply=(unsigned char)result;
    CALL(0x10388667u,z72_u32,z72_u32,z72_u32,z72_u32,const void *)(5,4,1,&reply);
    STORE(peer_active,0);
    z72_u32 ready=1;
    if (__atomic_compare_exchange_n(&card_jobs[slot][4],&ready,2,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) finish_card(result);
}

/* Restricted transcription of 0x102c0d00: exactly one JPEG RAM record. The
 * native task owns this job until its completion bit is posted below. All
 * other work, including normal card shooting, retains the original dispatcher.
 */
static void file_dispatch(z72_u32 flags)
{
    if (!(flags&1u) || !LOAD(in_flight) || fallback[7]==CARD_SHOT) {
        CALL(FILE_NATIVE,void,z72_u32)(flags); return;
    }
    z72_u32 request=WORD(FILE_STATE+0x30),owner=0,context=0,reject=0;
    ++file_observed[0]; file_observed[1]=flags;
    file_format=0xffffffffu;
    file_observed[2]=request;
    for (z72_u32 i=3;i<10;++i) file_observed[i]=0xffffffffu;
    if (pre_capture_ram(request,0x4bb8)) {
        owner=WORD(request+0xc); file_observed[5]=WORD(request+4);
        file_observed[9]=WORD(request+0x360);
        if (file_observed[5]) reject|=8;
        if (file_observed[9]!=1) reject|=128;
    } else reject|=1;
    file_observed[3]=owner;
    if (pre_capture_ram(owner,0x4f8)) {
        context=WORD(owner+0x4f4); file_observed[8]=WORD(owner+0x41c);
        if (!file_observed[8]) reject|=64;
    } else reject|=2;
    file_observed[4]=context;
    if (pre_capture_ram(context,0x27fb)) {
        file_observed[6]=BYTE(context+0xe7);
        file_observed[7]=BYTE(context+0x27fa);
        if (file_observed[6]) reject|=16;
        /* 0x102c0d22..2c accepts both native RAM destinations. */
        if (file_observed[7]!=1 && file_observed[7]!=2) reject|=32;
    } else reject|=4;
    file_observed[10]=reject;
    if (reject) {
        /* A mismatching job is not ours to cancel, free or rewrite. */
        CALL(FILE_NATIVE,void,z72_u32)(flags); return;
    }
    z72_u32 entry=request+0x2b0+0xb8,record=entry+0x58;
    z72_u32 format=CALL(0x105094dfu,z72_u32,z72_u32)(WORD(record));
    file_format=format;
    /* Native 0x102c0d66 table maps 0/1/2/3 directly to export modes;
     * the JPEG component assembler is selected only for 0 and 3.
     */
    if (format!=0 && format!=3) {
        file_observed[10]|=256;
        CALL(FILE_NATIVE,void,z72_u32)(flags); return;
    }
    batch_track(Z72_PRE_CONTINUOUS ? 0 : 0x041b0001u,request);
    z72_u32 claim=batch_claim(request);
    if (!claim) { CALL(FILE_NATIVE,void,z72_u32)(flags); return; }
    native_request=request; ++file_calls;
    /* Native master entry 0x104eeb66 clears the source flag; peer entry
     * 0x104ee954 preserves the slave's flag from wire+164 instead.
     */
    z72_u32 result=3;
    if (!BYTE(FILE_STATE+1)) {
        WORD(record+0x34)=0;
        CALL(0x104ef19bu,void,z72_u32,z72_u32)(format,record);
        result=export_frame(record,format,&jobs[claim-1]);
    }
    else CALL(0x104eeb87u,z72_u32,z72_u32)(WORD(record));
    BYTE(entry+0xb4)=(unsigned char)result;
    if (result) WORD(request+0x4bb4)=0;
    CALL(0x1030d9a5u,void,z72_u32,z72_u32)(WORD(owner+0x430),0xe);
    WORD(owner+0x430)=0;
    WORD(request+0x360)=0;
    STORE(jobs[claim-1].claimed,2);
    if (CALL(0x105095d9u,z72_u32,z72_u32)(WORD(owner+0x34))!=6) {
        z72_u32 done=WORD(FILE_STATE+0x14);
        CALL(done,void,z72_u32,z72_u32)(0x041b0002u,1);
        WORD(request+0x78)|=4;
    }
    CALL(0x103610cdu,void,z72_u32,z72_u32)(FILE_STATE+0x28,1);
    finish_frame(result);
    if (flags&~1u) CALL(FILE_NATIVE,void,z72_u32)(flags&~1u);
}

/* The native SDK expects a completion callback. The real stream and file
 * joins decide when capture has finished. */
static z72_u32 capture_done(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d; return 0;
}
#include "pre_capture_stream.h"
#include "pre_capture_promote.h"

static void clean_cache(void)
{
    /* Reserved native identities must not outlive their only JPEG bytes.
     * OFF/failure may leave numbering gaps, never recycle these identities. */
    if (store_plan[1]>store_plan[2]) return;
    if (!ring_enter()) return;
    for (z72_u32 i=0;i<RING_SLOTS;++i) ring_release(i);
    ring_leave();
}
static z72_u32 reserve_frames(void)
{
    if (store_plan[0]) return 1;
    if (!ring_enter()) return 0;
    store_plan[5]=0xffffffffu;
    z72_u32 cutoff=LOAD(trigger_tick),generation=LOAD(control)&~7u;
    /* Full press is ordered against source delivery. A source accepted before
     * it may finish copying later; no later source can evict frozen frames.
     * This build measures a first image-pipeline notice window, not a proven
     * sensor-exposure window. Encoding/IPC may finish after that fixed cutoff.
     */
    ring_expire(cutoff,generation);
    store_plan[7]=generation;
    window_stats[0]=window_stats[1]=window_stats[2]=0;
    window_stats[3]=cutoff; window_stats[7]=durations[duration_index];
    z72_u32 valid=store_prepare();
    store_plan[0]=valid && store_reserve() ? 1 : 2;
    if (store_plan[0]==2) {
        ++save_failures; STORE(pre_failed,1);
    }
    ring_leave(); return 1;
}
static z72_u32 commit_frames(void)
{
    if (!ring_enter()) return 0;
    z72_u32 cutoff=LOAD(trigger_tick);
    if (store_plan[0]!=1 || (store_plan[1] && !(Z72_PRE_CARD_STREAM && !LOAD(card_expected)) &&
        (!CALL(0x105094e5u,z72_u32,z72_u32)(native_card_id) ||
         (native_card_id>>29)!=store_plan[5] ||
         (native_card_id&0x1fffffe0u)<=(store_ids[store_order[store_plan[1]-1]]&0x1fffffe0u)))) {
        if (!save_failures) ++save_failures;
        STORE(enabled,0);
        ring_leave(); return 1;
    }
    for (z72_u32 i=store_plan[2];i<store_plan[1];++i) {
        z72_u32 slot=store_order[i];
        struct cached_jpeg *f=&ring[slot];
        z72_u32 result=store_native(slot);
        if (result) { ++save_failures; break; }
        saved_id=last_id; if (!cycle_pre_saves) cycle_first_file=last_id;
        if (!window_stats[0] || cutoff-f->ready_tick>cutoff-window_stats[1]) window_stats[1]=f->ready_tick;
        if (!window_stats[0] || cutoff-f->ready_tick<cutoff-window_stats[2]) window_stats[2]=f->ready_tick;
        if (cycle_pre_saves<RING_SLOTS) {
            store_times[cycle_pre_saves][0]=photo_stamps[slot].date;
            store_times[cycle_pre_saves][1]=photo_stamps[slot].clock;
            store_times[cycle_pre_saves][2]=photo_stamps[slot].fraction;
            store_times[cycle_pre_saves][3]=last_id;
        }
        ++window_stats[0];
        cycle_last_file=last_id; ++cycle_pre_saves; ++cache_saves; ++shot[1];
        shot[2]=last_id; ++store_plan[2];
        ring_release(slot);
    }
    if (store_plan[2]==store_plan[1]) store_plan[0]=3;
    if (save_failures || LOAD(pre_failed)) STORE(enabled,0);
    ring_leave(); return 1;
}
static void complete_card_shot(void)
{
    z72_u32 finished=LOAD(card_finished);
    fallback[1]+=finished; fallback[2]=CARD_SHOT;
    cycle_card_count+=finished;
    shot[6]=native_card_id;
    /* Commit the reserved pre prefix once, then publish every real formal
     * completion of this batch before admitting another bounded batch. */
    STORE(cycle_done,continuation ? 3 : 1); STORE(fallback[3],0);
}

/* The native remote capture entry rejects body input already active. Owning
 * pre-capture half-down/up avoids starting the competing ordinary half-press
 * operation. AF-ON and unrelated controls still reach native input unchanged.
 * Match the private SDK entry's scoped local-AF exception while retaining
 * every other real admission predicate. Never clear native masks.
 */
static z72_u32 admission_ready(z72_u32 now)
{
    admission_tick=now;
    admission[0]=CALL(0x103bbba9u,z72_u32,void)();
    admission[1]=CALL(0x1022aebdu,z72_u32,void)();
    admission[2]=CALL(0x1022bbd5u,z72_u32,void)();
    admission[3]=CALL(0x1022c8c7u,z72_u32,void)();
    admission[4]=admission[5]=0xffffffffu;
    z72_u32 af_mask=admission[2];
    if (af_mask==1 && local_af_active()) { af_mask=0; ++af_stats[3]; }
    if (admission[0] || admission[1] || af_mask || admission[3]) {
        ++admission_waits; return 0;
    }
    admission[4]=CALL(0x10229fddu,z72_u32,void)();
    admission[5]=CALL(0x103bbb37u,z72_u32,void)();
    if (!admission[4] && admission[5]!=2) {
        ++admission_waits; return 0;
    }
    return 1;
}

/* 0x103b9e8c changes destination 0x1b04 only for the SDK request and
 * 0x103bbafa restores its saved value. Stage RAM destination through the
 * native property API before submitting, so native cleanup restores RAM
 * while the asynchronous file pipeline still owns the frame. Restore the
 * original setting after file completion or stopping, never a busy mask.
 */
static void restore_destination(void)
{
    if (destination_phase!=3 || BYTE(0xb1869364u) || BYTE(0xb1869365u) || BYTE(0xb186936bu)) return;
    if (local_control_busy()) { destination_phase=0; return; }
    if (WORD(0xb18693a8u) || WORD(0xb18693acu)) ++af_stats[6];
    destination_value=CALL(0x1034ffbfu,z72_u32,z72_u32)(0x1b04);
    if (destination_value==destination_target && destination_before!=destination_target) {
        if (destination_restore_result==0xffffffffu)
            destination_restore_result=CALL(0x1034ffadu,z72_u32,z72_u32,z72_u32)(0x1b04,destination_before);
        destination_value=CALL(0x1034ffbfu,z72_u32,z72_u32)(0x1b04);
        if (destination_value==destination_target) return;
    }
    destination_phase=0;
}
static z72_u32 prepare_destination(z72_u32 now,z72_u32 target)
{
    restore_destination();
    if (destination_phase==3 || local_control_busy()) {
        ++destination_waits; return 0;
    }
    destination_value=CALL(0x1034ffbfu,z72_u32,z72_u32)(0x1b04);
    if (destination_phase==1 && destination_target!=target) {
        /* A full press can arrive while the first RAM write is still queued.
         * Confirm that write before issuing the opposite card destination.
         */
        if (now-destination_tick>=2000u) {
            STORE(enabled,0); destination_phase=3; restore_destination();
            return 0;
        }
        if (now-destination_tick<100u || destination_value!=destination_target) {
            ++destination_waits; return 0;
        }
        destination_phase=2;
    }
    if (!destination_phase || (destination_phase==2 && destination_target!=target)) {
        if (destination_value>2 || local_control_busy()) {
            ++destination_waits; return 0;
        }
        if (!destination_phase) destination_before=destination_value;
        destination_target=target; card[7]=target;
        destination_tick=now;
        destination_set_result=0; destination_restore_result=0xffffffffu;
        destination_phase=destination_value==target ? 2 : 1;
        if (destination_phase==1)
            destination_set_result=CALL(0x1034ffadu,z72_u32,z72_u32,z72_u32)(0x1b04,target);
    }
    if (destination_phase==1) {
        if (now-destination_tick>=2000u) {
            STORE(enabled,0); destination_phase=3; restore_destination();
            return 0;
        }
        if (now-destination_tick<100u || destination_value!=target) {
            ++destination_waits; return 0;
        }
        destination_phase=2;
    }
    if (destination_value==target) return 1;
    STORE(enabled,0); destination_phase=3; restore_destination();
    return 0;
}

static void enable_pre_capture(void)
{
    if (task_result || WORD(INPUT_SLOT)!=(z72_u32)input_dispatch ||
        WORD(FILE_SLOT)!=(z72_u32)file_dispatch || WORD(CARD_SLOT)!=CARD_NATIVE ||
        WORD(FILE_STATE+0x14)!=(z72_u32)pipeline_notify || WORD(PEER_SLOT)!=(z72_u32)peer_dispatch ||
        WORD(CARD_PEER_SLOT)!=(z72_u32)card_peer_dispatch || WORD(IMAGE_SLOT)!=(z72_u32)image_notice ||
        WORD(REVIEW_SLOT)!=(z72_u32)review_event ||
        WORD(GUI_CAPTURE_SLOT)!=(z72_u32)gui_capture ||
        WORD(GUI_CAPTURE_OTHER_SLOT)!=(z72_u32)gui_capture) {
        return;
    }
    if (!capture_allowed()) return;
    if (ring_bytes || save_failures || copy_failures || LOAD(pre_failed)) {
        return;
    }
    if (!finish_input_releases()) {
        STORE(enable_request,2); return;
    }
    session_control=0; STORE(control,0); STORE(triggered,0); STORE(fallback[3],0);
    STORE(enabled,1);
}

__attribute__((noinline)) static void worker_step(void)
{
    z72_u32 now=tick(); ++worker_steps;
    jpeg_start_poll();

    if (promote_step(now)) return;
    if (!LOAD(enabled) && !(Z72_PRE_CARD_STREAM && LOAD(in_flight) && fallback[7]==CARD_SHOT))
        finish_image_notice();
    if (
        /* A full edge can arrive after promote_step has already run this
         * tick. Leave it for the live stream instead of starting card fallback. */
        !(LOAD(in_flight) && fallback[7]==1 && LOAD(stream[0])==1) &&
        __atomic_exchange_n(&triggered,0,__ATOMIC_ACQ_REL)) {
        ++fallback[0]; ++shot[0]; STORE(fallback[3],1); fallback[4]=LOAD(trigger_tick);
        continuation=cycle_card_count=0;
        cycle_pre_saves=cycle_first_file=cycle_last_file=0;
    }
    ring_maintain();
    if (LOAD(in_flight)) {
        if (fallback[7]==1) gate_pump();
        if (fallback[7]==1 && LOAD(stream[0])) {
            if (!stream_drain(now)) return;
            STORE(stream[0],0); STORE(file_done,1);
        }
        if (fallback[7]==1 && batch_expected && LOAD(batch_seen)>=LOAD(batch_expected)) STORE(file_done,1);
        if (!LOAD(file_done) || (!Z72_PRE_CARD_STREAM && fallback[7]==CARD_SHOT &&
            (LOAD(peer_active) || (LOAD(card_expected)>1 && LOAD(enabled) &&
             !io_result && (LOAD(image_pending)==1 || LOAD(image_pending)==3 ||
              card_image_count<LOAD(card_expected) || card_native_busy()))))) {
            if (!drain_timed_out && now-waiting_since>=20000u) {
                drain_timed_out=1; ++peer_recovery[4];
                STORE(enabled,0); STORE(fallback[3],0);
                finish_image_notice();
            }
            return;
        }
        STORE(in_flight,0);
        if (drain_timed_out && !io_result && !copy_failures && !save_failures) clean_cache();
        drain_timed_out=0;
        if (io_result || copy_failures) {
            if (fallback[7]==1) STORE(pre_failed,1);
            /* Preserve the full-press card exposure if validation stopped
             * before DMA and all source outputs retired. */
            if (!(fallback[7]==1 && !copy_failures && LOAD(enabled) && LOAD(fallback[3]))) {
                STORE(enabled,0); STORE(fallback[3],0);
            }
        } else if (fallback[7]==CARD_SHOT && (Z72_PRE_CARD_STREAM || LOAD(fallback[3]))) {
            complete_card_shot();
        }
    }
    if (!LOAD(enabled)) finish_image_notice();
    if (destination_phase && (!LOAD(enabled) || LOAD(cycle_done) ||
        (!LOAD(fallback[3]) && (!LOAD(half) || !LOAD(half_latch))))) destination_phase=3;
    restore_destination();
    if (!LOAD(enabled)) finish_input_releases();
    if (LOAD(cycle_done)) {
        if (LOAD(cycle_done)==1) {
            if (!commit_frames()) return;
        }
        finish_image_notice();
        if (LOAD(image_pending)) return;
        if (!Z72_PRE_CARD_STREAM && LOAD(enabled) && continue_held() &&
            /* State 2 is final. A drained H+ promotion must not start a
             * second, card-destination burst from the same held press. */
            LOAD(cycle_done)!=2 &&
            LOAD(enable_request)!=1 && !save_failures && !LOAD(pre_failed)) {
            /* Each SDK request is bounded and must drain before the next.
             * Do not cancel already admitted native frames on physical up. */
            continuation=1; STORE(cycle_done,0); STORE(fallback[3],1);
        } else STORE(cycle_done,2);
    }
    if (LOAD(cycle_done)==2) {
        if (LOAD(half) || LOAD(full_latch) || destination_phase) return;
        if (!finish_input_releases()) return;
        if (LOAD(half) || LOAD(full_latch)) return;
        z72_u32 frozen=LOAD(control);
        if (!__atomic_compare_exchange_n(&control,&frozen,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return;
        session_control=0; STORE(cycle_done,0);
        STORE(review_ready,cycle_card_count && !save_failures && !LOAD(pre_failed));
        if (cycle_pre_saves && !save_failures) ++shot[5];
        if (LOAD(half) && LOAD(half_latch)) {
            z72_u32 empty=0;
            z72_u32 next=__atomic_add_fetch(&generation_sequence,8,__ATOMIC_RELAXED);
            __atomic_compare_exchange_n(&control,&empty,next,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
        }
        return;
    }
    if (LOAD(enable_request)) {
        STORE(enable_servicing,1);
        z72_u32 requested=__atomic_exchange_n(&enable_request,0,__ATOMIC_ACQ_REL);
        if (requested==1) {
            STORE(enabled,0); STORE(fallback[3],0); STORE(triggered,0);
            discard_review();
            if (!copy_failures && !save_failures) clean_cache();
        } else if (requested==2 && !LOAD(enabled)) enable_pre_capture();
        STORE(enable_servicing,0);
        return;
    }
    if (!LOAD(enabled)) return;
    if (!LOAD(fallback[3]) && (!LOAD(half) || !LOAD(half_latch))) {
        clean_cache(); finish_input_releases();
        if (!LOAD(review_ready)) discard_review();
        return;
    }
    if (!capture_allowed()) {
        STORE(enabled,0); STORE(fallback[3],0); clean_cache();
        return;
    }
    z72_u32 current=LOAD(control),normal=LOAD(fallback[3]);
    if (normal && continuation && !continue_held()) {
        STORE(fallback[3],0); STORE(cycle_done,2);
        return;
    }
    if (!capture_view(current,normal)) return;
    if (!normal && LOAD(release_pending) && !finish_input_releases()) {
        return;
    }
    if (!(current&~7u) || (!normal && (current&FROZEN))) return;
    if ((current&~7u)!=session_control) {
        clean_cache(); session_control=current&~7u;
        promote_reset();
        cycle_pre_saves=cycle_first_file=cycle_last_file=0;
        for (z72_u32 i=0;i<8;++i) store_plan[i]=0;
        for (z72_u32 i=0;i<RING_SLOTS;++i) store_ids[i]=0;
        native_card_id=0;
    }
    if (now-last_attempt<100u) return;
    last_attempt=now;
    if (!admission_ready(now)) return;
    if (normal && !reserve_frames()) return;
    if (!prepare_destination(now,normal ? 0 : 1)) return;
    current=LOAD(control);
    if (!LOAD(enabled) || (!normal && ((current&FROZEN) || !LOAD(half)))) return;
    if (normal && continuation && !continue_held()) {
        STORE(fallback[3],0); STORE(cycle_done,2);
        return;
    }
    if (!capture_view(current,normal)) return;
    frame_control=current; frame_tick=waiting_since=now;
    fallback[7]=normal ? CARD_SHOT : 1;
    if (normal) {
        fallback[5]=now; ++shot[3];
        for (z72_u32 i=0;i<6;++i) card[i]=0;
        STORE(card_expected,CARD_BURST); STORE(card_finished,0); STORE(card_tracked,0);
        card_image_count=0;
        for (z72_u32 i=0;i<CARD_RECORDS;++i) {
            STORE(card_routes[i],0);
        }
        for (z72_u32 i=0;i<CARD_RECORDS;++i) for (z72_u32 j=0;j<5;++j) card_jobs[i][j]=0;
    }
    for (z72_u32 i=0;i<BATCH_JOBS;++i) for (z72_u32 j=0;j<sizeof(jobs[i])/4;++j) ((volatile z72_u32 *)&jobs[i])[j]=0;
    if (normal) native_card_id=0;
    for (z72_u32 i=0;i<BATCH_JOBS;++i) STORE(gate_ready[i],0);
    STORE(gate_active,0); STORE(gate_fault,0);
    STORE(batch_created,0); STORE(batch_seen,0); STORE(batch_expected,normal ? CARD_BURST : BURST_FRAMES);
    STORE(peer[9],0); STORE(file_done,0); STORE(in_flight,1);
    io_result=close_result=0;
    /* Keep both RAM and card requests on the normal H* path. P4=1 at
     * 1700==5 sets SDK+38, selects +9 rather than +8 and enters the distinct
     * 103b930e special cleanup branch. Continuous variants use request+8 to
     * keep acquisition running past the native initial quota.
     * Native admission remains required. Never retire on the first writer.
     */
    z72_u32 request[5]={normal ? 0x00000200u : 0x00010200u,0,
        Z72_PRE_CONTINUOUS && (!normal || Z72_PRE_CARD_STREAM),0,
        normal ? CARD_BURST : BURST_FRAMES
    };
    if (!normal || Z72_PRE_CARD_STREAM) {
        STORE(stream[1],0); STORE(stream[2],0); STORE(stream[3],0);
        STORE(stream[7],tick()); STORE(stream[8],0); STORE(stream[11],0);
        STORE(stream[0],1);
    }
    ++batch_calls;
    backend_result=af_capture(request,(void *)capture_done);
    if (normal) {
        if (!backend_result) {
            z72_u32 accepted=WORD(0xb18693a0u);
            ++card_burst_stats[0]; card_burst_stats[1]=accepted;
            if (accepted && accepted<=
                CARD_BURST
            ) {
                STORE(card_expected,accepted); STORE(batch_expected,accepted);
                if (LOAD(card_finished)>=accepted) STORE(file_done,1);
            } else ++card_burst_stats[3];
        }
    }
    if (!backend_result && !normal) {
        z72_u32 accepted=WORD(0xb18693a0u);
        if (accepted && accepted<=BURST_FRAMES) STORE(batch_expected,accepted);
    }
    if (backend_result) {
        if (!normal || Z72_PRE_CARD_STREAM) { STORE(stream[0],0); STORE(stream[3],0x10); }
        STORE(enabled,0); STORE(fallback[3],0);
        if (backend_result==0x1du) { STORE(in_flight,0); destination_phase=3; restore_destination(); }
    }
}

static void worker(void *unused)
{
    (void)unused;
    for (;;) {
        worker_step();
        delay(LOAD(promote[0]) ? 10 : 50);
    }
}
static void jpeg_menu_refresh(void);
static z72_u32 jpeg_menu_status;
static void menu_refresh(void)
{
    jpeg_menu_refresh();
}
static z72_u32 pre_capture_review_update(void)
{
    z72_u32 active=LOAD(enabled);
    if (active!=jpeg_menu_status) menu_refresh();
    if (!LOAD(review_kind) && !active) return 0;
    if (!active) discard_review();
    /* Native 104e6cb4 consumes this pending review RPC before its display
     * transition. Parse through the same getter and acknowledge immediately
     * through 104bf398, so the SDK provider is never held waiting for release.
     * Defer only the display action, not the source/output/native ACK. */
    z72_u32 pending=CALL(0x104bf36fu,z72_u32,void)();
    if (pending && !hold_review()) discard_review();
    if (hold_review() && pending && !CALL(0x1063c7d5u,z72_u32,z72_u32)(0x1b15) &&
        !CALL(0x10633afbu,z72_u32,void)()) {
        z72_u32 value=0;
        if (CALL(0x104bf377u,z72_u32,z72_u32 *)(&value) && value<=6u) {
            z72_u32 display=BYTE(0xb202c017u);
            if (display && display!=3 && value) value=2;
            STORE(review_value,value); STORE(review_kind,1);
            CALL(0x104bf399u,void,z72_u32)(0);
        }
    }
    if (LOAD(review_kind) && LOAD(review_ready) && !hold_review() &&
        !LOAD(half) && !LOAD(full_latch) && !LOAD(release_pending) &&
        !LOAD(in_flight) && !LOAD(ring_busy) && !LOAD(peer_active) &&
        !LOAD(image_pending) && !LOAD(pre_notice_pending) && !destination_phase &&
        !BYTE(0xb1869364u) && !BYTE(0xb1869365u) && !BYTE(0xb186936bu) &&
        !WORD(0xb18693a8u) && !WORD(0xb18693acu) && photo_view() &&
        !CALL(0x1063c7d5u,z72_u32,z72_u32)(0x1b15) &&
        !CALL(0x10633afbu,z72_u32,void)()) {
        z72_u32 kind=__atomic_exchange_n(&review_kind,0,__ATOMIC_ACQ_REL);
        STORE(review_ready,0);
        if (kind==1) CALL(0x10551cf3u,z72_u32,z72_u32)(LOAD(review_value));
        else CALL(REVIEW_NATIVE,void,z72_u32,z72_u32)(6,0);
        return 1; /* The native display action may have switched context. */
    }
    return 0;
}
static z72_u32 gui_capture(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    if (pre_capture_review_update()) return 0;
    return CALL(GUI_CAPTURE_NATIVE,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(a,b,c,d);
}
static z72_u32 choose_on(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (LOAD(enabled)) return 0;
    if (LOAD(in_flight)) {
        menu_refresh(); return 0;
    }
    if (task_result) {
        menu_refresh(); return 0;
    }
    /* Build the parent page with the accepted value. Its live-view callback
     * does not run while the camera is still in the setup menu. Any pending
     * native input, destination restore or OFF cleanup stays on the worker. */
    if (!LOAD(enable_request) && !LOAD(enable_servicing) && !LOAD(release_pending) &&
        !LOAD(control) && !LOAD(half_latch) && !LOAD(full_latch) && !LOAD(cycle_done) &&
        !LOAD(triggered) && !LOAD(fallback[3]) && !destination_phase && !LOAD(ring_busy)) {
        enable_pre_capture();
        return 0;
    }
    STORE(enable_request,2);
    menu_refresh();
    return 0;
}
static z72_u32 choose_off(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    STORE(enabled,0); STORE(triggered,0); STORE(fallback[3],0); STORE(enable_request,1);
    menu_refresh(); return 0;
}
static z72_u32 select_time(z72_u32 index)
{
    if (LOAD(in_flight) || LOAD(half_latch) || LOAD(fallback[3]) || LOAD(cycle_done)) {
        return 0;
    }
    duration_index=index; menu_refresh(); return 0;
}
#include "pre_capture_jpeg_menu.h"

static z72_u32 pre_capture_ready(void)
{
    return WORD(STREAM_SLOT)==STREAM_NATIVE && WORD(INPUT_SLOT)==INPUT_NATIVE &&
        WORD(FILE_SLOT)==FILE_NATIVE && WORD(CARD_SLOT)==CARD_NATIVE &&
        WORD(GUI_SLOT)==GUI_NATIVE && WORD(REVIEW_SLOT)==REVIEW_NATIVE &&
        WORD(GUI_CAPTURE_SLOT)==GUI_CAPTURE_NATIVE && WORD(GUI_CAPTURE_OTHER_SLOT)==GUI_CAPTURE_NATIVE &&
        WORD(FILE_STATE+0x14)==FILE_NOTIFY_NATIVE && WORD(PEER_SLOT)==PEER_NATIVE &&
        WORD(CARD_PEER_SLOT)==CARD_PEER_NATIVE && WORD(IMAGE_SLOT)==IMAGE_NATIVE && !af_sdk_prepare();
}

static void pre_capture_install(void)
{
    const unsigned char *original=CALL(0x103d3dffu,const unsigned char *,z72_u32)(0x10005);
    z72_u32 address=(z72_u32)original;
    if (!pre_capture_ram(address,28) && (address<0x10020000u || address>0x12497b78u-28 || (address&3u))) {
        task_result=1; return;
    }
    for (z72_u32 i=0;i<28;++i) task_descriptor[i]=original[i];
    task_descriptor[0]=1; *(z72_u32 *)(task_descriptor+8)=16384;
    *(unsigned short *)(task_descriptor+26)=0;
    CALL(0x103dc46bu,int,void *,void (*)(void *),void *)(task_control,worker,task_descriptor);
    if ((int)task_control[0]<=0) { task_result=1; return; }
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(INPUT_SLOT)=(z72_u32)input_dispatch;
    WORD(FILE_SLOT)=(z72_u32)file_dispatch;
    WORD(STREAM_SLOT)=(z72_u32)stream_event;
    WORD(FILE_STATE+0x14)=(z72_u32)pipeline_notify;
    WORD(PEER_SLOT)=(z72_u32)peer_dispatch;
    WORD(CARD_PEER_SLOT)=(z72_u32)card_peer_dispatch;
    WORD(IMAGE_SLOT)=(z72_u32)image_notice;
    WORD(REVIEW_SLOT)=(z72_u32)review_event;
    WORD(GUI_CAPTURE_SLOT)=WORD(GUI_CAPTURE_OTHER_SLOT)=(z72_u32)gui_capture;
    task_result=CALL(0x103606dfu,z72_u32,void *)(task_control);
}
