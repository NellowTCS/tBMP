---
title: "tBMP for Embedded Systems"
description: "Patterns for using tBMP on RAM/flash constrained targets."
---

## Why tBMP Fits Embedded

Embedded projects usually need:

- Small, deterministic decode paths.
- Tight control over memory.
- No dependency sprawl.

tBMP is designed around those constraints.

## Memory Strategy

Use static or pooled buffers whenever possible.

Typical flow:

1. Read asset blob from flash/storage into a byte buffer.
2. Parse with `tbmp_open()`.
3. Decode into a caller-provided framebuffer or tile buffer.

Because parsing is zero-allocation, memory ownership stays simple and explicit.

## Choosing Encodings

- `RAW`: fastest decode path, larger files.
- `RLE` or `zero-range`: good trade-off for simple UI assets.
- Sparse modes: useful for sparse overlays and masks.

For hard real-time paths, benchmark worst-case decode time and choose the simplest encoding that fits size limits.

## Pixel Format Recommendations

- `RGB_565` for many LCDs and MCU graphics pipelines.
- Indexed formats (`INDEX_1/2/4/8`) when palette reuse is high.
- `RGBA_8888` only when alpha precision is required and memory budget allows.

If your display format differs, convert once during asset prep when you can.

## Asset Pipeline Tips

- Pre-encode assets on host tools, not on-device.
- Keep per-screen bundles to reduce load latency.
- Validate assets in CI with strict mode before firmware packaging.

Suggested validation command:

```bash
tbmp validate asset.tbmp --strict
```

## Storage and I/O

- Prefer contiguous storage for faster flash reads.
- Keep metadata small if read-time parsing cost matters.
- If streaming from external flash, validate section/chunk lengths before dereference.

## Reliability Checklist

1. Validate magic/version before any deeper parse.
2. Bounds-check section sizes against buffer length.
3. Reject unsupported pixel formats explicitly.
4. Fail closed on malformed payloads.
5. Test decode against corrupted/fuzzed inputs.

## Typical Integration Pattern

At boot:

- Initialize display.
- Load minimal UI tBMP assets from flash.
- Decode into draw buffer.
- Blit to display.

At runtime:

- Reuse the same decode buffer for transient assets.
- Avoid heap allocations in the frame loop.

## Debugging Advice

When a render is wrong:

- Inspect header fields and section sizes.
- Dump decoded RGBA with CLI and compare expected values.
- Verify palette/mask assumptions for indexed/custom formats.

The predictable wire layout makes binary inspection practical even with simple tooling.
