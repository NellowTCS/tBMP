<!-- markdownlint-disable MD033 -->
<!-- markdownlint-disable MD041 -->

<div style="display: flex; align-items: center; justify-content: space-between;">
  <h1 style="margin: 0;">tBMP</h1>
  <img src="Docs/docs/assets/tBMP_Logo.png" alt="tBMP Logo" width="50"/>
</div>

---

[![Build](https://github.com/NellowTCS/tBMP/actions/workflows/ci.yml/badge.svg)](https://github.com/NellowTCS/tBMP/actions/workflows/ci.yml)
[![Tests](https://github.com/NellowTCS/tBMP/actions/workflows/ci.yml/badge.svg?label=tests)](https://github.com/NellowTCS/tBMP/actions/workflows/ci.yml)
[![Docs](https://github.com/NellowTCS/tBMP/actions/workflows/static.yml/badge.svg)](https://github.com/NellowTCS/tBMP/actions/workflows/static.yml)
[![Version](https://img.shields.io/github/v/release/NellowTCS/tBMP?display_name=tag)](https://github.com/NellowTCS/tBMP/releases)

A tiny bitmap format library for compact, efficient image storage and decoding.

tBMP gives you a binary image format with multiple encoding options, schema-driven structure, and a clean C library for reading, writing, and manipulating tiny bitmaps.

---

## Quick Start

### Build

```bash
cmake -B build
cmake --build build
```

This produces `build/libtbmp.a`.

### Read an image

```c
#include "tbmp_reader.h"
#include "tbmp_decode.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    FILE *f = fopen("sprite.tbmp", "rb");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc(len);
    fread(buf, 1, len, f);
    fclose(f);

    TBmpImage img;
    TBmpError err = tbmp_open(buf, len, &img);
    if (err != TBMP_OK) {
        printf("Failed to parse: %s\n", tbmp_error_str(err));
        free(buf);
        return 1;
    }

    TBmpFrame frame;
    frame.pixels = malloc(img.width * img.height * sizeof(TBmpRGBA));
    tbmp_decode(&img, &frame);

    printf("Loaded %dx%d image\n", frame.width, frame.height);

    free(frame.pixels);
    free(buf);
    return 0;
}
```

### Write an image

```c
#include "tbmp_write.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    uint16_t w = 32, h = 32;
    TBmpRGBA *pixels = malloc(w * h * sizeof(TBmpRGBA));

    // ... fill pixels ...

    TBmpFrame frame = { .width = w, .height = h, .pixels = pixels };

    TBmpWriteParams params;
    tbmp_write_default_params(&params);
    params.encoding = TBMP_ENC_RLE;

    size_t needed = tbmp_write_needed_size(&frame, &params);
    uint8_t *out = malloc(needed);

    size_t written;
    tbmp_write(&frame, &params, out, needed, &written);

    FILE *f = fopen("output.tbmp", "wb");
    fwrite(out, 1, written, f);
    fclose(f);

    free(pixels);
    free(out);
    return 0;
}
```

---

## Repository layout

```txt
include/                 # Public headers
  tbmp_types.h           # Core types and enums
  tbmp_reader.h          # File parsing
  tbmp_decode.h          # Pixel decoding
  tbmp_write.h           # File writing
  tbmp_meta.h            # Metadata handling
  tbmp_rotate.h          # Rotation transforms
  tbmp_port.h            # Portability macros
  tbmp_msgpack.h         # MessagePack for metadata

src/                    # Implementation
  tbmp_reader.c
  tbmp_decode.c
  tbmp_write.c
  tbmp_meta.c
  tbmp_pixel.c
  tbmp_rotate.c
  tbmp_msgpack.c

tests/                  # Test suite
Docs/                   # Documentation site
tBMP.ksy                # Binary format schema
```

---

## Features

- Multiple encodings: RAW, RLE, zero-range, span, sparse pixel, block-sparse
- Zero dependencies: plain C, no external libs
- Schema-driven: format defined in `tBMP.ksy`
- Metadata support: palettes, masks, custom MessagePack metadata
- Zero-allocation design: caller provides buffers

---

## Demo

Want to see tBMP in action? Try the interactive web demo:

-> [Live Demo](https://nellowtcs.me/tBMP/)  
Load, inspect, and visualize .tbmp files right in your browser. Powered by WebAssembly and the same C core as the library.

---

## Documentation

Full documentation is available in the Docs/ folder.

---

## CLI Tools

When built with `-DTBMP_BUILD_TOOLS=ON`, the `tbmp` executable supports:

```bash
tbmp encode <input> <output.tbmp> [--format NAME] [--encoding NAME] [META_OPTS]
tbmp decode <input.tbmp> <output.ppm>
tbmp export-png <input.tbmp> <output.png>
tbmp validate <input.tbmp> [--strict]
tbmp inspect <input.tbmp>
tbmp dump-rgba <input.tbmp> <output.rgba>

# global options
tbmp --help
tbmp --ci <command> ...
```

`META_OPTS` for encode:

- `--title`
- `--author`
- `--description`
- `--created`
- `--software`
- `--license`
- `--tags` (comma-separated)
- `--dpi`
- `--colorspace`
- `--custom-map` (repeatable MessagePack map blob file)
- `--meta-file` (structured MessagePack metadata blob)

Encode helper flags:

- `--pick-encoding` (auto pick between `raw` and `rle`)
- `--auto-palette` (generate palette for indexed formats)
- `--dither` (Floyd-Steinberg dither to target palette)

Custom format options (when using `--format custom`):

- `--bit-depth` (`8`, `16`, `24`, or `32`; default `32`)
- `--mask-r` (required, decimal or `0x` hex)
- `--mask-g` (required, decimal or `0x` hex)
- `--mask-b` (required, decimal or `0x` hex)
- `--mask-a` (optional, default `0`)

- `inspect` prints header fields, section presence/sizes, EXTRA chunk summary,
  palette/masks info, metadata entries, and warnings.
- `export-png` decodes a `.tbmp` and writes PNG output for debugging/preview.
- `validate` checks parse integrity; `--strict` also rejects unknown/malformed
  EXTRA chunks and invalid structured metadata.
- `dump-rgba` decodes to raw RGBA bytes (`R,G,B,A` per pixel) for debugging
  or external processing.
- Default output is styled for interactive terminals.
- `--ci` disables styling and forces plain, script-friendly text.

---

## Contributing

Contributions are welcome. Here is how to get started:

1. Fork the repository.
2. Create a branch for your feature or fix (`git checkout -b feature/name`).
3. Commit your changes (`git commit -m "Description of change"`).
4. Push your branch (`git push origin feature/name`).
5. Open a Pull Request.

Please make sure all tests pass before submitting.

For review standards and the design-principle checklist, see [CONTRIBUTING.md](CONTRIBUTING.md).

Developer convenience script:

```bash
./scripts/dev.sh
```

This configures with tools/tests enabled, builds, and runs tests.

---

## Star History

<a href="https://www.star-history.com/?repos=tbmp%2Ftbmp&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/image?repos=NellowTCS/tBMP&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/image?repos=NellowTCS/tBMP&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/image?repos=NellowTCS/tBMP&type=date&legend=top-left" />
 </picture>
</a>

---

## License

Apache-v2.0
See [LICENSE](LICENSE).
