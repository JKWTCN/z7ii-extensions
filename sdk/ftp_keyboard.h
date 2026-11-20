/* SPDX-License-Identifier: MIT */
#ifndef Z72_FTP_KEYBOARD_H
#define Z72_FTP_KEYBOARD_H

/* C 1.70 text widgets. FTP owns the layout, input and events. */
enum {
    FTP_KEY_COLUMNS=10, FTP_KEY_LETTERS=40, FTP_KEY_BANK=FTP_KEY_LETTERS,
    FTP_KEY_LEFT, FTP_KEY_RIGHT, FTP_KEY_DELETE, FTP_KEY_DONE, FTP_KEY_CANCEL, FTP_KEY_COUNT,
    FTP_KEY_SLOT=41, FTP_KEY_PREVIEW=FTP_KEY_SLOT+FTP_KEY_COUNT,
    FTP_KEY_EVENTS=20
};
static struct {
    u32 header[6], title[4][8], maps[1], events[FTP_KEY_EVENTS][4];
    u32 labels[FTP_KEY_COUNT], preview, touch_area;
    u32 field, length, cursor, bank, selection;
    char text[sizeof(ftp_draft.password)];
} ftp_keyboard;

static const char ftp_key_layouts[][FTP_KEY_LETTERS+1]={
    "1234567890" "qwertyuiop" "asdfghjkl " "zxcvbnm._-",
    "1234567890" "QWERTYUIOP" "ASDFGHJKL " "ZXCVBNM._-",
    " !\"#$%&'()" "*+,-./:;<=" ">?@[\\]^_\x60{" "|}~0123456",
    "1234567890.:"
};

static u32 ftp_show(u32 view,u32 row);

static char *ftp_field(u32 field)
{
    if (!field) return ftp_draft.address;
    if (field==1) return ftp_draft.user;
    return field==2 ? ftp_draft.password : ftp_draft.folder;
}

static u32 ftp_key_length(void)
{
    return ftp_keyboard.field ? FTP_KEY_LETTERS : 12u;
}

static int ftp_key_visible(u32 key)
{
    return key<ftp_key_length() || (key>=FTP_KEY_BANK && (key!=FTP_KEY_BANK || ftp_keyboard.field));
}

static const char *ftp_key_layout(void)
{
    return ftp_key_layouts[ftp_keyboard.field ? ftp_keyboard.bank : 3];
}

static void ftp_key_text(u32 widget,u32 slot,const char *text)
{
    if (!widget) return;
    CALL(0x10636a07u,void,const char *,u32,u32)(text,slot,2);
    CALL(0x106361cdu,void,u32,u32)(widget,slot);
}

static void ftp_key_update_layout(void)
{
    const char *keys=ftp_key_layout();
    for (u32 i=0;i<ftp_key_length();++i) {
        char label[3]={keys[i],0,0};
        if (keys[i]==' ') { label[0]='S'; label[1]='P'; }
        ftp_key_text(ftp_keyboard.labels[i],FTP_KEY_SLOT+i,label);
    }
    static const char *const next[]={"ABC","#+=","abc"};
    ftp_key_text(ftp_keyboard.labels[FTP_KEY_BANK],FTP_KEY_SLOT+FTP_KEY_BANK,next[ftp_keyboard.bank]);
}

static void ftp_key_preview(void)
{
    enum { WIDTH=22 };
    char label[WIDTH+4],*out=label;
    u32 start=ftp_keyboard.cursor>WIDTH/2 ? ftp_keyboard.cursor-WIDTH/2 : 0;
    u32 last=ftp_keyboard.length>WIDTH ? ftp_keyboard.length-WIDTH : 0;
    if (start>last) start=last;
    if (start) *out++='<';
    for (u32 i=start;i<=ftp_keyboard.length;++i) {
        if (i==ftp_keyboard.cursor) *out++='|';
        if (i==ftp_keyboard.length) break;
        if (i==start+WIDTH) { *out++='>'; break; }
        *out++=ftp_keyboard.field==2 ? '*' : ftp_keyboard.text[i];
    }
    *out=0;
    ftp_key_text(ftp_keyboard.preview,FTP_KEY_PREVIEW,label);
}

