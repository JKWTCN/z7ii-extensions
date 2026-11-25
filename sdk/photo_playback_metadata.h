/* SPDX-License-Identifier: MIT */
#ifndef Z72_PHOTO_PLAYBACK_METADATA_H
#define Z72_PHOTO_PLAYBACK_METADATA_H
#include "photo_orientation.h"

struct photo_source {
    const unsigned char *data;
    z72_u32 size;
    const unsigned char *(*read)(const struct photo_source *,z72_u32,z72_u32);
};

struct photo_tiff {
    const struct photo_source *source;
    z72_u32 start,size,big,root;
};

static const unsigned char *photo_bytes(const struct photo_source *source,z72_u32 offset,z72_u32 size)
{
    if (offset>source->size || size>source->size-offset) return 0;
    return source->data ? source->data+offset : source->read(source,offset,size);
}

static const unsigned char *photo_tiff_bytes(const struct photo_tiff *tiff,z72_u32 offset,z72_u32 size)
{
    if (offset>tiff->size || size>tiff->size-offset) return 0;
    return photo_bytes(tiff->source,tiff->start+offset,size);
}

static z72_u32 photo_uint(const unsigned char *data,z72_u32 bytes,z72_u32 big)
{
    z72_u32 value=0;
    for (z72_u32 i=0;i<bytes;++i)
        value=value<<8|data[big ? i : bytes-i-1];
    return value;
}

static z72_u32 photo_tiff_open(struct photo_tiff *tiff,const struct photo_source *source,
                               z72_u32 start,z72_u32 size)
{
    if (size<8 || start>source->size || size>source->size-start) return 0;
    const unsigned char *data=photo_bytes(source,start,8);
    if (!data || data[0]!=data[1] || (data[0]!='I' && data[0]!='M')) return 0;
    tiff->source=source;
    tiff->start=start;
    tiff->size=size;
    tiff->big=data[0]=='M';
    tiff->root=photo_uint(data+4,4,tiff->big);
    return photo_uint(data+2,2,tiff->big)==42;
}

/* Return a TIFF-relative payload offset. The reader can reuse its buffer on
 * the next fetch, so callers keep offsets or decoded values, never pointers. */
static z72_u32 photo_tag(const struct photo_tiff *tiff,z72_u32 ifd,z72_u32 tag,
                         z72_u32 type,z72_u32 unit,z72_u32 *count)
{
    if (ifd<8) return 0;
    const unsigned char *header=photo_tiff_bytes(tiff,ifd,2);
    if (!header) return 0;
    z72_u32 entries=photo_uint(header,2,tiff->big),size=tiff->size-ifd-2;
    if (entries>size/12) return 0;
    for (z72_u32 i=0,at=ifd+2;i<entries;++i,at+=12) {
        const unsigned char *entry=photo_tiff_bytes(tiff,at,12);
        if (!entry) return 0;
        if (photo_uint(entry,2,tiff->big)!=tag) continue;
        z72_u32 n=photo_uint(entry+4,4,tiff->big);
        if (photo_uint(entry+2,2,tiff->big)!=type || !n || n>tiff->size || n*unit>tiff->size) return 0;
        *count=n;
        if (n*unit<=4) return at+8;
        z72_u32 offset=photo_uint(entry+8,4,tiff->big);
        return offset>=8 && offset<=tiff->size-n*unit ? offset : 0;
    }
    return 0;
}

static z72_u32 photo_equal(const unsigned char *data,const char *expected,z72_u32 size)
{
    if (!data) return 0;
    for (z72_u32 i=0;i<size;++i)
        if (data[i]!=(unsigned char)expected[i]) return 0;
    return 1;
}

/* ShotInfo 0803 encrypts bytes starting after its four-byte version.
 * Skip to the requested word by summing the stock cipher's increments. */
static z72_u32 photo_shot_word(const unsigned char *data,z72_u32 offset,z72_u32 key,z72_u32 seed)
{
    z72_u32 steps=(offset-4)&511u; /* The accumulated byte key repeats every 512 bytes. */
    key+=seed*(steps*0x60u+steps*(steps-1u)/2u);
    z72_u32 word=0;
    for (z72_u32 i=0;i<4;++i) {
        key+=seed*(0x60u+steps+i);
        word|=(z72_u32)(data[i]^(unsigned char)key)<<(8*i);
    }
    return word;
}

