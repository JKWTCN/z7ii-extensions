/* SPDX-License-Identifier: MIT */
#ifndef Z72_FLEXIBLE_CLOUD_H
#define Z72_FLEXIBLE_CLOUD_H

#include "np3_flexible.h"
#include "flexible_cloud_basis.h"
#include "flexible_kaiji.h"
#include "flexible_style_tables.h"

/* Constant divisors compile to shifts or reciprocal multiplies. Start the
 * variable quotient at its highest possible bit, omitting leading zeroes. */
static inline __attribute__((always_inline))
int z72_cloud_round(int value,int divisor)
{
    const int half=divisor/2;
    const int biased=value+(value<0 ? -half : half);
    if(__builtin_constant_p(divisor))
        return biased/divisor;
    unsigned remainder=biased<0 ? (unsigned)-biased : (unsigned)biased;
    unsigned d=(unsigned)divisor;
    if(remainder<d)
        return 0;
    unsigned shift=__builtin_clz(d)-__builtin_clz(remainder);
    unsigned result=0;
    for(unsigned bit=1u<<shift,trial=d<<shift;bit;bit>>=1,trial>>=1) {
        if(remainder>=trial) {
            remainder-=trial;
            result|=bit;
        }
    }
    return biased<0 ? -(int)result : (int)result;
}

static inline int z72_cloud_weighted(int a,int b,int c,int x,int y,int z)
{
#if defined(__arm__) && defined(__ARM_FEATURE_DSP)
    /* Both pairs fit signed halfwords. Reuse the dual MAC used by pixel shift. */
    unsigned weights=(uint16_t)a|((unsigned)(uint16_t)b<<16);
    unsigned values=(uint16_t)x|((unsigned)(uint16_t)y<<16);
    int sum;
    __asm__("smlad %0, %1, %2, %3" : "=r"(sum) : "r"(weights),"r"(values),"r"(c*z) : "cc");
    return sum;
#else
    return a*x+b*y+c*z;
#endif
}

/* The derived responses describe offsets at 9 RGB and 8 periodic sector
 * knots. All coefficients below are built once from the selected NP3. */
static inline void z72_cloud_tint(int hue, int saturation, int strength, int tint[3])
{
    /* Periodic 30-degree fit of the Z7 II reference's three-channel response. */
    static const int16_t palette[12][3] = {
        {  578, -592, -508}, {  544, -117, -590}, {  547,  362, -420},
        { -240,  283, -516}, { -985,  487, -178}, {-1031,  670,  346},
        {-1015,  538,  642}, { -972,  133,  545}, { -806, -136,  534},
        {  -73, -441,  587}, {  462, -431,  362}, {  524, -528,  -57},
    };
    const unsigned phase = (unsigned)hue % 360u;
    const unsigned index = phase / 30u;
    const int fraction = (int)(phase % 30u);
    /* The early RGB curves amplify orange and yellow grading more than the
     * reference. Interpolate the correction on the same periodic hue knots. */
    static const uint8_t warm[12] = {32, 112, 128, 128, 80, 16, 0, 0, 0, 0, 0, 0};
    const int attenuation = warm[index] + z72_cloud_round(
        (warm[(index + 1u) % 12u] - warm[index]) * fraction, 30);
    strength = (strength * (128 - (48 * attenuation + 64) / 128) + 64) / 128;
    for (unsigned channel = 0; channel < 3; ++channel) {
        const int a = palette[index][channel], b = palette[(index + 1u) % 12u][channel];
        const int color = a + z72_cloud_round((b - a) * fraction, 30);
        tint[channel] = z72_cloud_round(color * saturation * strength, 100 * 128);
    }
}

static inline int z72_cloud_transition(int position, int start, int width)
{
    int offset = position - start;
    return offset <= 0 ? 0 : offset >= width ? 4096 :
           z72_cloud_round(offset * 4096, width);
}

static inline int z72_cloud_curve(const struct z72_np3_flexible *profile, int value)
{
    int position = value * 256;
    unsigned index = (unsigned)(position / 4095);
    int knot = profile->curve[index];
    if (index < 256)
        knot += (profile->curve[index + 1] - knot) * (position % 4095) / 4095;
    return (knot * 4095 + 16383) / 32767;
}

