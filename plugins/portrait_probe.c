/* SPDX-License-Identifier: MIT */
#include "z72.h"
#include "menu.h"

#ifndef Z72_PORTRAIT_TRACE
#define Z72_PORTRAIT_TRACE 1
#endif

typedef z72_u32 u32;
#define WORD(address) (*(volatile u32 *)(address))
#define HALF(address) (*(volatile short *)(address))
#define BYTE(address) (*(volatile unsigned char *)(address))
#define CALL(address, result, ...) ((result (*)(__VA_ARGS__))(address))

#define GRAPHICS_TASK 0xb1d413a8u
#define GRAPHICS_TASK_ID 0xb1808bd0u
#define GRAPHICS_QUEUE_ENTRY 0x103d3d79u
#define DRAW_SLOT (GRAPHICS_TASK+0x24u)
#define DRAW_DISPATCH 0x1054ba99u
#define ORIENTATION_GET 0x103d4097u
#define DISPLAY_OUTPUTS 0xb181d3a4u
#define DRAW_TARGET 0xb1808becu
#define DISPLAY_SNAPSHOT 0xb202be70u
#define DRAW_FAMILY 0x42010000u
#define DRAW_CODES 0x63u

static struct z72_menu_storage menu;

#if Z72_PORTRAIT_TRACE
#define SAMPLE_LIMIT 64u
#define SAMPLE_WORDS 6u

static volatile u32 command_counts[DRAW_CODES];
static volatile u32 lcd_orientation_counts[5];
static volatile u32 last_command, last_orientation, last_outputs;
static u32 samples[3][SAMPLE_LIMIT][SAMPLE_WORDS];
static u32 sample_counts[3];
#endif

#ifdef Z72_PORTRAIT_IMAGE_TRIAL
#if Z72_PORTRAIT_TRACE
static volatile u32 rotated_images[2];
#endif

static u32 rotate_menu_image(volatile u32 *message,u32 orientation)
{
    /* Match the first i-menu image seen on both sides in the camera log.
     * Keep the rotated image inside the original clear rectangle. */
    if (message[4]!=0x4201001au || message[3]!=0x105a8b17u ||
        BYTE(DRAW_TARGET)!=2 || BYTE(DISPLAY_SNAPSHOT)!=0 ||
        BYTE(DISPLAY_SNAPSHOT+0x19e)!=0) return 0;
    u32 address=message[5];
    if (address<0xb1800000u || address>0xbfffffffu-0x63u || (address&3u)) return 0;
    const volatile u32 *payload=(const volatile u32 *)address;
    if (payload[0]!=1 || payload[6]!=(28u | (302u<<16)) ||
        payload[7]!=(124u | (334u<<16)) || (payload[16]&0xffffu)!=360u) return 0;

    u32 image[25];
    for (u32 i=0;i<25;++i) image[i]=payload[i];
    image[0]=0; /* Use the explicit destination box when rotated. */
    image[6]=70u | (302u<<16);
    image[7]=82u | (334u<<16);
    image[16]=(image[16]&0xffff0000u) | (orientation==2 ? 90u : 270u);
    /* The stock 0x1A branch renders synchronously through this adapter,
     * writes a zero result, then signals the waiting producer. */
    CALL(0x1054bf29,void,const u32 *)(image);
#if Z72_PORTRAIT_TRACE
    ++rotated_images[orientation-1];
#endif
    message[6]=0;
    CALL(0x105a8b17,void,u32,u32)(0x42010062u,0);
    return 1;
}
#endif

#if Z72_PORTRAIT_TRACE
static void sample_lcd_draw(u32 command,u32 orientation,const volatile u32 *payload)
{
    u32 count=sample_counts[orientation];
    if (count==SAMPLE_LIMIT || !payload || ((u32)payload&3u)) return;
    u32 geometry[4]={0,0,0,0};
#ifdef Z72_PORTRAIT_MENU
    u32 scope=portrait_sample(command-DRAW_FAMILY,orientation,payload,geometry);
    if (!scope) return;
#else
    switch (command-DRAW_FAMILY) {
    case 0x13: case 0x16: case 0x17:
        for (u32 i=0;i<4;++i) geometry[i]=payload[i];
        break;
    case 0x19: case 0x1a: case 0x1c:
        geometry[0]=payload[6];
        geometry[1]=payload[7];
        geometry[2]=payload[0];
        geometry[3]=payload[16];
        break;
    case 0x34:
        /* Text dispatch. Record the target without following its context. */
        break;
    default:
        return;
    }
#endif
    u32 context=BYTE(DRAW_TARGET) | (BYTE(DISPLAY_SNAPSHOT)<<8) |
                (BYTE(DISPLAY_SNAPSHOT+0x19e)<<16);
#ifdef Z72_PORTRAIT_MENU
    context|=(scope-1)<<24;
#endif
    for (u32 i=0;i<count;++i) {
        const u32 *row=samples[orientation][i];
        if (row[0]==command && row[1]==context && row[2]==geometry[0] &&
            row[3]==geometry[1] && row[4]==geometry[2] && row[5]==geometry[3]) return;
    }
    u32 *row=samples[orientation][count];
    row[0]=command;
    row[1]=context;
    for (u32 i=0;i<4;++i) row[2+i]=geometry[i];
    __atomic_store_n(&sample_counts[orientation],count+1,__ATOMIC_RELEASE);
}
#endif

