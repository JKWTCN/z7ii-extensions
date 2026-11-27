/* SPDX-License-Identifier: MIT */
#ifndef Z72_NP3_FLEXIBLE_H
#define Z72_NP3_FLEXIBLE_H

/* NP3 0310 parameter decoding only; this does not apply a picture control. */
#define Z72_NP3_FLEXIBLE_BASE 0x0020u
#define Z72_NP3_FLEXIBLE_VALUES 41u
#define Z72_NP3_COMMON_FIELDS 21u
#define Z72_NP3_CURVE_KNOTS 257u

enum z72_np3_flexible_status {
    Z72_NP3_OK = 0,
    Z72_NP3_ARGUMENT,
    Z72_NP3_SIGNATURE,
    Z72_NP3_TRUNCATED,
    Z72_NP3_DUPLICATE,
    Z72_NP3_FIELD,
    Z72_NP3_VERSION,
    Z72_NP3_BASE,
    Z72_NP3_MISSING
};

struct z72_np3_flexible {
    char name[21];
    unsigned short base;
    /* Raw two-byte fields for tags 0x0400 through 0x1800, in tag order.
     * Preserve enum/automatic markers and fractional sharpening precision.
     * Tag 0x0600 is sharpening; 0x1600 is mid-range sharpening. */
    unsigned char common[Z72_NP3_COMMON_FIELDS][2];
    unsigned int common_present;
    /* 0..5: contrast, highlight, shadow, white, black, saturation.
     * 6..29: mixer[red, orange, yellow, green, aqua, blue, purple, magenta]
     *         [hue, saturation, luminance].
     * 30..38: grading[shadow, midtones, highlight][hue, saturation, luminance].
     * 39: blending; 40: balance. Names do not specify rendering equations. */
    int values[Z72_NP3_FLEXIBLE_VALUES];
    unsigned int has_curve;
    unsigned char curve_header[64];
    unsigned short curve[Z72_NP3_CURVE_KNOTS];
};

static inline unsigned int z72_np3_be16(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | p[1];
}

static inline unsigned int z72_np3_be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | p[3];
}

/* Cortex-A9 soft-float modules cannot depend on __aeabi_idiv. Inputs here
 * have magnitude at most 32768; division truncates toward zero like NX. */
static inline int z72_np3_quotient(int value, unsigned int denominator)
{
    unsigned int magnitude = value < 0 ? (unsigned int)-value : (unsigned int)value;
    unsigned int quotient = 0, bit;
    for (bit = 32768u; bit; bit >>= 1) {
        if (denominator * bit <= magnitude) {
            magnitude -= denominator * bit;
            quotient |= bit;
        }
    }
    return value < 0 ? -(int)quotient : (int)quotient;
}

static inline int z72_np3_byte_value(unsigned int raw, unsigned int divisor,
                                     int fallback)
{
    return raw == 0u || raw == 1u || raw == 255u ? fallback :
           z72_np3_quotient((int)raw - 128, divisor);
}

static inline int z72_np3_values_valid(const int *values)
{
    unsigned int i;
    for (i = 0; i < 30; ++i)
        if (values[i] < -100 || values[i] > 100)
            return 0;
    for (i = 30; i < 39; i += 3)
        if (values[i] < 0 || values[i] > 360 ||
            values[i + 1] < 0 || values[i + 1] > 100 ||
            values[i + 2] < -100 || values[i + 2] > 100)
            return 0;
    return values[39] >= 0 && values[39] <= 100 &&
           values[40] >= -100 && values[40] <= 100;
}

/* All input bytes must be readable. Unknown TLVs are bounded and skipped.
 * Version, base and every Flexible Color extension field are mandatory;
 * common fields and the custom curve may be absent. The output is unchanged
 * on any error, including malformed trailing records. */
