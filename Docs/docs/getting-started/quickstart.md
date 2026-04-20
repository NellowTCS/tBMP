---
title: "Quick Start"
description: "Get tBMP up and running in 5 minutes."
---

::: callout tip
Want to try tBMP instantly? [Open the web demo](https://nellowtcs.me/tBMP/) to load and inspect .tbmp files in your browser.
:::

## Build the Library

```bash
# Clone the repo
git clone <repo-url>
cd <repo>

# Build with CMake
cmake -B build
cmake --build build
```

This produces:

- `build/libtbmp.a`: the static library
- `build/tbmp_test`: test runner

## Include tBMP in Your Project

Add `include/` to your header search path, link against `libtbmp.a`:

```bash
gcc -I/path/to/include my_program.c -L/path/to/build -ltbmp -o my_program
```

## Read an Image

```c
#include "tbmp_reader.h"
#include "tbmp_decode.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Load the file into memory
    FILE *f = fopen("sprite.tbmp", "rb");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc(len);
    fread(buf, 1, len, f);
    fclose(f);

    // Parse the header
    TBmpImage img;
    TBmpError err = tbmp_open(buf, len, &img);
    if (err != TBMP_OK) {
        printf("Failed to parse: %s\n", tbmp_error_str(err));
        free(buf);
        return 1;
    }

    // Decode to RGBA
    TBmpFrame frame;
    frame.pixels = malloc(img.width * img.height * sizeof(TBmpRGBA));

    err = tbmp_decode(&img, &frame);
    if (err != TBMP_OK) {
        printf("Failed to decode: %s\n", tbmp_error_str(err));
        free(buf);
        free(frame.pixels);
        return 1;
    }

    printf("Loaded %dx%d image\n", frame.width, frame.height);

    // ... use frame.pixels ...

    free(frame.pixels);
    free(buf);
    return 0;
}
```

## Write an Image

```c
#include "tbmp_write.h"
#include "tbmp_types.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Create a 32x32 test pattern
    uint16_t w = 32, h = 32;
    TBmpRGBA *pixels = malloc(w * h * sizeof(TBmpRGBA));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            pixels[y * w + x].r = (uint8_t)(x * 8);
            pixels[y * w + x].g = (uint8_t)(y * 8);
            pixels[y * w + x].b = 128;
            pixels[y * w + x].a = 255;
        }
    }

    TBmpFrame frame = { .width = w, .height = h, .pixels = pixels };

    // Configure encoding
    TBmpWriteParams params;
    tbmp_write_default_params(&params);
    params.encoding = TBMP_ENC_RLE;
    params.pixel_format = TBMP_FMT_RGBA_8888;

    // Get output size
    size_t needed = tbmp_write_needed_size(&frame, &params);
    uint8_t *out = malloc(needed);

    // Write
    size_t written;
    tbmp_write(&frame, &params, out, needed, &written);

    FILE *f = fopen("output.tbmp", "wb");
    fwrite(out, 1, written, f);
    fclose(f);

    printf("Wrote %zu bytes\n", written);

    free(pixels);
    free(out);
    return 0;
}
```

## Error Handling

All functions return `TBmpError` codes:

```c
if (err != TBMP_OK) {
    fprintf(stderr, "Error: %s\n", tbmp_error_str(err));
}
```

Common errors:

- `TBMP_ERR_BAD_MAGIC`: not a tBMP file
- `TBMP_ERR_BAD_VERSION`: unsupported format version
- `TBMP_ERR_TRUNCATED`: file too short for declared sizes
- `TBMP_ERR_BAD_ENCODING`: invalid encoding ID
- `TBMP_ERR_NO_PALETTE`: indexed format without palette

## What is Next

- [Core Concepts](../concepts): Deep dive into the format
- [Encoding Guide](../../guide/encoding): Choosing the right encoding
- [API Reference](../../api/): Full function documentation
