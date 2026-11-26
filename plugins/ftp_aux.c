/* SPDX-License-Identifier: MIT */
#define wifi_aux_ap ftp_native_ap
#define wifi_aux_sta ftp_native_sta
#include "wifi_aux.c"
#undef wifi_aux_ap
#undef wifi_aux_sta
#include "../sdk/ftp_client.h"

#define CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
_Alignas(64) struct ftp_request ftp_request;
_Alignas(64) struct ftp_result ftp_result;
_Alignas(64) unsigned char ftp_payload[FTP_SLOTS][FTP_SLOT_BYTES];
_Alignas(64) static unsigned char ftp_stack[16384];
static int started;


static void sleep_ms(u32 ms)
{
    CALL(0xb10a5f68u,u32,u32)(ms);
}

static int close_socket(int fd)
{
    return CALL(0xb10937d9u,int,int)(fd);
}

static int new_socket(void)
{
    return CALL(0xb10930cfu,int,int,int,int)(2,1,0);
}

#include "ftp_send_ring.h"
#include "wifi_gallery_http.h"

static u32 send_buffer(int fd)
{
    u32 size=0,length=sizeof(size);
    if (CALL(0xb1093947u,int,int,u32,u32,u32 *,u32 *)(fd,0xffffu,0x1001u,&size,&length) ||
        length!=sizeof(size)) return 0;
    return size;
}


static int prepare_data_socket(int fd)
{
    u32 before=send_buffer(fd);
    ftp_result.send_buffer=before;
    if (!before || before>=65536u || ftp_request.total<=65536u) return fd;

    /* Native allocator needs a new ring before freeing the old one.
     * Leave at least 32 KiB in its pool after this allocation. */
    u32 pool=*(volatile unsigned short *)0xb1200c28u, available[3];
    if (CALL(0xb10a43ccu,u32,u32,u32 *)(pool,available)) return fd;
    u32 wanted=65536u;
    if (available[1]<wanted+64u+32768u || available[2]<wanted+64u) wanted=32768u;
    if (wanted<=before || available[1]<wanted+64u+32768u || available[2]<wanted+64u)
        return fd;

    if (!CALL(0xb1093d35u,int,int,u32,u32,const void *,u32)(
        fd,0xffffu,0x1001u,&wanted,sizeof(wanted)) && send_buffer(fd)==wanted) {
        ftp_result.send_buffer=wanted;
        return fd;
    }

    /* A failed native resize can leave a capacity/ring mismatch. This socket
     * hasn't connected or sent anything. Discard it instead of using it. */
    ftp_result.send_buffer=0;
    if (close_socket(fd)) return -1;
    fd=new_socket();
    if (fd>=1) ftp_result.send_buffer=send_buffer(fd);
    return fd;
}

static int open_socket(u32 port,int data)
{
    _Alignas(4) const unsigned char address[16]={16,2,port>>8,port&255,
        ftp_request.ip[0],ftp_request.ip[1],ftp_request.ip[2],ftp_request.ip[3]};
    const u32 timeout[4]={8,0,0,0};
    int fd=new_socket();
    if (fd>=1 && data
        && (!ftp_request.large_send_address || ftp_ring.socket)
        ) fd=prepare_data_socket(fd);
    if (fd<1) {
        ftp_result.error=CALL(0xb1084aedu,int,void)();
        return -1;
    }
    int (*option)(int,u32,u32,const void *,u32)=(void *)0xb1093d35u;
    /* On this firmware TCP level/option 6/2 takes a timeval and sets the
     * connect wait. The two SOL_SOCKET options control send and receive. */
    if (option(fd,0xffffu,0x1005u,timeout,sizeof(timeout)) ||
        option(fd,0xffffu,0x1006u,timeout,sizeof(timeout)) ||
        option(fd,6,2,timeout,sizeof(timeout)) ||
        CALL(0xb10933cdu,int,int,const void *,u32)(fd,address,sizeof(address))) {
        ftp_result.error=CALL(0xb1084aedu,int,void)();
        close_socket(fd);
        return -1;
    }
    return fd;
}

static int send_bytes(int fd,const void *buffer,u32 size)
{
    const unsigned char *p=buffer;
    while (size && ftp_request.state==FTP_READY) {
        int n=CALL(0xb1093495u,int,int,const void *,u32,u32)(fd,p,size,0);
        if (n<=0 || (u32)n>size) {
            ftp_result.error=CALL(0xb1084aedu,int,void)();
            return 0;
        }
        p+=n; size-=(u32)n;
    }
    return size==0;
}

