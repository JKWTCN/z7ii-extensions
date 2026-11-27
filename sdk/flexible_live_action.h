/* SPDX-License-Identifier: MIT */
#ifndef Z72_FLEXIBLE_LIVE_ACTION_H
#define Z72_FLEXIBLE_LIVE_ACTION_H
#include "z72.h"

/* Relocate only the validated C 1.70 online-configuration action into module
 * RAM. Original code/data remain untouched; no vendor code is embedded here.
 */
#define Z72_FLEXIBLE_ACTION_SOURCE 0x10268ba4u
#define Z72_FLEXIBLE_ACTION_SOURCE_BYTES 454u
#define Z72_FLEXIBLE_ACTION_BYTES 568u
#define Z72_FLEXIBLE_ACTION_SLOT_COUNT 5u
#define Z72_FLEXIBLE_ACTION_STOCK 0x10268ba5u

static const z72_u32 z72_flexible_action_slots[Z72_FLEXIBLE_ACTION_SLOT_COUNT]={
    0xb2bcedf4u,0xb2bcfb78u,0xb2bd08fcu,0xb2bd30d4u,0xb2bd3c28u
};

struct z72_flexible_action {
    _Alignas(64) unsigned char code[Z72_FLEXIBLE_ACTION_BYTES];
};

static inline z72_u32 z72_flexible_action_ready(void)
{
    const volatile unsigned char *source=(const volatile unsigned char *)Z72_FLEXIBLE_ACTION_SOURCE;
    z72_u32 hash=2166136261u;
    for(z72_u32 i=0;i<Z72_FLEXIBLE_ACTION_SOURCE_BYTES;++i)
        hash=(hash^source[i])*16777619u;
    const volatile unsigned short *epilogue=(const volatile unsigned short *)0x10268836u;
    if(hash!=0xb488f9f0u || epilogue[0]!=0xe8bdu || epilogue[1]!=0x83feu)
        return 0;
    for(z72_u32 i=0;i<Z72_FLEXIBLE_ACTION_SLOT_COUNT;++i)
        if(*(const volatile z72_u32 *)z72_flexible_action_slots[i]!=Z72_FLEXIBLE_ACTION_STOCK)
            return 0;
    return 1;
}

/* Same cross-core code publication sequence as the existing SD host. Kept as
 * a separate function so isolated execution can replace cache maintenance.
 */
__attribute__((noinline)) static void z72_flexible_action_sync(void *address,z72_u32 size)
{
    z72_u32 end=(z72_u32)address+size;
    for(z72_u32 p=(z72_u32)address&~31u;p<end;p+=32)
        __asm__ volatile("mcr p15, 0, %0, c7, c11, 1" :: "r"(p) : "memory");
    z72_u32 zero=0;
    __asm__ volatile("dsb sy\n mcr p15, 0, %0, c7, c1, 0\n dsb sy\n isb sy"
                     :: "r"(zero) : "memory");
}

/* callback has the stock 0x102c04c6 signature. Preparation never publishes a
 * callback slot; the caller publishes only after all initialization succeeds.
 */
static inline z72_u32 z72_flexible_action_prepare(struct z72_flexible_action *action,
                                                z72_u32 callback)
{
    static const struct {unsigned short offset; z72_u32 target;} calls[]={
        {0x036,0x103f6a71u},{0x086,0x10361117u},{0x090,0x1026aa2bu},
        {0x09a,0x103610d3u},{0x0f2,0x103f6a71u},{0x0fc,0x103f6bb3u},
        {0x114,0x10263f99u},{0x11c,0},
        {0x14c,0x102ccdcbu},{0x15a,0x1026a993u},{0x168,0x102c05efu},
        {0x178,0x1026405bu},{0x1aa,0x102c05efu},{0x1be,0x102c0531u}
    };
    if(!action || ((z72_u32)action&63u) || !(callback&1u) ||
       !z72_flexible_action_ready()) return 1;
    const volatile unsigned char *source=(const volatile unsigned char *)Z72_FLEXIBLE_ACTION_SOURCE;
    for(z72_u32 i=0;i<Z72_FLEXIBLE_ACTION_SOURCE_BYTES;++i) action->code[i]=source[i];
    unsigned short *halfwords=(unsigned short *)action->code;
    /* Replace the last B16 to a shared epilogue with its identical POP.W. */
    halfwords[226]=0xe8bd; halfwords[227]=0x83fe;
    for(z72_u32 i=0;i<sizeof(calls)/sizeof(calls[0]);++i) {
        z72_u32 offset=calls[i].offset,veneer=456u+i*8u;
        halfwords[offset/2]=0xf000;
        halfwords[offset/2+1]=(unsigned short)(0xf800u|((veneer-offset-4u)>>1));
        z72_u32 *words=(z72_u32 *)(action->code+veneer);
        words[0]=0xf000f8dfu; /* LDR.W PC,[PC,#0]; following aligned target. */
        words[1]=calls[i].target ? calls[i].target : callback;
    }
    z72_flexible_action_sync(action->code,sizeof(action->code));
    return 0;
}

static inline z72_u32 z72_flexible_action_entry(const struct z72_flexible_action *action)
{
    return (z72_u32)action->code|1u;
}

#endif
