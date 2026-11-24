/* SPDX-License-Identifier: MIT */
#ifndef Z72_DCIM_INDEX_H
#define Z72_DCIM_INDEX_H

#include "card_read.h"

enum { FTP_PATH_BYTES=32 };
enum { FTP_MEDIA_UNKNOWN, FTP_MEDIA_JPEG, FTP_MEDIA_NEF, FTP_MEDIA_VIDEO };
enum { DCIM_CF, DCIM_SD, DCIM_CARD_COUNT, DCIM_FOLDER_START=1000 };
struct dcim_folder { z72_u32 card; unsigned short number; };

static inline z72_u32 dcim_id_card(z72_u32 id) { return id>>29; }
static inline z72_u32 dcim_id_folder(z72_u32 id) { return (id>>19)&0x3ffu; }

static inline int dcim_card_id(z72_u32 id)
{
    z72_u32 folder=dcim_id_folder(id);
    return dcim_id_card(id)<DCIM_CARD_COUNT && folder>=100 && folder<DCIM_FOLDER_START;
}

static int ftp_media_format(const char *ext)
{
    if (ext[0]!='.') return FTP_MEDIA_UNKNOWN;
    if (ext[1]=='J' && ext[2]=='P' && ext[3]=='G') return FTP_MEDIA_JPEG;
    if (ext[1]=='N' && ext[2]=='E' && ext[3]=='F') return FTP_MEDIA_NEF;
    if (ext[1]=='M' && ((ext[2]=='O' && ext[3]=='V') || (ext[2]=='P' && ext[3]=='4')))
        return FTP_MEDIA_VIDEO;
    return FTP_MEDIA_UNKNOWN;
}

static int dcim_media_path(z72_u32 id,char path[FTP_PATH_BYTES],z72_u32 protection,z72_u32 dates)
{
    if (!dcim_card_id(id)) return 0;
    /* Same reference/get-path/release sequence as native playback. */
    void *image=Z72_CARD_CALL(0x1063ea8bu,void *,z72_u32)(id);
    if (!image) return 0;
    int matches=!protection || Z72_CARD_CALL(0x10567e19u,z72_u32,void *)(image)==(protection==1);
    if (matches && dates) {
        const unsigned short *date=Z72_CARD_CALL(0x10567f05u,const unsigned short *,void *)(image);
        z72_u32 captured=date ? *date : 0, end=dates>>16;
        matches=captured && captured>=(dates&0xffffu) && (!end || captured<=end);
    }
    const char *name=matches ? Z72_CARD_CALL(0x10567f61u,const char *,void *)(image) : 0;
    z72_u32 length=0;
    if (name) {
        length=Z72_CARD_CALL(0x10509831u,z72_u32,const char *)(name);
        if (length>=FTP_PATH_BYTES) length=0;
        for (z72_u32 i=0;i<length;++i) path[i]=name[i];
    }
    Z72_CARD_CALL(0x1056788bu,void,void *)(image);
    path[length]=0;
    if (length<13 || path[0]!='A'+dcim_id_card(id) || path[1]!=':' || path[2]!='\\' ||
        path[3]!='D' || path[4]!='C' || path[5]!='I' || path[6]!='M' || path[7]!='\\' ||
        !valid_path(path+3,1)) return 0;
    return ftp_media_format(path+length-4);
}

static int ftp_media_path(z72_u32 id,char path[FTP_PATH_BYTES])
{
    return dcim_media_path(id,path,0,0);
}

static int dcim_previous_folder(z72_u32 card,unsigned short *folder)
{
    unsigned short previous=*folder;
    if (previous<=100) return 0;
    z72_u32 status=Z72_CARD_CALL(0x10507d0du,z72_u32,z72_u32,unsigned short *,z72_u32)(card,folder,1);
    return status==FS_OK && *folder>=100 && *folder<previous;
}

static inline int dcim_previous_card_folder(struct dcim_folder *folder)
{
    if (dcim_previous_folder(folder->card,&folder->number)) return 1;
    if (folder->card==DCIM_SD) {
        folder->card=DCIM_CF;
        folder->number=DCIM_FOLDER_START;
        if (dcim_previous_folder(folder->card,&folder->number)) return 1;
    }
    folder->number=0;
    return 0;
}

static inline z72_u32 dcim_find_photo(z72_u32 card,unsigned short folder,z72_u32 old,z72_u32 direction,
                                      const unsigned short *predicate)
{
    struct {
        unsigned char slot,pad;
        unsigned short folder;
        z72_u32 reserved[2],formats;
        unsigned char sort,groups,extra,padding;
    } query={(unsigned char)card,0,folder,{0,0},7u|(1u<<5)|(1u<<6),7,1,0,0}; /* Include MOV and MP4. */
    z72_u32 id=old;
    z72_u32 status=Z72_CARD_CALL(0x106d3285u,z72_u32,const void *,const void *,z72_u32,z72_u32 *)(
        &query,predicate,direction,&id);
    if (status!=0x042b0080u || dcim_id_card(id)!=card || dcim_id_folder(id)!=folder ||
        (old && (direction ? id>=old : id<=old))) return 0;
    return id;
}

static inline z72_u32 ftp_find_photo(z72_u32 card,unsigned short folder,z72_u32 old,z72_u32 direction)
{
    return dcim_find_photo(card,folder,old,direction,0);
}

static z72_u32 ftp_media_key(z72_u32 id)
{
    return (id&15u)<=1u ? id&~15u : id; /* Only JPEG and NEF share one selection. */
}

#endif
