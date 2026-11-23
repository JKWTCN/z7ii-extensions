/* SPDX-License-Identifier: MIT */
#ifndef Z72_COMPOSITION_REFERENCE_H
#define Z72_COMPOSITION_REFERENCE_H

#include "card_read.h"
#include "composition_reference_format.h"

enum { REFERENCE_LCD_TARGET=2, REFERENCE_SCRATCH_TARGET=1, REFERENCE_CLIP_RECTS=32 };
enum { REFERENCE_PARTIAL, REFERENCE_TOGGLE, REFERENCE_SCRATCH, REFERENCE_RESTORE };
#define REFERENCE_LCD_OSD 0xb181d384u
/* Allocation 5 used by the default photo LCD route in 0x106356b8.
 * Read its address directly because the native getter also marks it dirty. */
#define REFERENCE_LCD_SCRATCH_OSD 0xb181d388u

struct composition_file {
    u32 header[REFERENCE_HEADER_BYTES/4];
    unsigned char rgba[REFERENCE_STRIDE*REFERENCE_H*4], eof[128];
};
static _Alignas(64) struct composition_file composition_asset;
static u32 composition_image_ready,composition_requested,composition_asset_busy;
static u32 composition_painted,composition_redrawn,composition_kick,composition_present_pending;
static u32 composition_restore;
#ifdef Z72_WIFI_GALLERY
static u32 composition_loaded_upload;
#endif
/* One nontransparent reference pixel per tile, stored as LCD indices plus one. */
static u32 composition_anchors[3][3];
static const u32 composition_full_clear[7]={0,0,LCD_W,LCD_H,0,0,0};

static const unsigned char composition_alpha[Z72_REFERENCE_OPACITIES]={26,64,128,191,255};

#ifdef Z72_REFERENCE_DIAGNOSTIC
static int composition_live(void);
#include "composition_reference_trace.h"
#endif

static void composition_select_anchors(void)
{
    for (u32 row=0;row<3;++row) {
        for (u32 col=0;col<3;++col) {
            u32 x=(2+3*col)*REFERENCE_W/10, y=(2+3*row)*REFERENCE_H/10;
            u32 alpha=composition_asset.rgba[4*(y*REFERENCE_STRIDE+x)+3];
            if (!alpha) {
                /* Include sparse PNGs whose visible pixels miss the grid centers. */
                for (u32 sy=row*REFERENCE_H/3;sy<(row+1)*REFERENCE_H/3;++sy) {
                    for (u32 sx=col*REFERENCE_W/3;sx<(col+1)*REFERENCE_W/3;++sx) {
                        u32 candidate=composition_asset.rgba[4*(sy*REFERENCE_STRIDE+sx)+3];
                        if (candidate>alpha) { x=sx; y=sy; alpha=candidate; }
                    }
                }
            }
            composition_anchors[row][col]=alpha ? 2*y*LCD_W+2*x+1 : 0;
        }
    }
}

static int composition_read(u32 slot,struct composition_file *asset)
{
    char path[]="B:\\Z72REF1.RFG";
    path[9]=(char)('1'+slot);
    u32 file[10];
    int opened=card_open(file,path);
#ifdef Z72_WIFI_GALLERY
    if (!opened) {
        /* A previous slot remains usable if power was lost between renames. */
        path[11]='B'; path[12]='A'; path[13]='K';
        opened=card_open(file,path);
    }
#endif
    if (!opened) return 0;
    int ready=file[6]==REFERENCE_BYTES && !file[7] &&
        card_read(file,asset,sizeof(*asset),REFERENCE_BYTES,1);
    return card_close(file) && ready && reference_header_valid(asset->header);
}

static int composition_load(u32 image,u32 opacity)
{
    if (__atomic_exchange_n(&composition_asset_busy,1,__ATOMIC_ACQUIRE)) return 0;
    __atomic_store_n(&composition_image_ready,0,__ATOMIC_RELEASE);
    __atomic_store_n(&composition_requested,0,__ATOMIC_RELEASE);
#ifdef Z72_WIFI_GALLERY
    composition_loaded_upload=__atomic_load_n(&gallery_reference_uploads[image],__ATOMIC_ACQUIRE);
#endif
    int ready=composition_read(image,&composition_asset);
    if (ready) {
        u32 alpha=composition_alpha[opacity];
        for (u32 i=3;i<sizeof(composition_asset.rgba);i+=4)
            composition_asset.rgba[i]=(unsigned char)((composition_asset.rgba[i]*alpha+127u)/255u);
        composition_select_anchors();
        publish(composition_asset.rgba,sizeof(composition_asset.rgba));
    }
    __atomic_store_n(&composition_image_ready,ready,__ATOMIC_RELEASE);
    __atomic_store_n(&composition_asset_busy,0,__ATOMIC_RELEASE);
    return ready;
}

