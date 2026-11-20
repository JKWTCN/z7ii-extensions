/* SPDX-License-Identifier: MIT */
#ifndef Z72_FTP_WIFI_HOLD_H
#define Z72_FTP_WIFI_HOLD_H

/* C 1.70 main-CPU Wi-Fi task dispatch and its native one-shot timers. */
enum {
    FTP_LS_WIFI=0xb297b048u,
    FTP_LS_AP_HANDLER=FTP_LS_WIFI+0x38u,
    FTP_LS_CONNECTED_HANDLER=FTP_LS_WIFI+0x44u,
    FTP_LS_AP_NATIVE=0x1045680du,
    FTP_LS_CONNECTED_NATIVE=0x1045659fu,
    FTP_LS_IDLE=0x8000u,
    FTP_LS_SHUTDOWN_EVENTS=0x242u,
    FTP_PC_WIFI=0xb298e980u,
    FTP_PC_IDLE_HANDLER=FTP_PC_WIFI+8u,
    FTP_PC_IDLE_NATIVE=0x1045d081u,
    FTP_PC_CONNECTION_STATE=0xb29a1e8fu
};

static int ftp_holds_wifi(void)
{
    return ftp_transferring(ftp_load(&ftp_job)) || (ftp_load(&ftp_auto_epoch)&1u) ||
        (WORD(ROTATION_PAGE)==2 && WORD(ROTATION_PAGE+0x24) &&
         ftp_header_owned(WORD(ROTATION_PAGE+0x28)));
}

static u32 ftp_defer_ap_idle(u32 context,u32 events)
{
    if (context!=FTP_LS_WIFI || !(events&FTP_LS_IDLE) || (events&FTP_LS_SHUTDOWN_EVENTS) ||
        *(const volatile unsigned char *)(context+0x75)!=2 || !ftp_holds_wifi()) return events;
    CALL(0x103bf215u,void,u32,u32)(context+0x54,WORD(context+0x60));
    return events&~FTP_LS_IDLE;
}

static void ftp_ls_ap_event(u32 context,u32 events)
{
    events=ftp_defer_ap_idle(context,events);
    if (events) CALL(FTP_LS_AP_NATIVE,void,u32,u32)(context,events);
}

static void ftp_ls_connected_event(u32 context,u32 events)
{
    events=ftp_defer_ap_idle(context,events);
    if (events) CALL(FTP_LS_CONNECTED_NATIVE,void,u32,u32)(context,events);
}

static void ftp_pc_idle(void)
{
    u32 delay=WORD(FTP_PC_WIFI+0x4c);
    if (*(const volatile unsigned char *)FTP_PC_CONNECTION_STATE==2 && delay==300000u &&
        ftp_holds_wifi()) {
        CALL(0x10476a93u,void,u32)(delay);
    } else CALL(FTP_PC_IDLE_NATIVE,void,void)();
}

static void ftp_install_wifi_hold(void)
{
    u32 native=FTP_LS_AP_NATIVE;
    __atomic_compare_exchange_n((u32 *)FTP_LS_AP_HANDLER,&native,(u32)ftp_ls_ap_event,0,
                                __ATOMIC_RELEASE,__ATOMIC_RELAXED);
    native=FTP_LS_CONNECTED_NATIVE;
    __atomic_compare_exchange_n((u32 *)FTP_LS_CONNECTED_HANDLER,&native,(u32)ftp_ls_connected_event,0,
                                __ATOMIC_RELEASE,__ATOMIC_RELAXED);
    native=FTP_PC_IDLE_NATIVE;
    __atomic_compare_exchange_n((u32 *)FTP_PC_IDLE_HANDLER,&native,(u32)ftp_pc_idle,0,
                                __ATOMIC_RELEASE,__ATOMIC_RELAXED);
}

#endif