static void ftp_key_highlight(u32 key,u32 selected)
{
    u32 widget=ftp_keyboard.labels[key];
    if (!widget) return;
    WORD(widget+0x38)=selected ? 1u : 0u;
    WORD(widget+0x40)=selected ? 0x248u : 0x2a0u;
    CALL(0x1063618bu,void,u32,u32)(widget,1);
}

static void ftp_key_redraw(void)
{
    const u32 *page=(const u32 *)ROTATION_PAGE;
    CALL(0x1061db49u,void,const u32 *)(page);
    CALL(0x10635a8bu,void,u32,u32,u32)(0x106355a1u,3,7);
}

static void ftp_key_hit_noop(u32 widget,u32 state)
{
    (void)widget;
    (void)state;
}

static u32 ftp_key_widget(const u32 *page,u32 *index,u32 type,u32 slot,u32 xy,u32 end)
{
    u32 pool=page[9];
    if (*index>=(unsigned short)HALF(pool+4)) return 0;
    u32 widget=CALL(0x10634623u,u32,u32,u32,u32)(pool,0,(*index)++);
    if (!widget) return 0;
    WORD(widget+4)=0;
    WORD(widget+8)=slot;
    WORD(widget+0xc)=type;
    WORD(widget+0x10)=xy;
    WORD(widget+0x14)=end;
    WORD(widget+0x18)=0;
    WORD(widget+0x34)=WORD(widget+0x38)=WORD(widget+0x3c)=0;
    WORD(widget+0x40)=0x2a0;
    if (type==6) {
        CALL(0x1063639du,void,u32,u32)(widget,page[3]);
        /* Native touch dispatch calls this hook on both press and release. */
        CALL(0x1062262fu,void,u32,void (*)(u32,u32))(widget,ftp_key_hit_noop);
        CALL(0x106232efu,void,u32,u32,u32,u32)(widget,0x05cb02cbu,0,0);
        WORD(widget+0xcc)=0;
        WORD(widget+0x4c)=(u32)ftp_key_hit_noop;
    } else CALL(0x10636369u,void,u32)(widget);
    WORD(widget)=0;
    WORD(widget+0x48)=page[3];
    CALL(0x106361a5u,void,u32,u32)(widget,0);
    CALL(0x106361b7u,void,u32,u32)(widget,1);
    CALL(0x10636193u,void,u32,u32)(widget,0);
    CALL(0x10635cebu,void,u32,u32)(page[3],widget);
    return widget;
}

static void ftp_keyboard_init(const u32 *page)
{
    static const u32 footer[]={0,0,0,0x158,0x400,0x16a};
    ftp_keyboard.touch_area=0;
    ftp_keyboard.preview=0;
    for (u32 key=0;key<FTP_KEY_COUNT;++key)
        ftp_keyboard.labels[key]=0;
    if (!page[3] || !page[9]) return;
    u32 index=CALL(0x106345fbu,u32,u32,u32)(page[9],0);
    ftp_keyboard.preview=ftp_key_widget(page,&index,2,FTP_KEY_PREVIEW,72|(60u<<16),686|(103u<<16));
    for (u32 key=0;key<FTP_KEY_COUNT;++key) {
        if (!ftp_key_visible(key)) continue;
        u32 is_footer=key>=FTP_KEY_BANK, column=is_footer ? key-FTP_KEY_BANK : key%FTP_KEY_COLUMNS;
        u32 width=is_footer ? 96u : 56u, x=72u+column*(width+6u);
        u32 y=is_footer ? 392u : 126u+key/FTP_KEY_COLUMNS*61u;
        u32 end=(x+width)|((y+47u)<<16);
        u32 resource=is_footer ? footer[column] : 0;
        ftp_keyboard.labels[key]=ftp_key_widget(page,&index,resource ? 1u : 2u,
            resource ? resource : FTP_KEY_SLOT+key,x|(y<<16),end);
        ftp_key_highlight(key,key==ftp_keyboard.selection);
    }
    ftp_keyboard.touch_area=ftp_key_widget(page,&index,6,0,72|(126u<<16),686|(439u<<16));
    ftp_key_update_layout();
    ftp_key_text(ftp_keyboard.labels[FTP_KEY_LEFT],FTP_KEY_SLOT+FTP_KEY_LEFT,"<");
    ftp_key_text(ftp_keyboard.labels[FTP_KEY_RIGHT],FTP_KEY_SLOT+FTP_KEY_RIGHT,">");
    ftp_key_preview();
}

