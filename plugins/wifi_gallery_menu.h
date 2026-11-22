/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_GALLERY_MENU_H
#define Z72_WIFI_GALLERY_MENU_H

enum { GALLERY_MENU_ID=0x2ed8u };
static struct {
    u32 header[6],title[48],rows[2][7],maps[2];
    u32 events[2][ROTATION_EVENTS][4],callbacks[5];
} gallery_menu_views[2];
static u32 gallery_menu_layout[4],gallery_menu_view,gallery_menu_focus;
static u32 gallery_menu_available,gallery_menu_start_failed;

static u32 gallery_menu_owned(u32 header)
{
    return header==(u32)gallery_menu_views[0].header || header==(u32)gallery_menu_views[1].header;
}

static u32 gallery_menu_status_text(void)
{
    if (gallery_menu_start_failed) return Z72_TEXT_GALLERY_ERROR;
    if (!gallery_holds_wifi()) return Z72_TEXT_GALLERY_OFF;
    invalidate(GALLERY_STATE,64);
    switch (GALLERY_STATE->stage) {
    case GALLERY_LISTEN:
    case GALLERY_SERVE:
        if (GALLERY_STATE->download_buffer==GALLERY_BUFFER_LARGE) return Z72_TEXT_GALLERY_LARGE;
        if (GALLERY_STATE->download_buffer==GALLERY_BUFFER_NATIVE) return Z72_TEXT_GALLERY_NATIVE;
        return Z72_TEXT_GALLERY_READY;
    case GALLERY_SOCKET_ERROR: return Z72_TEXT_GALLERY_RETRY;
    case GALLERY_TASK_ERROR: return Z72_TEXT_GALLERY_ERROR;
    default: return Z72_TEXT_GALLERY_WAIT;
    }
}

static void gallery_menu_init(const u32 *page)
{
    u32 row=gallery_menu_view ? (u32)gallery_holds_wifi() : gallery_menu_focus;
    u32 id=GALLERY_MENU_ID+gallery_menu_view*8u+row;
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 gallery_menu_show(u32 view)
{
    u32 header=WORD(ROTATION_PAGE+0x28);
    if (WORD(ROTATION_PAGE)!=2 || (!gallery_menu_owned(header) &&
        (view || WORD(ROTATION_PAGE+0x24) || !pre_capture_menu_source(header)))) return 1;
    gallery_menu_views[0].rows[0][5]=rotation_value(gallery_holds_wifi());
    gallery_menu_views[0].title[32]=gallery_menu_status_text();
    gallery_menu_view=view;
    *(volatile unsigned short *)(ROTATION_PAGE+0x16)=0;
    *(volatile unsigned short *)(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)gallery_menu_views[view].callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)gallery_menu_views[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,0);
    return 0;
}

static u32 gallery_menu_open(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    gallery_menu_focus=0;
    return gallery_menu_show(0);
}

static u32 gallery_menu_back(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return gallery_menu_show(0);
}

static u32 gallery_menu_pick(u32 row)
{
    if (gallery_menu_view) {
        if (row) ftp_install_wifi_hold();
        gallery_menu_start_failed=!gallery_set_enabled((int)row);
        return gallery_menu_show(0);
    }
    gallery_menu_focus=row;
    return gallery_menu_show(!row);
}

static u32 gallery_menu_pick0(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return gallery_menu_pick(0);
}

static u32 gallery_menu_pick1(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return gallery_menu_pick(1);
}

static int gallery_menu_prepare(void)
{
    const u32 *stock=(const u32 *)0xb18545fcu,*setup=(const u32 *)Z72_SETUP_MENU;
    const u32 *callbacks=(const u32 *)0x10969c14u;
    const u32 *events=(const u32 *)((const u32 *)stock[4])[0];
    const z72_menu_callback actions[]={gallery_menu_pick0,gallery_menu_pick1};
    if (setup[1]!=0x1187c3d0u || events[(ROTATION_EVENTS-1)*4]!=0x31au ||
        !gallery_service_ready()) return 0;
    gallery_menu_layout[0]=2;
    for (u32 i=1;i<4;++i) gallery_menu_layout[i]=((const u32 *)setup[1])[i];
    for (u32 view=0;view<2;++view) {
        u32 *header=gallery_menu_views[view].header,*title=gallery_menu_views[view].title;
        for (u32 i=0;i<6;++i) header[i]=stock[i];
        if (!view) header[0]=setup[0];
        header[1]=(u32)gallery_menu_layout;
        header[2]=(u32)title;
        header[3]=(u32)gallery_menu_views[view].rows;
        header[4]=(u32)gallery_menu_views[view].maps;
        for (u32 i=0;i<16;++i) title[i]=((const u32 *)stock[2])[i];
        title[0]=Z72_TEXT_GALLERY_TITLE;
        title[1]=1;
        for (u32 line=0;line< (view ? 2u : 3u);++line) {
            u32 *hint=title+16u+line*8u;
            for (u32 i=0;i<8;++i) hint[i]=title[i];
            hint[0]=line<2u ? Z72_TEXT_GALLERY_HINT1+line : Z72_TEXT_GALLERY_OFF;
            hint[4]=((276u+line*44u)<<16)|78u;
            hint[5]=((316u+line*44u)<<16)|696u;
            hint[7]=(0x0a27u+line)<<16;
        }
        const u32 *rows=(const u32 *)(view ? stock[3] : setup[3]);
        for (u32 row=0;row<2;++row) {
            u32 *item=gallery_menu_views[view].rows[row];
            for (u32 col=0;col<7;++col) item[col]=rows[row*7+col];
            item[0]=view ? Z72_TEXT_GALLERY_OFF+row : Z72_TEXT_GALLERY_TITLE+row;
            item[1]=item[2]=0xffffu;
            item[3]=0;
            item[4]=view ? 0x1000u : 0x1001u;
            item[5]=view ? 0xffffu : row ? 0xd7u : 0xd1u;
            item[6]=GALLERY_MENU_ID+view*8u+row;
            for (u32 i=0;i<ROTATION_EVENTS;++i) {
                u32 *event=gallery_menu_views[view].events[row][i];
                for (u32 col=0;col<4;++col) event[col]=events[i*4+col];
                if (event[0]==0x257 || event[0]==0x25c || event[0]==0x27d || event[0]==0x252) {
                    event[1]=(u32)actions[row]; event[2]=0; event[3]=0x430;
                }
                if (event[0]==0x256 || event[0]==0x271 || event[0]==0x2cf || event[0]==0x27f) {
                    event[1]=view ? (u32)gallery_menu_back : 0; event[2]=0;
                    event[3]=view ? 0x430 : 0x431;
                }
            }
            gallery_menu_views[view].maps[row]=(u32)gallery_menu_views[view].events[row];
        }
        for (u32 i=0;i<5;++i) gallery_menu_views[view].callbacks[i]=callbacks[i];
        gallery_menu_views[view].callbacks[0]=(u32)gallery_menu_init;
    }
    return 1;
}

#endif
