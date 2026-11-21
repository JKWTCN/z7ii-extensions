/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIDE_AF_I_MENU_H
#define Z72_WIDE_AF_I_MENU_H

#include "wide_af_i_native.h"

#define WIDE_I_BYTE(address) (*(volatile unsigned char *)(address))

enum {
    WIDE_I_INPUT=0xb1821a1cu, WIDE_I_INPUT_NATIVE=0x105c1639u,
    WIDE_I_EVENT=0xb1821a20u, WIDE_I_EVENT_NATIVE=0x105c1eb1u,
    WIDE_I_APP_STATE=0xb1821964u, WIDE_I_SETTING=0xb1824270u,
    WIDE_I_VALUES=0xb20327b4u, WIDE_I_CURSOR=0xb1824564u, WIDE_I_NAV=0xb203250cu,
    WIDE_I_AF_S_SCENE=0x47, WIDE_I_AF_C_SCENE=0x48,
    WIDE_I_STOCK=9, WIDE_I_WIDE_CHOICES=11,
#ifdef Z72_SUBJECT_TRACKING
    WIDE_I_CHOICES=WIDE_I_WIDE_CHOICES+1,
#else
    WIDE_I_CHOICES=WIDE_I_WIDE_CHOICES,
#endif
    WIDE_I_SLOTS=16, WIDE_I_COLUMNS=5,
    WIDE_I_ROW_PITCH=46, WIDE_I_TILE_HEIGHT=42, WIDE_I_ICON_HEIGHT=32,
    WIDE_I_OK=0x1b, WIDE_I_TOUCH=0x50, WIDE_I_LAYOUT=0x10,
    WIDE_I_MAIN_EVENTS=31, WIDE_I_SETTINGS_EVENTS=36
};

static const struct {
    z72_u32 page,header,entries;
} wide_i_stock[]={
    {0xb2bc6ce0u,0x1147,0x116c7948u},
    {0xb2bc6cecu,0x1148,0x116c7a48u},
    {0xb2bc72f8u,0x1147,0x116c79c8u},
    {0xb2bc7304u,0x1148,0x116c7ac8u}
};

struct wide_i_page {
    z72_u32 header[3],entries[WIDE_I_SLOTS][2];
    z72_u32 title[5],title_spec[9],title_rows[WIDE_I_CHOICES][5];
    z72_u32 tiles[WIDE_I_CHOICES][5],tile_specs[WIDE_I_CHOICES][9];
    z72_u32 icons[WIDE_I_CHOICES][5],rects[WIDE_I_CHOICES][4][2];
    z72_u32 outlines[WIDE_I_CHOICES][6];
};

static struct wide_i_page wide_i_pages[sizeof(wide_i_stock)/sizeof(wide_i_stock[0])];
static z72_u32 wide_i_dispatch[3][2],wide_i_root_keys[WIDE_I_MAIN_EVENTS][2];
static z72_u32 wide_i_setting_keys[WIDE_I_SETTINGS_EVENTS][2];
#define WIDE_I_CODE_BYTES (WIDE_I_CODE_END-WIDE_I_CODE_START)
static _Alignas(64) unsigned char wide_i_code[WIDE_I_CODE_BYTES+sizeof(wide_i_branches)];

static z72_u32 wide_i_local(z72_u32 address)
{
    return (z72_u32)wide_i_code+address-WIDE_I_CODE_START;
}

static void wide_i_copy(void *destination,const void *source,z72_u32 bytes)
{
    ((void (*)(void *,const void *,z72_u32))0x10360facu)(destination,source,bytes);
}

static z72_u32 wide_i_choice_count(z72_u32 scene)
{
    return scene==WIDE_I_AF_C_SCENE ? WIDE_I_CHOICES : WIDE_I_WIDE_CHOICES;
}

static z72_u32 wide_i_active_selection(void)
{
#ifdef Z72_SUBJECT_TRACKING
    if (tracking_active()) return TRACK_SELECTION;
#endif
    return wide_af_active();
}

