/* SPDX-License-Identifier: MIT */
#include "z72.h"
#include "config.h"
#define Z72_MENU_SLOT 255u /* Grid labels use slots 250 through 254. */
#include "menu.h"
#include "menu_locale.h"

#define Z72_PORTRAIT_MENU 1
#ifndef Z72_PORTRAIT_TRACE
#define Z72_PORTRAIT_TRACE 0
#endif
static z72_u32 portrait_draw(volatile z72_u32 *message,z72_u32 outputs,z72_u32 orientation);
static z72_u32 portrait_ready(void);
static z72_u32 portrait_install_menu(void);
static void portrait_install(void);
#ifdef Z72_PHOTO_ZEBRA
static void zebra_graphics_observe(void);
#endif
#if Z72_PORTRAIT_TRACE
static z72_u32 portrait_sample(z72_u32 command,z72_u32 orientation,
                               const volatile z72_u32 *source,z72_u32 *geometry);
static void portrait_log(z72_u32 *log);
#endif
#define z72_main portrait_init
#include "portrait_probe.c"
#undef z72_main

#define APP_ROOTS 0xb202f728u
#define APP_CURRENT 0xb1821b18u
#define MENU_REQUEST 0xb181dabcu
#define MENU_TASK_STATE (0xb1824ea0u+0x6cu)
#define PLAYBACK_TASK_STATE (0xb1828108u+0x6cu)
#define SHOOTING_MODE 0xb25519b8u
#define LIVE_APP 0xb1821b1cu
#define LIVE_UPDATE (LIVE_APP+0x1cu)
#define LIVE_UPDATE_STOCK 0x104e6cb5u
#define I_MENU_APP 0xb1821a08u
#define I_MENU_STATE 0xb1821964u
#define APP_SWITCH_STOCK 0x1053cf67u
#define PICTURE_APP 0xb1823b30u
#define COLOR_APP 0xb1823d84u
#define COLOR_DETAIL_APP 0xb1824e48u
#define PICTURE_KEY 0x105915bbu
#define COLOR_KEY 0x105b5807u
#define COLOR_DETAIL_KEY 0x10658f75u
#define TOUCH_SLOT 0xb181da00u
#define TOUCH_STOCK 0x1053d635u
#define NAV_MODE 0xb18241dcu
#define NAV_SLOT 0xb18241e4u
#define NAV_TABLE 0x123be178u
#define NAV_MODES 18u
#define NAV_COUNT 0xb203250cu
#define NAV_INDEX 0xb2032510u
#define DRAW_COMPLETE 0x105a8b17u
#define TEXT_COMPLETE 0x106b5be3u
#define FONT_CONTEXT 0xb1808c1cu
#define FONT_TABLE 0x120687d4u
#define FONT_RENDER 0x10781afdu
#define FONT_SYMBOL_MEASURE 0x106b6cc1u
#define FONT_SYMBOL_DRAW 0x106b6e65u
#define FONT_BITMAP_LOCK 0xb1808c38u
#define OSD_CLEAR 0x105a95f3u
#define OSD_IMAGE 0x105a9911u
#define OSD_FINISH 0x105a8b43u

enum { LCD_W=720, LCD_H=480, OLD_TOP=280, GRID_TOP=440, CELL_W=120, CELL_H=73,
       TITLE_TOP=370, TITLE_SHIFT=162, STOCK_HINT_START=418, STOCK_GRID_END=426,
       FOOTER_Y=GRID_TOP+3*CELL_H+1, FOOTER_SCALE=57344,
       FOOTER_DX=-211, FOOTER_DY=295, STOCK_HINT_LEFT=242,
       DIAL_STOCK_TOP=44, DIAL_TOP=283, DIAL_SHIFT=254,
       SETTING_SHIFT=235, EXTERNAL_SHIFT=394,
       HUD_SCALE=41943, WIDTH_SCALE=43691, UNIT_SCALE=65536 };
enum page { OTHER, NATIVE_SCREEN, LIVE, GRID, DIAL, SETTING, EXTERNAL_SETTING };
enum placement { UNMAPPED, NORMAL, FIT_WIDTH, FIT_PAGE, HUD };
struct box { int x, y, w, h; };

/* 0 means no backdrop yet; 3 is a menu opened in landscape. */
static u32 layout_orientation;
static u32 external_app;
static u32 auto_rotate=1;
#if Z72_PORTRAIT_TRACE
static volatile u32 portrait_counts[10];
#endif
static u32 live_orientation;
/* Owned by the graphics task. The orientation records the strip to erase on exit. */
static u32 footer_orientation, footer_dirty;
static u32 dial_orientation;
static const struct box footer_area={0,FOOTER_Y,LCD_H,LCD_W-FOOTER_Y};
static const struct box dial_area={0,DIAL_TOP,LCD_H,GRID_TOP-DIAL_TOP};

static u32 rotation_enabled(void)
{
    return __atomic_load_n(&auto_rotate,__ATOMIC_ACQUIRE);
}

static u32 menu_orientation(void)
{
    return rotation_enabled() ? __atomic_load_n(&layout_orientation,__ATOMIC_ACQUIRE) : 0;
}

static u32 grid_page(enum page page)
{
    return page==GRID || page==DIAL;
}

static u32 setting_page(enum page page)
{
    return page==SETTING || page==EXTERNAL_SETTING;
}

static u32 external_root(u32 app)
{
    switch (app) {
    case 1: return PICTURE_APP;
    case 10: return COLOR_APP;
    case 14: return COLOR_DETAIL_APP;
    default: return 0;
    }
}

static enum page current_page(void)
{
    u32 app=BYTE(APP_CURRENT);
    /* Both tasks can take over the display while the shooting app is still selected. */
    u32 menu_state=BYTE(MENU_TASK_STATE), playback_state=BYTE(PLAYBACK_TASK_STATE);
    if (WORD(MENU_REQUEST) || (menu_state>=1 && menu_state<=3) ||
        (playback_state>=1 && playback_state<=3)) return NATIVE_SCREEN;
#ifdef Z72_CUSTOM_DISP
    if (app==15 && WORD(APP_ROOTS+15*4)==DISP_INFORMATION_APP) return NATIVE_SCREEN;
#endif
    if (app && app==__atomic_load_n(&external_app,__ATOMIC_ACQUIRE)) {
        u32 root=external_root(app);
        if (root && WORD(APP_ROOTS+app*4)==root) return EXTERNAL_SETTING;
    }
    if (BYTE(DISPLAY_SNAPSHOT)!=0) return OTHER;
    if (app==0 && WORD(APP_ROOTS)==LIVE_APP && !BYTE(DISPLAY_SNAPSHOT+0x1a6)) return LIVE;
    if ((app!=3 && app!=4) || WORD(APP_ROOTS+app*4)!=I_MENU_APP) return OTHER;
    u32 state=BYTE(I_MENU_STATE), variant=BYTE(I_MENU_STATE+1);
    if (state==3) return GRID;
    if (variant==0 || variant==0x31) {
        if (state==0) return GRID;
        if (state==2) return DIAL;
    }
    return state<=2 ? SETTING : OTHER;
}

static int clamp(int value,int low,int high)
{
    return value<low ? low : value>high ? high : value;
}

static u32 ram_words(u32 address,u32 words)
{
    return address>=0xb1800000u && address<=0xc0000000u-words*4 && !(address&3u);
}

static u32 onscreen(struct box b,int width,int height)
{
    return b.x>=0 && b.y>=0 && b.w>0 && b.h>0 && b.x+b.w<=width && b.y+b.h<=height;
}

static struct box tile_box(int index,int x,int y,int w,int h)
{
    return (struct box){(index%4)*CELL_W+x,GRID_TOP+(index/4)*CELL_H+y,w,h};
}

/* Both input and output here use a top-left origin. */
static struct box lcd_box(struct box box,u32 orientation)
{
    if (orientation==2) return (struct box){box.y,LCD_H-box.x-box.w,box.h,box.w};
    return (struct box){LCD_W-box.y-box.h,box.x,box.h,box.w};
}

static struct box portrait_box(struct box stock,u32 orientation)
{
    if (orientation==2) return (struct box){LCD_H-stock.y-stock.h,stock.x,stock.h,stock.w};
    return (struct box){stock.y,LCD_W-stock.x-stock.w,stock.h,stock.w};
}

static void clear_band(struct box box,u32 orientation)
{
    struct box dest=lcd_box(box,orientation);
    u32 args[7]={(u32)dest.x,(u32)(LCD_H-dest.y-dest.h),(u32)dest.w,(u32)dest.h,0,0,0};
    CALL(OSD_CLEAR,void,const u32 *)(args);
}

