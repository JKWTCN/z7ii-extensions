/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_GALLERY_ZIP_H
#define Z72_WIFI_GALLERY_ZIP_H

enum { ZIP_LOCAL=30, ZIP_DESCRIPTOR=16, ZIP_CENTRAL=46, ZIP_END=22 };
struct gallery_zip_entry {
    u32 id,route,size,offset,crc, name_length;
    char name[GALLERY_NAME_BYTES];
};

static struct gallery_zip_entry gallery_zip_entries[GALLERY_ZIP_FILES];
static u32 gallery_crc_table[256];
static int gallery_crc_ready;

static u32 gallery_zip_parse(const char *text)
{
    for (u32 i=0;i<GALLERY_ZIP_FILES;++i) {
        struct gallery_zip_entry *entry=&gallery_zip_entries[i];
        const char *end=gallery_photo_hex(text,&entry->id);
        if (!end || (*end!='j' && *end!='n')) return 0;
        entry->route=*end++=='j' ? GALLERY_JPG : GALLERY_NEF;
        if (!*end) return i+1u;
        if (*end!=',') return 0;
        text=end+1;
    }
    return 0;
}

static void gallery_crc_prepare(void)
{
    if (gallery_crc_ready) return;
    for (u32 i=0;i<256u;++i) {
        u32 crc=i;
        for (unsigned bit=0;bit<8u;++bit) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
        gallery_crc_table[i]=crc;
    }
    gallery_crc_ready=1;
}

static u32 gallery_crc_update(u32 crc,const unsigned char *data,u32 length)
{
    while (length--) crc=(crc>>8)^gallery_crc_table[(crc^*data++)&255u];
    return crc;
}

static void gallery_zip_clear(unsigned char *data,u32 length)
{
    volatile unsigned char *out=data;
    while (length--) *out++=0;
}

static void gallery_zip_u16(unsigned char *out,u32 value)
{
    out[0]=(u8)value;
    out[1]=(u8)(value>>8);
}

static void gallery_zip_u32(unsigned char *out,u32 value)
{
    gallery_zip_u16(out,value);
    gallery_zip_u16(out+2,value>>16);
}

static u32 gallery_zip_prepare(u32 count,u32 epoch,u32 *length,u32 *directory)
{
    u32 total=ZIP_END,local=0;
    for (u32 i=0;i<count;++i) {
        struct gallery_zip_entry *entry=&gallery_zip_entries[i];
        u32 status=gallery_request_media(entry->route,entry->id,GALLERY_META|GALLERY_ARCHIVE,0,epoch);
        gallery_request.cancelled=gallery_request.serial;
        if (status!=200u) return status;
        const char *name=gallery_response.name;
        u32 n=0;
        for (;n+1u<sizeof(entry->name) && name[n];++n)
            entry->name[n]=name[n]=='\\' ? '/' : name[n];
        entry->name[n]=0;
        entry->name_length=n;
        entry->size=gallery_response.length;
        entry->offset=local;
        for (u32 j=0;j<i;++j)
            if (gallery_equal(entry->name,gallery_zip_entries[j].name)) return 400;
        u32 overhead=ZIP_LOCAL+ZIP_DESCRIPTOR+ZIP_CENTRAL+2u*n;
        /* The maximum value is reserved for ZIP64 and HTTP chunked length. */
        u32 room=0xfffffffeu-total;
        if (overhead>room || entry->size>room-overhead) return 413;
        total+=overhead+entry->size;
        local+=ZIP_LOCAL+n+entry->size+ZIP_DESCRIPTOR;
    }
    *length=total;
    *directory=local;
    return 200;
}

static int gallery_zip_matches(const struct gallery_zip_entry *entry)
{
    if (entry->size!=gallery_response.length) return 0;
    const char *name=gallery_response.name;
    for (u32 i=0;i<entry->name_length;++i) {
        char c=name[i]=='\\' ? '/' : name[i];
        if (c!=entry->name[i]) return 0;
    }
    return !name[entry->name_length];
}