static z72_u32 photo_shot_inverted(const struct photo_tiff *tiff)
{
    z72_u32 count;
    z72_u32 model=photo_tag(tiff,tiff->root,0x110,2,1,&count);
    if (!model || count!=12 || !photo_equal(photo_tiff_bytes(tiff,model,12),"NIKON Z 7_2",12)) return 0;
    z72_u32 exif=photo_tag(tiff,tiff->root,0x8769,4,4,&count);
    if (!exif || count!=1) return 0;
    const unsigned char *ifd=photo_tiff_bytes(tiff,exif,4);
    if (!ifd) return 0;
    z72_u32 note=photo_tag(tiff,photo_uint(ifd,4,tiff->big),0x927c,7,1,&count);
    struct photo_tiff maker;
    if (!note || count<18 || !photo_equal(photo_tiff_bytes(tiff,note,7),"Nikon\0\002",7) ||
        !photo_tiff_open(&maker,tiff->source,tiff->start+note+10,count-10)) return 0;
    z72_u32 serial=photo_tag(&maker,maker.root,0x1d,2,1,&count);
    if (!serial || count<2 || count>16) return 0;
    const unsigned char *digits=photo_tiff_bytes(&maker,serial,count);
    if (!digits || digits[count-1]) return 0;
    z72_u32 serial_key=0;
    for (z72_u32 i=0;i<count-1;++i) {
        z72_u32 digit=digits[i]-'0';
        if (digit>9) return 0;
        serial_key=serial_key*10+digit;
    }
    z72_u32 shutter=photo_tag(&maker,maker.root,0xa7,4,4,&count);
    if (!shutter || count!=1) return 0;
    const unsigned char *frames=photo_tiff_bytes(&maker,shutter,4);
    if (!frames) return 0;
    z72_u32 shot_key=frames[0]^frames[1]^frames[2]^frames[3];
    z72_u32 shot_bytes,shot=photo_tag(&maker,maker.root,0x91,7,1,&shot_bytes);
    if (!shot || shot_bytes<0xa8 || !photo_equal(photo_tiff_bytes(&maker,shot,4),"0803",4)) return 0;
    z72_u32 seed=((const unsigned char *)0x12068376u)[serial_key&255u];
    z72_u32 key=((const unsigned char *)0x12068276u)[shot_key];
    const unsigned char *pointer=photo_tiff_bytes(&maker,shot+0x98,4);
    if (!pointer) return 0;
    z72_u32 offset=photo_shot_word(pointer,0x98,key,seed);
    if (offset<0xa8 || offset>shot_bytes-0x6c) return 0;
    const unsigned char *sample=photo_tiff_bytes(&maker,shot+offset,8);
    if (!sample) return 0;
    const z72_u32 angles[]={photo_shot_word(sample,offset,key,seed),
                            photo_shot_word(sample+4,offset+4,key,seed)};
    return photo_roll_inverted(angles);
}

static z72_u32 photo_header_tiff(struct photo_tiff *tiff,const struct photo_source *source)
{
    const unsigned char *start=photo_bytes(source,0,2);
    if (!start) return 0;
    if (start[0]!=0xff || start[1]!=0xd8) return photo_tiff_open(tiff,source,0,source->size);
    for (z72_u32 at=2;;) {
        const unsigned char *segment=photo_bytes(source,at,4);
        if (!segment || segment[0]!=0xff || segment[1]==0xda || segment[1]==0xd9) return 0;
        z72_u32 span=photo_uint(segment+2,2,1);
        if (span<2 || span>source->size-at-2) return 0;
        if (segment[1]==0xe1 && span>=8 && photo_equal(photo_bytes(source,at+4,6),"Exif\0",6))
            return photo_tiff_open(tiff,source,at+10,span-8);
        at+=span+2;
    }
}

static z72_u32 photo_orientation_tag(const struct photo_tiff *tiff)
{
    z72_u32 count,tag=photo_tag(tiff,tiff->root,0x112,3,2,&count);
    return tag && count==1 ? tag : 0;
}

static z72_u32 photo_header_inverted(const unsigned char *data,z72_u32 size)
{
    const struct photo_source source={data,size,0};
    struct photo_tiff tiff;
    if (!photo_header_tiff(&tiff,&source)) return 0;
    z72_u32 tag=photo_orientation_tag(&tiff);
    const unsigned char *value=tag ? photo_tiff_bytes(&tiff,tag,2) : 0;
    if (!value) return 0;
    z72_u32 orientation=photo_uint(value,2,tiff.big);
    return orientation==3 || (orientation==1 && photo_shot_inverted(&tiff));
}

#endif
