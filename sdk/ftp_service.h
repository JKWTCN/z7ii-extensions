/* SPDX-License-Identifier: MIT */
#ifndef Z72_FTP_SERVICE_H
#define Z72_FTP_SERVICE_H

#include "ftp_card.h"
#include "dcim_index.h"
#include "tcp_send_storage.h"
#define FTP_REQ ((struct ftp_request *)(wifi_aux+FTP_REQUEST_OFFSET))
#define FTP_RES ((const struct ftp_result *)(wifi_aux+FTP_RESULT_OFFSET))
#define FTP_DATA (wifi_aux+FTP_PAYLOAD_OFFSET)

enum ftp_local_state {
    FTP_LOCAL_IDLE, FTP_LOCAL_CONFIG, FTP_LOCAL_OPEN, FTP_LOCAL_BUSY,
    FTP_LOCAL_READ, FTP_LOCAL_TIMEOUT, FTP_LOCAL_COMPLETE, FTP_LOCAL_NETWORK,
    FTP_LOCAL_TASK, FTP_LOCAL_CANCELLED, FTP_LOCAL_WAIT_WIFI, FTP_LOCAL_TOO_LARGE,
};
enum ftp_job_type { FTP_JOB_NONE, FTP_JOB_UPLOAD, FTP_JOB_CONFIG_SAVE,
                    FTP_JOB_FOLDER_SEND, FTP_JOB_AUTO, FTP_JOB_CLAIM };
enum ftp_edit_state { FTP_EDIT_READY, FTP_EDIT_NEW, FTP_EDIT_BAD_ADDRESS, FTP_EDIT_BAD_USER,
                      FTP_EDIT_SAVED, FTP_EDIT_SAVE_FAILED };
enum { FTP_FOLDERS=6, FTP_CONFIGS=3, FTP_BATCH=1000, FTP_AUTO_CAPACITY=128 };
_Static_assert(Z72_FTP_FILE_LABEL_UNITS>=FTP_PATH_BYTES-8u+2u,"FTP photo label with selection mark");
enum { FTP_IMAGE_SLOT=0xb1888a9cu, FTP_IMAGE_CALLBACK=0x106d2f6du, FTP_IMAGE_CLOSED=0x041f0002u };
struct ftp_cursor { struct dcim_folder folder; z72_u32 id; };
struct ftp_photo { z72_u32 id; char path[FTP_PATH_BYTES]; };
static const char *const ftp_profiles[FTP_CONFIGS]={
    "B:\\Z72FTP.CFG","B:\\Z72FTP2.CFG","B:\\Z72FTP3.CFG"
};
static const char *const ftp_backups[FTP_CONFIGS]={
    "B:\\Z72FTP.BAK","B:\\Z72FTP2.BAK","B:\\Z72FTP3.BAK"
};
static const char *const ftp_temporary[FTP_CONFIGS]={
    "B:\\Z72FTP.TMP","B:\\Z72FTP2.TMP","B:\\Z72FTP3.TMP"
};
static struct ftp_config ftp_draft;
static z72_u32 ftp_edit_status;
static struct ftp_photo ftp_photos[Z72_FTP_PHOTOS_PER_LIST],ftp_batch[FTP_BATCH];
static struct ftp_cursor ftp_cursor;
static z72_u32 ftp_photo_count, ftp_more, ftp_profile, ftp_task_control[8];
static z72_u32 ftp_job, ftp_cancel, ftp_local, ftp_generation, ftp_total;
static z72_u32 ftp_first,ftp_last,ftp_completed,ftp_jpeg_only,ftp_overwrite,ftp_queue_full,ftp_discard;
static z72_u32 ftp_folder_count,ftp_folder_more;
static struct dcim_folder ftp_folder_next,ftp_folder_send,ftp_folders[FTP_FOLDERS];
static struct { z72_u32 id,profile; } ftp_auto[FTP_AUTO_CAPACITY];
static z72_u32 ftp_auto_read,ftp_auto_write,ftp_auto_epoch,ftp_auto_dropped,ftp_auto_guard,ftp_auto_retry;
static z72_u32 ftp_auto_error,ftp_auto_profile,ftp_auto_lookup_tries,ftp_shared_image_hook;
static _Alignas(4) unsigned char ftp_descriptor[28];
static z72_u32 ftp_main_tick(void) { return FTP_CALL(0x1062e59du,z72_u32,void)(); }

