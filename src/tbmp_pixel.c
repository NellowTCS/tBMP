#include "tbmp_pixel.h"

#include <stdlib.h>

/* Internal helpers (file-scope only). */

/*
 * ctz32 - count trailing zero bits (portable CTZ).
 * Returns 32 if mask == 0.
 */
static uint32_t ctz32(uint32_t mask) {
    if (mask == 0U)
        return 32U;
    uint32_t n = 0;
    while ((mask & 1U) == 0U) {
        mask >>= 1;
        n++;
    }
    return n;
}

/* popcount32 - count set bits (portable Hamming weight). */
static uint32_t popcount32(uint32_t v) {
    v = v - ((v >> 1) & 0x55555555U);
    v = (v & 0x33333333U) + ((v >> 2) & 0x33333333U);
    return (((v + (v >> 4)) & 0x0F0F0F0FU) * 0x01010101U) >> 24;
}

/* Public API. */

uint8_t tbmp_scale_channel(uint32_t val, uint32_t bits) {
    if (bits == 0U || bits >= 8U) {
        return (uint8_t)(val & 0xFFU);
    }
    uint32_t maxval = (1U << bits) - 1U;
    if (maxval == 0U)
        return 0;
    /* Round to nearest: (val * 255 + maxval/2) / maxval */
    return (uint8_t)(((val * 255U) + (maxval >> 1)) / maxval);
}

TBmpRGBA tbmp_palette_lookup(const TBmpPalette *pal, uint32_t idx) {
    TBmpRGBA black = {0, 0, 0, 0};
    if (pal == NULL || idx >= pal->count)
        return black;
    return pal->entries[idx];
}

TBmpRGBA tbmp_pixel_to_rgba(uint32_t v, TBmpPixelFormat fmt,
                            const TBmpPalette *pal, const TBmpMasks *masks) {
    TBmpRGBA c;

    switch (fmt) {

    /* Indexed (palette) formats. */
    case TBMP_FMT_INDEX_1:
    case TBMP_FMT_INDEX_2:
    case TBMP_FMT_INDEX_4:
    case TBMP_FMT_INDEX_8:
        return tbmp_palette_lookup(pal, v);

    /* RGB 565 - RRRRRGGGGGGBBBBB */
    case TBMP_FMT_RGB_565:
        c.r = tbmp_scale_channel((v >> 11) & 0x1FU, 5);
        c.g = tbmp_scale_channel((v >> 5) & 0x3FU, 6);
        c.b = tbmp_scale_channel(v & 0x1FU, 5);
        c.a = 255;
        return c;

    /* RGB 555 - xRRRRRGGGGGBBBBB */
    case TBMP_FMT_RGB_555:
        c.r = tbmp_scale_channel((v >> 10) & 0x1FU, 5);
        c.g = tbmp_scale_channel((v >> 5) & 0x1FU, 5);
        c.b = tbmp_scale_channel(v & 0x1FU, 5);
        c.a = 255;
        return c;

    /* RGB 444 - xxxxRRRRGGGGBBBB */
    case TBMP_FMT_RGB_444:
        c.r = tbmp_scale_channel((v >> 8) & 0x0FU, 4);
        c.g = tbmp_scale_channel((v >> 4) & 0x0FU, 4);
        c.b = tbmp_scale_channel(v & 0x0FU, 4);
        c.a = 255;
        return c;

    /* RGB 332 - RRRGGGBB */
    case TBMP_FMT_RGB_332:
        c.r = tbmp_scale_channel((v >> 5) & 0x07U, 3);
        c.g = tbmp_scale_channel((v >> 2) & 0x07U, 3);
        c.b = tbmp_scale_channel(v & 0x03U, 2);
        c.a = 255;
        return c;

    /* RGB 888 - wire bytes [R, G, B]; packed as R<<16|G<<8|B */
    case TBMP_FMT_RGB_888:
        c.r = (uint8_t)((v >> 16) & 0xFFU);
        c.g = (uint8_t)((v >> 8) & 0xFFU);
        c.b = (uint8_t)(v & 0xFFU);
        c.a = 255;
        return c;

    /* RGBA 8888 - wire bytes [R, G, B, A]; packed as R<<24|G<<16|B<<8|A */
    case TBMP_FMT_RGBA_8888:
        c.r = (uint8_t)((v >> 24) & 0xFFU);
        c.g = (uint8_t)((v >> 16) & 0xFFU);
        c.b = (uint8_t)((v >> 8) & 0xFFU);
        c.a = (uint8_t)(v & 0xFFU);
        return c;

    /* Custom masked format.
   * The masks define arbitrary bit-field positions for each channel. */
    case TBMP_FMT_CUSTOM: {
        TBmpRGBA black = {0, 0, 0, 255};
        if (masks == NULL)
            return black;

        uint32_t r_bits = popcount32(masks->r);
        uint32_t g_bits = popcount32(masks->g);
        uint32_t b_bits = popcount32(masks->b);
        uint32_t a_bits = popcount32(masks->a);

        uint32_t r_shift = ctz32(masks->r);
        uint32_t g_shift = ctz32(masks->g);
        uint32_t b_shift = ctz32(masks->b);
        uint32_t a_shift = ctz32(masks->a);

        c.r = (masks->r == 0U)
              ? 0U
              : tbmp_scale_channel((v & masks->r) >> r_shift, r_bits);
        c.g = (masks->g == 0U)
              ? 0U
              : tbmp_scale_channel((v & masks->g) >> g_shift, g_bits);
        c.b = (masks->b == 0U)
              ? 0U
              : tbmp_scale_channel((v & masks->b) >> b_shift, b_bits);
        c.a = (masks->a == 0U)
                  ? 255U
                  : tbmp_scale_channel((v & masks->a) >> a_shift, a_bits);
        return c;
    }

    default: {
        TBmpRGBA black = {0, 0, 0, 255};
        return black;
    }
    }
}

