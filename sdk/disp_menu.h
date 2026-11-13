/* SPDX-License-Identifier: MIT */
#ifndef Z72_DISP_MENU_H
#define Z72_DISP_MENU_H

enum { DISP_TITLE_SLOT=6, DISP_MASTER_SLOT=7, DISP_PRESET_SLOT=8,
       DISP_ENABLED_SLOT=12, DISP_COMPONENT_SLOT=13, DISP_RESET_SLOT=18,
       DISP_ENABLE_SLOT=19, DISP_DISABLE_SLOT=20, DISP_SUBTITLE_SLOT=21,
       DISP_INFORMATION_SLOT=22, DISP_INFO_SLOT=23,
       DISP_BATTERY_SLOT=32, DISP_BATTERY_CHOICE=16 }; /* Slot 286 holds live battery text. */

/* The native page transition disposes of the previous view's widgets before
 * constructing the target. Keep every view's records alive until power-off. */
static struct {
    u32 header[6],title[32],rows[8][7],maps[8],events[8][ROTATION_EVENTS][4];
} disp_views[4];
static u32 disp_callbacks[5],disp_view,disp_preset,disp_root_row,disp_editor_row;
static u32 disp_choice_target,disp_choice_parent,disp_info_row;
static u32 disp_list_layout[4];
static const u32 disp_preset_defaults[]={3,1,10,21};
static const char *const disp_preset_names[]={"Standard","Minimal","Composition","Exposure"};
static const char *const disp_info_names[]={"Shutter speed","Aperture","ISO sensitivity",
    "Shooting mode","Release mode","Focus information","Other information","Focal length display"};

static u32 disp_header_owned(u32 header)
{
    for (u32 i=0;i<4;++i) if (header==(u32)disp_views[i].header) return 1;
    return 0;
}

static u32 disp_choice_value(void)
{
    if (!disp_choice_target) return !!(z72_config_current.disp_enabled&16u);
    if (disp_choice_target==DISP_BATTERY_CHOICE)
        return !!(z72_config_current.disp_enabled&Z72_BATTERY_PERCENT_ON);
    if (disp_choice_target==7) return !!(z72_config_current.disp_enabled&Z72_DISP_INFORMATION);
    if (disp_choice_target==1) return !!(z72_config_current.disp_enabled&(1u<<disp_preset));
    if (disp_choice_target>=8)
        return !!(z72_disp_information(z72_config_current.disp_components[disp_preset],disp_preset)&
                  (1u<<(disp_choice_target-8)));
    return !!(z72_config_current.disp_components[disp_preset]&(1u<<(disp_choice_target-2)));
}

static void disp_menu_values(void)
{
    u32 enabled=z72_config_current.disp_enabled;
    disp_views[0].rows[0][5]=rotation_value(enabled&16u);
    for (u32 i=0;i<Z72_DISP_PAGES;++i)
        disp_views[0].rows[i+1][5]=rotation_value(enabled&(1u<<i));
    disp_views[0].rows[5][5]=rotation_value(enabled&Z72_DISP_INFORMATION);
    disp_views[0].rows[6][5]=rotation_value(enabled&Z72_BATTERY_PERCENT_ON);
    disp_views[1].rows[0][5]=rotation_value(enabled&(1u<<disp_preset));
    disp_views[1].rows[1][5]=0xd7; /* Native -- marks the information submenu. */
    for (u32 i=1;i<5;++i)
        disp_views[1].rows[i+1][5]=rotation_value(z72_config_current.disp_components[disp_preset]&(1u<<i));
    u32 information=z72_disp_information(z72_config_current.disp_components[disp_preset],disp_preset);
    for (u32 i=0;i<Z72_DISP_INFO_GROUPS;++i)
        disp_views[3].rows[i][5]=rotation_value(information&(1u<<i));
    menu.rows[2][5]=rotation_value(enabled&16u);
}

