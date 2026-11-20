/* SPDX-License-Identifier: MIT */
#include "pixel_shift_nef_note.h"

enum { NX_OK = 0x043a0080, NX_SCAN_BYTES = 2048 };
enum { NX_PATH = 1, NX_OPEN, NX_LAYOUT, NX_MEMORY, NX_APPEND, NX_REOPEN, NX_COMMIT };
static _Alignas(64) unsigned char nx_scan[NX_SCAN_BYTES];

struct nx_reference {
    z72_u32 offset, size, original[2], replacement[2];
};

static int nx_get_path(z72_u32 id,char path[96])
{
    /* The same reference, path and release calls used by native playback. */
    void *image=CALL(0x1063ea8bu,void *,z72_u32)(id);
    if (!image) return 0;
    const char *source=CALL(0x10567f61u,const char *,void *)(image);
    unsigned i=0;
    if (source) for (;i<95 && source[i];++i) path[i]=source[i];
    path[i]=0;
    CALL(0x1056788bu,void,void *)(image);
    return i>=16 && i<95 && (path[0]=='A' || path[0]=='B') && path[1]==':' &&
        nx_equal((const unsigned char *)path+2,(const unsigned char *)"\\DCIM\\",6) &&
        nx_equal((const unsigned char *)path+i-4,(const unsigned char *)".NEF",4);
}

static int nx_open(z72_u32 *file,const char *path)
{
    CALL(0x104d3c01u,void,z72_u32 *,const char *)(file,path);
    /* Mode 2 opens an existing file for read/write without creating it. */
    return CALL(0x104d3c05u,z72_u32,z72_u32 *,z72_u32,z72_u32)(file,2,0x20)==NX_OK;
}

static int nx_close(z72_u32 *file)
{
    return CALL(0x104d3c91u,z72_u32,z72_u32 *)(file)==NX_OK;
}

static int nx_seek(z72_u32 *file,z72_u32 offset)
{
    z72_u32 position=~0u;
    return CALL(0x104d3d91u,int,z72_u32 *,z72_u32,z72_u32,z72_u32 *)(file,offset,0,&position) &&
        position==offset;
}

static int nx_io(z72_u32 *file,z72_u32 entry,void *data,z72_u32 bytes)
{
    z72_u32 actual=0;
    struct { z72_u32 kind; void *buffer; z72_u32 capacity; } request={1,data,(bytes+7u)&~7u};
    return CALL(entry,z72_u32,z72_u32 *,const void *,z72_u32,z72_u32 *)(file,&request,bytes,&actual)==NX_OK &&
        actual==bytes;
}

static int nx_read(z72_u32 *file,z72_u32 offset,void *data,z72_u32 bytes)
{
    z72_u32 cache=(bytes+63u)&~63u;
    if (!nx_seek(file,offset)) return 0;
    CALL(0x1034fa29u,void,void *,z72_u32)(data,cache);
    int ok=nx_io(file,0x104d3d83u,data,bytes);
    CALL(0x1034fa29u,void,void *,z72_u32)(data,cache);
    return ok;
}

static int nx_write(z72_u32 *file,z72_u32 offset,void *data,z72_u32 bytes)
{
    if (!nx_seek(file,offset)) return 0;
    CALL(0x1034f9edu,void,void *,z72_u32)(data,(bytes+63u)&~63u);
    return nx_io(file,0x104d3d43u,data,bytes);
}

static int nx_directory(z72_u32 *file,z72_u32 offset)
{
    return offset<=file[6]-NX_SCAN_BYTES && nx_read(file,offset,nx_scan,NX_SCAN_BYTES) &&
        nx_u16(nx_scan)<=(NX_SCAN_BYTES-6)/NX_ENTRY;
}

static z72_u32 nx_tag_value(unsigned tag,unsigned kind)
{
    unsigned char *entry=nx_entry(nx_scan,tag);
    return nx_match(entry,kind,1) ? nx_u32(entry+8) : 0;
}

