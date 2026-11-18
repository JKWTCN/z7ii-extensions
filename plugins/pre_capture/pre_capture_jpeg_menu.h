/* SPDX-License-Identifier: MIT */
/* Owned C 1.70 JPEG pre-capture views. Include after the existing choice
 * callbacks. The caller owns jpeg_start_mode (0 SW1, 1 AF-ON, 2 either).
 * UTF-16 resources use a verified native Chinese face, never ASCII slots. */
#ifndef Z72_PRE_CAPTURE_JPEG_MENU_H
#define Z72_PRE_CAPTURE_JPEG_MENU_H

enum { JPEG_MENU_ID=0x2e80 };
static struct {
    z72_u32 header[6],title[40],rows[4][7],maps[4];
    z72_u32 events[4][CHOICE_EVENTS][4],callbacks[5];
} jpeg_menu_views[4];
static z72_u32 jpeg_menu_view,jpeg_menu_root_row,jpeg_menu_layout[4],jpeg_switch_layout[4];
static z72_u32 jpeg_menu_owned(z72_u32 header)
{
    for (z72_u32 i=0;i<4;++i) if (header==(z72_u32)jpeg_menu_views[i].header) return 1;
    return 0;
}
static void jpeg_menu_refresh(void)
{
    jpeg_menu_status=LOAD(enabled);
    z72_u32 value=jpeg_menu_status ? 0xd2u : 0xd1u;
    jpeg_menu_views[0].rows[0][5]=value;
    /* Refresh an already drawn root after the worker accepts or rejects ON. */
    if (WORD(CHOICE_PAGE+0x28)!=(z72_u32)jpeg_menu_views[0].header) return;
    z72_u32 widgets=WORD(CHOICE_PAGE+0x24);
    if (widgets && CALL(0x1063464fu,z72_u32,z72_u32,z72_u32,z72_u32)(widgets,1,0)) {
        z72_u32 status=CALL(0x1063466du,z72_u32,z72_u32,z72_u32,z72_u32)(widgets,1,0);
        if (status) CALL(0x106361cdu,void,z72_u32,z72_u32)(status,value);
    }
}
static void jpeg_menu_init(const z72_u32 *page)
{
    z72_u32 row=!jpeg_menu_view ? jpeg_menu_root_row : jpeg_menu_view==1 ? !LOAD(enabled) :
        jpeg_menu_view==2 ? duration_index : jpeg_start_mode;
    z72_u32 id=JPEG_MENU_ID+jpeg_menu_view*8+row;
    if (CALL(0x1063bf77u,z72_u32,void)()!=1)
        CALL(0x10638983u,void,const z72_u32 *,z72_u32)(page,id);
    CALL(0x1063ac0du,void,const z72_u32 *,z72_u32,z72_u32)(page,id,1);
}
static z72_u32 jpeg_menu_show(z72_u32 view)
{
    z72_u32 header=WORD(CHOICE_PAGE+0x28);
    if (view>3 || WORD(CHOICE_PAGE)!=2 ||
        (!jpeg_menu_owned(header) && (view || WORD(CHOICE_PAGE+0x24) ||
          !pre_capture_menu_source(header)))) return 1;
    jpeg_menu_refresh(); jpeg_menu_view=view;
    *(volatile unsigned short *)(CHOICE_PAGE+0x16)=0;
    *(volatile unsigned short *)(CHOICE_PAGE+0x18)=0;
    WORD(CHOICE_PAGE+0x10)=(z72_u32)jpeg_menu_views[view].callbacks;
    WORD(CHOICE_PAGE+0x28)=(z72_u32)jpeg_menu_views[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,z72_u32,z72_u32)(2,0); return 0;
}
static z72_u32 open_jpeg_menu(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ (void)a; (void)b; (void)c; (void)d; jpeg_menu_root_row=0; return jpeg_menu_show(0); }
static z72_u32 jpeg_menu_back(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ (void)a; (void)b; (void)c; (void)d; return jpeg_menu_show(0); }
static z72_u32 jpeg_menu_pick(z72_u32 row)
{
    if (!jpeg_menu_view) { jpeg_menu_root_row=row; return jpeg_menu_show(row+1); }
    if (jpeg_menu_view==1) {
        if (row) choose_off(0,0,0,0); else choose_on(0,0,0,0);
    } else if (jpeg_menu_view==2) select_time(row);
    else {
        if (LOAD(in_flight) || LOAD(half_latch) || LOAD(fallback[3]) || LOAD(cycle_done)) {
            return 0;
        }
        jpeg_start_mode=row;
    }
    return jpeg_menu_show(0);
}
static z72_u32 jpeg_menu_pick0(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ (void)a; (void)b; (void)c; (void)d; return jpeg_menu_pick(0); }
static z72_u32 jpeg_menu_pick1(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ (void)a; (void)b; (void)c; (void)d; return jpeg_menu_pick(1); }
static z72_u32 jpeg_menu_pick2(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ (void)a; (void)b; (void)c; (void)d; return jpeg_menu_pick(2); }
static z72_u32 jpeg_menu_pick3(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ (void)a; (void)b; (void)c; (void)d; return jpeg_menu_pick(3); }

