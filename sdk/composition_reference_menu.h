/* SPDX-License-Identifier: MIT */
#ifndef Z72_COMPOSITION_REFERENCE_MENU_H
#define Z72_COMPOSITION_REFERENCE_MENU_H

enum { REFERENCE_ROOT, REFERENCE_OPACITY, REFERENCE_GRID };
enum { REFERENCE_MENU_ID=0x2dc0u, REFERENCE_MAX_ROWS=5u };
static const u32 composition_rows[]={2,Z72_REFERENCE_OPACITIES};
static struct {
    u32 header[6],title[32],rows[REFERENCE_MAX_ROWS][7],maps[REFERENCE_MAX_ROWS];
    u32 events[REFERENCE_MAX_ROWS][ROTATION_EVENTS][4];
} composition_views[REFERENCE_GRID];
static u32 composition_callbacks[5],composition_layout[4];
static u32 composition_view,composition_root_row,composition_menu_title;

static u32 composition_show(u32 view);
static void composition_menu_choose_reference(u32 slot);
#include "composition_reference_picker.h"

static u32 composition_header_owned(u32 header)
{
    for (u32 view=0;view<REFERENCE_GRID;++view)
        if (header==(u32)composition_views[view].header) return 1;
    return header==(u32)composition_picker_page.header;
}

static void composition_page_init(const u32 *page)
{
    u32 view=composition_view;
    u32 row=view==REFERENCE_OPACITY ? z72_config_current.reference>>8 :
                   composition_root_row;
    u32 id=REFERENCE_MENU_ID+view*8+row;
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 composition_show(u32 view)
{
    u32 header=WORD(ROTATION_PAGE+0x28);
    if (WORD(ROTATION_PAGE)!=2 ||
#ifdef Z72_JPEG_PRE_CAPTURE
        (!pre_capture_menu_source(header) && !jpeg_menu_owned(header)) ||
#else
        (header!=(u32)rotation_menu.header && !zebra_header_owned(header)) ||
#endif
        (WORD(ROTATION_PAGE+0x24) && !composition_header_owned(header))) return 1;
    composition_view=view;
    composition_views[REFERENCE_ROOT].title[0]=composition_menu_title;
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    if (view==REFERENCE_GRID) {
        WORD(ROTATION_PAGE+0x10)=(u32)composition_picker_callbacks;
        WORD(ROTATION_PAGE+0x28)=(u32)composition_picker_page.header;
    } else {
        WORD(ROTATION_PAGE+0x10)=(u32)composition_callbacks;
        WORD(ROTATION_PAGE+0x28)=(u32)composition_views[view].header;
    }
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

static u32 open_composition_menu(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    composition_root_row=0;
    composition_menu_title=Z72_TEXT_REFERENCE_TITLE;
    return composition_show(REFERENCE_ROOT);
}

static u32 composition_menu_back(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return composition_show(REFERENCE_ROOT);
}

static void composition_menu_choose_reference(u32 slot)
{
    struct z72_config config=z72_config_current;
    u32 previous=(config.reference>>4)&15u, opacity=config.reference>>8;
    if (composition_load(slot,opacity)) {
        config.reference=(config.reference&~(15u<<4))|(slot<<4);
        z72_config_save(config);
    } else {
        composition_load(previous,opacity);
        composition_menu_title=Z72_TEXT_REFERENCE_UNAVAILABLE;
    }
    composition_show(REFERENCE_ROOT);
}

static u32 composition_menu_select(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    u32 view=composition_view;
    const u32 *page=(const u32 *)CALL(0x1063c2b3u,u32,void)();
    if (view>=REFERENCE_GRID || (u32)page!=ROTATION_PAGE ||
        page[10]!=(u32)composition_views[view].header) return 1;
    u32 row=HALF(ROTATION_PAGE+0x16)+HALF(ROTATION_PAGE+0x18);
    if (row>=composition_rows[view]) return 1;
    composition_menu_title=Z72_TEXT_REFERENCE_TITLE;
    if (!view) {
        composition_root_row=row;
        if (row) return composition_show(REFERENCE_OPACITY);
        composition_picker_load();
        return composition_show(REFERENCE_GRID);
    }
    struct z72_config config=z72_config_current;
    config.reference=(config.reference&0xffu)|(row<<8);
    if (!composition_load((config.reference>>4)&15u,row))
        composition_menu_title=Z72_TEXT_REFERENCE_UNAVAILABLE;
    z72_config_save(config);
    return composition_show(REFERENCE_ROOT);
}

static void composition_menu_prepare(void)
{
    const u32 *setup=(const u32 *)Z72_SETUP_MENU;
    const u32 *base_rows=(const u32 *)setup[3],*layout=(const u32 *)setup[1];
    const u32 *choice_title=(const u32 *)WORD(0xb18512e4u+8);
    const u32 *events=(const u32 *)WORD(0x118111bcu);
    composition_layout[0]=7;
    for (u32 i=1;i<4;++i) composition_layout[i]=layout[i]+8;
    for (u32 view=0;view<REFERENCE_GRID;++view) {
        u32 *header=composition_views[view].header,*title=composition_views[view].title;
        for (u32 i=0;i<6;++i) header[i]=setup[i];
        for (u32 i=0;i<8;++i) {
            title[i]=rotation_menu.title[i];
            title[8+i]=view ? choice_title[8+i] : rotation_menu.title[8+i];
            if (view) title[16+i]=choice_title[24+i];
        }
        title[0]=Z72_TEXT_REFERENCE_TITLE;
        title[1]=1;
        if (view) {
            title[8]=Z72_TEXT_REFERENCE_OPACITY;
            title[9]=1;
            header[1]=(u32)composition_layout;
        }
        header[2]=(u32)title;
        header[3]=(u32)composition_views[view].rows;
        header[4]=(u32)composition_views[view].maps;
        u32 count=composition_rows[view];
        header[5]=(count<<16)|count;
        for (u32 row=0;row<count;++row) {
            u32 *item=composition_views[view].rows[row];
            for (u32 i=0;i<7;++i) item[i]=base_rows[i];
            item[0]=view==REFERENCE_ROOT ? Z72_TEXT_REFERENCE_IMAGE+row : Z72_TEXT_REFERENCE_PERCENT+row;
            item[1]=item[2]=item[5]=0xffff;
            item[4]=0x1000u;
            item[6]=REFERENCE_MENU_ID+view*8+row;
            for (u32 i=0;i<ROTATION_EVENTS;++i) {
                u32 *entry=composition_views[view].events[row][i];
                for (u32 col=0;col<4;++col) entry[col]=events[i*4+col];
                u32 event=entry[0];
                if (event==0x257 || event==0x25c || event==0x27d || event==0x252) {
                    entry[1]=(u32)composition_menu_select; entry[2]=0; entry[3]=0x430;
                }
                if (event==0x256 || event==0x271 || event==0x2cf || event==0x27f) {
                    entry[1]=view ? (u32)composition_menu_back : 0; entry[2]=0;
                    entry[3]=view ? 0x430 : 0x431;
                }
            }
            composition_views[view].maps[row]=(u32)composition_views[view].events[row];
        }
    }
    for (u32 i=0;i<5;++i) composition_callbacks[i]=rotation_menu.callbacks[i];
    composition_callbacks[0]=(u32)composition_page_init;
    composition_picker_prepare();
    u32 settings=z72_config_current.reference;
    if (settings&Z72_REFERENCE_BUTTON_MASK) composition_load((settings>>4)&15u,settings>>8);
}

#endif
