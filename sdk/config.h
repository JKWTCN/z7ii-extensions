/* SPDX-License-Identifier: MIT */
#ifndef Z72_CONFIG_H
#define Z72_CONFIG_H
#include "z72.h"
#include "wifi_regions.h"
#include "playback_filter_config.h"
#include "composition_reference_format.h"

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
#define Z72_AF_SHORTCUT_NATIVE_TARGETS 10u
#define Z72_AF_SHORTCUT_WIDE_TARGETS 12u
#define Z72_CFG_SUBJECT_TRACKING 3u
#define Z72_AF_SHORTCUT_TRACKING_TARGET (Z72_AF_SHORTCUT_NATIVE_TARGETS+Z72_CFG_SUBJECT_TRACKING)
#ifdef Z72_SUBJECT_TRACKING
#define Z72_AF_SHORTCUT_TARGETS Z72_AF_SHORTCUT_TRACKING_TARGET
#else
#define Z72_AF_SHORTCUT_TARGETS Z72_AF_SHORTCUT_WIDE_TARGETS
#endif
#define Z72_SOUND_CLIP_COUNT 5u
#define Z72_SOUND_VOLUME_MAX 5u
enum { Z72_SOUND_BURST_FIRST=0, Z72_SOUND_BURST_EVERY, Z72_SOUND_BURST_RESTART, Z72_SOUND_BURST_MODE_COUNT };
#define Z72_WIDE_AF_MIN 5u
#define Z72_WIDE_AF_STOCK 20u
#define Z72_WIDE_AF_FIXED_GRID_MAX 25u
#define Z72_WIDE_AF_MAX 40u
#define Z72_WIDE_AF_DEFAULT_SIZE (Z72_WIDE_AF_STOCK|(Z72_WIDE_AF_STOCK<<8))
#define Z72_WIDE_AF_DEFAULTS {Z72_WIDE_AF_DEFAULT_SIZE|(1u<<16),Z72_WIDE_AF_DEFAULT_SIZE|(2u<<16)}
#define Z72_PIXEL_SHIFT_DEFAULT (2u<<16) /* Off, 4 shots, 2 s delay, 0 s interval. */
#define Z72_PIXEL_X_DEFAULT 11u
#define Z72_PIXEL_Y_DEFAULT 9u
#define Z72_PIXEL_X_MIN 8u
#define Z72_PIXEL_Y_MIN 6u
#define Z72_PIXEL_STEP_CHOICES 7u
#define Z72_PIXEL_HALF_STEPS_DEFAULT (Z72_PIXEL_X_DEFAULT|(Z72_PIXEL_Y_DEFAULT<<8))
#define Z72_REFERENCE_OPACITIES 5u
#define Z72_REFERENCE_BUTTON_MASK 7u
#define Z72_REFERENCE_HOLD (1u<<3)
#define Z72_REFERENCE_DEFAULT (2u<<8) /* Shortcut off, toggle, first image, 50%. */
#define Z72_CONFIG_DEFAULTS {Z72_CFG_NO_GRID,1,0,1,31,Z72_DISP_DEFAULTS,{0,0,0,0,0},Z72_WIFI_REGION_BODY,0,0,Z72_SOUND_BURST_EVERY,Z72_SOUND_VOLUME_MAX,0,Z72_WIDE_AF_DEFAULTS,0,Z72_PIXEL_SHIFT_DEFAULT,0,0,Z72_REFERENCE_DEFAULT,0,1,Z72_PIXEL_HALF_STEPS_DEFAULT}

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
    z72_u32 sound_choice;
    z72_u32 sound_burst_mode;
    z72_u32 sound_volume;
    z72_u32 playback_filter;
    z72_u32 wide_af[2]; /* Width, height and subject mode in successive bytes. */
    z72_u32 wide_af_selected; /* 0 = native, 1 = C1, 2 = C2, 3 = subject tracking. */
    z72_u32 pixel_shift; /* Enable, count, delay and interval indices in successive bytes. */
    z72_u32 tracking_assist;
    z72_u32 ftp_4mib;
    z72_u32 reference; /* Button (bits 0–2), hold (3), image (4–7), opacity (8–11). */
    z72_u32 gallery_enabled;
    z72_u32 rotate_inverted;
    z72_u32 pixel_half_steps; /* X and Y in successive bytes, in actuator command units. */
};

