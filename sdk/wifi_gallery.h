/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_GALLERY_H
#define Z72_WIFI_GALLERY_H

#define GALLERY_PORT 80u
#define GALLERY_PORT_TEXT "80"
#define GALLERY_CHUNK 65536u
#define GALLERY_SLOT_BYTES (GALLERY_CHUNK+64u)
#define GALLERY_SLOTS 2u
#define GALLERY_NAME_BYTES 32u
#define GALLERY_THUMB_BATCH 6u

enum gallery_route { GALLERY_HOME, GALLERY_LIST, GALLERY_THUMB, GALLERY_PREVIEW,
                     GALLERY_JPG, GALLERY_NEF, GALLERY_VIEW_JPG, GALLERY_THUMBS,
                     GALLERY_MISSING };
enum gallery_status { GALLERY_OFF, GALLERY_WAIT_AP, GALLERY_LISTEN, GALLERY_SERVE,
                      GALLERY_SOCKET_ERROR, GALLERY_TASK_ERROR };
enum gallery_buffer { GALLERY_BUFFER_UNUSED, GALLERY_BUFFER_NATIVE, GALLERY_BUFFER_LARGE };

/* The main CPU invalidates request/state lines written by the uncached ARM926.
 * Each side publishes only its own cache lines. A request's slot space stays
 * reserved until the receiver has sent or abandoned it and the producer has
 * closed its file and acknowledged release. */
struct gallery_control {
    volatile unsigned int epoch; /* Odd while enabled. */
};
struct gallery_request {
    volatile unsigned int serial, epoch, route, value, head, consumed, cancelled;
};
struct gallery_response {
    volatile unsigned int serial, status, length, produced, failed, released;
    char name[GALLERY_NAME_BYTES];
    volatile unsigned int send_address;
};
struct gallery_state {
    volatile unsigned int stage, network, large_in_use, download_buffer;
};

_Static_assert(sizeof(struct gallery_response)<=64,"Gallery response fits its cache line");

#endif
