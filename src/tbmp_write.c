#include "tbmp_write.h"
#include "tbmp_meta.h" /* tbmp_meta_encode */
#include "tbmp_pixel.h"
#include "tbmp_reader.h" /* tbmp_read_u16le / tbmp_read_u32le */

#include <limits.h>
#include <stdlib.h>
#include <string.h> /* memcpy, memset */

/* Internal write helpers. */

/* Portable bounded string length (bare-metal safe, no strnlen). */
static size_t wbuf_strnlen(const char *s, size_t max) {
    size_t n = 0;
    while (n < max && s[n] != '\0')
        n++;
    return n;
}

static int meta_has_content(const TBmpMeta *m) {
    return (m != NULL);
}

static size_t meta_estimated_size(const TBmpMeta *m) {
    if (!meta_has_content(m))
        return 0U;

    size_t sz = 5U; /* map header worst-case */

    /* required string fields */
    sz += 1U + 5U + wbuf_strnlen(m->title, TBMP_META_FIELD_MAX);
    sz += 1U + 6U + wbuf_strnlen(m->author, TBMP_META_FIELD_MAX);
    sz += 1U + 11U + wbuf_strnlen(m->description, TBMP_META_FIELD_MAX);
    sz += 1U + 7U + wbuf_strnlen(m->created, TBMP_META_FIELD_MAX);
    sz += 1U + 8U + wbuf_strnlen(m->software, TBMP_META_FIELD_MAX);
    sz += 1U + 7U + wbuf_strnlen(m->license, TBMP_META_FIELD_MAX);

    /* tags key + array + each tag string */
    sz += 1U + 4U;
    sz += 3U;
    for (uint32_t i = 0; i < m->tag_count; i++) {
        sz += 1U + wbuf_strnlen(m->tags[i], TBMP_META_TAG_MAX);
    }

    if (m->has_dpi) {
        sz += 1U + 3U;
        sz += 5U;
    }

    if (m->colorspace[0] != '\0') {
        sz += 1U + 10U + wbuf_strnlen(m->colorspace, TBMP_META_FIELD_MAX);
    }

    if (m->custom_count > 0) {
        sz += 1U + 6U;
        sz += 3U;
        for (uint32_t i = 0; i < m->custom_count; i++)
            sz += m->custom[i].len;
    }

    return sz;
}

/* Write buffer context - tracks position and remaining capacity. */
typedef struct WBuf {
    uint8_t *data;
    size_t pos;
    size_t cap;
} WBuf;

static int wbuf_write(WBuf *b, const void *src, size_t len) {
    if (len > b->cap - b->pos)
        return 0; /* would overflow */
    memcpy(b->data + b->pos, src, len);
    b->pos += len;
    return 1;
}

static int wbuf_write_u8(WBuf *b, uint8_t v) {
    return wbuf_write(b, &v, 1);
}

static int wbuf_write_u16le(WBuf *b, uint16_t v) __attribute__((unused));
static int wbuf_write_u16le(WBuf *b, uint16_t v) {
    uint8_t buf[2];
    buf[0] = (uint8_t)(v & 0xFFU);
    buf[1] = (uint8_t)((v >> 8) & 0xFFU);
    return wbuf_write(b, buf, 2);
}

static int wbuf_write_u32le(WBuf *b, uint32_t v) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(v & 0xFFU);
    buf[1] = (uint8_t)((v >> 8) & 0xFFU);
    buf[2] = (uint8_t)((v >> 16) & 0xFFU);
    buf[3] = (uint8_t)((v >> 24) & 0xFFU);
    return wbuf_write(b, buf, 4);
}

/* Reserve space and return a pointer to it; returns NULL on overflow. */
static uint8_t *wbuf_reserve(WBuf *b, size_t len) {
    if (len > b->cap - b->pos)
        return NULL;
    uint8_t *p = b->data + b->pos;
    b->pos += len;
    return p;
}

/* Section 1 - Pixel packing (RGBA8888 to target format). */

/*
 * rgba_to_packed - convert one RGBA pixel to the target pixel format and
 * return its raw bit value.
 */