static z72_u32 wide_i_is_open(void)
{
    if (WORD(0xb25519b8u) ||
        (HALF(WIDE_I_APP_STATE)!=0x1601u && HALF(WIDE_I_APP_STATE)!=0x1701u) ||
        (unsigned short)HALF(WIDE_I_VALUES+2)!=wide_i_choice_count(WIDE_I_BYTE(WIDE_I_SETTING))) return 0;
    z72_u32 page=WORD(WIDE_I_SETTING+0x70);
    for (z72_u32 i=0;i<sizeof(wide_i_stock)/sizeof(wide_i_stock[0]);++i)
        if (page==(z72_u32)wide_i_pages[i].header) return 1;
    return 0;
}

static void wide_i_extend(z72_u32 count)
{
    z72_u32 preset=wide_i_active_selection();
    z72_u32 selected=preset && preset<=count-WIDE_I_STOCK ?
        WIDE_I_STOCK+preset-1 : WORD(WIDE_I_CURSOR+0xc);
    if (selected>=count) selected=0;
    for (z72_u32 i=WIDE_I_STOCK;i<count;++i) {
        z72_u32 address=WIDE_I_VALUES+i*8;
        WORD(address+4)=0;
        WORD(address+8)=i;
        WORD(WIDE_I_VALUES+0x194+i*8)=0;
        WORD(WIDE_I_VALUES+0x198+i*8)=0;
    }
    HALF(WIDE_I_VALUES+2)=count;
    WORD(WIDE_I_CURSOR+0xc)=selected;
    WORD(WIDE_I_CURSOR+0x1c)=selected;
    WORD(WIDE_I_VALUES+0x1f8)=selected;
    WORD(WIDE_I_NAV)=count;
    WORD(WIDE_I_NAV+4)=selected;
}

