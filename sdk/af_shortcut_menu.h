/* SPDX-License-Identifier: MIT */
#ifndef Z72_AF_SHORTCUT_MENU_H
#define Z72_AF_SHORTCUT_MENU_H

/* Use the native Setup scrolling layout for photo f2 assignment lists.
 * The specialized f2 renderer has fixed per-category ROM layouts that cannot
 * describe added rows. Preserve native IDs/actions, not that private renderer. */
#include "af_control_bindings.h"
enum { AF_MENU_BUTTONS=5, AF_MENU_ROWS=47, AF_MENU_EVENTS=16, AF_MENU_NONE_EVENTS=15,
       AF_MENU_TOTAL_ROWS=161, AF_MENU_AF_ROW=1, AF_MENU_REFERENCE_ROW=2,
       AF_MENU_ROW_ID=0x2f10, AF_MENU_REFERENCE_ID=0x2f15,
       AF_MENU_CHOICE_ID=0x2f20, AF_MENU_REFERENCE_CHOICE_ID=AF_MENU_CHOICE_ID+Z72_AF_SHORTCUT_TARGETS };
static const u32 af_menu_pages[]={0xb2bbb800,0xb2bbb8b0,0xb2bbb960,0xb2bbba10,0xb2bbba68};
static const u32 af_menu_headers[]={0xb1850b64,0xb1850bc4,0xb1850c24,0xb1850c84,0xb1850cb4};
static const u32 af_menu_callback_tables[]={0x10968e54,0x10968ea4,0x10968ef4,0x10968f44,0x10968f6c};
static const u32 af_menu_counts[]={45,45,11,24,26};
static const u32 af_menu_none[]={0x106c8a71,0x106c9605,0x106c966d,0x106c9835,0x106c9969};
static const u32 af_menu_symbols[]={0x12e6,0x18c,0x12e5,0x12e8,0x12e7,
                                      0x15a7,0x15a8,0x18b,0x1585,0x1586};
static const u32 af_menu_root_rows[]={0,1,2,4,5};
static struct {
    u32 header[6],title[24],rows[AF_MENU_ROWS][7],maps[AF_MENU_ROWS],callbacks[5];
} af_menus[AF_MENU_BUTTONS];
static u32 af_menu_events[AF_MENU_TOTAL_ROWS][AF_MENU_EVENTS][4];
static struct {
    u32 header[6],title[24],rows[Z72_AF_SHORTCUT_TARGETS][7],maps[Z72_AF_SHORTCUT_TARGETS];
    u32 events[Z72_AF_SHORTCUT_TARGETS][ROTATION_EVENTS][4],callbacks[5];
} af_choice;
static u32 af_reference_header[6],af_reference_title[24],af_reference_rows[2][7];
static u32 af_choice_button,af_root_callbacks[5];
static u32 af_description_widget,af_description_native_draw;

static u32 af_menu_header_owned(u32 header)
{ return header==(u32)af_choice.header || header==(u32)af_reference_header; }

static u32 af_menu_button(u32 page)
{
    for (u32 i=0;i<AF_MENU_BUTTONS;++i) if (page==af_menu_pages[i]) return i;
    return AF_MENU_BUTTONS;
}

static void af_menu_save(struct z72_config config)
{
    if (config.reference!=z72_config_current.reference)
        __atomic_store_n(&composition_requested,0,__ATOMIC_RELEASE);
    z72_config_save(config);
}

static void af_menu_reconcile(void)
{
    struct z72_config config=z72_config_current;
    u32 changed=0;
    for (u32 i=0;i<AF_MENU_BUTTONS;++i) {
        u32 reference=(config.reference&Z72_REFERENCE_BUTTON_MASK)==i+1;
        if ((config.af_shortcuts[i] || reference) &&
            CALL(0x1063c7d5u,u32,u32)(af_shortcut_properties[i])) {
            config.af_shortcuts[i]=0;
            if (reference) config.reference&=~Z72_REFERENCE_BUTTON_MASK;
            changed=1;
        } else if (reference && config.af_shortcuts[i]) {
            config.reference&=~Z72_REFERENCE_BUTTON_MASK;
            changed=1;
        }
    }
    if (changed) af_menu_save(config);
}