struct ftp_control {
    int fd;
    u32 pos, end;
    char input[256], line[256];
};

static int read_line(struct ftp_control *c)
{
    for (u32 i=0;i<4;++i) c->line[i]=0;
    for (u32 i=0;i<sizeof(c->line)-1 && ftp_request.state==FTP_READY;++i) {
        if (c->pos==c->end) {
            int n=CALL(0xb109366bu,int,int,void *,u32,u32)(
                c->fd,c->input,sizeof(c->input),0);
            if (n<=0 || (u32)n>sizeof(c->input)) {
                ftp_result.error=CALL(0xb1084aedu,int,void)();
                return 0;
            }
            c->pos=0; c->end=(u32)n;
        }
        char ch=c->input[c->pos++];
        if (!ch) return 0;
        c->line[i]=ch;
        if (ch=='\n') {
            if (!i || c->line[i-1]!='\r') return 0;
            c->line[i-1]=0;
            return 1;
        }
    }
    return 0;
}

static int reply(struct ftp_control *c)
{
    ftp_result.reply=0;
    if (!read_line(c)) return 0;
    char first[3];
    for (u32 i=0;i<3;++i) {
        first[i]=c->line[i];
        if (first[i]<'0' || first[i]>'9') return 0;
    }
    int code=(first[0]-'0')*100+(first[1]-'0')*10+first[2]-'0';
    if (c->line[3]=='-') {
        for (u32 i=0;i<24;++i) {
            if (!read_line(c)) return 0;
            if (c->line[0]==first[0] && c->line[1]==first[1] &&
                c->line[2]==first[2] && c->line[3]==' ') {
                ftp_result.reply=(u32)code;
                return code;
            }
        }
        return 0;
    }
    if (c->line[3]!=' ') return 0;
    ftp_result.reply=(u32)code;
    return code;
}

static int command(struct ftp_control *c,const char *verb,const char *arg)
{
    char line[72];
    u32 n=0;
    while (*verb) line[n++]=*verb++;
    if (arg) {
        line[n++]=' ';
        while (*arg && n<sizeof(line)-2) line[n++]=*arg++;
        if (*arg) return 0;
    }
    line[n++]='\r'; line[n++]='\n';
    ftp_result.reply=0;
    return send_bytes(c->fd,line,n) ? reply(c) : 0;
}

static u32 number(const char **line,u32 limit)
{
    const char *p=*line;
    if (*p<'0' || *p>'9') return limit+1;
    u32 n=0;
    do {
        n=n*10u+(u32)(*p++-'0');
        if (n>limit) return limit+1;
    } while (*p>='0' && *p<='9');
    *line=p;
    return n;
}

static u32 passive_port(struct ftp_control *c)
{
    int code=command(c,"EPSV",0);
    if (code==500 || code==501 || code==502 || code==504 || code==522)
        code=command(c,"PASV",0);
    if (code!=229 && code!=227) return 0;
    const char *p=c->line;
    while (*p && *p!='(') ++p;
    if (!*p++) return 0;
    if (code==229) {
        char delim=*p++;
        if (delim<33 || delim>126 || *p++!=delim || *p++!=delim) return 0;
        u32 port=number(&p,65535);
        return port<=65535 && *p++==delim && *p==')' ? port : 0;
    }
    u32 high=0,low=0;
    for (u32 i=0;i<6;++i) {
        u32 field=number(&p,255);
        if (field>255 || *p++!=(i==5 ? ')' : ',')) return 0;
        high=low; low=field;
    }
    /* Ignore the advertised IPv4 address. Use the configured control peer. */
    return high*256u+low;
}

static const char *upload_name(struct ftp_control *c,char unique[64])
{
    const char *name=ftp_request.remote;
    if (ftp_request.overwrite) return name;
    int code=command(c,"SIZE",name);
    if (code==550) return name;
    if (code!=213) return 0;

    const char *suffix=name;
    u32 length=0;
    for (const char *p=name;*p;++p) { if (*p=='.') suffix=p; ++length; }
    if (suffix==name) suffix=name+length;
    u32 stem=(u32)(suffix-name), extension=length-stem;
    if (extension>=sizeof(ftp_request.remote)-5) return 0;
    if (stem+extension+5u>sizeof(ftp_request.remote)) stem=sizeof(ftp_request.remote)-extension-5u;
    for (u32 i=0;i<stem;++i) unique[i]=name[i];
    unique[stem]='_';
    for (u32 i=0;i<=extension;++i) unique[stem+4u+i]=suffix[i];
    for (u32 attempt=1;attempt<=999;++attempt) {
        if (ftp_request.state!=FTP_READY) return 0;
        unique[stem+1]=(char)('0'+attempt/100u);
        unique[stem+2]=(char)('0'+attempt/10u%10u);
        unique[stem+3]=(char)('0'+attempt%10u);
        code=command(c,"SIZE",unique);
        if (code==550) return unique;
        if (code!=213) break;
    }
    return 0;
}

