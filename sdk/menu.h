/* SPDX-License-Identifier: MIT */
#ifndef Z72_MENU_H
#define Z72_MENU_H
#include "z72.h"

/* C 1.70 platform binding for owned rows prepended to the setup menu. */
#define Z72_SETUP_MENU 0xb1853e64u
#define Z72_MENU_SLOT 255u /* Grid labels use slots 250 through 254. */
#define Z72_MENU_ROW_ID 0x2fffu
#define Z72_MENU_ROWS 42u
#define Z72_MENU_EVENTS 61u
#define Z72_MENU_EXTRA_ROWS 2u
typedef z72_u32 (*z72_menu_callback)(z72_u32,z72_u32,z72_u32,z72_u32);
struct z72_menu_row {
    const char *label;
    z72_menu_callback callback;
    z72_u32 value;
};
struct z72_menu_storage {
    z72_u32 rows[Z72_MENU_ROWS+Z72_MENU_EXTRA_ROWS][7];
    z72_u32 maps[Z72_MENU_ROWS+Z72_MENU_EXTRA_ROWS];
    z72_u32 events[Z72_MENU_EXTRA_ROWS][Z72_MENU_EVENTS][4];
};

static inline void z72_menu_set_label(z72_u32 row,const char *label)
{
    ((void (*)(const char *,z72_u32,z72_u32))0x10636a07)(label,Z72_MENU_SLOT+row,0);
}

/* Storage and callbacks must remain valid until power-off. */
static inline z72_u32 z72_menu_prepare_rows(struct z72_menu_storage *storage,
                                           const struct z72_menu_row *added,z72_u32 count)
{
    volatile z72_u32 *header=(volatile z72_u32 *)Z72_SETUP_MENU;
    const z72_u32 *rows=(const z72_u32 *)0x117d2e48;
    const z72_u32 *maps=(const z72_u32 *)0x11810d08;
    const z72_u32 *events=(const z72_u32 *)0x1180b848;
    if(!count || count>Z72_MENU_EXTRA_ROWS || header[3]!=(z72_u32)rows ||
       header[4]!=(z72_u32)maps || header[5]!=0x8002a ||
       events[(Z72_MENU_EVENTS-1)*4]!=0x31a) return 1;
    for(z72_u32 row=0;row<count;++row)
        if(!*(volatile z72_u32 *)(0xb20287b4+(Z72_MENU_SLOT+row)*12+4)) return 1;
    for(z72_u32 row=0;row<Z72_MENU_ROWS;++row) {
        for(z72_u32 col=0;col<7;++col) storage->rows[row+count][col]=rows[row*7+col];
        storage->maps[row+count]=maps[row];
    }
    for(z72_u32 row=0;row<count;++row) {
        for(z72_u32 col=0;col<7;++col) storage->rows[row][col]=rows[(Z72_MENU_ROWS-1)*7+col];
        storage->rows[row][0]=Z72_MENU_SLOT+row;
        storage->rows[row][4]=0x1201; /* Native dynamic ASCII label. */
        storage->rows[row][5]=added[row].value;
        storage->rows[row][6]=Z72_MENU_ROW_ID-row;
        for(z72_u32 i=0;i<Z72_MENU_EVENTS;++i) {
            z72_u32 ev=events[i*4], fn=events[i*4+1], pred=events[i*4+2], dest=events[i*4+3];
            if(ev==0x257 || ev==0x25c || ev==0x270 || ev==0x27d) {
                fn=(z72_u32)added[row].callback; pred=0; dest=0x430;
            } else if(fn!=0x1063aea9 && fn!=0x1063aeb5 && fn!=0x1063aec1 &&
                      fn!=0x1063aec5 && fn!=0x1063aec7 && fn!=0x1063aeed) {
                fn=0; pred=0; dest=dest==0x431 ? 0x431 : 0x430;
            }
            storage->events[row][i][0]=ev;
            storage->events[row][i][1]=fn;
            storage->events[row][i][2]=pred;
            storage->events[row][i][3]=dest;
        }
        storage->maps[row]=(z72_u32)storage->events[row];
        z72_menu_set_label(row,added[row].label);
    }
    return 0;
}

static inline void z72_menu_publish_rows(struct z72_menu_storage *storage,z72_u32 count)
{
    volatile z72_u32 *header=(volatile z72_u32 *)Z72_SETUP_MENU;
    __asm__ volatile("dmb sy" ::: "memory");
    header[3]=(z72_u32)storage->rows;
    header[4]=(z72_u32)storage->maps;
    header[5]=0x8002a+count;
}

static inline z72_u32 z72_menu_install_rows(struct z72_menu_storage *storage,
                                           const struct z72_menu_row *added,z72_u32 count)
{
    z72_u32 result=z72_menu_prepare_rows(storage,added,count);
    if(!result) z72_menu_publish_rows(storage,count);
    return result;
}

static inline z72_u32 z72_menu_install(struct z72_menu_storage *storage,
                                      const char *label,z72_menu_callback callback)
{
    const struct z72_menu_row row={label,callback,0xd7};
    return z72_menu_install_rows(storage,&row,1);
}
#endif
