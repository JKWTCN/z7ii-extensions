/* SPDX-License-Identifier: MIT */
#include "pixel_shift_merge_pixels.h"
#include "pixel_shift_merge_metadata.h"
#include "pixel_shift_merge_lens.h"

enum {
    MERGE_FILE_ERROR=0x10c, MERGE_LAYOUT_ERROR, MERGE_MEMORY_ERROR,
    MERGE_OUTPUT_ERROR, MERGE_CANCELLED, MERGE_DECODE_ERROR,
    MERGE_REVIEW_ERROR, MERGE_READ_CHUNK=0x10000, MERGE_KEEP_OPEN=4
};
enum { MERGE_TIFF=1, MERGE_NEFX=2 };
_Static_assert((unsigned)MERGE_INPUT_BYTES>=MERGE_READ_CHUNK,"Merge input fits a compressed read");

struct merge_lossless {
    z72_u32 next, end, bits;
    unsigned available, row;
    int previous[2][2];
};

struct merge_reader {
    unsigned char *buffer;
    unsigned used, count;
};

enum { MERGE_CODE_BITS=12 };
struct merge_codes {
    unsigned char prefix[256];
    short delta[1u<<MERGE_CODE_BITS];
};
static struct merge_codes merge_codes;

struct merge_source {
    char path[96];
    z72_u32 strip, size;
    struct merge_lossless compressed;
    unsigned packed, open;
    struct merge_offset position;
    unsigned position_valid;
    struct merge_alignment alignment;
    unsigned short *carry;
    z72_u32 file[10];
    struct merge_reader reader;
};
struct merge_color {
    unsigned black[3], gain[3], orientation;
};
static struct merge_source merge_sources[PIXEL_MAX_SHOTS];

static int merge_open_read(z72_u32 file[10],const char *path)
{
    for (unsigned i=0;i<10;++i) file[i]=0;
    CALL(0x104d3c01u,void,z72_u32 *,const char *)(file,path);
    return CALL(0x104d3c05u,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,0,0)==NX_OK;
}

static unsigned merge_ratio(unsigned numerator,unsigned denominator)
{
    /* These ratios are computed once for file metadata. Keep the module
     * freestanding on the Cortex-A9, which has no integer divide instruction. */
    unsigned quotient=0;
    for (unsigned bit=1u<<16;bit;bit>>=1) {
        unsigned trial=quotient|bit;
        if (trial*denominator<=numerator) quotient=trial;
    }
    return quotient;
}

static int merge_position_axis(const unsigned char *data,int *position)
{
    int value=(int)nx_u32(data);
    unsigned units=nx_u32(data+4);
    if (!units || units>16384 || value<-(int)(8*units) || value>(int)(8*units)) return 0;
    unsigned pixels=merge_ratio(merge_abs(value)*MERGE_SHIFT_ONE+units/2,units);
    *position=value<0 ? -(int)pixels : (int)pixels;
    return 1;
}

