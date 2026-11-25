/* SPDX-License-Identifier: MIT */
#ifndef Z72_PHOTO_PEER_ROTATION_H
#define Z72_PHOTO_PEER_ROTATION_H
#include "photo_playback_metadata.h"

#define PHOTO_PEER_CALL(a,result,...) ((result (*)(__VA_ARGS__))(a))

enum {
    PHOTO_CARD_PEER_SLOT=0xb29005f0u, PHOTO_CARD_PEER_NATIVE=0x104eea89u,
    PHOTO_PEER_EVENT=0x42070001u, PHOTO_PEER_CARD_WRITE=3,
    PHOTO_PEER_READ=0x105bf529u, PHOTO_PEER_WRITE=0x105bf507u,
    PHOTO_PEER_BASE=0xc0000000u, PHOTO_PEER_LIMIT=0xd0000000u
};

struct photo_peer_source {
    struct photo_source source;
    z72_u32 address,capacity,offset,loaded;
    _Alignas(8) unsigned char buffer[64];
};

static z72_u32 photo_peer_range(z72_u32 address,z72_u32 size)
{
    return !(address&7u) && address>=PHOTO_PEER_BASE && address<PHOTO_PEER_LIMIT &&
        size<=PHOTO_PEER_LIMIT-address;
}

static const unsigned char *photo_peer_bytes(const struct photo_source *source,z72_u32 offset,z72_u32 size)
{
    struct photo_peer_source *peer=(struct photo_peer_source *)source;
    if (offset>=peer->offset && offset+size<=peer->offset+peer->loaded)
        return peer->buffer+(offset-peer->offset);
    z72_u32 start=offset&~7u,end=(offset+size+7u)&~7u;
    z72_u32 chunk=peer->capacity-start;
    if (chunk>sizeof(peer->buffer)) chunk=sizeof(peer->buffer);
    chunk&=~7u;
    if (end-start>chunk) return 0;
    peer->loaded=0;
    if (!PHOTO_PEER_CALL(PHOTO_PEER_READ,z72_u32,void *,z72_u32,z72_u32)(
            peer->buffer,peer->address+start,chunk)) return 0;
    peer->offset=start;
    peer->loaded=chunk;
    return peer->buffer+(offset-start);
}

/* The slave has published its first file component and is waiting for the
 * card task's normal ACK. Keep all writes inside one aligned EXIF lane. */
static void photo_peer_fix(z72_u32 event,z72_u32 packet)
{
    if (event!=PHOTO_PEER_EVENT || packet<0xb0000000u || packet>0xbffffff0u || (packet&3u)) return;
    const z72_u32 *request=(const z72_u32 *)packet;
    const unsigned char *params=(const unsigned char *)packet+8;
    z72_u32 kind=params[1],wire=request[3];
    if (request[0]!=PHOTO_PEER_CARD_WRITE || (request[1]&0xffff00ffu)!=0x00080005u ||
        params[0] || (kind!=0 && kind!=1 && kind!=3) ||
        !photo_peer_range(wire,0x168)) return;
    _Alignas(8) z72_u32 descriptor[(0x168-0x120)/4];
    if (!PHOTO_PEER_CALL(PHOTO_PEER_READ,z72_u32,void *,z72_u32,z72_u32)(
            descriptor,wire+0x120,sizeof(descriptor)) || descriptor[(0x164-0x120)/4]!=1) return;
    z72_u32 address=descriptor[0],capacity=descriptor[1],size=descriptor[2];
    if (size<8 || size>capacity || !photo_peer_range(address,capacity)) return;
    struct photo_peer_source peer;
    peer.source=(struct photo_source){0,size,photo_peer_bytes};
    peer.address=address;
    peer.capacity=capacity;
    peer.offset=peer.loaded=0;
    struct photo_tiff tiff;
    if (!photo_header_tiff(&tiff,&peer.source) || (kind==1)!=(tiff.start==0)) return;
    z72_u32 tag=photo_orientation_tag(&tiff);
    const unsigned char *value=tag ? photo_tiff_bytes(&tiff,tag,2) : 0;
    if (!value || photo_uint(value,2,tiff.big)!=1 || !photo_shot_inverted(&tiff)) return;

    z72_u32 position=tiff.start+tag,shift=position&7u;
    z72_u32 lane_address=address+(position&~7u);
    _Alignas(8) unsigned char lane[8];
    if (shift>6 || (position&~7u)+sizeof(lane)>capacity ||
        !PHOTO_PEER_CALL(PHOTO_PEER_READ,z72_u32,void *,z72_u32,z72_u32)(lane,lane_address,sizeof(lane)) ||
        photo_uint(lane+shift,2,tiff.big)!=1) return;
    lane[shift+tiff.big]=3;
    if (PHOTO_PEER_CALL(PHOTO_PEER_WRITE,z72_u32,z72_u32,const void *,z72_u32)(
            lane_address,lane,sizeof(lane)))
        __asm__ volatile("dsb sy" ::: "memory");
}

static void photo_peer_dispatch(z72_u32 event,z72_u32 packet)
{
    photo_peer_fix(event,packet);
    PHOTO_PEER_CALL(PHOTO_CARD_PEER_NATIVE,void,z72_u32,z72_u32)(event,packet);
}

static __attribute__((noinline)) void photo_peer_install(void)
{
    volatile z72_u32 *slot=(volatile z72_u32 *)PHOTO_CARD_PEER_SLOT;
    if (*slot==PHOTO_CARD_PEER_NATIVE)
        __atomic_store_n(slot,(z72_u32)photo_peer_dispatch,__ATOMIC_RELEASE);
}

#undef PHOTO_PEER_CALL
#endif
