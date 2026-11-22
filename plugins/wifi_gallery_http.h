/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_GALLERY_HTTP_H
#define Z72_WIFI_GALLERY_HTTP_H

#include "../sdk/wifi_gallery.h"

#include "tcp_send_ring.h"
#define HEADER_LIMIT 4096u
#define CHUNKED_LENGTH (~0u)

_Alignas(64) struct gallery_control gallery_control;
_Alignas(64) struct gallery_request gallery_request;
_Alignas(64) struct gallery_response gallery_response;
_Alignas(64) struct gallery_state gallery_state;
_Alignas(64) unsigned char gallery_data[GALLERY_SLOTS][GALLERY_SLOT_BYTES];
_Alignas(64) static unsigned char gallery_stack[16384];
static char gallery_http_request[HEADER_LIMIT+1u];
static int gallery_worker_started;
static struct tcp_send_ring gallery_ring;
static struct { u32 token; unsigned char result; } gallery_direct_results[8];
static unsigned gallery_direct_next;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
static const unsigned char gallery_html[]={
#embed "wifi_gallery.html"
};
#pragma clang diagnostic pop

static u32 gallery_tick(void) { return *(const volatile u32 *)0xb1201de0u; }
static void gallery_delay(u32 ms) { CALL(0xb10a5f68u,u32,u32)(ms); }

static int gallery_ap_active(void)
{
    u32 state=*(const volatile u32 *)0xb12007f0u;
    return state==2 || state==3;
}

static int gallery_serving(u32 epoch)
{
    return (epoch&1u) && gallery_control.epoch==epoch && gallery_ap_active();
}

static void gallery_close_socket(int fd)
{
    CALL(0xb10937d9u,int,int)(fd);
}

static int gallery_socket_option(int fd,u32 option,const void *value,u32 bytes)
{
    return CALL(0xb1093d35u,int,int,u32,u32,const void *,u32)(fd,0xffffu,option,value,bytes);
}

static int gallery_write_all(int fd,const void *data,u32 length,u32 epoch)
{
    const unsigned char *p=data;
    while (length && gallery_serving(epoch)) {
        int sent=CALL(0xb1093495u,int,int,const void *,u32,u32)(fd,p,length,0);
        if (sent<=0 || (u32)sent>length) return 0;
        p+=sent;
        length-=(u32)sent;
    }
    return !length;
}

static char *gallery_append(char *out,const char *text)
{
    while (*text) *out++=*text++;
    return out;
}

static char *gallery_decimal(char *out,u32 value)
{
    static const u32 places[]={1000000000u,100000000u,10000000u,1000000u,100000u,10000u,1000u,100u,10u,1u};
    int started=0;
    for (unsigned i=0;i<sizeof(places)/sizeof(places[0]);++i) {
        unsigned digit=0;
        while (value>=places[i]) { value-=places[i]; ++digit; }
        if (digit || started || places[i]==1u) {
            *out++=(char)('0'+digit);
            started=1;
        }
    }
    return out;
}

static int gallery_reply(int fd,u32 status,const char *type,u32 length,const char *name,u32 epoch)
{
    const char *reason=status==200 ? "200 OK" : status==404 ? "404 Not Found" :
        status==503 ? "503 Service Unavailable" : status==431 ? "431 Request Header Fields Too Large" :
        status==405 ? "405 Method Not Allowed" : status==413 ? "413 Content Too Large" : "400 Bad Request";
    char header[448],*out=gallery_append(header,"HTTP/1.1 ");
    out=gallery_append(out,reason);
    out=gallery_append(out,"\r\nConnection: close\r\nCache-Control: no-store\r\n"
        "Cross-Origin-Resource-Policy: same-origin\r\nX-Content-Type-Options: nosniff\r\n");
    if (type) {
        out=gallery_append(out,"Content-Type: ");
        out=gallery_append(out,type);
        out=gallery_append(out,"\r\n");
    }
    if (name) {
        out=gallery_append(out,"Content-Disposition: attachment; filename=\"");
        out=gallery_append(out,name);
        out=gallery_append(out,"\"\r\n");
    }
    if (status==405u) out=gallery_append(out,"Allow: GET, HEAD\r\n");
    if (length==CHUNKED_LENGTH) out=gallery_append(out,"Transfer-Encoding: chunked");
    else {
        out=gallery_append(out,"Content-Length: ");
        out=gallery_decimal(out,length);
    }
    out=gallery_append(out,"\r\n\r\n");
    return gallery_write_all(fd,header,(u32)(out-header),epoch);
}