static void begin_footer(u32 orientation)
{
    if (!footer_dirty && footer_orientation==orientation) return;
    if (footer_orientation && footer_orientation!=orientation) clear_band(footer_area,footer_orientation);
    clear_band(footer_area,orientation);
    footer_orientation=orientation;
    footer_dirty=0;
}

static void begin_dial(u32 orientation)
{
    if (dial_orientation==orientation) return;
    if (dial_orientation) clear_band(dial_area,dial_orientation);
    clear_band(dial_area,orientation);
    dial_orientation=orientation;
}

static u32 overlaps(struct box a,struct box b)
{
    return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
}

static int fixed_product(int a,int b)
{
    return (int)(((long long)a*b)>>16);
}

static void scale_box(struct box *box,int sx,int sy,int dx,int dy)
{
    int right=fixed_product(box->x+box->w,sx), bottom=fixed_product(box->y+box->h,sy);
    int left=fixed_product(box->x,sx), top=fixed_product(box->y,sy);
    *box=(struct box){left+dx,top+dy,right>left ? right-left : 1,bottom>top ? bottom-top : 1};
}

static u32 stock_hint(int top,int bottom)
{
    return top>=STOCK_HINT_START && bottom>top && bottom>STOCK_GRID_END && bottom<=LCD_H+4;
}

static u32 stock_dial(int top,int bottom)
{
    return top>=DIAL_STOCK_TOP && bottom>top && bottom<=OLD_TOP;
}

static u32 relocate_hint(struct box *box,u32 command)
{
    int full_width=box->x==0 && box->w==LCD_W && (command==0x13 || command==0x17);
    if (box->x<STOCK_HINT_LEFT && !full_width) {
        if (command!=0x17 || box->x+box->w<=STOCK_HINT_LEFT) return 0;
        box->w-=STOCK_HINT_LEFT-box->x;
        box->x=STOCK_HINT_LEFT;
    }
    scale_box(box,FOOTER_SCALE,FOOTER_SCALE,FOOTER_DX,FOOTER_DY);
    if (full_width) { box->x=0; box->w=LCD_H; }
    return box->y>=FOOTER_Y && onscreen(*box,LCD_H,LCD_W);
}

/* The 0x19 renderer scales before rotating about the destination centre.
 * Its input box therefore describes the image before the quarter-turn. */
__attribute__((target("hwdiv")))
static struct box quarter_turn_image_box(struct box dest,int sw,int sh,u32 angle)
{
    int width=(dest.w*sw+sh/2)/sh, height=(dest.h*sh+sw/2)/sw;
    if (width<1) width=1;
    if (height<1) height=1;
    int half=(width+height)/2;
    if (angle==90) {
        dest.x+=height-half;
        dest.y+=dest.h-half;
    } else {
        dest.x+=dest.w-half;
        dest.y+=width-half;
    }
    dest.w=width;
    dest.h=height;
    return dest;
}

#if Z72_PORTRAIT_TRACE
static u32 portrait_sample(u32 command,u32 orientation,const volatile u32 *source,u32 *geometry)
{
    enum page page=current_page();
    if (page==NATIVE_SCREEN && BYTE(DRAW_TARGET)==2 && command==0x34) {
        geometry[0]=BYTE(MENU_TASK_STATE);
        geometry[1]=WORD(MENU_REQUEST);
        geometry[2]=BYTE(APP_CURRENT);
        geometry[3]=BYTE(PLAYBACK_TASK_STATE);
        return 4;
    }
    u32 child=setting_page(page);
    if ((!grid_page(page) && !child) || ((page==DIAL || child) && BYTE(DRAW_TARGET)!=2)) return 0;
    switch (command) {
    case 0x19: case 0x1a: case 0x1c: {
        if (!ram_words((u32)source,17)) return 0;
        int y=command==0x1c ? (int)source[7]>>16 : (short)(source[6]>>16);
        int x=command==0x1c ? (int)source[6]>>16 : (short)source[6];
        if (page==DIAL ? y<DIAL_STOCK_TOP || y>=OLD_TOP :
            child ? y<STOCK_HINT_START || y>=LCD_H :
            y>=LCD_H || (y<STOCK_HINT_START-12 &&
            !(orientation==1 && x<LCD_W-FOOTER_Y) &&
            !(orientation==2 && x>=FOOTER_Y))) return 0;
        geometry[0]=source[6];
        geometry[1]=source[7];
        geometry[2]=(u32)(unsigned short)((short)source[5]-(short)source[4]) |
            ((u32)(unsigned short)((short)(source[5]>>16)-(short)(source[4]>>16))<<16);
        geometry[3]=(source[16]&0xffffu) | ((source[0]&0xfu)<<16) |
            ((source[1]&0xffu)<<20) | ((source[15]!=0)<<28);
        break;
    }
    case 0x13: case 0x16: case 0x17:
        if (!ram_words((u32)source,4)) return 0;
        if (page==DIAL) {
            int bottom=LCD_H-(int)source[1];
            if (!stock_dial(bottom-(int)source[3],bottom)) return 0;
        } else if ((int)source[1]+(int)source[3]>LCD_H-(STOCK_HINT_START-12)) return 0;
        for (u32 i=0;i<4;++i) geometry[i]=source[i];
        break;
    case 0x34: case 0x37: case 0x38: {
        if (!ram_words((u32)source,1)) return 0;
        u32 address=source[0];
        if (!ram_words(address,0x68/4)) return 0;
        const volatile unsigned short *desc=(const volatile unsigned short *)address;
        int top=LCD_H-desc[0x64/2], bottom=LCD_H-desc[0x60/2];
        if (page==DIAL ? !stock_dial(top,bottom) : !stock_hint(top,bottom)) return 0;
        geometry[0]=desc[0x5e/2];
        geometry[1]=(u32)top;
        geometry[2]=desc[0x62/2];
        geometry[3]=(u32)bottom;
        break;
    }
    default: return 0;
    }
    return child ? 3 : page==DIAL ? 2 : 1;
}
#endif

static u32 relocate_tile(struct box *box,u32 image)
{
    if (box->x<0 || box->x>=LCD_W || box->y<OLD_TOP-2 || box->y>=OLD_TOP+2*CELL_H ||
        box->w<=0 || box->w>LCD_W || box->h<=0 || box->h>LCD_H) return 0;
    int cx=box->x+box->w/2, cy=box->y+box->h/2;
    if (cy<OLD_TOP || cy>=OLD_TOP+2*CELL_H || box->w>CELL_W+(image ? 12 : 0) || box->h>CELL_H) return 0;
    int col=clamp(cx/CELL_W,0,5), row=(cy-OLD_TOP)/CELL_H;
    int w=box->w, h=box->h;
    if (w>CELL_W) {
        /* The accepted overshoot is small, so finding the fitted height is bounded. */
        while (h>1 && h*w>box->h*CELL_W) --h;
        w=CELL_W;
    }
    int x=clamp(cx-col*CELL_W-w/2,0,CELL_W-w);
    int y=clamp(cy-OLD_TOP-row*CELL_H-h/2,0,CELL_H-h);
    *box=tile_box(row*6+col,x,y,w,h);
    return 1;
}

static int movie_meter_top(struct box box)
{
    if (!onscreen(box,138,LCD_H) || WORD(SHOOTING_MODE)!=0) return 0;
    /* Video has two native meter positions. The backgrounds, L/R labels and
     * the individual segments share the same bounds at each position. */
    if (box.y>=352 && box.y+box.h<=392) return 354;
    return box.y>=222 && box.y+box.h<=262 ? 224 : 0;
}

static u32 movie_meter_segment(struct box box)
{
    if (box.w!=4 || box.h!=10 || box.x<22 || box.x>126 || (box.x-22)%8) return 0;
    int top=movie_meter_top(box);
    return top && (box.y==top+4 || box.y==top+22);
}

static enum placement relocate_live(struct box *box)
{
    int x=box->x, y=box->y;
    /* The top status rows form a single strip. Keep their stock spacing,
     * including independently drawn symbols and their clear rectangles. */
    if (y>=0 && y+box->h<=112 && x>=0 && x+box->w<=LCD_W+4) {
        scale_box(box,HUD_SCALE,HUD_SCALE,10,8);
        return HUD;
    }
    if (box->w>240 || box->h>155) return 0;
    int meter_top=movie_meter_top(*box);
    if (meter_top) { box->x+=8; box->y+=374-meter_top; }
    else if (x<95 && y>=90 && y<232) { box->x+=3; box->y+=365; }
    else if (x>=655 && y>=120 && y<=185) { box->x-=243; box->y+=366; }
    else if (x>=680 && y>=225 && y<440) { box->x-=240; box->y+=13; }
    else if (x<45 && y>=300 && y<=355) { box->x+=4; box->y+=108; }
    else if (x<80 && y>=392 && y<430) { box->x+=380; box->y+=191; }
    else if (x>=110 && x<180 && y>=392 && y<430) { box->x+=10; box->y+=242; }
    else if (x>=620 && y>=388 && y<430) { box->x-=240; box->y+=154; }
    else if (x<50 && y>=430) { box->x+=108; box->y+=160; }
    else if (x>=590 && y>=425) { box->x-=240; box->y+=180; }
    else if (x>=125 && x<590 && y>=425) { box->x-=125; box->y+=236; }
    else return 0;
    box->x=clamp(box->x,0,LCD_H-box->w);
    box->y=clamp(box->y,0,LCD_W-box->h);
    return NORMAL;
}

