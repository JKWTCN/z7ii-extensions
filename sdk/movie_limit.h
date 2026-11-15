/* SPDX-License-Identifier: MIT */
#ifndef Z72_MOVIE_LIMIT_H
#define Z72_MOVIE_LIMIT_H
#include "z72.h"

#define ML_WORD(a) (*(volatile z72_u32 *)(a))
#define ML_BYTE(a) (*(volatile unsigned char *)(a))
#define ML_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
#define ML_STOCK_MS 1799000u
/* The native alternative duration policy's existing 99:59:59 ceiling. */
#define ML_MAX_MS 359999000u
#define ML_UI_SLOT 0xb2982114u
#define ML_UI_NATIVE 0x104584cfu
#define ML_RECORD_SLOT 0xb1f5926cu
#define ML_RECORD_NATIVE 0x104a5aa3u


static z72_u32 movie_limit_normal(z72_u32 config)
{
    z72_u32 index=14u*ML_BYTE(config+4)+2u*ML_BYTE(config+5)+ML_BYTE(config+12);
    return !ML_BYTE(0xb188abd2u) && ML_WORD(config)==1 && index<32 &&
        ML_WORD(config+8)!=1 && ML_WORD(config+16)!=1;
}

/* Equivalent to the selected-volume branch of 104757A4 for ordinary movies,
 * retaining its reservations, FAT32/file-count limits and native bitrate
 * conversion. Do not use its alternative policy directly: that would also
 * skip the original selected-volume 4 GiB constraint. */
static z72_u32 movie_limit_capacity(z72_u32 config)
{
    z72_u32 selected=ML_BYTE(config+0x88),volumes=config+0x40;
    if (!movie_limit_normal(config) || selected>1 || !ML_WORD(volumes)) return 0;
    z72_u32 volume=volumes+32u*selected;
    z72_u32 reserve=ML_WORD(config+0x94),files=ML_WORD(config+0xcc);
    if (ML_BYTE(volume+8)!=5 || ML_WORD(volume+0x20)<files+ML_WORD(config+0x90)) return 0;
    z72_u32 lo=ML_WORD(volume+0x10),hi=ML_WORD(volume+0x14);
    if (!hi && lo<=reserve) return 0;
    hi-=lo<reserve;
    lo-=reserve;
    if (!ML_BYTE(config+4) && ML_BYTE(config+6)<=1 && !ML_BYTE(volume+0x1c) && hi) {
        lo=0xffffffffu;
        hi=0;
    }
    z72_u32 request[8]={0,config,lo,hi,ML_WORD(volume+0x18),files,0,0};
    z72_u32 duration=ML_CALL(0x1048fcfdu,z72_u32,z72_u32)((z72_u32)request);
    return duration>ML_MAX_MS ? ML_MAX_MS : duration;
}

static void movie_limit_record(z72_u32 events)
{
    /* 10486614 has already validated the native short duration. The queued
     * writer/encoder configuration is private to this task until its native
     * start handler copies it onwards. Never clear stop or error events. */
    const z72_u32 config=0xb1f598c8u,parent=0xb1f59078u;
    z72_u32 extended=0;
    if ((events&1u) && !ML_BYTE(0xb1811798u) && !ML_BYTE(config+0x58) &&
        !ML_BYTE(config+0x64) && ML_BYTE(config+0x67)<7 &&
        ML_WORD(config+0x5c)==ML_STOCK_MS && !ML_BYTE(parent+0x64) &&
        !ML_BYTE(parent+0x58) && ML_WORD(parent+0x5c)==ML_STOCK_MS) {
        z72_u32 duration=movie_limit_capacity(0xb2982518u);
        if (duration>ML_STOCK_MS) {
            ML_WORD(config+0x5c)=extended=duration;
            /* Ordinary mode has +64=0 (10490060). The parent next copies
             * its own packet into the producer (1048673E), not this writer
             * copy. Update both before the native handler acknowledges start
             * and can wake that parent; otherwise its countdown/stop stays
             * at 29:59 even though the writer received the extended value. */
            ML_WORD(parent+0x5c)=duration;
        }
    }
    ML_CALL(ML_RECORD_NATIVE,void,z72_u32)(events);
    /* 4K 60/50p also has a native chunk-count ceiling (3596/3749).
     * Its initializer clamps that budget independently of the millisecond
     * field. Extend it after initialization, before this task handles frames. */
    z72_u32 budget=ML_WORD(0xb18117dcu);
    if (extended && ML_WORD(config+0x70)==1 && ML_BYTE(0xb1811798u)==1 &&
        (budget==3596 || budget==3749)) {
        const z72_u32 format=0xb1f59958u;
        z72_u32 numerator=ML_WORD(format+4),denominator=ML_WORD(format+8);
        z72_u32 chunk=*(volatile unsigned short *)(format+0x12);
        if (numerator && denominator && chunk) {
            typedef unsigned long long u64;
            u64 divisor=(u64)denominator*chunk*1000u;
            u64 dividend=(u64)(extended+1000u)*numerator+divisor-1;
            u64 frames=ML_CALL(0x10436e40u,u64,u64,u64)(dividend,divisor);
            if (frames>budget && frames<=0xffffffffu) ML_WORD(0xb18117dcu)=(z72_u32)frames;
        }
    }
}

static void movie_limit_ui(z72_u32 events)
{
    ML_CALL(ML_UI_NATIVE,void,z72_u32)(events);
    if (ML_BYTE(0xb188abd2u)) return;
    ML_CALL(0x10360f19u,void,z72_u32)(0xb188abd8u);
    const z72_u32 configs[2]={0xb2982160u,0xb2982518u};
    z72_u32 changed=0;
    for (z72_u32 i=0;i<2;++i) {
        z72_u32 config=configs[i];
        if (ML_WORD(config+0xd0)!=ML_STOCK_MS) continue;
        z72_u32 duration=movie_limit_capacity(config);
        if (duration<=ML_STOCK_MS) continue;
        ML_WORD(config+0xd0)=duration;
        changed|=1u<<i;
    }
    /* Publish total-time changes during recording too. The original start
     * path already published 29:59; changing its source RAM without this
     * message leaves the display subtracting elapsed time from that old total. */
    if (changed&1u)
        ML_CALL(0x104572f5u,void,z72_u32)(configs[0]);
    ML_CALL(0x10360f13u,void,z72_u32)(0xb188abd8u);
    if ((changed&2u) && ML_BYTE(0xb188abd1u))
        ML_CALL(0x10457c27u,void,void)();
}

static z72_u32 movie_limit_ready(void)
{
    return ML_WORD(ML_UI_SLOT)==ML_UI_NATIVE && ML_WORD(ML_RECORD_SLOT)==ML_RECORD_NATIVE;
}

static void movie_limit_install(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
    ML_WORD(ML_RECORD_SLOT)=(z72_u32)movie_limit_record;
    ML_WORD(ML_UI_SLOT)=(z72_u32)movie_limit_ui;
}
#endif
