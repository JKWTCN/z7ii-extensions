/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIDE_AF_GEOMETRY_H
#define Z72_WIDE_AF_GEOMETRY_H

/* C 1.70 AF profile banks and the cache copied into each new AF request. */
enum {
    WIDE_GEOMETRY_BANKS=2,
    WIDE_GEOMETRY_MODES=12,
    WIDE_GEOMETRY_PROFILES=9,
    WIDE_GEOMETRY_FIRST=4*WIDE_GEOMETRY_PROFILES,
    WIDE_GEOMETRY_COUNT=WIDE_GEOMETRY_MODES*WIDE_GEOMETRY_PROFILES,
    WIDE_GEOMETRY_FRAME=0x118da648u,
    WIDE_GEOMETRY_SOURCE_A=0x118da694u,
    WIDE_GEOMETRY_SOURCE_B=0x118db2acu,
    WIDE_GEOMETRY_SLOT_A=0xb1866db0u,
    WIDE_GEOMETRY_SLOT_B=0xb1866db8u,
    WIDE_GEOMETRY_LIVE=0xb23cda64u,
    WIDE_GEOMETRY_CACHE=0xb23cce18u,
    WIDE_GEOMETRY_PROFILE=0xb1866d57u,
    WIDE_GEOMETRY_MODE=0xb1866d5du,
    WIDE_GEOMETRY_LOCK=0xb1866d68u
};

struct wide_geometry_rect {
    unsigned short left,top,right,bottom;
};

struct wide_geometry_row {
    struct wide_geometry_rect limits;
    unsigned short width,height,pad_x,pad_y,pitch_x,pitch_y;
    z72_u32 columns,rows;
};

_Static_assert(sizeof(struct wide_geometry_row)==28,"C 1.70 AF profile row layout");

static const struct wide_geometry_row *const wide_geometry_stock[]={
    (const struct wide_geometry_row *)WIDE_GEOMETRY_SOURCE_A,
    (const struct wide_geometry_row *)WIDE_GEOMETRY_SOURCE_B
};
static const z72_u32 wide_geometry_slots[]={WIDE_GEOMETRY_SLOT_A,WIDE_GEOMETRY_SLOT_B};
static struct wide_geometry_row wide_geometry_shadow[2][WIDE_GEOMETRY_BANKS][WIDE_GEOMETRY_COUNT];
static z72_u32 wide_geometry_published;

static z72_u32 wide_geometry_row_valid(const struct wide_geometry_row *row)
{
    return row->width && row->height && row->pitch_x && row->pitch_y &&
           row->columns && row->rows && row->limits.left<row->limits.right &&
           row->limits.top<row->limits.bottom;
}

static int wide_af_scaled_extent(int stock,int steps,int extent)
{
    if (steps<=(int)Z72_WIDE_AF_FIXED_GRID_MAX)
        return stock*steps/(int)Z72_WIDE_AF_STOCK;
    int base=stock*(int)Z72_WIDE_AF_FIXED_GRID_MAX/(int)Z72_WIDE_AF_STOCK;
    if (base>=extent) return extent;
    return base+(extent-base)*(steps-(int)Z72_WIDE_AF_FIXED_GRID_MAX)/
        (int)(Z72_WIDE_AF_MAX-Z72_WIDE_AF_FIXED_GRID_MAX);
}

static int wide_af_fit_start(int start,int low,int high)
{
    if (start<low) return low;
    return start>high ? high : start;
}

static void wide_geometry_scale_axis(int steps,int extent,unsigned short *low,
                                     unsigned short *high,unsigned short *size,
                                     unsigned short *pitch,z72_u32 *count)
{
    int resized=wide_af_scaled_extent(*size,steps,extent);
    int start=*low+*size/2-resized/2;
    int end=*high+resized-*size+start-*low;
    if (steps>(int)Z72_WIDE_AF_FIXED_GRID_MAX) {
        int available=extent-resized;
        int intervals=(int)*count-1;
        /* Native requests derive each origin from the first origin and pitch. */
        if (available<intervals) {
            *count=1;
            *pitch=1; /* The native row validity check requires a nonzero pitch. */
            intervals=0;
        } else if (intervals && *pitch*intervals>available) {
            int fitted_pitch;
            __asm__ (".arch_extension idiv\n\tudiv %0,%1,%2"
                     : "=r"(fitted_pitch) : "r"(available),"r"(intervals));
            *pitch=fitted_pitch;
        }
        int travel=*pitch*intervals;
        start=wide_af_fit_start(start,0,available-travel);
        end=start+resized+travel;
    }
    *low=start;
    *high=end;
    *size=resized;
}

