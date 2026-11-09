/* SPDX-License-Identifier: MIT */
#include "platform.h"

#define HEADER 64u
#define MAX_FILE 65536u
#define MAX_MEMORY 131072u
enum { NO_CARD=1, NO_FILE, BAD_SIZE, ALLOC_FAILED, READ_FAILED, CLOSE_FAILED,
       BAD_HEADER, BAD_CRC, BAD_RELOC, MODULE_FAILED, LOADED };

static u32 mode(void) { return *(const volatile u8 *)0xb1818d28; }
struct state { u32 task[8]; const u16 *grid; u8 *image; u32 memory_size; const u16 *pending_grid; };
#define ANCHOR ((volatile u32 *)0xb18aedb8)
static struct state *current(void)
{
    u32 p=ANCHOR[0];
    if(!p || (p^ANCHOR[1])!=~0u) return 0;
    return (struct state *)p;
}
static u32 grid_table(const u16 *table)
{
    struct state *s=current();
    u32 p=(u32)table;
    if(!s || !s->image || p%2 || p<(u32)s->image ||
       s->memory_size<96 || p-(u32)s->image>s->memory_size-96) return 0;
    for(u32 i=0;i<3;++i)
        if(!table[i*16+5] || table[i*16+5]>8 || !table[i*16+11] || table[i*16+11]>8) return 0;
    __asm__ volatile("dmb sy" ::: "memory");
    s->pending_grid=table;
    return 1;
}
const u16 *extension_grid_table(const u16 *original)
{
    struct state *s=current();
    return s && s->grid ? s->grid : original;
}
u32 extension_grid_enabled(u32 original)
{
    struct state *s=current();
    return s && s->grid ? 1 : original;
}
static const struct z72_api api = {Z72_ABI, sizeof(api), Z72_TARGET, mode, grid_table};

static void copy(u8 *dst, const u8 *src, u32 size)
{
    for (u32 i=0; i<size; ++i) dst[i] = src[i];
}