static uint32_t rgba_to_packed(TBmpRGBA px, TBmpPixelFormat fmt,
                               const TBmpPalette *pal, const TBmpMasks *masks) {
    switch (fmt) {

    case TBMP_FMT_INDEX_1:
    case TBMP_FMT_INDEX_2:
    case TBMP_FMT_INDEX_4:
    case TBMP_FMT_INDEX_8: {
        /* Nearest palette entry by L1 distance in RGB space. */
        if (pal == NULL || pal->count == 0)
            return 0;
        uint32_t best_idx = 0;
        uint32_t best_dist = UINT32_MAX;
        for (uint32_t i = 0; i < pal->count; i++) {
            uint32_t dr = (px.r > pal->entries[i].r) ? px.r - pal->entries[i].r
                                                     : pal->entries[i].r - px.r;
            uint32_t dg = (px.g > pal->entries[i].g) ? px.g - pal->entries[i].g
                                                     : pal->entries[i].g - px.g;
            uint32_t db = (px.b > pal->entries[i].b) ? px.b - pal->entries[i].b
                                                     : pal->entries[i].b - px.b;
            uint32_t dist = dr + dg + db;
            if (dist < best_dist) {
                best_dist = dist;
                best_idx = i;
                if (dist == 0)
                    break;
            }
        }
        return best_idx;
    }

    case TBMP_FMT_RGB_565:
        return (((uint32_t)px.r >> 3) << 11) | (((uint32_t)px.g >> 2) << 5) |
               ((uint32_t)px.b >> 3);

    case TBMP_FMT_RGB_555:
        return (((uint32_t)px.r >> 3) << 10) | (((uint32_t)px.g >> 3) << 5) |
               ((uint32_t)px.b >> 3);

    case TBMP_FMT_RGB_444:
        return (((uint32_t)px.r >> 4) << 8) | (((uint32_t)px.g >> 4) << 4) |
               ((uint32_t)px.b >> 4);

    case TBMP_FMT_RGB_332:
        return ((uint32_t)(px.r & 0xE0U)) | (((uint32_t)px.g & 0xE0U) >> 3) |
               ((uint32_t)px.b >> 6);

    case TBMP_FMT_RGB_888:
        /* Wire bytes: [R, G, B] -> pack as R<<16|G<<8|B */
        return ((uint32_t)px.r << 16) | ((uint32_t)px.g << 8) | (uint32_t)px.b;

    case TBMP_FMT_RGBA_8888:
        /* Wire bytes: [R, G, B, A] -> pack as R<<24|G<<16|B<<8|A */
        return ((uint32_t)px.r << 24) | ((uint32_t)px.g << 16) |
               ((uint32_t)px.b << 8) | (uint32_t)px.a;

    case TBMP_FMT_CUSTOM: {
        if (masks == NULL)
            return 0;
        /* Find LSB position of each mask to determine shift amount. */
        uint32_t r_shift = 0, g_shift = 0, b_shift = 0, a_shift = 0;
        {
            uint32_t m;
            m = masks->r;
            while (m && !(m & 1U)) {
                r_shift++;
                m >>= 1;
            }
            m = masks->g;
            while (m && !(m & 1U)) {
                g_shift++;
                m >>= 1;
            }
            m = masks->b;
            while (m && !(m & 1U)) {
                b_shift++;
                m >>= 1;
            }
            m = masks->a;
            while (m && !(m & 1U)) {
                a_shift++;
                m >>= 1;
            }
        }
        uint32_t v = 0;
        if (masks->r)
            v |= (((uint32_t)px.r) << r_shift) & masks->r;
        if (masks->g)
            v |= (((uint32_t)px.g) << g_shift) & masks->g;
        if (masks->b)
            v |= (((uint32_t)px.b) << b_shift) & masks->b;
        if (masks->a)
            v |= (((uint32_t)px.a) << a_shift) & masks->a;
        return v;
    }

    default:
        return 0;
    }
}

/* Pack a uint32 raw value into a byte buffer using MSB-first bit packing. */
static int pack_pixel(uint8_t *buf, size_t buf_cap, uint32_t pixel_idx,
                      uint8_t bit_depth, uint32_t value) {
    switch (bit_depth) {
    case 1: {
        size_t byte_idx = pixel_idx >> 3;
        uint32_t bit_off = 7U - (pixel_idx & 7U);
        if (byte_idx >= buf_cap)
            return 0;
        buf[byte_idx] &= ~(1U << bit_off);
        buf[byte_idx] |= (value & 1U) << bit_off;
        return 1;
    }
    case 2: {
        size_t byte_idx = pixel_idx >> 2;
        uint32_t bit_off = (3U - (pixel_idx & 3U)) * 2U;
        if (byte_idx >= buf_cap)
            return 0;
        buf[byte_idx] &= ~(0x03U << bit_off);
        buf[byte_idx] |= (value & 0x03U) << bit_off;
        return 1;
    }
    case 4: {
        size_t byte_idx = pixel_idx >> 1;
        if (byte_idx >= buf_cap)
            return 0;
        if (pixel_idx & 1U) {
            buf[byte_idx] = (buf[byte_idx] & 0xF0U) | (value & 0x0FU);
        } else {
            buf[byte_idx] = (buf[byte_idx] & 0x0FU) | ((value & 0x0FU) << 4);
        }
        return 1;
    }
    case 8: {
        if (pixel_idx >= buf_cap)
            return 0;
        buf[pixel_idx] = (uint8_t)(value & 0xFFU);
        return 1;
    }
    case 16: {
        size_t off = (size_t)pixel_idx * 2;
        if (off + 2 > buf_cap)
            return 0;
        buf[off] = (uint8_t)(value & 0xFFU);
        buf[off + 1] = (uint8_t)((value >> 8) & 0xFFU);
        return 1;
    }
    case 24: {
        size_t off = (size_t)pixel_idx * 3;
        if (off + 3 > buf_cap)
            return 0;
        /* value is R<<16|G<<8|B; emit as [R, G, B] */
        buf[off] = (uint8_t)((value >> 16) & 0xFFU);
        buf[off + 1] = (uint8_t)((value >> 8) & 0xFFU);
        buf[off + 2] = (uint8_t)(value & 0xFFU);
        return 1;
    }
    case 32: {
        size_t off = (size_t)pixel_idx * 4;
        if (off + 4 > buf_cap)
            return 0;
        /* value is R<<24|G<<16|B<<8|A; emit as [R, G, B, A] */
        buf[off] = (uint8_t)((value >> 24) & 0xFFU);
        buf[off + 1] = (uint8_t)((value >> 16) & 0xFFU);
        buf[off + 2] = (uint8_t)((value >> 8) & 0xFFU);
        buf[off + 3] = (uint8_t)(value & 0xFFU);
        return 1;
    }
    default:
        return 0;
    }
}

