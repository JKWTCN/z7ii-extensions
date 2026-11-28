/* SPDX-License-Identifier: MIT */
#ifndef Z72_COMPOSITION_REFERENCE_PICKER_H
#define Z72_COMPOSITION_REFERENCE_PICKER_H

#include "ftp_photo_grid_layout.h"

enum { REFERENCE_PICKER_WIDGET=0x2de4, REFERENCE_PICKER_LABEL=0x2de8,
       REFERENCE_PICKER_FOOTER=2+Z72_REFERENCE_SLOTS,
       REFERENCE_THUMB_W=REFERENCE_W/2, REFERENCE_THUMB_H=REFERENCE_H/2,
       REFERENCE_THUMB_STRIDE=(REFERENCE_THUMB_W+15)&~15,
       REFERENCE_TILE_X=92, REFERENCE_TILE_Y=96, REFERENCE_TILE_W=184, REFERENCE_TILE_H=126,
       REFERENCE_TILE_STEP_X=200, REFERENCE_TILE_STEP_Y=154, REFERENCE_TILE_LABEL_H=28 };
_Static_assert(FTP_PHOTO_GRID_TILES==Z72_REFERENCE_SLOTS,"All reference slots fit on one page");

static struct {
    u32 header[6],title[FTP_PHOTO_GRID_TILES+5][8],map;
} composition_picker_page;
static u32 composition_picker_callbacks[5];
static u32 composition_picker_focus,composition_picker_images;
typedef unsigned char composition_thumb[REFERENCE_THUMB_STRIDE*REFERENCE_THUMB_H*4];
static composition_thumb *composition_picker_thumbs;

static struct box composition_picker_tile(u32 index)
{
    return (struct box){REFERENCE_TILE_X+(int)(index%FTP_PHOTO_GRID_COLUMNS)*REFERENCE_TILE_STEP_X,
        REFERENCE_TILE_Y+(int)(index/FTP_PHOTO_GRID_COLUMNS)*REFERENCE_TILE_STEP_Y,
        REFERENCE_TILE_W,REFERENCE_TILE_H};
}

static u32 composition_picker_xy(int x,int y) { return (u32)x|((u32)y<<16); }

static void composition_picker_fill(struct box box,u32 color,u32 origin)
{
    u32 rect[]={composition_picker_xy(box.x,box.y),composition_picker_xy(box.x+box.w,box.y+box.h)};
    const u32 paint[]={2,color}; /* Native 0xRRGGBBAA solid paint. */
    CALL(0x1074c3cdu,void,const u32 *,const u32 *,const u32 *,u32)(rect,&origin,paint,0);
}

static void composition_picker_image(const unsigned char *pixels,struct box dest)
{
    u32 image[23];
    CALL(0x10361035u,void,void *,u32)(image,sizeof(image));
    image[1]=1;
    image[2]=(u32)pixels;
    image[5]=composition_picker_xy(REFERENCE_THUMB_W,REFERENCE_THUMB_H);
    /* The texture renderer applies its own Y transform. Feeding it the
     * bottom-left rectangle used for solid fills swaps the thumbnail rows. */
    image[6]=composition_picker_xy(dest.x,dest.y);
    image[7]=composition_picker_xy(dest.x+dest.w,dest.y+dest.h);
    image[16]=360;
    image[19]=REFERENCE_THUMB_STRIDE;
    image[20]=REFERENCE_THUMB_H;
    CALL(0x10739985u,void,void)();
    CALL(0x105aa2dbu,void,const u32 *)(image); /* Synchronous native 0x4201001a. */
    CALL(0x105a8b47u,void,void)();
    CALL(0x10739989u,void,void)();
}

static void composition_picker_paint(u32 widget)
{
    CALL(0x10739c3bu,void,u32)(widget);
    if (!WORD(widget+0x44)) return;
    u32 origin=CALL(0x10739c47u,u32,u32)(widget);
    u32 selected=(z72_config_current.reference>>4)&15u;
    for (u32 i=0;i<Z72_REFERENCE_SLOTS;++i) {
        struct box tile=composition_picker_tile(i);
        u32 border=i==composition_picker_focus ? 0xf4c95affu : i==selected ? 0x91cbf5ffu : 0x666666ffu;
        composition_picker_fill(tile,border,origin);
        struct box inset={tile.x+2,tile.y+2,tile.w-4,tile.h-4};
        composition_picker_fill(inset,0x272727ffu,origin);
        if (composition_picker_images&(1u<<i)) {
            struct box picture={tile.x+2,tile.y+4,REFERENCE_THUMB_W,REFERENCE_THUMB_H};
            composition_picker_image(composition_picker_thumbs[i],picture);
        }
    }
}

