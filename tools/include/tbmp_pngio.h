#ifndef TBMP_CLI_PNGIO_H
#define TBMP_CLI_PNGIO_H

#include <stdint.h>

/*
 * tbmp_cli_write_png_rgba - Write an RGBA8888 buffer to a PNG file.
 *
 * path   : output file path (const, non-NULL).
 * width  : image width in pixels.
 * height : image height in pixels.
 * rgba   : pointer to RGBA8888 pixel buffer (const, non-NULL).
 *
 * Returns 0 on success, negative on error.
 *
 * Thread safety: Not thread-safe (uses file I/O and global state).
 * Ownership: The caller retains ownership of all buffers and structs.
 */
int tbmp_cli_write_png_rgba(const char *path, int width, int height,
                            const uint8_t *rgba);

#endif /* TBMP_CLI_PNGIO_H */