/* Section 2 - Encoding modes. */

/*
 * value_field_bytes - bytes needed for an inline pixel value in RLE/Span/
 * SparsePixel.  Mirrors the identical helper in tbmp_decode.c.
 */
static uint8_t value_field_bytes(uint8_t bit_depth) {
    if (bit_depth <= 8U)
        return 1U;
    if (bit_depth == 16U)
        return 2U;
    if (bit_depth == 24U)
        return 3U;
    return 4U; /* 32 */
}

/*
 * write_value_field - emit a multi-byte pixel value field.
 *
 * 1-byte: plain u8.
 * 2-byte: LE u16 (matching the 16-bit pixel convention).
 * 3-byte: big-endian natural [R, G, B] (R<<16|G<<8|B).
 * 4-byte: big-endian natural [R, G, B, A] (R<<24|G<<16|B<<8|A).
 */
static int write_value_field(WBuf *b, uint32_t val, uint8_t nbytes) {
    switch (nbytes) {
    case 1: {
        uint8_t v = (uint8_t)(val & 0xFFU);
        return wbuf_write(b, &v, 1);
    }
    case 2: {
        uint8_t buf[2];
        buf[0] = (uint8_t)(val & 0xFFU);
        buf[1] = (uint8_t)((val >> 8) & 0xFFU);
        return wbuf_write(b, buf, 2);
    }
    case 3: {
        uint8_t buf[3];
        buf[0] = (uint8_t)((val >> 16) & 0xFFU);
        buf[1] = (uint8_t)((val >> 8) & 0xFFU);
        buf[2] = (uint8_t)(val & 0xFFU);
        return wbuf_write(b, buf, 3);
    }
    default: {
        uint8_t buf[4];
        buf[0] = (uint8_t)((val >> 24) & 0xFFU);
        buf[1] = (uint8_t)((val >> 16) & 0xFFU);
        buf[2] = (uint8_t)((val >> 8) & 0xFFU);
        buf[3] = (uint8_t)(val & 0xFFU);
        return wbuf_write(b, buf, 4);
    }
    }
}

/* Compute packed DATA byte size for n pixels at bit_depth (RAW). */
static size_t raw_data_size(uint32_t n, uint8_t bit_depth) {
    /* Ceiling division for sub-byte depths. */
    uint64_t bits = (uint64_t)n * bit_depth;
    return (size_t)((bits + 7U) / 8U);
}

/*
 * rle_max_data_size - worst-case RLE byte count for n pixels.
 *
 * Each entry is 1 (count) + vbytes (value).  In the worst case every
 * pixel is a unique run of length 1.
 */
static size_t rle_max_data_size(uint32_t n, uint8_t bit_depth) {
    uint8_t vbytes = value_field_bytes(bit_depth);
    uint64_t per_run = 1U + vbytes;
    uint64_t total = (uint64_t)n * per_run;
    return (total > (size_t)-1) ? (size_t)-1 : (size_t)total;
}

static size_t estimate_rle_size(const TBmpFrame *frame,
                                const TBmpWriteParams *params) {
    uint32_t n = (uint32_t)frame->width * frame->height;
    uint8_t vbytes = value_field_bytes(params->bit_depth);
    size_t total = 0;

    uint32_t i = 0;
    while (i < n) {
        uint32_t val = rgba_to_packed(frame->pixels[i], params->pixel_format,
                                      params->palette, params->masks);
        uint32_t run = 1;
        while (run < 255U && i + run < n) {
            uint32_t next =
                rgba_to_packed(frame->pixels[i + run], params->pixel_format,
                               params->palette, params->masks);
            if (next != val)
                break;
            run++;
        }
        total += 1U + vbytes;
        i += run;
    }

    return total;
}

/* Forward declaration for block-sparse tile packing reuse. */
static size_t encode_raw(const TBmpFrame *frame, const TBmpWriteParams *params,
                         uint8_t *out, size_t cap);

