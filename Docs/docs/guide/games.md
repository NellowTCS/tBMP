---
title: "tBMP for Games"
description: "Using tBMP for sprites, tiles, UI, and game asset pipelines."
---

## Where tBMP Helps in Games

tBMP works well for:

- Pixel-art sprites.
- UI icon atlases.
- Retro or fixed-palette art.
- Runtime overlays, masks, and debug layers.

It gives you explicit control over size vs decode cost per asset class.

## Suggested Asset Buckets

Split assets by usage profile:

- Hot-path sprites: favor fast decode (`RAW` or simple `RLE`).
- Bulk content: favor size (`RLE`, sparse, block-sparse depending on data).
- UI and icons: often good candidates for indexed + palette.

Treat encoding as a per-bucket choice, not a global toggle.

## Sprites and Atlases

Recommended patterns:

- Keep frequently co-rendered sprites near each other in storage.
- Use metadata to track atlas region names, frame IDs, and tags.
- Use strict validation in build pipelines to catch malformed assets early.

## Palette-Driven Workflows

For pixel art and retro aesthetics:

- Use indexed formats with `PALT`.
- Keep palette changes intentional to avoid visual drift.
- Version palettes alongside art to keep reproducibility.

## Runtime Considerations

- Decode off the render thread when possible.
- Cache decoded surfaces for high-frequency assets.
- Keep fallback placeholders for failed decode paths.

If memory is tight, cache only hot assets and stream less common ones.

## Metadata for Tooling

Structured metadata can carry:

- Authoring tool version.
- Asset category (`enemy`, `ui`, `fx`).
- Build revision or content hash.
- Tags used by runtime lookup.

Custom metadata maps are useful for engine-specific properties that should travel with the asset.

## Example Build Pipeline

1. Source art exported to intermediate format.
2. Convert to tBMP with chosen pixel format/encoding.
3. Inject structured metadata.
4. Run strict validation.
5. Package into game asset bundle.

This keeps runtime simple and deterministic.

## Deterministic Build Note

If assets affect deterministic outcomes (rare, but possible), pin encode settings and asset versions in your build graph.

Avoid nondeterministic preprocessing steps across build machines.

## Practical Rule of Thumb

Optimize for frame-time stability first, then for disk size.

A slightly larger file with stable decode cost is often a better game-time trade than maximal compression.
