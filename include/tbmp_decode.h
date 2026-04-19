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
 *
 * Thread safety: This function is thread-safe as long as each thread uses separate img and frame instances.
 * Ownership: The caller retains ownership of all buffers and structs.
 */
TBmpError tbmp_decode(const TBmpImage *img, TBmpFrame *frame);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_DECODE_H */
