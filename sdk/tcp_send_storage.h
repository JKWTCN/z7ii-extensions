/* SPDX-License-Identifier: MIT */
#ifndef Z72_TCP_SEND_STORAGE_H
#define Z72_TCP_SEND_STORAGE_H

#include "card_read.h"
#include "tcp_send_buffer.h"

static z72_u32 tcp_allocate_send_buffer(z72_u32 pools,void **owner)
{
    const z72_u32 size=TCP_LARGE_SEND_BYTES+63u;
    *owner=0;
    /* General and Heap both support this allocator. Leave 4 MiB available. */
    for (z72_u32 index=0;index<pools;++index) {
        z72_u32 pool=*(const volatile z72_u32 *)(0xb1f7cbacu+20u*index),status[4];
        if (!pool || Z72_CARD_CALL(0x10104788u,int,z72_u32,z72_u32 *)(pool,status) ||
            status[1] || status[2]<size+TCP_LARGE_SEND_BYTES || status[3]<size) continue;
        void *raw=Z72_CARD_CALL(0x1034fe31u,void *,z72_u32,z72_u32)(size,index);
        if (!raw) continue;
        z72_u32 address=((z72_u32)raw+63u)&~63u;
        if (address<TCP_SHARED_RAM_MIN || address>TCP_SHARED_RAM_END-TCP_LARGE_SEND_BYTES) {
            Z72_CARD_CALL(0x1034fe7fu,void,void *)(raw);
            continue;
        }
        publish((void *)address,TCP_LARGE_SEND_BYTES);
        invalidate((void *)address,TCP_LARGE_SEND_BYTES);
        *owner=raw;
        return address;
    }
    return 0;
}

static inline void tcp_free_send_buffer(void *owner,z72_u32 address)
{
    invalidate((void *)address,TCP_LARGE_SEND_BYTES);
    Z72_CARD_CALL(0x1034fe7fu,void,void *)(owner);
}

#endif