static void composition_picker_touch_widget(u32 widget,u32 state) { (void)widget; (void)state; }

static u32 composition_picker_widget(const u32 *page)
{
    return page[3] ? CALL(0x10635cefu,u32,u32,u32)(REFERENCE_PICKER_WIDGET,page[3]) : 0;
}

static void composition_picker_init(const u32 *page)
{
    u32 widget=composition_picker_widget(page);
    if (!widget || HALF(widget+4)!=6 || WORD(widget+0x4c)!=0x106eefb3u) return;
    CALL(0x1062262fu,void,u32,void (*)(u32,u32))(widget,composition_picker_touch_widget);
    CALL(0x106232efu,void,u32,u32,u32,u32)(widget,0x05cb02cbu,0,0);
    WORD(widget+0xcc)=0;
    WORD(widget+0x4c)=(u32)composition_picker_paint;
}

static void composition_picker_redraw(void)
{
    const u32 *page=(const u32 *)ROTATION_PAGE;
    u32 widget=composition_picker_widget(page);
    if (!widget) return;
    CALL(0x1063618bu,void,u32,u32)(widget,1);
    CALL(0x1063618bu,void,u32,u32)(WORD(widget+0x48),1);
    CALL(0x1061db49u,void,const u32 *)(page);
    CALL(0x10635a8bu,void,u32,u32,u32)(0x106355a1u,3,7);
}

static void composition_picker_horizontal(u32 step)
{
    u32 row=composition_picker_focus/FTP_PHOTO_GRID_COLUMNS;
    composition_picker_focus=FTP_PHOTO_GRID_COLUMNS*row+
        (composition_picker_focus+step)%FTP_PHOTO_GRID_COLUMNS;
    composition_picker_redraw();
}
static void composition_picker_up_down(void)
{
    composition_picker_focus=(composition_picker_focus+FTP_PHOTO_GRID_COLUMNS)%FTP_PHOTO_GRID_TILES;
    composition_picker_redraw();
}
static void composition_picker_left(void) { composition_picker_horizontal(FTP_PHOTO_GRID_COLUMNS-1); }
static void composition_picker_right(void) { composition_picker_horizontal(1); }

static void composition_picker_back(void) { composition_show(REFERENCE_ROOT); }

static void composition_picker_choose(void)
{
    if (composition_picker_images&(1u<<composition_picker_focus))
        composition_menu_choose_reference(composition_picker_focus);
}

static void composition_picker_touch(void)
{
    u32 touch=0,widget=0;
    CALL(0x10623521u,void,u32 *,u32 *)(&touch,&widget);
    if (!touch || widget!=composition_picker_widget((const u32 *)ROTATION_PAGE) || BYTE(touch+8)!=1)
        return;
    int x=(unsigned short)HALF(touch+0x12), y=(unsigned short)HALF(touch+0x14);
    for (u32 i=0;i<Z72_REFERENCE_SLOTS;++i) {
        struct box tile=composition_picker_tile(i);
        if (x<tile.x || x>=tile.x+tile.w || y<tile.y || y>=tile.y+tile.h+REFERENCE_TILE_LABEL_H) continue;
        composition_picker_focus=i;
        if (composition_picker_images&(1u<<i)) composition_picker_choose();
        else composition_picker_redraw();
        return;
    }
}

static void composition_picker_load(void)
{
    composition_picker_images=0;
    composition_picker_focus=(z72_config_current.reference>>4)&15u;
    u32 *hint=composition_picker_page.title[REFERENCE_PICKER_FOOTER];
    hint[0]=Z72_TEXT_REFERENCE_UNAVAILABLE;
    /* The renderer keeps stable texture addresses across page redraws. */
    if (!composition_picker_thumbs) {
        void *owner=CALL(0x1034fe31u,void *,u32,u32)(Z72_REFERENCE_SLOTS*sizeof(composition_thumb)+63u,0);
        if (!owner) return;
        composition_picker_thumbs=(composition_thumb *)(((u32)owner+63u)&~63u);
    }
    void *raw=CALL(0x1034fe31u,void *,u32,u32)(sizeof(struct composition_file)+63,0);
    if (!raw) return;
    struct composition_file *file=(struct composition_file *)(((u32)raw+63)&~63u);
    for (u32 slot=0;slot<Z72_REFERENCE_SLOTS;++slot) {
        if (!composition_read(slot,file)) continue;
        unsigned char *thumb=composition_picker_thumbs[slot];
        const unsigned char *source=file->rgba;
        for (u32 y=0;y<REFERENCE_THUMB_H;++y) {
            for (u32 x=0;x<REFERENCE_THUMB_W;++x) {
                u32 dest=4*(y*REFERENCE_THUMB_STRIDE+x), from=8*(y*REFERENCE_STRIDE+x);
                for (u32 c=0;c<4;++c) thumb[dest+c]=source[from+c];
            }
        }
        publish(thumb,sizeof(composition_picker_thumbs[slot]));
        composition_picker_images|=1u<<slot;
    }
    CALL(0x1034fe7fu,void,void *)(raw);
    if (composition_picker_images) hint[0]=Z72_TEXT_REFERENCE_PICK;
}

