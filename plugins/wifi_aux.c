/* SPDX-License-Identifier: MIT */
/* Executed by the auxiliary ARM926. Keep this image Thumb1 and independent
 * of main-CPU APIs. Only the selected region is shared with the main CPU. */
#include "../sdk/wifi_regions.h"

typedef unsigned int u32;
typedef unsigned char u8;
typedef u32 (*native_handler)(u32,volatile u32 *);

volatile u32 wifi_aux_region=Z72_WIFI_REGION_BODY;
static const unsigned char countries[Z72_WIFI_REGION_BODY][3]=Z72_WIFI_COUNTRIES;
static const u32 country_revisions[Z72_WIFI_REGION_BODY]=Z72_WIFI_COUNTRY_REVISIONS;

static u32 delegate(native_handler original,u32 event,volatile u32 *request,u32 failed)
{
    u32 status=request[0];
    if (failed) request[0]=0xffffffffu;
    u32 result=original(event,request);
    if (failed) request[0]=status;
    return result;
}

static u32 password_valid(const volatile u8 *config)
{
    for (u32 i=0;i<=64u;i++) if (!config[0x2cu+i]) return 1;
    return 0;
}

static u32 prepare(u32 region,u32 ap)
{
    u32 result=((u32 (*)(u32,u32,u32,u32))0xb10bfbb7u)(0,1,0,0);
    if (!result) {
        result=((u32 (*)(u32,u32))0xb10bf5cdu)(0,0);
        /* Unlock the previous band before country, whose native helper first
         * selects channel 1. AP then locks 5 GHz; STA stays in AUTO. */
        if (!result) result=((u32 (*)(u32))0xb10b4347u)(0);
        if (!result) result=((u32 (*)(const unsigned char *,u32,u32))0xb10b108fu)(
            countries[region],2,country_revisions[region]);
        if (!result && ap) result=((u32 (*)(u32))0xb10b4347u)(1);
        if (!result && ap) {
            u32 nmode=1;
            result=((u32 (*)(u32 *,u32,u32))0xb10c0027u)(&nmode,1,0);
        }
        if (!result && ap) {
            u32 wme=1;
            result=((u32 (*)(u32 *,u32,u32))0xb10bfd37u)(&wme,1,0);
        }
        if (!result && ap) {
            u32 vht=1;
            result=((u32 (*)(u32 *,u32,u32))0xb10c136bu)(&vht,1,0);
        }
        if (!result && ap) {
            /* Permit native ACS to choose among allowed 20/40/80 MHz channels. */
            u32 bandwidth[2]={1,7};
            result=((u32 (*)(u32 *,u32,u32))0xb10c0bc3u)(bandwidth,1,0);
        }
        /* Restore UP even if DOWN/configuration failed; retain the first error. */
        u32 up=((u32 (*)(u32,u32))0xb10bf5cdu)(1,0);
        if (!result) result=up;
    }
    if (!result && ap) {
        u32 band=0;
        result=((u32 (*)(u32 *))0xb10b4369u)(&band);
        if (!result && band!=1u) result=1;
    }
    return result;
}

__attribute__((used,noinline))
u32 wifi_aux_ap(u32 event,volatile u32 *request)
{
    native_handler original=(native_handler)0xb107182bu;
    u32 region=wifi_aux_region;
    if (region>=Z72_WIFI_REGION_BODY || request[0]==0xffffffffu)
        return original(event,request);
    volatile u8 *config=(volatile u8 *)request[1];
    if (!password_valid(config)) return original(event,request);
    if (prepare(region,1)) return delegate(original,event,request,1);
    u8 channel=config[0],disable_ht=config[0x7d];
    /* Delegate channel selection to native ACS. Its AP tail must keep HT on. */
    config[0]=0;
    config[0x7d]=0;
    u32 result=original(event,request);
    config[0]=channel;
    config[0x7d]=disable_ht;
    return result;
}

__attribute__((used,noinline))
u32 wifi_aux_sta(u32 event,volatile u32 *request)
{
    native_handler original=(native_handler)0xb107225fu;
    u32 region=wifi_aux_region;
    /* Native invalid/busy guards must precede changes to the radio. */
    if (region>=Z72_WIFI_REGION_BODY || request[0]==0xffffffffu ||
        *(volatile u32 *)0xb1200820u) return original(event,request);
    if (prepare(region,0)) return delegate(original,event,request,1);
    return original(event,request);
}

/* Only the private image builder uses the regular module linker entry. */
u32 z72_main(const void *api)
{
    (void)api;
    return 0;
}