static enum placement relocate_box(struct box *box,enum page page,u32 image)
{
    if (page==LIVE) return relocate_live(box);
    if (page==SETTING) {
        if (!onscreen(*box,LCD_W,LCD_H)) return 0;
        scale_box(box,WIDTH_SCALE,UNIT_SCALE,0,SETTING_SHIFT);
        return FIT_WIDTH;
    }
    if (page==EXTERNAL_SETTING) {
        if (!onscreen(*box,LCD_W,LCD_H)) return 0;
        scale_box(box,WIDTH_SCALE,WIDTH_SCALE,0,EXTERNAL_SHIFT);
        return FIT_PAGE;
    }
    if (page==DIAL && stock_dial(box->y,box->y+box->h)) {
        if (!onscreen(*box,LCD_W,LCD_H)) return 0;
        scale_box(box,WIDTH_SCALE,WIDTH_SCALE,0,DIAL_SHIFT);
        return FIT_PAGE;
    }
    if (box->y>=208 && box->y+box->h<=OLD_TOP) {
        if (box->w>LCD_H) { box->x=0; box->w=LCD_H; }
        else box->x=clamp(box->x-120,0,LCD_H-box->w);
        box->y+=TITLE_SHIFT;
        return NORMAL;
    }
    return relocate_tile(box,image);
}

static enum placement relocate_label(struct box *stock,struct box *dest,enum page page,int anchor_y)
{
    /* The font API positions the lower edge in its bottom-left coordinate space. */
    stock->y=clamp(anchor_y-stock->h,0,LCD_H-stock->h);
    if (grid_page(page) && anchor_y>=218 && anchor_y<OLD_TOP && stock->h<=OLD_TOP-208)
        stock->y=clamp(stock->y,208,OLD_TOP-stock->h);
    *dest=*stock;
    enum placement placement=relocate_box(dest,page,0);
    if (placement) return placement;
    /* Some live widgets supply a tighter box than their surrounding UI slot. */
    if (page!=LIVE) return 0;
    stock->y=clamp(anchor_y,0,LCD_H-stock->h);
    *dest=*stock;
    return relocate_box(dest,page,0);
}

static u32 title_clear(u32 command,struct box stock)
{
    return command==0x17 && stock.x==88 && stock.y==228 && stock.w==544 && stock.h==44;
}

/* An edge shared in the original 6x2 grid can border two separate tiles. */
static u32 relocate_edge(struct box stock,struct box *boxes)
{
    if (stock.w<=2 && stock.h>=CELL_H-3 && stock.h<=CELL_H+3) {
        int col=(stock.x+CELL_W/2)/CELL_W;
        int y=stock.y-OLD_TOP;
        if (col<0 || col>6 || stock.x<col*CELL_W-2 || stock.x>col*CELL_W+1) return 0;
        int row=y<=2 && y>=-2 ? 0 : y>=CELL_H-2 && y<=CELL_H+2 ? 1 : -1;
        if (row<0) return 0;
        int right=row*6+col;
        if (col>0) boxes[0]=tile_box(right-1,CELL_W-stock.w,0,stock.w,CELL_H);
        if (col==6 || (col>0 && right%4!=0)) return 1;
        boxes[col>0]=tile_box(right,0,0,stock.w,CELL_H);
        return col>0 ? 2 : 1;
    }
    if (stock.h<=2 && stock.w>=CELL_W-2 && stock.w<=CELL_W+3) {
        int col=(stock.x+stock.w/2)/CELL_W;
        int y=stock.y-OLD_TOP;
        if (col<0 || col>=6 || stock.x<col*CELL_W-2 || stock.x>col*CELL_W+2) return 0;
        int edge=y<=2 && y>=-2 ? 0 : y>=CELL_H-3 && y<=CELL_H+2 ? 1 :
                 y>=2*CELL_H-3 && y<=2*CELL_H+2 ? 2 : -1;
        if (edge<0) return 0;
        if (edge>0) boxes[0]=tile_box((edge-1)*6+col,0,CELL_H-stock.h,CELL_W,stock.h);
        if (edge<2) boxes[edge>0]=tile_box(edge*6+col,0,0,CELL_W,stock.h);
        return edge==1 ? 2 : 1;
    }
    return 0;
}

static void defer_completion(u32 command,u32 result)
{
    (void)command; (void)result;
}

static const volatile unsigned char *text_descriptor;
static enum page text_page;
static u32 text_orientation;
static int text_symbol_sx, text_symbol_sy, text_symbol_top, text_symbol_bottom;

static u32 hint_text(const volatile unsigned short *desc)
{
    return stock_hint(LCD_H-desc[0x64/2],LCD_H-desc[0x60/2]);
}

/* Decode through the same native bitmap reader and allocator as the stock symbol callback. */
struct symbol_info {
    u32 width,height,depth,format,attributes[3],palette,bytes,stride,source[3];
};

static u32 decode_symbol(u32 asset,u32 length,struct symbol_info *info)
{
    u32 reader[3]={0}, allocation=0;
    CALL(0x103d5919u,void,u32)(FONT_BITMAP_LOCK);
    CALL(0x106b76fdu,void,u32 *)(reader);
    if (reader[0]) {
        CALL(0x106b75a5u,void,u32,u32,u32 *,struct symbol_info *,u32)(asset,length,reader,info,0);
        if (info->width && info->width<=LCD_H && info->height && info->height<=LCD_H &&
            info->stride && info->bytes && info->bytes<=128*1024 &&
            info->stride<=info->bytes && (unsigned long long)info->stride*info->height<=info->bytes) {
            allocation=CALL(0x106b7395u,u32,u32)(info->bytes+127);
        }
        if (allocation) {
            u32 output[2]={(allocation+127)&~127u,info->bytes};
            if (!CALL(0x106b7521u,u32,u32 *,struct symbol_info *,const u32 *)(reader,info,output)) {
                CALL(0x1034fe7fu,void,u32)(allocation);
                allocation=0;
            }
        } else {
            CALL(0x1054d799u,void,u32 *,u32 *,u32 *)(reader,reader+1,reader+2);
        }
    }
    CALL(0x103d58fbu,void,u32)(FONT_BITMAP_LOCK);
    return allocation;
}

static u32 draw_menu_symbol(const u32 *glyph,u32 capacity,u32 *attributes,u32 descriptor)
{
    (void)capacity; (void)attributes;
    if (!glyph[0]) return 0; /* Measurement is handled by the stock callback. */
    u32 code=glyph[1], asset=0, length=0;
    if (code>=0xe700 && code<=0xe702 && BYTE(descriptor+0x7e + 8*(code-0xe700))==1) {
        u32 override=WORD(descriptor+0x80+8*(code-0xe700));
        CALL(0x106b734bu,void,u32,u32 *,u32 *)(override,&length,&asset);
    } else {
        CALL(0x106b7345u,void,u32,u32 *,u32 *)(code,&length,&asset);
    }
    if (!asset || !length) return 0;

    int metrics[4];
    u32 context=WORD(descriptor+0x94);
    if (CALL(0x10673cb5u,u32,u32,int *)(context,metrics)) return 0;
    struct symbol_info info;
    CALL(0x10361035u,void,void *,u32)(&info,sizeof(info));
    u32 allocation=decode_symbol(asset,length,&info);
    if (!allocation) return 0;

    const int *matrix=(const int *)glyph[2];
    int w=(int)info.width, h=(int)info.height;
    int x=matrix[5], baseline=matrix[2];
    if (text_orientation==1) {
        x=LCD_H*UNIT_SCALE-x;
        baseline=LCD_W*UNIT_SCALE-baseline;
    }
    int ascent=(int)((long long)(unsigned short)HALF(descriptor+0xa)*
                     ((long long)metrics[1]-metrics[2])/2);
    /* The native provider vertically centres the bitmap on the font's em box. */
    int top=baseline-fixed_product(h*(UNIT_SCALE/2)+ascent,text_symbol_sy);
    struct box box={x>>16,top>>16,fixed_product(w,text_symbol_sx),fixed_product(h,text_symbol_sy)};
    if (box.y>=text_symbol_top && onscreen(box,LCD_H,text_symbol_bottom)) {
        static const unsigned char formats[]={0,1,5,7,9,10};
        u32 kind[5]={info.depth,info.format,info.palette,0,0};
        CALL(0x106b74c3u,void,u32 *)(kind);
        if (kind[3]<sizeof(formats)) {
            struct box dest=lcd_box(box,text_orientation);
            u32 image[23];
            CALL(0x10361035u,void,void *,u32)(image,sizeof(image));
            image[1]=formats[kind[3]];
            image[2]=(allocation+127)&~127u;
            image[5]=(u32)w | ((u32)h<<16);
            image[6]=(u32)dest.x | ((u32)dest.y<<16);
            image[7]=(u32)(dest.x+dest.w) | ((u32)(dest.y+dest.h)<<16);
            image[8]=WORD(descriptor+0x28);
            image[14]=kind[4];
            image[16]=text_orientation==2 ? 90u : 270u;
            image[19]=((u32)w+15)&~15u;
            image[20]=((u32)h+7)&~7u;
            CALL(OSD_IMAGE,void,const u32 *)(image);
            CALL(OSD_FINISH,void,void)();
            CALL(0x10673ecfu,u32,u32,u32)(context,image[8]);
        }
    }
    CALL(0x1034fe7fu,void,u32)(allocation);
    return 0;
}

