/* SPDX-License-Identifier: MIT */
/* Use the original numeric date widgets with an independent draft value. */
static z72_u32 date_widget(const z72_u32 *page,z72_u32 id)
{
    return NATIVE(0x10635cefu,z72_u32,z72_u32,z72_u32)(id,page[3]);
}

static z72_u32 date_number_widget(const z72_u32 *page,z72_u32 field)
{
    static const unsigned short ids[]={0xa82,0xa84,0xa86};
    return date_widget(page,ids[field]);
}

static void draw_date_field(z72_u32 widget,z72_u32 field,z72_u32 index)
{
    if (!widget) return;
    NATIVE(0x106fd321u,void,z72_u32,const struct playback_date *,z72_u32,z72_u32,z72_u32)(
        widget,&draft_date,field,date_field,index);
}

static void date_page_init(const z72_u32 *page)
{
    const z72_u32 separators[][2]={{0xa81,0x1209ce78u},{0xa83,0x1209e5e8u},{0xa85,0x1209e5e8u}};
    z72_u32 index=0;
    for (z72_u32 i=0;i<COUNT(separators);++i) {
        z72_u32 widget=date_widget(page,separators[i][0]);
        if (!widget) continue;
        NATIVE(0x10636a07u,void,z72_u32,z72_u32,z72_u32)(separators[i][1],index,2);
        NATIVE(0x106361cdu,void,z72_u32,z72_u32)(widget,index++);
        NATIVE(0x106361b7u,void,z72_u32,z72_u32)(widget,1);
    }
    for (z72_u32 field=0;field<3;++field) {
        z72_u32 widget=date_number_widget(page,field);
        draw_date_field(widget,field,index++);
        if (widget) WORD(widget+0x88)=0x009202cbu;
    }
    z72_u32 widget=date_number_widget(page,date_field);
    if (widget) {
        NATIVE(0x10635cf3u,void,z72_u32,z72_u32)(page[3],widget);
        NATIVE(0x10635cebu,void,z72_u32,z72_u32)(page[3],widget);
    }
}

static void focus_date(const z72_u32 *page,z72_u32 field)
{
    z72_u32 previous=date_field;
    date_field=field;
    z72_u32 widget=date_number_widget(page,previous);
    if (widget) draw_date_field(widget,previous,WORD(widget+8));
    widget=date_number_widget(page,field);
    if (widget) {
        draw_date_field(widget,field,WORD(widget+8));
        NATIVE(0x10635cf3u,void,z72_u32,z72_u32)(page[3],widget);
        NATIVE(0x10635cebu,void,z72_u32,z72_u32)(page[3],widget);
    }
    redraw_page(page);
}

static void step_date(z72_u32 advance)
{
    const z72_u32 *page=(const z72_u32 *)choice_address();
    int step=advance ? 1 : -1;
    struct playback_date previous=draft_date;
    if (date_field==0) {
        int year=draft_date.year+step;
        draft_date.year=year<2000 ? 2099 : year>2099 ? 2000 : year;
    } else if (date_field==1) {
        int month=draft_date.month+step;
        draft_date.month=month<1 ? 12 : month>12 ? 1 : month;
    }
    z72_u32 days=z72_filter_days_in_month(draft_date.year,draft_date.month);
    if (date_field==2) {
        int day=draft_date.day+step;
        draft_date.day=day<1 ? days : (z72_u32)day>days ? 1u : (z72_u32)day;
    } else if (draft_date.day>days) {
        draft_date.day=days;
    }
    z72_u32 widget=date_number_widget(page,date_field);
    if (widget) draw_date_field(widget,date_field,WORD(widget+8));
    if (date_field!=2 && previous.day!=draft_date.day) {
        widget=date_number_widget(page,2);
        if (widget) draw_date_field(widget,2,WORD(widget+8));
    }
    redraw_page(page);
}

static void date_up(void) { step_date(1); }
static void date_down(void) { step_date(0); }
static void date_left(void) { focus_date((const z72_u32 *)choice_address(),(date_field+2)%3); }
static void date_right(void) { focus_date((const z72_u32 *)choice_address(),(date_field+1)%3); }

static void date_touch(void)
{
    z72_u32 details[4]={0},widget=0;
    NATIVE(0x10623521u,void,z72_u32 *,z72_u32 *)(details,&widget);
    if (!widget) return;
    const z72_u32 *page=(const z72_u32 *)choice_address();
    for (z72_u32 field=0;field<3;++field) {
        if (widget==date_number_widget(page,field)) {
            focus_date(page,field);
            return;
        }
    }
}

static void apply_date(void)
{
    z72_u32 packed=((draft_date.year-1980u)<<9)|(draft_date.month<<5)|draft_date.day;
    save_settings((settings()&((1u<<Z72_DATE_SHIFT)-1))|(packed<<Z72_DATE_SHIFT));
    show_page(CONDITIONS,3);
}

static void prepare_date_page(void)
{
    static const unsigned char entries[]={0,1,2,3,4,8,9,10,11,12,13,19,20,21};
    for (z72_u32 i=0;i<COUNT(entries);++i)
        copy_bytes(filter_menu.date_widgets[i],(const void *)(0x117c4768u+entries[i]*32),32);
    filter_menu.date_widgets[0][0]=TEXT_DATE;
    filter_menu.date_widgets[1][0]=0x159; /* Select date. */
    const z72_u32 *stock=(const z72_u32 *)DATE_HEADER;
    struct filter_page *editor=&PAGE(DATE_EDITOR);
    copy_bytes(editor->header,stock,sizeof(editor->header));
    editor->header[2]=(z72_u32)filter_menu.date_widgets;
    editor->header[4]=(z72_u32)editor->maps;
    editor->maps[0]=(z72_u32)filter_menu.date_events;
    copy_bytes(filter_menu.date_events,(const void *)WORD(stock[4]),18*16);
    copy_bytes(filter_menu.date_events[18],filter_menu.date_events[17],16);
    filter_menu.date_events[17][0]=0x27f;
    for (z72_u32 i=0;i<DATE_EVENTS;++i) {
        z72_u32 *event=filter_menu.date_events[i];
        z72_u32 action=0;
        switch (event[0]) {
        case 0x254: case 0x26f: action=(z72_u32)date_up; break;
        case 0x255: case 0x26e: action=(z72_u32)date_down; break;
        case 0x256: case 0x271: action=(z72_u32)date_left; break;
        case 0x257: case 0x270: action=(z72_u32)date_right; break;
        case 0x25c: case 0x27d: action=(z72_u32)apply_date; break;
        case 0x2cb: action=(z72_u32)date_touch; break;
        case 0x252: case 0x2cf: case 0x27f: action=(z72_u32)back_to_conditions; break;
        }
        if (action) {
            event[1]=action;
            if (event[0]==0x252 || event[0]==0x2cf || event[0]==0x27f) event[2]=0;
            event[3]=0x430;
        }
    }
    copy_bytes(filter_menu.i_headers[DATE_EDITOR],editor->header,sizeof(editor->header));
}
