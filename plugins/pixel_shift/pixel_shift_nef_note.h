/* SPDX-License-Identifier: MIT */
enum { NX_ENTRY = 12, NX_GROWTH = 36, NX_EXTRA = 104, NX_NOTE_LIMIT = 512 * 1024 };
enum { NX_RAW_LOSSLESS = 3, NX_RAW_PACKED_14 = 10 };

static unsigned nx_u16(const unsigned char *p)
{
    return p[0] | (unsigned)p[1]<<8;
}

static z72_u32 nx_u32(const unsigned char *p)
{
    return nx_u16(p) | (z72_u32)nx_u16(p+2)<<16;
}

static void nx_put(unsigned char *p,z72_u32 value,unsigned size)
{
    for (unsigned i=0;i<size;++i) p[i]=(unsigned char)(value>>(8*i));
}

static int nx_equal(const unsigned char *p,const unsigned char *value,unsigned size)
{
    for (unsigned i=0;i<size;++i) if (p[i]!=value[i]) return 0;
    return 1;
}

static void nx_insert(unsigned char *ifd,unsigned tag,z72_u32 size,z72_u32 offset)
{
    unsigned count=nx_u16(ifd),i=count;
    unsigned char *e=ifd+2+NX_ENTRY*count;
    while (i && nx_u16(e-NX_ENTRY)>tag) {
        for (unsigned j=0;j<NX_ENTRY;++j) e[j]=e[(int)j-NX_ENTRY];
        e-=NX_ENTRY;
        --i;
    }
    nx_put(e,tag,2);
    nx_put(e+2,7,2);
    nx_put(e+4,size,4);
    nx_put(e+8,offset,4);
    nx_put(ifd,count+1,2);
}

/* Relocate TIFF data references. Nikon preview tag 0x11 and its JPEG offset
 * 0x201 are pointers even though they are represented as an inline LONG. */
static int nx_relocate(unsigned char *ifd,z72_u32 first,z72_u32 size,unsigned pointer_tag,
                       unsigned growth)
{
    static const unsigned char widths[]={0,1,1,2,4,8,1,1,2,4,8,4,8};
    unsigned count=nx_u16(ifd),last=0;
    for (unsigned i=0;i<count;++i) {
        unsigned char *entry=ifd+2+i*NX_ENTRY;
        unsigned tag=nx_u16(entry),kind=nx_u16(entry+2);
        z72_u32 n=nx_u32(entry+4),value=nx_u32(entry+8);
        if (tag<=last || !kind || kind>=sizeof(widths) || n>size) return 0;
        last=tag;
        z72_u32 bytes=n*widths[kind];
        if (bytes>4 || tag==pointer_tag) {
            if (value<first-10 || value>size-10 || bytes>size-10-value) return 0;
            nx_put(entry+8,value+growth,4);
        }
    }
    return 1;
}

static unsigned char *nx_entry(unsigned char *ifd,unsigned tag)
{
    for (unsigned i=0;i<nx_u16(ifd);++i) {
        unsigned char *entry=ifd+2+i*NX_ENTRY;
        if (nx_u16(entry)==tag) return entry;
    }
    return 0;
}

static int nx_match(const unsigned char *entry,unsigned kind,z72_u32 count)
{
    return entry && nx_u16(entry+2)==kind && nx_u32(entry+4)==count;
}

/* The buffer belongs to the module. Existing file bytes are unaffected until
 * this entire expanded note has been validated and appended successfully. */
static int nx_expand_note(unsigned char *note,z72_u32 size,z72_u32 group,unsigned frame,
                          unsigned shots,int packed_raw)
{
    static const unsigned char header[]={ 'N','i','k','o','n',0,2,0x11,0,0,'I','I',42,0,8,0,0,0 };
    static const unsigned char version[]={'0','1','0','0'};
    if (size<24 || !nx_equal(note,header,sizeof(header))) return 0;
    unsigned char *ifd=note+18;
    z72_u32 end=24+nx_u16(ifd)*NX_ENTRY;
    if (end>size || nx_u32(note+end-4) || nx_entry(ifd,0x51) ||
        nx_entry(ifd,0x56) || nx_entry(ifd,0x5e)) return 0;
    unsigned char *position=nx_entry(ifd,0x54),*preview=nx_entry(ifd,0x11);
    unsigned char *compression=nx_entry(ifd,0x93);
    if (!nx_match(position,7,20) || !nx_match(preview,4,1) || !nx_match(compression,3,1)) return 0;
    unsigned format=nx_u16(compression+8);
    if (format!=NX_RAW_PACKED_14 || !packed_raw) return 0;
    z72_u32 pos=nx_u32(position+8),prev=nx_u32(preview+8);
    if (pos<end-10 || pos>size-30 || prev<end-10 || prev>size-16) return 0;
    if (!nx_equal(note+pos+10,version,4) || !nx_u32(note+pos+18) || !nx_u32(note+pos+26)) return 0;
    unsigned char *preview_ifd=note+prev+10;
    z72_u32 preview_bytes=6+nx_u16(preview_ifd)*NX_ENTRY;
    if (preview_bytes>size-10-prev || nx_u32(preview_ifd+preview_bytes-4) ||
        !nx_relocate(preview_ifd,end,size,0x201,NX_GROWTH) ||
        !nx_relocate(ifd,end,size,0x11,NX_GROWTH)) return 0;
    for (z72_u32 i=size;i>end;--i) note[i+NX_GROWTH-1]=note[i-1];

    z72_u32 data=size+NX_GROWTH;
    nx_insert(ifd,0x51,24,data-10);
    nx_insert(ifd,0x56,16,data+24-10);
    nx_insert(ifd,0x5e,28,data+40-10);
    nx_put(note+end+NX_GROWTH-4,0,4);
    for (unsigned i=0;i<68;++i) note[data+i]=0;
    nx_put(note+data,0x32303130u,4); /* 0102 */
    nx_put(note+data+10,format,2);
    nx_put(note+data+24,0x30303130u,4);
    nx_put(note+data+28,group,4);
    nx_put(note+data+32,frame,4);
    nx_put(note+data+36,1,4);
    nx_put(note+data+40,0x30303130u,4);
    note[data+44]=(unsigned char)(shots==4 ? 1 : shots==8 ? 2 : shots==16 ? 3 : 4);
    note[data+45]=(unsigned char)(shots==8 && frame>4 ? frame+12 : frame);
    return 1;
}
