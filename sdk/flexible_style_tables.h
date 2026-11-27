/* SPDX-License-Identifier: MIT */
#ifndef Z72_FLEXIBLE_STYLE_TABLES_H
#define Z72_FLEXIBLE_STYLE_TABLES_H

#include <stddef.h>
#include <stdint.h>

/* Fixed, learned corrections, not a copy of the native calibration tables.
 * RGB knots are at min(32*i,4095), on the native RGB-curve output domain.
 * CPXS rows are A/B/C/D/E/F; the same delta is added to both native banks.
 */
struct z72_style_fixed {
    uint16_t rgb[3][129];
    int16_t cpxs_delta[6][32];
};

#define Z72_STYLE_RGB_BYTES 132u
#define Z72_STYLE_CPXS_BYTES 64u
#define Z72_STYLE_RGB_MASK 0x3fu
#define Z72_STYLE_CPXS_MASK 0xfffu

enum z72_style_chroma_mode {
    Z72_STYLE_CHROMA_NORMAL,
    Z72_STYLE_CHROMA_NONE,
    Z72_STYLE_CHROMA_LUMA_ONLY,
};

/* One instance per physical target, held under the caller's publication lock.
 * Originals are copied from native input, never from generated output. They
 * represent the sequence of successfully published native delta requests.
 */
struct z72_style_state {
    uint32_t rgb_valid, cpxs_valid, rgb_modified, cpxs_modified;
    uint32_t rgb_mode, matrix_valid; /* 0 unknown,1 alternating65,2 split129 */
    uint8_t rgb_kind[6]; /* 0 unknown,1 full65,2 first66,3 final63 */
    uint8_t matrix[176];
    _Alignas(4) uint16_t rgb[6][66];
    _Alignas(4) int16_t cpxs[12][32];
};

struct z72_style_transaction {
    struct z72_style_state next;
    uint32_t root[53], rgb_pointers[6], cpxs_pointers[22];
    uint16_t rgb[6][66];
    int16_t cpxs[12][32];
    uint32_t applied, restored, deferred;
};

static inline int z72_style_ram(uintptr_t address, size_t size)
{
    return !(address & 3u) && address >= 0xb0000000u &&
           address < 0xc0000000u && size <= 0xc0000000u - address;
}

static inline void z72_style_copy(void *destination, const void *source, size_t size)
{
    volatile uint8_t *out = (volatile uint8_t *)destination;
    const volatile uint8_t *in = (const volatile uint8_t *)source;
    if (!(((uintptr_t)out | (uintptr_t)in) & 3u)) {
        typedef uint32_t __attribute__((may_alias)) copy_word;
        volatile copy_word *to = (volatile copy_word *)out;
        const volatile copy_word *from = (const volatile copy_word *)in;
        size_t words = size / sizeof(copy_word);
        for (size_t i = 0; i < words; ++i)
            to[i] = from[i];
        out += words * sizeof(copy_word);
        in += words * sizeof(copy_word);
        size %= sizeof(copy_word);
    }
    for (size_t i = 0; i < size; ++i)
        out[i] = in[i];
}

static inline int z72_style_valid(const struct z72_style_fixed *style)
{
    if (!style)
        return 0;
    for (unsigned channel = 0; channel < 3; ++channel)
        for (unsigned i = 0; i < 129; ++i)
            if (style->rgb[channel][i] > 4095u ||
                (i && style->rgb[channel][i] < style->rgb[channel][i - 1u]))
                return 0;
    return 1;
}

/* Small signed divide by31 with nearest/ties-away rounding. All other knot
 * intervals divide by32. Avoid a compiler division-runtime dependency on ARM.
 */
static inline int32_t z72_style_div31(int32_t value)
{
    uint32_t n = value < 0 ? (uint32_t)-value : (uint32_t)value;
    n += 15u;
    uint32_t quotient = 0;
    for (unsigned bit = 17; bit-- > 0;)
        if ((31u << bit) <= n) {
            n -= 31u << bit;
            quotient |= 1u << bit;
        }
    return value < 0 ? -(int32_t)quotient : (int32_t)quotient;
}

static inline uint16_t z72_style_rgb(const uint16_t curve[129], uint16_t value)
{
    unsigned index = value >> 5, fraction = value & 31u;
    if (value >= 4095u)
        return curve[128];
    int32_t difference = (int32_t)curve[index + 1u] - curve[index];
    int32_t product = difference * (int32_t)fraction;
    int32_t offset;
    if (index == 127u)
        offset = z72_style_div31(product);
    else
        offset = product < 0 ? -(int32_t)(((uint32_t)-product + 16u) >> 5) :
                               (int32_t)(((uint32_t)product + 16u) >> 5);
    return (uint16_t)((int32_t)curve[index] + offset);
}

