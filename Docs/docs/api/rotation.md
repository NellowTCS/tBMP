---
title: "Rotation API"
description: "Rotate and transform images"
---

## Lossless Rotations

```c
TBmpError tbmp_rotate90(const TBmpFrame *src, TBmpFrame *dst);
TBmpError tbmp_rotate180(const TBmpFrame *src, TBmpFrame *dst);
TBmpError tbmp_rotate270(const TBmpFrame *src, TBmpFrame *dst);
```

Lossless 90-degree rotations. No floating-point required.

- `tbmp_rotate90`: 90° clockwise (width ↔ height swapped)
- `tbmp_rotate180`: 180° (dimensions unchanged)
- `tbmp_rotate270`: 270° clockwise (width ↔ height swapped)

## Arbitrary Angle

```c
void tbmp_rotate_output_dims(uint16_t src_w, uint16_t src_h, double angle_rad,
                              uint16_t *out_w, uint16_t *out_h);

TBmpError tbmp_rotate(const TBmpFrame *src, TBmpFrame *dst, double angle_rad,
                       TBmpRGBA bg, TBmpRotateFilter filter);
```

Rotate by arbitrary angle in radians (counter-clockwise positive).

**Requires:** Floating-point enabled (compile with TBMP_NO_FLOAT to disable)

**Filters:**

- `TBMP_ROTATE_NEAREST` (0) - nearest-neighbor, fastest
- `TBMP_ROTATE_BILINEAR` (1) - bilinear interpolation

Background `bg` fills unmapped pixels.

## Filter Enum

```c
typedef enum TBmpRotateFilter {
    TBMP_ROTATE_NEAREST = 0,
    TBMP_ROTATE_BILINEAR = 1,
} TBmpRotateFilter;
```

## Platform Notes

With `TBMP_NO_FLOAT` defined:

- Arbitrary rotation not available
- `tbmp_rotate_output_dims` falls back to nearest 90° quadrant
- Integer-only rotations always available
