/* SPDX-License-Identifier: MIT */
#include "menu.h"
#include "movie_raw_source.h"
#ifdef Z72_RAW_COPY_DMA
#include "movie_raw_dma.h"
#endif
#ifdef Z72_RAW_RECORD_PACKED
#include "movie_raw_pack.h"
#endif

typedef z72_u32 u32;
#define WORD(a) (*(volatile u32 *)(a))
#define BYTE(a) (*(const volatile unsigned char *)(a))
#define CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))

#ifdef Z72_RAW_SELECT_OUTPUT
#ifdef Z72_RAW_NATIVE_MENU
/* Two bits per menu selection, in display order. */
enum { RAW_MENU_DEFAULTS=1u|(1u<<2)|(2u<<6) };
static u32 raw_settings=RAW_MENU_DEFAULTS;
#ifdef Z72_RAW_SIZE_SPLIT
static u32 raw_split_choice;
#endif
#ifdef Z72_RAW_DROP_EXPIRED
enum { RAW_TIMEOUT_BLACK,RAW_TIMEOUT_PREVIOUS,RAW_TIMEOUT_STOP };
static u32 raw_timeout_choice=RAW_TIMEOUT_PREVIOUS,clip_timeout_choice;
#endif
#ifdef Z72_RAW_COMPRESSION_MENU
enum { RAW_COMPRESSION_MIXED,RAW_COMPRESSION_ALL,RAW_COMPRESSION_OFF };
static u32 raw_compression_choice=RAW_COMPRESSION_ALL;
#endif
#ifdef Z72_RAW_MOV_SYNC_MENU
enum { RAW_MOV_SYNC_ON,RAW_MOV_SYNC_OFF };
static u32 raw_mov_sync_choice=RAW_MOV_SYNC_ON;
#endif
#define RECORD_ONLY() (!(load(&raw_settings)&1u) && (load(&raw_settings)&16u))
#else
static u32 record_only;
#define RECORD_ONLY() load(&record_only)
#endif
#define ARM_LABEL (RECORD_ONLY() ? "ZRV only: arm" : "MOV + ZRV: arm")
#define OPENING_LABEL (RECORD_ONLY() ? "ZRV only: opening" : "MOV + ZRV: opening")
#define ARMED_LABEL (RECORD_ONLY() ? "ZRV only: armed" : "MOV + ZRV: armed")
#elif defined(Z72_RAW_PAIR_AUTO)
#define ARM_LABEL "Pair v5: arm both"
#define OPENING_LABEL "Pair v5: opening"
#define ARMED_LABEL "Pair v5: standby"
#elif defined(Z72_RAW_REFERENCE_CODEC)
#define ARM_LABEL "Pair v4: arm standby"
#define OPENING_LABEL "Pair v4: opening"
#define ARMED_LABEL "Pair v4: armed"
#elif defined(Z72_RAW_REFERENCE_PROFILE)
#define ARM_LABEL "Pair: arm standby"
#define OPENING_LABEL "Pair: opening"
#define ARMED_LABEL "Pair: armed"
#elif defined(Z72_RAW_RECORD_STANDBY)
#define ARM_LABEL "ZRV only v1: arm"
#define OPENING_LABEL "ZRV only v1: opening"
#define ARMED_LABEL "ZRV only v1: armed"
#elif defined(Z72_RAW_RECHECK_SLOT)
#define ARM_LABEL "RAW 25p RAM v2: arm"
#define OPENING_LABEL "RAW 25p RAM v2: preparing"
#define ARMED_LABEL "RAW 25p RAM v2: armed"
#elif defined(Z72_RAW_DUAL_25P)
#define ARM_LABEL "MOV + ZRV 25p: arm"
#define OPENING_LABEL "MOV + ZRV 25p: opening"
#define ARMED_LABEL "MOV + ZRV 25p: armed"
#elif defined(Z72_RAW_DIRECT_60S)
#define ARM_LABEL "ZRV direct 60s: arm"
#define OPENING_LABEL "ZRV direct 60s: opening"
#define ARMED_LABEL "ZRV direct 60s: armed"
#elif defined(Z72_RAW_DMA_BUSY_RETRY)
#define ARM_LABEL "ZRV 30s v2: arm"
#define OPENING_LABEL "ZRV 30s v2: opening"
#define ARMED_LABEL "ZRV 30s v2: armed"
#elif defined(Z72_RAW_DIRECT_30S)
#define ARM_LABEL "ZRV direct 30s: arm"
#define OPENING_LABEL "ZRV direct 30s: opening"
#define ARMED_LABEL "ZRV direct 30s: armed"
#elif defined(Z72_RAW_DIRECT_FILE)
#define ARM_LABEL "ZRV direct 25p: arm"
#define OPENING_LABEL "ZRV direct 25p: opening"
#define ARMED_LABEL "ZRV direct 25p: armed"
#elif defined(Z72_RAW_ZRV_ONLY)
#define ARM_LABEL "ZRV only 25p: arm"
#define OPENING_LABEL "ZRV only 25p: opening"
#define ARMED_LABEL "ZRV only 25p: armed"
#elif defined(Z72_RAW_PHOTO_FILE_EXACT_SIZE)
#define ARM_LABEL "RAW 25p photo v2: arm"
#define OPENING_LABEL "RAW 25p photo v2: opening"
#define ARMED_LABEL "RAW 25p photo v2: armed"
#elif defined(Z72_RAW_PHOTO_FILE)
#define ARM_LABEL "RAW 25p photo I/O: arm"
#define OPENING_LABEL "RAW 25p photo I/O: opening"
#define ARMED_LABEL "RAW 25p photo I/O: armed"
#elif defined(Z72_RAW_DMA_FAST_STREAM)
#define ARM_LABEL "RAW DMA 25p v2: arm"
#define OPENING_LABEL "RAW DMA 25p v2: opening"
#define ARMED_LABEL "RAW DMA 25p v2: armed"
#elif defined(Z72_RAW_DMA_STREAM_25P)
#define ARM_LABEL "RAW DMA 25p: arm"
#define OPENING_LABEL "RAW DMA 25p: opening"
#define ARMED_LABEL "RAW DMA 25p: armed"
#elif defined(Z72_RAW_STREAM_SLOT_RECHECK)
#define ARM_LABEL "RAW DMA 12.5p v2: arm"
#define OPENING_LABEL "RAW DMA 12.5p v2: opening"
#define ARMED_LABEL "RAW DMA 12.5p v2: armed"
#elif defined(Z72_RAW_DMA_STREAM)
#define ARM_LABEL "RAW DMA 12.5p: arm"
#define OPENING_LABEL "RAW DMA 12.5p: opening"
#define ARMED_LABEL "RAW DMA 12.5p: armed"
#elif defined(Z72_RAW_COPY_DMA)
#define ARM_LABEL "RAW DMA v1: arm"
#define OPENING_LABEL "RAW DMA v1: preparing"
#define ARMED_LABEL "RAW DMA v1: armed"
#elif defined(Z72_RAW_PREVIEW_PRIORITY)
#define ARM_LABEL "RAW 25p RAM v4: arm"
#define OPENING_LABEL "RAW 25p RAM v4: preparing"
#define ARMED_LABEL "RAW 25p RAM v4: armed"
#elif defined(Z72_RAW_RECOVER_NEXT_FRAME)
#define ARM_LABEL "RAW 25p RAM v3: arm"
#define OPENING_LABEL "RAW 25p RAM v3: preparing"
#define ARMED_LABEL "RAW 25p RAM v3: armed"
#elif defined(Z72_RAW_RECORD_ROLLING)
#define ARM_LABEL "RAW 25p RAM: arm"
#define OPENING_LABEL "RAW 25p RAM: preparing"
#define ARMED_LABEL "RAW 25p RAM: armed"
#elif defined(Z72_RAW_RECORD_DEFERRED)
#define ARM_LABEL "RAW burst: arm"
#define OPENING_LABEL "RAW burst: preparing"
#define ARMED_LABEL "RAW burst: armed"
#elif defined(Z72_RAW_RECORD_PACKED_BYTE)
#define ARM_LABEL "RAW CFB lossless v2: arm"
#define OPENING_LABEL "RAW CFB lossless v2: opening"
#define ARMED_LABEL "RAW CFB lossless v2: armed"
#elif defined(Z72_RAW_RECORD_PACKED)
#define ARM_LABEL "RAW CFB lossless v1: arm"
#define OPENING_LABEL "RAW CFB lossless v1: opening"
#define ARMED_LABEL "RAW CFB lossless v1: armed"
#elif defined(Z72_RAW_RECORD_FULL_CFB_12)
#define ARM_LABEL "RAW CFB full v2: arm"
#define OPENING_LABEL "RAW CFB full v2: opening"
#define ARMED_LABEL "RAW CFB full v2: armed"
#elif defined(Z72_RAW_RECORD_FULL_CFB)
#define ARM_LABEL "RAW CFB full: arm"
#define OPENING_LABEL "RAW CFB full: opening"
#define ARMED_LABEL "RAW CFB full: armed"
#elif defined(Z72_RAW_RECORD_720P) && defined(Z72_RAW_RECORD_HEAP)
#define ARM_LABEL "RAW CFB 720p v3: arm"
#define OPENING_LABEL "RAW CFB 720p v3: opening"
#define ARMED_LABEL "RAW CFB 720p v3: armed"
#elif defined(Z72_RAW_RECORD_720P)
#define ARM_LABEL "RAW CFB 720p v2: arm"
#define OPENING_LABEL "RAW CFB 720p v2: opening"
#define ARMED_LABEL "RAW CFB 720p v2: armed"
#elif defined(Z72_RAW_RECORD_CFB)
#define ARM_LABEL "RAW CFB v12: arm"
#define OPENING_LABEL "RAW CFB v12: opening"
#define ARMED_LABEL "RAW CFB v12: armed"
#else
#define ARM_LABEL "RAW record v11: arm"
#define OPENING_LABEL "RAW record v11: opening"
#define ARMED_LABEL "RAW record v11: armed"
#endif

#ifdef Z72_RAW_RECORD_CFB
#define RECORD_CHUNK 0x40000u
#ifdef Z72_RAW_DUAL_25P
#define RECORD_PATH "A:\\Z72DM000.ZRV"
#elif defined(Z72_RAW_ZRV_ONLY)
#define RECORD_PATH "A:\\Z72DZ000.ZRV"
#elif defined(Z72_RAW_PHOTO_FILE)
#define RECORD_PATH "A:\\Z72DP000.ZRV"
#elif defined(Z72_RAW_DMA_STREAM_25P)
#define RECORD_PATH "A:\\Z72DF000.ZRV"
#elif defined(Z72_RAW_DMA_STREAM)
#define RECORD_PATH "A:\\Z72DS000.ZRV"
#elif defined(Z72_RAW_COPY_DMA)
#define RECORD_PATH "A:\\Z72RD000.ZRV"
#elif defined(Z72_RAW_RECORD_ROLLING)
#define RECORD_PATH "A:\\Z72RR000.ZRV"
#elif defined(Z72_RAW_RECORD_DEFERRED)
#define RECORD_PATH "A:\\Z72RB000.ZRV"
#elif defined(Z72_RAW_RECORD_PACKED)
#define RECORD_PATH "A:\\Z72RL000.ZRV"
#else
#define RECORD_PATH "A:\\Z72RS000.ZRV"
#endif
#define OPEN_FAILED_LABEL "RAW record: CFB open failed"
#else
#define RECORD_CHUNK 0x20000u
#define RECORD_PATH "B:\\Z72RS000.ZRV"
#define OPEN_FAILED_LABEL "RAW record: SD open failed"
#endif

#ifdef Z72_RAW_AUTO_25P
/* Direct filesystem calls accept a whole frame without a staging slot. */
#define FILE_WRITE_CHUNK (64u+5744u*2158u)
#elif defined(Z72_RAW_DMA_STREAM_25P)
/* Native op 0x0D stages up to 0x400000 payload bytes at a time. */
#define FILE_WRITE_CHUNK 0x400000u
#elif defined(Z72_RAW_DMA_STREAM)
#define FILE_WRITE_CHUNK 0x100000u
#else
#define FILE_WRITE_CHUNK RECORD_CHUNK
#endif

#ifdef Z72_RAW_PHOTO_FILE
#define FILE_WRITE_ENTRY 0x105bee53u
#else
#define FILE_WRITE_ENTRY 0x105bea79u
#endif

enum {
    IDLE, OPENING, ARMED, FINISHED,
#ifdef Z72_RAW_AUTO_25P
    WAIT_PROFILE,
#endif
#ifdef Z72_RAW_FULL_REFERENCE
    LINEAR_WIDTH=2070, LINEAR_HEIGHT=1166, LINEAR_X=0, LINEAR_Y=0, LINEAR_STRIDE=3120,
#else
    LINEAR_WIDTH=640, LINEAR_HEIGHT=360, LINEAR_X=712, LINEAR_Y=402, LINEAR_STRIDE=960,
#endif
#ifdef Z72_RAW_REFERENCE_SERIES
    LINEAR_LIMIT=4, LINEAR_FORMAT=4, LINEAR_STEP=8,
#elif defined(Z72_RAW_FULL_REFERENCE)
    LINEAR_LIMIT=1, LINEAR_FORMAT=2, LINEAR_STEP=1,
#elif defined(Z72_RAW_REFERENCE_PROFILE)
    LINEAR_LIMIT=25, LINEAR_FORMAT=2, LINEAR_STEP=1,
#endif
#ifdef Z72_RAW_RECORD_FULL_CFB
    WIDTH=RAW_WIDTH, HEIGHT=RAW_HEIGHT, CROP_X=0, CROP_Y=0, STRIDE=RAW_STRIDE, SOURCE_STRIDE=RAW_STRIDE,
#ifdef Z72_RAW_RECORD_DEFERRED
    FRAME_STEP=1, FRAME_LIMIT=3, FORMAT_VERSION=2,
#elif defined(Z72_RAW_DIRECT_60S)
    FRAME_STEP=1, FRAME_LIMIT=1500, FORMAT_VERSION=2,
#elif defined(Z72_RAW_DIRECT_30S)
    FRAME_STEP=1, FRAME_LIMIT=750, FORMAT_VERSION=2,
#elif defined(Z72_RAW_DMA_STREAM_25P)
    FRAME_STEP=1, FRAME_LIMIT=150, FORMAT_VERSION=2,
#else
#ifdef Z72_RAW_RECORD_FULL_CFB_12
    FRAME_STEP=2,
#else
    FRAME_STEP=3,
#endif
    FRAME_LIMIT=300/FRAME_STEP,
#ifdef Z72_RAW_RECORD_PACKED_BYTE
    FORMAT_VERSION=6,
#elif defined(Z72_RAW_RECORD_PACKED)
    FORMAT_VERSION=5,
#else
    FORMAT_VERSION=4,
#endif
#endif
    QUEUE_SIZE=3, CODE_BITS=12, CFA=0x02010100u,
#elif defined(Z72_RAW_RECORD_STANDBY)
    WIDTH=RAW_WIDTH, HEIGHT=RAW_HEIGHT, CROP_X=0, CROP_Y=0, STRIDE=1038, SOURCE_STRIDE=STRIDE,
    FRAME_LIMIT=125, FRAME_STEP=1, QUEUE_SIZE=4, FORMAT_VERSION=3, CODE_BITS=6, CFA=0,
    STANDBY_SETTLE_MS=1500u, STANDBY_TIMEOUT_MS=8000u,
#else
#ifdef Z72_RAW_RECORD_720P
    WIDTH=1280, HEIGHT=720, CROP_X=392, CROP_Y=222,
    FRAME_LIMIT=250,
#ifdef Z72_RAW_RECORD_HEAP
    QUEUE_SIZE=8,
#else
    QUEUE_SIZE=3,
#endif
#else
    WIDTH=640, HEIGHT=360, CROP_X=712, CROP_Y=402,
    FRAME_LIMIT=1500, QUEUE_SIZE=10,
#endif
    STRIDE=WIDTH*3/2, SOURCE_STRIDE=RAW_STRIDE, FORMAT_VERSION=2,
    FRAME_STEP=1, CODE_BITS=12, CFA=0x02010100u,
#endif
    PIXEL_BYTES=STRIDE*HEIGHT, PACKET_BYTES=64+PIXEL_BYTES,
    FIRST_BYTE=CROP_Y*RAW_STRIDE+CROP_X*3/2,
    COPY_ROWS=16, CHUNK=RECORD_CHUNK,
    BLACK_LEVEL=252, BLACK_LEVEL_DARK_REFERENCE=1,
    MOVIE_PHASE=0xb1811788u, RECORDER_PHASE=0xb181176cu,
#ifdef Z72_RAW_RECORD_CFB
    MOVIE_PROGRESS_MS=0xb1f58f44u, START_DELAY_MS=2000u,
    MEDIA_TABLE_SLOT=0xb18105bcu, MEDIA_TABLE_AB=0x108abb28u,
#endif
    STOP_TAIL_MS=120, SOURCE_IDLE_MS=1000,
    FILE_OK=0x043a0080u, FILE_MISSING=0x043a0084u
};
#ifdef Z72_RAW_DMA_STREAM_25P
_Static_assert(PACKET_BYTES<=FILE_WRITE_CHUNK,"Full-rate RAW uses one write request per frame");
#endif
#ifndef Z72_RAW_RECORD_STANDBY
/* The native row copy at 0x10360F44 uses aligned LDM and STM. */
_Static_assert(((FIRST_BYTE|SOURCE_STRIDE|STRIDE|PACKET_BYTES)&3u)==0,
               "Native RAW row copy requires word-aligned addresses");
#endif
enum {
    COMPLETE=1, USER_STOP, SEQUENCE_GAP, COPY_FAILED, QUEUE_FULL, WRITE_FAILED,
    NATIVE_STOP, SOURCE_STALLED
};

