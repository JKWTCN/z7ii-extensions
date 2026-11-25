/* SPDX-License-Identifier: MIT */
enum {
    NEFX_PREVIEW_W=640, NEFX_PREVIEW_H=424, NEFX_PREVIEW_HEADER=16,
    NEFX_PREVIEW_BYTES=NEFX_PREVIEW_HEADER+NEFX_PREVIEW_W*NEFX_PREVIEW_H*6,
    NEFX_RESULT_BYTES=16, NEFX_NOTE_GROWTH=2*NX_ENTRY
};
_Static_assert(MERGE_WIDTH%64==0,"NEFX packed RGB rows require no alignment padding");

struct merge_nefx {
    unsigned char *preview;
    unsigned preview_offset,raw_offset,rows,next_preview_row;
};

struct nefx_directories {
    unsigned char root[NX_SCAN_BYTES],exif[NX_SCAN_BYTES],image[NX_SCAN_BYTES],profile[NX_SCAN_BYTES];
    _Alignas(64) unsigned char values[64];
};

static unsigned nefx_read_ifd(z72_u32 *file,unsigned offset,unsigned char *ifd)
{
    if (!nx_directory(file,offset)) return 0;
    unsigned size=6+NX_ENTRY*nx_u16(nx_scan);
    if (nx_u32(nx_scan+size-4)) return 0;
    __aeabi_memcpy4(ifd,nx_scan,size);
    return size;
}

static int nefx_ifd_value(unsigned char *ifd,unsigned tag,unsigned type,unsigned value)
{
    unsigned char *entry=nx_entry(ifd,tag);
    if (!nx_match(entry,type,1)) return 0;
    nx_put(entry+8,value,4);
    return 1;
}

/* All the untouched out-of-line IFD values still address the copied NEF prefix. */
static int nefx_prefix_values(unsigned char *ifd,unsigned prefix,unsigned replaced)
{
    static const unsigned char shifts[]={0,0,0,1,2,3,0,0,1,2,3,2,3};
    for (unsigned i=0;i<nx_u16(ifd);++i) {
        const unsigned char *e=ifd+2+NX_ENTRY*i;
        unsigned kind=nx_u16(e+2),n=nx_u32(e+4),at=nx_u32(e+8);
        if (nx_u16(e)==replaced) continue;
        if (!kind || kind>=sizeof(shifts) || n>(prefix>>shifts[kind])) return 0;
        unsigned size=n<<shifts[kind];
        if (size>4 && (at<8 || at>prefix || size>prefix-at)) return 0;
    }
    return 1;
}

static int nefx_expand_note(unsigned char *note,unsigned size,unsigned width,unsigned height,
                            unsigned count)
{
    unsigned char *ifd=note+18;
    if (!nx_equal(note,(const unsigned char *)"Nikon\0\2\x11\0\0II*\0\10\0\0",18))
        return 0;
    unsigned end=24+NX_ENTRY*nx_u16(ifd);
    if (end>size || nx_u32(note+end-4) || nx_entry(ifd,0xe2d) || nx_entry(ifd,0xe2e)) return 0;
    const unsigned char *preview=nx_entry(ifd,0x11),*crop=nx_entry(ifd,0x45);
    const unsigned char *format=nx_entry(ifd,0x93),*raw=nx_entry(ifd,0x51);
    if (!nx_match(preview,4,1) || !nx_match(crop,3,4) || !nx_match(format,3,1) ||
        nx_u16(format+8)!=NX_RAW_PACKED_14 || !nx_match(raw,7,24)) return 0;
    unsigned preview_at=nx_u32(preview+8),crop_at=nx_u32(crop+8),raw_at=nx_u32(raw+8);
    if (preview_at<end-10 || preview_at>size-16 || crop_at>size-18 ||
        raw_at>size-34 || nx_u16(note+raw_at+20)!=NX_RAW_PACKED_14) return 0;
    unsigned char *preview_ifd=note+10+preview_at;
    unsigned preview_size=6+NX_ENTRY*nx_u16(preview_ifd);
    if (preview_size>size-10-preview_at || nx_u32(preview_ifd+preview_size-4) ||
        !nx_relocate(preview_ifd,end,size,0x201,NEFX_NOTE_GROWTH) ||
        !nx_relocate(ifd,end,size,0x11,NEFX_NOTE_GROWTH)) return 0;
    for (unsigned i=size;i>end;--i) note[i+NEFX_NOTE_GROWTH-1]=note[i-1];
    unsigned char *crop_data=note+10+crop_at+NEFX_NOTE_GROWTH;
    nx_put(crop_data,0,4);
    nx_put(crop_data+4,width,2);
    nx_put(crop_data+6,height,2);
    unsigned result=(size+NEFX_NOTE_GROWTH+1u)&~1u;
    if (size&1u) note[result-1]=0;
    nx_insert(ifd,0xe2d,NEFX_RESULT_BYTES,result-10);
    nx_insert(ifd,0xe2e,NEFX_PREVIEW_BYTES,result+NEFX_RESULT_BYTES-10);
    nx_put(note+end+NEFX_NOTE_GROWTH-4,0,4);
    unsigned mode=count==4 ? 1u : count==8 ? 2u : count==16 ? 3u : 4u;
    nx_put(note+result,0x30303130u,4);
    nx_put(note+result+4,2u|(mode<<16)|(mode<<24),4);
    nx_put(note+result+8,0,4);
    nx_put(note+result+12,0,4);
    return 1;
}