static int merge_read_note(z72_u32 *file,z72_u32 note,z72_u32 size,struct merge_source *source,
                           struct merge_color *color)
{
    if (size<NX_SCAN_BYTES || !nx_read(file,note,nx_scan,NX_SCAN_BYTES) ||
        !nx_equal(nx_scan,(const unsigned char *)"Nikon\0\2\x11\0\0II*\0\10\0\0",18) ||
        nx_u16(nx_scan+18)>(NX_SCAN_BYTES-24)/NX_ENTRY) return 0;
    const unsigned char *ifd=nx_scan+18;
    const unsigned char *crop=nx_entry((unsigned char *)ifd,0x45);
    const unsigned char *black=nx_entry((unsigned char *)ifd,0x3d);
    const unsigned char *wb=nx_entry((unsigned char *)ifd,0x0c);
    const unsigned char *position=nx_entry((unsigned char *)ifd,0x54);
    if (!nx_match(crop,3,4) || !nx_match(black,3,4) || !nx_match(wb,5,4)) return 0;
    unsigned crop_at=nx_u32(crop+8),black_at=nx_u32(black+8),wb_at=nx_u32(wb+8);
    unsigned pos_at=nx_match(position,7,20) ? nx_u32(position+8) : size;
    if (!source->packed) {
        const unsigned char *format=nx_entry((unsigned char *)ifd,0x93);
        const unsigned char *predictor=nx_entry((unsigned char *)ifd,0x96);
        if (!nx_match(format,3,1) || nx_u16(format+8)!=NX_RAW_LOSSLESS ||
            !predictor || nx_u16(predictor+2)!=7) return 0;
        unsigned at=nx_u32(predictor+8),bytes=nx_u32(predictor+4);
        if (bytes<10 || at>size-10 || bytes>size-10-at ||
            !nx_read(file,note+10+at,nx_scan,10) || nx_u16(nx_scan)!=0x3046u)
            return 0;
        struct merge_lossless *state=&source->compressed;
        state->next=source->strip;
        state->row=state->bits=state->available=0;
        for (unsigned y=0;y<2;++y) {
            for (unsigned x=0;x<2;++x) {
                unsigned first=nx_u16(nx_scan+2+4*y+2*x);
                if (first>=16384) return 0;
                state->previous[y][x]=(int)first;
            }
        }
    }
    if (crop_at>size-18 || black_at>size-18 || wb_at>size-42 ||
        !nx_read(file,note+10+crop_at,nx_scan,8) ||
        nx_u16(nx_scan)!=MERGE_LEFT || nx_u16(nx_scan+2)!=MERGE_TOP ||
        nx_u16(nx_scan+4)!=MERGE_WIDTH || nx_u16(nx_scan+6)!=MERGE_HEIGHT ||
        !nx_read(file,note+10+black_at,nx_scan,8)) return 0;

    /* MakerNote black levels are in RGGB order. WB rationals are R,B,G1,G2. */
    color->black[0]=nx_u16(nx_scan);
    color->black[1]=(nx_u16(nx_scan+2)+nx_u16(nx_scan+4))/2;
    color->black[2]=nx_u16(nx_scan+6);
    if (!nx_read(file,note+10+wb_at,nx_scan,32)) return 0;
    unsigned gains[4];
    for (unsigned i=0;i<4;++i) {
        unsigned n=nx_u32(nx_scan+8*i),d=nx_u32(nx_scan+8*i+4);
        if (!n || !d || n>4096 || d>4096 || n*8<d || n>d*8) return 0;
        gains[i]=merge_ratio(n*1024,d);
    }
    unsigned green=(gains[2]+gains[3])/2;
    const unsigned wb_rgb[]={gains[0],green,gains[1]};
    for (unsigned i=0;i<3;++i) {
        if (color->black[i]>2048 || wb_rgb[i]>green*4) return 0;
        unsigned wb_q10=merge_ratio(wb_rgb[i]*1024,green);
        unsigned white_q10=merge_ratio(16383u*1024,16383u-color->black[i]);
        color->gain[i]=(wb_q10*white_q10+512)/1024;
    }
    source->position_valid=pos_at<=size-30 && nx_read(file,note+10+pos_at,nx_scan,20) &&
        nx_equal(nx_scan,(const unsigned char *)"0100",4) &&
        merge_position_axis(nx_scan+4,&source->position.x) &&
        merge_position_axis(nx_scan+12,&source->position.y);
    return 1;
}

static int merge_read_layout(z72_u32 file[10],struct merge_source *source,struct merge_color *color,
                             struct merge_metadata *metadata,struct merge_ca_profile *profile)
{
    z72_u32 note,size,root;
    int packed=0;
    if (!nx_find_note(file,&note,&size,&root,&packed) || !nx_directory(file,root))
        return 0;
    unsigned sub_offset=nx_u32(nx_entry(nx_scan,0x14a)+8);
    color->orientation=nx_tag_value(0x112,3);
    if (color->orientation<1 || color->orientation>8 || !nx_read(file,sub_offset,nx_scan,8) ||
        !nx_directory(file,nx_u32(nx_scan+4)) ||
        !nx_match(nx_entry(nx_scan,0x828e),1,4) || nx_tag_value(0x11c,3)!=1 ||
        nx_tag_value(0x106,3)!=32803 || nx_tag_value(0x115,3)!=1 ||
        nx_tag_value(0x116,4)!=MERGE_RAW_HEIGHT ||
        !nx_equal(nx_entry(nx_scan,0x828e)+8,(const unsigned char *)"\0\1\1\2",4))
        return 0;
    source->strip=nx_tag_value(0x111,4);
    source->size=file[6];
    source->packed=(unsigned)packed;
    unsigned bytes=nx_tag_value(0x117,4);
    if (source->strip<8 || source->strip>source->size || !bytes || bytes>source->size-source->strip)
        return 0;
    source->compressed.end=source->strip+bytes;
    if (profile) merge_ca_read(file,profile);
    if (!merge_read_note(file,note,size,source,color)) return 0;
    if (metadata) merge_read_metadata(file,root,metadata);
    return 1;
}