static int ftp_key_focus(u32 key)
{
    if (ftp_keyboard.selection==key) return 0;
    ftp_key_highlight(ftp_keyboard.selection,0);
    ftp_keyboard.selection=key;
    ftp_key_highlight(key,1);
    return 1;
}

static void ftp_key_select(u32 key)
{
    if (ftp_key_focus(key)) ftp_key_redraw();
}

static void ftp_key_horizontal(int delta)
{
    u32 key=ftp_keyboard.selection, count=ftp_key_length();
    u32 first=key<FTP_KEY_BANK ? key-key%FTP_KEY_COLUMNS : FTP_KEY_BANK+(!ftp_keyboard.field);
    u32 end=key<FTP_KEY_BANK ? first+FTP_KEY_COLUMNS : FTP_KEY_COUNT;
    if (key<FTP_KEY_BANK && count<end) end=count;
    int next=(int)key+delta;
    ftp_key_select(next<(int)first ? end-1 : (u32)next>=end ? first : (u32)next);
}

static void ftp_key_vertical(int delta)
{
    u32 key=ftp_keyboard.selection,count=ftp_key_length();
    u32 row=key<FTP_KEY_BANK ? key/FTP_KEY_COLUMNS : (count+FTP_KEY_COLUMNS-1u)/FTP_KEY_COLUMNS;
    u32 footer=(count+FTP_KEY_COLUMNS-1u)/FTP_KEY_COLUMNS;
    u32 column=key<FTP_KEY_BANK ? key%FTP_KEY_COLUMNS : (key-FTP_KEY_BANK)*2;
    int next=(int)row+delta;
    if (next<0) next=(int)footer;
    else if ((u32)next>footer) next=0;
    if ((u32)next==footer) {
        u32 control=FTP_KEY_BANK+column/2;
        ftp_key_select(ftp_key_visible(control) ? control : FTP_KEY_LEFT);
    } else {
        u32 cell=(u32)next*FTP_KEY_COLUMNS+column;
        ftp_key_select(cell<count ? cell : count-1u);
    }
}

static void ftp_key_up(void) { ftp_key_vertical(-1); }
static void ftp_key_down(void) { ftp_key_vertical(1); }
static void ftp_key_left(void) { ftp_key_horizontal(-1); }
static void ftp_key_right(void) { ftp_key_horizontal(1); }

static void ftp_key_erase(void)
{
    if (ftp_keyboard.cursor) {
        --ftp_keyboard.cursor;
        for (u32 i=ftp_keyboard.cursor;i<ftp_keyboard.length;++i)
            ftp_keyboard.text[i]=ftp_keyboard.text[i+1];
        --ftp_keyboard.length;
        ftp_key_preview();
    }
    ftp_key_redraw();
}

static void ftp_key_leave(u32 apply)
{
    u32 field=ftp_keyboard.field;
    if (apply) {
        ftp_copy_field(ftp_field(field),ftp_keyboard.text);
        ftp_edit_status=FTP_EDIT_READY;
    }
    for (u32 i=0;i<sizeof(ftp_keyboard.text);++i) ((volatile char *)ftp_keyboard.text)[i]=0;
    ftp_keyboard.length=ftp_keyboard.cursor=0;
    ftp_show(FTP_MENU_EDIT,field);
}

static void ftp_key_cancel(void) { ftp_key_leave(0); }

static void ftp_key_choose(void)
{
    u32 key=ftp_keyboard.selection;
    if (!ftp_keyboard.labels[key]) return;
    if (key==FTP_KEY_CANCEL || key==FTP_KEY_DONE) {
        ftp_key_leave(key==FTP_KEY_DONE);
        return;
    }
    if (key==FTP_KEY_DELETE) { ftp_key_erase(); return; }
    if (key==FTP_KEY_BANK) {
        ftp_keyboard.bank=(ftp_keyboard.bank+1)%3;
        ftp_key_update_layout();
    } else if (key==FTP_KEY_LEFT) {
        if (ftp_keyboard.cursor) --ftp_keyboard.cursor;
    } else if (key==FTP_KEY_RIGHT) {
        if (ftp_keyboard.cursor<ftp_keyboard.length) ++ftp_keyboard.cursor;
    } else {
        u32 limit=ftp_keyboard.field ? sizeof(ftp_keyboard.text) : sizeof(ftp_draft.address);
        if (ftp_keyboard.length+1<limit) {
            for (u32 i=ftp_keyboard.length+1;i>ftp_keyboard.cursor;--i)
                ftp_keyboard.text[i]=ftp_keyboard.text[i-1];
            ftp_keyboard.text[ftp_keyboard.cursor++]=ftp_key_layout()[key];
            ++ftp_keyboard.length;
        }
    }
    ftp_key_preview();
    ftp_key_redraw();
}