static int nefx_remove_lateral_ca(unsigned char *profile,unsigned size)
{
    if (size<24 || !nx_equal(profile,(const unsigned char *)"Nikon\0\1\1\0\0II*\0",14)) return 0;
    unsigned offset=nx_u32(profile+14);
    if (offset<8 || offset>size-16) return 0;
    unsigned char *ifd=profile+10+offset;
    unsigned count=nx_u16(ifd);
    if (count>(size-16-offset)/NX_ENTRY) return 0;
    unsigned char *entry=nx_entry(ifd,7);
    if (!entry) return 0;
    unsigned char *end=ifd+2+NX_ENTRY*count;
    for (;entry+NX_ENTRY<end;++entry) *entry=entry[NX_ENTRY];
    nx_put(end-NX_ENTRY,0,4);
    nx_put(ifd,count-1,2);
    return 1;
}

static int nefx_write_zeros(int file,unsigned char *scratch,unsigned bytes)
{
    if (!bytes) return 1;
    CALL(0x10361035u,void,void *,z72_u32)(scratch,MERGE_READ_CHUNK);
    while (bytes) {
        unsigned chunk=bytes<MERGE_READ_CHUNK ? bytes : MERGE_READ_CHUNK;
        if (!merge_write(file,scratch,chunk)) return 0;
        bytes-=chunk;
    }
    return 1;
}

