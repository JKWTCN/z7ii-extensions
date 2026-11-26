/* SPDX-License-Identifier: MIT */
enum {
    MERGE_MENU_GROUPS, MERGE_MENU_FORMAT, MERGE_MENU_PROGRESS, MERGE_MENU_PAGES,
    MERGE_MENU_CANVAS=0x2ef0, MERGE_MENU_LABEL=0x2ef1, MERGE_MENU_STATUS=0x2ef7,
    MERGE_MENU_PREV=0x2ef8, MERGE_MENU_NEXT=0x2ef9, MERGE_MENU_PERCENT=0x2efa,
    MERGE_MENU_ROW=0x2efb,
    MERGE_MENU_INPUT_SLOT=0xb1824f14u, MERGE_MENU_INPUT_NATIVE=0x1044f6e1u,
    MERGE_MENU_PENDING=~0u
};

static struct {
    z72_u32 header[6],title[13][8],rows[2][7],maps[2],events[2][PIXEL_MENU_EVENTS][4];
} merge_menus[MERGE_MENU_PAGES];
static z72_u32 merge_menu_callbacks[5],merge_menu_view,merge_menu_seen,merge_menu_focus;
static z72_u32 merge_menu_refresh_pending;
static z72_u32 merge_menu_input(z72_u32 event,z72_u32 message,z72_u32 c,z72_u32 d);