static void composition_input(u32 packet,u32 photo_context)
{
    u32 settings=z72_config_current.reference, button=settings&Z72_REFERENCE_BUTTON_MASK;
    if (!button) return;
    if (!photo_context || (WORD(DISPLAY_OUTPUTS)&7u)!=1u) {
        __atomic_store_n(&composition_requested,0,__ATOMIC_RELEASE);
        return;
    }
    if (!packet || !__atomic_load_n(&composition_image_ready,__ATOMIC_ACQUIRE)) return;
    u32 offset=af_shortcut_held_offsets[button-1],mask=af_shortcut_masks[button-1];
    u32 pressed=WORD(packet+offset+0x0c)&mask;
    if (!pressed && (!(settings&Z72_REFERENCE_HOLD) || (WORD(packet+offset)&mask))) return;
    if (!pressed || z72_config_current.af_shortcuts[button-1] ||
        CALL(0x1063c7d5u,u32,u32)(af_shortcut_properties[button-1])) {
        __atomic_store_n(&composition_requested,0,__ATOMIC_RELEASE);
    } else if (settings&Z72_REFERENCE_HOLD) {
        __atomic_store_n(&composition_requested,1,__ATOMIC_RELEASE);
    } else {
        __atomic_fetch_xor(&composition_requested,1,__ATOMIC_ACQ_REL);
    }
#ifdef Z72_REFERENCE_DIAGNOSTIC
    if (__atomic_load_n(&composition_requested,__ATOMIC_ACQUIRE)) {
        u32 waiting=0;
        __atomic_compare_exchange_n(&composition_trace_active,&waiting,1,0,
                                    __ATOMIC_RELEASE,__ATOMIC_RELAXED);
    }
    composition_trace_record(REFERENCE_TRACE_REQUEST,0,0);
#endif
}

static int composition_live(void)
{
    return (WORD(DISPLAY_OUTPUTS)&7u)==1u && af_shortcut_context();
}

static void composition_texture(void)
{
    u32 image[23];
    CALL(0x10361035u,void,void *,u32)(image,sizeof(image));
    image[1]=1; /* Native RGBA texture. */
    image[2]=(u32)composition_asset.rgba;
    image[5]=REFERENCE_W|(REFERENCE_H<<16);
    image[7]=LCD_W|(LCD_H<<16);
    image[16]=360;
    image[19]=REFERENCE_STRIDE;
    image[20]=REFERENCE_H;
    CALL(OSD_IMAGE,void,const u32 *)(image);
}

static int composition_active_osd(u32 canvas)
{
    u32 index;
    /* Compare registry indices because the active-surface getter translates
     * buffer addresses. The native OSD allocation is a raw address. */
    return canvas && CALL(0x1056a091u,u32,u32,u32,u32,u32 *)(
        canvas,LCD_W|(LCD_H<<16),0,&index) && index &&
        index==CALL(0x1056a0dbu,u32,void)();
}

static int composition_canvas_erased(void)
{
    u32 canvas=WORD(REFERENCE_LCD_OSD);
    /* The display allocation is in native direct-access RAM. */
    if ((canvas&3u) || canvas<0xbd200000u || canvas>0xd0000000u-LCD_W*LCD_H*4)
        return 0;
    const volatile u32 *pixels=(const volatile u32 *)canvas;
    u32 checked=0;
    for (u32 row=0;row<3;++row) {
        for (u32 col=0;col<3;++col) {
            u32 anchor=composition_anchors[row][col];
            if (!anchor) continue;
            if (pixels[anchor-1]>>24) return 0;
            checked=1;
        }
    }
    return checked && composition_active_osd(canvas);
}