static void disp_page_init(const u32 *page)
{
    u32 row=disp_view==0 ? disp_root_row : disp_view==1 ? disp_editor_row :
            disp_view==3 ? disp_info_row : !disp_choice_value();
    u32 id=0x2f40u+disp_view*16+row;
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 disp_show(u32 view)
{
    u32 header=WORD(ROTATION_PAGE+0x28);
    u32 owned=disp_header_owned(header),known=owned || header==(u32)rotation_menu.header;
#ifdef Z72_AF_SHORTCUT
    known=known || af_menu_header_owned(header);
#endif
#ifdef Z72_PHOTO_ZEBRA
    known=known || zebra_header_owned(header);
#endif
    if (view>3 || WORD(ROTATION_PAGE)!=2 || !known ||
        (WORD(ROTATION_PAGE+0x24) && !owned)) return 1;
    const char *subtitle=disp_preset_names[disp_preset];
    u32 subtitle_id=Z72_TEXT_DISP_STANDARD+disp_preset;
    if (view==2) {
        static const char *const labels[]={"Custom pages","Page enabled","Shooting info",
            "Framing grid","Histogram","Virtual horizon","Photo highlights","Information display"};
        static const u32 ids[]={Z72_TEXT_DISP_MASTER,Z72_TEXT_DISP_PAGE_ENABLED,
            Z72_TEXT_DISP_SHOOTING_INFO,Z72_TEXT_DISP_FRAMING_GRID,Z72_TEXT_DISP_HISTOGRAM,
            Z72_TEXT_DISP_HORIZON,Z72_TEXT_DISP_PHOTO_HIGHLIGHTS,Z72_TEXT_DISP_INFORMATION};
        if (disp_choice_target==DISP_BATTERY_CHOICE) {
            subtitle="Battery percentage display";
            subtitle_id=Z72_TEXT_BATTERY_PERCENT;
        } else {
            subtitle=disp_choice_target>=8 ? disp_info_names[disp_choice_target-8] : labels[disp_choice_target];
            subtitle_id=disp_choice_target>=8 ? Z72_TEXT_DISP_SHUTTER+disp_choice_target-8 : ids[disp_choice_target];
        }
    }
    if (choice_localized && view) {
        disp_views[view].title[8]=subtitle_id;
        disp_views[view].title[9]=1;
    } else z72_menu_set_label(DISP_SUBTITLE_SLOT,subtitle);
    disp_menu_values();
    disp_view=view;
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)disp_callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)disp_views[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

static u32 disp_back(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return disp_show(disp_view==2 ? disp_choice_parent : disp_view==3 ? 1 : 0);
}

static void disp_save(struct z72_config config)
{
    z72_config_save(config);
    disp_settings_changed();
    disp_menu_values();
    u32 widgets=WORD(SETUP_PAGE+0x24);
    if (widgets && CALL(0x1063464fu,u32,u32,u32,u32)(widgets,2,0)) {
        u32 status=CALL(0x1063466du,u32,u32,u32,u32)(widgets,2,0);
        if (status) CALL(0x106361cdu,void,u32,u32)(status,rotation_value(config.disp_enabled&16u));
    }
}

static u32 disp_select(u32 row)
{
    if (disp_view==2) {
        if (row>1) return 1;
        struct z72_config config=z72_config_current;
        u32 value=row==0;
        if (!disp_choice_target) config.disp_enabled=(config.disp_enabled&~16u)|(value<<4);
        else if (disp_choice_target==DISP_BATTERY_CHOICE)
            config.disp_enabled=(config.disp_enabled&~Z72_BATTERY_PERCENT_ON)|
                                (value ? Z72_BATTERY_PERCENT_ON : 0);
        else if (disp_choice_target==1 || disp_choice_target==7) {
            u32 pages=config.disp_enabled&Z72_DISP_ENABLED;
            u32 bit=disp_choice_target==7 ? Z72_DISP_INFORMATION : 1u<<disp_preset;
            /* A single enabled page remains reachable even if all others are off. */
            if (value || (pages&~bit))
                config.disp_enabled=(config.disp_enabled&~bit)|(value ? bit : 0);
        } else if (disp_choice_target>=8) {
            u32 components=config.disp_components[disp_preset];
            u32 information=z72_disp_information(components,disp_preset);
            u32 bit=1u<<(disp_choice_target-8);
            information=(information&~bit)|(value ? bit : 0);
            config.disp_components[disp_preset]=
                (components&~(Z72_DISP_INFO|Z72_DISP_INFO_FIELDS))|Z72_DISP_INFO_DETAIL|
                (information<<6)|(information ? Z72_DISP_INFO : 0);
        } else {
            u32 bit=1u<<(disp_choice_target-2);
            config.disp_components[disp_preset]=(config.disp_components[disp_preset]&~bit)|
                                               (value ? bit : 0);
        }
        disp_save(config);
        return disp_show(disp_choice_parent);
    }
    if (disp_view==3) {
        if (row>=Z72_DISP_INFO_GROUPS) return 1;
        disp_info_row=row;
        disp_choice_target=8+row;
        disp_choice_parent=3;
        return disp_show(2);
    }
    if (!disp_view) {
        if (row>Z72_DISP_PAGES+2) return 1;
        disp_root_row=row;
        if (row==Z72_DISP_PAGES+2) {
            disp_choice_target=DISP_BATTERY_CHOICE;
            disp_choice_parent=0;
            return disp_show(2);
        }
        if (row==Z72_DISP_PAGES+1) {
            disp_choice_target=7;
            disp_choice_parent=0;
            return disp_show(2);
        }
        if (row) {
            disp_preset=row-1;
            disp_editor_row=0;
            return disp_show(1);
        }
        disp_choice_target=0;
        disp_choice_parent=0;
    } else {
        if (row>6) return 1;
        disp_editor_row=row;
        if (row==1) {
            disp_info_row=0;
            return disp_show(3);
        }
        if (row==6) {
            struct z72_config config=z72_config_current;
            config.disp_components[disp_preset]=disp_preset_defaults[disp_preset];
            disp_save(config);
            return disp_show(1);
        }
        disp_choice_target=row+1;
        disp_choice_parent=1;
    }
    return disp_show(2);
}

#define Z72_DISP_PICK(n) \
static u32 disp_pick_##n(u32 a,u32 b,u32 c,u32 d) \
{ (void)a; (void)b; (void)c; (void)d; return disp_select(n); }
Z72_DISP_PICK(0)
Z72_DISP_PICK(1)
Z72_DISP_PICK(2)
Z72_DISP_PICK(3)
Z72_DISP_PICK(4)
Z72_DISP_PICK(5)
Z72_DISP_PICK(6)
Z72_DISP_PICK(7)
#undef Z72_DISP_PICK

static u32 open_disp_menu(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    disp_root_row=0;
    return disp_show(0);
}

static void disp_menu_prepare(void)
{
    static const char *const labels[]={"Custom DISP pages","Custom pages","Standard","Minimal",
        "Composition","Exposure","Page enabled","Shooting info","Framing grid","Histogram",
        "Virtual horizon","Photo highlights","Reset preset","Enable","Disable"};
    static const u32 counts[]={7,7,2,Z72_DISP_INFO_GROUPS};
    const z72_menu_callback picks[]={disp_pick_0,disp_pick_1,disp_pick_2,disp_pick_3,
                                   disp_pick_4,disp_pick_5,disp_pick_6,disp_pick_7};
    const u32 *events=(const u32 *)WORD(0x118111bcu);
    for (u32 i=0;i<sizeof(labels)/sizeof(labels[0]);++i) z72_menu_set_label(DISP_TITLE_SLOT+i,labels[i]);
    z72_menu_set_label(DISP_INFORMATION_SLOT,"Information display");
    z72_menu_set_label(DISP_BATTERY_SLOT,"Battery percentage display");
    for (u32 i=0;i<Z72_DISP_INFO_GROUPS;++i) z72_menu_set_label(DISP_INFO_SLOT+i,disp_info_names[i]);
    const u32 *setup=(const u32 *)Z72_SETUP_MENU;
    const u32 *setup_layout=(const u32 *)setup[1];
    /* The subtitle occupies the first native Setup row. Reuse its remaining
     * seven rows, including separate label/status columns and scrollbar bounds. */
    disp_list_layout[0]=7;
    for (u32 i=1;i<4;++i) disp_list_layout[i]=setup_layout[i]+8;
    for (u32 view=0;view<4;++view) {
        const u32 *stock=(const u32 *)(view==2 ? 0xb18545fcu : 0xb18512e4u);
        const u32 *stock_title=(const u32 *)stock[2];
        const u32 *stock_rows=(const u32 *)(view==2 ? stock[3] : setup[3]);
        u32 *header=disp_views[view].header,*title=disp_views[view].title;
        for (u32 i=0;i<6;++i) header[i]=stock[i];
        if (view!=2) {
            header[0]=setup[0];
            header[1]=view ? (u32)disp_list_layout : setup[1];
        }
        for (u32 i=0;i<8;++i) title[i]=rotation_menu.title[i];
        title[0]=choice_localized ? (view==3 ? Z72_TEXT_DISP_SHOOTING_INFO : Z72_TEXT_DISP_TITLE) :
                 Z72_MENU_SLOT+(view==3 ? DISP_COMPONENT_SLOT : DISP_TITLE_SLOT);
        title[1]=choice_localized ? 1 : 2;
        if (view!=0) {
            const u32 *sub=(const u32 *)WORD(0xb18512e4u+8);
            for (u32 i=0;i<8;++i) title[8+i]=sub[8+i];
            title[8]=Z72_MENU_SLOT+DISP_SUBTITLE_SLOT; title[9]=2;
        }
        u32 back=view ? 16u : 8u;
        const u32 *native_back=view==2 ? stock_title+8 : stock_title+24;
        for (u32 i=0;i<8;++i) title[back+i]=native_back[i];
        header[2]=(u32)title; header[3]=(u32)disp_views[view].rows; header[4]=(u32)disp_views[view].maps;
        u32 visible=counts[view]>7 ? 7u : counts[view];
        header[5]=(visible<<16)|counts[view];
        for (u32 row=0;row<counts[view];++row) {
            u32 *item=disp_views[view].rows[row];
            for (u32 i=0;i<7;++i) item[i]=stock_rows[i];
            u32 label=view==0 ? (row==6 ? DISP_BATTERY_SLOT : row==5 ? DISP_INFORMATION_SLOT : DISP_MASTER_SLOT+row) :
                      view==1 ? DISP_ENABLED_SLOT+row : view==3 ? DISP_INFO_SLOT+row : DISP_ENABLE_SLOT+row;
            item[0]=choice_localized ? (view==3 ? Z72_TEXT_DISP_SHUTTER+row :
                    view==0 && row==6 ? Z72_TEXT_BATTERY_PERCENT :
                    view==0 && row==5 ? Z72_TEXT_DISP_INFORMATION :
                    Z72_TEXT_DISP_TITLE+label-DISP_TITLE_SLOT) : Z72_MENU_SLOT+label;
            item[1]=item[2]=0xffff;
            item[4]=(choice_localized ? 0x1000u : 0x1200u)|(!(view==2 || (view==1 && row==6)));
            item[5]=0xffff; item[6]=0x2f40u+view*16+row;
            for (u32 i=0;i<ROTATION_EVENTS;++i) {
                u32 *entry=disp_views[view].events[row][i];
                for (u32 col=0;col<4;++col) entry[col]=events[i*4+col];
                u32 event=entry[0];
                if (event==0x257 || event==0x25c || event==0x27d || event==0x252) {
                    entry[1]=(u32)picks[row]; entry[2]=0; entry[3]=0x430;
                }
                if (event==0x256 || event==0x271 || event==0x2cf || event==0x27f) {
                    entry[1]=view ? (u32)disp_back : 0; entry[2]=0;
                    entry[3]=view ? 0x430 : 0x431;
                }
            }
            disp_views[view].maps[row]=(u32)disp_views[view].events[row];
        }
    }
    for (u32 i=0;i<5;++i) disp_callbacks[i]=rotation_menu.callbacks[i];
    disp_callbacks[0]=(u32)disp_page_init;
    disp_menu_values();
}
#endif
