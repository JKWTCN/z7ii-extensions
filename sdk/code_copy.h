/* SPDX-License-Identifier: MIT */
#ifndef Z72_CODE_COPY_H
#define Z72_CODE_COPY_H
#include "z72.h"

static z72_u32 copy_code(unsigned char *destination, z72_u32 start, z72_u32 size, z72_u32 expected)
{
    const volatile unsigned char *source=(const volatile unsigned char *)start;
    z72_u32 hash=2166136261u;
    for (z72_u32 i=0;i<size;++i)
        hash=(hash^source[i])*16777619u;
    if (hash!=expected) return 1;
    for (z72_u32 i=0;i<size;++i)
        destination[i]=source[i];
    return 0;
}

static void write_stub(z72_u32 stub, z72_u32 target)
{
    *(volatile z72_u32 *)stub=0xf000f8dfu;
    *(volatile z72_u32 *)(stub+4)=target;
}

static void write_call(z72_u32 site, z72_u32 stub, z72_u32 target)
{
    z72_u32 delta=stub-site-4u;
    write_stub(stub,target);
    /* All stubs are in the positive BL range with S=0 and J1=J2=1. */
    *(volatile unsigned short *)site=(unsigned short)(0xf000u|(delta>>12&0x3ffu));
    *(volatile unsigned short *)(site+2)=(unsigned short)(0xf800u|(delta>>1&0x7ffu));
}

static void clean_code(unsigned char *bytes, z72_u32 size)
{
    for (z72_u32 p=(z72_u32)bytes;p<(z72_u32)bytes+size;p+=32)
        __asm__ volatile("mcr p15,0,%0,c7,c11,1" :: "r"(p) : "memory");
}

static void sync_code(void)
{
    z72_u32 zero=0;
    __asm__ volatile("dsb sy\n mcr p15,0,%0,c7,c1,0\n dsb sy\n isb sy"
                     :: "r"(zero) : "memory");
}
#endif