/* Encode Zero-Range (mode 1). Returns bytes written or 0 on error. */
static size_t encode_zero_range(const TBmpFrame *frame,
                                const TBmpWriteParams *params, uint8_t *out,
                                size_t cap) {
    uint32_t n = (uint32_t)frame->width * frame->height;
    if (params->bit_depth > 8U)
        return 0;

    uint8_t *vals = (uint8_t *)malloc(n ? n : 1U);
    if (!vals)
        return 0;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t v = rgba_to_packed(frame->pixels[i], params->pixel_format,
                                    params->palette, params->masks);
        vals[i] = (uint8_t)(v & 0xFFU);
    }

    uint32_t zero_ranges = 0;
    uint32_t nonzero = 0;
    for (uint32_t i = 0; i < n;) {
        if (vals[i] == 0) {
            zero_ranges++;
            while (i < n && vals[i] == 0)
                i++;
        } else {
            nonzero++;
            i++;
        }
    }

    size_t needed = 4U + (size_t)zero_ranges * 8U + 4U + nonzero;
    if (needed > cap) {
        free(vals);
        return 0;
    }

    WBuf wb = {out, 0, cap};
    if (!wbuf_write_u32le(&wb, zero_ranges)) {
        free(vals);
        return 0;
    }

    for (uint32_t i = 0; i < n;) {
        if (vals[i] == 0) {
            uint32_t start = i;
            while (i < n && vals[i] == 0)
                i++;
            if (!wbuf_write_u32le(&wb, start) ||
                !wbuf_write_u32le(&wb, i - start)) {
                free(vals);
                return 0;
            }
        } else {
            i++;
        }
    }

    if (!wbuf_write_u32le(&wb, nonzero)) {
        free(vals);
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (vals[i] != 0 && !wbuf_write_u8(&wb, vals[i])) {
            free(vals);
            return 0;
        }
    }

    free(vals);
    return wb.pos;
}

/* Encode Span (mode 3). Returns bytes written or 0 on error. */
static size_t encode_span(const TBmpFrame *frame, const TBmpWriteParams *params,
                          uint8_t *out, size_t cap) {
    uint32_t n = (uint32_t)frame->width * frame->height;
    uint8_t vbytes = value_field_bytes(params->bit_depth);

    uint32_t *vals = (uint32_t *)malloc((n ? n : 1U) * sizeof(uint32_t));
    if (!vals)
        return 0;

    for (uint32_t i = 0; i < n; i++) {
        vals[i] = rgba_to_packed(frame->pixels[i], params->pixel_format,
                                 params->palette, params->masks);
    }

    uint32_t spans = 0;
    for (uint32_t i = 0; i < n;) {
        uint32_t v = vals[i];
        uint32_t start = i;
        while (i < n && vals[i] == v)
            i++;
        if (v != 0U && i > start)
            spans++;
    }

    size_t needed = 4U + (size_t)spans * (8U + vbytes);
    if (needed > cap) {
        free(vals);
        return 0;
    }

    WBuf wb = {out, 0, cap};
    if (!wbuf_write_u32le(&wb, spans)) {
        free(vals);
        return 0;
    }

    for (uint32_t i = 0; i < n;) {
        uint32_t v = vals[i];
        uint32_t start = i;
        while (i < n && vals[i] == v)
            i++;
        uint32_t len = i - start;
        if (v == 0U)
            continue;

        if (!wbuf_write_u32le(&wb, start) || !wbuf_write_u32le(&wb, len) ||
            !write_value_field(&wb, v, vbytes)) {
            free(vals);
            return 0;
        }
    }

    free(vals);
    return wb.pos;
}

/* Encode Sparse Pixel (mode 4). Returns bytes written or 0 on error. */
static size_t encode_sparse_pixel(const TBmpFrame *frame,
                                  const TBmpWriteParams *params, uint8_t *out,
                                  size_t cap) {
    uint8_t vbytes = value_field_bytes(params->bit_depth);

    uint32_t count = 0;
    for (uint32_t y = 0; y < frame->height; y++) {
        for (uint32_t x = 0; x < frame->width; x++) {
            uint32_t idx = y * frame->width + x;
            uint32_t v =
                rgba_to_packed(frame->pixels[idx], params->pixel_format,
                               params->palette, params->masks);
            if (v != 0U)
                count++;
        }
    }

    size_t needed = 4U + (size_t)count * (4U + vbytes);
    if (needed > cap)
        return 0;

    WBuf wb = {out, 0, cap};
    if (!wbuf_write_u32le(&wb, count))
        return 0;

    for (uint32_t y = 0; y < frame->height; y++) {
        for (uint32_t x = 0; x < frame->width; x++) {
            uint32_t idx = y * frame->width + x;
            uint32_t v =
                rgba_to_packed(frame->pixels[idx], params->pixel_format,
                               params->palette, params->masks);
            if (v == 0U)
                continue;

            if (!wbuf_write_u16le(&wb, (uint16_t)x) ||
                !wbuf_write_u16le(&wb, (uint16_t)y) ||
                !write_value_field(&wb, v, vbytes)) {
                return 0;
            }
        }
    }

    return wb.pos;
}

