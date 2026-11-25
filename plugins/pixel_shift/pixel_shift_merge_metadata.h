/* SPDX-License-Identifier: MIT */
enum { MERGE_HEADER_BYTES=1024, MERGE_TIFF_EXIF=224, MERGE_TIFF_VALUES=480 };
enum {
    META_MAKE,META_MODEL,META_EXPOSURE,META_APERTURE,META_PROGRAM,
    META_ISO,META_ISO_TYPE,META_ISO_SPEED,META_DATE,META_TIME_ZONE,
    META_COMPENSATION,META_FOCAL,META_SUBSECOND,META_EXPOSURE_MODE,
    META_WHITE_BALANCE,META_LENS_MAKE,META_LENS_MODEL,META_FIELDS
};

static const struct merge_metadata_tag {
    unsigned short tag;
    unsigned char type,size;
} merge_metadata_tags[META_FIELDS]={
    [META_MAKE]={0x010f,2,32}, [META_MODEL]={0x0110,2,32},
    [META_EXPOSURE]={0x829a,5,8}, [META_APERTURE]={0x829d,5,8},
    [META_PROGRAM]={0x8822,3,2}, [META_ISO]={0x8827,3,2},
    [META_ISO_TYPE]={0x8830,3,2}, [META_ISO_SPEED]={0x8833,4,4},
    [META_DATE]={0x9003,2,20}, [META_TIME_ZONE]={0x9011,2,7},
    [META_COMPENSATION]={0x9204,10,8}, [META_FOCAL]={0x920a,5,8},
    [META_SUBSECOND]={0x9291,2,16}, [META_EXPOSURE_MODE]={0xa402,3,2},
    [META_WHITE_BALANCE]={0xa403,3,2}, [META_LENS_MAKE]={0xa433,2,64},
    [META_LENS_MODEL]={0xa434,2,64}
};

struct merge_metadata_value { unsigned char size,data[64]; };
struct merge_metadata { struct merge_metadata_value fields[META_FIELDS]; unsigned count; };

static void merge_read_metadata_fields(z72_u32 *file,struct merge_metadata *metadata,
                                        unsigned first,unsigned end)
{
    /* The native reader performs cache maintenance on the whole buffer line. */
    _Alignas(64) unsigned char value[64];
    for (unsigned i=first;i<end;++i) {
        const struct merge_metadata_tag *tag=&merge_metadata_tags[i];
        const unsigned char *entry=nx_entry(nx_scan,tag->tag);
        if (!entry || nx_u16(entry+2)!=tag->type) continue;
        unsigned count=nx_u32(entry+4),size=tag->size;
        if (tag->type==2) {
            if (!count) continue;
            if (count<size) size=count;
        } else if (count!=1) continue;

        const unsigned char *data=entry+8;
        if (size>4) {
            unsigned offset=nx_u32(data);
            unsigned length=tag->type==2 ? count : size;
            if (offset<8 || offset>file[6] || length>file[6]-offset ||
                !nx_read(file,offset,value,size)) continue;
            data=value;
        }
        if (tag->type==2) {
            unsigned end=0;
            while (end<size && data[end]) ++end;
            if (!end || end==size) continue;
            size=end+1; /* NEF ASCII fields can have multiple trailing NUL bytes. */
        } else if ((tag->type==5 || tag->type==10) && !nx_u32(data+4)) continue;
        struct merge_metadata_value *field=metadata->fields+i;
        for (unsigned n=0;n<size;++n) field->data[n]=data[n];
        field->size=(unsigned char)size;
        ++metadata->count;
    }
}

static void merge_read_metadata(z72_u32 *file,z72_u32 root,struct merge_metadata *metadata)
{
    metadata->count=0;
    for (unsigned i=0;i<META_FIELDS;++i) metadata->fields[i].size=0;
    if (!nx_directory(file,root)) return;
    unsigned exif=nx_tag_value(0x8769,4);
    merge_read_metadata_fields(file,metadata,META_MAKE,META_EXPOSURE);
    if (exif && nx_directory(file,exif))
        merge_read_metadata_fields(file,metadata,META_EXPOSURE,META_FIELDS);
}

struct merge_tiff_ifd {
    unsigned char *header;
    unsigned start,next;
    unsigned *values;
};

