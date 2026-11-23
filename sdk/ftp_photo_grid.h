/* SPDX-License-Identifier: MIT */
#ifndef Z72_FTP_PHOTO_GRID_H
#define Z72_FTP_PHOTO_GRID_H

#include "ftp_photo_grid_layout.h"

enum { FTP_GRID_THUMBNAILS, FTP_GRID_PREVIEW, FTP_GRID_VIEWS };
enum { FTP_GRID_CARD=0x3b14, FTP_GRID_IMAGE=0x3b10, FTP_GRID_ROW=0x2d70 };
enum { FTP_GRID_TILES=FTP_PHOTO_GRID_TILES, FTP_GRID_RENDER_TABLE=0xb208f558u, FTP_GRID_STOCK_RENDER=0x109724a4u };
enum { FTP_GRID_DOUBLE_TAP_MS=450 };
enum { FTP_GRID_INPUT_SLOT=0xb1824f14u, FTP_GRID_STOCK_INPUT=0x1044f6e1u };
enum { FTP_GRID_INPUT_READ=0xb181d309u };
enum { FTP_GRID_INPUT_EVENT=3, FTP_GRID_TOUCH_PACKET=0x50 };

static struct {
    u32 header[FTP_GRID_VIEWS][6], callbacks[5], maps[FTP_GRID_VIEWS];
    u32 thumbnails[116][8], preview[8][8], row[7], render[9];
    u32 ready,active,view,transition,returning,scene_open,grid_planes;
    u32 saved_card,saved_image,saved_display;
    struct { u32 id,selected; } marks[FTP_BATCH]; /* Edits awaiting OK. */
    u32 mark_count,resume_image;
    u32 block_touch_ok,pressed_image,pressed_tick,tapped_image,tapped_tick;
} ftp_grid;

static u32 ftp_grid_input(u32 event,u32 message,u32 c,u32 d);

static u32 ftp_grid_header_owned(u32 header)
{
    return header==(u32)ftp_grid.header[FTP_GRID_THUMBNAILS] ||
           header==(u32)ftp_grid.header[FTP_GRID_PREVIEW];
}

static int ftp_grid_is_visible(u32 view)
{
    return ftp_grid.active && ftp_grid.scene_open && !ftp_grid.transition &&
           ftp_grid.view==view && WORD(ROTATION_PAGE+0x28)==(u32)ftp_grid.header[view] &&
           CALL(0x1063c2b3u,u32,void)()==ROTATION_PAGE;
}

static u32 ftp_grid_find_mark(u32 id,u32 count)
{
    for (u32 i=0;i<count;++i)
        if (ftp_media_key(ftp_load(&ftp_grid.marks[i].id))==ftp_media_key(id)) return i;
    return count;
}

static void ftp_grid_clear_marks(void)
{
    ftp_store(&ftp_grid.mark_count,0);
    ftp_grid.resume_image=0;
}

static u32 ftp_grid_tile_image(u32 index)
{
    if (index>=FTP_GRID_TILES || index>=BYTE(0xb208f565u)) return 0;
    u32 tile=CALL(0x1070b7b1u,u32,u32)(index);
    return tile ? WORD(tile) : 0;
}

static void ftp_grid_draw_mark(u32 index)
{
    if (!ftp_grid.scene_open || ftp_grid.view!=FTP_GRID_THUMBNAILS || index>=FTP_GRID_TILES) return;
    u32 id=ftp_grid_tile_image(index), marked=0;
    if (id>>29==1u) {
        u32 count=ftp_load(&ftp_grid.mark_count), mark=ftp_grid_find_mark(id,count);
        marked=mark<count ? ftp_load(&ftp_grid.marks[mark].selected) : ftp_marked(id);
    }
    CALL(0x105dd265u,void,u32,u32)(index,marked);
}

static void ftp_grid_set_planes(u32 grid)
{
    /* The native transition compares two page headers. FTP reuses one slot,
     * so switch the image surface to the selector's geometry explicitly. */
    CALL(0x10637289u,void,u32,u32,u32,u32,u32)(7,1,grid ? 2 : 0,0,!grid);
    if (grid)
        CALL(0x10637289u,void,u32,u32,u32,u32,u32)(4,1,2,0,1);
    u32 alternate=CALL(0x10631947u,u32,void)()==4;
    CALL(0x10637289u,void,u32,u32,u32,u32,u32)(4,2,0,0,grid ? !alternate : alternate);
    ftp_grid.grid_planes=grid;
}

