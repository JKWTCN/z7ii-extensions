/* SPDX-License-Identifier: MIT */
#ifndef Z72_PHOTO_PLAYBACK_H
#define Z72_PHOTO_PLAYBACK_H
#include "playback_shadow.h"
#include "photo_playback_metadata.h"

#define PHOTO_PLAY_CALL(a,result,...) ((result (*)(__VA_ARGS__))(a))

static z72_u32 photo_play_inverted=1;

static inline z72_u32 photo_play_enabled(void)
{
    return __atomic_load_n(&photo_play_inverted,__ATOMIC_ACQUIRE);
}

static inline void photo_play_set_enabled(z72_u32 enabled)
{
    __atomic_store_n(&photo_play_inverted,enabled,__ATOMIC_RELEASE);
}

struct photo_play_cache {
    z72_u32 id,turn;
};

/* Owned by the ordinary playback task. */
static struct photo_play_cache photo_selected;

static z72_u32 photo_play_read(z72_u32 id,z72_u32 *turn)
{
    /* Borrow the native file header while its parser lock is held. */
    const z72_u32 *ops=PHOTO_PLAY_CALL(0x10529e9du,const z72_u32 *,z72_u32)(0);
    z72_u32 owner=PHOTO_PLAY_CALL(ops[1],z72_u32,void)(),handle=0,size=0,loaded=0;
    if (!PHOTO_PLAY_CALL(ops[2],z72_u32,z72_u32,z72_u32,z72_u32 *)(id,owner,&handle)) return 0;
    if (PHOTO_PLAY_CALL(0x10529ebbu,z72_u32,z72_u32,z72_u32 *)(handle,&size)) {
        const unsigned char *data=0;
        PHOTO_PLAY_CALL(0x10529f1bu,void,const unsigned char **,z72_u32 *)(&data,&size);
        if (data && size && size<=0x40000u) loaded=photo_header_turn(data,size,turn);
    }
    owner=PHOTO_PLAY_CALL(ops[1],z72_u32,void)();
    PHOTO_PLAY_CALL(ops[3],z72_u32,z72_u32,z72_u32,z72_u32)(id,owner,handle);
    return loaded;
}

static z72_u32 photo_play_lookup(const z72_u32 *image,struct photo_play_cache *cache)
{
    z72_u32 stock=PHOTO_PLAY_CALL(0x1060c647u,z72_u32,const z72_u32 *)(image);
    if (stock || !photo_play_enabled()) return stock;
    z72_u32 id=image[0];
    if ((id&15u)>3 || !PHOTO_PLAY_CALL(0x105094e5u,z72_u32,z72_u32)(id)) return 0;
    if (id==cache->id) return cache->turn;
    if (!photo_play_read(id,&cache->turn)) return 0;
    cache->id=id;
    return cache->turn;
}

static z72_u32 photo_play_orientation(const z72_u32 *image)
{
    return photo_play_lookup(image,&photo_selected);
}

static void photo_play_invalidate(void)
{
    PHOTO_PLAY_CALL(0x1060c6bfu,void,void)();
    photo_selected.id=0;
}

static inline z72_u32 photo_play_target(z72_u32 target)
{
    if (target==0x1060c647u) return (z72_u32)photo_play_orientation;
    if (target==0x1060c6bfu) return (z72_u32)photo_play_invalidate;
    return target;
}

static z72_u32 photo_play_map_current(z72_u32 turn,z72_u32 skip)
{
    /* Play may have opened this file before the SD module took over the task. */
    if (!turn && !skip && photo_play_enabled() &&
        photo_play_orientation((const z72_u32 *)0xb29d1d58u)==2) turn=2;
    return PHOTO_PLAY_CALL(translate(0x10669f51u),z72_u32,z72_u32,z72_u32)(turn,skip);
}

static inline void photo_play_prepare(void)
{
    /* Inverted photos use index 2 regardless of the native Rotate tall setting. */
    unsigned char *rotations=(unsigned char *)translate(0x119e0f98u);
    rotations[2]=rotations[6]=2;
    write_call(translate(0x10661070u),(z72_u32)native_code+sizeof(native_code)-8,
               (z72_u32)photo_play_map_current);
}

/* Message 0x04130081 builds a preloaded image on a separate native task. */
enum {
    PHOTO_PREFILL_SLOT=0xb29d4e38u, PHOTO_PREFILL_ENTRY=0x1076ef27u,
    PHOTO_PREFILL_START=0x1076ec20u, PHOTO_PREFILL_BYTES=0x36cu,
    PHOTO_PREFILL_HASH=0x6d949022u
};

