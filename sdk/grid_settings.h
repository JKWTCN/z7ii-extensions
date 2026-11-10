/* SPDX-License-Identifier: MIT */
#ifndef Z72_GRID_SETTINGS_H
#define Z72_GRID_SETTINGS_H
#include "config.h"

volatile z72_u32 grid_saved_style=Z72_CFG_NO_GRID;
volatile z72_u32 grid_save_status;

static void z72_grid_settings_save(z72_u32 style)
{
    struct z72_config config=z72_config_current;
    config.grid_style=style;
    z72_u32 result=z72_config_save(config);
    grid_saved_style=result==Z72_CFG_OK ? style : Z72_CFG_NO_GRID;
    grid_save_status=result;
}
#endif