struct packet {
#if defined(Z72_RAW_RECORD_STANDBY) || defined(Z72_RAW_RECORD_HEAP)
    _Alignas(64)
#endif
    u32 magic;
    u32 header_bytes, payload_bytes, ordinal, address, start_tick, copy_ms, tail_token;
    u32 reserved[8];
#ifdef Z72_RAW_AUTO_25P
    unsigned char pixels[];
#elif !defined(Z72_RAW_FULL_REFERENCE)
    unsigned char pixels[PIXEL_BYTES];
#endif
};
#ifdef Z72_RAW_MIXED_FRAMES
enum { MIXED_NEF=1u,MIXED_RAW12=2u };
#ifdef Z72_RAW_DROP_EXPIRED
enum { MIXED_VERSION=12u,MIXED_DROPPED=3u };
#else
enum { MIXED_VERSION=11u };
#endif
_Static_assert(__builtin_offsetof(struct packet,reserved[7])==60u,"ZRV11 frame encoding offset");
#endif
#ifdef Z72_RAW_FULL_REFERENCE
struct standby_packet {
    _Alignas(64) struct packet header;
    unsigned char pixels[PIXEL_BYTES];
};
struct linear_packet {
    _Alignas(64) struct packet header;
    unsigned char pixels[LINEAR_STRIDE*LINEAR_HEIGHT];
};
_Static_assert(sizeof(struct packet)==64,"ZRV packet header size");
_Static_assert(QUEUE_SIZE*sizeof(struct standby_packet)<=sizeof(struct linear_packet),"Shared reference storage size");
static union {
    struct standby_packet standby[QUEUE_SIZE];
    struct linear_packet linear;
} reference_frames;
#else
_Static_assert(__builtin_offsetof(struct packet,pixels)==64,"ZRV packet header size");
#ifdef Z72_RAW_RECORD_HEAP
_Static_assert(sizeof(struct packet)%64u==0,"RAW heap packets must occupy whole cache lines");
#ifdef Z72_RAW_FRAME_POOLS
static void *frame_owners[QUEUE_SIZE];
static struct packet *frame_slots[QUEUE_SIZE];
static u32 frame_capacity;
#else
static void *queue_owner;
static struct packet *queue;
#endif
#else
static _Alignas(64) struct packet queue[QUEUE_SIZE];
#endif
#endif
#ifdef Z72_RAW_AUTO_25P
static _Alignas(64) u32 header[]={
#else
static _Alignas(64) const u32 header[]={
#endif
    0x3153525au, FORMAT_VERSION, Z72_TARGET, 64, WIDTH, HEIGHT, STRIDE, CODE_BITS,
    25, FRAME_STEP, CFA, RAW_WIDTH, RAW_HEIGHT, CROP_X, CROP_Y, FRAME_LIMIT
};
#ifdef Z72_RAW_REFERENCE_PROFILE
static _Alignas(64) const u32 linear_header[]={
    0x3153525au, LINEAR_FORMAT, Z72_TARGET, 64, LINEAR_WIDTH, LINEAR_HEIGHT, LINEAR_STRIDE, 12,
    25, LINEAR_STEP, 0x02010100u, 2070, 1166, LINEAR_X, LINEAR_Y, LINEAR_LIMIT
};
#endif
struct write_sample { u32 start_tick, elapsed_ms, queued; };
static _Alignas(64) struct write_sample writes[FRAME_LIMIT];
#ifdef Z72_RAW_NEF_BURST_LIMIT
_Static_assert(Z72_RAW_NEF_BURST_LIMIT<=FRAME_LIMIT,"UHD frame count exceeds write timing capacity");
#endif
static _Alignas(64) u32 timing_header[16]={
    0x4f49545au,64,sizeof(struct write_sample),0,
#ifdef Z72_RAW_RECORD_ROLLING
    3,[11]=QUEUE_SIZE,
#elif defined(Z72_RAW_RECORD_HEAP)
    2,[11]=QUEUE_SIZE,
#else
    1,
#endif
};
static _Alignas(64) struct stream_footer {
    u32 magic, bytes, reason, produced, written, attempts, tail_count, peak_queue;
    u32 copy_result, copy_ms, ordinal, write_ms, max_write_ms, observed_token, live_index, live_address;
} footer;

struct worker_task { u32 control[8], descriptor[7]; int started; };
static struct worker_task capture_task, card_task;
#ifdef Z72_RAW_PREFETCH_NEF
static struct worker_task prefetch_task;
static int start_worker(struct worker_task *task,void (*entry)(void *),u32 core_mask);
static void *prefetched_owner;
static u32 prefetched_address;
#endif
#ifdef Z72_RAW_STAGED_NEF_LIVE
static struct worker_task encoder_task;
static void *stage_owners[2];
#define STAGED_NEF_LIVE 1
#else
#define STAGED_NEF_LIVE 0
#endif
#ifdef Z72_RAW_RECORD_PACKED_BYTE
static struct worker_task pack_task;
static u32 packed;
#endif
#ifndef Z72_RAW_NATIVE_MENU
static struct z72_menu_storage menu;
#endif
static u32 state, stop_requested, card_error, produced, consumed, tail_token;
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
static u32 nef_previous_tail;
#ifdef Z72_RAW_TERMINAL_ROW
static struct { u32 token,ms; } first_tails[RAW_SLOT_COUNT];
#endif
#endif
static u32 last_attempt, last_ordinal, last_frame_tick, next_file;
static int movie_started;
#ifdef Z72_RAW_RECORD_ROLLING
static u32 first_saved;
#endif
#ifdef Z72_RAW_RECHECK_SLOT
static u32 snapshot_retries;
#endif
#ifdef Z72_RAW_RECOVER_NEXT_FRAME
static u32 consecutive_rejections, recovered_gaps;
#endif
#ifdef Z72_RAW_COPY_DMA
/* Nonzero while the driver may still be writing a frame buffer. */
static u32 dma_pending_channel;
#endif
#ifdef Z72_RAW_PAIR_AUTO
static u32 auto_pair;
#endif
#ifdef Z72_RAW_RECORD_STANDBY
static u32 standby_start;
#endif
static int file=-1;
#ifdef Z72_RAW_PHOTO_FILE_EXACT_SIZE
#if defined(Z72_RAW_DIRECT_60S) || defined(Z72_RAW_NEF_60S) || defined(Z72_RAW_WIDE_RECORD_TOTALS)
static unsigned long long file_bytes;
#else
static u32 file_bytes;
#endif
#endif

static u32 load(const u32 *word) { return __atomic_load_n(word,__ATOMIC_ACQUIRE); }
static void publish(u32 *word,u32 value) { __atomic_store_n(word,value,__ATOMIC_RELEASE); }

#ifdef Z72_RAW_EARLY_DUAL_START
static int raw_start_ready(u32 progress)
{
    if (progress>=START_DELAY_MS) return 1;
    const u32 settings=load(&raw_settings);
    return progress && (settings&0x15u)==4u && (settings>>6)!=1u &&
           BYTE(CAPTURE_TYPE)==z72_uhd.type && !WORD(PENDING_STOP) &&
           WORD(MEDIA_TABLE_SLOT)==MEDIA_TABLE_AB &&
           WORD(0xb2982160u)==1u && BYTE(0xb2982178u)==1u;
}
#else
#define raw_start_ready(progress) ((progress)>=START_DELAY_MS)
#endif

#ifdef Z72_RAW_AUTO_25P
#define CLIP_VALUE(field,ordinary) (z72_mode->field)
#ifdef Z72_RAW_PCM_AUTO_RATE
#define PCM_STREAM_MODE 1
#else
#define PCM_STREAM_MODE (z72_mode==&z72_uhd)
#endif
#ifdef Z72_RAW_PACKED_UHD_CFB
static u32 packed_uhd;
#else
#define packed_uhd 0
#endif
#ifdef Z72_RAW_STAGED_NEF_TEST
static void *stage_owner;
static u32 stage_address;
#define STAGED_NEF_TEST 1
#else
#define STAGED_NEF_TEST 0
#endif
#define STAGED_NEF_INPUT (STAGED_NEF_TEST || STAGED_NEF_LIVE)
static u32 clip_pixel_bytes(void) { return z72_mode->stride*z72_mode->height; }
static u32 packet_span(void)
{
#ifdef Z72_RAW_PCM_AUTO_RATE
    return (64u+clip_pixel_bytes()+4095u)&~4095u;
#else
    return (64u+clip_pixel_bytes()+63u)&~63u;
#endif
}
static const char *prepare_error;
#else
#define CLIP_VALUE(field,ordinary) (ordinary)
static inline u32 clip_pixel_bytes(void) { return PIXEL_BYTES; }
#endif

#if defined(Z72_RAW_ZRV_ONLY) || defined(Z72_RAW_SELECT_OUTPUT)
static int waiting_for_rec(u32 phase)
{
#ifdef Z72_RAW_AUTO_25P
    return phase==WAIT_PROFILE || phase==ARMED;
#else
    return phase==ARMED;
#endif
}
#endif

#ifdef Z72_RAW_SELECT_OUTPUT
#define HALF(a) (*(const volatile unsigned short *)(a))
enum { PRIMARY=0x20069800u, PREVIEW_CONFIG=CONFIG };
#include "movie_raw_mode_profile.h"
static u32 profile_busy;
static char clip_path[]=RECORD_PATH;
/* Row 0 in the recorder denotes the active option, row 1 the status / stop action. */
static void record_label(u32 row,const char *label)
{
#ifdef Z72_RAW_NATIVE_MENU
    (void)row; (void)label;
#else
    z72_menu_set_label(row==0 ? RECORD_ONLY() : 2u,label);
#endif
}
#else
#define record_label z72_menu_set_label
#define RECORD_ONLY() 1u
#endif

#ifdef Z72_RAW_ZRV_ONLY
/* Use the same native pose as the MOV display matrix. TIFF 8 and 6
 * rotate the stored raster counterclockwise and clockwise respectively. */
static u32 raw_clip_orientation;

static u32 raw_latch_orientation(void)
{
    u32 orientation=load(&raw_clip_orientation);
    if (orientation) return orientation;
    static const u32 tags[]={1,8,6,3};
    const u32 pose=CALL(0x103d4097u,u32,void)();
    const u32 next=pose<4u ? tags[pose] : tags[0];
    return __atomic_compare_exchange_n(&raw_clip_orientation,&orientation,next,0,
                                       __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE) ? next : orientation;
}
#include "movie_raw_mov_sink.h"
#endif
#ifdef Z72_RAW_NEF_QUIET_STOP
enum { NEF_STOPPING=14 };

static int nef_capture_stopping(void)
{
    const u32 encoder=BYTE(RECORDER_PHASE);
    return BYTE(MOVIE_PHASE)!=1u || (encoder!=1u && encoder!=2u) ||
           WORD(PENDING_STOP) || load(&stop_requested) ||
           (RECORD_ONLY() && load(&mov_sink_state)!=MOV_SINK_ACTIVE);
}
#endif
static u32 tick(void) { return CALL(0x1062e59du,u32,void)(); }

static int recording_standby(void)
{
#ifdef Z72_RAW_REFERENCE_PROFILE
    return !load(&z72_raw_reference_linear);
#elif defined(Z72_RAW_RECORD_STANDBY)
    return 1;
#else
    return 0;
#endif
}

static struct packet *queued_packet(u32 index)
{
#ifdef Z72_RAW_FRAME_POOLS
    u32 slot=frame_capacity==3u ? index%3u : frame_capacity==2u ? index%2u : 0u;
    return frame_slots[slot];
#elif defined(Z72_RAW_AUTO_25P)
    return (struct packet *)((unsigned char *)queue+index%QUEUE_SIZE*packet_span());
#elif defined(Z72_RAW_FULL_REFERENCE)
    return recording_standby() ? &reference_frames.standby[index%QUEUE_SIZE].header :
                                &reference_frames.linear.header;
#else
    return &queue[index%QUEUE_SIZE];
#endif
}

#ifdef Z72_RAW_ALIGNED_WRITE
enum { WRITE_ALIGNMENT=4096 };
/* Keep the producer's writable metadata on separate cache lines. Place the
 * serialized packet at the same page offset it occupies in the ZRV file. */
static unsigned char *stream_packet(struct packet *packet,u32 index)
{
#ifdef Z72_RAW_NEF_STREAM
    const u32 offset=packet->reserved[5];
    (void)index;
#else
    u32 offset=sizeof(header)+index*(64u+clip_pixel_bytes());
#endif
    return (unsigned char *)packet+WRITE_ALIGNMENT+(offset&(WRITE_ALIGNMENT-1u));
}
#endif

static unsigned char *packet_pixels(struct packet *packet)
{
#ifdef Z72_RAW_ALIGNED_WRITE
    if (z72_mode==&z72_uhd) return stream_packet(packet,load(&produced))+64u;
#endif
#ifdef Z72_RAW_FULL_REFERENCE
    return recording_standby() ? ((struct standby_packet *)packet)->pixels :
                                ((struct linear_packet *)packet)->pixels;
#else
    return packet->pixels;
#endif
}

#ifdef Z72_RAW_REFERENCE_PROFILE
static const u32 *recording_header(void)
{
    return recording_standby() ? header : linear_header;
}
#define REFERENCE_VALUE(ordinary,linear) (recording_standby() ? (ordinary) : (linear))
#else
#define REFERENCE_VALUE(ordinary,linear) (ordinary)
#endif

#ifdef Z72_RAW_MOV_SYNC
#include "movie_raw_mov_sync.h"
#endif
#ifdef Z72_RAW_HD_LIVE_CAPTURE
#include "movie_raw_hd_live.h"
#endif

static u32 raw_irq(u32 a,u32 b,u32 c,u32 d)
{
#ifdef Z72_RAW_AUTO_25P
    const u32 phase=load(&state);
#ifdef Z72_RAW_NEF_QUIET_STOP
    if (phase==ARMED && z72_mode==&z72_uhd && nef_capture_stopping())
        return CALL(RAW_IRQ_NATIVE,u32,u32,u32,u32,u32)(a,b,c,d);
#endif
#ifdef Z72_RAW_MOV_SYNC
    const int tracking=phase==ARMED && load(&mov_sync.enabled);
    if (tracking) publish(&mov_sync.irq,1);
#endif
    if (waiting_for_rec(phase) && (WORD(RAW_IRQ)&WORD(RAW_MASK)&0x40u)) {
#else
    if (load(&state)==ARMED && (WORD(RAW_IRQ)&WORD(RAW_MASK)&0x40u)) {
#endif
#ifdef Z72_RAW_SELECT_OUTPUT
        if (!__atomic_exchange_n(&profile_busy,1u,__ATOMIC_ACQUIRE)) {
            if (waiting_for_rec(load(&state))) record_tail(WORD(RAW_IRQ),WORD(RAW_IRQ+0x20u),WORD(RAW_MASK));
            publish(&profile_busy,0);
        }
#endif
#ifdef Z72_RAW_AUTO_25P
        if (phase==ARMED)
#endif
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
        {
            const u32 previous=load(&tail_token);
            const u32 next=z72_raw_next_tail(previous,z72_raw_completed_slot());
#ifdef Z72_RAW_HD_LIVE_CAPTURE
            if (hd_live_capture())
                __atomic_store_n(&hd_tails[(next>>RAW_TOKEN_SHIFT)%RAW_SLOT_COUNT],
                    ((unsigned long long)next<<32)|tick(),__ATOMIC_RELEASE);
#endif
#ifdef Z72_RAW_MOV_SYNC
            if (tracking) publish(&mov_sync.tails[(next>>RAW_TOKEN_SHIFT)%SYNC_SLOTS],next);
#endif
            if (z72_mode==&z72_uhd) publish(&nef_previous_tail,previous);
#ifdef Z72_RAW_TERMINAL_ROW
            if (z72_mode==&z72_uhd && z72_clip_fps_num==30000u && !load(&produced)) {
                const u32 i=(next>>RAW_TOKEN_SHIFT)%RAW_SLOT_COUNT;
                publish(&first_tails[i].token,0);
                publish(&first_tails[i].ms,tick());
                publish(&first_tails[i].token,next);
            }
#endif
            publish(&tail_token,next);
        }
#else
            publish(&tail_token,z72_raw_next_tail(load(&tail_token),z72_raw_completed_slot()));
#endif
    }
#ifdef Z72_RAW_MOV_SYNC
    const u32 result=CALL(RAW_IRQ_NATIVE,u32,u32,u32,u32,u32)(a,b,c,d);
    if (tracking) publish(&mov_sync.irq,0);
    return result;
#else
    return CALL(RAW_IRQ_NATIVE,u32,u32,u32,u32,u32)(a,b,c,d);
#endif
}

static int sample_matches(const unsigned char *pixels,u32 source,u32 row)
{
    const u32 stride=CLIP_VALUE(stride,REFERENCE_VALUE(STRIDE,LINEAR_STRIDE));
    const u32 addr=source+row*CLIP_VALUE(stride,REFERENCE_VALUE(SOURCE_STRIDE,3120u))+stride-64u;
    const unsigned char *copy=pixels+row*stride+stride-64u;
    CALL(0x1034fa29u,u32,u32,u32)(addr,64);
#ifdef Z72_RAW_DMA_FAST_STREAM
    CALL(0x1034fa29u,u32,u32,u32)((u32)copy,64);
#endif
    for (u32 i=0;i<64u;++i)
        if (copy[i]!=BYTE(addr+i)) return 0;
    return 1;
}

#ifdef Z72_RAW_COPY_DMA
static void invalidate_dma_pixels(u32 address,u32 bytes)
{
    while (bytes) {
        const u32 chunk=bytes<CHUNK ? bytes : CHUNK;
        CALL(0x1034fa29u,u32,u32,u32)(address,chunk);
        address+=chunk;
        bytes-=chunk;
    }
}
#endif

#ifdef Z72_RAW_SPLIT_DMA
/* Only the capture worker waits on or releases its channels. Publish the
 * remaining mask after recording completion, so the writer can drain safely. */
static u32 dma_start_tick;
static u32 wait_frame_dma(u32 start,u32 budget)
{
    u32 pending=load(&dma_pending_channel),result=RAW_COPY_OK;
    for (u32 i=0;i<RAW_DMA_CHANNELS;++i) {
        if (!(pending&(1u<<i))) continue;
        u32 elapsed=tick()-start;
        u32 event=z72_raw_dma_wait(i,elapsed<budget ? budget-elapsed : 0);
        if (event==RAW_DMA_PENDING) continue;
        pending&=~(1u<<i);
        if (event!=RAW_DMA_DONE) result=RAW_DMA_ERROR;
    }
    if (!pending) log.completion_ms=tick()-dma_start_tick;
    publish(&dma_pending_channel,pending);
    return pending ? RAW_COPY_TIMEOUT : result;
}

static u32 transfer_frame(u32 first,u32 dest,u32 source,u32 bytes,u32 start)
{
    u32 second=RAW_DMA_CHANNELS,first_bytes=bytes;
    if (z72_mode==&z72_uhd) second=z72_raw_dma_acquire();
    if (tick()-start>=RAW_COPY_MS) {
        if (second!=RAW_DMA_CHANNELS) z72_raw_dma_release(second);
        z72_raw_dma_release(first);
        return RAW_COPY_TIMEOUT;
    }
    u32 mask=1u<<first;
    if (second!=RAW_DMA_CHANNELS) {
        first_bytes=(bytes/2u)&~63u;
        mask|=1u<<second;
        ++log.split_transfers;
    } else ++log.single_transfers;
    log.last_channels=mask;
    log.late_channels=0;
    log.completion_ms=0;
    dma_start_tick=tick();
    publish(&dma_pending_channel,mask);
    z72_raw_dma_start(first,dest,source,first_bytes);
    if (second!=RAW_DMA_CHANNELS)
        z72_raw_dma_start(second,dest+first_bytes,source+first_bytes,bytes-first_bytes);
    u32 result=wait_frame_dma(start,RAW_COPY_MS);
    log.late_channels=load(&dma_pending_channel);
    return result;
}
#endif

#ifdef Z72_RAW_STREAM_SLOT_RECHECK
static u32 stream_copy_status(u32 token,u32 start,struct z72_raw_status *status,int *can_recheck)
{
    const u32 result=z72_raw_copy_status(&tail_token,token,start,status);
    if (result!=RAW_UNSAFE_SLOT || !*can_recheck || status->observed_token!=token) return result;

    /* Only resample a possible transition between the two safe live slots. */
    const u32 anchor=(token&RAW_TOKEN_SLOT)-1u, next=(anchor+1u)%RAW_SLOT_COUNT;
    if ((status->live_index!=anchor && status->live_index!=next) ||
        (status->live_address!=z72_raw_slot_address(anchor) &&
         status->live_address!=z72_raw_slot_address(next))) return result;

    *can_recheck=0;
    ++timing_header[13];
    CALL(0x103dc539u,void,u32)(1);
    const u32 again=z72_raw_copy_status(&tail_token,token,start,status);
    if (again==RAW_COPY_OK) ++timing_header[14];
    return again;
}
#define LEGACY_COPY_STATUS() stream_copy_status(token,packet->start_tick,status,&can_recheck)
#else
#define LEGACY_COPY_STATUS() z72_raw_copy_status(&tail_token,token,packet->start_tick,status)
#endif
#ifdef Z72_RAW_HD_LIVE_CAPTURE
#define COPY_STATUS() (hd_live_capture() ? hd_copy_status(token,status) : LEGACY_COPY_STATUS())
#else
#define COPY_STATUS() LEGACY_COPY_STATUS()
#endif

#ifdef Z72_RAW_RASTER_COPY
#include "movie_raw_raster_copy.h"
#endif
#ifdef Z72_RAW_NEF_STREAM
static void clean_bytes(const void *data,u32 bytes);
#include "movie_raw_nef_stream.h"
#endif

#ifdef Z72_RAW_PACKED_RASTER_BOUNDARY
/* A completed copy may reach the bank just before the source. Require
 * one observed, adjacent IRQ and a stable in-bank cursor for that case. */
static int completed_raster_boundary(u32 result,u32 token,const struct z72_raw_status *status)
{
    return result==RAW_UNSAFE_SLOT && (token&RAW_TOKEN_PAIR) &&
           status->observed_token==z72_raw_next_tail(token,(token&RAW_TOKEN_SLOT)%RAW_SLOT_COUNT+1u) &&
           load(&nef_previous_tail)==token && nef_before_wrap(token,status);
}

static u32 check_raster_copy(const struct packet *packet,u32 token,struct z72_raw_status *status)
{
#ifdef Z72_RAW_STAGED_NEF_LIVE
    const u32 result=nef_source_status(token,packet->start_tick,status,RAW_COPY_MS);
#else
    const u32 result=z72_raw_copy_status(&tail_token,token,packet->start_tick,status);
#endif
    log.raster_boundary.raw_result=result;
    log.raster_boundary.cursor=status->live_tail;
    return result;
}

static u32 verify_packed_raster(const struct packet *packet,u32 token,struct z72_raw_status *status,
                              const unsigned char *pixels)
{
    const u32 before=check_raster_copy(packet,token,status);
    if (before!=RAW_COPY_OK && !completed_raster_boundary(before,token,status)) return before;
    const u32 height=z72_mode->height,source=packet->address;
    if (!sample_matches(pixels,source,0) || !sample_matches(pixels,source,height/2u) ||
        !sample_matches(pixels,source,height-1u)) return RAW_SAMPLE_CHANGED;
    const u32 after=check_raster_copy(packet,token,status);
    if (after==RAW_COPY_OK) return before==RAW_COPY_OK ? RAW_COPY_OK : RAW_UNSAFE_SLOT;
    if (!completed_raster_boundary(after,token,status)) return after;
    ++log.raster_boundary.accepted;
    return RAW_COPY_OK;
}
#endif

#if defined(Z72_RAW_STAGED_NEF_TEST) || defined(Z72_RAW_STAGED_NEF_LIVE)
static u32 copy_staged_raster(const struct packet *packet,u32 token,struct z72_raw_status *status,u32 dest)
{
    const u32 source=packet->address;
    if (!z72_raw_identity_span(source,clip_pixel_bytes())) return RAW_SOURCE_MAPPING;
    u32 result=check_raster_copy(packet,token,status);
    if (result!=RAW_COPY_OK) return result;
    const u32 stride=z72_mode->stride,height=z72_mode->height;
    const int sliced=BYTE(RASTER_MODE)==1;
    const u32 band=sliced ? 0x40000u/5744u : height;
    for (u32 row=0;row<height;row+=band) {
        const u32 rows=height-row<band ? height-row : band;
        const u32 offset=row*stride;
        result=raster_copy_rows(dest+offset,source+offset,stride,rows,packet->start_tick,sliced);
        if (result!=RAW_COPY_OK) return result;
        if (row+rows<height) {
            result=check_raster_copy(packet,token,status);
            if (result!=RAW_COPY_OK) return result;
        }
    }
    return verify_packed_raster(packet,token,status,(const unsigned char *)dest);
}
#endif

#ifdef Z72_RAW_STAGED_NEF_TEST
#include "movie_raw_staged_nef.h"
#endif
#ifdef Z72_RAW_PREFETCH_NEF
#include "movie_raw_prefetch.h"
enum { PREFETCH_AWAIT_REAL_TAIL=0x101u };

static void reserve_prefetch_buffer(void)
{
    const u32 id=WORD(0xb1f7cbacu),bytes=clip_pixel_bytes();
    u32 info[4];
#ifdef Z72_RAW_MIXED_FRAMES
    const u32 used=64u+bytes,span=(used+WRITE_ALIGNMENT-1u)&~(WRITE_ALIGNMENT-1u);
    const u32 request=span+WRITE_ALIGNMENT-1u;
    if ((int)id<=0 || CALL(0x10104788u,int,u32,u32 *)(id,info) || info[1] ||
        info[3]<request || info[2]<request+0x80000u) return;
    void *owner=CALL(0x1034fe31u,void *,u32,u32)(request,0u);
    if (!owner) return;
    const u32 page=((u32)owner+WRITE_ALIGNMENT-1u)&~(WRITE_ALIGNMENT-1u),addr=page+64u;
    if (!z72_raw_identity_span(page,span) || !start_worker(&prefetch_task,prefetch_worker,1u)) {
#else
    if ((int)id<=0 || CALL(0x10104788u,int,u32,u32 *)(id,info) || info[1] ||
        info[3]<bytes+63u || info[2]<bytes+63u+0x80000u) return;
    void *owner=CALL(0x1034fe31u,void *,u32,u32)(bytes+63u,0u);
    if (!owner) return;
    const u32 addr=((u32)owner+63u)&~63u;
    if (!z72_raw_identity_span(addr,bytes) || !start_worker(&prefetch_task,prefetch_worker,1u)) {
#endif
        CALL(0x1034fe7fu,void,void *)(owner);
        return;
    }
    invalidate_dma_pixels(addr,bytes);
#ifdef Z72_RAW_MIXED_FRAMES
    /* The raster overwrites only its exact pixel span. This includes the
     * padding in the final partial cache line before the first transfer. */
    CALL(0x10361035u,void,void *,u32)((void *)(page+used),span-used);
    clean_bytes((void *)(page+used),span-used);
#endif
    prefetched_owner=owner;
    publish(&prefetched_address,addr);
    prefetch_start();
}
#ifdef Z72_RAW_MIXED_FRAMES
static int take_mixed_frame(struct packet *packet,u32 token,struct z72_raw_status *status)
{
    u32 started;
    struct z72_raw_status copy_status;
    if (!prefetch_take_packed(token,&copy_status,&started)) return 0;
    const u32 seen=load(&tail_token);
    if (load(&card_error) || ((seen>>RAW_TOKEN_SHIFT)==(token>>RAW_TOKEN_SHIFT) && seen!=token)) {
        prefetch_release_packed(packet->ordinal);
        return 0;
    }
    packet->reserved[3]=packet->reserved[4]=packet->reserved[5]=0;
    packet->reserved[7]=MIXED_RAW12;
    packet->payload_bytes=clip_pixel_bytes();
    packet->start_tick=started;
    *status=copy_status;
    prefetch_accept(packet->ordinal);
    return 1;
}
#endif
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
static int live_source_near_reuse(u32 token,const struct packet *packet)
{
    struct z72_raw_status live={0};
    const u32 result=z72_raw_copy_status(&tail_token,token,packet->start_tick,&live);
    if (nef_late_bank(result,token,&live)) return 1;
    if (result!=RAW_WINDOW_EXPIRED && result!=RAW_UNSAFE_SLOT) return 0;

    /* Two further tails may arrive just as DMA begins overwriting the
     * source. The third consecutive bank position is the source's own tail. */
    const u32 seen=live.observed_token,anchor=token>>RAW_TOKEN_SHIFT;
    if (!(seen&RAW_TOKEN_PAIR) || (seen>>RAW_TOKEN_SHIFT)<anchor+2u) return 0;
    const u32 advance=(seen>>RAW_TOKEN_SHIFT)-anchor;
    const u32 expected=((token&RAW_TOKEN_SLOT)-1u+advance)%RAW_SLOT_COUNT+1u;
    if ((seen&RAW_TOKEN_SLOT)!=expected || load(&tail_token)!=seen) return 0;
    if (advance>=3u) return 1;
    return live.live_address==packet->address && live.live_index<RAW_SLOT_COUNT &&
           z72_raw_slot_address(live.live_index)==packet->address &&
           live.live_tail>=packet->address && live.live_tail<=packet->address+clip_pixel_bytes();
}
#endif
#ifdef Z72_RAW_PREFETCH_PARALLEL
static u32 encode_with_backup(struct packet *packet,u32 token,struct z72_raw_status *status)
{
    if (!load(&prefetch_enabled)) return nef_encode(packet,token,status,packet->address);

    const u32 ordinal=packet->ordinal,anchor=token>>RAW_TOKEN_SHIFT;
    const u32 actual=load(&tail_token);
    const int stopping=nef_capture_stopping();
    if (!stopping && (actual>>RAW_TOKEN_SHIFT)<anchor) return PREFETCH_AWAIT_REAL_TAIL;
    const int same_tail=(actual>>RAW_TOKEN_SHIFT)!=anchor || actual==token;
#ifdef Z72_RAW_MIXED_FRAMES
    if (header[1]==MIXED_VERSION && same_tail) {
        if (take_mixed_frame(packet,token,status)) return RAW_COPY_OK;
        if (load(&card_error)) return NEF_OUTPUT_BUSY;
    }
#endif
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    prefetch_claim_output_wait(token);
    if (!stopping && same_tail && prefetch_ready_output_wait(token) && live_source_near_reuse(token,packet)) {
        u32 copy_token,started;
        struct z72_raw_status verified;
        const u32 source=prefetch_take(ordinal,&verified,&copy_token,&started);
        if (source) {
            if (load(&card_error)) { prefetch_release(); return NEF_OUTPUT_BUSY; }
            const u32 seen=load(&tail_token);
            if (copy_token==token && ((seen>>RAW_TOKEN_SHIFT)!=anchor || seen==token)) {
                *status=verified;
                packet->start_tick=started;
                ++log.output_wait.preferred;
                const u32 result=nef_encode(packet,token,status,source);
                prefetch_release();
                if (result==RAW_COPY_OK) prefetch_accept(ordinal);
                return result;
            }
            prefetch_release();
        }
    }
#endif
    const int racing=!stopping && same_tail && prefetch_ready_token(ordinal)==token;
    if (racing) ++log.backup.racing;
    u32 result=stopping ? NEF_STOPPING : nef_encode(packet,token,status,packet->address);
    if (result==RAW_COPY_OK) {
        prefetch_accept(ordinal);
        if (racing) ++log.backup.live_saved;
        return result;
    }

    prefetch_reject(ordinal);
    if (!same_tail || !nef_backup_reason(result) || load(&nef_pending) || load(&card_error)) return result;
#ifdef Z72_RAW_MIXED_FRAMES
    if (header[1]==MIXED_VERSION) return take_mixed_frame(packet,token,status) ? RAW_COPY_OK : result;
#endif
    u32 copy_token,started;
    struct z72_raw_status copy_status;
    const u32 source=prefetch_take(ordinal,&copy_status,&copy_token,&started);
    if (!source) return result;
    const u32 observed=load(&tail_token);
    if (!load(&card_error) && copy_token==token && ((observed>>RAW_TOKEN_SHIFT)!=anchor || observed==token)) {
        ++log.backup.retries;
        log.backup.last_ordinal=ordinal;
        log.backup.last_reason=result;
        *status=copy_status;
        packet->start_tick=started;
        result=nef_encode(packet,copy_token,status,source);
    }
    prefetch_release();
    if (result==RAW_COPY_OK) {
        ++log.backup.recovered;
        prefetch_accept(ordinal);
    }
    return result;
}
#endif
#endif

static u32 copy_crop(struct packet *packet,u32 token,struct z72_raw_status *status)
{
#ifdef Z72_RAW_NEF_STREAM
    if (z72_mode==&z72_uhd && !packed_uhd) {
#ifdef Z72_RAW_STAGED_NEF_TEST
        u32 result=stage_nef_input(packet,token,status);
        if (result==RAW_COPY_OK) result=nef_encode(packet,token,status,stage_address);
#elif defined(Z72_RAW_PREFETCH_PARALLEL)
        const u32 result=encode_with_backup(packet,token,status);
#ifdef Z72_RAW_MIXED_FRAMES
        if (result==RAW_COPY_OK && header[1]==MIXED_VERSION && packet->reserved[7]!=MIXED_RAW12)
            packet->reserved[7]=MIXED_NEF;
#endif
#elif defined(Z72_RAW_PREFETCH_NEF)
        u32 source=prefetch_take(packet->ordinal,status,&token,&packet->start_tick);
        if (!source && nef_capture_stopping()) return NEF_STOPPING;
        if (!source && (load(&tail_token)>>RAW_TOKEN_SHIFT)<(token>>RAW_TOKEN_SHIFT))
            return PREFETCH_AWAIT_REAL_TAIL;
        u32 result=nef_encode(packet,token,status,source ? source : packet->address);
        if (source) prefetch_release();
#else
        const u32 result=nef_encode(packet,token,status,packet->address);
#endif
#ifdef Z72_RAW_NEF_EVENT_WAIT
        log.nef.last_error_ordinal=result==RAW_COPY_OK ? 0 : packet->ordinal;
        log.nef.last_error_result=result==RAW_COPY_OK ? 0 : result;
        if (result==RAW_COPY_OK) log.nef.last_error_stage=0;
#endif
        return result;
    }
#endif
    const u32 source=packet->address+REFERENCE_VALUE(FIRST_BYTE,LINEAR_Y*3120u+LINEAR_X*3u/2u);
    const u32 height=CLIP_VALUE(height,REFERENCE_VALUE(HEIGHT,LINEAR_HEIGHT));
#ifdef Z72_RAW_STREAM_SLOT_RECHECK
    int can_recheck=1;
#endif
#ifdef Z72_RAW_COPY_DMA
    const u32 dest=(u32)packet_pixels(packet);
    const u32 bytes=clip_pixel_bytes();
    if (!z72_raw_identity_span(source,bytes) || !z72_raw_identity_span(dest,bytes))
        return RAW_SOURCE_MAPPING;
#ifndef Z72_RAW_DMA_FAST_STREAM
    invalidate_dma_pixels(dest,bytes);
#endif
    const u32 initial=COPY_STATUS();
    if (initial!=RAW_COPY_OK) return initial;
#ifdef Z72_RAW_RASTER_COPY
    if (z72_mode==&z72_uhd
#ifdef Z72_RAW_HD_LIVE_CAPTURE
        || hd_live_capture()
#endif
    ) {
        const u32 stride=z72_mode->stride;
        const int sliced=BYTE(RASTER_MODE)==1;
        /* Same 256 KiB scheduling boundary as Nikon's 0x1038A112. */
        const u32 band=sliced ? (
#ifdef Z72_RAW_HD_LIVE_CAPTURE
            hd_live_capture() ? 0x40000u/RAW_STRIDE :
#endif
            0x40000u/5744u) : height;
        packet->reserved[3]=0;
        for (u32 row=0;row<height;row+=band) {
            u32 rows=height-row<band ? height-row : band;
            u32 result=raster_copy_rows(dest+row*stride,source+row*stride,stride,rows,
                                        packet->start_tick,sliced);
#ifdef Z72_RAW_HD_LIVE_CAPTURE
            if (!row && result==RAW_DMA_BUSY && hd_live_capture()) goto transfer_linear;
#endif
            if (result!=RAW_COPY_OK) return result;
#ifdef Z72_RAW_PACKED_RASTER_BOUNDARY
            if (packed_uhd && row+rows==height)
                return verify_packed_raster(packet,token,status,(const unsigned char *)dest);
#endif
            result=COPY_STATUS();
            if (result!=RAW_COPY_OK) return result;
        }
        goto verify_samples;
    }
#endif
#ifdef Z72_RAW_HD_LIVE_CAPTURE
transfer_linear:;
#endif
    u32 channel=z72_raw_dma_acquire();
#ifdef Z72_RAW_DMA_BUSY_RETRY
    /* The stock allocator yields for one tick before scanning again. Limit
     * the wait within this frame and retain the original copy deadline. */
    while (channel==RAW_DMA_CHANNELS && tick()-packet->start_tick<8u && packet->reserved[4]<8u) {
        CALL(0x103dc539u,void,u32)(1);
        ++packet->reserved[4];
        const u32 check=COPY_STATUS();
        if (check!=RAW_COPY_OK) return check;
        if (tick()-packet->start_tick>8u) break;
        channel=z72_raw_dma_acquire();
    }
#endif
    if (channel==RAW_DMA_CHANNELS) return RAW_DMA_BUSY;
#ifdef Z72_RAW_SPLIT_DMA
    u32 transfer=transfer_frame(channel,dest,source,bytes,packet->start_tick);
    if (transfer!=RAW_COPY_OK) return transfer;
#else
    const u32 elapsed=tick()-packet->start_tick;
    if (elapsed>=RAW_COPY_MS) {
        z72_raw_dma_release(channel);
        return RAW_COPY_TIMEOUT;
    }
    publish(&dma_pending_channel,channel+1u);
    z72_raw_dma_start(channel,dest,source,bytes);
    const u32 result=z72_raw_dma_wait(channel,RAW_COPY_MS-elapsed);
    if (result==RAW_DMA_PENDING) return RAW_COPY_TIMEOUT;
    publish(&dma_pending_channel,0);
    if (result!=RAW_DMA_DONE) return RAW_DMA_ERROR;
#endif
#ifndef Z72_RAW_DMA_FAST_STREAM
    invalidate_dma_pixels(dest,bytes);
#endif
#else
#if defined(Z72_RAW_RECORD_STANDBY) || defined(Z72_RAW_RECORD_FULL_CFB)
#if defined(Z72_RAW_FULL_REFERENCE) || defined(Z72_RAW_RECORD_FULL_CFB)
    {
        const u32 span=REFERENCE_VALUE(PIXEL_BYTES,LINEAR_STRIDE*LINEAR_HEIGHT);
#else
    if (recording_standby()) {
        const u32 span=PIXEL_BYTES;
#endif
        if (!z72_raw_identity_span(source,span)) return RAW_SOURCE_MAPPING;
        for (u32 offset=0;offset<span;offset+=CHUNK) {
            const u32 check=z72_raw_copy_status(&tail_token,token,packet->start_tick,status);
            if (check!=RAW_COPY_OK) return check;
            const u32 bytes=span-offset<CHUNK ? span-offset : CHUNK;
            CALL(0x1034fa29u,u32,u32,u32)(source+offset,bytes);
            CALL(0x10360f44u,void,void *,const void *,u32)(
                packet_pixels(packet)+offset,(const void *)(source+offset),bytes);
        }
    }
#if defined(Z72_RAW_REFERENCE_PROFILE) && !defined(Z72_RAW_FULL_REFERENCE)
    else
#endif
#endif
#if (!defined(Z72_RAW_RECORD_STANDBY) && !defined(Z72_RAW_RECORD_FULL_CFB)) || \
    (defined(Z72_RAW_REFERENCE_PROFILE) && !defined(Z72_RAW_FULL_REFERENCE))
    {
        const u32 input_stride=REFERENCE_VALUE(RAW_STRIDE,3120u);
        const u32 stride=REFERENCE_VALUE(STRIDE,LINEAR_STRIDE);
        if (!z72_raw_identity_span(source,(height-1u)*input_stride+stride)) return RAW_SOURCE_MAPPING;
        for (u32 start=0;start<height;start+=COPY_ROWS) {
            u32 check=z72_raw_copy_status(&tail_token,token,packet->start_tick,status);
            if (check!=RAW_COPY_OK) return check;
            u32 rows=height-start<COPY_ROWS ? height-start : COPY_ROWS;
            u32 address=source+start*input_stride;
            CALL(0x1034fa29u,u32,u32,u32)(address,(rows-1u)*input_stride+stride);
            for (u32 y=0;y<rows;++y)
                CALL(0x10360f44u,void,void *,const void *,u32)(
                    packet_pixels(packet)+(start+y)*stride,(const void *)(address+y*input_stride),stride);
        }
    }
#endif
#endif
#ifdef Z72_RAW_RASTER_COPY
verify_samples:;
#endif
    u32 check=COPY_STATUS();
    if (check!=RAW_COPY_OK) return check;
    const unsigned char *pixels=packet_pixels(packet);
    if (!sample_matches(pixels,source,0) || !sample_matches(pixels,source,height/2u) ||
        !sample_matches(pixels,source,height-1u)) return RAW_SAMPLE_CHANGED;
    return COPY_STATUS();
}
#undef COPY_STATUS
#undef LEGACY_COPY_STATUS

static void clean_bytes(const void *data,u32 bytes)
{
    const unsigned char *cursor=data;
    while (bytes) {
        u32 chunk=bytes<CHUNK ? bytes : CHUNK;
        CALL(0x1034f9edu,void,const void *,u32)(cursor,chunk);
        cursor+=chunk;
        bytes-=chunk;
    }
}

/* The producer alone changes the capture result and publishes FINISHED. */
static void finish(u32 reason)
{
    footer.reason=reason;
    footer.tail_count=load(&tail_token)>>RAW_TOKEN_SHIFT;
    /* Capture stop state before the writer drains the queue. */
    timing_header[5]=movie_started;
    timing_header[6]=BYTE(MOVIE_PHASE);
    timing_header[7]=BYTE(RECORDER_PHASE);
    timing_header[8]=BYTE(CAPTURE_TYPE);
    timing_header[9]=WORD(PENDING_STOP);
    timing_header[10]=load(&produced) ? tick()-last_frame_tick : 0;
#ifdef Z72_RAW_ZRV_ONLY
    timing_header[15]=load(&mov_discarded_bytes);
#endif
#ifdef Z72_RAW_RECORD_DEFERRED
#ifdef Z72_RAW_RECORD_ROLLING
    timing_header[14]=load(&produced);
    timing_header[15]=reason;
#else
    timing_header[12]=movie_started ? WORD(MOVIE_PROGRESS_MS) : 0;
#endif
#endif
#if defined(Z72_RAW_RECORD_DEFERRED) || defined(Z72_RAW_DMA_STREAM)
    u32 hook=(u32)raw_irq;
    __atomic_compare_exchange_n(&WORD(RAW_IRQ_SLOT),&hook,RAW_IRQ_NATIVE,0,
                                __ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
#endif
    publish(&state,FINISHED);
}

#if !defined(Z72_RAW_RECORD_STANDBY) || defined(Z72_RAW_REFERENCE_PROFILE)
static int native_movie_ended(void)
{
    const u32 phase=BYTE(MOVIE_PHASE);
    if (phase==1u) movie_started=1;
    /* 0x1048677A moves through 1, 2, 3, 0. The ordinary encoder's
     * callback enters phase 3 at 0x1048607E when it acknowledges stop. */
    return movie_started && (phase==0u || phase==2u || phase==3u || BYTE(RECORDER_PHASE)==3u);
}
#endif

#ifdef Z72_RAW_STAGED_NEF_LIVE
#include "movie_raw_staged_nef_live.h"
#endif

static void capture_step(void)
{
    if (STAGED_NEF_LIVE) {
#ifdef Z72_RAW_STAGED_NEF_LIVE
        staged_capture_step();
#endif
        return;
    }
    if (load(&state)!=ARMED) return;
    if (load(&card_error)) { finish(WRITE_FAILED); return; }
#ifdef Z72_RAW_PREFETCH_NEF
    /* A verified copy from this REC can outlive its sensor slot and the stop
     * IRQ. Only drain the frame immediately after the last published frame. */
    const u32 saved_anchor=load(&produced) ? prefetch_ready_token(last_ordinal+1u) : 0;
#else
    const u32 saved_anchor=0;
#endif
    if (load(&stop_requested) && !saved_anchor) { finish(USER_STOP); return; }

    int stopping=0;
    u32 count;
#ifdef Z72_RAW_RECORD_STANDBY
#ifdef Z72_RAW_REFERENCE_PROFILE
    if (recording_standby())
#endif
    {
        count=load(&produced);
        if (!z72_raw_movie_idle()) { finish(NATIVE_STOP); return; }
        const u32 elapsed=tick()-standby_start;
        if (elapsed<STANDBY_SETTLE_MS) return;
        if (!count && elapsed>=STANDBY_TIMEOUT_MS) { finish(SOURCE_STALLED); return; }
    }
#ifdef Z72_RAW_REFERENCE_PROFILE
    else
#endif
#endif
#if !defined(Z72_RAW_RECORD_STANDBY) || defined(Z72_RAW_REFERENCE_PROFILE)
    {
        stopping=native_movie_ended();
        count=load(&produced);
#ifdef Z72_RAW_NEF_QUIET_STOP
        if (z72_mode==&z72_uhd && !saved_anchor && nef_capture_stopping()) { finish(NATIVE_STOP); return; }
#endif
#ifdef Z72_RAW_RECORD_ROLLING
        if (movie_started) {
            const u32 progress=WORD(MOVIE_PROGRESS_MS);
            if (progress>timing_header[12]) timing_header[12]=progress;
        }
#endif
#if defined(Z72_RAW_RECORD_ROLLING) || defined(Z72_RAW_DMA_STREAM)
        if (stopping && !saved_anchor) { finish(NATIVE_STOP); return; }
#endif
#ifdef Z72_RAW_ZRV_ONLY
        const u32 sink=load(&mov_sink_state);
        if (RECORD_ONLY()) {
            if (sink==MOV_SINK_CLOSED) { finish(NATIVE_STOP); return; }
            if (sink!=MOV_SINK_ACTIVE) return;
        }
#endif
#ifdef Z72_RAW_PAIR_AUTO
        if (load(&auto_pair) && !movie_started && tick()-standby_start>=30000u) {
            finish(SOURCE_STALLED);
            return;
        }
#endif
#if defined(Z72_RAW_RECORD_CFB)
        if (!count) {
            if (stopping) { finish(NATIVE_STOP); return; }
            const u32 encoder=BYTE(RECORDER_PHASE);
            if (!movie_started || (encoder!=1u && encoder!=2u) ||
                !raw_start_ready(WORD(MOVIE_PROGRESS_MS))) return;
        }
#endif
    }
#endif
#ifdef Z72_RAW_HD_LIVE_CAPTURE
    const int recent_hd=hd_live_capture();
#else
    const int recent_hd=0;
#endif
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
#ifdef Z72_RAW_NEF_SLOT_ENCODE
    /* The encoder owns a writer slot until its packet is drained. Choose a
     * live source only after a destination becomes available. */
    if (z72_mode==&z72_uhd && count-load(&consumed)>=frame_capacity) {
#ifdef Z72_RAW_RESUME_QUEUE_FULL
        /* Keep the queued frames intact. The ordinal gap will supply drop
         * markers after the writer releases a slot. */
        if (header[1]==MIXED_VERSION && clip_timeout_choice!=RAW_TIMEOUT_STOP) return;
#endif
        if (tick()-last_frame_tick>=NEF_OUTPUT_WAIT_MS) finish(QUEUE_FULL);
        return;
    }
#endif
    u32 token=load(&tail_token);
#ifdef Z72_RAW_LIVE_REANCHOR
    /* Leave a gap marker, then sample the live tail again. A verified copy
     * in the private pool is independent of the sensor slot's lifetime. */
    const int use_latest=header[1]==MIXED_VERSION && clip_timeout_choice!=RAW_TIMEOUT_STOP &&
        !stopping && !load(&stop_requested);
#endif
#ifdef Z72_RAW_HD_LIVE_CAPTURE
    if (recent_hd && count
#ifdef Z72_RAW_LIVE_REANCHOR
        && !use_latest
#endif
    ) token=hd_capture_token(token,last_ordinal+1u);
#endif
    int backfilled=0;
#ifdef Z72_RAW_PREFETCH_NEF
#ifdef Z72_RAW_PREFETCH_PARALLEL
    /* If the genuine IRQ has arrived it owns the slot identity. */
    if (saved_anchor && (saved_anchor>>RAW_TOKEN_SHIFT)!=(token>>RAW_TOKEN_SHIFT)
#ifdef Z72_RAW_LIVE_REANCHOR
        && (!use_latest || load(&prefetch_state)==PREFETCH_READY)
#endif
    ) token=saved_anchor;
#else
    if (saved_anchor) token=saved_anchor;
#endif
#endif
#ifdef Z72_RAW_BANK_HANDOFF
    u32 early=0;
#ifdef Z72_RAW_TERMINAL_ROW
    if (!STAGED_NEF_TEST && !packed_uhd && z72_mode==&z72_uhd && z72_clip_fps_num==30000u &&
        (!count || (token>>RAW_TOKEN_SHIFT)==last_ordinal+1u)) {
        early=terminal_row_anchor(token);
        if (!early) early=next_bank_anchor(token);
#else
    if (count && z72_mode==&z72_uhd && z72_clip_fps_num==30000u &&
        (token>>RAW_TOKEN_SHIFT)==last_ordinal+1u) {
        early=next_bank_anchor(token);
#endif
        if (early) token=early;
    }
#endif
#if defined(Z72_RAW_LOSSLESS_REANCHOR) && !defined(Z72_RAW_LIVE_REANCHOR)
    /* Backfilling a retired bank can use up the current frame's encoding
     * window. Let the ordinal gap emit a marker, then sample the live tail
     * again before encoding. The encoder still validates the chosen bank. */
    const int use_latest=z72_clip_fps_num==30000u &&
        log.compression.choice==RAW_COMPRESSION_ALL && header[1]==MIXED_VERSION &&
        clip_timeout_choice!=RAW_TIMEOUT_STOP && !stopping && !load(&stop_requested);
#endif
    if (count && z72_mode==&z72_uhd && (token&RAW_TOKEN_PAIR) &&
#if defined(Z72_RAW_LOSSLESS_REANCHOR) || defined(Z72_RAW_LIVE_REANCHOR)
        !use_latest &&
#endif
        (token>>RAW_TOKEN_SHIFT)==last_ordinal+NEF_FRAME_STEP+2u) {
        const u32 prior=load(&nef_previous_tail);
#ifdef Z72_RAW_DROP_EXPIRED
        if (header[1]==MIXED_VERSION && clip_timeout_choice!=RAW_TIMEOUT_STOP) {
            if (load(&tail_token)!=token) return;
            if ((prior&RAW_TOKEN_PAIR) && (prior>>RAW_TOKEN_SHIFT)==last_ordinal+NEF_FRAME_STEP+1u) {
                token=prior;
                backfilled=1;
            }
        } else
#endif
        {
            if (load(&tail_token)!=token || !(prior&RAW_TOKEN_PAIR) ||
                (prior>>RAW_TOKEN_SHIFT)!=last_ordinal+NEF_FRAME_STEP+1u) return;
            token=prior;
            backfilled=1;
        }
    }
#else
    const u32 token=load(&tail_token);
#endif
    const int profile_lost=count && footer.copy_result==RAW_PROFILE_CHANGED;
    if (profile_lost || !(token&RAW_TOKEN_PAIR) || token==last_attempt ||
        (recent_hd && count && (token>>RAW_TOKEN_SHIFT)<=last_ordinal)
#ifdef Z72_RAW_BANK_HANDOFF
        /* An early frame may finish before the next IRQ. This is still
         * the saved source tail, not a new or skipped frame. */
        || (count && z72_mode==&z72_uhd && z72_clip_fps_num==30000u &&
            (token>>RAW_TOKEN_SHIFT)==last_ordinal)
#endif
    ) {
        if (count) {
            const u32 idle=tick()-last_frame_tick;
            if (stopping && (profile_lost || idle>=STOP_TAIL_MS)) finish(NATIVE_STOP);
            else if (idle>=SOURCE_IDLE_MS) finish(profile_lost ? COPY_FAILED : SOURCE_STALLED);
        }
        return;
    }
    last_attempt=token;
    const u32 source_ordinal=(token>>RAW_TOKEN_SHIFT)-(recent_hd ? 0u : 1u);
#ifdef Z72_RAW_DROP_EXPIRED
    u32 ordinal=source_ordinal;
#else
    const u32 ordinal=source_ordinal;
#endif
#ifdef Z72_RAW_REFERENCE_SERIES
    const u32 step=REFERENCE_VALUE(1u,LINEAR_STEP);
#else
#ifdef Z72_RAW_NEF_STREAM
    const u32 step=z72_mode==&z72_uhd ? NEF_FRAME_STEP : FRAME_STEP;
#else
    const u32 step=FRAME_STEP;
#endif
#endif
#if defined(Z72_RAW_REFERENCE_SERIES) || defined(Z72_RAW_RECORD_FULL_CFB) || defined(Z72_RAW_NEF_STREAM)
    if (count && ordinal-last_ordinal<step) return;
#endif
#ifdef Z72_RAW_DROP_EXPIRED
    const int resumable=header[1]==MIXED_VERSION && clip_timeout_choice!=RAW_TIMEOUT_STOP;
    if (resumable && count && ordinal<=last_ordinal) return;
    const u32 skipped=resumable && count ? ordinal-last_ordinal-1u : 0u;
    if (skipped) ordinal=last_ordinal+1u;
#endif
    u32 backlog=count-load(&consumed);
    footer.ordinal=ordinal;
#ifdef Z72_RAW_RECOVER_NEXT_FRAME
    const int gap=count && ordinal!=last_ordinal+step;
#else
    if (count && ordinal!=last_ordinal+step) { finish(SEQUENCE_GAP); return; }
#endif
#ifdef Z72_RAW_REFERENCE_SERIES
    const u32 capacity=REFERENCE_VALUE(QUEUE_SIZE,1u);
#elif defined(Z72_RAW_FRAME_POOLS)
    const u32 capacity=frame_capacity;
#else
    const u32 capacity=QUEUE_SIZE;
#endif
#ifdef Z72_RAW_NEF_SPILL
    const int scratch_only=z72_mode==&z72_uhd && backlog==capacity;
    struct packet scratch_header;
#endif
    if (backlog==capacity) {
#ifdef Z72_RAW_RECORD_ROLLING
        /* Release the oldest packet before overwriting it. A failed copy
         * then leaves the other complete packets available for saving. */
        publish(&consumed,count-capacity+1u);
        --backlog;
#else
#ifdef Z72_RAW_NEF_SPILL
        if (!scratch_only)
#endif
        {
#ifdef Z72_RAW_HD_LIVE_CAPTURE
            if (resumable && (recent_hd
#ifdef Z72_RAW_LIVE_REANCHOR
                || z72_mode==&z72_hd
#endif
            )) { last_attempt=0; return; }
#endif
            finish(QUEUE_FULL); return;
        }
#endif
    }

    struct packet *packet=queued_packet(count);
#ifdef Z72_RAW_NEF_SPILL
    if (scratch_only) packet=&scratch_header;
#endif
    CALL(0x10361035u,void,void *,u32)(packet,64);
    packet->magic=0x3152465au; /* ZFR1 */
    packet->header_bytes=64;
    packet->payload_bytes=REFERENCE_VALUE(clip_pixel_bytes(),LINEAR_STRIDE*LINEAR_HEIGHT);
    packet->ordinal=ordinal;
#ifdef Z72_RAW_DROP_EXPIRED
    const u32 bank=(token&RAW_TOKEN_SLOT)+RAW_SLOT_COUNT-2u+(u32)recent_hd;
    packet->address=z72_raw_slot_address((bank+RAW_SLOT_COUNT-skipped%RAW_SLOT_COUNT)%RAW_SLOT_COUNT);
#else
    packet->address=z72_raw_slot_address(((token&RAW_TOKEN_SLOT)+RAW_SLOT_COUNT-2u)%RAW_SLOT_COUNT);
#endif
    packet->start_tick=tick();
#ifdef Z72_RAW_DMA_BUSY_RETRY
    packet->reserved[3]=0x3152425au; /* ZBR1: DMA channel retry waits */
#endif
    if (!recording_standby()) {
        packet->reserved[0]=0x314c425au; /* ZBL1 */
        packet->reserved[1]=BLACK_LEVEL;
        packet->reserved[2]=BLACK_LEVEL_DARK_REFERENCE;
    }
#ifdef Z72_RAW_REFERENCE_CODEC
    /* 0x102090A0 configures the input format. 0x1020915A writes
     * the two 10-bit parameters when the geometry enables the lookup table. */
    packet->reserved[3]=0x3143465au; /* ZFC1 */
    packet->reserved[4]=WORD(GEOMETRY+0x14u);
    packet->reserved[5]=WORD(GEOMETRY+0x30u);
#endif
    struct z72_raw_status status;
    CALL(0x10361035u,void,void *,u32)(&status,sizeof(status));
    ++footer.attempts;
#ifdef Z72_RAW_BANK_HANDOFF
    publish(&early_anchor,early);
#ifdef Z72_RAW_TERMINAL_ROW
    if (!count) log.terminal_row.first_anchored=0;
#endif
    if (early) ++log.bank_handoff.attempts;
#endif
#ifdef Z72_RAW_DROP_EXPIRED
    /* A slot-less marker leaves a frame interval available for PCM and export. */
#ifdef Z72_RAW_COMPRESSION_MENU
    const int dma_source=packed_uhd || recent_hd
#ifdef Z72_RAW_LIVE_REANCHOR
        || z72_mode==&z72_hd
#endif
        ;
#endif
    footer.copy_result=skipped ? RAW_WINDOW_EXPIRED : copy_crop(packet,token,&status);
    if (resumable && (count || !recent_hd) && !load(&nef_pending) && !load(&card_error) && !nef_capture_stopping() &&
#ifdef Z72_RAW_COMPRESSION_MENU
        (!dma_source || !load(&dma_pending_channel)) &&
#endif
        (footer.copy_result==RAW_WINDOW_EXPIRED || footer.copy_result==RAW_UNSAFE_SLOT ||
         footer.copy_result==RAW_SNAPSHOT_UNSTABLE || footer.copy_result==RAW_SAMPLE_CHANGED ||
#ifdef Z72_RAW_COMPRESSION_MENU
         (dma_source && footer.copy_result==RAW_DMA_BUSY) ||
#endif
         footer.copy_result==RAW_COPY_TIMEOUT)) {
        packet->payload_bytes=0;
        packet->reserved[5]=0;
        packet->reserved[7]=MIXED_DROPPED;
        status.observed_token=load(&tail_token);
        ++log.dropped.frames;
        log.dropped.last_ordinal=ordinal;
        log.dropped.last_reason=footer.copy_result;
        prefetch_accept(ordinal);
        if (skipped) last_attempt=0;
        footer.copy_result=RAW_COPY_OK;
    }
#else
    footer.copy_result=copy_crop(packet,token,&status);
#endif
#ifdef Z72_RAW_RECHECK_SLOT
    snapshot_retries+=status.snapshot_retries;
#endif
    footer.copy_ms=packet->copy_ms=tick()-packet->start_tick;
    footer.observed_token=packet->tail_token=status.observed_token;
    footer.live_index=status.live_index;
    footer.live_address=status.live_address;
    if (footer.copy_result!=RAW_COPY_OK) {
#ifdef Z72_RAW_PREFETCH_NEF
        /* An early copy can be rejected before the actual next tail. A
         * direct encode must wait for the genuine tail in that case. */
        if (footer.copy_result==PREFETCH_AWAIT_REAL_TAIL) {
            last_attempt=0;
            return;
        }
#endif
#ifdef Z72_RAW_BANK_HANDOFF
        if (early && log.nef.last_error_stage==1u && !load(&nef_pending) &&
            (footer.copy_result==RAW_WINDOW_EXPIRED || footer.copy_result==RAW_UNSAFE_SLOT ||
             footer.copy_result==RAW_SNAPSHOT_UNSTABLE)) {
            last_attempt=0; /* Nothing submitted. Re-evaluate this source on the next poll. */
            return;
        }
#endif
#ifdef Z72_RAW_NEF_QUIET_STOP
        if (z72_mode==&z72_uhd && (footer.copy_result==NEF_STOPPING || nef_capture_stopping())) {
            finish(load(&stop_requested) ? USER_STOP : NATIVE_STOP);
            return;
        }
#endif
#ifdef Z72_RAW_NEF_SPILL
        if (footer.copy_result==NEF_OUTPUT_BUSY) {
            finish(load(&card_error) ? WRITE_FAILED : QUEUE_FULL);
            return;
        }
#endif
#ifdef Z72_RAW_NEF_STREAM
        if (load(&nef_pending) || footer.copy_result==NEF_OVERSIZE) { finish(COPY_FAILED); return; }
#endif
#ifdef Z72_RAW_COPY_DMA
        if (load(&dma_pending_channel) || footer.copy_result==RAW_DMA_ERROR) { finish(COPY_FAILED); return; }
#endif
#ifdef Z72_RAW_RECOVER_NEXT_FRAME
        /* Keep the original DMA check and wait for the next frame token. */
        if (count && (footer.copy_result==RAW_UNSAFE_SLOT ||
                      footer.copy_result==RAW_WINDOW_EXPIRED ||
#ifdef Z72_RAW_COPY_DMA
                      footer.copy_result==RAW_DMA_BUSY ||
#endif
                      footer.copy_result==RAW_SAMPLE_CHANGED) && ++consecutive_rejections<5u) return;
#endif
        if (count && footer.copy_result!=RAW_PROFILE_CHANGED) finish(COPY_FAILED);
        else if (!count && footer.attempts==5u) finish(COPY_FAILED);
        return;
    }
#ifdef Z72_RAW_RECOVER_NEXT_FRAME
    consecutive_rejections=0;
    if (gap) {
        /* ZRV playback requires consecutive ordinals. Keep the old tail until
         * a newer frame is fully validated, then start a new contiguous tail. */
        publish(&consumed,count);
        backlog=0;
        ++recovered_gaps;
    }
#endif
#ifdef Z72_RAW_BANK_HANDOFF
    if (early) ++log.bank_handoff.saved;
#endif
    last_ordinal=ordinal;
    last_frame_tick=packet->start_tick;
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
    if (backfilled) ++log.nef_terminal.backfilled;
#endif
#ifdef Z72_RAW_NEF_SPILL
    if (scratch_only) {
        packet=queued_packet(count);
        CALL(0x10360f44u,void,void *,const void *,u32)(packet,&scratch_header,64u);
        backlog=count-load(&consumed);
    }
#endif
    if (backlog+1u>footer.peak_queue) footer.peak_queue=backlog+1u;
#ifdef Z72_RAW_MOV_SYNC_HD
    /* Tagged HD identifies packed RAW separately from a zero-payload drop. */
    if (z72_mode==&z72_hd && packet->reserved[7]!=MIXED_DROPPED && (load(&mov_sync.enabled)
#ifdef Z72_RAW_LIVE_REANCHOR
        || header[1]==MIXED_VERSION
#endif
    ))
        packet->reserved[7]=MIXED_RAW12;
#endif
#ifdef Z72_RAW_COPY_DMA
    clean_bytes(packet,64);
#else
    clean_bytes(packet,REFERENCE_VALUE(PACKET_BYTES,64+LINEAR_STRIDE*LINEAR_HEIGHT));
#endif
#ifdef Z72_RAW_MIXED_FRAMES
    if (header[1]==MIXED_VERSION) {
#ifdef Z72_RAW_COMPRESSION_MENU
        if (packed_uhd && packet->reserved[7]!=MIXED_DROPPED)
            packet->reserved[7]=MIXED_RAW12;
#endif
        if (packet->reserved[7]==MIXED_RAW12) ++log.mixed.packed_frames;
#ifdef Z72_RAW_DROP_EXPIRED
        else if (packet->reserved[7]==MIXED_NEF) ++log.mixed.nef_frames;
#else
        else ++log.mixed.nef_frames;
#endif
    }
#endif
    publish(&produced,count+1u);
#ifdef Z72_RAW_PREFETCH_OUTPUT_WAIT
    if (load(&prefetch_enabled) && frame_capacity==2u && count+1u-load(&consumed)==frame_capacity)
        prefetch_offer_output_wait(token);
#endif
#ifdef Z72_RAW_STAGED_NEF_TEST
    finish(COMPLETE);
#endif
#ifdef Z72_RAW_NEF_TWO_FRAME_BURST
    /* An oversize first frame owns the encoder scratch until the writer drains it. */
    if (z72_mode==&z72_uhd && !count && nef_scratch_until) {
        finish(QUEUE_FULL);
        return;
    }
#endif
#ifndef Z72_RAW_RECORD_ROLLING
#ifdef Z72_RAW_PCM_SEGMENTS
    if (!PCM_STREAM_MODE)
#endif
    if (count+1u==CLIP_VALUE(limit,REFERENCE_VALUE(FRAME_LIMIT,LINEAR_LIMIT))) finish(COMPLETE);
#endif
}

static void capture_worker(void *arg)
{
    (void)arg;
    for (;;) {
#ifdef Z72_RAW_NEF_STREAM
        if (!STAGED_NEF_LIVE && load(&nef_pending) && load(&nef_complete)
#ifdef Z72_RAW_NEF_EVENT_WAIT
            && ((int)nef_event<=0 || nef_wait_event(0))
#endif
        ) publish(&nef_pending,0);
#endif
#ifdef Z72_RAW_COPY_DMA
        const u32 pending=load(&dma_pending_channel);
#ifdef Z72_RAW_SPLIT_DMA
#ifdef Z72_RAW_RASTER_COPY
        if (pending==RASTER_PENDING) {
#ifdef Z72_RAW_PREFETCH_NEF
            /* The prefetch worker owns both the DMA wait and any late reap. */
            if (!load(&prefetched_address))
#endif
                raster_reap(0);
        }
        else
#endif
        if (pending) wait_frame_dma(0,0);
#else
        if (pending && z72_raw_dma_wait(pending-1u,0)!=RAW_DMA_PENDING) publish(&dma_pending_channel,0);
#endif
#endif
#ifdef Z72_RAW_PCM_2997_HANDOFF
        const u32 before=load(&produced);
#endif
        capture_step();
#ifdef Z72_RAW_PCM_2997_HANDOFF
        /* A completed encode can wake after the following frame IRQ. Consume
         * that pending event now. All no-progress paths still yield. */
        if (load(&state)==ARMED && ((z72_mode==&z72_uhd && z72_clip_fps_num==30000u)
#ifdef Z72_RAW_HD_LIVE_CAPTURE
            || hd_live_capture()
#endif
#ifdef Z72_RAW_LIVE_REANCHOR
            || !last_attempt /* Retry immediately after publishing a gap marker. */
#endif
            ) &&
            load(&produced)!=before
#ifndef Z72_RAW_BANK_HANDOFF
            && load(&tail_token)!=last_attempt
#endif
        ) continue;
#endif
        CALL(0x103dc539u,void,u32)(load(&state)==ARMED ? (STAGED_NEF_LIVE ? 1u : 2u) : 20u);
    }
}

static u32 file_status(const char *path)
{
    u32 object[10];
    CALL(0x10361035u,void,void *,u32)(object,sizeof(object));
    CALL(0x104d3c01u,void,u32 *,const char *)(object,path);
    u32 result=CALL(0x104d3c05u,u32,u32 *,u32,u32)(object,0,0);
    if (result==FILE_OK && CALL(0x104d3c91u,u32,u32 *)(object)!=FILE_OK) return 1;
    return result;
}

#ifdef Z72_RAW_PCM_ORDINARY_FILES
static u32 failed_roll_footer_written;

static void segment_file_error(u32 operation,u32 result)
{
    if (!log.segment_files.operation) {
        log.segment_files.operation=operation;
        log.segment_files.result=result;
    }
}

/* The ordinary service registers each handle separately (command 0x11).
 * Its matching close (0x0a) commits the directory entry and releases it. */
static u32 close_segment_file(int fd,unsigned long long length)
{
    const u32 sized=length ? CALL(0x1047102fu,u32,int,u32,u32,u32)(
        fd,0,(u32)length,(u32)(length>>32)) : 0;
    const u32 closed=CALL(0x105be9ddu,u32,int)(fd);
    if (sized) segment_file_error(4,sized);
    if (closed) segment_file_error(5,closed);
    if (!sized && !closed) ++log.segment_files.closed;
    return sized ? sized : closed;
}
#endif

#ifdef Z72_RAW_NUMBERED_EXTENSIONS
static u32 next_part;
/* The stem and dot are shared by the main clip and its numbered parts. */
enum { PART_PREFIX=sizeof(clip_path)-4u, PART_PATH_BYTES=PART_PREFIX+1u+10u+1u };
#ifdef Z72_RAW_MOVIE_DIRECTORY
#include "movie_raw_directory.h"
#else
#define raw_file_status file_status
#endif

static void part_path(char *out,const char *base,u32 part)
{
    for (u32 i=0;i<PART_PREFIX;++i) out[i]=base[i];
    char digits[10];
    u32 count=0;
    do {
        digits[count++]=(char)('0'+part%10u);
        part/=10u;
    } while (part);
    if (count==1u) digits[count++]='0';
    out+=PART_PREFIX;
    *out++='Z';
    while (count) *out++=digits[--count];
    *out=0;
}

static int create_raw_file(const char *name)
{
#ifdef Z72_RAW_MOVIE_DIRECTORY
    char path[RAW_PATH_BYTES];
    name=movie_raw_path(path,name);
#endif
    const int fd=CALL(0x105beb2bu,int,const char *,u32)(name,0x20u);
    if (fd<0) segment_file_error(3,(u32)fd);
    else ++log.segment_files.opened;
    return fd;
}

static int open_clip_part(void)
{
    char name[PART_PATH_BYTES];
    part_path(name,clip_path,next_part);
    const u32 status=raw_file_status(name);
    if (status!=FILE_MISSING) {
        segment_file_error(1,status);
        return -1;
    }
    const int fd=create_raw_file(name);
    if (fd>=0) ++next_part;
    return fd;
}
#endif

static int open_clip(void)
{
    char name[]=RECORD_PATH;
#ifdef Z72_RAW_NATIVE_MENU
    name[0]=(load(&raw_settings)&4u) ? 'A' : 'B';
    clip_path[0]=name[0];
#ifdef Z72_RAW_MOVIE_DIRECTORY
    if (!prepare_movie_directory(name[0])) return -1;
#endif
#endif
#ifdef Z72_RAW_SELECT_OUTPUT
    name[7]=RECORD_ONLY() ? 'Z' : 'M';
#endif
#ifdef Z72_RAW_NEF_STREAM
    if (z72_mode==&z72_uhd) name[6]='N';
#endif
#ifdef Z72_RAW_REFERENCE_PROFILE
    name[6]='P';
#ifdef Z72_RAW_FULL_REFERENCE
    name[7]=recording_standby() ? 'S' : 'F';
#else
    name[7]=recording_standby() ? 'S' : 'R';
#endif
#endif
#ifdef Z72_RAW_PCM_SEGMENTS
    for (;;++next_file) {
        if (next_file>=1000u) {
            /* Eight 8.3 basename characters, independent of previous clips. */
            u32 number=next_file;
            name[3]=RECORD_ONLY() ? 'R' : 'M';
            for (u32 i=10;i>3;--i) {
                const u32 digit=number%36u;
                name[i]=(char)(digit<10u ? '0'+digit : 'A'+digit-10u);
                number/=36u;
            }
        } else
#else
    for (;next_file<1000u;++next_file) {
#endif
        {
            name[8]=(char)('0'+next_file/100u);
            name[9]=(char)('0'+next_file/10u%10u);
            name[10]=(char)('0'+next_file%10u);
        }
#ifdef Z72_RAW_MOVIE_DIRECTORY
        u32 status=raw_file_status(name);
#else
        u32 status=file_status(name);
#endif
        if (status==FILE_OK) {
#ifdef Z72_RAW_MOVIE_STEM
            movie_stem[0]=0; /* The paired name is occupied. Use the RAW counter. */
#endif
            continue;
        }
        if (status!=FILE_MISSING) {
#ifdef Z72_RAW_PCM_ORDINARY_FILES
            segment_file_error(1,status);
#endif
            break;
        }
#ifdef Z72_RAW_SELECT_OUTPUT
        for (u32 i=3;i<=10u;++i) clip_path[i]=name[i];
#endif
#ifdef Z72_RAW_NUMBERED_EXTENSIONS
        char part[PART_PATH_BYTES];
        part_path(part,name,0);
        status=raw_file_status(part);
        if (status==FILE_OK) {
#ifdef Z72_RAW_MOVIE_STEM
            movie_stem[0]=0;
#endif
            continue;
        }
        if (status!=FILE_MISSING) {
            segment_file_error(1,status);
            break;
        }
        const int fd=create_raw_file(name);
        if (fd>=0) {
            ++next_file;
            next_part=0;
        }
        return fd;
#elif defined(Z72_RAW_PCM_ORDINARY_FILES)
        const int fd=CALL(0x105beb2bu,int,const char *,u32)(name,0x20u);
        if (fd<0) segment_file_error(3,(u32)fd);
        else {
            ++next_file; /* Never probe the path of a still-open segment. */
            ++log.segment_files.opened;
        }
        return fd;
#elif defined(Z72_RAW_PHOTO_FILE)
        return CALL(0x105bee0du,int,const char *,u32)(name,0x20u);
#else
        return CALL(0x105beb2bu,int,const char *,u32)(name,0);
#endif
    }
    return -1;
}

static int write_bytes(const void *data,u32 bytes)
{
    const unsigned char *cursor=data;
    while (bytes) {
        const u32 chunk=bytes<FILE_WRITE_CHUNK ? bytes : FILE_WRITE_CHUNK;
        u32 actual=0;
#ifdef Z72_RAW_DIRECT_FILE
        /* Photo open returns the filesystem handle. Use the same locked write as
         * Nikon's 0x1044B270 worker. Every block of this file uses this path. */
        const u32 status=CALL(0x1047109du,u32,int,const void *,u32,u32 *)(file,cursor,chunk,&actual);
#else
        const struct { u32 kind; const void *data; u32 length; } io={1,cursor,chunk};
        const u32 status=CALL(FILE_WRITE_ENTRY,u32,int,const void *,u32,u32 *)(file,&io,chunk,&actual);
#endif
#ifdef Z72_RAW_PHOTO_FILE_EXACT_SIZE
        if (actual<=chunk) file_bytes+=actual;
#endif
        if (status || actual!=chunk)
            return 0;
        cursor+=chunk;
        bytes-=chunk;
    }
    return 1;
}

#ifdef Z72_RAW_STAGED_NEF_TEST
static int save_nef_reference(void)
{
    char name[sizeof(clip_path)];
    for (u32 i=0;i<PART_PREFIX;++i) name[i]=clip_path[i];
    name[PART_PREFIX]='R';
    name[PART_PREFIX+1u]='1';
    name[PART_PREFIX+2u]='2';
    name[PART_PREFIX+3u]=0;
    char path[RAW_PATH_BYTES];
    movie_raw_path(path,name);
    if (file_status(path)!=FILE_MISSING) return 0;
    const int fd=CALL(0x105beb2bu,int,const char *,u32)(path,0x20u);
    if (fd<0) return 0;
    const u32 bytes=clip_pixel_bytes();
    u32 written=0;
    int complete=1;
    while (written<bytes) {
        const u32 length=bytes-written<0x100000u ? bytes-written : 0x100000u;
        u32 actual=0;
        const u32 result=CALL(0x1047109du,u32,int,const void *,u32,u32 *)(
            fd,(const void *)(stage_address+written),length,&actual);
        if (actual<=length) written+=actual;
        if (result || actual!=length) {
            complete=0;
            break;
        }
    }
    const u32 sized=CALL(0x1047102fu,u32,int,u32,u32,u32)(fd,0,written,0);
    const u32 closed=CALL(0x105be9ddu,u32,int)(fd);
    if (!complete || sized || closed || written!=bytes) return 0;
    log.staged_nef.reference_bytes=written;
    return 1;
}
#endif

#ifdef Z72_RAW_PCM_AUDIO
#include "movie_raw_pcm.h"
#endif
#ifdef Z72_RAW_VOLUME_TIMING
#include "movie_raw_volume_timing.h"
#endif
#ifdef Z72_RAW_NVME_ADMIN_SNAPSHOT
#include "movie_raw_nvme_admin_snapshot.h"
#endif
#ifdef Z72_RAW_NVME_POWER_TEST
#include "movie_raw_nvme_power_test.h"
#endif
#ifdef Z72_RAW_NVME_REC_TEST
#include "movie_raw_nvme_rec_test.h"
#endif

#ifdef Z72_RAW_ALIGNED_WRITE
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
static int same_card_cfexpress(void)
{
    return clip_path[0]=='A' && !RECORD_ONLY() && log.route.descriptor_slot==0u && mxic_record_route_enabled();
}
#endif
#ifdef Z72_RAW_HD_LIVE_CAPTURE
static int write_hd_packet(u32 index)
{
    struct packet *packet=queued_packet(index);
#ifdef Z72_RAW_HD_MXIC_PAIR
    if (packet->reserved[7]!=MIXED_DROPPED) {
        if (clip_path[0]=='A') return write_frame_bulk((const unsigned char *)packet,packet_span(),same_card_cfexpress());
        return hd_live_capture() ? write_timed_bulk(packet,packet_span(),0) : write_bytes(packet,packet_span());
    }
    /* The preceding frame may have retained FS_LOCK for a second write. */
    release_volume_lock();
#elif !defined(Z72_RAW_LIVE_REANCHOR)
    if (!hd_live_capture()) return write_bytes(packet,packet_span());
    if (packet->reserved[7]!=MIXED_DROPPED) return write_timed_bulk(packet,packet_span(),0);
#else
    if (packet->reserved[7]!=MIXED_DROPPED)
        return hd_live_capture() ? write_timed_bulk(packet,packet_span(),0) : write_bytes(packet,packet_span());
#endif
    CALL(0x10361035u,void,void *,u32)(packet->pixels,WRITE_ALIGNMENT-64u);
    clean_bytes(packet,WRITE_ALIGNMENT);
    return write_bytes(packet,WRITE_ALIGNMENT);
}
#endif
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
/* The encoder already places each packet at its final file offset modulo
 * 4096. Keep the unfinished page outside its slot before releasing it. */
static struct {
    _Alignas(64) unsigned char tail[WRITE_ALIGNMENT];
    u32 enabled,pending;
} same_card_pages;
#endif

static int write_uhd_packet(u32 index)
{
    struct packet *packet=queued_packet(index);
    unsigned char *start=stream_packet(packet,index);
#ifdef Z72_RAW_MIXED_FRAMES
    const int mixed_packed=header[1]==MIXED_VERSION && packet->reserved[7]==MIXED_RAW12;
#ifdef Z72_RAW_DROP_EXPIRED
    const int mixed_dropped=header[1]==MIXED_VERSION && packet->reserved[7]==MIXED_DROPPED;
    if (mixed_dropped && packet->payload_bytes) return 0;
#else
    const int mixed_dropped=0;
#endif
    if (mixed_packed) {
#ifdef Z72_RAW_COMPRESSION_MENU
        if (packet->payload_bytes!=clip_pixel_bytes()) return 0;
        if (!packed_uhd) {
            const u32 pixels=prefetch_packed_address(packet->ordinal);
            if (!pixels) return 0;
            start=(unsigned char *)(pixels-64u);
        }
#else
        const u32 pixels=prefetch_packed_address(packet->ordinal);
        if (!pixels || packet->payload_bytes!=clip_pixel_bytes()) return 0;
        start=(unsigned char *)(pixels-64u);
#endif
    } else if (header[1]==MIXED_VERSION && !mixed_dropped &&
               (packet->reserved[7]!=MIXED_NEF || packet->payload_bytes>nef_slot_payload_limit))
        return 0;
#endif
#ifdef Z72_RAW_NEF_DIRECT_OVERSIZE
    if (!NEF_SLOT_ENCODE && nef_write_from_scratch(index,packet->payload_bytes))
        start=nef_scratch_packet(packet->reserved[5]);
#endif
#ifdef Z72_RAW_NEF_STREAM
    u32 bytes=64u+packet->payload_bytes;
#ifdef Z72_RAW_NEF_DMA_STAGE
    bytes=(bytes+63u)&~63u;
#ifdef Z72_RAW_SIZE_SPLIT
    const u32 padded=(bytes+WRITE_ALIGNMENT-1u)&~(WRITE_ALIGNMENT-1u);
    if (padded>bytes) {
        CALL(0x10361035u,void,void *,u32)(start+bytes,padded-bytes);
        clean_bytes(start+bytes,padded-bytes);
    }
    bytes=padded;
#endif
#endif
#else
    u32 bytes=64u+clip_pixel_bytes();
#endif
    u32 head=(-(u32)start)&(WRITE_ALIGNMENT-1u);
    u32 bulk=(bytes-head)&~(WRITE_ALIGNMENT-1u);
    u32 tail=bytes-head-bulk;
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
    if (same_card_pages.enabled) {
        const u32 prefix=same_card_pages.pending;
        if ((file_bytes&(WRITE_ALIGNMENT-1u)) ||
#ifdef Z72_RAW_MIXED_FRAMES
            (mixed_packed
#ifdef Z72_RAW_COMPRESSION_MENU
             && !packed_uhd
#endif
             ? prefix!=0u : start!=(unsigned char *)packet+WRITE_ALIGNMENT+prefix)) {
#else
            start!=(unsigned char *)packet+WRITE_ALIGNMENT+prefix) {
#endif
#ifdef Z72_RAW_PCM_BATCH_WRITE
            /* A queued packet can fail validation before the inner write.
             * Preserve the previous frame's tail and avoid recursive locking. */
            release_volume_lock();
            if (!(file_bytes&(WRITE_ALIGNMENT-1u)) && prefix) {
                clean_bytes(same_card_pages.tail,prefix);
                write_bytes(same_card_pages.tail,prefix);
            }
#endif
            return 0;
        }
        unsigned char *page=start-prefix;
        if (prefix) CALL(0x10360f44u,void,void *,const void *,u32)(page,same_card_pages.tail,prefix);
        CALL(0x10360f44u,void,void *,const void *,u32)(start,packet,64u);
        clean_bytes(page,prefix+64u);
        start=page;
        head=0;
        bulk=(prefix+bytes)&~(WRITE_ALIGNMENT-1u);
        tail=prefix+bytes-bulk;
    } else {
#endif
    if ((((u32)file_bytes^(u32)start)&(WRITE_ALIGNMENT-1u))!=0) return 0;

    /* The filesystem CPU-copies partial sectors. Discard cached edges before
     * reading DMA pixels or touching the cache line shared with the header. */
    invalidate_dma_pixels((u32)start,head>=64u ? head : 64u);
    if (tail) invalidate_dma_pixels((u32)start+head+bulk,tail);
    CALL(0x10360f44u,void,void *,const void *,u32)(start,packet,64u);
    clean_bytes(start,64u);
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
    }
#endif

    u32 before=tick();
#ifdef Z72_RAW_NVME_TIMING
    const u32 reads=log.volume.read_calls,payload_calls=log.volume.data_calls,other=log.volume.other_calls;
    const u32 writes=log.nvme.writes,write_ms=log.nvme.write_ms;
    const u32 flushes=log.nvme.flushes,flush_ms=log.nvme.flush_ms;
#endif
#ifdef Z72_RAW_EXFAT_PREFETCH
    const u32 bitmap_reads=log.bitmap.candidates,bitmap_hits=log.bitmap.hits;
    const u32 bitmap_batches=log.bitmap.prefetch_calls;
#endif
    if (!write_bytes(start,head)) return 0;
    u32 bulk_start=tick();
#ifdef Z72_RAW_VOLUME_TIMING
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
    const u32 pos_before=(u32)file_bytes;
#ifdef Z72_RAW_PCM_BOUNDED_WRITE
    if (!write_frame_bulk(start+head,bulk,same_card_pages.enabled)) {
#elif defined(Z72_RAW_PCM_BATCH_WRITE)
    if (!write_timed_bulk(start+head,bulk,same_card_pages.enabled
#if defined(Z72_RAW_PCM_SEGMENTS) && !defined(Z72_RAW_SIZE_SPLIT)
                          && (index+1u)%Z72_RAW_NEF_BURST_LIMIT
#endif
                          )) {
#else
    if (!write_timed_bulk(start+head,bulk)) {
#endif
        const u32 actual=(u32)file_bytes-pos_before;
        if (same_card_pages.enabled && actual<same_card_pages.pending) {
            clean_bytes(same_card_pages.tail,same_card_pages.pending);
            write_bytes(same_card_pages.tail+actual,same_card_pages.pending-actual);
        }
        return 0;
    }
#else
    if (!write_timed_bulk(start+head,bulk)) return 0;
#endif
#else
    if (!write_bytes(start+head,bulk)) return 0;
#endif
    u32 bulk_end=tick();
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
    if (same_card_pages.enabled) {
        if (tail) {
            invalidate_dma_pixels((u32)start+bulk,tail);
            CALL(0x10360f44u,void,void *,const void *,u32)(same_card_pages.tail,start+bulk,tail);
        }
        same_card_pages.pending=tail;
    } else
#endif
    if (!write_bytes(start+head+bulk,tail)) return 0;
    log.bulk_bytes+=bulk;
#ifdef Z72_RAW_NVME_TIMING
    if (log.bulk_bytes<bulk) ++log.nvme.bulk_bytes_high;
#endif
    log.bulk_ms+=bulk_end-bulk_start;
    u32 end=tick();
    log.edge_ms+=bulk_start-before+end-bulk_end;
#ifdef Z72_RAW_NVME_TIMING
    if (end-before>log.slow_write.elapsed_ms) {
        log.slow_write.ordinal=packet->ordinal;
        log.slow_write.elapsed_ms=end-before;
        log.slow_write.bulk_ms=bulk_end-bulk_start;
        log.slow_write.reads=log.volume.read_calls-reads;
        log.slow_write.payload_calls=log.volume.data_calls-payload_calls;
        log.slow_write.other_writes=log.volume.other_calls-other;
        log.slow_write.writes=log.nvme.writes-writes;
        log.slow_write.write_ms=log.nvme.write_ms-write_ms;
        log.slow_write.flushes=log.nvme.flushes-flushes;
        log.slow_write.flush_ms=log.nvme.flush_ms-flush_ms;
#ifdef Z72_RAW_EXFAT_PREFETCH
        log.bitmap.slow_candidates=log.bitmap.candidates-bitmap_reads;
        log.bitmap.slow_hits=log.bitmap.hits-bitmap_hits;
        log.bitmap.slow_prefetch_calls=log.bitmap.prefetch_calls-bitmap_batches;
#endif
    }
#endif
    return 1;
}
#ifdef Z72_RAW_MIXED_FRAMES
static int write_queued_uhd(u32 index)
{
    const struct packet *packet=queued_packet(index);
    const int saved=write_uhd_packet(index);
    if (header[1]==MIXED_VERSION && packet->reserved[7]==MIXED_RAW12) {
        if (saved) ++log.mixed.packed_written;
#ifdef Z72_RAW_COMPRESSION_MENU
        if (!packed_uhd)
#endif
            prefetch_release_packed(packet->ordinal);
    }
    return saved;
}
#else
#define write_queued_uhd write_uhd_packet
#endif

static int write_queued_aligned(u32 index)
{
    if (z72_mode==&z72_uhd) return write_queued_uhd(index);
#ifdef Z72_RAW_HD_LIVE_CAPTURE
    return write_hd_packet(index);
#elif defined(Z72_RAW_PCM_AUTO_RATE)
    return write_bytes(queued_packet(index),packet_span());
#else
    return write_bytes(queued_packet(index),64u+clip_pixel_bytes());
#endif
}
#endif

#ifdef Z72_RAW_NEF_ROLLOVER
static void begin_clip_session(int resume)
#else
static void begin_clip(void)
#endif
{
#ifdef Z72_RAW_NEF_ROLLOVER
    const u32 index=resume ? log.nef_segment.index+1u : 1u;
    const u32 previous=resume ? footer.reason : 0u;
#endif
#ifdef Z72_RAW_AUTO_25P
    prepare_error=0;
#endif
#ifdef Z72_RAW_NEF_STREAM
#ifdef Z72_RAW_SIZE_SPLIT
    nef_offset=0;
#else
    nef_offset=sizeof(header);
#endif
#endif
#ifdef Z72_RAW_NEF_DIRECT_OVERSIZE
    nef_scratch_until=0;
#endif
#ifdef Z72_RAW_NEF_COMPLETION_SNAPSHOT
    publish(&nef_previous_tail,0);
#ifdef Z72_RAW_TERMINAL_ROW
    for (u32 i=0;i<RAW_SLOT_COUNT;++i) publish(&first_tails[i].token,0);
#endif
#endif
    CALL(0x10361035u,void,void *,u32)(&footer,sizeof(footer));
#ifdef Z72_RAW_SELECT_OUTPUT
#ifdef Z72_RAW_50P_SINGLE_FRAME
    publish(&frame_sample_ready,0);
#endif
    reset_profile_log();
#endif
#ifdef Z72_RAW_NVME_ADMIN_SNAPSHOT
    if (!resume && RECORD_ONLY() && (load(&raw_settings)&4u)) snapshot_nvme_admin(NVME_ADMIN_ARMED);
#endif
#ifdef Z72_RAW_NEF_ROLLOVER
    log.nef_segment.index=index;
    log.nef_segment.previous_end=previous;
#endif
#ifdef Z72_RAW_ZRV_ONLY
#ifdef Z72_RAW_NEF_ROLLOVER
    if (!resume) {
#endif
    publish(&raw_clip_orientation,0);
    publish(&mov_discarded_bytes,0);
    publish(&mov_sink_state,MOV_SINK_WAITING);
#ifdef Z72_RAW_NEF_ROLLOVER
    }
#endif
#endif
#ifdef Z72_RAW_PHOTO_FILE_EXACT_SIZE
    file_bytes=0;
#endif
    publish(&card_error,0);
    publish(&tail_token,0);
    publish(&produced,0);
    publish(&consumed,0);
#ifdef Z72_RAW_STAGED_NEF_LIVE
    reset_staged_stream();
#endif
#ifdef Z72_RAW_RECORD_PACKED_BYTE
    publish(&packed,0);
#endif
    last_attempt=0;
    last_ordinal=0;
#ifdef Z72_RAW_NEF_ROLLOVER
    movie_started=resume;
#else
    movie_started=0;
#endif
#ifdef Z72_RAW_RECHECK_SLOT
    snapshot_retries=0;
#endif
#ifdef Z72_RAW_RECOVER_NEXT_FRAME
    consecutive_rejections=0;
    recovered_gaps=0;
#endif
#ifdef Z72_RAW_RECORD_DEFERRED
    timing_header[12]=0;
    timing_header[13]=0;
#endif
#if defined(Z72_RAW_STREAM_SLOT_RECHECK) || defined(Z72_RAW_NEF_SLOT_VALIDATE)
#ifdef Z72_RAW_ZRV_ONLY
    timing_header[12]=RECORD_ONLY() ? 0x314f535au : 0x3152535au; /* ZSO1 / ZSR1 */
#else
    timing_header[12]=0x3152535au; /* ZSR1 */
#endif
    timing_header[13]=0;
    timing_header[14]=0;
#endif
    record_label(0,OPENING_LABEL);
    record_label(1,"RAW record: finish clip");
    publish(&state,OPENING);
#ifdef Z72_RAW_NVME_POWER_TEST
    start_nvme_power_test();
#endif
}

#ifdef Z72_RAW_NEF_ROLLOVER
static void begin_clip(void)
{
    begin_clip_session(0);
}

static int nef_restart_clip(void)
{
    const u32 encoder=BYTE(RECORDER_PHASE);
    if (!RECORD_ONLY() || load(&stop_requested) || BYTE(MOVIE_PHASE)!=1u ||
        (encoder!=1u && encoder!=2u) || load(&mov_sink_state)!=MOV_SINK_ACTIVE ||
        z72_mode!=&z72_uhd || !nef_owner || frame_capacity!=2u || !z72_raw_profile_matches()) return 0;
    u32 stock=RAW_IRQ_NATIVE;
    if (!__atomic_compare_exchange_n(&WORD(RAW_IRQ_SLOT),&stock,(u32)raw_irq,0,
                                     __ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) return 0;
    begin_clip_session(1);
    return 1;
}
#endif

#ifdef Z72_RAW_50P_SINGLE_FRAME
#include "movie_raw_50p_frame.h"
#endif
#if defined(Z72_RAW_UHD_2997) && defined(Z72_RAW_PCM_AUTO_RATE)
static int unsupported_source(void)
{
    const struct z72_raw_mode *mode=log.source.type==z72_uhd.type ? &z72_uhd : &z72_hd;
    return log.source.num && log.source.den && log.source.width && log.source.height &&
           (log.source.type!=mode->type ||
            !z72_raw_mode_fps_num(mode,log.source.num,log.source.den) ||
            log.source.width!=mode->width || log.source.height!=mode->height);
}

static int write_report_buffer(const char *path,const void *data,u32 bytes)
{
    int fd=CALL(0x105beb2bu,int,const char *,u32)(path,0);
    if (fd<0) return 0;
    const unsigned char *cursor=data;
    while (bytes) {
        const u32 chunk=bytes<0x10000u ? bytes : 0x10000u;
        const struct { u32 kind; const void *buffer; u32 capacity; } io={1u,cursor,chunk};
        u32 actual=0;
        clean_bytes(cursor,chunk);
        if (CALL(0x105bea79u,u32,int,const void *,u32,u32 *)(fd,&io,chunk,&actual) || actual!=chunk) break;
        cursor+=chunk;
        bytes-=chunk;
    }
    const u32 close=CALL(0x105be9ddu,u32,int)(fd);
    return !close && !bytes;
}

static int save_unmatched_profile(void)
{
    char path[]="A:\\Z72FP000.LOG";
    if (!(load(&raw_settings)&4u)) path[0]='B';
    for (u32 i=0;i<1000u;++i) {
        path[8]='0'+i/100u; path[9]='0'+i/10u%10u; path[10]='0'+i%10u;
        const u32 status=file_status(path);
        if (status==FILE_MISSING) {
#ifdef Z72_RAW_50P_SINGLE_FRAME
            if (log.single_frame.result==FRAME50_COPIED) {
                char raw_path[sizeof(path)];
                for (u32 j=0;j<sizeof(path);++j) raw_path[j]=path[j];
                raw_path[12]='R'; raw_path[13]='1'; raw_path[14]='2';
                const u32 raw_status=file_status(raw_path);
                if (raw_status==FILE_OK) continue;
                if (raw_status!=FILE_MISSING) return 0;
                if (!write_report_buffer(raw_path,frame50_pixels,FRAME50_BYTES))
                    log.single_frame.result=FRAME50_WRITE_FAILED;
            }
#endif
            return write_report_buffer(path,&log,sizeof(log));
        }
        if (status!=FILE_OK) break;
    }
    return 0;
}
#endif

static void close_clip(const char *label,int pair_next)
{
#ifdef Z72_RAW_MOV_SYNC
    sync_uninstall();
#endif
#ifdef Z72_RAW_PCM_MOV_CONTENTION
    mov_contention_close();
#endif
#ifdef Z72_RAW_NVME_POWER_TEST
    end_nvme_power_test();
#endif
#if defined(Z72_RAW_UHD_2997) && defined(Z72_RAW_PCM_AUTO_RATE)
    if (file<0 && footer.reason==NATIVE_STOP && unsupported_source())
        label=save_unmatched_profile() ? "RAW: unsupported mode, see LOG" : "RAW: unsupported mode";
#endif
#ifdef Z72_RAW_50P_SINGLE_FRAME
    if (frame50_owner) {
        CALL(0x1034fe7fu,void,void *)(frame50_owner);
        frame50_owner=0;
        frame50_pixels=0;
    }
#endif
    if (file>=0) {
#ifdef Z72_RAW_PCM_ORDINARY_FILES
        const u32 status=close_segment_file(file,file_bytes);
        if (!status && failed_roll_footer_written) ++log.session.sealed;
        failed_roll_footer_written=0;
#elif defined(Z72_RAW_PHOTO_FILE_EXACT_SIZE)
        const u32 status=CALL(0x105beeafu,u32,int,u32,u32,u32)(file,0,(u32)file_bytes,
                                                (u32)((unsigned long long)file_bytes>>32));
#elif defined(Z72_RAW_PHOTO_FILE)
        const u32 status=CALL(0x105beeafu,u32,int,u32,u32,u32)(file,0,0,0);
#else
        const u32 status=CALL(0x105be9ddu,u32,int)(file);
#endif
        if (status) {
#ifdef Z72_RAW_PCM_SEGMENTS
            if (!log.session.failure) log.session.failure=7;
#endif
            label="RAW record: close failed";
            pair_next=0;
        }
        file=-1;
#ifdef Z72_RAW_STAGED_NEF_TEST
        if (!status && log.session.written==1u && !log.session.failure) {
            label=save_nef_reference() ? "RAW DMA to NEF: pair saved" : "RAW DMA to NEF: reference failed";
        }
#endif
    }
#ifdef Z72_RAW_NEF_ROLLOVER
    if (pair_next && nef_restart_clip()) return;
#endif
#ifdef Z72_RAW_RECORD_HEAP
    /* OPENING has no producer. FINISHED has handed all packets to the writer. */
#ifdef Z72_RAW_PREFETCH_NEF
    if (prefetched_owner) {
        publish(&prefetched_address,0);
        CALL(0x1034fe7fu,void,void *)(prefetched_owner);
        prefetched_owner=0;
    }
#endif
#ifdef Z72_RAW_STAGED_NEF_TEST
    if (stage_owner) {
        CALL(0x1034fe7fu,void,void *)(stage_owner);
        stage_owner=0;
        stage_address=0;
    }
#endif
#ifdef Z72_RAW_STAGED_NEF_LIVE
    for (u32 i=0;i<2u;++i) {
        if (stage_owners[i]) CALL(0x1034fe7fu,void,void *)(stage_owners[i]);
        stage_owners[i]=0;
    }
#endif
#ifdef Z72_RAW_NEF_STREAM
    if (nef_owner) {
        CALL(0x1034fe7fu,void,void *)(nef_owner);
        nef_owner=0;
        nef_scratch=0;
    }
#endif
#ifdef Z72_RAW_FRAME_POOLS
    while (frame_capacity) {
        --frame_capacity;
        if (frame_owners[frame_capacity])
            CALL(0x1034fe7fu,void,void *)(frame_owners[frame_capacity]);
        frame_slots[frame_capacity]=0;
        frame_owners[frame_capacity]=0;
    }
#else
    if (queue_owner) {
        CALL(0x1034fe7fu,void,void *)(queue_owner);
        queue_owner=0;
        queue=0;
    }
#endif
#endif
#ifdef Z72_RAW_PAIR_AUTO
    if (pair_next && !load(&stop_requested) && z72_raw_movie_idle()) {
        publish(&z72_raw_reference_linear,1);
        begin_clip();
        return;
    }
    publish(&auto_pair,0);
#else
    (void)pair_next;
#endif
    u32 hook=(u32)raw_irq;
    __atomic_compare_exchange_n(&WORD(RAW_IRQ_SLOT),&hook,RAW_IRQ_NATIVE,0,
                                __ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    record_label(0,ARM_LABEL);
#ifdef Z72_RAW_MXIC_RECORD_TEST
    if (mxic_record_disabled) label="RAW: power cycle camera";
#endif
    record_label(1,label);
    if (raw_media_owner) {
        CALL(0x1034fe7fu,void,void *)(raw_media_owner);
        raw_media_owner=0;
        pcm_data=0;
        media_page=0;
    }
    publish(&state,IDLE);
}

static int write_timing(u32 count)
{
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
    if (same_card_pages.enabled && same_card_pages.pending) {
        clean_bytes(same_card_pages.tail,same_card_pages.pending);
        if (!write_bytes(same_card_pages.tail,same_card_pages.pending)) return 0;
        same_card_pages.pending=0;
    }
#endif
    timing_header[3]=count; /* ZTIO maps entries to ZFR1 blocks in file order. */
    clean_bytes(timing_header,sizeof(timing_header));
    clean_bytes(writes,count*sizeof(writes[0]));
    return write_bytes(timing_header,sizeof(timing_header)) && write_bytes(writes,count*sizeof(writes[0]));
}

#ifdef Z72_RAW_RECORD_PACKED_BYTE
static void pack_step(void)
{
    const u32 current=load(&state);
    if (current!=ARMED && current!=FINISHED) return;
    const u32 index=load(&packed);
    if (index>=load(&produced)) return;
    struct packet *packet=queued_packet(index);
    u32 start=tick();
    packet->payload_bytes=z72_raw_pack_frame(packet_pixels(packet));
    packet->reserved[3]=tick()-start;
    packet->reserved[4]=start;
    clean_bytes(packet,64+packet->payload_bytes);
    publish(&packed,index+1u);
}

static void pack_worker(void *arg)
{
    (void)arg;
    for (;;) {
        pack_step();
        CALL(0x103dc539u,void,u32)(load(&state)==IDLE ? 20u : 1u);
    }
}
#endif

#ifdef Z72_RAW_PCM_BATCH_WRITE
#ifdef Z72_RAW_PCM_SEGMENTS
#ifdef Z72_RAW_SIZE_SPLIT
#include "movie_raw_size_segments.h"
#else
#include "movie_raw_pcm_segments.h"
#endif
#endif
static void record_write_time(u32 row,u32 start,u32 depth)
{
    const u32 elapsed=tick()-start;
#ifdef Z72_RAW_PCM_SEGMENTS
    if (PCM_STREAM_MODE) {
        segment_last_ordinal=queued_packet(row)->ordinal;
#ifdef Z72_RAW_SIZE_SPLIT
        row%=FRAME_LIMIT;
#else
        row-=segment_first;
#endif
    }
#endif
    writes[row]=(struct write_sample){start,elapsed,depth};
    footer.write_ms+=elapsed;
    if (elapsed>footer.max_write_ms) footer.max_write_ms=elapsed;
}
#endif

static void writer_step(void)
{
    const u32 current=load(&state);
    if (current==IDLE) return;
#ifdef Z72_RAW_AUTO_25P
    if (current==OPENING) {
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
        same_card_pages.enabled=same_card_pages.pending=0;
#endif
#ifdef Z72_RAW_PCM_SEGMENTS
        segment_first=segment_last_ordinal=0;
#ifdef Z72_RAW_SIZE_SPLIT
        meta_done=timeline_anchor=current_partial=0;
#ifndef Z72_RAW_REC_SPLIT_SNAPSHOT
        const u32 split=load(&raw_split_choice);
        file_cap=split ? 1ull<<(31u+split) : 0;
#endif
#endif
#endif
#ifdef Z72_RAW_PCM_AUDIO
        CALL(0x10361035u,void,void *,u32)(&pcm_clip,sizeof(pcm_clip));
#endif
#ifndef Z72_RAW_NATIVE_MENU
        file=open_clip();
        if (file<0) { close_clip(OPEN_FAILED_LABEL,0); return; }
#endif
        publish(&state,WAIT_PROFILE);
        record_label(0,ARMED_LABEL);
        return;
    }
    if (current==WAIT_PROFILE) {
        const int ended=native_movie_ended();
        const u32 encoder=BYTE(RECORDER_PHASE);
#ifdef Z72_RAW_PCM_AUTO_RATE
        if (movie_started && !ended && (encoder==1u || encoder==2u)) {
            log.source.type=BYTE(CAPTURE_TYPE);
            log.source.num=WORD(0xb1f5995cu);
            log.source.den=WORD(0xb1f59960u);
            log.source.width=WORD(GEOMETRY+0x18u)>>16;
            log.source.height=WORD(GEOMETRY+0x1cu)>>16;
            log.source.stride=WORD(PREVIEW+8u);
            log.source.format=WORD(GEOMETRY+0x14u);
            log.source.progress=WORD(MOVIE_PROGRESS_MS);
        }
#endif
        if (ended || load(&stop_requested)) {
            prepare_error="RAW: stopped before source ready";
            finish(NATIVE_STOP);
            return;
        }
#ifdef Z72_RAW_NVME_ADMIN_SNAPSHOT
        const u32 admin_start=log.nvme_admin[NVME_ADMIN_STARTED].status;
        if ((!admin_start || admin_start==NVME_ADMIN_BUSY) && movie_started &&
            (encoder==1u || encoder==2u) && RECORD_ONLY() && (load(&raw_settings)&4u))
            snapshot_nvme_admin(NVME_ADMIN_STARTED);
#endif
        if (!movie_started || (encoder!=1u && encoder!=2u) || !raw_start_ready(WORD(MOVIE_PROGRESS_MS)) ||
            (!STAGED_NEF_INPUT && RECORD_ONLY() && load(&mov_sink_state)!=MOV_SINK_ACTIVE)) return;
#ifdef Z72_RAW_50P_SINGLE_FRAME
        if (BYTE(CAPTURE_TYPE)==0x12u) capture_50p_frame();
#endif
        z72_mode=BYTE(CAPTURE_TYPE)==z72_uhd.type ? &z72_uhd : &z72_hd;
#ifdef Z72_RAW_UHD_2997
        z72_clip_fps_num=z72_source_fps_num();
#endif
#ifdef Z72_RAW_NEF_ROLLOVER
        if (nef_owner && z72_mode!=&z72_uhd) {
            prepare_error="RAW: mode changed between segments";
            finish(NATIVE_STOP);
            return;
        }
#endif
        if (!z72_raw_completed_slot()) return;
        if (!allocate_raw_media()) {
            prepare_error="RAW: no audio and segment buffer";
            finish(WRITE_FAILED);
            return;
        }
#ifdef Z72_RAW_NVME_ADMIN_SNAPSHOT
        if (z72_mode==&z72_uhd && RECORD_ONLY() && (load(&raw_settings)&4u))
            snapshot_nvme_admin(NVME_ADMIN_PRE_FRAME);
#endif
#ifdef Z72_RAW_RECORDER_TARGET
        log.route.settings=load(&raw_settings);
        log.route.factory=CALL(0x10100491u,u32,u32)(0xc3a);
        /* 0x10457BBC builds the kind-0 descriptor under this semaphore. */
        CALL(0x10360f19u,void,u32)(0xb188abd8u);
        log.route.descriptor_valid=WORD(0xb2982160u);
        log.route.descriptor_slot=BYTE(0xb2982178u);
        CALL(0x10360f13u,void,u32)(0xb188abd8u);
#ifdef Z72_RAW_EARLY_DUAL_START
        if (WORD(MOVIE_PROGRESS_MS)<START_DELAY_MS &&
            (log.route.descriptor_valid!=1u || log.route.descriptor_slot!=1u)) return;
#endif
#endif
#if defined(Z72_RAW_STAGED_NEF_TEST) || defined(Z72_RAW_STAGED_NEF_LIVE)
        if (z72_mode!=&z72_uhd || z72_clip_fps_num!=30000u || RECORD_ONLY() ||
            !(load(&raw_settings)&4u) || log.route.descriptor_valid!=1u || log.route.descriptor_slot!=0u) {
            prepare_error="RAW DMA to NEF: use 4K30 dual CFexpress";
            finish(NATIVE_STOP);
            return;
        }
#endif
#ifdef Z72_RAW_NATIVE_MENU
#ifdef Z72_RAW_REC_SPLIT_SNAPSHOT
        /* The menu remains editable in WAIT_PROFILE. Latch once per REC,
         * after the native recorder and its frame source are ready. */
        const u32 split=load(&raw_split_choice);
        file_cap=split ? 1ull<<(31u+split) : 0;
        log.recording.selection=split+1u;
        log.recording.cap_low=(u32)file_cap;
        log.recording.cap_high=(u32)(file_cap>>32);
#endif
        file=open_clip();
        if (file<0) { prepare_error=OPEN_FAILED_LABEL; finish(WRITE_FAILED); return; }
#if defined(Z72_RAW_PACKED_UHD_CFB) && !defined(Z72_RAW_STAGED_NEF_TEST) && !defined(Z72_RAW_STAGED_NEF_LIVE)
        /* Latch once from the opened destination. The writer and producer
         * use the same representation for this recording and all its parts. */
#ifdef Z72_RAW_COMPRESSION_MENU
        const u32 compression=load(&raw_compression_choice);
        packed_uhd=z72_mode==&z72_uhd && compression==RAW_COMPRESSION_OFF;
        log.compression.choice=compression;
#else
        packed_uhd=z72_mode==&z72_uhd && z72_clip_fps_num==30000u && clip_path[0]=='A';
#endif
#endif
#ifdef Z72_RAW_NEF_ROLLOVER
        log.nef_segment.open_progress_ms=WORD(MOVIE_PROGRESS_MS);
#endif
#ifdef Z72_RAW_NVME_PRE_PAYLOAD_TEST
        if (RECORD_ONLY() && z72_mode==&z72_uhd && clip_path[0]=='A' && log.nef_segment.index==1u) {
            test_write_before_first_frame();
            if (native_movie_ended() || load(&stop_requested)) {
                prepare_error="RAW: stopped before source ready";
                finish(NATIVE_STOP);
                return;
            }
        }
#endif
#endif
#ifdef Z72_RAW_FRAME_POOLS
#ifdef Z72_RAW_STAGED_NEF_LIVE
        if (!allocate_staged_stream()) {
            prepare_error="RAW DMA to NEF: insufficient buffer memory";
            finish(WRITE_FAILED);
            return;
        }
#endif
#ifdef Z72_RAW_STAGED_NEF_TEST
        if (!allocate_nef_reference()) {
            prepare_error="RAW DMA to NEF: no reference buffer";
            finish(WRITE_FAILED);
            return;
        }
#endif
#ifdef Z72_RAW_ALIGNED_WRITE
        const u32 alignment=PCM_STREAM_MODE ? WRITE_ALIGNMENT : 64u;
#ifdef Z72_RAW_NEF_STREAM
        if (z72_mode==&z72_uhd
#ifdef Z72_RAW_NEF_ROLLOVER
            && !nef_owner
#endif
            && !NEF_SLOT_ENCODE && !nef_allocate()) {
            prepare_error="RAW: no NEF hardware buffer";
            finish(WRITE_FAILED);
            return;
        }
#ifdef Z72_RAW_NEF_ADAPTIVE_SLOTS
        if (z72_mode==&z72_uhd && !packed_uhd) {
#ifdef Z72_RAW_NEF_24P_SLOT_BYTES
            if (!frame_capacity)
                nef_slot_payload_limit=z72_clip_fps_num==24000u ? Z72_RAW_NEF_24P_SLOT_BYTES : NEF_CAPACITY;
#endif
            if (!NEF_SLOT_ENCODE && !frame_capacity) nef_allocate_slot_pair();
            log.nef_buffers.payload_limit=nef_slot_payload_limit;
            log.nef_buffers.combined_request=nef_slot_pair_bytes;
        }
#endif
        const u32 span=packed_uhd ? WRITE_ALIGNMENT+packet_span() :
                       z72_mode==&z72_uhd ? nef_slot_payload_limit+64u+2u*WRITE_ALIGNMENT+NEF_SLOT_GUARD : packet_span();
#else
        const u32 span=packet_span()+(z72_mode==&z72_uhd ? 2u*WRITE_ALIGNMENT : 0u);
#endif
#else
        const u32 alignment=64u,span=packet_span();
#endif
        const u32 request=span+alignment-1u;
        log.requested=request;
#ifdef Z72_RAW_NEF_ROLLOVER
        if (nef_owner && frame_capacity) log.allocated_slots[0]=frame_capacity;
#endif
        const u32 pool_count=
#ifdef Z72_RAW_NEF_STREAM
            z72_mode==&z72_uhd && !NEF_SLOT_ENCODE ? 1u :
#endif
            2u;
        const u32 slot_limit=
#ifdef Z72_RAW_STAGED_NEF_TEST
            1u;
#else
#ifdef Z72_RAW_NEF_STREAM
            z72_mode==&z72_uhd && !NEF_SLOT_ENCODE ? 2u :
#endif
            QUEUE_SIZE;
#endif
#ifdef Z72_RAW_PREFETCH_NEF
        const int private_pool=z72_mode==&z72_uhd && z72_clip_fps_num==30000u &&
#ifdef Z72_RAW_COMPRESSION_MENU
                               compression==RAW_COMPRESSION_MIXED &&
#endif
                               !RECORD_ONLY() && clip_path[0]=='A' &&
                               log.route.descriptor_valid==1u && log.route.descriptor_slot==0u;
        /* Secure two full-size compressed destinations first. If the RAW
         * reserve cannot be allocated, keep using ordinary output slots. */
        for (u32 pass=0;pass<pool_count;++pass) {
            const u32 pool=private_pool ? 1u-pass : pass;
            if (private_pool && !pool && frame_capacity>=2u) reserve_prefetch_buffer();
#else
        for (u32 pool=0;pool<pool_count;++pool) {
#endif
            while (frame_capacity<slot_limit) {
                u32 info[4];
                u32 id=WORD(0xb1f7cbacu+pool*20u);
                if ((int)id<=0 || CALL(0x10104788u,int,u32,u32 *)(id,info)) break;
                if (!log.allocated_slots[pool]) {
                    log.free_bytes[pool]=info[2];
                    log.largest_block[pool]=info[3];
                }
                /* Keep headroom for ordinary services when adding optional slots. */
                u32 reserve=0x200000u;
#ifdef Z72_RAW_NEF_STREAM
                if (z72_mode==&z72_uhd) reserve=0x80000u;
#endif
                if (info[1] || info[3]<request || (frame_capacity && info[2]<request+reserve)) break;
                void *owner=CALL(0x1034fe31u,void *,u32,u32)(request,pool);
                if (!owner) break;
                u32 address=((u32)owner+alignment-1u)&~(alignment-1u);
                if (!z72_raw_identity_span(address,span)) {
                    CALL(0x1034fe7fu,void,void *)(owner);
                    break;
                }
                frame_owners[frame_capacity]=owner;
                frame_slots[frame_capacity++]=(struct packet *)address;
                ++log.allocated_slots[pool];
                invalidate_dma_pixels(address,span);
#ifdef Z72_RAW_PCM_AUTO_RATE
                /* Packed DMA rewrites only the raster. Initialize its page padding once,
                 * before handing the slot to the producer. */
                if (z72_mode==&z72_hd || packed_uhd) {
                    const u32 used=(packed_uhd ? WRITE_ALIGNMENT : 0u)+64u+clip_pixel_bytes();
                    void *pad=(void *)(address+used);
                    CALL(0x10361035u,void,void *,u32)(pad,span-used);
                    clean_bytes(pad,span-used);
                }
#endif
            }
        }
        timing_header[11]=frame_capacity;
        if (!frame_capacity
#ifdef Z72_RAW_NEF_STREAM
            || (!STAGED_NEF_INPUT && z72_mode==&z72_uhd && frame_capacity<2u)
#endif
        ) {
            prepare_error="RAW: no full frame buffer";
            finish(WRITE_FAILED);
            return;
        }
#else
        const u32 span=QUEUE_SIZE*packet_span();
        queue_owner=CALL(0x1034fe31u,void *,u32,u32)(span+63u,0);
        if (!queue_owner) {
            prepare_error="RAW CFB: memory unavailable";
            finish(WRITE_FAILED);
            return;
        }
        queue=(void *)(((u32)queue_owner+63u)&~63u);
        if (!z72_raw_identity_span((u32)queue,span)) {
            prepare_error="RAW CFB: DMA memory unavailable";
            finish(WRITE_FAILED);
            return;
        }
        invalidate_dma_pixels((u32)queue,span);
#endif
        header[4]=header[11]=z72_mode->width;
        header[5]=header[12]=z72_mode->height;
        header[6]=z72_mode->stride;
        header[15]=z72_mode->limit;
#ifdef Z72_RAW_NEF_STREAM
#ifdef Z72_RAW_NEF_DMA_STAGE
        const u32 nef_version=8u;
#else
        const u32 nef_version=7u;
#endif
        header[1]=z72_mode==&z72_uhd ? nef_version : FORMAT_VERSION;
        header[9]=z72_mode==&z72_uhd ? NEF_FRAME_STEP : FRAME_STEP;
#ifdef Z72_RAW_UHD_2997
        header[8]=z72_clip_fps_num;
        if (z72_fractional_fps_num(z72_clip_fps_num)) header[9]=1001u;
#endif
#endif
        clean_bytes(header,sizeof(header));
#ifdef Z72_RAW_SIZE_SPLIT
        if (PCM_STREAM_MODE) {
            header[1]=z72_mode==&z72_uhd && !packed_uhd ? 9u : 10u;
#ifdef Z72_RAW_MIXED_FRAMES
#ifdef Z72_RAW_DROP_EXPIRED
            clip_timeout_choice=load(&raw_timeout_choice);
#ifdef Z72_RAW_COMPRESSION_MENU
            if (z72_mode==&z72_uhd
#ifdef Z72_RAW_LIVE_REANCHOR
                || clip_timeout_choice!=RAW_TIMEOUT_STOP
#endif
            ) header[1]=MIXED_VERSION;
            log.compression.mixed_active=load(&prefetch_enabled);
#else
            if (private_pool) header[1]=MIXED_VERSION;
#endif
#else
            if (load(&prefetch_enabled)) header[1]=MIXED_VERSION;
#endif
#endif
            header[15]=0;
            same_card_pages.enabled=z72_mode==&z72_uhd && same_card_cfexpress();
#ifdef Z72_RAW_MOV_SYNC
            sync_install(!RECORD_ONLY() && movie_stem[0]);
#endif
#ifdef Z72_RAW_HD_LIVE_CAPTURE
            if (hd_live_capture() && load(&mov_sync.enabled)) header[1]=MIXED_VERSION;
#endif
            if (!write_stream_header()) {
                prepare_error="RAW: header write failed";
                finish(WRITE_FAILED);
                return;
            }
            publish(&state,ARMED);
            return;
        }
#endif
#ifdef Z72_RAW_PCM_SAME_CARD_PAGES
        if (z72_mode==&z72_uhd && same_card_cfexpress()) {
            same_card_pages.enabled=1;
            same_card_pages.pending=sizeof(header);
            CALL(0x10360f44u,void,void *,const void *,u32)(same_card_pages.tail,header,sizeof(header));
        } else
#endif
        if (!write_bytes(header,sizeof(header))) {
            prepare_error="RAW record: write failed";
            finish(WRITE_FAILED);
            return;
        }
        publish(&state,ARMED);
        return;
    }
#else
#ifdef Z72_RAW_RECORD_DEFERRED
    if (current==OPENING) {
        queue_owner=CALL(0x1034fe31u,void *,u32,u32)(QUEUE_SIZE*sizeof(*queue)+63u,0);
        if (!queue_owner) { close_clip("RAW CFB: memory unavailable",0); return; }
        queue=(void *)(((u32)queue_owner+63u)&~63u);
        record_label(0,ARMED_LABEL);
        publish(&state,ARMED);
        return;
    }
    if (current!=FINISHED) return;
    if (file<0 && movie_started) {
        const u32 progress=WORD(MOVIE_PROGRESS_MS);
        if (progress>timing_header[12]) timing_header[12]=progress;
    }
#ifdef Z72_RAW_COPY_DMA
    if (load(&dma_pending_channel)) return;
#endif
    if (BYTE(MOVIE_PHASE) || BYTE(RECORDER_PHASE)) return;
    if (file<0) {
        timing_header[13]=tick();
#ifdef Z72_RAW_RECORD_ROLLING
        first_saved=load(&consumed);
#endif
#else
    if (current==OPENING) {
#ifdef Z72_RAW_RECORD_HEAP
        queue_owner=CALL(0x1034fe31u,void *,u32,u32)(QUEUE_SIZE*sizeof(*queue)+63u,0);
        if (!queue_owner) { close_clip("RAW CFB: memory unavailable",0); return; }
        queue=(void *)(((u32)queue_owner+63u)&~63u);
#ifdef Z72_RAW_DMA_FAST_STREAM
        /* The CPU only writes packet headers. Pixel writes use DMA. */
        const u32 span=QUEUE_SIZE*sizeof(*queue);
        if (!z72_raw_identity_span((u32)queue,span)) {
            close_clip("RAW CFB: DMA memory unavailable",0);
            return;
        }
        invalidate_dma_pixels((u32)queue,span);
#endif
#endif
#endif
        file=open_clip();
        if (file<0) { close_clip(OPEN_FAILED_LABEL,0); return; }
#ifdef Z72_RAW_REFERENCE_PROFILE
        const u32 *clip_header=recording_header();
#else
        const u32 *clip_header=header;
#endif
        clean_bytes(clip_header,sizeof(header));
        if (!write_bytes(clip_header,sizeof(header))) { close_clip("RAW record: write failed",0); return; }
#ifndef Z72_RAW_RECORD_DEFERRED
#ifdef Z72_RAW_PAIR_AUTO
        record_label(0,recording_standby() ? ARMED_LABEL : "Pair v5: ready, press REC");
#else
        record_label(0,ARMED_LABEL);
#endif
#ifdef Z72_RAW_RECORD_STANDBY
        standby_start=tick();
#endif
        publish(&state,ARMED);
#endif
        return;
    }
#endif /* auto profile or fixed geometry */

#ifdef Z72_RAW_NEF_TWO_FRAME_BURST
    if (z72_mode==&z72_uhd && current!=FINISHED) return;
#endif
    const u32 written=load(&consumed);
#ifdef Z72_RAW_RECORD_ROLLING
    const u32 first=first_saved;
#else
    const u32 first=0;
#endif
#ifdef Z72_RAW_RECORD_PACKED_BYTE
    const u32 writable=load(&packed), ready=load(&produced);
#else
    const u32 ready=load(&produced), writable=ready;
#endif
#ifdef Z72_RAW_PCM_SEGMENTS
    if (current==FINISHED) {
        log.session.produced=ready;
        log.session.written=written;
        log.session.last_ordinal=segment_last_ordinal;
        log.session.reason=footer.reason;
    }
#ifdef Z72_RAW_SIZE_SPLIT
    if (PCM_STREAM_MODE && !service_metadata(written,current==FINISHED && (written>=writable || load(&card_error))))
#else
    if (!drain_segment(current==FINISHED && (written>=writable || load(&card_error))))
#endif
        publish(&card_error,1u);
#endif
    if (!load(&card_error) && written<writable) {
#ifdef Z72_RAW_PCM_SEGMENTS
#ifdef Z72_RAW_SIZE_SPLIT
        if (PCM_STREAM_MODE && frame_needs_file(written) &&
#else
        if (z72_mode==&z72_uhd && written-segment_first==Z72_RAW_NEF_BURST_LIMIT &&
#endif
            !start_next_segment(written)) {
            publish(&card_error,1u);
            return;
        }
#endif
#ifdef Z72_RAW_PCM_AUDIO
        if (PCM_STREAM_MODE && !written) {
            pcm_clip.active=1;
            pcm_clip.anchor=queued_packet(written)->start_tick;
#ifdef Z72_RAW_TERMINAL_ROW
            if (log.terminal_row.first_anchored) pcm_clip.anchor=log.terminal_row.audio_ms;
#endif
#ifdef Z72_RAW_SIZE_SPLIT
            timeline_anchor=pcm_clip.anchor;
#endif
        }
        pcm_poll(PCM_LIMIT);
#endif
#ifdef Z72_RAW_MOV_SYNC
        if (!written && load(&mov_sync.enabled)) mov_sync.first_ordinal=queued_packet(written)->ordinal;
#endif
#if defined(Z72_RAW_RECHECK_SLOT) || defined(Z72_RAW_RECOVER_NEXT_FRAME)
        if (written==first) {
            struct packet *packet=queued_packet(written);
#ifdef Z72_RAW_RECHECK_SLOT
            packet->reserved[3]=0x314e535au; /* ZSN1 */
            packet->reserved[4]=snapshot_retries;
#else
            packet->reserved[3]=0x3147525au; /* ZRG1 */
            packet->reserved[4]=recovered_gaps;
#endif
            clean_bytes(packet,64);
        }
#endif
#ifdef Z72_RAW_RECORD_PACKED_BYTE
        struct packet *packet=queued_packet(written);
        const u32 bytes=64+packet->payload_bytes;
#elif defined(Z72_RAW_RECORD_PACKED)
        struct packet *packet=queued_packet(written);
        u32 pack_start=tick();
        packet->payload_bytes=z72_raw_pack_frame(packet_pixels(packet));
        packet->reserved[3]=tick()-pack_start;
        const u32 bytes=64+packet->payload_bytes;
        clean_bytes(packet,bytes);
#endif
        u32 start=tick();
#ifdef Z72_RAW_RECORD_PACKED
        int saved=write_bytes(packet,bytes);
#elif defined(Z72_RAW_ALIGNED_WRITE)
        int saved=write_queued_aligned(written);
#else
        int saved=write_bytes(queued_packet(written),REFERENCE_VALUE(64u+clip_pixel_bytes(),64+LINEAR_STRIDE*LINEAR_HEIGHT));
#endif
        if (!saved) {
#ifdef Z72_RAW_PCM_SEGMENTS
            log.session.failure=5;
#endif
            publish(&card_error,1u);
            return;
        }
#ifdef Z72_RAW_PCM_BATCH_WRITE
        record_write_time(written-first,start,ready-written);
#else
        u32 elapsed=tick()-start;
        writes[written-first]=(struct write_sample){start,elapsed,ready-written};
        footer.write_ms+=elapsed;
        if (elapsed>footer.max_write_ms) footer.max_write_ms=elapsed;
#endif
#ifdef Z72_RAW_NVME_ADMIN_SNAPSHOT
        if (!written && z72_mode==&z72_uhd && RECORD_ONLY() && clip_path[0]=='A')
            snapshot_nvme_admin(NVME_ADMIN_FIRST_WRITE);
#endif
        publish(&consumed,written+1u);
#ifdef Z72_RAW_PCM_BATCH_WRITE
        if (volume_lock_held) {
            const u32 next=written+1u,depth=load(&produced)-next,begin=tick();
#ifdef Z72_RAW_SIZE_SPLIT
            if (frame_needs_file(next)) {
                release_volume_lock();
                return;
            }
#endif
            const int ok=write_queued_aligned(next);
            release_volume_lock(); /* Covers a packet rejected before its bulk write. */
            if (!ok) {
#ifdef Z72_RAW_PCM_SEGMENTS
                log.session.failure=5;
#endif
                publish(&card_error,1u);
            }
            else {
                record_write_time(next-first,begin,depth);
                publish(&consumed,next+1u);
            }
        }
#endif
        return;
    }
    if (current!=FINISHED) return;
#ifdef Z72_RAW_PREFETCH_NEF
    if (!prefetch_quiesce()) return;
#endif
#ifdef Z72_RAW_NEF_STREAM
    if (load(&nef_pending)) return;
#endif
#ifdef Z72_RAW_DMA_STREAM
    if (load(&dma_pending_channel)) return;
#endif
#ifdef Z72_RAW_SELECT_OUTPUT
    if (load(&profile_busy)) return;
#endif
#ifdef Z72_RAW_AUTO_25P
    if (prepare_error) { close_clip(prepare_error,0); return; }
#endif
#ifdef Z72_RAW_RECORD_PACKED_BYTE
    /* Let the encoder release all packets before freeing the heap on error. */
    if (writable!=ready) return;
#endif
    if (load(&card_error)) { close_clip("RAW record: write failed",0); return; }
    footer.magic=0x444e455au; /* ZEND */
    footer.bytes=sizeof(footer);
    const u32 saved_count=written-first;
#ifdef Z72_RAW_PCM_SEGMENTS
    const u32 local_count=saved_count-segment_first;
    footer.produced=footer.written=local_count;
#ifndef Z72_RAW_SIZE_SPLIT
    if (z72_mode==&z72_uhd && local_count==Z72_RAW_NEF_BURST_LIMIT) footer.reason=COMPLETE;
#endif
#else
    const u32 local_count=saved_count;
    footer.produced=ready-first;
    footer.written=saved_count;
#endif
#ifdef Z72_RAW_RECORD_ROLLING
    if (saved_count==FRAME_LIMIT) footer.reason=COMPLETE;
#endif
    clean_bytes(&footer,sizeof(footer));
    int saved=
#ifdef Z72_RAW_SIZE_SPLIT
        PCM_STREAM_MODE ? write_bytes(&footer,sizeof(footer)) :
#endif
        write_timing(local_count)
#ifdef Z72_RAW_PCM_AUDIO
        && pcm_write_track(local_count)
#endif
        && write_bytes(&footer,sizeof(footer));
#ifdef Z72_RAW_PCM_SEGMENTS
    if (!saved) log.session.failure=6;
#endif
    int normal=footer.reason==COMPLETE || footer.reason==NATIVE_STOP || footer.reason==USER_STOP;
    int pair_next=0;
#ifdef Z72_RAW_PAIR_AUTO
    pair_next=saved && footer.reason==COMPLETE && load(&auto_pair) && recording_standby();
#elif defined(Z72_RAW_NEF_ROLLOVER) && !defined(Z72_RAW_NEF_BURST_LIMIT)
    const u32 result=footer.copy_result;
    const int invalid_source=result==RAW_COPY_TIMEOUT || result==RAW_WINDOW_EXPIRED ||
        result==RAW_UNSAFE_SLOT || result==RAW_SAMPLE_CHANGED || result==RAW_SNAPSHOT_UNSTABLE;
    pair_next=saved && saved_count && z72_mode==&z72_uhd &&
        (footer.reason==QUEUE_FULL || footer.reason==SEQUENCE_GAP ||
         (footer.reason==COPY_FAILED && invalid_source));
#endif
    close_clip(!saved ? "RAW record: write failed" :
               !saved_count ? "RAW record: saved, no pixels" :
               normal ? "RAW record: saved clip" : "RAW record: saved partial clip",pair_next);
}

#ifdef Z72_RAW_NATIVE_MENU
static u32 arm_mode(u32 only);
#endif
static void card_worker(void *arg)
{
    (void)arg;
#ifdef Z72_RAW_PCI_CACHE_RANGE
    publish(&writer_stack_anchor,stack_pointer());
#endif
    for (;;) {
#ifdef Z72_RAW_NATIVE_MENU
        if (!(load(&raw_settings)&1u) && load(&state)==IDLE && !BYTE(MOVIE_PHASE) &&
            !BYTE(RECORDER_PHASE) && load(&mov_sink_state)!=MOV_SINK_ACTIVE) arm_mode(0);
#endif
#ifdef Z72_RAW_DMA_FAST_STREAM
        const u32 before=load(&consumed);
#endif
#ifdef Z72_RAW_PCM_AUDIO
        if (load(&state)==ARMED) pcm_poll(PCM_LIMIT);
#endif
        writer_step();
#ifdef Z72_RAW_DMA_FAST_STREAM
        const u32 after=load(&consumed);
        if (before!=after && after<load(&produced)) continue;
#endif
#ifdef Z72_RAW_RECORD_DEFERRED
        CALL(0x103dc539u,void,u32)(load(&state)==ARMED || file<0 ? 20u : 1u);
#else
        CALL(0x103dc539u,void,u32)(load(&state)==IDLE ? 20u : 1u);
#endif
    }
}

static int start_worker(struct worker_task *task,void (*entry)(void *),u32 core_mask)
{
    if (task->started) return 1;
    if (!(int)task->control[0]) {
        const u32 *source=CALL(0x103d3dffu,const u32 *,u32)(0x10005);
        u32 address=(u32)source;
        if ((address&3u) || !((address>=0x10020000u && address<=0x12497b78u-sizeof(task->descriptor)) ||
            (address>=0xb1800000u && address<=0xb5f00000u-sizeof(task->descriptor)))) return 0;
        for (u32 i=0;i<7u;++i) task->descriptor[i]=source[i];
        task->descriptor[0]=(task->descriptor[0]&~255u)|core_mask;
#ifdef Z72_RAW_PREVIEW_PRIORITY
        /* Keep the RAW copy below the native GUISRV (priority 26). */
        if (task==&capture_task
#ifdef Z72_RAW_PREFETCH_NEF
            || task==&prefetch_task
#endif
        ) task->descriptor[1]=Z72_RAW_PREVIEW_PRIORITY;
#endif
        task->descriptor[2]=8192u;
        task->descriptor[6]&=0xffffu;
        CALL(0x103dc46bu,int,void *,void (*)(void *),void *)(task->control,entry,task->descriptor);
    }
    task->started=(int)task->control[0]>0 && !CALL(0x103606dfu,int,void *)(task->control);
    return task->started;
}

#ifdef Z72_RAW_SELECT_OUTPUT
static u32 arm_mode(u32 only)
#else
static u32 arm(u32 a,u32 b,u32 c,u32 d)
#endif
{
#ifndef Z72_RAW_SELECT_OUTPUT
    (void)a; (void)b; (void)c; (void)d;
#endif
    if (load(&state)!=IDLE) return 0;
#ifdef Z72_RAW_ZRV_ONLY
    if (BYTE(MOVIE_PHASE) || BYTE(RECORDER_PHASE) || load(&mov_sink_state)==MOV_SINK_ACTIVE) {
        record_label(1,"ZRV only: stop REC first");
        return 0;
    }
#ifdef Z72_RAW_NATIVE_MENU
    (void)only;
#elif defined(Z72_RAW_SELECT_OUTPUT)
    publish(&record_only,only);
#endif
    if (!STAGED_NEF_INPUT && RECORD_ONLY() && !mov_sink_install()) {
        record_label(1,"ZRV only: MOV hook unavailable");
        return 0;
    }
#endif
#ifdef Z72_RAW_RECORD_STANDBY
    if (!z72_raw_movie_idle()) {
        record_label(1,"ZRV only: stop MOV first");
        return 0;
    }
#endif
#ifdef Z72_RAW_RECORD_CFB
    if (WORD(MEDIA_TABLE_SLOT)!=MEDIA_TABLE_AB) {
        record_label(1,"RAW CFB: storage map unavailable");
        return 0;
    }
#endif
    if (!start_worker(&capture_task,capture_worker,1u) || !start_worker(&card_task,card_worker,2u)) {
        record_label(1,"RAW record: task unavailable");
        return 0;
    }
#ifdef Z72_RAW_STAGED_NEF_LIVE
    if (!start_worker(&encoder_task,staged_encoder_worker,1u)) {
        record_label(1,"RAW DMA to NEF: encoder task unavailable");
        return 0;
    }
#endif
#ifdef Z72_RAW_RECORD_PACKED_BYTE
    if (!start_worker(&pack_task,pack_worker,2u)) {
        record_label(1,"RAW record: pack task unavailable");
        return 0;
    }
#endif
    u32 stock=RAW_IRQ_NATIVE;
    if (!__atomic_compare_exchange_n(&WORD(RAW_IRQ_SLOT),&stock,(u32)raw_irq,0,
                                    __ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) {
        record_label(1,"RAW record: IRQ unavailable");
        return 0;
    }
    publish(&stop_requested,0);
    begin_clip();
    return 0;
}

#if defined(Z72_RAW_SELECT_OUTPUT) && !defined(Z72_RAW_NATIVE_MENU)
static u32 arm_dual(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return arm_mode(0);
}
static u32 arm_only(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return arm_mode(1);
}
#endif

#ifndef Z72_RAW_NATIVE_MENU
static u32 stop(u32 a,u32 b,u32 c,u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    const u32 current=load(&state);
    if (current==OPENING || current==ARMED
#ifdef Z72_RAW_AUTO_25P
        || current==WAIT_PROFILE
#endif
#ifdef Z72_RAW_PAIR_AUTO
        || (current==FINISHED && load(&auto_pair))
#endif
    ) publish(&stop_requested,1u);
    return 0;
}

#ifdef Z72_RAW_REFERENCE_PROFILE
static u32 arm_standby(u32 a,u32 b,u32 c,u32 d)
{
    if (load(&state)!=IDLE) return 0;
#ifdef Z72_RAW_PAIR_AUTO
    publish(&auto_pair,1);
#endif
    publish(&z72_raw_reference_linear,0);
    return arm(a,b,c,d);
}

static u32 arm_linear(u32 a,u32 b,u32 c,u32 d)
{
    if (load(&state)!=IDLE) return 0;
#ifdef Z72_RAW_PAIR_AUTO
    publish(&auto_pair,0);
#endif
    publish(&z72_raw_reference_linear,1);
    return arm(a,b,c,d);
}
#endif

u32 z72_main(const struct z72_api *api)
{
    if (api->abi!=Z72_ABI || api->target!=Z72_TARGET || api->size<sizeof(*api)) return 1;
    const struct z72_menu_row rows[]={
#ifdef Z72_RAW_SELECT_OUTPUT
        {"MOV + ZRV: arm",arm_dual,0xd7}, {"ZRV only: arm",arm_only,0xd7},
        {"RAW record: finish clip",stop,0xd7},
#elif defined(Z72_RAW_REFERENCE_PROFILE)
        {ARM_LABEL,arm_standby,0xd7}, {"RAW record: finish clip",stop,0xd7},
#ifdef Z72_RAW_FULL_REFERENCE
        {"Pair: arm full RAW12",arm_linear,0xd7},
#else
        {"Pair: arm linear RAW12",arm_linear,0xd7},
#endif
#else
        {ARM_LABEL,arm,0xd7}, {"RAW record: finish clip",stop,0xd7},
#endif
    };
    return z72_menu_install_rows(&menu,rows,sizeof(rows)/sizeof(rows[0]));
}
#else
#include "movie_raw_native_menu.h"
#endif
