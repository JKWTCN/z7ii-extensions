/* SPDX-License-Identifier: MIT */
#include "z72.h"
#include "grid_menu.h"
#include "grid_settings.h"
#ifdef Z72_GRID_GEOMETRY
#include "grid_geometry.h"
static volatile z72_u32 transformed_counts[2];
#endif

typedef z72_u32 u32;
typedef u32 (*handler)(void *,u32,u32,u32);
#define PRIMARY_SLOT ((volatile u32 *)0xb1d02098)
#define SECONDARY_SLOT ((volatile u32 *)0xb1d0da44)
#define PRIMARY_HANDLER 0x104d80cfu
#define SECONDARY_HANDLER 0x10445713u
#define RECORDS 16u
#define RECORD_WORDS 24u
#define BANK_WORDS (8u+RECORDS*RECORD_WORDS)
#define LOG_WORDS (32u+2u*BANK_WORDS)

volatile u32 selected_style;
volatile u32 trace_write_status;
/* Each native display task is the sole writer of its bank. Published records
 * are immutable; the menu task never resets banks or waits for a writer.
 * stats: calls, batches, descriptors, published, dropped, code, return, rejected.
 */
static volatile struct {
    u32 stats[8];
    u32 records[RECORDS][RECORD_WORDS];
} trace_banks[2];
static _Alignas(64) u32 trace_log[LOG_WORDS];

static u32 ram(u32 address,u32 bytes)
{
    return !(address&3u) && address>=0xb0000000u &&
           bytes<=0x10000000u && address<=0xc0000000u-bytes;
}

static void capture(u32 channel,void *message,u32 mode,u32 immediate)
{
    volatile u32 *stats=trace_banks[channel].stats;
    ++stats[0];
    if(!ram((u32)message,24)) { ++stats[7]; return; }
    const volatile u32 *msg=message;
    u32 code=msg[4];
    stats[5]=code;
    if(!immediate || (code!=immediate && code!=immediate+1)) return;
    ++stats[1];
    u32 payload=msg[5];
    if(!ram(payload,8)) { ++stats[7]; return; }
    const volatile u32 *batch=(const volatile u32 *)payload;
    u32 count=batch[0],entries=batch[1];
    if(!count || count>5 || !ram(entries,count*8)) { ++stats[7]; return; }
    for(u32 i=0;i<count;++i) {
        const volatile u32 *entry=(const volatile u32 *)(entries+i*8);
        if((entry[0]&255u)!=4) continue;
        u32 descriptor=entry[1];
        if(!ram(descriptor,48)) { ++stats[7]; continue; }
        ++stats[2];
        u32 index=stats[3];
        u32 style=selected_style;
        volatile u32 *record=0;
        if(index>=RECORDS) ++stats[4];
        else {
            record=trace_banks[channel].records[index];
            record[0]=stats[0]; record[1]=(u32)message; record[2]=code;
            record[3]=payload; record[4]=count; record[5]=i;
            record[6]=descriptor; record[7]=style;
            record[8]=mode; record[9]=channel; record[10]=0; record[11]=0;
            for(u32 word=0;word<12;++word)
                record[12+word]=((const volatile u32 *)descriptor)[word];
        }
#ifdef Z72_GRID_GEOMETRY
        /* Keep the original pointer: deferred completion queues return it to
         * its producer. A stack or shared scratch replacement is not valid.
         * Capturing is bounded, but transformation continues after saturation.
         */
        u32 changed=grid_transform((void *)descriptor,style);
        transformed_counts[channel]+=changed;
        if(record) record[11]=changed;
#endif
        if(record) {
            __asm__ volatile("dmb sy" ::: "memory");
            stats[3]=index+1;
        }
    }
}

u32 grid_primary(void *message,u32 a,u32 b,u32 c)
{
    capture(0,message,0xffffffffu,0x42061004);
    u32 result=((handler)PRIMARY_HANDLER)(message,a,b,c);
    trace_banks[0].stats[6]=result;
    return result;
}

u32 grid_secondary(void *message,u32 a,u32 b,u32 c)
{
    u32 mode=*(const volatile unsigned char *)0xb180854c;
    capture(1,message,mode,mode==0 ? 0x42060004u : mode==1 ? 0x42062004u : 0);
    u32 result=((handler)SECONDARY_HANDLER)(message,a,b,c);
    trace_banks[1].stats[6]=result;
    return result;
}

