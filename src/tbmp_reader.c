#include "tbmp_reader.h"

#include <limits.h> /* SIZE_MAX */
#include <string.h> /* memcmp, memset, memcpy */

/* Internal helpers. */

/*
 * safe_add_size - add two size_t values, returning 0 and setting *overflow
 * on overflow.  Used throughout to guard against crafted files.
 */
static size_t safe_add_size(size_t a, size_t b, int *overflow) {
    if (a > (SIZE_MAX - b)) {
        *overflow = 1;
        return 0;
    }
    return a + b;
}

/*
 * tbmp_parse_head - deserialise TBMP_HEAD_WIRE_SIZE (26) bytes of
 * wire-format header into h.  Uses byte-by-byte reads so it is safe on
 * unaligned access targets (Cortex-M0, etc.).
 */
static void tbmp_parse_head(const uint8_t *p, TBmpHead *h) {
    h->version = tbmp_read_u16le(p);
    p += 2;
    h->width = tbmp_read_u16le(p);
    p += 2;
    h->height = tbmp_read_u16le(p);
    p += 2;

    h->bit_depth = p[0];
    p += 1;
    h->encoding = p[0];
    p += 1;
    h->pixel_format = p[0];
    p += 1;
    h->flags = p[0];
    p += 1;

    h->data_size = tbmp_read_u32le(p);
    p += 4;
    h->extra_size = tbmp_read_u32le(p);
    p += 4;
    h->meta_size = tbmp_read_u32le(p);
    p += 4;
    h->reserved = tbmp_read_u32le(p);
    (void)p;
}

/* Extra chunk parsing helpers. */

/* Parse a PALT sub-chunk; returns TBMP_OK or an error. */
static TBmpError parse_palette(const uint8_t *body, size_t body_len,
                               TBmpPalette *pal) {
    if (body_len < 4U)
        return TBMP_ERR_TRUNCATED;

    uint32_t count = tbmp_read_u32le(body);
    if (count > TBMP_MAX_PALETTE)
        return TBMP_ERR_BAD_PALETTE;

    /* 4 bytes for count + count*4 bytes for entries */
    size_t required = 4U + (size_t)count * 4U;
    if (body_len < required)
        return TBMP_ERR_TRUNCATED;

    pal->count = count;
    const uint8_t *ep = body + 4;
    for (uint32_t i = 0; i < count; i++, ep += 4) {
        pal->entries[i].r = ep[0];
        pal->entries[i].g = ep[1];
        pal->entries[i].b = ep[2];
        pal->entries[i].a = ep[3];
    }
    return TBMP_OK;
}

/* Parse a MASK sub-chunk; returns TBMP_OK or an error. */
static TBmpError parse_masks(const uint8_t *body, size_t body_len,
                             TBmpMasks *masks) {
    if (body_len < 16U)
        return TBMP_ERR_TRUNCATED;
    masks->r = tbmp_read_u32le(body);
    masks->g = tbmp_read_u32le(body + 4);
    masks->b = tbmp_read_u32le(body + 8);
    masks->a = tbmp_read_u32le(body + 12);
    return TBMP_OK;
}

/*
 * parse_extra - walk the EXTRA chunk, populating palette/masks in img.
 * Each sub-entry is: 4-byte ASCII tag + 4-byte LE length + body.
 */
static TBmpError parse_extra(const uint8_t *extra, size_t extra_len,
                             TBmpImage *img) {
    size_t pos = 0;

    while (pos < extra_len) {
        /* Need at least 8 bytes for tag + length. */
        if (extra_len - pos < 8U)
            return TBMP_ERR_TRUNCATED;

        const uint8_t *tag = extra + pos;
        uint32_t entry_len = tbmp_read_u32le(extra + pos + 4);
        pos += 8U;

        if (entry_len > (extra_len - pos))
            return TBMP_ERR_TRUNCATED;

        const uint8_t *body = extra + pos;
        size_t body_len = entry_len;

        if (memcmp(tag, "PALT", 4) == 0) {
            TBmpError e = parse_palette(body, body_len, &img->palette);
            if (e != TBMP_OK)
                return e;
            img->has_palette = 1;

        } else if (memcmp(tag, "MASK", 4) == 0) {
            TBmpError e = parse_masks(body, body_len, &img->masks);
            if (e != TBMP_OK)
                return e;
            img->has_masks = 1;
        }
        /* Unknown tags are silently skipped for forward compatibility. */

        pos += entry_len;
    }
    return TBMP_OK;
}

/* Public API. */