static void wide_geometry_scale_row(struct wide_geometry_row *out,int width,int height)
{
    if (!wide_geometry_row_valid(out)) return;
    const unsigned short *frame=(const unsigned short *)WIDE_GEOMETRY_FRAME;
    wide_geometry_scale_axis(width,frame[0],&out->limits.left,&out->limits.right,
                             &out->width,&out->pitch_x,&out->columns);
    wide_geometry_scale_axis(height,frame[1],&out->limits.top,&out->limits.bottom,
                             &out->height,&out->pitch_y,&out->rows);
}

static z72_u32 wide_geometry_match(const volatile struct wide_geometry_row *a,
                                  const struct wide_geometry_row *b)
{
    return a->limits.left==b->limits.left && a->limits.top==b->limits.top &&
           a->limits.right==b->limits.right && a->limits.bottom==b->limits.bottom &&
           a->width==b->width && a->height==b->height && a->pad_x==b->pad_x &&
           a->pad_y==b->pad_y && a->pitch_x==b->pitch_x && a->pitch_y==b->pitch_y &&
           a->columns==b->columns && a->rows==b->rows;
}

static z72_u32 wide_geometry_ready(void)
{
    return WORD(0x103a573cu)==WIDE_GEOMETRY_FRAME && WORD(0x10354834u)==0xb23cd674u &&
           WORD(WIDE_GEOMETRY_FRAME)==(8256u|(5504u<<16)) &&
           WORD(0x10353eecu)==WIDE_GEOMETRY_CACHE &&
           wide_geometry_stock[0][WIDE_GEOMETRY_FIRST].width==2244 &&
           wide_geometry_stock[0][WIDE_GEOMETRY_FIRST].height==1503 &&
           wide_geometry_stock[1][WIDE_GEOMETRY_FIRST+4].width==2486 &&
           wide_geometry_stock[1][WIDE_GEOMETRY_FIRST+4].height==1681;
}

static void wide_geometry_update_cache(const struct wide_geometry_row *old,
                                       const struct wide_geometry_row *next)
{
    volatile struct wide_geometry_rect *live=(volatile struct wide_geometry_rect *)WIDE_GEOMETRY_CACHE;
    struct wide_geometry_rect rect=*live;
    if (*(const volatile unsigned char *)WIDE_GEOMETRY_MODE!=4 ||
        !wide_geometry_row_valid(old) || rect.right<rect.left || rect.bottom<rect.top ||
        rect.right-rect.left!=old->width || rect.bottom-rect.top!=old->height)
        return;

    rect.left=wide_af_fit_start(rect.left+old->width/2-next->width/2,
                               next->limits.left,next->limits.right-next->width);
    rect.top=wide_af_fit_start(rect.top+old->height/2-next->height/2,
                              next->limits.top,next->limits.bottom-next->height);
    rect.right=rect.left+next->width;
    rect.bottom=rect.top+next->height;
    *live=rect;
}

static void wide_geometry_update_live(volatile struct wide_geometry_row *live,
                                      const struct wide_geometry_row *next)
{
    /* Special-mode readers can inspect the table outside the AF geometry lock. */
    if (next->width<live->width) live->width=next->width;
    if (next->height<live->height) live->height=next->height;
    if (next->columns<live->columns) live->columns=next->columns;
    if (next->rows<live->rows) live->rows=next->rows;
    if (next->pitch_x<live->pitch_x) live->pitch_x=next->pitch_x;
    if (next->pitch_y<live->pitch_y) live->pitch_y=next->pitch_y;
    __asm__ volatile("dmb sy" ::: "memory");
    live->limits=next->limits;
    __asm__ volatile("dmb sy" ::: "memory");
    live->pitch_x=next->pitch_x;
    live->pitch_y=next->pitch_y;
    live->columns=next->columns;
    live->rows=next->rows;
    live->width=next->width;
    live->height=next->height;
}

