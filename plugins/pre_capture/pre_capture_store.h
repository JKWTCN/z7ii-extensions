/* SPDX-License-Identifier: MIT */
/* Worker-only EXIF ordering and native DCF file creation. Pixel data remains
 * in separately owned pool blocks; only a bounded JPEG header is DMA-read. */
#define STORE_HEADER_BYTES 0x20000u
struct photo_stamp {
    z72_u32 date,clock,fraction,width,height,valid;
};
static struct photo_stamp photo_stamps[RING_SLOTS];
static _Alignas(64) unsigned char store_header[STORE_HEADER_BYTES];
/* 104ef466 reads only this raw datetime at record.context+2864 when emitting
 * the native completed-image notification. No borrowed capture context. */
static _Alignas(4) unsigned char store_context[0x2870];
static z72_u32 store_stats[12],store_times[RING_SLOTS][4];
static z72_u32 native_card_id,store_first_id,store_last_id;
/* State (0 new, 1 reserved, 2 failed, 3 saved), reserved/written counts,
 * reservation start/duration, actual medium, result, control generation. */
static z72_u32 store_plan[8],store_ids[RING_SLOTS],store_order[RING_SLOTS];
static char store_first_path[32],store_last_path[32];
static z72_u32 check_jpeg(const char *path
);

static z72_u32 exif_u16(const unsigned char *p,z72_u32 little)
{ return little ? p[0]|((z72_u32)p[1]<<8) : ((z72_u32)p[0]<<8)|p[1]; }
static z72_u32 exif_u32(const unsigned char *p,z72_u32 little)
{
    return little ? exif_u16(p,1)|(exif_u16(p+2,1)<<16) :
        (exif_u16(p,0)<<16)|exif_u16(p+2,0);
}
static z72_u32 exif_ascii(const unsigned char *t,z72_u32 size,z72_u32 offset,
    z72_u32 tag,const unsigned char **text,z72_u32 *length,z72_u32 little)
{
    if (offset>size || size-offset<2) return 0;
    z72_u32 count=exif_u16(t+offset,little);
    if (count>256 || count>(size-offset-2)/12) return 0;
    for (z72_u32 i=0;i<count;++i) {
        const unsigned char *e=t+offset+2+i*12;
        if (exif_u16(e,little)!=tag) continue;
        if (exif_u16(e+2,little)!=2) return 0;
        z72_u32 n=exif_u32(e+4,little),p=n<=4 ? offset+2+i*12+8 : exif_u32(e+8,little);
        if (!n || p>size || n>size-p) return 0;
        *text=t+p; *length=n; return 1;
    }
    return 0;
}
static z72_u32 exif_time(const unsigned char *t,z72_u32 size,struct photo_stamp *stamp)
{
    if (size<8 || !((t[0]=='I' && t[1]=='I') || (t[0]=='M' && t[1]=='M'))) return 0;
    z72_u32 little=t[0]=='I',offset=exif_u32(t+4,little),sub=0;
    if (exif_u16(t+2,little)!=42 || offset>size || size-offset<2) return 0;
    z72_u32 count=exif_u16(t+offset,little);
    if (count>256 || count>(size-offset-2)/12) return 0;
    for (z72_u32 i=0;i<count;++i) {
        const unsigned char *e=t+offset+2+i*12;
        if (exif_u16(e,little)==0x8769 && exif_u16(e+2,little)==4 && exif_u32(e+4,little)==1)
            sub=exif_u32(e+8,little);
    }
    const unsigned char *date,*fraction;
    z72_u32 length,n;
    if (!sub || !exif_ascii(t,size,sub,0x9003,&date,&length,little) || length!=20 || date[19] ||
        !exif_ascii(t,size,sub,0x9291,&fraction,&n,little) || n<2 || n>10 || fraction[n-1]) return 0;
    const z72_u32 separators[5]={4,7,10,13,16};
    const char expected[5]={':',':',' ',':',':'};
    for (z72_u32 i=0;i<5;++i) if (date[separators[i]]!=(unsigned char)expected[i]) return 0;
    z72_u32 fields[6]={0},f=0;
    for (z72_u32 i=0;i<19;++i) {
        if (i==4 || i==7 || i==10 || i==13 || i==16) { ++f; continue; }
        if (date[i]<'0' || date[i]>'9') return 0;
        fields[f]=fields[f]*10+date[i]-'0';
    }
    z72_u32 year=fields[0],month=fields[1],day=fields[2];
    static const unsigned char days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if (year<1980 || year>2107 || !month || month>12 || !day ||
        day>days[month-1]+(month==2 && !(year%4) && (year%100 || !(year%400))) ||
        fields[3]>23 || fields[4]>59 || fields[5]>59) return 0;
    z72_u32 value=0;
    for (z72_u32 i=0;i<9;++i) {
        value*=10;
        if (i<n-1) {
            if (fraction[i]<'0' || fraction[i]>'9') return 0;
            value+=fraction[i]-'0';
        }
    }
    stamp->date=year*10000+month*100+day;
    stamp->clock=fields[3]*3600+fields[4]*60+fields[5];
    stamp->fraction=value;
    return 1;
}
/* Parse only marker segments before SOS; never search compressed pixels for
 * an EXIF signature. TIFF offsets remain relative to their APP1 boundary. */
