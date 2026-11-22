/* SPDX-License-Identifier: MIT */
#ifndef Z72_WIFI_GALLERY_PREVIEW_H
#define Z72_WIFI_GALLERY_PREVIEW_H

struct gallery_image { z72_u32 offset, length; };
struct gallery_tiff { z72_u32 base, end, little; };
struct gallery_ifd {
    z72_u32 next, exif, note, note_length, sub, sub_count, preview;
    z72_u32 jpeg, jpeg_length, strip, strip_length, width;
};

static _Alignas(64) unsigned char gallery_metadata[2368];

static int gallery_seek(z72_u32 *file,z72_u32 offset)
{
    z72_u32 actual=~0u;
    return Z72_CARD_CALL(0x104d3d91u,int,z72_u32 *,z72_u32,z72_u32,z72_u32 *)(
        file,offset,0,&actual) && actual==offset;
}

static int metadata_read(z72_u32 *file,z72_u32 offset,z72_u32 length)
{
    return offset<=file[6] && length<=file[6]-offset &&
        length<sizeof(gallery_metadata) && gallery_seek(file,offset) &&
        card_read(file,gallery_metadata,sizeof(gallery_metadata),length,offset+length==file[6]);
}

static z72_u32 tiff_u16(const unsigned char *p,z72_u32 little)
{
    return little ? p[0]|((z72_u32)p[1]<<8) : ((z72_u32)p[0]<<8)|p[1];
}

static z72_u32 tiff_u32(const unsigned char *p,z72_u32 little)
{
    return little ? tiff_u16(p,1)|(tiff_u16(p+2,1)<<16) :
        (tiff_u16(p,0)<<16)|tiff_u16(p+2,0);
}

static int tiff_read(z72_u32 *file,const struct gallery_tiff *tiff,z72_u32 offset,z72_u32 length)
{
    z72_u32 extent=tiff->end-tiff->base;
    return offset<=extent && length<=extent-offset &&
        metadata_read(file,tiff->base+offset,length);
}

static z72_u32 tiff_start(z72_u32 *file,struct gallery_tiff *tiff)
{
    if (!tiff_read(file,tiff,0,8)) return 0;
    const unsigned char *p=gallery_metadata;
    if (p[0]!=p[1] || (p[0]!='I' && p[0]!='M')) return 0;
    tiff->little=p[0]=='I';
    return tiff_u16(p+2,tiff->little)==42 ? tiff_u32(p+4,tiff->little) : 0;
}

static int tiff_ifd(z72_u32 *file,const struct gallery_tiff *tiff,z72_u32 offset,
                    struct gallery_ifd *ifd)
{
    if (!offset || !tiff_read(file,tiff,offset,2)) return 0;
    z72_u32 count=tiff_u16(gallery_metadata,tiff->little);
    if (count>192u || !tiff_read(file,tiff,offset+2u,count*12u+4u)) return 0;
    ifd->next=tiff_u32(gallery_metadata+count*12u,tiff->little);
    for (z72_u32 i=0;i<count;++i) {
        const unsigned char *entry=gallery_metadata+i*12u;
        z72_u32 tag=tiff_u16(entry,tiff->little),type=tiff_u16(entry+2,tiff->little);
        z72_u32 n=tiff_u32(entry+4,tiff->little),value=tiff_u32(entry+8,tiff->little);
        if (tag==0x927cu && type==7u) {
            ifd->note=value;
            ifd->note_length=n;
        }
        if (tag==330u && type==4u && n && n<=8u) {
            ifd->sub=value;
            ifd->sub_count=n;
        }
        if (n!=1u || (type!=3u && type!=4u && type!=13u)) continue;
        if (type==3u) value=tiff_u16(entry+8,tiff->little);
        switch (tag) {
        case 0x11: ifd->preview=value; break;
        case 0x100: ifd->width=value; break;
        case 0x111: ifd->strip=value; break;
        case 0x117: ifd->strip_length=value; break;
        case 0x201: ifd->jpeg=value; break;
        case 0x202: ifd->jpeg_length=value; break;
        case 0x8769: ifd->exif=value; break;
        }
    }
    return 1;
}

