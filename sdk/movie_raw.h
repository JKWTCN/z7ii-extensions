/* SPDX-License-Identifier: MIT */
#ifndef Z72_MOVIE_RAW_H
#define Z72_MOVIE_RAW_H
#include "z72.h"

#define Z72_RAW_CFG_SYNC_OFF (4u<<24)
#define Z72_RAW_CFG_DEFAULT 0x01010085u
#define Z72_RAW_LABEL_COUNT 24u

static inline int z72_raw_config_valid(z72_u32 value)
{
    return !(value&0x2au) && ((value>>6)&3u)<=2u && ((value>>8)&255u)<=4u &&
           ((value>>16)&255u)<=2u && ((value&~Z72_RAW_CFG_SYNC_OFF)>>24)<=2u;
}

const unsigned short *z72_raw_menu_label(z72_u32 language,z72_u32 label);
int z72_raw_save_settings(z72_u32 value);
void z72_raw_video_install(z72_u32 text_base,z72_u32 settings,void (*movie_ui)(z72_u32));
#endif