__attribute__((noinline)) z72_u32 photo_header(const unsigned char *data,z72_u32 bytes,struct photo_stamp *stamp)
{
    stamp->valid=0; stamp->width=stamp->height=0;
    if (bytes<4 || data[0]!=255 || data[1]!=0xd8) return 0;
    z72_u32 p=2,has_time=0;
    for (z72_u32 i=0;i<128 && p+4<=bytes;++i) {
        if (data[p++]!=255) return 0;
        while (p<bytes && data[p]==255) ++p;
        if (p>=bytes) return 0;
        z72_u32 marker=data[p++];
        if (marker==0xda || marker==0xd9) break;
        if (bytes-p<2) return 0;
        z72_u32 n=exif_u16(data+p,0);
        if (n<2 || n>bytes-p) return 0;
        if (marker==0xe1 && n>=8 && data[p+2]=='E' && data[p+3]=='x' &&
            data[p+4]=='i' && data[p+5]=='f' && !data[p+6] && !data[p+7]) {
            if (has_time || !exif_time(data+p+8,n-8,stamp)) return 0;
            has_time=1;
        }
        if ((marker>=0xc0 && marker<=0xc3) || (marker>=0xc5 && marker<=0xc7) ||
            (marker>=0xc9 && marker<=0xcb) || (marker>=0xcd && marker<=0xcf)) {
            if (n<8) return 0;
            stamp->height=exif_u16(data+p+3,0); stamp->width=exif_u16(data+p+5,0);
        }
        p+=n;
        if (has_time && stamp->width && stamp->height) { stamp->valid=1; return 1; }
    }
    return 0;
}
static z72_u32 store_prepare(void)
{
    for (z72_u32 i=0;i<RING_SLOTS;++i) {
        photo_stamps[i].valid=0;
        for (z72_u32 j=0;j<4;++j) store_times[i][j]=0;
    }
    store_first_id=store_last_id=0; store_first_path[0]=store_last_path[0]=0;
    for (z72_u32 i=0;i<RING_SLOTS;++i) if (ring[i].state==2) {
        z72_u32 bytes=ring[i].size<STORE_HEADER_BYTES ? ring[i].size : STORE_HEADER_BYTES;
        CALL(0x1034fa29u,void,void *,z72_u32)(store_header,bytes);
        z72_u32 r=ring_copy_bytes((z72_u32)store_header,ring[i].address,bytes,0);
        CALL(0x1034fa29u,void,void *,z72_u32)(store_header,bytes);
        if (r || !photo_header(store_header,bytes,&photo_stamps[i])) {
            ++store_stats[1]; store_stats[9]=1; return 0;
        }
        ++store_stats[0];
    }
    return 1;
}
static z72_u32 store_before(z72_u32 i,z72_u32 j)
{
    const struct photo_stamp *a=&photo_stamps[i],*b=&photo_stamps[j];
    return a->date<b->date || (a->date==b->date && (a->clock<b->clock ||
        (a->clock==b->clock && (a->fraction<b->fraction ||
        (a->fraction==b->fraction && ring[i].order<ring[j].order)))));
}
static z72_u32 store_reserve(void)
{
    z72_u32 count=0;
    for (z72_u32 i=0;i<RING_SLOTS;++i) if (ring[i].state==2) {
        z72_u32 at=count;
        while (at && store_before(i,store_order[at-1])) {
            store_order[at]=store_order[at-1]; --at;
        }
        store_order[at]=i; ++count;
    }
    z72_u32 previous=0;
    for (z72_u32 i=0;i<count;++i) {
        z72_u32 slot=store_order[i],job=ring_card_job[slot];
        if (!job || job>CARD_RECORDS || LOAD(card_jobs[job-1][4])!=2 ||
            LOAD(card_queue_frames[job-1].discard)) return 0;
        z72_u32 id=LOAD(card_jobs[job-1][3]);
        if (!CALL(0x105094e5u,z72_u32,z72_u32)(id)) return 0;
        store_ids[slot]=id; ++store_plan[1]; store_stats[4]=id;
        if (!i) store_plan[5]=id>>29;
        if ((id>>29)!=store_plan[5] ||
            (i && (id&0x1fffffe0u)<=(previous&0x1fffffe0u))) { store_plan[6]=3; return 0; }
        previous=id;
    }
    return 1;
}

