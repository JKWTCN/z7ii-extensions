/* SPDX-License-Identifier: MIT */
#ifndef Z72_CARD_READ_H
#define Z72_CARD_READ_H

#include "z72.h"
#define FS_OK 0x043a0080u
#define Z72_CARD_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))

static void publish(volatile void *p,z72_u32 size)
{
    Z72_CARD_CALL(0x1034f9edu,void,volatile void *,z72_u32)(p,size);
}

static void invalidate(const void *p,z72_u32 size)
{
    Z72_CARD_CALL(0x1034fa29u,void,const void *,z72_u32)(p,size);
}

static int card_open(z72_u32 *file,const char *path)
{
    for (z72_u32 i=0;i<10;++i) ((volatile z72_u32 *)file)[i]=0;
    Z72_CARD_CALL(0x104d3c01u,void,z72_u32 *,const char *)(file,path);
    return Z72_CARD_CALL(0x104d3c05u,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)==FS_OK;
}

static int card_close(z72_u32 *file)
{
    return Z72_CARD_CALL(0x104d3c91u,z72_u32,z72_u32 *)(file)==FS_OK;
}

static int card_read(z72_u32 *file,void *buffer,z72_u32 capacity,z72_u32 count,z72_u32 eof)
{
    z72_u32 size=count+eof, actual=0;
    struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,buffer,size};
    invalidate(buffer,capacity);
    z72_u32 result=Z72_CARD_CALL(0x104d3d83u,z72_u32,z72_u32 *,void *,z72_u32,z72_u32 *)(
        file,&request,size,&actual);
    invalidate(buffer,capacity);
    return result==FS_OK && actual==count;
}

static int name_char(unsigned char c)
{
    return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') ||
        c=='_' || c=='-';
}

static int valid_path(const char *p,int local)
{
    if (!name_char((unsigned char)*p)) return 0;
    for (++p;*p;++p) {
        if (local && (*p=='/' || *p=='\\')) {
            if (!name_char((unsigned char)p[1])) return 0;
        } else if (!name_char((unsigned char)*p) && *p!='.') return 0;
    }
    return 1;
}

#endif
