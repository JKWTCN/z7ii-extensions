/* SPDX-License-Identifier: MIT */
enum { FS_LOCK=0xb1808ab4u, FS_VOLUMES=0xb2b464c0u, FS_RETRY=0x80005010u };
static u32 volume_handler,volume_begin,volume_end;
#ifdef Z72_RAW_PCI_CACHE_RANGE
#include "movie_raw_pci_cache.h"
#endif
#ifdef Z72_RAW_NVME_TIMING
#include "movie_raw_nvme_timing.h"
#endif
#ifdef Z72_RAW_EXFAT_PREFETCH
#include "movie_raw_exfat_prefetch.h"
#endif

#ifdef Z72_RAW_RECORDER_TARGET
static u32 nvme_block_controller(u32 block)
{
    if ((block&3u) || block<0xb1800058u || block>0xb5f00000u-0x1168u ||
        WORD(block-0x58u)!=0x44535358u) return 0;
    const u32 controller=WORD(block-0x58u+0x1168u);
    if ((controller&3u) || controller<0xb1800000u || controller>0xb5f00000u-0xb4u ||
        WORD(controller)!=0x454d564eu) return 0;
    return controller;
}

static u32 nvme_sync_counter(const void *buffer)
{
    if (volume_handler!=0x104825b3u) return 0;
    const u32 ops=WORD(0xb1801710u);
    if (ops!=0xb1801738u && ops!=0xb1801714u) return 0;
    log.route.adapter=WORD(ops+12u);
    if (ops!=0xb1801738u || log.route.adapter!=0x104b7a43u) return 0;
    /* Block I/O is namespace+0x58. Only inspect recognized mapped RAM. */
    const u32 block=WORD(0xb18012ecu);
    const u32 controller=nvme_block_controller(block);
    if (!controller) return 0;
    /* 0x10513B90..0x10513BDC chooses a 4 KiB bounce, MDTS, or 1024
     * logical blocks. Report that limit for this buffer when identifiable. */
    u32 space=block-0x58u,align=WORD(space+0x38u),identify=WORD(controller+0x4cu);
    log.route.io_alignment=align;
    log.route.command_bytes=0;
    if (align && ((u32)buffer&(align-1u))) log.route.command_bytes=4096u;
    else if (identify>=0xb1800000u && identify<=0xb5f00000u-0x4eu) {
        u32 mdts=BYTE(identify+0x4du),shift=12u+(BYTE(controller+0xb2u)&15u)+mdts;
        if (!mdts) log.route.command_bytes=1024u*WORD(space+0x34u);
        else if (shift<32u) log.route.command_bytes=1u<<shift;
    }
    /* 0x10515820 increments the halfword at controller+0xA6+2*queue.
     * The synchronous I/O path selects queue 1 at 0x105157E0. */
    return controller+0xa8u;
}
#endif

static int timed_volume_read(void *context,u32 sector,u32 count,void *buffer)
{
    u32 start=tick();
    int result=0;
#ifdef Z72_RAW_EXFAT_PREFETCH
    if (!bitmap_read(context,sector,count,buffer))
#endif
        result=CALL(log.volume.reader,int,void *,u32,u32,void *)(context,sector,count,buffer);
    log.volume.read_ms+=tick()-start;
    ++log.volume.read_calls;
    return result;
}

/* 0x1047E768 passes the device context, sector, sector count and buffer.
 * The native filesystem lock serializes dispatch and the mount table. */
