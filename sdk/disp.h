/* SPDX-License-Identifier: MIT */
#ifndef Z72_DISP_H
#define Z72_DISP_H
#include "config.h"

#define DISP_WORD(a) (*(volatile z72_u32 *)(a))
#define DISP_BYTE(a) (*(volatile unsigned char *)(a))
#define DISP_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
#define DISP_KEY_SLOT 0xb1821b30u
#define DISP_KEY_NATIVE 0x104e6c1du
#define DISP_INFORMATION_APP 0xb1823598u
#define DISP_INFORMATION_KEY 0x104ea4cfu
#define DISP_INFORMATION_UPDATE 0x104ea8e9u
#define DISP_WIDGETS 64u

static z72_u32 disp_preset_index,disp_active,disp_saved_property,disp_saved_page,disp_owned_page;
static z72_u32 disp_masks[DISP_WIDGETS],disp_operation_masks[DISP_WIDGETS],disp_mask_key=0xffffffffu;
static volatile z72_u32 disp_dirty;
static z72_u32 disp_suspended;
static z72_u32 disp_restore_pending;

/* Photo LCD only. The stock parameter display has its own app and snapshot;
 * menus, playback, movie mode and other outputs retain their native policy. */
static z72_u32 disp_photo_context(void)
{
    z72_u32 menu=DISP_BYTE(0xb1824f0cu),playback=DISP_BYTE(0xb1828174u);
    return (DISP_WORD(0xb181d3a4u)&7u)==1 && !DISP_BYTE(0xb181da91u) &&
        !DISP_WORD(0xb25519b8u) &&
        !DISP_WORD(0xb181dabcu) && !(menu>=1 && menu<=3) &&
        !(playback>=1 && playback<=3);
}

static z72_u32 disp_photo_lcd(void)
{
    return disp_photo_context() && !DISP_BYTE(0xb1821b18u) &&
        DISP_WORD(0xb202f728u)==0xb1821b1cu && !DISP_BYTE(0xb202be70u) &&
        !DISP_BYTE(0xb202c016u) && !DISP_BYTE(0xb202c017u);
}

static z72_u32 disp_information_visible(void)
{
    return disp_photo_context() && DISP_BYTE(0xb1821b18u)==15 &&
        DISP_WORD(0xb202f764u)==DISP_INFORMATION_APP;
}

static z72_u32 disp_components(void)
{
    if (disp_information_visible()) return 0;
    if (!__atomic_load_n(&disp_active,__ATOMIC_ACQUIRE) || !disp_photo_lcd() ||
        !(z72_config_current.disp_enabled&Z72_DISP_CUSTOM)) return 31;
    return disp_preset_index<Z72_DISP_PAGES ? z72_config_current.disp_components[disp_preset_index] : 0;
}

static z72_u32 disp_custom_live(void)
{
    return __atomic_load_n(&disp_active,__ATOMIC_ACQUIRE) && disp_photo_lcd() &&
           (z72_config_current.disp_enabled&Z72_DISP_CUSTOM);
}

static void disp_settings_changed(void)
{
    disp_suspended=0;
    __atomic_store_n(&disp_dirty,1,__ATOMIC_RELEASE);
}

static z72_u32 disp_next(z72_u32 start)
{
    for (z72_u32 i=1;i<=Z72_DISP_CHOICES;++i) {
        z72_u32 page=start+i;
        if (page>=Z72_DISP_CHOICES) page-=Z72_DISP_CHOICES;
        z72_u32 bit=page==Z72_DISP_PAGES ? Z72_DISP_INFORMATION : 1u<<page;
        if (z72_config_current.disp_enabled&bit) return page;
    }
    return 0; /* Configuration validation keeps at least one page enabled. */
}

static void disp_restore_page(void)
{
    if (!disp_restore_pending) return;
    z72_u32 page=DISP_CALL(0x1063c7d5u,z72_u32,z72_u32)(disp_saved_property);
    if (page!=disp_owned_page || page==disp_saved_page ||
        DISP_CALL(0x1063c7e1u,z72_u32,z72_u32,z72_u32)(disp_saved_property,disp_saved_page))
        disp_restore_pending=0;
}

static void disp_restore(void)
{
    if (!disp_active) return;
    __atomic_store_n(&disp_active,0,__ATOMIC_RELEASE);
    for (z72_u32 i=0;i<DISP_WIDGETS;++i) {
        z72_u32 descriptor=DISP_WORD(0x1094aa24u+4*i);
        DISP_WORD(descriptor+4)=disp_masks[i];
        DISP_WORD(descriptor+0x10)=disp_operation_masks[i];
    }
    __asm__ volatile("dmb sy" ::: "memory");
    disp_restore_pending=1;
    disp_restore_page();
    disp_mask_key=0xffffffffu;
    DISP_CALL(0x10554cf9u,void,void)();
}

