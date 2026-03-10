#include "tbmp_rotate.h"

#include <stdint.h>
#include <string.h>

/* Floating-point paths (arbitrary angle + output-dims calculation). */

#if TBMP_HAS_FLOAT

#include <math.h> /* cos, sin, fabs, floor */

/* Internal helpers. */

/* clampd: available for future use; suppress unused warning. */
static double clampd(double v, double lo, double hi) __attribute__((unused));
static double clampd(double v, double lo, double hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/* Linear interpolation of two uint8 values with weight t in [0,1]. */
static uint8_t lerp_u8(uint8_t a, uint8_t b, double t) {
    return (uint8_t)((double)a + t * ((double)b - (double)a) + 0.5);
}

/* Sample src with nearest-neighbour; returns bg if out of bounds. */
static TBmpRGBA sample_nearest(const TBmpFrame *src, double fx, double fy,
                               TBmpRGBA bg) {
    int ix = (int)floor(fx + 0.5);
    int iy = (int)floor(fy + 0.5);
    if (ix < 0 || iy < 0 || ix >= (int)src->width || iy >= (int)src->height)
        return bg;
    return src->pixels[(uint32_t)iy * src->width + (uint32_t)ix];
}

/* Sample src with bilinear interpolation; returns bg if out of bounds. */
static TBmpRGBA sample_bilinear(const TBmpFrame *src, double fx, double fy,
                                TBmpRGBA bg) {
    int x0 = (int)floor(fx);
    int y0 = (int)floor(fy);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    double tx = fx - (double)x0;
    double ty = fy - (double)y0;

#define GET(xi, yi)                                                            \
    (((xi) >= 0 && (yi) >= 0 && (xi) < (int)(src)->width &&                    \
      (yi) < (int)(src)->height)                                               \
         ? (src)->pixels[(uint32_t)(yi) * (src)->width + (uint32_t)(xi)]       \
         : bg)

    TBmpRGBA c00 = GET(x0, y0);
    TBmpRGBA c10 = GET(x1, y0);
    TBmpRGBA c01 = GET(x0, y1);
    TBmpRGBA c11 = GET(x1, y1);
#undef GET

    TBmpRGBA out;
    out.r = lerp_u8(lerp_u8(c00.r, c10.r, tx), lerp_u8(c01.r, c11.r, tx), ty);
    out.g = lerp_u8(lerp_u8(c00.g, c10.g, tx), lerp_u8(c01.g, c11.g, tx), ty);
    out.b = lerp_u8(lerp_u8(c00.b, c10.b, tx), lerp_u8(c01.b, c11.b, tx), ty);
    out.a = lerp_u8(lerp_u8(c00.a, c10.a, tx), lerp_u8(c01.a, c11.a, tx), ty);
    return out;
}

/* Public API - arbitrary angle. */

void tbmp_rotate_output_dims(uint16_t src_w, uint16_t src_h, double angle_rad,
                             uint16_t *out_w, uint16_t *out_h) {
    double cosA = fabs(cos(angle_rad));
    double sinA = fabs(sin(angle_rad));

    double new_w = (double)src_w * cosA + (double)src_h * sinA;
    double new_h = (double)src_w * sinA + (double)src_h * cosA;

    uint32_t w = (uint32_t)(new_w + 0.9999);
    uint32_t h = (uint32_t)(new_h + 0.9999);
    if (w > 0xFFFFU)
        w = 0xFFFFU;
    if (h > 0xFFFFU)
        h = 0xFFFFU;

    *out_w = (uint16_t)w;
    *out_h = (uint16_t)h;
}

TBmpError tbmp_rotate(const TBmpFrame *src, TBmpFrame *dst, double angle_rad,
                      TBmpRGBA bg, TBmpRotateFilter filter) {
    if (src == NULL || dst == NULL)
        return TBMP_ERR_NULL_PTR;
    if (src->pixels == NULL || dst->pixels == NULL)
        return TBMP_ERR_NULL_PTR;
    if (src->width == 0 || src->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;

    uint16_t dw, dh;
    tbmp_rotate_output_dims(src->width, src->height, angle_rad, &dw, &dh);

    dst->width = dw;
    dst->height = dh;

    double cosA = cos(angle_rad);
    double sinA = sin(angle_rad);

    double src_cx = (double)(src->width - 1) * 0.5;
    double src_cy = (double)(src->height - 1) * 0.5;
    double dst_cx = (double)(dw - 1) * 0.5;
    double dst_cy = (double)(dh - 1) * 0.5;

    /* Inverse rotation: for each dst pixel compute the corresponding src
   * coordinate by rotating backwards (by -angle_rad). */
    for (uint16_t dy = 0; dy < dh; dy++) {
        double ty = (double)dy - dst_cy;

        for (uint16_t dx = 0; dx < dw; dx++) {
            double tx = (double)dx - dst_cx;

            double sx = cosA * tx + sinA * ty + src_cx;
            double sy = -sinA * tx + cosA * ty + src_cy;

            TBmpRGBA colour;
            if (filter == TBMP_ROTATE_BILINEAR) {
                colour = sample_bilinear(src, sx, sy, bg);
            } else {
                colour = sample_nearest(src, sx, sy, bg);
            }

            dst->pixels[(uint32_t)dy * dw + dx] = colour;
        }
    }

    return TBMP_OK;
}

#endif /* TBMP_HAS_FLOAT */

/* Public API - exact 90/180/270-degree lossless rotations.
 * Always available, no floating-point. */

TBmpError tbmp_rotate90(const TBmpFrame *src, TBmpFrame *dst) {
    if (src == NULL || dst == NULL)
        return TBMP_ERR_NULL_PTR;
    if (src->pixels == NULL || dst->pixels == NULL)
        return TBMP_ERR_NULL_PTR;
    if (src->width == 0 || src->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;

    dst->width = src->height;
    dst->height = src->width;

    /* CW 90 deg: dst[y][x] = src[src_h - 1 - x][y] */
    uint16_t sw = src->width;
    uint16_t sh = src->height;
    for (uint16_t sy = 0; sy < sh; sy++) {
        for (uint16_t sx = 0; sx < sw; sx++) {
            uint32_t src_idx = (uint32_t)sy * sw + sx;
            uint32_t dst_idx = (uint32_t)sx * sh + (sh - 1U - sy);
            dst->pixels[dst_idx] = src->pixels[src_idx];
        }
    }
    return TBMP_OK;
}

TBmpError tbmp_rotate180(const TBmpFrame *src, TBmpFrame *dst) {
    if (src == NULL || dst == NULL)
        return TBMP_ERR_NULL_PTR;
    if (src->pixels == NULL || dst->pixels == NULL)
        return TBMP_ERR_NULL_PTR;
    if (src->width == 0 || src->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;

    dst->width = src->width;
    dst->height = src->height;

    uint32_t n = (uint32_t)src->width * src->height;
    for (uint32_t i = 0; i < n; i++) {
        dst->pixels[i] = src->pixels[n - 1U - i];
    }
    return TBMP_OK;
}

TBmpError tbmp_rotate270(const TBmpFrame *src, TBmpFrame *dst) {
    if (src == NULL || dst == NULL)
        return TBMP_ERR_NULL_PTR;
    if (src->pixels == NULL || dst->pixels == NULL)
        return TBMP_ERR_NULL_PTR;
    if (src->width == 0 || src->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;

    dst->width = src->height;
    dst->height = src->width;

    /* CCW 90 deg (= CW 270 deg): dst[y][x] = src[x][sw - 1 - y] */
    uint16_t sw = src->width;
    uint16_t sh = src->height;
    for (uint16_t sy = 0; sy < sh; sy++) {
        for (uint16_t sx = 0; sx < sw; sx++) {
            uint32_t src_idx = (uint32_t)sy * sw + sx;
            uint32_t dst_idx = (uint32_t)(sw - 1U - sx) * sh + sy;
            dst->pixels[dst_idx] = src->pixels[src_idx];
        }
    }
    return TBMP_OK;
}
