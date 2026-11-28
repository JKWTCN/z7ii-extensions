/* SPDX-License-Identifier: MIT */
#ifndef Z72_MOVIE_RAW_SOURCE_H
#define Z72_MOVIE_RAW_SOURCE_H

#include "z72.h"

enum {
    RAW_IRQ_SLOT=0xb2b5e098u, RAW_IRQ_NATIVE=0x1020a9b3u,
    RAW_IRQ=0x20068210u, RAW_MASK=0xb180681cu, PREVIEW=0x20069a00u,
    GEOMETRY=0x20069500u, CONFIG=0xb26b4984u, PAGE_TABLE=0xb2b74000u,
    CAPTURE_TYPE=0xb1864834u, PENDING_STOP=0xb26b38c8u,
    RAW_MOVIE_PHASE=0xb1811788u, RAW_ENCODER_PHASE=0xb181176cu,
#ifdef Z72_RAW_STANDBY
    RAW_TYPE=0x1d, RAW_FORMAT=6, RAW_WIDTH=1380, RAW_HEIGHT=778, RAW_STRIDE=2080,
    RAW_TAIL_OFFSET=0xc5200u,
#else
    RAW_TYPE=0x0a, RAW_FORMAT=2,
    RAW_WIDTH=2070, RAW_HEIGHT=1166, RAW_STRIDE=3120,
    RAW_TAIL_OFFSET=0x378200u,
#endif
    RAW_FRAME_BYTES=RAW_STRIDE*RAW_HEIGHT,
    RAW_FIRST_SLOT=0x54900000u, RAW_SLOT_STRIDE=0x220d000u, RAW_SLOT_COUNT=4,
    RAW_TOKEN_SHIFT=4, RAW_TOKEN_SLOT=7, RAW_TOKEN_PAIR=8, RAW_COPY_MS=60
};

enum {
    RAW_COPY_OK=1, RAW_WINDOW_EXPIRED, RAW_COPY_TIMEOUT, RAW_SOURCE_MAPPING,
    RAW_SAMPLE_CHANGED, RAW_PROFILE_CHANGED, RAW_UNSAFE_SLOT, RAW_SNAPSHOT_UNSTABLE,
    RAW_DMA_BUSY, RAW_DMA_ERROR
};

#if defined(Z72_RAW_RECHECK_SLOT) || defined(Z72_RAW_NEF_TOKEN_RECHECK)
#define Z72_RAW_STABLE_SNAPSHOT
#endif

struct z72_raw_status {
    z72_u32 observed_token, live_index, live_address, live_tail, pending_stop;
#ifdef Z72_RAW_STABLE_SNAPSHOT
    z72_u32 snapshot_retries;
#endif
};

#define Z72_RAW_WORD(a) (*(const volatile z72_u32 *)(a))
#define Z72_RAW_HALF(a) (*(const volatile unsigned short *)(a))
#define Z72_RAW_BYTE(a) (*(const volatile unsigned char *)(a))

#ifdef Z72_RAW_AUTO_25P
struct z72_raw_mode { z72_u32 type,width,height,stride,tail_offset,limit; };
static const struct z72_raw_mode z72_hd={RAW_TYPE,RAW_WIDTH,RAW_HEIGHT,RAW_STRIDE,RAW_TAIL_OFFSET,750};
#ifdef Z72_RAW_NEF_BURST_LIMIT
static const struct z72_raw_mode z72_uhd={0x10,3826,2158,5744,0xbd2400,Z72_RAW_NEF_BURST_LIMIT};
#elif defined(Z72_RAW_NEF_60S)
static const struct z72_raw_mode z72_uhd={0x10,3826,2158,5744,0xbd2400,750};
#else
static const struct z72_raw_mode z72_uhd={0x10,3826,2158,5744,0xbd2400,150};
#endif
static const struct z72_raw_mode *z72_mode=&z72_hd;
#define Z72_EXPECT(field,ordinary,reference) (z72_mode->field)
#else
#define Z72_EXPECT(field,ordinary,reference) Z72_RAW_PROFILE_VALUE(ordinary,reference)
#endif

static inline z72_u32 z72_raw_slot_address(z72_u32 slot)
{
    return RAW_FIRST_SLOT+slot*RAW_SLOT_STRIDE;
}

#ifdef Z72_RAW_STANDBY
static inline int z72_raw_movie_idle(void)
{
    return !Z72_RAW_BYTE(RAW_MOVIE_PHASE) && !Z72_RAW_BYTE(RAW_ENCODER_PHASE);
}
#endif

#ifdef Z72_RAW_REFERENCE_PROFILE
static z72_u32 z72_raw_reference_linear;
#define Z72_RAW_PROFILE_MODE const int linear=__atomic_load_n(&z72_raw_reference_linear,__ATOMIC_ACQUIRE)
#define Z72_RAW_PROFILE_VALUE(ordinary,reference) (linear ? (reference) : (ordinary))
#else
#define Z72_RAW_PROFILE_MODE
#define Z72_RAW_PROFILE_VALUE(ordinary,reference) (ordinary)
#endif

