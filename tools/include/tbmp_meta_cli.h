#ifndef TBMP_CLI_META_H
#define TBMP_CLI_META_H

#include "tbmp_types.h"

/*
 * tbmp_cli_parse_encode_meta_flags - Parse CLI flags for encoding metadata.
 *
 * argc, argv : standard argument count and vector.
 * meta       : pointer to TBmpMeta to fill (non-NULL).
 * has_meta   : output flag set to 1 if meta was parsed (non-NULL).
 *
 * Returns 0 on success, negative on error.
 *
 * Thread safety: Not thread-safe (uses global state and environment).
 * Ownership: The caller retains ownership of all buffers and structs.
 */
int tbmp_cli_parse_encode_meta_flags(int argc, char **argv, TBmpMeta *meta,
                                     int *has_meta);

/*
 * tbmp_cli_print_meta - Print metadata from a TBmpImage to stdout.
 *
 * img : pointer to TBmpImage (const, non-NULL).
 *
 * Returns number of fields printed.
 *
 * Thread safety: Not thread-safe (writes to shared stdout).
 */
uint32_t tbmp_cli_print_meta(const TBmpImage *img);

#endif /* TBMP_CLI_META_H */
