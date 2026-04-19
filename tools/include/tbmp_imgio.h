#ifndef TBMP_CLI_IMGIO_H
#define TBMP_CLI_IMGIO_H

#include "tbmp_types.h"

/*
 * tbmp_cli_img_load - Load an image from disk into RGBA8888 format.
 *
 * path         : path to image file (const, non-NULL).
 * out_w, out_h : output width and height (non-NULL).
 * out_channels : output number of channels (non-NULL).
 * out_uses_stb : output flag set to 1 if stb_image was used (non-NULL).
 *
 * Returns pointer to pixel buffer (must be freed with tbmp_cli_img_free), or NULL on error.
 *
 * Thread safety: Not thread-safe (uses global state and file I/O).
 * Ownership: The caller is responsible for freeing the returned buffer.
 */
TBmpRGBA *tbmp_cli_img_load(const char *path, int *out_w, int *out_h,
                            int *out_channels, int *out_uses_stb);

/*
 * tbmp_cli_img_free - Free a pixel buffer returned by tbmp_cli_img_load.
 *
 * pixels   : pointer to pixel buffer (may be NULL).
 * uses_stb : flag indicating if stb_image was used.
 *
 * Thread safety: Not thread-safe (may interact with global state).
 */
void tbmp_cli_img_free(TBmpRGBA *pixels, int uses_stb);

#endif /* TBMP_CLI_IMGIO_H */