static uint8_t clamp_u8_int(int v) {
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return (uint8_t)v;
}

static TBmpRGBA nearest_palette_color(const TBmpPalette *pal, uint8_t r,
                                      uint8_t g, uint8_t b) {
    uint32_t best_idx = 0;
    uint32_t best_dist = UINT32_MAX;

    for (uint32_t i = 0; i < pal->count; i++) {
        int dr = (int)r - (int)pal->entries[i].r;
        int dg = (int)g - (int)pal->entries[i].g;
        int db = (int)b - (int)pal->entries[i].b;
        uint32_t dist = (uint32_t)(dr * dr + dg * dg + db * db);
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
            if (dist == 0U)
                break;
        }
    }

    return pal->entries[best_idx];
}

TBmpError tbmp_dither_to_palette(TBmpFrame *frame, const TBmpPalette *pal) {
    if (frame == NULL || pal == NULL || frame->pixels == NULL)
        return TBMP_ERR_NULL_PTR;
    if (frame->width == 0 || frame->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;
    if (pal->count == 0)
        return TBMP_ERR_BAD_PALETTE;

    size_t row_stride = ((size_t)frame->width + 2U) * 3U;
    int *err_cur = (int *)calloc(row_stride, sizeof(int));
    int *err_next = (int *)calloc(row_stride, sizeof(int));
    if (!err_cur || !err_next) {
        free(err_cur);
        free(err_next);
        return TBMP_ERR_OUT_OF_MEMORY;
    }

    for (uint16_t y = 0; y < frame->height; y++) {
        for (uint16_t x = 0; x < frame->width; x++) {
            size_t pi = (size_t)y * frame->width + x;
            size_t ei = ((size_t)x + 1U) * 3U;

            int r = (int)frame->pixels[pi].r + err_cur[ei + 0] / 16;
            int g = (int)frame->pixels[pi].g + err_cur[ei + 1] / 16;
            int b = (int)frame->pixels[pi].b + err_cur[ei + 2] / 16;

            uint8_t rr = clamp_u8_int(r);
            uint8_t gg = clamp_u8_int(g);
            uint8_t bb = clamp_u8_int(b);

            TBmpRGBA chosen = nearest_palette_color(pal, rr, gg, bb);

            int dr = (int)rr - (int)chosen.r;
            int dg = (int)gg - (int)chosen.g;
            int db = (int)bb - (int)chosen.b;

            frame->pixels[pi] = chosen;

            err_cur[ei + 3] += dr * 7;
            err_cur[ei + 4] += dg * 7;
            err_cur[ei + 5] += db * 7;

            err_next[ei - 3] += dr * 3;
            err_next[ei - 2] += dg * 3;
            err_next[ei - 1] += db * 3;

            err_next[ei + 0] += dr * 5;
            err_next[ei + 1] += dg * 5;
            err_next[ei + 2] += db * 5;

            err_next[ei + 3] += dr;
            err_next[ei + 4] += dg;
            err_next[ei + 5] += db;
        }

        int *tmp = err_cur;
        err_cur = err_next;
        err_next = tmp;
        for (size_t i = 0; i < row_stride; i++)
            err_next[i] = 0;
    }

    free(err_cur);
    free(err_next);
    return TBMP_OK;
}