/* Encode Block Sparse (mode 5). Returns bytes written or 0 on error. */
static size_t encode_block_sparse(const TBmpFrame *frame,
                                  const TBmpWriteParams *params, uint8_t *out,
                                  size_t cap) {
    const uint16_t bw = 8U;
    const uint16_t bh = 8U;
    uint32_t tiles_x = ((uint32_t)frame->width + bw - 1U) / bw;
    uint32_t tiles_y = ((uint32_t)frame->height + bh - 1U) / bh;
    uint32_t total_tiles = tiles_x * tiles_y;
    size_t block_data_size =
        raw_data_size((uint32_t)bw * bh, params->bit_depth);

    TBmpRGBA tile_px[64];
    uint8_t tile_data[256];

    uint32_t non_empty_blocks = 0;
    for (uint32_t t = 0; t < total_tiles; t++) {
        uint32_t tile_col = t % tiles_x;
        uint32_t tile_row = t / tiles_x;
        uint32_t ox = tile_col * bw;
        uint32_t oy = tile_row * bh;

        for (uint32_t ty = 0; ty < bh; ty++) {
            for (uint32_t tx = 0; tx < bw; tx++) {
                uint32_t di = ty * bw + tx;
                uint32_t sx = ox + tx;
                uint32_t sy = oy + ty;
                if (sx < frame->width && sy < frame->height) {
                    tile_px[di] = frame->pixels[sy * frame->width + sx];
                } else {
                    tile_px[di] = (TBmpRGBA){0, 0, 0, 0};
                }
            }
        }

        TBmpFrame tile = {bw, bh, tile_px};
        size_t w = encode_raw(&tile, params, tile_data, sizeof(tile_data));
        if (w != block_data_size)
            return 0;

        int any = 0;
        for (size_t i = 0; i < block_data_size; i++) {
            if (tile_data[i] != 0U) {
                any = 1;
                break;
            }
        }
        if (any)
            non_empty_blocks++;
    }

    size_t needed = 8U + (size_t)non_empty_blocks * (8U + block_data_size);
    if (needed > cap)
        return 0;

    WBuf wb = {out, 0, cap};
    if (!wbuf_write_u16le(&wb, bw) || !wbuf_write_u16le(&wb, bh) ||
        !wbuf_write_u32le(&wb, non_empty_blocks)) {
        return 0;
    }

    for (uint32_t t = 0; t < total_tiles; t++) {
        uint32_t tile_col = t % tiles_x;
        uint32_t tile_row = t / tiles_x;
        uint32_t ox = tile_col * bw;
        uint32_t oy = tile_row * bh;

        for (uint32_t ty = 0; ty < bh; ty++) {
            for (uint32_t tx = 0; tx < bw; tx++) {
                uint32_t di = ty * bw + tx;
                uint32_t sx = ox + tx;
                uint32_t sy = oy + ty;
                if (sx < frame->width && sy < frame->height) {
                    tile_px[di] = frame->pixels[sy * frame->width + sx];
                } else {
                    tile_px[di] = (TBmpRGBA){0, 0, 0, 0};
                }
            }
        }

        TBmpFrame tile = {bw, bh, tile_px};
        size_t w = encode_raw(&tile, params, tile_data, sizeof(tile_data));
        if (w != block_data_size)
            return 0;

        int any = 0;
        for (size_t i = 0; i < block_data_size; i++) {
            if (tile_data[i] != 0U) {
                any = 1;
                break;
            }
        }
        if (!any)
            continue;

        if (!wbuf_write_u32le(&wb, t) ||
            !wbuf_write_u32le(&wb, (uint32_t)block_data_size) ||
            !wbuf_write(&wb, tile_data, block_data_size)) {
            return 0;
        }
    }

    return wb.pos;
}

/* Encode a RAW pixel array.  Returns bytes written or 0 on error. */
static size_t encode_raw(const TBmpFrame *frame, const TBmpWriteParams *params,
                         uint8_t *out, size_t cap) {
    uint32_t n = (uint32_t)frame->width * frame->height;
    size_t dlen = raw_data_size(n, params->bit_depth);
    if (dlen > cap)
        return 0;

    memset(out, 0, dlen);

    for (uint32_t i = 0; i < n; i++) {
        uint32_t packed = rgba_to_packed(frame->pixels[i], params->pixel_format,
                                         params->palette, params->masks);
        if (!pack_pixel(out, dlen, i, params->bit_depth, packed))
            return 0;
    }
    return dlen;
}

/* Encode RLE.  Returns bytes written or 0 on error. */
static size_t encode_rle(const TBmpFrame *frame, const TBmpWriteParams *params,
                         uint8_t *out, size_t cap) {
    uint32_t n = (uint32_t)frame->width * frame->height;
    uint8_t vbytes = value_field_bytes(params->bit_depth);
    size_t pos = 0;

    /* Wrap the output slice in a WBuf for write_value_field. */
    WBuf wb;
    wb.data = out;
    wb.pos = 0;
    wb.cap = cap;

    uint32_t i = 0;
    while (i < n) {
        uint32_t val = rgba_to_packed(frame->pixels[i], params->pixel_format,
                                      params->palette, params->masks);

        /* Count run (max 255 per RLE entry).
     * Comparison uses the full raw packed value so wide pixels are handled
     * correctly (no 8-bit truncation). */
        uint32_t run = 1;
        while (run < 255U && i + run < n) {
            uint32_t next =
                rgba_to_packed(frame->pixels[i + run], params->pixel_format,
                               params->palette, params->masks);
            if (next != val)
                break;
            run++;
        }

        /* 1 byte count + vbytes value. */
        if (wb.pos + 1U + vbytes > wb.cap)
            return 0;
        if (!wbuf_write_u8(&wb, (uint8_t)run))
            return 0;
        if (!write_value_field(&wb, val, vbytes))
            return 0;

        i += run;
    }

    (void)pos; /* pos unused; wb.pos tracks progress */
    return wb.pos;
}

