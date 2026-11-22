/* SPDX-License-Identifier: MIT */
#ifndef Z72_DCIM_INDEX_H
#define Z72_DCIM_INDEX_H

#include "card_read.h"

enum { FTP_PATH_BYTES=32 };
enum { FTP_MEDIA_UNKNOWN, FTP_MEDIA_JPEG, FTP_MEDIA_NEF, FTP_MEDIA_VIDEO };

static int ftp_media_format(const char *ext)
{
    if (ext[0]!='.') return FTP_MEDIA_UNKNOWN;
    if (ext[1]=='J' && ext[2]=='P' && ext[3]=='G') return FTP_MEDIA_JPEG;
    if (ext[1]=='N' && ext[2]=='E' && ext[3]=='F') return FTP_MEDIA_NEF;
    if (ext[1]=='M' && ((ext[2]=='O' && ext[3]=='V') || (ext[2]=='P' && ext[3]=='4')))
        return FTP_MEDIA_VIDEO;
    return FTP_MEDIA_UNKNOWN;
}

static int ftp_media_path(z72_u32 id,char path[FTP_PATH_BYTES])
{
    /* Same reference/get-path/release sequence as native playback. */
    void *image=Z72_CARD_CALL(0x1063ea8bu,void *,z72_u32)(id);
    if (!image) return 0;
    const char *name=Z72_CARD_CALL(0x10567f61u,const char *,void *)(image);
    z72_u32 length=0;
    if (name) {
        length=Z72_CARD_CALL(0x10509831u,z72_u32,const char *)(name);
        if (length>=FTP_PATH_BYTES) length=0;
        for (z72_u32 i=0;i<length;++i) path[i]=name[i];
    }
    Z72_CARD_CALL(0x1056788bu,void,void *)(image);
    path[length]=0;
    if (length<13 || path[0]!='B' || path[1]!=':' || path[2]!='\\' ||
        path[3]!='D' || path[4]!='C' || path[5]!='I' || path[6]!='M' || path[7]!='\\' ||
        !valid_path(path+3,1)) return 0;
    return ftp_media_format(path+length-4);
}

static int ftp_previous_folder(unsigned short *folder)
{
    unsigned short previous=*folder;
    z72_u32 status=Z72_CARD_CALL(0x10507d0du,z72_u32,z72_u32,unsigned short *,z72_u32)(1,folder,1);
    return status==FS_OK && *folder>=100 && *folder<previous;
}

static z72_u32 ftp_find_photo(unsigned short folder,z72_u32 old,z72_u32 direction)
{
    struct {
        unsigned char slot,pad;
        unsigned short folder;
        z72_u32 reserved[2],formats;
        unsigned char sort,groups,extra,padding;
    } query={1,0,folder,{0,0},7u|(1u<<5)|(1u<<6),7,1,0,0}; /* Include MOV and MP4. */
    z72_u32 id=old;
    z72_u32 status=Z72_CARD_CALL(0x106d3285u,z72_u32,const void *,const void *,z72_u32,z72_u32 *)(
        &query,0,direction,&id);
    if (status!=0x042b0080u || id>>29!=1u || ((id>>19)&0x3ffu)!=folder ||
        (old && (direction ? id>=old : id<=old))) return 0;
    return id;
}

static z72_u32 ftp_media_key(z72_u32 id)
{
    return (id&15u)<=1u ? id&~15u : id; /* Only JPEG and NEF share one selection. */
}

#endif