static u32 af_menu_target_count(void)
{
#ifdef Z72_WIDE_AF
    if (rotation_menu.title[0]) return Z72_AF_SHORTCUT_TARGETS;
#endif
    return Z72_AF_SHORTCUT_NATIVE_TARGETS;
}

static u32 af_menu_target_icon(u32 target)
{
    if (!target || target>af_menu_target_count()) return 0xd7;
#ifdef Z72_SUBJECT_TRACKING
    if (target==Z72_AF_SHORTCUT_TRACKING_TARGET)
        return wide_icons_load() ? TRACK_ICON : af_menu_symbols[7];
#endif
#ifdef Z72_WIDE_AF
    if (target>Z72_AF_SHORTCUT_NATIVE_TARGETS)
        return wide_icons_load() ? wide_icon_id(target-Z72_AF_SHORTCUT_NATIVE_TARGETS,0) : af_menu_symbols[4];
#endif
    return af_menu_symbols[target-1];
}

static u32 af_reference_assigned(u32 button)
{
    return af_reference_header[5] && (z72_config_current.reference&Z72_REFERENCE_BUTTON_MASK)==button+1;
}

static u32 af_menu_symbol(u32 button)
{
    u32 value=af_menu_target_icon(z72_config_current.af_shortcuts[button]);
    if (value!=0xd7) return value;
    return af_reference_assigned(button) ? rotation_value(1) : 0xd7;
}

static void af_menu_set_status(u32 widgets,u32 row,u32 value)
{
    if (widgets && CALL(0x1063464fu,u32,u32,u32,u32)(widgets,row,0)) {
        u32 widget=CALL(0x1063466du,u32,u32,u32,u32)(widgets,row,0);
        if (widget) CALL(0x106361cdu,void,u32,u32)(widget,value);
    }
}

static u32 af_menu_target_label(u32 target)
{
    if (!target || target>af_menu_target_count()) return 0;
#ifdef Z72_SUBJECT_TRACKING
    if (target==Z72_AF_SHORTCUT_TRACKING_TARGET) return Z72_TEXT_SUBJECT_TRACKING;
#endif
#ifdef Z72_WIDE_AF
    if (target>Z72_AF_SHORTCUT_NATIVE_TARGETS)
        return Z72_TEXT_WIDE_C1+target-Z72_AF_SHORTCUT_NATIVE_TARGETS-1;
#endif
    return WORD(0x11824624u+(target-1)*28);
}

static void af_menu_refresh_values(u32 button)
{
    u32 widgets=WORD(af_menu_pages[button]+0x24);
    u32 value=af_menu_target_icon(z72_config_current.af_shortcuts[button]);
    af_menus[button].rows[AF_MENU_AF_ROW][5]=value;
    af_menu_set_status(widgets,AF_MENU_AF_ROW,value);
    if (!af_reference_header[5]) return;
    value=af_reference_assigned(button) ? rotation_value(1) : 0xd7;
    af_menus[button].rows[AF_MENU_REFERENCE_ROW][5]=value;
    af_menu_set_status(widgets,AF_MENU_REFERENCE_ROW,value);
}

static u32 af_description_label(const u32 *page)
{
    u32 row=HALF((u32)page+0x16)+HALF((u32)page+0x18);
    const u32 *header=(const u32 *)page[10];
    if (row>=(header[5]&0xffffu)) return 0;
    u32 id=WORD(header[3]+row*28+24);
    for (u32 button=0;button<AF_MENU_BUTTONS;++button) {
        if (id!=WORD(0x116e2040u+af_menu_root_rows[button]*28+24)) continue;
        if (af_reference_assigned(button)) return Z72_TEXT_REFERENCE_TITLE;
        return af_menu_target_label(z72_config_current.af_shortcuts[button]);
    }
    return 0;
}

