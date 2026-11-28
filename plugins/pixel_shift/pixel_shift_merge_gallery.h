/* SPDX-License-Identifier: MIT */
#include "dcim_index.h"
#include "ftp_photo_grid_layout.h"

enum { MERGE_GALLERY_TILES=FTP_PHOTO_GRID_TILES, MERGE_THUMB_W=160, MERGE_THUMB_H=120,
       MERGE_THUMB_RGB_BYTES=MERGE_THUMB_W*MERGE_THUMB_H*3 };
struct merge_group {
    z72_u32 ids[PIXEL_MAX_SHOTS];
    unsigned count,packed,thumbnail;
    char label[20];
};
struct merge_gallery_cursor {
    struct dcim_folder folder;
    z72_u32 id;
};
struct merge_gallery_page {
    struct merge_gallery_cursor start;
    struct merge_gallery_page *previous,*next;
};
static struct merge_gallery_page merge_gallery_first,*merge_gallery_page;
static struct merge_gallery_cursor merge_gallery_next;
static struct merge_group merge_gallery_groups[MERGE_GALLERY_TILES];
static unsigned char (*merge_gallery_pixels)[MERGE_THUMB_W*MERGE_THUMB_H*4];
static void *merge_gallery_pixels_owner;
static unsigned merge_gallery_count;

static void merge_gallery_reset(void)
{
    /* Allocate the previews only when used to stay within the module image limit. */
    if (!merge_gallery_pixels)
        merge_gallery_pixels=nx_alloc(MERGE_GALLERY_TILES*sizeof(*merge_gallery_pixels),
                                     &merge_gallery_pixels_owner);
    struct merge_gallery_page *page=merge_gallery_first.next;
    while (page) {
        struct merge_gallery_page *next=page->next;
        CALL(0x1034fe7fu,void,void *)(page);
        page=next;
    }
    merge_gallery_first=(struct merge_gallery_page){{{DCIM_SD,DCIM_FOLDER_START},0},0,0};
    merge_gallery_page=&merge_gallery_first;
}

static z72_u32 merge_gallery_previous_id(struct merge_gallery_cursor *cursor)
{
    while (cursor->folder.number) {
        if (cursor->folder.number!=DCIM_FOLDER_START) {
            cursor->id=dcim_find_photo(cursor->folder.card,cursor->folder.number,cursor->id,1,0);
            if (cursor->id) return cursor->id;
        }
        if (!dcim_previous_card_folder(&cursor->folder)) break;
        cursor->id=0;
    }
    return 0;
}

struct merge_group_member {
    z72_u32 group;
    unsigned frame,count,packed,root;
};

static int merge_gallery_member(z72_u32 *file,struct merge_group_member *member)
{
    z72_u32 at,bytes,root;
    int packed;
    if (!nx_find_note(file,&at,&bytes,&root,&packed) ||
        !nx_read(file,at,nx_scan,bytes<NX_SCAN_BYTES ? bytes : NX_SCAN_BYTES) ||
        !nx_equal(nx_scan,(const unsigned char *)"Nikon\0\2\x11\0\0II*\0\10\0\0",18)) return 0;
    unsigned end=24+NX_ENTRY*nx_u16(nx_scan+18);
    if (end>NX_SCAN_BYTES || end>bytes || nx_u32(nx_scan+end-4)) return 0;
    const unsigned char *sequence=nx_entry(nx_scan+18,0x56),*shot=nx_entry(nx_scan+18,0x5e);
    const unsigned char *format=nx_entry(nx_scan+18,0x93);
    if (!nx_match(sequence,7,16) || !nx_match(shot,7,28) || !nx_match(format,3,1) ||
        nx_u16(format+8)!=(packed ? NX_RAW_PACKED_14 : NX_RAW_LOSSLESS)) return 0;
    unsigned sequence_at=nx_u32(sequence+8),shot_at=nx_u32(shot+8);
    if (bytes<38 || sequence_at<end-10 || sequence_at>bytes-26 ||
        shot_at<end-10 || shot_at>bytes-38 ||
        !nx_read(file,at+10+sequence_at,nx_scan,16) || nx_u32(nx_scan)!=0x30303130u) return 0;
    z72_u32 group=nx_u32(nx_scan+4),frame=nx_u32(nx_scan+8);
    if (!group || !frame || nx_u32(nx_scan+12)!=1 ||
        !nx_read(file,at+10+shot_at,nx_scan,28) || nx_u32(nx_scan)!=0x30303130u) return 0;
    unsigned mode=nx_scan[4];
    if (!mode || mode>4 || frame>(4u<<(mode-1))) return 0;
    *member=(struct merge_group_member){group,frame,4u<<(mode-1),(unsigned)packed,root};
    return 1;
}

