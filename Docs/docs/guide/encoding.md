---
title: "Encoding Guide"
description: "Choose the right encoding mode for your image data."
---

## When to Use Each Encoding

tBMP gives you six encoding modes. Here is when to reach for each one:

### RAW (0)

Straight pixel data, no compression. No decode overhead - just copy the bytes.

**Best for:**

- Already-compressed data
- Pixel formats that do not compress well
- Maximum decode speed priority

**Example use case:** RGBA screenshots, photo data

### Zero-Range (1)

Tracks regions of zero-valued pixels separately from non-zero data.

**Best for:**

- Images with large black/transparent areas
- Depth maps
- Alpha-cutout sprites

**How it works:**

```txt
[count of zero-ranges][(start, length) pairs...][non-zero pixel count][non-zero values...]
```

### RLE (2)

Simple run-length encoding: (count, value) pairs.

**Best for:**

- Simple patterns with repeated values
- Pixel art with solid color runs
- When you need something faster than zero-range

**How it works:**

```txt
[(count, value), (count, value), ...]
```

### Span (3)

Like RLE but specifies absolute positions: (pixel_index, length, value).

**Best for:**

- Data that does not compress well with pure RLE
- Random-access decode scenarios

### Sparse Pixel (4)

Explicit (x, y, value) coordinates for each non-zero pixel.

**Best for:**

- Very sparse images (few non-zero pixels)
- Debug overlays
- Hitmasks

**Trade-off:** Higher overhead per pixel, but only stores what you need.

### Block-Sparse (5)

Tiles the image into blocks, stores only populated blocks.

**Best for:**

- Tiled game sprites
- Images with localized detail
- When you want random access to tile regions

**Trade-off:** Slightly more complex decode, but great compression for structured data.

## Choosing Automatically

If you are not sure, let the library decide:

```c
TBmpWriteParams params;
tbmp_write_default_params(&params);
params.encoding = TBMP_ENC_RAW;  // library picks based on data
```

The encoder analyzes your data and picks the best encoding when you leave it at RAW.

## Manual Selection

Force a specific encoding:

```c
TBmpWriteParams params;
tbmp_write_default_params(&params);
params.encoding = TBMP_ENC_RLE;
```

## Encoding Reference

| Encoding     | Decode Speed | Compression | Best For                |
| ------------ | ------------ | ----------- | ----------------------- |
| RAW          | Fastest      | None        | Uncompressed source     |
| Zero-Range   | Fast         | Medium      | Black/transparent areas |
| RLE          | Fast         | Medium-Low  | Repeated patterns       |
| Span         | Medium       | Medium      | Sparse with position    |
| Sparse Pixel | Medium       | Depends     | Very sparse images      |
| Block-Sparse | Medium       | High        | Tiled images            |

## Performance Tips

1. **Match encoding to your data** - do not force RLE on random noise
2. **Use appropriate pixel formats** - RGB_565 is half the size of RGBA_8888
3. **Consider indexed formats** - INDEX_8 + palette beats RGBA for 256-color art
4. **Profile your use case** - decode speed often matters more than file size
