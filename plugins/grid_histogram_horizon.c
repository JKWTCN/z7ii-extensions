/* SPDX-License-Identifier: MIT */
static void histogram_export(void);
#define Z72_GRID_2X2 1
#define Z72_GRID_AFTER_EXPORT histogram_export
#define Z72_HISTOGRAM_DIAGNOSTIC 1
#include "grid_histogram_horizon_ram.c"

#define HISTOGRAM_SNAPSHOT_WORDS 16u
#define EVF_LOG_OFFSET 102u

static volatile u32 results[HISTOGRAM_RESULTS];
static volatile u32 horizon_results[HISTOGRAM_RESULTS];
static volatile u32 last_update[HISTOGRAM_SNAPSHOT_WORDS];
static volatile u32 last_horizon[HISTOGRAM_SNAPSHOT_WORDS];
static volatile u32 updates,horizon_updates;
static volatile u32 evf_horizon_results[HISTOGRAM_RESULTS];
static volatile u32 last_evf_horizon[6];

static void histogram_snapshot(volatile u32 *row,u32 result,u32 before,u32 after)
{
    row[0]=result;
    row[1]=before;
    row[2]=after;
    row[3]=BYTE(DISPLAY_SNAPSHOT+0x19e);
    row[4]=BYTE(LAST_DISPLAY_SNAPSHOT+0x19e);
    row[5]=BYTE(DISPLAY_SNAPSHOT+0x1a6);
    row[6]=BYTE(0xb1821b18);
    row[7]=BYTE(DISPLAY_SCENE);
    row[8]=BYTE(0xb202e294);
    row[9]=BYTE(HISTOGRAM_CONTROL)|(BYTE(HISTOGRAM_CONTROL+1)<<8);
    row[10]=WORD(HISTOGRAM_CONTROL+0x24);
    row[11]=WORD(HISTOGRAM_MASKS);
    row[12]=BYTE(0xb1864d49);
    row[13]=BYTE(0xb1864d4a);
    row[14]=WORD(ACTIVE_OUTPUT_BITS);
    row[15]=CALL(0x1061e41d,u32,void)();
}

static void histogram_observe(enum histogram_result result,u32 before,u32 after)
{
    ++updates;
    ++results[result];
    histogram_snapshot(last_update,result,before,after);
    if (last_update[3]==3) {
        ++horizon_updates;
        ++horizon_results[result];
        for (u32 i=0;i<HISTOGRAM_SNAPSHOT_WORDS;++i) last_horizon[i]=last_update[i];
        if ((last_update[14]&7u)==EVF_OUTPUT) {
            ++evf_horizon_results[result];
            last_evf_horizon[0]=before;
            last_evf_horizon[1]=after;
            last_evf_horizon[2]=last_update[4];
            last_evf_horizon[3]=last_update[7];
            last_evf_horizon[4]=last_update[14];
            last_evf_horizon[5]=last_update[15];
        }
    }
}

/* Export after d12 confirmation on the menu task, outside the display path. */
static void histogram_export(void)
{
    static _Alignas(64) u32 log[EVF_LOG_OFFSET+HISTOGRAM_RESULTS+6];
    _Static_assert(sizeof(log)==512,"Unexpected histogram log size");
    log[0]=0x5453485au; /* ZHST */
    log[1]=3;
    log[2]=sizeof(log);
    log[3]=updates;
    log[4]=horizon_updates;
    log[5]=WORD(GUI_UPDATE_SLOT);
    log[6]=(u32)gui_update;
    log[7]=WORD(0xb202f728);
    for (u32 i=0;i<HISTOGRAM_RESULTS;++i) {
        log[8+i]=results[i];
        log[8+HISTOGRAM_RESULTS+i]=horizon_results[i];
        log[EVF_LOG_OFFSET+i]=evf_horizon_results[i];
    }
    u32 offset=8+2*HISTOGRAM_RESULTS;
    for (u32 i=0;i<HISTOGRAM_SNAPSHOT_WORDS;++i) {
        log[offset+i]=last_update[i];
        log[offset+HISTOGRAM_SNAPSHOT_WORDS+i]=last_horizon[i];
    }
    for (u32 i=0;i<6;++i) log[EVF_LOG_OFFSET+HISTOGRAM_RESULTS+i]=last_evf_horizon[i];
    u32 tick=WORD(DISPLAY_UPDATE_TICK);
    histogram_snapshot(log+offset+2*HISTOGRAM_SNAPSHOT_WORDS,HISTOGRAM_RESULTS,tick,tick);
    log[96]=painter_calls;
    log[97]=visible_painter_calls;
    log[98]=present_requests;
    log[99]=painter_object;
    log[100]=painter_visibility;
    log[101]=painter_scale;
    int file=Z72_CFG_FN(0x105beb2b,int,const char *,u32)("B:\\Z72HIST.LOG",0);
    if (file<0) return;
    u32 actual=0;
    struct { u32 kind; const void *buffer; u32 capacity; } request={1,log,sizeof(log)};
    Z72_CFG_FN(0x1034f9ed,void,void *,u32)(log,sizeof(log));
    Z72_CFG_FN(0x105bea79,u32,int,void *,u32,u32 *)(file,&request,sizeof(log),&actual);
    Z72_CFG_FN(0x105be9dd,u32,int)(file);
}
