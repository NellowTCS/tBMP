/* tBMP rotation API.
 *
 * Rotates a decoded RGBA8888 TBmpFrame by an arbitrary angle.
 * Two quality modes are provided:
 *   Nearest-neighbour : fast, O(1) scratch, suitable for ESP32.
 *   Bilinear          : smoother, still O(1) scratch, slightly slower.
 *
 * The output frame dimensions are expanded to fit the entire rotated image
 * (no cropping).  The caller must provide an output buffer of at least
 * tbmp_rotate_output_size() TBmpRGBA elements.
 *
 * When TBMP_NO_FLOAT is defined (bare-metal / no-FPU targets):
 *   tbmp_rotate()             is a stub returning TBMP_ERR_NOT_SUPPORTED.
 *   tbmp_rotate_output_dims() is a stub that copies src dimensions to out.
 *   tbmp_rotate90/180/270()   are always available (integer only).
 *
 */

#ifndef TBMP_ROTATE_H
#define TBMP_ROTATE_H

#include "tbmp_port.h" /* TBMP_HAS_FLOAT, TBMP_ERR_NOT_SUPPORTED */
#include "tbmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Filter quality for rotation. */
typedef enum TBmpRotateFilter {
    TBMP_ROTATE_NEAREST = 0,  /* nearest-neighbour, fastest       */
    TBMP_ROTATE_BILINEAR = 1, /* bilinear interpolation, smoother */
} TBmpRotateFilter;

#if TBMP_HAS_FLOAT

/*
 * tbmp_rotate_output_dims - compute output dimensions for a given rotation.
 *
 * src_w, src_h : source image dimensions.
 * angle_rad    : rotation angle in radians (counter-clockwise).
 * out_w, out_h : receives the output dimensions.
 *
 * Not available when TBMP_NO_FLOAT is defined.
 */
void tbmp_rotate_output_dims(uint16_t src_w, uint16_t src_h, double angle_rad,
                             uint16_t *out_w, uint16_t *out_h);

/*
 * tbmp_rotate - rotate a TBmpFrame by angle_rad radians (counter-clockwise).
 *
 * src       : input frame (must be fully decoded).
 * dst       : output frame.  dst->pixels must point to a buffer of at
 *             least out_w * out_h TBmpRGBA elements (use
 *             tbmp_rotate_output_dims to determine the size).
 *             dst->width and dst->height are set by this function.
 * angle_rad : rotation angle in radians (counter-clockwise positive).
 * bg        : background colour to fill unmapped pixels.
 * filter    : interpolation quality.
 * Returns TBMP_OK on success, negative TBmpError on failure.
 *
 * Not available when TBMP_NO_FLOAT is defined.
 */
TBmpError tbmp_rotate(const TBmpFrame *src, TBmpFrame *dst, double angle_rad,
                      TBmpRGBA bg, TBmpRotateFilter filter);

#else /* !TBMP_HAS_FLOAT */

/* Stubs for no-FPU targets.  Always inline so no .o symbol is emitted. */
static inline void tbmp_rotate_output_dims(uint16_t src_w, uint16_t src_h,
                                           double angle_rad, uint16_t *out_w,
                                           uint16_t *out_h) {
    (void)angle_rad;
    if (out_w)
        *out_w = src_w;
    if (out_h)
        *out_h = src_h;
}

static inline TBmpError tbmp_rotate(const TBmpFrame *src, TBmpFrame *dst,
                                    double angle_rad, TBmpRGBA bg,
                                    TBmpRotateFilter filter) {
    (void)src;
    (void)dst;
    (void)angle_rad;
    (void)bg;
    (void)filter;
    return (TBmpError)TBMP_ERR_NOT_SUPPORTED;
}

#endif /* TBMP_HAS_FLOAT */

/*
 * tbmp_rotate90  - lossless 90-degree clockwise rotation.
 * tbmp_rotate180 - lossless 180-degree rotation.
 * tbmp_rotate270 - lossless 270-degree clockwise rotation.
 *
 * These avoid floating-point entirely and use exact pixel mappings.
 * Output dimensions: 90/270 swap width/height; 180 keeps them equal.
 */
TBmpError tbmp_rotate90(const TBmpFrame *src, TBmpFrame *dst);
TBmpError tbmp_rotate180(const TBmpFrame *src, TBmpFrame *dst);
TBmpError tbmp_rotate270(const TBmpFrame *src, TBmpFrame *dst);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_ROTATE_H */
