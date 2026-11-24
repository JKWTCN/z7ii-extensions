/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_GALLERY_SERVICE_H
#define Z72_WIFI_GALLERY_SERVICE_H

#include "wifi_gallery.h"
#include "composition_reference_format.h"
#include "dcim_index.h"
#include "playback_filter_config.h"
#include "tcp_send_storage.h"
#include "wifi_gallery_preview.h"

#define GALLERY_CONTROL ((struct gallery_control *)(wifi_aux+GALLERY_CONTROL_OFFSET))
#define GALLERY_REQUEST ((const struct gallery_request *)(wifi_aux+GALLERY_REQUEST_OFFSET))
#define GALLERY_RESPONSE ((struct gallery_response *)(wifi_aux+GALLERY_RESPONSE_OFFSET))
#define GALLERY_STATE ((const struct gallery_state *)(wifi_aux+GALLERY_STATE_OFFSET))
#define GALLERY_DATA ((unsigned char (*)[GALLERY_SLOT_BYTES])(wifi_aux+GALLERY_DATA_OFFSET))
#define GALLERY_UPLOAD ((unsigned char *)(wifi_aux+GALLERY_UPLOAD_OFFSET))

enum { GALLERY_PAGE = 24, GALLERY_MAX_FILE = 256u*1024u*1024u };
_Static_assert(GALLERY_PAGE*(GALLERY_NAME_BYTES+64u)+64u<GALLERY_CHUNK,"Gallery page fits one slot");
static z72_u32 gallery_task_control[8];
static _Alignas(4) unsigned char gallery_descriptor[28];
static int gallery_worker_started;
static void *gallery_send_owner;
static z72_u32 gallery_send_address, gallery_send_serial;
#ifdef Z72_COMPOSITION_REFERENCE
static z72_u32 gallery_reference_uploads[Z72_REFERENCE_SLOTS];
#endif

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

static z72_u32 gallery_save_reference(z72_u32 slot,z72_u32 serial,z72_u32 epoch)
{
    if (!slot || slot>Z72_REFERENCE_SLOTS) return 400;
    invalidate(GALLERY_UPLOAD,REFERENCE_BYTES);
    if (!reference_header_valid((const z72_u32 *)GALLERY_UPLOAD)) return 400;

    char path[]="B:\\Z72REF1.RFG",temp[]="B:\\Z72REF1.TMP",backup[]="B:\\Z72REF1.BAK";
    path[9]=temp[9]=backup[9]=(char)('0'+slot);
    int handle=Z72_CARD_CALL(0x105beb2bu,int,const char *,z72_u32)(temp,0);
    if (handle<0) return 500;
    z72_u32 offset=0;
    while (offset<REFERENCE_BYTES && request_active(serial,epoch)) {
        z72_u32 size=REFERENCE_BYTES-offset,actual=0;
        if (size>GALLERY_CHUNK) size=GALLERY_CHUNK;
        struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,GALLERY_UPLOAD+offset,size};
        if (Z72_CARD_CALL(0x105bea79u,z72_u32,int,void *,z72_u32,z72_u32 *)(
                handle,&request,size,&actual) || actual!=size) break;
        offset+=size;
    }
    z72_u32 closed=Z72_CARD_CALL(0x105be9ddu,z72_u32,int)(handle);
    int active=request_active(serial,epoch);
    if (closed || offset!=REFERENCE_BYTES || !active) {
        Z72_CARD_CALL(0x105beb81u,z72_u32,const char *)(temp);
        return active ? 500 : 503;
    }

    z72_u32 file[10];
    int saved=0,backed_up=0;
    /* Publish by rename only after the complete upload is closed. During
     * replacement the old image remains readable through its backup. */
    if (card_open(file,path)) {
        if (!card_close(file)) goto cleanup;
        if (card_open(file,backup)) {
            if (!card_close(file) || Z72_CARD_CALL(0x105beb81u,z72_u32,const char *)(backup))
                goto cleanup;
        }
        if (Z72_CARD_CALL(0x105beb5bu,z72_u32,const char *,const char *)(path,backup)) goto cleanup;
        backed_up=1;
    }
    saved=!Z72_CARD_CALL(0x105beb5bu,z72_u32,const char *,const char *)(temp,path);
    if (saved) {
#ifdef Z72_COMPOSITION_REFERENCE
        __atomic_add_fetch(&gallery_reference_uploads[slot-1],1,__ATOMIC_RELEASE);
#endif
        if (backed_up || card_open(file,backup)) {
            if (backed_up || card_close(file))
                Z72_CARD_CALL(0x105beb81u,z72_u32,const char *)(backup);
        }
        return 200;
    }
    if (backed_up) Z72_CARD_CALL(0x105beb5bu,z72_u32,const char *,const char *)(backup,path);
