---
title: "Decoder API"
description: "Decode tBMP to RGBA pixels"
---

## tbmp_decode

```c
TBmpError tbmp_decode(const TBmpImage *img, TBmpFrame *frame);
```

Fully decode an image to a flat RGBA8888 pixel array.

**Parameters:**

- `img` - parsed image from tbmp_open() (non-NULL)
- `frame` - output frame, caller provides `pixels` buffer

**Note:** Output is always top-down, left-to-right, RGBA8888.

## tbmp_palette_lookup

```c
TBmpRGBA tbmp_palette_lookup(const TBmpPalette *pal, uint32_t idx);
```

Safe palette index lookup.

**Returns:** `{0,0,0,0}` for out-of-range indices.

## tbmp_pixel_to_rgba

```c
TBmpRGBA tbmp_pixel_to_rgba(uint32_t pixel_val, TBmpPixelFormat fmt,
                            const TBmpPalette *pal, const TBmpMasks *masks);
```

Convert a packed pixel to RGBA8888.

**Parameters:**

- `pixel_val` - the raw bit-packed pixel value
- `fmt` - TBmpPixelFormat
- `pal` - palette (required for INDEX\_\* formats)
- `masks` - masks (required for CUSTOM format)

**Returns:** RGBA8888 colour. Returns `{0,0,0,255}` on invalid input.

## tbmp_scale_channel

```c
uint8_t tbmp_scale_channel(uint32_t val, uint32_t bits);
```

Scale an N-bit unsigned value to 8 bits by MSB replication.

**Parameters:**

- `val` - input value
- `bits` - number of bits in input (0..32)

**Examples:**

- `tbmp_scale_channel(31, 5)` → `255`
- `tbmp_scale_channel(0, 8)` → `0`
- `tbmp_scale_channel(255, 8)` → `255`

## tbmp_dither_to_palette

```c
TBmpError tbmp_dither_to_palette(TBmpFrame *frame, const TBmpPalette *pal);
```

Apply Floyd-Steinberg dithering to palette. Modifies frame in place.

## tbmp_value_field_bytes

```c
static inline uint8_t tbmp_value_field_bytes(uint8_t bit_depth);
```

Bytes needed for inline pixel value in RLE/Span/Sparse modes.

| bit_depth  | bytes |
| ---------- | ----- |
| 1, 2, 4, 8 | 1     |
| 16         | 2     |
| 24         | 3     |
| 32         | 4     |
