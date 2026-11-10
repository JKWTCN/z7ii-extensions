/* SPDX-License-Identifier: MIT */
#ifndef Z72_CONFIG_H
#define Z72_CONFIG_H
#include "z72.h"

/* C 1.70 synchronous file-object API, on the SD volume only. */
#define Z72_CFG_FN(a, result, ...) ((result (*)(__VA_ARGS__))(a))
#define Z72_CFG_OK 0x043a0080u
#define Z72_CFG_MAGIC 0x3147525au /* ZRG1 */
#define Z72_CFG_NO_GRID 0xffffffffu

struct z72_config {
    z72_u32 grid_style;
    z72_u32 auto_rotate;
};

/* Both menu callbacks run on the same task and save the whole record. */
static struct z72_config z72_config_current={Z72_CFG_NO_GRID,1};
static z72_u32 z72_config_pending;

static inline struct z72_config z72_config_load(void)
{
    struct z72_config config={Z72_CFG_NO_GRID,1};
    z72_u32 file[10],actual=0;
    _Alignas(64) z72_u32 data[16];
    for(z72_u32 i=0;i<10;++i) ((volatile z72_u32 *)file)[i]=0;
    Z72_CFG_FN(0x104d3c01,void,z72_u32 *,const char *)(file,"B:\\Z72APP.CFG");
    if(Z72_CFG_FN(0x104d3c05,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)!=Z72_CFG_OK)
        return config;
    z72_u32 size=file[6],result=1;
    if((size==16 || size==24) && file[7]==0) {
        /* The native reader needs one byte of capacity beyond EOF. */
        struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,data,size+1};
        Z72_CFG_FN(0x1034fa29,void,void *,z72_u32)(data,sizeof(data));
        result=Z72_CFG_FN(0x104d3d83,z72_u32,z72_u32 *,void *,z72_u32,z72_u32 *)(file,&request,size+1,&actual);
        Z72_CFG_FN(0x1034fa29,void,void *,z72_u32)(data,sizeof(data));
    }
    z72_u32 closed=Z72_CFG_FN(0x104d3c91,z72_u32,z72_u32 *)(file);
    if(result!=Z72_CFG_OK || closed!=Z72_CFG_OK || actual!=size ||
       data[0]!=Z72_CFG_MAGIC || data[3]!=~data[2]) return config;
    if(size==16 && data[1]==1 && data[2]<=4) config.grid_style=data[2];
    if(size==24 && data[1]==2 && (data[2]<=4 || data[2]==Z72_CFG_NO_GRID) &&
       data[4]<=1 && data[5]==~data[4])
        config=(struct z72_config){data[2],data[4]};
    return config;
}

static inline z72_u32 z72_config_save(struct z72_config config)
{
    if(!z72_config_pending && config.grid_style==z72_config_current.grid_style &&
       config.auto_rotate==z72_config_current.auto_rotate) return Z72_CFG_OK;
    z72_config_current=config;
    z72_config_pending=1;
    /* Keep writing v1 when its grid-only record can represent the settings. */
    z72_u32 version=config.auto_rotate && config.grid_style!=Z72_CFG_NO_GRID ? 1 : 2;
    z72_u32 size=version==1 ? 16 : 24,actual=0;
    _Alignas(64) z72_u32 data[16]={Z72_CFG_MAGIC,version,config.grid_style,~config.grid_style,
                                  config.auto_rotate,~config.auto_rotate};
    struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,data,size};
    int opened=Z72_CFG_FN(0x105beb2b,int,const char *,z72_u32)("B:\\Z72APP.CFG",0);
    if(opened<0) return (z72_u32)opened;
    Z72_CFG_FN(0x1034f9ed,void,void *,z72_u32)(data,sizeof(data));
    z72_u32 result=Z72_CFG_FN(0x105bea79,z72_u32,int,void *,z72_u32,z72_u32 *)(opened,&request,size,&actual);
    z72_u32 closed=Z72_CFG_FN(0x105be9dd,z72_u32,int)(opened);
    if(result) return result;
    if(closed) return closed;
    if(actual!=size) return 1;
    z72_config_pending=0;
    return Z72_CFG_OK;
}
#endif