/* Run on the existing GUI task before the native snapshot and layout pass.
 * Use native standard/horizon pages so angle sampling and page cleanup remain
 * native. Never disguise the snapshot by changing it around a callback. */
static void disp_update(void)
{
    disp_restore_page();
    if (disp_restore_pending) return;
    if ((!disp_photo_lcd() && !disp_information_visible()) ||
        !(z72_config_current.disp_enabled&Z72_DISP_CUSTOM)) {
        disp_restore();
        disp_suspended=0;
        return;
    }
    if (disp_suspended) return;
    if (DISP_CALL(0x106355a1u,z72_u32,void)()) return;
    if (disp_active && DISP_CALL(0x1063c7d5u,z72_u32,z72_u32)(disp_saved_property)!=disp_owned_page) {
        disp_restore();
        disp_suspended=1;
        return;
    }
    if (!disp_active) {
        z72_u32 property=DISP_CALL(0x1055213bu,z72_u32,void)();
        /* The selector must identify the photo LCD's own native property. */
        if (property!=0x3b15u) return;
        z72_u32 page=DISP_CALL(0x1063c7d5u,z72_u32,z72_u32)(property);
        if (page>4) return; /* Native photo LCD has a fifth information page. */
        disp_saved_property=property; disp_saved_page=page; disp_owned_page=page;
        __atomic_store_n(&disp_active,1,__ATOMIC_RELEASE);
    }
    z72_u32 bit=disp_preset_index==Z72_DISP_PAGES ? Z72_DISP_INFORMATION : 1u<<disp_preset_index;
    if (!(z72_config_current.disp_enabled&bit))
        disp_preset_index=disp_next(disp_preset_index);
    z72_u32 information=disp_preset_index==Z72_DISP_PAGES;
    z72_u32 components=information ? 0 : z72_config_current.disp_components[disp_preset_index];
    z72_u32 base=information ? 4u : components&Z72_DISP_HORIZON ? 3u : 0u;
    z72_u32 details=z72_disp_information(components,disp_preset_index);
    z72_u32 key=(disp_preset_index<<24)|components;
    z72_u32 dirty=__atomic_exchange_n(&disp_dirty,0,__ATOMIC_ACQ_REL);
    if (key!=disp_mask_key || dirty) {
        /* Explicit ordinary status groups only. Unidentified controls,
         * always-visible warnings and the separate AF layer stay native. */
        static const unsigned char information_groups[DISP_WIDGETS]={
            [10]=Z72_DISP_SHUTTER,[11]=Z72_DISP_APERTURE,[15]=Z72_DISP_ISO,
            [5]=Z72_DISP_SHOOTING,[3]=Z72_DISP_RELEASE,[6]=Z72_DISP_FOCUS,
            [2]=Z72_DISP_STATUS,[7]=Z72_DISP_STATUS,[8]=Z72_DISP_STATUS,[9]=Z72_DISP_STATUS,
            [13]=Z72_DISP_STATUS,[14]=Z72_DISP_STATUS,[16]=Z72_DISP_STATUS,[17]=Z72_DISP_STATUS,
            [26]=Z72_DISP_STATUS,[27]=Z72_DISP_STATUS,[34]=Z72_DISP_STATUS,[38]=Z72_DISP_STATUS,
            [41]=Z72_DISP_STATUS,[44]=Z72_DISP_STATUS,[46]=Z72_DISP_STATUS,[50]=Z72_DISP_STATUS,
            [53]=Z72_DISP_STATUS,[60]=Z72_DISP_FOCAL};
        for (z72_u32 i=0;i<DISP_WIDGETS;++i) {
            if (i==32) continue; /* Histogram uses its guarded extra pass. */
            z72_u32 ordinary=information_groups[i];
            /* Snapshot +0x300 selects the second LCD bank during the observed
             * AF operation. Leave the other outputs and third bank native. */
            for (z72_u32 bank=0;bank<2;++bank) {
                z72_u32 mask=bank ? disp_operation_masks[i] : disp_masks[i];
                if (ordinary && !information) {
                    z72_u32 visible=details&ordinary;
                    /* Cover page 0/3 in both native visibility nibbles. */
                    mask=(mask&~0x99u)|((visible && (mask&1u)) ? 0x99u : 0u);
                }
                DISP_WORD(DISP_WORD(0x1094aa24u+4*i)+(bank ? 0x10u : 4u))=mask;
            }
        }
        __asm__ volatile("dmb sy" ::: "memory");
        disp_mask_key=key;
        DISP_CALL(0x10554cf9u,void,void)();
    }
    if (DISP_CALL(0x1063c7d5u,z72_u32,z72_u32)(disp_saved_property)!=base) {
        if (!DISP_CALL(0x105543bbu,z72_u32,z72_u32)(base)) {
            /* The native OSD setter writes its snapshot before the property.
             * Roll back through the same API if the property write fails. */
            DISP_CALL(0x105543bbu,z72_u32,z72_u32)(disp_owned_page);
            disp_restore();
            return;
        }
        disp_owned_page=base;
        DISP_CALL(0x10554cf9u,void,void)();
    }
    /* Page four is a native app transition, not just an OSD property. Retry on
     * the next idle update if the stock controller is temporarily blocked. */
    if (information && disp_photo_lcd())
        DISP_CALL(0x1061ff63u,void,z72_u32)(0xb1821b1cu);
    else if (!information && disp_information_visible())
        DISP_CALL(0x105af219u,void,z72_u32,z72_u32,z72_u32)(DISP_INFORMATION_APP,4,0x100000);
}

