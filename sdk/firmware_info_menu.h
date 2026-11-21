/* SPDX-License-Identifier: MIT */
#ifndef Z72_FIRMWARE_INFO_MENU_H
#define Z72_FIRMWARE_INFO_MENU_H

enum { FIRMWARE_INFO_LINES=Z72_TEXT_FIRMWARE_INFO_TARGET-Z72_TEXT_FIRMWARE_INFO_VERSION+1,
       BATTERY_INFO_CONTROL_EVENTS=13, BATTERY_INFO_BACK_EVENT=29 };
static struct {
    u32 header[6],text[1+FIRMWARE_INFO_LINES+2][8],navigation_row[7],map[1];
    u32 events[BATTERY_INFO_CONTROL_EVENTS+2][4],callbacks[5];
} firmware_info_menu;

static u32 firmware_info_header_owned(u32 header)
{
    return header==(u32)firmware_info_menu.header;
}

static u32 open_firmware_info(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    u32 header=WORD(ROTATION_PAGE+0x28);
    u32 known=header==(u32)rotation_menu.header || zebra_header_owned(header);
#ifdef Z72_JPEG_PRE_CAPTURE
    known=known || pre_capture_menu_source(header) || jpeg_menu_owned(header);
#endif
    if (WORD(ROTATION_PAGE)!=2 || WORD(ROTATION_PAGE+0x24) || !known) return 1;
    HALF(ROTATION_PAGE+0x16)=0;
    HALF(ROTATION_PAGE+0x18)=0;
    WORD(ROTATION_PAGE+0x10)=(u32)firmware_info_menu.callbacks;
    WORD(ROTATION_PAGE+0x28)=(u32)firmware_info_menu.header;
    __asm__ volatile("dmb sy" ::: "memory");
    CALL(0x1063ab75u,void,u32,u32)(2,ROTATION_PAGE_ID);
    return 0;
}

static void firmware_info_prepare(void)
{
    const u32 *battery=(const u32 *)0xb18553acu;
    const u32 (*text)[8]=(const u32 (*)[8])battery[2];
    const u32 (*events)[4]=(const u32 (*)[4])*(const u32 *)battery[4];
    const u32 *navigation_row=(const u32 *)battery[3];
    const u32 *callbacks=(const u32 *)0x10969804u;
    for (u32 i=0;i<6;++i) firmware_info_menu.header[i]=battery[i];
    firmware_info_menu.header[2]=(u32)firmware_info_menu.text;
    firmware_info_menu.header[3]=(u32)firmware_info_menu.navigation_row;
    firmware_info_menu.header[4]=(u32)firmware_info_menu.map;

    /* Battery info uses fixed text widgets and a single invisible event row. */
    const u32 lines[]={1,2,3,7};
    for (u32 col=0;col<8;++col) {
        firmware_info_menu.text[0][col]=text[0][col];
        firmware_info_menu.text[1+FIRMWARE_INFO_LINES][col]=rotation_menu.title[8+col];
        firmware_info_menu.text[2+FIRMWARE_INFO_LINES][col]=rotation_menu.title[16+col];
    }
    firmware_info_menu.text[0][0]=Z72_TEXT_FIRMWARE_INFO;
    for (u32 line=0;line<FIRMWARE_INFO_LINES;++line) {
        u32 *label=firmware_info_menu.text[line+1];
        for (u32 col=0;col<8;++col) label[col]=text[lines[line]][col];
        u32 top=0x76u+line*0x34u;
        label[0]=Z72_TEXT_FIRMWARE_INFO_VERSION+line;
        label[4]=(top<<16)|0x004eu;
        label[5]=((top+0x28u)<<16)|0x02b8u;
    }

    for (u32 i=0;i<7;++i) firmware_info_menu.navigation_row[i]=navigation_row[i];
    firmware_info_menu.navigation_row[1]=firmware_info_menu.navigation_row[2]=0xffffu;
    firmware_info_menu.navigation_row[6]=0x2ed0u;
    for (u32 i=0;i<BATTERY_INFO_CONTROL_EVENTS;++i)
        for (u32 col=0;col<4;++col) firmware_info_menu.events[i][col]=events[i][col];
    /* Skip the actions that update battery readings. Keep Back and the terminator. */
    for (u32 i=0;i<2;++i)
        for (u32 col=0;col<4;++col)
            firmware_info_menu.events[BATTERY_INFO_CONTROL_EVENTS+i][col]=events[BATTERY_INFO_BACK_EVENT+i][col];
    firmware_info_menu.map[0]=(u32)firmware_info_menu.events;
    firmware_info_menu.callbacks[2]=callbacks[2]; /* Common menu-state notification. */
}
#endif
