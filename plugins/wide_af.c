/* SPDX-License-Identifier: MIT */
enum { WIDE_AREA=0x056c, WIDE_SUBJECT=0x058a, WIDE_L=4 };

static z72_u32 wide_subject[2],wide_size[2];
static z72_u32 wide_active_preset,wide_live_size,wide_restore_pending,wide_selecting;
static z72_u32 wide_boot_preset;

#include "wide_af_geometry.h"
#include "wide_af_icons.h"
#ifdef Z72_SUBJECT_TRACKING
static void wide_af_remember(z72_u32 preset);
#include "subject_tracking.h"
#endif

static z72_u32 wide_af_clear(void)
{
    __atomic_store_n(&wide_live_size,0,__ATOMIC_RELEASE);
    if (wide_active_preset) wide_icons_set(0);
    if (wide_active_preset || wide_restore_pending) wide_restore_pending=wide_geometry_clear();
    __atomic_store_n(&wide_active_preset,0,__ATOMIC_RELAXED);
    return wide_restore_pending;
}

static z72_u32 wide_af_active(void)
{
    if (!wide_active_preset || wide_selecting) return 0;
    z72_u32 preset=wide_active_preset-1;
    if (WORD(0xb25519b8u) || CALL(0x1063c7d5u,z72_u32,z72_u32)(WIDE_AREA)!=WIDE_L ||
        CALL(0x1063c7d5u,z72_u32,z72_u32)(WIDE_SUBJECT)!=wide_subject[preset])
        wide_af_clear();
    return wide_active_preset;
}

static z72_u32 wide_af_select(z72_u32 preset)
{
    if (WORD(0xb25519b8u) || (wide_restore_pending && wide_af_clear())) return 1;
    z72_u32 size=wide_size[preset];
    if (wide_geometry_set(size&255u,size>>8)) return 1;
    /* Native property setters can refresh the GUI before both values are set. */
    wide_selecting=1;
    __atomic_store_n(&wide_active_preset,preset+1,__ATOMIC_RELAXED);
    wide_icons_set(preset+1);
    CALL(0x1063c7e1u,z72_u32,z72_u32,z72_u32)(WIDE_AREA,WIDE_L);
    if (CALL(0x1063c7d5u,z72_u32,z72_u32)(WIDE_AREA)==WIDE_L) {
        CALL(0x1063c7e1u,z72_u32,z72_u32,z72_u32)(WIDE_SUBJECT,wide_subject[preset]);
        if (CALL(0x1063c7d5u,z72_u32,z72_u32)(WIDE_SUBJECT)==wide_subject[preset]) {
            __atomic_store_n(&wide_live_size,size,__ATOMIC_RELEASE);
            wide_selecting=0;
            return 0;
        }
    }
    wide_af_clear();
    wide_selecting=0;
    return 1;
}

static void wide_af_remember(z72_u32 preset)
{
    wide_boot_preset=0;
    struct z72_config config=z72_config_current;
    config.wide_af_selected=preset;
    z72_config_save(config);
}

static z72_u32 wide_af_choose(z72_u32 selection)
{
#ifdef Z72_SUBJECT_TRACKING
    if (selection==TRACK_SELECTION) {
        if (!wide_size[0] || !tracking_available() || wide_af_clear() || tracking_select_area()) return 1;
        wide_af_remember(TRACK_SELECTION);
        return 0;
    }
#endif
    if (selection>2 || (selection && !wide_size[selection-1])) return 1;
#ifdef Z72_SUBJECT_TRACKING
    tracking_leave();
#endif
    z72_u32 result=selection ? wide_af_select(selection-1) : wide_af_clear();
    if (!result) wide_af_remember(selection);
#ifdef Z72_SUBJECT_TRACKING
    else if (z72_config_current.wide_af_selected==TRACK_SELECTION) wide_af_remember(0);
#endif
    return result;
}

