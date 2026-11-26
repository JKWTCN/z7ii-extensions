/* SPDX-License-Identifier: MIT */
/* Nikon Z 7II C 1.70 still-photo pixel shift. Included by the combined module. */
#define PIXEL_LOAD(var) __atomic_load_n(&(var),__ATOMIC_ACQUIRE)
#define PIXEL_STORE(var,value) __atomic_store_n(&(var),(value),__ATOMIC_RELEASE)

#include "pixel_shift_vr.h"
#include "pixel_shift_nef_file.h"

enum {
    PIXEL_OFF=0, PIXEL_ONE, PIXEL_REPEAT,
    PIXEL_IDLE=0, PIXEL_RUNNING, PIXEL_STARTING, PIXEL_REVIEW_READY, PIXEL_REVIEW_SHOWING,
    PIXEL_MERGE_STARTING, PIXEL_MERGE_SCANNING, PIXEL_MERGING,
    PIXEL_MAX_SHOTS=32,
    PIXEL_REVIEW_EVENT=1,
    PIXEL_REVIEW_PROPERTY=0x0308u,
    PIXEL_REVIEW_DESCRIPTOR=0x118fb584u,
    PIXEL_REVIEW_CACHE=0xb257586cu
};
enum { PIXEL_FILE_DONE=1, PIXEL_FILE_FAILED, PIXEL_IMAGE_CLOSED };

static const unsigned char pixel_delays[]={0,1,2,3,5,10};
static const unsigned char pixel_gaps[]={0,1,2,3,5,10,0};
static z72_u32 pixel_mode,pixel_count_index,pixel_delay_index=2,pixel_gap_index;
static z72_u32 pixel_full_owned,pixel_pressed_at,pixel_count,pixel_delay,pixel_gap;
static z72_u32 pixel_state,pixel_tasks_started,pixel_review_request;
static z72_u32 pixel_job,pixel_release_result;
static z72_u32 pixel_observe_file,pixel_file_request,pixel_file_id,pixel_file_result;
static z72_u32 pixel_half_steps=Z72_PIXEL_HALF_STEPS_DEFAULT;
static z72_u32 pixel_merge_input_lock;
static void merge_menu_wake(void);

#include "pixel_shift_merge_file.h"
#include "pixel_shift_merge_gallery.h"

enum {
    PIXEL_REFRESH_DISPLAY_TIMER=0x106533d5u, PIXEL_REFRESH_STANDBY_TIMER=0x103db2c1u,
    PIXEL_DISPLAY_ACTIVITY=1u, PIXEL_MENU_ACTIVITY=2u,
    PIXEL_DISPLAY_REFRESH_MS=500
};
static unsigned pixel_merge_format,pixel_merge_selection;
static unsigned pixel_merge_percent,pixel_merge_result,pixel_merge_cancel,pixel_merge_ca;

static int pixel_merge_stop_requested(void)
{
    unsigned mode=BYTE(0xb1824f0cu);
    return PIXEL_LOAD(pixel_merge_cancel) || mode<1 || mode>3;
}