/* The freestanding build lowers copies of the settings record to these helpers. */
void __aeabi_memcpy4(void *dst,const void *src,z72_u32 size)
{
    /* Native memcpy uses an ARM entry point. */
    Z72_CFG_FN(0x10360facu,void *,void *,const void *,z72_u32)(dst,src,size);
}

void __aeabi_memcpy8(void *dst,const void *src,z72_u32 size)
    __attribute__((alias("__aeabi_memcpy4")));

/* Menu callbacks run on the same task and save the whole record. */
static struct z72_config z72_config_current=Z72_CONFIG_DEFAULTS;
static z72_u32 z72_config_pending;

static inline z72_u32 z72_wide_af_valid(z72_u32 value)
{
    z72_u32 width=value&255u,height=(value>>8)&255u;
    return width>=Z72_WIDE_AF_MIN && width<=Z72_WIDE_AF_MAX &&
           height>=Z72_WIDE_AF_MIN && height<=Z72_WIDE_AF_MAX && (value>>16)<=2;
}

static inline struct z72_config z72_config_load(void)
{
    struct z72_config config=Z72_CONFIG_DEFAULTS;
    z72_u32 file[10],actual=0;
    _Alignas(64) z72_u32 data[64];
    for(z72_u32 i=0;i<10;++i) ((volatile z72_u32 *)file)[i]=0;
    Z72_CFG_FN(0x104d3c01,void,z72_u32 *,const char *)(file,"B:\\Z72APP.CFG");
    if(Z72_CFG_FN(0x104d3c05,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)!=Z72_CFG_OK)
        return config;
    z72_u32 size=file[6],result=1;
    if((size==16 || size==24 || size==32 || size==40 || size==80 || size==120 || size==128 ||
        size==136 || size==160 || size==168 || size==184 || size==192 || size==200 || size==208 || size==216 ||
        size==224 || size==232 || size==240 || size==248) && file[7]==0) {
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
        (size==120 && data[1]==10) || (size==128 && (data[1]==11 || data[1]==12)) ||
        (size==136 && data[1]==13) || (size==160 && data[1]==14) || (size==168 && data[1]==15) ||
        (size==184 && data[1]==16) || (size==192 && data[1]==17) ||
        (size==200 && data[1]==18) || (size==208 && data[1]==19) ||
        (size==216 && data[1]==20) || (size==224 && data[1]==21) || (size==232 && data[1]==22) ||
        (size==240 && data[1]==23) || (size==248 && data[1]==24)) &&
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
                    if(data[20+2*i]>(data[1]>=17 ? Z72_AF_SHORTCUT_WIDE_TARGETS : Z72_AF_SHORTCUT_NATIVE_TARGETS) ||
                       data[21+2*i]!=~data[20+2*i]) return config;
            if(size>=128 && (data[30]>=(data[1]==11 ? 2u : Z72_WIFI_REGION_COUNT) ||
                            data[31]!=~data[30])) return config;
            if(size>=136 && (data[32]>1u || data[33]!=~data[32])) return config;
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
            if(size>=136) config.wifi_80mhz=data[32];
            if(size>=160 && data[34]<=Z72_SOUND_CLIP_COUNT && data[35]==~data[34] &&
               data[36]<Z72_SOUND_BURST_MODE_COUNT && data[37]==~data[36] &&
               data[38]>=1 && data[38]<=Z72_SOUND_VOLUME_MAX && data[39]==~data[38]) {
                config.sound_choice=data[34];
                config.sound_burst_mode=data[36];
                config.sound_volume=data[38];
            }
            if(size>=168 && data[41]==~data[40] && z72_playback_filter_valid(data[40]))
                config.playback_filter=data[40];
            z72_u32 selected=size>=192 && data[47]==~data[46] ? data[46] : 0;
#ifdef Z72_SUBJECT_TRACKING
            /* Low two bits select the AF mode. The next five mark tracking
             * shortcuts whose legacy slots are zero. Older readers can
             * still read the rest of the v17 record. */
            if(selected<(4u<<Z72_AF_SHORTCUT_BUTTONS)) {
                for(z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i)
                    if((selected&(4u<<i)) && !config.af_shortcuts[i])
                        config.af_shortcuts[i]=Z72_AF_SHORTCUT_TRACKING_TARGET;
                selected&=3u;
            }
            if(selected==Z72_CFG_SUBJECT_TRACKING) config.wide_af_selected=selected;
#endif
            if(size>=200 && data[49]==~data[48] && (data[48]&255u)<=1 &&
               ((data[48]>>8)&255u)<=3 && ((data[48]>>16)&255u)<=5 && (data[48]>>24)<=6)
                config.pixel_shift=data[48];
            if(size>=208 && data[50]<=1 && data[51]==~data[50]) config.tracking_assist=data[50];
            if(size>=216 && data[52]<=1 && data[53]==~data[52]) config.ftp_4mib=data[52];
            if(size>=224 && data[55]==~data[54] && (data[54]&Z72_REFERENCE_BUTTON_MASK)<=Z72_AF_SHORTCUT_BUTTONS &&
               ((data[54]>>4)&15u)<Z72_REFERENCE_SLOTS && (data[54]>>8)<Z72_REFERENCE_OPACITIES)
                config.reference=data[54];
            if(size>=232 && data[56]<=1 && data[57]==~data[56]) config.gallery_enabled=data[56];
            if(size>=240 && data[58]<=1 && data[59]==~data[58]) config.rotate_inverted=data[58];
            if(size==248 && data[61]==~data[60] &&
               (data[60]&255u)-Z72_PIXEL_X_MIN<Z72_PIXEL_STEP_CHOICES &&
               (data[60]>>8)-Z72_PIXEL_Y_MIN<Z72_PIXEL_STEP_CHOICES)
                config.pixel_half_steps=data[60];
            if(size>=184)
                for(z72_u32 i=0;i<2;++i) {
                    if(data[43+2*i]!=~data[42+2*i] || !z72_wide_af_valid(data[42+2*i])) continue;
                    config.wide_af[i]=data[42+2*i];
                    if(selected==i+1) config.wide_af_selected=selected;
                }
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
       config.sound_choice==z72_config_current.sound_choice &&
       config.sound_burst_mode==z72_config_current.sound_burst_mode &&
       config.sound_volume==z72_config_current.sound_volume &&
       config.playback_filter==z72_config_current.playback_filter &&
       config.wide_af[0]==z72_config_current.wide_af[0] &&
       config.wide_af[1]==z72_config_current.wide_af[1] &&
       config.wide_af_selected==z72_config_current.wide_af_selected &&
       config.pixel_shift==z72_config_current.pixel_shift &&
       config.pixel_half_steps==z72_config_current.pixel_half_steps &&
       config.tracking_assist==z72_config_current.tracking_assist &&
       config.ftp_4mib==z72_config_current.ftp_4mib &&
       config.reference==z72_config_current.reference &&
       config.gallery_enabled==z72_config_current.gallery_enabled &&
       config.rotate_inverted==z72_config_current.rotate_inverted &&
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
    z72_u32 wide_shortcut=0;
    for(z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i) {
        if(config.af_shortcuts[i]) af=1;
        if(config.af_shortcuts[i]>Z72_AF_SHORTCUT_NATIVE_TARGETS) wide_shortcut=1;
    }
    z72_u32 sound=config.sound_choice || config.sound_burst_mode!=Z72_SOUND_BURST_EVERY ||
        config.sound_volume!=Z72_SOUND_VOLUME_MAX;
    static const z72_u32 wide_defaults[2]=Z72_WIDE_AF_DEFAULTS;
    z72_u32 wide=config.wide_af[0]!=wide_defaults[0] || config.wide_af[1]!=wide_defaults[1];
    z72_u32 version=config.pixel_half_steps!=Z72_PIXEL_HALF_STEPS_DEFAULT ? 24 :
                    !config.rotate_inverted ? 23 : config.gallery_enabled ? 22 : config.reference!=Z72_REFERENCE_DEFAULT ? 21 :
                    config.ftp_4mib ? 20 : config.tracking_assist ? 19 :
                    config.pixel_shift!=Z72_PIXEL_SHIFT_DEFAULT ? 18 :
                    config.wide_af_selected || wide_shortcut ? 17 : wide ? 16 : config.playback_filter ? 15 : sound ? 14 :
                    config.wifi_80mhz ? 13 : config.wifi_region!=Z72_WIFI_REGION_BODY ? 12 :
                    af ? 10 : config.disp_enabled&Z72_BATTERY_PERCENT_ON ? 9 :
                    detail ? 8 : config.disp_enabled&Z72_DISP_INFORMATION ? 6 : custom ? 5 :
                    config.photo_zebra_pattern || config.photo_zebra_threshold!=1 ? 4 :
                    config.auto_rotate && config.grid_style!=Z72_CFG_NO_GRID ? 1 : 2;
    z72_u32 size=version==1 ? 16 : version==2 ? 24 : version==4 ? 40 :
                 version==24 ? 248 : version==23 ? 240 : version==22 ? 232 : version==21 ? 224 : version==20 ? 216 : version==19 ? 208 : version==18 ? 200 :
                 version==17 ? 192 : version==16 ? 184 : version==15 ? 168 :
                 version==14 ? 160 : version==13 ? 136 :
                 version==12 ? 128 : version==10 ? 120 : 80,actual=0;
    _Alignas(64) z72_u32 data[64];
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
    data[46]=config.wide_af_selected;
    for(z72_u32 i=0;i<Z72_AF_SHORTCUT_BUTTONS;++i) {
        z72_u32 target=config.af_shortcuts[i];
#ifdef Z72_SUBJECT_TRACKING
        if(target==Z72_AF_SHORTCUT_TRACKING_TARGET) {
            data[46]|=4u<<i;
            target=0;
        }
#endif
        data[20+2*i]=target;
        data[21+2*i]=~target;
    }
    data[30]=config.wifi_region;
    data[31]=~config.wifi_region;
    data[32]=config.wifi_80mhz;
    data[33]=~config.wifi_80mhz;
    data[34]=config.sound_choice;
    data[35]=~config.sound_choice;
    data[36]=config.sound_burst_mode;
    data[37]=~config.sound_burst_mode;
    data[38]=config.sound_volume;
    data[39]=~config.sound_volume;
    data[40]=config.playback_filter;
    data[41]=~config.playback_filter;
    for(z72_u32 i=0;i<2;++i) {
        data[42+2*i]=config.wide_af[i];
        data[43+2*i]=~data[42+2*i];
    }
    data[47]=~data[46];
    data[48]=config.pixel_shift;
    data[49]=~data[48];
    data[50]=config.tracking_assist;
    data[51]=~data[50];
    data[52]=config.ftp_4mib;
    data[53]=~data[52];
    data[54]=config.reference;
    data[55]=~data[54];
    data[56]=config.gallery_enabled;
    data[57]=~data[56];
    data[58]=config.rotate_inverted;
    data[59]=~data[58];
    data[60]=config.pixel_half_steps;
    data[61]=~data[60];
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