/* Validate the FX/L layout and inspect the strip descriptor. */
static z72_u32 nx_find_note(z72_u32 *file,z72_u32 *where,z72_u32 *bytes,z72_u32 *root_ifd,
                           int *packed_raw)
{
    if (file[7] || file[6]<NX_SCAN_BYTES || file[6]>0x7ff00000u ||
        !nx_read(file,0,nx_scan,8) || nx_u32(nx_scan)!=0x002a4949u) return 0;
    z72_u32 root=nx_u32(nx_scan+4);
    if (root<8 || !nx_directory(file,root)) return 0;
    *root_ifd=root;
    unsigned char *model=nx_entry(nx_scan,0x110),*sub=nx_entry(nx_scan,0x14a);
    z72_u32 exif=nx_tag_value(0x8769,4);
    if (!nx_match(model,2,12) || !sub || nx_u16(sub+2)!=4 || nx_u32(sub+4)<2 ||
        !exif) return 0;
    z72_u32 model_offset=nx_u32(model+8),sub_offset=nx_u32(sub+8);
    if (model_offset>file[6]-12 || sub_offset>file[6]-8 ||
        !nx_read(file,model_offset,nx_scan,12) ||
        !nx_equal(nx_scan,(const unsigned char *)"NIKON Z 7_2",12) ||
        !nx_read(file,sub_offset,nx_scan,8)) return 0;
    z72_u32 raw=nx_u32(nx_scan+4);
    if (!raw || !nx_directory(file,raw) || !nx_match(nx_entry(nx_scan,0xfe),4,1) ||
        nx_tag_value(0xfe,4) || nx_tag_value(0x100,4)!=8288 || nx_tag_value(0x101,4)!=5520 ||
        nx_tag_value(0x102,3)!=14 || nx_tag_value(0x103,3)!=34713) return 0;
    /* The native uncompressed 14-bit strip has 16-byte aligned rows. */
    enum { PACKED_RAW_BYTES = ((8288 * 14 / 8 + 15) & ~15) * 5520 };
    z72_u32 strip=nx_tag_value(0x111,4);
    *packed_raw=nx_tag_value(0x106,3)==32803 && nx_tag_value(0x115,3)==1 &&
        nx_tag_value(0x116,4)==5520 && nx_tag_value(0x117,4)==PACKED_RAW_BYTES &&
        strip>=8 && strip<=file[6] && PACKED_RAW_BYTES<=file[6]-strip;
    if (!nx_directory(file,exif)) return 0;
    unsigned char *desc=nx_entry(nx_scan,0x927c);
    if (!desc || nx_u16(desc+2)!=7) return 0;
    *bytes=nx_u32(desc+4);
    *where=nx_u32(desc+8);
    if (*bytes<24 || *bytes>NX_NOTE_LIMIT || *where<=exif || *where>file[6] || *bytes>file[6]-*where) return 0;
    return exif+(z72_u32)(desc-nx_scan)+4;
}

static int nx_reference_matches(z72_u32 *file,const struct nx_reference *ref,const z72_u32 *values)
{
    if (!nx_read(file,ref->offset,nx_scan,ref->size)) return 0;
    for (unsigned i=0;i<ref->size/4;++i) if (nx_u32(nx_scan+4*i)!=values[i]) return 0;
    return 1;
}

static int nx_write_reference(z72_u32 *file,const struct nx_reference *ref,const z72_u32 *values)
{
    for (unsigned i=0;i<ref->size/4;++i) nx_put(nx_scan+4*i,values[i],4);
    return nx_write(file,ref->offset,nx_scan,ref->size);
}

/* NX Studio 1.10.1 derives XMP noise reduction for 8/32 frames with a separate
 * camera table. Z7 II is absent. Point such files at a copy of IFD0 without
 * tag 700. The old IFD0, EXIF and XMP packet stay intact in the file. */
