/* SPDX-License-Identifier: MIT */
#ifndef Z72_BATTERY_PERCENT_H
#define Z72_BATTERY_PERCENT_H

/* A spare regular-object slot in the native live GUI's own container. Its
 * normal teardown releases the text object and font cache with the other HUD
 * controls. Never replace a slot occupied by another native control. */
#define BATTERY_POOL_SLOT 1199u
#define BATTERY_LABEL_SLOT 286u
#define BATTERY_TEXT_ID 0x2f20u
static u32 battery_sampled,battery_sample_tick,battery_percent=0xffffffffu;
static u32 battery_text_value=0xffffffffu,battery_text_object,battery_text_orientation;

static u32 battery_ready(void)
{
    return WORD(0x10949b30u+18u*4)==0x1052498du;
}

static u32 battery_owned_object(void)
{
    /* Revalidate ownership after a native GUI container rebuild. */
    u32 container=WORD(0xb202ce20u);
    if (container<0xb1800000u || container>=0xc0000000u || (container&3u) ||
        *(volatile unsigned short *)(container+4)<=BATTERY_POOL_SLOT) return 0;
    u32 slots=WORD(container+8);
    if (slots<0xb1800000u || slots>=0xc0000000u-4*(BATTERY_POOL_SLOT+1) || (slots&3u)) return 0;
    u32 object=WORD(slots+4*BATTERY_POOL_SLOT);
    return object && object==battery_text_object && WORD(object)==BATTERY_TEXT_ID ? object : 0;
}

__attribute__((noinline)) static void battery_prepare(void)
{
    if ((z72_config_current.disp_enabled&Z72_BATTERY_PERCENT_ON) || !battery_text_object) return;
    u32 object=battery_owned_object();
    if (!object || !WORD(object+0x28)) return;
    if (display_work_pending()) return;
    /* Retire the overlay BEFORE native painting, then let the stock battery
     * layers restore the hatch. A hidden dirty text node drawn last would
     * erase part of that newly restored icon. */
    CALL(0x106361b7u,u32,u32,u32)(object,0);
    WORD(object+0x44)=0;
    CALL(0x10554cf9u,void,void)();
}

static void battery_dirty(void)
{
    /* Repaint last in the same LCD list when AF redraws the native layers. */
    if (!(z72_config_current.disp_enabled&Z72_BATTERY_PERCENT_ON)) return;
    u32 object=battery_owned_object();
    if (object && WORD(object+0x28)) WORD(object+0x44)=1;
}

static u32 battery_native_text(u32 a,u32 b,u32 c,u32 d)
{
    u32 result=CALL(0x10650bd1u,u32,u32,u32,u32,u32)(a,b,c,d);
    battery_dirty();
    return result;
}

static u32 battery_native_rectangle(u32 a,u32 b,u32 c,u32 d)
{
    u32 result=CALL(0x106eefb3u,u32,u32,u32,u32,u32)(a,b,c,d);
    battery_dirty();
    return result;
}

static u32 battery_hook_native(void)
{
    u32 rebound=0;
    /* Only RAM object callbacks are replaced. Constructors can restore their
     * native painter during a GUI rebuild; bind those again after the pass
     * and refresh the percentage once to cover that already-painted frame. */
    for (u32 i=0;i<10;++i) for (u32 j=0;j<2;++j) {
        u32 object=WORD(0xb2bb22e8u+48*i+0x24+4*j);
        if (object<0xb1800000u || object>=0xc0000000u || (object&3u)) continue;
        u32 painter=WORD(object+0x4c);
        if (painter==0x10650bd1u || painter==0x106eefb3u) {
            WORD(object+0x4c)=painter==0x10650bd1u ?
                (u32)battery_native_text : (u32)battery_native_rectangle;
            rebound=1;
        }
    }
    return rebound;
}

static u32 battery_painter(u32 object,u32 next,u32 c,u32 d)
{
    __atomic_store_n(&battery_painting,1,__ATOMIC_RELEASE);
    if (WORD(object+0x28)) {
        /* Redraw the whole silhouette: the stock glyph's visible frame is
         * inset from its layout box, so an interior-only cover clips it.
         * Keep a two-pixel white frame with transparent padding. Native RGBA
         * zero clears the old OSD pixels and skips the fill, removing the
         * stock hatch and old digits rather than blending over them. */
        static const unsigned short rectangles[5][4]={
            {2,396,62,428},{8,398,60,426},{10,400,58,424},
            {3,405,8,419},{5,407,8,417}
        };
        u32 backdrop[24];
        for (u32 i=0;i<24;++i) backdrop[i]=WORD(object+4*i);
        backdrop[1]=6;
        backdrop[2]=0;
        backdrop[3]=6;
        backdrop[13]=2; /* Direct RGBA color, rather than a palette index. */
        for (u32 i=0;i<5;++i) {
            backdrop[4]=rectangles[i][0]|((u32)rectangles[i][1]<<16);
            backdrop[5]=rectangles[i][2]|((u32)rectangles[i][3]<<16);
            backdrop[14]=(i==1 || i==3) ? 0xffffffffu : 0u;
            backdrop[17]=1;
            CALL(0x106eefb3u,void,u32 *,u32,u32,u32)(backdrop,next,c,d);
        }
    }
    u32 result=CALL(0x10650bd1u,u32,u32,u32,u32,u32)(object,next,c,d);
    __atomic_store_n(&battery_painting,0,__ATOMIC_RELEASE);
    return result;
}

