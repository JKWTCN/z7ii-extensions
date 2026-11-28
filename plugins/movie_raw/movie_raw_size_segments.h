/* SPDX-License-Identifier: MIT */
/* ZRV9 pages hold a RAW frame or a batch of per-frame timings and PCM.
 * Only the card worker changes the file positions and audio cursor. */
enum { MEDIA_FRAMES=25, MEDIA_PAGE=4096,
       /* 24000/1001 is the longest supported frame at 48 kHz. */
#ifdef Z72_RAW_PCM_AUTO_RATE
       MAX_PCM_SAMPLES=2002,
#else
       MAX_PCM_SAMPLES=1920,
#endif
#ifdef Z72_RAW_MOV_SYNC
       MEDIA_SYNC_BYTES=4u,
#else
       MEDIA_SYNC_BYTES=0u,
#endif
       MEDIA_BYTES=(64u+MEDIA_FRAMES*(sizeof(struct write_sample)+MAX_PCM_SAMPLES*4u+MEDIA_SYNC_BYTES)+4095u)&~4095u };
static int allocate_raw_media(void)
{
    if (raw_media_owner) return 1;
    const u32 bytes=MEDIA_BYTES+RAW_PCM_BYTES;
    for (u32 pool=0;pool<2u;++pool) {
        void *owner=CALL(0x1034fe31u,void *,u32,u32)(bytes+63u,pool);
        if (!owner) continue;
        const u32 page=((u32)owner+63u)&~63u;
        if (z72_raw_identity_span(page,bytes)) {
            raw_media_owner=owner;
            media_page=(u32 *)page;
            pcm_data=(unsigned char *)(page+MEDIA_BYTES);
            return 1;
        }
        CALL(0x1034fe7fu,void,void *)(owner);
    }
    return 0;
}
static unsigned long long file_cap;
static u32 segment_first,segment_last_ordinal,meta_done,timeline_anchor,current_partial;
static struct {
    int fd;
    unsigned long long length;
    u32 first,end,partial;
    struct stream_footer footer;
} previous_file={.fd=-1};

static u32 page_bytes(u32 n) { return (n+MEDIA_PAGE-1u)&~(MEDIA_PAGE-1u); }
#ifdef Z72_RAW_PCM_AUTO_RATE
static int pcm_has_five_frame_period(void)
{
#ifdef Z72_RAW_PCM_HD_HIGH_RATE
    if (z72_clip_fps_num==60000u) return 1;
#endif
    return z72_clip_fps_num==30000u;
}
#endif
static u32 group_samples(u32 start,u32 frames)
{
#ifdef Z72_RAW_PCM_AUTO_RATE
    if (pcm_has_five_frame_period()) {
        const u32 phase=start%5u;
        const u32 samples=z72_clip_fps_num==30000u ? 8008u : 4004u;
        return (phase+frames)*samples/5u-phase*samples/5u;
    }
    if (z72_clip_fps_num==24000u) return frames*2002u;
#ifdef Z72_RAW_PCM_HD_HIGH_RATE
    if (z72_clip_fps_num==50u) return frames*960u;
#endif
#else
    (void)start;
#endif
    return frames*1920u;
}
static u32 group_bytes(u32 start,u32 frames)
{
    u32 row=sizeof(struct write_sample);
#ifdef Z72_RAW_MOV_SYNC
    if (load(&mov_sync.enabled)) row+=MEDIA_SYNC_BYTES;
#endif
    return page_bytes(64u+frames*row+group_samples(start,frames)*4u);
}

#ifdef Z72_RAW_PCM_AUTO_RATE
static u32 frame_elapsed_ms(u32 n)
{
    if (z72_clip_fps_num==25u) return n*40u;
    if (z72_clip_fps_num==30000u) return n/30u*1001u+n%30u*1001u/30u;
#ifdef Z72_RAW_PCM_HD_HIGH_RATE
    if (z72_clip_fps_num==50u) return n*20u;
    if (z72_clip_fps_num==60000u) return n/60u*1001u+n%60u*1001u/60u;
#endif
    return n/24u*1001u+n%24u*1001u/24u;
}
#endif

