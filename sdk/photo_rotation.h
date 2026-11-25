/* SPDX-License-Identifier: MIT */
#ifndef Z72_PHOTO_ROTATION_H
#define Z72_PHOTO_ROTATION_H
#include "photo_orientation.h"

enum {
    PHOTO_IFD_GET_SLOT=0xb18897c4u, PHOTO_IFD_GET_STOCK=0x104fbf73u,
    PHOTO_IFD_COUNT=0x104fbf9fu, PHOTO_IFD_STOCK=0x119c19b0u,
    PHOTO_ORIENTATION_WRITE=0x104f6c0fu, PHOTO_CAPTURE_ORIENTATION=0x2855,
    PHOTO_CAPTURE_ANGLES_SIZE=0x2c70, PHOTO_CAPTURE_ANGLES=0x2c74,
    PHOTO_CAPTURE_ANGLES_BYTES=0x6c,
    PHOTO_IFD_ENTRIES=0x26
};

struct photo_ifd_entry {
    z72_u32 type_tag,count,flags;
    void (*write)(unsigned short *,const z72_u32 *,void *);
};

static struct photo_ifd_entry photo_ifd_entries[PHOTO_IFD_ENTRIES];

static void photo_orientation_write(unsigned short *entry,const z72_u32 *metadata,void *writer)
{
    const unsigned char *capture=(const unsigned char *)metadata[2];
    z72_u32 pose=capture[PHOTO_CAPTURE_ORIENTATION];
    const z72_u32 *angles=*(const z72_u32 *const *)(capture+PHOTO_CAPTURE_ANGLES);
    /* This is the exposure's sensor sample, also used by Nikon ShotInfo.
     * The discrete pose can stay horizontal while roll shows an inversion. */
    if ((pose==0 || pose==4) && angles &&
        *(const z72_u32 *)(capture+PHOTO_CAPTURE_ANGLES_SIZE)==PHOTO_CAPTURE_ANGLES_BYTES &&
        photo_roll_inverted(angles)) pose=3;
    if (pose==3) {
        entry[4]=3; /* Exif IFD0 orientation, rotate the stored image by 180°. */
        return;
    }
    ((void (*)(unsigned short *,const z72_u32 *,void *))PHOTO_ORIENTATION_WRITE)(
        entry,metadata,writer);
}

static const struct photo_ifd_entry *photo_ifd_get(void)
{
    return photo_ifd_entries;
}

static void photo_rot_install(void)
{
    volatile z72_u32 *slot=(volatile z72_u32 *)PHOTO_IFD_GET_SLOT;
    const struct photo_ifd_entry *stock=(const struct photo_ifd_entry *)PHOTO_IFD_STOCK;
    if (slot[0]!=PHOTO_IFD_GET_STOCK || slot[1]!=PHOTO_IFD_COUNT ||
        stock[9].type_tag!=0x00030112u || stock[9].count!=1 ||
        (z72_u32)stock[9].write!=PHOTO_ORIENTATION_WRITE) return;
    for (z72_u32 i=0;i<PHOTO_IFD_ENTRIES;++i) photo_ifd_entries[i]=stock[i];
    photo_ifd_entries[9].write=photo_orientation_write;
    __atomic_store_n(slot,(z72_u32)photo_ifd_get,__ATOMIC_RELEASE);
}

#endif