static u32 checksum(const u8 *data, u32 size)
{
    u32 crc = ~0u;
    for (u32 i=0; i<size; ++i) {
        crc ^= i >= 32 && i < 36 ? 0 : data[i];
        for (u32 bit=0; bit<8; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
    }
    return ~crc;
}

static void executable(u8 *image, u32 size)
{
    /* Clean modified data to PoU before publishing executable code. */
    for (u32 p=(u32)image; p<(u32)image+size; p+=32)
        __asm__ volatile("mcr p15, 0, %0, c7, c11, 1" :: "r"(p) : "memory");
    u32 zero=0;
    __asm__ volatile("dsb sy\n mcr p15, 0, %0, c7, c1, 0\n dsb sy\n isb sy"
                     :: "r"(zero) : "memory");
}

/* Stable observation points; they do not write firmware or emulator state. */
__attribute__((noinline)) u32 extension_call(u8 *image, u32 entry)
{
    return ((u32 (*)(const struct z72_api *))((u32)image + entry + 1))(&api);
}
__attribute__((noinline)) void extension_result(u32 status, void *image, u32 result)
{
    __asm__ volatile("" :: "r"(status), "r"(image), "r"(result) : "memory");
}

static u32 load(u8 **retained, u32 *module_result)
{
    u32 file[10];
    for(u32 i=0;i<10;++i) ((volatile u32 *)file)[i]=0;
    FN(0x104d3c01, void, u32 *, const char *)(file, "B:\\Z72APP.BIN");
    if (FN(0x104d3c05, u32, u32 *, u32, u32)(file, 0, 0) != FS_OK)
        return NO_FILE;
    u32 size=file[6], status=BAD_SIZE;
    u8 *raw=0, *body=0;
    if (!file[7] && size>=HEADER+2 && size<=MAX_FILE) {
        raw=allocate(size);
        if (!raw) status=ALLOC_FAILED;
        else {
            _Alignas(64) u8 buffer[576];
            status=0;
            for (u32 offset=0; offset<size;) {
                u32 count=size-offset, actual=0;
                if (count>512) count=512;
                u32 capacity=count+(offset+count==size);
                struct { u32 kind; u8 *buffer; u32 capacity; } request={1,buffer,capacity};
                invalidate(buffer, sizeof(buffer));
                u32 result=FN(0x104d3d83,u32,u32 *,void *,u32,u32 *)(file,&request,capacity,&actual);
                invalidate(buffer, sizeof(buffer));
                if (result!=FS_OK || actual!=count) { status=READ_FAILED; break; }
                copy(raw+offset,buffer,count);
                offset+=count;
            }
        }
    }
    if (FN(0x104d3c91,u32,u32 *)(file)!=FS_OK) status=CLOSE_FAILED;
    if (status) { if(raw) release(raw); return status; }

    u32 *h=(u32 *)raw, image=h[4], bss=h[5], entry=h[6], relocs=h[7];
    if (h[0]!=0x4d32375a || h[1]!=Z72_ABI || h[2]!=Z72_TARGET || h[3]!=size ||
        !image || image%64 || image>MAX_FILE-HEADER || bss>MAX_MEMORY-image ||
        entry%2 || entry>=image || relocs>(MAX_FILE-HEADER-image)/4 ||
        size!=HEADER+image+4*relocs) status=BAD_HEADER;
    for(u32 i=9;i<16;++i) if(h[i]) status=BAD_HEADER;
    if (!status && checksum(raw,size)!=h[8]) status=BAD_CRC;
    if (status) { release(raw); return status; }

    void *allocation=allocate(image+bss+63);
    if (!allocation) { release(raw); return ALLOC_FAILED; }
    body=(u8 *)(((u32)allocation+63)&~63u);
    copy(body,raw+HEADER,image);
    for(u32 i=0;i<bss;++i) body[image+i]=0;
    u32 *rel=(u32 *)(raw+HEADER+image), previous=0;
    for(u32 i=0;i<relocs;++i) {
        u32 off=rel[i];
        if(off%4 || off>image-4 || (i && off<=previous) || *(u32 *)(body+off)>=image+bss) {
            status=BAD_RELOC; break;
        }
        *(u32 *)(body+off)+=(u32)body;
        previous=off;
    }
    release(raw);
    if(status) { release(allocation); return status; }
    executable(body,image+bss);
    struct state *s=current();
    s->image=body;
    s->memory_size=image+bss;
    *module_result=extension_call(body,entry);
    if(*module_result) {
        s->pending_grid=0;
        s->image=0;
        release(allocation);
        return MODULE_FAILED;
    }
    __asm__ volatile("dmb sy" ::: "memory");
    s->grid=s->pending_grid;
    if(s->grid) FN(0x105b00fd,void,u32)(FN(0x105b0105,u32,void)());
    /* The module and its BSS remain valid until power-off. No hot unload. */
    *retained=body;
    return LOADED;
}

static void worker(void *task)
{
    (void)task;
    u32 status=NO_CARD, result=0;
    u8 *image=0;
    for(u32 attempt=0;attempt<100;++attempt) {
        const volatile u8 *app=(const volatile u8 *)0xb1811e40;
        if(app[0]==1 && app[16]==3 && app[24]==2) {
            void *slot=FN(0x10509395,void *,u32)(1);
            u32 ready=0,answered=0;
            if(slot && FN(0x10495c1d,u32,void *,u32,u32 *,u32 *)(slot,0,&ready,&answered)==FS_OK && ready) {
                status=load(&image,&result);
                if(status!=NO_FILE) break;
            }
        }
        delay(100);
    }
    extension_result(status,image,result);
    /* Native tk_ext_tsk makes this task dormant; its stack is kept until reboot. */
    for(;;) FN(0x101045f1,void,void)();
}

/* Called once by the original AUTOOP initialization, without changing inputs. */
void extension_boot(void)
{
    FN(0x104e0695,void,void)();
    if(current()) return;
    struct state *s=allocate(sizeof(*s));
    if(!s) return;
    for(u32 i=0;i<sizeof(*s)/4;++i) ((volatile u32 *)s)[i]=0;
    _Alignas(4) u8 descriptor[28];
    const u8 *original=FN(0x103d3dff,const u8 *,u32)(0x10005);
    copy(descriptor,original,sizeof(descriptor));
    descriptor[0]=1;
    *(u32 *)(descriptor+8)=8192;
    *(u16 *)(descriptor+26)=0;
    FN(0x103dc46b,int,void *,void (*)(void *),void *)(s,worker,descriptor);
    if((int)s->task[0]<=0) { release(s); return; }
    ANCHOR[0]=(u32)s;
    ANCHOR[1]=~(u32)s;
    __asm__ volatile("dmb sy" ::: "memory");
    FN(0x103606df,int,void *)(s);
}