/* The native graphics task calls this slot for each dequeued message. */
static void observe_draw(volatile u32 *message)
{
#ifdef Z72_PHOTO_ZEBRA
    zebra_graphics_observe();
#endif
    u32 command=message[4];
    if (command>=DRAW_FAMILY && command-DRAW_FAMILY<DRAW_CODES) {
        u32 outputs=WORD(DISPLAY_OUTPUTS)&7u;
        u32 orientation=outputs==1 ? CALL(ORIENTATION_GET,u32,void)() : 0;
#if Z72_PORTRAIT_TRACE
        ++command_counts[command-DRAW_FAMILY];
        if (outputs==1 && orientation<5) {
            ++lcd_orientation_counts[orientation];
            if (orientation<3) sample_lcd_draw(command,orientation,(const volatile u32 *)message[5]);
        }
        last_command=command;
        last_orientation=orientation;
        last_outputs=outputs;
#endif
#ifdef Z72_PORTRAIT_MENU
        if (portrait_draw(message,outputs,orientation)) return;
#endif
#ifdef Z72_PORTRAIT_IMAGE_TRIAL
        if (outputs==1 && (orientation==1 || orientation==2) &&
            rotate_menu_image(message,orientation)) return;
#endif
    }
    CALL(DRAW_DISPATCH,void,const volatile u32 *)(message);
}

#if Z72_PORTRAIT_TRACE
/* File I/O happens only on confirmation of the owned setup row. */
static u32 export_probe(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    static _Alignas(64) u32 log[128+3*SAMPLE_LIMIT*SAMPLE_WORDS];
    log[0]=0x4955565au; /* ZVUI */
#ifdef Z72_PORTRAIT_MENU
    log[1]=6;
#else
    log[1]=2;
#endif
    log[2]=sizeof(log);
    log[3]=WORD(DRAW_SLOT);
    log[4]=last_command;
    log[5]=last_orientation;
    log[6]=last_outputs;
    log[7]=DRAW_FAMILY;
    log[8]=DRAW_CODES;
    for (u32 i=0;i<5;++i) log[9+i]=lcd_orientation_counts[i];
    log[14]=SAMPLE_LIMIT;
    log[15]=SAMPLE_WORDS;
    for (u32 i=0;i<DRAW_CODES;++i) log[16+i]=command_counts[i];
#ifdef Z72_PORTRAIT_MENU
    portrait_log(log+118);
#endif
#ifdef Z72_PORTRAIT_IMAGE_TRIAL
    log[118]=rotated_images[0];
    log[119]=rotated_images[1];
#endif
    for (u32 orientation=0;orientation<3;++orientation) {
        u32 count=__atomic_load_n(&sample_counts[orientation],__ATOMIC_ACQUIRE);
        log[115+orientation]=count;
        u32 *dest=log+128+orientation*SAMPLE_LIMIT*SAMPLE_WORDS;
        for (u32 row=0;row<count;++row)
            for (u32 col=0;col<SAMPLE_WORDS;++col)
                dest[row*SAMPLE_WORDS+col]=samples[orientation][row][col];
    }
    int file=CALL(0x105beb2b,int,const char *,u32)("B:\\Z72VUI.LOG",0);
    if (file<0) return 1;
    CALL(0x1034f9ed,void,void *,u32)(log,sizeof(log));
    u32 result=0;
    for (u32 offset=0;offset<sizeof(log);offset+=512) {
        u32 actual=0;
        struct { u32 kind; const void *buffer; u32 capacity; } request={1,log+offset/4,512};
        result=CALL(0x105bea79,u32,int,void *,u32,u32 *)(file,&request,512,&actual);
        if (result || actual!=512) { result=1; break; }
    }
    u32 closed=CALL(0x105be9dd,u32,int)(file);
    return result || closed;
}
#endif

z72_u32 z72_main(const struct z72_api *api)
{
    if (api->abi!=Z72_ABI || api->target!=Z72_TARGET || api->size<sizeof(*api) ||
        WORD(DRAW_SLOT)!=DRAW_DISPATCH || WORD(GRAPHICS_TASK+4)!=GRAPHICS_QUEUE_ENTRY ||
        HALF(GRAPHICS_TASK_ID)<=0 || HALF(GRAPHICS_TASK_ID)!=HALF(GRAPHICS_TASK)) return 1;
#ifdef Z72_PORTRAIT_MENU
    if (!portrait_ready()) return 1;
    u32 result=portrait_install_menu();
#else
    u32 result=z72_menu_install(&menu,"Portrait probe: export log",export_probe);
#endif
    if (result) return result;
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(DRAW_SLOT)=(u32)observe_draw;
#ifdef Z72_PORTRAIT_MENU
    portrait_install();
#endif
    return 0;
}