static unsigned merge_prepare(const z72_u32 *ids,unsigned count,struct merge_color *color,
                              struct merge_metadata *metadata,struct merge_ca_profile *profile,
                              int (*stop_requested)(void),void (*progress)(unsigned))
{
    for (unsigned i=0;i<count;++i) {
        if (stop_requested()) return MERGE_CANCELLED;
        struct merge_source *source=&merge_sources[i];
        z72_u32 file[10];
        if (!nx_get_path(ids[i],source->path) || !merge_open_read(file,source->path)) return MERGE_FILE_ERROR;
        struct merge_color current;
        int layout=merge_read_layout(file,source,&current,i ? 0 : metadata,i ? 0 : profile);
        if (!nx_close(file)) return MERGE_FILE_ERROR;
        if (!layout) return MERGE_LAYOUT_ERROR;
        if (!i) {
            for (unsigned c=0;c<3;++c) {
                color->gain[c]=current.gain[c];
                color->black[c]=current.black[c];
            }
            color->orientation=current.orientation;
        } else {
            if (current.orientation!=color->orientation || source->path[0]!=merge_sources[0].path[0])
                return MERGE_LAYOUT_ERROR;
            for (unsigned c=0;c<3;++c)
                if (current.black[c]!=color->black[c]) return MERGE_LAYOUT_ERROR;
        }
        progress(merge_ratio(5*(i+1),count));
    }
    return 0;
}

static void merge_close_sources(unsigned count)
{
    for (unsigned i=0;i<count;++i) {
        struct merge_source *source=&merge_sources[i];
        if (source->open) {
            nx_close(source->file);
            source->open=0;
        }
    }
}

static int merge_open_source(struct merge_source *source,unsigned row,unsigned char *buffer)
{
    if (!merge_open_read(source->file,source->path)) return 0;
    source->open=1;
    z72_u32 offset=source->strip+(source->packed ? row*MERGE_ROW_BYTES : 0);
    if (source->file[7] || source->file[6]!=source->size || !nx_seek(source->file,offset)) return 0;
    source->reader=(struct merge_reader){buffer,0,0};
    return 1;
}

static unsigned merge_first_row(const struct merge_alignment *plan)
{
    return (unsigned)(MERGE_TOP+plan->y-(plan->fy<0 ? MERGE_HALO : 0));
}

static void merge_keep_sources_open(unsigned count,unsigned char *buffer)
{
    for (unsigned i=0;i<count;++i) {
        struct merge_source *source=&merge_sources[i];
        if (!merge_open_source(source,merge_first_row(&source->alignment),buffer)) {
            /* The per-band reader also works with a single available handle. */
            merge_close_sources(i+1);
            return;
        }
        if (!source->packed) buffer+=MERGE_READ_CHUNK;
    }
}

/* Nikon 14-bit lossless NEF stores signed differences from the preceding
 * sample of the same Bayer phase. These are the canonical code counts and
 * symbols of the format, corroborated against the C 1.70 encoding tables.
 * https://github.com/LibRaw/LibRaw/blob/master/src/decoders/decoders_dcraw.cpp#L849 */
static int merge_delta(unsigned value,unsigned length)
{
    unsigned limit=1u<<length;
    int delta=(int)(value&(limit-1u));
    if (length && (unsigned)delta<(limit>>1)) delta-=(int)(limit-1u);
    return delta;
}

