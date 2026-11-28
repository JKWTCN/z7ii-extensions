/* SPDX-License-Identifier: MIT */
/* C 1.70: 1038E8B6 publishes four timestamped descriptors under this mutex.
 * 1038DBE0 hands completed buffers to 10484C3A, trimming 192 bytes per ms. */
enum { PCM_RING=0xb21e26f0u, PCM_MUTEX=0xb1864708u,
#ifdef Z72_RAW_SIZE_SPLIT
       PCM_LIMIT=1920u*75u };
#else
       PCM_LIMIT=1920u*Z72_RAW_NEF_BURST_LIMIT };
#endif
/* Preserve the suffix of the descriptor straddling a file boundary. */
enum { RAW_PCM_BYTES=4u*PCM_LIMIT+192000u };
static unsigned char *pcm_data;
static u32 *media_page;
static void *raw_media_owner;
static struct {
    u32 active,anchor,lead,frames,blocks,last_slot,last_tick,last_bytes,error;
#ifdef Z72_RAW_PCM_2997_HANDOFF
    u32 last_address;
#endif
} pcm_clip;

#ifdef Z72_RAW_PCM_POOL
/* 1038A928 publishes {base, total bytes, max block, blocks}; 1038DB56
 * passes this same base and count to the native PCM recording service. */
static int pcm_pool_span(u32 addr,u32 bytes)
{
    const u32 table=0xb1f720a4u;
    const u32 base=WORD(table),total=WORD(table+4u),block=WORD(table+8u),slots=WORD(table+12u);
    log.pcm_pool.base=base; log.pcm_pool.total=total;
    log.pcm_pool.block=block; log.pcm_pool.slots=slots;
    log.pcm_pool.source=addr; log.pcm_pool.bytes=bytes;
    return total==0x754e0u && block==0x17760u && slots==5u &&
           base>=0xb7d00000u && base<=0xbaa00000u-total && !(base&63u) &&
           !(addr&3u) && bytes<=block && addr>=base && addr-base<=total-bytes;
}
#endif