static void ftp_grid_close_scene(u32 leaving)
{
    if (!ftp_grid.scene_open) return;
    ftp_grid.scene_open=0;
    if (WORD(FTP_GRID_INPUT_SLOT)==(u32)ftp_grid_input)
        WORD(FTP_GRID_INPUT_SLOT)=FTP_GRID_STOCK_INPUT;
    ftp_grid.pressed_image=ftp_grid.tapped_image=0;
    if (WORD(FTP_GRID_RENDER_TABLE)==(u32)ftp_grid.render)
        WORD(FTP_GRID_RENDER_TABLE)=FTP_GRID_STOCK_RENDER;
    if (ftp_grid.view==FTP_GRID_PREVIEW)
        CALL(0x1061d141u,void,void)();
    if (leaving)
        /* Stock schedules this reset when the selector is removed from the menu stack. */
        CALL(0x1065602bu,void,void)();
    else if (ftp_grid.view==FTP_GRID_THUMBNAILS)
        CALL(0x10655e19u,u32,u32,u32,void *,void *,void *)(0x32,0,0,0,0);
}

static void ftp_leave_photo_grid(void)
{
    if (!ftp_grid.active) return;
    if (ftp_grid.scene_open && ftp_grid.view==FTP_GRID_THUMBNAILS &&
        (ftp_load(&ftp_grid.mark_count) || ftp_pending())) {
        u32 id=ftp_grid_tile_image(CALL(0x1070f1b5u,u32,void)());
        if (id>>29==1u) ftp_grid.resume_image=id;
    }
    ftp_grid_close_scene(1);
    if (ftp_grid.grid_planes) ftp_grid_set_planes(0);
    ftp_grid.active=0;
    CALL(0x1063c7e1u,void,u32,u32)(FTP_GRID_CARD,ftp_grid.saved_card);
    CALL(0x1063c7e1u,void,u32,u32)(FTP_GRID_IMAGE,ftp_grid.saved_image);
    CALL(0x106167e1u,void,u32)(ftp_grid.saved_display);
    CALL(0x1062deedu,u32,u32,u32)(5,0);
}

static void ftp_grid_destroy(const u32 *page)
{
    if ((u32)page==ROTATION_PAGE && !ftp_grid.transition) ftp_leave_photo_grid();
}

static void ftp_grid_init(const u32 *page)
{
    if ((u32)page!=ROTATION_PAGE || !ftp_grid.active) return;
    ftp_grid.scene_open=1;
    if (ftp_grid.view==FTP_GRID_PREVIEW) {
        CALL(0x1070d81fu,void,const u32 *)(page);
        return;
    }
    /* Follow the native renderer's preview-return path for image focus. */
    u32 returning=ftp_grid.returning;
    unsigned char previous=BYTE(0xb181b5ccu);
    if (returning) BYTE(0xb181b5ccu)=1;
    CALL(0x1070d473u,void,const u32 *,u32)(page,0x5d);
    if (returning) BYTE(0xb181b5ccu)=previous;
    if (WORD(FTP_GRID_RENDER_TABLE)==FTP_GRID_STOCK_RENDER) {
        /* Let scrolling and delayed thumbnail draws paint the FTP marks. */
        __asm__ volatile("dmb sy" ::: "memory");
        WORD(FTP_GRID_RENDER_TABLE)=(u32)ftp_grid.render;
        for (u32 i=0;i<FTP_GRID_TILES;++i) ftp_grid_draw_mark(i);
    }
    if (WORD(FTP_GRID_INPUT_SLOT)==FTP_GRID_STOCK_INPUT)
        WORD(FTP_GRID_INPUT_SLOT)=(u32)ftp_grid_input;
}

static int ftp_grid_show(u32 view,u32 returning)
{
    ftp_grid_close_scene(0);
    ftp_grid.transition=1;
    ftp_grid.view=view;
    ftp_grid.returning=returning;
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)ftp_grid.callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)ftp_grid.header[view];
    __asm__ volatile("dmb sy" ::: "memory");
    u32 shown=CALL(0x1063a7a7u,u32,u32,u32)(2,ROTATION_PAGE_ID);
    ftp_grid.transition=0;
    if (shown)
        CALL(0x10635a8bu,void,u32,u32,u32)(0x106355a1u,3,7);
    return shown!=0;
}