/* This callback is installed only for the duration of one synchronous text
 * packet. Bounding-box queries retain the stock matrix for text alignment. */
static u32 render_text(u32 *context,void *options,const int *origin,void *metrics,void *bounds)
{
    u32 (*render)(u32 *,void *,const int *,void *,void *)=CALL(FONT_RENDER,u32,u32 *,void *,const int *,void *,void *);
    u32 *matrix=context+0x34/4;
    if (metrics || bounds || !origin) return render(context,options,origin,metrics,bounds);
    const volatile unsigned short *desc=(const volatile unsigned short *)text_descriptor;
    u32 hint=grid_page(text_page) && hint_text(desc);
    u32 child_hint=setting_page(text_page) && hint_text(desc);
    if (!matrix[0] || !matrix[4] || matrix[0]>2*UNIT_SCALE ||
        matrix[4]>2*UNIT_SCALE || matrix[1] || matrix[3])
        return hint || child_hint ? 0 : render(context,options,origin,metrics,bounds);

    int x=(int)((u32)fixed_product(origin[0],(int)matrix[0])+matrix[2])>>16;
    int y=LCD_H-((int)((u32)fixed_product(origin[1],(int)matrix[4])+matrix[5])>>16);
    int width=desc[0xac/2], height=desc[0xae/2];
    if (!width) width=desc[8/2];
    if (!height) height=desc[0xa/2];
    width=fixed_product(width,(int)matrix[0]);
    height=fixed_product(height,(int)matrix[4]);
    hint=hint || (grid_page(text_page) && y>STOCK_GRID_END);
    child_hint=child_hint || (setting_page(text_page) && y>STOCK_GRID_END);
    if (x<0 || x>=LCD_W || y<0 || y>LCD_H || (!child_hint &&
        (width<=0 || width>LCD_W || height<=0 || height>LCD_H)))
        return hint || child_hint ? 0 : render(context,options,origin,metrics,bounds);
    struct box stock={x,y,width,height}, dest;
    int anchor=y;
    if (grid_page(text_page)) {
        int left=desc[0x5e/2], bottom=LCD_H-desc[0x60/2];
        int right=desc[0x62/2], top=LCD_H-desc[0x64/2];
        if (left>=88 && right<=632 && right>left && top>=208 && bottom<=OLD_TOP && bottom>top)
            anchor=bottom<OLD_TOP ? bottom : OLD_TOP-1;
    }
    enum placement place=UNMAPPED;
    if (hint) {
        dest=(struct box){x,y-height,width,height};
        if (!relocate_hint(&dest,0)) return 0;
        begin_footer(text_orientation);
    } else if (child_hint) {
        place=text_page==SETTING ? FIT_WIDTH : FIT_PAGE;
    } else {
        place=relocate_label(&stock,&dest,text_page,anchor);
        if (!place || !onscreen(dest,LCD_H,LCD_W))
            return render(context,options,origin,metrics,bounds);
    }
    u32 dial=text_page==DIAL && place==FIT_PAGE;
    if (dial) begin_dial(text_orientation);

    u32 saved[7];
    for (u32 i=0;i<7;++i) saved[i]=matrix[i];
    /* Use the same scale as the corresponding rectangles and bitmaps. */
    int sx=hint ? FOOTER_SCALE : place==HUD ? HUD_SCALE :
           place==FIT_WIDTH || place==FIT_PAGE ? WIDTH_SCALE : UNIT_SCALE;
    int sy=hint ? FOOTER_SCALE : place==HUD ? HUD_SCALE : place==FIT_PAGE ? WIDTH_SCALE : UNIT_SCALE;
    int child_shift=text_page==SETTING ? SETTING_SHIFT : EXTERNAL_SHIFT;
    int dx=hint ? FOOTER_DX*UNIT_SCALE : child_hint ? 0 : dest.x*UNIT_SCALE-x*sx;
    int dy=hint ? FOOTER_DY*UNIT_SCALE : child_hint ? child_shift*UNIT_SCALE :
           dest.y*UNIT_SCALE-stock.y*sy;
    int tx=dx+fixed_product((int)saved[2],sx);
    int ty=dy+LCD_H*sy-fixed_product((int)saved[5],sy);
    int ax=fixed_product((int)saved[0],sx), ay=fixed_product((int)saved[4],sy);
    matrix[0]=matrix[4]=matrix[6]=0;
    if (text_orientation==2) {
        matrix[1]=(u32)-ay;
        matrix[2]=(u32)ty;
        matrix[3]=(u32)ax;
        matrix[5]=(u32)tx;
    } else {
        matrix[1]=(u32)ay;
        matrix[2]=(u32)(LCD_W*UNIT_SCALE-ty);
        matrix[3]=(u32)-ax;
        matrix[5]=(u32)(LCD_H*UNIT_SCALE-tx);
    }
    /* The stock inline bitmap provider uses only the glyph translation.
     * Settings dialogs can place button hints anywhere in the panel. */
    u32 provider_slot=0, provider=0, local_provider[5];
    if (hint || setting_page(text_page) || dial) {
        u32 state=context[0x45d8/4];
        if (ram_words(state,0xbc/4) && WORD(state+0xb4)==1) {
            provider=WORD(state+0xb8);
            if (ram_words(provider,5) && WORD(provider+8)==FONT_SYMBOL_MEASURE &&
                WORD(provider+12)==FONT_SYMBOL_DRAW && WORD(provider+16)==(u32)text_descriptor) {
                for (u32 i=0;i<5;++i) local_provider[i]=WORD(provider+i*4);
                local_provider[3]=(u32)draw_menu_symbol;
                text_symbol_sx=sx;
                text_symbol_sy=sy;
                text_symbol_top=hint ? FOOTER_Y : dial ? DIAL_TOP : child_shift;
                text_symbol_bottom=dial ? GRID_TOP : LCD_W;
                provider_slot=state+0xb8;
                WORD(provider_slot)=(u32)local_provider;
            }
        }
    }
    u32 result=render(context,options,origin,metrics,bounds);
    if (provider_slot) WORD(provider_slot)=provider;
    for (u32 i=0;i<7;++i) matrix[i]=saved[i];
#if Z72_PORTRAIT_TRACE
    ++portrait_counts[text_orientation+5];
#endif
    return result;
}

static u32 portrait_text(volatile u32 *message,enum page page,u32 orientation,u32 words)
{
    u32 address=message[5];
    if (message[3]!=TEXT_COMPLETE || !ram_words(address,words)) return 0;
    u32 descriptor=WORD(address), context=WORD(FONT_CONTEXT);
    if (!ram_words(descriptor,0xb0/4) || !ram_words(context,0x4604/4) ||
        WORD(descriptor+0x94)!=context || WORD(context+0x45b4)!=FONT_TABLE ||
        WORD(FONT_TABLE+0x1c)!=FONT_RENDER) return 0;

    u32 table[8], local[7];
    for (u32 i=0;i<8;++i) table[i]=WORD(FONT_TABLE+i*4);
    for (u32 i=0;i<7;++i) local[i]=message[i];
    table[7]=(u32)render_text;
    local[3]=(u32)defer_completion;
    text_descriptor=(const volatile unsigned char *)descriptor;
    text_page=page;
    text_orientation=orientation;
    const volatile unsigned short *desc=(const volatile unsigned short *)descriptor;
    u32 hint=(grid_page(page) || setting_page(page)) && hint_text(desc);
    u32 dial=page==DIAL && stock_dial(LCD_H-desc[0x64/2],LCD_H-desc[0x60/2]);
    /* The native 0x34 path can paint a backing rectangle before calling the glyph renderer. */
    u32 background=(hint || dial) && message[4]==DRAW_FAMILY+0x34 ? WORD(descriptor+0x2c) : 0;
    if (background) WORD(descriptor+0x2c)=0;
    WORD(context+0x45b4)=(u32)table;
    CALL(DRAW_DISPATCH,void,const u32 *)(local);
    WORD(context+0x45b4)=FONT_TABLE;
    if (background) WORD(descriptor+0x2c)=background;
    message[6]=local[6];
    CALL(TEXT_COMPLETE,void,u32,u32)(0x42010063u,local[6]);
    return 1;
}