static unsigned nefx_write_header(int output,const struct merge_source *source,
                                  struct nefx_directories *dirs,struct merge_nefx *nefx,
                                  unsigned char *scratch,unsigned count,unsigned scale,
                                  int ca_applied,int (*stop_requested)(void))
{
    z72_u32 file[10],root,note_at,note_bytes;
    int packed;
    if (!merge_open_read(file,source->path)) return MERGE_FILE_ERROR;
    unsigned status=MERGE_LAYOUT_ERROR;
    void *note_owner=0;
    const unsigned prefix=source->strip,width=MERGE_WIDTH*scale,height=MERGE_HEIGHT*scale;
    if (!nx_find_note(file,&note_at,&note_bytes,&root,&packed) || !packed) goto close;
    unsigned note_header_bytes=((note_bytes+NEFX_NOTE_GROWTH+1u)&~1u)+NEFX_RESULT_BYTES;
    nefx->preview_offset=prefix+note_header_bytes;
    unsigned root_bytes=nefx_read_ifd(file,root,dirs->root);
    if (!root_bytes || !nefx_prefix_values(dirs->root,prefix,0x14a)) goto close;
    unsigned char *subs=nx_entry(dirs->root,0x14a),*exif=nx_entry(dirs->root,0x8769);
    if (!subs || !nx_match(exif,4,1)) goto close;
    unsigned sub_count=nx_u32(subs+4),subs_at=nx_u32(subs+8);
    if (sub_count<2 || sub_count>8 || !nx_match(subs,4,sub_count) ||
        subs_at<8 || subs_at>prefix || sub_count*4>prefix-subs_at ||
        !nx_read(file,subs_at,dirs->values,sub_count*4)) goto close;
    for (unsigned i=0;i<sub_count;++i) {
        unsigned at=nx_u32(dirs->values+4*i);
        if (at<8 || at>=prefix) goto close;
    }
    unsigned image_bytes=nefx_read_ifd(file,nx_u32(dirs->values+4),dirs->image);
    unsigned exif_bytes=nefx_read_ifd(file,nx_u32(exif+8),dirs->exif);
    if (!image_bytes || !exif_bytes || !nefx_prefix_values(dirs->image,prefix,0xc7d5) ||
        !nefx_prefix_values(dirs->exif,prefix,0x927c)) goto close;
    unsigned char *note_reference=nx_entry(dirs->exif,0x927c);
    if (!nx_match(note_reference,7,note_bytes)) goto close;

    unsigned profile_bytes=0;
    if (ca_applied) {
        unsigned char *entry=nx_entry(dirs->image,0xc7d5);
        if (!entry || nx_u16(entry+2)!=7) goto close;
        profile_bytes=nx_u32(entry+4);
        unsigned at=nx_u32(entry+8);
        if (profile_bytes>NX_SCAN_BYTES || at<8 || at>prefix || profile_bytes>prefix-at ||
            !nx_read(file,at,dirs->profile,profile_bytes) ||
            !nefx_remove_lateral_ca(dirs->profile,profile_bytes)) goto close;
        nx_put(entry+8,nefx->preview_offset+NEFX_PREVIEW_BYTES,4);
    } else if (!nefx_prefix_values(dirs->image,prefix,0)) goto close;

    unsigned char *note=nx_alloc(note_header_bytes,&note_owner);
    if (!note) {
        status=MERGE_MEMORY_ERROR;
        goto close;
    }
    if (!nx_read(file,note_at,note,note_bytes) || !nefx_expand_note(note,note_bytes,width,height,count))
        goto close;

    unsigned values_at=nefx->preview_offset+NEFX_PREVIEW_BYTES+profile_bytes+(profile_bytes&1u);
    unsigned image_at=values_at+sizeof(dirs->values);
    unsigned exif_at=image_at+image_bytes,root_at=exif_at+exif_bytes;
    unsigned pad=(0u-(root_at+root_bytes))&63u;
    nefx->raw_offset=root_at+root_bytes+pad;
    nx_put(note_reference+4,note_header_bytes+NEFX_PREVIEW_BYTES,4);
    nx_put(note_reference+8,prefix,4);
    nx_put(exif+8,exif_at,4);
    nx_put(subs+8,values_at,4);
    nx_put(dirs->values+4,image_at,4);
    static const char software[]="Z72 SD Pixel Shift";
    unsigned char *software_tag=nx_entry(dirs->root,0x131);
    if (software_tag && nx_u16(software_tag+2)==2) {
        __aeabi_memcpy4(dirs->values+40,software,sizeof(software));
        nx_put(software_tag+4,sizeof(software),4);
        nx_put(software_tag+8,values_at+40,4);
    }
    unsigned char *bits=nx_entry(dirs->image,0x102);
    if (!nx_match(bits,3,1) ||
        !nefx_ifd_value(dirs->image,0x100,4,width) || !nefx_ifd_value(dirs->image,0x101,4,height) ||
        !nefx_ifd_value(dirs->image,0x106,3,2) || !nefx_ifd_value(dirs->image,0x115,3,3) ||
        !nefx_ifd_value(dirs->image,0x116,4,height) ||
        !nefx_ifd_value(dirs->image,0x111,4,nefx->raw_offset) ||
        !nefx_ifd_value(dirs->image,0x117,4,width*height*21/4)) goto close;
    nx_put(bits+4,3,4);
    nx_put(bits+8,values_at+32,4);
    for (unsigned i=0;i<3;++i) nx_put(dirs->values+32+2*i,14,2);

    status=MERGE_OUTPUT_ERROR;
    for (unsigned at=0;at<prefix;) {
        if (stop_requested()) {
            status=MERGE_CANCELLED;
            goto close;
        }
        unsigned chunk=prefix-at<MERGE_READ_CHUNK ? prefix-at : MERGE_READ_CHUNK;
        if (!nx_read(file,at,scratch,chunk)) {
            status=MERGE_FILE_ERROR;
            goto close;
        }
        if (!at) nx_put(scratch+4,root_at,4);
        if (!merge_write(output,scratch,chunk)) goto close;
        at+=chunk;
    }
    if (merge_write(output,note,note_header_bytes) &&
        nefx_write_zeros(output,scratch,NEFX_PREVIEW_BYTES) &&
        merge_write(output,dirs->profile,profile_bytes) &&
        (!(profile_bytes&1u) || nefx_write_zeros(output,scratch,1)) &&
        merge_write(output,dirs->values,sizeof(dirs->values)) &&
        merge_write(output,dirs->image,image_bytes) &&
        merge_write(output,dirs->exif,exif_bytes) &&
        merge_write(output,dirs->root,root_bytes) && nefx_write_zeros(output,scratch,pad))
        status=0;
close:
    if (note_owner) CALL(0x1034fe7fu,void,void *)(note_owner);
    if (!nx_close(file) && !status) status=MERGE_FILE_ERROR;
    return status;
}

