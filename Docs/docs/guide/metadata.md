---
title: "Metadata and Extras"
description: "Using palettes, masks, and custom metadata in tBMP."
---

## Extra Section

The EXTRA section stores optional chunks identified by 4-byte tags. It appears after the DATA section when `head.extra_size > 0`.

### Palette (PALT)

Used with indexed pixel formats (INDEX_1 through INDEX_8).

**Structure:**
```
[tag: "PALT"][length: 4 bytes]
  [palette_count: 4 bytes]
  [entries: palette_count * 4 bytes]
    each entry: R(1) G(1) B(1) A(1)
```

**Example:**
```c
TBmpPalette palette;
palette.count = 256;
for (int i = 0; i < 256; i++) {
    palette.entries[i].r = i;
    palette.entries[i].g = i;
    palette.entries[i].b = i;
    palette.entries[i].a = 255;
}

params.palette = &palette;
```

The library automatically writes the PALT chunk when you set `params.palette`.

### Masks (MASK)

Used with the CUSTOM pixel format to define channel layouts.

**Structure:**
```
[tag: "MASK"][length: 4 bytes]
  [mask_r: 4 bytes]
  [mask_g: 4 bytes]
  [mask_b: 4 bytes]
  [mask_a: 4 bytes]
```

**Example:**
```c
TBmpMasks masks;
masks.r = 0x00FF0000;  // R in bits 16-23
masks.g = 0x0000FF00;  // G in bits 8-15
masks.b = 0x000000FF;  // B in bits 0-7
masks.a = 0xFF000000;  // A in bits 24-31

params.masks = &masks;
```

## Metadata Section (META)

The META section is a strict structured MessagePack map.

**Structure:** Single MessagePack map at the end of the file (after EXTRA, if present).

### Writing Metadata

```c
TBmpMeta meta;
memset(&meta, 0, sizeof(meta));

strcpy(meta.title, "Forest Tiles");
strcpy(meta.author, "Nellow");
strcpy(meta.description, "Top-down biome tiles");
strcpy(meta.created, "2026-04-16T19:30:00Z");
strcpy(meta.software, "tbmp-cli 0.1");
strcpy(meta.license, "CC-BY-4.0");

meta.tag_count = 2;
strcpy(meta.tags[0], "tileset");
strcpy(meta.tags[1], "forest");

meta.has_dpi = 1;
meta.dpi = 144;
strcpy(meta.colorspace, "sRGB");

params.meta = &meta;
```

### Reading Metadata

```c
TBmpMeta meta;
if (tbmp_meta_parse(img.meta, img.meta_len, &meta) == TBMP_OK) {
  printf("Title: %s\n", meta.title);
  printf("Author: %s\n", meta.author);
}
```

`custom` remains the extensibility path: it stores `array<map<string, any>>` where each item is preserved as raw MessagePack map bytes.

### Custom Fields and Namespacing

For application-specific metadata, place fields in `custom` maps and namespace
keys so multiple tools can coexist safely.

Recommended key style:

- reverse-DNS style: `com.example.tool.build`
- project-prefixed style: `mygame.level_id`

Example custom map item:

```json
{
  "com.example.pipeline.asset_id": "tileset-forest-01",
  "com.example.pipeline.revision": 7,
  "mygame.spawn_table": "forest_a"
}
```

## Metadata Schema

tBMP supports a strict structured metadata schema validator for MessagePack META blobs.

Required fields:

- `title`: string
- `author`: string
- `description`: string
- `created`: string
- `software`: string
- `license`: string
- `tags`: array<string>

Optional fields:

- `dpi`: non-negative int/uint
- `colorspace`: string
- `custom`: array<map<string, any>>

Unknown top-level keys are rejected by schema validation.

### Validate Metadata

```c
int rc = tbmp_meta_validate_structured_blob(img.meta, img.meta_len);
if (rc != TBMP_OK) {
    /* metadata does not match the strict schema */
}
```

### Example Structured META

```json
{
  "title": "Forest Tiles",
  "author": "Nellow",
  "description": "Top-down biome tiles",
  "created": "2026-04-16T19:30:00Z",
  "software": "tbmp-cli 0.1",
  "license": "CC-BY-4.0",
  "tags": ["tileset", "rpg", "forest"],
  "dpi": 144,
  "colorspace": "sRGB",
  "custom": [
    { "team": "gfx", "build": 7 },
    { "experimental": true }
  ]
}
```

## When to Use Each

| Feature  | Use Case                             |
|----------|--------------------------------------|
| Palette  | 256 colors or fewer, indexed access  |
| Masks    | Custom bit layouts, hardware formats |
| Metadata | Application info, not for pixel data |
