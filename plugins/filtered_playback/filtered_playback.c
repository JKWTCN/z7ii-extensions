/* SPDX-License-Identifier: MIT */
/* C 1.70 filtered playback. Catalogue queries run in the image-control task. */
#include "config.h"
#define NATIVE CALL
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#include "playback_shadow.h"
#ifdef Z72_PHOTO_PLAYBACK
#include "photo_playback.h"
#endif

static void copy_bytes(void *to,const void *from,z72_u32 size)
{
    NATIVE(0x10360facu,void *,void *,const void *,z72_u32)(to,from,size);
}

enum {
    RATING_ALL=0x7f, METADATA_FILTER=0x8000, NATIVE_ATTRIBUTES=0xfff,
    NATIVE_FOUND=0x042b0080, NATIVE_NO_MATCH=0x042b0083
};
static volatile z72_u32 filter_settings;
/* Image-control dispatch is serialized. All queries in one request use the same settings. */
static z72_u32 playback_settings;
static z72_u32 ordinal_in_progress;
static _Alignas(4) unsigned char ordinal_view[0x3f0];

/* Extend the release build's no-op normalized-query validation calls. Admit
 * the rating condition before native seeking/counting, including native
 * no-match/fallback decisions. Each descriptor is a native mutable RAM query.
 * Mode 1 is ordinary playback, including its thumbnail grid. Selection/editing,
 * slideshows and capture-source playback retain their original conditions. */
static z72_u32 ordinary_query(const unsigned char *spec)
{
    return (playback_settings&Z72_FILTER_ENABLED) && spec[0x1a]==1 &&
           (spec[0x26]==0 || spec[0x26]==4) && !WORD(0xb188b580u);
}

static void query_filter(unsigned char *spec)
{
    if ((z72_u32)spec<0xb0000000u || (z72_u32)spec>0xbfffffcfu || !ordinary_query(spec))
        return;
    unsigned short *rating=(unsigned short *)(spec+0x28);
    z72_u32 mask=(playback_settings>>Z72_RATING_SHIFT)&RATING_ALL;
    z72_u32 media=(playback_settings>>Z72_MEDIA_SHIFT)&3;
    if (playback_settings!=Z72_FILTER_ENABLED) {
        *rating=(spec[0x26]==4 ? *rating : RATING_ALL)&(mask ? mask : RATING_ALL);
        spec[0x26]=4;
    }
    if (media) {
        /* Preserve the native descriptor's independent folder modifier. */
        unsigned short *formats=(unsigned short *)(spec+0x14);
        *formats&=0x400u|(media==1 ? 0x07u : 0x70u);
    }
}

static void build_filter(unsigned char *spec,void *descriptor,unsigned short *filter)
{
    NATIVE(translate(0x1063e777u),void,unsigned char *,void *,unsigned short *)(spec,descriptor,filter);
    if (ordinary_query(spec) && (((playback_settings>>Z72_PROTECTION_SHIFT)&3) ||
        playback_settings>>Z72_DATE_SHIFT)) filter[1]|=METADATA_FILTER;
}

static z72_u32 metadata_matches(z72_u32 id)
{
    z72_u32 protection=(playback_settings>>Z72_PROTECTION_SHIFT)&3;
    z72_u32 date=playback_settings>>Z72_DATE_SHIFT;
    /* Read the native catalogue object's protection bit and packed capture date. */
    void *object=NATIVE(0x1063ea8bu,void *,z72_u32)(id);
    if (!object) return 0;
    z72_u32 matches=(!protection ||
        NATIVE(0x10567e19u,z72_u32,void *)(object)==(protection==1));
    if (matches && date)
        matches=*NATIVE(0x10567f05u,const unsigned short *,void *)(object)==date;
    NATIVE(0x1056788bu,void,void *)(object);
    return matches;
}

