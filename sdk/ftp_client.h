/* SPDX-License-Identifier: MIT */
#ifndef Z72_FTP_CLIENT_H
#define Z72_FTP_CLIENT_H

#define FTP_CHUNK 65536u
#define FTP_SLOTS 2u
#define FTP_POLL_MS 2u
#define FTP_SLOT_BYTES (FTP_CHUNK+64u) /* Aligned EOF read slack. */
#define FTP_LARGE_SEND_BYTES (4u * 1024u * 1024u)
#define FTP_SHARED_RAM_MIN 0xb1600000u
#define FTP_SHARED_RAM_END 0xc0000000u
enum ftp_file_state { FTP_IDLE, FTP_READY, FTP_FILE_ERROR, FTP_FILE_TIMEOUT, FTP_FILE_CANCELLED };
enum ftp_stage {
    FTP_WAIT_AP=0, FTP_TASK, FTP_CONNECT, FTP_GREETING, FTP_LOGIN, FTP_TYPE,
    FTP_PASSIVE, FTP_DATA_CONNECT, FTP_STORE, FTP_READ_CARD, FTP_SEND,
    FTP_FINISH, FTP_DONE,
    FTP_WAIT_BUFFER,
};

/* Main and auxiliary CPUs write separate sets of cache lines. In the menu
 * version, sequence is the latest published chunk and consumed is the latest
 * chunk accepted by TCP. The producer may be at most FTP_SLOTS ahead. */
struct ftp_request {
    volatile unsigned int state, sequence, size, total, port;
    volatile unsigned int generation;
    unsigned char ip[4];
    char user[64], password[64], remote[64];
    unsigned int large_send_address;
    unsigned int overwrite;
    char folder[64];
};
struct ftp_result {
    volatile unsigned int stage;
    volatile unsigned int consumed;
    volatile unsigned int sent, reply, done;
    volatile int error;
    volatile unsigned int generation, network;
    volatile unsigned int send_buffer;
    volatile unsigned int large_in_use, wifi_stop;
};
#endif
