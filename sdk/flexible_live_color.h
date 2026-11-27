/* SPDX-License-Identifier: MIT */
#ifndef Z72_FLEXIBLE_LIVE_COLOR_H
#define Z72_FLEXIBLE_LIVE_COLOR_H

#include "flexible_live_action.h"

#define Z72_LIVE_COLOR_STOCK 0x1026755fu
#define Z72_LIVE_COLOR_GENERATE 0x10274563u

/* The matching producer nodes precede the five live publisher nodes. These
 * are the C 1.70 table entries for ordinary live graphs. */
static const z72_u32 z72_live_color_slots[]={
    0xb2bced0cu,0xb2bcfa9cu,0xb2bd0820u,0xb2bd2fecu,0xb2bd3b40u
};

struct z72_live_color {
    _Alignas(64) unsigned char code[0x290];
};

static inline z72_u32 z72_live_color_hash(const volatile unsigned char *data,z72_u32 size)
{
    z72_u32 hash=2166136261u;
    for(z72_u32 i=0;i<size;++i)
        hash=(hash^data[i])*16777619u;
    return hash;
}

static inline z72_u32 z72_live_color_prepare(struct z72_live_color *action,z72_u32 callback)
{
    const volatile unsigned char *source=(const volatile unsigned char *)(Z72_LIVE_COLOR_STOCK-1);
    const volatile z72_u32 *pool=(const volatile z72_u32 *)0x10267a30u;
    static const unsigned short load_offsets[]={0xd6,0x230,0x234,0x238,0x23c};
    static const struct {unsigned short offset; z72_u32 target;} calls[]={
        {0x00e,0x10361035u},{0x0ac,0x103f6a71u},{0x0b6,0x103f6bb3u},
        {0x142,0x10263f67u},{0x1c2,0},{0x23e,0x102745cdu}
    };
    z72_u32 literal_count=sizeof(load_offsets)/sizeof(load_offsets[0]);
    if(z72_live_color_hash(source,0x24a)!=0xf84d9718u ||
       z72_live_color_hash((const volatile unsigned char *)pool,literal_count*sizeof(*pool))!=0xa353b775u)
        return 1;
    for(z72_u32 i=0;i<sizeof(z72_live_color_slots)/sizeof(z72_live_color_slots[0]);++i)
        if(*(const volatile z72_u32 *)z72_live_color_slots[i]!=Z72_LIVE_COLOR_STOCK)
            return 1;
    /* Preserve the stock entry's offset of two bytes from a word boundary.
     * Only calls and the five PC-relative loads outside the body move. */
    for(z72_u32 i=0;i<0x24a;++i)
        action->code[2+i]=source[i];
    unsigned short *halfwords=(unsigned short *)action->code;
    for(z72_u32 i=0;i<literal_count;++i) {
        z72_u32 offset=2u+load_offsets[i],literal=0x24cu+i*4u;
        *(z72_u32 *)(action->code+literal)=pool[i];
        halfwords[offset/2]=(halfwords[offset/2]&0xff00u)|((literal-((offset+4u)&~3u))/4u);
    }
    for(z72_u32 i=0;i<sizeof(calls)/sizeof(calls[0]);++i) {
        z72_u32 offset=2u+calls[i].offset,veneer=0x260u+i*8u;
        halfwords[offset/2]=0xf000;
        halfwords[offset/2+1]=0xf800u|((veneer-offset-4u)/2u);
        z72_u32 *words=(z72_u32 *)(action->code+veneer);
        words[0]=0xf000f8dfu;
        words[1]=calls[i].target ? calls[i].target : callback;
    }
    z72_flexible_action_sync(action->code,sizeof(action->code));
    return 0;
}

#endif
