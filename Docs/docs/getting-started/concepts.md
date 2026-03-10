---
title: "Core Concepts"
description: "How tBMP structures its data and why it works the way it does."
---

## File Structure

A tBMP file looks like this on the wire:

```
[magic: "tBMP"][header][data][extra][meta]
```

Every file starts with the 4-byte magic `"tBMP"` to identify the format. After that comes the header, then your encoded pixel data, then optional extra chunks (palette, masks), and finally an optional metadata section.

### Header Layout

The header is exactly 26 bytes:

| Offset | Size | Field        | Description                                |
| ------ | ---- | ------------ | ------------------------------------------ |
| 0      | 2    | version      | File format version (0x0100 for 1.0)       |
| 2      | 2    | width        | Image width in pixels (1-65535)            |
| 4      | 2    | height       | Image height in pixels (1-65535)           |
| 6      | 1    | bit_depth    | Bits per pixel: 1, 2, 4, 8, 16, 24, or 32  |
| 7      | 1    | encoding     | Encoding mode (0-5)                        |
| 8      | 1    | pixel_format | Pixel format enum                          |
| 9      | 1    | flags        | Bitfield: bit0=has_palette, bit1=has_masks |
| 10     | 4    | data_size    | Byte length of DATA section                |
| 14     | 4    | extra_size   | Byte length of EXTRA section (0 if absent) |
| 18     | 4    | meta_size    | Byte length of META section (0 if absent)  |
| 22     | 4    | reserved     | Must be zero                               |

The wire format is strictly little-endian. The library handles byte-ordering for you on read/write.

## Encodings

tBMP supports six encoding modes. Pick the one that matches your data:

| ID  | Name         | Best For                                                |
| --- | ------------ | ------------------------------------------------------- |
| 0   | RAW          | Uncompressed pixel data: simplest, no decode cost       |
| 1   | Zero-Range   | Images with large empty (zero) regions                  |
| 2   | RLE          | Simple repeated patterns: (count, value) pairs          |
| 3   | Span         | Sparse non-repeating data: (index, length, value)       |
| 4   | Sparse Pixel | Images with few non-zero pixels: explicit (x, y, value) |
| 5   | Block-Sparse | Tiled images with populated regions: block-indexed      |

The library picks automatically if you don't specify, or you can force a specific encoding when writing.

## Pixel Formats

From 1-bit indexed to 32-bit RGBA:

| ID  | Name      | Bits | Description                               |
| --- | --------- | ---- | ----------------------------------------- |
| 0   | INDEX_1   | 1    | 1-bit palette index                       |
| 1   | INDEX_2   | 2    | 2-bit palette index                       |
| 2   | INDEX_4   | 4    | 4-bit palette index                       |
| 3   | INDEX_8   | 8    | 8-bit palette index                       |
| 4   | RGB_565   | 16   | 5-6-5 bit RGB                             |
| 5   | RGB_555   | 16   | 5-5-5 bit RGB (1 bit padding)             |
| 6   | RGB_444   | 16   | 4-4-4 bit RGB (4 bits padding)            |
| 7   | RGB_332   | 8    | 3-3-2 bit RGB                             |
| 8   | RGB_888   | 24   | 8-8-8 bit RGB (big-endian in stream)      |
| 9   | RGBA_8888 | 32   | 8-8-8-8 bit RGBA                          |
| 10  | CUSTOM    | var  | Mask-defined format (requires MASK chunk) |

Indexed formats (INDEX\_\*) require a palette in the EXTRA section. The CUSTOM format requires explicit channel masks.

## Extra Chunks

The EXTRA section holds optional data identified by 4-byte tags:

- **PALT** (Palette): Up to 256 RGBA entries. Used with INDEX\_\* formats.
- **MASK** (Masks): Four 32-bit channel masks for CUSTOM pixel format.

Each chunk is: `[tag: 4 bytes][length: 4 bytes][body: length bytes]`

## Metadata Section

The META section is a raw MessagePack map. Store whatever key-value pairs you need, creation time, author, tool info, custom tags. The library doesn't interpret this section; it's purely for your application data.

## Zero-Allocation Design

The library is designed for embedded and memory-constrained environments:

- All parsing is zero-allocation. You provide the buffer, the library points into it.
- Decoded frames use a caller-supplied pixel buffer.
- No heap allocations in the hot path.

This makes tBMP suitable for bare-metal systems, game consoles, and firmware.