static u32 is_portrait(u32 orientation)
{
    return orientation==1 || orientation==2;
}

static u32 focus_color(u32 color)
{
    u32 r=color>>24, g=(color>>16)&255u, b=(color>>8)&255u;
    return (r>=0xc0u && g<0x80u && b<0x60u) || (r<0x40u && g>=0xc0u && b<0x40u);
}

static u32 consume_draw(volatile u32 *message)
{
    message[6]=0;
    CALL(DRAW_COMPLETE,void,u32,u32)(0x42010062u,0);
    return 1;
}

static u32 portrait_draw(volatile u32 *message,u32 outputs,u32 orientation)
{
    enum page page=outputs==1 ? current_page() : OTHER;
    if (page!=NATIVE_SCREEN && !rotation_enabled()) page=OTHER;
    u32 command=message[4]-DRAW_FAMILY, target=BYTE(DRAW_TARGET), words;
    u32 grid=grid_page(page);
    if (page==NATIVE_SCREEN) footer_orientation=footer_dirty=dial_orientation=0;
    if (!grid && outputs==1 && target==2 && footer_orientation) {
        clear_band(footer_area,footer_orientation);
        footer_orientation=0;
        footer_dirty=0;
    }
    if (page!=DIAL && outputs==1 && target==2 && dial_orientation) {
        clear_band(dial_area,dial_orientation);
        dial_orientation=0;
    }
    if (page==OTHER || page==NATIVE_SCREEN || page==LIVE) {
        if (__atomic_load_n(&external_app,__ATOMIC_RELAXED))
            __atomic_store_n(&external_app,0,__ATOMIC_RELEASE);
        if (__atomic_load_n(&layout_orientation,__ATOMIC_RELAXED))
            __atomic_store_n(&layout_orientation,0,__ATOMIC_RELEASE);
    }
    u32 layout=page==LIVE ? orientation : __atomic_load_n(&layout_orientation,__ATOMIC_ACQUIRE);
    if (grid && is_portrait(layout) && target==1 && command==0x17 &&
        message[3]==DRAW_COMPLETE && ram_words(message[5],7)) {
        const volatile u32 *rect=(const volatile u32 *)message[5];
        if (rect[0]==0 && rect[1]==0 && rect[2]==LCD_W && rect[3]==LCD_H) footer_dirty=1;
    }
    if (page==OTHER || page==NATIVE_SCREEN || target!=2) return 0;
    if (page==SETTING && !layout) {
        layout=is_portrait(orientation) ? orientation : 3;
        __atomic_store_n(&layout_orientation,layout,__ATOMIC_RELEASE);
    }
    if (command==0x34 || command==0x37 || command==0x38) {
        if (grid && !layout) layout=orientation;
        if (!is_portrait(layout)) return 0;
        u32 handled=portrait_text(message,page,layout,command==0x38 ? 3 : 2);
        if (!handled && grid) footer_dirty=1;
        return handled;
    }
    if (message[3]!=DRAW_COMPLETE) return 0;
    u32 image=command>=0x19 && command<=0x1c && command!=0x1b;
    switch (command) {
    case 0x13: words=8; break;
    case 0x16: case 0x17: words=7; break;
    case 0x19: words=19; break;
    case 0x1a: words=25; break;
    case 0x1c: words=19; break;
    default: return 0;
    }
    u32 address=message[5];
    if (!ram_words(address,words)) return 0;
    const volatile u32 *source=(const volatile u32 *)address;
    struct box stock;
    u32 angle=0;
    int source_width=0, source_height=0;
    if (image) {
        u32 mode=source[0], start=source[6], end=source[7];
        u32 source_start=source[4], source_end=source[5];
        if (mode>1) return 0;
        int x,y,w,h;
        if (command==0x1c) {
            /* This renderer has 16.16 destination coordinates and no angle. */
            x=(int)start>>16;
            y=(int)end>>16;
            w=((int)source[8]>>16)-x;
            h=((int)source[9]>>16)-y;
        } else {
            angle=source[16]&0xffffu;
            x=(short)start;
            y=(short)(start>>16);
            w=(short)end-x;
            h=(short)(end>>16)-y;
        }
        source_width=(short)source_end-(short)source_start;
        source_height=(short)(source_end>>16)-(short)(source_start>>16);
        if (mode==1) { w=source_width; h=source_height; }
        stock=(struct box){x,y,w,h};
        if (w>=600 && h>=400) {
            if (grid && is_portrait(layout)) footer_dirty=1;
            return 0; /* Full preview and surface copies. */
        }
    } else {
        int x=(int)source[0], y=(int)source[1], w=(int)source[2], h=(int)source[3];
        if (x<0 || x>LCD_W || y<-4 || y>=LCD_H || w<=0 || w>LCD_W || h<=0 || h>LCD_H) return 0;
        if (w==LCD_W && h==LCD_H) {
            if (grid && is_portrait(layout)) footer_dirty=1;
            return 0;
        }
        stock=(struct box){x,LCD_H-y-h,w,h};
    }
    if (page==LIVE && ((image && command!=0x1c && focus_color(source[8])) ||
        (command==0x13 && focus_color(source[4]))) && !movie_meter_segment(stock)) return 0;

    u32 backdrop=grid && (command==0x13 || command==0x17) && stock.x==0 && stock.y==OLD_TOP &&
                 stock.w==LCD_W && stock.h==2*CELL_H;
    u32 clear_title=page==GRID && title_clear(command,stock);
    u32 dial=page==DIAL && stock_dial(stock.y,stock.y+stock.h);
    /* The top grid border can extend two pixels into the stock dial strip. */
    if (dial && !image && stock.y>=OLD_TOP-2 && stock.h<=2 &&
        stock.w>=CELL_W-2 && stock.w<=CELL_W+3) dial=0;
    if ((backdrop || clear_title || dial) && layout==0) {
        layout=is_portrait(orientation) ? orientation : 3;
        __atomic_store_n(&layout_orientation,layout,__ATOMIC_RELEASE);
#if Z72_PORTRAIT_TRACE
        if (layout!=3) ++portrait_counts[layout-1];
#endif
    }
    if (!is_portrait(layout)) return 0;
    u32 hint=grid && onscreen(stock,LCD_W+4,LCD_H+4) && stock_hint(stock.y,stock.y+stock.h);
    u32 strip=hint || dial;
    if (!hint && !onscreen(stock,LCD_W+4,LCD_H)) return dial ? consume_draw(message) : 0;
    if (image) {
        if (command==0x1c && (source[17] || source[18])) return strip ? consume_draw(message) : 0;
        if (!hint && grid && stock.y>=OLD_TOP && (source_width>2*CELL_W || source_height>2*CELL_H)) return 0;
        if (angle==360) angle=0;
        if (angle && (angle>270 || angle%90 || (!strip && page!=EXTERNAL_SETTING)))
            return strip ? consume_draw(message) : 0;
        /* This path's mirror flag reflects across the entire LCD. */
        if (strip && command==0x19 && (source[15] || source_width<=0 || source_height<=0 ||
            source_width>LCD_W || source_height>LCD_H)) return consume_draw(message);
    }

    struct box boxes[2];
    struct box panel={0,GRID_TOP,LCD_H,3*CELL_H};
    u32 count=1;
    if (hint) {
        boxes[0]=stock;
        if (!relocate_hint(&boxes[0],command)) return consume_draw(message);
        begin_footer(layout);
    } else if (dial) {
        boxes[0]=stock;
        if (!relocate_box(&boxes[0],page,image)) return consume_draw(message);
        begin_dial(layout);
    } else if (backdrop) boxes[0]=panel;
    else if (clear_title) {
        /* Clear the reserved title band and the part of the old sideways
         * title above it, keeping the current grid and footer intact. */
        boxes[0]=(struct box){0,TITLE_TOP,LCD_H,GRID_TOP-TITLE_TOP};
        boxes[1]=portrait_box(stock,layout);
        int bottom=clamp(boxes[1].y+boxes[1].h,0,TITLE_TOP);
        boxes[1].h=bottom-boxes[1].y;
        count=boxes[1].h>0 ? 2 : 1;
    } else {
        count=grid && !image ? relocate_edge(stock,boxes) : 0;
        if (!count) {
            boxes[0]=stock;
            if (!relocate_box(&boxes[0],page,image)) {
                /* An unrelated partial clear in landscape coordinates can erase a reflowed tile. */
                if (grid) {
                    if (command==0x17 && (overlaps(stock,lcd_box(panel,layout)) ||
                        overlaps(stock,lcd_box(footer_area,layout)))) return consume_draw(message);
                    if (overlaps(stock,lcd_box(footer_area,layout))) footer_dirty=1;
                }
                return 0;
            }
            count=1;
        }
    }

    u32 payload[25], local[7];
    if (command==0x1c) {
        for (u32 i=0;i<25;++i) payload[i]=0;
        for (u32 i=0;i<6;++i) payload[i]=source[i];
        payload[8]=0xffffffffu;
        payload[9]=source[11];
        payload[11]=source[10];
        payload[17]=source[12];
        payload[18]=source[13];
        payload[20]=source[14];
        payload[21]=source[15];
        payload[22]=source[16];
    } else {
        for (u32 i=0;i<words;++i) payload[i]=source[i];
    }
    /* Image commands can clear beyond their destination before painting. */
    if (strip && image) payload[9]=0;
    for (u32 i=0;i<7;++i) local[i]=message[i];
    local[3]=(u32)defer_completion;
    local[5]=(u32)payload;
    if (command==0x1c) local[4]=DRAW_FAMILY+0x1a;
    angle+=layout==2 ? 90u : 270u;
    if (angle>=360) angle-=360;
    for (u32 i=0;i<count;++i) {
        struct box dest=lcd_box(boxes[i],layout);
        if (image) {
            if (strip && command==0x19 && (angle==90 || angle==270)) {
                dest=quarter_turn_image_box(dest,source_width,source_height,angle);
                if (dest.w>LCD_W || dest.h>LCD_H) return consume_draw(message);
            }
            payload[0]=0;
            payload[6]=(u32)(unsigned short)dest.x | ((u32)(unsigned short)dest.y<<16);
            payload[7]=(u32)(unsigned short)(dest.x+dest.w) | ((u32)(unsigned short)(dest.y+dest.h)<<16);
            payload[16]=(payload[16]&0xffff0000u) | angle;
        } else {
            payload[0]=(u32)dest.x;
            payload[1]=(u32)(LCD_H-dest.y-dest.h);
            payload[2]=(u32)dest.w;
            payload[3]=(u32)dest.h;
        }
        CALL(DRAW_DISPATCH,void,const u32 *)(local);
    }
#if Z72_PORTRAIT_TRACE
    if (page==LIVE) ++portrait_counts[layout+7];
    else if (image) ++portrait_counts[layout+1];
#endif
    message[6]=local[6];
    CALL(DRAW_COMPLETE,void,u32,u32)(0x42010062u,local[6]);
    return 1;
}