static z72_u32 disp_can_change(void)
{
    /* Input ownership must not depend on the renderer being idle, or on an
     * earlier GUI pass having installed our layout. Consume the eligible DISP
     * request now; disp_update applies it when rendering can proceed. */
    if ((!disp_photo_lcd() && !disp_information_visible()) ||
        !(z72_config_current.disp_enabled&Z72_DISP_CUSTOM)) return 0;
    /* Match the actual OSD/DISP handler (0x105543d3), rather than the zoom
     * handler's operation flags. Rendering-busy checks belong to disp_update. */
    if (disp_information_visible()) {
        z72_u32 state=DISP_BYTE(0xb2030091u)&63u;
        if (state==2 || state==5) return 0;
    } else {
        if (DISP_CALL(0x10630d23u,z72_u32,void)()==1) return 0;
        z72_u32 monitor=DISP_CALL(0x10631501u,z72_u32,void)();
        if ((monitor==3 || monitor==4) &&
            (!DISP_CALL(0x1069577du,z72_u32,void)() || DISP_CALL(0x1063192du,z72_u32,void)())) return 0;
    }
    if (!DISP_CALL(0x1061dd6du,z72_u32,z72_u32)(0x2a) &&
        (DISP_CALL(0x1061dd6du,z72_u32,z72_u32)(2) ||
         !DISP_CALL(0x10631f23u,z72_u32,void)())) return 0;
    return !DISP_CALL(0x10633d5du,z72_u32,void)() &&
           !DISP_CALL(0x10633d45u,z72_u32,void)();
}

static void disp_advance(void)
{
    z72_u32 next=disp_next(disp_preset_index);
    if (next==disp_preset_index) return;
    disp_preset_index=next;
    disp_settings_changed();
    disp_update();
}

static z72_u32 disp_key(z72_u32 key,z72_u32 a,z72_u32 b,z72_u32 c)
{
    /* Ordinary shooting keys go straight to the native handler. The GUI task
     * already applies settings; only DISP needs layout sync before advancing. */
    if (key!=0x28)
        return DISP_CALL(DISP_KEY_NATIVE,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(key,a,b,c);
    disp_update();
    if (disp_can_change()) {
        disp_advance();
        return 0x101;
    }
    return DISP_CALL(DISP_KEY_NATIVE,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(key,a,b,c);
}

static z72_u32 disp_information_key(z72_u32 key,z72_u32 a,z72_u32 b,z72_u32 c)
{
    if (key==0x28 && disp_can_change()) {
        disp_advance();
        return 0x101;
    }
    return DISP_CALL(DISP_INFORMATION_KEY,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(key,a,b,c);
}

static z72_u32 disp_information_update(void)
{
    disp_update();
    if (DISP_BYTE(0xb1821b18u)!=15) return 0;
    return DISP_CALL(DISP_INFORMATION_UPDATE,z72_u32,void)();
}

static z72_u32 disp_ready(void)
{
    if (DISP_WORD(DISP_KEY_SLOT)!=DISP_KEY_NATIVE ||
        DISP_WORD(DISP_INFORMATION_APP+0x14)!=DISP_INFORMATION_KEY ||
        DISP_WORD(DISP_INFORMATION_APP+0x1c)!=DISP_INFORMATION_UPDATE) return 0;
    for (z72_u32 i=0;i<DISP_WIDGETS;++i) {
        z72_u32 descriptor=DISP_WORD(0x1094aa24u+4*i);
        if (descriptor<0xb1800000u || descriptor>=0xb1830000u || (descriptor&3u)) return 0;
    }
    return 1;
}

static void disp_install(void)
{
    for (z72_u32 i=0;i<DISP_WIDGETS;++i) {
        z72_u32 descriptor=DISP_WORD(0x1094aa24u+4*i);
        disp_masks[i]=DISP_WORD(descriptor+4);
        disp_operation_masks[i]=DISP_WORD(descriptor+0x10);
    }
    __asm__ volatile("dmb sy" ::: "memory");
    DISP_WORD(DISP_KEY_SLOT)=(z72_u32)disp_key;
    DISP_WORD(DISP_INFORMATION_APP+0x14)=(z72_u32)disp_information_key;
    DISP_WORD(DISP_INFORMATION_APP+0x1c)=(z72_u32)disp_information_update;
}
#endif