static void composition_picker_prepare(void)
{
    /* Reuse the native date editor's single event row and the FTP selector's
     * heading layout. RFG images have no native playback media IDs. */
    const u32 *stock=(const u32 *)0xb1853fb4u;
    static const u32 grid_events[][4]={
        {0x24f,(u32)composition_picker_back,0,0x430},
        {0x252,(u32)composition_picker_back,0,0x430},
        {0x254,(u32)composition_picker_up_down,0,0x430}, {0x255,(u32)composition_picker_up_down,0,0x430},
        {0x256,(u32)composition_picker_left,0,0x430}, {0x257,(u32)composition_picker_right,0,0x430},
        {0x25c,(u32)composition_picker_choose,0,0x430},
        {0x26e,(u32)composition_picker_up_down,0,0x430}, {0x26f,(u32)composition_picker_up_down,0,0x430},
        {0x270,(u32)composition_picker_right,0,0x430}, {0x271,(u32)composition_picker_left,0,0x430},
        {0x27d,(u32)composition_picker_choose,0,0x430},
        {0x27f,(u32)composition_picker_back,0,0x430}, {0x2cb,(u32)composition_picker_touch,0,0x430},
        {0x2cf,(u32)composition_picker_back,0,0x430}, {0x31a,0,0,0x430}
    };
    u32 *header=composition_picker_page.header;
    u32 (*title)[8]=composition_picker_page.title;
    const u32 *heading=(const u32 *)FTP_PHOTO_GRID_WIDGETS;
    for (u32 i=0;i<6;++i) header[i]=stock[i];
    for (u32 i=0;i<8;++i) title[0][i]=heading[i];
    title[0][0]=Z72_TEXT_REFERENCE_IMAGE;
    title[1][1]=6;
    title[1][4]=composition_picker_xy(72,78);
    title[1][5]=composition_picker_xy(694,410);
    title[1][6]=1;
    title[1][7]=(REFERENCE_PICKER_WIDGET<<16)|1;
    for (u32 i=0;i<Z72_REFERENCE_SLOTS;++i) {
        struct box tile=composition_picker_tile(i);
        u32 *label=title[2+i];
        for (u32 j=0;j<8;++j) label[j]=title[0][j];
        label[0]=Z72_TEXT_REFERENCE_SLOT+i;
        label[4]=composition_picker_xy(tile.x+4,tile.y+tile.h);
        label[5]=composition_picker_xy(tile.x+tile.w,tile.y+tile.h+REFERENCE_TILE_LABEL_H);
        label[7]=(REFERENCE_PICKER_LABEL+i)<<16;
    }
    u32 *hint=title[REFERENCE_PICKER_FOOTER];
    for (u32 j=0;j<8;++j) hint[j]=title[0][j];
    hint[0]=Z72_TEXT_REFERENCE_PICK;
    hint[4]=composition_picker_xy(76,429);
    hint[5]=composition_picker_xy(692,479);
    hint[7]=(REFERENCE_PICKER_LABEL+Z72_REFERENCE_SLOTS)<<16;
    const u32 *back=(const u32 *)FTP_PHOTO_PREVIEW_WIDGETS+6*8;
    for (u32 j=0;j<8;++j) title[3+Z72_REFERENCE_SLOTS][j]=back[j];
    title[4+Z72_REFERENCE_SLOTS][6]=title[4+Z72_REFERENCE_SLOTS][7]=1;
    header[2]=(u32)title;
    header[4]=(u32)&composition_picker_page.map;
    composition_picker_page.map=(u32)grid_events;
    for (u32 i=0;i<5;++i) composition_picker_callbacks[i]=rotation_menu.callbacks[i];
    composition_picker_callbacks[0]=(u32)composition_picker_init;
}

#endif
