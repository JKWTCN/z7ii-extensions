/* SPDX-License-Identifier: MIT */
#ifndef Z72_PHOTO_ZEBRA_MENU_H
#define Z72_PHOTO_ZEBRA_MENU_H

/* Each view owns its rows and event maps. Native reopening of slot zero
 * destroys the old widgets before creating the newly selected view. */
static struct {
    u32 header[6],title[32],rows[8][7],maps[8],events[8][ROTATION_EVENTS][4];
} zebra_views[3];
static u32 zebra_callbacks[5],zebra_view,zebra_root_row;

static u32 zebra_header_owned(u32 header)
{
    for (u32 i=0;i<3;++i) if (header==(u32)zebra_views[i].header) return 1;
#ifdef Z72_WIFI_5GHZ
    if (wifi_header_owned(header)) return 1;
#endif
#ifdef Z72_AF_SHORTCUT
    if (af_menu_header_owned(header)) return 1;
#endif
#ifdef Z72_CUSTOM_DISP
    return disp_header_owned(header);
#else
    return 0;
#endif
}

static u32 zebra_pattern_value(u32 pattern)
{
    return pattern==1 ? 0x695u : pattern==2 ? 0x696u : 0x697u;
}
static u32 zebra_threshold_value(u32 threshold)
{
    static const u32 labels[]={0x686,0x687,0x688,0x69e,0x689,0x68a,0x68b,0x68c};
    return labels[threshold<8 ? threshold : 1];
}
static void zebra_menu_values(void)
{
    static const char *const thresholds[]={"255","248","235","224","213","202","191","180"};
    u32 pattern=z72_config_current.photo_zebra_pattern;
    u32 threshold=z72_config_current.photo_zebra_threshold;
    /* Native status symbols and dynamic numeric text use different resources
     * from the localized choice labels. Keep our text slot independent. */
    zebra_views[0].rows[0][5]=pattern==1 ? 0xbb4u : pattern==2 ? 0xfbeu : 0xd1u;
    zebra_views[0].rows[1][5]=Z72_MENU_SLOT+5;
    CALL(0x10636a07u,void,const char *,u32,u32)(thresholds[threshold<8 ? threshold : 1],Z72_MENU_SLOT+5,2);
    menu.rows[1][5]=rotation_value(z72_config_current.photo_zebra_pattern);
}

