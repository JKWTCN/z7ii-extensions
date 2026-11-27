/* SPDX-License-Identifier: MIT */
#ifndef Z72_FLEXIBLE_PUBLISH_ACTION_H
#define Z72_FLEXIBLE_PUBLISH_ACTION_H
#include "flexible_live_action.h"

/* Copy the two validated worker callbacks into module RAM. Only calls of the
 * synchronous configuration publisher are redirected. Entry prechecks, frame
 * start, error delivery and every other native call keep their original order.
 * No vendor instruction bytes are embedded in this header.
 */
#define Z72_FLEXIBLE_WORKER_BYTES 384u
struct z72_flexible_workers {
    _Alignas(64) unsigned char code[2][Z72_FLEXIBLE_WORKER_BYTES];
};

static inline z72_u32 z72_flexible_publish_hash(z72_u32 source,z72_u32 bytes)
{
    const volatile unsigned char *p=(const volatile unsigned char *)source;
    z72_u32 hash=2166136261u;
    for(z72_u32 i=0;i<bytes;++i) hash=(hash^p[i])*16777619u;
    return hash;
}

static inline z72_u32 z72_flexible_workers_ready(void)
{
    if(z72_flexible_publish_hash(0x102e5168u,252)!=0x4bb5b5a0u ||
       z72_flexible_publish_hash(0x102e505cu,186)!=0xf6c58c72u ||
       z72_flexible_publish_hash(0x102e49feu,1372)!=0x4e0c5884u ||
       z72_flexible_publish_hash(0x103d58d0u,140)!=0x555f2eddu)
        return 0;
    /* The fingerprints above exclude the workers' out-of-line constants. */
    const volatile z72_u32 *literals=(const volatile z72_u32 *)0x102e5344u;
    return literals[0]==0xb18095c4u && literals[2]==0xb1d52a8cu &&
           literals[5]==0x42190806u && literals[6]==0xbde6f7ffu;
}

/* bank0/1 are actual native target handles, shared by workers and running
 * updates. Handle byte+4 selects the physical ISP register bank.
 */
static inline z72_u32 z72_flexible_publish_target(z72_u32 bank)
{
    if(bank>1) return 0;
    return ((const volatile z72_u32 *)0xb18095c4u)[bank+1];
}

static inline z72_u32 z72_flexible_workers_prepare(struct z72_flexible_workers *workers,
                                                  z72_u32 callback)
{
    static const struct {unsigned short offset; unsigned char kind; z72_u32 target;} calls0[]={
        {0x02e,1,0x102e4725u},{0x038,0,0x102e58b1u},{0x046,0,0x102e5127u},
        {0x052,0,0},{0x068,0,0},{0x07a,0,0},{0x08c,2,0x102e4f5bu},
        {0x0ae,0,0x102e58b1u},{0x0b6,0,0x102e5127u},{0x0c2,0,0},{0x0d2,0,0}
    },calls1[]={
        {0x02c,0,0x102e4725u},{0x034,0,0x102e5897u},{0x040,0,0},
        {0x054,0,0x102e4f5bu},{0x05a,0,0x102e5897u},{0x062,0,0x102e5897u},
        {0x072,0,0x103854cdu},{0x07c,0,0x103854d5u},{0x094,0,0}
    };
    static const struct {unsigned short offset; unsigned char reg,wide; z72_u32 source;} loads0[]={
        {0x004,5,0,0x102e5344u},{0x008,7,0,0x102e5358u},
        {0x00a,2,0,0x102e535cu},{0x08e,2,0,0x102e5344u},
        {0x09c,8,1,0x102e5358u},{0x0ee,0,0,0x102e5358u}
    },loads1[]={
        {0x004,7,0,0x102e5358u},{0x008,1,0,0x102e535cu},
        {0x012,5,0,0x102e5344u},{0x06e,0,0,0x102e534cu},
        {0x076,0,0,0x102e534cu},{0x082,2,0,0x102e5344u},
        {0x0a0,0,0,0x102e5358u},{0x0ae,0,0,0x102e5358u}
    };
    if(!workers || ((z72_u32)workers&63u) || !(callback&1u) ||
       !z72_flexible_workers_ready()) return 1;
    for(z72_u32 bank=0;bank<2;++bank) {
        unsigned char *code=workers->code[bank];
        const volatile unsigned char *source=(const volatile unsigned char *)(
            bank ? 0x102e505cu : 0x102e5168u);
        z72_u32 length=bank ? 186u : 252u;
        for(z72_u32 i=0;i<Z72_FLEXIBLE_WORKER_BYTES;++i) code[i]=0;
        for(z72_u32 i=0;i<length;++i) code[i]=source[i];
        unsigned short *halfwords=(unsigned short *)code;
        z72_u32 cursor=(length+3u)&~3u;
        const __typeof__(*calls0) *calls=bank ? calls1 : calls0;
        z72_u32 count=bank ? sizeof(calls1)/sizeof(calls1[0]) : sizeof(calls0)/sizeof(calls0[0]);
        for(z72_u32 i=0;i<count;++i) {
            z72_u32 offset=calls[i].offset,delta=cursor-offset-4u;
            if(calls[i].kind==2) halfwords[offset/2]=(unsigned short)(0xe000u|(delta>>1));
            else {
                halfwords[offset/2]=0xf000u;
                halfwords[offset/2+1]=(unsigned short)((calls[i].kind ? 0xb800u : 0xf800u)|(delta>>1));
            }
            z72_u32 *veneer=(z72_u32 *)(code+cursor);
            veneer[0]=0xf000f8dfu;
            veneer[1]=calls[i].target ? calls[i].target : callback;
            cursor+=8;
        }
        const __typeof__(*loads0) *loads=bank ? loads1 : loads0;
        count=bank ? sizeof(loads1)/sizeof(loads1[0]) : sizeof(loads0)/sizeof(loads0[0]);
        for(z72_u32 i=0;i<count;++i) {
            z72_u32 offset=loads[i].offset,delta=cursor-((offset+4u)&~3u);
            *(z72_u32 *)(code+cursor)=*(const volatile z72_u32 *)loads[i].source;
            if(loads[i].wide) {
                halfwords[offset/2]=0xf8dfu;
                halfwords[offset/2+1]=(unsigned short)((loads[i].reg<<12)|delta);
            } else halfwords[offset/2]=(unsigned short)(0x4800u|(loads[i].reg<<8)|(delta>>2));
            cursor+=4;
        }
        z72_flexible_action_sync(code,Z72_FLEXIBLE_WORKER_BYTES);
    }
    return 0;
}

static inline z72_u32 z72_flexible_workers_entry(const struct z72_flexible_workers *workers,
                                                 z72_u32 bank)
{
    return (z72_u32)workers->code[bank]|1u;
}
#endif
