/* SPDX-License-Identifier: MIT */

enum {
    MOVIE_HEADER=0xb1852724u, MOVIE_PAGE=0xb2bb9d88u, RAW_PAGE=0xb2bc2d0cu, RAW_PAGE_ID=1088u,
    CHOICE_HEADER=0xb18545fcu, STOCK_ROWS=29, INSERT_ROW=7, RAW_ROW_ID=0x3200,
    ROOT_EVENTS=58, CHOICE_EVENTS=16,
#ifdef Z72_RAW_MOV_SYNC_MENU
    VIEWS=9, VIEW_ROWS=8, RAW_CFG_SYNC_OFF=Z72_RAW_CFG_SYNC_OFF,
#elif defined(Z72_RAW_COMPRESSION_MENU)
    VIEWS=8, VIEW_ROWS=7, LOCALE_CHARS=320,
#elif defined(Z72_RAW_DROP_EXPIRED)
    VIEWS=7, VIEW_ROWS=6, LOCALE_CHARS=280,
#elif defined(Z72_RAW_SIZE_SPLIT)
    VIEWS=6, VIEW_ROWS=5, LOCALE_CHARS=208,
#else
    VIEWS=5, VIEW_ROWS=4, LOCALE_CHARS=180,
#endif
    TEXT_RAW=0, TEXT_TARGET, TEXT_DUAL, TEXT_MOV_TARGET,
    TEXT_ON, TEXT_OFF, TEXT_SD, TEXT_CF, TEXT_FOLLOW,
#ifdef Z72_RAW_SIZE_SPLIT
    TEXT_SPLIT,TEXT_SINGLE,TEXT_4G,TEXT_8G,TEXT_16G,TEXT_32G,
#endif
#ifdef Z72_RAW_DROP_EXPIRED
    TEXT_TIMEOUT,TEXT_BLACK,TEXT_PREVIOUS,TEXT_STOP,
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
    TEXT_COMPRESSION,TEXT_MIXED,TEXT_ALL,TEXT_DISABLED,
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
    TEXT_MOV_SYNC,
#endif
    TEXT_END,
    MOV_UI_SLOT=0xb2982114u
};
static void (*raw_movie_ui_next)(u32);
#ifdef Z72_RAW_COMPRESSION_MENU
static const u32 row_counts[VIEWS]={VIEW_ROWS,2,2,2,3,5,3,3
#ifdef Z72_RAW_MOV_SYNC_MENU
    ,2
#endif
};
static const u32 labels[VIEWS][VIEW_ROWS]={
    {TEXT_RAW,TEXT_TARGET,TEXT_DUAL,TEXT_MOV_TARGET,TEXT_SPLIT,TEXT_TIMEOUT,TEXT_COMPRESSION
#ifdef Z72_RAW_MOV_SYNC_MENU
    ,TEXT_MOV_SYNC
#endif
    },
    {TEXT_ON,TEXT_OFF},{TEXT_SD,TEXT_CF},{TEXT_ON,TEXT_OFF},{TEXT_SD,TEXT_CF,TEXT_FOLLOW},
    {TEXT_SINGLE,TEXT_4G,TEXT_8G,TEXT_16G,TEXT_32G},{TEXT_BLACK,TEXT_PREVIOUS,TEXT_STOP},
    {TEXT_MIXED,TEXT_ALL,TEXT_DISABLED}
#ifdef Z72_RAW_MOV_SYNC_MENU
    ,{TEXT_ON,TEXT_OFF}
#endif
};
#elif defined(Z72_RAW_DROP_EXPIRED)
static const u32 row_counts[VIEWS]={6,2,2,2,3,5,3};
static const u32 labels[VIEWS][VIEW_ROWS]={
    {TEXT_RAW,TEXT_TARGET,TEXT_DUAL,TEXT_MOV_TARGET,TEXT_SPLIT,TEXT_TIMEOUT},{TEXT_ON,TEXT_OFF},
    {TEXT_SD,TEXT_CF},{TEXT_ON,TEXT_OFF},{TEXT_SD,TEXT_CF,TEXT_FOLLOW},
    {TEXT_SINGLE,TEXT_4G,TEXT_8G,TEXT_16G,TEXT_32G},{TEXT_BLACK,TEXT_PREVIOUS,TEXT_STOP}
};
#elif defined(Z72_RAW_SIZE_SPLIT)
static const u32 row_counts[VIEWS]={5,2,2,2,3,5};
static const u32 labels[VIEWS][VIEW_ROWS]={
    {TEXT_RAW,TEXT_TARGET,TEXT_DUAL,TEXT_MOV_TARGET,TEXT_SPLIT},{TEXT_ON,TEXT_OFF},
    {TEXT_SD,TEXT_CF},{TEXT_ON,TEXT_OFF},{TEXT_SD,TEXT_CF,TEXT_FOLLOW},
    {TEXT_SINGLE,TEXT_4G,TEXT_8G,TEXT_16G,TEXT_32G}
};
#else
static const u32 row_counts[VIEWS]={4,2,2,2,3};
static const u32 labels[VIEWS][VIEW_ROWS]={
    {TEXT_RAW,TEXT_TARGET,TEXT_DUAL,TEXT_MOV_TARGET},{TEXT_ON,TEXT_OFF},
    {TEXT_SD,TEXT_CF},{TEXT_ON,TEXT_OFF},{TEXT_SD,TEXT_CF,TEXT_FOLLOW}
};
#endif
static struct {
    u32 rows[STOCK_ROWS+1][7],maps[STOCK_ROWS+1],events[ROOT_EVENTS][4];
    u32 layout[4],callbacks[5];
} movie_menu;
static struct raw_view {
    u32 header[6],title[24],rows[VIEW_ROWS][7],maps[VIEW_ROWS],events[VIEW_ROWS][CHOICE_EVENTS][4];
} raw_views[VIEWS];
static u32 raw_view_index,root_cursor;

