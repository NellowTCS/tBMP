#ifndef TBMP_META_H
#define TBMP_META_H

#include "tbmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Additional error/warning codes returned by tbmp_meta_parse(). */

/* The root MessagePack object is not a map.
 * Returned as a negative int (compatible with TBmpError). */
#define TBMP_META_ERR_NOT_A_MAP (-20)

/* A map key was not a MessagePack string.
 * Returned as a negative int (compatible with TBmpError). */
#define TBMP_META_ERR_BAD_KEY (-21)

/* The blob is truncated / malformed (MPack flagged a reader error).
 * Returned as a negative int (compatible with TBmpError). */
#define TBMP_META_ERR_TRUNCATED (-22)

/* Non-fatal: at least one string/bin value was silently truncated to
 * TBMP_META_STR_MAX bytes.  Parse continues; partial results are valid.
 * Returned as a positive int (not an error). */
#define TBMP_META_WARN_STR_TRUNC (1)

/*
 * tbmp_meta_parse - decode a raw MessagePack META blob into TBmpMeta.
 *
 * blob : pointer to raw MessagePack bytes.  May be NULL iff len == 0.
 * len  : byte length of the blob.
 * out  : caller-allocated TBmpMeta to receive the decoded entries.
 *        Always zeroed on entry.
 *
 * Returns:
 *   TBMP_OK (0)                    - all entries decoded successfully.
 *   TBMP_META_WARN_STR_TRUNC (1)   - success with at least one truncated str.
 *   TBMP_ERR_NULL_PTR (-1)         - out is NULL, or blob is NULL and len > 0.
 *   TBMP_META_ERR_NOT_A_MAP (-20)  - root object is not a MessagePack map.
 *   TBMP_META_ERR_BAD_KEY   (-21)  - a map key is not a string.
 *   TBMP_META_ERR_TRUNCATED (-22)  - blob is malformed / unexpectedly short.
 *
 * Entries beyond TBMP_META_MAX_ENTRIES are silently dropped.
 * Unsupported value types (array, nested map, ext) are stored as
 * TBMP_META_NIL with the key present.
 */
int tbmp_meta_parse(const uint8_t *blob, size_t len, TBmpMeta *out);

/*
 * tbmp_meta_find - look up a decoded entry by key (linear scan, O(n)).
 *
 * meta : pointer to a successfully parsed TBmpMeta.
 * key  : NUL-terminated key string to search for.
 *
 * Returns a pointer to the first matching TBmpMetaEntry, or NULL if not
 * found.
 */
const TBmpMetaEntry *tbmp_meta_find(const TBmpMeta *meta, const char *key);

/*
 * tbmp_meta_encode - serialize a TBmpMeta to a raw MessagePack blob.
 *
 * meta    : source metadata (may be NULL: writes empty map, 1 byte).
 * out     : caller-supplied output buffer.
 * out_cap : capacity of out in bytes.
 * out_len : receives the number of bytes written on success.
 *
 * Returns TBMP_OK on success, TBMP_ERR_NULL_PTR if out/out_len are NULL,
 * TBMP_ERR_OUT_OF_MEMORY if the buffer is too small.
 */
int tbmp_meta_encode(const TBmpMeta *meta, uint8_t *out, size_t out_cap,
                     size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_META_H */