static inline void z72_cloud_build_curve(const struct z72_np3_flexible *profile,
                                         uint16_t curve[129])
{
    /* Neutral Z7 II ISP response, sampled at 256-code intervals and blended
     * with the identity to keep the clipped highlight shoulder invertible.
     * The NP3 curve acts on late luma. Approximate it at the earlier RGB
     * stage by mapping to that domain and back. Keep the authored range. */
    static const uint16_t luma[17] = {
        0, 122, 350, 706, 1140, 1606, 2063, 2476, 2821,
        3104, 3338, 3530, 3691, 3832, 3961, 4031, 4095,
    };
    const int black = z72_cloud_curve(profile, 0);
    const int white = z72_cloud_curve(profile, 4095);
    const int span = white - black;
    curve[0] = (uint16_t)black;
    curve[128] = (uint16_t)white;
    for (unsigned i = 1; i < 128; ++i) {
        const int direct = z72_cloud_curve(profile, (int)i * 32);
        int output = direct;
        if (span) {
            const unsigned left = i >> 3;
            const int rise = (luma[left + 1] - luma[left]) * (int)(i & 7u) * 32;
            const int input = luma[left] + (left == 15 ? (rise + 127) / 255 : (rise + 128) / 256);
            const int wanted = z72_cloud_round((z72_cloud_curve(profile, input) - black) * 4095, span);
            unsigned right = 1;
            while (luma[right] < wanted)
                ++right;
            const int inverse = (int)(right - 1) * 256 + z72_cloud_round(
                (wanted - luma[right - 1]) * (right == 16 ? 255 : 256), luma[right] - luma[right - 1]);
            const int mapped = black + (inverse * span + 2047) / 4095;
            /* A partial remap limits chroma changes when the RGB channels
             * differ. The same transfer is used for every custom curve. */
            const int delta = mapped - direct;
            output += (delta + (delta < 0 ? -2 : 2)) / 4;
        }
        curve[i] = (uint16_t)z72_kj_clip(output, curve[i - 1], white);
    }
}

static inline int z72_cloud_curve_deficit(const struct z72_np3_flexible *profile, int value)
{
    int linear = (profile->curve[0] * (4095 - value) +
                  profile->curve[256] * value + 16383) / 32767;
    return z72_kj_clip(linear - z72_cloud_curve(profile, value), 0, 512);
}

static inline void z72_cloud_compensate_curve(const struct z72_np3_flexible *profile,
                                              struct z72_style_fixed *style)
{
    int black = z72_cloud_curve(profile, 0);
    int white = z72_cloud_curve(profile, 4095);
    for (unsigned channel = 0; channel < 3; ++channel) {
        uint16_t *curve = style->rgb[channel];
        int black_delta = black - curve[0];
        int white_delta = white - curve[128];
        curve[0] = (uint16_t)black;
        curve[128] = (uint16_t)white;
        for (int i = 1; i < 128; ++i) {
            int value = curve[i];
            /* Compensate only portions below the authored endpoint line.
             * Ease the endpoint correction over the outer eight knots. */
            int lift = z72_cloud_round(24 * z72_cloud_curve_deficit(profile, value), 128);
            int edge = i < 8 ? black_delta * (8 - i) :
                       i > 120 ? white_delta * (i - 120) : 0;
            curve[i] = (uint16_t)z72_kj_clip(value + lift + z72_cloud_round(edge, 8),
                                            curve[i - 1], white);
        }
    }
}

static inline int z72_cloud_profile_supported(const struct z72_np3_flexible *profile)
{
    if (profile->base != Z72_NP3_FLEXIBLE_BASE || !z72_np3_values_valid(profile->values))
        return 0;
    if (profile->has_curve) {
        const unsigned char *header = profile->curve_header;
        /* The 257 samples already include the authored output range.
         * The interpolator assumes a full-range input domain. */
        if (header[2] != 0 || header[3] != 255 || header[4] >= header[5])
            return 0;
        for (unsigned i = 0; i < 257; ++i)
            if (profile->curve[i] > 32767 || (i && profile->curve[i] < profile->curve[i - 1]))
                return 0;
    }
    return 1;
}

static inline enum z72_style_chroma_mode z72_cloud_chroma_mode(const struct z72_np3_flexible *profile)
{
    const int *values = profile->values;
    if (values[5] == -100)
        return Z72_STYLE_CHROMA_NONE;
    if (values[31] || values[34] || values[37])
        return Z72_STYLE_CHROMA_NORMAL;
    for (unsigned i = 7; i < 30; i += 3)
        if (values[i] != -100)
            return Z72_STYLE_CHROMA_NORMAL;
    return Z72_STYLE_CHROMA_LUMA_ONLY;
}

/* Return nonzero before writing output when a custom curve cannot be
 * represented. The legacy sharpening and other common fields are native. */
static inline int z72_cloud_prepare(const struct z72_np3_flexible *profile,
                                    struct z72_style_fixed *style)
{
    if (!z72_cloud_profile_supported(profile))
        return 1;
    const int *values = profile->values;
    int amounts[30], offsets[75], mixer_rgb[27];
    const int mixer_mono = z72_cloud_chroma_mode(profile) == Z72_STYLE_CHROMA_LUMA_ONLY;
    for (unsigned i = 0; i < 30; ++i) {
        /* Full mixer desaturation is applied at the chroma output. Keep
         * the source chroma available for the per-hue lightness controls. */
        const int saturation = i == 5 || (i >= 7 && (i - 7) % 3 == 0);
        amounts[i] = mixer_mono && saturation ? 0 : values[i];
    }
    for (unsigned row = 0; row < 75; ++row) {
        int total = 0, basic = 0;
        for (unsigned i = 0; i < 30; ++i) {
            if (i == 6)
                basic = total;
            int amount = amounts[i];
            const int16_t *response = z72_cloud_basis[2u * i + (amount > 0)];
            total += (amount < 0 ? -amount : amount) * response[row];
        }
        offsets[row] = (total + (total < 0 ? -50 : 50)) / 100;
        if (row < 27) {
            const int mixer = total - basic;
            mixer_rgb[row] = (mixer + (mixer < 0 ? -50 : 50)) / 100;
        }
    }

