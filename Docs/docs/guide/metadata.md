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

The META section is a raw MessagePack map. Store arbitrary key-value data here - creation time, author, tool info, application-specific tags.

**Structure:** Single MessagePack map at the end of the file (after EXTRA, if present).

### Writing Metadata

```c
TBmpMeta meta;
meta.count = 3;

strcpy(meta.entries[0].key, "author");
meta.entries[0].value.type = TBMP_META_STR;
strcpy((char *)meta.entries[0].value.s, "Game Studio");
meta.entries[0].value.bin_len = strlen("Game Studio");

strcpy(meta.entries[1].key, "created");
meta.entries[1].value.type = TBMP_META_UINT;
meta.entries[1].value.u = 1699999999;

strcpy(meta.entries[2].key, "layer");
meta.entries[2].value.type = TBMP_META_NIL;

params.meta = &meta;
```

### Reading Metadata

```c
TBmpMeta meta;
int ret = tbmp_meta_parse(img.meta, img.meta_len, &meta);
if (ret >= 0) {
    const TBmpMetaEntry *e = tbmp_meta_find(&meta, "author");
    if (e && e->value.type == TBMP_META_STR) {
        printf("Author: %s\n", e->value.s);
    }
}
```

### Supported Value Types

- `TBMP_META_NIL` - null
- `TBMP_META_BOOL` - true/false
- `TBMP_META_UINT` - unsigned integer
- `TBMP_META_INT` - signed integer
- `TBMP_META_FLOAT` - floating point
- `TBMP_META_STR` - UTF-8 string
- `TBMP_META_BIN` - binary data

Limits: 32 entries max, 63-byte keys, 127-byte strings.

## When to Use Each

| Feature | Use Case |
|---------|----------|
| Palette | 256 colors or fewer, indexed access |
| Masks | Custom bit layouts, hardware formats |
| Metadata | Application info, not for pixel data |