static void menu_copy(void *out,const void *in,u32 size)
{ CALL(0x10360facu,void,void *,const void *,u32)(out,in,size); }

_Static_assert(TEXT_END==Z72_RAW_LABEL_COUNT,"RAW menu label count");

const unsigned short *z72_raw_menu_label(u32 language,u32 label)
{
    static const unsigned short *const names[3][TEXT_END-TEXT_RAW]={
        {u"RAW video recording",u"RAW destination",u"Record normal video too",u"Normal video destination",
         u"ON",u"OFF",u"SD card",u"CFexpress/XQD",u"Use camera setting"
#ifdef Z72_RAW_SIZE_SPLIT
         ,u"RAW video file splitting",u"No splitting",u"4 GiB",u"8 GiB",u"16 GiB",u"32 GiB"
#endif
#ifdef Z72_RAW_DROP_EXPIRED
         ,u"Timeout handling",u"Black frame",u"Repeat previous frame",u"Stop recording"
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
         ,u"Lossless compression",u"Mixed",u"All frames",u"Off"
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
         ,u"MOV black frame sync"
#endif
        },
        {u"RAW 视频录制",u"RAW 目标位置",u"同时录制普通视频",u"普通视频目标位置",
         u"ON",u"OFF",u"SD 卡",u"CFexpress/XQD",u"跟随原厂设置"
#ifdef Z72_RAW_SIZE_SPLIT
         ,u"RAW 视频文件分段",u"不分段",u"4 GiB",u"8 GiB",u"16 GiB",u"32 GiB"
#endif
#ifdef Z72_RAW_DROP_EXPIRED
         ,u"超时设置",u"黑帧",u"复制上一帧",u"停止录制"
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
         ,u"无损压缩",u"混合",u"全部",u"关闭"
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
         ,u"MOV 黑帧同步"
#endif
        },
        {u"RAW 影片錄製",u"RAW 目標位置",u"同時錄製普通影片",u"普通影片目標位置",
         u"ON",u"OFF",u"SD 卡",u"CFexpress/XQD",u"跟隨原廠設定"
#ifdef Z72_RAW_SIZE_SPLIT
         ,u"RAW 影片檔案分段",u"不分段",u"4 GiB",u"8 GiB",u"16 GiB",u"32 GiB"
#endif
#ifdef Z72_RAW_DROP_EXPIRED
         ,u"逾時設定",u"黑畫格",u"複製上一格",u"停止錄製"
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
         ,u"無損壓縮",u"混合",u"全部",u"關閉"
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
         ,u"MOV 黑畫格同步"
#endif
        }
    };
    return names[language][label];
}

static int menu_write_settings(u32 value)
{
    return z72_raw_save_settings(value);
}

#ifdef Z72_RAW_MOV_SYNC_MENU
static u32 menu_sync_flag(void)
{
    return load(&raw_mov_sync_choice)==RAW_MOV_SYNC_OFF ? RAW_CFG_SYNC_OFF : 0u;
}
#else
#define menu_sync_flag() 0u
#endif

/* The recorder reads property 0xc3a into its private snapshot at 10458322.
 * Scope the override to the movie UI job that builds both card descriptors.
 * Keep the user's saved Nikon property and the native capacity checks. */
