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
static z72_u32 tracking_poll_tick,tracking_owned_reticle;

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
    return WORD(TRACK_KEY_SLOT)==TRACK_KEY_NATIVE &&
        (unsigned short)HALF(TRACK_STATE-1)==0xb510u && (unsigned short)HALF(TRACK_ENTER-1)==0xb57cu &&
        (unsigned short)HALF(TRACK_EXIT-1)==0xb5f8u;
}

#undef TRACK_BYTE
#endif