static void wide_af_resume(void)
{
    if (!wide_boot_preset || WORD(0xb25519b8u) ||
        WORD(0xb202f728u)!=0xb1821b1cu || !CALL(0x1052336bu,z72_u32,void)()) return;
#ifdef Z72_SUBJECT_TRACKING
    if (wide_boot_preset==TRACK_SELECTION) {
        if (!tracking_live()) return;
        /* Resume after native AF activation and area have been restored. */
        if (tracking_available() && tracking_area_selected()) {
            tracking_select_area();
            wide_boot_preset=0;
        } else {
            wide_af_remember(0);
        }
        return;
    }
#endif
    z72_u32 preset=wide_boot_preset-1;
    /* The camera has already restored its native AF settings. Bind the
     * custom preset only if they still represent that preset. */
    if (CALL(0x1063c7d5u,z72_u32,z72_u32)(WIDE_AREA)!=WIDE_L ||
        CALL(0x1063c7d5u,z72_u32,z72_u32)(WIDE_SUBJECT)!=wide_subject[preset]) {
        wide_boot_preset=0;
        return;
    }
    z72_u32 size=wide_size[preset];
    if (!wide_icons_load() || wide_geometry_set(size&255u,size>>8)) return;
    wide_icons_set(wide_boot_preset);
    __atomic_store_n(&wide_active_preset,wide_boot_preset,__ATOMIC_RELAXED);
    __atomic_store_n(&wide_live_size,size,__ATOMIC_RELEASE);
    wide_boot_preset=0;
}

static void wide_af_prepare_live(void)
{
    if (wide_restore_pending) wide_af_clear();
    if (wide_boot_preset) wide_af_resume();
#ifdef Z72_SUBJECT_TRACKING
    tracking_update();
#endif
}

static z72_u32 wide_af_live_scale(void)
{
    return WORD(0xb25519b8u) ? 0 : __atomic_load_n(&wide_live_size,__ATOMIC_ACQUIRE);
}

#include "wide_af_display.h"
#include "wide_af_hud.h"

#include "wide_af_photo_menu.h"
#include "wide_af_i_menu.h"

enum { WIDE_ROW_ID=0x2e20, WIDE_ROOT=0, WIDE_PRESET, WIDE_SIZE, WIDE_SUBJECT_VIEW,
#ifdef Z72_SUBJECT_TRACKING
       WIDE_AF_IMPROVEMENTS, WIDE_AF_REACQUIRE,
#endif
       WIDE_VIEW_COUNT, WIDE_STOCK_EVENTS=ROTATION_EVENTS, WIDE_MAX_ROWS=3, WIDE_FOCUS_ROW_ID=0x2e34,
       WIDE_PREVIEW_EDGES=4, WIDE_PREVIEW_HINT_ID=0x2e30, WIDE_PREVIEW_WIDGET_ID=0x2e31 };
static const z72_u32 wide_subject_values[]={1,2,0};
static struct {
    z72_u32 header[6],title[40],rows[WIDE_MAX_ROWS][7],maps[WIDE_MAX_ROWS];
    z72_u32 events[WIDE_MAX_ROWS][WIDE_STOCK_EVENTS+1][4];
} wide_views[WIDE_VIEW_COUNT];
static z72_u32 wide_callbacks[5],wide_layout[4],wide_view,wide_editing,wide_preset_row,wide_size_draft;
static const z72_u32 wide_size_row[]={72u|(388u<<16),702u|(436u<<16)};
static const z72_u32 wide_size_label[]={78u|(392u<<16),670u|(432u<<16)};
static const z72_u32 wide_size_layout[]={1,(z72_u32)wide_size_row,
    (z72_u32)wide_size_label,0x11874408u};