static inline enum z72_np3_flexible_status z72_np3_parse(
    const unsigned char *data, unsigned int size, struct z72_np3_flexible *output,
    unsigned int additional_base)
{
    struct z72_np3_flexible parsed;
    unsigned char *clear = (unsigned char *)&parsed;
    unsigned int seen = 0, offset = 4, i;
    if (!data || !output)
        return Z72_NP3_ARGUMENT;
    if (size < 4)
        return Z72_NP3_TRUNCATED;
    if (data[0] != 'N' || data[1] != 'C' || data[2] != 'P' || data[3])
        return Z72_NP3_SIGNATURE;
    for (i = 0; i < sizeof(parsed); ++i)
        clear[i] = 0;
    for (;;) {
        unsigned int tag, length, bit = 0;
        const unsigned char *p;
        if (size - offset < 4)
            return Z72_NP3_TRUNCATED;
        tag = z72_np3_be32(data + offset);
        offset += 4;
        if (!tag) {
            if (offset != size)
                return Z72_NP3_FIELD;
            break;
        }
        if (size - offset < 4)
            return Z72_NP3_TRUNCATED;
        length = z72_np3_be32(data + offset);
        offset += 4;
        if (length > size - offset)
            return Z72_NP3_TRUNCATED;
        p = data + offset;
        offset += length;
        if (tag >= 0x100u && tag <= 0x2000u && !(tag & 0xffu)) {
            bit = 1u << ((tag >> 8) - 1u);
            if (seen & bit)
                return Z72_NP3_DUPLICATE;
            seen |= bit;
        }
        if (tag == 0x100u) {
            if (length != 4u)
                return Z72_NP3_FIELD;
            if (p[0] != '0' || p[1] != '3' || p[2] != '1' || p[3] != '0')
                return Z72_NP3_VERSION;
        } else if (tag == 0x200u) {
            if (length != 20u)
                return Z72_NP3_FIELD;
            for (i = 0; i < 20; ++i)
                parsed.name[i] = (char)p[i];
        } else if (tag == 0x300u) {
            if (length != 2u)
                return Z72_NP3_FIELD;
            parsed.base = (unsigned short)z72_np3_be16(p);
            if (parsed.base != Z72_NP3_FLEXIBLE_BASE &&
                (!additional_base || parsed.base != additional_base))
                return Z72_NP3_BASE;
        } else if (bit && tag >= 0x400u && tag <= 0x1800u) {
            unsigned int index = (tag >> 8) - 4u;
            if (length != 2u)
                return Z72_NP3_FIELD;
            /* Filter-effect and toning enum fields legally have divisor 0. */
            if (tag != 0x400u && tag != 0xc00u && tag != 0xd00u && !p[1])
                return Z72_NP3_FIELD;
            parsed.common[index][0] = p[0];
            parsed.common[index][1] = p[1];
            parsed.common_present |= 1u << index;
        } else if (bit && tag >= 0x1900u && tag <= 0x1e00u) {
            if (length != 2u || !p[1])
                return Z72_NP3_FIELD;
            parsed.values[(tag >> 8) - 25u] = z72_np3_byte_value(p[0], p[1], 0);
        } else if (tag == 0x1f00u) {
            if (length != 28u || !p[24] || !p[25] || !p[26])
                return Z72_NP3_FIELD;
            for (i = 0; i < 24; ++i)
                parsed.values[6 + i] = z72_np3_byte_value(p[i], p[24 + i % 3u], 0);
        } else if (tag == 0x2000u) {
            if (length != 20u || !p[12] || !p[13] || !p[14] || !p[17] || !p[19])
                return Z72_NP3_FIELD;
            for (i = 0; i < 3; ++i) {
                unsigned int hue = z72_np3_be16(p + i * 4u);
                parsed.values[30 + i * 3] = hue == 0xffffu ? 0 :
                    z72_np3_quotient((int)hue - 32768, p[12]);
                parsed.values[31 + i * 3] = z72_np3_byte_value(p[i * 4 + 2], p[13], 0);
                parsed.values[32 + i * 3] = z72_np3_byte_value(p[i * 4 + 3], p[14], 0);
            }
            parsed.values[39] = z72_np3_byte_value(p[16], p[17], 50);
            parsed.values[40] = z72_np3_byte_value(p[18], p[19], 0);
        } else if (tag == 2u) {
            if (parsed.has_curve)
                return Z72_NP3_DUPLICATE;
            if (length != 578u || p[0] != 'I' || p[1] != '0' || p[8] > 20u)
                return Z72_NP3_FIELD;
            parsed.has_curve = 1;
            for (i = 0; i < 64; ++i)
                parsed.curve_header[i] = p[i];
            for (i = 0; i < Z72_NP3_CURVE_KNOTS; ++i)
                parsed.curve[i] = (unsigned short)z72_np3_be16(p + 64 + i * 2u);
        }
    }
    if ((seen & 0xff000005u) != 0xff000005u)
        return Z72_NP3_MISSING;
    if (!z72_np3_values_valid(parsed.values))
        return Z72_NP3_FIELD;
    for (i = 0; i < sizeof(parsed); ++i)
        ((unsigned char *)output)[i] = clear[i];
    return Z72_NP3_OK;
}

static inline enum z72_np3_flexible_status z72_np3_flexible_parse(
    const unsigned char *data, unsigned int size, struct z72_np3_flexible *output)
{
    return z72_np3_parse(data, size, output, 0);
}

#endif