static int gallery_read_header(int fd,u32 epoch)
{
    u32 used=0,start=gallery_tick();
    while (used<HEADER_LIMIT && gallery_serving(epoch) && gallery_tick()-start<6000u) {
        int received=CALL(0xb109366bu,int,int,void *,u32,u32)(
            fd,gallery_http_request+used,HEADER_LIMIT-used,0);
        if (received<=0 || (u32)received>HEADER_LIMIT-used) return 0;
        u32 end=used+(u32)received;
        for (u32 i=used>3u ? used-3u : 0;i+3u<end;++i) {
            if (gallery_http_request[i]=='\r' && gallery_http_request[i+1]=='\n' &&
                gallery_http_request[i+2]=='\r' && gallery_http_request[i+3]=='\n') {
                gallery_http_request[i+4]=0;
                return 200;
            }
        }
        used=end;
    }
    return used==HEADER_LIMIT ? 431 : 0;
}

static int gallery_equal(const char *left,const char *right)
{
    while (*left && *left==*right) { ++left; ++right; }
    return *left==*right;
}

static const char *gallery_prefix(const char *text,const char *match)
{
    while (*match) if (*text++!=*match++) return 0;
    return text;
}

static char *gallery_end_line(char *p)
{
    while (*p && *p!='\r' && *p!='\n') ++p;
    if (*p!='\r' || p[1]!='\n') return 0;
    *p=0;
    return p+2;
}

static int gallery_valid_host(const char *p)
{
    while (*p==' ' || *p=='\t') ++p;
    for (unsigned i=0;i<4u;++i) {
        u32 value=0,digits=0;
        while (*p>='0' && *p<='9' && digits<3u) {
            value=value*10u+(u32)(*p++-'0');
            ++digits;
        }
        if (!digits || value>255u) return 0;
        if (i<3u && *p++!='.') return 0;
    }
    if (*p==':') {
        p=gallery_prefix(p+1,GALLERY_PORT_TEXT);
        if (!p) return 0;
    } else if (GALLERY_PORT!=80u) {
        return 0;
    }
    while (*p==' ' || *p=='\t') ++p;
    return !*p;
}

static const char *gallery_photo_hex(const char *text,u32 *id)
{
    u32 value=0;
    for (unsigned i=0;i<8u;++i) {
        unsigned c=(unsigned char)*text++,digit;
        if (c>='0' && c<='9') digit=c-'0';
        else if ((c|32u)>='a' && (c|32u)<='f') digit=(c|32u)-'a'+10u;
        else return 0;
        value=(value<<4)|digit;
    }
    *id=value;
    return value ? text : 0;
}

static u32 gallery_zip_parse(const char *text);

static u32 gallery_date_pair(const char *text)
{
    u32 tens=(u32)(text[0]-'0');
    if (tens>9u) return 100u;
    u32 ones=(u32)(text[1]-'0');
    return ones>9u ? 100u : tens*10u+ones;
}

static const char *gallery_date(const char *text,u32 *packed)
{
    if (*text=='0' && (!text[1] || text[1]=='/')) {
        *packed=0;
        return text+1;
    }
    if (!(text=gallery_prefix(text,"20"))) return 0;
    u32 year=gallery_date_pair(text);
    if (year>99u || text[2]!='-') return 0;
    u32 month=gallery_date_pair(text+3);
    if (!month || month>12u || text[5]!='-') return 0;
    u32 day=gallery_date_pair(text+6);
    if (!day || day>31u) return 0;
    *packed=((20u+year)<<9)|(month<<5)|day;
    return text+8;
}