cleanup:
    Z72_CARD_CALL(0x105beb81u,z72_u32,const char *)(temp);
    return 500;
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

static z72_u32 gallery_list(z72_u32 cursor,z72_u32 serial,z72_u32 epoch,z72_u32 flags,z72_u32 dates)
{
    z72_u32 rating=(flags>>GALLERY_RATING_SHIFT)&7u,protection=(flags>>GALLERY_PROTECTION_SHIFT)&3u;
    z72_u32 card=(flags>>GALLERY_CARD_SHIFT)&3u;
    /* Native rating bits 1–6 represent no rating and one through five stars. */
    const unsigned short predicate[]={(unsigned short)(1u<<rating),0xfff};
    struct dcim_folder folder=cursor ?
        (struct dcim_folder){dcim_id_card(cursor),(unsigned short)dcim_id_folder(cursor)} :
        (struct dcim_folder){card ? card-1u : DCIM_SD,DCIM_FOLDER_START};
    z72_u32 id=cursor,last=cursor,previous=cursor ? ftp_media_key(cursor) : 0;
    /* A scan can stop on a rejected NEF immediately before its matching JPG. */
    if ((protection || dates) && (cursor&15u)==1u) {
        char path[FTP_PATH_BYTES];
        if (!photo_format(dcim_media_path(cursor,path,protection,dates))) previous=0;
    }
    unsigned count=0,scanned=0;
    char *start=(char *)GALLERY_DATA[0],*out=copy_text(start,"{\"items\":[");
    while (scanned<256u && request_active(serial,epoch)) {
        if (!id && !(card ? dcim_previous_folder(folder.card,&folder.number) :
                     dcim_previous_card_folder(&folder))) { last=0; break; }
        id=dcim_find_photo(folder.card,folder.number,id,1,rating ? predicate : 0);
        if (!id) continue;
        ++scanned;
        z72_u32 key=ftp_media_key(id);
        char path[FTP_PATH_BYTES];
        int format=key!=previous ? dcim_media_path(id,path,protection,dates) : 0;
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
        out=copy_text(out,"\",\"card\":");
        *out++=(char)('0'+folder.card);
        out=copy_text(out,",\"jpg\":");
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
    if (!photo_format(ftp_media_path(id,path))) return 0;
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
            if (route==GALLERY_REFERENCE_UPLOAD) {
                z72_u32 status=gallery_save_reference(value,serial,epoch);
                gallery_reply(serial,status,0,0,"",0);
            } else if (route==GALLERY_LIST && (!value || dcim_card_id(value))) {
                z72_u32 dates=GALLERY_REQUEST->dates,flags=GALLERY_REQUEST->flags;
                z72_u32 card=(flags>>GALLERY_CARD_SHIFT)&3u;
                if ((!card || (card<=DCIM_CARD_COUNT && (!value || card==dcim_id_card(value)+1u))) &&
                    z72_playback_filter_valid((dates&0xffffu)<<Z72_DATE_SHIFT) &&
                    z72_playback_filter_valid((dates>>16)<<Z72_DATE_SHIFT)) {
                    z72_u32 length=gallery_list(value,serial,epoch,flags,dates);
                    gallery_reply(serial,200,length,1,"",0);
                } else gallery_reply(serial,400,0,0,"",0);
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
                        char archive[FTP_PATH_BYTES];
                        const char *name=basename(path);
                        if (flags&GALLERY_ARCHIVE) {
                            archive[0]=path[0];
                            archive[1]='/';
                            *copy_text(archive+2,path+8)=0;
                            name=archive;
                        }
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