static z72_u32 ftp_load(const z72_u32 *p) { return __atomic_load_n(p,__ATOMIC_ACQUIRE); }
static void ftp_store(z72_u32 *p,z72_u32 value) { __atomic_store_n(p,value,__ATOMIC_RELEASE); }
static void ftp_delay(z72_u32 ms) { FTP_CALL(0x103dc539u,void,z72_u32)(ms); }

static int ftp_claim_job(void)
{
    z72_u32 idle=FTP_JOB_NONE;
    return __atomic_compare_exchange_n(&ftp_job,&idle,FTP_JOB_CLAIM,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}

static void ftp_auto_observe(z72_u32 event,z72_u32 argument,z72_u32 *epoch_out,
                             z72_u32 *profile_out,z72_u32 *id_out)
{
    z72_u32 epoch=ftp_load(&ftp_auto_epoch),profile=ftp_load(&ftp_auto_profile),id=0;
    if ((epoch&1u) && event==FTP_IMAGE_CLOSED && !(argument&3u) &&
        argument>=0xb1800000u && argument<=0xb5f00000u-20u)
        id=*(const z72_u32 *)argument;
    *epoch_out=epoch;
    *profile_out=profile;
    *id_out=id;
}

static void ftp_auto_enqueue(z72_u32 id,z72_u32 epoch,z72_u32 profile)
{
    if (!dcim_card_id(id) || (id&15u)>1u || epoch!=ftp_load(&ftp_auto_epoch)) return;
    z72_u32 free=0;
    if (!__atomic_compare_exchange_n(&ftp_auto_guard,&free,1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) {
        __atomic_add_fetch(&ftp_auto_dropped,1,__ATOMIC_RELAXED);
        return;
    }
    z72_u32 write=ftp_load(&ftp_auto_write),read=ftp_load(&ftp_auto_read);
    if (write-read==FTP_AUTO_CAPACITY) __atomic_add_fetch(&ftp_auto_dropped,1,__ATOMIC_RELAXED);
    else {
        ftp_auto[write%FTP_AUTO_CAPACITY].id=id;
        ftp_auto[write%FTP_AUTO_CAPACITY].profile=profile;
        ftp_store(&ftp_auto_write,write+1u);
    }
    ftp_store(&ftp_auto_guard,0);
}

static void ftp_image_closed(z72_u32 event,z72_u32 argument)
{
    z72_u32 epoch,profile,id;
    ftp_auto_observe(event,argument,&epoch,&profile,&id);
    FTP_CALL(FTP_IMAGE_CALLBACK,void,z72_u32,z72_u32)(event,argument);
    ftp_auto_enqueue(id,epoch,profile);
}

static int ftp_read_profile(struct ftp_config *config,z72_u32 profile)
{
    int result=read_config(config,ftp_profiles[profile]);
    if (result==FTP_CONFIG_MISSING) result=read_config(config,ftp_backups[profile]);
    return result==FTP_CONFIG_VALID;
}

static void ftp_edit_read(void)
{
    if (!ftp_read_profile(&ftp_draft,ftp_profile)) {
        ftp_draft.address[0]=ftp_draft.user[0]=ftp_draft.password[0]=ftp_draft.folder[0]=0;
        ftp_edit_status=FTP_EDIT_NEW;
        return;
    }
    struct ftp_request address;
    ftp_edit_status=endpoint(ftp_draft.address,&address) ? FTP_EDIT_READY : FTP_EDIT_BAD_ADDRESS;
}

static int ftp_config_save(void)
{
    const char *path=ftp_profiles[ftp_profile], *backup=ftp_backups[ftp_profile],
               *temp=ftp_temporary[ftp_profile];
    _Alignas(64) char data[256];
    const char *fields[]={ftp_draft.address,ftp_draft.user,ftp_draft.password};
    z72_u32 count=0,actual=0;
    for (z72_u32 i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
        for (const char *p=fields[i];*p;++p) data[count++]=*p;
        data[count++]='\r'; data[count++]='\n';
    }
    if (ftp_draft.folder[0]) {
        data[count++]='C'; data[count++]='W'; data[count++]='D'; data[count++]=':';
        for (const char *p=ftp_draft.folder;*p;++p) data[count++]=*p;
        data[count++]='\r'; data[count++]='\n';
    }
    int handle=FTP_CALL(0x105beb2bu,int,const char *,z72_u32)(temp,0);
    if (handle<0) return 0;
    struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,data,count};
    publish(data,sizeof(data));
    z72_u32 wrote=FTP_CALL(0x105bea79u,z72_u32,int,void *,z72_u32,z72_u32 *)(handle,&request,count,&actual);
    z72_u32 closed=FTP_CALL(0x105be9ddu,z72_u32,int)(handle);
    if (wrote || closed || actual!=count) return 0;
    z72_u32 file[10];
    if (!card_open(file,temp)) return 0;
    int read=file[6]==count && !file[7] && card_read(file,card_buffer,sizeof(card_buffer),count,1);
    if (!card_close(file) || !read) return 0;
    for (z72_u32 i=0;i<count;++i) if ((unsigned char)data[i]!=card_buffer[i]) return 0;

    /* The old profile remains readable through its backup between the two renames. */
    if (card_open(file,path)) {
        if (!card_close(file)) return 0;
        if (card_open(file,backup)) {
            if (!card_close(file) || FTP_CALL(0x105beb81u,z72_u32,const char *)(backup)) return 0;
        }
        if (FTP_CALL(0x105beb5bu,z72_u32,const char *,const char *)(path,backup)) return 0;
    }
    return !FTP_CALL(0x105beb5bu,z72_u32,const char *,const char *)(temp,path);
}

static z72_u32 ftp_large_send_address;
static void *ftp_large_raw;
static z72_u32 ftp_large_enabled,ftp_large_state,ftp_large_link,ftp_large_save_failed,ftp_large_attempt;
enum { FTP_BUFFER_OFF, FTP_BUFFER_READY, FTP_BUFFER_NO_MEMORY, FTP_BUFFER_RELEASING };

static z72_u32 ftp_prepare_large_send(z72_u32 total)
{
    if (total<FTP_LARGE_SEND_BYTES || ftp_large_send_address) return ftp_large_send_address;
    ftp_large_send_address=tcp_allocate_send_buffer(2,&ftp_large_raw);
    return ftp_large_send_address;
}

static z72_u32 ftp_wifi_link(void)
{
    z72_u32 mode=FTP_RES->network;
    return mode ? (FTP_RES->wifi_stop<<2)|mode : 0;
}

/* The main FTP worker owns the allocation. It calls this only between
 * requests, after the auxiliary task has acknowledged the previous one. */
static void ftp_large_update(z72_u32 retry_now)
{
    if (!ftp_large_raw && !ftp_load(&ftp_large_enabled) && !ftp_large_link) return;
    invalidate(FTP_RES,sizeof(*FTP_RES));
    z72_u32 link=ftp_load(&ftp_large_enabled) ? ftp_wifi_link() : 0;
    if (link==ftp_large_link && ftp_load(&ftp_large_state)!=FTP_BUFFER_RELEASING) {
        if (!link || ftp_large_raw) return;
        if (!retry_now && ftp_main_tick()-ftp_large_attempt<2000u) return;
    }
    if (ftp_large_raw) {
        if (FTP_RES->large_in_use) {
            ftp_store(&ftp_large_state,FTP_BUFFER_RELEASING);
            return;
        }
        tcp_free_send_buffer(ftp_large_raw,ftp_large_send_address);
        ftp_large_raw=0;
        ftp_large_send_address=0;
    }
    ftp_large_link=link;
    z72_u32 state=FTP_BUFFER_OFF;
    if (link) {
        ftp_large_attempt=ftp_main_tick();
        state=ftp_prepare_large_send(FTP_LARGE_SEND_BYTES) ? FTP_BUFFER_READY : FTP_BUFFER_NO_MEMORY;
    }
    ftp_store(&ftp_large_state,state);
}

static const char *ftp_basename(const char *path)
{
    const char *name=path;
    for (;*path;++path) if (*path=='\\' || *path=='/') name=path+1;
    return name;
}

static void ftp_scan(z72_u32 limit)
{
    z72_u32 count=0;
    for (z72_u32 tries=0;tries<3u*Z72_FTP_PHOTOS_PER_LIST;++tries) {
        struct ftp_cursor previous=ftp_cursor;
        if (!ftp_cursor.id && !dcim_previous_card_folder(&ftp_cursor.folder)) break;
        ftp_cursor.id=ftp_find_photo(ftp_cursor.folder.card,ftp_cursor.folder.number,ftp_cursor.id,1);
        struct ftp_photo photo;
        if (!ftp_cursor.id || !ftp_media_path(ftp_cursor.id,photo.path)) continue;
        if (count==limit) {
            /* Leave this photo for the next list. */
            ftp_cursor=previous;
            break;
        }
        photo.id=ftp_cursor.id;
        ftp_photos[count++]=photo;
    }
    ftp_photo_count=count;
    ftp_more=ftp_cursor.folder.number>=100;
}

static void ftp_scan_folders(void)
{
    struct dcim_folder folder=ftp_folder_next;
    z72_u32 count=0;
    while (count<FTP_FOLDERS && dcim_previous_card_folder(&folder)) ftp_folders[count++]=folder;
    ftp_folder_count=count;
    ftp_folder_more=count==FTP_FOLDERS;
    if (count) ftp_folder_next=ftp_folders[count-1];
}

static z72_u32 ftp_pending(void)
{
    z72_u32 first=ftp_load(&ftp_first),last=ftp_load(&ftp_last);
    return first<last ? last-first : 0;
}

static void ftp_compact_batch(void)
{
    z72_u32 first=ftp_load(&ftp_first),last=ftp_load(&ftp_last);
    for (z72_u32 i=first;i<last;++i) ftp_batch[i-first]=ftp_batch[i];
    ftp_store(&ftp_first,0);
    ftp_store(&ftp_last,last-first);
}

static z72_u32 ftp_marked(z72_u32 id)
{
    z72_u32 first=ftp_load(&ftp_first),last=ftp_load(&ftp_last);
    for (z72_u32 i=first;i<last;++i)
        if (ftp_media_key(ftp_batch[i].id)==ftp_media_key(id)) return 1;
    return 0;
}

static int ftp_companion(const char *path,char other[FTP_PATH_BYTES],int jpeg_only)
{
    z72_u32 length=0;
    while (path[length]) {
        other[length]=path[length];
        ++length;
    }
    int format=ftp_media_format(path+length-4);
    if (format!=FTP_MEDIA_NEF && (jpeg_only || format!=FTP_MEDIA_JPEG)) return 0;
    ftp_copy_field(other+length-3,format==FTP_MEDIA_NEF ? "JPG" : "NEF");
    z72_u32 file[10];
    if (!card_open(file,other)) return 0;
    int valid=file[6] && !file[7];
    return card_close(file) && valid;
}

static int ftp_mark(const struct ftp_photo *photo)
{
    if (ftp_marked(photo->id)) return 1;
    struct ftp_photo other;
    other.id=photo->id^1u;
    z72_u32 pair=ftp_companion(photo->path,other.path,0);
    z72_u32 last=ftp_load(&ftp_last);
    if (last+1u+pair>FTP_BATCH) { ftp_store(&ftp_queue_full,1); return 0; }
    ftp_batch[last]=*photo;
    if (pair) ftp_batch[last+1]=other;
    ftp_store(&ftp_last,last+1u+pair);
    return 1;
}

static void ftp_unmark(z72_u32 id)
{
    z72_u32 last=ftp_load(&ftp_last),count=0;
    for (z72_u32 i=0;i<last;++i)
        if (ftp_media_key(ftp_batch[i].id)!=ftp_media_key(id)) ftp_batch[count++]=ftp_batch[i];
    ftp_store(&ftp_last,count);
}

static int ftp_toggle_mark(const struct ftp_photo *photo)
{
    if (!ftp_claim_job()) return 0;
    ftp_compact_batch();
    ftp_store(&ftp_queue_full,0);
    if (!ftp_marked(photo->id)) ftp_mark(photo);
    else ftp_unmark(photo->id);
    ftp_store(&ftp_job,FTP_JOB_NONE);
    return !ftp_load(&ftp_queue_full);
}

static void ftp_send_file(z72_u32 *file,z72_u32 generation)
{
    z72_u32 offset=0,idle=0;
    z72_u32 outcome=FTP_LOCAL_BUSY;
    for (;;) {
        invalidate(FTP_RES,sizeof(*FTP_RES));
        z72_u32 ours=FTP_RES->generation==generation;
        if (ours && FTP_RES->done) {
            if (outcome==FTP_LOCAL_BUSY)
                outcome=FTP_RES->stage==FTP_DONE ? FTP_LOCAL_COMPLETE : FTP_LOCAL_NETWORK;
            break;
        }
        z72_u32 cancel=ftp_load(&ftp_cancel),disconnected=0;
        disconnected=FTP_REQ->large_send_address && ftp_wifi_link()!=ftp_large_link;
        if (outcome==FTP_LOCAL_BUSY && (cancel || disconnected || idle>=150000u)) {
            outcome=cancel ? FTP_LOCAL_CANCELLED : disconnected ? FTP_LOCAL_NETWORK : FTP_LOCAL_TIMEOUT;
            FTP_REQ->state=(cancel || disconnected) ? FTP_FILE_CANCELLED : FTP_FILE_TIMEOUT;
            publish(FTP_REQ,sizeof(*FTP_REQ));
        }
        /* The pending request stays owned until the auxiliary task has
         * acknowledged it and closed both sockets. */
        z72_u32 produced=FTP_REQ->sequence;
        z72_u32 consumed=ours ? FTP_RES->consumed : 0;
        z72_u32 stage=ours ? FTP_RES->stage : FTP_WAIT_AP;
        z72_u32 streaming=stage==FTP_READ_CARD || stage==FTP_SEND;
        if (outcome==FTP_LOCAL_BUSY && streaming && consumed<=produced &&
            produced-consumed<FTP_SLOTS && offset<FTP_REQ->total) {
            z72_u32 size=FTP_REQ->total-offset;
            if (size>FTP_CHUNK) size=FTP_CHUNK;
            unsigned char *slot=FTP_DATA+(produced%FTP_SLOTS)*FTP_SLOT_BYTES;
            int read_ok=card_read(file,slot,FTP_SLOT_BYTES,size,offset+size==FTP_REQ->total);
            if (!read_ok) {
                FTP_REQ->state=FTP_FILE_ERROR;
                publish(FTP_REQ,sizeof(*FTP_REQ));
                outcome=FTP_LOCAL_READ;
                continue;
            }
            FTP_REQ->sequence=produced+1u;
            publish(FTP_REQ,sizeof(*FTP_REQ));
            offset+=size;
            idle=0;
        } else {
            z72_u32 wait=outcome==FTP_LOCAL_BUSY && streaming ? FTP_POLL_MS : 10u;
            ftp_delay(wait);
            idle+=wait;
        }
    }
    ftp_store(&ftp_local,outcome);
}

static int ftp_has_jpeg(const char *source)
{
    char path[FTP_PATH_BYTES];
    return ftp_companion(source,path,1);
}

static void ftp_begin_transfer(void)
{
    ftp_store(&ftp_generation,0);
    ftp_store(&ftp_total,0);
    ftp_store(&ftp_local,FTP_LOCAL_BUSY);
}

static void ftp_upload_file(const char *path,z72_u32 profile,z72_u32 wait_for_network)
{
    ftp_begin_transfer();
    struct ftp_config config;
    /* FTP_IDLE prevents the auxiliary worker from claiming a partial request. */
    FTP_REQ->state=FTP_IDLE;
    FTP_REQ->sequence=FTP_REQ->size=0;
    if (!ftp_read_profile(&config,profile) || !ftp_config_request(FTP_REQ,&config)) {
        ftp_store(&ftp_local,FTP_LOCAL_CONFIG);
        return;
    }
    ftp_copy_field(FTP_REQ->remote,ftp_basename(path));
    FTP_REQ->overwrite=ftp_load(&ftp_overwrite);
    z72_u32 file[10];
    if (!card_open(file,path)) {
        ftp_store(&ftp_local,FTP_LOCAL_OPEN);
        return;
    }
    if (file[7]) ftp_store(&ftp_local,FTP_LOCAL_TOO_LARGE);
    else if (!file[6]) ftp_store(&ftp_local,FTP_LOCAL_OPEN);
    else {
        ftp_store(&ftp_total,file[6]);
        z72_u32 wait=0;
        for (;;) {
            invalidate(FTP_RES,sizeof(*FTP_RES));
            if (ftp_load(&ftp_cancel) || FTP_RES->network || !wait_for_network || wait++>=600u) break;
            ftp_store(&ftp_local,FTP_LOCAL_WAIT_WIFI);
            ftp_delay(250);
        }
        if (ftp_load(&ftp_cancel)) ftp_store(&ftp_local,FTP_LOCAL_CANCELLED);
        else if (!FTP_RES->network) ftp_store(&ftp_local,FTP_LOCAL_TIMEOUT);
        else {
            ftp_large_update(1);
            FTP_REQ->large_send_address=ftp_load(&ftp_large_enabled) &&
                ftp_load(&ftp_large_state)==FTP_BUFFER_READY && ftp_wifi_link()==ftp_large_link ?
                ftp_large_send_address : 0;
            static z72_u32 generation;
            if (!++generation) ++generation;
            ftp_store(&ftp_local,FTP_LOCAL_BUSY);
            FTP_REQ->generation=generation;
            FTP_REQ->total=file[6];
            publish(FTP_REQ,sizeof(*FTP_REQ));
            FTP_REQ->state=FTP_READY;
            publish(FTP_REQ,64);
            ftp_store(&ftp_generation,generation);
            ftp_send_file(file,generation);
        }
    }
    if (!card_close(file) && ftp_load(&ftp_local)==FTP_LOCAL_COMPLETE)
        ftp_store(&ftp_local,FTP_LOCAL_READ);
}

static void ftp_wipe_transfer_password(void)
{
    for (z72_u32 i=0;i<sizeof(FTP_REQ->password);++i) ((volatile char *)FTP_REQ->password)[i]=0;
    publish(FTP_REQ,sizeof(*FTP_REQ));
}

static void ftp_batch_upload(void)
{
    z72_u32 end=ftp_load(&ftp_last),profile=ftp_profile,only_jpeg=ftp_load(&ftp_jpeg_only);
    for (z72_u32 i=ftp_load(&ftp_first);i<end;++i) {
        if (ftp_load(&ftp_cancel)) { ftp_store(&ftp_local,FTP_LOCAL_CANCELLED); return; }
        const struct ftp_photo *photo=&ftp_batch[i];
        if (!only_jpeg || !ftp_has_jpeg(photo->path)) {
            ftp_upload_file(photo->path,profile,1);
            ftp_wipe_transfer_password();
            if (ftp_load(&ftp_local)!=FTP_LOCAL_COMPLETE) return;
            ftp_store(&ftp_completed,ftp_load(&ftp_completed)+1u);
        }
        ftp_store(&ftp_first,i+1u);
    }
    ftp_store(&ftp_local,FTP_LOCAL_COMPLETE);
}

static void ftp_send_folder(void)
{
    struct ftp_photo photo;
    photo.id=0;
    ftp_compact_batch();
    ftp_store(&ftp_queue_full,0);
    for (z72_u32 visited=0;visited<20000u && !ftp_load(&ftp_cancel);++visited) {
        photo.id=ftp_find_photo(ftp_folder_send.card,ftp_folder_send.number,photo.id,0);
        if (!photo.id) break;
        if (ftp_media_path(photo.id,photo.path) && !ftp_mark(&photo)) break;
    }
    if (ftp_load(&ftp_cancel)) ftp_store(&ftp_local,FTP_LOCAL_CANCELLED);
    else if (!ftp_pending()) ftp_store(&ftp_local,FTP_LOCAL_OPEN);
    else ftp_batch_upload();
}

static void ftp_discard_pending(void)
{
    ftp_store(&ftp_first,0);
    ftp_store(&ftp_last,0);
    ftp_store(&ftp_auto_read,ftp_load(&ftp_auto_write));
    ftp_store(&ftp_queue_full,0);
    ftp_store(&ftp_auto_dropped,0);
    ftp_auto_lookup_tries=ftp_auto_retry=0;
    ftp_store(&ftp_discard,0);
}

static int ftp_try_auto(void)
{
    z72_u32 read=ftp_load(&ftp_auto_read);
    if (read==ftp_load(&ftp_auto_write)) return 0;
    invalidate(FTP_RES,sizeof(*FTP_RES));
    if (!FTP_RES->network || ftp_auto_retry) return 0;
    if (!ftp_claim_job()) return 0;
    ftp_store(&ftp_cancel,0);
    if (ftp_load(&ftp_discard)) {
        ftp_store(&ftp_job,FTP_JOB_NONE);
        return 1;
    }
    z72_u32 id=ftp_auto[read%FTP_AUTO_CAPACITY].id;
    z72_u32 profile=ftp_auto[read%FTP_AUTO_CAPACITY].profile;
    char path[FTP_PATH_BYTES];
    ftp_begin_transfer();
    ftp_store(&ftp_job,FTP_JOB_AUTO);
    int found=ftp_media_path(id,path);
    if (!found && !ftp_load(&ftp_cancel) && ++ftp_auto_lookup_tries<4) {
        ftp_auto_retry=2;
        ftp_store(&ftp_job,FTP_JOB_NONE);
        return 0;
    }
    if (found && (!ftp_load(&ftp_jpeg_only) || !ftp_has_jpeg(path))) {
        ftp_upload_file(path,profile,0);
        ftp_wipe_transfer_password();
        z72_u32 state=ftp_load(&ftp_local);
        if (state==FTP_LOCAL_COMPLETE) ftp_store(&ftp_completed,ftp_load(&ftp_completed)+1u);
        else if (state==FTP_LOCAL_NETWORK || state==FTP_LOCAL_TIMEOUT || state==FTP_LOCAL_CONFIG) {
            ftp_auto_retry=20;
            ftp_store(&ftp_job,FTP_JOB_NONE);
            return 0;
        }
    } else ftp_store(&ftp_local,found ? FTP_LOCAL_IDLE : FTP_LOCAL_OPEN);
    ftp_auto_lookup_tries=0;
    ftp_store(&ftp_auto_read,read+1u);
    ftp_store(&ftp_job,FTP_JOB_NONE);
    return 1;
}

static void ftp_worker(void *ignored)
{
    (void)ignored;
    for (;;) {
        z72_u32 job=ftp_load(&ftp_job);
        if (job==FTP_JOB_CONFIG_SAVE)
            ftp_edit_status=ftp_config_save() ? FTP_EDIT_SAVED : FTP_EDIT_SAVE_FAILED;
        else if (job==FTP_JOB_UPLOAD) ftp_batch_upload();
        else if (job==FTP_JOB_FOLDER_SEND) ftp_send_folder();
        else {
            if (job==FTP_JOB_NONE) {
                ftp_large_update(0);
                if (ftp_load(&ftp_discard) && ftp_claim_job()) {
                    ftp_discard_pending();
                    ftp_store(&ftp_job,FTP_JOB_NONE);
                    continue;
                }
                if (ftp_try_auto()) continue;
            }
            if (ftp_auto_retry) --ftp_auto_retry;
            ftp_delay(250);
            continue;
        }
        ftp_store(&ftp_job,FTP_JOB_NONE);
    }
}

static int ftp_service_ready(void)
{
    const unsigned char *original=FTP_CALL(0x103d3dffu,const unsigned char *,z72_u32)(0x10005);
    z72_u32 addr=(z72_u32)original;
    if ((addr&3u) || !((addr>=0x10020000u && addr<=0x12497b78u-sizeof(ftp_descriptor)) ||
        (addr>=0xb1800000u && addr<=0xb5f00000u-sizeof(ftp_descriptor)))) return 0;
    for (z72_u32 i=0;i<sizeof(ftp_descriptor);++i) ftp_descriptor[i]=original[i];
    ftp_descriptor[0]=1;
    *(z72_u32 *)(ftp_descriptor+8)=8192;
    *(unsigned short *)(ftp_descriptor+26)=0;
    return 1;
}

static int ftp_worker_alive;

static int ftp_start_job(z72_u32 job)
{
    if (!ftp_claim_job()) return 0;
    if (!ftp_worker_alive) {
        if (!(int)ftp_task_control[0])
            FTP_CALL(0x103dc46bu,int,void *,void (*)(void *),void *)(
                ftp_task_control,ftp_worker,ftp_descriptor);
        if ((int)ftp_task_control[0]<=0 ||
            FTP_CALL(0x103606dfu,int,void *)(ftp_task_control)) {
            ftp_store(&ftp_local,FTP_LOCAL_TASK);
            ftp_store(&ftp_job,FTP_JOB_NONE);
            return 0;
        }
        ftp_worker_alive=1;
    }
    if (job==FTP_JOB_UPLOAD || job==FTP_JOB_FOLDER_SEND) {
        ftp_store(&ftp_cancel,0);
        ftp_begin_transfer();
    }
    ftp_store(&ftp_job,job);
    return 1;
}

static void ftp_restore_large_option(void)
{
    if (z72_config_current.ftp_4mib) {
        ftp_store(&ftp_large_enabled,1);
        ftp_start_job(FTP_JOB_NONE);
    }
}

static void ftp_set_large(z72_u32 enabled)
{
    if (enabled==ftp_load(&ftp_large_enabled) && !z72_config_pending && !ftp_large_save_failed) return;
    if (enabled && !ftp_worker_alive && !ftp_start_job(FTP_JOB_NONE)) {
        ftp_large_save_failed=1;
        return;
    }
    struct z72_config config=z72_config_current;
    config.ftp_4mib=enabled;
    ftp_large_save_failed=z72_config_save(config)!=Z72_CFG_OK;
    ftp_store(&ftp_large_enabled,enabled);
}

static void ftp_set_auto(z72_u32 enabled)
{
    z72_u32 epoch=ftp_load(&ftp_auto_epoch);
    if (!enabled) {
        if (epoch&1u) ftp_store(&ftp_auto_epoch,epoch+1u);
        ftp_auto_error=0;
        return;
    }
    if (epoch&1u) return;
    if (!ftp_start_job(FTP_JOB_NONE)) { ftp_auto_error=1; return; }
    z72_u32 shared=ftp_load(&ftp_shared_image_hook);
    if (!shared || *(volatile z72_u32 *)FTP_IMAGE_SLOT!=shared) {
        z72_u32 native=FTP_IMAGE_CALLBACK;
        if (!__atomic_compare_exchange_n((z72_u32 *)FTP_IMAGE_SLOT,&native,(z72_u32)ftp_image_closed,0,
                                        __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE) && native!=(z72_u32)ftp_image_closed) {
            ftp_auto_error=1;
            return;
        }
    }
    ftp_store(&ftp_auto_profile,ftp_profile);
    ftp_auto_error=0;
    ftp_store(&ftp_auto_epoch,epoch+1u);
}

static int ftp_snapshot(struct ftp_result *out)
{
    invalidate(FTP_RES,sizeof(*FTP_RES));
    z72_u32 generation=ftp_load(&ftp_generation);
    if (!generation || generation!=FTP_RES->generation) return 0;
    out->stage=FTP_RES->stage;
    out->sent=FTP_RES->sent;
    out->reply=FTP_RES->reply;
    out->error=FTP_RES->error;
    out->done=FTP_RES->done;
    out->send_buffer=FTP_RES->send_buffer;
    return generation==FTP_RES->generation;
}
#endif