static z72_u32 jpeg_menu_prepare(void)
{
    const z72_u32 *stock=(const z72_u32 *)0xb18545fcu;
    const z72_u32 *setup=(const z72_u32 *)Z72_SETUP_MENU;
    const z72_u32 *callbacks=(const z72_u32 *)0x10969c14u;
    if (CALL(0x1061daafu,z72_u32,z72_u32,z72_u32)(2,0)!=CHOICE_PAGE ||
        WORD(SETUP_PAGE)!=0x01ed0002u || WORD(SETUP_PAGE+0x28)!=Z72_SETUP_MENU ||
        stock[0]!=0x9008 || stock[1]!=0x1187c500 || stock[2]!=0x117ccfe8 ||
        stock[3]!=0x117d4f34 || stock[4]!=0x118111bc || stock[5]!=0x20002 ||
        setup[1]!=0x1187c3d0u || setup[3]!=0x117d2e48u ||
        setup[4]!=0x11810d08u || setup[5]!=0x8002au) return 1;
    for (z72_u32 i=0;i<11;++i) if (WORD(CHOICE_PAGE+4*i)) return 1;
    const z72_u32 counts[]={3,2,4,3};
    const z72_u32 titles[]={Z72_TEXT_PRECAP_TITLE,Z72_TEXT_PRECAP_SWITCH,Z72_TEXT_PRECAP_TIME,Z72_TEXT_PRECAP_START};
    const z72_u32 *stock_rows=(const z72_u32 *)stock[3];
    const z72_u32 *stock_events=(const z72_u32 *)((const z72_u32 *)stock[4])[0];
    const z72_u32 *setup_rows=(const z72_u32 *)setup[3];
    const z72_menu_callback actions[]={jpeg_menu_pick0,jpeg_menu_pick1,jpeg_menu_pick2,jpeg_menu_pick3};
    if (stock_events[(CHOICE_EVENTS-1)*4]!=0x31a) return 1;
    jpeg_menu_layout[0]=3;
    jpeg_switch_layout[0]=2;
    for (z72_u32 i=1;i<4;++i) {
        jpeg_menu_layout[i]=((const z72_u32 *)setup[1])[i];
        jpeg_switch_layout[i]=jpeg_menu_layout[i];
    }
    for (z72_u32 view=0;view<4;++view) {
        z72_u32 *header=jpeg_menu_views[view].header,*title=jpeg_menu_views[view].title;
        for (z72_u32 i=0;i<6;++i) header[i]=stock[i];
        header[1]=view==0 ? (z72_u32)jpeg_menu_layout :
            view==1 ? (z72_u32)jpeg_switch_layout : view==2 ? 0x1187c530u : 0x1187c510u;
        if (!view) header[0]=setup[0];
        for (z72_u32 i=0;i<16;++i) title[i]=((const z72_u32 *)stock[2])[i];
        title[0]=titles[view]; title[1]=1;
        for (z72_u32 i=16;i<40;++i) title[i]=0;
        if (view<=1) for (z72_u32 line=0;line<2;++line) {
            z72_u32 *hint=title+16+line*8;
            for (z72_u32 i=0;i<8;++i) hint[i]=title[i];
            hint[0]=Z72_TEXT_PRECAP_HINT1+line;
            hint[4]=((276u+line*44u)<<16)|78u;
            hint[5]=((316u+line*44u)<<16)|696u;
            hint[7]=(0x0a27u+line)<<16;
        }
        header[2]=(z72_u32)title; header[3]=(z72_u32)jpeg_menu_views[view].rows;
        header[4]=(z72_u32)jpeg_menu_views[view].maps;
        header[5]=(counts[view]<<16)|counts[view];
        for (z72_u32 row=0;row<counts[view];++row) {
            z72_u32 *item=jpeg_menu_views[view].rows[row];
            for (z72_u32 col=0;col<7;++col) item[col]=view ? stock_rows[col] : setup_rows[col];
            item[0]=view==0 ? Z72_TEXT_PRECAP_SWITCH+row : view==1 ? Z72_TEXT_PRECAP_ON+row :
                view==2 ? Z72_TEXT_PRECAP_03+row : Z72_TEXT_PRECAP_SW1+row;
            item[1]=item[2]=0xffff; item[3]=0;
            item[4]=view ? 0x1000u : 0x1001u;
            item[5]=view ? 0xffffu : row ? 0xd7u : 0xd1u;
            item[6]=JPEG_MENU_ID+view*8+row;
            for (z72_u32 i=0;i<CHOICE_EVENTS;++i) {
                z72_u32 *event=jpeg_menu_views[view].events[row][i];
                for (z72_u32 col=0;col<4;++col) event[col]=stock_events[i*4+col];
                if (event[0]==0x257 || event[0]==0x25c || event[0]==0x27d || event[0]==0x252) {
                    event[1]=(z72_u32)actions[row]; event[2]=0; event[3]=0x430;
                }
                if (event[0]==0x256 || event[0]==0x271 || event[0]==0x2cf || event[0]==0x27f) {
                    event[1]=view ? (z72_u32)jpeg_menu_back : 0; event[2]=0;
                    event[3]=view ? 0x430 : 0x431;
                }
            }
            jpeg_menu_views[view].maps[row]=(z72_u32)jpeg_menu_views[view].events[row];
        }
        for (z72_u32 i=0;i<5;++i) jpeg_menu_views[view].callbacks[i]=callbacks[i];
        jpeg_menu_views[view].callbacks[0]=(z72_u32)jpeg_menu_init;
    }
    duration_index=3; jpeg_start_mode=2; return 0;
}
#endif
