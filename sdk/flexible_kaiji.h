/* SPDX-License-Identifier: MIT */
#ifndef Z72_FLEXIBLE_KAIJI_H
#define Z72_FLEXIBLE_KAIJI_H

#include <stddef.h>
#include <stdint.h>

/* RGB branch of the Kaiji LUT sample evaluator, Rome2 2AEE40.
 * This consumes a trusted, already baked Kaiji body, NOT an NP3 file or its
 * 41 parameters. Baking those parameters is a separate operation.
 * Inputs use the full unsigned 16-bit range. Native RGB output uses 0..65280.
 * All body offsets below are bytes, little endian; no alignment is required.
 */
#define Z72_KAIJI_BODY_SIZE 0x34370u

static inline int32_t z72_kj_clip(int32_t x, int32_t lo, int32_t hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline int32_t z72_kj_wrap(int64_t x)
{
    uint32_t u = (uint32_t)x;
    return u <= INT32_MAX ? (int32_t)u : -1 - (int32_t)(UINT32_MAX - u);
}

static inline int32_t z72_kj_mul(int32_t a, int32_t b)
{
    return z72_kj_wrap((int64_t)a * b);
}

/* Restoring division avoids compiler runtime helpers on the SD target. */
static inline uint32_t z72_kj_udiv32(uint32_t n, uint32_t d)
{
    uint32_t q = 0, remainder = 0;
    unsigned i;
    for (i = 0; i < 32; ++i) {
        uint32_t carry = remainder >> 31;
        remainder = (remainder << 1) | (n >> 31);
        n <<= 1;
        q <<= 1;
        if (carry || remainder >= d) {
            remainder -= d;
            q |= 1;
        }
    }
    return q;
}

static inline int32_t z72_kj_div32(int32_t n, uint32_t d)
{
    uint32_t magnitude = n < 0 ? 0u - (uint32_t)n : (uint32_t)n;
    uint32_t q = z72_kj_udiv32(magnitude, d);
    return n < 0 ? z72_kj_wrap(-(int64_t)q) : (int32_t)q;
}

/* Only the final nonnegative RGB rational needs a 64-bit numerator.
 * Its quotient is at most65280 after the caller's range check. */
static inline uint32_t z72_kj_rgb_div(uint64_t n)
{
    uint32_t q = 0, remainder = 0;
    unsigned i;
    for (i = 0; i < 64; ++i) {
        remainder = (remainder << 1) | (uint32_t)(n >> 63);
        n <<= 1;
        q <<= 1;
        if (remainder >= 2570000u) {
            remainder -= 2570000u;
            q |= 1;
        }
    }
    return q;
}

static inline int32_t z72_kj_u16(const uint8_t *p, size_t off)
{
    return (int32_t)((uint32_t)p[off] | ((uint32_t)p[off + 1] << 8));
}

static inline int32_t z72_kj_s16(const uint8_t *p, size_t off)
{
    int32_t u = z72_kj_u16(p, off);
    return u < 32768 ? u : u - 65536;
}

static inline int32_t z72_kj_s32(const uint8_t *p, size_t off)
{
    uint32_t u = (uint32_t)z72_kj_u16(p, off)
               | ((uint32_t)z72_kj_u16(p, off + 2) << 16);
    return z72_kj_wrap(u);
}

/* Native arithmetic right shift, including negative operands. */
static inline int32_t z72_kj_sar(int32_t x, unsigned bits)
{
    uint32_t u = (uint32_t)x;
    return z72_kj_wrap(x >= 0 ? u >> bits : ~((~u) >> bits));
}

static inline int64_t z72_kj_sar64(int64_t x, unsigned bits)
{
    uint64_t u = (uint64_t)x;
    if (x >= 0) return (int64_t)(u >> bits);
    return -1 - (int64_t)((~u) >> bits);
}

/* Signed nearest rounding in the original integer pipeline, ties away. */
static inline int32_t z72_kj_round(int32_t x, int32_t divisor)
{
    int32_t half = (int32_t)((uint32_t)divisor >> 1);
    return z72_kj_div32(z72_kj_wrap((int64_t)x + (x >= 0 ? half : -half)),
                        (uint32_t)divisor);
}

static inline int32_t z72_kj_abs(int32_t x)
{
    return x < 0 ? -x : x;
}

/* Four chroma quadrants, eight line segments each. */
static const uint8_t z72_kj_sector[64] = {
    0,1, 1,2, 2,3, 3,4, 4,5, 5,6, 6,7, 7,16,
    8,9, 9,10, 10,11, 11,12, 12,13, 13,14, 14,15, 15,16,
    0,17, 17,18, 18,19, 19,20, 20,21, 21,22, 22,23, 23,24,
    8,25, 25,26, 26,27, 27,28, 28,29, 29,30, 30,31, 31,24
};

static inline unsigned z72_kj_sector_index(int32_t u, int32_t v,
                                          unsigned precision, int32_t *fraction)
{
    int32_t av = z72_kj_abs(v), total = av + z72_kj_abs(u);
    int32_t ratio = total ? (int32_t)z72_kj_udiv32((uint32_t)(total / 2 + av * (int32_t)(1u << precision)), (uint32_t)total) : 0;
    unsigned quadrant = v > 0 ? (unsigned)(u >= 0) + 2u : (unsigned)(u > 0);
    ratio = z72_kj_clip(ratio, 0, (int32_t)(1u << precision) - 1);
    *fraction = ratio & ((1 << (precision - 3)) - 1);
    return (unsigned)(ratio >> (precision - 3)) + 8u * quadrant;
}

static inline int32_t z72_kj_interp(const uint8_t *p, size_t off,
                                   unsigned sector, int32_t fraction,
                                   int32_t divisor, int32_t scale)
{
    int32_t a = z72_kj_s16(p, off + 2u * z72_kj_sector[2u * sector]);
    int32_t b = z72_kj_s16(p, off + 2u * z72_kj_sector[2u * sector + 1]);
    return scale * a + z72_kj_round(fraction * (b - a), divisor);
}

static inline int32_t z72_kj_curve(const uint8_t *p, size_t off,
                                  int32_t factor, int32_t value)
{
    int32_t index = z72_kj_clip(z72_kj_mul(factor, value), 0, 4095);
    return z72_kj_u16(p, off + 2u * (unsigned)index);
}

static inline int32_t z72_kj_mix_gain(int32_t weight, int32_t curve)
{
    return z72_kj_clip(256 - weight + z72_kj_round(z72_kj_mul(weight, curve), 256), 0, 2047);
}

/* Exact rational form of native double RGB conversion and 65280 scaling.
 * Original coefficients are 1.4021, 1.7712, -0.3455, -0.7146.
 * Numerator is divisible by4096, while a half-step denominator2570000/2
 * is not divisible by16: no exact rounding ties occur for integer Y/U/V.
 */
static inline uint16_t z72_kj_rgb_channel(int32_t y, int32_t u, int32_t v,
                                        int32_t cu, int32_t cv)
{
    int64_t n = (int64_t)y * 10000 + (int64_t)u * cu + (int64_t)v * cv;
    if (n <= 0) return 0;
    if (n >= 40959375) return 65280;
    return (uint16_t)z72_kj_rgb_div((uint64_t)(n * 4096 + 1285000));
}

static inline int z72_kaiji_rgb16(const void *body, size_t body_size,
                                 const uint16_t in[3], uint16_t out[3])
{
    const uint8_t *p = (const uint8_t *)body;
    int32_t rgb[3], yuv[3], matrix[6], weight[10], factor[12];
    int32_t fraction, chroma, magnitude, gain_a, gain_b, gain_c, gain_d;
    int32_t hue, cosine, sine, delta, y, u, v, a, b, c, gain;
    unsigned sector, i, j;
    if (!p || body_size < Z72_KAIJI_BODY_SIZE || !in || !out) return 0;
    /* A generated body uses this bounded shift; reject malformed bodies. */
    if (z72_kj_u16(p, 197218) > 15) return 0;

    /* Input curves and RGB -> unsigned Y / signed chroma matrix. */
    for (i = 0; i < 3; ++i) {
        unsigned index = ((uint32_t)in[i] * 4095u + 32767u) / 65535u;
        rgb[i] = z72_kj_u16(p, 468u + 8192u * i + 2u * index);
        rgb[i] = z72_kj_clip(rgb[i], -z72_kj_s16(p, 76), 4095);
    }
    for (i = 0; i < 3; ++i) {
        a = 0;
        for (j = 0; j < 3; ++j) {
            b = rgb[j] * z72_kj_s16(p, 25044u + 6u * i + 2u * j);
            /* The first matrix uses a distinct negative tie rule. */
            a += z72_kj_sar(b - (b < 0) + 32, 6);
        }
        yuv[i] = z72_kj_clip(z72_kj_sar(a - (a < 0) + 4, 3), i ? -4096 : 0, 4095);
    }
    y = yuv[0]; u = yuv[1]; v = yuv[2];
    sector = z72_kj_sector_index(u, v, 9, &fraction);
    for (i = 0; i < 6; ++i)
        matrix[i] = z72_kj_clip(z72_kj_interp(p, 136134u + 64u * i, sector, fraction, 16, 4), -2048, 2047);
    for (i = 0; i < 8; ++i)
        weight[i] = z72_kj_clip(z72_kj_interp(p, 26026u + 64u * i, sector, fraction, 64, 1), -512, 511);
    weight[8] = z72_kj_clip(z72_kj_interp(p, 25896, sector, fraction, 64, 1), -512, 511);
    weight[9] = z72_kj_clip(z72_kj_interp(p, 26538, sector, fraction, 64, 1), -512, 511);
    a = z72_kj_clip(z72_kj_abs(u), 0, 4095);
    b = z72_kj_clip(z72_kj_abs(v), 0, 4095);
    magnitude = z72_kj_clip(z72_kj_round(2 * (a + b) + z72_kj_abs(b - a), 4), 0, 32767);
    for (i = 0; i < 12; ++i) factor[i] = z72_kj_s32(p, 25080u + 4u * i);

    /* Chroma, luminance and tone-dependent sector gains. */
    a = z72_kj_mix_gain(weight[0], z72_kj_curve(p, 27590, factor[0], magnitude));
    b = z72_kj_mix_gain(weight[1], z72_kj_curve(p, 35782, factor[1], magnitude));
    c = z72_kj_curve(p, 43974, factor[2], magnitude);
    gain_a = z72_kj_clip(z72_kj_round(z72_kj_mul(z72_kj_mul(c, a), b), 65536), 0, 2047);
    a = z72_kj_mix_gain(weight[2], z72_kj_curve(p, 52166, factor[3], magnitude));
    b = z72_kj_mix_gain(weight[3], z72_kj_curve(p, 60358, factor[4], magnitude));
    gain_b = z72_kj_clip(z72_kj_round(z72_kj_mul(a, b), 256), 0, 2047);
    a = z72_kj_mix_gain(weight[4], z72_kj_curve(p, 68550, factor[5], y));
    b = z72_kj_mix_gain(weight[5], z72_kj_curve(p, 76742, factor[6], y));
    gain_c = z72_kj_clip(z72_kj_round(z72_kj_mul(a, b), 256), 0, 2047);
    a = z72_kj_clip(z72_kj_round(z72_kj_mul(weight[6], z72_kj_curve(p, 84934, factor[7], magnitude)), 256), -512, 511);
    b = z72_kj_clip(z72_kj_round(z72_kj_mul(weight[7], z72_kj_curve(p, 93126, factor[8], magnitude)), 256), -512, 511);
    hue = z72_kj_clip(z72_kj_sar(a + b, 1), -256, 255) + 256;
    cosine = z72_kj_s16(p, 134086u + 2u * (unsigned)hue);
    sine = z72_kj_s16(p, 135110u + 2u * (unsigned)hue);
    a = z72_kj_curve(p, 101318, factor[9], magnitude);
    b = z72_kj_curve(p, 109510, factor[10], 4095 - y);
    a = z72_kj_clip(z72_kj_round(z72_kj_mul(a, b), 256), 0, 1023);
    b = z72_kj_clip(256 - a, 0, 1023);
    gain_d = z72_kj_clip(256 - weight[8] + z72_kj_round(z72_kj_mul(weight[8], b), 256), 0, 1023);
    delta = z72_kj_round(z72_kj_mul(weight[9], z72_kj_s16(p, 125894u + 2u * (unsigned)y)), 256);
    a = z72_kj_curve(p, 117702, factor[11], magnitude);
    delta = z72_kj_clip(delta, -a, a);
    a = delta * 256 + y * gain_b + (u * matrix[4] + v * matrix[5]) / 4;
    y = z72_kj_clip(z72_kj_round(a, 256), 0, 4095);
    gain = z72_kj_clip(z72_kj_sar(z72_kj_mul(z72_kj_mul(gain_d, gain_a), gain_c), 16), 0, 4095);
    a = z72_kj_clip(z72_kj_round(z72_kj_mul(gain, z72_kj_sar(u * matrix[0] + v * matrix[1], 8)), 1024), -4096, 4095);
    b = z72_kj_clip(z72_kj_round(z72_kj_mul(gain, z72_kj_sar(u * matrix[2] + v * matrix[3], 8)), 1024), -4096, 4095);
    u = z72_kj_clip(z72_kj_round(cosine * a - sine * b, 1024), -4096, 4095);
    v = z72_kj_clip(z72_kj_round(sine * a + cosine * b, 1024), -4096, 4095);

    /* Tone curve, grading target blend and second color transform. */
    gain = z72_kj_s32(p, 145240);
    u = z72_kj_clip(u + z72_kj_mul(gain, z72_kj_s16(p, 136524) - u) / 1024, -4096, 4095);
    v = z72_kj_clip(v + z72_kj_mul(gain, z72_kj_s16(p, 136522) - v) / 1024, -4096, 4095);
    y = z72_kj_u16(p, 137048u + 2u * (unsigned)y);
    for (i = 0; i < 3; ++i) {
        a = z72_kj_mul(y, z72_kj_s16(p, 145244u + 6u * i)) / 128;
        a += z72_kj_mul(u, z72_kj_s16(p, 145246u + 6u * i)) / 128;
        a += z72_kj_mul(v, z72_kj_s16(p, 145248u + 6u * i)) / 128;
        yuv[i] = z72_kj_clip(a / 8 + z72_kj_s16(p, 145262u + 2u * i), i ? -4096 : 0, 4095);
    }
    y = yuv[0]; u = yuv[1]; v = yuv[2];
    sector = z72_kj_sector_index(u, v, 7, &fraction);
    a = z72_kj_u16(p, 145268u + 2u * z72_kj_sector[2u * sector]);
    b = z72_kj_u16(p, 145268u + 2u * z72_kj_sector[2u * sector + 1]);
    chroma = z72_kj_clip(z72_kj_round(16 * a + fraction * (b - a), 16), 0, 4095);
    gain = z72_kj_u16(p, 145462u + 2u * (unsigned)y);
    u = z72_kj_clip(z72_kj_sar(u * gain, 10), -4096, 4095);
    v = z72_kj_clip(z72_kj_sar(v * gain, 10), -4096, 4095);
    a = z72_kj_clip(z72_kj_abs(u), 0, 4095);
    b = z72_kj_clip(z72_kj_abs(v), 0, 4095);
    magnitude = z72_kj_round(a + b + z72_kj_clip(a + b, 0, 4095) + z72_kj_clip(z72_kj_abs(a - b), 0, 4095), 4);
    magnitude = z72_kj_clip(magnitude, 0, 4095);
    a = z72_kj_clip(z72_kj_sar(y * 1024 + chroma * magnitude, 10), 0, 4095);
    gain = z72_kj_s32(p, 172100u + 4u * (unsigned)a);
    y = z72_kj_clip(z72_kj_wrap(z72_kj_sar64((int64_t)y * z72_kj_s32(p, 155716u + 4u * (unsigned)a), 20)), 0, 4095);
    u = z72_kj_clip(z72_kj_wrap(z72_kj_sar64((int64_t)gain * u, 20)), -4096, 4095);
    v = z72_kj_clip(z72_kj_wrap(z72_kj_sar64((int64_t)gain * v, 20)), -4096, 4095);
    y = z72_kj_u16(p, 188998u + 2u * (unsigned)y);

    /* Final matrix and highlight/shadow gamut compression. */
    for (i = 0; i < 3; ++i) {
        a = z72_kj_sar(y * z72_kj_s16(p, 197190u + 6u * i), 10);
        a += z72_kj_sar(u * z72_kj_s16(p, 197192u + 6u * i), 10);
        a += z72_kj_sar(v * z72_kj_s16(p, 197194u + 6u * i), 10);
        rgb[i] = z72_kj_clip(a + z72_kj_s16(p, 197208u + 2u * i), -8192, 8191);
    }
    a = rgb[0] > rgb[1] ? rgb[0] : rgb[1];
    a = a > rgb[2] ? a : rgb[2];
    a = a < 0 ? 0 : a;
    b = rgb[0] < rgb[1] ? rgb[0] : rgb[1];
    b = b < rgb[2] ? b : rgb[2];
    j = (unsigned)z72_kj_u16(p, 197218);
    a = z72_kj_clip(z72_kj_mul(a - z72_kj_u16(p, 197216), (int32_t)(1u << j)), 0, 4095);
    b = z72_kj_clip(z72_kj_mul(z72_kj_u16(p, 197214) - b, (int32_t)(1u << j)), 0, 4095);
    a = z72_kj_u16(p, 197484u + 2u * (unsigned)a);
    b = z72_kj_u16(p, 205676u + 2u * (unsigned)b);
    c = z72_kj_wrap((int64_t)z72_kj_mul(z72_kj_u16(p, 197220), y * (a - 2048) / 1024)
        + z72_kj_mul(z72_kj_u16(p, 197222), (2048 - b) * (4095 - y) / 1024));
    c = z72_kj_clip(z72_kj_sar(c, 8), -4096, 4095);
    y = z72_kj_clip(y + c, 0, 4095);
    gain = z72_kj_sar(z72_kj_mul(a, b), 11);
    u = z72_kj_clip(z72_kj_sar(z72_kj_mul(u, gain), 11), -2048, 2047);
    v = z72_kj_clip(z72_kj_sar(z72_kj_mul(v, gain), 11), -2048, 2047);
    out[0] = z72_kj_rgb_channel(y, u, v, 0, 14021);
    out[1] = z72_kj_rgb_channel(y, u, v, -3455, -7146);
    out[2] = z72_kj_rgb_channel(y, u, v, 17712, 0);
    return 1;
}

#endif
