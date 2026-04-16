#ifndef TBMP_WRITE_H
#define TBMP_WRITE_H

#include "tbmp_meta.h"
#include "tbmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* tbmp_write_params - configuration for the encoder. */
typedef struct TBmpWriteParams {
    TBmpEncoding encoding;        /* which encoding mode to use        */
    TBmpPixelFormat pixel_format; /* output pixel format               */
    uint8_t bit_depth;            /* output bit depth (must match fmt) */

    /* Optional palette - required for INDEX_* formats.  The encoder will
   * use palette->count entries and emit a PALT EXTRA chunk. */
    const TBmpPalette *palette; /* NULL means no palette */

    /* Optional masks - required for CUSTOM format.  Emits a MASK chunk. */
    const TBmpMasks *masks; /* NULL means no masks */

    /* Optional structured META section. If non-NULL, the encoder writes the
   * strict metadata schema (title/author/description/created/software/license,
   * tags, optional dpi/colorspace/custom). */
    const TBmpMeta *meta; /* NULL means no META section */

    uint16_t version; /* TBMP_VERSION_1_0 */
} TBmpWriteParams;

/*
 * tbmp_write_default_params - fill p with safe defaults:
 *   encoding     = TBMP_ENC_RAW
 *   pixel_format = TBMP_FMT_RGBA_8888
 *   bit_depth    = 32
 *   version      = TBMP_VERSION_1_0
 *   palette      = NULL
 *   masks        = NULL
 *   meta         = NULL
 */
void tbmp_write_default_params(TBmpWriteParams *p);

/*
 * tbmp_write_needed_size - return the minimum output buffer size needed to
 * encode a frame with the given parameters.
 *
 * Returns byte count, or 0 on overflow / invalid parameters.
 */
size_t tbmp_write_needed_size(const TBmpFrame *frame,
                              const TBmpWriteParams *params);

/*
 * tbmp_write - encode a TBmpFrame to a tBMP byte stream.
 *
 * frame   : source frame (RGBA8888).
 * params  : encoding parameters.
 * out     : caller-supplied output buffer.
 * out_cap : byte capacity of out.
 * out_len : receives the number of bytes actually written.
 * Returns TBMP_OK on success, negative TBmpError on failure.
 */
TBmpError tbmp_write(const TBmpFrame *frame, const TBmpWriteParams *params,
                     uint8_t *out, size_t out_cap, size_t *out_len);

/*
 * tbmp_pick_best_encoding - pick a practical encoding for the given frame.
 *
 * This helper currently compares RAW and RLE output size estimates for the
 * selected pixel format/bit depth and returns the smaller option.
 */
TBmpEncoding tbmp_pick_best_encoding(const TBmpFrame *frame,
                                     const TBmpWriteParams *params);

/*
 * tbmp_auto_palette_from_frame - generate a palette from frame colors.
 *
 * frame      : source RGBA frame.
 * max_colors : target palette size (1..TBMP_MAX_PALETTE).
 * out        : receives generated palette.
 *
 * Returns TBMP_OK on success, negative error code on invalid arguments.
 */
TBmpError tbmp_auto_palette_from_frame(const TBmpFrame *frame,
                                       uint32_t max_colors, TBmpPalette *out);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_WRITE_H */