static u32 gallery_route_for(const char *path,u32 ids[GALLERY_THUMB_BATCH],u32 *count)
{
    if (gallery_equal(path,"/")) return GALLERY_HOME;
    if (gallery_equal(path,"/photos")) return GALLERY_LIST;
    const char *filter=gallery_prefix(path,"/photos/filter/");
    if (filter) {
        u32 rating=(u32)(*filter++-'0');
        if (rating>6u) return GALLERY_MISSING;
        u32 protection=(u32)(*filter++-'0');
        if (protection>2u) return GALLERY_MISSING;
        ids[1]=(rating<<GALLERY_RATING_SHIFT)|(protection<<GALLERY_PROTECTION_SHIFT);
        const char *dates=gallery_prefix(filter,"/dates/");
        if (dates) {
            u32 start,end;
            dates=gallery_date(dates,&start);
            if (!dates || *dates++!='/') return GALLERY_MISSING;
            filter=gallery_date(dates,&end);
            if (!filter || (end && start>end)) return GALLERY_MISSING;
            ids[2]=start|(end<<16);
        }
        if (!*filter) return GALLERY_LIST;
        if (*filter++!='/') return GALLERY_MISSING;
        const char *end=gallery_photo_hex(filter,ids);
        return end && !*end ? GALLERY_LIST : GALLERY_MISSING;
    }
    const char *zip=gallery_prefix(path,"/zip/");
    if (zip) {
        *count=gallery_zip_parse(zip);
        return *count ? GALLERY_ZIP : GALLERY_MISSING;
    }
    const char *direct=gallery_prefix(path,"/direct/");
    if (direct) {
        const char *end=gallery_photo_hex(direct,&ids[1]);
        if (!end || *end!='/') return GALLERY_MISSING;
        end=gallery_photo_hex(end+1,&ids[0]);
        if (!end || (*end!='j' && *end!='n') || end[1]) return GALLERY_MISSING;
        ids[2]=*end=='j' ? GALLERY_JPG : GALLERY_NEF;
        return GALLERY_DIRECT;
    }
    const char *status=gallery_prefix(path,"/direct-status/");
    if (status) {
        const char *end=gallery_photo_hex(status,&ids[0]);
        return end && !*end ? GALLERY_DIRECT_STATUS : GALLERY_MISSING;
    }
    const char *batch=gallery_prefix(path,"/thumbs/");
    if (batch) {
        for (u32 i=0;i<GALLERY_THUMB_BATCH;++i) {
            batch=gallery_photo_hex(batch,&ids[i]);
            if (!batch) break;
            if (!*batch) {
                *count=i+1u;
                return GALLERY_THUMBS;
            }
            if (*batch++!=',') break;
        }
        return GALLERY_MISSING;
    }
    static const char *const routes[]={"/photos/","/thumb/","/preview/","/jpg/","/nef/","/view-jpg/"};
    for (u32 i=0;i<sizeof(routes)/sizeof(routes[0]);++i) {
        const char *hex=gallery_prefix(path,routes[i]);
        if (!hex) continue;
        const char *end=gallery_photo_hex(hex,ids);
        return end && !*end ? GALLERY_LIST+i : GALLERY_MISSING;
    }
    return GALLERY_MISSING;
}

static unsigned gallery_parse_header(u32 *route,u32 ids[GALLERY_THUMB_BATCH],u32 *count,int *head)
{
    char *headers=gallery_end_line(gallery_http_request),*path=gallery_http_request;
    if (!headers) return 400;
    if (gallery_prefix(path,"GET ")) path+=4;
    else if (gallery_prefix(path,"HEAD ")) { path+=5; *head=1; }
    else return 405;
    char *version=path;
    while (*version && *version!=' ') ++version;
    if (!*version) return 400;
    *version++=0;
    if (!gallery_equal(version,"HTTP/1.1") && !gallery_equal(version,"HTTP/1.0")) return 400;
    int host=0;
    while (*headers!='\r') {
        char *line=headers;
        headers=gallery_end_line(headers);
        if (!headers) return 400;
        if ((line[0]|32)=='h' && (line[1]|32)=='o' && (line[2]|32)=='s' &&
            (line[3]|32)=='t' && line[4]==':') {
            if (host || !gallery_valid_host(line+5)) return 400;
            host=1;
        }
    }
    if (!host) return 400;
    *route=gallery_route_for(path,ids,count);
    return *route==GALLERY_MISSING ? 404 : 200;
}

