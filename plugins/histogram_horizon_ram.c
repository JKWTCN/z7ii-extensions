/* SPDX-License-Identifier: MIT */
#include "z72.h"

typedef z72_u32 u32;
typedef unsigned char u8;
#define WORD(a) (*(volatile u32 *)(a))
#define BYTE(a) (*(volatile u8 *)(a))
#define CALL(a, result, ...) ((result (*)(__VA_ARGS__))(a))

#define GUI_UPDATE_SLOT 0xb1821b38u
#define GUI_UPDATE 0x104e6cb5u
#define DISPLAY_SNAPSHOT 0xb202be70u
#define LAST_DISPLAY_SNAPSHOT 0xb202c1f4u
#define LCD_UPDATE_TICK 0xb181dd90u
#define HISTOGRAM_MASKS 0xb181f8b0u
#define HISTOGRAM_CONTROL 0xb181f87cu
#define HISTOGRAM_STATE 0xb202bc38u
#define HISTOGRAM_BINS 0xb202c9fcu
#define HISTOGRAM_WIDGET 32u
#define DEFAULT_DRAW 0x106eefb3u
#define HISTOGRAM_DRAW 0x10521c39u

static u32 updating;
static u32 last_unthrottled_update;
static _Alignas(64) u32 samples[256];
static volatile u32 paint_visible;

enum histogram_result {
    NO_TICK, OTHER_PAGE, PREVIOUS_PAGE, WRONG_CONTEXT, MENU_BLOCKED,
    GUI_NOT_READY, UPDATE_BLOCKED, WIDGET_BLOCKED, LCD_INACTIVE,
    INVALID_OBJECT, CONTROL_BUSY, INELIGIBLE, MASK_MISMATCH, SAMPLE_BLOCKED,
    SAMPLE_FAILED, SAMPLE_EMPTY, LAYOUT_HIDDEN, PAINTER_MISMATCH,
    SUBMIT_FAILED, DRAWN, HISTOGRAM_RESULTS
};

#ifdef Z72_HISTOGRAM_DIAGNOSTIC
static void histogram_observe(enum histogram_result result,u32 before,u32 after);
static volatile u32 painter_calls,visible_painter_calls,present_requests;
static volatile u32 painter_object,painter_visibility,painter_scale;
#endif

static u32 histogram_painter(u32 object,u32 next,u32 c,u32 d)
{
    u32 visible=WORD(object+0x28);
    if (visible) paint_visible=1;
#ifdef Z72_HISTOGRAM_DIAGNOSTIC
    ++painter_calls;
    if (visible) ++visible_painter_calls;
    painter_object=object;
    painter_visibility=visible;
    painter_scale=WORD(HISTOGRAM_STATE+0x34);
#endif
    return CALL(HISTOGRAM_DRAW,u32,u32,u32,u32,u32)(object,next,c,d);
}

static u32 native_sampling_allowed(void)
{
    u32 (*property)(u32)=CALL(0x1063c7d5,u32,u32);
    if (property(0x168c)==1) return 1;
    return property(0x112c)==1 && !CALL(0x105b0505,u32,void)() &&
           !CALL(0x1062031d,u32,void)();
}

static void layout_histogram(void)
{
    CALL(0x1055503b,void,u32)(HISTOGRAM_WIDGET);
    CALL(0x10555007,void,u32)(HISTOGRAM_WIDGET);
    CALL(0x10522ec9,void,u32)(HISTOGRAM_WIDGET);
}

/* Freestanding module builds do not link the compiler's division helper. */
static u32 average(u32 total,u32 count)
{
    u32 quotient=0,remainder=0;
    for (int bit=31;bit>=0;--bit) {
        remainder=(remainder<<1)|((total>>bit)&1);
        if (remainder>=count) {
            remainder-=count;
            quotient|=1u<<bit;
        }
    }
    return quotient;
}

static enum histogram_result update_histogram(void)
{
    if (BYTE(DISPLAY_SNAPSHOT+0x19e)!=3) return OTHER_PAGE;
    if (BYTE(LAST_DISPLAY_SNAPSHOT+0x19e)!=3) return PREVIOUS_PAGE;
    if (BYTE(DISPLAY_SNAPSHOT+0x1a6) || BYTE(0xb1821b18) || BYTE(0xb181da91) ||
        BYTE(0xb202e294)) return WRONG_CONTEXT;
    if (CALL(0x104a9825,u32,void)()) return MENU_BLOCKED;
    if (!CALL(0x1052336b,u32,void)()) return GUI_NOT_READY;
    if (CALL(0x10630d23,u32,void)()==1) return UPDATE_BLOCKED;
    if (CALL(0x1061dd6d,u32,u32)(12)) return WIDGET_BLOCKED;
    if (!CALL(0x10636b83,u32,u32)(1)) return LCD_INACTIVE;

    u32 object=WORD(HISTOGRAM_CONTROL+0x24);
    if (object<0xb1800000u || object>=0xc0000000u || (object&3)) return INVALID_OBJECT;
    if (BYTE(HISTOGRAM_CONTROL) || BYTE(HISTOGRAM_CONTROL+1)) return CONTROL_BUSY;
    if (!native_sampling_allowed()) return INELIGIBLE;
    for (u32 i=0;i<9;++i)
        if (WORD(HISTOGRAM_MASKS+4*i)!=0x444u) return MASK_MISMATCH;