static void merge_huffman(struct merge_codes *codes)
{
    static const unsigned char counts[]={0,1,4,2,2,3,1,2};
    static const unsigned char differences[]={7,6,8,5,9,4,10,3,11,12,2,0,1,13,14};
    unsigned index=0;
    unsigned char *lookup=codes->prefix;
    for (unsigned length=1;length<=8;++length) {
        for (unsigned i=0;i<counts[length-1];++i) {
            unsigned char entry=(unsigned char)((length<<4)|differences[index++]);
            unsigned span=1u<<(8-length);
            while (span--) *lookup++=entry;
        }
    }
    for (unsigned i=0;i<(1u<<MERGE_CODE_BITS);++i) {
        unsigned symbol=codes->prefix[i>>(MERGE_CODE_BITS-8)];
        unsigned length=symbol&15u,total=(symbol>>4)+length;
        /* A zero entry leaves longer codes for the general decoder. */
        codes->delta[i]=total<=MERGE_CODE_BITS ?
            (short)(16*merge_delta(i>>(MERGE_CODE_BITS-total),length)+(int)total) : 0;
    }
}

static int merge_read_input(z72_u32 *file,int sequential,z72_u32 offset,void *data,
                            unsigned bytes)
{

    return sequential ? nx_read_current(file,data,bytes) : nx_read(file,offset,data,bytes);
}

struct merge_bits {
    const unsigned char *cursor,*limit;
    /* Bits occupy the high end of the word. Next is the offset after limit. */
    z72_u32 bits,next;
    unsigned available;
};

static int merge_refill(struct merge_bits *stream,unsigned char *buffer,z72_u32 end,
                        z72_u32 *file,int sequential)
{
    unsigned left=end-stream->next;
    unsigned size=left>MERGE_READ_CHUNK ? MERGE_READ_CHUNK : left;
    if (!size) return MERGE_DECODE_ERROR;
    if (!merge_read_input(file,sequential,stream->next,buffer,size))
        return MERGE_FILE_ERROR;
    stream->cursor=buffer;
    stream->limit=buffer+size;
    stream->next+=size;
    return 0;
}

static inline __attribute__((always_inline)) int merge_fill(
    struct merge_bits *stream,unsigned char *buffer,z72_u32 end,unsigned need,
    z72_u32 *file,int sequential)
{
    while (stream->available<need) {
        if (stream->cursor==stream->limit) {
            int status=merge_refill(stream,buffer,end,file,sequential);
            if (status) return status;
        }
        if (stream->available<=16 && stream->limit-stream->cursor>=2) {
            unsigned word=((unsigned)stream->cursor[0]<<8)|stream->cursor[1];
            stream->bits|=word<<(16-stream->available);
            stream->cursor+=2;
            stream->available+=16;
        } else {
            stream->bits|=(unsigned)*stream->cursor++<<(24-stream->available);
            stream->available+=8;
        }
    }
    return 0;
}

static unsigned merge_read_lossless(struct merge_reader *reader,struct merge_lossless *state,
                                    const struct merge_codes *codes,unsigned short *pixels,unsigned row,
                                    unsigned rows,z72_u32 *file,int sequential)
{
    if (state->row>row) return MERGE_DECODE_ERROR;
    unsigned char *buffer=reader->buffer;
    struct merge_bits stream={buffer+reader->used,buffer+reader->count,state->bits,
        state->next+reader->count-reader->used,state->available};
    z72_u32 end=state->end;
    unsigned stop=row+rows;
    for (unsigned y=state->row;y<stop;++y) {
        int *previous=state->previous[y&1u];
        int predict=previous[0],other=previous[1];
        unsigned short *dest=pixels;
        for (unsigned x=0;x<MERGE_RAW_WIDTH;++x) {
            if (stream.available<MERGE_CODE_BITS) {
                unsigned status=merge_fill(&stream,buffer,end,MERGE_CODE_BITS,file,sequential);
                /* Up to four lookahead bits may be absent at the end of the strip. */
                if (status && (status!=MERGE_DECODE_ERROR || stream.available<8)) return status;
            }
            int entry=codes->delta[stream.bits>>(32-MERGE_CODE_BITS)];
            unsigned used=(unsigned)entry&15u;
            int difference=entry>>4;
            if (!used) {
                unsigned symbol=codes->prefix[stream.bits>>24];
                unsigned length=symbol&15u;
                used=(symbol>>4)+length;
                unsigned status=merge_fill(&stream,buffer,end,used,file,sequential);
                if (status) return status;
                difference=merge_delta(stream.bits>>(32-used),length);
            }
            if (used>stream.available) return MERGE_DECODE_ERROR;
            stream.bits<<=used;
            stream.available-=used;
            int value=predict+difference;
            if ((unsigned)value>=16384u) return MERGE_DECODE_ERROR;
            predict=other;
            other=value;
            *dest++=(unsigned short)value;
        }
        previous[0]=pixels[0];
        previous[1]=pixels[1];
        if (y>=row) pixels=dest;
    }
    state->row=stop;
    state->next=stream.next-(unsigned)(stream.limit-stream.cursor);
    state->bits=stream.bits;
    state->available=stream.available;
    reader->used=(unsigned)(stream.cursor-buffer);
    reader->count=(unsigned)(stream.limit-buffer);
    return 0;
}

