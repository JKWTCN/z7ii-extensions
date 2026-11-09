/* SPDX-License-Identifier: MIT */
#ifndef Z72_H
#define Z72_H
typedef unsigned int z72_u32;
#define Z72_ABI 1u
#define Z72_TARGET 0x01700072u

/* Valid for the lifetime of a successfully initialized module. */
struct z72_api {
    z72_u32 abi, size, target;
    z72_u32 (*system_mode)(void);
    /* Three native 32-byte grid templates; remains owned by the module. */
    z72_u32 (*grid_table)(const unsigned short *table);
};

/* Return zero to retain the image and its BSS until power-off.
 * On failure return nonzero, with no outstanding tasks or callbacks.
 * Initializers must return; native modules are trusted code, not sandboxed.
 */
z72_u32 z72_main(const struct z72_api *api);
#endif
