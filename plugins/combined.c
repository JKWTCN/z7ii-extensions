/* SPDX-License-Identifier: MIT */
#define Z72_GRID_2X2 1
#define Z72_GRID_TRACE 0
#define Z72_PORTRAIT_TRACE 0
#define Z72_GRID_WITH_PORTRAIT 1
#define Z72_PHOTO_ZEBRA 1
#define Z72_MENU_EXTRA_ROWS 3u
#define Z72_CUSTOM_DISP 1
#define Z72_FOCAL_LENGTH 1
#define Z72_BATTERY_PERCENT 1
#define Z72_AF_SHORTCUT 1
#include "disp.h"
#include "af_shortcut.h"
#ifndef Z72_ZEBRA_OBSERVE_ONLY
#define Z72_ZEBRA_OBSERVE_ONLY 0
#endif
#include "grid_live.c"
#include "portrait_menu.c"
#include "af_shortcut_menu.h"

z72_u32 z72_main(const struct z72_api *api)
{
    if (!grid_ready(api) || !z72_grid_menu_ready() || !histogram_horizon_ready(api) || !disp_ready() ||
        !focal_ready() || !battery_ready() || !af_shortcut_ready() || !af_menu_ready())
        return 1;
    z72_u32 result=portrait_init(api);
    if (result) return result;
    grid_install(rotation_menu.title[0] ? Z72_TEXT_GRID_GOLDEN : 0);
    zebra_producer_install();
    disp_install();
    af_menu_install();
    af_shortcut_install();
    return 0;
}
