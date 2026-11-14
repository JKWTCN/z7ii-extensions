/* SPDX-License-Identifier: MIT */
/* Executed by the auxiliary ARM926. Keep this image Thumb1 and independent
 * of main-CPU APIs. Region and bandwidth preference share one control word. */
#include "../sdk/wifi_regions.h"

typedef unsigned int u32;
typedef unsigned char u8;
typedef u32 (*native_handler)(u32,volatile u32 *);

volatile u32 wifi_aux_control=Z72_WIFI_REGION_BODY;
static const unsigned char countries[Z72_WIFI_REGION_BODY][3]=Z72_WIFI_COUNTRIES;
static const u32 country_revisions[Z72_WIFI_REGION_BODY]=Z72_WIFI_COUNTRY_REVISIONS;
/* Native query capacity is 410 bytes. Isolate the whole scratch from cache
 * lines containing main-CPU data, and keep it off the wireless task's stack. */
_Alignas(64) static u32 wifi_aux_channels[112];

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

static u32 primary_80(u32 raw)
{
    if (raw>0xffffu || (raw&0xf800u)!=0xe000u) return 0;
    u32 sideband=(raw>>8)&7u,center=raw&255u;
    if (sideband>=4u || center<7u) return 0;
    u32 primary=center+4u*sideband-6u;
    return primary>14u && primary<=255u ? primary : 0;
}

static u32 choose_80(u8 *channel)
{
    for (u32 i=0;i<104u;i++) wifi_aux_channels[i]=0;
    ((u8 *)wifi_aux_channels)[1]=0xe0;
    ((u8 *)wifi_aux_channels)[6]=100;
    if (((u32 (*)(const char *,void *,u32,u32,u32))0xb10bf373u)(
        "chanspecs",wifi_aux_channels,410,0,0)) return 0;
    u32 count=wifi_aux_channels[0],allowed=0;
    if (count>100u) return 0;
    for (u32 i=1;i<=count;i++) if (primary_80(wifi_aux_channels[i])) allowed=1;
    if (!allowed) return 0;

    u32 saved=0;
    if (((u32 (*)(u32,u32 *))0xb10b4513u)(0,&saved) || saved>4u) return 0;
    /* Native AP initialization already provides AP mode and a default SSID.
     * Match native Auto: disable spectrum management around ACS, then enable
     * it before starting the BSS. It must not remain disabled on success. */
    u32 level=0;
    u32 error=((u32 (*)(u32,u32 *))0xb10b4513u)(1,&level);
    if (!error) error=((u32 (*)(u32))0xb10afb99u)(0);
    unsigned short current=0;
    if (!error) error=((u32 (*)(unsigned short *))0xb10b2b93u)(&current);
    u32 primary=current&255u,candidate=0,rollback=0;
    if (!error) for (u32 i=1;i<=count;i++) {
        u32 raw=wifi_aux_channels[i];
        if (primary && primary_80(raw)==primary) { candidate=raw; break; }
    }
    if (candidate) {
        /* This native entry expects the PRIMARY channel, not the raw center. */
        error=((u32 (*)(u32))0xb10afb99u)((candidate&0xff00u)|primary);
        if (error) rollback=((u32 (*)(u32))0xb10afb99u)(current);
    }
    level=candidate && !error && !rollback ? 1u : saved;
    u32 restored=((u32 (*)(u32,u32 *))0xb10b4513u)(1,&level);
    if (restored || rollback) {
        /* The native spect helper can fail at DOWN before attempting UP. */
        if (restored) ((u32 (*)(u32,u32 *))0xb10b4513u)(1,&saved);
        ((u32 (*)(u32,u32))0xb10bf5cdu)(1,0);
        return restored ? restored : rollback;
    }
    if (!candidate || error) return 0;
    /* Spectrum restoration cycles DOWN/UP, so verify only AFTER that cycle. */
    unsigned short actual=0;
    error=((u32 (*)(unsigned short *,u32,u32))0xb10c041bu)(&actual,0,0);
    if (error || actual!=candidate) {
        rollback=((u32 (*)(u32))0xb10afb99u)(current);
        restored=((u32 (*)(u32,u32 *))0xb10b4513u)(1,&saved);
        if (rollback || restored) {
            ((u32 (*)(u32,u32))0xb10bf5cdu)(1,0);
            return rollback ? rollback : restored;
        }
        return 0;
    }
    *channel=(u8)primary;
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
    u32 control=wifi_aux_control;
    u32 region=control&Z72_WIFI_REGION_MASK;
    if (region>=Z72_WIFI_REGION_BODY || request[0]==0xffffffffu)
        return original(event,request);
    volatile u8 *config=(volatile u8 *)request[1];
    if (!password_valid(config)) return original(event,request);
    if (prepare(region,1)) return delegate(original,event,request,1);
    u8 selected=0;
    if ((control&Z72_WIFI_80MHZ_ON) && choose_80(&selected))
        return delegate(original,event,request,1);
    u8 channel=config[0],disable_ht=config[0x7d];
    /* >11 retains our verified 5 GHz choice; zero delegates to native Auto. */
    config[0]=selected;
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
    u32 control=wifi_aux_control;
    u32 region=control&Z72_WIFI_REGION_MASK;
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