static void wide_preview_draw(z72_u32 widget)
{
    /* Erase and draw within one fixed menu widget. Moving the native line
     * widgets leaves their previous pixels in the menu surface. */
    CALL(0x10739c3bu,void,z72_u32)(widget);
    if (!WORD(widget+0x44)) return;
    z72_u32 origin=CALL(0x10739c47u,z72_u32,z72_u32)(widget);
    const z72_u32 *canvas=(const z72_u32 *)(widget+0x10);
    z72_u32 paint[2];
    CALL(0x1073b9f5u,void,z72_u32,z72_u32 *,z72_u32)(0,paint,1);
    CALL(0x1074c549u,void,const z72_u32 *,const z72_u32 *,z72_u32)(canvas,&origin,0);

    z72_u32 size=wide_size_draft,w=15*(size&255u),h=10*(size>>8);
    z72_u32 left=387-w/2,top=224-h/2,right=left+w,bottom=top+h;
    const z72_u32 edges[WIDE_PREVIEW_EDGES][2]={
        {left|(top<<16),right|((top+3)<<16)},
        {left|((bottom-3)<<16),right|(bottom<<16)},
        {left|(top<<16),(left+3)|(bottom<<16)},
        {(right-3)|(top<<16),right|(bottom<<16)}
    };
    for (z72_u32 i=0;i<WIDE_PREVIEW_EDGES;++i)
        CALL(0x1074c3cdu,void,const z72_u32 *,const z72_u32 *,const z72_u32 *,z72_u32)(
            edges[i],&origin,paint,0);
}

static z72_u32 wide_input_row_draw(z72_u32 widget)
{
    (void)widget;
    return 1;
}

static z72_u32 wide_preview_widget(z72_u32 scene)
{
    if (!scene) return 0;
    z72_u32 widget=CALL(0x10635cefu,z72_u32,z72_u32,z72_u32)(WIDE_PREVIEW_WIDGET_ID,scene);
    if (!widget || HALF(widget+4)!=6) return 0;
    return widget;
}

static z72_u32 wide_row_id(z72_u32 view,z72_u32 row)
{
#ifdef Z72_SUBJECT_TRACKING
    if (view>=WIDE_AF_IMPROVEMENTS) return WIDE_FOCUS_ROW_ID+(view-WIDE_AF_IMPROVEMENTS)*4+row;
#endif
    return WIDE_ROW_ID+view*4+row;
}

static void wide_page_init(const z72_u32 *page)
{
    z72_u32 row=0,choice=wide_view==WIDE_SUBJECT_VIEW;
    if (wide_view==WIDE_SIZE) {
        /* Keep native selection and navigation active on the backing row.
         * Only the separate, fixed preview widget paints the frame. */
        z72_u32 id=wide_row_id(WIDE_SIZE,0);
        CALL(0x10638983u,void,const z72_u32 *,z72_u32)(page,id);
        z72_u32 row=CALL(0x10634623u,z72_u32,z72_u32,z72_u32,z72_u32)(page[9],1,0);
        if (row && WORD(row)==id && HALF(row+4)==15 && WORD(row+0x4c)==0x10710a4du)
            WORD(row+0x4c)=(z72_u32)wide_input_row_draw;
        z72_u32 widget=wide_preview_widget(page[3]);
        if (widget && WORD(widget+0x4c)==0x106eefb3u)
            WORD(widget+0x4c)=(z72_u32)wide_preview_draw;
        return;
    }
    if (wide_view==WIDE_ROOT) row=wide_editing;
    if (wide_view==WIDE_PRESET) row=wide_preset_row;
    if (wide_view==WIDE_SUBJECT_VIEW)
        for (z72_u32 i=0;i<WIDE_MAX_ROWS;++i)
            if (wide_subject_values[i]==wide_subject[wide_editing]) row=i;
#ifdef Z72_SUBJECT_TRACKING
    if (wide_view==WIDE_AF_REACQUIRE) {
        row=!tracking_assist_enabled();
        choice=1;
    }
#endif
    z72_u32 id=wide_row_id(wide_view,row);
    if (CALL(0x1063bf77u,z72_u32,void)()!=1)
        CALL(0x10638983u,void,const z72_u32 *,z72_u32)(page,id);
    if (choice)
        CALL(0x1063ac0du,void,const z72_u32 *,z72_u32,z72_u32)(page,id,1);
}

static z72_u32 wide_header_owned(z72_u32 header)
{
    for (z72_u32 i=0;i<WIDE_VIEW_COUNT;++i)
        if (header==(z72_u32)wide_views[i].header) return 1;
    return 0;
}

