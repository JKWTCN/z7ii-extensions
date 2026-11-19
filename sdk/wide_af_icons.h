/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIDE_AF_ICONS_H
#define Z72_WIDE_AF_ICONS_H

#include "wide_af_icon_assets.h"

enum {
    WIDE_ICON_ATLAS=0xb18107acu, WIDE_ICON_LIMIT=0xb181079cu,
    WIDE_ICON_STOCK_ATLAS=0x110595dcu, WIDE_ICON_STOCK_COUNT=0x15e4,
    WIDE_ICON_COUNT=4, WIDE_ICON_I_TABLE=0xb182a174u,
    WIDE_ICON_I_ROWS=WIDE_ICON_I_TABLE+4*20+8,
    WIDE_ICON_PAINTER=0x10650bd1u
};

static const z72_u32 wide_icon_stock[3][2]={
    {0x10b7,0x10b8}, {0x159b,0x159d}, {0x159e,0x15a0}
};

struct wide_icon_resource {
    z72_u32 flags,bytes,reserved[2];
    const unsigned char *data;
};

static struct wide_icon_resource wide_icon_atlas[WIDE_ICON_STOCK_COUNT+WIDE_ICON_COUNT];

static z72_u32 wide_icons_ready(void)
{
    /* The photo i-menu role table and its three Wide-area L variants. */
    if (WORD(0x116cb574u)!=WIDE_ICON_I_TABLE || WORD(0x116cb578u)!=10)
        return 0;
    for (z72_u32 i=0;i<3;++i)
        if (WORD(WIDE_ICON_I_ROWS+i*20)!=wide_icon_stock[i][0]) return 0;
    return 1;
}

static z72_u32 wide_icons_load(void)
{
    if (WORD(WIDE_ICON_ATLAS)==(z72_u32)wide_icon_atlas) return 1;
    if (WORD(WIDE_ICON_ATLAS)!=WIDE_ICON_STOCK_ATLAS ||
        WORD(WIDE_ICON_LIMIT)<WIDE_ICON_STOCK_COUNT+WIDE_ICON_COUNT) return 0;

    /* Register the additional masks after the native atlas has initialized.
     * Keep every stock asset ID and resource entry intact. */
    static const struct wide_icon_resource extra[WIDE_ICON_COUNT]={
        {0x100,sizeof(wide_c1_foreground),{0,0},wide_c1_foreground},
        {0x100,sizeof(wide_c1_outline),{0,0},wide_c1_outline},
        {0x100,sizeof(wide_c2_foreground),{0,0},wide_c2_foreground},
        {0x100,sizeof(wide_c2_outline),{0,0},wide_c2_outline}
    };
    typedef void (*copy_fn)(void *,const void *,z72_u32);
    copy_fn copy=(copy_fn)0x10360facu;
    copy(wide_icon_atlas,(const void *)WIDE_ICON_STOCK_ATLAS,
         WIDE_ICON_STOCK_COUNT*sizeof(wide_icon_atlas[0]));
    copy(wide_icon_atlas+WIDE_ICON_STOCK_COUNT,extra,sizeof(extra));
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(WIDE_ICON_ATLAS)=(z72_u32)wide_icon_atlas;
    return 1;
}

static z72_u32 wide_icon_id(z72_u32 preset,z72_u32 outline)
{
    return WIDE_ICON_STOCK_COUNT+2*(preset-1)+outline;
}

static void wide_icons_set(z72_u32 preset)
{
    if (preset && !wide_icons_load()) return;
    /* The root i menu consumes these rows when constructing and refreshing
     * its standard AF icon. The eleven-option picker has its own rows. */
    for (z72_u32 i=0;i<3;++i)
        WORD(WIDE_ICON_I_ROWS+i*20)=preset ? wide_icon_id(preset,0) : wide_icon_stock[i][0];
}

#endif