static z72_u32 wide_i_draw(z72_u32 scene,z72_u32 lcd,z72_u32 evf,z72_u32 pool,z72_u32 parent)
{
    z72_u32 count=HALF(WIDE_I_VALUES+2),choices=wide_i_choice_count(scene);
    if (!lcd || !evf || !pool || WORD(0xb25519b8u) ||
        (scene!=WIDE_I_AF_S_SCENE && scene!=WIDE_I_AF_C_SCENE) || WIDE_I_BYTE(WIDE_I_VALUES)!=scene-0x24 ||
        (count!=WIDE_I_STOCK && count!=choices) ||
        CALL(0x104e6403u,z72_u32,void)()>=0xf)
        return CALL(0x10613a3du,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(
            scene,lcd,evf,pool,parent);

    /* Stock 10613a3c scene initialization for the photo AF view. Substitute
     * the private descriptor before its first render, inside native open. */
    if (count==WIDE_I_STOCK) wide_i_extend(choices);
    z72_u32 display=CALL(0x10631947u,z72_u32,void)();
    z72_u32 first=display==1 || (display==4 && CALL(0x10632495u,z72_u32,void)());
    struct wide_i_page *view=&wide_i_pages[(first ? 0u : 2u)+scene-WIDE_I_AF_S_SCENE];
    if (wide_icons_load()) {
        for (z72_u32 i=WIDE_I_STOCK;i<WIDE_I_WIDE_CHOICES;++i) {
            view->icons[i][1]=0;
            view->icons[i][2]=wide_icon_id(i-WIDE_I_STOCK+1,0);
        }
#ifdef Z72_SUBJECT_TRACKING
        if (choices==WIDE_I_CHOICES) view->icons[WIDE_I_WIDE_CHOICES][2]=TRACK_ICON;
#endif
    }
    z72_u32 page=(z72_u32)view->header;
    CALL(0x1060cfa5u,void,void)();
    WIDE_I_BYTE(WIDE_I_SETTING)=scene;
    WORD(WIDE_I_SETTING+0x30)=parent;
    WORD(WIDE_I_SETTING+0x6c)=page;
    WORD(WIDE_I_SETTING+0x70)=page;
    WORD(WIDE_I_SETTING+0x74)=0;
    WORD(WIDE_I_SETTING+0x78)=lcd;
    WORD(WIDE_I_SETTING+0x7c)=evf;
    WORD(WIDE_I_SETTING+0x80)=pool;
    return CALL(0x10613875u,z72_u32,void)();
}

static z72_u32 wide_i_photo_af_tile(z72_u32 row)
{
    return !WORD(0xb25519b8u) && row<12 && WIDE_I_BYTE(WIDE_I_VALUES)==1 &&
        CALL(0x10602087u,z72_u32,z72_u32)(row)==0x15 &&
        CALL(0x106020edu,z72_u32,void)()==0x23;
}

#include "wide_af_i_wheel.h"

static z72_u32 wide_i_root_ok(void)
{
    z72_u32 row=CALL(0x10601e77u,z72_u32,void)(),status[2];
    if (!wide_i_photo_af_tile(row)) return CALL(0x105c2193u,z72_u32,void)();
    CALL(0x1060262bu,void,z72_u32 *,z72_u32)(status,row);
    if (status[1]) return CALL(0x105c2193u,z72_u32,void)();

    CALL(0x10613c73u,void,void)();
    CALL(0x10606f1du,void,z72_u32)(0x23);
    CALL(wide_i_local(0x105c1215u),void,z72_u32)(0x16);
    return 0;
}

static z72_u32 wide_i_confirm(void)
{
    if (wide_i_is_open() && !CALL(0x1060e05fu,z72_u32,void)()) {
        z72_u32 selected=CALL(0x10601e77u,z72_u32,void)();
        if (selected>=WIDE_I_STOCK && selected<(unsigned short)HALF(WIDE_I_VALUES+2)) {
            if (wide_af_choose(selected-WIDE_I_STOCK+1)) return 0;
        } else if (selected<WIDE_I_STOCK && wide_af_choose(0)) {
            return 0;
        }
    }
    return CALL(0x105c2055u,z72_u32,void)();
}

static z72_u32 wide_i_input(z72_u32 key)
{
    z72_u32 controller=WIDE_I_INPUT_NATIVE;
    if ((key==WIDE_I_OK || key==WIDE_I_TOUCH) &&
        (wide_i_is_open() || (!WIDE_I_BYTE(WIDE_I_APP_STATE) &&
         WIDE_I_BYTE(WIDE_I_VALUES)==1 && !WORD(0xb25519b8u))))
        controller=wide_i_local(WIDE_I_INPUT_NATIVE);
    if (key>=WIDE_I_WHEEL_FIRST_KEY && key<=WIDE_I_WHEEL_LAST_KEY &&
        (wide_i_wheel_active() || wide_i_wheel_available()))
        controller=wide_i_local(WIDE_I_INPUT_NATIVE);
    return CALL(controller,z72_u32,z72_u32)(key);
}

static z72_u32 wide_i_event(z72_u32 event)
{
    if (event!=2 || !wide_i_is_open()) return CALL(WIDE_I_EVENT_NATIVE,z72_u32,z72_u32)(event);
    /* Match the native event 2 admission before using its private rebuild. */
    if (!CALL(0x1061dd6du,z72_u32,z72_u32)(0x2c) &&
        (CALL(0x10631501u,z72_u32,void)()!=4 || !CALL(0x1069577du,z72_u32,void)()))
        CALL(wide_i_local(0x105c1d87u),void,void)();
    return 0;
}

static void wide_i_build(z72_u32 variant)
{
    const z72_u32 (*entries)[2]=(const z72_u32 (*)[2])wide_i_stock[variant].entries;
    struct wide_i_page *out=&wide_i_pages[variant];
    /* Native redraw visits every non-null entry. Leave the AF-C-only tile's
     * slot empty in AF-S, just like the other unused stock slots. */
    z72_u32 choices=wide_i_choice_count(WIDE_I_AF_S_SCENE+(variant&1u));
    out->header[0]=(wide_i_stock[variant].header&255u)|(WIDE_I_LAYOUT<<8);
    out->header[1]=(z72_u32)out->entries;
    out->header[2]=WIDE_I_SLOTS;
    wide_i_copy(out->entries,entries,sizeof(out->entries));
    wide_i_copy(out->title,(const void *)entries[0][0],sizeof(out->title));
    wide_i_copy(out->title_spec,(const void *)out->title[0],sizeof(out->title_spec));
    wide_i_copy(out->title_rows,(const void *)out->title_spec[1],WIDE_I_STOCK*sizeof(out->title_rows[0]));
    out->title[0]=(z72_u32)out->title_spec;
    out->title_spec[1]=(z72_u32)out->title_rows;
    out->title_spec[2]=choices;
    out->entries[0][0]=(z72_u32)out->title;

    const z72_u32 *first_tile=(const z72_u32 *)entries[1][0];
    const z72_u32 *first_spec=(const z72_u32 *)first_tile[1];
    wide_i_copy(out->icons,(const void *)first_spec[1],WIDE_I_STOCK*sizeof(out->icons[0]));
    for (z72_u32 i=0;i<choices;++i) {
        if (i>=WIDE_I_STOCK && i<WIDE_I_WIDE_CHOICES) {
            z72_u32 preset=i-WIDE_I_STOCK;
            out->title_rows[i][0]=i;
            out->title_rows[i][1]=1;
            out->title_rows[i][2]=Z72_TEXT_WIDE_C1+preset;
            out->icons[i][0]=i;
            out->icons[i][1]=1;
            out->icons[i][2]=Z72_TEXT_WIDE_TILE_C1+preset;
        }
#ifdef Z72_SUBJECT_TRACKING
        if (i==WIDE_I_WIDE_CHOICES) {
            out->title_rows[i][0]=i;
            out->title_rows[i][1]=1;
            out->title_rows[i][2]=Z72_TEXT_SUBJECT_TRACKING;
            out->icons[i][0]=i;
            out->icons[i][1]=0;
            out->icons[i][2]=0x10b1u; /* Auto-area fallback before the atlas loads. */
        }
#endif
        z72_u32 source=i<WIDE_I_STOCK ? i : 3;
        wide_i_copy(out->tiles[i],(const void *)entries[source+1][0],sizeof(out->tiles[i]));
        wide_i_copy(out->tile_specs[i],(const void *)out->tiles[i][1],sizeof(out->tile_specs[i]));
        wide_i_copy(out->outlines[i],(const void *)out->tiles[i][4],sizeof(out->outlines[i]));
        out->tiles[i][1]=(z72_u32)out->tile_specs[i];
        out->tiles[i][4]=(z72_u32)out->outlines[i];
        out->tile_specs[i][1]=(z72_u32)out->icons;
        out->tile_specs[i][2]=choices;
        /* Center each 32-pixel icon within its compact highlight rectangle.
         * The original two-row icon offsets are too low for the shorter rows. */
        for (z72_u32 rect=0;rect<4;++rect) {
            const z72_u32 *stock=(const z72_u32 *)first_spec[3+rect];
            const z72_u32 *frame=(const z72_u32 *)first_spec[4+(rect&2u)];
            z72_u32 x=(i%WIDE_I_COLUMNS)*120, y=(i/WIDE_I_COLUMNS)*WIDE_I_ROW_PITCH;
            z72_u32 height=(rect&1u) ? WIDE_I_TILE_HEIGHT : WIDE_I_ICON_HEIGHT;
            z72_u32 top=(frame[0]>>16)+y+(WIDE_I_TILE_HEIGHT-height)/2;
            out->rects[i][rect][0]=((stock[0]&0xffffu)+x)|(top<<16);
            out->rects[i][rect][1]=((stock[1]&0xffffu)+x)|((top+height)<<16);
            out->tile_specs[i][3+rect]=(z72_u32)out->rects[i][rect];
        }
        out->entries[i+1][0]=(z72_u32)out->tiles[i];
        out->entries[i+1][1]=1;
    }
}

static z72_u32 wide_i_ready(void)
{
    if (WORD(WIDE_I_INPUT)!=WIDE_I_INPUT_NATIVE || WORD(WIDE_I_EVENT)!=WIDE_I_EVENT_NATIVE ||
        WORD(0x116d2110u)!=0x116d1e08u || WORD(0x116d2114u)!=WIDE_I_MAIN_EVENTS ||
        WORD(0x116d2118u)!=0x116d1f00u || WORD(0x116d211cu)!=WIDE_I_SETTINGS_EVENTS ||
        WORD(0x116d2120u)!=0x116d2020u || WORD(0x116d2124u)!=WIDE_I_WHEEL_EVENTS ||
        WORD(0x116d13b8u)!=0x105c44b7u || WORD(0x116d1e4cu)!=0x105c2193u || WORD(0x116d1f8cu)!=0x105c2055u) return 0;
    z72_u32 hash=2166136261u;
    for (z72_u32 a=WIDE_I_CODE_START;a<WIDE_I_CODE_END;++a)
        hash=(hash^WIDE_I_BYTE(a))*16777619u;
    if (hash!=WIDE_I_CODE_HASH) return 0;
    for (z72_u32 i=0;i<sizeof(wide_i_stock)/sizeof(wide_i_stock[0]);++i) {
        z72_u32 page=wide_i_stock[i].page;
        if (WORD(page)!=wide_i_stock[i].header || WORD(page+4)!=wide_i_stock[i].entries ||
            WORD(page+8)!=WIDE_I_SLOTS) return 0;
    }
    return 1;
}

static void wide_i_install(void)
{
    static z72_u32 wheel_keys[WIDE_I_WHEEL_EVENTS][2];
    static z72_u32 (*const dial[4])(void)={
        wide_i_wheel_41,wide_i_wheel_42,wide_i_wheel_43,wide_i_wheel_44
    };
    for (z72_u32 i=0;i<sizeof(wide_i_stock)/sizeof(wide_i_stock[0]);++i) wide_i_build(i);
    wide_i_copy(wide_i_dispatch,(const void *)0x116d2110u,sizeof(wide_i_dispatch));
    wide_i_copy(wide_i_root_keys,(const void *)wide_i_dispatch[0][0],sizeof(wide_i_root_keys));
    wide_i_copy(wide_i_setting_keys,(const void *)wide_i_dispatch[1][0],sizeof(wide_i_setting_keys));
    wide_i_copy(wheel_keys,(const void *)wide_i_dispatch[2][0],sizeof(wheel_keys));
    for (z72_u32 i=0;i<4;++i) {
        wide_i_root_keys[10+i][1]=(z72_u32)dial[i];
        wheel_keys[10+i][1]=(z72_u32)dial[i];
    }
    wide_i_dispatch[2][0]=(z72_u32)wheel_keys;
    wide_i_root_keys[8][1]=(z72_u32)wide_i_root_ok;
    wide_i_setting_keys[17][1]=(z72_u32)wide_i_confirm;
    wide_i_dispatch[0][0]=(z72_u32)wide_i_root_keys;
    wide_i_dispatch[1][0]=(z72_u32)wide_i_setting_keys;
    for (z72_u32 state=0;state<2;++state)
        for (z72_u32 i=0;i<wide_i_dispatch[state][1];++i) {
            z72_u32 (*keys)[2]=(z72_u32 (*)[2])wide_i_dispatch[state][0];
            if (keys[i][1]==0x105c254du) keys[i][1]=wide_i_local(keys[i][1]);
        }
    wide_i_copy(wide_i_code,(const void *)WIDE_I_CODE_START,WIDE_I_CODE_BYTES);
    WORD(wide_i_local(0x105c19d4u))=(z72_u32)wide_i_dispatch;
    WORD(wide_i_local(0x105c2668u))=(z72_u32)wide_i_dispatch;
    for (z72_u32 i=0;i<sizeof(wide_i_branches)/sizeof(wide_i_branches[0]);++i) {
        z72_u32 site=wide_i_local(wide_i_branches[i][0]);
        z72_u32 stub=(z72_u32)wide_i_code+WIDE_I_CODE_BYTES+i*8;
        z72_u32 target=wide_i_branches[i][1];
        WORD(stub)=0xf000f8dfu;
        WORD(stub+4)=target==0x10613a3du ? (z72_u32)wide_i_draw : target;
        z72_u32 delta=stub-site-4;
        z72_u32 j1=1u^(delta>>23&1u),j2=1u^(delta>>22&1u);
        z72_u32 link=HALF(site+2)&0x4000u;
        HALF(site)=(unsigned short)(0xf000u|(delta>>12&0x3ffu));
        HALF(site+2)=(unsigned short)(0x9000u|link|(j1<<13)|(j2<<11)|(delta>>1&0x7ffu));
    }
    for (z72_u32 p=(z72_u32)wide_i_code;p<(z72_u32)wide_i_code+sizeof(wide_i_code);p+=32)
        __asm__ volatile("mcr p15,0,%0,c7,c11,1" :: "r"(p) : "memory");
    z72_u32 zero=0;
    __asm__ volatile("dsb sy\n mcr p15,0,%0,c7,c1,0\n dsb sy\n isb sy" :: "r"(zero) : "memory");
    WORD(WIDE_I_EVENT)=(z72_u32)wide_i_event;
    WORD(WIDE_I_INPUT)=(z72_u32)wide_i_input;
}

#endif