static z72_u32 wide_show(z72_u32 view)
{
    z72_u32 root=view==WIDE_ROOT;
#ifdef Z72_SUBJECT_TRACKING
    root=root || view==WIDE_AF_IMPROVEMENTS;
#endif
    if (WORD(ROTATION_PAGE)!=2 ||
        (!root && !wide_header_owned(WORD(ROTATION_PAGE+0x28)))) return 1;
    wide_view=view;
    wide_views[WIDE_PRESET].title[0]=Z72_TEXT_WIDE_C1+wide_editing;
#ifdef Z72_SUBJECT_TRACKING
    if (view==WIDE_AF_IMPROVEMENTS)
        wide_views[WIDE_AF_IMPROVEMENTS].rows[0][5]=rotation_value(tracking_assist_enabled());
#endif
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(z72_u32)wide_callbacks;
    WORD(ROTATION_PAGE+0x28)=(z72_u32)wide_views[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,z72_u32,z72_u32)(2,0);
    return 0;
}

static z72_u32 wide_open(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (WORD(ROTATION_PAGE+0x24)) return 1;
    wide_editing=0;
    return wide_show(WIDE_ROOT);
}

#ifdef Z72_SUBJECT_TRACKING
static z72_u32 focus_improvements_open(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (WORD(ROTATION_PAGE+0x24)) return 1;
    return wide_show(WIDE_AF_IMPROVEMENTS);
}
#endif

static z72_u32 wide_back(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
#ifdef Z72_SUBJECT_TRACKING
    if (wide_view==WIDE_AF_REACQUIRE) return wide_show(WIDE_AF_IMPROVEMENTS);
#endif
    return wide_show(wide_view==WIDE_PRESET ? WIDE_ROOT : WIDE_PRESET);
}

static z72_u32 wide_pick(z72_u32 row)
{
#ifdef Z72_SUBJECT_TRACKING
    if (wide_view==WIDE_AF_IMPROVEMENTS) return wide_show(WIDE_AF_REACQUIRE);
    if (wide_view==WIDE_AF_REACQUIRE) {
        if (WORD(ROTATION_PAGE+0x28)!=(z72_u32)wide_views[WIDE_AF_REACQUIRE].header ||
            tracking_assist_save(!row)) return 1;
        return wide_show(WIDE_AF_IMPROVEMENTS);
    }
#endif
    if (wide_view==WIDE_ROOT) {
        wide_editing=row;
        wide_preset_row=0;
        return wide_show(WIDE_PRESET);
    }
    if (wide_view==WIDE_PRESET) {
        wide_preset_row=row;
        if (!row) wide_size_draft=wide_size[wide_editing];
        return wide_show(row ? WIDE_SUBJECT_VIEW : WIDE_SIZE);
    }
    z72_u32 active=wide_af_active();
    z72_u32 old_size=wide_size[wide_editing],old_subject=wide_subject[wide_editing];
    if (wide_view==WIDE_SIZE) wide_size[wide_editing]=wide_size_draft;
    else wide_subject[wide_editing]=wide_subject_values[row];
    if (active==wide_editing+1 && wide_af_select(wide_editing)) {
        wide_size[wide_editing]=old_size;
        wide_subject[wide_editing]=old_subject;
        return 1;
    }
    struct z72_config config=z72_config_current;
    config.wide_af[wide_editing]=wide_size[wide_editing]|(wide_subject[wide_editing]<<16);
    z72_config_save(config);
    return wide_show(WIDE_PRESET);
}

#define WIDE_PICK(row) \
    static z72_u32 wide_pick##row(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d) \
    { (void)a; (void)b; (void)c; (void)d; return wide_pick(row); }
WIDE_PICK(0)
WIDE_PICK(1)
WIDE_PICK(2)
#undef WIDE_PICK

static z72_u32 wide_size_adjust(z72_u32 shift,int direction)
{
    if (wide_view!=WIDE_SIZE || WORD(ROTATION_PAGE+0x28)!=(z72_u32)wide_views[WIDE_SIZE].header) return 1;
    z72_u32 before=wide_size_draft;
    int value=(int)((before>>shift)&255u)+direction;
    if (value>=(int)Z72_WIDE_AF_MIN && value<=(int)Z72_WIDE_AF_MAX) {
        wide_size_draft=(before&~(255u<<shift))|((z72_u32)value<<shift);
        z72_u32 widget=wide_preview_widget(WORD(ROTATION_PAGE+0x0c));
        if (widget) {
            CALL(0x1063618bu,void,z72_u32,z72_u32)(widget,1);
            CALL(0x1063618bu,void,z72_u32,z72_u32)(WORD(widget+0x48),1);
            /* Draw the dirty widgets before presenting the menu, as stock navigation does. */
            CALL(0x1061db49u,void,z72_u32)(ROTATION_PAGE);
            CALL(0x10635a8bu,void,z72_u32,z72_u32,z72_u32)(0x106355a1u,3,7);
        }
    }
    return 0;
}

