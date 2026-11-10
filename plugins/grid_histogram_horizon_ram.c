/* SPDX-License-Identifier: MIT */
#define z72_main grid_init
#include "grid_live.c"
#undef z72_main

#define Z72_HISTOGRAM_WITH_GRID 1
#include "histogram_horizon_ram.c"

z72_u32 z72_main(const struct z72_api *api)
{
    if (!histogram_horizon_ready(api)) return 1;
    z72_u32 result=grid_init(api);
    if (!result) histogram_horizon_install();
    return result;
}
