/* SPDX-License-Identifier: MIT */
#define Z72_GRID_2X2 1
#define Z72_GRID_TRACE 0
#define Z72_GRID_WITH_PORTRAIT 1
#include "grid_live.c"
#include "portrait_menu.c"

z72_u32 z72_main(const struct z72_api *api)
{
    if (!grid_ready(api) || !z72_grid_menu_ready() || !histogram_horizon_ready(api))
        return 1;
    z72_u32 result=portrait_init(api);
    if (result) return result;
    grid_install();
    return 0;
}
