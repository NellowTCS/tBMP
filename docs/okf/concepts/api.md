---
type: api
title: "API Reference"
description: "Complete function reference for the tBMP library."
source: "https://NellowTCS.github.io/tBMP/docs/api/"
path: /api/
updated: 2026-07-16
okf:
  generated_by: "@docmd/plugin-okf"
  generated_at: "2026-07-16T09:04:18.672Z"
---
---
title: "API Reference"
description: "Complete function reference for the tBMP library."
---

## Core Types

- [Core Types](./types) - Enums, structs, constants

## API Modules

- [Reader API](./reader) - Parse tBMP files
- [Decoder API](./decoder) - Decode to RGBA
- [Writer API](./writer) - Encode tBMP files
- [Metadata API](./metadata) - Handle META sections
- [Rotation API](./rotation) - Rotate images
- [WASM API](./wasm) - JavaScript/WebAssembly

## Quick Reference

### Read -> Decode -> Use

```c
TBmpImage img;
tbmp_open(buf, len, &img);

TBmpFrame frame = { .pixels = malloc(w*h*4) };
tbmp_decode(&img, &frame);

// use frame.pixels...
```

### Encode → Write

```c
TBmpWriteParams params;
tbmp_write_default_params(&params);
params.encoding = TBMP_ENC_RLE;

size_t needed = tbmp_write_needed_size(&frame, &params);
tbmp_write(&frame, &params, out, needed, &written);
```