/* Only the card worker reads or writes pcm_clip and pcm_data. */
#ifdef Z72_RAW_PCM_FINAL_DESCRIPTOR
static void pcm_poll_blocks(u32 limit,int include_head)
#else
static void pcm_poll(u32 limit)
#endif
{
    if (!pcm_clip.active || pcm_clip.error || pcm_clip.lead+pcm_clip.frames>=limit ||
        BYTE(0xb1f59931u)!=1u || (int)WORD(PCM_MUTEX)<=0 ||
        !CALL(0x10360f2fu,int,u32,u32)(PCM_MUTEX,0u)) return;

    const u32 head=WORD(PCM_RING+0x60),valid=WORD(PCM_RING+0x64);
    if (head<4u && valid && valid<=4u) {
        for (u32 back=valid;back;--back) {
            const u32 slot=(head+5u-back)%4u,entry=PCM_RING+24u*slot;
            const u32 stamp=WORD(entry+16),bytes=WORD(entry+4),addr=WORD(entry);
#ifdef Z72_RAW_PCM_FINAL_DESCRIPTOR
            if (slot==head && !include_head) continue;
#else
            /* The native stop path can submit only a prefix of the newest slot. */
            if (slot==head) continue;
#endif
            if (!bytes || bytes%4u || bytes>192000u || tick()-stamp>1200u) continue;
            int tolerance=8;
#ifdef Z72_RAW_PCM_FINAL_DESCRIPTOR
            if (slot==head) {
                /* The native callback published the address and byte count
                 * under PCM_MUTEX. Require the next block of the 29.97p
                 * pool, including a short final descriptor when present. */
                if (pcm_clip.last_bytes!=96096u || slot!=(pcm_clip.last_slot+1u)%4u ||
                    !pcm_pool_span(addr,bytes)) continue;
                const u32 offset=pcm_clip.last_address-log.pcm_pool.base;
                if (offset>=5u*96096u || offset%96096u ||
                    addr!=log.pcm_pool.base+(offset+96096u)%(5u*96096u)) continue;
                tolerance=34;
            }
#endif
            u32 skip=0;
#ifdef Z72_RAW_PCM_SEGMENTS
            if (pcm_clip.last_bytes) {
#else
            if (pcm_clip.blocks) {
#endif
                const int elapsed=(int)(stamp-pcm_clip.last_tick);
                if (elapsed<=0) continue;
                const int drift=elapsed-(int)(pcm_clip.last_bytes/192u);
#ifdef Z72_RAW_PCM_2997_HANDOFF
                /* Full 29.97p blocks contain 15 video frames (24024 sample
                 * pairs). An adjacent block in both native rings has an exact
                 * sample boundary even if its publication tick jitters. */
                if (z72_clip_fps_num==30000u && bytes==96096u && pcm_clip.last_bytes==bytes &&
                    pcm_pool_span(addr,bytes)) {
                    const u32 offset=pcm_clip.last_address-log.pcm_pool.base;
                    if (offset<5u*bytes && offset%96096u==0 &&
                        addr==log.pcm_pool.base+(offset+96096u)%(5u*96096u)) tolerance=34;
                }
#endif
                if (slot!=(pcm_clip.last_slot+1u)%4u || drift < -tolerance || drift > tolerance) {
#ifdef Z72_RAW_PCM_FINAL_DESCRIPTOR
                    if (slot==head) continue;
#endif
#ifdef Z72_RAW_PCM_2997_HANDOFF
                    if (!log.handoff.pcm_errors++) {
                        log.handoff.previous_slot=pcm_clip.last_slot;
                        log.handoff.slot=slot;
                        log.handoff.elapsed=(u32)elapsed;
                        log.handoff.previous_bytes=pcm_clip.last_bytes;
                    }
#endif
                    pcm_clip.error=2; break;
                }
#ifdef Z72_RAW_PCM_2997_HANDOFF
                if (drift < -8 || drift > 8) ++log.handoff.jitter_blocks;
#endif
            } else {
                u32 start=pcm_clip.anchor;
#ifdef Z72_RAW_PCM_PREROLL_MS
                /* anchor is the RAW task timestamp stored in ZFR1/ZAU1.
                 * Select earlier PCM for the exposure represented by that slot. */
                start-=Z72_RAW_PCM_PREROLL_MS;
#endif
                const int delta=(int)(stamp-start);
                if (delta < -1000 || delta>1000) continue;
                if (delta<0) {
                    skip=(u32)(-delta)*48u;
                    if (skip>=bytes/4u) continue;
                } else {
                    if ((u32)delta*48u>=limit) continue;
                    pcm_clip.lead=(u32)delta*48u;
                }
            }
#ifdef Z72_RAW_PCM_POOL
            if (!pcm_pool_span(addr,bytes)) { pcm_clip.error=4; break; }
#else
            if (addr+bytes<addr || !((addr>=0x40000000u && addr+bytes<=0x80000000u) ||
                (addr>=0xb1800000u && addr+bytes<=0xb6000000u)) ||
                !z72_raw_identity_span(addr,bytes)) { pcm_clip.error=4; break; }
#endif
            const u32 room=limit-pcm_clip.lead-pcm_clip.frames;
            u32 take=bytes/4u-skip;
#ifndef Z72_RAW_PCM_SEGMENTS
            if (take>room) take=room;
#endif
            CALL(0x10360facu,void,void *,const void *,u32)(pcm_data+4u*pcm_clip.frames,
                                                            (const void *)(addr+4u*skip),4u*take);
            pcm_clip.frames+=take;
            pcm_clip.last_bytes=bytes;
#ifdef Z72_RAW_PCM_2997_HANDOFF
            pcm_clip.last_address=addr;
#endif
            pcm_clip.last_slot=slot;
            pcm_clip.last_tick=stamp;
            ++pcm_clip.blocks;
#ifdef Z72_RAW_PCM_SEGMENTS
            if (take>=room) break;
#else
            if (take==room) break;
#endif
        }
    }
    CALL(0x10360f13u,void,u32)(PCM_MUTEX);
}

#ifdef Z72_RAW_PCM_FINAL_DESCRIPTOR
static void pcm_poll(u32 limit)
{
    pcm_poll_blocks(limit,0);
}
#endif

#ifdef Z72_RAW_PCM_SEGMENTS
static u32 pcm_track_header(u32 wanted,u32 h[16])
{
    const u32 lead=pcm_clip.lead<wanted ? pcm_clip.lead : wanted;
    const u32 n=pcm_clip.frames<wanted-lead ? pcm_clip.frames : wanted-lead;
    const u32 status=n==wanted ? 0u : pcm_clip.error ? pcm_clip.error : n ? 3u : 1u;
    h[0]=0x3155415au; h[1]=64; h[2]=4u*n; h[3]=1; h[4]=48000; h[5]=2; h[6]=16;
    h[7]=pcm_clip.anchor; h[8]=lead; h[9]=n; h[10]=status; h[11]=pcm_clip.blocks; h[12]=pcm_clip.last_bytes;
    h[13]=h[14]=h[15]=0;
    return n;
}
#endif

static int pcm_write_track(u32 count)
{
    if (!count || !pcm_clip.active) return 1;
    const u32 wanted=count*1920u,begin=tick();
    while (!pcm_clip.error && pcm_clip.frames+pcm_clip.lead<wanted && tick()-begin<600u) {
        pcm_poll(wanted);
        if (pcm_clip.frames+pcm_clip.lead<wanted) CALL(0x103dc539u,void,u32)(2u);
    }
#ifdef Z72_RAW_PCM_SEGMENTS
    u32 h[16];
    const u32 n=pcm_track_header(wanted,h);
    if (h[10]) ++log.session.pcm_partial;
#else
    u32 lead=pcm_clip.lead<wanted ? pcm_clip.lead : wanted;
    u32 n=pcm_clip.frames<wanted-lead ? pcm_clip.frames : wanted-lead;
    const u32 status=n==wanted ? 0u : pcm_clip.error ? pcm_clip.error : n ? 3u : 1u;
    const u32 h[16]={0x3155415au,64,4u*n,1,48000,2,16,pcm_clip.anchor,lead,n,status,
                     pcm_clip.blocks,pcm_clip.last_bytes};
#endif
    clean_bytes(h,sizeof(h));
    if (n) clean_bytes(pcm_data,4u*n);
    return write_bytes(h,sizeof(h)) && (!n || write_bytes(pcm_data,4u*n));
}
