/* SPDX-License-Identifier: MIT */
#ifndef Z72_CONFIG_H
#define Z72_CONFIG_H
#include "z72.h"
#include "wifi_regions.h"

/* C 1.70 synchronous file-object API, on the SD volume only. */
#define Z72_CFG_FN(a, result, ...) ((result (*)(__VA_ARGS__))(a))
#define Z72_CFG_OK 0x043a0080u
#define Z72_CFG_MAGIC 0x3147525au /* ZRG1 */
#define Z72_CFG_NO_GRID 0xffffffffu
#define Z72_DISP_PAGES 4u
#define Z72_DISP_CHOICES 5u
#define Z72_DISP_ENABLED 47u /* Four live presets plus the native information page. */
#define Z72_BATTERY_PERCENT_ON 64u /* Global style, disabled by default and in legacy CFGs. */
#define Z72_DISP_INFO_GROUPS 8u
#define Z72_DISP_INFO_ALL 255u
#define Z72_DISP_INFO_DETAIL 32u
#define Z72_DISP_INFO_FIELDS (Z72_DISP_INFO_ALL<<6)
enum { Z72_DISP_INFO=1u, Z72_DISP_GRID=2u, Z72_DISP_HISTOGRAM=4u,
       Z72_DISP_HORIZON=8u, Z72_DISP_ZEBRA=16u, Z72_DISP_CUSTOM=16u,
       Z72_DISP_INFORMATION=32u };
#define Z72_DISP_DEFAULTS {3u,1u,10u,21u}
#define Z72_AF_SHORTCUT_BUTTONS 5u
#define Z72_AF_SHORTCUT_TARGETS 10u
#define Z72_CONFIG_DEFAULTS {Z72_CFG_NO_GRID,1,0,1,31,Z72_DISP_DEFAULTS,{0,0,0,0,0},Z72_WIFI_REGION_BODY,0}

enum { Z72_DISP_SHUTTER=1u, Z72_DISP_APERTURE=2u, Z72_DISP_ISO=4u,
       Z72_DISP_SHOOTING=8u, Z72_DISP_RELEASE=16u,
       Z72_DISP_FOCUS=32u, Z72_DISP_STATUS=64u, Z72_DISP_FOCAL=128u };

/* Legacy information On means all groups, except Minimal's exposure fields.
 * Detailed selections retain the original five component bits and CFG size. */
static inline z72_u32 z72_disp_information(z72_u32 components,z72_u32 preset)
{
    if (!(components&Z72_DISP_INFO)) return 0;
    if (components&Z72_DISP_INFO_DETAIL) return (components>>6)&Z72_DISP_INFO_ALL;
    return preset==1 ? Z72_DISP_SHUTTER|Z72_DISP_APERTURE|Z72_DISP_ISO : Z72_DISP_INFO_ALL;
}

static inline z72_u32 z72_disp_components_valid(z72_u32 components)
{
    if (components&~(31u|Z72_DISP_INFO_DETAIL|Z72_DISP_INFO_FIELDS)) return 0;
    if (!(components&Z72_DISP_INFO_DETAIL)) return components<=31;
    return !!(components&Z72_DISP_INFO)==!!(components&Z72_DISP_INFO_FIELDS);
}

struct z72_config {
    z72_u32 grid_style;
    z72_u32 auto_rotate;
    z72_u32 photo_zebra_pattern;
    z72_u32 photo_zebra_threshold;
    z72_u32 disp_enabled;
    z72_u32 disp_components[Z72_DISP_PAGES];
    z72_u32 af_shortcuts[Z72_AF_SHORTCUT_BUTTONS];
    z72_u32 wifi_region;
    z72_u32 wifi_80mhz;
};

/* Menu callbacks run on the same task and save the whole record. */
static struct z72_config z72_config_current=Z72_CONFIG_DEFAULTS;
static z72_u32 z72_config_pending;