static u32 ftp_grid_back(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return ftp_show(FTP_MENU_ROOT,0);
}

static void ftp_grid_toggle_tile(u32 index)
{
    u32 id=ftp_grid_tile_image(index);
    if (id>>29!=1u) return;
    u32 count=ftp_load(&ftp_grid.mark_count), mark=ftp_grid_find_mark(id,count);
    if (mark<count) {
        ftp_store(&ftp_grid.marks[mark].id,ftp_load(&ftp_grid.marks[count-1].id));
        ftp_store(&ftp_grid.marks[mark].selected,ftp_load(&ftp_grid.marks[count-1].selected));
        ftp_store(&ftp_grid.mark_count,count-1);
    } else {
        u32 selected=!ftp_marked(id);
        char path[FTP_PATH_BYTES];
        if (count==FTP_BATCH || (selected && !ftp_media_path(id,path))) return;
        ftp_store(&ftp_grid.marks[count].id,id);
        ftp_store(&ftp_grid.marks[count].selected,selected);
        ftp_store(&ftp_grid.mark_count,count+1);
    }
    ftp_grid_draw_mark(index);
    ftp_key_redraw();
}

static u32 ftp_grid_toggle(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (!ftp_grid_is_visible(FTP_GRID_THUMBNAILS)) return 1;
    ftp_grid_toggle_tile(CALL(0x1070f1b5u,u32,void)());
    return 0;
}

static void ftp_grid_touch(u32 touch)
{
    if (BYTE(touch+9)!=1) {
        ftp_grid.pressed_image=ftp_grid.tapped_image=0;
        return;
    }
    u32 index=CALL(0x1072db59u,u32,u32)(touch);
    u32 id=ftp_grid_tile_image(index);
    if (id>>29!=1u) {
        ftp_grid.pressed_image=ftp_grid.tapped_image=0;
        return;
    }
    u32 now=ftp_main_tick();
    if (BYTE(touch+8)==1) {
        ftp_grid.pressed_image=id;
        ftp_grid.pressed_tick=now;
        return;
    }
    u32 pressed=ftp_grid.pressed_image;
    ftp_grid.pressed_image=0;
    if (id!=pressed || now-ftp_grid.pressed_tick>FTP_GRID_DOUBLE_TAP_MS) {
        ftp_grid.tapped_image=0;
    } else if (id==ftp_grid.tapped_image && now-ftp_grid.tapped_tick<=FTP_GRID_DOUBLE_TAP_MS) {
        ftp_grid.tapped_image=0;
        ftp_grid_toggle_tile(index);
    } else {
        ftp_grid.tapped_image=id;
        ftp_grid.tapped_tick=now;
    }
}

static u32 ftp_grid_input(u32 event,u32 message,u32 c,u32 d)
{
    u32 (*stock)(u32,u32,u32,u32)=CALL(FTP_GRID_STOCK_INPUT,u32,u32,u32,u32,u32);
    u32 packet[15];
    if (event!=FTP_GRID_INPUT_EVENT || !message || WORD(message) ||
        !ftp_grid_is_visible(FTP_GRID_THUMBNAILS) ||
        CALL(0x1053c3f3u,u32,u32,u32,u32 *)(0,0,packet)!=FTP_GRID_TOUCH_PACKET)
        return stock(event,message,c,d);

    /* The gallery's hit test and focus handler use the raw touch coordinates.
     * Setup applies its coordinate conversion only to ordinary menu widgets. */
    u32 touch=(u32)packet, state=ftp_grid.block_touch_ok;
    u32 read=BYTE(FTP_GRID_INPUT_READ);
    u32 bounds=CALL(0x1070c249u,u32,u32)(1);
    ftp_grid.block_touch_ok=CALL(0x103fae65u,u32,u32,u32)(bounds,touch+0x12);
    u32 result=stock(event,message,c,d);
    ftp_grid.block_touch_ok=state;
    if (BYTE(FTP_GRID_INPUT_READ)!=read && ftp_grid_is_visible(FTP_GRID_THUMBNAILS))
        ftp_grid_touch(touch);
    return result;
}