static void ftp_key_touch(void)
{
    u32 touch=0, widget=0;
    CALL(0x10623521u,void,u32 *,u32 *)(&touch,&widget);
    if (!touch || !widget || widget!=ftp_keyboard.touch_area ||
        *(const unsigned char *)(touch+8)!=1) return;
    u32 x=(unsigned short)HALF(touch+0x12), y=(unsigned short)HALF(touch+0x14);
    for (u32 key=0;key<FTP_KEY_COUNT;++key) {
        u32 label=ftp_keyboard.labels[key];
        if (label && x>=(unsigned short)HALF(label+0x10) && x<(unsigned short)HALF(label+0x14) &&
            y>=(unsigned short)HALF(label+0x12) && y<(unsigned short)HALF(label+0x16)) {
            ftp_key_focus(key);
            ftp_key_choose();
            return;
        }
    }
}

static u32 ftp_keyboard_open(u32 field)
{
    static const u32 titles[]={0x417,0x42c,0x428,0x423};
    for (u32 i=FTP_KEY_SLOT;i<=FTP_KEY_PREVIEW;++i)
        if (!WORD(0xb20287b4u+i*12+4)) return 1;
    ftp_keyboard.title[0][0]=titles[field];
    ftp_keyboard.field=field;
    ftp_keyboard.length=ftp_keyboard.bank=ftp_keyboard.selection=0;
    ftp_copy_field(ftp_keyboard.text,ftp_field(field));
    while (ftp_keyboard.text[ftp_keyboard.length]) ++ftp_keyboard.length;
    ftp_keyboard.cursor=ftp_keyboard.length;
    return ftp_show(FTP_MENU_INPUT,0);
}

static void ftp_keyboard_prepare(void)
{
    const u32 *header=(const u32 *)0xb1853fb4u; /* Native date editor page type. */
    const u32 *widgets[]={(const u32 *)0x117c4768u,(const u32 *)0x117cbf68u,
                         (const u32 *)0x117c49e8u,(const u32 *)0x117c4a08u};
    static const struct { u32 code; void (*action)(void); } bindings[]={
        {0x24f,ftp_key_cancel}, {0x254,ftp_key_up}, {0x255,ftp_key_down},
        {0x256,ftp_key_left}, {0x257,ftp_key_right}, {0x25c,ftp_key_choose},
        {0x26e,ftp_key_down}, {0x26f,ftp_key_up}, {0x270,ftp_key_right}, {0x271,ftp_key_left},
        {0x27d,ftp_key_choose}, {0x27f,ftp_key_cancel}, {0x280,ftp_key_erase},
        {0x2cb,ftp_key_touch}, {0x2cf,ftp_key_cancel}
    };
    for (u32 i=0;i<6;++i) ftp_keyboard.header[i]=header[i];
    for (u32 i=0;i<4;++i)
        for (u32 j=0;j<8;++j) ftp_keyboard.title[i][j]=widgets[i][j];
    ftp_keyboard.header[2]=(u32)ftp_keyboard.title;
    ftp_keyboard.header[4]=(u32)ftp_keyboard.maps;
    ftp_keyboard.maps[0]=(u32)ftp_keyboard.events;
    for (u32 i=0;i<sizeof(bindings)/sizeof(bindings[0]);++i) {
        ftp_keyboard.events[i][0]=bindings[i].code;
        ftp_keyboard.events[i][1]=(u32)bindings[i].action;
        ftp_keyboard.events[i][3]=0x430;
    }
    u32 end=sizeof(bindings)/sizeof(bindings[0]);
    ftp_keyboard.events[end][0]=0x31a;
    ftp_keyboard.events[end][3]=0x430;
}
#endif