static void merge_menu_release_input(void)
{
    z72_u32 owner=(z72_u32)merge_menu_input;
    __atomic_compare_exchange_n(&WORD(MERGE_MENU_INPUT_SLOT),&owner,MERGE_MENU_INPUT_NATIVE,
                                0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}

static int merge_menu_header(z72_u32 header)
{
    for (unsigned i=0;i<MERGE_MENU_PAGES;++i)
        if (header==(z72_u32)merge_menus[i].header) return 1;
    return 0;
}

static int merge_menu_active(unsigned view)
{
    return WORD(ROTATION_PAGE+0x28)==(z72_u32)merge_menus[view].header &&
        !pixel_merge_stop_requested() &&
        CALL(0x1063c2b3u,z72_u32,void)()==ROTATION_PAGE;
}

static z72_u32 merge_menu_xy(unsigned x,unsigned y) { return x|(y<<16); }

static void merge_menu_fill(unsigned x,unsigned y,unsigned w,unsigned h,z72_u32 color,z72_u32 origin)
{
    z72_u32 rect[]={merge_menu_xy(x,y),merge_menu_xy(x+w,y+h)},paint[]={2,color};
    CALL(0x1074c3cdu,void,const z72_u32 *,const z72_u32 *,const z72_u32 *,z72_u32)(
        rect,&origin,paint,0);
}

static void merge_menu_thumbnail(const unsigned char *pixels,unsigned x,unsigned y)
{
    z72_u32 image[23];
    CALL(0x10361035u,void,void *,z72_u32)(image,sizeof(image));
    image[1]=1;
    image[2]=(z72_u32)pixels;
    image[5]=merge_menu_xy(MERGE_THUMB_W,MERGE_THUMB_H);
    image[6]=merge_menu_xy(x,y);
    image[7]=merge_menu_xy(x+160,y+120);
    image[16]=360;
    image[19]=MERGE_THUMB_W;
    image[20]=MERGE_THUMB_H;
    CALL(0x10739985u,void,void)();
    CALL(0x105aa2dbu,void,const z72_u32 *)(image);
    CALL(0x105a8b47u,void,void)();
    CALL(0x10739989u,void,void)();
}

static void merge_menu_draw(z72_u32 widget)
{
    CALL(0x10739c3bu,void,z72_u32)(widget);
    if (!WORD(widget+0x44)) return;
    z72_u32 origin=CALL(0x10739c47u,z72_u32,z72_u32)(widget);
    CALL(0x1074c549u,void,const z72_u32 *,const z72_u32 *,z72_u32)(
        (const z72_u32 *)(widget+0x10),&origin,0);
    if (merge_menu_view==MERGE_MENU_PROGRESS) {
        unsigned percent=PIXEL_LOAD(pixel_merge_percent);
        merge_menu_fill(132,203,504,30,0xa8a8a8ffu,origin);
        merge_menu_fill(134,205,500,26,0x262626ffu,origin);
        if (percent) merge_menu_fill(134,205,5*percent,26,0xf4c95affu,origin);
    } else if (PIXEL_LOAD(pixel_state)!=PIXEL_MERGE_SCANNING &&
               PIXEL_LOAD(pixel_state)!=PIXEL_MERGE_STARTING) {
        for (unsigned i=0;i<merge_gallery_count;++i) {
            unsigned x=92+i%FTP_PHOTO_GRID_COLUMNS*200,y=88+i/FTP_PHOTO_GRID_COLUMNS*154;
            merge_menu_fill(x,y,184,126,i==merge_menu_focus ? 0xf4c95affu : 0x656565ffu,origin);
            merge_menu_fill(x+2,y+2,180,122,0x222222ffu,origin);
            if (merge_gallery_groups[i].thumbnail)
                merge_menu_thumbnail(merge_gallery_pixels[i],x+12,y+3);
        }
    }
}

static z72_u32 merge_menu_widget(z72_u32 scene,unsigned id)
{
    return scene ? CALL(0x10635cefu,z72_u32,z72_u32,z72_u32)(id,scene) : 0;
}

static void merge_menu_touch_widget(z72_u32 widget,z72_u32 value) { (void)widget; (void)value; }

static void merge_menu_init(const z72_u32 *page)
{
    merge_menu_seen=~0u;
    if (merge_menu_view==MERGE_MENU_FORMAT) {
        z72_u32 id=MERGE_MENU_ROW;
        if (CALL(0x1063bf77u,z72_u32,void)()!=1)
            CALL(0x10638983u,void,const z72_u32 *,z72_u32)(page,id);
        CALL(0x1063ac0du,void,const z72_u32 *,z72_u32,z72_u32)(page,id,1);
        return;
    }
    z72_u32 canvas=merge_menu_widget(page[3],MERGE_MENU_CANVAS);
    if (!canvas || *(const volatile unsigned short *)(canvas+4)!=6 ||
        WORD(canvas+0x4c)!=0x106eefb3u) return;
    if (merge_menu_view==MERGE_MENU_GROUPS) {
        CALL(0x1062262fu,void,z72_u32,void (*)(z72_u32,z72_u32))(canvas,merge_menu_touch_widget);
        CALL(0x106232efu,void,z72_u32,z72_u32,z72_u32,z72_u32)(canvas,0x05cb02cbu,0,0);
        *(volatile z72_u32 *)(canvas+0xcc)=0;
    }
    *(volatile z72_u32 *)(canvas+0x4c)=(z72_u32)merge_menu_draw;
}

static int merge_menu_show(unsigned view)
{
    volatile z72_u32 *page=(volatile z72_u32 *)ROTATION_PAGE;
    z72_u32 previous=page[10],callbacks=page[4],previous_view=merge_menu_view;
    if (page[0]!=2 || (!merge_menu_header(previous) &&
        previous!=(z72_u32)pixel_menu_views[pixel_menu_view].header)) return 0;
    if (view==MERGE_MENU_GROUPS && previous==(z72_u32)merge_menus[MERGE_MENU_PROGRESS].header)
        pixel_merge_refresh_timers();
    merge_menu_view=view;
    merge_menu_seen=~0u;
    *(volatile unsigned short *)(ROTATION_PAGE+0x16)=0;
    *(volatile unsigned short *)(ROTATION_PAGE+0x18)=0;
    page[4]=(z72_u32)merge_menu_callbacks;
    page[10]=(z72_u32)merge_menus[view].header;
    __asm__ volatile("dmb sy" ::: "memory");
    if (CALL(0x1063a7a7u,z72_u32,z72_u32,z72_u32)(2,0)) {
        PIXEL_STORE(pixel_merge_input_lock,view==MERGE_MENU_PROGRESS);
        CALL(0x10635a8bu,void,z72_u32,z72_u32,z72_u32)(0x106355a1u,3,7);
        return 1;
    }
    page[4]=callbacks;
    page[10]=previous;
    merge_menu_view=previous_view;
    return 0;
}

static void merge_menu_text(z72_u32 scene,unsigned widget,unsigned text,unsigned kind)
{
    z72_u32 object=merge_menu_widget(scene,widget);
    if (object) {
        CALL(0x106361c9u,void,z72_u32,z72_u32,z72_u32)(object,text,kind);
        CALL(0x1063618bu,void,z72_u32,z72_u32)(object,1);
    }
}

static void merge_menu_labels(unsigned count,const char *percentage)
{
    _Static_assert(Z72_PIXEL_LIVE_LABELS==MERGE_GALLERY_TILES+1,"Pixel shift label count");
    z72_u32 lang=CALL(0x1073b8a1u,z72_u32,void)();
    if (lang>=Z72_MENU_LANGUAGES) lang=1;
    z72_u32 *offsets=&z72_pixel_locale->offsets[lang][Z72_TEXT_PIXEL_LIVE];
    z72_u32 offset=offsets[0];
    for (unsigned i=0;i<Z72_PIXEL_LIVE_LABELS;++i) {
        const char *label=i<count ? merge_gallery_groups[i].label :
            i==MERGE_GALLERY_TILES ? percentage : "";
        unsigned short *text=(unsigned short *)(z72_pixel_locale->texts[lang]+offset);
        unsigned n=0;
        while (n<Z72_PIXEL_LIVE_UNITS && label[n]) {
            text[n]=(unsigned char)label[n];
            ++n;
        }
        offset+=n*sizeof(*text);
        offsets[i+1]=offset;
    }
}

static void merge_menu_redraw(z72_u32 canvas)
{
    CALL(0x1063618bu,void,z72_u32,z72_u32)(canvas,1);
    CALL(0x1063618bu,void,z72_u32,z72_u32)(WORD(canvas+0x48),1);
    CALL(0x1061db49u,void,z72_u32)(ROTATION_PAGE);
    CALL(0x10635a8bu,void,z72_u32,z72_u32,z72_u32)(0x106355a1u,3,7);
}

static void merge_menu_update(void)
{
    unsigned view=merge_menu_view;
    if (view==MERGE_MENU_FORMAT || WORD(ROTATION_PAGE+0x28)!=(z72_u32)merge_menus[view].header ||
        CALL(0x1063c2b3u,z72_u32,void)()!=ROTATION_PAGE) {
        PIXEL_STORE(merge_menu_refresh_pending,0);
        if (__atomic_exchange_n(&pixel_merge_input_lock,0,__ATOMIC_ACQ_REL))
            PIXEL_STORE(pixel_merge_cancel,1);
        return;
    }
    unsigned current=PIXEL_LOAD(pixel_state),percent=PIXEL_LOAD(pixel_merge_percent);
    unsigned cancel=PIXEL_LOAD(pixel_merge_cancel),result=PIXEL_LOAD(pixel_merge_result);
    if (view==MERGE_MENU_PROGRESS && cancel && current!=PIXEL_MERGING && current!=PIXEL_MERGE_STARTING) {
        if (!merge_menu_show(MERGE_MENU_GROUPS)) return;
        PIXEL_STORE(pixel_merge_cancel,0);
        view=MERGE_MENU_GROUPS;
    }
    z72_u32 key=view==MERGE_MENU_GROUPS ? (current<<16)|merge_menu_focus :
        (current<<16)|(cancel<<8)|percent;
    if (key==merge_menu_seen || CALL(0x106355a1u,z72_u32,void)()) return;
    z72_u32 scene=WORD(ROTATION_PAGE+0x0c),canvas=merge_menu_widget(scene,MERGE_MENU_CANVAS);
    if (!canvas || WORD(canvas+0x4c)!=(z72_u32)merge_menu_draw) return;
    if (view==MERGE_MENU_PROGRESS && current==PIXEL_IDLE) pixel_merge_refresh_timers();
    unsigned status=Z72_TEXT_PIXEL_MERGE_BLANK;
    if (view==MERGE_MENU_GROUPS) {
        int scanning=current==PIXEL_MERGE_SCANNING || current==PIXEL_MERGE_STARTING;
        if (scanning) status=Z72_TEXT_PIXEL_SCAN;
        else if (!merge_gallery_count) status=Z72_TEXT_PIXEL_NO_GROUPS;
        merge_menu_labels(scanning ? 0 : merge_gallery_count,"");
        for (unsigned i=0;i<MERGE_GALLERY_TILES;++i)
            merge_menu_text(scene,MERGE_MENU_LABEL+i,Z72_TEXT_PIXEL_LIVE+i,1);
        merge_menu_text(scene,MERGE_MENU_PREV,!scanning && merge_gallery_page->previous ?
            Z72_TEXT_PIXEL_MERGE_PREVIOUS : Z72_TEXT_PIXEL_MERGE_BLANK,1);
        merge_menu_text(scene,MERGE_MENU_NEXT,!scanning && merge_gallery_count==MERGE_GALLERY_TILES ?
            Z72_TEXT_PIXEL_MERGE_NEXT : Z72_TEXT_PIXEL_MERGE_BLANK,1);
    } else {
        if (cancel) status=Z72_TEXT_PIXEL_MERGE_CANCELLING;
        else if (current==PIXEL_MERGING || current==PIXEL_MERGE_STARTING)
            status=percent<5 ? Z72_TEXT_PIXEL_MERGE_READ : percent<25 ? Z72_TEXT_PIXEL_MERGE_ALIGN :
                percent<98 ? Z72_TEXT_PIXEL_MERGE_RENDER : Z72_TEXT_PIXEL_MERGE_SAVE;
        else if (result==MERGE_CANCELLED) status=Z72_TEXT_PIXEL_MERGE_CANCELLED;
        else if (result) status=Z72_TEXT_PIXEL_MERGE_FAILED;
        else status=PIXEL_LOAD(pixel_merge_ca)==MERGE_CA_APPLIED ?
            Z72_TEXT_PIXEL_MERGE_SAVED : Z72_TEXT_PIXEL_MERGE_NO_CA;
        char label[5],*p=label;
        if (percent>=100) *p++='1';
        if (percent>=10) *p++=(char)('0'+(percent/10)%10);
        *p++=(char)('0'+percent%10);
        *p++='%';
        *p=0;
        merge_menu_labels(0,label);
        merge_menu_text(scene,MERGE_MENU_PERCENT,Z72_TEXT_PIXEL_LIVE+MERGE_GALLERY_TILES,1);
    }
    merge_menu_text(scene,MERGE_MENU_STATUS,status,1);
    merge_menu_seen=key;
    merge_menu_redraw(canvas);
    if (current!=PIXEL_MERGING && current!=PIXEL_MERGE_SCANNING && current!=PIXEL_MERGE_STARTING)
        PIXEL_STORE(merge_menu_refresh_pending,0);
}

static void merge_menu_wake(void)
{
    if (!PIXEL_LOAD(merge_menu_refresh_pending) && !PIXEL_LOAD(pixel_merge_input_lock)) return;
    unsigned mode=BYTE(0xb1824f0cu);
    if (mode==1 && PIXEL_LOAD(merge_menu_refresh_pending)) {
        /* Bit 2 wakes GuiMnu and dispatches event 3 even with an empty input
         * queue. Painting stays in that task, where native menus draw. */
        z72_u32 event=CALL(0x10634031u,z72_u32,void)();
        CALL(0x10634089u,void,z72_u32,z72_u32)(event,2);
    } else if (mode<1 || mode>3) {
        PIXEL_STORE(merge_menu_refresh_pending,0);
        PIXEL_STORE(pixel_merge_input_lock,0);
        PIXEL_STORE(pixel_merge_cancel,1);
        merge_menu_release_input();
    }
}

static z72_u32 merge_menu_input(z72_u32 event,z72_u32 message,z72_u32 c,z72_u32 d)
{
    if (event==3 && message && !WORD(message) &&
        WORD(ROTATION_PAGE+0x28)==(z72_u32)merge_menus[MERGE_MENU_PROGRESS].header &&
        CALL(0x1063c2b3u,z72_u32,void)()==ROTATION_PAGE) {
        z72_u32 packet[17];
        z72_u32 input=CALL(0x1053c3f3u,z72_u32,z72_u32,z72_u32,z72_u32 *)(0,0,packet);
        /* Only the MENU key and the native touch path for the back icon
         * reach the controller. Drain other physical input normally. */
        if (input!=0xb4 && input!=0x50 && input!=0x1c) {
            CALL(0x1053c285u,z72_u32,z72_u32,z72_u32,z72_u32 *)(message,0,packet);
            CALL(0x10633e3fu,void,z72_u32)(0);
            *(volatile z72_u32 *)message=0;
            merge_menu_update();
            return 0;
        }
    }
    z72_u32 result=CALL(MERGE_MENU_INPUT_NATIVE,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(event,message,c,d);
    if (!merge_menu_header(WORD(ROTATION_PAGE+0x28)) ||
        CALL(0x1063c2b3u,z72_u32,void)()!=ROTATION_PAGE) {
        PIXEL_STORE(merge_menu_refresh_pending,0);
        PIXEL_STORE(pixel_merge_input_lock,0);
        PIXEL_STORE(pixel_merge_cancel,1);
        merge_menu_release_input();
    } else if (PIXEL_LOAD(merge_menu_refresh_pending) || PIXEL_LOAD(pixel_merge_input_lock)) {
        merge_menu_update();
    }
    return result;
}

static void merge_menu_scan_page(int step)
{
    if (!pixel_merge_claim()) return;
    struct merge_gallery_page *target=step<0 ? merge_gallery_page->previous : merge_gallery_page->next;
    if (!target && step>0 && merge_gallery_count==MERGE_GALLERY_TILES) {
        target=CALL(0x1034fe31u,void *,z72_u32,z72_u32)(sizeof(*target),0);
        if (target) {
            *target=(struct merge_gallery_page){merge_gallery_next,merge_gallery_page,0};
            merge_gallery_page->next=target;
        }
    }
    if (!target) {
        PIXEL_STORE(pixel_state,PIXEL_IDLE);
        return;
    }
    merge_gallery_page=target;
    merge_menu_focus=0;
    PIXEL_STORE(pixel_merge_cancel,0);
    PIXEL_STORE(pixel_state,PIXEL_MERGE_SCANNING);
    PIXEL_STORE(merge_menu_refresh_pending,1);
    merge_menu_update();
}

static z72_u32 merge_menu_open(void)
{
    if (!pixel_start_merge_task() || !pixel_merge_claim()) return 1;
    z72_u32 native=MERGE_MENU_INPUT_NATIVE;
    if (!__atomic_compare_exchange_n(&WORD(MERGE_MENU_INPUT_SLOT),&native,(z72_u32)merge_menu_input,
                                     0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
        PIXEL_STORE(pixel_state,PIXEL_IDLE);
        return 1;
    }
    merge_gallery_reset();
    merge_gallery_count=merge_menu_focus=0;
    merge_menu_labels(0,"");
    PIXEL_STORE(pixel_merge_cancel,0);
    if (!merge_menu_show(MERGE_MENU_GROUPS)) {
        merge_menu_release_input();
        PIXEL_STORE(pixel_state,PIXEL_IDLE);
        return 1;
    }
    PIXEL_STORE(pixel_state,PIXEL_MERGE_SCANNING);
    PIXEL_STORE(merge_menu_refresh_pending,1);
    merge_menu_update();
    return 0;
}

static void merge_menu_move(int shift)
{
    if (!merge_menu_active(MERGE_MENU_GROUPS) || PIXEL_LOAD(pixel_state)==PIXEL_MERGE_SCANNING) return;
    int target=(int)merge_menu_focus+shift;
    if (target<0 && shift==-1) merge_menu_scan_page(-1);
    else if (target>=(int)merge_gallery_count && shift==1) merge_menu_scan_page(1);
    else if (target>=0 && target<(int)merge_gallery_count) {
        merge_menu_focus=(unsigned)target;
        merge_menu_update();
    }
}

static z72_u32 merge_menu_choose_group(void)
{
    if (!merge_menu_active(MERGE_MENU_GROUPS) || PIXEL_LOAD(pixel_state)==PIXEL_MERGE_SCANNING ||
        merge_menu_focus>=merge_gallery_count) return 1;
    pixel_merge_selection=merge_menu_focus;
    merge_menus[MERGE_MENU_FORMAT].rows[1][0]=merge_gallery_groups[merge_menu_focus].packed ?
        Z72_TEXT_PIXEL_NEFX : Z72_TEXT_PIXEL_NEFX_UNAVAILABLE;
    return !merge_menu_show(MERGE_MENU_FORMAT);
}

static void merge_menu_touch(void)
{
    z72_u32 touch=0,widget=0;
    CALL(0x10623521u,void,z72_u32 *,z72_u32 *)(&touch,&widget);
    if (PIXEL_LOAD(pixel_state)==PIXEL_MERGE_SCANNING || !touch || BYTE(touch+8)!=1 ||
        widget!=merge_menu_widget(WORD(ROTATION_PAGE+0x0c),MERGE_MENU_CANVAS)) return;
    unsigned x=*(const volatile unsigned short *)(touch+0x12),y=*(const volatile unsigned short *)(touch+0x14);
    if (y>=427) {
        if (x<230) merge_menu_scan_page(-1);
        else if (x>=520) merge_menu_scan_page(1);
        return;
    }
    if (x<92 || x>=676 || y<88 || y>=368) return;
    unsigned i=(y-88)/154*FTP_PHOTO_GRID_COLUMNS+(x-92)/200;
    if ((x-92)%200>=184 || (y-88)%154>=150 || i>=merge_gallery_count) return;
    merge_menu_focus=i;
    merge_menu_choose_group();
}

static z72_u32 merge_menu_start(unsigned format)
{
    if (!merge_menu_active(MERGE_MENU_FORMAT) ||
        (format==MERGE_NEFX && !merge_gallery_groups[pixel_merge_selection].packed) ||
        !pixel_merge_claim()) return 1;
    PIXEL_STORE(pixel_merge_percent,0);
    merge_menu_labels(0,"0%");
    PIXEL_STORE(pixel_merge_result,MERGE_MENU_PENDING);
    PIXEL_STORE(pixel_merge_cancel,0);
    pixel_merge_format=format;
    if (!merge_menu_show(MERGE_MENU_PROGRESS)) {
        PIXEL_STORE(pixel_state,PIXEL_IDLE);
        return 1;
    }
    PIXEL_STORE(pixel_state,PIXEL_MERGING);
    PIXEL_STORE(merge_menu_refresh_pending,1);
    merge_menu_update();
    return 0;
}

static z72_u32 merge_menu_return(void)
{
    if (merge_menu_view==MERGE_MENU_GROUPS) {
        PIXEL_STORE(pixel_merge_cancel,1);
        PIXEL_STORE(merge_menu_refresh_pending,0);
        z72_u32 result=pixel_menu_show(PIXEL_ROOT_VIEW);
        if (!result) merge_menu_release_input();
        return result;
    }
    if (merge_menu_view==MERGE_MENU_PROGRESS && PIXEL_LOAD(pixel_state)==PIXEL_MERGING) {
        PIXEL_STORE(pixel_merge_cancel,1);
        merge_menu_update();
        return 0;
    }
    PIXEL_STORE(pixel_merge_cancel,0);
    return !merge_menu_show(MERGE_MENU_GROUPS);
}

#define MERGE_ACTION(name,action) \
    static z72_u32 name(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d) \
    { (void)a; (void)b; (void)c; (void)d; action; }
MERGE_ACTION(merge_menu_left,merge_menu_move(-1); return 0)
MERGE_ACTION(merge_menu_right,merge_menu_move(1); return 0)
MERGE_ACTION(merge_menu_up,merge_menu_move(-3); return 0)
MERGE_ACTION(merge_menu_down,merge_menu_move(3); return 0)
MERGE_ACTION(merge_menu_pick,return merge_menu_choose_group())
MERGE_ACTION(merge_menu_tap,merge_menu_touch(); return 0)
MERGE_ACTION(merge_menu_tif,return merge_menu_start(MERGE_TIFF))
MERGE_ACTION(merge_menu_nefx,return merge_menu_start(MERGE_NEFX))
MERGE_ACTION(merge_menu_back,return merge_menu_return())
#undef MERGE_ACTION

static void merge_menu_title(z72_u32 *widget,unsigned id,unsigned text,unsigned type,
                             z72_u32 begin,z72_u32 end)
{
    __aeabi_memcpy4(widget,(const void *)FTP_PHOTO_GRID_WIDGETS,8*sizeof(*widget));
    widget[0]=text;
    widget[1]=type;
    widget[4]=begin;
    widget[5]=end;
    widget[7]=id<<16;
}

static int merge_menu_prepare(void)
{
    const z72_u32 *modal=(const z72_u32 *)0xb1853fb4u,*choice=(const z72_u32 *)0xb18545fcu;
    if (WORD(MERGE_MENU_INPUT_SLOT)!=MERGE_MENU_INPUT_NATIVE ||
        WORD(PIXEL_REFRESH_DISPLAY_TIMER&~1u)!=0x4604b510u ||
        WORD(PIXEL_REFRESH_STANDBY_TIMER&~1u)!=0x22004931u ||
        modal[5]!=0x10001 || !modal[1] || !modal[3] || !choice[1]) return 0;
    const z72_u32 *back=(const z72_u32 *)(FTP_PHOTO_PREVIEW_WIDGETS+6*32);
    static const z72_u32 gallery_events[][4]={
        {0x24f,(z72_u32)merge_menu_back,0,0x430},
        {0x252,(z72_u32)merge_menu_back,0,0x430},
        {0x254,(z72_u32)merge_menu_up,0,0x430}, {0x255,(z72_u32)merge_menu_down,0,0x430},
        {0x256,(z72_u32)merge_menu_left,0,0x430}, {0x257,(z72_u32)merge_menu_right,0,0x430},
        {0x25c,(z72_u32)merge_menu_pick,0,0x430}, {0x26e,(z72_u32)merge_menu_up,0,0x430},
        {0x26f,(z72_u32)merge_menu_down,0,0x430}, {0x270,(z72_u32)merge_menu_right,0,0x430},
        {0x271,(z72_u32)merge_menu_left,0,0x430}, {0x27d,(z72_u32)merge_menu_pick,0,0x430},
        {0x2cb,(z72_u32)merge_menu_tap,0,0x430}, {0x2cf,(z72_u32)merge_menu_back,0,0x430},
        {0x31a,0,0,0x430}
    };
    static const z72_u32 progress_events[][4]={
        {0x24f,(z72_u32)merge_menu_back,0,0x430},
        {0x2cf,(z72_u32)merge_menu_back,0,0x430}, {0x31a,0,0,0x430}
    };
    for (unsigned view=0;view<MERGE_MENU_PAGES;++view) {
        int format=view==MERGE_MENU_FORMAT;
        const z72_u32 *source=format ? choice : modal;
        z72_u32 *header=merge_menus[view].header;
        z72_u32 (*title)[8]=merge_menus[view].title;
        __aeabi_memcpy4(header,source,6*sizeof(*header));
        header[2]=(z72_u32)title;
        header[3]=(z72_u32)merge_menus[view].rows;
        header[4]=(z72_u32)merge_menus[view].maps;
        for (unsigned row=0;row<(format ? 2u : 1u);++row) {
            z72_u32 *item=merge_menus[view].rows[row];
            __aeabi_memcpy4(item,(const void *)source[3],7*sizeof(*item));
            item[6]=MERGE_MENU_ROW+row;
            merge_menus[view].maps[row]=(z72_u32)merge_menus[view].events[row];
            if (!format) continue;
            item[0]=row ? Z72_TEXT_PIXEL_NEFX : Z72_TEXT_PIXEL_TIF;
            item[1]=item[2]=item[5]=0xffff;
            item[3]=0;
            item[4]=0x1000;
            z72_u32 (*events)[4]=merge_menus[view].events[row];
            __aeabi_memcpy4(events,(const void *)WORD(choice[4]),PIXEL_MENU_EVENTS*16u);
            for (unsigned i=0;i<PIXEL_MENU_EVENTS;++i) {
                unsigned event=events[i][0];
                if (event==0x257 || event==0x25c || event==0x27d || event==0x270) {
                    events[i][1]=(z72_u32)(row ? merge_menu_nefx : merge_menu_tif);
                    events[i][2]=0; events[i][3]=0x430;
                } else if (event==0x24f || event==0x252 || event==0x256 ||
                           event==0x271 || event==0x27f || event==0x2cf) {
                    events[i][1]=(z72_u32)merge_menu_back;
                    events[i][2]=0; events[i][3]=0x430;
                }
            }
        }
        if (format) {
            __aeabi_memcpy4(title,(const void *)choice[2],24*sizeof(z72_u32));
            title[0][0]=Z72_TEXT_PIXEL_OUTPUT;
            continue;
        }
        merge_menu_title(title[0],0x0a25,Z72_TEXT_PIXEL_MERGE,1,merge_menu_xy(78,8),merge_menu_xy(625,48));
        title[1][1]=6;
        title[1][4]=view==MERGE_MENU_GROUPS ? merge_menu_xy(72,78) : merge_menu_xy(72,190);
        title[1][5]=view==MERGE_MENU_GROUPS ? merge_menu_xy(694,475) : merge_menu_xy(694,249);
        title[1][6]=1;
        title[1][7]=(MERGE_MENU_CANVAS<<16)|1;
        unsigned index=2;
        if (view==MERGE_MENU_GROUPS) {
            __aeabi_memcpy4(merge_menus[view].events[0],gallery_events,sizeof(gallery_events));
            for (unsigned i=0;i<MERGE_GALLERY_TILES;++i) {
                unsigned x=92+i%FTP_PHOTO_GRID_COLUMNS*200,y=214+i/FTP_PHOTO_GRID_COLUMNS*154;
                merge_menu_title(title[index++],MERGE_MENU_LABEL+i,Z72_TEXT_PIXEL_LIVE+i,
                                 1,merge_menu_xy(x+4,y),merge_menu_xy(x+184,y+27));
            }
            merge_menu_title(title[index++],MERGE_MENU_STATUS,Z72_TEXT_PIXEL_SCAN,1,
                             merge_menu_xy(120,396),merge_menu_xy(650,426));
            merge_menu_title(title[index++],MERGE_MENU_PREV,Z72_TEXT_PIXEL_MERGE_BLANK,1,
                             merge_menu_xy(84,427),merge_menu_xy(242,471));
            merge_menu_title(title[index++],MERGE_MENU_NEXT,Z72_TEXT_PIXEL_MERGE_BLANK,1,
                             merge_menu_xy(528,427),merge_menu_xy(689,471));
        } else {
            __aeabi_memcpy4(merge_menus[view].events[0],progress_events,sizeof(progress_events));
            merge_menu_title(title[index++],MERGE_MENU_STATUS,Z72_TEXT_PIXEL_MERGE_READ,1,
                             merge_menu_xy(132,138),merge_menu_xy(639,182));
            merge_menu_title(title[index++],MERGE_MENU_PERCENT,Z72_TEXT_PIXEL_LIVE+MERGE_GALLERY_TILES,1,
                             merge_menu_xy(132,255),merge_menu_xy(639,299));
        }
        __aeabi_memcpy4(title[index++],back,8*sizeof(z72_u32));
        title[index][6]=title[index][7]=1;
    }
    __aeabi_memcpy4(merge_menu_callbacks,(const void *)0x10969c14u,sizeof(merge_menu_callbacks));
    merge_menu_callbacks[0]=(z72_u32)merge_menu_init;
    return 1;
}
