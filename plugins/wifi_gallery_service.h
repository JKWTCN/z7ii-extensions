/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_GALLERY_SERVICE_H
#define Z72_WIFI_GALLERY_SERVICE_H

#include "wifi_gallery.h"
#include "dcim_index.h"
#include "tcp_send_storage.h"
#include "wifi_gallery_preview.h"

#define GALLERY_CONTROL ((struct gallery_control *)(wifi_aux+GALLERY_CONTROL_OFFSET))
#define GALLERY_REQUEST ((const struct gallery_request *)(wifi_aux+GALLERY_REQUEST_OFFSET))
#define GALLERY_RESPONSE ((struct gallery_response *)(wifi_aux+GALLERY_RESPONSE_OFFSET))
#define GALLERY_STATE ((const struct gallery_state *)(wifi_aux+GALLERY_STATE_OFFSET))
#define GALLERY_DATA ((unsigned char (*)[GALLERY_SLOT_BYTES])(wifi_aux+GALLERY_DATA_OFFSET))

enum { GALLERY_PAGE = 24, GALLERY_MAX_FILE = 256u*1024u*1024u };
_Static_assert(GALLERY_PAGE*(GALLERY_NAME_BYTES+64u)+64u<GALLERY_CHUNK,"Gallery page fits one slot");
static z72_u32 gallery_task_control[8];
static _Alignas(4) unsigned char gallery_descriptor[28];
static int gallery_worker_started;
static void *gallery_send_owner;
static z72_u32 gallery_send_address, gallery_send_serial;

static void gallery_delay(z72_u32 ms)
{
    Z72_CARD_CALL(0x103dc539u,void,z72_u32)(ms);
}

static int request_active(z72_u32 serial,z72_u32 epoch)
{
    invalidate(GALLERY_REQUEST,64);
    return GALLERY_REQUEST->serial==serial && GALLERY_REQUEST->cancelled!=serial &&
        GALLERY_CONTROL->epoch==epoch;
}

static void gallery_release_buffer(void)
{
    if (!gallery_send_owner) return;
    invalidate(GALLERY_STATE,64);
    if ((GALLERY_CONTROL->epoch&1u) && GALLERY_STATE->network) return;
    invalidate(GALLERY_REQUEST,64);
    /* The auxiliary task may still be about to attach the last address we
     * published. Wait for its acknowledgement before looking at ring ownership. */
    if (GALLERY_REQUEST->serial==gallery_send_serial &&
        GALLERY_REQUEST->cancelled!=gallery_send_serial) return;
    invalidate(GALLERY_STATE,64);
    if (GALLERY_STATE->large_in_use) return;
    tcp_free_send_buffer(gallery_send_owner,gallery_send_address);
    gallery_send_owner=0;
    gallery_send_address=0;
}

static char *copy_text(char *out,const char *in)
{
    while (*in) *out++=*in++;
    return out;
}

static char *copy_id(char *out,z72_u32 id)
{
    for (unsigned shift=32;shift;shift-=4) *out++="0123456789abcdef"[(id>>(shift-4))&15u];
    return out;
}

static char *basename(char *path)
{
    char *name=path;
    for (;*path;++path) if (*path=='\\') name=path+1;
    return name;
}

static int photo_format(int format)
{
    return format==FTP_MEDIA_JPEG || format==FTP_MEDIA_NEF;
}

static z72_u32 photo_folder(z72_u32 id) { return (id>>19)&0x3ffu; }

static int photo_id(z72_u32 id)
{
    return id>>29==1u && photo_folder(id)>=100u && photo_folder(id)<=999u;
}

static void set_extension(char *path,int raw)
{
    char *end=path;
    while (*end) ++end;
    copy_text(end-3,raw ? "NEF" : "JPG");
}

static int open_photo(z72_u32 *file,const char *path)
{
    if (!card_open(file,path)) return 0;
    if (!file[7] && file[6] && file[6]<=GALLERY_MAX_FILE) return 1;
    card_close(file);
    return 0;
}