static u32 af_description_draw(u32 widget,u32 predicate)
{
    /* Run inside the native window's existing draw pass, before the original
     * text renderer establishes its clip/geometry. Never draw a window here. */
    if (widget==af_description_widget) {
        u32 label=af_description_label((const u32 *)0xb2bbb7d4u);
        if (label && WORD(widget+8)!=label)
            CALL(0x106361cdu,void,u32,u32)(widget,label);
    }
    return CALL(af_description_native_draw,u32,u32,u32)(widget,predicate);
}

static void af_menu_description(const u32 *page)
{
    u32 widget=CALL(0x10635cefu,u32,u32,u32)(0x86b,page[3]);
    if (!widget) return;
    u32 draw=WORD(widget+0x4c);
    if (!draw || draw==(u32)af_description_draw) return;
    af_description_widget=widget;
    af_description_native_draw=draw;
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(widget+0x4c)=(u32)af_description_draw;
}

static void af_menu_summary(const u32 *page)
{
    af_menu_reconcile();
    if (!page[9]) return;
    for (u32 i=0;i<AF_MENU_BUTTONS;++i) {
        af_menu_refresh_values(i);
        u32 value=af_menu_symbol(i);
        if (value!=0xd7) af_menu_set_status(page[9],af_menu_root_rows[i],value);
    }
    af_menu_description(page);
}

static void af_menu_root_init(const u32 *page)
{
    CALL(0x106c227bu,void,const u32 *)(page);
    af_menu_summary(page);
}
static void af_menu_root_update(const u32 *page)
{
    CALL(0x106c24bfu,void,const u32 *)(page);
    af_menu_summary(page);
}

