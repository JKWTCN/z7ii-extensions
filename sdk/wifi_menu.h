/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_MENU_H
#define Z72_WIFI_MENU_H

enum { WIFI_MENU_ROW=3, WIFI_MENU_TITLE_SLOT=34, WIFI_MENU_LABEL_SLOT=35,
       WIFI_MENU_REGION_ID=0x2fd0u, WIFI_MENU_80_SLOT=38, WIFI_MENU_80_ID=0x2fd4u,
       WIFI_SETTINGS_SLOT=39, WIFI_SETTINGS_ID=0x2fd8u };
static struct {
    u32 header[6],title[24],rows[Z72_WIFI_REGION_COUNT][7],maps[Z72_WIFI_REGION_COUNT];
    u32 events[Z72_WIFI_REGION_COUNT][ROTATION_EVENTS][4],callbacks[5];
} wifi_menu;
static struct {
    u32 header[6],title[24],rows[2][7],maps[2],events[2][ROTATION_EVENTS][4],callbacks[5];
} wifi_80_menu;
static struct {
    u32 header[6],title[24],rows[2][7],maps[2],events[2][ROTATION_EVENTS][4],callbacks[5];
} wifi_settings_menu;
static u32 wifi_root_row;

static u32 wifi_header_owned(u32 header)
{
    return header==(u32)wifi_menu.header || header==(u32)wifi_80_menu.header ||
        header==(u32)wifi_settings_menu.header;
}

static void wifi_settings_values(void)
{
    wifi_settings_menu.rows[0][5]=0xd7u;
    wifi_settings_menu.rows[1][5]=rotation_value(z72_config_current.wifi_80mhz);
}

