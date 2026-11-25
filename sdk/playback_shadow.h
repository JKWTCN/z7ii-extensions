/* SPDX-License-Identifier: MIT */
#ifndef Z72_PLAYBACK_SHADOW_H
#define Z72_PLAYBACK_SHADOW_H
#include "code_copy.h"
#include "playback_shadow_native.h"

enum { PLAYBACK_TASK_SLOT=0xb29d2ec0u, PLAYBACK_TASK_STOCK=0x1063dc05u };

static _Alignas(64) unsigned char native_code[FILTER_NATIVE_BYTES+sizeof(filter_branches)/sizeof(*filter_branches)*8+8*8];
static _Alignas(4) unsigned char native_tables[FILTER_TABLE_BYTES];

static z72_u32 translate(z72_u32 address)
{
    if (address>=FILTER_NATIVE_START && address<FILTER_NATIVE_START+FILTER_NATIVE_BYTES)
        return (z72_u32)native_code+address-FILTER_NATIVE_START;
    if (address>=FILTER_TABLE_START && address<FILTER_TABLE_START+FILTER_TABLE_BYTES)
        return (z72_u32)native_tables+address-FILTER_TABLE_START;
    return address;
}

static void branch(z72_u32 site,z72_u32 target,z72_u32 link)
{
    z72_u32 delta=target-site-4,s=delta>>24&1;
    z72_u32 j1=1^((delta>>23&1)^s),j2=1^((delta>>22&1)^s);
    *(volatile unsigned short *)site=(unsigned short)(0xf000|(s<<10)|(delta>>12&0x3ff));
    *(volatile unsigned short *)(site+2)=(unsigned short)((link ? 0xd000 : 0x9000)|
        (j1<<13)|(j2<<11)|(delta>>1&0x7ff));
}

static z72_u32 playback_shadow_prepare(z72_u32 (*target_for)(z72_u32,z72_u32))
{
    if (copy_code(native_code,FILTER_NATIVE_START,FILTER_NATIVE_BYTES,FILTER_NATIVE_HASH) ||
        copy_code(native_tables,FILTER_TABLE_START,FILTER_TABLE_BYTES,FILTER_TABLE_HASH)) return 1;
    for (z72_u32 i=0;i<sizeof(filter_branches)/sizeof(*filter_branches);++i) {
        z72_u32 site=FILTER_NATIVE_START+filter_branches[i].offset;
        z72_u32 stub=(z72_u32)native_code+FILTER_NATIVE_BYTES+i*8;
        z72_u32 target=target_for(site,filter_branches[i].target);
        if (filter_branches[i].link) write_call(translate(site),stub,target);
        else {
            write_stub(stub,target);
            branch(translate(site),stub,0);
        }
    }
    for (z72_u32 i=0;i<sizeof(filter_pools)/sizeof(*filter_pools);++i) {
        z72_u32 *literal=(z72_u32 *)(native_code+filter_pools[i]);
        *literal=translate(*literal);
    }
    for (z72_u32 i=0;i<FILTER_TABLE_BYTES;i+=4) {
        z72_u32 *entry=(z72_u32 *)(native_tables+i);
        *entry=translate(*entry);
    }
    return 0;
}

static inline void playback_shadow_sync(void)
{
    clean_code(native_code,sizeof(native_code));
    sync_code();
}

#endif