static unsigned merge_read_band(struct merge_source *source,unsigned char *input,unsigned short *pixels,
                                unsigned row,unsigned rows)
{
    z72_u32 local[10],*file=source->open ? source->file : local;
    if (!source->open && !merge_open_read(file,source->path)) return MERGE_FILE_ERROR;
    unsigned status=MERGE_FILE_ERROR;
    if (!file[7] && file[6]==source->size) {
        if (source->packed) {
            if (merge_read_input(file,source->open,source->strip+row*MERGE_ROW_BYTES,
                                 input,rows*MERGE_ROW_BYTES)) {
                merge_unpack_packed(pixels,input,rows);
                status=0;
            }
        } else {
            struct merge_reader local_reader={input,0,0};
            struct merge_reader *reader=source->open ? &source->reader : &local_reader;
            status=merge_read_lossless(reader,&source->compressed,&merge_codes,pixels,row,rows,
                                       file,source->open);
        }
    }
    if (!source->open && !nx_close(file) && !status) status=MERGE_FILE_ERROR;
    return status;
}

static unsigned merge_probe_source(struct merge_source *source,unsigned char *input,
                                   unsigned short *raw,struct merge_probe_point *reference,
                                   struct merge_probe_fit *fit,int (*stop_requested)(void))
{
    struct merge_lossless initial=source->compressed;
    unsigned status=MERGE_FILE_ERROR;
    if (merge_open_source(source,MERGE_TOP,input)) {

        status=0;
        for (unsigned band=0;band<MERGE_PROBE_BANDS;++band) {
            if (stop_requested()) {
                status=MERGE_CANCELLED;
                break;
            }
            status=merge_read_band(source,input,raw,MERGE_TOP+band*MERGE_ROWS,MERGE_ROWS);
            if (status) break;
            merge_probe_band(raw,reference,fit);
            reference+=2*MERGE_PROBE_TILES*MERGE_PROBE_POINTS;
            if ((band&7u)==7) pixel_sleep(1);
        }
    }
    if (source->open && !nx_close(source->file) && !status) status=MERGE_FILE_ERROR;
    source->open=0;
    source->compressed=initial;
    return status;
}

static unsigned merge_align(unsigned count,unsigned scale,unsigned char *input,unsigned short *raw,
                            struct merge_probe_point *reference,
                            int (*stop_requested)(void),void (*progress)(unsigned))
{
    _Static_assert(MERGE_PROBE_SAMPLES*MERGE_PROBE_TILES*sizeof(*reference)<=MERGE_WIDTH*MERGE_ROWS*6,
                   "Registration reference fits in the minimum TIFF band");
    const struct merge_source *first=&merge_sources[0];
    unsigned status=merge_probe_source(&merge_sources[0],input,raw,reference,0,stop_requested);
    for (unsigned frame=0;frame<count && !status;++frame) {
        struct merge_source *source=&merge_sources[frame];
        struct merge_alignment *plan=&source->alignment;
        *plan=merge_nominal(frame,scale);
        source->carry=0;
        if (!frame) {
            progress(5+merge_ratio(20,count));
            continue;
        }

        struct merge_offset nominal=merge_nominal_offset(plan),initial=nominal,measured;
        if (first->position_valid && source->position_valid) {
            struct merge_offset recorded={source->position.x-first->position.x,
                                          source->position.y-first->position.y};
            if (merge_near(recorded,nominal,2*MERGE_SHIFT_LIMIT)) initial=recorded;
        }
        struct merge_probe_fit fit[MERGE_PROBE_TILES];
        CALL(0x10361035u,void,void *,z72_u32)(fit,sizeof(fit));
        status=merge_probe_source(source,input,raw,reference,fit,stop_requested);
        if (!status && merge_select_fit(fit,nominal,initial,&measured)) {

            int fx=measured.x-nominal.x,fy=measured.y-nominal.y;
            /* Preserve exact sensor samples when the correction is below the noise floor. */
            plan->fx=merge_abs(fx)>1 ? fx : 0;
            plan->fy=merge_abs(fy)>1 ? fy : 0;
        }
        if (!status) progress(5+merge_ratio(20*(frame+1),count));
    }
    return status;
}

