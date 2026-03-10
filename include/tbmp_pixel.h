#ifndef TBMP_PIXEL_H
#define TBMP_PIXEL_H

#include "tbmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * tbmp_pixel_to_rgba - convert a packed pixel value to RGBA8888.
 *
 * The meaning of pixel_val depends on the format:
 *   INDEX_1/2/4/8 : palette index.
 *   RGB_565 / RGB_555 / RGB_444 / RGB_332 : packed bits as per format name.
 *   RGB_888   : big-endian packed: bits[23:16]=R, [15:8]=G, [7:0]=B.
 *   RGBA_8888 : big-endian packed: bits[31:24]=R, [23:16]=G,
 *               [15:8]=B, [7:0]=A.
 *   CUSTOM    : raw value; channels extracted using the provided masks.
 *
 * pixel_val : packed pixel value.
 * fmt       : TBmpPixelFormat.
 * pal       : palette (required for INDEX_* formats; may be NULL otherwise).
 * masks     : masks (required for CUSTOM format; may be NULL otherwise).
 * Returns RGBA8888 colour.  Returns {0,0,0,255} on invalid input rather
 * than crashing.
 */
TBmpRGBA tbmp_pixel_to_rgba(uint32_t pixel_val, TBmpPixelFormat fmt,
                            const TBmpPalette *pal, const TBmpMasks *masks);

/*
 * tbmp_palette_lookup - safe palette index lookup.
 *
 * Returns {0,0,0,0} if pal is NULL or idx is out of range.
 */
TBmpRGBA tbmp_palette_lookup(const TBmpPalette *pal, uint32_t idx);

/*
 * tbmp_scale_channel - scale an N-bit unsigned value to 8 bits by MSB
 * replication.
 *
 * Examples: scale_channel(31, 5) == 255, scale_channel(0, 5) == 0.
 *
 * This is part of the public API so that callers implementing custom
 * pixel-format converters can use the same scaling logic.
 */
uint8_t tbmp_scale_channel(uint32_t val, uint32_t bits);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_PIXEL_H */