TBmpError tbmp_validate_head(const TBmpHead *head) {
    if (head == NULL)
        return TBMP_ERR_NULL_PTR;

    /* Version. */
    if (head->version != TBMP_VERSION_1_0) {
        return TBMP_ERR_BAD_VERSION;
    }

    /* Dimensions. */
    if (head->width == 0 || head->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;

    /* bit_depth: only 1, 2, 4, 8, 16, 24, 32 are valid. */
    switch (head->bit_depth) {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 24:
    case 32:
        break;
    default:
        return TBMP_ERR_BAD_BIT_DEPTH;
    }

    /* encoding. */
    if (head->encoding >= (uint8_t)TBMP_ENC_MAX_)
        return TBMP_ERR_BAD_ENCODING;

    /* pixel_format. */
    if (head->pixel_format >= (uint8_t)TBMP_FMT_MAX_)
        return TBMP_ERR_BAD_PIXEL_FORMAT;

    /* Indexed formats must have a bit_depth that matches the index width. */
    if (head->pixel_format <= (uint8_t)TBMP_FMT_INDEX_8) {
        static const uint8_t kIndexBits[4] = {1, 2, 4, 8};
        uint8_t expected = kIndexBits[head->pixel_format];
        if (head->bit_depth != expected)
            return TBMP_ERR_BAD_BIT_DEPTH;
    }

    return TBMP_OK;
}

TBmpError tbmp_open(const uint8_t *buf, size_t buf_len, TBmpImage *out) {
    if (buf == NULL || out == NULL)
        return TBMP_ERR_NULL_PTR;

    memset(out, 0, sizeof(*out));

    /* 1. Minimum size check. */
    if (buf_len < TBMP_MIN_FILE_SIZE)
        return TBMP_ERR_TRUNCATED;

    /* 2. Magic. */
    if (memcmp(buf, TBMP_MAGIC, TBMP_MAGIC_LEN) != 0)
        return TBMP_ERR_BAD_MAGIC;

    /* 3. Parse header. */
    tbmp_parse_head(buf + TBMP_MAGIC_LEN, &out->head);

    TBmpError ve = tbmp_validate_head(&out->head);
    if (ve != TBMP_OK)
        return ve;

    /* 4. Verify total buffer is large enough.
   *
   * Layout after magic+head:
   *   [DATA  : data_size  bytes]
   *   [EXTRA : extra_size bytes]
   *   [META  : meta_size  bytes]
   */
    int ov = 0;
    size_t body_start =
        TBMP_MIN_FILE_SIZE; /* = 4 (magic) + 26 (wire head) = 30 */
    size_t data_end =
        safe_add_size(body_start, (size_t)out->head.data_size, &ov);
    size_t extra_end =
        safe_add_size(data_end, (size_t)out->head.extra_size, &ov);
    size_t meta_end =
        safe_add_size(extra_end, (size_t)out->head.meta_size, &ov);

    if (ov)
        return TBMP_ERR_OVERFLOW;
    if (buf_len < meta_end)
        return TBMP_ERR_TRUNCATED;

    /* 5. Set section pointers. */
    out->data = buf + body_start;
    out->data_len = out->head.data_size;

    if (out->head.extra_size > 0) {
        out->extra = buf + data_end;
        out->extra_len = out->head.extra_size;
    } else {
        out->extra = NULL;
        out->extra_len = 0;
    }

    if (out->head.meta_size > 0) {
        out->meta = buf + extra_end;
        out->meta_len = out->head.meta_size;
    } else {
        out->meta = NULL;
        out->meta_len = 0;
    }

    /* 6. Parse EXTRA sub-chunks. */
    if (out->extra != NULL) {
        TBmpError e = parse_extra(out->extra, out->extra_len, out);
        if (e != TBMP_OK)
            return e;
    }

    /* 7. Cross-check flags vs actual chunks. */
    if ((out->head.flags & TBMP_FLAG_HAS_PALETTE) && !out->has_palette)
        return TBMP_ERR_NO_PALETTE;
    if ((out->head.flags & TBMP_FLAG_HAS_MASKS) && !out->has_masks)
        return TBMP_ERR_NO_MASKS;

    /* Indexed formats require a palette. */
    if (out->head.pixel_format <= (uint8_t)TBMP_FMT_INDEX_8 &&
        !out->has_palette) {
        return TBMP_ERR_NO_PALETTE;
    }

    /* CUSTOM format requires masks. */
    if (out->head.pixel_format == (uint8_t)TBMP_FMT_CUSTOM && !out->has_masks) {
        return TBMP_ERR_NO_MASKS;
    }

    return TBMP_OK;
}

/* Error strings. */

const char *tbmp_error_str(TBmpError err) {
    switch (err) {
    case TBMP_OK:
        return "OK";
    case TBMP_ERR_NULL_PTR:
        return "null pointer";
    case TBMP_ERR_BAD_MAGIC:
        return "bad magic (not a tBMP file)";
    case TBMP_ERR_BAD_VERSION:
        return "unsupported version";
    case TBMP_ERR_BAD_BIT_DEPTH:
        return "invalid bit_depth";
    case TBMP_ERR_BAD_ENCODING:
        return "unknown encoding mode";
    case TBMP_ERR_BAD_PIXEL_FORMAT:
        return "unknown pixel_format";
    case TBMP_ERR_BAD_DIMENSIONS:
        return "bad image dimensions";
    case TBMP_ERR_TRUNCATED:
        return "buffer truncated";
    case TBMP_ERR_OVERFLOW:
        return "size overflow";
    case TBMP_ERR_NO_PALETTE:
        return "indexed format without palette";
    case TBMP_ERR_NO_MASKS:
        return "custom format without masks";
    case TBMP_ERR_BAD_PALETTE:
        return "palette count too large";
    case TBMP_ERR_BAD_SPAN:
        return "span out of canvas bounds";
    case TBMP_ERR_BAD_BLOCK:
        return "block index out of range";
    case TBMP_ERR_BAD_PIXEL_COORD:
        return "sparse pixel coordinate out of range";
    case TBMP_ERR_OUT_OF_MEMORY:
        return "output buffer too small";
    case TBMP_ERR_ZERO_DIMENSIONS:
        return "zero width or height";
    case TBMP_ERR_INCONSISTENT:
        return "data_size inconsistent with encoding";
    default:
        return "unknown error";
    }
}