static u32 composition_clear(volatile u32 *message,u32 paint,u32 mode,const u32 *rect)
{
    u32 graphics=CALL(0x106449afu,u32,void)();
    if (!graphics) return 0;
    if (paint && __atomic_exchange_n(&composition_asset_busy,1,__ATOMIC_ACQUIRE)) return 0;
    if (paint && !__atomic_load_n(&composition_image_ready,__ATOMIC_ACQUIRE)) {
        __atomic_store_n(&composition_asset_busy,0,__ATOMIC_RELEASE);
        return 0;
    }
    /* The stock GUI can erase the whole primary OSD outside the clear queue.
     * Recover before the next widget draw even if the scratch route was skipped. */
    if (mode==REFERENCE_PARTIAL && paint && composition_canvas_erased())
        mode=REFERENCE_RESTORE;
    u32 refresh=mode!=REFERENCE_PARTIAL;
    u32 enabled=CALL(0x10643441u,u32,u32)(0x1131);
    u32 old_count=WORD(WORD(graphics+0x1c)+0x208), count=0;
    if (old_count>REFERENCE_CLIP_RECTS) {
        if (paint) __atomic_store_n(&composition_asset_busy,0,__ATOMIC_RELEASE);
        return 0;
    }
    int old[REFERENCE_CLIP_RECTS][4], clipped[REFERENCE_CLIP_RECTS][4];
    /* Match integer getter and setter, including the native list length. */
    if (old_count) CALL(0x10643495u,void,u32,u32,void *)(0x1106,old_count*4,old);
    if (refresh) {
        CALL(0x10643407u,void,u32,u32,const void *)(0x1106,4,composition_full_clear);
        if (!enabled) CALL(0x106433cbu,void,u32,u32)(0x1131,1);
    } else {
        if (!enabled) {
            for (u32 i=0;i<4;++i) clipped[0][i]=(int)rect[i];
            count=1;
        } else {
            int x0=(int)rect[0], y0=(int)rect[1];
            int x1=x0+(int)rect[2], y1=y0+(int)rect[3];
            for (u32 i=0;i<old_count;++i) {
                int left=clamp(old[i][0],x0,x1), bottom=clamp(old[i][1],y0,y1);
                int right=clamp((int)((u32)old[i][0]+(u32)old[i][2]),x0,x1);
                int top=clamp((int)((u32)old[i][1]+(u32)old[i][3]),y0,y1);
                if (right<=left || top<=bottom) continue;
                clipped[count][0]=left; clipped[count][1]=bottom;
                clipped[count][2]=right-left; clipped[count++][3]=top-bottom;
            }
        }
    }

    u32 payload=refresh ? (u32)composition_full_clear : message[5];
    u32 local[7]={message[0],message[1],message[2],(u32)defer_completion,message[4],payload,0};
    CALL(DRAW_DISPATCH,void,const u32 *)(local);
    if (refresh) {
        if (paint) composition_texture();
        CALL(OSD_FINISH,void,void)();
        if (mode==REFERENCE_SCRATCH) composition_restore=1;
        else composition_restore=0;
        if (mode==REFERENCE_TOGGLE) {
            __atomic_store_n(&composition_painted,paint,__ATOMIC_RELEASE);
            __atomic_store_n(&composition_redrawn,1,__ATOMIC_RELEASE);
        }
#ifdef Z72_REFERENCE_DIAGNOSTIC
        composition_trace_record(REFERENCE_TRACE_PAINT,mode,0);
#endif
    } else if (count) {
        CALL(0x10643407u,void,u32,u32,const void *)(0x1106,count*4,clipped);
        if (!enabled) CALL(0x106433cbu,void,u32,u32)(0x1131,1);
        composition_texture();
        CALL(OSD_FINISH,void,void)();
    }
    if (refresh || count) {
        CALL(0x10643407u,void,u32,u32,const void *)(0x1106,old_count*4,old);
        if (!enabled) CALL(0x106433cbu,void,u32,u32)(0x1131,0);
    }
    if (paint) __atomic_store_n(&composition_asset_busy,0,__ATOMIC_RELEASE);
    if (refresh && mode!=REFERENCE_SCRATCH)
        __atomic_store_n(&composition_present_pending,1,__ATOMIC_RELEASE);
    message[6]=local[6];
    CALL(message[3],void,u32,u32)(0x42010062u,local[6]);
    return 1;
}