static unsigned nefx_linear_sample(const unsigned short *samples,unsigned i,unsigned repeated)
{
    return samples[i]>>(repeated+(i==1));
}

static void nefx_preview_band(struct merge_nefx *nefx,const unsigned short *band,unsigned rows,
                              unsigned scale,unsigned repeated)
{
    /* Sample the corrected linear image at the center of each preview cell. */
    unsigned width=MERGE_WIDTH*scale,height=MERGE_HEIGHT*scale;
    while (nefx->next_preview_row<NEFX_PREVIEW_H) {
        unsigned y=(2*nefx->next_preview_row+1)*height/(2*NEFX_PREVIEW_H);
        if (y>=nefx->rows+rows) break;
        const unsigned short *row=band+(y-nefx->rows)*width*3;
        unsigned char *dest=nefx->preview+NEFX_PREVIEW_HEADER+6*NEFX_PREVIEW_W*nefx->next_preview_row++;
        for (unsigned x=0;x<NEFX_PREVIEW_W;++x) {
            const unsigned short *pixel=row+3*((2*x+1)*width/(2*NEFX_PREVIEW_W));
            for (unsigned c=0;c<3;++c) nx_put(dest+6*x+2*c,nefx_linear_sample(pixel,c,repeated),2);
        }
    }
    nefx->rows+=rows;
}

static unsigned nefx_pack_band(unsigned short *band,unsigned pixels,unsigned repeated)
{
    /* Preserve the RAW black level and white balance stored in the MakerNote. */
    for (unsigned i=0;i<pixels;++i)
        for (unsigned c=0;c<3;++c) band[3*i+c]=(unsigned short)nefx_linear_sample(band+3*i,c,repeated);
    unsigned char *dest=(unsigned char *)band;
    /* Compact forward in the same buffer after loading all four input samples. */
    for (unsigned i=0;i<pixels*3;i+=4,dest+=7) {
        unsigned a=band[i],b=band[i+1],c=band[i+2],d=band[i+3];
        nx_put(dest,a|(b<<14)|(c<<28),4);
        nx_put(dest+4,(c>>4)|(d<<10),3);
    }
    return pixels*21/4;
}

static int nefx_finish_preview(const char *temp,struct merge_nefx *nefx,unsigned scale,
                               int (*stop_requested)(void))
{
    z72_u32 file[10];
    if (nefx->rows!=MERGE_HEIGHT*scale || nefx->next_preview_row!=NEFX_PREVIEW_H ||
        !nx_open(file,temp)) return 0;
    unsigned expected=nefx->raw_offset+MERGE_WIDTH*MERGE_HEIGHT*scale*scale*21/4;
    int ok=!file[7] && file[6]==expected;
    nx_put(nefx->preview,0x30303130u,4);
    nx_put(nefx->preview+4,NEFX_PREVIEW_HEADER,4);
    nx_put(nefx->preview+8,NEFX_PREVIEW_W|(NEFX_PREVIEW_H<<16),4);
    nx_put(nefx->preview+12,0x0e0e0e03u,4);
    for (unsigned at=0;ok && at<NEFX_PREVIEW_BYTES;at+=MERGE_READ_CHUNK) {
        unsigned chunk=NEFX_PREVIEW_BYTES-at<MERGE_READ_CHUNK ? NEFX_PREVIEW_BYTES-at : MERGE_READ_CHUNK;
        ok=!stop_requested() && nx_write(file,nefx->preview_offset+at,nefx->preview+at,chunk);
    }
    return nx_close(file) && ok;
}