static void raw_movie_ui(u32 events)
{
    const u32 settings=load(&raw_settings);
#ifdef Z72_RAW_RECORDER_TARGET
    /* The factory recorder still runs when its MOV handle is virtual.
     * Give that session the slot that actually receives the RAW clip. */
    const u32 dest=(settings&16u) ? ((settings>>2)&1u) : settings>>6;
    if ((settings&1u) || dest==2) {
#else
    const u32 dest=settings>>6;
    if ((settings&0x11u) || dest==2) {
#endif
        raw_movie_ui_next(events);
        return;
    }
    if (events&1u) {
        CALL(0x103dc539u,void,u32)(200);
        CALL(0x10458277u,void,void)();
        CALL(0x10457045u,void,void)();
        events&=~1u;
    }
    volatile unsigned char *slot=(volatile unsigned char *)0xb298239cu;
    const unsigned char choice=dest==0 ? 1 : 0;
    CALL(0x10360f19u,void,u32)(0xb188abdcu);
    *slot=choice;
    CALL(0x10360f13u,void,u32)(0xb188abdcu);
    raw_movie_ui_next(events);
    CALL(0x10360f19u,void,u32)(0xb188abdcu);
    *slot=(unsigned char)CALL(0x10100491u,u32,u32)(0xc3a);
    CALL(0x10360f13u,void,u32)(0xb188abdcu);
}

static u32 raw_item_id(u32 view,u32 row) { return RAW_ROW_ID+1u+VIEW_ROWS*view+row; }

static void raw_view_init(const u32 *page)
{
    u32 view=raw_view_index;
    u32 row=view ? (load(&raw_settings)>>(2*(view-1)))&3u : root_cursor;
#ifdef Z72_RAW_SIZE_SPLIT
    if (view==5u) row=load(&raw_split_choice);
#endif
#ifdef Z72_RAW_DROP_EXPIRED
    if (view==6u) row=load(&raw_timeout_choice);
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
    if (view==7u) row=load(&raw_compression_choice);
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
    if (view==8u) row=load(&raw_mov_sync_choice);
#endif
    u32 id=raw_item_id(view,row);
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    if (view) CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 raw_show(u32 view)
{
    raw_view_index=view;
    *(volatile unsigned short *)(RAW_PAGE+0x16)=0;
    *(volatile unsigned short *)(RAW_PAGE+0x18)=0;
    WORD(RAW_PAGE+0x28)=(u32)raw_views[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,RAW_PAGE_ID);
    return 0;
}

static u32 raw_open(u32 a,u32 b,u32 c,u32 d)
{ (void)a;(void)b;(void)c;(void)d; root_cursor=0; return raw_show(0); }
static u32 raw_back(u32 a,u32 b,u32 c,u32 d)
{ (void)a;(void)b;(void)c;(void)d; return raw_show(0); }

static int raw_enable_resources(void)
{
    return start_worker(&capture_task,capture_worker,1u) && start_worker(&card_task,card_worker,2u) &&
           (STAGED_NEF_INPUT || mov_sink_install());
}

static u32 raw_pick(u32 row)
{
    if (row>=row_counts[raw_view_index]) return 1;
    if (!raw_view_index) { root_cursor=row; return raw_show(row+1); }
    if (BYTE(MOVIE_PHASE) || BYTE(RECORDER_PHASE) || load(&mov_sink_state)==MOV_SINK_ACTIVE ||
        load(&state)==ARMED || load(&state)==FINISHED) return 1;
    u32 shift=2*(raw_view_index-1),old=load(&raw_settings);
#ifdef Z72_RAW_MOV_SYNC_MENU
    if (raw_view_index==8u) {
        if (row!=load(&raw_mov_sync_choice)) {
            const u32 flag=row==RAW_MOV_SYNC_OFF ? RAW_CFG_SYNC_OFF : 0u;
            if (!menu_write_settings(old|(load(&raw_split_choice)<<8)|(load(&raw_timeout_choice)<<16)|
                                     (load(&raw_compression_choice)<<24)|flag)) return 1;
            publish(&raw_mov_sync_choice,row);
        }
        return raw_show(0);
    }
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
    if (raw_view_index==7u) {
        if (row!=load(&raw_compression_choice)) {
            if (!menu_write_settings(old|(load(&raw_split_choice)<<8)|
                                     (load(&raw_timeout_choice)<<16)|(row<<24)|menu_sync_flag())) return 1;
            publish(&raw_compression_choice,row);
        }
        return raw_show(0);
    }
#endif
#ifdef Z72_RAW_DROP_EXPIRED
    if (raw_view_index==6u) {
        if (row!=load(&raw_timeout_choice)) {
            if (!menu_write_settings(old|(load(&raw_split_choice)<<8)|(row<<16)|menu_sync_flag()
#ifdef Z72_RAW_COMPRESSION_MENU
                                     |(load(&raw_compression_choice)<<24)
#endif
                                     )) return 1;
            publish(&raw_timeout_choice,row);
        }
        return raw_show(0);
    }
#endif
#ifdef Z72_RAW_SIZE_SPLIT
    if (raw_view_index==5u) {
        if (row!=load(&raw_split_choice)) {
            if (!menu_write_settings(old|(row<<8)|menu_sync_flag()
#ifdef Z72_RAW_DROP_EXPIRED
                                     |(load(&raw_timeout_choice)<<16)
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
                                     |(load(&raw_compression_choice)<<24)
#endif
                                     )) return 1;
            publish(&raw_split_choice,row);
        }
        return raw_show(0);
    }
#endif
    u32 next=(old&~(3u<<shift))|(row<<shift);
    if (next!=old) {
        if (!(next&1u) && !raw_enable_resources()) return 1;
        if (!menu_write_settings(next|menu_sync_flag()
#ifdef Z72_RAW_SIZE_SPLIT
                                 |(load(&raw_split_choice)<<8)
#endif
#ifdef Z72_RAW_DROP_EXPIRED
                                 |(load(&raw_timeout_choice)<<16)
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
                                 |(load(&raw_compression_choice)<<24)
#endif
                                 )) return 1;
        if (next&1u) publish(&stop_requested,1);
        publish(&raw_settings,next);
    }
    return raw_show(0);
}

#define RAW_PICK(N) static u32 raw_pick##N(u32 a,u32 b,u32 c,u32 d) \
{ (void)a;(void)b;(void)c;(void)d;return raw_pick(N); }
RAW_PICK(0) RAW_PICK(1) RAW_PICK(2) RAW_PICK(3)
#ifdef Z72_RAW_SIZE_SPLIT
RAW_PICK(4)
#endif
#ifdef Z72_RAW_DROP_EXPIRED
RAW_PICK(5)
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
RAW_PICK(6)
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
RAW_PICK(7)
#endif
#undef RAW_PICK

static int raw_menu_prepare(u32 text_base,void (*movie_ui)(u32))
{
    const u32 *root=(const u32 *)MOVIE_HEADER,*choice=(const u32 *)CHOICE_HEADER;
    const u32 (*rows)[7]=(const u32 (*)[7])root[3];
    const u32 *maps=(const u32 *)root[4];
    if (WORD(MOVIE_PAGE)!=0xfd0002u || WORD(MOVIE_PAGE+0x28)!=MOVIE_HEADER ||
        root[3]!=0x11766f68u || root[4]!=0x1178a3acu || root[5]!=0x8001du ||
        rows[INSERT_ROW-1][0]!=0x5f9 || maps[INSERT_ROW-1]!=0x1176c51cu ||
        WORD(maps[INSERT_ROW-1]+16*(ROOT_EVENTS-1))!=0x31a ||
        choice[3]!=0x117d4f34u || choice[4]!=0x118111bcu ||
        WORD(WORD(choice[4])+16*(CHOICE_EVENTS-1))!=0x31a ||
        CALL(0x1061daafu,u32,u32,u32)(2,RAW_PAGE_ID)!=RAW_PAGE || WORD(MOV_UI_SLOT)!=(u32)movie_ui) return 0;
    for (u32 i=0;i<11;++i) if (WORD(RAW_PAGE+4*i)) return 0;
    for (u32 row=0;row<STOCK_ROWS;++row) {
        u32 to=row+(row>=INSERT_ROW);
        menu_copy(movie_menu.rows[to],rows[row],28);
        movie_menu.maps[to]=maps[row];
    }
    u32 *item=movie_menu.rows[INSERT_ROW];
    menu_copy(item,rows[INSERT_ROW-1],28);
    item[0]=text_base+TEXT_RAW; item[1]=item[2]=0xffff; item[4]=0x1001; item[5]=0xd7; item[6]=RAW_ROW_ID;
    menu_copy(movie_menu.events,(void *)maps[INSERT_ROW-1],sizeof(movie_menu.events));
    for (u32 i=0;i<ROOT_EVENTS;++i) {
        u32 *e=movie_menu.events[i];
        if (e[0]==0x257 || e[0]==0x25c || e[0]==0x270 || e[0]==0x27d) {
            e[1]=(u32)raw_open; e[3]=0x430;
        }
    }
    movie_menu.maps[INSERT_ROW]=(u32)movie_menu.events;
    menu_copy(movie_menu.layout,(void *)root[1],sizeof(movie_menu.layout));
#ifdef Z72_RAW_MENU_EIGHT_ROWS
    /* The stock movie coordinates have eight rows below the single title. */
    movie_menu.layout[0]=VIEW_ROWS;
#else
    movie_menu.layout[0]=VIEW_ROWS>7u ? 7u : VIEW_ROWS;
#endif
    menu_copy(movie_menu.callbacks,(void *)0x10969c14u,sizeof(movie_menu.callbacks));
    movie_menu.callbacks[0]=(u32)raw_view_init;
    const z72_menu_callback picks[]={raw_pick0,raw_pick1,raw_pick2,raw_pick3
#ifdef Z72_RAW_SIZE_SPLIT
        ,raw_pick4
#endif
#ifdef Z72_RAW_DROP_EXPIRED
        ,raw_pick5
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
        ,raw_pick6
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
        ,raw_pick7
#endif
    };
    for (u32 view=0;view<VIEWS;++view) {
        struct raw_view *v=&raw_views[view];
        u32 count=row_counts[view];
        menu_copy(v->header,view ? choice : (const u32 *)Z72_SETUP_MENU,24);
        menu_copy(v->title,(void *)choice[2],sizeof(v->title));
        v->title[0]=text_base+(view ? labels[0][view-1] : TEXT_RAW); v->title[1]=1;
        v->header[1]=view ? (count==2 ? 0x1187c500u : 0x1187c510u) : (u32)movie_menu.layout;
#ifdef Z72_RAW_SIZE_SPLIT
        if (view && count==5) v->header[1]=0x1187c540u;
#endif
        v->header[2]=(u32)v->title; v->header[3]=(u32)v->rows; v->header[4]=(u32)v->maps;
#if defined(Z72_RAW_MOV_SYNC_MENU) && !defined(Z72_RAW_MENU_EIGHT_ROWS)
        const u32 visible=count>7u ? 7u : count;
        v->header[5]=count|(visible<<16);
#else
        v->header[5]=count|(count<<16);
#endif
        for (u32 row=0;row<count;++row) {
            u32 *entry=v->rows[row];
            entry[0]=text_base+labels[view][row]; entry[1]=entry[2]=0xffff;
            entry[4]=view ? 0x1000 : 0x1001; entry[5]=view ? 0xffff : 0xd7;
            entry[6]=raw_item_id(view,row);
            v->maps[row]=(u32)v->events[row];
            menu_copy(v->events[row],(void *)WORD(choice[4]),sizeof(v->events[row]));
            for (u32 i=0;i<CHOICE_EVENTS;++i) {
                u32 *e=v->events[row][i];
                if (e[0]==0x257 || e[0]==0x25c || e[0]==0x27d || e[0]==0x252) {
                    e[1]=(u32)picks[row]; e[2]=0; e[3]=0x430;
                } else if (e[0]==0x256 || e[0]==0x271 || e[0]==0x2cf || e[0]==0x27f) {
                    e[1]=view ? (u32)raw_back : 0; e[2]=0; e[3]=view ? 0x430 : 0x431;
                }
            }
        }
    }
    return 1;
}

void z72_raw_video_install(u32 text_base,u32 config,void (*movie_ui)(u32))
{
    if (!raw_menu_prepare(text_base,movie_ui)) return;
    raw_movie_ui_next=movie_ui;
    WORD(RAW_PAGE+4)=WORD(MOVIE_PAGE+4);
    WORD(RAW_PAGE+0x10)=(u32)movie_menu.callbacks;
    WORD(RAW_PAGE+0x28)=(u32)raw_views[0].header;
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(RAW_PAGE)=2u|(RAW_PAGE_ID<<16);
    WORD(MOV_UI_SLOT)=(u32)raw_movie_ui;
    WORD(MOVIE_HEADER+12)=(u32)movie_menu.rows;
    WORD(MOVIE_HEADER+16)=(u32)movie_menu.maps;
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(MOVIE_HEADER+20)=0x8001eu;

    publish(&raw_split_choice,(config>>8)&255u);
    publish(&raw_timeout_choice,(config>>16)&255u);
    publish(&raw_compression_choice,(config&~RAW_CFG_SYNC_OFF)>>24);
    publish(&raw_mov_sync_choice,config&RAW_CFG_SYNC_OFF ? RAW_MOV_SYNC_OFF : RAW_MOV_SYNC_ON);
    u32 settings=config&255u;
    if (!(settings&1u) && !raw_enable_resources()) settings|=1u;
    publish(&raw_settings,settings);
}