static void wifi_settings_page_init(const u32 *page)
{
    u32 id=WIFI_SETTINGS_ID+wifi_root_row;
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static void wifi_page_init(const u32 *page)
{
    u32 id=WIFI_MENU_REGION_ID+z72_config_current.wifi_region;
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static void wifi_80_page_init(const u32 *page)
{
    u32 id=WIFI_MENU_80_ID+!z72_config_current.wifi_80mhz;
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 wifi_show(u32 view)
{
    u32 header=WORD(ROTATION_PAGE+0x28);
    u32 owned=wifi_header_owned(header),known=header==(u32)rotation_menu.header || owned;
#ifdef Z72_PHOTO_ZEBRA
    known=known || zebra_header_owned(header);
#endif
#ifdef Z72_CUSTOM_DISP
    known=known || disp_header_owned(header);
#endif
#ifdef Z72_AF_SHORTCUT
    known=known || af_menu_header_owned(header);
#endif
    if (view>2 || WORD(ROTATION_PAGE)!=2 || !known ||
        (WORD(ROTATION_PAGE+0x24) && !owned)) return 1;
    wifi_settings_values();
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)(view==2 ? wifi_80_menu.callbacks :
        view==1 ? wifi_menu.callbacks : wifi_settings_menu.callbacks);
    WORD(ROTATION_PAGE+0x28)=(u32)(view==2 ? wifi_80_menu.header :
        view==1 ? wifi_menu.header : wifi_settings_menu.header);
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

static u32 open_wifi_settings(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    wifi_root_row=0;
    return wifi_show(0);
}

static u32 wifi_back(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; return wifi_show(0); }

static u32 open_wifi_menu(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    wifi_root_row=0;
    return wifi_show(1);
}
static u32 open_wifi_80mhz_menu(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; wifi_root_row=1; return wifi_show(2); }

static u32 wifi_80_pick(u32 enabled)
{
    struct z72_config config=z72_config_current;
    config.wifi_80mhz=enabled;
    wifi_set_80mhz(enabled);
    z72_config_save(config);
    return wifi_show(0);
}

static u32 wifi_80_enable(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; return wifi_80_pick(1); }
static u32 wifi_80_disable(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; return wifi_80_pick(0); }

static u32 wifi_pick(u32 region)
{
    if (region>=Z72_WIFI_REGION_COUNT) return 1;
    struct z72_config config=z72_config_current;
    config.wifi_region=region;
    wifi_set_region(region);
    z72_config_save(config);
    return wifi_show(0);
}

static u32 wifi_mainland(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; return wifi_pick(Z72_WIFI_REGION_MAINLAND); }
static u32 wifi_us(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; return wifi_pick(Z72_WIFI_REGION_US); }
static u32 wifi_body(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; return wifi_pick(Z72_WIFI_REGION_BODY); }

static void wifi_menu_prepare(void)
{
    const z72_menu_callback actions[]={wifi_mainland,wifi_us,wifi_body};
    static const char *const names[]={"Mainland China","United States","Follow camera"};
    const u32 *setup=(const u32 *)Z72_SETUP_MENU;
    const u32 *stock_rows=(const u32 *)setup[3];
    for (u32 i=0;i<6;++i) wifi_menu.header[i]=setup[i];
    for (u32 i=0;i<24;++i) wifi_menu.title[i]=rotation_menu.title[i];
    wifi_menu.title[0]=choice_localized ? Z72_TEXT_WIFI_REGION : Z72_MENU_SLOT+WIFI_MENU_TITLE_SLOT;
    wifi_menu.title[1]=choice_localized ? 1 : 2;
    wifi_menu.header[2]=(u32)wifi_menu.title;
    wifi_menu.header[3]=(u32)wifi_menu.rows;
    wifi_menu.header[4]=(u32)wifi_menu.maps;
    wifi_menu.header[5]=(Z72_WIFI_REGION_COUNT<<16)|Z72_WIFI_REGION_COUNT;
    for (u32 row=0;row<Z72_WIFI_REGION_COUNT;++row) {
        u32 *item=wifi_menu.rows[row];
        for (u32 col=0;col<7;++col) item[col]=stock_rows[col];
        item[0]=choice_localized ? Z72_TEXT_WIFI_CN+row : Z72_MENU_SLOT+WIFI_MENU_LABEL_SLOT+row;
        item[1]=item[2]=0xffff;
        item[3]=0;
        item[4]=choice_localized ? 0x1000u : 0x1200u;
        item[5]=0xffff;
        item[6]=WIFI_MENU_REGION_ID+row;
        for (u32 i=0;i<ROTATION_EVENTS;++i) {
            u32 *entry=wifi_menu.events[row][i];
            for (u32 col=0;col<4;++col) entry[col]=rotation_menu.events[0][i][col];
            if (entry[0]==0x257 || entry[0]==0x25c || entry[0]==0x27d || entry[0]==0x252) {
                entry[1]=(u32)actions[row]; entry[2]=0; entry[3]=0x430;
            }
            if (entry[0]==0x256 || entry[0]==0x271 || entry[0]==0x2cf || entry[0]==0x27f) {
                entry[1]=(u32)wifi_back; entry[2]=0; entry[3]=0x430;
            }
        }
        wifi_menu.maps[row]=(u32)wifi_menu.events[row];
        z72_menu_set_label(WIFI_MENU_LABEL_SLOT+row,names[row]);
    }
    for (u32 i=0;i<5;++i) wifi_menu.callbacks[i]=rotation_menu.callbacks[i];
    wifi_menu.callbacks[0]=(u32)wifi_page_init;
    z72_menu_set_label(WIFI_MENU_TITLE_SLOT,"Wi-Fi region");

    const z72_menu_callback bandwidth_actions[]={wifi_80_enable,wifi_80_disable};
    for (u32 i=0;i<6;++i) wifi_80_menu.header[i]=rotation_menu.header[i];
    for (u32 i=0;i<24;++i) wifi_80_menu.title[i]=rotation_menu.title[i];
    wifi_80_menu.title[0]=choice_localized ? Z72_TEXT_WIFI_80MHZ : Z72_MENU_SLOT+WIFI_MENU_80_SLOT;
    wifi_80_menu.title[1]=choice_localized ? 1 : 2;
    wifi_80_menu.header[2]=(u32)wifi_80_menu.title;
    wifi_80_menu.header[3]=(u32)wifi_80_menu.rows;
    wifi_80_menu.header[4]=(u32)wifi_80_menu.maps;
    for (u32 row=0;row<2;++row) {
        for (u32 col=0;col<7;++col) wifi_80_menu.rows[row][col]=rotation_menu.rows[row][col];
        wifi_80_menu.rows[row][6]=WIFI_MENU_80_ID+row;
        for (u32 i=0;i<ROTATION_EVENTS;++i) {
            for (u32 col=0;col<4;++col)
                wifi_80_menu.events[row][i][col]=rotation_menu.events[row][i][col];
            u32 *entry=wifi_80_menu.events[row][i];
            if (entry[0]==0x257 || entry[0]==0x25c || entry[0]==0x27d || entry[0]==0x252) {
                entry[1]=(u32)bandwidth_actions[row]; entry[2]=0; entry[3]=0x430;
            }
            if (entry[0]==0x256 || entry[0]==0x271 || entry[0]==0x2cf || entry[0]==0x27f) {
                entry[1]=(u32)wifi_back; entry[2]=0; entry[3]=0x430;
            }
        }
        wifi_80_menu.maps[row]=(u32)wifi_80_menu.events[row];
    }
    for (u32 i=0;i<5;++i) wifi_80_menu.callbacks[i]=rotation_menu.callbacks[i];
    wifi_80_menu.callbacks[0]=(u32)wifi_80_page_init;
    z72_menu_set_label(WIFI_MENU_80_SLOT,"Enable 80 MHz WiFi");
    const z72_menu_callback settings_actions[]={open_wifi_menu,open_wifi_80mhz_menu};
    for (u32 i=0;i<6;++i) wifi_settings_menu.header[i]=setup[i];
    for (u32 i=0;i<24;++i) wifi_settings_menu.title[i]=rotation_menu.title[i];
    wifi_settings_menu.title[0]=choice_localized ? Z72_TEXT_WIFI_SETTINGS : Z72_MENU_SLOT+WIFI_SETTINGS_SLOT;
    wifi_settings_menu.title[1]=choice_localized ? 1 : 2;
    wifi_settings_menu.header[2]=(u32)wifi_settings_menu.title;
    wifi_settings_menu.header[3]=(u32)wifi_settings_menu.rows;
    wifi_settings_menu.header[4]=(u32)wifi_settings_menu.maps;
    wifi_settings_menu.header[5]=0x20002u;
    for (u32 row=0;row<2;++row) {
        u32 *item=wifi_settings_menu.rows[row];
        for (u32 col=0;col<7;++col) item[col]=stock_rows[col];
        item[0]=choice_localized ? (row ? Z72_TEXT_WIFI_80MHZ : Z72_TEXT_WIFI_REGION) :
            Z72_MENU_SLOT+(row ? WIFI_MENU_80_SLOT : WIFI_MENU_TITLE_SLOT);
        item[1]=item[2]=0xffff;
        item[3]=0; item[4]=choice_localized ? 0x1001u : 0x1201u;
        item[6]=WIFI_SETTINGS_ID+row;
        for (u32 i=0;i<ROTATION_EVENTS;++i) {
            u32 *entry=wifi_settings_menu.events[row][i];
            for (u32 col=0;col<4;++col) entry[col]=rotation_menu.events[0][i][col];
            if (entry[0]==0x257 || entry[0]==0x25c || entry[0]==0x27d || entry[0]==0x252) {
                entry[1]=(u32)settings_actions[row]; entry[2]=0; entry[3]=0x430;
            }
            if (entry[0]==0x256 || entry[0]==0x271 || entry[0]==0x2cf || entry[0]==0x27f) {
                entry[1]=entry[2]=0; entry[3]=0x431;
            }
        }
        wifi_settings_menu.maps[row]=(u32)wifi_settings_menu.events[row];
    }
    for (u32 i=0;i<5;++i) wifi_settings_menu.callbacks[i]=rotation_menu.callbacks[i];
    wifi_settings_menu.callbacks[0]=(u32)wifi_settings_page_init;
    wifi_settings_values();
    z72_menu_set_label(WIFI_SETTINGS_SLOT,"Wi-Fi settings");
    if (!choice_localized) menu.rows[WIFI_MENU_ROW][0]=Z72_MENU_SLOT+WIFI_SETTINGS_SLOT;
}
#endif