static int gallery_zip_member(int fd,struct gallery_zip_entry *entry,u32 epoch)
{
    unsigned char header[ZIP_LOCAL];
    gallery_zip_clear(header,sizeof(header));
    gallery_zip_u32(header,0x04034b50u);
    gallery_zip_u16(header+4,20);
    gallery_zip_u16(header+6,8); /* CRC and sizes follow the stored file. */
    gallery_zip_u16(header+12,0x21); /* 1980-01-01, no timestamp from the index. */
    gallery_zip_u16(header+26,entry->name_length);
    u32 crc=~0u;
    if (!gallery_write_all(fd,header,sizeof(header),epoch) ||
        !gallery_write_all(fd,entry->name,entry->name_length,epoch) ||
        !gallery_body(fd,entry->size,epoch,0,&crc)) return 0;
    entry->crc=~crc;
    gallery_zip_u32(header,0x08074b50u);
    gallery_zip_u32(header+4,entry->crc);
    gallery_zip_u32(header+8,entry->size);
    gallery_zip_u32(header+12,entry->size);
    return gallery_write_all(fd,header,ZIP_DESCRIPTOR,epoch);
}

static void gallery_zip_directory(int fd,u32 count,u32 offset,u32 bytes,u32 epoch)
{
    unsigned char header[ZIP_CENTRAL];
    for (u32 i=0;i<count;++i) {
        const struct gallery_zip_entry *entry=&gallery_zip_entries[i];
        gallery_zip_clear(header,sizeof(header));
        gallery_zip_u32(header,0x02014b50u);
        gallery_zip_u16(header+4,20);
        gallery_zip_u16(header+6,20);
        gallery_zip_u16(header+8,8);
        gallery_zip_u16(header+14,0x21);
        gallery_zip_u32(header+16,entry->crc);
        gallery_zip_u32(header+20,entry->size);
        gallery_zip_u32(header+24,entry->size);
        gallery_zip_u16(header+28,entry->name_length);
        gallery_zip_u32(header+42,entry->offset);
        if (!gallery_write_all(fd,header,sizeof(header),epoch) ||
            !gallery_write_all(fd,entry->name,entry->name_length,epoch)) return;
    }
    gallery_zip_clear(header,ZIP_END);
    gallery_zip_u32(header,0x06054b50u);
    gallery_zip_u16(header+8,count);
    gallery_zip_u16(header+10,count);
    gallery_zip_u32(header+12,bytes);
    gallery_zip_u32(header+16,offset);
    gallery_write_all(fd,header,ZIP_END,epoch);
}

static void gallery_zip_download(int fd,u32 count,int head,u32 epoch)
{
    u32 length=0,directory=0;
    u32 status=gallery_zip_prepare(count,epoch,&length,&directory);
    if (status!=200u) {
        if (status==413u) {
            static const char message[]="所选原文件超过 ZIP 容量，请减少选择后重试。";
            if (gallery_reply(fd,status,"text/plain; charset=utf-8",sizeof(message)-1u,0,epoch) && !head)
                gallery_write_all(fd,message,sizeof(message)-1u,epoch);
        } else gallery_reply(fd,status,0,0,0,epoch);
        return;
    }
    if (head) {
        gallery_reply(fd,200,"application/zip",length,"Z72-Photos.zip",epoch);
        return;
    }
    gallery_crc_prepare();
    for (u32 i=0;i<count;++i) {
        struct gallery_zip_entry *entry=&gallery_zip_entries[i];
        u32 flags=GALLERY_ARCHIVE|(!i && length>=TCP_LARGE_SEND_BYTES ? GALLERY_LARGE : 0);
        status=gallery_request_media(entry->route,entry->id,flags,0,epoch);
        int ready=status==200u && gallery_zip_matches(entry);
        if (!i) {
            if (ready) {
                gallery_state.download_buffer=gallery_prepare_download_ring(fd,gallery_response.send_address,epoch);
                ready=gallery_reply(fd,200,"application/zip",length,"Z72-Photos.zip",epoch);
            } else gallery_reply(fd,status==200u ? 503u : status,0,0,0,epoch);
        }
        int sent=ready && gallery_zip_member(fd,entry,epoch);
        gallery_request.cancelled=gallery_request.serial;
        if (!sent) return;
    }
    gallery_zip_directory(fd,count,directory,length-directory-ZIP_END,epoch);
}

#endif
