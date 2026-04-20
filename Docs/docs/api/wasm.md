---
title: "WASM API"
description: "WebAssembly JavaScript API for tBMP"
---

The WASM build exposes a JavaScript-friendly API for use in browsers.

## Loading

| Function               | Description                        |
| ---------------------- | ---------------------------------- |
| `tbmp_load(data, len)` | Load tBMP file into internal state |
| `tbmp_reset()`         | Clear state for re-use             |
| `tbmp_cleanup()`       | Free all allocated memory          |

## Info

| Function              | Returns                    |
| --------------------- | -------------------------- |
| `tbmp_width()`        | Image width                |
| `tbmp_height()`       | Image height               |
| `tbmp_pixel_format()` | TBmpPixelFormat enum value |
| `tbmp_encoding()`     | TBmpEncoding enum value    |
| `tbmp_bit_depth()`    | Bits per pixel             |
| `tbmp_error()`        | Error code (0 = OK)        |
| `tbmp_has_palette()`  | 1 if palette present       |
| `tbmp_has_masks()`    | 1 if masks present         |
| `tbmp_has_extra()`    | 1 if EXTRA chunk present   |
| `tbmp_has_meta()`     | 1 if META chunk present    |

## Pixels

| Function                         | Description                   |
| -------------------------------- | ----------------------------- |
| `tbmp_pixels_len()`              | Pixel buffer byte size        |
| `tbmp_pixels_ptr()`              | Direct pointer to RGBA pixels |
| `tbmp_pixels_copy(addr, maxLen)` | Copy pixels to caller buffer  |

## Encoding

| Function                                                       | Description          |
| -------------------------------------------------------------- | -------------------- |
| `tbmp_encode(pixels, w, h, encoding, fmt, bitDepth, out, cap)` | Encode RGBA to tBMP  |
| `tbmp_write_needed(w, h, encoding, fmt, bitDepth)`             | Required buffer size |

## Rotation

| Function                                           | Description           |
| -------------------------------------------------- | --------------------- |
| `tbmp_rotate_90()`                                 | Rotate 90° clockwise  |
| `tbmp_rotate_180()`                                | Rotate 180°           |
| `tbmp_rotate_270()`                                | Rotate 270° clockwise |
| `tbmp_rotate_any(angle, r, g, b, a, filter)`       | Arbitrary angle       |
| `tbmp_rotate_output_size(w, h, angle, outW, outH)` | Output dimensions     |

`filter`: 0 = nearest-neighbor, 1 = bilinear

## Conversion

| Function                                  | Description            |
| ----------------------------------------- | ---------------------- |
| `tbmp_convert_pixel(val, fmt)`            | Packed pixel → RGBA    |
| `tbmp_dither(numColors)`                  | Floyd-Steinberg dither |
| `tbmp_validate(w, h, bitDepth, enc, fmt)` | Validate header params |

## JSON Output

| Function                               | Output                                            |
| -------------------------------------- | ------------------------------------------------- |
| `tbmp_image_info_json(addr, maxLen)`   | `{"width", "height", "pixel_format", "encoding"}` |
| `tbmp_meta_json(addr, maxLen)`         | Parsed metadata                                   |
| `tbmp_palette_json(addr, maxLen)`      | Palette array                                     |
| `tbmp_masks_json(addr, maxLen)`        | Masks object                                      |
| `tbmp_extra_json(addr, maxLen)`        | Raw EXTRA chunk                                   |
| `tbmp_error_string(err, addr, maxLen)` | Error message                                     |

## JavaScript Usage

```javascript
import tbmpWasm from "./wasm/tbmp_wasm.js";

const tbmp = await tbmpWasm();

// Load a file
const data = await fetch("image.tbmp").then((r) => r.arrayBuffer());
const err = tbmp._tbmp_load(new Uint8Array(data));

if (err !== 0) {
  const msg = tbmp.UTF8ToString(tbmp._tbmp_error_string(err));
  throw new Error(msg);
}

// Get image info
const w = tbmp._tbmp_width();
const h = tbmp._tbmp_height();

// Access pixels (RGBA8888, w*h*4 bytes)
const pixels = tbmp._tbmp_pixels_ptr();

// Copy pixels to JS array
const rgba = new Uint8Array(tbmp.HEAPU8.buffer, pixels, w * h * 4);

// Encode back to tBMP
const needed = tbmp._tbmp_write_needed(w, h, 0, 9, 32);
const outBuf = tbmp._malloc(needed);
const rc = tbmp._tbmp_encode(rgba, w, h, 0, 9, 32, outBuf, needed);

// Get output
const output = new Uint8Array(tbmp.HEAPU8.buffer, outBuf, needed);

// Cleanup
tbmp._tbmp_reset();
// or tbmp._tbmp_cleanup() to free everything
```

## Error Codes

Same as C API. Call `tbmp_error_string(err, buf, len)` to get human-readable message.
