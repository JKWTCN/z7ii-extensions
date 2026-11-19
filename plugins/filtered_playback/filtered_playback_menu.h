/* SPDX-License-Identifier: MIT */
enum {
    ROOT_HEADER=0xb1853984u, ROOT_PAGE=0xb2bb7524u, ROOT_ROWS=11, ROOT_EVENTS=61,
    I_HEADER=0xb1856c24u, I_PAGE=0xb2bc4720u, I_ROWS=14, I_EVENTS=42,
    EMPTY_HEADER=0xb1856c0cu, EMPTY_PAGE=0xb2bc46f4u, EMPTY_EVENTS=11,
    FILTER_CHOICE_PAGE=ROTATION_PAGE, I_CHOICE_PAGE=0xb2bc3f64u, CHOICE_EVENTS=16,
    DATE_HEADER=0xb1853fb4u, DATE_EVENTS=19, I_CHOICE_EVENTS=27,
    FILTER_ROW_ID=0x2e40u
};
/* Page indices are separate from Nikon's row IDs and native page numbers. */
enum { CONDITIONS, PROTECTION, MEDIA, RATING, DATE, DATE_EDITOR, SWITCH, FILTER_VIEW_COUNT };
enum { FILTER_VALUE_ACTION=0xd7 };
struct filter_page {
    z72_u32 header[6],title[24],rows[7][7],maps[7],events[7][CHOICE_EVENTS][4];
};
static struct filter_page pages[FILTER_VIEW_COUNT];
#define PAGE(view) pages[view]
static struct {
    z72_u32 root_rows[ROOT_ROWS+1][7],root_maps[ROOT_ROWS+1],root_events[ROOT_EVENTS][4];
    z72_u32 i_rows[I_ROWS+2][7],i_maps[I_ROWS+2],i_events[I_ROWS+2][I_EVENTS][4];
    z72_u32 empty_events[EMPTY_EVENTS+1][4],empty_map[1],layout[4],callbacks[2][5];
    z72_u32 date_widgets[15][8],date_events[DATE_EVENTS][4];
    z72_u32 i_headers[FILTER_VIEW_COUNT][6],i_titles[FILTER_VIEW_COUNT][5][8];
    z72_u32 i_choice_maps[FILTER_VIEW_COUNT][7],i_choice_events[FILTER_VIEW_COUNT][7][I_CHOICE_EVENTS][4];
} filter_menu;
static z72_u32 view=CONDITIONS,focus,opened_from_i,refresh_i;
enum { FROM_MENU, FROM_I, FROM_EMPTY };
static struct playback_date {
    unsigned short year;
    unsigned char month,day;
} draft_date;
static z72_u32 date_field;
static void date_page_init(const z72_u32 *page);

static z72_u32 choice_address(void)
{
    return opened_from_i ? I_CHOICE_PAGE : FILTER_CHOICE_PAGE;
}

static z72_u32 *view_header(z72_u32 page_view)
{
    return opened_from_i ? filter_menu.i_headers[page_view] : PAGE(page_view).header;
}

static z72_u32 settings(void)
{
    return __atomic_load_n(&filter_settings,__ATOMIC_ACQUIRE);
}

static void save_settings(z72_u32 next)
{
    z72_u32 previous=settings();
    if (previous==next && !z72_config_pending) return;
    if (previous!=next) {
        if (opened_from_i==FROM_I && ((previous|next)&Z72_FILTER_ENABLED)) refresh_i=1;
        __atomic_store_n(&filter_settings,next,__ATOMIC_RELEASE);
    }
    struct z72_config config=z72_config_current;
    config.playback_filter=next;
    z72_config_save(config);
}

static void begin_date(void)
{
    z72_u32 packed=settings()>>Z72_DATE_SHIFT;
    if (packed) {
        draft_date.year=1980+(packed>>9);
        draft_date.month=(packed>>5)&15;
        draft_date.day=packed&31;
    } else {
        z72_u32 clock[2]={0};
        union { struct playback_date date; z72_u32 buffer[4]; } local={0};
        NATIVE(0x106fce95u,void,z72_u32 *,z72_u32)(clock,1);
        NATIVE(0x1063c9d7u,void,const z72_u32 *,void *)(clock,&local);
        draft_date=local.date;
    }
    /* The original date widget encodes the year as two digits after "20". */
    if (draft_date.year<2000 || draft_date.year>2099 || draft_date.month<1 ||
        draft_date.month>12 || draft_date.day<1 ||
        draft_date.day>z72_filter_days_in_month(draft_date.year,draft_date.month))
        draft_date=(struct playback_date){2000,1,1};
    date_field=0;
}

