/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIDE_AF_I_WHEEL_H
#define Z72_WIDE_AF_I_WHEEL_H

enum {
    WIDE_I_WHEEL_EVENTS=30, WIDE_I_WHEEL_STATE=2,
    WIDE_I_WHEEL_OPTIONS=0xb1824b80u, WIDE_I_WHEEL_ICONS=0xb2098464u,
    WIDE_I_WHEEL_TITLES=0xb2098478u, WIDE_I_WHEEL_SCENE=0xb202f644u,
    WIDE_I_WHEEL_MODEL=0xb202ee24u, WIDE_I_WHEEL_FIRST_KEY=0x41, WIDE_I_WHEEL_LAST_KEY=0x44
};

static unsigned short wide_i_wheel_icons[WIDE_I_CHOICES];
static z72_u32 wide_i_wheel_titles[WIDE_I_CHOICES];
static z72_u32 wide_i_wheel_stock,wide_i_wheel_count,wide_i_wheel_index;

static z72_u32 wide_i_wheel_active(void)
{
    return !WORD(0xb25519b8u) && WIDE_I_BYTE(WIDE_I_APP_STATE)==WIDE_I_WHEEL_STATE &&
        WORD(WIDE_I_WHEEL_MODEL+0x14)==(z72_u32)wide_i_wheel_icons &&
        WORD(0xb1823aacu)==WIDE_I_WHEEL_SCENE;
}

static z72_u32 wide_i_wheel_available(void)
{
    if (WIDE_I_BYTE(WIDE_I_APP_STATE) ||
        !wide_i_photo_af_tile(CALL(0x10601e77u,z72_u32,void)()) ||
        CALL(0x1063c7d5u,z72_u32,z72_u32)(0x168c)) return 0;
    z72_u32 focus=CALL(0x1063c7d5u,z72_u32,z72_u32)(0x56b);
    return (focus==1 || focus==2) && wide_icons_load();
}

static z72_u32 wide_i_wheel_gap(z72_u32 index)
{
    return index<wide_i_wheel_stock && CALL(0x106075b1u,z72_u32,z72_u32)(index);
}

static z72_u32 wide_i_wheel_step(z72_u32 key,z72_u32 steps)
{
    z72_u32 count=wide_i_wheel_count,next=wide_i_wheel_index,moves=0;
    z72_u32 stride=key==0x41 || key==0x44 ? count-1 : 1;
    for (z72_u32 step=0;step<steps;++step) {
        do {
            next+=stride;
            if (next>=count) next-=count;
            ++moves;
        } while (wide_i_wheel_gap(next));
    }
    if (!moves) return 0;
    if (next!=wide_i_wheel_index) {
        z72_u32 preset=next<wide_i_wheel_stock ? 0 : next-wide_i_wheel_stock+1;
        if (wide_af_choose(preset)) return 0;
        if (!preset)
            CALL(0x10601c37u,void,z72_u32)(WORD(WIDE_I_WHEEL_OPTIONS+next*8));
        wide_i_wheel_index=next;
    }
    for (z72_u32 move=0;move<moves;++move)
        CALL(0x1058be99u,void,z72_u32)(key);
    CALL(0x1058b30bu,void,void)();
    CALL(0x105c2ad1u,void,z72_u32)(0);
    return 1;
}