/* Section 3 - EXTRA chunks. */

/* Write a PALT chunk to wbuf.  Returns 1 on success, 0 on overflow. */
static int write_palt(WBuf *b, const TBmpPalette *pal) {
    /* tag */
    if (!wbuf_write(b, "PALT", 4))
        return 0;
    /* length = 4 (count) + count*4 */
    uint32_t body_len = 4U + pal->count * 4U;
    if (!wbuf_write_u32le(b, body_len))
        return 0;
    if (!wbuf_write_u32le(b, pal->count))
        return 0;
    for (uint32_t i = 0; i < pal->count; i++) {
        if (!wbuf_write_u8(b, pal->entries[i].r))
            return 0;
        if (!wbuf_write_u8(b, pal->entries[i].g))
            return 0;
        if (!wbuf_write_u8(b, pal->entries[i].b))
            return 0;
        if (!wbuf_write_u8(b, pal->entries[i].a))
            return 0;
    }
    return 1;
}

/* Write a MASK chunk to wbuf.  Returns 1 on success, 0 on overflow. */
static int write_mask(WBuf *b, const TBmpMasks *masks) {
    if (!wbuf_write(b, "MASK", 4))
        return 0;
    if (!wbuf_write_u32le(b, 16U)) /* 4 * u32 */
        return 0;
    if (!wbuf_write_u32le(b, masks->r))
        return 0;
    if (!wbuf_write_u32le(b, masks->g))
        return 0;
    if (!wbuf_write_u32le(b, masks->b))
        return 0;
    if (!wbuf_write_u32le(b, masks->a))
        return 0;
    return 1;
}

/* Section 4 - Public API. */
void tbmp_write_default_params(TBmpWriteParams *p) {
    if (p == NULL)
        return;
    p->encoding = TBMP_ENC_RAW;
    p->pixel_format = TBMP_FMT_RGBA_8888;
    p->bit_depth = 32;
    p->palette = NULL;
    p->masks = NULL;
    p->meta = NULL;
    p->version = TBMP_VERSION_1_0;
}

size_t tbmp_write_needed_size(const TBmpFrame *frame,
                              const TBmpWriteParams *params) {
    if (frame == NULL || params == NULL)
        return 0;
    if (frame->width == 0 || frame->height == 0)
        return 0;

    /* Overflow-safe product. */
    if ((uint32_t)frame->width > UINT32_MAX / frame->height)
        return 0;
    uint32_t n = (uint32_t)frame->width * frame->height;

    /* Data size upper bound by mode. */
    uint8_t vbytes = value_field_bytes(params->bit_depth);
    uint64_t data_max_u64;
    switch (params->encoding) {
    case TBMP_ENC_RLE:
        data_max_u64 = (uint64_t)rle_max_data_size(n, params->bit_depth);
        break;
    case TBMP_ENC_ZERO_RANGE:
        /* 4 + ranges(8 each) + 4 + values(1 each), worst-case ranges==n */
        data_max_u64 = 8U + (uint64_t)n * 9U;
        break;
    case TBMP_ENC_SPAN:
        data_max_u64 = 4U + (uint64_t)n * (8U + vbytes);
        break;
    case TBMP_ENC_SPARSE_PIXEL:
        data_max_u64 = 4U + (uint64_t)n * (4U + vbytes);
        break;
    case TBMP_ENC_BLOCK_SPARSE: {
        uint32_t tiles_x = ((uint32_t)frame->width + 7U) / 8U;
        uint32_t tiles_y = ((uint32_t)frame->height + 7U) / 8U;
        uint64_t blocks = (uint64_t)tiles_x * (uint64_t)tiles_y;
        uint64_t block_data = raw_data_size(64U, params->bit_depth);
        data_max_u64 = 8U + blocks * (8U + block_data);
    } break;
    default: {
        uint64_t bits = (uint64_t)n * params->bit_depth;
        data_max_u64 = (bits + 7U) / 8U;
        break;
    }
    }
    if (data_max_u64 > (uint64_t)SIZE_MAX)
        return 0;
    size_t data_max = (size_t)data_max_u64;

    /* EXTRA size. */
    size_t extra = 0;
    if (params->palette)
        extra += 8U + 4U + params->palette->count * 4U;
    if (params->masks)
        extra += 8U + 16U;

    size_t meta = meta_estimated_size(params->meta);

    /* 4 (magic) + 26 (wire head) + data_max + extra + meta */
    size_t total = TBMP_MIN_FILE_SIZE + data_max + extra + meta;
    if (total < TBMP_MIN_FILE_SIZE)
        return 0; /* overflow */
    return total;
}

