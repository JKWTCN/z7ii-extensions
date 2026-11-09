/* SPDX-License-Identifier: MIT */
#ifndef Z72_GRID_SETTINGS_H
#define Z72_GRID_SETTINGS_H
#include "z72.h"

/* C 1.70 synchronous file-object API, on the SD volume only. */
#define Z72_CFG_FN(a, result, ...) ((result (*)(__VA_ARGS__))(a))
#define Z72_CFG_OK 0x043a0080u
#define Z72_CFG_MAGIC 0x3147525au /* ZRG1 */

volatile z72_u32 grid_saved_style=0xffffffffu;
volatile z72_u32 grid_save_status;

static void z72_grid_settings_load(void)
{
    z72_u32 file[10],actual=0;
    _Alignas(64) z72_u32 data[16];
    for(z72_u32 i=0;i<10;++i) ((volatile z72_u32 *)file)[i]=0;
    struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,data,17};
    Z72_CFG_FN(0x104d3c01,void,z72_u32 *,const char *)(file,"B:\\Z72APP.CFG");
    if(Z72_CFG_FN(0x104d3c05,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)!=Z72_CFG_OK)
        return;
    z72_u32 result=1;
    if(file[6]==16 && file[7]==0) {
        Z72_CFG_FN(0x1034fa29,void,void *,z72_u32)(data,sizeof(data));
        result=Z72_CFG_FN(0x104d3d83,z72_u32,z72_u32 *,void *,z72_u32,z72_u32 *)(file,&request,17,&actual);
        Z72_CFG_FN(0x1034fa29,void,void *,z72_u32)(data,sizeof(data));
    }
    z72_u32 closed=Z72_CFG_FN(0x104d3c91,z72_u32,z72_u32 *)(file);
    if(result==Z72_CFG_OK && closed==Z72_CFG_OK && actual==16 &&
       data[0]==Z72_CFG_MAGIC && data[1]==1 && data[2]<=3 && data[3]==~data[2])
        grid_saved_style=data[2];
}

static void z72_grid_settings_save(z72_u32 style)
{
    if(style==grid_saved_style) return;
    z72_u32 actual=0;
    _Alignas(64) z72_u32 data[16];
    for(z72_u32 i=0;i<16;++i) ((volatile z72_u32 *)data)[i]=0;
    data[0]=Z72_CFG_MAGIC; data[1]=1; data[2]=style; data[3]=~style;
    struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,data,16};
    /* Ordinary create/write/close services (operations 17/13/10).
     * The streaming write service requires a different handle registration.
     */
    int opened=Z72_CFG_FN(0x105beb2b,int,const char *,z72_u32)("B:\\Z72APP.CFG",0);
    if(opened<0) { grid_save_status=(z72_u32)opened; return; }
    grid_saved_style=0xffffffffu;
    Z72_CFG_FN(0x1034f9ed,void,void *,z72_u32)(data,sizeof(data));
    z72_u32 result=Z72_CFG_FN(0x105bea79,z72_u32,int,void *,z72_u32,z72_u32 *)(opened,&request,16,&actual);
    z72_u32 closed=Z72_CFG_FN(0x105be9dd,z72_u32,int)(opened);
    grid_save_status=result ? result : closed;
    if(!result && !closed && actual==16) {
        grid_saved_style=style;
        grid_save_status=Z72_CFG_OK;
    } else if(!grid_save_status) grid_save_status=1;
}
#endif