static inline struct z72_config z72_config_load(void)
{
    struct z72_config config=Z72_CONFIG_DEFAULTS;
    z72_u32 file[10],actual=0;
    _Alignas(64) z72_u32 data[48];
    for(z72_u32 i=0;i<10;++i) ((volatile z72_u32 *)file)[i]=0;
    Z72_CFG_FN(0x104d3c01,void,z72_u32 *,const char *)(file,"B:\\Z72APP.CFG");
    if(Z72_CFG_FN(0x104d3c05,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)!=Z72_CFG_OK)
        return config;
    z72_u32 size=file[6],result=1;
    if((size==16 || size==24 || size==32 || size==40 || size==80 || size==120 || size==128 || size==136) && file[7]==0) {
        /* The native reader needs one byte of capacity beyond EOF. */
        struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,data,size+1};
        Z72_CFG_FN(0x1034fa29,void,void *,z72_u32)(data,sizeof(data));
        result=Z72_CFG_FN(0x104d3d83,z72_u32,z72_u32 *,void *,z72_u32,z72_u32 *)(file,&request,size+1,&actual);
        Z72_CFG_FN(0x1034fa29,void,void *,z72_u32)(data,sizeof(data));
    }
    z72_u32 closed=Z72_CFG_FN(0x104d3c91,z72_u32,z72_u32 *)(file);
    if(result!=Z72_CFG_OK || closed!=Z72_CFG_OK || actual!=size ||
       data[0]!=Z72_CFG_MAGIC || data[3]!=~data[2]) return config;
    if(size==16 && data[1]==1 && data[2]<=4) config.grid_style=data[2];
    if(size==24 && data[1]==2 && (data[2]<=4 || data[2]==Z72_CFG_NO_GRID) &&
       data[4]<=1 && data[5]==~data[4])
        { config.grid_style=data[2]; config.auto_rotate=data[4]; }
    if(size==32 && data[1]==3 && (data[2]<=4 || data[2]==Z72_CFG_NO_GRID) &&
       data[4]<=1 && data[5]==~data[4] && data[6]<=1 && data[7]==~data[6])
        { config.grid_style=data[2]; config.auto_rotate=data[4]; config.photo_zebra_pattern=data[6]; }
    if(((size==40 && data[1]==4) || (size==80 && data[1]>=5 && data[1]<=9) ||
        (size==120 && data[1]==10) || (size==128 && (data[1]==11 || data[1]==12)) || (size==136 && data[1]==13)) &&
       (data[2]<=4 || data[2]==Z72_CFG_NO_GRID) &&
       data[4]<=1 && data[5]==~data[4] && data[6]<=2 && data[7]==~data[6] &&
       data[8]<=7 && data[9]==~data[8])
    {
        if(size>=80) {
            z72_u32 maximum=data[1]>=9 ? 127u : data[1]>=6 ? 63u : 31u;
            z72_u32 pages=data[1]>=6 ? Z72_DISP_ENABLED : 15u;
            if(data[10]>maximum || !(data[10]&pages) || data[11]!=~data[10]) return config;
            for(z72_u32 i=0;i<Z72_DISP_PAGES;++i)
                if((data[1]>=7 ? !z72_disp_components_valid(data[12+2*i]) : data[12+2*i]>31) ||
                   (data[1]==7 && data[12+2*i]>8191u) ||
                   data[13+2*i]!=~data[12+2*i]) return config;
            if(size>=120)
                for(z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i)
                    if(data[20+2*i]>Z72_AF_SHORTCUT_TARGETS ||
                       data[21+2*i]!=~data[20+2*i]) return config;
            if(size>=128 && (data[30]>=(data[1]==11 ? 2u : Z72_WIFI_REGION_COUNT) ||
                            data[31]!=~data[30])) return config;
            if(size==136 && (data[32]>1u || data[33]!=~data[32])) return config;
            config.disp_enabled=data[10];
            for(z72_u32 i=0;i<Z72_DISP_PAGES;++i) {
                z72_u32 components=data[12+2*i];
                /* v7 grouped focal length under Other information. Preserve
                 * that choice when splitting it into an independent switch. */
                if(data[1]==7 && (components&Z72_DISP_INFO_DETAIL) &&
                   (components&(Z72_DISP_STATUS<<6))) components|=Z72_DISP_FOCAL<<6;
                config.disp_components[i]=components;
            }
            if(size>=120)
                for(z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i)
                    config.af_shortcuts[i]=data[20+2*i];
            if(size>=128) config.wifi_region=data[1]>=12 ? data[30] :
                data[30] ? Z72_WIFI_REGION_MAINLAND : Z72_WIFI_REGION_BODY;
            if(size==136) config.wifi_80mhz=data[32];
        }
        config.grid_style=data[2]; config.auto_rotate=data[4];
        config.photo_zebra_pattern=data[6]; config.photo_zebra_threshold=data[8];
    }
    return config;
}