TBmpError tbmp_write(const TBmpFrame *frame, const TBmpWriteParams *params,
                     uint8_t *out, size_t out_cap, size_t *out_len) {
    if (frame == NULL || params == NULL || out == NULL || out_len == NULL)
        return TBMP_ERR_NULL_PTR;
    if (frame->pixels == NULL)
        return TBMP_ERR_NULL_PTR;
    if (frame->width == 0 || frame->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;

    *out_len = 0;

    WBuf b;
    b.data = out;
    b.pos = 0;
    b.cap = out_cap;

    /* 1. Magic. */
    if (!wbuf_write(&b, TBMP_MAGIC, TBMP_MAGIC_LEN))
        return TBMP_ERR_OUT_OF_MEMORY;

    /* 2. Reserve header slot (filled after we know the section sizes). */
    size_t head_pos = b.pos;
    uint8_t *head_slot = wbuf_reserve(&b, TBMP_HEAD_WIRE_SIZE);
    if (head_slot == NULL)
        return TBMP_ERR_OUT_OF_MEMORY;

    /* 3. Encode DATA. */
    size_t data_start = b.pos;
    uint32_t n = (uint32_t)frame->width * frame->height;

    /* Pre-compute max data size and verify capacity. */
    uint8_t vbytes = value_field_bytes(params->bit_depth);
    uint64_t data_max_u64;
    switch (params->encoding) {
    case TBMP_ENC_RLE:
        data_max_u64 = (uint64_t)rle_max_data_size(n, params->bit_depth);
        break;
    case TBMP_ENC_ZERO_RANGE:
        data_max_u64 = 8U + (uint64_t)n * 9U;
        break;
    case TBMP_ENC_SPAN:
        data_max_u64 = 4U + (uint64_t)n * (8U + vbytes);
        break;
    case TBMP_ENC_SPARSE_PIXEL:
        data_max_u64 = 4U + (uint64_t)n * (4U + vbytes);
        break;
    case TBMP_ENC_BLOCK_SPARSE: {
        uint32_t tiles_x = ((uint32_t)frame->width + 7U) / 8U;
        uint32_t tiles_y = ((uint32_t)frame->height + 7U) / 8U;
        uint64_t blocks = (uint64_t)tiles_x * (uint64_t)tiles_y;
        uint64_t block_data = raw_data_size(64U, params->bit_depth);
        data_max_u64 = 8U + blocks * (8U + block_data);
    } break;
    default: {
        uint64_t bits = (uint64_t)n * params->bit_depth;
        data_max_u64 = (bits + 7U) / 8U;
        break;
    }
    }
    if (data_max_u64 > (uint64_t)SIZE_MAX)
        return TBMP_ERR_OVERFLOW;
    size_t data_max = (size_t)data_max_u64;

    if (data_max > b.cap - b.pos)
        return TBMP_ERR_OUT_OF_MEMORY;

    size_t data_written;
    switch (params->encoding) {
    case TBMP_ENC_ZERO_RANGE:
        data_written =
            encode_zero_range(frame, params, b.data + b.pos, b.cap - b.pos);
        break;
    case TBMP_ENC_RLE:
        data_written = encode_rle(frame, params, b.data + b.pos, b.cap - b.pos);
        break;
    case TBMP_ENC_SPAN:
        data_written =
            encode_span(frame, params, b.data + b.pos, b.cap - b.pos);
        break;
    case TBMP_ENC_SPARSE_PIXEL:
        data_written =
            encode_sparse_pixel(frame, params, b.data + b.pos, b.cap - b.pos);
        break;
    case TBMP_ENC_BLOCK_SPARSE:
        data_written =
            encode_block_sparse(frame, params, b.data + b.pos, b.cap - b.pos);
        break;
    default:
        data_written = encode_raw(frame, params, b.data + b.pos, b.cap - b.pos);
        break;
    }

    if (data_written == 0 && n > 0)
        return TBMP_ERR_OUT_OF_MEMORY;
    b.pos += data_written;
    uint32_t data_size = (uint32_t)data_written;

    /* 4. Encode EXTRA. */
    size_t extra_start = b.pos;

    if (params->palette != NULL) {
        if (!write_palt(&b, params->palette))
            return TBMP_ERR_OUT_OF_MEMORY;
    }
    if (params->masks != NULL) {
        if (!write_mask(&b, params->masks))
            return TBMP_ERR_OUT_OF_MEMORY;
    }

    uint32_t extra_size = (uint32_t)(b.pos - extra_start);

    /* 5. META. */
    size_t meta_start = b.pos;
    if (meta_has_content(params->meta)) {
        size_t meta_written = 0;
        int rc = tbmp_meta_encode(params->meta, b.data + b.pos, b.cap - b.pos,
                                  &meta_written);
        if (rc == TBMP_ERR_OUT_OF_MEMORY)
            return TBMP_ERR_OUT_OF_MEMORY;
        if (rc != TBMP_OK)
            return TBMP_ERR_INCONSISTENT;
        b.pos += meta_written;
    }
    uint32_t meta_size = (uint32_t)(b.pos - meta_start);

    /* 6. Fill in header. */
    {
        uint8_t flags = 0;
        if (params->palette)
            flags |= TBMP_FLAG_HAS_PALETTE;
        if (params->masks)
            flags |= TBMP_FLAG_HAS_MASKS;

        uint8_t *p = head_slot;
        /* version */
        p[0] = (uint8_t)(params->version & 0xFFU);
        p[1] = (uint8_t)((params->version >> 8) & 0xFFU);
        p += 2;
        /* width */
        p[0] = (uint8_t)(frame->width & 0xFFU);
        p[1] = (uint8_t)((frame->width >> 8) & 0xFFU);
        p += 2;
        /* height */
        p[0] = (uint8_t)(frame->height & 0xFFU);
        p[1] = (uint8_t)((frame->height >> 8) & 0xFFU);
        p += 2;
        /* bit_depth, encoding, pixel_format, flags */
        p[0] = params->bit_depth;
        p[1] = (uint8_t)params->encoding;
        p[2] = (uint8_t)params->pixel_format;
        p[3] = flags;
        p += 4;
        /* data_size */
        p[0] = (uint8_t)(data_size & 0xFFU);
        p[1] = (uint8_t)((data_size >> 8) & 0xFFU);
        p[2] = (uint8_t)((data_size >> 16) & 0xFFU);
        p[3] = (uint8_t)((data_size >> 24) & 0xFFU);
        p += 4;
        /* extra_size */
        p[0] = (uint8_t)(extra_size & 0xFFU);
        p[1] = (uint8_t)((extra_size >> 8) & 0xFFU);
        p[2] = (uint8_t)((extra_size >> 16) & 0xFFU);
        p[3] = (uint8_t)((extra_size >> 24) & 0xFFU);
        p += 4;
        /* meta_size */
        p[0] = (uint8_t)(meta_size & 0xFFU);
        p[1] = (uint8_t)((meta_size >> 8) & 0xFFU);
        p[2] = (uint8_t)((meta_size >> 16) & 0xFFU);
        p[3] = (uint8_t)((meta_size >> 24) & 0xFFU);
        p += 4;
        /* reserved */
        p[0] = p[1] = p[2] = p[3] = 0;
    }
    (void)head_pos;
    (void)data_start;

    *out_len = b.pos;
    return TBMP_OK;
}

TBmpEncoding tbmp_pick_best_encoding(const TBmpFrame *frame,
                                     const TBmpWriteParams *params) {
    if (frame == NULL || params == NULL || frame->pixels == NULL)
        return TBMP_ENC_RAW;
    if (frame->width == 0 || frame->height == 0)
        return TBMP_ENC_RAW;

    uint32_t n = (uint32_t)frame->width * frame->height;
    size_t raw_sz = raw_data_size(n, params->bit_depth);
    size_t rle_sz = estimate_rle_size(frame, params);

    if (rle_sz < raw_sz)
        return TBMP_ENC_RLE;
    return TBMP_ENC_RAW;
}

TBmpError tbmp_auto_palette_from_frame(const TBmpFrame *frame,
                                       uint32_t max_colors, TBmpPalette *out) {
    if (frame == NULL || out == NULL || frame->pixels == NULL)
        return TBMP_ERR_NULL_PTR;
    if (frame->width == 0 || frame->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;
    if (max_colors == 0 || max_colors > TBMP_MAX_PALETTE)
        return TBMP_ERR_BAD_PALETTE;

    uint32_t count[32768];
    uint32_t sum_r[32768];
    uint32_t sum_g[32768];
    uint32_t sum_b[32768];
    memset(count, 0, sizeof(count));
    memset(sum_r, 0, sizeof(sum_r));
    memset(sum_g, 0, sizeof(sum_g));
    memset(sum_b, 0, sizeof(sum_b));

    uint32_t n = (uint32_t)frame->width * frame->height;
    for (uint32_t i = 0; i < n; i++) {
        TBmpRGBA p = frame->pixels[i];
        uint32_t idx = ((uint32_t)(p.r >> 3) << 10) |
                       ((uint32_t)(p.g >> 3) << 5) | (uint32_t)(p.b >> 3);
        count[idx]++;
        sum_r[idx] += p.r;
        sum_g[idx] += p.g;
        sum_b[idx] += p.b;
    }

    out->count = 0;
    for (uint32_t k = 0; k < max_colors; k++) {
        uint32_t best = 0;
        uint32_t best_count = 0;
        for (uint32_t i = 0; i < 32768U; i++) {
            if (count[i] > best_count) {
                best_count = count[i];
                best = i;
            }
        }
        if (best_count == 0)
            break;

        out->entries[out->count].r = (uint8_t)(sum_r[best] / best_count);
        out->entries[out->count].g = (uint8_t)(sum_g[best] / best_count);
        out->entries[out->count].b = (uint8_t)(sum_b[best] / best_count);
        out->entries[out->count].a = 255;
        out->count++;
        count[best] = 0;
    }

    if (out->count == 0)
        return TBMP_ERR_BAD_PALETTE;

    while (out->count < max_colors) {
        out->entries[out->count] = out->entries[0];
        out->count++;
    }

    return TBMP_OK;
}