#ifdef Z72_RAW_UHD_2997
/* Latch before opening the clip. A rate change during recording invalidates the source. */
static z72_u32 z72_clip_fps_num;
static inline int z72_fractional_fps_num(z72_u32 num)
{
#ifdef Z72_RAW_UHD_23976
    if (num==24000u) return 1;
#endif
#ifdef Z72_RAW_PCM_HD_HIGH_RATE
    if (num==60000u) return 1;
#endif
    return num==30000u;
}
static inline z72_u32 z72_raw_mode_fps_num(const struct z72_raw_mode *mode,z72_u32 num,z72_u32 den)
{
    (void)mode;
    if (z72_fractional_fps_num(num) && den==1001u
#ifdef Z72_RAW_PCM_HD_HIGH_RATE
        && (num!=60000u || mode==&z72_hd)
#endif
#ifndef Z72_RAW_PCM_AUTO_RATE
        && mode==&z72_uhd
#endif
    ) return num;
#ifdef Z72_RAW_PCM_HD_HIGH_RATE
    if (mode==&z72_hd && den && den<=0xffffffffu/50u && num==50u*den) return 50u;
#endif
    return den && den<=0xffffffffu/25u && num==25u*den ? 25u : 0u;
}

static inline z72_u32 z72_source_fps_num(void)
{
    return z72_raw_mode_fps_num(z72_mode,Z72_RAW_WORD(0xb1f5995cu),Z72_RAW_WORD(0xb1f59960u));
}
#endif

static inline int z72_raw_profile_matches(void)
{
    Z72_RAW_PROFILE_MODE;
#ifdef Z72_RAW_UHD_2997
    if (!z72_clip_fps_num || z72_source_fps_num()!=z72_clip_fps_num) return 0;
#elif defined(Z72_RAW_AUTO_25P)
    const z72_u32 den=Z72_RAW_WORD(0xb1f59960u);
    if (!den || den>0xffffffffu/25u || Z72_RAW_WORD(0xb1f5995cu)!=25u*den) return 0;
#endif
#ifdef Z72_RAW_STANDBY
    if (Z72_RAW_PROFILE_VALUE(!z72_raw_movie_idle(),0)) return 0;
#endif
    const z72_u32 stride=Z72_EXPECT(stride,RAW_STRIDE,3120u);
    if (Z72_RAW_BYTE(CAPTURE_TYPE)!=Z72_EXPECT(type,RAW_TYPE,0x0au) || Z72_RAW_WORD(PENDING_STOP) ||
        Z72_RAW_WORD(GEOMETRY+0x2cu)!=2u || Z72_RAW_WORD(GEOMETRY+0x14u)!=Z72_RAW_PROFILE_VALUE(RAW_FORMAT,2u) ||
        Z72_RAW_WORD(GEOMETRY+0x18u)!=(Z72_EXPECT(width,RAW_WIDTH,2070u)<<16) ||
        Z72_RAW_WORD(GEOMETRY+0x1cu)!=(Z72_EXPECT(height,RAW_HEIGHT,1166u)<<16) ||
        Z72_RAW_WORD(PREVIEW+8u)!=stride || Z72_RAW_BYTE(CONFIG)!=2u ||
        Z72_RAW_HALF(CONFIG+0x1au)!=stride) return 0;
    for (z72_u32 i=0;i<2u*RAW_SLOT_COUNT;++i) {
        z72_u32 expected=z72_raw_slot_address(i%RAW_SLOT_COUNT);
        if (Z72_RAW_WORD(PREVIEW+0x50u+4u*i)!=expected ||
            Z72_RAW_WORD(CONFIG+0x24u+4u*i)!=expected) return 0;
    }
    return 1;
}

static inline z72_u32 z72_raw_completed_slot(void)
{
    Z72_RAW_PROFILE_MODE;
    if (!z72_raw_profile_matches()) return 0;
    const z72_u32 slot=Z72_RAW_WORD(PREVIEW+0x74u);
    return (slot<RAW_SLOT_COUNT && Z72_RAW_WORD(PREVIEW+0x70u)==3u &&
            Z72_RAW_WORD(PREVIEW+0x78u)==z72_raw_slot_address(slot) &&
            Z72_RAW_WORD(PREVIEW+0x7cu)==z72_raw_slot_address(slot)+Z72_EXPECT(tail_offset,RAW_TAIL_OFFSET,0x378200u) &&
            Z72_RAW_WORD(PREVIEW+0x74u)==slot) ? slot+1u : 0;
}