static int nx_compat_directories(z72_u32 *file,z72_u32 root,unsigned char *payload,
                                 z72_u32 offset,z72_u32 *total,struct nx_reference *ref)
{
    if (!nx_directory(file,root)) return 0;
    if (!nx_entry(nx_scan,700)) return 1;
    z72_u32 exif=nx_tag_value(0x8769,4);
    if (!exif || ref->offset<exif || !nx_directory(file,exif)) return 0;
    unsigned exif_bytes=6+NX_ENTRY*nx_u16(nx_scan);
    unsigned desc=ref->offset-exif;
    if (desc>exif_bytes-8) return 0;
    if (*total&1u) payload[(*total)++]=0;
    z72_u32 exif_copy=offset+*total;
    nx_put(nx_scan+desc,ref->replacement[0],4);
    nx_put(nx_scan+desc+4,offset,4);
    for (unsigned i=0;i<exif_bytes;++i) payload[(*total)++]=nx_scan[i];

    if (!nx_directory(file,root)) return 0;
    unsigned char *exif_entry=nx_entry(nx_scan,0x8769),*xmp=nx_entry(nx_scan,700);
    if (!exif_entry || !xmp) return 0;
    nx_put(exif_entry+8,exif_copy,4);
    unsigned count=nx_u16(nx_scan),root_bytes=6+NX_ENTRY*count;
    nx_put(nx_scan,count-1,2);
    z72_u32 root_copy=offset+*total;
    unsigned omit=xmp-nx_scan;
    for (unsigned i=0;i<root_bytes;++i)
        if (i<omit || i>=omit+NX_ENTRY) payload[(*total)++]=nx_scan[i];
    ref->offset=4;
    ref->size=4;
    ref->original[0]=root;
    ref->replacement[0]=root_copy;
    return 1;
}

static unsigned nx_tag_file(z72_u32 id,z72_u32 group,unsigned frame,unsigned shots)
{
    char path[96];
    for (unsigned i=0;!nx_get_path(id,path);++i) {
        if (i==15) return NX_PATH;
        pixel_sleep(40);
    }
    z72_u32 file[10];
    if (!nx_open(file,path)) return NX_OPEN;
    int packed_raw=0;
    z72_u32 where=0,bytes=0,root=0,patch=nx_find_note(file,&where,&bytes,&root,&packed_raw),end=file[6];
    z72_u32 total=bytes+NX_EXTRA,offset=end+(end&1u);
    int compat=shots==8 || shots==32;
    struct nx_reference ref={patch,8,{bytes,where},{total,offset}};
    unsigned char *note=patch ? CALL(0x1034fe43u,void *,z72_u32,z72_u32,z72_u32)(
        total+64+(compat ? 2*NX_SCAN_BYTES : 0),0,64) : 0;
    unsigned result=patch ? NX_MEMORY : NX_LAYOUT;
    if (note) {
        result=NX_LAYOUT;
        if (nx_read(file,where,note,bytes) && nx_expand_note(note,bytes,group,frame,shots,packed_raw) &&
            (!compat || nx_compat_directories(file,root,note,offset,&total,&ref))) {
            result=NX_APPEND;
            nx_scan[0]=0;
            if ((end==offset || nx_write(file,end,nx_scan,1)) && nx_write(file,offset,note,total)) result=0;
        }
        CALL(0x1034febdu,void,void *)(note);
    }
    /* Close before changing the EXIF reference so an interrupted append
     * leaves the original MakerNote usable. Confirm the physical extent. */
    if (!nx_close(file) && !result) result=NX_APPEND;
    if (result) return result;
    if (!nx_open(file,path)) return NX_REOPEN;
    unsigned ok=file[7]==0 && file[6]==offset+total && nx_reference_matches(file,&ref,ref.original);
    result=NX_REOPEN;
    if (ok) {
        result=NX_COMMIT;
        if (nx_write_reference(file,&ref,ref.replacement) && nx_reference_matches(file,&ref,ref.replacement)) result=0;
        else nx_write_reference(file,&ref,ref.original);
    }
    if (!nx_close(file) && !result) result=NX_COMMIT;
    return result;
}