static void menu_switch(u32 kind,u32 event)
{
    if (kind==4) {
        u32 child=0;
        enum page page=current_page();
        if ((WORD(DISPLAY_OUTPUTS)&7u)==1 && (grid_page(page) || page==SETTING) &&
            is_portrait(menu_orientation())) {
            switch (event) {
            case 0x00100002u: child=1; break;
            case 0x00100012u: child=10; break;
            case 0x0010001bu: child=14; break;
            }
        }
        __atomic_store_n(&external_app,child,__ATOMIC_RELEASE);
    }
    CALL(APP_SWITCH_STOCK,void,u32,u32)(kind,event);
}

static u32 external_key(u32 app,u32 key)
{
    if (key<2 || key>5 || BYTE(APP_CURRENT)!=app || (WORD(DISPLAY_OUTPUTS)&7u)!=1 ||
        current_page()!=EXTERNAL_SETTING) return key;
    u32 layout=menu_orientation();
    static const unsigned char rotated[2][4]={{4,5,3,2},{5,4,2,3}};
    return is_portrait(layout) ? rotated[layout-1][key-2] : key;
}

static void picture_key(u32 key) { CALL(PICTURE_KEY,void,u32)(external_key(1,key)); }
static void color_key(u32 key) { CALL(COLOR_KEY,void,u32)(external_key(10,key)); }
static void color_detail_key(u32 key) { CALL(COLOR_DETAIL_KEY,void,u32)(external_key(14,key)); }

enum { TOUCH_W=1024, TOUCH_H=682, TOUCH_BYTES=0x3c };

/* Match scale_box's floor-rounded edges when converting a touched pixel back. */
__attribute__((target("hwdiv")))
static int unscale_pixel(int pixel,int scale)
{
    return ((pixel+1)*UNIT_SCALE-1)/scale;
}

static void menu_touch_point(short *point,enum page page,u32 orientation)
{
    int x=(point[0]*LCD_W+TOUCH_W/2)/TOUCH_W;
    int y=(point[1]*LCD_H+TOUCH_H/2)/TOUCH_H;
    int u=orientation==2 ? LCD_H-1-y : y;
    int v=orientation==2 ? x : LCD_W-1-x;
    int stock_x=-1, stock_y=-1;

    if (u>=0 && u<LCD_H && v>=0 && v<LCD_W) {
        if (grid_page(page)) {
            if (v>=GRID_TOP && v<GRID_TOP+3*CELL_H) {
                int row=(v-GRID_TOP)/CELL_H, col=u/CELL_W;
                int cell=row*4+col;
                stock_x=(cell%6)*CELL_W+u%CELL_W;
                stock_y=OLD_TOP+(cell/6)*CELL_H+(v-GRID_TOP)%CELL_H;
            } else if (v>=FOOTER_Y && v<FOOTER_DY+fixed_product(LCD_H,FOOTER_SCALE)) {
                stock_x=unscale_pixel(u-FOOTER_DX,FOOTER_SCALE);
                stock_y=unscale_pixel(v-FOOTER_DY,FOOTER_SCALE);
            } else if (page==DIAL && v>=DIAL_TOP && v<GRID_TOP) {
                stock_x=unscale_pixel(u,WIDTH_SCALE);
                stock_y=unscale_pixel(v-DIAL_SHIFT,WIDTH_SCALE);
            }
        } else {
            int shift=page==SETTING ? SETTING_SHIFT : EXTERNAL_SHIFT;
            int sy=page==SETTING ? UNIT_SCALE : WIDTH_SCALE;
            if (v>=shift && v<shift+fixed_product(LCD_H,sy)) {
                stock_x=unscale_pixel(u,WIDTH_SCALE);
                stock_y=unscale_pixel(v-shift,sy);
            }
        }
    }

    /* Outside the visible controls, send a no-hit point and retain the release event. */
    if (stock_x<0 || stock_x>=LCD_W || stock_y<0 || stock_y>=LCD_H) {
        point[0]=point[1]=-4;
        return;
    }
    point[0]=(short)((stock_x*TOUCH_W+LCD_W/2)/LCD_W);
    point[1]=(short)((stock_y*TOUCH_H+LCD_H/2)/LCD_H);
}

static void portrait_touch(const u32 *event)
{
    void (*stock)(const u32 *)=CALL(TOUCH_STOCK,void,const u32 *);
    u32 orientation=menu_orientation();
    if (!event || !is_portrait(orientation) || (WORD(DISPLAY_OUTPUTS)&7u)!=1) {
        stock(event);
        return;
    }
    enum page page=current_page();
    u32 kind=((const unsigned char *)event)[9];
    if ((!grid_page(page) && !setting_page(page)) || !kind || kind>=9) {
        stock(event);
        return;
    }

    /* The native callback copies this event into the queue synchronously. */
    union { u32 words[TOUCH_BYTES/4]; short points[TOUCH_BYTES/2]; } local;
    for (u32 i=0;i<TOUCH_BYTES/4;++i) local.words[i]=event[i];
    static const unsigned char point_offsets[]={0x12,0x1a,0x28,0x2c,0x30,0x34};
    for (u32 i=0;i<sizeof(point_offsets);++i) {
        short *point=local.points+point_offsets[i]/2;
        if (i==0 || point[0] || point[1]) menu_touch_point(point,page,orientation);
    }
    stock(local.words);
}