static void tiff_candidate(z72_u32 *file,const struct gallery_tiff *tiff,
                           const struct gallery_ifd *ifd,struct gallery_image *choice,int large)
{
    z72_u32 offset=ifd->jpeg ? ifd->jpeg : ifd->strip;
    z72_u32 length=ifd->jpeg ? ifd->jpeg_length : ifd->strip_length;
    z72_u32 extent=tiff->end-tiff->base;
    if (!offset || offset>extent || length<4 || length>extent-offset ||
        length>2u*1024u*1024u || ifd->width>2048u ||
        !tiff_read(file,tiff,offset,3) || gallery_metadata[0]!=0xff ||
        gallery_metadata[1]!=0xd8 || gallery_metadata[2]!=0xff) return;
    if (!choice->length || (large ? length>choice->length : length<choice->length)) {
        choice->offset=tiff->base+offset;
        choice->length=length;
    }
}

static void nikon_candidate(z72_u32 *file,const struct gallery_tiff *tiff,z72_u32 exif,
                            struct gallery_image *choice,int large)
{
    struct gallery_ifd tags={0};
    if (!tiff_ifd(file,tiff,exif,&tags) || tags.note_length<18u ||
        tags.note>tiff->end-tiff->base || tags.note_length>tiff->end-tiff->base-tags.note ||
        !tiff_read(file,tiff,tags.note,10)) return;
    const unsigned char *p=gallery_metadata;
    if (p[0]!='N' || p[1]!='i' || p[2]!='k' || p[3]!='o' || p[4]!='n' ||
        p[5] || p[6]!=2) return;
    struct gallery_tiff maker={tiff->base+tags.note+10u,tiff->end,0};
    struct gallery_ifd note={0},preview={0};
    if (tiff_ifd(file,&maker,tiff_start(file,&maker),&note) &&
        tiff_ifd(file,&maker,note.preview,&preview))
        tiff_candidate(file,&maker,&preview,choice,large);
}

static int jpeg_tiff(z72_u32 *file,struct gallery_tiff *tiff)
{
    if (!metadata_read(file,0,2) || gallery_metadata[0]!=0xff || gallery_metadata[1]!=0xd8)
        return 0;
    z72_u32 offset=2;
    for (unsigned i=0;i<48u && metadata_read(file,offset,4);++i) {
        const unsigned char *p=gallery_metadata;
        unsigned marker=p[1],length=tiff_u16(p+2,0);
        if (p[0]!=0xff || marker==0xda || marker==0xd9) break;
        if (marker==0xff) { ++offset; continue; }
        if (length<2u || length>file[6]-offset-2u) break;
        if (marker==0xe1 && length>=16u && metadata_read(file,offset+4u,6) &&
            p[0]=='E' && p[1]=='x' && p[2]=='i' && p[3]=='f' && !p[4] && !p[5]) {
            tiff->base=offset+10u;
            tiff->end=offset+2u+length;
            return 1;
        }
        offset+=2u+length;
    }
    return 0;
}

static struct gallery_image gallery_embedded(z72_u32 *file,int raw,int large)
{
    struct gallery_image choice={0};
    struct gallery_tiff tiff={0,file[6],0};
    struct gallery_ifd root={0};
    if ((!raw && !jpeg_tiff(file,&tiff)) ||
        !tiff_ifd(file,&tiff,tiff_start(file,&tiff),&root)) return choice;
    struct gallery_ifd next={0};
    if (tiff_ifd(file,&tiff,root.next,&next))
        tiff_candidate(file,&tiff,&next,&choice,large);
    nikon_candidate(file,&tiff,root.exif,&choice,large);
    if (!raw || (!large && choice.length)) return choice;
    tiff_candidate(file,&tiff,&root,&choice,large);
    z72_u32 offsets[8];
    if (root.sub_count==1) offsets[0]=root.sub;
    else if (root.sub_count && tiff_read(file,&tiff,root.sub,root.sub_count*4u)) {
        for (z72_u32 i=0;i<root.sub_count;++i)
            offsets[i]=tiff_u32(gallery_metadata+i*4u,tiff.little);
    } else return choice;
    for (z72_u32 i=0;i<root.sub_count;++i) {
        struct gallery_ifd sub={0};
        if (tiff_ifd(file,&tiff,offsets[i],&sub))
            tiff_candidate(file,&tiff,&sub,&choice,large);
    }
    return choice;
}

#endif
