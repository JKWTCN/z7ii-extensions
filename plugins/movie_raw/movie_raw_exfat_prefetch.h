/* SPDX-License-Identifier: MIT */
enum { BITMAP_SECTOR_BYTES=512, BITMAP_PREFETCH_SECTORS=8 };
static _Alignas(64) unsigned char bitmap_prefetch[BITMAP_SECTOR_BYTES*BITMAP_PREFETCH_SECTORS];
static struct {
    const void *context;
    u32 first,end,cached,cached_count;
} bitmap;

static void begin_bitmap_prefetch(u32 volume,u32 reader,u32 handler,u32 sector_bytes)
{
    bitmap.context=0;
    bitmap.cached_count=0;
    if (reader!=0x1048250du || handler!=0x104825b3u || BYTE(volume+0x3au)!=4u ||
        sector_bytes!=BITMAP_SECTOR_BYTES ||
        !z72_raw_identity_span((u32)bitmap_prefetch,sizeof(bitmap_prefetch))) return;

    /* Match the allocation bitmap sector mapping in 0x1047C6E8. */
    const u32 cluster=WORD(volume+(WORD(volume+0x84u) ? 0x74u : 0x70u));
    const u32 shift=BYTE(volume+0x2au)-BYTE(volume+0x28u);
    const u32 bits=WORD(volume+0x50u);
    if (cluster<2u || shift>16u || !bits) return;
    const unsigned long long first=WORD(volume+0x5cu)+((unsigned long long)(cluster-2u)<<shift);
    const u32 sectors=(bits-1u)/(BITMAP_SECTOR_BYTES*8u)+1u;
    if (first+sectors>WORD(volume+0x58u)) return;
    bitmap.first=(u32)first;
    bitmap.end=bitmap.first+sectors;
    bitmap.context=(const void *)WORD(volume);
}

static int bitmap_read(void *context,u32 sector,u32 count,void *dest)
{
    if (context!=bitmap.context || !bitmap.context || count!=1u ||
        sector<bitmap.first || sector>=bitmap.end) return 0;
    ++log.bitmap.candidates;
    if (sector>=bitmap.cached && sector-bitmap.cached<bitmap.cached_count) ++log.bitmap.hits;
    else {
        u32 blocks=bitmap.end-sector;
        if (blocks>BITMAP_PREFETCH_SECTORS) blocks=BITMAP_PREFETCH_SECTORS;
        if (blocks==1u) return 0;
        const u32 bytes=blocks*BITMAP_SECTOR_BYTES;
        bitmap.cached_count=0;
        invalidate_dma_pixels((u32)bitmap_prefetch,bytes);
        ++log.bitmap.prefetch_calls;
        log.bitmap.prefetch_sectors+=blocks;
        if (CALL(log.volume.reader,int,void *,u32,u32,void *)(context,sector,blocks,bitmap_prefetch)) {
            ++log.bitmap.retries;
            bitmap.context=0;
            return 0;
        }
        invalidate_dma_pixels((u32)bitmap_prefetch,bytes);
        bitmap.cached=sector;
        bitmap.cached_count=blocks;
    }
    CALL(0x10360f44u,void,void *,const void *,u32)(
        dest,bitmap_prefetch+(sector-bitmap.cached)*BITMAP_SECTOR_BYTES,BITMAP_SECTOR_BYTES);
    clean_bytes(dest,BITMAP_SECTOR_BYTES);
    return 1;
}

static void invalidate_bitmap_write(const void *context,u32 sector,u32 count)
{
    if (context==bitmap.context && bitmap.cached_count && sector<bitmap.cached+bitmap.cached_count &&
        (sector>=bitmap.cached || count>bitmap.cached-sector)) {
        bitmap.cached_count=0;
        ++log.bitmap.invalidations;
    }
}