static int frame_needs_file(u32 index)
{
    if (!file_cap || index==segment_first) return 0;
    const u32 first=meta_done>segment_first ? meta_done : segment_first;
    const u32 frames=index+1u-first,tail=frames%MEDIA_FRAMES;
    const unsigned long long reserved=(unsigned long long)(frames/MEDIA_FRAMES)*group_bytes(first,MEDIA_FRAMES)
                                     +(tail ? group_bytes(first,tail) : 0u);
    return file_bytes+page_bytes(64u+queued_packet(index)->payload_bytes)+reserved+sizeof(footer)>file_cap;
}

static int write_stream_header(void)
{
    CALL(0x10361035u,void,void *,u32)(media_page,MEDIA_PAGE);
    CALL(0x10360facu,void,void *,const void *,u32)(media_page,header,64u);
#ifdef Z72_RAW_PCM_AUTO_RATE
    if (pcm_has_five_frame_period()) media_page[16]=segment_first%5u;
#endif
#ifdef Z72_RAW_DROP_EXPIRED
    if (header[1]==MIXED_VERSION) media_page[17]=clip_timeout_choice;
#endif
#ifdef Z72_RAW_MOV_SYNC
    if (load(&mov_sync.enabled)) {
        media_page[1]=13u;
        media_page[18]=SYNC_MAGIC;
        media_page[19]=mov_sync.session;
    }
#endif
    media_page[20]=0x31524f5au; /* ZOR1, followed by TIFF Orientation. */
    media_page[21]=raw_latch_orientation();
    clean_bytes(media_page,MEDIA_PAGE);
    return write_bytes(media_page,MEDIA_PAGE);
}

static int media_write(int fd,unsigned long long *position,const void *p,u32 n)
{
    clean_bytes(p,n);
    u32 actual=0;
    const u32 result=CALL(0x1047109du,u32,int,const void *,u32,u32 *)(fd,p,n,&actual);
    if (actual<=n) *position+=actual;
    return !result && actual==n;
}

static int emit_group(int fd,unsigned long long *position,u32 first,u32 count)
{
    const u32 wanted=group_samples(meta_done,count),lead=pcm_clip.lead<wanted ? pcm_clip.lead : wanted;
    const u32 n=pcm_clip.frames<wanted-lead ? pcm_clip.frames : wanted-lead;
    const u32 pcm_end=64u+count*sizeof(writes[0])+4u*n;
#ifdef Z72_RAW_MOV_SYNC
    const int synced=load(&mov_sync.enabled)!=0;
    const u32 used=pcm_end+(synced ? MEDIA_SYNC_BYTES*count : 0u);
#else
    const u32 used=pcm_end;
#endif
    const u32 total=page_bytes(used);
    const u32 status=lead+n==wanted && !lead ? 0u : pcm_clip.error ? pcm_clip.error : n ? 3u : 1u;
    const u32 h[16]={0x3156415au,64,total,meta_done-first,count,lead,n,status,48000,2,16};
    CALL(0x10360facu,void,void *,const void *,u32)(media_page,h,64);
    struct write_sample *rows=(struct write_sample *)(media_page+16);
    for (u32 i=0;i<count;++i) rows[i]=writes[(meta_done+i)%FRAME_LIMIT];
    if (n) CALL(0x10360facu,void,void *,const void *,u32)(rows+count,pcm_data,4u*n);
#ifdef Z72_RAW_MOV_SYNC
    if (synced) {
        u32 *mapping=media_page+pcm_end/4u;
        for (u32 i=0;i<count;++i) mapping[i]=sync_movie_frame(mov_sync.first_ordinal+meta_done+i);
    }
#endif
    CALL(0x10361035u,void,void *,u32)((unsigned char *)media_page+used,total-used);
    if (!media_write(fd,position,media_page,total)) return 0;
    meta_done+=count;
    u32 *partial=fd==previous_file.fd ? &previous_file.partial : &current_partial;
    if (status && !*partial) {
        *partial=1;
        ++log.session.pcm_partial;
    }
    if (lead+n<wanted || pcm_clip.error) {
        CALL(0x10361035u,void,void *,u32)(&pcm_clip,sizeof(pcm_clip));
        pcm_clip.active=1;
#ifdef Z72_RAW_PCM_AUTO_RATE
        pcm_clip.anchor=timeline_anchor+frame_elapsed_ms(meta_done);
#else
        pcm_clip.anchor=timeline_anchor+meta_done*40u;
#endif
    } else {
        /* The source can overlap the destination. Copy forward in sample order. */
        pcm_clip.frames-=n;
        if (n) for (u32 i=0;i<4u*pcm_clip.frames;++i) pcm_data[i]=pcm_data[4u*n+i];
        pcm_clip.lead-=lead;
    }
    return 1;
}

