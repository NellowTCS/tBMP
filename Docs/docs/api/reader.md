---
title: "Reader API"
description: "Parse and validate tBMP files"
---

## tbmp_open

```c
TBmpError tbmp_open(const uint8_t *buf, size_t buf_len, TBmpImage *out);
```

Parse and validate a tBMP buffer. Zero-allocation - pointers alias into the caller buffer.

**Parameters:**

- `buf` - pointer to file data (non-NULL)
- `buf_len` - byte length of buffer
- `out` - output structure to fill (non-NULL)

**Returns:** TBmpError code

**On success:** `out->head`, `out->data`, `out->extra`, `out->meta`, `out->palette`, `out->masks` are populated.

**On failure:** `out` is zeroed.

## tbmp_validate_head

```c
TBmpError tbmp_validate_head(const TBmpHead *head);
```

Validate only the header. Useful for streaming scenarios.

## tbmp_error_str

```c
const char *tbmp_error_str(TBmpError err);
```

Get human-readable string for an error code.

## tbmp_read_u16le / tbmp_read_u32le

```c
static inline uint16_t tbmp_read_u16le(const uint8_t *p);
static inline uint32_t tbmp_read_u32le(const uint8_t *p);
```

Read little-endian integers from unaligned pointers. Safe on all architectures.