static z72_u32 store_native(z72_u32 slot)
{
    struct cached_jpeg *frame=&ring[slot];
    const struct photo_stamp *stamp=&photo_stamps[slot];
    z72_u32 id=store_ids[slot],actual=0;
    if (!id || !CALL(0x105094e5u,z72_u32,z72_u32)(id)) return 3;
    store_stats[9]=3;
    z72_u32 object=CALL(0x10567c09u,z72_u32,z72_u32)(id);
    if (!object) return 3;
    z72_u32 path=CALL(0x10567f61u,z72_u32,z72_u32)(object),length=0;
    if (pre_capture_ram(path,32)) {
        while (length<32 && BYTE(path+length)) { destination[length]=(char)BYTE(path+length); ++length; }
    }
    if (!length || length==32) { CALL(0x1056788bu,void,z72_u32)(object); return 3; }
    destination[length]=0;
    store_stats[5]=CALL(0x10567cafu,z72_u32,z72_u32)(object);
    z72_u32 result=3;
    if (store_stats[5]==SDK_OK) {
        z72_u32 source[3]={1,frame->address,frame->size};
        store_stats[9]=4;
        store_stats[6]=CALL(0x10567dadu,z72_u32,z72_u32,void *,z72_u32,z72_u32 *)(object,source,frame->size,&actual);
        store_stats[9]=5;
        _Alignas(8) z72_u32 record[14]={0};
        record[0]=id; record[2]=(z72_u32)store_context; record[6]=actual;
        z72_u32 year=stamp->date/10000;
        store_context[0x2864]=(unsigned char)year; store_context[0x2865]=(unsigned char)(year>>8);
        store_context[0x2866]=(unsigned char)((stamp->date/100)%100);
        store_context[0x2867]=(unsigned char)(stamp->date%100);
        store_context[0x2868]=(unsigned char)(stamp->clock/3600);
        store_context[0x2869]=(unsigned char)((stamp->clock/60)%60);
        store_context[0x286a]=(unsigned char)(stamp->clock%60);
        store_context[0x286b]=0;
        z72_u32 millis=stamp->fraction/1000000u;
        store_context[0x286c]=(unsigned char)millis; store_context[0x286d]=(unsigned char)(millis>>8);
        /* The original finalizer closes/registers, releases the object,
         * applies its flush policy and emits 041f0002. It owns that release. */
        STORE(pre_notice_id,id);
        store_stats[7]=CALL(0x104ef467u,z72_u32,z72_u32,z72_u32,void *)(object,
            store_stats[6]==SDK_OK && actual==frame->size ? 0 : 3,record);
        if (!store_stats[7]) {
            {
            /* A reserved ID can be read before its delayed JPEG exists.
             * The native reader caches that error as valid ImageInfo and a
             * later cache hit returns 1 without reparsing. Invalidate only
             * this closed file through the native locked/dirty API, release
             * that reference, then fill the complete metadata from disk.
             */
            ++metadata_stats[0]; store_stats[9]=7;
            metadata_stats[2]=0;
            z72_u32 closed=CALL(0x10567c09u,z72_u32,z72_u32)(id);
            if (closed) {
                CALL(0x10567fffu,void,z72_u32)(closed);
                CALL(0x1056788bu,void,z72_u32)(closed);
                metadata_stats[2]=CALL(0x1055e1a1u,z72_u32,z72_u32)(id);
            }
            if (metadata_stats[2]==1) { ++metadata_stats[1]; result=0; }
            }
        }
            finish_pre_notice();
    } else {
        CALL(0x1056788bu,void,z72_u32)(object);
    }
    if (result) return result;
    store_stats[9]=6;
    store_stats[8]=0xffffffffu; /* Flush is managed by the native finalizer. */
    result=check_jpeg(destination
    );
    if (result) { ++store_stats[10]; return result; }
    last_id=id; store_last_id=id;
    for (z72_u32 i=0;i<=length;++i) store_last_path[i]=destination[i];
    if (!store_first_id) {
        store_first_id=id;
        for (z72_u32 i=0;i<=length;++i) store_first_path[i]=destination[i];
    }
    store_stats[9]=0; return 0;
}
