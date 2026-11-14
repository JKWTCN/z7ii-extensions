/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_H
#define Z72_WIFI_H
#include "z72.h"
#include "wifi_regions.h"
#include "wifi_aux_blob.h"

#define Z72_WIFI_WORD(a) (*(volatile z72_u32 *)(a))
#define Z72_WIFI_BYTE(a) (*(volatile unsigned char *)(a))
#define Z72_WIFI_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
#define Z72_WIFI_INIT_NATIVE 0x10476fcdu
#define Z72_WIFI_INIT_SLOT_0 0xb2bf7b30u
#define Z72_WIFI_INIT_SLOT_1 0xb2bf7b44u
#define Z72_WIFI_AP_SLOT 0xb10d65f4u
#define Z72_WIFI_AP_NATIVE 0xb107182bu
#define Z72_WIFI_STA_SLOT 0xb10d664cu
#define Z72_WIFI_STA_NATIVE 0xb107225fu

/* This private ARM926 image runs through an uncached executable auxiliary
 * mapping. Main-CPU code must never branch into this Thumb1 callback. */
_Alignas(64) static unsigned char wifi_aux[WIFI_AUX_IMAGE_BYTES+WIFI_AUX_BSS_BYTES];

/* Read only by the native wireless task; changing the setting applies at the
 * next initialization, without restarting a connection from the GUI task. */
static volatile z72_u32 wifi_region=Z72_WIFI_REGION_BODY;
static volatile z72_u32 wifi_80mhz;
static const unsigned char wifi_countries[Z72_WIFI_REGION_BODY][3]=Z72_WIFI_COUNTRIES;
static const z72_u32 wifi_country_revisions[Z72_WIFI_REGION_BODY]=Z72_WIFI_COUNTRY_REVISIONS;
static z72_u32 wifi_aux_installed;

static void wifi_aux_publish(void)
{
    *(volatile z72_u32 *)(wifi_aux+WIFI_AUX_CONTROL_OFFSET)=
        __atomic_load_n(&wifi_region,__ATOMIC_ACQUIRE)|
        (__atomic_load_n(&wifi_80mhz,__ATOMIC_ACQUIRE) ? Z72_WIFI_80MHZ_ON : 0u);
    Z72_WIFI_CALL(0x1034f9edu,void,void *,z72_u32)(
        wifi_aux+WIFI_AUX_CONTROL_OFFSET,4);
}

static void wifi_set_region(z72_u32 region)
{
    if (region<Z72_WIFI_REGION_COUNT) {
        __atomic_store_n(&wifi_region,region,__ATOMIC_RELEASE);
        if (__atomic_load_n(&wifi_aux_installed,__ATOMIC_ACQUIRE)) wifi_aux_publish();
    }
}

static void wifi_set_80mhz(z72_u32 enabled)
{
    if (enabled<=1u) {
        __atomic_store_n(&wifi_80mhz,enabled,__ATOMIC_RELEASE);
        if (__atomic_load_n(&wifi_aux_installed,__ATOMIC_ACQUIRE)) wifi_aux_publish();
    }
}

static z72_u32 wifi_init(void)
{
    z72_u32 region=__atomic_load_n(&wifi_region,__ATOMIC_ACQUIRE);
    z72_u32 mode=Z72_WIFI_BYTE(0xb29a198du);
    z72_u32 enabled=region!=Z72_WIFI_REGION_BODY && mode==0;
    if (!enabled)
        return Z72_WIFI_CALL(Z72_WIFI_INIT_NATIVE,z72_u32,void)();

    /* Preserve the native callbacks and asynchronous initialization request.
     * The country getter remains the native prerequisite; its saved value and
     * the body NVM are never changed. Snapshot the region for this request;
     * its static country string survives later setting changes. */
    Z72_WIFI_CALL(0x1047b403u,z72_u32,z72_u32)(0x10476fa7u);
    Z72_WIFI_CALL(0x1047b445u,z72_u32,z72_u32)(0x10476f83u);
    z72_u32 native_country[3];
    if (!Z72_WIFI_CALL(0x103a49fdu,z72_u32,z72_u32 *)(native_country)) return 0;
    struct {
        z72_u32 first,second,country_enabled;
        const unsigned char *country;
        z72_u32 revision,context;
    } request={0,1,1,wifi_countries[region],wifi_country_revisions[region],0};
    z72_u32 result=Z72_WIFI_CALL(0x1047b475u,z72_u32,const void *,z72_u32)(
        &request,0x10476f61u);
    if (result) Z72_WIFI_BYTE(0xb29a198cu)=1;
    return result;
}