static int upload(struct ftp_control *c,int *data)
{
    ftp_result.stage=FTP_GREETING;
    int code=reply(c);
    if (code==120) code=reply(c);
    if (code!=220) return 0;
    ftp_result.stage=FTP_LOGIN;
    code=command(c,"USER",ftp_request.user);
    if (code==331) code=command(c,"PASS",ftp_request.password);
    if (code!=230) return 0;
    ftp_result.stage=FTP_TYPE;
    if (command(c,"TYPE","I")!=200) return 0;
    const char *remote=ftp_request.remote;
    if (ftp_request.folder[0] && command(c,"CWD",ftp_request.folder)!=250) return 0;
    char unique[64];
    remote=upload_name(c,unique);
    if (!remote) return 0;
    ftp_result.stage=FTP_PASSIVE;
    u32 port=passive_port(c);
    if (!port) return 0;
    ftp_result.stage=FTP_DATA_CONNECT;
    *data=open_socket(port,1);
    if (*data<1) return 0;
    if (ftp_request.large_send_address && !ftp_ring.socket) {
        ftp_result.stage=FTP_WAIT_BUFFER;
        u32 attached=0;
        for (u32 wait=0;;wait+=10u) {
            if (ftp_request.state!=FTP_READY) return 0;
            attached=ftp_attach_send_ring(*data);
            if (attached || wait==500u) break;
            sleep_ms(10);
        }
        /* A connected socket whose ring could not be replaced is still usable. */
        ftp_result.send_buffer=attached ? FTP_LARGE_SEND_BYTES : send_buffer(*data);
    }
    ftp_result.stage=FTP_STORE;
    code=command(c,"STOR",remote);
    if (code!=125 && code!=150) return 0;
    for (u32 seq=1;ftp_result.sent<ftp_request.total;++seq) {
        ftp_result.stage=FTP_READ_CARD;
        for (u32 wait=0;ftp_request.sequence<seq;wait+=FTP_POLL_MS) {
            if (ftp_request.state!=FTP_READY || wait>=10000u) return 0;
            sleep_ms(FTP_POLL_MS);
        }
        u32 size=ftp_request.total-ftp_result.sent;
        if (size>FTP_CHUNK) size=FTP_CHUNK;
        ftp_result.stage=FTP_SEND;
        int sent=send_bytes(*data,ftp_payload[(seq-1u)%FTP_SLOTS],size);
        if (!sent) return 0;
        ftp_result.sent+=size;
        /* Release the slot only after the last short send has returned. */
        ftp_result.consumed=seq;
    }
    ftp_result.stage=FTP_FINISH;
    int closed=close_socket(*data);
    *data=-1;
    if (closed) return 0;
    ftp_result.reply=0;
    code=reply(c);
    return code==226 || code==250;
}

static void transfer(void)
{
    struct ftp_control control;
    control.fd=-1; control.pos=0; control.end=0;
    int data=-1;
    ftp_result.stage=FTP_CONNECT;
    for (u32 attempt=0;attempt<6 && ftp_request.state==FTP_READY;++attempt) {
        ftp_result.error=0;
        control.fd=open_socket(ftp_request.port,0);
        if (control.fd>=1) break;
        if (attempt!=5) sleep_ms(2500);
    }
    if (control.fd>=1 && upload(&control,&data)) {
        ftp_result.stage=FTP_DONE;
        /* Closing the control connection ends the FTP session. */
    }
    if (data>=1) close_socket(data);
    if (control.fd>=1) close_socket(control.fd);
    ftp_reclaim_send_ring();
    ftp_result.done=1;
}

#define AP_STATE (*(volatile u32 *)0xb12007f0u)
#define STA_CONTEXT (*(volatile u32 *)0xb1200820u)

static int ftp_ap_active(void)
{
    /* Native AP network operations accept both states (C 1.70 0x1211ee1a). */
    u32 state=AP_STATE;
    return state==2 || state==3;
}