static int pixel_merge_claim(void)
{
    z72_u32 idle=PIXEL_IDLE;
    return __atomic_compare_exchange_n(&pixel_state,&idle,PIXEL_MERGE_STARTING,0,
                                        __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}

static void pixel_merge_progress(unsigned percent)
{
    if (percent!=PIXEL_LOAD(pixel_merge_percent)) PIXEL_STORE(pixel_merge_percent,percent);
}

static void pixel_merge_worker(void *arg)
{
    (void)arg;
    for (;;) {
        z72_u32 job=PIXEL_LOAD(pixel_state);
        if (job==PIXEL_MERGE_SCANNING) {
            merge_gallery_scan(pixel_merge_stop_requested);
        } else if (job==PIXEL_MERGING) {
            const struct merge_group *group=&merge_gallery_groups[pixel_merge_selection];
            unsigned ca_status=0;
            unsigned result=merge_write_image(group->ids,group->count,pixel_merge_stop_requested,
                                              &ca_status,pixel_merge_format,pixel_merge_progress);
            PIXEL_STORE(pixel_merge_ca,ca_status);
            PIXEL_STORE(pixel_merge_result,result);
        } else {
            pixel_sleep(50);
            continue;
        }
        PIXEL_STORE(pixel_state,PIXEL_IDLE);
    }
}

static void pixel_merge_refresh_timers(void)
{
    /* The firmware tracks menu activity separately from display activity. */
    CALL(PIXEL_REFRESH_DISPLAY_TIMER,void,z72_u32)(PIXEL_DISPLAY_ACTIVITY|PIXEL_MENU_ACTIVITY);
    CALL(PIXEL_REFRESH_STANDBY_TIMER,void,void)();
}

static void pixel_merge_refresh_display(z72_u32 *last_refresh)
{
    unsigned state=PIXEL_LOAD(pixel_state);
    if ((state!=PIXEL_MERGING && state!=PIXEL_MERGE_SCANNING) || pixel_merge_stop_requested()) return;
    z72_u32 now=pixel_tick();
    if (now-*last_refresh<PIXEL_DISPLAY_REFRESH_MS) return;
    pixel_merge_refresh_timers();
    *last_refresh=now;
}

static int pixel_photo_view(void)
{
    z72_u32 output=WORD(0xb181d3a4u)&7u,scene=BYTE(0xb181da91u);
    z72_u32 menu_mode=BYTE(0xb1824f0cu),review=BYTE(0xb1828174u);
    return ((output==1 && scene==0) || (output==4 && scene==2)) &&
        !BYTE(0xb1821b18u) && !WORD(0xb25519b8u) &&
        !BYTE(0xb202c016u) && !BYTE(0xb202c017u) &&
        !(menu_mode>=1 && menu_mode<=3) && !(review>=1 && review<=3);
}

static int pixel_capture_idle(void)
{
    return BYTE(0xb23f6a80u)==2 && !BYTE(0xb23f6a81u) &&
        !BYTE(0xb1869364u) && !BYTE(0xb1869365u) && !BYTE(0xb186936bu) &&
        !WORD(0xb18693a8u) && !WORD(0xb18693acu);
}

#include "pixel_shift_capture_settings.h"

static void pixel_target(unsigned i,z72_u32 steps,z72_u32 *x,z72_u32 *y)
{
    /* Each group of four samples the four Bayer phases one pixel apart. */
    static const unsigned char corners[][2]={{0,0},{1,0},{1,1},{0,1}};
    const unsigned char *coarse=corners[i&3];
    const unsigned char *phase=corners[pixel_count>=16 ? (i>>2)&3 : 0];
    *x=21u*coarse[0]+phase[0]*((steps&255u)-coarse[0]);
    *y=18u*coarse[1]+phase[1]*(steps>>8);
}

static int pixel_cancelled(void)
{
    return PIXEL_LOAD(pixel_mode)==PIXEL_OFF || !pixel_photo_view();
}

static int pixel_running(void)
{
    z72_u32 state=PIXEL_LOAD(pixel_state);
    return state!=PIXEL_IDLE && state!=PIXEL_REVIEW_READY;
}

static int pixel_shutter_event(z72_u32 callback)
{
    if (callback==0x1022aa25u)
        return __atomic_exchange_n(&pixel_full_owned,0,__ATOMIC_ACQ_REL)!=0;
    if (callback!=0x1022aa5du) return 0;
    if (PIXEL_LOAD(pixel_full_owned) || pixel_running() || PIXEL_LOAD(pixel_job)==2) {
        PIXEL_STORE(pixel_full_owned,1);
        return 1;
    }
    if (!PIXEL_LOAD(pixel_tasks_started) || PIXEL_LOAD(pixel_mode)==PIXEL_OFF || !pixel_photo_view() ||
        CALL(0x104d1e8bu,z72_u32,void)()!=1) return 0;

    PIXEL_STORE(pixel_full_owned,1);
    if (PIXEL_LOAD(reply)==0x102u) return 1;
    z72_u32 state=PIXEL_LOAD(pixel_state);
    do {
        if (state!=PIXEL_IDLE && state!=PIXEL_REVIEW_READY) return 1;
    } while (!__atomic_compare_exchange_n(&pixel_state,&state,PIXEL_STARTING,0,
                                          __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE));
    PIXEL_STORE(pixel_review_request,0);
    pixel_pressed_at=pixel_tick();
    pixel_count=4u<<PIXEL_LOAD(pixel_count_index);
    pixel_delay=pixel_delays[PIXEL_LOAD(pixel_delay_index)];
    pixel_gap=pixel_gaps[PIXEL_LOAD(pixel_gap_index)];
    PIXEL_STORE(pixel_state,PIXEL_RUNNING);
    return 1;
}

static int pixel_capture_ready(int require_silent)
{
    return !pixel_cancelled() && pixel_capture_idle() &&
        !CALL(0x1022cfa7u,z72_u32,z72_u32)(0x58) &&
        !CALL(0x1022cfa7u,z72_u32,z72_u32)(0x59) &&
        (!require_silent || CALL(0x1063c7d5u,z72_u32,z72_u32)(PIXEL_SILENT_PHOTOGRAPHY)==1) &&
        CALL(0x104d1e8bu,z72_u32,void)()==1 &&
        CALL(0x1034ffbfu,z72_u32,z72_u32)(0x1b04)==0 &&
        CALL(0x10496b8bu,z72_u32,void)()==1 &&
        CALL(0x103c09e9u,z72_u32,z72_u32)(0x11b)!=1 &&
        !CALL(0x103bbba9u,z72_u32,void)() && !CALL(0x1022aebdu,z72_u32,void)() &&
        !CALL(0x1022bbd5u,z72_u32,void)() && !CALL(0x1022c8c7u,z72_u32,void)() &&
        (CALL(0x10229fddu,z72_u32,void)() || CALL(0x103bbb37u,z72_u32,void)()==2);
}

static int pixel_wait_capture(z72_u32 start)
{
    const z72_u32 delay_ms=pixel_delay*1000u;
    while (pixel_tick()-start<delay_ms+12000u) {
        if (pixel_cancelled()) return 0;
        if (pixel_tick()-start>=delay_ms && pixel_capture_ready(0)) return 1;
        pixel_sleep(100);
    }
    return 0;
}

static int pixel_ram_range(z72_u32 address,z72_u32 size)
{
    return !(address&3u) && address>=0xb1800000u && address<=0xb5f00000u-size;
}

static int pixel_record_file_id(z72_u32 id)
{
    if ((id&15u)!=1) return 0;
    if (!CALL(0x105094e5u,z72_u32,z72_u32)(id)) return 0;
    z72_u32 empty=0;
    return __atomic_compare_exchange_n(&pixel_file_id,&empty,id,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED) ||
        empty==id;
}

static void pixel_finish_file(z72_u32 finished)
{
    z72_u32 pending=0;
    __atomic_compare_exchange_n(&pixel_file_result,&pending,finished,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
}

static z72_u32 pixel_file_notice(z72_u32 event,z72_u32 argument)
{
    z72_u32 finished=0;
    if (PIXEL_LOAD(pixel_observe_file) && !PIXEL_LOAD(pixel_file_result) &&
        (event==0x041b0030u || event==0x041b0031u || event==0x041b0090u || event==0x041b0095u) &&
        pixel_ram_range(argument,0x364)) {
        z72_u32 owner=WORD(argument+0xc);
        z72_u32 context=pixel_ram_range(owner,0x4f8) ? WORD(owner+0x4f4) : 0;
        if (pixel_ram_range(context,0x27fb) && BYTE(context+0x27fa)==0) {
            /* Peer output can finish without passing through the local
             * card worker. Its one-file group already contains the DCF ID.
             */
            if ((event==0x041b0090u || event==0x041b0095u) && !WORD(argument+4) &&
                WORD(owner+0x10)==1 && WORD(owner+0x14)==1)
                pixel_record_file_id(WORD(owner+0x18));
            if (event==0x041b0030u && !PIXEL_LOAD(pixel_file_request) &&
                !WORD(argument+4) && WORD(argument+0x7c)==1 && !WORD(argument+0x360) &&
                pixel_record_file_id(WORD(argument+0xd8))) {
                PIXEL_STORE(pixel_file_request,argument);
            }
            if (event==0x041b0031u && PIXEL_LOAD(pixel_file_request)==argument &&
                !WORD(argument+0x7c))
                finished=WORD(argument+4) || BYTE(argument+0x134) ? PIXEL_FILE_FAILED : PIXEL_FILE_DONE;
        }
    }
    return finished;
}

static z72_u32 pixel_image_notice(z72_u32 event,z72_u32 argument)
{
    if (event!=0x041f0002u || !PIXEL_LOAD(pixel_observe_file)) return 0;
    z72_u32 id=PIXEL_LOAD(pixel_file_id);
    return id && pixel_ram_range(argument,20) && WORD(argument)==id ? PIXEL_IMAGE_CLOSED : 0;
}

static z72_u32 pixel_release_done(z72_u32 event,z72_u32 argument,z72_u32 c,z72_u32 d)
{
    (void)event; (void)argument; (void)c; (void)d;
    return 0;
}

static void pixel_capture_worker(void *arg)
{
    (void)arg;
    static const z72_u32 settings[5]={0x100,0,0,1,1};
    for (;;) {
        z72_u32 expected=1;
        if (!__atomic_compare_exchange_n(&pixel_job,&expected,2,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) {
            pixel_sleep(PIXEL_LOAD(pixel_state)==PIXEL_RUNNING ? 10 : 100);
            continue;
        }
        pixel_release_result=1;
        if (pixel_capture_ready(1) && pixel_settings_match()) {
            PIXEL_STORE(pixel_observe_file,1);
            pixel_release_result=CALL(0x103ba6ebu,z72_u32,const z72_u32 *,
                z72_u32 (*)(z72_u32,z72_u32,z72_u32,z72_u32))(settings,pixel_release_done);
        }
        PIXEL_STORE(pixel_job,3);
    }
}

static int pixel_take_frame(void)
{
    if (!pixel_capture_ready(1)) return 0;
    PIXEL_STORE(pixel_file_request,0);
    PIXEL_STORE(pixel_file_id,0);
    PIXEL_STORE(pixel_file_result,0);
    PIXEL_STORE(pixel_job,1);
    z72_u32 submitted=pixel_tick();
    do {
        z72_u32 job=PIXEL_LOAD(pixel_job),file=PIXEL_LOAD(pixel_file_result);
        if (job==3 && pixel_capture_idle() && (file || pixel_release_result)) break;
        /* F040 shares the VR dispatcher with the native exposure. */
        pixel_sleep(50);
    } while (pixel_tick()-submitted<15000u);

    PIXEL_STORE(pixel_observe_file,0);
    z72_u32 queued=1;
    __atomic_compare_exchange_n(&pixel_job,&queued,0,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    z72_u32 file=PIXEL_LOAD(pixel_file_result);
    return PIXEL_LOAD(pixel_job)==3 && !pixel_release_result && pixel_capture_idle() &&
        (file==PIXEL_FILE_DONE || file==PIXEL_IMAGE_CLOSED);
}

static int pixel_raw_vr_idle(void)
{
    return pixel_capture_idle() && !WORD(VR_CAPTURE_PENDING) && bus_idle();
}

static int pixel_wait_between_frames(struct d2_settings *saved)
{
    for (z72_u32 i=0;i<30;++i) {
        if (pixel_cancelled()) return 0;
        if (pixel_capture_ready(1) && pixel_raw_vr_idle()) {
            struct vr_sample sample;
            read_sample(&sample);
            if (sample.status==0x102u) return 0;
            if (!sample.status && near_center(&sample) && settled(&sample) && wait_bus(100) &&
                (sample.telemetry[VR_REPORT]==VR_NORMAL ||
                 sample.telemetry[VR_REPORT]==VR_HOLD) && snapshot_d2(saved)) return 1;
        }
        pixel_sleep(100);
    }
    return 0;
}

static int pixel_capture_series(struct d2_settings *saved)
{
    static unsigned char payload[]={0,0,0,0,0,16,16,0,0,2};
    static const struct vr_request command={.opcode=0xf1f1,.payload=payload};
    struct vr_sample center;
    z72_u32 ids[PIXEL_MAX_SHOTS],count=pixel_count,gap_ms=pixel_gap*1000u,frame_started=0;
    z72_u32 steps=PIXEL_LOAD(pixel_half_steps);
    unsigned i=0;
    int position_issued=0;
    for (;i<count;++i) {
        while (i && pixel_tick()-frame_started<gap_ms && !pixel_cancelled()) pixel_sleep(50);
        if (pixel_cancelled()) break;
        if (i && !pixel_wait_between_frames(saved)) break;
        z72_u32 x,y;
        pixel_target(i,steps,&x,&y);
        payload[1]=(unsigned char)x;
        payload[3]=(unsigned char)y;
        z72_u32 result=send_position(&command,saved);
        if (may_have_applied(result)) position_issued=1;
        if (result || !wait_position(i ? &center : 0,-4*(int)x,-4*(int)y,i ? 0 : &center)) break;
        frame_started=pixel_tick();
        if (!pixel_take_frame()) break;
        z72_u32 id=PIXEL_LOAD(pixel_file_id);
        for (unsigned j=0;j<i;++j) if (ids[j]==id) id=0;
        if (!id) break;
        ids[i]=id;
    }
    if (!position_issued || !pixel_capture_idle() || PIXEL_LOAD(pixel_job)==2) return 0;

    z72_u32 idle_wait=pixel_tick();
    while (!pixel_raw_vr_idle()) {
        if (!pixel_capture_idle() || pixel_tick()-idle_wait>=1500u) return 0;
        pixel_sleep(50);
    }
    /* Native release can replace the D2 parameters on the last shot. */
    if (wait_bus(250)) snapshot_d2(saved);
    int returned=!send_position(&center_request,saved) && wait_position(0,0,0,0);
    if (!restore_d2(saved) || !returned || i!=count) return 0;

    z72_u32 group=(ids[0]&0x3fffffe0u)|2u;
    for (i=0;i<count;++i)
        if (nx_tag_file(ids[i],group,i+1,count)) return 0;
    return 1;
}

static int pixel_capture_with_review_deferred(struct d2_settings *saved)
{
    /* The capture controller latches 0308 before the first GUI notification.
     * Mask only its live property value. Do not call the settings writer or
     * take over the controller's review state after it has already changed. */
    z72_u32 configured=CALL(0x10100491u,z72_u32,z72_u32)(PIXEL_REVIEW_PROPERTY);
    int review=configured && CALL(0x10632663u,z72_u32,void)();
    z72_u32 original=configured;
    if (configured>2 || !__atomic_compare_exchange_n(&WORD(PIXEL_REVIEW_CACHE),&original,0,0,
                                                       __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return 0;

    int complete=pixel_capture_series(saved);
    z72_u32 suspended=0;
    if (configured && !__atomic_compare_exchange_n(&WORD(PIXEL_REVIEW_CACHE),&suspended,configured,0,
                                                    __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) review=0;
    /* Normally the source is silent during the group. Reuse the native
     * GUI event at completion, unless a stock review RPC was already held. */
    z72_u32 empty=0;
    if (complete && review)
        __atomic_compare_exchange_n(&pixel_review_request,&empty,PIXEL_REVIEW_EVENT,
                                    0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    return complete;
}

static void pixel_worker(void *arg)
{
    (void)arg;
    z72_u32 display_refreshed=0,last_restore=0;
    for (;;) {
        while (PIXEL_LOAD(pixel_state)!=PIXEL_RUNNING) {
            if (pixel_settings_restore_mask && pixel_tick()-last_restore>=1000u) {
                pixel_settings_restore();
                last_restore=pixel_tick();
            }
            pixel_merge_refresh_display(&display_refreshed);
            merge_menu_wake();
            pixel_sleep(100);
        }
        struct d2_settings saved;
        int complete=pixel_wait_capture(pixel_pressed_at) && pixel_settings_prepare() &&
            wait_baseline(&saved) && pixel_capture_ready(1) && pixel_capture_with_review_deferred(&saved);
        if (!pixel_settings_restore()) complete=0;
        z72_u32 one=PIXEL_ONE;
        __atomic_compare_exchange_n(&pixel_mode,&one,PIXEL_OFF,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
        PIXEL_STORE(pixel_state,complete ? PIXEL_REVIEW_READY : PIXEL_IDLE);
    }
}

static int pixel_ready(void)
{
    return WORD(PIXEL_REVIEW_DESCRIPTOR)==7 && WORD(PIXEL_REVIEW_DESCRIPTOR+4)==4 &&
        WORD(PIXEL_REVIEW_DESCRIPTOR+8)==PIXEL_REVIEW_CACHE &&
        WORD(0x10393a2cu)==0x460cb570u && WORD(0x103aa70cu)==0x43f8e92du &&
        WORD(0x103939a0u)==0x4614b5f8u && (unsigned short)HALF(0x103b7cfau)==0xe92du &&
        (unsigned short)HALF(0x103ba6eau)==0xe92du && WORD(0x103ba6ecu)==0x24004ff3u;
}

static int pixel_start_task(const unsigned char *original,unsigned char *descriptor,
                            z72_u32 *control,void (*worker)(void *),unsigned char core)
{
    for (z72_u32 i=0;i<28;++i) descriptor[i]=original[i];
    descriptor[0]=core;
    *(z72_u32 *)(descriptor+8)=8192;
    *(unsigned short *)(descriptor+26)=0;
    CALL(0x103dc46bu,int,void *,void (*)(void *),void *)(control,worker,descriptor);
    return (int)control[0]>0 && !CALL(0x103606dfu,z72_u32,void *)(control);
}

static const unsigned char *pixel_task_template(void)
{
    const unsigned char *original=CALL(0x103d3dffu,const unsigned char *,z72_u32)(0x10005);
    z72_u32 address=(z72_u32)original;
    if (!pixel_ram_range(address,28) &&
        ((address&3u) || address<0x10020000u || address>0x12497b78u-28)) return 0;
    return original;
}

static int pixel_start_tasks(void)
{
    static int attempted;
    static _Alignas(4) unsigned char descriptors[2][28];
    static z72_u32 controls[2][8];
    if (!attempted) {
        attempted=1;
        const unsigned char *original=pixel_task_template();
        if (!original) return 0;
        PIXEL_STORE(pixel_tasks_started,pixel_start_task(original,descriptors[0],controls[0],pixel_worker,1) &&
            pixel_start_task(original,descriptors[1],controls[1],pixel_capture_worker,1));
    }
    return PIXEL_LOAD(pixel_tasks_started);
}

static int pixel_start_merge_task(void)
{
    static int attempted,started;
    static _Alignas(4) unsigned char descriptor[28];
    static z72_u32 control[8];
    if (!pixel_start_tasks()) return 0;
    if (!attempted) {
        attempted=1;
        const unsigned char *original=pixel_task_template();
        started=original && pixel_start_task(original,descriptor,control,pixel_merge_worker,2);
    }
    return started;
}