static unsigned merge_read_aligned(struct merge_source *source,unsigned char *input,unsigned short *raw,
                                   unsigned y)
{
    const struct merge_alignment *plan=&source->alignment;
    unsigned row=merge_first_row(plan)+y;
    if (!plan->fy) return merge_read_band(source,input,raw,row,MERGE_ROWS);
    enum { CARRY_PIXELS=MERGE_HALO*MERGE_RAW_WIDTH, CARRY_BYTES=CARRY_PIXELS*2 };
    unsigned status;
    if (!y) {
        status=merge_read_band(source,input,raw,row,MERGE_ROWS+MERGE_HALO);
    } else {
        __aeabi_memcpy4(raw,source->carry,CARRY_BYTES);
        status=merge_read_band(source,input,raw+CARRY_PIXELS,row+MERGE_HALO,MERGE_ROWS);
    }
    if (!status) __aeabi_memcpy4(source->carry,raw+MERGE_ROWS*MERGE_RAW_WIDTH,CARRY_BYTES);
    return status;
}

static int merge_write(int file,const void *data,unsigned size)
{
    const unsigned char *cursor=data;
    while (size) {
        unsigned chunk=size>0x20000u ? 0x20000u : size;
        z72_u32 actual=0;
        const struct { z72_u32 kind; const void *buffer; z72_u32 capacity; } request={1,cursor,chunk};
        CALL(0x1034f9edu,void,const void *,z72_u32)(cursor,chunk);
        if (CALL(0x105bea79u,z72_u32,int,const void *,z72_u32,z72_u32 *)(
                file,&request,chunk,&actual) || actual!=chunk) return 0;
        cursor+=chunk;
        size-=chunk;
    }
    return 1;
}

#include "pixel_shift_merge_nefx.h"

#define MERGE_OUTPUT_DIRECTORY "B:\\Synthesis"
enum {
    MERGE_OUTPUT_NAME_OFFSET=sizeof(MERGE_OUTPUT_DIRECTORY),
    MERGE_OUTPUT_PATH_SIZE=MERGE_OUTPUT_NAME_OFFSET+sizeof("PS000000.NEFX")
};

static int merge_output_paths(char final[MERGE_OUTPUT_PATH_SIZE],char temp[MERGE_OUTPUT_PATH_SIZE])
{
    const char *source=merge_sources[0].path;
    unsigned len=0;
    while (source[len]) ++len;
    if (len<12) return 0;
    char *final_name=final+MERGE_OUTPUT_NAME_OFFSET,*temp_name=temp+MERGE_OUTPUT_NAME_OFFSET;
    for (unsigned i=0;i<4;++i) {
        char digit=source[len-8+i];
        if (digit<'0' || digit>'9') return 0;
        final_name[4+i]=temp_name[4+i]=digit;
    }
    char directory[]=MERGE_OUTPUT_DIRECTORY;
    directory[0]=final[0]=temp[0]=source[0];
    /* Use the same directory lookup and creation calls as the native card UI. */
    if (CALL(0x105bedc5u,int,const char *)(directory) &&
        CALL(0x105bec39u,int,const char *,z72_u32)(directory,0) &&
        CALL(0x105bedc5u,int,const char *)(directory)) return 0;

    for (unsigned n=0;n<100;++n) {
        final_name[2]=temp_name[2]=(char)('0'+n/10);
        final_name[3]=temp_name[3]=(char)('0'+n%10);
        z72_u32 file[10];
        if (merge_open_read(file,final)) {
            if (!nx_close(file)) return 0;
        } else if (merge_open_read(file,temp)) {
            if (!nx_close(file)) return 0;
        } else return 1;
    }
    return 0;
}

