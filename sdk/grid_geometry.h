/* SPDX-License-Identifier: MIT */
#ifndef Z72_GRID_GEOMETRY_H
#define Z72_GRID_GEOMETRY_H
#include "grid_menu.h"

/* The packet already contains the stock viewport, clipping and line color.
 * Only move its interior lines. The rectangle endpoints remain unchanged,
 * making repeated processing of a queued packet idempotent.
 */
z72_u32 grid_transform(void *descriptor,z72_u32 style)
{
    unsigned char *p=descriptor;
    unsigned short *v=descriptor;
    if(style==Z72_GRID_OFF || style>Z72_GRID_STYLE_MAX) return 0;
    if(style==Z72_GRID_FOUR && p[0x12]==3 && p[0x1e]==3) return 0;
    z72_u32 x=v[0x14/2], y=v[0x22/2];
    z72_u32 right=v[0x18/2], bottom=v[0x26/2];
    if(right<=x || bottom<=y || v[0x1a/2]<v[0x16/2] || v[0x24/2]<v[0x20/2]) return 0;
    z72_u32 width=right-x, height=bottom-y;
    z72_u32 horizontal_thickness=v[0x1a/2]-v[0x16/2];
    z72_u32 vertical_thickness=v[0x24/2]-v[0x20/2];
    if(width<8 || height<8) return 0;
    z72_u32 first_x,first_y,step_x,step_y,lines;
    if(style==Z72_GRID_FOUR) {
        first_x=((width+4)/8)*2;
        first_y=((height+4)/8)*2;
        step_x=first_x; step_y=first_y; lines=3;
    } else if(style==Z72_GRID_TWO) {
        /* Round the midpoint to the nearest even pixel, as for other styles. */
        first_x=((width+2)/4)*2;
        first_y=((height+2)/4)*2;
        step_x=first_x; step_y=first_y; lines=1;
    } else {
        if(style==Z72_GRID_THIRDS) {
            first_x=((width+3)/6)*2;
            first_y=((height+3)/6)*2;
        } else {
            /* 25033 / 65536 approximates 0.381966; round to even pixels. */
            first_x=((width*25033u+65536u)>>17)*2;
            first_y=((height*25033u+65536u)>>17)*2;
        }
        step_x=(width-2*first_x)&~1u;
        step_y=(height-2*first_y)&~1u;
        lines=2;
    }
    if(x+first_x+vertical_thickness>right || y+first_y+horizontal_thickness>bottom)
        return 0;
    v[0x16/2]=(unsigned short)(y+first_y);
    v[0x1a/2]=(unsigned short)(y+first_y+horizontal_thickness);
    v[0x20/2]=(unsigned short)(x+first_x);
    v[0x24/2]=(unsigned short)(x+first_x+vertical_thickness);
    v[0x10/2]=(unsigned short)step_y;
    v[0x1c/2]=(unsigned short)step_x;
    p[0x12]=(unsigned char)lines;
    p[0x1e]=(unsigned char)lines;
    return 1;
}

#endif
