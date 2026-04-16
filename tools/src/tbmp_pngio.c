#include "tbmp_pngio.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

/* Suppress warnings from vendored headers that we do not control. */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "stb_image_write.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <stdio.h>

int tbmp_cli_write_png_rgba(const char *path, int width, int height,
                            const uint8_t *rgba) {
    if (!path || !rgba || width <= 0 || height <= 0) {
        fprintf(stderr, "error: invalid PNG writer arguments\n");
        return 1;
    }

    int stride = width * 4;
    if (!stbi_write_png(path, width, height, 4, rgba, stride)) {
        fprintf(stderr, "error: failed to write PNG '%s'\n", path);
        return 1;
    }

    return 0;
}
