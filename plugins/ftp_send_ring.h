/* SPDX-License-Identifier: MIT */
#ifndef Z72_FTP_SEND_RING_H
#define Z72_FTP_SEND_RING_H

#include "tcp_send_ring.h"

static struct tcp_send_ring ftp_ring;

static int ftp_attach_send_ring(int fd)
{
    int attached=tcp_attach_send_ring(&ftp_ring,fd,ftp_request.large_send_address);
    if (attached) ftp_result.large_in_use=1;
    return attached;
}

static void ftp_reclaim_send_ring(void)
{
    if (!ftp_ring.socket) return;
    tcp_reclaim_send_ring(&ftp_ring);
    if (!ftp_ring.socket) ftp_result.large_in_use=0;
}
#endif
