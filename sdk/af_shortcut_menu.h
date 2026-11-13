/* SPDX-License-Identifier: MIT */
#ifndef Z72_AF_SHORTCUT_MENU_H
#define Z72_AF_SHORTCUT_MENU_H

/* Use the native Setup scrolling layout for photo f2 assignment lists.
 * The specialized f2 renderer has fixed per-category ROM layouts that cannot
 * describe added rows. Preserve native IDs/actions, not that private renderer. */
#include "af_control_bindings.h"
enum { AF_MENU_BUTTONS=5, AF_MENU_ROWS=46, AF_MENU_EVENTS=16, AF_MENU_NONE_EVENTS=15,
       AF_MENU_TOTAL_ROWS=156,
       AF_MENU_ROW_ID=0x2f10, AF_MENU_CHOICE_ID=0x2f20 };
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
    u32 header[6],title[24],rows[10][7],maps[10],events[10][ROTATION_EVENTS][4],callbacks[5];
} af_choice;
static u32 af_choice_button,af_root_callbacks[5];
static u32 af_description_widget,af_description_native_draw;

static u32 af_menu_header_owned(u32 header)
{ return header==(u32)af_choice.header; }

static u32 af_menu_button(u32 page)
{
    for (u32 i=0;i<AF_MENU_BUTTONS;++i) if (page==af_menu_pages[i]) return i;
    return AF_MENU_BUTTONS;
}

static void af_menu_reconcile(void)
{
    struct z72_config config=z72_config_current;
    u32 changed=0;
    for (u32 i=0;i<AF_MENU_BUTTONS;++i)
        if (config.af_shortcuts[i] && CALL(0x1063c7d5u,u32,u32)(af_shortcut_properties[i])) {
            config.af_shortcuts[i]=0;
            changed=1;
        }
    if (changed) z72_config_save(config);
}

static u32 af_menu_value(u32 button)
{
    u32 target=z72_config_current.af_shortcuts[button];
    return target && target<=10 ? af_menu_symbols[target-1] : 0xd7;
}

static void af_menu_refresh_value(u32 button)
{
    u32 value=af_menu_value(button),widgets=WORD(af_menu_pages[button]+0x24);
    af_menus[button].rows[1][5]=value;
    if (widgets && CALL(0x1063464fu,u32,u32,u32,u32)(widgets,1,0)) {
        u32 widget=CALL(0x1063466du,u32,u32,u32,u32)(widgets,1,0);
        if (widget) CALL(0x106361cdu,void,u32,u32)(widget,value);
    }
}