static z72_u32 seek_filtered(const void *descriptor,const unsigned short *filter,z72_u32 direction,z72_u32 *id)
{
    const z72_u32 seek=0x106d3285u;
    if (!filter || !(filter[1]&METADATA_FILTER))
        return NATIVE(seek,z72_u32,const void *,const unsigned short *,z72_u32,z72_u32 *)(descriptor,filter,direction,id);
    const unsigned short native_filter[]={filter[0],filter[1]&~METADATA_FILTER};
    const unsigned short *predicate=(native_filter[0]==RATING_ALL && native_filter[1]==NATIVE_ATTRIBUTES) ?
        0 : native_filter;
    z72_u32 candidate=*id;
    for (;;) {
        z72_u32 previous=candidate;
        z72_u32 result=NATIVE(seek,z72_u32,const void *,const unsigned short *,z72_u32,z72_u32 *)(
            descriptor,predicate,direction,&candidate);
        if (result!=NATIVE_FOUND) return result;
        if ((direction==0 && candidate<=previous) ||
            (direction==1 && previous && candidate>=previous)) return NATIVE_NO_MATCH;
        if (metadata_matches(candidate)) {
            *id=candidate;
            return NATIVE_FOUND;
        }
        if (direction>=2) return NATIVE_NO_MATCH;
    }
}

static z72_u32 count_filtered(const void *descriptor,const unsigned short *filter)
{
    if (!filter || !(filter[1]&METADATA_FILTER))
        return NATIVE(0x106d32cdu,z72_u32,const void *,const unsigned short *)(descriptor,filter);
    z72_u32 id=0,count=0;
    while (seek_filtered(descriptor,filter,0,&id)==NATIVE_FOUND) ++count;
    return count;
}

static z72_u32 *ordinal_workspace(void)
{
    return ordinal_in_progress ? (z72_u32 *)ordinal_view : NATIVE(0x1060be9bu,z72_u32 *,void)();
}

static const void *ordinal_view_state(void)
{
    return ordinal_in_progress ? ordinal_view : NATIVE(0x1063e2f3u,const void *,void)();
}

static z72_u32 use_filtered_ordinal(z72_u32 mode)
{
    return mode==1 && (playback_settings&Z72_FILTER_ENABLED) &&
           !WORD(0xb188b580u) && !WORD(0xb188b588u);
}

static z72_u32 directory_ordinal(z72_u32 id,const unsigned char *spec)
{
    /* The native ordinal helper derives its file-type mask from this view mode.
     * Ordinary playback doesn't initialize the shared view's mode field. Use
     * a private ordinary view for both that read and the per-folder search. */
    ordinal_view[0x3bc]=1;
    ordinal_in_progress=1;
    z72_u32 index=NATIVE(translate(0x10669f53u),z72_u32,const z72_u32 *,const unsigned char *)(&id,spec);
    ordinal_in_progress=0;
    return index;
}

__attribute__((used,noinline)) static z72_u32 ordinal_in_mode(z72_u32 id,z72_u32 mode)
{
    if (!id || !use_filtered_ordinal(mode)) return NATIVE(0x10667287u,z72_u32,z72_u32)(id);
    unsigned char spec[0x30] __attribute__((aligned(4)));
    NATIVE(0x10361035u,void,void *,z72_u32)(spec,sizeof(spec));
    spec[0x1a]=1;
    return directory_ordinal(id,spec);
}

static z72_u32 current_ordinal(z72_u32 id)
{
    return ordinal_in_mode(id,BYTE(0xb188b57bu));
}

/* Initial playback calculates the position before publishing its mode. The
 * call at 1066d32c already has the effective mode in its stack query. */
__attribute__((naked)) static z72_u32 initial_ordinal(z72_u32 id __attribute__((unused)))
{
    __asm__ volatile("ldrb.w r1,[sp,#0x1a]\n b.w ordinal_in_mode");
}