    /* Reduce the mixer contribution perpendicular to the neutral axis in
     * the RGB midtones. The luma component and hue-sector tables still
     * carry the recipe's selective color and lightness controls. */
    static const uint8_t neutral_weight[9] = {0, 32, 96, 128, 128, 112, 64, 32, 0};
    for (unsigned i = 0; i < 9; ++i) {
        const int luma = z72_cloud_round(54 * mixer_rgb[i] + 183 * mixer_rgb[i + 9] +
                                      19 * mixer_rgb[i + 18], 256);
        for (unsigned channel = 0; channel < 3; ++channel) {
            const unsigned j = i + 9u * channel;
            offsets[j] += z72_cloud_round((luma - mixer_rgb[j]) * 48 * neutral_weight[i], 128 * 128);
        }
    }

    static const uint8_t tint_strength[3] = {98, 95, 118}; /* Q7 */
    int tint[3][3];
    for (unsigned region = 0; region < 3; ++region)
        z72_cloud_tint(values[30 + 3u * region], values[31 + 3u * region],
                       tint_strength[region], tint[region]);
    const int width = 224 + z72_cloud_round(280 * values[39], 100);
    const int balance = z72_cloud_round(384 * values[40], 100);
    const int shadow_light = z72_cloud_round(192 * values[32], 128);
    /* At low blend, start midtone tint later so it contributes less in
     * shadows. Keep the end of the ramp and the lightness weights fixed. */
    const int low_blend = values[39] < 50 ? 50 - values[39] : 0;
    const int tint_delay = z72_cloud_round(48 * low_blend, 50);
    uint16_t tone[129];
    if (profile->has_curve)
        z72_cloud_build_curve(profile, tone);
    for (unsigned i = 0; i <= 128; ++i) {
        int x = i == 128 ? 4095 : (int)i * 32;
        int shoulder = (x * (4095 - x) + 2048) / 4096;
        int position = z72_cloud_round(x * 995, 4096) + 78 + balance;
        /* Keep independent ramps so the bands can overlap even at blend 0. */
        int shadows = 4096 - z72_cloud_transition(position, 220, width);
        int midtone_falloff = z72_cloud_transition(position, 528, width);
        int midtones = z72_cloud_transition(position, 352 - width, width) - midtone_falloff;
        int midtone_tint = tint_delay ?
            z72_cloud_transition(position, 352 - width + tint_delay, width - tint_delay) -
            midtone_falloff : midtones;
        int highlights = z72_cloud_transition(position, 672 - width, width);
        unsigned index = (unsigned)(x * 8 / 4095);
        int fraction = x * 8 % 4095;
        int lightness = z72_cloud_round(z72_cloud_weighted(
            shadows, midtones, highlights, shadow_light, values[35], values[38]), 4096);
        int light_gain = z72_cloud_round((shoulder < 260 ? shoulder : 260) *
                                      (shoulder < 100 ? shoulder : 100), 100);
        int light_offset = z72_cloud_round(lightness * light_gain, 100);
        for (unsigned channel = 0; channel < 3; ++channel) {
            const int *knots = offsets + 9u * channel;
            int response = knots[index];
            if (index < 8)
                response += (knots[index + 1] - response) * fraction / 4095;
            int color = z72_cloud_round(z72_cloud_weighted(
                shadows, midtone_tint, highlights, tint[0][channel], tint[1][channel], tint[2][channel]), 4096);
            int output = x + response + z72_cloud_round(shoulder * color, 1024) + light_offset;
            output = z72_kj_clip(output, 0, 4095);
            if (profile->has_curve)
                output = z72_style_rgb(tone, (uint16_t)output);
            if (i && output < style->rgb[channel][i - 1])
                output = style->rgb[channel][i - 1];
            style->rgb[channel][i] = (uint16_t)output;
        }
    }
    int curve_depth = 0;
    if (profile->has_curve) {
        z72_cloud_compensate_curve(profile, style);
        int low = z72_cloud_curve_deficit(profile, 512);
        int mid = z72_cloud_curve_deficit(profile, 1024);
        curve_depth = low > mid ? low : mid;
    }
    /* The same bounded correction reaches both CPXS banks. The final two
     * rows feed chroma into luma and are kept out of the color correction. */
    static const int8_t curve_chroma[6] = {24, -16, 8, 12, 0, 0};
    for (unsigned row = 0; row < 6; ++row) {
        const int *knots = offsets + 27u + 8u * row;
        int correction = z72_cloud_round(curve_chroma[row] * curve_depth, 512);
        for (unsigned i = 0; i < 32; ++i) {
            int left = knots[i / 4], right = knots[(i / 4 + 1u) % 8u];
            int delta = left + (right - left) * (int)(i % 4u) / 4;
            style->cpxs_delta[row][i] = (int16_t)z72_kj_clip(delta + correction, -1023, 1023);
        }
    }
    return 0;
}

#endif