static void wide_i_wheel_open(z72_u32 key)
{
    /* Stock photo AF wheel data builder. Keep its filtered options and put the
     * two extra icon/title records in private arrays, beyond the stock count. */
    z72_u32 focus=CALL(0x1063c7d5u,z72_u32,z72_u32)(0x56b),index=0;
    unsigned char count=0;
    CALL(0x10607509u,void,z72_u32,unsigned char *)(focus,&count);
    if (!count || count>WIDE_I_STOCK ||
        !CALL(0x10606fc7u,z72_u32,z72_u32,z72_u32 *)(focus,&index) || index>=count) {
        CALL(0x105c44b7u,void,z72_u32)(key);
        return;
    }
    wide_i_wheel_stock=count;
    for (z72_u32 i=0;i<count;++i) {
        wide_i_wheel_icons[i]=HALF(WIDE_I_WHEEL_ICONS+2*i);
        wide_i_wheel_titles[i]=WORD(WIDE_I_WHEEL_TITLES+4*i);
    }
    for (z72_u32 i=0;i<2;++i) {
        wide_i_wheel_icons[count+i]=(unsigned short)wide_icon_id(i+1,0);
        wide_i_wheel_titles[count+i]=Z72_TEXT_WIDE_C1+i;
    }
    wide_i_wheel_count=count+2;
#ifdef Z72_SUBJECT_TRACKING
    if (tracking_available()) {
        wide_i_wheel_icons[wide_i_wheel_count]=TRACK_ICON;
        wide_i_wheel_titles[wide_i_wheel_count++]=Z72_TEXT_SUBJECT_TRACKING;
    }
#endif
    z72_u32 preset=wide_i_active_selection();
    wide_i_wheel_index=preset ? count+preset-1 : index;
    WORD(WIDE_I_APP_STATE+0x80)=wide_i_wheel_index;
    CALL(0x105c2a01u,void,z72_u32)(4);
    CALL(0x105c73d3u,void,z72_u32,z72_u32,z72_u32,z72_u32,z72_u32)(
        0x105c73f7u,0,0,(z72_u32)wide_i_wheel_gap,0);
    WORD(WIDE_I_APP_STATE+0x84)=1;
    CALL(0x105c2a5du,void,z72_u32,const unsigned short *,z72_u32,z72_u32)(
        wide_i_wheel_count,wide_i_wheel_icons,0,0);
    CALL(0x1058b8c7u,void,z72_u32)(WIDE_I_WHEEL_SCENE);
    CALL(0x105c2c37u,void,const z72_u32 *)(wide_i_wheel_titles);
    if (key) wide_i_wheel_step(key,1);
    else CALL(0x105c2ad1u,void,z72_u32)(0);
}

static z72_u32 wide_i_wheel_input(z72_u32 key)
{
    if (wide_i_wheel_active())
        return wide_i_wheel_step(key,CALL(0x10542629u,z72_u32,void)());
    if (!CALL(0x10604b0du,z72_u32,void)()) return 0;

    /* Enter the same native dial state as the root key handler. The regular
     * i-menu close, display updates, and dial animation still own the scene. */
    WORD(WIDE_I_APP_STATE+0x38)=key;
    CALL(0x105c0245u,void,void)();
    WIDE_I_BYTE(WIDE_I_APP_STATE)=WIDE_I_WHEEL_STATE;
    CALL(0x106b91a1u,void,z72_u32)(0x5f);
    CALL(0x1053ca5du,void,z72_u32,z72_u32)(0,0x100);
    for (z72_u32 display=0;display<2;++display) {
        CALL(0x1060d03fu,void,z72_u32)(display);
        CALL(0x105c07a3u,void,void)();
    }
    CALL(0x1060d03fu,void,z72_u32)(0);
    CALL(0x105c72ddu,void,void)();
    if (CALL(0x104a96ffu,z72_u32,void)()) WORD(WIDE_I_APP_STATE+0x38)=0;
    wide_i_wheel_open(WORD(WIDE_I_APP_STATE+0x38));
    if (CALL(0x104a96ffu,z72_u32,void)()) {
        const void *model=CALL(0x104a9721u,const void *,z72_u32)(3);
        wide_i_copy((void *)0xb12b6880u,model,0x50);
        WORD(WIDE_I_APP_STATE+0x8c)=CALL(0x104a9721u,z72_u32,z72_u32)(4);
    }
    CALL(0x1060d03fu,void,z72_u32)(0);
    return 0;
}

#define WIDE_I_WHEEL_KEY(key) \
    static z72_u32 wide_i_wheel_##key(void) { return wide_i_wheel_input(0x##key); }
WIDE_I_WHEEL_KEY(41)
WIDE_I_WHEEL_KEY(42)
WIDE_I_WHEEL_KEY(43)
WIDE_I_WHEEL_KEY(44)
#undef WIDE_I_WHEEL_KEY

#endif