static int service_metadata(u32 written,int finishing)
{
    const u32 began=tick();
    for (;;) {
        const int previous=previous_file.fd>=0;
        const u32 end=previous ? previous_file.end : written;
        if (!previous && load(&card_error)) return 1;
        const u32 left=end-meta_done;
        if (left) {
            if (!previous && !finishing && left<MEDIA_FRAMES) return 1;
            const u32 count=left<MEDIA_FRAMES ? left : MEDIA_FRAMES;
#ifdef Z72_RAW_MOV_SYNC_DEFER_METADATA
            /* The newest RAW packet can reach the writer before the matching
             * recorder notice. Let the writer advance without sealing its row. */
            if (!finishing && load(&mov_sync.enabled) && written-meta_done<count+3u &&
                !sync_movie_frame(mov_sync.first_ordinal+meta_done+count-1u)) return 1;
#endif
            if (!pcm_clip.error && pcm_clip.lead+pcm_clip.frames<group_samples(meta_done,count)) {
#ifdef Z72_RAW_PCM_FINAL_DESCRIPTOR
                const int tail=finishing && footer.reason==NATIVE_STOP && !RECORD_ONLY() &&
                               z72_mode==&z72_uhd && z72_clip_fps_num==30000u;
                pcm_poll_blocks(PCM_LIMIT,tail);
#else
                pcm_poll(PCM_LIMIT);
#endif
                if (!pcm_clip.error && pcm_clip.lead+pcm_clip.frames<group_samples(meta_done,count)) {
                    if (!finishing && written-meta_done<count+MEDIA_FRAMES) return 1;
                    if (finishing && tick()-began<650u) {
                        CALL(0x103dc539u,void,u32)(2u);
                        continue;
                    }
                }
            }
            if (!emit_group(previous ? previous_file.fd : file,previous ? &previous_file.length : &file_bytes,
                            previous ? previous_file.first : segment_first,count)) {
                log.session.failure=previous ? 3u : 6u;
                if (previous) {
                    close_segment_file(previous_file.fd,previous_file.length);
                    previous_file.fd=-1;
                }
                return 0;
            }
            if (!finishing && (!previous || meta_done!=end)) return 1;
        } else if (!previous) return 1;
        if (previous && meta_done==end) {
            const int ok=media_write(previous_file.fd,&previous_file.length,&previous_file.footer,sizeof(footer));
            const u32 closed=close_segment_file(previous_file.fd,previous_file.length);
            previous_file.fd=-1;
            if (!ok || closed) { log.session.failure=3; return 0; }
            ++log.session.sealed;
        }
    }
}

static int start_next_segment(u32 written)
{
    if (previous_file.fd>=0 && !service_metadata(written,1)) return 0;
#ifdef Z72_RAW_NUMBERED_EXTENSIONS
    const int next=open_clip_part();
#else
    char old_path[sizeof(clip_path)];
    for (u32 i=0;i<sizeof(old_path);++i) old_path[i]=clip_path[i];
    const int next=open_clip();
#endif
    struct stream_footer end=footer;
    end.magic=0x444e455au; end.bytes=sizeof(end); end.reason=COMPLETE;
    end.produced=end.written=written-segment_first;
    end.ordinal=segment_last_ordinal;
    end.tail_count=load(&tail_token)>>RAW_TOKEN_SHIFT;
    if (next<0) {
#ifndef Z72_RAW_NUMBERED_EXTENSIONS
        for (u32 i=0;i<sizeof(old_path);++i) clip_path[i]=old_path[i];
#endif
        log.session.failure=1;
        if (service_metadata(written,1) && media_write(file,&file_bytes,&end,sizeof(end)))
            failed_roll_footer_written=1;
        return 0;
    }
    previous_file.fd=file;
    previous_file.length=file_bytes;
    previous_file.first=segment_first;
    previous_file.end=written;
    previous_file.footer=end;
    previous_file.partial=current_partial;
    current_partial=0;
    file=next;
    file_bytes=0;
    segment_first=written;
    footer.write_ms=footer.max_write_ms=0;
    if (write_stream_header()) return 1;
    log.session.failure=4;
    return 0;
}