static z72_u32 gallery_list(z72_u32 cursor,z72_u32 serial,z72_u32 epoch)
{
    unsigned short folder=cursor ? (unsigned short)photo_folder(cursor) : 1000u;
    z72_u32 id=cursor,last=cursor,previous=cursor ? ftp_media_key(cursor) : 0;
    unsigned count=0,scanned=0;
    char *start=(char *)GALLERY_DATA[0],*out=copy_text(start,"{\"items\":[");
    while (scanned<256u && request_active(serial,epoch)) {
        if (!id && !ftp_previous_folder(&folder)) { last=0; break; }
        id=ftp_find_photo(folder,id,1);
        if (!id) continue;
        ++scanned;
        z72_u32 key=ftp_media_key(id);
        char path[FTP_PATH_BYTES];
        int format=key!=previous ? ftp_media_path(id,path) : 0;
        if (!photo_format(format)) { last=id; continue; }
        if (count==GALLERY_PAGE) break;
        previous=key;
        last=id;
        if (count++) *out++=',';
        out=copy_text(out,"{\"id\":\"");
        out=copy_id(out,id);
        out=copy_text(out,"\",\"name\":\"");
        out=copy_text(out,basename(path));
        set_extension(path,format==FTP_MEDIA_JPEG);
        z72_u32 alternate[10];
        int pair=open_photo(alternate,path);
        if (pair) card_close(alternate);
        out=copy_text(out,"\",\"jpg\":");
        *out++=(char)('0'+(pair || format==FTP_MEDIA_JPEG));
        out=copy_text(out,",\"nef\":");
        *out++=(char)('0'+(pair || format==FTP_MEDIA_NEF));
        *out++='}';
    }
    out=copy_text(out,"],\"next\":\"");
    if (last) out=copy_id(out,last);
    out=copy_text(out,"\"}");
    z72_u32 length=(z72_u32)(out-start);
    publish(start,length);
    return length;
}

static int open_preview(z72_u32 *file,char path[FTP_PATH_BYTES],int raw,int large,
                        struct gallery_image *image)
{
    set_extension(path,raw);
    if (!open_photo(file,path)) return 0;
    *image=gallery_embedded(file,raw,large);
    if (image->length && gallery_seek(file,image->offset)) return 1;
    card_close(file);
    return 0;
}

static int gallery_open(z72_u32 route,z72_u32 id,z72_u32 *file,char path[FTP_PATH_BYTES],
                        struct gallery_image *image)
{
    if (!photo_id(id) || !photo_format(ftp_media_path(id,path))) return 0;
    if (route==GALLERY_JPG || route==GALLERY_NEF || route==GALLERY_VIEW_JPG) {
        set_extension(path,route==GALLERY_NEF);
        if (!open_photo(file,path)) return 0;
        *image=(struct gallery_image){0,file[6]};
        return 1;
    }
    int large=route==GALLERY_PREVIEW;
    return open_preview(file,path,large,large,image) ||
        open_preview(file,path,!large,large,image);
}

static void gallery_reply(z72_u32 serial,z72_u32 status,z72_u32 length,z72_u32 produced,
                          const char *name,z72_u32 send_address)
{
    GALLERY_RESPONSE->status=status;
    GALLERY_RESPONSE->length=length;
    GALLERY_RESPONSE->produced=produced;
    GALLERY_RESPONSE->failed=0;
    *copy_text(GALLERY_RESPONSE->name,name)=0;
    GALLERY_RESPONSE->send_address=send_address;
    publish(GALLERY_RESPONSE,64);
    GALLERY_RESPONSE->serial=serial;
    publish(GALLERY_RESPONSE,64);
}

static void gallery_stream(z72_u32 serial,z72_u32 epoch,z72_u32 *file,struct gallery_image image)
{
    z72_u32 offset=0,produced=0;
    while (offset<image.length && request_active(serial,epoch)) {
        z72_u32 consumed=GALLERY_REQUEST->consumed;
        if (consumed>produced || produced-consumed>=GALLERY_SLOTS) {
            gallery_delay(2);
            continue;
        }
        z72_u32 size=image.length-offset;
        if (size>GALLERY_CHUNK) size=GALLERY_CHUNK;
        unsigned char *slot=GALLERY_DATA[produced%GALLERY_SLOTS];
        if (!card_read(file,slot,GALLERY_SLOT_BYTES,size,image.offset+offset+size==file[6])) {
            GALLERY_RESPONSE->failed=1;
            publish(GALLERY_RESPONSE,64);
            break;
        }
        offset+=size;
        GALLERY_RESPONSE->produced=++produced;
        publish(GALLERY_RESPONSE,64);
    }
}

