---
title: "API Reference"
description: "Complete function reference for the tBMP library."
---

## Core Types

### TBmpError

Return codes used throughout the library:

| Code | Name | Description |
|------|------|-------------|
| 0 | TBMP_OK | Success |
| -1 | TBMP_ERR_NULL_PTR | NULL pointer passed |
| -2 | TBMP_ERR_BAD_MAGIC | Not a tBMP file |
| -3 | TBMP_ERR_BAD_VERSION | Unsupported version |
| -4 | TBMP_ERR_BAD_BIT_DEPTH | Invalid bit depth |
| -5 | TBMP_ERR_BAD_ENCODING | Invalid encoding ID |
| -6 | TBMP_ERR_BAD_PIXEL_FORMAT | Invalid pixel format |
| -7 | TBMP_ERR_BAD_DIMENSIONS | Invalid width/height |
| -8 | TBMP_ERR_TRUNCATED | Buffer too small |
| -9 | TBMP_ERR_OVERFLOW | Arithmetic overflow |
| -10 | TBMP_ERR_NO_PALETTE | Indexed without palette |
| -11 | TBMP_ERR_NO_MASKS | CUSTOM without masks |
| -12 | TBMP_ERR_BAD_PALETTE | Palette too large |
| -13 | TBMP_ERR_BAD_SPAN | Span overflows canvas |
| -14 | TBMP_ERR_BAD_BLOCK | Block index out of range |
| -15 | TBMP_ERR_BAD_PIXEL_COORD | Sparse pixel out of range |
| -16 | TBMP_ERR_OUT_OF_MEMORY | Buffer too small |
| -17 | TBMP_ERR_ZERO_DIMENSIONS | Width or height is zero |
| -18 | TBMP_ERR_INCONSISTENT | Data size mismatch |

### TBmpEncoding

```c
typedef enum TBmpEncoding {
    TBMP_ENC_RAW = 0,
    TBMP_ENC_ZERO_RANGE = 1,
    TBMP_ENC_RLE = 2,
    TBMP_ENC_SPAN = 3,
    TBMP_ENC_SPARSE_PIXEL = 4,
    TBMP_ENC_BLOCK_SPARSE = 5,
} TBmpEncoding;
```

### TBmpPixelFormat

```c
typedef enum TBmpPixelFormat {
    TBMP_FMT_INDEX_1 = 0,
    TBMP_FMT_INDEX_2 = 1,
    TBMP_FMT_INDEX_4 = 2,
    TBMP_FMT_INDEX_8 = 3,
    TBMP_FMT_RGB_565 = 4,
    TBMP_FMT_RGB_555 = 5,
    TBMP_FMT_RGB_444 = 6,
    TBMP_FMT_RGB_332 = 7,
    TBMP_FMT_RGB_888 = 8,
    TBMP_FMT_RGBA_8888 = 9,
    TBMP_FMT_CUSTOM = 10,
} TBmpPixelFormat;
```

## Reader API (tbmp_reader.h)

### tbmp_open

```c
TBmpError tbmp_open(const uint8_t *buf, size_t buf_len, TBmpImage *out);
```

Parse and validate a tBMP buffer. Zero-allocation - pointers alias into the caller buffer.

**Parameters:**
- `buf` - pointer to file data
- `buf_len` - byte length of buffer
- `out` - output structure to fill

**Returns:** TBmpError code

**On success:** `out->head`, `out->data`, `out->extra`, `out->meta`, `out->palette`, `out->masks` are populated.

**On failure:** `out` is zeroed.

### tbmp_validate_head

```c
TBmpError tbmp_validate_head(const TBmpHead *head);
```

Validate only the header. Useful for streaming scenarios.

### tbmp_error_str

```c
const char *tbmp_error_str(TBmpError err);
```

Get human-readable string for an error code.

## Decoder API (tbmp_decode.h)

### tbmp_decode

```c
TBmpError tbmp_decode(const TBmpImage *img, TBmpFrame *frame);
```

Fully decode an image to RGBA8888.

**Parameters:**
- `img` - parsed image from tbmp_open()
- `frame` - output frame, caller provides `pixels` buffer