/* Called only by the loader/menu task, never by a display callback. */
u32 trace_export(u32 requested_style,u32 reason)
{
    for(u32 i=0;i<LOG_WORDS;++i) ((volatile u32 *)trace_log)[i]=0;
    trace_log[0]=0x5432375a; trace_log[1]=1; trace_log[2]=sizeof(trace_log);
    trace_log[3]=RECORDS; trace_log[4]=RECORD_WORDS; trace_log[5]=BANK_WORDS;
    trace_log[6]=requested_style; trace_log[7]=selected_style;
    trace_log[8]=grid_save_status; trace_log[9]=grid_saved_style;
    trace_log[10]=reason;
    trace_log[11]=*PRIMARY_SLOT; trace_log[12]=*SECONDARY_SLOT;
    trace_log[13]=(u32)grid_primary; trace_log[14]=(u32)grid_secondary;
#ifdef Z72_GRID_GEOMETRY
    trace_log[15]=1;
    trace_log[24]=transformed_counts[0]; trace_log[25]=transformed_counts[1];
#endif
    trace_log[16]=*(const volatile u32 *)(0xb2bbb56c+16);
    for(u32 i=0;i<7;++i) trace_log[17+i]=((const volatile u32 *)0xb18509fc)[i];
    for(u32 channel=0;channel<2;++channel) {
        u32 count=trace_banks[channel].stats[3];
        __asm__ volatile("dmb sy" ::: "memory");
        u32 at=32+channel*BANK_WORDS;
        for(u32 i=0;i<8;++i) trace_log[at+i]=trace_banks[channel].stats[i];
        trace_log[at+3]=count;
        for(u32 i=0;i<count;++i)
            for(u32 word=0;word<RECORD_WORDS;++word)
                trace_log[at+8+i*RECORD_WORDS+word]=trace_banks[channel].records[i][word];
    }
    int file=Z72_CFG_FN(0x105beb2b,int,const char *,u32)("B:\\Z72TRACE.LOG",0);
    if(file<0) return trace_write_status=(u32)file;
    Z72_CFG_FN(0x1034f9ed,void,void *,u32)(trace_log,sizeof(trace_log));
    u32 result=0;
    for(u32 offset=0;offset<sizeof(trace_log);) {
        u32 size=sizeof(trace_log)-offset,actual=0;
        if(size>512) size=512;
        struct { u32 kind; void *buffer; u32 capacity; } request={1,(unsigned char *)trace_log+offset,size};
        result=Z72_CFG_FN(0x105bea79,u32,int,void *,u32,u32 *)(file,&request,size,&actual);
        if(result || actual!=size) { if(!result) result=1; break; }
        offset+=size;
    }
    u32 closed=Z72_CFG_FN(0x105be9dd,u32,int)(file);
    return trace_write_status=result ? result : closed;
}

u32 grid_apply(u32 style)
{
    if(style>Z72_GRID_STYLE_MAX) return 1;
    z72_grid_settings_save(style);
    trace_export(style,1);
#ifdef Z72_GRID_AFTER_EXPORT
    Z72_GRID_AFTER_EXPORT();
#endif
    return 0;
}

u32 z72_main(const struct z72_api *api)
{
    if(api->abi!=Z72_ABI || api->target!=Z72_TARGET || api->size<sizeof(*api) ||
       *PRIMARY_SLOT!=PRIMARY_HANDLER || *SECONDARY_SLOT!=SECONDARY_HANDLER) return 1;
    selected_style=((u32 (*)(u32))0x1063c7d5)(0x112d)==1 ? Z72_GRID_FOUR : Z72_GRID_OFF;
    z72_grid_settings_load();
    if(selected_style && grid_saved_style>=Z72_GRID_FOUR && grid_saved_style<=Z72_GRID_STYLE_MAX)
        selected_style=grid_saved_style;
    if(z72_grid_menu_install()) return 2;
    __asm__ volatile("dmb sy" ::: "memory");
    *PRIMARY_SLOT=(u32)grid_primary;
    *SECONDARY_SLOT=(u32)grid_secondary;
    __asm__ volatile("dmb sy" ::: "memory");
    trace_export(selected_style,0);
    return 0;
}