static int gallery_write_chunk(int fd,const void *data,u32 length,u32 epoch)
{
    char header[10],*start=header+8;
    header[8]='\r';
    header[9]='\n';
    for (u32 size=length;size;size>>=4) *--start="0123456789abcdef"[size&15u];
    return gallery_write_all(fd,start,(u32)(header+sizeof(header)-start),epoch) &&
        gallery_write_all(fd,data,length,epoch) && gallery_write_all(fd,"\r\n",2,epoch);
}

static u32 gallery_crc_update(u32 crc,const unsigned char *data,u32 length);

static int gallery_body(int fd,u32 length,u32 epoch,int chunked,u32 *crc)
{
    u32 sent=0,consumed=0,start=gallery_tick();
    while (sent<length && gallery_serving(epoch)) {
        if (gallery_response.failed || gallery_tick()-start>=6000u) break;
        if (gallery_response.produced==consumed) { gallery_delay(2); continue; }
        u32 size=length-sent;
        if (size>GALLERY_CHUNK) size=GALLERY_CHUNK;
        const void *data=gallery_data[consumed%GALLERY_SLOTS];
        if (crc) *crc=gallery_crc_update(*crc,data,size);
        int wrote=chunked ? gallery_write_chunk(fd,data,size,epoch) : gallery_write_all(fd,data,size,epoch);
        if (!wrote) break;
        sent+=size;
        gallery_request.consumed=++consumed;
        start=gallery_tick();
    }
    return sent==length;
}

static void gallery_reclaim_download_ring(void)
{
    if (!gallery_ring.socket) return;
    tcp_reclaim_send_ring(&gallery_ring);
    if (!gallery_ring.socket) gallery_state.large_in_use=0;
}

static u32 gallery_prepare_download_ring(int fd,u32 address,u32 epoch)
{
    if (!address) return GALLERY_BUFFER_NATIVE;
    u32 start=gallery_tick();
    while (gallery_ring.socket && gallery_serving(epoch)) {
        gallery_reclaim_download_ring();
        if (!gallery_ring.socket || gallery_tick()-start>=8000u) break;
        gallery_delay(50);
    }
    if (gallery_ring.socket) return GALLERY_BUFFER_NATIVE;
    start=gallery_tick();
    while (gallery_serving(epoch)) {
        if (tcp_attach_send_ring(&gallery_ring,fd,address)) {
            gallery_state.large_in_use=1;
            return GALLERY_BUFFER_LARGE;
        }
        if (gallery_tick()-start>=500u) break;
        gallery_delay(10);
    }
    return GALLERY_BUFFER_NATIVE;
}

static u32 gallery_request_media(u32 route,u32 id,u32 flags,u32 dates,u32 epoch)
{
    u32 start=gallery_tick();
    while (gallery_response.released!=gallery_request.serial && gallery_serving(epoch) &&
        gallery_tick()-start<6000u) gallery_delay(5);
    if (!gallery_serving(epoch) || gallery_response.released!=gallery_request.serial) return 503;
    u32 serial=gallery_request.serial+1u;
    if (!serial) ++serial;
    gallery_request.epoch=epoch;
    gallery_request.route=route;
    gallery_request.value=id;
    gallery_request.flags=flags;
    gallery_request.dates=dates;
    gallery_request.consumed=0;
    gallery_request.cancelled=0;
    __asm__ volatile("" ::: "memory");
    gallery_request.serial=serial;
    start=gallery_tick();
    while (gallery_response.serial!=serial && gallery_serving(epoch) && gallery_tick()-start<6000u) gallery_delay(5);
    return gallery_response.serial==serial ? gallery_response.status : 503u;
}