static z72_u32 wide_geometry_apply(z72_u32 width,z72_u32 height)
{
    const z72_u32 scaled=width!=Z72_WIDE_AF_STOCK || height!=Z72_WIDE_AF_STOCK;
    if (!scaled && !wide_geometry_published) return 0;
    if (!WORD(WIDE_GEOMETRY_LOCK) ||
        !CALL(0x10360f2fu,z72_u32,z72_u32,z72_u32)(WIDE_GEOMETRY_LOCK,0)) return 1;

    z72_u32 result=1;
    const z72_u32 published=wide_geometry_published;
    const z72_u32 previous=published==2;
    const z72_u32 next_copy=published==1;
    volatile struct wide_geometry_row *live=(volatile struct wide_geometry_row *)WIDE_GEOMETRY_LIVE;
    const struct wide_geometry_row *before[WIDE_GEOMETRY_PROFILES];
    const struct wide_geometry_row *stock[WIDE_GEOMETRY_PROFILES];

    for (z72_u32 bank=0;bank<WIDE_GEOMETRY_BANKS;++bank) {
        z72_u32 source=WORD(wide_geometry_slots[bank]);
        if (scaled && source!=(z72_u32)wide_geometry_stock[bank] &&
            (!published || source!=(z72_u32)wide_geometry_shadow[previous][bank]))
            goto done;
    }
    for (z72_u32 p=0;p<WIDE_GEOMETRY_PROFILES;++p) {
        z72_u32 index=WIDE_GEOMETRY_FIRST+p;
        before[p]=stock[p]=0;
        for (z72_u32 bank=0;bank<WIDE_GEOMETRY_BANKS;++bank) {
            const struct wide_geometry_row *native=&wide_geometry_stock[bank][index];
            const struct wide_geometry_row *shadow=&wide_geometry_shadow[previous][bank][index];
            if (published && wide_geometry_match(&live[p],shadow)) {
                before[p]=shadow;
                stock[p]=native;
                break;
            }
            if (wide_geometry_match(&live[p],native)) {
                before[p]=stock[p]=native;
                break;
            }
        }
        if (scaled && !stock[p]) goto done;
    }

    /* The native request builder records the active profile at 0x103541ca. */
    z72_u32 active_profile=*(const volatile unsigned char *)WIDE_GEOMETRY_PROFILE;
    for (z72_u32 p=0;p<WIDE_GEOMETRY_PROFILES;++p) {
        if (!stock[p] || !wide_geometry_row_valid(stock[p])) continue;
        struct wide_geometry_row next=*stock[p];
        wide_geometry_scale_row(&next,width,height);
        if (p==active_profile) wide_geometry_update_cache(before[p],&next);
        wide_geometry_update_live(&live[p],&next);
    }
    for (z72_u32 bank=0;bank<WIDE_GEOMETRY_BANKS;++bank) {
        if (scaled) {
            /* Build away from the source pointers already published to the native copier. */
            struct wide_geometry_row *copy=wide_geometry_shadow[next_copy][bank];
            CALL(0x10360facu,void,void *,const void *,z72_u32)(
                copy,wide_geometry_stock[bank],sizeof(wide_geometry_shadow[next_copy][bank]));
            for (z72_u32 p=0;p<WIDE_GEOMETRY_PROFILES;++p) {
                z72_u32 index=WIDE_GEOMETRY_FIRST+p;
                wide_geometry_scale_row(&copy[index],width,height);
            }
            __asm__ volatile("dmb sy" ::: "memory");
            WORD(wide_geometry_slots[bank])=(z72_u32)copy;
        } else if (published &&
                   WORD(wide_geometry_slots[bank])==(z72_u32)wide_geometry_shadow[previous][bank]) {
            WORD(wide_geometry_slots[bank])=(z72_u32)wide_geometry_stock[bank];
        }
    }
    wide_geometry_published=scaled ? next_copy+1 : 0;
    result=0;

done:
    CALL(0x10360f13u,void,z72_u32)(WIDE_GEOMETRY_LOCK);
    return result;
}

static z72_u32 wide_geometry_set(z72_u32 width_steps,z72_u32 height_steps)
{
    if (width_steps<Z72_WIDE_AF_MIN || width_steps>Z72_WIDE_AF_MAX ||
        height_steps<Z72_WIDE_AF_MIN || height_steps>Z72_WIDE_AF_MAX) return 1;
    return wide_geometry_apply(width_steps,height_steps);
}

static z72_u32 wide_geometry_clear(void)
{
    return wide_geometry_apply(Z72_WIDE_AF_STOCK,Z72_WIDE_AF_STOCK);
}

#endif