static inline int32_t z72_style_delta(const struct z72_style_state *state,
                                      const struct z72_style_fixed *style,
                                      unsigned slot, unsigned node)
{
    int32_t delta = style->cpxs_delta[slot >> 1][node];
#ifdef Z72_STYLE_LIMIT_PAIRED_DELTA
    /* Use the common safe interval so both bank updates stay equal. */
    int32_t a = state->cpxs[slot & ~1u][node];
    int32_t b = state->cpxs[slot | 1u][node];
    if (a < -512 || a > 511 || b < -512 || b > 511)
        return 0; /* The existing range check rejects this original pair. */
    int32_t low = -512 - (a < b ? a : b), high = 511 - (a > b ? a : b);
    return delta < low ? low : delta > high ? high : delta;
#else
    (void)state;
    return delta;
#endif
}

/* Prepare without changing authoritative state. Return -1 for invalid native
 * pointers,0 to forward the original root,1 to publish transaction.root.
 * Every nonnegative result requires commit ONLY after the original uploader
 * returned success. Its preflight failure must leave authoritative state alone.
 * No external payload pointer survives this synchronous transaction.
 */
static inline int z72_style_prepare(const struct z72_style_state *state,
                                     struct z72_style_transaction *transaction,
                                     const uint32_t *root,
                                     const struct z72_style_fixed *style,
                                     unsigned enabled, enum z72_style_chroma_mode chroma_mode)
{
    if (!state || !transaction || !z72_style_ram((uintptr_t)root, 53u * 4u))
        return -1;
    struct z72_style_state *next = &transaction->next;
    const unsigned clear_cpxs = chroma_mode == Z72_STYLE_CHROMA_LUMA_ONLY ? 8u :
                                chroma_mode == Z72_STYLE_CHROMA_NONE ? 12u : 0u;
    z72_style_copy(next, state, sizeof(*next));
    z72_style_copy(transaction->root, root, sizeof(transaction->root));
    transaction->applied = transaction->restored = transaction->deferred = 0;
    uint32_t matrix = root[21], rgb_table = root[25], cpxs_table = root[26];
    if (matrix) {
        if (!z72_style_ram(matrix, 176))
            return -1;
        z72_style_copy(next->matrix, (const void *)(uintptr_t)matrix, 176);
        next->matrix_valid = 1;
    }
    uint32_t incoming_rgb = 0;
    if (rgb_table) {
        if (!z72_style_ram(rgb_table, 6u * 4u))
            return -1;
        const volatile uint32_t *pointers = (const volatile uint32_t *)(uintptr_t)rgb_table;
        for (unsigned i = 0; i < 6; ++i) {
            uint32_t pointer = pointers[i];
            if (pointer) {
                if (!z72_style_ram(pointer, Z72_STYLE_RGB_BYTES))
                    return -1;
                z72_style_copy(next->rgb[i], (const void *)(uintptr_t)pointer,
                               Z72_STYLE_RGB_BYTES);
                incoming_rgb |= 1u << i;
            }
        }
    }
    next->rgb_valid |= incoming_rgb;
    if (incoming_rgb == Z72_STYLE_RGB_MASK) {
        next->rgb_mode = 2;
        for (unsigned i = 0; i < 6; ++i)
            next->rgb_kind[i] = (i & 1u) ? 3u : 2u;
    } else if ((incoming_rgb == 0x15u || incoming_rgb == 0x2au) &&
             next->matrix_valid && next->matrix[0x3a] <= 1u &&
             incoming_rgb == (next->matrix[0x3a] ? 0x2au : 0x15u)) {
        next->rgb_mode = 1;
        for (unsigned i = 0; i < 6; ++i)
            if (incoming_rgb & (1u << i))
                next->rgb_kind[i] = 1;
    } else if (incoming_rgb) {
        next->rgb_mode = 0;
        for (unsigned i = 0; i < 6; ++i)
            if (incoming_rgb & (1u << i))
                next->rgb_kind[i] = 0;
    }
    for (unsigned i = 0; i < 22; ++i)
        transaction->cpxs_pointers[i] = 0;
    if (cpxs_table) {
        if (!z72_style_ram(cpxs_table, 22u * 4u))
            return -1;
        const volatile uint32_t *pointers = (const volatile uint32_t *)(uintptr_t)cpxs_table;
        z72_style_copy(transaction->cpxs_pointers, (const void *)(uintptr_t)cpxs_table,
                       sizeof(transaction->cpxs_pointers));
        for (unsigned i = 0; i < 12; ++i) {
            uint32_t pointer = pointers[i];
            if (pointer) {
                if (!z72_style_ram(pointer, Z72_STYLE_CPXS_BYTES))
                    return -1;
                z72_style_copy(next->cpxs[i], (const void *)(uintptr_t)pointer,
                               Z72_STYLE_CPXS_BYTES);
                next->cpxs_valid |= 1u << i;
            }
        }
    }
    uint32_t rgb_mask = next->rgb_mode == 2 ? Z72_STYLE_RGB_MASK :
        next->rgb_mode == 1 && next->matrix_valid && next->matrix[0x3a] <= 1u ?
        (next->matrix[0x3a] ? 0x2au : 0x15u) : 0;
    int ready = enabled && z72_style_valid(style) && rgb_mask &&
        (next->rgb_valid & rgb_mask) == rgb_mask &&
        next->cpxs_valid == Z72_STYLE_CPXS_MASK;
    if (ready) {
        for (unsigned i = 0; i < 6; ++i)
            if (rgb_mask & (1u << i)) {
                unsigned kind = next->rgb_mode == 1 ? 1u : (i & 1u) ? 3u : 2u;
                if (next->rgb_kind[i] != kind)
                    ready = 0;
                unsigned count = next->rgb_mode == 1 ? 65u : (i & 1u) ? 63u : 66u;
                for (unsigned node = 0; node < count; ++node)
                    if (next->rgb[i][node] > 4095u)
                        ready = 0;
            }
        for (unsigned i = clear_cpxs; i < 12; ++i)
            for (unsigned node = 0; node < 32; ++node) {
                int32_t value = (int32_t)next->cpxs[i][node] + z72_style_delta(next, style, i, node);
                /* Restore/defer outside the supported coefficient range. */
                if (value < -512 || value > 511)
                    ready = 0;
            }
    }
    if (ready) {
        for (unsigned i = 0; i < 6; ++i) {
            transaction->rgb_pointers[i] = 0;
            if (rgb_mask & (1u << i)) {
                z72_style_copy(transaction->rgb[i], next->rgb[i], Z72_STYLE_RGB_BYTES);
                unsigned count = next->rgb_mode == 1 ? 65u : (i & 1u) ? 63u : 66u;
                for (unsigned node = 0; node < count; ++node)
                    transaction->rgb[i][node] = z72_style_rgb(style->rgb[i >> 1], next->rgb[i][node]);
                transaction->rgb_pointers[i] = (uint32_t)(uintptr_t)transaction->rgb[i];
            }
        }
        /* Master desaturation feeds zero chroma into the mixer. Desaturation
         * in all eight sectors still allows chroma to contribute to luma. */
        for (unsigned i = 0; i < 12; ++i) {
            for (unsigned node = 0; node < 32; ++node)
                transaction->cpxs[i][node] = i < clear_cpxs ? 0 : (int16_t)(
                    (int32_t)next->cpxs[i][node] + z72_style_delta(next, style, i, node));
            transaction->cpxs_pointers[i] = (uint32_t)(uintptr_t)transaction->cpxs[i];
        }
        transaction->root[25] = (uint32_t)(uintptr_t)transaction->rgb_pointers;
        transaction->root[26] = (uint32_t)(uintptr_t)transaction->cpxs_pointers;
        next->rgb_modified |= rgb_mask;
        next->cpxs_modified = Z72_STYLE_CPXS_MASK;
        transaction->applied = 1;
        return 1;
    }
    transaction->deferred = !!enabled;
    /* On OFF or unsupported input, explicitly restore every owned entry, even
     * when the original native request omits both pointer-table families.
     */
    if (next->rgb_modified) {
        if (rgb_table)
            z72_style_copy(transaction->rgb_pointers, (const void *)(uintptr_t)rgb_table,
                           sizeof(transaction->rgb_pointers));
        else
            for (unsigned i = 0; i < 6; ++i)
                transaction->rgb_pointers[i] = 0;
        for (unsigned i = 0; i < 6; ++i)
            if (next->rgb_modified & (1u << i))
                transaction->rgb_pointers[i] = (uint32_t)(uintptr_t)next->rgb[i];
        transaction->root[25] = (uint32_t)(uintptr_t)transaction->rgb_pointers;
    }
    if (next->cpxs_modified) {
        for (unsigned i = 0; i < 12; ++i)
            if (next->cpxs_modified & (1u << i))
                transaction->cpxs_pointers[i] = (uint32_t)(uintptr_t)next->cpxs[i];
        transaction->root[26] = (uint32_t)(uintptr_t)transaction->cpxs_pointers;
    }
    transaction->restored = !!(next->rgb_modified | next->cpxs_modified);
    next->rgb_modified = next->cpxs_modified = 0;
    return transaction->restored ? 1 : 0;
}

static inline void z72_style_commit(struct z72_style_state *state,
                                     const struct z72_style_transaction *transaction)
{
    z72_style_copy(state, &transaction->next, sizeof(*state));
}

#endif
