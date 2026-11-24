/* SPDX-License-Identifier: MIT */
#ifndef Z72_HOST_VERSION_H
#define Z72_HOST_VERSION_H
#include "z72.h"

static inline z72_u32 z72_host_code_hash(z72_u32 size)
{
    const volatile unsigned char *code=(const volatile unsigned char *)0x12497b78u;
    z72_u32 hash=2166136261u;
    /* FNV-1a over the appended host code, excluding the member CRC. */
    for (z72_u32 i=0;i<size;++i) hash=(hash^code[i])*16777619u;
    return hash;
}

static inline const unsigned short *z72_host_version_label(void)
{
    const volatile z72_u32 *hook=(const volatile z72_u32 *)0x1044e010u;
    if (hook[0]!=0xf000f8dfu || hook[1]!=0x12497b79u)
        return u"Host: unknown";

    z72_u32 copy_size=*(const volatile z72_u32 *)0x1002c020u;
    if (copy_size==0xaedb8u) {
        if (z72_host_code_hash(1664u)==0x52f92a4du)
            return u"Host: 1.70-r1";
        if (z72_host_code_hash(1676u)==0xd67cce8bu)
            return u"Host: 1.70-r2";
    }
    if (copy_size==0xaedc0u && z72_host_code_hash(1668u)==0x53d71e97u)
        return u"Host: 1.70-r3";
    return u"Host: unknown";
}
#endif
