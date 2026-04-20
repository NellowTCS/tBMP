---
title: "Core Types"
description: "Type definitions, enums, and constants"
---

## Error Codes

| Code | Name                      | Description               |
| ---- | ------------------------- | ------------------------- |
| 0    | TBMP_OK                   | Success                   |
| -1   | TBMP_ERR_NULL_PTR         | NULL pointer passed       |
| -2   | TBMP_ERR_BAD_MAGIC        | Not a tBMP file           |
| -3   | TBMP_ERR_BAD_VERSION      | Unsupported version       |
| -4   | TBMP_ERR_BAD_BIT_DEPTH    | Invalid bit depth         |
| -5   | TBMP_ERR_BAD_ENCODING     | Invalid encoding ID       |
| -6   | TBMP_ERR_BAD_PIXEL_FORMAT | Invalid pixel format      |
| -7   | TBMP_ERR_BAD_DIMENSIONS   | Invalid width/height      |
| -8   | TBMP_ERR_TRUNCATED        | Buffer too small          |
| -9   | TBMP_ERR_OVERFLOW         | Arithmetic overflow       |
| -10  | TBMP_ERR_NO_PALETTE       | Indexed without palette   |
| -11  | TBMP_ERR_NO_MASKS         | CUSTOM without masks      |
| -12  | TBMP_ERR_BAD_PALETTE      | Palette too large         |
| -13  | TBMP_ERR_BAD_SPAN         | Span overflows canvas     |
| -14  | TBMP_ERR_BAD_BLOCK        | Block index out of range  |
| -15  | TBMP_ERR_BAD_PIXEL_COORD  | Sparse pixel out of range |
| -16  | TBMP_ERR_OUT_OF_MEMORY    | Buffer too small          |
| -17  | TBMP_ERR_ZERO_DIMENSIONS  | Width or height is zero   |
| -18  | TBMP_ERR_INCONSISTENT     | Data size mismatch        |

## Encoding Modes

```c
typedef enum TBmpEncoding {
    TBMP_ENC_RAW = 0,
    TBMP_ENC_ZERO_RANGE = 1,
    TBMP_ENC_RLE = 2,
    TBMP_ENC_SPAN = 3,
    TBMP_ENC_SPARSE_PIXEL = 4,
    TBMP_ENC_BLOCK_SPARSE = 5,
    TBMP_ENC_MAX_ = 6,
} TBmpEncoding;
```

## Pixel Formats

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
    TBMP_FMT_MAX_ = 11,
} TBmpPixelFormat;
```

## Key Structures

### TBmpHead

```c
typedef struct TBmpHead {
    uint16_t version;      // TBMP_VERSION_1_0
    uint16_t width;       // 1..65535
    uint16_t height;      // 1..65535
    uint8_t bit_depth;   // 1,2,4,8,16,24,32
    uint8_t encoding;    // TBmpEncoding
    uint8_t pixel_format;// TBmpPixelFormat
    uint8_t flags;       // TBMP_FLAG_*
    uint32_t data_size;
    uint32_t extra_size;
    uint32_t meta_size;
    uint32_t reserved;
} TBmpHead;
```

### TBmpImage

```c
typedef struct TBmpImage {
    TBmpHead head;       // copy of header
    const uint8_t *data;  // pointer to DATA section
    uint32_t data_len;   // byte length
    int has_palette;
    TBmpPalette palette;
    int has_masks;
    TBmpMasks masks;
    const uint8_t *extra;
    uint32_t extra_len;
    const uint8_t *meta;
    uint32_t meta_len;
} TBmpImage;
```

### TBmpFrame

```c
typedef struct TBmpFrame {
    uint16_t width;
    uint16_t height;
    TBmpRGBA *pixels;    // caller provides buffer
} TBmpFrame;
```

### TBmpRGBA

```c
typedef struct TBmpRGBA {
    uint8_t r, g, b, a;
} TBmpRGBA;
```

### TBmpPalette

```c
typedef struct TBmpPalette {
    uint32_t count;
    TBmpRGBA entries[256];
} TBmpPalette;
```

### TBmpMasks

```c
typedef struct TBmpMasks {
    uint32_t r, g, b, a;
} TBmpMasks;
```

## Constants

| Constant            | Value  | Description            |
| ------------------- | ------ | ---------------------- |
| TBMP_MAGIC          | "tBMP" | File signature         |
| TBMP_MAGIC_LEN      | 4      | Signature length       |
| TBMP_VERSION_1_0    | 0x0100 | Version 1.0            |
| TBMP_HEAD_WIRE_SIZE | 26     | Header bytes on wire   |
| TBMP_MAX_PALETTE    | 256    | Max palette entries    |
| TBMP_BLOCK_SIZE     | 8      | Block-sparse tile size |
| TBMP_MAX_DIM        | 65535  | Max image dimension    |

## Flags

| Flag                  | Value | Description          |
| --------------------- | ----- | -------------------- |
| TBMP_FLAG_HAS_PALETTE | 0x01  | EXTRA has PALT chunk |
| TBMP_FLAG_HAS_MASKS   | 0x02  | EXTRA has MASK chunk |