static const struct { z72_u32 offset,target; } photo_prefill_calls[]={
    {0x014u,0x10361035u}, {0x07au,0x106602adu}, {0x0cau,0x10360f44u},
    {0x114u,0x10360f44u}, {0x11cu,0x106604cdu}, {0x122u,0x10669f37u},
    {0x12eu,0x10669e9fu}, {0x138u,0x10660377u}, {0x14cu,0x10669f37u},
    {0x154u,0x10660343u}, {0x168u,0x1060b369u}, {0x17au,0x1063e283u},
    {0x184u,0x1065ff13u}, {0x18au,0x103dc539u}, {0x18eu,0x1065feb9u},
    {0x19cu,0x1063e2f9u}, {0x1d2u,0x1063e26bu}, {0x1dau,0x1063ec15u},
    {0x1deu,0x106601f9u}, {0x1e6u,0x1063f0c1u}, {0x1f8u,0x1063e283u},
    {0x20au,0x1060beabu}, {0x210u,0x1063e283u}, {0x220u,0x1063e283u},
    {0x244u,0x1060b4efu}, {0x270u,0x106091f9u}, {0x288u,0x1066a441u},
    {0x29cu,0x1066beebu}, {0x2b2u,0x1060b489u}, {0x2beu,0x1060c1e7u},
    {0x2c6u,0x106607b9u}, {0x2ceu,0x1060c269u}, {0x2d4u,0x106667dbu},
    {0x2d8u,0x1063e283u}, {0x2f4u,0x1063e2b9u}, {0x2f8u,0x1052ae01u},
    {0x2fcu,0x1052ad91u}, {0x31cu,0x10361035u}, {0x328u,0x1060b18du},
};
static _Alignas(32) unsigned char photo_prefill_code[PHOTO_PREFILL_BYTES+
    sizeof(photo_prefill_calls)/sizeof(*photo_prefill_calls)*8];
static z72_u32 photo_prefill_mapped;

static z72_u32 photo_prefill_orientation(const z72_u32 *image)
{
    struct photo_play_cache cache={0};
    z72_u32 turn=photo_play_lookup(image,&cache);
    z72_u32 skip=PHOTO_PLAY_CALL(0x1063f1a1u,z72_u32,z72_u32)(image[0]);
    photo_prefill_mapped=PHOTO_PLAY_CALL(translate(0x10669f51u),z72_u32,z72_u32,z72_u32)(turn,skip);
    return photo_prefill_mapped;
}

static z72_u32 photo_prefill_draw_orientation(void)
{
    /* Reuse the orientation already used to lay out this preload request. */
    return photo_prefill_mapped;
}

static void photo_prefill_dispatch(const z72_u32 *message)
{
    PHOTO_PLAY_CALL((z72_u32)photo_prefill_code+PHOTO_PREFILL_ENTRY-PHOTO_PREFILL_START,
                    void,const z72_u32 *)(message);
}

static __attribute__((noinline)) void photo_prefill_install(void)
{
    volatile z72_u32 *slot=(volatile z72_u32 *)PHOTO_PREFILL_SLOT;
    if (*slot!=PHOTO_PREFILL_ENTRY ||
        copy_code(photo_prefill_code,PHOTO_PREFILL_START,PHOTO_PREFILL_BYTES,PHOTO_PREFILL_HASH)) return;
    z72_u32 start=(z72_u32)photo_prefill_code;
    for (z72_u32 i=0;i<sizeof(photo_prefill_calls)/sizeof(*photo_prefill_calls);++i) {
        z72_u32 target=photo_prefill_calls[i].target;
        if (photo_prefill_calls[i].offset==0x122u) target=(z72_u32)photo_prefill_orientation;
        if (photo_prefill_calls[i].offset==0x14cu) target=(z72_u32)photo_prefill_draw_orientation;
        write_call(start+photo_prefill_calls[i].offset,start+PHOTO_PREFILL_BYTES+i*8,target);
    }
    clean_code(photo_prefill_code,sizeof(photo_prefill_code));
    sync_code();
    __atomic_store_n(slot,(z72_u32)photo_prefill_dispatch,__ATOMIC_RELEASE);
}

#undef PHOTO_PLAY_CALL
#endif
