#ifndef TBMP_DECODE_H
#define TBMP_DECODE_H

#include "tbmp_pixel.h"
#include "tbmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * tbmp_decode - fully decode an image to a flat RGBA8888 pixel array.
 *
 * img   : parsed image from tbmp_open() (non-NULL).
 * frame : output frame (non-NULL).
 *         frame->pixels must point to a caller-allocated buffer of at
 *         least img->head.width * img->head.height TBmpRGBA elements.
 *         frame->width and frame->height are set by this function.
 *
 * Returns TBMP_OK on success, negative TBmpError on failure.
 *
 * Important: The output is always top-down, left-to-right, RGBA8888.
 */
TBmpError tbmp_decode(const TBmpImage *img, TBmpFrame *frame);

/*
 * tbmp_palette_lookup - safe palette index lookup.
 *
 * Returns {0,0,0,0} for out-of-range indices (never crashes).
 */
TBmpRGBA tbmp_palette_lookup(const TBmpPalette *pal, uint32_t idx);

/*
 * tbmp_pixel_to_rgba - convert a packed pixel value for the given pixel
 * format (and optional masks/palette) to RGBA8888.
 *
 * pixel_val : the raw bit-packed pixel value.
 * fmt       : the TBmpPixelFormat.
 * pal       : palette pointer (may be NULL for non-indexed formats).
 * masks     : masks pointer (may be NULL for non-CUSTOM formats).
 * Returns RGBA8888 colour.
 */
TBmpRGBA tbmp_pixel_to_rgba(uint32_t pixel_val, TBmpPixelFormat fmt,
                            const TBmpPalette *pal, const TBmpMasks *masks);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_DECODE_H */
