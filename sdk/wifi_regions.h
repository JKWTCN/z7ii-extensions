/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_REGIONS_H
#define Z72_WIFI_REGIONS_H

/* These indexes are persisted in Z72APP.CFG. Append rather than reorder. */
enum {
    Z72_WIFI_REGION_MAINLAND=0,
    Z72_WIFI_REGION_US,
    Z72_WIFI_REGION_BODY,
    Z72_WIFI_REGION_COUNT
};
#define Z72_WIFI_COUNTRIES {"CN","US"}
#define Z72_WIFI_COUNTRY_REVISIONS {38u,0u}
#endif
