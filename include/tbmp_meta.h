#ifndef TBMP_META_H
#define TBMP_META_H

#include "tbmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Additional error/warning codes returned by metadata helpers. */

/* The root MessagePack object is not a map.
 * Returned as a negative int. */
#define TBMP_META_ERR_NOT_A_MAP (-20)

/* A map key was not a MessagePack string.
 * Returned as a negative int. */
#define TBMP_META_ERR_BAD_KEY (-21)

/* The blob is truncated / malformed (MPack flagged a reader error).
 * Returned as a negative int. */
#define TBMP_META_ERR_TRUNCATED (-22)

/* A required structured field is missing.
 * Returned as a negative int. */
#define TBMP_META_ERR_REQUIRED_MISSING (-23)

/* A metadata value has an unexpected type.
 * Returned as a negative int. */
#define TBMP_META_ERR_TYPE_MISMATCH (-24)

/* A string/array metadata value length is outside allowed bounds.
 * Returned as a negative int. */
#define TBMP_META_ERR_LENGTH_OUT_OF_RANGE (-25)

/* Structured metadata contains an unknown top-level key.
 * Returned as a negative int. */
#define TBMP_META_ERR_SCHEMA_UNKNOWN_KEY (-26)

/* Structured metadata repeats a top-level key.
 * Returned as a negative int. */
#define TBMP_META_ERR_SCHEMA_DUPLICATE_KEY (-27)

/*
 * tbmp_meta_parse - Parse strict structured metadata from MessagePack blob into TBmpMeta.
 *
 * blob   : pointer to the MessagePack blob (const, non-NULL, must remain valid for the call duration).
 * len    : length of the blob in bytes.
 * out    : caller-allocated TBmpMeta to fill (non-NULL).
 *
 * Returns TBMP_OK on success, TBMP_ERR_NULL_PTR for NULL pointers, TBMP_META_ERR_* for schema errors.
 *
 * Thread safety: This function is thread-safe as long as each thread uses separate TBmpMeta and blob instances.
 * Ownership: The caller retains ownership of the blob and output struct.
 */
int tbmp_meta_parse(const uint8_t *blob, size_t len, TBmpMeta *out);

/*
 * tbmp_meta_encode - Serialize a TBmpMeta to a raw MessagePack blob.
 *
 * meta    : source structured metadata (const, non-NULL).
 * out     : caller-supplied output buffer (non-NULL).
 * out_cap : capacity of out in bytes.
 * out_len : receives the number of bytes written on success (non-NULL).
 *
 * Returns TBMP_OK on success.
 * Returns TBMP_ERR_NULL_PTR for NULL pointers,
 * TBMP_ERR_OUT_OF_MEMORY if the buffer is too small,
 * or TBMP_META_ERR_* when schema requirements are not met.
 *
 * Thread safety: This function is thread-safe as long as each thread uses separate TBmpMeta and output buffer instances.
 * Ownership: The caller retains ownership of all buffers and structs.
 */
int tbmp_meta_encode(const TBmpMeta *meta, uint8_t *out, size_t out_cap,
                     size_t *out_len);

/*
 * tbmp_meta_validate_structured_blob - Validate strict structured metadata.
 *
 * blob   : pointer to the MessagePack blob (const, non-NULL).
 * len    : length of the blob in bytes.
 *
 * Required fields:
 *   title, author, description, created, software, license: string
 *   tags: array<string>
 * Optional fields:
 *   dpi: non-negative int/uint
 *   colorspace: string
 *   custom: array<map<string, any>>
 *
 * Returns TBMP_OK on success, TBMP_META_ERR_* for schema errors.
 *
 * Unknown or duplicate top-level keys are rejected.
 * Thread safety: This function is thread-safe.
 */
int tbmp_meta_validate_structured_blob(const uint8_t *blob, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_META_H */
