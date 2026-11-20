/* SPDX-License-Identifier: MIT */
#ifndef Z72_FTP_CARD_H
#define Z72_FTP_CARD_H

#include "ftp_client.h"
#define FS_OK 0x043a0080u
#define FTP_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
static _Alignas(64) unsigned char card_buffer[576];

static void publish(volatile void *p,z72_u32 size)
{
    FTP_CALL(0x1034f9edu,void,volatile void *,z72_u32)(p,size);
}

static void invalidate(const void *p,z72_u32 size)
{
    FTP_CALL(0x1034fa29u,void,const void *,z72_u32)(p,size);
}

static int card_open(z72_u32 *file,const char *path)
{
    for (z72_u32 i=0;i<10;++i) ((volatile z72_u32 *)file)[i]=0;
    FTP_CALL(0x104d3c01u,void,z72_u32 *,const char *)(file,path);
    return FTP_CALL(0x104d3c05u,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)==FS_OK;
}

static int card_close(z72_u32 *file)
{
    return FTP_CALL(0x104d3c91u,z72_u32,z72_u32 *)(file)==FS_OK;
}

static int card_read(z72_u32 *file,void *buffer,z72_u32 capacity,z72_u32 count,z72_u32 eof)
{
    z72_u32 size=count+eof, actual=0;
    struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,buffer,size};
    invalidate(buffer,capacity);
    z72_u32 result=FTP_CALL(0x104d3d83u,z72_u32,z72_u32 *,void *,z72_u32,z72_u32 *)(
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

static int endpoint(const char *p,struct ftp_request *request)
{
    for (z72_u32 field=0;field<5;++field) {
        z72_u32 limit=field<4 ? 255u : 65535u, value=0;
        if (*p<'0' || *p>'9') return 0;
        do {
            value=10u*value+(z72_u32)(*p++-'0');
            if (value>limit) return 0;
        } while (*p>='0' && *p<='9');
        if (field<4) {
            if (*p++!=(field==3 ? ':' : '.')) return 0;
            request->ip[field]=(unsigned char)value;
        } else {
            if (*p || !value || !request->ip[0] || request->ip[0]>=224 ||
                request->ip[0]==127) return 0;
            request->port=value;
        }
    }
    return 1;
}

struct ftp_config {
    char address[24], user[64], password[64];
    char folder[64];
};

enum { FTP_CONFIG_MISSING, FTP_CONFIG_INVALID, FTP_CONFIG_VALID };

static int read_config(struct ftp_config *config,const char *config_path)
{
    z72_u32 file[10];
    if (!card_open(file,config_path)) return FTP_CONFIG_MISSING;
    z72_u32 size=file[6];
    int ok=size && size<=512 && !file[7] && card_read(file,card_buffer,sizeof(card_buffer),size,1);
    if (!card_close(file) || !ok) return FTP_CONFIG_INVALID;

    char *fields[]={config->address,config->user,config->password};
    const z72_u32 limits[]={sizeof(config->address),sizeof(config->user),sizeof(config->password)};
    z72_u32 pos=0;
    for (z72_u32 i=0;i<3u;++i) {
        if (pos>=size) return FTP_CONFIG_INVALID;
        z72_u32 n=0;
        while (pos<size && card_buffer[pos]!='\n' && card_buffer[pos]!='\r') {
            unsigned char c=card_buffer[pos++];
            if (c<32 || c>126 || n+1>=limits[i]) return FTP_CONFIG_INVALID;
            fields[i][n++]=(char)c;
        }
        if (!n && i!=2) return FTP_CONFIG_INVALID;
        fields[i][n]=0;
        if (pos<size && card_buffer[pos]=='\r') ++pos;
        if (pos<size && card_buffer[pos++]!='\n') return FTP_CONFIG_INVALID;
    }
    config->folder[0]=0;
    /* An optional tagged line specifies the server directory. */
    if (size-pos>=4 && card_buffer[pos]=='C' && card_buffer[pos+1]=='W' &&
        card_buffer[pos+2]=='D' && card_buffer[pos+3]==':') {
        z72_u32 length=0;
        pos+=4;
        while (pos<size && card_buffer[pos]!='\r' && card_buffer[pos]!='\n') {
            unsigned char c=card_buffer[pos++];
            if (c<32 || c>126 || length+1u>=sizeof(config->folder)) return FTP_CONFIG_INVALID;
            config->folder[length++]=(char)c;
        }
        config->folder[length]=0;
    }
    return FTP_CONFIG_VALID;
}

static void ftp_copy_field(char *out,const char *in)
{
    while ((*out++=*in++)) {}
}

static int ftp_config_request(struct ftp_request *request,const struct ftp_config *config)
{
    if (!endpoint(config->address,request)) return 0;
    ftp_copy_field(request->user,config->user);
    ftp_copy_field(request->password,config->password);
    ftp_copy_field(request->folder,config->folder);
    return 1;
}

#endif