static u32 gallery_media_request(int fd,u32 route,u32 id,u32 flags,u32 dates,u32 epoch)
{
    int head=(flags&GALLERY_META)!=0;
    u32 status=gallery_request_media(route,id,flags,dates,epoch);
    if (status==200u) {
        int download=route==GALLERY_JPG || route==GALLERY_NEF;
        const char *type=route==GALLERY_LIST ? "application/json" :
            route==GALLERY_NEF ? "application/octet-stream" : "image/jpeg";
        u32 length=gallery_response.length;
        const char *name=download ? gallery_response.name : 0;
        if (download && !head)
            gallery_state.download_buffer=gallery_prepare_download_ring(fd,gallery_response.send_address,epoch);
        if (!gallery_reply(fd,status,type,length,name,epoch) ||
            (!head && !gallery_body(fd,length,epoch,0,0))) status=503;
    } else gallery_reply(fd,status,0,0,0,epoch);
    gallery_request.cancelled=gallery_request.serial;
    return status;
}

static void gallery_direct_result(u32 token,unsigned char result)
{
    const unsigned count=sizeof(gallery_direct_results)/sizeof(gallery_direct_results[0]);
    for (unsigned i=0;i<count;++i) {
        if (gallery_direct_results[i].token==token) {
            gallery_direct_results[i].result=result;
            return;
        }
    }
    gallery_direct_results[gallery_direct_next].token=token;
    gallery_direct_results[gallery_direct_next].result=result;
    gallery_direct_next=(gallery_direct_next+1u)%count;
}

static void gallery_direct_status(int fd,u32 token,int head,u32 epoch)
{
    unsigned char result='0';
    for (unsigned i=0;i<sizeof(gallery_direct_results)/sizeof(gallery_direct_results[0]);++i)
        if (gallery_direct_results[i].token==token) {
            result=gallery_direct_results[i].result;
            break;
        }
    if (gallery_reply(fd,200,"text/plain",1,0,epoch) && !head)
        gallery_write_all(fd,&result,1,epoch);
}

static void gallery_thumbnail_batch(int fd,const u32 *ids,u32 count,int head,u32 epoch)
{
    if (!gallery_reply(fd,200,"application/octet-stream",CHUNKED_LENGTH,0,epoch) || head) return;
    for (u32 i=0;i<count && gallery_serving(epoch);++i) {
        u32 status=gallery_request_media(GALLERY_THUMB,ids[i],0,0,epoch);
        /* Each thumbnail has a little-endian length followed by its JPEG.
         * Zero marks an unavailable preview and all ones asks for a retry. */
        u32 length=status==200 ? gallery_response.length : status==404 ? 0 : ~0u;
        unsigned char record[4]={(u8)length,(u8)(length>>8),(u8)(length>>16),(u8)(length>>24)};
        int sent=gallery_write_chunk(fd,record,sizeof(record),epoch);
        if (sent && status==200) sent=gallery_body(fd,length,epoch,1,0);
        gallery_request.cancelled=gallery_request.serial;
        if (!sent) return;
    }
    gallery_write_all(fd,"0\r\n\r\n",5,epoch);
}

#include "wifi_gallery_zip.h"

