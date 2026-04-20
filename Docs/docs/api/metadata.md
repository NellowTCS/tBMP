---
title: "Metadata API"
description: "Handle META sections"
---

## tbmp_meta_parse

```c
int tbmp_meta_parse(const uint8_t *blob, size_t len, TBmpMeta *out);
```

Parse structured metadata from MessagePack blob.

**Returns:**

- TBMP_OK (0) - success
- Negative - error (TBMP*META_ERR*\*)

## tbmp_meta_encode

```c
int tbmp_meta_encode(const TBmpMeta *meta, uint8_t *out, size_t out_cap,
                     size_t *out_len);
```

Serialize TBmpMeta to MessagePack.

**Returns:** TBMP_OK on success.

## tbmp_meta_validate_structured_blob

```c
int tbmp_meta_validate_structured_blob(const uint8_t *blob, size_t len);
```

Validate that a META blob matches the strict schema.

**Required fields:** title, author, description, created, software, license, tags
**Optional fields:** dpi, colorspace, custom

**Returns:** TBMP_OK or error code.

## Error Codes

| Code                                     | Description            |
| ---------------------------------------- | ---------------------- |
| TBMP_META_ERR_NOT_A_MAP (-20)            | Root is not a map      |
| TBMP_META_ERR_BAD_KEY (-21)              | Map key not string     |
| TBMP_META_ERR_TRUNCATED (-22)            | Blob truncated         |
| TBMP_META_ERR_REQUIRED_MISSING (-23)     | Required field missing |
| TBMP_META_ERR_TYPE_MISMATCH (-24)        | Unexpected value type  |
| TBMP_META_ERR_LENGTH_OUT_OF_RANGE (-25)  | String/array too long  |
| TBMP_META_ERR_SCHEMA_UNKNOWN_KEY (-26)   | Unknown top-level key  |
| TBMP_META_ERR_SCHEMA_DUPLICATE_KEY (-27) | Duplicate key          |

## TBmpMeta Structure

```c
typedef struct TBmpMeta {
    char title[TBMP_META_MAX_STR];
    char author[TBMP_META_MAX_STR];
    // ... more fields ...
    uint32_t tag_count;
    char tags[TBMP_META_MAX_TAGS][TBMP_META_MAX_STR];
} TBmpMeta;
```

See [Metadata Guide](/guide/metadata) for full structure definition.
