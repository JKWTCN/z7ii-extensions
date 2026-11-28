/* SPDX-License-Identifier: MIT */
#ifndef Z72_MOVIE_RAW_DMA_H
#define Z72_MOVIE_RAW_DMA_H

#include "z72.h"

enum {
    RAW_DMA_EVENT=0xb180945cu, RAW_DMA_LOCK=0xb1809460u,
    RAW_DMA_POOL=0xb1809464u, RAW_DMA_DRIVER=0xb18066f0u, RAW_DMA_CHANNELS=4u,
    RAW_DMA_COMPLETION=0x103a3901u
};
enum { RAW_DMA_PENDING, RAW_DMA_DONE, RAW_DMA_BAD_STATUS };

struct z72_raw_dma_channel {
    z72_u32 driver;
    unsigned char mode, owner, reserved[2], status, padding[3];
    z72_u32 transfer_count, event_mask;
};

_Static_assert(sizeof(struct z72_raw_dma_channel)==20,"Native memory-mover channel size");

static inline volatile struct z72_raw_dma_channel *z72_raw_dma_channel(z72_u32 index)
{
    return (volatile struct z72_raw_dma_channel *)RAW_DMA_POOL+index;
}

static inline z72_u32 z72_raw_dma_acquire_mask(z72_u32 allowed)
{
    if ((int)*(const volatile z72_u32 *)RAW_DMA_EVENT<=0 ||
        (int)*(const volatile z72_u32 *)RAW_DMA_LOCK<=0 ||
        !((int (*)(z72_u32,z72_u32))0x10360f2fu)(RAW_DMA_LOCK,0)) return RAW_DMA_CHANNELS;

    z72_u32 index=0;
    for (;index<RAW_DMA_CHANNELS;++index) {
        volatile struct z72_raw_dma_channel *channel=z72_raw_dma_channel(index);
        if ((allowed&(1u<<index)) && !channel->owner && channel->driver==RAW_DMA_DRIVER+36u*index &&
            channel->event_mask==(1u<<index)) {
            channel->owner=1;
            break;
        }
    }
    ((void (*)(z72_u32))0x10360f13u)(RAW_DMA_LOCK);
    return index;
}

static inline z72_u32 z72_raw_dma_acquire(void)
{
    return z72_raw_dma_acquire_mask((1u<<RAW_DMA_CHANNELS)-1u);
}

static inline void z72_raw_dma_start_with_callback(z72_u32 index,z72_u32 dest,z72_u32 source,z72_u32 size,
                                                  z72_u32 callback)
{
    volatile struct z72_raw_dma_channel *channel=z72_raw_dma_channel(index);
    z72_u32 config[8];
    ((void (*)(void *,z72_u32))0x10361035u)(config,sizeof(config));
    ((void (*)(void *))0x103a3a91u)(config);
    config[0]=size-1u;
    config[1]=source;
    config[2]=dest;
    channel->mode=1;
    ((void (*)(z72_u32,z72_u32))0x103610d3u)(RAW_DMA_EVENT,channel->event_mask);
    ((void (*)(z72_u32,const void *,z72_u32))0x10294c5du)(channel->driver,config,callback);
    ((void (*)(z72_u32,z72_u32))0x10294d0du)(channel->driver,1);
}

static inline void z72_raw_dma_start(z72_u32 index,z72_u32 dest,z72_u32 source,z72_u32 size)
{
    z72_raw_dma_start_with_callback(index,dest,source,size,RAW_DMA_COMPLETION);
}

static inline void z72_raw_dma_release(z72_u32 index)
{
    ((void (*)(z72_u32))0x103a3cf5u)(index);
}

static inline z72_u32 z72_raw_dma_wait_completion(z72_u32 index,z72_u32 timeout)
{
    const volatile struct z72_raw_dma_channel *channel=z72_raw_dma_channel(index);
    const z72_u32 mask=channel->event_mask;
    z72_u32 events=0;
    if (!((int (*)(z72_u32,z72_u32,z72_u32 *,z72_u32))0x10361117u)(RAW_DMA_EVENT,mask,&events,timeout) ||
        !(events&mask)) return RAW_DMA_PENDING;
    return channel->status==8u ? RAW_DMA_DONE : RAW_DMA_BAD_STATUS;
}

static inline z72_u32 z72_raw_dma_wait(z72_u32 index,z72_u32 timeout)
{
    const z72_u32 result=z72_raw_dma_wait_completion(index,timeout);
    if (result!=RAW_DMA_PENDING) z72_raw_dma_release(index);
    return result;
}

#endif
