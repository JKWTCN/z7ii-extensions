/* SPDX-License-Identifier: MIT */
#define Z72_GRID_2X2 1
#define Z72_GRID_TRACE 0
#define Z72_PORTRAIT_TRACE 0
#define Z72_GRID_WITH_PORTRAIT 1
#define Z72_PHOTO_ZEBRA 1
#define Z72_MENU_EXTRA_ROWS 10u
#define Z72_FIRMWARE_INFO 1
#define Z72_WIDE_AF 1
#define Z72_SUBJECT_TRACKING 1
#define Z72_CUSTOM_DISP 1
#define Z72_FOCAL_LENGTH 1
#define Z72_BATTERY_PERCENT 1
#define Z72_AF_SHORTCUT 1
#define Z72_WIFI_5GHZ 1
#define Z72_FTP_FULL 1
#define Z72_JPEG_PRE_CAPTURE 1
#define Z72_SHUTTER_SOUND 1
#define Z72_MENU_SOUND_FONT 1
#define Z72_FILTERED_PLAYBACK 1
#define Z72_PIXEL_SHIFT 1
#include "disp.h"
#include "af_shortcut.h"
#include "movie_limit.h"
#include "movie_rotation.h"
#include "wifi.h"
#ifndef Z72_ZEBRA_OBSERVE_ONLY
#define Z72_ZEBRA_OBSERVE_ONLY 0
#endif
#include "grid_live.c"
#include "portrait_menu.c"
#include "wide_af.c"
#include "af_shortcut_menu.h"
#include "filtered_playback/filtered_playback.c"
#include "pixel_shift/pixel_shift.c"

static u32 pre_capture_menu_source(u32 header)
{
    return header==(u32)rotation_menu.header || zebra_header_owned(header) ||
        disp_header_owned(header) || wifi_header_owned(header) || af_menu_header_owned(header) ||
        sound_header_owned(header) || filter_header_owned(header) || wide_header_owned(header) ||
        pixel_header_owned(header) || ftp_header_owned(header);
}

static void sound_shutter_event(z72_u32 fn);
#include "pre_capture/pre_capture.c"
#include "shutter_sound.c"
#include "pixel_shift/pixel_shift_menu.h"

z72_u32 z72_main(const struct z72_api *api)
{
    if (!grid_ready(api) || !z72_grid_menu_ready() || !histogram_horizon_ready(api) || !disp_ready() ||
        !focal_ready() || !battery_ready() || !af_shortcut_ready() || !af_menu_ready() || !wifi_ready() ||
        !movie_limit_ready() || !pre_capture_ready() || !filtered_playback_ready() || !wide_ready())
        return 1;
    z72_u32 result=portrait_init(api);
    if (result) return result;
    grid_install(rotation_menu.title[0] ? Z72_TEXT_GRID_GOLDEN : 0);
    zebra_producer_install();
    disp_install();
    if (rotation_menu.title[0]) wide_install();
    af_menu_install();
    af_shortcut_install();
    wifi_install();
    movie_limit_install();
    movie_rot_install();
    if (pre_capture_menu_available) pre_capture_install();
    if (pixel_menu_available) {
        if (WORD(INPUT_SLOT)==INPUT_NATIVE) install_shared_capture_hooks();
        pixel_load_settings();
    }
    if (sound_menu_available) sound_install();
    if (rotation_menu.title[0]) filtered_playback_install();
    return 0;
}
