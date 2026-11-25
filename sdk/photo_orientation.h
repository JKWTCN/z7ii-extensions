/* SPDX-License-Identifier: MIT */
#ifndef Z72_PHOTO_ORIENTATION_H
#define Z72_PHOTO_ORIENTATION_H
#include "z72.h"

static inline z72_u32 photo_roll_inverted(const z72_u32 *angles)
{
    /* Nominal orientation sector of 0x103D41FC. The pitch check uses
     * the stock level classifier to exclude straight-up/down views. */
    return angles[0]>(135u<<16) && angles[0]<=(225u<<16) &&
        angles[1]<(360u<<16) &&
        !((z72_u32 (*)(const z72_u32 *,z72_u32))0x103d41c5u)(angles,3);
}

#endif