static u32 ftp_grid_confirm(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (!ftp_grid_is_visible(FTP_GRID_THUMBNAILS)) return 1;
    if (ftp_grid.block_touch_ok) return 0;
    if (!ftp_claim_job()) return 1;

    ftp_compact_batch();
    ftp_store(&ftp_queue_full,0);
    u32 count=ftp_load(&ftp_grid.mark_count);
    for (u32 i=0;i<count;++i) {
        u32 id=ftp_load(&ftp_grid.marks[i].id);
        if (!ftp_load(&ftp_grid.marks[i].selected) && ftp_marked(id)) ftp_unmark(id);
    }
    for (u32 i=0;i<count;++i) {
        if (!ftp_load(&ftp_grid.marks[i].selected)) continue;
        struct ftp_photo photo;
        photo.id=ftp_load(&ftp_grid.marks[i].id);
        if (!ftp_marked(photo.id) && ftp_media_path(photo.id,photo.path) && !ftp_mark(&photo)) break;
    }
    ftp_store(&ftp_job,FTP_JOB_NONE);
    if (ftp_load(&ftp_queue_full)) {
        ftp_status_parent=FTP_MENU_ROOT;
        return ftp_show(FTP_MENU_STATUS,3);
    }
    ftp_store(&ftp_grid.mark_count,0);
    return ftp_show(FTP_MENU_ROOT,0);
}

static u32 ftp_grid_end_preview(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (!ftp_grid_is_visible(FTP_GRID_PREVIEW)) return 1;
    return ftp_grid_show(FTP_GRID_THUMBNAILS,1) ? 0 : ftp_show(FTP_MENU_ROOT,0);
}

static u32 ftp_grid_check_zoom_button(u32 a,u32 b,u32 c,u32 d)
{
    if (CALL(0x1053c48bu,u32,u32,u32,u32,u32,u32)(0,8,0,0,1)) return 0;
    return ftp_grid_end_preview(a,b,c,d);
}

static u32 ftp_grid_zoom(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (!ftp_grid_is_visible(FTP_GRID_THUMBNAILS)) return 1;
    u32 id=CALL(0x1063c7d5u,u32,u32)(FTP_GRID_IMAGE);
    if (id>>29!=1u) return 1;
    int index=CALL(0x1070dbffu,int,u32)(id);
    if (index<0) return 1;
    u32 image=CALL(0x1070b7b1u,u32,int)(index);
    if (!image || (HALF(image+0xa)&0x100)) return 1;
    CALL(0x10618511u,void,u32)(!CALL(0x1053cb13u,u32,void)());
    if (ftp_grid_show(FTP_GRID_PREVIEW,0)) return 0;
    if (ftp_grid_show(FTP_GRID_THUMBNAILS,1)) return 1;
    return ftp_show(FTP_MENU_ROOT,0);
}

static int ftp_open_photo_grid(void)
{
    if (!ftp_grid.ready || ftp_grid.active ||
        CALL(0x1063c2b3u,u32,void)()!=ROTATION_PAGE ||
        !CALL(0x1063c7d5u,u32,u32)(0x2f01) ||
        CALL(0x1062e919u,u32,void)() || CALL(0x1071ac9bu,u32,void)()) return 0;

    if (!CALL(0x1062dea5u,u32,u32,u32)(5,0)) return 0;
    ftp_grid.saved_card=CALL(0x1063c7d5u,u32,u32)(FTP_GRID_CARD);
    ftp_grid.saved_image=CALL(0x1063c7d5u,u32,u32)(FTP_GRID_IMAGE);
    ftp_grid.saved_display=CALL(0x10616d5fu,u32,void)();
    ftp_grid.active=1;
    CALL(0x1063c7e1u,void,u32,u32)(FTP_GRID_CARD,1);
    if (ftp_grid.resume_image>>29==1u)
        CALL(0x1063c7e1u,void,u32,u32)(FTP_GRID_IMAGE,ftp_grid.resume_image);
    else if (ftp_grid.saved_image>>29!=1u)
        CALL(0x1063c7e1u,void,u32,u32)(FTP_GRID_IMAGE,0);
    CALL(0x106167e1u,void,u32)(1);
    ftp_grid_set_planes(1);
    if (ftp_grid_show(FTP_GRID_THUMBNAILS,0)) return 1;
    ftp_leave_photo_grid();
    return 0;
}