static void gallery_worker(void *argument)
{
    (void)argument;
    z72_u32 last=0;
    for (;;) {
        gallery_release_buffer();
        invalidate(GALLERY_REQUEST,64);
        z72_u32 serial=GALLERY_REQUEST->serial;
        if (!serial || serial==last) {
            gallery_delay(GALLERY_CONTROL->epoch&1u ? 10u : 100u);
            continue;
        }
        z72_u32 epoch=GALLERY_REQUEST->epoch,route=GALLERY_REQUEST->route,value=GALLERY_REQUEST->value;
        if (request_active(serial,epoch)) {
            if (route==GALLERY_LIST && (!value || photo_id(value))) {
                z72_u32 length=gallery_list(value,serial,epoch);
                gallery_reply(serial,200,length,1,"",0);
            } else if (route>=GALLERY_THUMB && route<=GALLERY_VIEW_JPG) {
                char path[FTP_PATH_BYTES];
                z72_u32 file[10];
                struct gallery_image image;
                if (gallery_open(route,value,file,path,&image)) {
                    if (request_active(serial,epoch)) {
                        z72_u32 flags=GALLERY_REQUEST->flags,send_address=0;
                        z72_u32 large=(flags&GALLERY_ARCHIVE) ? flags&GALLERY_LARGE :
                            image.length>=TCP_LARGE_SEND_BYTES;
                        if ((route==GALLERY_JPG || route==GALLERY_NEF) &&
                            !(flags&GALLERY_META) && large) {
                            if (!gallery_send_owner)
                                gallery_send_address=tcp_allocate_send_buffer(2,&gallery_send_owner);
                            send_address=gallery_send_address;
                            if (send_address) gallery_send_serial=serial;
                        }
                        const char *name=(flags&GALLERY_ARCHIVE) ? path+8 : basename(path);
                        gallery_reply(serial,200,image.length,0,name,send_address);
                        if (!(flags&GALLERY_META)) gallery_stream(serial,epoch,file,image);
                    }
                    card_close(file);
                } else gallery_reply(serial,404,0,0,"",0);
            } else gallery_reply(serial,404,0,0,"",0);
        }
        last=serial;
        GALLERY_RESPONSE->released=serial;
        publish(GALLERY_RESPONSE,64);
    }
}

static int gallery_holds_wifi(void)
{
    return GALLERY_CONTROL->epoch&1u;
}


static int gallery_service_ready(void)
{
    const unsigned char *original=Z72_CARD_CALL(0x103d3dffu,const unsigned char *,z72_u32)(0x10005);
    z72_u32 addr=(z72_u32)original;
    if ((addr&3u) || !((addr>=0x10020000u && addr<=0x12497b78u-sizeof(gallery_descriptor)) ||
        (addr>=0xb1800000u && addr<=0xb5f00000u-sizeof(gallery_descriptor)))) return 0;
    for (z72_u32 i=0;i<sizeof(gallery_descriptor);++i) gallery_descriptor[i]=original[i];
    gallery_descriptor[0]=1;
    *(z72_u32 *)(gallery_descriptor+8)=8192;
    *(unsigned short *)(gallery_descriptor+26)=0;
    return 1;
}

static int gallery_set_enabled(int enabled)
{
    if (!!enabled==gallery_holds_wifi()) return 1;
    if (enabled && !gallery_worker_started) {
        if (!(int)gallery_task_control[0]) Z72_CARD_CALL(0x103dc46bu,int,void *,void (*)(void *),void *)(
            gallery_task_control,gallery_worker,gallery_descriptor);
        if ((int)gallery_task_control[0]<=0 || Z72_CARD_CALL(0x103606dfu,int,void *)(gallery_task_control)) {
            return 0;
        }
        gallery_worker_started=1;
    }
    ++GALLERY_CONTROL->epoch;
    publish(GALLERY_CONTROL,64);
    return 1;
}

#endif
