/* SPDX-License-Identifier: MIT */
#ifndef Z72_TCP_SEND_RING_H
#define Z72_TCP_SEND_RING_H

enum {
    TCP_MANAGER_LOCK=0xb1201b58u, TCP_ACTIVE_LIST=0xb1201b70u,
    TCP_DESCRIPTOR_TABLE=0xb140e750u, TCP_PROTOCOL=0xb1201604u,
    TCP_OBJECTS=16, TCP_RING_OFFSET=0x188u, TCP_INITIAL_SEND_BYTES=16384u
};
#include "../sdk/tcp_send_buffer.h"

struct tcp_send_ring { u32 socket, header, data; };

static int tcp_ram(u32 address,u32 size)
{
    return !(address&3u) && address>=0xb1000000u && address<=TCP_SHARED_RAM_END-size;
}

static u32 tcp_list_contains(u32 socket)
{
    /* The active TCP list links through TCB+8 and has at most 16 objects. */
    u32 node=*(const volatile u32 *)TCP_ACTIVE_LIST;
    for (u32 i=0;node && i<TCP_OBJECTS;++i) {
        if (!tcp_ram(node,8)) return 2;
        if (node==socket+8u) return 1;
        node=*(const volatile u32 *)node;
    }
    return node ? 2 : 0;
}

static u32 tcp_try_manager(void)
{
    u32 mutex=*(const volatile u32 *)TCP_MANAGER_LOCK;
    return mutex && !CALL(0xb107a12bu,u32,u32,u32,u32)(mutex,0,0) ? mutex : 0;
}

static u32 tcp_try_socket(u32 socket)
{
    u32 mutex=*(const volatile u32 *)(socket+0x1cu);
    /* Only try here: native destruction can take the locks in reverse order. */
    return mutex && !CALL(0xb107a12bu,u32,u32,u32,u32)(mutex,0,0) ? mutex : 0;
}

static int tcp_attach_send_ring(struct tcp_send_ring *owner,int fd,u32 data)
{
    if (owner->socket || fd<1 || fd>31 ||
        data<TCP_SHARED_RAM_MIN || data>TCP_SHARED_RAM_END-TCP_LARGE_SEND_BYTES || (data&63u))
        return 0;
    u32 manager=tcp_try_manager();
    if (!manager) return 0;
    const volatile u32 *desc=(const volatile u32 *)(TCP_DESCRIPTOR_TABLE+12u*(u32)fd);
    u32 socket=desc[2],lock=0;
    int attached=0;
    if (desc[0]==1 && desc[1]==TCP_PROTOCOL && tcp_ram(socket,TCP_RING_OFFSET+4u) &&
        tcp_list_contains(socket)==1) lock=tcp_try_socket(socket);
    if (lock) {
        volatile u32 *capacity=(volatile u32 *)(socket+0x28u);
        u32 header=*(const volatile u32 *)(socket+TCP_RING_OFFSET);
        volatile u32 *ring=(volatile u32 *)header;
        if (*(const volatile unsigned char *)(socket+0xe4u)==5 && *capacity==TCP_INITIAL_SEND_BYTES &&
            tcp_ram(header,20) && ring[2]==TCP_INITIAL_SEND_BYTES && !ring[3] && ring[4]==header+20u) {
            ring[0]=ring[1]=0;
            ring[4]=data;
            ring[2]=*capacity=TCP_LARGE_SEND_BYTES;
            owner->socket=socket;
            owner->header=header;
            owner->data=data;
            attached=1;
        }
        CALL(0xb107a211u,void,u32)(lock);
    }
    CALL(0xb107a211u,void,u32)(manager);
    return attached;
}

static void tcp_reclaim_send_ring(struct tcp_send_ring *owner)
{
    u32 socket=owner->socket;
    if (!socket) return;
    u32 manager=tcp_try_manager();
    if (!manager) return;
    u32 active=tcp_list_contains(socket);
    int reclaimed=active==0;
    /* C 1.70 removes the object from the active list before freeing its ring
     * header. The destructor frees the header only; it never reads its data.
     * When still active, restore the empty ring under the socket mutex. TCP
     * copies its payload out of this ring into separate retransmit packets. */
    if (active==1) {
        u32 lock=tcp_try_socket(socket);
        if (lock) {
            u32 header=*(const volatile u32 *)(socket+TCP_RING_OFFSET);
            volatile u32 *ring=(volatile u32 *)header;
            if (header!=owner->header || ring[4]!=owner->data) reclaimed=1;
            else if (!ring[3] && ring[2]==TCP_LARGE_SEND_BYTES) {
                ring[0]=ring[1]=0;
                ring[4]=header+20u;
                ring[2]=TCP_INITIAL_SEND_BYTES;
                *(volatile u32 *)(socket+0x28u)=TCP_INITIAL_SEND_BYTES;
                reclaimed=1;
            }
            CALL(0xb107a211u,void,u32)(lock);
        }
    }
    CALL(0xb107a211u,void,u32)(manager);
    if (reclaimed) owner->socket=0;
}
#endif
