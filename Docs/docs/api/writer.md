---
title: "Writer API"
description: "Encode tBMP files"
---

## tbmp_write_params

```c
typedef struct TBmpWriteParams {
    TBmpEncoding encoding;        // encoding mode
    TBmpPixelFormat pixel_format; // output pixel format
    uint8_t bit_depth;         // output bit depth

    const TBmpPalette *palette;  // NULL = no palette
    const TBmpMasks *masks;    // NULL = no masks
    const TBmpMeta *meta;      // NULL = no META

    uint16_t version;          // TBMP_VERSION_1_0
} TBmpWriteParams;
```

Configuration for the encoder.

## tbmp_write_default_params

```c
void tbmp_write_default_params(TBmpWriteParams *p);
```

Fill p with safe defaults:

- encoding = TBMP_ENC_RAW
- pixel_format = TBMP_FMT_RGBA_8888
- bit_depth = 32
- version = TBMP_VERSION_1_0
- palette/masks/meta = NULL

## tbmp_write_needed_size

```c
size_t tbmp_write_needed_size(const TBmpFrame *frame, const TBmpWriteParams *params);
```

Return minimum output buffer size needed.

**Returns:** 0 on overflow or invalid params.

## tbmp_write

```c
TBmpError tbmp_write(const TBmpFrame *frame, const TBmpWriteParams *params,
                     uint8_t *out, size_t out_cap, size_t *out_len);
```

Encode a TBmpFrame to tBMP byte stream.

## tbmp_pick_best_encoding

```c
TBmpEncoding tbmp_pick_best_encoding(const TBmpFrame *frame, const TBmpWriteParams *params);
```

Pick between RAW and RLE based on estimated output size.

## tbmp_auto_palette_from_frame

```c
TBmpError tbmp_auto_palette_from_frame(const TBmpFrame *frame,
                                uint32_t max_colors, TBmpPalette *out);
```

Generate a palette from frame colors.

**Parameters:**

- `frame` - source RGBA frame
- `max_colors` - target palette size (1..TBMP_MAX_PALETTE)
- `out` - receives generated palette
