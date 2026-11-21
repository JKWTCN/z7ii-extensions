/* SPDX-License-Identifier: MIT */
#ifndef Z72_SUBJECT_TRACKING_H
#define Z72_SUBJECT_TRACKING_H

/* Photo AF mode 0x056c stays on native Auto-area. The module selects
 * its tracking reticle; native SW1/AF-ON still own the actual AF cycle. */
enum {
    TRACK_AF_MODE=0x056b, TRACK_AF_C=2, TRACK_AF_AREA=0x056c,
    TRACK_AUTO_AREA=6, TRACK_SELECTION=Z72_CFG_SUBJECT_TRACKING,
    TRACK_KEY_SLOT=0xb1821b30u, TRACK_KEY_NATIVE=0x104e6c1du,
    TRACK_STATE=0x1061dcbfu, TRACK_ENTER=0x10633291u, TRACK_EXIT=0x10553eedu,
    TRACK_CANCEL=0x106332ffu, TRACK_OK_KEY=0x1b, TRACK_EXIT_KEY=0x19
};

#define TRACK_BYTE(address) (*(volatile unsigned char *)(address))

/* Written by menu/GUI callbacks. Zero means native AF settings own the UI. */
static z72_u32 tracking_selected;
static z72_u32 tracking_generation;
static z72_u32 tracking_assist;
static z72_u32 tracking_poll_tick,tracking_owned_reticle;

static z72_u32 tracking_assist_enabled(void)
{
    return __atomic_load_n(&tracking_assist,__ATOMIC_ACQUIRE);
}

static z72_u32 tracking_assist_save(z72_u32 enabled)
{
    z72_u32 previous=tracking_assist_enabled();
    if (enabled==previous) return 0;
    struct z72_config config=z72_config_current;
    config.tracking_assist=enabled;
    if (z72_config_save(config)!=Z72_CFG_OK) {
        z72_config_current.tracking_assist=previous;
        return 1;
    }
    __atomic_add_fetch(&tracking_generation,1,__ATOMIC_RELEASE);
    __atomic_store_n(&tracking_assist,enabled,__ATOMIC_RELEASE);
    return 0;
}

static z72_u32 tracking_available(void)
{
    return !WORD(0xb25519b8u) &&
        CALL(0x1063c7d5u,z72_u32,z72_u32)(TRACK_AF_MODE)==TRACK_AF_C;
}

static z72_u32 tracking_area_selected(void)
{
    return CALL(0x1063c7d5u,z72_u32,z72_u32)(TRACK_AF_AREA)==TRACK_AUTO_AREA;
}

static z72_u32 tracking_active(void)
{
    return __atomic_load_n(&tracking_selected,__ATOMIC_ACQUIRE) &&
        tracking_available() && tracking_area_selected();
}

static z72_u32 tracking_live(void)
{
    z72_u32 output=WORD(0xb181d3a4u)&7u,scene=TRACK_BYTE(0xb181da91u);
    z72_u32 menu=TRACK_BYTE(0xb1824f0cu),playback=TRACK_BYTE(0xb1828174u);
    return ((output==1 && !scene) || (output==4 && scene==2)) &&
        !TRACK_BYTE(0xb1821b18u) && WORD(0xb202f728u)==0xb1821b1cu &&
        !TRACK_BYTE(0xb202be70u) &&
        !HALF(0xb202c016u) && !WORD(0xb181dabcu) &&
        !(menu>=1 && menu<=3) && !(playback>=1 && playback<=3);
}

enum {
    TRACK_STEP_202=0xb186c5e8u, TRACK_STEP_205=0xb186d7e8u,
    TRACK_STEP_NATIVE=0x102711edu, TRACK_MATCH_NATIVE=0x10367403u,
    TRACK_WORK=0xb23fb970u, TRACK_MATCH_STATUS=0xb18a6d60u,
    TRACK_TEMPLATE_INIT=0xb18a6db0u, TRACK_WEAK_MATCHES=0xb18a6e50u,
    TRACK_REACQUIRE_MS=400, TRACK_RETRY_MS=80
};

/* Owned by the native tracker worker. Coordinates use its input image space. */
static struct {
    z72_u32 generation,model,extent,position,tick,retry_tick;
    unsigned char valid,clean;
} tracking_anchor;

static z72_u32 tracking_position_fits(z72_u32 position,z72_u32 extent,z72_u32 side)
{
    return side && !(position&0x80008000u) && (position&0xffffu)+side<=(extent&0xffffu) &&
        (position>>16)+side<=(extent>>16);
}

/* Run the stock 0x202/0x205 match and delivery once, retaining the matcher's
 * return code (the stock stage returns zero even when the matcher fails). */