static void redraw_page(const z72_u32 *page)
{
    NATIVE(0x1061db49u,void,const z72_u32 *)(page);
    NATIVE(0x10635a8bu,void,z72_u32,z72_u32,z72_u32)(0x106355a1u,3,7);
}

static void rating_markers(const z72_u32 *page,z72_u32 selected)
{
    struct filter_page *rating=&PAGE(RATING);
    for (z72_u32 row=0;row<COUNT(rating->rows);++row) {
        z72_u32 value=(selected&(1u<<row)) ? 0xd5u : 0xd6u;
        rating->rows[row][5]=value;
        z72_u32 widget=NATIVE(0x1063466du,z72_u32,z72_u32,z72_u32,z72_u32)(page[9],row,0);
        if (widget) {
            NATIVE(0x106361cdu,void,z72_u32,z72_u32)(widget,value);
            z72_u32 highlight=NATIVE(0x10634623u,z72_u32,z72_u32,z72_u32,z72_u32)(page[9],1,row);
            NATIVE(0x1063618bu,void,z72_u32,z72_u32)(highlight,1);
            NATIVE(0x1063ba3bu,void,const z72_u32 *,const z72_u32 *,z72_u32)(page,rating->rows[row],row);
        }
    }
}

static void set_markers(const z72_u32 *page,z72_u32 selected)
{
    const struct filter_page *current=&PAGE(view);
    z72_u32 rows=current->header[5]&0xffff;
    for (z72_u32 row=0;row<rows;++row)
        NATIVE(0x1063ac0du,void,const z72_u32 *,z72_u32,z72_u32)(
            page,current->rows[row][6],(selected>>row)&1);
}

static void page_init(const z72_u32 *page)
{
    z72_u32 config=settings(),row=focus,selected=0;
    switch (view) {
    case SWITCH: row=(config&Z72_FILTER_ENABLED) ? 0 : 1; selected=1u<<row; break;
    case PROTECTION: row=(config>>Z72_PROTECTION_SHIFT)&3; selected=1u<<row; break;
    case MEDIA: row=(config>>Z72_MEDIA_SHIFT)&3; selected=1u<<row; break;
    case RATING:
        selected=(config>>Z72_RATING_SHIFT)&RATING_ALL;
        if (!selected) selected=1;
        break;
    case DATE: row=(config>>Z72_DATE_SHIFT)!=0; selected=1u<<row; break;
    case DATE_EDITOR: date_page_init(page); return;
    }
    if (NATIVE(0x1063bf77u,z72_u32,void)()!=1)
        NATIVE(0x10638983u,void,const z72_u32 *,z72_u32)(page,PAGE(view).rows[row][6]);
    if (view==RATING) rating_markers(page,selected);
    else if (selected) set_markers(page,selected);
}

static z72_u32 filter_header_owned(z72_u32 header)
{
    for (z72_u32 i=0;i<FILTER_VIEW_COUNT;++i) if (header==(z72_u32)PAGE(i).header) return 1;
    return 0;
}

static z72_u32 show_page(z72_u32 next,z72_u32 row)
{
    z72_u32 page=choice_address(),kind=opened_from_i ? 3 : 2;
    if (WORD(page)!=kind || NATIVE(0x1061daafu,z72_u32,z72_u32,z72_u32)(kind,0)!=page) return 1;
    view=next; focus=row;
    *(volatile unsigned short *)(page+0x16)=0;
    *(volatile unsigned short *)(page+0x18)=0;
    if (kind==2) WORD(page+0x10)=(z72_u32)filter_menu.callbacks[0];
    WORD(page+0x28)=(z72_u32)view_header(view);
    __asm__ volatile("dmb sy" ::: "memory");
    NATIVE(0x1063ab75u,void,z72_u32,z72_u32)(kind,0);
    return 0;
}

static z72_u32 from_i_menu(void)
{
    return NATIVE(0x1063c2b3u,z72_u32,void)()==I_PAGE;
}

static z72_u32 open_conditions(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    opened_from_i=from_i_menu() ? FROM_I : FROM_MENU;
    if (!opened_from_i && WORD(FILTER_CHOICE_PAGE+0x24)) return 1;
    return show_page(CONDITIONS,0);
}

