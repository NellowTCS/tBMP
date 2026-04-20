---
title: "API Reference"
description: "Complete function reference for the tBMP library."
---

## Core Types

- [Core Types](/api/types) - Enums, structs, constants

## API Modules

- [Reader API](/api/reader) - Parse tBMP files
- [Decoder API](/api/decoder) - Decode to RGBA
- [Writer API](/api/writer) - Encode tBMP files
- [Metadata API](/api/metadata) - Handle META sections
- [Rotation API](/api/rotation) - Rotate images
- [WASM API](/api/wasm) - JavaScript/WebAssembly

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
