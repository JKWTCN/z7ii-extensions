/* SPDX-License-Identifier: MIT */
#ifndef Z72_PHOTO_PLAYBACK_DRAW_H
#define Z72_PHOTO_PLAYBACK_DRAW_H
#include "code_copy.h"

#define PHOTO_DRAW_CALL(a,result,...) ((result (*)(__VA_ARGS__))(a))

enum {
    PHOTO_DRAW_START=0x105a9910u, PHOTO_DRAW_BYTES=0xc7cu,
    PHOTO_DRAW_HASH=0x232d94b7u
};

/* External calls on the 180-degree path in the C1.70 graphics worker. */
static const struct { z72_u32 offset,target; } photo_draw_calls[]={
    {0x018,0x10360f44},{0x036,0x103fac67},{0x066,0x103fac67},{0x0aa,0x1056a101},
    {0x0cc,0x106432cb},{0x0d4,0x10643441},{0x0de,0x10643441},{0x0e8,0x10643441},
    {0x0f2,0x10643441},{0x0fa,0x10643313},{0x102,0x10643441},{0x152,0x106433cb},
    {0x176,0x106433cb},{0x184,0x106433cb},{0x194,0x106433cb},{0x1c0,0x106433cb},
    {0x208,0x103c5ac7},{0x216,0x1034f9ed},{0x222,0x105a8ceb},{0x246,0x106433cb},
    {0x250,0x106433cb},{0x254,0x1054c06b},{0x260,0x10643407},{0x288,0x10644765},
    {0x28e,0x106432e7},{0x29a,0x10643d47},{0x2a0,0x10643cf9},{0x2ac,0x105a8f1b},
    {0x2ba,0x10643557},{0x2c8,0x106435fb},{0x2d0,0x10643d2b},{0x34a,0x106448ad},
    {0x386,0x106433cb},{0x392,0x106433e7},{0x39c,0x103f7029},
    {0x8b6,0x10643475},{0x8c2,0x105a8f1b},{0x8cc,0x106433e7},{0x8f6,0x10643a9b},
    {0x90c,0x10643a9b},{0x924,0x10643a9b},{0x93e,0x10643a9b},{0x956,0x106445b5},
    {0x95c,0x10643ddd},{0x96a,0x10643d2b},{0x972,0x10643d13},{0x97c,0x10643d2b},
    {0x98a,0x106433e7},{0x994,0x106433cb},{0x99e,0x106433cb},{0x9a8,0x106433cb},
    {0x9b0,0x105a8ceb},{0x9ba,0x106433cb},{0x9c0,0x106432fb},
};
static _Alignas(32) unsigned char photo_draw_code[PHOTO_DRAW_BYTES+
    (sizeof(photo_draw_calls)/sizeof(*photo_draw_calls)+1)*8];
static z72_u32 photo_draw_ready;

static void photo_draw_half_turn(const z72_u32 *request,z72_u32 source,z72_u32 destination,int height)
{
    z72_u32 (*to_float)(int)=PHOTO_DRAW_CALL(0x103f8615u,z72_u32,int);
    z72_u32 (*divide)(z72_u32,z72_u32)=PHOTO_DRAW_CALL(0x103f83f1u,z72_u32,z72_u32,z72_u32);
    z72_u32 x=divide(to_float(destination&0xffffu),to_float(source&0xffffu));
    z72_u32 y=divide(to_float(destination>>16),to_float(source>>16));
    int right=(short)request[7],bottom=(short)(request[7]>>16);
    /* Translate on the native identity matrix before reversing the X scale.
     * The renderer already reverses Y for unrotated images. The far edges
     * are exclusive, including when the source is cropped for zoom. */
    PHOTO_DRAW_CALL(0x10643343u,void,z72_u32,z72_u32)(to_float(right),to_float(height-bottom));
    PHOTO_DRAW_CALL(0x1064335fu,void,z72_u32,z72_u32)(x^0x80000000u,y);
}

static __attribute__((noinline)) void photo_draw_install(void)
{
    if (copy_code(photo_draw_code,PHOTO_DRAW_START,PHOTO_DRAW_BYTES,PHOTO_DRAW_HASH)) return;
    z72_u32 start=(z72_u32)photo_draw_code,stub=start+PHOTO_DRAW_BYTES;
    for (z72_u32 i=0;i<sizeof(photo_draw_calls)/sizeof(*photo_draw_calls);++i,stub+=8)
        write_call(start+photo_draw_calls[i].offset,stub,photo_draw_calls[i].target);

    /* r4=request, native packed sizes at sp+0x10 and sp+0x24,
     * native surface height at sp+0x32. Continue at the stock texture draw. */
    static const unsigned short setup[]={0x4620,0x9904,0x9a09,0xf9bd,0x3032};
    unsigned short *splice=(unsigned short *)(photo_draw_code+0x3ba);
    for (z72_u32 i=0;i<sizeof(setup)/sizeof(*setup);++i) splice[i]=setup[i];
    write_call(start+0x3c4,stub,(z72_u32)photo_draw_half_turn);
    *(unsigned short *)(photo_draw_code+0x3c8)=0xe270; /* b to worker +0x8ac */
    clean_code(photo_draw_code,sizeof(photo_draw_code));
    sync_code();
    __atomic_store_n(&photo_draw_ready,1,__ATOMIC_RELEASE);
}

static z72_u32 photo_draw_playback(volatile z72_u32 *message)
{
    if (message[4]!=0x4201001au || message[3]!=0x105a8b17u ||
        !__atomic_load_n(&photo_draw_ready,__ATOMIC_ACQUIRE)) return 0;
    const volatile z72_u32 *payload=(const volatile z72_u32 *)message[5];
    if (!payload || (payload[16]&0xffffu)!=180u || payload[0] || (payload[1]!=1 && payload[1]!=12) ||
        payload[10]!=255 || payload[11]!=1) return 0;

    /* Adapt the graphics queue packet exactly as the stock 0x1A handler. */
    z72_u32 request[23];
    for (z72_u32 i=0;i<16;++i) request[i]=payload[i];
    request[16]=180;
    request[17]=(payload[17]&0xffu)|((payload[18]&0xffu)<<8);
    for (z72_u32 i=18;i<23;++i) request[i]=payload[i+2];
    PHOTO_DRAW_CALL((z72_u32)photo_draw_code|1u,void,const z72_u32 *)(request);
    message[6]=0;
    PHOTO_DRAW_CALL(0x105a8b17u,void,z72_u32,z72_u32)(0x42010062u,0);
    return 1;
}

#undef PHOTO_DRAW_CALL
#endif