static void ftp_prepare_photo_grid(void)
{
    /* Use the standard playback multi-select renderer and the zoom widget.
     * FTP owns all selection, confirmation and exit events. */
    static const u32 widgets[]={FTP_PHOTO_GRID_WIDGETS,FTP_PHOTO_PREVIEW_WIDGETS};
    static const u32 layouts[]={0x02252002u,0x00251040u};
    static const u32 thumbnail_events[][4]={
        {0x24f,0,0,1}, {0x250,0x1063aec7u,0x1063af65u,0x430}, {0x251,0x1063aeedu,0,0x430},
        {0x254,0x107249a7u,0,0x430}, {0x255,0x107249adu,0,0x430},
        {0x256,0x107249a1u,0,0x430}, {0x257,0x1072499bu,0,0x430},
        {0x258,0x107249b3u,0,0x430}, {0x259,0x107249b9u,0,0x430},
        {0x25a,0x107249bfu,0,0x430}, {0x25b,0x107249c5u,0,0x430},
        {0x25c,(u32)ftp_grid_toggle,0,0x430}, {0x26e,0x107249a1u,0x1063af55u,0x430},
        {0x26f,0x1072499bu,0x1063af55u,0x430}, {0x270,0x107249cbu,0x1063af55u,0x430},
        {0x271,0x107249cfu,0x1063af55u,0x430},
        {0x27d,(u32)ftp_grid_confirm,0,0x430}, {0x27e,(u32)ftp_grid_zoom,0,0x430},
        {0x27f,(u32)ftp_grid_toggle,0,0x430},
        {0x2cf,(u32)ftp_grid_back,0,0x430}, {0x31a,0,0,0x430}
    };
    static const u32 preview_events[][4]={
        {0x24f,0,0,1}, {0x250,0x1063aec7u,0x1063af65u,0x430}, {0x251,0x1063aeedu,0,0x430},
        {0x2ce,0x1061cc97u,0,0x430}, {0x2cf,(u32)ftp_grid_end_preview,0,0x430},
        {0x2d1,(u32)ftp_grid_end_preview,0x1061cc99u,0x430},
        {0x24a,(u32)ftp_grid_check_zoom_button,0,0x430},
        {0x13b,(u32)ftp_grid_end_preview,0,0x430}, {0x31a,0,0,0x430}
    };
    const u32 *native_render=(const u32 *)FTP_GRID_STOCK_RENDER;
    if (native_render[4]!=0x1070f0d3u || native_render[8]!=0) return;
    for (u32 i=0;i<9;++i) ftp_grid.render[i]=native_render[i];
    ftp_grid.render[4]=(u32)ftp_grid_draw_mark;

    ftp_grid.row[1]=ftp_grid.row[2]=ftp_grid.row[5]=0xffffu;
    ftp_grid.row[4]=0x400;
    ftp_grid.row[6]=FTP_GRID_ROW;
    ftp_grid.maps[FTP_GRID_THUMBNAILS]=(u32)thumbnail_events;
    ftp_grid.maps[FTP_GRID_PREVIEW]=(u32)preview_events;
    for (u32 menu=0;menu<FTP_GRID_VIEWS;++menu) {
        const u32 (*component)[8]=(const u32 (*)[8])widgets[menu];
        u32 records=menu==FTP_GRID_THUMBNAILS ?
            sizeof(ftp_grid.thumbnails)/sizeof(ftp_grid.thumbnails[0]) :
            sizeof(ftp_grid.preview)/sizeof(ftp_grid.preview[0]);
        if (component[0][1]!=1 || component[records-1][6]!=1 || component[records-1][7]!=1) return;
        u32 (*titles)[8]=menu==FTP_GRID_THUMBNAILS ? ftp_grid.thumbnails : ftp_grid.preview;
        for (u32 row=0;row<records;++row)
            for (u32 col=0;col<8;++col) titles[row][col]=component[row][col];
        titles[0][0]=FTP_WT7_UPLOAD_TEXT;
        if (menu==FTP_GRID_THUMBNAILS) {
            titles[1][0]=0x333;
            /* Use the photo selector's mark glyph for the six selected states. */
            for (u32 i=0;i<FTP_GRID_TILES;++i) titles[23+i][0]=0x7c4;
        }
        u32 *header=ftp_grid.header[menu];
        header[0]=layouts[menu];
        header[1]=0x1187c340u;
        header[2]=(u32)titles;
        header[3]=(u32)ftp_grid.row;
        header[4]=(u32)&ftp_grid.maps[menu];
        header[5]=0x10001u;
    }
    ftp_grid.callbacks[0]=(u32)ftp_grid_init;
    ftp_grid.callbacks[1]=(u32)ftp_grid_destroy;
    ftp_grid.callbacks[2]=0x1063b1e5u;
    ftp_grid.ready=1;
}
#endif