/* Tagged series are written consecutively. Only publish groups with every
 * sequence index and the original group identifier on the same card. */
static int merge_gallery_accept(struct merge_group *group,z72_u32 *key,unsigned *expected,
                                unsigned *columns,z72_u32 id,const struct merge_group_member *member)
{
    if (!*expected || member->group!=*key || member->count!=group->count ||
        member->frame!=pixel_grid_index(*expected-1u,*columns)+1u ||
        dcim_id_card(id)!=dcim_id_card(group->ids[pixel_grid_index(group->count-1u,*columns)])) {
        *expected=0;
        /* The newest file is frame 7 for column order and the highest frame for sequential series. */
        *columns=member->count==PIXEL_COLUMN_ORDER_SHOTS &&
            member->frame==pixel_grid_index(PIXEL_COLUMN_ORDER_SHOTS-1u,1u)+1u;
        if (member->frame!=member->count && !*columns) return 0;
        *expected=member->count;
        group->count=member->count;
        group->packed=member->packed;
        *key=member->group;
    }
    group->ids[member->frame-1]=id;
    group->packed&=member->packed;
    return --*expected==0;
}

static int merge_gallery_thumbnail(z72_u32 *file,unsigned root,unsigned slot)
{
    if (!merge_gallery_pixels || !nx_directory(file,root) || nx_tag_value(0x100,4)!=MERGE_THUMB_W ||
        nx_tag_value(0x101,4)!=MERGE_THUMB_H || nx_tag_value(0x103,3)!=1 ||
        nx_tag_value(0x106,3)!=2 || nx_tag_value(0x115,3)!=3 || nx_tag_value(0x11c,3)!=1 ||
        nx_tag_value(0x117,4)!=MERGE_THUMB_RGB_BYTES) return 0;
    unsigned offset=nx_tag_value(0x111,4);
    unsigned char *rgba=merge_gallery_pixels[slot];
    if (offset<8 || offset>file[6] || MERGE_THUMB_RGB_BYTES>file[6]-offset ||
        !nx_read(file,offset,rgba,MERGE_THUMB_RGB_BYTES)) return 0;
    /* Expand from the end so unread RGB samples stay intact. */
    for (unsigned i=MERGE_THUMB_W*MERGE_THUMB_H;i--;) {
        rgba[4*i+3]=255;
        rgba[4*i+2]=rgba[3*i+2];
        rgba[4*i+1]=rgba[3*i+1];
        rgba[4*i]=rgba[3*i];
    }
    CALL(0x1034f9edu,void,const void *,z72_u32)(rgba,sizeof(merge_gallery_pixels[slot]));
    return 1;
}

static void merge_gallery_label(struct merge_group *group,const char *path)
{
    const char *name=path;
    for (const char *p=path;*p;++p) if (*p=='\\') name=p+1;
    char *out=group->label;
    while (*name && *name!='.' && out<group->label+10) *out++=*name++;
    *out++=' ';
    *out++='(';
    if (group->count>=10) *out++=(char)('0'+group->count/10);
    *out++=(char)('0'+group->count%10);
    *out++=')';
    *out=0;
}

static void merge_gallery_scan(int (*stop_requested)(void))
{
    unsigned count=0,expected=0,checked=0,columns=0;
    z72_u32 key=0;
    struct merge_gallery_cursor cursor=merge_gallery_page->start;
    while (count<MERGE_GALLERY_TILES && !stop_requested()) {
        z72_u32 id=merge_gallery_previous_id(&cursor);
        if (!id) break;
        if ((++checked&7u)==0) pixel_sleep(1);
        char path[96];
        z72_u32 file[10];
        if (!nx_get_path(id,path) || !merge_open_read(file,path)) continue;
        struct merge_group_member member;
        struct merge_group *group=&merge_gallery_groups[count];
        int complete=merge_gallery_member(file,&member) &&
            merge_gallery_accept(group,&key,&expected,&columns,id,&member);
        if (complete) {
            merge_gallery_label(group,path);
            group->thumbnail=merge_gallery_thumbnail(file,member.root,count);
        }
        nx_close(file);
        count+=complete;
    }
    merge_gallery_next=cursor;
    merge_gallery_count=count;
}
