/* SPDX-License-Identifier: MIT */
#ifndef Z72_GRID_MENU_H
#define Z72_GRID_MENU_H
#include "z72.h"

/* C 1.70 native framing-grid page. This storage remains live until power-off. */
enum {
    Z72_GRID_OFF = 0,
    Z72_GRID_FOUR = 1,
    Z72_GRID_THIRDS = 2,
    Z72_GRID_GOLDEN = 3,
    Z72_GRID_TWO = 4
};

/* Provided by the module: return zero only after applying the requested style. */
extern volatile z72_u32 selected_style;
extern z72_u32 grid_apply(z72_u32 style);

#ifdef Z72_GRID_2X2
#define Z72_GRID_STYLE_MAX Z72_GRID_TWO
#define Z72_GRID_MENU_ROWS 5u
#define Z72_GRID_MENU_LAYOUT 0x1187c540u
static const unsigned char z72_grid_style_order[Z72_GRID_MENU_ROWS]={
    Z72_GRID_OFF,Z72_GRID_TWO,Z72_GRID_THIRDS,Z72_GRID_FOUR,Z72_GRID_GOLDEN
};
#else
#define Z72_GRID_STYLE_MAX Z72_GRID_GOLDEN
#define Z72_GRID_MENU_ROWS 4u
#define Z72_GRID_MENU_LAYOUT 0x1187c530u
#endif
#define Z72_GRID_MENU_EVENTS 17u
#define Z72_GRID_MENU_SLOT 250u
#define Z72_GRID_MENU_ID 0x2ff8u

typedef z72_u32 (*z72_grid_menu_fn)(z72_u32,z72_u32,z72_u32,z72_u32);

static struct {
    z72_u32 rows[Z72_GRID_MENU_ROWS][7];
    z72_u32 maps[Z72_GRID_MENU_ROWS];
    z72_u32 events[Z72_GRID_MENU_ROWS][Z72_GRID_MENU_EVENTS][4];
    z72_u32 callbacks[5];
} z72_grid_menu;

static z72_u32 z72_grid_menu_pick(z72_u32 style, z72_u32 a, z72_u32 b,
                                 z72_u32 c, z72_u32 d)
{
    z72_u32 result=grid_apply(style);
    if(result) return result;
    selected_style=style;
    __asm__ volatile("dmb sy" ::: "memory");
    /* Preserve the stock property notification and persistence behavior. */
    z72_grid_menu_fn stock=(z72_grid_menu_fn)(style==Z72_GRID_OFF ? 0x106c7c69u : 0x106c7c5fu);
    return stock(a,b,c,d);
}

static z72_u32 z72_grid_menu_off(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ return z72_grid_menu_pick(Z72_GRID_OFF,a,b,c,d); }
static z72_u32 z72_grid_menu_four(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ return z72_grid_menu_pick(Z72_GRID_FOUR,a,b,c,d); }
static z72_u32 z72_grid_menu_thirds(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ return z72_grid_menu_pick(Z72_GRID_THIRDS,a,b,c,d); }
static z72_u32 z72_grid_menu_golden(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ return z72_grid_menu_pick(Z72_GRID_GOLDEN,a,b,c,d); }

#ifdef Z72_GRID_2X2
static z72_u32 z72_grid_menu_two(z72_u32 a,z72_u32 b,z72_u32 c,z72_u32 d)
{ return z72_grid_menu_pick(Z72_GRID_TWO,a,b,c,d); }
#endif

static z72_u32 z72_grid_menu_init(z72_u32 page,z72_u32 b,z72_u32 c,z72_u32 d)
{
    (void)b; (void)c; (void)d;
    z72_u32 row=selected_style;
#ifdef Z72_GRID_2X2
    z72_u32 style=row;
    for(row=0;row<Z72_GRID_MENU_ROWS && z72_grid_style_order[row]!=style;++row) {}
    if(row==Z72_GRID_MENU_ROWS) row=0;
#endif
    z72_u32 id=Z72_GRID_MENU_ID+row;
    /* Match the original initializer's cursor-restoration condition. */
    if(((z72_u32 (*)(void))0x1063bf77)()!=1)
        ((void (*)(z72_u32,z72_u32))0x10638983)(page,id);
    return ((z72_u32 (*)(z72_u32,z72_u32,z72_u32))0x1063ac0d)(page,id,1);
}