static z72_u32 open_switch(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    /* The ordinary empty-playback page can open the switch directly so a
     * filter with no matches can always be turned off. Its native entry
     * callback already counts the catalogue afresh when Back restores it. */
    opened_from_i=from_i_menu() ? FROM_I : FROM_EMPTY;
    if (opened_from_i==FROM_EMPTY && !(settings()&Z72_FILTER_ENABLED)) return 0;
    return show_page(SWITCH,0);
}

static z72_u32 back_to_conditions(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    if (view==DATE_EDITOR) return show_page(DATE,0);
    return show_page(CONDITIONS,view-PROTECTION);
}

static z72_u32 pick(z72_u32 row)
{
    z72_u32 config=settings();
    switch (view) {
    case CONDITIONS: return show_page(PROTECTION+row,0);
    case SWITCH:
        save_settings((config&~Z72_FILTER_ENABLED)|(row==0));
        return 0;
    case PROTECTION:
        save_settings((config&~(3u<<Z72_PROTECTION_SHIFT))|(row<<Z72_PROTECTION_SHIFT));
        return show_page(CONDITIONS,0);
    case MEDIA:
        save_settings((config&~(3u<<Z72_MEDIA_SHIFT))|(row<<Z72_MEDIA_SHIFT));
        return show_page(CONDITIONS,1);
    case RATING: {
        z72_u32 mask=(config>>Z72_RATING_SHIFT)&RATING_ALL;
        mask=row ? mask^(1u<<row) : 0;
        save_settings((config&~(RATING_ALL<<Z72_RATING_SHIFT))|(mask<<Z72_RATING_SHIFT));
        const z72_u32 *page=(const z72_u32 *)choice_address();
        rating_markers(page,mask ? mask : 1);
        redraw_page(page);
        break;
    }
    case DATE:
        if (row) { begin_date(); return show_page(DATE_EDITOR,0); }
        save_settings(config&((1u<<Z72_DATE_SHIFT)-1));
        return show_page(CONDITIONS,3);
    }
    return 0;
}

#include "filtered_playback_date.h"

#define PICK(n) static z72_u32 pick##n(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d) \
    { (void)a; (void)b; (void)c; (void)d; return pick(n); }
PICK(0) PICK(1) PICK(2) PICK(3) PICK(4) PICK(5) PICK(6)
#undef PICK