static inline z72_u32 z72_config_save(struct z72_config config)
{
    z72_u32 same=1;
    for(z72_u32 i=0;i<Z72_DISP_PAGES;++i)
        if(config.disp_components[i]!=z72_config_current.disp_components[i]) same=0;
    for(z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i)
        if(config.af_shortcuts[i]!=z72_config_current.af_shortcuts[i]) same=0;
    if(!z72_config_pending && same && config.disp_enabled==z72_config_current.disp_enabled &&
       config.grid_style==z72_config_current.grid_style &&
       config.auto_rotate==z72_config_current.auto_rotate &&
       config.wifi_region==z72_config_current.wifi_region &&
       config.wifi_80mhz==z72_config_current.wifi_80mhz &&
       config.photo_zebra_pattern==z72_config_current.photo_zebra_pattern &&
       config.photo_zebra_threshold==z72_config_current.photo_zebra_threshold) return Z72_CFG_OK;
    z72_config_current=config;
    z72_config_pending=1;
    /* Keep writing v1 when its grid-only record can represent the settings. */
    static const z72_u32 defaults[Z72_DISP_PAGES]=Z72_DISP_DEFAULTS;
    z72_u32 custom=config.disp_enabled!=31,detail=0,af=0;
    for(z72_u32 i=0;i<Z72_DISP_PAGES;++i) {
        if(config.disp_components[i]!=defaults[i]) custom=1;
        if(config.disp_components[i]&Z72_DISP_INFO_DETAIL) detail=1;
    }
    for(z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i)
        if(config.af_shortcuts[i]) af=1;
    z72_u32 version=config.wifi_80mhz ? 13 : config.wifi_region!=Z72_WIFI_REGION_BODY ? 12 : af ? 10 : config.disp_enabled&Z72_BATTERY_PERCENT_ON ? 9 :
                    detail ? 8 : config.disp_enabled&Z72_DISP_INFORMATION ? 6 : custom ? 5 :
                    config.photo_zebra_pattern || config.photo_zebra_threshold!=1 ? 4 :
                    config.auto_rotate && config.grid_style!=Z72_CFG_NO_GRID ? 1 : 2;
    z72_u32 size=version==1 ? 16 : version==2 ? 24 : version==4 ? 40 :
                 version==13 ? 136 : version==12 ? 128 : version==10 ? 120 : 80,actual=0;
    _Alignas(64) z72_u32 data[34];
    for(z72_u32 i=0;i<34;++i) ((volatile z72_u32 *)data)[i]=0;
    data[0]=Z72_CFG_MAGIC; data[1]=version;
    data[2]=config.grid_style; data[3]=~config.grid_style;
    data[4]=config.auto_rotate; data[5]=~config.auto_rotate;
    data[6]=config.photo_zebra_pattern; data[7]=~config.photo_zebra_pattern;
    data[8]=config.photo_zebra_threshold; data[9]=~config.photo_zebra_threshold;
    data[10]=config.disp_enabled; data[11]=~config.disp_enabled;
    for(z72_u32 i=0;i<Z72_DISP_PAGES;++i) {
        data[12+2*i]=config.disp_components[i];
        data[13+2*i]=~config.disp_components[i];
    }
    for(z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i) {
        data[20+2*i]=config.af_shortcuts[i];
        data[21+2*i]=~config.af_shortcuts[i];
    }
    data[30]=config.wifi_region;
    data[31]=~config.wifi_region;
    data[32]=config.wifi_80mhz;
    data[33]=~config.wifi_80mhz;
    struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,data,size};
    int opened=Z72_CFG_FN(0x105beb2b,int,const char *,z72_u32)("B:\\Z72APP.CFG",0);
    if(opened<0) return (z72_u32)opened;
    Z72_CFG_FN(0x1034f9ed,void,void *,z72_u32)(data,sizeof(data));
    z72_u32 result=Z72_CFG_FN(0x105bea79,z72_u32,int,void *,z72_u32,z72_u32 *)(opened,&request,size,&actual);
    z72_u32 closed=Z72_CFG_FN(0x105be9dd,z72_u32,int)(opened);
    if(result) return result;
    if(closed) return closed;
    if(actual!=size) return 1;
    z72_config_pending=0;
    return Z72_CFG_OK;
}
#endif
