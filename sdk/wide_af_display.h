/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIDE_AF_DISPLAY_H
#define Z72_WIDE_AF_DISPLAY_H

/* C 1.70 focus overlay's primary drawing method. This runs after the AF
 * display task has built the stock rectangles, before emitting draw commands. */
#define WIDE_DISPLAY_RENDERER 0xb181a5d8u
#define WIDE_DISPLAY_DRAW_SLOT (*(volatile z72_u32 *)(WIDE_DISPLAY_RENDERER+4u))
#define WIDE_DISPLAY_DRAW_NATIVE 0x10767d11u

struct wide_display_rect {
    short x,y,width,height;
    z72_u32 attributes;
};

typedef z72_u32 (*wide_display_draw_fn)(z72_u32,const struct wide_display_rect *,
                                        unsigned char *,z72_u32,z72_u32);

static void wide_display_resize_axis(short *origin,short *size,int steps,int low,int high)
{
    int span=high-low;
    int expand=steps>(int)Z72_WIDE_AF_FIXED_GRID_MAX;
    if (expand && span<*size) return;
    int resized=expand ? wide_af_scaled_extent(*size,steps,span) :
        (*size*steps+(int)Z72_WIDE_AF_STOCK/2)/(int)Z72_WIDE_AF_STOCK;
    if (resized<4) resized=4; /* The native renderer draws two-pixel sides. */
    int start=*origin+(*size-resized)/2;
    *origin=expand ? wide_af_fit_start(start,low,high-resized) : start;
    *size=resized;
}

static z72_u32 wide_display_draw(z72_u32 renderer,const struct wide_display_rect *rect,
                                 unsigned char *request,z72_u32 count,z72_u32 display)
{
    wide_display_draw_fn draw=(wide_display_draw_fn)WIDE_DISPLAY_DRAW_NATIVE;
    if (renderer!=WIDE_DISPLAY_RENDERER || (z72_u32)rect!=0xb1fa3bc0u ||
        (z72_u32)request!=0xb1fa3dc8u || (display!=1 && display!=4) ||
        !((count==1 && request[0]==1) || (count==2 && request[0]==7)))
        return draw(renderer,rect,request,count,display);

    z72_u32 scale=wide_af_live_scale();
    z72_u32 width_steps=scale&0xffu,height_steps=(scale>>8)&0xffu;
    if (!z72_wide_af_valid(scale) || scale==Z72_WIDE_AF_DEFAULT_SIZE ||
        rect[0].width<4 || rect[0].height<4)
        return draw(renderer,rect,request,count,display);

    /* 0x106b8508 copies the same viewport edges used by the native position clamp. */
    const short *viewport=(const short *)(request+0x0a);
    struct wide_display_rect resized[2];
    resized[0]=rect[0];
    wide_display_resize_axis(&resized[0].x,&resized[0].width,width_steps,viewport[0],viewport[2]);
    wide_display_resize_axis(&resized[0].y,&resized[0].height,height_steps,viewport[1],viewport[3]);
    /* The second rectangle is the native subject mark. The drawing routine
     * consumes both entries synchronously to build its own command records. */
    if (count==2) resized[1]=rect[1];
    return draw(renderer,resized,request,count,display);
}

static z72_u32 wide_display_ready(void)
{
    return WIDE_DISPLAY_DRAW_SLOT==WIDE_DISPLAY_DRAW_NATIVE;
}

static z72_u32 wide_display_work_pending(void)
{
    return CALL(0x106355a1u,z72_u32,void)() ||
        (WORD(0xb181d3b0u) && (WORD(0xb181d3b4u) || CALL(0x1062e289u,z72_u32,void)()));
}

static void wide_display_install(void)
{
    WIDE_DISPLAY_DRAW_SLOT=(z72_u32)wide_display_draw;
}

#undef WIDE_DISPLAY_DRAW_NATIVE
#undef WIDE_DISPLAY_DRAW_SLOT
#undef WIDE_DISPLAY_RENDERER
#endif