static int tracking_match_step(z72_u32 job,z72_u32 use_anchor)
{
    if (WORD(TRACK_WORK+0x18) &&
        !CALL(0x102cb8cbu,z72_u32,z72_u32,z72_u32)(
            job+0x2c,CALL(0x10271115u,z72_u32,z72_u32,z72_u32)(job,7)))
        WORD(TRACK_WORK+0x18)=0;

    z72_u32 previous=WORD(TRACK_WORK+0x44);
    if (use_anchor) WORD(TRACK_WORK+0x44)=tracking_anchor.position;
    int status=CALL(TRACK_MATCH_NATIVE,int,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(
        TRACK_WORK,TRACK_WORK+0x3c,TRACK_WORK+0x1c,TRACK_WORK+0x20,
        TRACK_WORK+0x28,TRACK_WORK+0x30);
    if (status && use_anchor) WORD(TRACK_WORK+0x44)=previous;
    WORD(TRACK_WORK+0x40)=WORD(TRACK_WORK+0x44);

    if (!CALL(0x102ed9d3u,z72_u32,void)()) return status;
    z72_u32 frame=WORD(job);
    z72_u32 output=WORD(TRACK_WORK+0x20);
    if (output) CALL(0x102edc29u,void,z72_u32,z72_u32,z72_u32 *)(frame,job+0x88,&output);
    output=WORD(TRACK_WORK+0x28);
    if (WORD(TRACK_WORK+0x1c) && output)
        CALL(0x102edc5bu,void,z72_u32,z72_u32,z72_u32 *)(frame,job+0x88,&output);
    return status;
}

static z72_u32 tracking_reacquire_step(z72_u32 job)
{
    if (!tracking_assist_enabled() ||
        !__atomic_load_n(&tracking_selected,__ATOMIC_ACQUIRE) || !tracking_live() ||
        WORD(0xb25519b8u) || WORD(job+4) || WORD(job+0x80) ||
        !WORD(job+0x180) || WORD(job+0x184) || TRACK_BYTE(job+0x178)!=TRACK_AUTO_AREA ||
        (!CALL(0x1022cfa7u,z72_u32,z72_u32)(0x58) && !CALL(0x1022cfa7u,z72_u32,z72_u32)(0x5a)) ||
        TRACK_BYTE(TRACK_TEMPLATE_INIT) || TRACK_BYTE(TRACK_MATCH_STATUS)>1u ||
        !WORD(TRACK_WORK+0x2a4)) {
        tracking_anchor.valid=tracking_anchor.clean=0;
        return CALL(TRACK_STEP_NATIVE,z72_u32,z72_u32)(job);
    }

    z72_u32 generation=__atomic_load_n(&tracking_generation,__ATOMIC_ACQUIRE);
    z72_u32 now=CALL(0x1062e59du,z72_u32,void)();
    z72_u32 model=WORD(TRACK_WORK+0x2a4),extent=WORD(TRACK_WORK+0x10);
    if (tracking_anchor.generation!=generation || tracking_anchor.model!=model ||
        tracking_anchor.extent!=extent) {
        tracking_anchor.generation=generation;
        tracking_anchor.model=model;
        tracking_anchor.extent=extent;
        tracking_anchor.valid=tracking_anchor.clean=0;
    }
    if (tracking_anchor.valid &&
        (now-tracking_anchor.tick>=TRACK_REACQUIRE_MS ||
         !tracking_position_fits(tracking_anchor.position,extent,HALF(TRACK_WORK+0x3c))))
        tracking_anchor.valid=0;

    /* Native status 1 uses a saved rectangle and its own recovery. The weak
     * streak begins earlier, while status is still zero and the seed is used. */
    z72_u32 use_anchor=tracking_anchor.valid && !TRACK_BYTE(TRACK_MATCH_STATUS) &&
        HALF(TRACK_WEAK_MATCHES) && WORD(TRACK_WORK+0x44)!=tracking_anchor.position &&
        now-tracking_anchor.retry_tick>=TRACK_RETRY_MS;
    if (use_anchor) tracking_anchor.retry_tick=now;
    int result=tracking_match_step(job,use_anchor);
    z72_u32 position=WORD(TRACK_WORK+0x44);
    if (result || TRACK_BYTE(TRACK_TEMPLATE_INIT) || TRACK_BYTE(TRACK_MATCH_STATUS) ||
        HALF(TRACK_WEAK_MATCHES) || !tracking_position_fits(position,extent,HALF(TRACK_WORK+0x3c))) {
        tracking_anchor.clean=0;
    } else if (tracking_anchor.clean) {
        tracking_anchor.position=position;
        tracking_anchor.tick=now;
        tracking_anchor.retry_tick=now-TRACK_RETRY_MS;
        tracking_anchor.valid=1;
    } else {
        tracking_anchor.clean=1;
    }
    return 0;
}

static void tracking_reacquire_install(void)
{
    __atomic_store_n(&WORD(TRACK_STEP_202),(z72_u32)tracking_reacquire_step,__ATOMIC_RELEASE);
    __atomic_store_n(&WORD(TRACK_STEP_205),(z72_u32)tracking_reacquire_step,__ATOMIC_RELEASE);
}

static void tracking_forget(void)
{
    __atomic_store_n(&tracking_selected,0,__ATOMIC_RELEASE);
    tracking_owned_reticle=0;
    tracking_icons_set(0);
}

static void tracking_leave(void)
{
    if (!__atomic_load_n(&tracking_selected,__ATOMIC_ACQUIRE)) return;
    z72_u32 owned=tracking_owned_reticle;
    tracking_forget();
    if (WORD(0xb25519b8u)) return;
    if (tracking_area_selected())
        CALL(TRACK_EXIT,z72_u32,z72_u32)(TRACK_EXIT_KEY);
    else if (owned)
        /* The i menu previews a native area's property on highlight, before
         * OK. The regular exit dispatcher is then gated off by the area. */
        CALL(TRACK_CANCEL,z72_u32,void)();
}

static z72_u32 tracking_select_area(void)
{
    /* The Auto-area subject subtype is inherited from the camera. */
    if (!tracking_area_selected())
        CALL(0x1063c7e1u,z72_u32,z72_u32,z72_u32)(TRACK_AF_AREA,TRACK_AUTO_AREA);
    if (!tracking_area_selected()) return 1;
    if (!__atomic_load_n(&tracking_selected,__ATOMIC_ACQUIRE)) {
        tracking_icons_set(1);
        tracking_poll_tick=0;
        __atomic_add_fetch(&tracking_generation,1,__ATOMIC_RELEASE);
        __atomic_store_n(&tracking_selected,1,__ATOMIC_RELEASE);
    }
    return 0;
}

static void tracking_update(void)
{
    if (!__atomic_load_n(&tracking_selected,__ATOMIC_ACQUIRE) || !tracking_live()) return;
    if (!tracking_available() || !tracking_area_selected()) {
        tracking_leave();
        wide_af_remember(0);
        return;
    }
    z72_u32 now=CALL(0x1062e59du,z72_u32,void)();
    if ((tracking_poll_tick && (int)(now-tracking_poll_tick)<0) ||
        CALL(0x106355a1u,z72_u32,void)()) return;
    /* Native app/standby transitions can dismiss the reticle. Re-arm it at
     * a bounded rate while this AF area is still selected. */
    tracking_poll_tick=now+100u;
    z72_u32 state=CALL(TRACK_STATE,z72_u32,void)();
    if (state<=1u) {
        /* This is the non-latching branch taken by the first native OK. */
        const unsigned char source=0;
        __atomic_add_fetch(&tracking_generation,1,__ATOMIC_RELEASE);
        tracking_owned_reticle=CALL(TRACK_ENTER,z72_u32,const unsigned char *)(&source);
    } else if (state<=3u) {
        tracking_owned_reticle=1;
    }
}

static z72_u32 tracking_live_key(z72_u32 key,z72_u32 a,z72_u32 b,z72_u32 c)
{
    z72_u32 armed=(key==TRACK_OK_KEY || key==TRACK_EXIT_KEY) && tracking_active() && tracking_live();
    /* The original OK action can latch the target after AF is released. */
    if (armed && key==TRACK_OK_KEY) return 0;
    if (armed) tracking_forget();
    z72_u32 result=disp_key(key,a,b,c);
    if (armed) wide_af_remember(0);
    return result;
}

static z72_u32 tracking_ready(void)
{
    return WORD(TRACK_STEP_202)==TRACK_STEP_NATIVE && WORD(TRACK_STEP_205)==TRACK_STEP_NATIVE &&
        WORD(TRACK_STEP_NATIVE-1)==0x4604b57cu && WORD(TRACK_MATCH_NATIVE-1)==0x4fffe92du &&
        WORD(TRACK_KEY_SLOT)==TRACK_KEY_NATIVE &&
        (unsigned short)HALF(TRACK_STATE-1)==0xb510u && (unsigned short)HALF(TRACK_ENTER-1)==0xb57cu &&
        (unsigned short)HALF(TRACK_EXIT-1)==0xb5f8u;
}

#undef TRACK_BYTE
#endif