static z72_u32 close_i(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{
    z72_u32 result=NATIVE(0x10723aebu,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(a,b,c,d);
    if (refresh_i && !from_i_menu()) {
        refresh_i=0;
        /* Native playback page selection rebuilds both the single-image and
         * thumbnail views from the image-control service. */
        NATIVE(0x10722e83u,void,void)();
    }
    return result;
}

static z72_u32 check_menu(void)
{
    if (WORD(ROOT_HEADER)!=0x1002u || WORD(ROOT_HEADER+12)!=0x117bae48u ||
        WORD(ROOT_HEADER+16)!=0x117c3464u || WORD(ROOT_HEADER+20)!=0x8000bu ||
        WORD(ROOT_PAGE)!=0x00120002u || WORD(ROOT_PAGE+0x28)!=ROOT_HEADER ||
        WORD(ROOT_PAGE+4)!=WORD(SETUP_PAGE+4) ||
        WORD(I_HEADER)!=0x800u || WORD(I_HEADER+12)!=0x11896980u ||
        WORD(I_HEADER+16)!=0x118aba44u || WORD(I_HEADER+20)!=0x7000eu ||
        WORD(I_PAGE)!=0x002d0003u || WORD(I_PAGE+0x28)!=I_HEADER ||
        WORD(EMPTY_PAGE)!=0x002c0003u || WORD(EMPTY_PAGE+0x28)!=EMPTY_HEADER ||
        WORD(EMPTY_HEADER+16)!=0x118aba40u ||
        WORD(0x1189ea34u+(EMPTY_EVENTS-1)*16)!=0x31au ||
        WORD(0xb29d2ec0u)!=0x1063dc05u ||
        WORD(DATE_HEADER)!=0x2000u || WORD(DATE_HEADER+8)!=0x117c4768u ||
        WORD(DATE_HEADER+16)!=0x11810e9cu ||
        WORD(0x1189fce4u+25*16)!=0x31au ||
        WORD(0x1075334cu)!=I_CHOICE_PAGE ||
        NATIVE(0x1061daafu,z72_u32,z72_u32,z72_u32)(2,0)!=FILTER_CHOICE_PAGE) return 1;
    for (z72_u32 i=0;i<11;++i) if (WORD(FILTER_CHOICE_PAGE+i*4) || WORD(I_CHOICE_PAGE+i*4)) return 1;
    if (WORD(0xb18545fcu)!=0x9008u || WORD(0xb1854610u)!=0x20002u ||
        WORD(0x1180b848u+(ROOT_EVENTS-1)*16)!=0x31au ||
        WORD(0x117ef0a8u+(CHOICE_EVENTS-1)*16)!=0x31au) return 1;
    /* All legitimate i event maps terminate before the next stock map. */
    for (z72_u32 row=0;row<I_ROWS;++row) {
        z72_u32 map=WORD(0x118aba44u+row*4),i=0;
        while (i<I_EVENTS && WORD(map+16*i)!=0x31au) ++i;
        if (i==I_EVENTS) return 1;
    }
    return 0;
}

static z72_u32 is_select(z72_u32 event)
{
    return event==0x257 || event==0x25c || event==0x27d;
}

static void prepare_root(void)
{
    const z72_u32 *stock=(const z72_u32 *)ROOT_HEADER;
    copy_bytes(filter_menu.root_rows+1,(const void *)stock[3],ROOT_ROWS*sizeof(filter_menu.root_rows[0]));
    copy_bytes(filter_menu.root_maps+1,(const void *)stock[4],ROOT_ROWS*4);
    const z72_u32 row[]={TEXT_CONDITIONS,0xffff,0xffff,0,0x1001,FILTER_VALUE_ACTION,FILTER_ROW_ID-1};
    copy_bytes(filter_menu.root_rows[0],row,sizeof(row));
    copy_bytes(filter_menu.root_events,(const void *)0x1180b848u,sizeof(filter_menu.root_events));
    for (z72_u32 i=0;i<ROOT_EVENTS;++i) {
        z72_u32 *event=filter_menu.root_events[i];
        if (is_select(event[0]) || event[0]==0x270) {
            event[1]=(z72_u32)open_conditions; event[2]=0; event[3]=0x430;
        } else if (event[1]!=0x1063aea9u && event[1]!=0x1063aeb5u &&
                   event[1]!=0x1063aec1u && event[1]!=0x1063aec5u &&
                   event[1]!=0x1063aec7u && event[1]!=0x1063aeedu) {
            event[1]=event[2]=0; event[3]=event[3]==0x431 ? 0x431 : 0x430;
        }
    }
    filter_menu.root_maps[0]=(z72_u32)filter_menu.root_events;
}

static void prepare_i(void)
{
    const z72_u32 *stock=(const z72_u32 *)I_HEADER;
    copy_bytes(filter_menu.i_rows+2,(const void *)stock[3],I_ROWS*sizeof(filter_menu.i_rows[0]));
    for (z72_u32 row=0;row<I_ROWS+2;++row) {
        z72_u32 source=WORD(stock[4]+(row<2 ? 4 : (row-2)*4));
        filter_menu.i_maps[row]=(z72_u32)filter_menu.i_events[row];
        if (row<2) {
            copy_bytes(filter_menu.i_rows[row],filter_menu.i_rows[3],sizeof(filter_menu.i_rows[row]));
            filter_menu.i_rows[row][0]=row ? TEXT_CONDITIONS : TEXT_FILTER;
            filter_menu.i_rows[row][6]=FILTER_ROW_ID-2-row;

        }
        for (z72_u32 i=0;i<I_EVENTS;++i) {
            z72_u32 *event=filter_menu.i_events[row][i];
            copy_bytes(event,(const void *)(source+i*16),16);
            if (event[1]==0x10723aebu) event[1]=(z72_u32)close_i;
            if (row<2 && is_select(event[0])) {
                event[1]=row ? (z72_u32)open_conditions : (z72_u32)open_switch;
                event[2]=0; event[3]=0x4a;
            }
            if (event[0]==0x31a) break;
        }
    }
}

static void prepare_empty_page(void)
{
    copy_bytes(filter_menu.empty_events,(const void *)0x1189ea34u,(EMPTY_EVENTS-1)*16);
    z72_u32 *event=filter_menu.empty_events[EMPTY_EVENTS-1];
    event[0]=0x252; event[1]=(z72_u32)open_switch; event[3]=0x4a;
    copy_bytes(filter_menu.empty_events[EMPTY_EVENTS],(const void *)(0x1189ea34u+(EMPTY_EVENTS-1)*16),16);
    filter_menu.empty_map[0]=(z72_u32)filter_menu.empty_events;
}

static void back_from_i_choice(void)
{
    if (view==CONDITIONS || view==SWITCH)
        NATIVE(0x1063ab75u,void,z72_u32,z72_u32)(3,opened_from_i==FROM_I ? 0x2d : 0x2c);
    else
        back_to_conditions(0,0,0,0);
}

static void switch_on(void) { pick(0); back_from_i_choice(); }
static void switch_off(void) { pick(1); back_from_i_choice(); }

static void prepare_i_choices(z72_u32 page_view,z72_u32 count,z72_u32 title,const z72_u32 *actions)
{
    z72_u32 *header=filter_menu.i_headers[page_view];
    copy_bytes(header,PAGE(page_view).header,sizeof(PAGE(page_view).header));
    header[0]=0x8808u;
    header[1]=count==7 ? 0x1187cb00u :
        count==4 ? 0x1187cad0u : count==3 ? 0x1187cac0u : 0x1187cab0u;
    header[2]=(z72_u32)filter_menu.i_titles[page_view];
    header[4]=(z72_u32)filter_menu.i_choice_maps[page_view];
    z72_u32 (*widgets)[8]=filter_menu.i_titles[page_view];
    copy_bytes(widgets,(const void *)0x118940b0u,sizeof(filter_menu.i_titles[0]));
    copy_bytes(widgets[3],(const void *)0x117cd008u,32);
    z72_u32 rect=WORD(header[1]+4);
    z72_u32 first=WORD(rect)>>16,last=WORD(rect+(count-1)*8+4)>>16;
    widgets[0][4]=((first-44)<<16)|0x4e;
    widgets[0][5]=((last+12)<<16)|0x280;
    widgets[1][4]=((first-46)<<16)|0x4c;
    widgets[1][5]=((last+14)<<16)|0x282;
    widgets[2][0]=title;
    widgets[2][4]=((first-40)<<16)|0x60;
    widgets[2][5]=((first-2)<<16)|0x270;
    for (z72_u32 row=0;row<count;++row) {
        z72_u32 (*map)[4]=filter_menu.i_choice_events[page_view][row];
        filter_menu.i_choice_maps[page_view][row]=(z72_u32)map;
        copy_bytes(map,(const void *)0x1189fce4u,26*16);
        copy_bytes(map[26],map[25],16);
        map[25][0]=0x27f;
        for (z72_u32 i=0;i<I_CHOICE_EVENTS;++i) {
            z72_u32 *event=map[i];
            if (event[1]==0x10723aebu) event[1]=(z72_u32)close_i;
            if (is_select(event[0]) || event[0]==0x270) {
                event[1]=actions[row]; event[2]=0; event[3]=0x4a;
                if (page_view==SWITCH) event[1]=row ? (z72_u32)switch_off : (z72_u32)switch_on;
            } else if (event[0]==0x256 || event[0]==0x271 || event[0]==0x252 || event[0]==0x27f) {
                event[1]=(z72_u32)back_from_i_choice; event[2]=0; event[3]=0x4a;
            }
        }
    }
}

static void prepare_pages(void)
{
    static const struct {
        z72_u32 title,count,layout,labels[7];
    } spec[]={
        {TEXT_CONDITIONS,4,0,{TEXT_PROTECTION,TEXT_MEDIA,0x49b,TEXT_DATE}},
        {TEXT_PROTECTION,3,0x1187c510,{TEXT_ANY,TEXT_PROTECTED,TEXT_UNPROTECTED}},
        {TEXT_MEDIA,3,0x1187c510,{TEXT_ALL,TEXT_PHOTOS,0x366}},
        {0x49b,7,0x1187c560,{TEXT_ANY,TEXT_UNRATED,0x14f,0x14e,0x14d,0x14c,0x14b}},
        {TEXT_DATE,2,0x1187c500,{TEXT_ANY,0x159}},
        {0,0,0,{0}}, /* The date editor owns a native widget layout. */
        {TEXT_FILTER,2,0x1187c500,{TEXT_ON,TEXT_OFF}},
    };
    const z72_u32 *choice=(const z72_u32 *)0xb18545fcu;
    const z72_u32 actions[]={(z72_u32)pick0,(z72_u32)pick1,(z72_u32)pick2,
        (z72_u32)pick3,(z72_u32)pick4,(z72_u32)pick5,(z72_u32)pick6};
    copy_bytes(filter_menu.layout,(const void *)WORD(ROOT_HEADER+4),sizeof(filter_menu.layout));
    filter_menu.layout[0]=4;
    for (z72_u32 index=0;index<COUNT(spec);++index) {
        const z72_u32 this_view=index,count=spec[index].count;
        struct filter_page *page=&pages[index];
        if (this_view==DATE_EDITOR) {
            prepare_date_page();
            continue;
        }
        copy_bytes(page->header,choice,sizeof(page->header));
        copy_bytes(page->title,(const void *)choice[2],sizeof(page->title));
        page->title[0]=spec[index].title; page->title[1]=1;
        if (this_view==CONDITIONS || this_view==RATING) page->header[0]=WORD(ROOT_HEADER);
        page->header[1]=spec[index].layout ? spec[index].layout : (z72_u32)filter_menu.layout;
        page->header[2]=(z72_u32)page->title;
        page->header[3]=(z72_u32)page->rows;
        page->header[4]=(z72_u32)page->maps;
        page->header[5]=(count<<16)|count;
        for (z72_u32 row=0;row<count;++row) {
            z72_u32 *item=page->rows[row];
            item[0]=spec[index].labels[row];
            item[1]=item[2]=0xffffu;
            item[4]=this_view==CONDITIONS ? 0x1001u : 0x1000u;
            item[5]=this_view==CONDITIONS ? FILTER_VALUE_ACTION : 0xffffu;
            if (this_view==RATING) {
                item[3]=4; item[4]=5; item[5]=row ? 0xd6 : 0xd5;
            }
            item[6]=FILTER_ROW_ID+index*8+row;
            page->maps[row]=(z72_u32)page->events[row];
            copy_bytes(page->events[row],(const void *)0x117ef0a8u,sizeof(page->events[row]));
            for (z72_u32 i=0;i<CHOICE_EVENTS;++i) {
                z72_u32 *event=page->events[row][i];
                if (is_select(event[0])) {
                    event[1]=actions[row]; event[2]=0; event[3]=this_view==SWITCH ? 0x431 : 0x430;
                }
                if (this_view!=CONDITIONS && this_view!=SWITCH &&
                    (event[0]==0x256 || event[0]==0x271 || event[0]==0x2cf || event[0]==0x27f)) {
                    event[1]=(z72_u32)back_to_conditions; event[2]=0; event[3]=0x430;
                }
            }
        }
        prepare_i_choices(this_view,count,spec[index].title,actions);
    }
}

static void install_menu(void)
{
    prepare_root();
    prepare_i();
    prepare_empty_page();
    prepare_pages();
    copy_bytes(filter_menu.callbacks[0],(const void *)0x10969c14u,sizeof(filter_menu.callbacks[0]));
    filter_menu.callbacks[0][0]=(z72_u32)page_init;
    filter_menu.callbacks[1][0]=(z72_u32)page_init;
    filter_menu.callbacks[1][1]=0x10703273u; /* Native no-op choice dispatch. */
    filter_menu.callbacks[1][2]=0x1074fb7du; /* Playback scene lifecycle. */
    filter_menu.callbacks[1][3]=0x10703275u;
    /* Use an unused page in the playback family, following the original
     * playback i dialog's leaf-page lifecycle and return targets. */
    WORD(I_CHOICE_PAGE+0x10)=(z72_u32)filter_menu.callbacks[1];
    WORD(I_CHOICE_PAGE+0x28)=(z72_u32)filter_menu.i_headers[CONDITIONS];
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(I_CHOICE_PAGE)=3;
    WORD(ROOT_HEADER+12)=(z72_u32)filter_menu.root_rows;
    WORD(ROOT_HEADER+16)=(z72_u32)filter_menu.root_maps;
    WORD(ROOT_HEADER+20)=0x8000cu;
    WORD(I_HEADER+12)=(z72_u32)filter_menu.i_rows;
    WORD(I_HEADER+16)=(z72_u32)filter_menu.i_maps;
    WORD(I_HEADER+20)=0x70010u;
    WORD(EMPTY_HEADER+16)=(z72_u32)filter_menu.empty_map;
}