static void merge_tiff_tag(struct merge_tiff_ifd *ifd,unsigned tag,unsigned type,
                           unsigned count,unsigned value)
{
    unsigned char *entry=ifd->header+ifd->next;
    nx_put(entry,tag,2);
    nx_put(entry+2,type,2);
    nx_put(entry+4,count,4);
    nx_put(entry+8,value,4);
    ifd->next+=12;
}

static unsigned merge_tiff_data(struct merge_tiff_ifd *ifd,const unsigned char *data,unsigned size)
{
    unsigned at=*ifd->values;
    for (unsigned i=0;i<size;++i) ifd->header[at+i]=data[i];
    *ifd->values=at+((size+1u)&~1u);
    return at;
}

static void merge_tiff_metadata_fields(struct merge_tiff_ifd *ifd,const struct merge_metadata *metadata,
                                       unsigned first,unsigned end)
{
    for (unsigned i=first;i<end;++i) {
        const struct merge_metadata_value *field=metadata->fields+i;
        unsigned size=field->size;
        if (!size) continue;
        const struct merge_metadata_tag *tag=&merge_metadata_tags[i];
        unsigned value=0;
        if (size>4) value=merge_tiff_data(ifd,field->data,size);
        else for (unsigned j=0;j<size;++j) value|=(unsigned)field->data[j]<<(8*j);
        merge_tiff_tag(ifd,tag->tag,tag->type,tag->type==2 ? size : 1,value);
    }
}

static void merge_tiff_finish(const struct merge_tiff_ifd *ifd)
{
    nx_put(ifd->header+ifd->start,(ifd->next-ifd->start-2)/12,2);
}

static void merge_tiff_header(unsigned char *header,unsigned scale,unsigned orientation,
                              const struct merge_metadata *metadata)
{
    _Static_assert(14+16*12<=MERGE_TIFF_EXIF && MERGE_TIFF_EXIF+6+META_FIELDS*12<=MERGE_TIFF_VALUES,
                   "The TIFF directories and value area do not overlap");
    _Static_assert(MERGE_TIFF_VALUES+12+2*32+4*8+20+8+16+2*64<=MERGE_HEADER_BYTES,
                   "The TIFF header fits the selected metadata values");
    static const unsigned char bits[]={16,0,16,0,16,0},format[]={1,0,1,0,1,0};
    const unsigned width=MERGE_WIDTH*scale,height=MERGE_HEIGHT*scale;
    unsigned values=MERGE_TIFF_VALUES;
    struct merge_tiff_ifd image={header,8,10,&values};
    struct merge_tiff_ifd exif={header,MERGE_TIFF_EXIF,MERGE_TIFF_EXIF+2,&values};
    for (unsigned i=0;i<MERGE_HEADER_BYTES;++i) header[i]=0;
    nx_put(header,0x002a4949u,4);
    nx_put(header+4,image.start,4);
    merge_tiff_tag(&image,254,4,1,0);
    merge_tiff_tag(&image,256,4,1,width);
    merge_tiff_tag(&image,257,4,1,height);
    merge_tiff_tag(&image,258,3,3,merge_tiff_data(&image,bits,sizeof(bits)));
    merge_tiff_tag(&image,259,3,1,1);
    merge_tiff_tag(&image,262,3,1,2);
    merge_tiff_metadata_fields(&image,metadata,META_MAKE,META_EXPOSURE);
    merge_tiff_tag(&image,273,4,1,MERGE_HEADER_BYTES);
    merge_tiff_tag(&image,274,3,1,orientation);
    merge_tiff_tag(&image,277,3,1,3);
    merge_tiff_tag(&image,278,4,1,height);
    merge_tiff_tag(&image,279,4,1,width*height*6);
    merge_tiff_tag(&image,284,3,1,1);
    merge_tiff_tag(&image,339,3,3,merge_tiff_data(&image,format,sizeof(format)));
    merge_tiff_tag(&image,0x8769,4,1,exif.start);
    merge_tiff_finish(&image);

    merge_tiff_metadata_fields(&exif,metadata,META_EXPOSURE,META_DATE);
    merge_tiff_tag(&exif,0x9000,7,4,0x31333230u); /* 0231 */
    merge_tiff_metadata_fields(&exif,metadata,META_DATE,META_EXPOSURE_MODE);
    merge_tiff_tag(&exif,0xa001,3,1,1); /* sRGB */
    merge_tiff_metadata_fields(&exif,metadata,META_EXPOSURE_MODE,META_FIELDS);
    merge_tiff_finish(&exif);
}