static u32 af_description_label(const u32 *page)
{
    u32 row=HALF((u32)page+0x16)+HALF((u32)page+0x18);
    const u32 *header=(const u32 *)page[10];
    if (row>=(header[5]&0xffffu)) return 0;
    u32 id=WORD(header[3]+row*28+24);
    for (u32 button=0;button<AF_MENU_BUTTONS;++button) {
        if (id!=WORD(0x116e2040u+af_menu_root_rows[button]*28+24)) continue;
        u32 target=z72_config_current.af_shortcuts[button];
        return target && target<=10 ? WORD(0x11824624u+(target-1)*28) : 0;
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
        af_menu_refresh_value(i);
        u32 target=z72_config_current.af_shortcuts[i];
        if (!target || target>10 || !CALL(0x1063464fu,u32,u32,u32,u32)(page[9],af_menu_root_rows[i],0)) continue;
        u32 widget=CALL(0x1063466du,u32,u32,u32,u32)(page[9],af_menu_root_rows[i],0);
        if (widget) CALL(0x106361cdu,void,u32,u32)(widget,af_menu_symbols[target-1]);
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
    af_menu_refresh_value(button);
    /* Child native dialogs may have installed category scrolling callbacks.
     * This list uses only the generic native focus/scrollbar controller. */
    CALL(0x106230f7u,void,u32)(0);
    CALL(0x106230fdu,void,u32)(0);
    u32 value=CALL(0x1063c7d5u,u32,u32)(af_shortcut_properties[button]);
    const u32 *stock=(const u32 *)af_menu_headers[button];
    u32 id=value<84 ? af_control_bindings[button][value] :
                     WORD(stock[3]+(af_menu_counts[button]-1)*28+24);
    if (z72_config_current.af_shortcuts[button]) id=AF_MENU_ROW_ID+button;
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
    z72_config_save(config);
    af_menu_refresh_value(button);
    return result;
}

static void af_choice_init(const u32 *page)
{
    u32 target=z72_config_current.af_shortcuts[af_choice_button];
    u32 id=AF_MENU_CHOICE_ID+(target ? target-1 : 1);
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 af_menu_open(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    u32 button=af_menu_button(CALL(0x1063c2b3u,u32,void)());
    u32 header=WORD(ROTATION_PAGE+0x28);
    if (button==AF_MENU_BUTTONS || WORD(ROTATION_PAGE)!=2 || WORD(ROTATION_PAGE+0x24) ||
        !(header==(u32)rotation_menu.header || af_menu_header_owned(header) ||
          disp_header_owned(header) || zebra_header_owned(header))) return 1;
    af_choice_button=button;
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)af_choice.callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)af_choice.header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

static u32 af_choice_pick(u32 a,u32 b,u32 c,u32 d)
{
    u32 page=CALL(0x1063c2b3u,u32,void)();
    if (page!=ROTATION_PAGE || WORD(page+0x28)!=(u32)af_choice.header) return 0;
    u32 row=HALF(page+0x16)+HALF(page+0x18),button=af_choice_button;
    if (row>=10 || button>=AF_MENU_BUTTONS) return 0;
    /* Reuse native None to preserve paired power-zoom assignment cleanup.
     * No unsupported function ID is ever stored in the native properties. */
    CALL(af_menu_none[button],u32,u32,u32,u32,u32)(a,b,c,d);
    if (CALL(0x1063c7d5u,u32,u32)(af_shortcut_properties[button])) return 0;
    struct z72_config config=z72_config_current;
    config.af_shortcuts[button]=row+1;
    z72_config_save(config);
    af_menu_refresh_value(button);
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
    u32 offset=0;
    for (u32 button=0;button<AF_MENU_BUTTONS;++button) {
        const u32 *stock=(const u32 *)af_menu_headers[button];
        u32 count=af_menu_counts[button];
        for (u32 i=0;i<6;++i) af_menus[button].header[i]=setup[i];
        for (u32 i=0;i<24;++i) af_menus[button].title[i]=rotation_menu.title[i];
        af_menus[button].title[0]=titles[button];
        af_menus[button].title[1]=1;
        for (u32 row=0;row<=count;++row) {
            /* Put the shortcut immediately below the original Press heading.
             * Original stable IDs, labels, icons and confirmation actions stay. */
            u32 source=row==1 ? count-1 : row>1 ? row-1 : 0;
            u32 *item=af_menus[button].rows[row];
            for (u32 i=0;i<7;++i) item[i]=WORD(stock[3]+source*28+i*4);
            if (row==1) {
                item[0]=0x22a;
                item[1]=item[2]=0xffff;
                item[4]=0x1001; item[5]=af_menu_value(button);
                item[6]=AF_MENU_ROW_ID+button;
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
                    if (row==1) {
                        entry[1]=entry[2]=0; entry[3]=0x430;
                        if (ev==0x257 || ev==0x25c || ev==0x27d) entry[1]=(u32)af_menu_open;
                    } else if (source==count-1 && (ev==0x257 || ev==0x25c || ev==0x27d))
                        entry[1]=(u32)af_menu_clear;
                }
            }
            af_menus[button].maps[row]=(u32)af_menu_events[offset+row];
        }
        offset+=count+1;
        for (u32 i=0;i<5;++i)
            af_menus[button].callbacks[i]=WORD(af_menu_callback_tables[button]+i*4);
        af_menus[button].callbacks[0]=(u32)af_menu_init;
        af_menus[button].header[2]=(u32)af_menus[button].title;
        af_menus[button].header[3]=(u32)af_menus[button].rows;
        af_menus[button].header[4]=(u32)af_menus[button].maps;
        af_menus[button].header[5]=0x80000u|(count+1);
    }
    /* An ordinary native scrolling choice list, independent of the f2
     * controller's private tables. All ten labels and symbols are stock. */
    const u32 *stock=(const u32 *)Z72_SETUP_MENU,*native_af=(const u32 *)0xb185600cu;
    const u32 *events=(const u32 *)WORD(0x118111bcu);
    for (u32 i=0;i<6;++i) af_choice.header[i]=stock[i];
    for (u32 i=0;i<24;++i) af_choice.title[i]=rotation_menu.title[i];
    af_choice.title[0]=0x22a; af_choice.title[1]=1;
    af_choice.header[2]=(u32)af_choice.title;
    af_choice.header[3]=(u32)af_choice.rows;
    af_choice.header[4]=(u32)af_choice.maps;
    af_choice.header[5]=0x8000a;
    for (u32 row=0;row<10;++row) {
        for (u32 i=0;i<7;++i) af_choice.rows[row][i]=WORD(native_af[3]+row*28+i*4);
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