static unsigned merge_write_image(const z72_u32 *ids,unsigned count,int (*stop_requested)(void),
                                  unsigned *ca_status,unsigned format,void (*progress)(unsigned))
{
    if (stop_requested()) return MERGE_CANCELLED;

    struct merge_color color;
    struct merge_metadata metadata;
    struct merge_ca_profile profile;
    unsigned status=merge_prepare(ids,count,&color,&metadata,&profile,stop_requested,progress);

    if (status) return status;
    if (format==MERGE_NEFX && !merge_sources[0].packed) return MERGE_LAYOUT_ERROR;

    char final[MERGE_OUTPUT_PATH_SIZE]=MERGE_OUTPUT_DIRECTORY "\\PS000000.";
    char temp[MERGE_OUTPUT_PATH_SIZE]=MERGE_OUTPUT_DIRECTORY "\\PS000000.TMP";
    const char *extension=format==MERGE_NEFX ? "NEFX" : "TIF";
    for (unsigned i=0;extension[i];++i) final[MERGE_OUTPUT_NAME_OFFSET+9+i]=extension[i];
    if (!merge_output_paths(final,temp)) return MERGE_OUTPUT_ERROR;
    const unsigned scale=count>=16 ? 2u : 1u,repeated=(count==8 || count==32);
    const unsigned pixels=MERGE_WIDTH*MERGE_ROWS*scale*scale;
    const unsigned band_bytes=pixels*6,raw_bytes=(MERGE_ROWS+MERGE_HALO)*MERGE_RAW_WIDTH*2;
    const unsigned workspace=band_bytes+raw_bytes+MERGE_INPUT_BYTES;
    int keep=count<=MERGE_KEEP_OPEN;
    unsigned readahead=0;
    if (keep)
        for (unsigned i=0;i<count;++i)
            if (!merge_sources[i].packed) readahead+=MERGE_READ_CHUNK;
    void *owner;
    unsigned short *band=nx_alloc(workspace+readahead,&owner);
    if (!band && readahead) {
        keep=0;
        band=nx_alloc(workspace,&owner);
    }
    if (!band) return MERGE_MEMORY_ERROR;
    unsigned short *raw=(unsigned short *)((unsigned char *)band+band_bytes);
    unsigned char *input=(unsigned char *)raw+raw_bytes;
    merge_huffman(&merge_codes);

    status=merge_align(count,scale,input,raw,(struct merge_probe_point *)band,stop_requested,progress);

    if (status) {

        CALL(0x1034fe7fu,void,void *)(owner);
        return status;
    }
    unsigned carries=0;
    for (unsigned frame=0;frame<count;++frame) carries+=merge_sources[frame].alignment.fy!=0;
    void *carry_owner=0;
    const unsigned carry_pixels=MERGE_HALO*MERGE_RAW_WIDTH;
    unsigned short *carry=carries ? nx_alloc(carries*carry_pixels*2,&carry_owner) : 0;
    for (unsigned frame=0;frame<count;++frame) {
        struct merge_source *source=&merge_sources[frame];
        if (source->alignment.fy && carry) {
            source->carry=carry;
            carry+=carry_pixels;
        } else source->alignment.fy=0;

    }
    struct merge_ca ca;
    void *ca_owner=0;
    struct merge_offset position={0,0};
    if (merge_sources[0].position_valid) position=merge_sources[0].position;
    *ca_status=merge_ca_init(&ca,&profile,position,scale,&ca_owner);
    enum { NEFX_PREVIEW_ALLOCATION=(NEFX_PREVIEW_BYTES+63u)&~63u };
    struct merge_nefx nefx={0};
    void *nefx_owner=0;
    if (format==MERGE_NEFX)
        nefx.preview=nx_alloc(NEFX_PREVIEW_ALLOCATION+sizeof(struct nefx_directories),&nefx_owner);

    int cancelled=stop_requested();

    status=cancelled ? MERGE_CANCELLED : format==MERGE_NEFX && !nefx.preview ?
        MERGE_MEMORY_ERROR : MERGE_OUTPUT_ERROR;
    int file=status==MERGE_OUTPUT_ERROR ? CALL(0x105beb2bu,int,const char *,z72_u32)(temp,0) : -1;

    if (file>=0) {

        if (format==MERGE_NEFX) {
            struct nefx_directories *dirs=(struct nefx_directories *)(nefx.preview+NEFX_PREVIEW_ALLOCATION);
            status=nefx_write_header(file,&merge_sources[0],dirs,&nefx,input,count,scale,
                                     *ca_status==MERGE_CA_APPLIED,stop_requested);
        } else {
            _Alignas(64) unsigned char header[MERGE_HEADER_BYTES];
            merge_tiff_header(header,scale,color.orientation,&metadata);
            status=merge_write(file,header,sizeof(header)) ? 0 : MERGE_OUTPUT_ERROR;
        }

        if (!status) {
            if (keep && !stop_requested()) merge_keep_sources_open(count,input+MERGE_INPUT_BYTES);
            unsigned written_rows=0;
            for (unsigned y=0;!status &&
                 (y<MERGE_HEIGHT || (ca.rows && ca.output_rows<ca.height));y+=MERGE_ROWS) {
                if (stop_requested()) {
                    status=MERGE_CANCELLED;
                    break;
                }
                unsigned rows=0;
                if (y<MERGE_HEIGHT) {

                    for (unsigned i=0;i<pixels*3;++i) band[i]=0;
                    for (unsigned frame=0;frame<count;++frame) {
                        if (stop_requested()) {
                            status=MERGE_CANCELLED;
                            break;
                        }
                        struct merge_source *source=&merge_sources[frame];
                        status=merge_read_aligned(source,input,raw,y);
                        if (status) break;
                        const unsigned short *samples=raw;
                        if (source->alignment.fy<0) samples+=MERGE_HALO*MERGE_RAW_WIDTH;
                        merge_accumulate(band,samples,&source->alignment,scale);
                    }

                    rows=MERGE_ROWS*scale;
                }
                if (status) break;

                if (ca.rows) rows=merge_ca_band(&ca,band,rows);
                unsigned samples=rows*MERGE_WIDTH*scale;
                unsigned bytes=samples*6;
                if (format==MERGE_NEFX) {
                    nefx_preview_band(&nefx,band,rows,scale,repeated);
                    bytes=nefx_pack_band(band,samples,repeated);
                } else merge_render(band,samples,repeated,color.black,color.gain);

                if (!merge_write(file,band,bytes)) status=MERGE_OUTPUT_ERROR;
                else {
                    written_rows+=rows;
                    progress(25+merge_ratio(72*written_rows,MERGE_HEIGHT*scale));
                }

                pixel_sleep(1);
            }
        }
    }

    merge_close_sources(count);

    if (file>=0) {
        if (!status) progress(98);

        if (CALL(0x105be9ddu,z72_u32,int)(file) && !status) status=MERGE_OUTPUT_ERROR;
        if (!status && format==MERGE_NEFX && !nefx_finish_preview(temp,&nefx,scale,stop_requested))
            status=stop_requested() ? MERGE_CANCELLED : MERGE_OUTPUT_ERROR;
        if (!status && stop_requested()) status=MERGE_CANCELLED;
        if (!status && CALL(0x105beb5bu,z72_u32,const char *,const char *)(temp,final))
            status=MERGE_OUTPUT_ERROR;
        if (status) CALL(0x105beb81u,z72_u32,const char *)(temp);

        if (!status) progress(100);
    }
    if (nefx_owner) CALL(0x1034fe7fu,void,void *)(nefx_owner);
    if (ca_owner) CALL(0x1034fe7fu,void,void *)(ca_owner);
    if (carry_owner) CALL(0x1034fe7fu,void,void *)(carry_owner);
    CALL(0x1034fe7fu,void,void *)(owner);
    return status;
}
