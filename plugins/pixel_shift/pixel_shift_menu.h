/* SPDX-License-Identifier: MIT */
enum {
    PIXEL_MENU_ID=0x2e60, PIXEL_MENU_EVENTS=16, PIXEL_MENU_MAX_ROWS=7,
    PIXEL_ROOT_VIEW=0, PIXEL_MODE_VIEW, PIXEL_COUNT_VIEW, PIXEL_DELAY_VIEW, PIXEL_GAP_VIEW,
    PIXEL_VIEW_COUNT
};

static struct {
    z72_u32 header[6],title[24],rows[PIXEL_MENU_MAX_ROWS][7],maps[PIXEL_MENU_MAX_ROWS];
    z72_u32 events[PIXEL_MENU_MAX_ROWS][PIXEL_MENU_EVENTS][4];
} pixel_menu_views[PIXEL_VIEW_COUNT];
static z72_u32 pixel_menu_view,pixel_menu_root_row,pixel_menu_layout[4],pixel_menu_callbacks[5];

static const struct {
    z72_u32 layout,title,count,labels[PIXEL_MENU_MAX_ROWS];
} pixel_view_specs[]={
    {0,Z72_TEXT_PIXEL_TITLE,4,{Z72_TEXT_PIXEL_MODE,Z72_TEXT_PIXEL_COUNT,Z72_TEXT_PIXEL_DELAY,Z72_TEXT_PIXEL_GAP}},
    {0x1187c510u,Z72_TEXT_PIXEL_MODE,3,{Z72_TEXT_PIXEL_REPEAT,Z72_TEXT_PIXEL_ONE,Z72_TEXT_PIXEL_OFF}},
    {0x1187c530u,Z72_TEXT_PIXEL_COUNT,4,{Z72_TEXT_PIXEL_4,Z72_TEXT_PIXEL_8,Z72_TEXT_PIXEL_16,Z72_TEXT_PIXEL_32}},
    {0x1187c550u,Z72_TEXT_PIXEL_DELAY,sizeof(pixel_delays),
        {Z72_TEXT_PIXEL_OFF,Z72_TEXT_PIXEL_1S,Z72_TEXT_PIXEL_2S,Z72_TEXT_PIXEL_3S,Z72_TEXT_PIXEL_5S,Z72_TEXT_PIXEL_10S}},
    {0x1187c560u,Z72_TEXT_PIXEL_GAP,sizeof(pixel_gaps),
        {Z72_TEXT_PIXEL_0S,Z72_TEXT_PIXEL_1S,Z72_TEXT_PIXEL_2S,Z72_TEXT_PIXEL_3S,Z72_TEXT_PIXEL_5S,Z72_TEXT_PIXEL_10S,Z72_TEXT_PIXEL_OFF}}
};

static void pixel_menu_init(const z72_u32 *page)
{
    z72_u32 row=pixel_menu_root_row;
    switch (pixel_menu_view) {
    case PIXEL_MODE_VIEW: row=PIXEL_REPEAT-PIXEL_LOAD(pixel_mode); break;
    case PIXEL_COUNT_VIEW: row=PIXEL_LOAD(pixel_count_index); break;
    case PIXEL_DELAY_VIEW: row=PIXEL_LOAD(pixel_delay_index); break;
    case PIXEL_GAP_VIEW: row=PIXEL_LOAD(pixel_gap_index); break;
    }
    z72_u32 id=PIXEL_MENU_ID+pixel_menu_view*8+row;
    if (CALL(0x1063bf77u,z72_u32,void)()!=1)
        CALL(0x10638983u,void,const z72_u32 *,z72_u32)(page,id);
    CALL(0x1063ac0du,void,const z72_u32 *,z72_u32,z72_u32)(page,id,1);
}

static u32 pixel_header_owned(u32 header)
{
    for (u32 i=0;i<PIXEL_VIEW_COUNT;++i) if (header==(u32)pixel_menu_views[i].header) return 1;
    return 0;
}

static void pixel_load_settings(void)
{
    u32 value=z72_config_current.pixel_shift;
    PIXEL_STORE(pixel_count_index,(value>>8)&255u);
    PIXEL_STORE(pixel_delay_index,(value>>16)&255u);
    PIXEL_STORE(pixel_gap_index,value>>24);
    if ((value&255u) && pixel_start_tasks()) PIXEL_STORE(pixel_mode,PIXEL_REPEAT);
}

static z72_u32 pixel_menu_show(z72_u32 view)
{
    u32 header=WORD(ROTATION_PAGE+0x28);
    if (WORD(ROTATION_PAGE)!=2 ||
        (!pixel_header_owned(header) && (view || WORD(ROTATION_PAGE+0x24) ||
         (!pre_capture_menu_source(header) && !jpeg_menu_owned(header))))) return 1;
    pixel_menu_views[PIXEL_ROOT_VIEW].rows[0][5]=rotation_value(PIXEL_LOAD(pixel_mode)!=PIXEL_OFF);
    pixel_menu_view=view;
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)pixel_menu_callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)pixel_menu_views[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,z72_u32,z72_u32)(2,0);
    return 0;
}

static z72_u32 pixel_menu_open(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    pixel_menu_root_row=0;
    return pixel_menu_show(PIXEL_ROOT_VIEW);
}

static z72_u32 pixel_menu_back(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return pixel_menu_show(PIXEL_ROOT_VIEW);
}