__attribute__((noinline)) static void battery_update(u32 output_before,u32 orientation)
{
    /* battery_prepare retires the owned overlay before the native pass. No
     * container, icon, readiness or clock queries are needed while disabled. */
    if (!(z72_config_current.disp_enabled&Z72_BATTERY_PERCENT_ON)) {
        battery_sampled=0;
        return;
    }
    u32 container=WORD(0xb202ce20u);
    if (container<0xb1800000u || container>=0xc0000000u || (container&3u)) return;
    if (*(volatile unsigned short *)(container+4)<=BATTERY_POOL_SLOT) return;
    u32 slots=WORD(container+8);
    if (slots<0xb1800000u || slots>=0xc0000000u-4*(BATTERY_POOL_SLOT+1) || (slots&3u)) return;
    u32 object=WORD(slots+4*BATTERY_POOL_SLOT);
    if (object && (object<0xb1800000u || object>=0xc0000000u || (object&3u) ||
                   WORD(object)!=BATTERY_TEXT_ID)) return;
    u32 menu=BYTE(0xb1824f0cu),playback=BYTE(0xb1828174u);
    u32 visible=output_before==1 && (WORD(ACTIVE_OUTPUT_BITS)&7u)==1 &&
        !BYTE(DISPLAY_SCENE) && !WORD(0xb25519b8u) && !BYTE(0xb1821b18u) &&
        WORD(0xb202f728u)==0xb1821b1cu &&
        !BYTE(DISPLAY_SNAPSHOT) && !BYTE(DISPLAY_SNAPSHOT+0x1a6) &&
        !BYTE(DISPLAY_SNAPSHOT+0x1a7) && !WORD(0xb181dabcu) &&
        !(menu>=1 && menu<=3) && !(playback>=1 && playback<=3);
    u32 icon=0;
    for (u32 i=0;i<10;++i) icon|=BYTE(0xb2bb22e9u+48*i);
    visible=visible && icon;
    if (display_work_pending() || !CALL(0x1052336bu,u32,void)()) return;
    if (!visible) {
        if (object) CALL(0x106361b7u,u32,u32,u32)(object,0);
        battery_sampled=0;
        return;
    }
    u32 now=CALL(0x1062e59du,u32,void)();
    if (!battery_sampled || now-battery_sample_tick>=1000u) {
        _Alignas(4) u32 info[20];
        /* The same locked 80-byte snapshot used by native Battery info.
         * Its +0x18 word is formatted as %d%% by 0x106FE2B9. */
        CALL(0x103f9a2bu,void,u32 *)(info);
        battery_percent=info[6];
        battery_sample_tick=now;
        battery_sampled=1;
    }
    if (battery_percent>100) {
        if (object) CALL(0x106361b7u,u32,u32,u32)(object,0);
        return;
    }
    if (!WORD(0xb20287b8u+12*BATTERY_LABEL_SLOT)) return;
    u32 rebound=battery_hook_native();
    u32 created=!object;
    if (created) {
        object=CALL(0x10634623u,u32,u32,u32,u32)(container,0,BATTERY_POOL_SLOT);
        if (!object) return;
        WORD(object)=BATTERY_TEXT_ID;
        *(volatile unsigned short *)(object+4)=2;
        WORD(object+8)=BATTERY_LABEL_SLOT;
        WORD(object+0xc)=2;
        WORD(object+0x10)=10u|(400u<<16);
        WORD(object+0x14)=60u|(424u<<16);
        WORD(object+0x18)=0; /* Native horizontal centering. */
        WORD(object+0x28)=0;
        WORD(object+0x2c)=1;
        WORD(object+0x34)=WORD(object+0x3c)=0x1000;
        WORD(object+0x38)=1;
        WORD(object+0x40)=0x2ed;
        WORD(object+0x48)=0xb202ce28u;
        WORD(object+0x98)=WORD(object+0x10);
        WORD(object+0x9c)=WORD(object+0x14);
        CALL(0x1065101bu,void,u32)(object);
        CALL(0x1065109bu,u32,u32,u32)(object,1); /* Measure before centering. */
        CALL(0x10739a7du,u32,u32,u32)(object,(u32)battery_painter);
        CALL(0x10635cebu,u32,u32,u32)(0xb202ce28u,object);
    }
    if (created || rebound || object!=battery_text_object || battery_percent!=battery_text_value ||
        orientation!=battery_text_orientation || !WORD(object+0x28)) {
        if (object==battery_text_object && orientation!=battery_text_orientation)
            battery_clear(battery_text_orientation);
        char text[8];
        CALL(0x103fa101u,u32,char *,const char *,u32)(text,"%d%%",battery_percent);
        CALL(0x10636a07u,void,const char *,u32,u32)(text,BATTERY_LABEL_SLOT,BYTE(0xb181dc42u));
        CALL(0x106361c9u,u32,u32,u32,u32)(object,BATTERY_LABEL_SLOT,2);
        CALL(0x106361b7u,u32,u32,u32)(object,1);
        WORD(object+0x44)=1;
        CALL(0x1063618bu,void,u32,u32)(0xb202ce28u,1);
        battery_text_object=object;
        battery_text_value=battery_percent;
        battery_text_orientation=orientation;
        if (CALL(0x1052273fu,u32,void)() && !display_work_pending())
            CALL(0x105218bbu,void,void)();
    }
}
#endif