static z72_u32 wifi_ready(void)
{
    return Z72_WIFI_WORD(Z72_WIFI_INIT_SLOT_0)==Z72_WIFI_INIT_NATIVE &&
        Z72_WIFI_WORD(Z72_WIFI_INIT_SLOT_1)==Z72_WIFI_INIT_NATIVE &&
        (z72_u32)wifi_aux>=0xb1600000u &&
        Z72_WIFI_CALL(0x104a1385u,z72_u32,void)() &&
        Z72_WIFI_WORD(Z72_WIFI_AP_SLOT-4)==3 &&
        Z72_WIFI_WORD(Z72_WIFI_AP_SLOT)==Z72_WIFI_AP_NATIVE &&
        Z72_WIFI_WORD(Z72_WIFI_STA_SLOT-4)==14 &&
        Z72_WIFI_WORD(Z72_WIFI_STA_SLOT)==Z72_WIFI_STA_NATIVE;
}

static void wifi_install(void)
{
    for (z72_u32 i=0;i<WIFI_AUX_IMAGE_BYTES;++i) wifi_aux[i]=wifi_aux_blob[i];
    for (z72_u32 i=WIFI_AUX_IMAGE_BYTES;i<sizeof(wifi_aux);++i) wifi_aux[i]=0;
    for (z72_u32 i=0;i<WIFI_AUX_RELOCATION_COUNT;++i) {
        z72_u32 *target=(z72_u32 *)(wifi_aux+wifi_aux_relocations[i]);
        *target+=(z72_u32)wifi_aux;
    }
    *(z72_u32 *)(wifi_aux+WIFI_AUX_CONTROL_OFFSET)=
        __atomic_load_n(&wifi_region,__ATOMIC_ACQUIRE)|
        (__atomic_load_n(&wifi_80mhz,__ATOMIC_ACQUIRE) ? Z72_WIFI_80MHZ_ON : 0u);
    /* The loader cleans to PoU; publish this cross-CPU image to PoC too. */
    Z72_WIFI_CALL(0x1034f9edu,void,void *,z72_u32)(wifi_aux,sizeof(wifi_aux));
    Z72_WIFI_WORD(Z72_WIFI_AP_SLOT)=((z72_u32)wifi_aux+WIFI_AUX_AP_OFFSET)|1u;
    Z72_WIFI_WORD(Z72_WIFI_STA_SLOT)=((z72_u32)wifi_aux+WIFI_AUX_STA_OFFSET)|1u;
    __asm__ volatile("dsb sy" ::: "memory");
    /* The auxiliary table is cached there. Its native blocking cache RPC
     * invalidates both 32-byte lines before subsequent AP/STA requests. */
    z72_u32 invalidate[4]={Z72_WIFI_AP_SLOT&~31u,32,0,0};
    Z72_WIFI_CALL(0x104ba045u,z72_u32,z72_u32,z72_u32,void *,z72_u32)(
        1,0,invalidate,1);
    invalidate[0]=Z72_WIFI_STA_SLOT&~31u;
    Z72_WIFI_CALL(0x104ba045u,z72_u32,z72_u32,z72_u32,void *,z72_u32)(
        1,0,invalidate,1);
    __asm__ volatile("dmb ish" ::: "memory");
    Z72_WIFI_WORD(Z72_WIFI_INIT_SLOT_0)=(z72_u32)wifi_init;
    Z72_WIFI_WORD(Z72_WIFI_INIT_SLOT_1)=(z72_u32)wifi_init;
    __atomic_store_n(&wifi_aux_installed,1,__ATOMIC_RELEASE);
}
#endif