static u32 portrait_nav(u32 direction)
{
    u32 mode=BYTE(NAV_MODE);
    if (mode>=NAV_MODES) return 0;
    u32 (*stock)(u32)=CALL(WORD(NAV_TABLE+8*mode),u32,u32);
    enum page page=current_page();
    u32 layout=menu_orientation();
    if (!is_portrait(layout) || direction>=4 || (WORD(DISPLAY_OUTPUTS)&7u)!=1)
        return stock(direction);
    if (page==SETTING) {
        static const unsigned char rotated[2][4]={{3,2,0,1},{2,3,1,0}};
        return stock(rotated[layout-1][direction]);
    }
    if (page!=GRID || BYTE(I_MENU_STATE)!=0 || mode!=0 || WORD(NAV_COUNT)!=12)
        return stock(direction);
    u32 current=CALL(0x10601e77,u32,void)();
    if (current>=12) return stock(direction);

    /* Stock direction order is up, down, right, left. */
    int x=(int)(current%4), y=(int)(current/4);
    int sign=layout==2 ? 1 : -1;
    int dx=direction==0 ? sign : direction==1 ? -sign : 0;
    int dy=direction==2 ? sign : direction==3 ? -sign : 0;
    u32 target;
    for (;;) {
        x=(x+dx+4)%4;
        y=(y+dy+3)%3;
        target=(u32)(y*4+x);
        if (target==current) break;
        u32 item[2];
        CALL(0x1060262b,void,u32 *,u32)(item,target);
        if (!item[0]) break;
    }
    WORD(NAV_INDEX)=target;
    CALL(0x1060263b,void,u32)(target);
#if Z72_PORTRAIT_TRACE
    ++portrait_counts[layout+3];
#endif
    return 0;
}

#define Z72_PORTRAIT_HISTOGRAM 1
#include "histogram_horizon_ram.c"

enum { PLOT_X=212, PLOT_Y=474, PLOT_W=200, PLOT_H=101 };
static u32 last_plot_orientation;

static u32 histogram_lcd_ready(void)
{
    if ((WORD(DISPLAY_OUTPUTS)&7u)!=LCD_OUTPUT || BYTE(DISPLAY_SCENE)!=LCD_SCENE ||
        !WORD(0xb181d384u)) return 0;
    const unsigned short *size=CALL(0x10635147u,const unsigned short *,u32)(0);
    return size[0]==LCD_W && size[1]==LCD_H;
}

static u32 portrait_histogram_orientation(void)
{
    if (!rotation_enabled() || (WORD(DISPLAY_OUTPUTS)&7u)!=LCD_OUTPUT || current_page()!=LIVE) return 0;
    u32 orientation=CALL(ORIENTATION_GET,u32,void)();
    return is_portrait(orientation) && histogram_lcd_ready() ? orientation : 0;
}

static void histogram_rectangle(struct box box,u32 orientation,u32 color)
{
    struct box dest=lcd_box(box,orientation);
    short rect[4]={dest.x,dest.y,dest.x+dest.w,dest.y+dest.h};
    CALL(0x10528f2du,void,const short *,u32,u32)(rect,color,0);
}

static void clear_portrait_histogram(u32 orientation)
{
    if (!last_plot_orientation || last_plot_orientation==orientation ||
        !histogram_lcd_ready() || display_work_pending()) return;
    if (current_page()==LIVE) {
        histogram_rectangle((struct box){PLOT_X,PLOT_Y,PLOT_W,PLOT_H},last_plot_orientation,0x2ed);
        CALL(0x10637aa9u,void,u32)(0);
        CALL(0x10554cf9u,void,void)();
    }
    last_plot_orientation=0;
}

__attribute__((target("hwdiv")))
static u32 draw_portrait_histogram(u32 orientation)
{
#ifdef Z72_CUSTOM_DISP
    if (disp_custom_live() && !(disp_components()&Z72_DISP_HISTOGRAM)) return 0;
#endif
    u32 scale=WORD(HISTOGRAM_STATE+0x34);
    if (!scale || !BYTE(HISTOGRAM_STATE+0x10) || portrait_histogram_orientation()!=orientation)
        return 0;
    histogram_rectangle((struct box){PLOT_X,PLOT_Y,PLOT_W,PLOT_H},orientation,0x2f4);
    for (int x=50;x<PLOT_W;x+=50)
        histogram_rectangle((struct box){PLOT_X+x,PLOT_Y,2,PLOT_H},orientation,0x2da);

    /* The native LCD graph groups its 256 input bins into 100 columns. */
    u32 index=0;
    for (int column=0;column<PLOT_W/2;++column) {
        u32 count=2+(!(column&1) || column%9==0);
        u32 peak=0;
        for (u32 i=0;i<count;++i) {
            u32 bin=WORD(HISTOGRAM_BINS+4*index++);
            if (bin>peak) peak=bin;
        }
        u32 divisor=scale;
        if (divisor>0xffffffffu/PLOT_H) { divisor>>=7; peak>>=7; }
        int height=peak>=divisor ? PLOT_H : (int)(peak*PLOT_H/divisor);
        if (height)
            histogram_rectangle((struct box){PLOT_X+2*column,PLOT_Y+PLOT_H-height,2,height},orientation,1);
    }
    histogram_rectangle((struct box){PLOT_X,PLOT_Y,PLOT_W,1},orientation,0x2d9);
    histogram_rectangle((struct box){PLOT_X,PLOT_Y+PLOT_H-1,PLOT_W,1},orientation,0x2d9);
    histogram_rectangle((struct box){PLOT_X,PLOT_Y,1,PLOT_H},orientation,0x2d9);
    histogram_rectangle((struct box){PLOT_X+PLOT_W-1,PLOT_Y,1,PLOT_H},orientation,0x2d9);
    CALL(0x10637aa9u,void,u32)(0);
    last_plot_orientation=orientation;
    return 1;
}

static u32 live_update(u32 a,u32 b,u32 c,u32 d)
{
#ifdef Z72_CUSTOM_DISP
    disp_update();
    if (BYTE(APP_CURRENT)==15) {
        live_orientation=0;
        last_plot_orientation=0;
#ifdef Z72_PHOTO_ZEBRA
        z72_photo_zebra_update();
#endif
        return 0; /* The native parameter app now owns the GUI update. */
    }
#endif
    if (current_page()!=LIVE) {
        live_orientation=0;
        last_plot_orientation=0;
        u32 result=CALL(LIVE_UPDATE_STOCK,u32,u32,u32,u32,u32)(a,b,c,d);
#ifdef Z72_PHOTO_ZEBRA
        z72_photo_zebra_update();
#endif
        return result;
    }
    u32 orientation=0;
    if (rotation_enabled() && (WORD(DISPLAY_OUTPUTS)&7u)==LCD_OUTPUT) {
        u32 value=CALL(ORIENTATION_GET,u32,void)();
        if (is_portrait(value)) orientation=value;
    }
    if (orientation!=live_orientation) {
        if (!CALL(0x106355a1,u32,void)()) {
            /* The stock GUI invalidation refreshes unchanged widgets on the next pass. */
            live_orientation=orientation;
            CALL(0x10554cf9,void,void)();
        }
    }
    return gui_update(a,b,c,d);
}

static u32 portrait_ready(void)
{
    if (WORD(0x11901f2cu)!=SHOOTING_MODE || WORD(APP_ROOTS)!=LIVE_APP ||
        WORD(LIVE_UPDATE)!=LIVE_UPDATE_STOCK || WORD(TOUCH_SLOT)!=TOUCH_STOCK ||
        WORD(I_MENU_APP)!=APP_SWITCH_STOCK || WORD(PICTURE_APP+0x14)!=PICTURE_KEY ||
        WORD(COLOR_APP+0x14)!=COLOR_KEY || WORD(COLOR_DETAIL_APP+0x14)!=COLOR_DETAIL_KEY) return 0;
    for (u32 mode=0;mode<NAV_MODES;++mode)
        if (WORD(NAV_SLOT+8*mode)!=WORD(NAV_TABLE+8*mode)) return 0;
    return 1;
}

enum {
    /* Kind 2 slot zero is empty on C 1.70. */
    ROTATION_PAGE_ID=0, ROTATION_PAGE_WORDS=11, ROTATION_EVENTS=16,
    ROTATION_ENABLE_ID=0x2fe0u, ROTATION_TITLE_ID=0x0a25u,
    ROTATION_TITLE_WIDGET=2
};
#define ROTATION_PAGE 0xb2bb720cu
#define SETUP_PAGE 0xb2bbc6c8u

static struct {
    u32 header[6], title[24], rows[2][7], maps[2];
    u32 events[2][ROTATION_EVENTS][4], callbacks[5];
} rotation_menu;
#ifdef Z72_PHOTO_ZEBRA
static u32 choice_localized;
static u32 open_rotation_choice(void);
#endif

static u32 rotation_value(u32 enabled)
{
    return enabled ? 0xd2u : 0xd1u; /* Plain stock ON and OFF indicators. */
}