static void af_menu_init(const u32 *page)
{
    u32 button=af_menu_button((u32)page);
    if (button==AF_MENU_BUTTONS) return;
    af_menu_reconcile();
    af_menu_refresh_values(button);
    /* Child native dialogs may have installed category scrolling callbacks.
     * This list uses only the generic native focus/scrollbar controller. */
    CALL(0x106230f7u,void,u32)(0);
    CALL(0x106230fdu,void,u32)(0);
    u32 value=CALL(0x1063c7d5u,u32,u32)(af_shortcut_properties[button]);
    const u32 *stock=(const u32 *)af_menu_headers[button];
    u32 id=value<84 ? af_control_bindings[button][value] :
                     WORD(stock[3]+(af_menu_counts[button]-1)*28+24);
    if (z72_config_current.af_shortcuts[button]) id=AF_MENU_ROW_ID+button;
    if (af_reference_assigned(button)) id=AF_MENU_REFERENCE_ID+button;
    if ((int)CALL(0x10638957u,u32,const u32 *,u32)(page,id)<0)
        id=WORD(stock[3]+(af_menu_counts[button]-1)*28+24);
    CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 af_menu_clear(u32 a,u32 b,u32 c,u32 d)
{
    u32 button=af_menu_button(CALL(0x1063c2b3u,u32,void)());
    if (button==AF_MENU_BUTTONS) return 0;
    u32 result=CALL(af_menu_none[button],u32,u32,u32,u32,u32)(a,b,c,d);
    struct z72_config config=z72_config_current;
    config.af_shortcuts[button]=0;
    if ((config.reference&Z72_REFERENCE_BUTTON_MASK)==button+1)
        config.reference&=~Z72_REFERENCE_BUTTON_MASK;
    af_menu_save(config);
    af_menu_refresh_values(button);
    return result;
}

static void af_choice_init(const u32 *page)
{
    u32 target=z72_config_current.af_shortcuts[af_choice_button];
    u32 id=page[10]==(u32)af_reference_header ?
        AF_MENU_REFERENCE_CHOICE_ID+!!(z72_config_current.reference&Z72_REFERENCE_HOLD) :
        AF_MENU_CHOICE_ID+(target && target<=af_menu_target_count() ? target-1 : 1);
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 af_choice_open(const u32 *choice)
{
    u32 button=af_menu_button(CALL(0x1063c2b3u,u32,void)());
    u32 header=WORD(ROTATION_PAGE+0x28);
    u32 known=header==(u32)rotation_menu.header || af_menu_header_owned(header) ||
        disp_header_owned(header) || zebra_header_owned(header);
#ifdef Z72_FTP_FULL
    known=known || ftp_header_owned(header);
#endif
#ifdef Z72_JPEG_PRE_CAPTURE
    known=known || jpeg_menu_owned(header);
#endif
#ifdef Z72_WIDE_AF
    known=known || wide_header_owned(header);
#endif
#ifdef Z72_SHUTTER_SOUND
    known=known || sound_header_owned(header);
#endif
#ifdef Z72_PIXEL_SHIFT
    known=known || pixel_header_owned(header);
#endif
#ifdef Z72_FILTERED_PLAYBACK
    known=known || filter_header_owned(header);
#endif
    if (button==AF_MENU_BUTTONS || WORD(ROTATION_PAGE)!=2 || WORD(ROTATION_PAGE+0x24) || !known) return 1;
    af_choice_button=button;
#ifdef Z72_WIDE_AF
    for (u32 i=Z72_AF_SHORTCUT_NATIVE_TARGETS;i<af_menu_target_count();++i)
        af_choice.rows[i][5]=af_menu_target_icon(i+1);
#endif
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)af_choice.callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)choice;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

static u32 af_menu_open(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return af_choice_open(af_choice.header);
}

static u32 af_reference_open(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return af_choice_open(af_reference_header);
}

static u32 af_choice_pick(u32 a,u32 b,u32 c,u32 d)
{
    u32 page=CALL(0x1063c2b3u,u32,void)();
    if (page!=ROTATION_PAGE) return 0;
    u32 header=WORD(page+0x28),reference=header==(u32)af_reference_header;
    if (!af_menu_header_owned(header)) return 0;
    u32 row=HALF(page+0x16)+HALF(page+0x18),button=af_choice_button;
    if (row>=(reference ? 2u : af_menu_target_count()) || button>=AF_MENU_BUTTONS) return 0;
    /* Reuse native None to preserve paired power-zoom assignment cleanup.
     * No unsupported function ID is ever stored in the native properties. */
    CALL(af_menu_none[button],u32,u32,u32,u32,u32)(a,b,c,d);
    if (CALL(0x1063c7d5u,u32,u32)(af_shortcut_properties[button])) return 0;
    struct z72_config config=z72_config_current;
    u32 previous=config.reference&Z72_REFERENCE_BUTTON_MASK;
    if (reference) {
        config.af_shortcuts[button]=0;
        config.reference=(config.reference&~(Z72_REFERENCE_BUTTON_MASK|Z72_REFERENCE_HOLD)) |
            (button+1) | (row ? Z72_REFERENCE_HOLD : 0);
        if (!__atomic_load_n(&composition_image_ready,__ATOMIC_ACQUIRE))
            composition_load((config.reference>>4)&15u,config.reference>>8);
    } else {
        config.af_shortcuts[button]=row+1;
        if (previous==button+1) config.reference&=~Z72_REFERENCE_BUTTON_MASK;
    }
    af_menu_save(config);
    af_menu_refresh_values(button);
    if (reference && previous && previous!=button+1) af_menu_refresh_values(previous-1);
    return 0; /* Native event destination 431 returns to the assignment list. */
}

static u32 af_menu_ready(void)
{
    if (WORD(0xb2bbb7d4u+0x28)!=0xb1850b4c || WORD(0xb2bbb7d4u+0x10)!=0x10968e18) return 0;
    for (u32 i=0;i<AF_MENU_BUTTONS;++i) {
        const u32 *header=(const u32 *)af_menu_headers[i];
        if (WORD(af_menu_pages[i]+0x28)!=af_menu_headers[i] ||
            WORD(af_menu_pages[i]+0x10)!=af_menu_callback_tables[i] ||
            header[0]!=0x1030 || header[5]!=(0x70000u|af_menu_counts[i])) return 0;
        u32 map=WORD(header[4]+(af_menu_counts[i]-1)*4);
        u32 first=WORD(header[4]+4);
        if (WORD(map+(AF_MENU_NONE_EVENTS-1)*16)!=0x31a ||
            WORD(first+(AF_MENU_EVENTS-1)*16)!=0x31a) return 0;
    }
    return 1;
}

static void af_menu_install(void)
{
    const u32 *setup=(const u32 *)Z72_SETUP_MENU;
    const u32 *generic=(const u32 *)WORD(0x118111bcu);
    static const u32 titles[]={0x2b2,0x2b3,0x4ba,0x4d5,0x70b};
    u32 added=composition_views[REFERENCE_ROOT].header[5] ? 2u : 1u;
    u32 offset=0;
    for (u32 button=0;button<AF_MENU_BUTTONS;++button) {
        const u32 *stock=(const u32 *)af_menu_headers[button];
        u32 count=af_menu_counts[button];
        for (u32 i=0;i<6;++i) af_menus[button].header[i]=setup[i];
        for (u32 i=0;i<24;++i) af_menus[button].title[i]=rotation_menu.title[i];
        af_menus[button].title[0]=titles[button];
        af_menus[button].title[1]=1;
        for (u32 row=0;row<count+added;++row) {
            /* Put the shortcuts immediately below the original Press heading.
             * Original stable IDs, labels, icons and confirmation actions stay. */
            u32 custom=row && row<=added;
            u32 source=custom ? count-1 : row ? row-added : 0;
            u32 *item=af_menus[button].rows[row];
            for (u32 i=0;i<7;++i) item[i]=WORD(stock[3]+source*28+i*4);
            if (custom) {
                item[0]=row==AF_MENU_AF_ROW ? 0x22a : Z72_TEXT_REFERENCE_TITLE;
                item[1]=item[2]=0xffff;
                item[4]=0x1001; item[5]=0xd7;
                item[6]=(row==AF_MENU_AF_ROW ? AF_MENU_ROW_ID : AF_MENU_REFERENCE_ID)+button;
            }
            const u32 *native=(const u32 *)WORD(stock[4]+source*4);
            for (u32 i=0;i<AF_MENU_EVENTS;++i) {
                u32 *entry=af_menu_events[offset+row][i];
                for (u32 col=0;col<4;++col) entry[col]=generic[i*4+col];
                u32 ev=entry[0];
                if (ev==0x257 || ev==0x25c || ev==0x27d || ev==0x252 || ev==0x27f) {
                    /* The generic list owns focus, scrolling and Back. Only
                     * confirmation/help routes come from the original row. */
                    entry[1]=entry[2]=0; entry[3]=0x430;
                    u32 wanted=ev==0x25c ? 0x257 : ev;
                    for (u32 j=0;j<AF_MENU_EVENTS;++j) {
                        if (native[j*4]==0x31a) break;
                        if (native[j*4]!=wanted) continue;
                        for (u32 col=1;col<4;++col) entry[col]=native[j*4+col];
                        break;
                    }
                    if (custom) {
                        entry[1]=entry[2]=0; entry[3]=0x430;
                        if (ev==0x257 || ev==0x25c || ev==0x27d)
                            entry[1]=(u32)(row==AF_MENU_AF_ROW ? af_menu_open : af_reference_open);
                    } else if (source==count-1 && (ev==0x257 || ev==0x25c || ev==0x27d))
                        entry[1]=(u32)af_menu_clear;
                }
            }
            af_menus[button].maps[row]=(u32)af_menu_events[offset+row];
        }
        offset+=count+added;
        for (u32 i=0;i<5;++i)
            af_menus[button].callbacks[i]=WORD(af_menu_callback_tables[button]+i*4);
        af_menus[button].callbacks[0]=(u32)af_menu_init;
        af_menus[button].header[2]=(u32)af_menus[button].title;
        af_menus[button].header[3]=(u32)af_menus[button].rows;
        af_menus[button].header[4]=(u32)af_menus[button].maps;
        af_menus[button].header[5]=0x80000u|(count+added);
    }
    /* An ordinary native scrolling choice list, independent of the f2
     * controller's private tables. Stock modes retain their original rows. */
    const u32 *stock=(const u32 *)Z72_SETUP_MENU,*native_af=(const u32 *)0xb185600cu;
    const u32 *events=(const u32 *)WORD(0x118111bcu);
    for (u32 i=0;i<6;++i) af_choice.header[i]=stock[i];
    for (u32 i=0;i<24;++i) af_choice.title[i]=rotation_menu.title[i];
    af_choice.title[0]=0x22a; af_choice.title[1]=1;
    af_choice.header[2]=(u32)af_choice.title;
    af_choice.header[3]=(u32)af_choice.rows;
    af_choice.header[4]=(u32)af_choice.maps;
    u32 targets=af_menu_target_count();
    af_choice.header[5]=0x80000|targets;
    for (u32 row=0;row<targets;++row) {
        u32 source=row<Z72_AF_SHORTCUT_NATIVE_TARGETS ? row : 4;
        for (u32 i=0;i<7;++i) af_choice.rows[row][i]=WORD(native_af[3]+source*28+i*4);
        if (row>=Z72_AF_SHORTCUT_NATIVE_TARGETS)
            af_choice.rows[row][0]=af_menu_target_label(row+1);
        af_choice.rows[row][1]=af_choice.rows[row][2]=0xffff;
        af_choice.rows[row][4]=0x1001;
        af_choice.rows[row][6]=AF_MENU_CHOICE_ID+row;
        for (u32 i=0;i<ROTATION_EVENTS;++i) {
            u32 *entry=af_choice.events[row][i];
            for (u32 col=0;col<4;++col) entry[col]=events[i*4+col];
            u32 ev=entry[0];
            if (ev==0x257 || ev==0x25c || ev==0x27d) {
                entry[1]=(u32)af_choice_pick; entry[2]=0; entry[3]=0x431;
            }
            if (ev==0x252 || ev==0x27f) { entry[1]=entry[2]=0; entry[3]=0x430; }
        }
        af_choice.maps[row]=(u32)af_choice.events[row];
    }
    if (added==2) {
        for (u32 i=0;i<6;++i) af_reference_header[i]=af_choice.header[i];
        for (u32 i=0;i<24;++i) af_reference_title[i]=af_choice.title[i];
        af_reference_title[0]=Z72_TEXT_REFERENCE_TITLE;
        af_reference_header[2]=(u32)af_reference_title;
        af_reference_header[3]=(u32)af_reference_rows;
        af_reference_header[5]=0x80002;
        for (u32 row=0;row<2;++row) {
            u32 *item=af_reference_rows[row];
            for (u32 i=0;i<7;++i) item[i]=WORD(stock[3]+i*4);
            item[0]=Z72_TEXT_REFERENCE_TOGGLE+row;
            item[1]=item[2]=item[5]=0xffff;
            item[4]=0x1000;
            item[6]=AF_MENU_REFERENCE_CHOICE_ID+row;
        }
    }
    for (u32 i=0;i<5;++i) {
        af_choice.callbacks[i]=rotation_menu.callbacks[i];
        af_root_callbacks[i]=WORD(0x10968e18u+i*4);
    }
    af_choice.callbacks[0]=(u32)af_choice_init;
    af_root_callbacks[0]=(u32)af_menu_root_init;
    af_root_callbacks[4]=(u32)af_menu_root_update;
    __asm__ volatile("dmb sy" ::: "memory");
    for (u32 i=0;i<AF_MENU_BUTTONS;++i) {
        WORD(af_menu_pages[i]+0x10)=(u32)af_menus[i].callbacks;
        WORD(af_menu_pages[i]+0x28)=(u32)af_menus[i].header;
    }
    WORD(0xb2bbb7d4u+0x10)=(u32)af_root_callbacks;
}
#endif