static inline z72_u32 z72_raw_next_tail(z72_u32 previous,z72_u32 slot)
{
    const z72_u32 previous_slot=previous&RAW_TOKEN_SLOT;
    const z72_u32 flags=slot && previous_slot && slot==previous_slot%RAW_SLOT_COUNT+1u ?
        slot|RAW_TOKEN_PAIR : slot;
    return (((previous>>RAW_TOKEN_SHIFT)+1u)<<RAW_TOKEN_SHIFT)|flags;
}

static inline int z72_raw_identity_span(z72_u32 address,z72_u32 bytes)
{
    for (z72_u32 section=address>>20;section<=(address+bytes-1u)>>20;++section) {
        z72_u32 entry=Z72_RAW_WORD(PAGE_TABLE+4u*section)^(section<<20);
        if (entry!=0x14422u && entry!=0x14426u) return 0;
    }
    return 1;
}

static inline z72_u32 z72_raw_copy_status_once(const z72_u32 *tail_token,z72_u32 anchor,
                                              z72_u32 start,struct z72_raw_status *status,
                                              z72_u32 max_ms)
{
    const z72_u32 observed=__atomic_load_n(tail_token,__ATOMIC_ACQUIRE);
    const z72_u32 advance=(observed>>RAW_TOKEN_SHIFT)-(anchor>>RAW_TOKEN_SHIFT);
    const z72_u32 anchor_slot=(anchor&RAW_TOKEN_SLOT)-1u;
    status->observed_token=observed;
    status->live_index=Z72_RAW_WORD(PREVIEW+0x74u);
    status->live_address=Z72_RAW_WORD(PREVIEW+0x78u);
    status->live_tail=Z72_RAW_WORD(PREVIEW+0x7cu);
    status->pending_stop=Z72_RAW_WORD(PENDING_STOP);
    if (((z72_u32 (*)(void))0x1062e59du)()-start>=max_ms) return RAW_COPY_TIMEOUT;
    if (!z72_raw_profile_matches() || status->pending_stop) return RAW_PROFILE_CHANGED;
#ifdef Z72_RAW_STABLE_SNAPSHOT
    /* The DMA exposes the index and address as separate registers. */
    if (Z72_RAW_WORD(PREVIEW+0x74u)!=status->live_index ||
        Z72_RAW_WORD(PREVIEW+0x78u)!=status->live_address ||
        __atomic_load_n(tail_token,__ATOMIC_ACQUIRE)!=observed) return 0;
#endif
    if (!(observed&RAW_TOKEN_PAIR) || advance>1u ||
        (observed&RAW_TOKEN_SLOT)!=(anchor_slot+advance)%RAW_SLOT_COUNT+1u
#ifndef Z72_RAW_STABLE_SNAPSHOT
        || __atomic_load_n(tail_token,__ATOMIC_ACQUIRE)!=observed
#endif
    ) return RAW_WINDOW_EXPIRED;
    /* The two slots after the source may be visible. Reject the third. */
    const z72_u32 live=status->live_index;
    const z72_u32 offset=(live+RAW_SLOT_COUNT-anchor_slot)%RAW_SLOT_COUNT;
    if (live>=RAW_SLOT_COUNT || offset<advance || offset>1u ||
        status->live_address!=z72_raw_slot_address(live)
#ifndef Z72_RAW_STABLE_SNAPSHOT
        || Z72_RAW_WORD(PREVIEW+0x74u)!=live || Z72_RAW_WORD(PREVIEW+0x78u)!=status->live_address
#endif
    ) return RAW_UNSAFE_SLOT;
    return RAW_COPY_OK;
}

static inline z72_u32 z72_raw_copy_status_limit(const z72_u32 *tail_token,z72_u32 anchor,
                                               z72_u32 start,struct z72_raw_status *status,
                                               z72_u32 max_ms)
{
#ifdef Z72_RAW_STABLE_SNAPSHOT
    for (z72_u32 attempt=0;attempt<3u;++attempt) {
        const z72_u32 result=z72_raw_copy_status_once(tail_token,anchor,start,status,max_ms);
        if (result) return result;
        ++status->snapshot_retries;
    }
#ifdef Z72_RAW_NEF_TOKEN_RECHECK
    return RAW_SNAPSHOT_UNSTABLE;
#else
    return RAW_UNSAFE_SLOT;
#endif
#else
    return z72_raw_copy_status_once(tail_token,anchor,start,status,max_ms);
#endif
}

static inline z72_u32 z72_raw_copy_status(const z72_u32 *tail_token,z72_u32 anchor,
                                         z72_u32 start,struct z72_raw_status *status)
{
    return z72_raw_copy_status_limit(tail_token,anchor,start,status,RAW_COPY_MS);
}

#undef Z72_RAW_WORD
#undef Z72_RAW_HALF
#undef Z72_RAW_BYTE
#undef Z72_EXPECT
#undef Z72_RAW_PROFILE_MODE
#undef Z72_RAW_PROFILE_VALUE
#undef Z72_RAW_STABLE_SNAPSHOT

#endif