#define WIDE_ADJUST(name,shift,direction) \
    static z72_u32 name(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d) \
    { (void)a; (void)b; (void)c; (void)d; return wide_size_adjust(shift,direction); }
WIDE_ADJUST(wide_left,0,-1)
WIDE_ADJUST(wide_right,0,1)
WIDE_ADJUST(wide_up,8,1)
WIDE_ADJUST(wide_down,8,-1)
#undef WIDE_ADJUST

static void wide_menu_prepare(void)
{
    const z72_u32 *setup=(const z72_u32 *)Z72_SETUP_MENU;
    const z72_u32 *stock=(const z72_u32 *)0xb18545fcu;
    const z72_u32 *events=(const z72_u32 *)WORD(stock[4]);

    const z72_u32 counts[]={2,2,1,3,
#ifdef Z72_SUBJECT_TRACKING
        1,2,
#endif
    };
    const z72_u32 titles[]={Z72_TEXT_WIDE_TITLE,Z72_TEXT_WIDE_C1,Z72_TEXT_WIDE_SIZE,Z72_TEXT_WIDE_SUBJECT,
#ifdef Z72_SUBJECT_TRACKING
        Z72_TEXT_AF_IMPROVEMENTS,Z72_TEXT_AF_REACQUIRE,
#endif
    };
    const z72_u32 labels[]={Z72_TEXT_WIDE_C1,0,0,Z72_TEXT_WIDE_PEOPLE,
#ifdef Z72_SUBJECT_TRACKING
        Z72_TEXT_AF_REACQUIRE,0x52fu, /* Stock Enable and Disable. */
#endif
    };
    const z72_u32 preset_labels[]={Z72_TEXT_WIDE_SIZE,Z72_TEXT_WIDE_SUBJECT};
    const z72_menu_callback picks[]={wide_pick0,wide_pick1,wide_pick2};
    for (z72_u32 i=1;i<4;++i) wide_layout[i]=WORD(setup[1]+4*i);
    wide_layout[0]=2;
    for (z72_u32 view=0;view<WIDE_VIEW_COUNT;++view) {
        z72_u32 choice=view==WIDE_SUBJECT_VIEW;
        z72_u32 nested=view!=WIDE_ROOT;
#ifdef Z72_SUBJECT_TRACKING
        choice=choice || view==WIDE_AF_REACQUIRE;
        nested=nested && view!=WIDE_AF_IMPROVEMENTS;
#endif
        z72_u32 *header=wide_views[view].header;
        const z72_u32 *source=choice ? stock : setup;
        for (z72_u32 i=0;i<6;++i) header[i]=source[i];
        header[1]=choice ? (view==WIDE_SUBJECT_VIEW ? 0x1187c510u : stock[1]) :
            (z72_u32)(view==WIDE_SIZE ? wide_size_layout : wide_layout);
        for (z72_u32 i=0;i<24;++i) wide_views[view].title[i]=WORD(stock[2]+4*i);
        wide_views[view].title[0]=titles[view];
        wide_views[view].title[1]=1;
        if (view==WIDE_SIZE) {
            z72_u32 *hint=&wide_views[view].title[16];
            for (z72_u32 i=0;i<8;++i) hint[i]=wide_views[view].title[i];
            hint[0]=Z72_TEXT_WIDE_SAVE;
            hint[4]=78u|(388u<<16);
            hint[5]=694u|(436u<<16);
            hint[7]=WIDE_PREVIEW_HINT_ID<<16;
            z72_u32 *preview=&wide_views[view].title[24];
            preview[1]=6;
            preview[4]=226u|(114u<<16);
            preview[5]=548u|(334u<<16);
            preview[6]=1;
            preview[7]=(WIDE_PREVIEW_WIDGET_ID<<16)|1;
        }
        header[2]=(z72_u32)wide_views[view].title;
        header[3]=(z72_u32)wide_views[view].rows;
        header[4]=(z72_u32)wide_views[view].maps;
        header[5]=(counts[view]<<16)|counts[view];
        for (z72_u32 row=0;row<counts[view];++row) {
            z72_u32 *item=wide_views[view].rows[row];
            for (z72_u32 i=0;i<7;++i) item[i]=WORD(source[3]+i*4);
            item[0]=view==WIDE_PRESET ? preset_labels[row] : labels[view]+row;
            item[1]=item[2]=0xffffu;
            item[3]=0;
            item[4]=choice || view==WIDE_SIZE ? 0x1000u : 0x1001u;
            item[5]=choice || view==WIDE_SIZE ? 0xffffu : 0xd7u;
            item[6]=wide_row_id(view,row);
            z72_u32 event_count=WIDE_STOCK_EVENTS+(view==WIDE_SIZE);
            for (z72_u32 i=0;i<event_count;++i) {
                z72_u32 *map=wide_views[view].events[row][i];
                z72_u32 index=i==WIDE_STOCK_EVENTS ? WIDE_STOCK_EVENTS-1 : i;
                for (z72_u32 j=0;j<4;++j) map[j]=events[index*4+j];
                if (view==WIDE_SIZE && i==WIDE_STOCK_EVENTS-1) map[0]=0x270;
                if (map[0]==0x257 || map[0]==0x25c || map[0]==0x27d || map[0]==0x252 ||
                    (view==WIDE_SIZE && map[0]==0x24f)) {
                    map[1]=(z72_u32)picks[row]; map[2]=0; map[3]=0x430;
                }
                if (map[0]==0x256 || map[0]==0x271 || map[0]==0x2cf || map[0]==0x27f) {
                    map[1]=nested ? (z72_u32)wide_back : 0;
                    map[2]=0; map[3]=nested ? 0x430 : 0x431;
                }
                if (view==WIDE_SIZE) {
                    z72_menu_callback adjust=0;
                    switch (map[0]) {
                    case 0x256: case 0x271: adjust=wide_left; break;
                    case 0x257: case 0x270: adjust=wide_right; break;
                    case 0x254: case 0x26e: adjust=wide_up; break;
                    case 0x255: case 0x26f: adjust=wide_down; break;
                    }
                    if (adjust) {
                        map[1]=(z72_u32)adjust; map[2]=0; map[3]=0x430;
                    } else if (map[0]==0x25c || map[0]==0x27d) {
                        map[1]=0;
                    }
                }
            }
            wide_views[view].maps[row]=(z72_u32)wide_views[view].events[row];
        }
    }
    for (z72_u32 i=1;i<5;++i) wide_callbacks[i]=WORD(0x10969c14u+i*4);
    wide_callbacks[0]=(z72_u32)wide_page_init;
}

static z72_u32 wide_ready(void)
{
#ifdef Z72_SUBJECT_TRACKING
    if (!tracking_ready()) return 0;
#endif
    return WORD(Z72_SETUP_MENU+4)==0x1187c3d0u && WORD(0x1187c510u)==3 &&
        wide_geometry_ready() && wide_display_ready() && wide_icons_ready() &&
        wide_hud_ready() && wide_photo_menu_ready() && wide_i_ready();
}

static void wide_install(void)
{
    wide_boot_preset=z72_config_current.wide_af_selected;
    for (z72_u32 i=0;i<2;++i) {
        z72_u32 value=z72_config_current.wide_af[i];
        wide_size[i]=value&0xffffu;
        wide_subject[i]=value>>16;
    }
    wide_photo_menu_install();
    wide_i_install();
    wide_display_install();
#ifdef Z72_SUBJECT_TRACKING
    tracking_assist=z72_config_current.tracking_assist;
    tracking_reacquire_install();
    /* The shared shooting-key slot delegates the original DISP path. */
    WORD(TRACK_KEY_SLOT)=(z72_u32)tracking_live_key;
#endif
}
