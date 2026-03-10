/* tBMP file parsing and validation API.
 *
 * All parsing is zero-allocation: the caller provides a contiguous buffer
 * holding the entire file and tbmp_open() returns a TBmpImage whose pointer
 * fields point directly into that buffer.  The buffer must remain live for
 * the lifetime of the TBmpImage.
 *
 */

#ifndef TBMP_READER_H
#define TBMP_READER_H

#include "tbmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * tbmp_open - parse and validate a tBMP buffer.
 *
 * buf     : pointer to the start of the file data (non-NULL).
 * buf_len : total byte length of the buffer.
 * out     : caller-allocated TBmpImage to fill (non-NULL).
 *
 * Returns TBMP_OK on success, negative TBmpError on failure.
 *
 * On success *out is fully populated:
 *   out->head    contains the decoded header.
 *   out->data    points into buf at the DATA section.
 *   out->extra   points into buf at the EXTRA section (NULL if absent).
 *   out->meta    points into buf at the META section  (NULL if absent).
 *   out->palette and out->masks are populated from EXTRA sub-chunks.
 *
 * On failure *out is zeroed.
 */
TBmpError tbmp_open(const uint8_t *buf, size_t buf_len, TBmpImage *out);

/*
 * tbmp_validate_head - validate only the header portion.
 *
 * Useful for streaming scenarios where the header is checked before reading
 * the rest of the file.
 *
 * head : pointer to a TBmpHead already populated from the wire format.
 * Returns TBMP_OK on success, negative TBmpError on failure.
 */
TBmpError tbmp_validate_head(const TBmpHead *head);

/* tbmp_read_u16le - read a little-endian uint16 from an unaligned pointer.
 * Safe on all architectures including Cortex-M0. */
static inline uint16_t tbmp_read_u16le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* tbmp_read_u32le - read a little-endian uint32 from an unaligned pointer. */
static inline uint32_t tbmp_read_u32le(const uint8_t *p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* tbmp_error_str - return a human-readable string for a TBmpError code. */
const char *tbmp_error_str(TBmpError err);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_READER_H */