static z72_u32 requested_ordinal(const z72_u32 *view,unsigned char *spec)
{
    /* The index-only request passes mode zero even during ordinary playback. */
    if (!spec[0x1a] && use_filtered_ordinal(BYTE(0xb188b57bu)))
        return ordinal_in_mode(view[0],1);
    if (use_filtered_ordinal(spec[0x1a])) return directory_ordinal(view[0],spec);
    return NATIVE(translate(0x10669f53u),z72_u32,const z72_u32 *,unsigned char *)(view,spec);
}

static z72_u32 filter_target(z72_u32 site,z72_u32 target)
{
    if (site==0x1066719au) return (z72_u32)ordinal_workspace;
    if (target==0x106d3285u) return (z72_u32)seek_filtered;
    if (target==0x106d32cdu) return (z72_u32)count_filtered;
#ifdef Z72_PHOTO_PLAYBACK
    return photo_play_target(target);
#else
    return target;
#endif
}

static __attribute__((noinline)) z72_u32 prepare_native(void)
{
    if (playback_shadow_prepare(filter_target)) return 1;
    z72_u32 stub=(z72_u32)native_code+FILTER_NATIVE_BYTES+COUNT(filter_branches)*8;
    WORD(stub)=0xf000f8dfu; WORD(stub+4)=(z72_u32)query_filter;
    for (z72_u32 i=0;i<COUNT(filter_query_calls);++i)
        branch((z72_u32)native_code+filter_query_calls[i],stub,1);
    stub+=8;
    WORD(stub)=0xf000f8dfu; WORD(stub+4)=(z72_u32)build_filter;
    static const z72_u32 descriptor_calls[]={
        0x1063e80a,0x1063e85a,0x1063e8ce,0x1063e976,0x1063ea0e,0x1063eb68,0x1063fcc8
    };
    for (z72_u32 i=0;i<COUNT(descriptor_calls);++i) branch(translate(descriptor_calls[i]),stub,1);
    const z72_u32 ordinal_calls[][2]={
        {0x10669f7au,(z72_u32)ordinal_view_state},
        {0x106672dcu,(z72_u32)current_ordinal},
        {0x1066d32cu,(z72_u32)initial_ordinal},
        {0x1067019cu,(z72_u32)requested_ordinal},
        {0x106702bau,(z72_u32)requested_ordinal},
    };
    for (z72_u32 i=0;i<COUNT(ordinal_calls);++i) {
        stub+=8;
        WORD(stub)=0xf000f8dfu;
        WORD(stub+4)=ordinal_calls[i][1];
        branch(translate(ordinal_calls[i][0]),stub,1);
    }
#ifdef Z72_PHOTO_PLAYBACK
    photo_play_prepare();
#endif
    playback_shadow_sync();
    return 0;
}

static void playback_dispatch(const z72_u32 *message)
{
    playback_settings=__atomic_load_n(&filter_settings,__ATOMIC_ACQUIRE);
#ifdef Z72_PHOTO_PLAYBACK
    z72_u32 entry=translate(PLAYBACK_TASK_STOCK);
#else
    z72_u32 entry=(playback_settings&Z72_FILTER_ENABLED) && playback_settings!=Z72_FILTER_ENABLED ?
        translate(PLAYBACK_TASK_STOCK) : PLAYBACK_TASK_STOCK;
#endif
    NATIVE(entry,void,const z72_u32 *)(message);
}

#include "filtered_playback_menu.h"

static int filtered_playback_ready(void)
{
    return !check_menu() && !prepare_native();
}

static void filtered_playback_install(void)
{
    __atomic_store_n(&filter_settings,z72_config_current.playback_filter,__ATOMIC_RELEASE);
    photo_play_set_enabled(z72_config_current.rotate_inverted);
    photo_draw_install();
    install_menu();
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(PLAYBACK_TASK_SLOT)=(z72_u32)playback_dispatch;
#ifdef Z72_PHOTO_PLAYBACK
    photo_prefill_install();
#endif
    __asm__ volatile("dmb sy" ::: "memory");
}

#undef PAGE
#undef COUNT
#undef NATIVE
