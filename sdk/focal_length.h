/* SPDX-License-Identifier: MIT */
#ifndef Z72_FOCAL_LENGTH_H
#define Z72_FOCAL_LENGTH_H

#define FOCAL_DESCRIPTOR 0xb18213d4u
#define FOCAL_CONTROL 0xb18213a4u
#define FOCAL_WIDGET 60u

static u32 focal_value,focal_object,focal_output;

static u32 focal_ready(void)
{
    return WORD(0x1094aa24u+4*FOCAL_WIDGET)==FOCAL_DESCRIPTOR &&
        WORD(0x10949b30u+4*FOCAL_WIDGET)==0x1052819bu &&
        WORD(FOCAL_DESCRIPTOR+0x2c)==FOCAL_CONTROL && BYTE(FOCAL_DESCRIPTOR+0x28)==1;
}

/* Extend only the native focal-length text. The W/T bar and power-zoom
 * capability flags remain under the stock lens controller. Called on the GUI
 * task after the normal pass, so both paths use the same allocated object. */
__attribute__((noinline)) static void focal_update(u32 output_before)
{
    u32 object=WORD(FOCAL_CONTROL+0x24);
    if (object<0xb1800000u || object>=0xc0000000u || (object&3u)) {
        focal_value=0;
        return;
    }
    u32 output=active_photo_output();
    u32 menu=BYTE(0xb1824f0cu),playback=BYTE(0xb1828174u);
    if (!output || output!=output_before || WORD(0xb25519b8u) ||
        BYTE(0xb1821b18u) || WORD(0xb202f728u)!=0xb1821b1cu ||
        BYTE(DISPLAY_SNAPSHOT) || BYTE(DISPLAY_SNAPSHOT+0x1a6) ||
        BYTE(DISPLAY_SNAPSHOT+0x1a7) || WORD(0xb181dabcu) ||
        (menu>=1 && menu<=3) || (playback>=1 && playback<=3)) {
        focal_value=0;
        return;
    }
    if (display_work_pending() || !CALL(0x1052336bu,u32,void)()) return;
    /* This native query returns 1 when the selected page/bank excludes the
     * widget. Its query is conditional on the record's dirty byte. */
    u32 dirty=BYTE(FOCAL_CONTROL);
    BYTE(FOCAL_CONTROL)=1;
    u32 excluded=CALL(0x10522db1u,u32,u32)(FOCAL_WIDGET);
    BYTE(FOCAL_CONTROL)=dirty;
    if (excluded) {
        focal_value=0;
        return;
    }
    /* The stock PZ path already owns this text when both capability fields
     * are set. Do not add a second updater for those lenses. */
    if (WORD(DISPLAY_SNAPSHOT+0x31c) && WORD(DISPLAY_SNAPSHOT+0x370)) {
        focal_value=0;
        return;
    }
    u32 type=CALL(0x10634177u,u32,u32)(0x2014);
    u32 value=(type==1 || type==3) ? CALL(0x10634177u,u32,u32)(0x2001) : 0;
    if (value==0xffffffffu) value=0;
    if ((!value && !focal_value) ||
        (value==focal_value && object==focal_object && output==focal_output &&
         BYTE(FOCAL_CONTROL+1) && !CALL(0x10551fefu,u32,void)())) return;

    BYTE(FOCAL_CONTROL)=1;
    BYTE(FOCAL_CONTROL+1)=!!value;
    /* Preserve the native force-hidden contexts and page/output masks. */
    if (CALL(0x1055503bu,u32,u32)(FOCAL_WIDGET)) {
        focal_value=0;
        return;
    }
    CALL(0x10555007u,void,u32)(FOCAL_WIDGET);
    CALL(0x10522ec9u,void,u32)(FOCAL_WIDGET);
    if (value && BYTE(FOCAL_CONTROL+1)) {
        /* Same formatter, dynamic text slot, object and styles as 1052819B.
         * Read the live property rather than changing the stock snapshot:
         * that snapshot's PZ field can retain an older focal length. */
        char text[16];
        u32 whole=value/10u,part=value%10u;
        if (part) CALL(0x103fa101u,u32,char *,const char *,u32,u32)(text,"%d.%dmm",whole,part);
        else CALL(0x103fa101u,u32,char *,const char *,u32)(text,"%dmm",whole);
        WORD(object+0x40)=0x2ed;
        CALL(0x10523373u,void,u32,u32)(FOCAL_WIDGET,0);
        CALL(0x10523b8du,void,u32,u32,u32,const char *,u32)(19,FOCAL_WIDGET,0,text,
            BYTE(DISPLAY_SCENE)==EVF_SCENE ? 0x2f8 : 1);
        focal_value=value;
        focal_object=object;
        focal_output=output;
    } else {
        focal_value=0;
    }
    if (CALL(0x1052273fu,u32,void)() && !display_work_pending() &&
        active_photo_output()==output)
        CALL(0x105218bbu,void,void)();
}
#endif