    _Alignas(4) u8 readiness[0x468];
    CALL(0x106b8dcf,void,void *)(readiness);
    if (readiness[0x28] || readiness[0x29]) return SAMPLE_BLOCKED;
    if (!CALL(0x1063d397,u32,u32 *)(samples)) return SAMPLE_FAILED;

    u32 peak=0,total=0,count=0;
    for (u32 i=0;i<256;++i) {
        u32 bin=samples[i];
        if (bin>peak) peak=bin;
        if (bin) { total+=bin; ++count; }
    }
    if (!count) return SAMPLE_EMPTY;
    u32 scale=average(total,count)<<2;
    if (peak<scale) scale=peak;
    for (u32 i=0;i<256;++i) WORD(HISTOGRAM_BINS+4*i)=samples[i];
    WORD(HISTOGRAM_STATE+0x34)=scale;
    BYTE(HISTOGRAM_STATE+0x10)=1;

    /* This runs on the GUI task after the stock LCD pass. The masks only
     * need to be extended during placement of this one widget. */
    BYTE(HISTOGRAM_CONTROL)=1;
    BYTE(HISTOGRAM_CONTROL+1)=1;
    for (u32 i=0;i<9;++i) WORD(HISTOGRAM_MASKS+4*i)=0xcccu;
    layout_histogram();
    for (u32 i=0;i<9;++i) WORD(HISTOGRAM_MASKS+4*i)=0x444u;
    enum histogram_result result=BYTE(HISTOGRAM_CONTROL+1)==1 ? PAINTER_MISMATCH : LAYOUT_HIDDEN;
    u32 painter=WORD(object+0x4c);
    if (BYTE(HISTOGRAM_CONTROL+1)==1 && WORD(HISTOGRAM_CONTROL+0x24)==object &&
        (painter==DEFAULT_DRAW || painter==HISTOGRAM_DRAW)) {
        CALL(0x10526171,void,void)();
        if (WORD(object+0x4c)==HISTOGRAM_DRAW) {
            paint_visible=0;
            CALL(0x10739a7d,u32,u32,u32)(object,(u32)histogram_painter);
            u32 drawn=CALL(0x1052273f,u32,void)();
            /* The native submit consumes dirty callbacks synchronously.
             * Restore the ordinary painter before another page can use it. */
            if (WORD(HISTOGRAM_CONTROL+0x24)!=object) return INVALID_OBJECT;
            CALL(0x10739a7d,u32,u32,u32)(object,DEFAULT_DRAW);
            if (drawn && paint_visible && BYTE(DISPLAY_SNAPSHOT+0x19e)==3 &&
                !BYTE(0xb181da91)) {
                /* The stock LCD path presents after all scene passes. */
                CALL(0x105218bb,void,void)();
#ifdef Z72_HISTOGRAM_DIAGNOSTIC
                ++present_requests;
#endif
                return DRAWN;
            }
            CALL(0x1063618b,u32,u32,u32)(object,1);
            if (!drawn) result=SUBMIT_FAILED;
        }
    }

    BYTE(HISTOGRAM_STATE+0x10)=0;
    BYTE(HISTOGRAM_CONTROL)=1;
    BYTE(HISTOGRAM_CONTROL+1)=0;
    layout_histogram();
    return result;
}

static u32 gui_update(u32 a,u32 b,u32 c,u32 d)
{
    u32 (*original)(u32,u32,u32,u32)=CALL(GUI_UPDATE,u32,u32,u32,u32,u32);
    if (__atomic_exchange_n(&updating,1,__ATOMIC_ACQUIRE)) return original(a,b,c,d);
    u32 before=WORD(LCD_UPDATE_TICK);
    u32 result=original(a,b,c,d);
    u32 after=WORD(LCD_UPDATE_TICK);
    enum histogram_result outcome=NO_TICK;
    if (after && after!=before) {
        outcome=update_histogram();
    } else if (BYTE(DISPLAY_SNAPSHOT+0x19e)==3 &&
               !CALL(0x1061e41d,u32,void)()) {
        /* The native LCD path skips its timestamp write when its interval
         * is zero. Rate-limit the extra pass against the same system clock. */
        u32 now=CALL(0x1062e59d,u32,void)();
        if (now && (!last_unthrottled_update || now-last_unthrottled_update>=100u)) {
            last_unthrottled_update=now;
            outcome=update_histogram();
        }
    }
#ifdef Z72_HISTOGRAM_DIAGNOSTIC
    histogram_observe(outcome,before,after);
#else
    (void)outcome;
#endif
    __atomic_store_n(&updating,0,__ATOMIC_RELEASE);
    return result;
}

static u32 histogram_horizon_ready(const struct z72_api *api)
{
    return api->abi==Z72_ABI && api->target==Z72_TARGET && api->size>=sizeof(*api) &&
           WORD(0xb202f728)==0xb1821b1cu && WORD(GUI_UPDATE_SLOT)==GUI_UPDATE &&
           WORD(0x1094aaa4)==0xb181f8acu && WORD(0xb181f8d8)==HISTOGRAM_CONTROL;
}

static void histogram_horizon_install(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(GUI_UPDATE_SLOT)=(u32)gui_update;
}

#ifndef Z72_HISTOGRAM_WITH_GRID
z72_u32 z72_main(const struct z72_api *api)
{
    if (!histogram_horizon_ready(api)) return 1;
    histogram_horizon_install();
    return 0;
}
#endif