**Note:** Output is always top-down, left-to-right, RGBA8888.

### tbmp_palette_lookup

```c
TBmpRGBA tbmp_palette_lookup(const TBmpPalette *pal, uint32_t idx);
```

Safe palette index lookup. Returns {0,0,0,0} for out-of-range indices.

### tbmp_pixel_to_rgba

```c
TBmpRGBA tbmp_pixel_to_rgba(uint32_t pixel_val, TBmpPixelFormat fmt,
                            const TBmpPalette *pal, const TBmpMasks *masks);
```

Convert a packed pixel to RGBA8888. Returns {0,0,0,255} on invalid input.

## Writer API (tbmp_write.h)

### tbmp_write_params

```c
typedef struct TBmpWriteParams {
    TBmpEncoding encoding;
    TBmpPixelFormat pixel_format;
    uint8_t bit_depth;
    const TBmpPalette *palette;
    const TBmpMasks *masks;
    const TBmpMeta *meta;
    uint16_t version;
} TBmpWriteParams;
```

Configuration for the encoder.

### tbmp_write_default_params

```c
void tbmp_write_default_params(TBmpWriteParams *p);
```

Fill p with safe defaults:
- encoding = TBMP_ENC_RAW
- pixel_format = TBMP_FMT_RGBA_8888
- bit_depth = 32
- version = TBMP_VERSION_1_0

### tbmp_write_needed_size

```c
size_t tbmp_write_needed_size(const TBmpFrame *frame, const TBmpWriteParams *params);
```

Return minimum output buffer size needed. Returns 0 on overflow/invalid params.

### tbmp_write

```c
TBmpError tbmp_write(const TBmpFrame *frame, const TBmpWriteParams *params,
                     uint8_t *out, size_t out_cap, size_t *out_len);
```

Encode a TBmpFrame to tBMP byte stream.

## Metadata API (tbmp_meta.h)

### tbmp_meta_parse

```c
int tbmp_meta_parse(const uint8_t *blob, size_t len, TBmpMeta *out);
```

Decode a MessagePack META blob.

**Returns:**
- TBMP_OK (0) - success
- TBMP_META_WARN_STR_TRUNC (1) - success, string truncated
- Negative - error

### tbmp_meta_find

```c
const TBmpMetaEntry *tbmp_meta_find(const TBmpMeta *meta, const char *key);
```

Look up an entry by key. Returns NULL if not found.

### tbmp_meta_encode

```c
int tbmp_meta_encode(const TBmpMeta *meta, uint8_t *out, size_t out_cap, size_t *out_len);
```

Serialize TBmpMeta to MessagePack.

## Rotation API (tbmp_rotate.h)

### tbmp_rotate

```c
TBmpError tbmp_rotate(const TBmpFrame *src, TBmpFrame *dst, double angle_rad,
                      TBmpRGBA bg, TBmpRotateFilter filter);
```

Rotate by arbitrary angle (radians, counter-clockwise).

Requires floating-point (disabled with TBMP_NO_FLOAT).

### tbmp_rotate90 / 180 / 270

```c
TBmpError tbmp_rotate90(const TBmpFrame *src, TBmpFrame *dst);
TBmpError tbmp_rotate180(const TBmpFrame *src, TBmpFrame *dst);
TBmpError tbmp_rotate270(const TBmpFrame *src, TBmpFrame *dst);
```

Lossless 90-degree rotations. No floating-point required.

## Portability API (tbmp_port.h)

### Platform Macros

| Macro | Description |
|-------|-------------|
| TBMP_NO_FLOAT | Compile out floating-point code |
| TBMP_ASSERT(expr) | Override assertion (default no-op) |
| TBMP_STATIC_ASSERT(expr, msg) | Compile-time assertion |
| TBMP_INLINE | Inline hint |

### tbmp_rotate_output_dims

```c
void tbmp_rotate_output_dims(uint16_t src_w, uint16_t src_h, double angle_rad,
                             uint16_t *out_w, uint16_t *out_h);
```

Compute output dimensions for a rotation. Not available when TBMP_NO_FLOAT is defined.