static int timed_volume_write(void *context,u32 sector,u32 count,const void *buffer)
{
#ifdef Z72_RAW_EXFAT_PREFETCH
    invalidate_bitmap_write(context,sector,count);
#endif
#ifdef Z72_RAW_RECORDER_TARGET
    u32 address=(u32)buffer;
    int payload=address>=volume_begin && address<volume_end &&
                count<=((volume_end-address)>>__builtin_ctz(log.volume.sector_bytes));
    u32 counter=payload ? nvme_sync_counter(buffer) : 0,before=counter ? HALF(counter) : 0;
#endif
    if (volume_handler==0x10444b79u) {
        /* The stock partition adapter tail-calls context->operations[1]. */
        u32 operations=WORD((u32)context+8u);
        log.volume.device_write=WORD(operations+4u);
    }
    u32 start=tick();
#ifdef Z72_RAW_PCI_CACHE_RANGE
    int scoped=begin_pci_sync(counter,address,count*log.volume.sector_bytes);
#endif
#ifdef Z72_RAW_NVME_TIMING
    u32 *nvme_slot=begin_nvme_timing(counter);
#endif
    int result=CALL(volume_handler,int,void *,u32,u32,const void *)(context,sector,count,buffer);
#ifdef Z72_RAW_NVME_TIMING
    if (nvme_slot) end_nvme_timing(nvme_slot);
#endif
#ifdef Z72_RAW_PCI_CACHE_RANGE
    if (scoped) {
        end_pci_sync();
        ++log.cache.scoped_calls;
    }
#endif
    u32 elapsed=tick()-start;
#ifdef Z72_RAW_RECORDER_TARGET
    if (payload) {
#else
    u32 address=(u32)buffer;
    if (address>=volume_begin && address<volume_end &&
        count<=((volume_end-address)>>__builtin_ctz(log.volume.sector_bytes))) {
#endif
        ++log.volume.data_calls;
        const u32 bytes=count*log.volume.sector_bytes;
        log.volume.data_bytes+=bytes;
#ifdef Z72_RAW_NVME_TIMING
        if (log.volume.data_bytes<bytes) ++log.nvme.payload_bytes_high;
#endif
        log.volume.data_ms+=elapsed;
#ifdef Z72_RAW_RECORDER_TARGET
        if (counter && nvme_sync_counter(buffer)==counter) {
            ++log.route.observed_calls;
            log.route.io_commands+=(unsigned short)(HALF(counter)-before);
        }
#endif
        if (count>log.volume.max_sectors) log.volume.max_sectors=count;
    } else {
        ++log.volume.other_calls;
        log.volume.other_ms+=elapsed;
    }
    return result;
}

#ifdef Z72_RAW_PCM_MOV_CONTENTION
#include "movie_raw_mov_contention.h"
#endif
#ifdef Z72_RAW_PCM_BATCH_WRITE
/* Owned only by the ZRV card worker. Never wait for another frame under this lock. */
static u32 volume_lock_held,volume_lock_start;

static void release_volume_lock(void)
{
    if (!volume_lock_held) return;
    const u32 held=tick()-volume_lock_start;
    if (held>log.write_batch.max_hold_ms) log.write_batch.max_hold_ms=held;
    volume_lock_held=0;
    CALL(0x103d58fbu,void,u32)(FS_LOCK);
}

static int write_timed_bulk(const void *data,u32 bytes,int allow_pair)
#else
static int write_timed_bulk(const void *data,u32 bytes)
#endif
{
    u32 drive=(u32)file>>16;
    if (drive>=2u) return write_bytes(data,bytes);
    u32 volume=FS_VOLUMES+136u*drive,start=tick();
#ifdef Z72_RAW_PCM_BATCH_WRITE
    const int reused=volume_lock_held;
    if (reused) ++log.write_batch.reuses;
    else {
#ifdef Z72_RAW_PCM_MOV_CONTENTION
        if (mov_volume_slot) {
            mov_wait_overlap=0;
            publish(&mov_wait_start,start);
        }
#endif
        CALL(0x103d5919u,void,u32)(FS_LOCK);
#ifdef Z72_RAW_PCM_MOV_CONTENTION
        publish(&mov_wait_start,0);
#endif
        volume_lock_start=tick();
#ifdef Z72_RAW_PCM_MOV_CONTENTION
        if (mov_volume_slot && volume_lock_start-start>log.mov_io.max_wait_ms) {
            log.mov_io.max_wait_ms=volume_lock_start-start;
            log.mov_io.mov_ms_in_max_wait=mov_wait_overlap;
        }
#endif
        volume_lock_held=1;
        ++log.write_batch.locks;
    }
    u32 acquired=reused ? start : volume_lock_start,handler=WORD(volume+0x1cu),reader=WORD(volume+0x18u);
#else
    CALL(0x103d5919u,void,u32)(FS_LOCK);
    u32 acquired=tick(),handler=WORD(volume+0x1cu),reader=WORD(volume+0x18u);
#endif
    u32 sector=*(const volatile unsigned short *)(volume+0x3cu);
#ifdef Z72_RAW_PCM_MOV_CONTENTION
    if (handler==(u32)mov_volume_write && mov_volume_slot==(u32 *)(volume+0x1cu)) handler=MOV_CF_WRITE;
#endif
    log.volume.handler=handler;
    log.volume.reader=reader;
    log.volume.sector_bytes=sector;
    if (handler<0x10020000u || handler>=0x12497b78u || reader<0x10020000u || reader>=0x12497b78u ||
        sector<512u || sector>4096u || (sector&(sector-1u))) {
#ifdef Z72_RAW_PCM_BATCH_WRITE
        release_volume_lock();
#else
        CALL(0x103d58fbu,void,u32)(FS_LOCK);
#endif
        return write_bytes(data,bytes);
    }
    volume_handler=handler;
#ifdef Z72_RAW_EXFAT_PREFETCH
    begin_bitmap_prefetch(volume,reader,handler,sector);
#endif
    volume_begin=(u32)data;
    volume_end=volume_begin+bytes;
    ++log.volume.requests;
    log.volume.lock_ms+=acquired-start;
    publish((u32 *)(volume+0x1cu),(u32)timed_volume_write);
    publish((u32 *)(volume+0x18u),(u32)timed_volume_read);

    /* Mirror the partial-write and one-error retry of 0x1047109C while
     * holding its lock. Its inner write still allocates and checks all LBAs. */
    u32 total=0,status,retry=0;
    do {
        u32 actual=0,remaining=bytes-total;
        status=CALL(0x104666ffu,u32,int,const void *,u32,u32 *)(
            file,(const unsigned char *)data+total,remaining,&actual);
        if (actual>remaining) { status=1; break; }
        total+=actual;
        if (status) {
            if (status!=FS_RETRY || retry) break;
            if (total==bytes) status=0;
            retry=1;
        } else {
            if (!actual) break;
            retry=0;
        }
    } while (total<bytes);

    publish((u32 *)(volume+0x1cu),handler);
    publish((u32 *)(volume+0x18u),reader);
#ifdef Z72_RAW_PCM_MOV_CONTENTION
    if (allow_pair && handler==MOV_CF_WRITE && sector==512u && !status && total==bytes) {
        if (load(&mov_fs_token)) log.mov_io.file_seen=1;
        mov_contention_install((u32 *)(volume+0x1cu));
    }
    if (mov_volume_slot==(u32 *)(volume+0x1cu) && handler==MOV_CF_WRITE)
        publish(mov_volume_slot,(u32)mov_volume_write);
#endif
    log.volume.locked_ms+=tick()-acquired;
#ifdef Z72_RAW_PCM_BATCH_WRITE
    if (reused || !allow_pair || status || total!=bytes || mxic_record_disabled ||
        load(&produced)<=load(&consumed)+1u || !mxic_record_route_enabled()) release_volume_lock();
#else
    CALL(0x103d58fbu,void,u32)(FS_LOCK);
#endif
    file_bytes+=total;
    return !status && total==bytes;
}

#ifdef Z72_RAW_PCM_BOUNDED_WRITE
static int write_frame_bulk(const unsigned char *data,u32 bytes,int same_card)
{
    if (!same_card || z72_clip_fps_num!=30000u)
        return write_timed_bulk(data,bytes,same_card);

    /* v15's 66 ms whole-frame write overlapped a 70 ms encoder callback.
     * Keep the MOV hook and native file accounting for each piece, then
     * release the filesystem lock before giving a pending encode CPU time. */
    const u32 limit=2u<<20;
    while (bytes) {
        const u32 piece=bytes<limit ? bytes : limit;
        const int ok=write_timed_bulk(data,piece,1);
        release_volume_lock();
        if (!ok) return 0;
        bytes-=piece;
        data+=piece;
        if (bytes && load(&nef_pending)) CALL(0x103dc539u,void,u32)(1);
    }
    return 1;
}
#endif
