/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIDE_AF_HUD_H
#define Z72_WIDE_AF_HUD_H

#define WIDE_HUD_BYTE(address) (*(volatile unsigned char *)(address))

enum { WIDE_HUD_AF=0xb181dedcu, WIDE_HUD_CONTROL=0xb181de7cu,
       WIDE_HUD_OUTPUT=0xb181d3a4u };

static z72_u32 wide_hud_ready(void)
{
    return WORD(0x1094aa30u)==WIDE_HUD_AF && WORD(0x10949b3cu)==0x10523a87u &&
        WORD(WIDE_HUD_AF+0x2c)==WIDE_HUD_CONTROL && WIDE_HUD_BYTE(WIDE_HUD_AF+0x28)==2;
}

static z72_u32 wide_hud_layer(z72_u32 asset,z72_u32 layer)
{
    return asset==wide_icon_stock[0][layer] || asset==wide_icon_stock[1][layer] ||
        asset==wide_icon_stock[2][layer];
}

static z72_u32 wide_hud_painted_preset,wide_hud_painted_owner;

static void wide_hud_erase(z72_u32 outline)
{
    /* The native outline rectangle contains both bitmaps in every layout.
     * Its owner supplies the active LCD/EVF buffer and clipping. */
    z72_u32 backdrop[24];
    for (z72_u32 i=0;i<24;++i) backdrop[i]=WORD(outline+4*i);
    backdrop[1]=6;
    backdrop[2]=0;
    backdrop[3]=6;
    backdrop[13]=2; /* Direct RGBA. Alpha zero clears without a fill. */
    backdrop[14]=0;
    backdrop[17]=1;
    CALL(0x106eefb3u,void,z72_u32 *)(backdrop);
}

static void wide_hud_draw_layer(z72_u32 object,z72_u32 replacement)
{
    z72_u32 asset=WORD(object+8),alternate=WORD(object+0xe4);
    if (replacement) {
        WORD(object+8)=replacement;
        /* The native sprite renderer can otherwise select the alternate
         * resource at +0xe8 after our bitmap ID has been chosen. */
        WORD(object+0xe4)=0;
    }
    WORD(object+0x44)=1;
    CALL(WIDE_ICON_PAINTER,void,z72_u32)(object);
    if (replacement) {
        WORD(object+8)=asset;
        WORD(object+0xe4)=alternate;
    }
}

static void wide_hud_paint(z72_u32 object)
{
    z72_u32 preset=wide_af_live_scale() ? __atomic_load_n(&wide_active_preset,__ATOMIC_RELAXED) : 0;
    z72_u32 outline=WORD(WIDE_HUD_CONTROL+0x24),icon=WORD(WIDE_HUD_CONTROL+0x54);
    z72_u32 owner=WORD(object+0x48),painter=(z72_u32)wide_hud_paint;
    if ((!preset && (!wide_hud_painted_preset || owner!=wide_hud_painted_owner)) ||
        !outline || !icon || (object!=outline && object!=icon) || !WORD(object+0x44) ||
        WORD(outline+0x48)!=owner || WORD(icon+0x48)!=owner ||
        WORD(outline+0x4c)!=painter || WORD(icon+0x4c)!=painter ||
        !WORD(icon+0x28) || WORD(icon+0xc) || WORD(outline+0xc) ||
        (preset && (WORD(WIDE_ICON_ATLAS)!=(z72_u32)wide_icon_atlas ||
                    !wide_hud_layer(WORD(icon+8),0) || !wide_hud_layer(WORD(outline+8),1)))) {
        CALL(WIDE_ICON_PAINTER,void,z72_u32)(object);
        return;
    }

    /* Paint the overlapping layers together. A dirty callback for either
     * layer consumes the pair, even when the native list has only one dirty. */
    wide_hud_erase(outline);
    if (WORD(outline+0x28)) wide_hud_draw_layer(outline,preset ? wide_icon_id(preset,1) : 0);
    wide_hud_draw_layer(icon,preset ? wide_icon_id(preset,0) : 0);
    WORD(outline+0x44)=0;
    WORD(icon+0x44)=0;
    wide_hud_painted_preset=preset;
    wide_hud_painted_owner=owner;
}

static void wide_hud_update(z72_u32 preset,z72_u32 output_before)
{
    z72_u32 output=WORD(WIDE_HUD_OUTPUT)&7u,scene=WIDE_HUD_BYTE(0xb181da91u);
    if (output!=output_before || !((output==1 && !scene) || (output==4 && scene==2)) ||
        WORD(0xb25519b8u) || WIDE_HUD_BYTE(0xb1821b18u) || WORD(0xb202f728u)!=0xb1821b1cu ||
        WIDE_HUD_BYTE(0xb202be70u) || HALF(0xb202c016u) || WORD(0xb181dabcu)) return;
    z72_u32 menu=WIDE_HUD_BYTE(0xb1824f0cu),playback=WIDE_HUD_BYTE(0xb1828174u);
    if ((menu>=1 && menu<=3) || (playback>=1 && playback<=3)) return;

    z72_u32 outline=WORD(WIDE_HUD_CONTROL+0x24),icon=WORD(WIDE_HUD_CONTROL+0x54);
    if (!outline || !icon) return;
    z72_u32 owner=output==1 ? 0xb202ce28u : 0xb202cfb8u;
    if (WORD(outline+0x48)!=owner || WORD(icon+0x48)!=owner) return;
    z72_u32 painter=(z72_u32)wide_hud_paint;
    z72_u32 foreground_painter=WORD(icon+0x4c),outline_painter=WORD(outline+0x4c);
    if (!WORD(icon+0x28) || WORD(WIDE_ICON_ATLAS)!=(z72_u32)wide_icon_atlas) return;
    if (foreground_painter!=painter || outline_painter!=painter) {
        if (!preset || WORD(icon+0xc) || WORD(outline+0xc) ||
            !wide_hud_layer(WORD(icon+8),0) || !wide_hud_layer(WORD(outline+8),1) ||
            (foreground_painter!=painter && foreground_painter!=WIDE_ICON_PAINTER) ||
            (outline_painter!=painter && outline_painter!=WIDE_ICON_PAINTER)) return;
    } else if (wide_hud_painted_preset==preset && wide_hud_painted_owner==owner) return;

    WORD(icon+0x4c)=painter;
    WORD(outline+0x4c)=painter;
    WORD(icon+0x44)=1;
    WORD(outline+0x44)=1;
    WORD(owner+0x44)=1;
    if (!wide_display_work_pending() && CALL(0x1052336bu,z72_u32,void)() &&
        (WORD(WIDE_HUD_OUTPUT)&7u)==output && CALL(0x1052273fu,z72_u32,void)() &&
        !wide_display_work_pending())
        CALL(0x105218bbu,void,void)();
}

#undef WIDE_HUD_BYTE
#endif