static void zebra_page_init(const u32 *page)
{
    u32 row=zebra_root_row;
    if (zebra_view==1) row=z72_config_current.photo_zebra_pattern ? z72_config_current.photo_zebra_pattern-1 : 2;
    if (zebra_view==2) row=z72_config_current.photo_zebra_threshold;
    u32 id=0x2f80u+zebra_view*16+row;
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 zebra_show(u32 view)
{
    if (view>2 || WORD(ROTATION_PAGE)!=2 ||
        (WORD(ROTATION_PAGE+0x28)!=(u32)rotation_menu.header &&
         !zebra_header_owned(WORD(ROTATION_PAGE+0x28)))) return 1;
    if (WORD(ROTATION_PAGE+0x24) && !zebra_header_owned(WORD(ROTATION_PAGE+0x28))) return 1;
    zebra_menu_values();
    zebra_view=view;
    /* The old widget tree keeps its own storage. Reset its saved row before
     * the native transition reads the target header, which can have fewer rows. */
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)zebra_callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)zebra_views[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

static u32 zebra_back(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; return zebra_show(0); }
static u32 open_zebra_pattern(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; zebra_root_row=0; return zebra_show(1); }
static u32 open_zebra_threshold(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; zebra_root_row=1; return zebra_show(2); }
static u32 open_zebra_menu(u32 a,u32 b,u32 c,u32 d)
{ (void)a; (void)b; (void)c; (void)d; zebra_root_row=0; return zebra_show(0); }

static u32 zebra_pick(u32 row)
{
    if (!zebra_view || row>=(zebra_view==1 ? 3u : 8u)) return 1;
    struct z72_config config=z72_config_current;
    if (zebra_view==1) config.photo_zebra_pattern=row<2 ? row+1 : 0;
    else config.photo_zebra_threshold=row;
    z72_config_save(config);
    u32 widgets=WORD(SETUP_PAGE+0x24);
    if (widgets && CALL(0x1063464fu,u32,u32,u32,u32)(widgets,1,0)) {
        u32 status=CALL(0x1063466du,u32,u32,u32,u32)(widgets,1,0);
        if (status) CALL(0x106361cdu,void,u32,u32)(status,rotation_value(config.photo_zebra_pattern));
    }
    return zebra_show(0);
}
#define Z72_CHOICE_PICK(n) \
static u32 choice_pick_##n(u32 a,u32 b,u32 c,u32 d) \
{ (void)a; (void)b; (void)c; (void)d; return zebra_pick(n); }
Z72_CHOICE_PICK(0)
Z72_CHOICE_PICK(1)
Z72_CHOICE_PICK(2)
Z72_CHOICE_PICK(3)
Z72_CHOICE_PICK(4)
Z72_CHOICE_PICK(5)
Z72_CHOICE_PICK(6)
Z72_CHOICE_PICK(7)
#undef Z72_CHOICE_PICK

static void zebra_menu_prepare(void)
{
    static const u32 counts[]={2,3,8},headers[]={0xb18512b4,0xb18512cc,0xb18512e4};
    static const u32 titles[]={Z72_TEXT_PHOTO_ZEBRA,Z72_TEXT_PHOTO_PATTERN,Z72_TEXT_PHOTO_THRESHOLD};
    const z72_menu_callback picks[]={choice_pick_0,choice_pick_1,choice_pick_2,choice_pick_3,
                                   choice_pick_4,choice_pick_5,choice_pick_6,choice_pick_7};
    const z72_menu_callback root[]={open_zebra_pattern,open_zebra_threshold};
    const u32 *events=(const u32 *)WORD(0x118111bcu);
    for (u32 view=0;view<3;++view) {
        u32 *header=zebra_views[view].header;
        const u32 *stock=(const u32 *)headers[view];
        const u32 *stock_title=(const u32 *)stock[2],*stock_rows=(const u32 *)stock[3];
        for (u32 i=0;i<6;++i) header[i]=stock[i];
        for (u32 i=0;i<(view ? 32u : 24u);++i) zebra_views[view].title[i]=stock_title[i];
        /* The native G7 graphic is a separate record before the Back button.
         * Remove it and terminate the shortened list; use the full title width. */
        u32 number=view ? 16u : 8u;
        for (u32 i=0;i<8;++i) {
            zebra_views[view].title[number+i]=stock_title[number+8+i];
            zebra_views[view].title[number+8+i]=0;
        }
        for (u32 i=0;i<8;++i) zebra_views[view].title[i]=rotation_menu.title[i];
        zebra_views[view].title[0]=choice_localized ? Z72_TEXT_PHOTO_ZEBRA : Z72_MENU_SLOT+1;
        zebra_views[view].title[1]=choice_localized ? 1 : 2;
        if (view) {
            zebra_views[view].title[8]=choice_localized ? titles[view] : Z72_MENU_SLOT+view+2;
            zebra_views[view].title[9]=choice_localized ? 1 : 2;
        }
        header[2]=(u32)zebra_views[view].title;
        header[3]=(u32)zebra_views[view].rows; header[4]=(u32)zebra_views[view].maps;
        for (u32 row=0;row<counts[view];++row) {
            u32 *item=zebra_views[view].rows[row];
            for (u32 i=0;i<7;++i) item[i]=stock_rows[row*7+i];
            item[0]=view==1 ? zebra_pattern_value(row<2 ? row+1 : 0) :
                    view==2 ? zebra_threshold_value(row) :
                    choice_localized ? (row ? Z72_TEXT_PHOTO_THRESHOLD : Z72_TEXT_PHOTO_PATTERN) : Z72_MENU_SLOT+3+row;
            if (!view && !choice_localized) item[4]=(item[4]&~0xf00u)|0x200u;
            item[6]=0x2f80u+view*16+row;
            for (u32 i=0;i<ROTATION_EVENTS;++i) {
                u32 *entry=zebra_views[view].events[row][i];
                for (u32 col=0;col<4;++col) entry[col]=events[i*4+col];
                u32 event=entry[0];
                if (event==0x257 || event==0x25c || event==0x27d || event==0x252) {
                    entry[1]=(u32)(view ? picks[row] : root[row]); entry[2]=0; entry[3]=0x430;
                }
                if (event==0x256 || event==0x271 || event==0x2cf || event==0x27f) {
                    entry[1]=view ? (u32)zebra_back : 0; entry[2]=0;
                    entry[3]=view ? 0x430 : 0x431;
                }
            }
            zebra_views[view].maps[row]=(u32)zebra_views[view].events[row];
        }
    }
    for (u32 i=0;i<5;++i) zebra_callbacks[i]=rotation_menu.callbacks[i];
    zebra_callbacks[0]=(u32)zebra_page_init;
    z72_menu_set_label(3,"Display pattern");
    z72_menu_set_label(4,"Highlight threshold");
    zebra_menu_values();
}

/* The rotation choice continues to use its original independent view. */
static u32 open_rotation_choice(void)
{
    if (WORD(ROTATION_PAGE)!=2 || WORD(ROTATION_PAGE+0x24) ||
        (WORD(ROTATION_PAGE+0x28)!=(u32)rotation_menu.header &&
         !zebra_header_owned(WORD(ROTATION_PAGE+0x28)))) return 1;
    WORD(ROTATION_PAGE+0x10)=(u32)rotation_menu.callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)rotation_menu.header;
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

#endif