static void composition_dispatch_impl(volatile u32 *message)
{
    u32 requested=__atomic_load_n(&composition_requested,__ATOMIC_ACQUIRE);
    u32 painted=__atomic_load_n(&composition_painted,__ATOMIC_ACQUIRE);
    u32 address=message[5], completion=message[3], target=BYTE(DRAW_TARGET);
    if ((requested || painted) && (target==REFERENCE_LCD_TARGET || target==REFERENCE_SCRATCH_TARGET) &&
        message[4]==DRAW_FAMILY+0x17 &&
        (completion==DRAW_COMPLETE || completion==(u32)defer_completion) && ram_words(address,7)) {
        const volatile u32 *source=(const volatile u32 *)address;
        u32 rect[4]={source[0],source[1],source[2],source[3]};
        if (!source[4] && !source[5] && !source[6] && rect[2] && rect[3] &&
            rect[0]<LCD_W && rect[1]<LCD_H && rect[2]<=LCD_W-rect[0] && rect[3]<=LCD_H-rect[1] &&
            composition_live()) {
            if (target==REFERENCE_LCD_TARGET) {
                u32 mode=REFERENCE_PARTIAL;
                if (requested!=painted) mode=REFERENCE_TOGGLE;
                else if (composition_restore && composition_active_osd(WORD(REFERENCE_LCD_OSD)))
                    mode=REFERENCE_RESTORE;
                if (composition_clear(message,requested,mode,rect)) return;
            } else if (requested && !rect[0] && !rect[1] && rect[2]==LCD_W && rect[3]==LCD_H &&
                       composition_active_osd(WORD(REFERENCE_LCD_SCRATCH_OSD))) {
                /* The temporary present starts with a native full clear.
                 * Seed it before its widgets, then seed the regular surface
                 * on its first clear after the original target is restored. */
                if (composition_clear(message,1,REFERENCE_SCRATCH,rect)) return;
            }
        }
    }
    CALL(DRAW_DISPATCH,void,const volatile u32 *)(message);
}

static void composition_dispatch(volatile u32 *message)
{
#ifdef Z72_REFERENCE_DIAGNOSTIC
    u32 event=composition_trace_graph(message);
#endif
    composition_dispatch_impl(message);
#ifdef Z72_REFERENCE_DIAGNOSTIC
    /* Completion can release the caller's original message and payload. */
    if (event) composition_trace_record(event,0,0);
#endif
}

static void composition_update(void)
{
    u32 requested=__atomic_load_n(&composition_requested,__ATOMIC_ACQUIRE);
    u32 painted=__atomic_load_n(&composition_painted,__ATOMIC_ACQUIRE);
#ifdef Z72_WIFI_GALLERY
    /* Load a replaced selected slot when the overlay is hidden in live view. */
    u32 settings=z72_config_current.reference, slot=(settings>>4)&15u;
    if (!requested && !painted && (settings&Z72_REFERENCE_BUTTON_MASK) &&
        composition_loaded_upload!=__atomic_load_n(&gallery_reference_uploads[slot],__ATOMIC_ACQUIRE) &&
        current_page()==LIVE && composition_live() && !display_work_pending())
        composition_load(slot,settings>>8);
#endif
    if (!requested && !painted && !__atomic_load_n(&composition_redrawn,__ATOMIC_ACQUIRE)) {
        composition_kick=0;
        return;
    }
    if (!composition_live()) {
        __atomic_store_n(&composition_requested,0,__ATOMIC_RELEASE);
        composition_kick=0;
        return;
    }
    if (current_page()!=LIVE || !HALF(GRAPHICS_TASK_ID) || !histogram_lcd_ready() ||
        display_work_pending()) return;
    u32 kick=requested!=painted && composition_kick!=requested+1;
    if (kick) {
        composition_kick=requested+1;
        if (BYTE(DRAW_TARGET)==REFERENCE_LCD_TARGET)
            CALL(0x105a9669u,void,const u32 *)(composition_full_clear);
    }
    u32 redrawn=__atomic_exchange_n(&composition_redrawn,0,__ATOMIC_ACQ_REL);
    if (redrawn || requested==painted) composition_kick=0;
    if (kick || redrawn) CALL(0x10554cf9u,void,void)();
}

static void composition_present(void)
{
    /* A busy LCD can defer submission until a later native GUI pass. */
    if (!__atomic_load_n(&composition_present_pending,__ATOMIC_ACQUIRE) ||
        current_page()!=LIVE || !composition_live() || display_work_pending()) return;
    if (!__atomic_exchange_n(&composition_present_pending,0,__ATOMIC_ACQ_REL)) return;
    CALL(0x105218bbu,void,void)();
#ifdef Z72_REFERENCE_DIAGNOSTIC
    composition_trace_record(REFERENCE_TRACE_PRESENT,0,0);
#endif
}

#endif