static u32 ftp_network(void)
{
    /* Radio shutdown clears its mode before the STA context can be recycled. */
    u32 state=AP_STATE;
    if (!state) return 0;
    return ftp_ap_active() ? 1u : (state==6 || STA_CONTEXT) ? 2u : 0u;
}
static void ftp_wifi_changed(u32 previous)
{
    /* Native Wi-Fi command callbacks run on the same auxiliary task. */
    u32 mode=ftp_network();
    if (previous && mode!=previous) ++ftp_result.wifi_stop;
    ftp_result.network=mode;
}

__attribute__((used,noinline))
static void ftp_task(u32 argument)
{
    (void)argument;
    for (;;) {
        ftp_reclaim_send_ring();
        ftp_result.network=ftp_network();
        u32 generation=ftp_request.generation;
        if (generation && generation!=ftp_result.generation && ftp_request.state!=FTP_IDLE) {
            ftp_result.done=0;
            ftp_result.consumed=0;
            ftp_result.sent=0;
            ftp_result.reply=0;
            ftp_result.error=0;
            ftp_result.send_buffer=0;
            ftp_result.stage=FTP_WAIT_AP;
            ftp_result.generation=generation;
            /* The main worker owns the deadline and publishes cancellation. */
            while (ftp_request.state==FTP_READY && !ftp_result.network) {
                sleep_ms(250);
                ftp_result.network=ftp_network();
            }
            if (ftp_request.large_send_address && ftp_ring.socket) {
                ftp_result.stage=FTP_WAIT_BUFFER;
                for (u32 wait=0;ftp_ring.socket && ftp_request.state==FTP_READY && wait<8000u;wait+=50u) {
                    ftp_reclaim_send_ring();
                    if (ftp_ring.socket) sleep_ms(50);
                }
            }
            if (ftp_request.state==FTP_READY) transfer();
            else ftp_result.done=1;
        }
        sleep_ms(ftp_result.network ? 250u : 500u);
    }
}

static void start_worker(void)
{
    if (started) return;
    started=1;
    ftp_result.stage=FTP_TASK;
    struct {
        unsigned short attributes,pad0;
        u32 argument,entry;
        unsigned short priority,pad1;
        u32 stack_bytes,stack;
    } task={2,0,0,(u32)ftp_task,10,0,sizeof(ftp_stack),(u32)ftp_stack};
    int id=CALL(0xb10a6d40u,int,const void *)(&task);
    if (id<=0) {
        ftp_result.error=id;
        ftp_result.done=1;
        started=0; /* Retry after the next native connection request. */
    }
}

__attribute__((used,noinline))
u32 wifi_aux_ap(u32 event,volatile u32 *request)
{
    u32 start=request[0]!=0xffffffffu;
    u32 result=ftp_native_ap(event,request);
    ftp_wifi_changed(0);
    if (start && ftp_ap_active()) {
        start_worker();
        gallery_aux_start();
    }
    return result;
}

__attribute__((used,noinline))
u32 wifi_aux_ftp_init(u32 event,volatile u32 *request)
{
    u32 initialize=request[0]!=0xffffffffu;
    u32 result=CALL(0xb107149du,u32,u32,volatile u32 *)(event,request);
    ftp_wifi_changed(0);
    if (initialize) {
        start_worker();
        gallery_aux_start();
    }
    return result;
}

__attribute__((used,noinline))
u32 wifi_aux_sta(u32 event,volatile u32 *request)
{
    u32 context=request[1];
    u32 start=request[0]!=0xffffffffu && context && !STA_CONTEXT;
    u32 result=ftp_native_sta(event,request);
    ftp_wifi_changed(0);
    /* STA command 14 stores this context before scheduling association. The
     * FTP connect attempts wait out association/DHCP using normal TCP errors. */
    if (start && STA_CONTEXT==context) {
        start_worker();
        gallery_aux_start();
    }
    return result;
}

__attribute__((used,noinline))
u32 wifi_aux_ftp_radio_stop(u32 event,volatile u32 *request)
{
    u32 previous=ftp_network();
    u32 result=CALL(0xb10715fbu,u32,u32,volatile u32 *)(event,request);
    ftp_wifi_changed(previous);
    return result;
}

__attribute__((used,noinline))
u32 wifi_aux_ftp_ap_stop(u32 event,volatile u32 *request)
{
    u32 previous=ftp_network();
    u32 result=CALL(0xb1071a7du,u32,u32,volatile u32 *)(event,request);
    ftp_wifi_changed(previous);
    return result;
}