static void rotation_page_init(const u32 *page)
{
    {
        u32 title=CALL(0x10634623u,u32,u32,u32,u32)(page[9],0,ROTATION_TITLE_WIDGET);
        if (title && WORD(title)==ROTATION_TITLE_ID) {
            CALL(0x106361c9u,void,u32,u32,u32)(title,0,0);
            if (rotation_menu.title[0])
                CALL(0x106361c9u,void,u32,u32,u32)(title,rotation_menu.title[0],1);
            else {
                u32 slot=Z72_MENU_SLOT;
                CALL(0x106361c9u,void,u32,u32,u32)(title,slot,2);
            }
        }
    }
    u32 id=ROTATION_ENABLE_ID+!rotation_enabled();
    if (CALL(0x1063bf77u,u32,void)()!=1)
        CALL(0x10638983u,void,const u32 *,u32)(page,id);
    CALL(0x1063ac0du,void,const u32 *,u32,u32)(page,id,1);
}

static u32 open_rotation_menu(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
#ifdef Z72_PHOTO_ZEBRA
    return open_rotation_choice();
#else
    if (WORD(ROTATION_PAGE)==2 && WORD(ROTATION_PAGE+0x28)==(u32)rotation_menu.header)
        CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
#endif
}

static u32 apply_rotation(u32 enabled)
{
    if (enabled==rotation_enabled() && !z72_config_pending) return 0;
    __atomic_store_n(&auto_rotate,enabled,__ATOMIC_RELEASE);
    u32 row=0;
    struct z72_config config=z72_config_current;
    config.auto_rotate=enabled;
    u32 value=rotation_value(enabled);
    menu.rows[row][5]=value;
    /* The parent can retain its widgets until the native transition back. */
    u32 widgets=WORD(SETUP_PAGE+0x24);
    if (widgets && CALL(0x1063464fu,u32,u32,u32,u32)(widgets,0,row)) {
        u32 status=CALL(0x1063466du,u32,u32,u32,u32)(widgets,0,row);
        if (status) CALL(0x106361cdu,void,u32,u32)(status,value);
    }
    z72_config_save(config);
    return 0;
}

static u32 enable_rotation(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return apply_rotation(1);
}

static u32 disable_rotation(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return apply_rotation(0);
}

#ifdef Z72_PHOTO_ZEBRA
#ifdef Z72_CUSTOM_DISP
static u32 disp_header_owned(u32 header);
#endif
#include "photo_zebra_menu.h"
#endif
#ifdef Z72_CUSTOM_DISP
#include "disp_menu.h"
#endif

static u32 portrait_install_menu(void)
{
    volatile u32 *page=(volatile u32 *)ROTATION_PAGE;
    const u32 *stock_header=(const u32 *)0xb18545fcu;
    const u32 *stock_callbacks=(const u32 *)0x10969c14u;
    if (CALL(0x1061daafu,u32,u32,u32)(2,ROTATION_PAGE_ID)!=(u32)page ||
        WORD(SETUP_PAGE)!=0x01ed0002 || WORD(SETUP_PAGE+0x28)!=Z72_SETUP_MENU ||
        stock_header[0]!=0x9008 || stock_header[1]!=0x1187c500 ||
        stock_header[2]!=0x117ccfe8 || stock_header[3]!=0x117d4f34 ||
        stock_header[4]!=0x118111bc || stock_header[5]!=0x20002) return 1;
    for (u32 i=0;i<ROTATION_PAGE_WORDS;++i)
        if (page[i]) return 1;

    /* Copy the Airplane mode layout and native navigation. All setting
     * callbacks and help links belong to this independent page. */
    const u32 *stock_title=(const u32 *)stock_header[2];
    const u32 *stock_rows=(const u32 *)stock_header[3];
    const u32 *stock_maps=(const u32 *)stock_header[4];
    const z72_menu_callback actions[]={enable_rotation,disable_rotation};
    for (u32 row=0;row<2;++row) {
        const u32 *events=(const u32 *)stock_maps[row];
        if (events[(ROTATION_EVENTS-1)*4]!=0x31a) return 1;
        for (u32 col=0;col<7;++col) rotation_menu.rows[row][col]=stock_rows[row*7+col];
        rotation_menu.rows[row][1]=rotation_menu.rows[row][2]=0xffff;
        rotation_menu.rows[row][6]=ROTATION_ENABLE_ID+row;
        for (u32 i=0;i<ROTATION_EVENTS;++i) {
            const u32 *event=events+i*4;
            u32 *entry=rotation_menu.events[row][i];
            for (u32 col=0;col<4;++col) entry[col]=event[col];
            if (event[0]==0x257 || event[0]==0x25c || event[0]==0x27d)
                entry[1]=(u32)actions[row];
            if (event[0]==0x252 || event[0]==0x27f) {
                entry[1]=entry[2]=0;
                entry[3]=0x430;
            }
        }
        rotation_menu.maps[row]=(u32)rotation_menu.events[row];
    }
    for (u32 i=0;i<24;++i) rotation_menu.title[i]=stock_title[i];
    rotation_menu.title[0]=0;
    rotation_menu.header[0]=stock_header[0];
    rotation_menu.header[1]=stock_header[1];
    rotation_menu.header[2]=(u32)rotation_menu.title;
    rotation_menu.header[3]=(u32)rotation_menu.rows;
    rotation_menu.header[4]=(u32)rotation_menu.maps;
    rotation_menu.header[5]=stock_header[5];
    for (u32 i=1;i<5;++i) rotation_menu.callbacks[i]=stock_callbacks[i];
    rotation_menu.callbacks[0]=(u32)rotation_page_init;

    z72_config_current=z72_config_load();
    auto_rotate=z72_config_current.auto_rotate;
    const struct z72_menu_row rows[]={
        {"Auto rotate info",open_rotation_menu,rotation_value(auto_rotate)},
#ifdef Z72_PHOTO_ZEBRA
        {"Photo shooting highlights",open_zebra_menu,rotation_value(z72_config_current.photo_zebra_pattern)},
#endif
#ifdef Z72_CUSTOM_DISP
        {"Custom DISP pages",open_disp_menu,rotation_value(z72_config_current.disp_enabled&Z72_DISP_CUSTOM)},
#endif
#if Z72_PORTRAIT_TRACE
        {"Portrait probe: export log",export_probe,0xd7},
#endif
    };
    const u32 count=sizeof(rows)/sizeof(rows[0]);
    if (z72_menu_prepare_rows(&menu,rows,count)) return 1;
    if (z72_menu_locale_install()) {
        menu.rows[0][0]=Z72_TEXT_AUTO_ROTATE;
        menu.rows[0][4]=0x1001;
#ifdef Z72_PHOTO_ZEBRA
        menu.rows[1][0]=Z72_TEXT_PHOTO_ZEBRA;
        menu.rows[1][4]=0x1001;
#endif
        rotation_menu.title[0]=Z72_TEXT_AUTO_ROTATE;
#ifdef Z72_CUSTOM_DISP
        menu.rows[2][0]=Z72_TEXT_DISP_TITLE;
        menu.rows[2][4]=0x1001;
#endif
    }
#ifdef Z72_PHOTO_ZEBRA
    choice_localized=rotation_menu.title[0]!=0;
    zebra_menu_prepare();
#endif
#ifdef Z72_CUSTOM_DISP
    disp_menu_prepare();
#endif
    z72_menu_publish_rows(&menu,count);
    page[1]=WORD(SETUP_PAGE+4);
    page[4]=(u32)rotation_menu.callbacks;
    page[10]=(u32)rotation_menu.header;
    __asm__ volatile("dmb sy" ::: "memory");
    page[0]=2;
    return 0;
}

static void portrait_install(void)
{
    for (u32 mode=0;mode<NAV_MODES;++mode) WORD(NAV_SLOT+8*mode)=(u32)portrait_nav;
    WORD(LIVE_UPDATE)=(u32)live_update;
    WORD(TOUCH_SLOT)=(u32)portrait_touch;
    WORD(PICTURE_APP+0x14)=(u32)picture_key;
    WORD(COLOR_APP+0x14)=(u32)color_key;
    WORD(COLOR_DETAIL_APP+0x14)=(u32)color_detail_key;
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(I_MENU_APP)=(u32)menu_switch;
}

#if Z72_PORTRAIT_TRACE
static void portrait_log(u32 *log)
{
    for (u32 i=0;i<10;++i) log[i]=portrait_counts[i];
}
#endif

#ifndef Z72_GRID_WITH_PORTRAIT
z72_u32 z72_main(const struct z72_api *api)
{
    if (!histogram_horizon_ready(api)) return 1;
    return portrait_init(api);
}
#endif