static volatile z72_u32 *const grid_menu_header=(volatile z72_u32 *)0xb18509fc;
static volatile z72_u32 *const grid_menu_page=(volatile z72_u32 *)0xb2bbb56c;
static const z72_u32 *const stock_grid_rows=(const z72_u32 *)0x116e16f4;
static const z72_u32 *const stock_grid_maps=(const z72_u32 *)0x1171e804;
static const z72_u32 *const stock_grid_callbacks=(const z72_u32 *)0x10968d64;

static z72_u32 z72_grid_menu_ready(void)
{
    if(grid_menu_page[0]!=0x01880002 || grid_menu_page[4]!=(z72_u32)stock_grid_callbacks ||
       grid_menu_page[10]!=(z72_u32)grid_menu_header || grid_menu_header[1]!=0x1187c500 ||
       grid_menu_header[3]!=(z72_u32)stock_grid_rows || grid_menu_header[4]!=(z72_u32)stock_grid_maps ||
       grid_menu_header[5]!=0x20002 || *(const z72_u32 *)Z72_GRID_MENU_LAYOUT!=Z72_GRID_MENU_ROWS ||
       stock_grid_callbacks[0]!=0x106c17a9) return 0;
    for(z72_u32 row=0;row<Z72_GRID_MENU_ROWS;++row)
        if(!*(const volatile z72_u32 *)(0xb20287b4+(Z72_GRID_MENU_SLOT+row)*12+4))
            return 0;
    for(z72_u32 row=0;row<2;++row) {
        const z72_u32 *events=(const z72_u32 *)stock_grid_maps[row];
        if(events[(Z72_GRID_MENU_EVENTS-1)*4]!=0x31a) return 0;
    }
    return 1;
}

/* Publish only after all components have passed their compatibility checks. */
static void z72_grid_menu_publish(z72_u32 golden_text)
{
    const char *labels[Z72_GRID_MENU_ROWS]={
        "Off","4x4","3x3","Golden ratio",
#ifdef Z72_GRID_2X2
        "2x2",
#endif
    };
    z72_grid_menu_fn handlers[Z72_GRID_MENU_ROWS]={
        z72_grid_menu_off,z72_grid_menu_four,z72_grid_menu_thirds,z72_grid_menu_golden,
#ifdef Z72_GRID_2X2
        z72_grid_menu_two,
#endif
    };
    for(z72_u32 row=0;row<Z72_GRID_MENU_ROWS;++row) {
        z72_u32 style=row;
#ifdef Z72_GRID_2X2
        style=z72_grid_style_order[row];
#endif
        z72_u32 stock_row=style==Z72_GRID_OFF ? 1u : 0u;
        const z72_u32 *events=(const z72_u32 *)stock_grid_maps[stock_row];
        for(z72_u32 col=0;col<7;++col)
            z72_grid_menu.rows[row][col]=stock_grid_rows[stock_row*7+col];
        if(style==Z72_GRID_GOLDEN && golden_text) {
            z72_grid_menu.rows[row][0]=golden_text;
        } else {
            z72_grid_menu.rows[row][0]=Z72_GRID_MENU_SLOT+row;
            z72_grid_menu.rows[row][4]=0x1200;
            ((void (*)(const char *,z72_u32,z72_u32))0x10636a07)(labels[style],Z72_GRID_MENU_SLOT+row,0);
        }
        z72_grid_menu.rows[row][6]=Z72_GRID_MENU_ID+row;
        for(z72_u32 ev=0;ev<Z72_GRID_MENU_EVENTS;++ev) {
            for(z72_u32 col=0;col<4;++col)
                z72_grid_menu.events[row][ev][col]=events[ev*4+col];
            if(events[ev*4]==0x257 || events[ev*4]==0x27d)
                z72_grid_menu.events[row][ev][1]=(z72_u32)handlers[style];
        }
        z72_grid_menu.maps[row]=(z72_u32)z72_grid_menu.events[row];
    }
    for(z72_u32 i=0;i<5;++i) z72_grid_menu.callbacks[i]=stock_grid_callbacks[i];
    z72_grid_menu.callbacks[0]=(z72_u32)z72_grid_menu_init;
    __asm__ volatile("dmb sy" ::: "memory");
    grid_menu_page[4]=(z72_u32)z72_grid_menu.callbacks;
    grid_menu_header[1]=Z72_GRID_MENU_LAYOUT;
    grid_menu_header[3]=(z72_u32)z72_grid_menu.rows;
    grid_menu_header[4]=(z72_u32)z72_grid_menu.maps;
    grid_menu_header[5]=(Z72_GRID_MENU_ROWS<<16)|Z72_GRID_MENU_ROWS;
}
#endif
