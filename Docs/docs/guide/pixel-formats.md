---
title: "Pixel Formats"
description: "Understanding pixel format options in tBMP."
---

## Overview

tBMP supports 11 pixel formats, from 1-bit palette indices to 32-bit RGBA. Choose based on your color depth needs and storage constraints.

## Indexed Formats (0-3)

Index formats use a palette (stored in the EXTRA section) to define colors.

| Format  | Bits | Colors | Palette Size |
| ------- | ---- | ------ | ------------ |
| INDEX_1 | 1    | 2      | 2 entries    |
| INDEX_2 | 2    | 4      | 4 entries    |
| INDEX_4 | 4    | 16     | 16 entries   |
| INDEX_8 | 8    | 256    | 256 entries  |

**Best for:**

- Pixel art with limited palettes
- UI icons
- Anywhere you have 256 colors or fewer

**Requires:** A palette in the EXTRA section (PALT chunk).

## Packed RGB Formats (4-7)

These pack RGB into 16 or 8 bits:

| Format  | Bits | Description                      |
| ------- | ---- | -------------------------------- |
| RGB_565 | 16   | 5 red, 6 green, 5 blue           |
| RGB_555 | 16   | 5 red, 5 green, 5 blue, 1 unused |
| RGB_444 | 16   | 4 red, 4 green, 4 blue, 4 unused |
| RGB_332 | 8    | 3 red, 3 green, 2 blue           |

**Best for:**

- Low-memory scenarios
- Simple 3D textures
- When you do not need alpha

## True Color Formats (8-9)

Full color, no palette needed:

| Format    | Bits | Channel Order                              |
| --------- | ---- | ------------------------------------------ |
| RGB_888   | 24   | R(8) G(8) B(8) - big-endian in stream      |
| RGBA_8888 | 32   | R(8) G(8) B(8) A(8) - big-endian in stream |

**Best for:**

- Full color images
- When you need transparency
- Photo data

## Custom Masked Format (10)

Define your own channel layout using masks in the EXTRA section.

**Requires:** A MASK chunk in the EXTRA section specifying R, G, B, A bit masks.

**Best for:**

- Unusual color depths
- Hardware-specific formats
- When you need precise control

## Choosing a Format

Consider these questions:

1. **Do you need transparency?** Use RGBA formats.
2. **Is memory tight?** Try RGB_565 or indexed formats.
3. **Are you using pixel art?** INDEX_8 with a palette is often best.
4. **Does your hardware require a specific layout?** Use CUSTOM with masks.

## Conversion

The library converts all formats to RGBA_8888 on decode. Use `tbmp_pixel_to_rgba()` for manual conversion:

```c
TBmpRGBA color = tbmp_pixel_to_rgba(raw_value, fmt, &palette, &masks);
```

For indexed formats, pass the palette. For CUSTOM format, pass the masks. Other formats ignore these parameters.
