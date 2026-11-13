/* SPDX-License-Identifier: MIT */
#ifndef Z72_AF_SHORTCUT_H
#define Z72_AF_SHORTCUT_H
#include "config.h"

#define Z72_AF_WORD(a) (*(volatile z72_u32 *)(a))
#define Z72_AF_BYTE(a) (*(volatile unsigned char *)(a))
#define Z72_AF_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
#define Z72_AF_INPUT_SLOT 0xb181d9fcu
#define Z72_AF_INPUT_NATIVE 0x1053d50fu

/* Body Fn1, Fn2, AF-ON, sub-selector center and movie-record button.
 * Zero retains the native assignment; 1..10 select the native photo AF areas. */
static const z72_u32 af_shortcut_properties[Z72_AF_SHORTCUT_BUTTONS]={
    0x1145,0x114a,0x114f,0x1153,0x1156
};
static volatile z72_u32 af_shortcut_pending;

static z72_u32 af_shortcut_context(void)
{
    z72_u32 output=Z72_AF_WORD(0xb181d3a4u)&7u;
    z72_u32 scene=Z72_AF_BYTE(0xb181da91u);
    z72_u32 menu=Z72_AF_BYTE(0xb1824f0cu),playback=Z72_AF_BYTE(0xb1828174u);
    return ((output==1 && scene==0) || (output==4 && scene==2)) && !Z72_AF_BYTE(0xb1821b18u) &&
        Z72_AF_WORD(0xb202f728u)==0xb1821b1cu && !Z72_AF_BYTE(0xb202be70u) &&
        !Z72_AF_WORD(0xb25519b8u) && !Z72_AF_WORD(0xb181dabcu) &&
        !Z72_AF_BYTE(0xb202c016u) && !Z72_AF_BYTE(0xb202c017u) &&
        !(menu>=1 && menu<=3) && !(playback>=1 && playback<=3);
}

static z72_u32 af_shortcut_input(z72_u32 packet,z72_u32 a,z72_u32 b,z72_u32 c)
{
    /* The input callback precedes the GUI task. Capture only; never change
     * properties or write the CFG here. Native assignment None still reaches
     * this callback. +10/+38/+24 are press edges, not the held-state words. */
    if (!af_shortcut_context())
        __atomic_store_n(&af_shortcut_pending,0,__ATOMIC_RELEASE);
    else if (packet) {
        static const z72_u32 offsets[Z72_AF_SHORTCUT_BUTTONS]={0x10,0x38,0x10,0x38,0x24};
        static const z72_u32 masks[Z72_AF_SHORTCUT_BUTTONS]={0x4000,0x4000,0x200000,0x8000,0x2000};
        z72_u32 pressed=0;
        for (z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i)
            if (z72_config_current.af_shortcuts[i] &&
                (Z72_AF_WORD(packet+offsets[i])&masks[i])) pressed|=1u<<i;
        if (pressed) __atomic_fetch_or(&af_shortcut_pending,pressed,__ATOMIC_RELEASE);
    }
    return Z72_AF_CALL(Z72_AF_INPUT_NATIVE,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(packet,a,b,c);
}

/* Called on the existing GUI task before its native snapshot/layout pass.
 * Rendering congestion may defer a request; leaving photo live view cancels
 * it, so a menu, playback or movie transition cannot apply a stale shortcut. */
static void af_shortcut_update(void)
{
    if (!__atomic_load_n(&af_shortcut_pending,__ATOMIC_ACQUIRE)) return;
    if (!af_shortcut_context()) {
        __atomic_store_n(&af_shortcut_pending,0,__ATOMIC_RELEASE);
        return;
    }
    if (Z72_AF_CALL(0x106355a1u,z72_u32,void)()) return;
    z72_u32 pending=__atomic_exchange_n(&af_shortcut_pending,0,__ATOMIC_ACQ_REL);
    if (Z72_AF_CALL(0x10580591u,z72_u32,z72_u32,z72_u32)(0x44,0)) return;
    for (z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i) {
        z72_u32 target=z72_config_current.af_shortcuts[i];
        if (!(pending&(1u<<i)) || !target || target>Z72_AF_SHORTCUT_TARGETS ||
            Z72_AF_CALL(0x1063c7d5u,z72_u32,z72_u32)(af_shortcut_properties[i])) continue;
        if ((target==1 && Z72_AF_CALL(0x10580591u,z72_u32,z72_u32,z72_u32)(0x45,0)) ||
            (target==3 && Z72_AF_CALL(0x10580591u,z72_u32,z72_u32,z72_u32)(0x46,0))) continue;
        static const z72_u32 areas[Z72_AF_SHORTCUT_TARGETS]={0,1,2,3,4,4,4,6,6,6};
        static const z72_u32 subtypes[Z72_AF_SHORTCUT_TARGETS]={0,0,0,0,0,1,2,0,1,2};
        z72_u32 area=areas[target-1];
        /* The area setter refreshes subtype state. Set the requested subtype
         * afterwards, and leave both properties selected after release. */
        Z72_AF_CALL(0x1063c7e1u,z72_u32,z72_u32,z72_u32)(0x056c,area);
        if (Z72_AF_CALL(0x1063c7d5u,z72_u32,z72_u32)(0x056c)!=area) return;
        if (area==4 || area==6)
            Z72_AF_CALL(0x1063c7e1u,z72_u32,z72_u32,z72_u32)(
                area==4 ? 0x058a : 0x058b,subtypes[target-1]);
        return;
    }
}

static z72_u32 af_shortcut_ready(void)
{
    return Z72_AF_WORD(Z72_AF_INPUT_SLOT)==Z72_AF_INPUT_NATIVE;
}

static void af_shortcut_install(void)
{
    __atomic_store_n(&af_shortcut_pending,0,__ATOMIC_RELEASE);
    Z72_AF_WORD(Z72_AF_INPUT_SLOT)=(z72_u32)af_shortcut_input;
}
#endif