static void gallery_serve(int fd,u32 epoch)
{
    const u32 send[4]={20,0,0,0},receive[4]={3,0,0,0};
    if (gallery_socket_option(fd,0x1005u,send,sizeof(send)) ||
        gallery_socket_option(fd,0x1006u,receive,sizeof(receive))) return;
    u32 status=(u32)gallery_read_header(fd,epoch),route=GALLERY_MISSING,count=0;
    u32 ids[GALLERY_THUMB_BATCH];
    ids[0]=0;
    ids[1]=0;
    ids[2]=0;
    int head=0;
    if (!status) return;
    if (status==200u) status=gallery_parse_header(&route,ids,&count,&head);
    u32 flags=head ? GALLERY_META : 0;
    if (status!=200u) gallery_reply(fd,status,0,0,0,epoch);
    else if (route==GALLERY_HOME) {
        if (gallery_reply(fd,200,"text/html; charset=utf-8",sizeof(gallery_html),0,epoch) && !head)
            gallery_write_all(fd,gallery_html,sizeof(gallery_html),epoch);
    } else if (route==GALLERY_THUMBS) gallery_thumbnail_batch(fd,ids,count,head,epoch);
    else if (route==GALLERY_ZIP) gallery_zip_download(fd,count,head,epoch);
    else if (route==GALLERY_DIRECT) {
        u32 sent=gallery_media_request(fd,ids[2],ids[0],flags,0,epoch);
        if (!head) gallery_direct_result(ids[1],sent==200u ? '1' : '2');
    } else if (route==GALLERY_DIRECT_STATUS) gallery_direct_status(fd,ids[0],head,epoch);
    else if (route==GALLERY_LIST) gallery_media_request(fd,route,ids[0],flags|ids[1],ids[2],epoch);
    else gallery_media_request(fd,route,ids[0],flags,0,epoch);
}

static int gallery_open_listener(void)
{
    int fd=CALL(0xb10930cfu,int,int,int,int)(2,1,0);
    if (fd<1) return -1;
    const unsigned char address[16]={16,2,GALLERY_PORT>>8,GALLERY_PORT&255,0,0,0,0};
    if (fd>32 || CALL(0xb10931c7u,int,int,const void *,u32)(fd,address,sizeof(address)) ||
        CALL(0xb1093245u,int,int,int)(fd,6)) {
        gallery_close_socket(fd);
        return -1;
    }
    return fd;
}

static void gallery_listen_to_browser(int listener,u32 epoch)
{
    while (gallery_serving(epoch)) {
        gallery_reclaim_download_ring();
        gallery_state.stage=GALLERY_LISTEN;
        u32 mask=1u<<((u32)listener-1u),ready=mask;
        const u32 wait[4]={1,0,0,0};
        int result=CALL(0xb1094c6fu,int,int,u32 *,void *,void *,const u32 *)(
            listener,&ready,0,0,wait);
        if (result<0) break;
        if (!gallery_serving(epoch) || !(result && (ready&mask))) continue;
        const volatile u32 *entry=(const volatile u32 *)(TCP_DESCRIPTOR_TABLE+(u32)listener*12u);
        if (entry[0]!=1u || entry[1]!=TCP_PROTOCOL || !entry[2] ||
            *(const volatile unsigned char *)(entry[2]+0xe4u)!=2u) break;
        int client=CALL(0xb109328fu,int,int,void *,void *)(listener,0,0);
        if (client<1) break;
        gallery_state.stage=GALLERY_SERVE;
        gallery_serve(client,epoch);
        gallery_close_socket(client);
    }
}

static void gallery_task(u32 argument)
{
    (void)argument;
    for (;;) {
        gallery_reclaim_download_ring();
        gallery_state.network=(u32)gallery_ap_active();
        u32 epoch=gallery_control.epoch;
        if (!gallery_serving(epoch)) {
            gallery_state.stage=epoch&1u ? GALLERY_WAIT_AP : GALLERY_OFF;
            gallery_delay(250);
            continue;
        }
        int listener=gallery_open_listener();
        if (listener>=1) {
            gallery_state.download_buffer=GALLERY_BUFFER_UNUSED;
            gallery_listen_to_browser(listener,epoch);
            gallery_close_socket(listener);
        }
        if (gallery_serving(epoch)) {
            gallery_state.stage=GALLERY_SOCKET_ERROR;
            gallery_delay(500);
        }
    }
}

static void gallery_aux_start(void)
{
    if (!gallery_worker_started) {
        struct {
            unsigned short attributes,pad0;
            u32 argument,entry;
            unsigned short priority,pad1;
            u32 stack_bytes,stack;
        } task={2,0,0,(u32)gallery_task,10,0,sizeof(gallery_stack),(u32)gallery_stack};
        int id=CALL(0xb10a6d40u,int,const void *)(&task);
        if (id>0) gallery_worker_started=1;
        else gallery_state.stage=GALLERY_TASK_ERROR;
    }
}

#endif