static z72_u32 pixel_menu_pick(z72_u32 row)
{
    if (row>=pixel_view_specs[pixel_menu_view].count) return 1;
    switch (pixel_menu_view) {
    case PIXEL_ROOT_VIEW:
        pixel_menu_root_row=row;
        return pixel_menu_show(PIXEL_MODE_VIEW+row);
    case PIXEL_MODE_VIEW: {
        z72_u32 selected=PIXEL_REPEAT-row;
        if (selected==PIXEL_OFF || (!pixel_running() && pixel_start_tasks()))
            PIXEL_STORE(pixel_mode,selected);
        break;
    }
    case PIXEL_COUNT_VIEW: PIXEL_STORE(pixel_count_index,row); break;
    case PIXEL_DELAY_VIEW: PIXEL_STORE(pixel_delay_index,row); break;
    case PIXEL_GAP_VIEW: PIXEL_STORE(pixel_gap_index,row); break;
    }
    struct z72_config config=z72_config_current;
    /* Single-set mode disarms after the group and never starts armed on boot. */
    config.pixel_shift=(PIXEL_LOAD(pixel_mode)==PIXEL_REPEAT)|(PIXEL_LOAD(pixel_count_index)<<8)|
        (PIXEL_LOAD(pixel_delay_index)<<16)|(PIXEL_LOAD(pixel_gap_index)<<24);
    z72_config_save(config);
    return pixel_menu_show(PIXEL_ROOT_VIEW);
}

#define PIXEL_PICK(row) \
    static z72_u32 pixel_menu_pick##row(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d) \
    { (void)a; (void)b; (void)c; (void)d; return pixel_menu_pick(row); }
PIXEL_PICK(0)
PIXEL_PICK(1)
PIXEL_PICK(2)
PIXEL_PICK(3)
PIXEL_PICK(4)
PIXEL_PICK(5)
PIXEL_PICK(6)
#undef PIXEL_PICK

static int pixel_menu_prepare(void)
{
    const z72_u32 *setup=(const z72_u32 *)Z72_SETUP_MENU;
    const z72_u32 *choice=(const z72_u32 *)0xb18545fcu;
    /* The shared page and native choice template are validated by portrait_install_menu. */
    if (!pixel_ready()) return 1;
    const z72_u32 *events=(const z72_u32 *)WORD(choice[4]);
    if (events[(PIXEL_MENU_EVENTS-1)*4]!=0x31au) return 1;
    for (z72_u32 view=1;view<PIXEL_VIEW_COUNT;++view)
        if (WORD(pixel_view_specs[view].layout)!=pixel_view_specs[view].count) return 1;

    const z72_menu_callback actions[]={pixel_menu_pick0,pixel_menu_pick1,pixel_menu_pick2,pixel_menu_pick3,
        pixel_menu_pick4,pixel_menu_pick5,pixel_menu_pick6};
    void *(*copy)(void *,const void *,z72_u32)=(void *)0x10360facu;
    copy(pixel_menu_layout,(const void *)setup[1],sizeof(pixel_menu_layout));
    pixel_menu_layout[0]=pixel_view_specs[PIXEL_ROOT_VIEW].count;
    for (z72_u32 view=0;view<PIXEL_VIEW_COUNT;++view) {
        z72_u32 *header=pixel_menu_views[view].header,*title=pixel_menu_views[view].title;
        z72_u32 count=pixel_view_specs[view].count;
        const z72_u32 *template=view==PIXEL_ROOT_VIEW ? setup : choice;
        copy(header,template,sizeof(pixel_menu_views[view].header));
        copy(title,(const void *)choice[2],sizeof(pixel_menu_views[view].title));
        if (view==PIXEL_GAP_VIEW) header[0]=setup[0]; /* Native seven-row geometry. */
        header[1]=view==PIXEL_ROOT_VIEW ? (z72_u32)pixel_menu_layout : pixel_view_specs[view].layout;
        title[0]=pixel_view_specs[view].title;
        title[1]=1;
        header[2]=(z72_u32)title;
        header[3]=(z72_u32)pixel_menu_views[view].rows;
        header[4]=(z72_u32)pixel_menu_views[view].maps;
        header[5]=(count<<16)|count;
        for (z72_u32 row=0;row<count;++row) {
            z72_u32 *item=pixel_menu_views[view].rows[row];
            copy(item,(const void *)template[3],sizeof(pixel_menu_views[view].rows[row]));
            item[0]=pixel_view_specs[view].labels[row];
            item[1]=item[2]=0xffffu;
            item[3]=0;
            item[4]=view==PIXEL_ROOT_VIEW ? 0x1001u : 0x1000u;
            item[5]=view==PIXEL_ROOT_VIEW ? 0xd7u : 0xffffu;
            item[6]=PIXEL_MENU_ID+view*8+row;
            copy(pixel_menu_views[view].events[row],events,PIXEL_MENU_EVENTS*16);
            for (z72_u32 i=0;i<PIXEL_MENU_EVENTS;++i) {
                z72_u32 *event=pixel_menu_views[view].events[row][i];
                if (event[0]==0x257 || event[0]==0x25c || event[0]==0x27d || event[0]==0x252) {
                    event[1]=(z72_u32)actions[row]; event[2]=0; event[3]=0x430;
                } else if (event[0]==0x256 || event[0]==0x271 || event[0]==0x2cf || event[0]==0x27f) {
                    event[1]=view==PIXEL_ROOT_VIEW ? 0 : (z72_u32)pixel_menu_back;
                    event[2]=0;
                    event[3]=view==PIXEL_ROOT_VIEW ? 0x431 : 0x430;
                }
            }
            pixel_menu_views[view].maps[row]=(z72_u32)pixel_menu_views[view].events[row];
        }
    }
    for (u32 i=1;i<5;++i) pixel_menu_callbacks[i]=rotation_menu.callbacks[i];
    pixel_menu_callbacks[0]=(z72_u32)pixel_menu_init;
    return 0;
}
