#include "tbmp_decode.h"
#include "tbmp_pixel.h"
#include "tbmp_reader.h" /* tbmp_read_u16le / tbmp_read_u32le */

#include <limits.h> /* SIZE_MAX */
#include <string.h> /* memset */

/* Wire-value helpers. */

/* Sentinel value used internally by zero-range decoding to mark pixels
 * that need to be filled from the non-zero values array. 0xFE is used
 * because it's unlikely to appear as a real alpha value in normal images. */
#define ZR_SENTINEL 0xFEU

/*
 * read_value_field - read a multi-byte value field from the wire.
 *
 * For 1-byte fields the value is simply the byte.
 * For 2-byte fields it is a LE u16 (matching the 16-bit pixel convention).
 * For 3- and 4-byte fields the bytes are in big-endian natural order
 * (R<<16|G<<8|B and R<<24|G<<16|B<<8|A) to match extract_pixel_raw().
 *
 * p      : pointer to the first byte of the field.
 * nbytes : number of bytes to read (1..4).
 * Returns the decoded raw pixel value.
 */
static uint32_t read_value_field(const uint8_t *p, uint8_t nbytes) {
    switch (nbytes) {
    case 1:
        return (uint32_t)p[0];
    case 2:
        return tbmp_read_u16le(p);
    case 3:
        return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    default:
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
}

/* Raw bit extraction. */

/*
 * extract_pixel_raw - extract one pixel value from a packed bit buffer.
 *
 * Pixels are packed MSB-first within each byte for sub-byte depths (1, 2,
 * 4), matching the top-down orientation of tBMP.  For multi-byte depths
 * (16, 24, 32) the value is assembled in natural big-endian order so that
 * the returned uint32_t matches the convention expected by
 * tbmp_pixel_to_rgba():
 *
 *   depth 24: v = R<<16 | G<<8 | B
 *   depth 32: v = R<<24 | G<<16 | B<<8 | A
 *
 * buf       : packed pixel data.
 * buf_len   : length of buf in bytes.
 * pixel_idx : zero-based pixel index.
 * bit_depth : bits per pixel: 1, 2, 4, 8, 16, 24, or 32.
 * out       : receives the raw pixel value.
 * Returns TBMP_OK, or TBMP_ERR_TRUNCATED / TBMP_ERR_BAD_BIT_DEPTH.
 */
static TBmpError extract_pixel_raw(const uint8_t *buf, size_t buf_len,
                                   uint32_t pixel_idx, uint8_t bit_depth,
                                   uint32_t *out) {
    switch (bit_depth) {

    case 1: {
        uint32_t byte_idx = pixel_idx >> 3;
        uint32_t bit_off = 7U - (pixel_idx & 7U); /* MSB-first */
        if (byte_idx >= buf_len)
            return TBMP_ERR_TRUNCATED;
        *out = (buf[byte_idx] >> bit_off) & 0x01U;
        return TBMP_OK;
    }

    case 2: {
        uint32_t byte_idx = pixel_idx >> 2;
        uint32_t bit_off = (3U - (pixel_idx & 3U)) * 2U;
        if (byte_idx >= buf_len)
            return TBMP_ERR_TRUNCATED;
        *out = (buf[byte_idx] >> bit_off) & 0x03U;
        return TBMP_OK;
    }

    case 4: {
        uint32_t byte_idx = pixel_idx >> 1;
        if (byte_idx >= buf_len)
            return TBMP_ERR_TRUNCATED;
        /* High nibble for even pixels, low nibble for odd pixels. */
        *out = (pixel_idx & 1U) ? (buf[byte_idx] & 0x0FU)
                                : ((buf[byte_idx] >> 4) & 0x0FU);
        return TBMP_OK;
    }

    case 8: {
        if (pixel_idx >= buf_len)
            return TBMP_ERR_TRUNCATED;
        *out = buf[pixel_idx];
        return TBMP_OK;
    }

    case 16: {
        size_t off = (size_t)pixel_idx * 2U;
        if (off + 2U > buf_len)
            return TBMP_ERR_TRUNCATED;
        *out = tbmp_read_u16le(buf + off);
        return TBMP_OK;
    }

    case 24: {
        size_t off = (size_t)pixel_idx * 3U;
        if (off + 3U > buf_len)
            return TBMP_ERR_TRUNCATED;
        /* Wire order: [R, G, B] -> R<<16|G<<8|B */
        *out = ((uint32_t)buf[off + 0] << 16) | ((uint32_t)buf[off + 1] << 8) |
               (uint32_t)buf[off + 2];
        return TBMP_OK;
    }

    case 32: {
        size_t off = (size_t)pixel_idx * 4U;
        if (off + 4U > buf_len)
            return TBMP_ERR_TRUNCATED;
        /* Wire order: [R, G, B, A] -> R<<24|G<<16|B<<8|A */
        *out = ((uint32_t)buf[off + 0] << 24) | ((uint32_t)buf[off + 1] << 16) |
               ((uint32_t)buf[off + 2] << 8) | (uint32_t)buf[off + 3];
        return TBMP_OK;
    }

    default:
        return TBMP_ERR_BAD_BIT_DEPTH;
    }
}

/* Encoding mode decoders. */

/* total_pixels - convenience: pixel count (fits uint32_t; w,h <= 65535). */
static uint32_t total_pixels(const TBmpHead *h) {
    return (uint32_t)h->width * (uint32_t)h->height;
}

/*
 * decode_raw - Mode 0 RAW decoder.
 *
 * Flat packed pixel array; each pixel extracted with extract_pixel_raw.
 */
static TBmpError decode_raw(const TBmpImage *img, TBmpFrame *frame) {
    const TBmpHead *h = &img->head;
    uint32_t n = total_pixels(h);
    TBmpPixelFormat fmt = (TBmpPixelFormat)h->pixel_format;
    const TBmpPalette *pal = img->has_palette ? &img->palette : NULL;
    const TBmpMasks *msks = img->has_masks ? &img->masks : NULL;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t raw = 0;
        TBmpError e =
            extract_pixel_raw(img->data, img->data_len, i, h->bit_depth, &raw);
        if (e != TBMP_OK)
            return e;
        frame->pixels[i] = tbmp_pixel_to_rgba(raw, fmt, pal, msks);
    }
    return TBMP_OK;
}

/*
 * decode_zero_range - Mode 1 Zero-Range decoder.
 *
 * Wire format:
 *   u32  num_zero_ranges
 *   [start: u32, length: u32] x num_zero_ranges
 *   u32  num_values
 *   u8   values[num_values]  -- one byte per non-zero pixel, in scan order
 *
 * Algorithm (O(1) scratch, two implicit passes over the output buffer):
 *   1. Mark every pixel with a sentinel alpha to mean "needs a value".
 *   2. Overwrite the zero-range pixels with the zero colour.
 *   3. Walk the output once more; wherever the sentinel remains, pull the
 *      next byte from the values array.
 */
static TBmpError decode_zero_range(const TBmpImage *img, TBmpFrame *frame) {
    const TBmpHead *h = &img->head;
    uint32_t n = total_pixels(h);
    TBmpPixelFormat fmt = (TBmpPixelFormat)h->pixel_format;
    const TBmpPalette *pal = img->has_palette ? &img->palette : NULL;
    const TBmpMasks *msks = img->has_masks ? &img->masks : NULL;

    const uint8_t *p = img->data;
    size_t rem = img->data_len;

#define ZR_NEED(bytes)                                                         \
    do {                                                                       \
        if ((bytes) > rem)                                                     \
            return TBMP_ERR_TRUNCATED;                                         \
    } while (0)

    ZR_NEED(4U);
    uint32_t num_ranges = tbmp_read_u32le(p);
    p += 4;
    rem -= 4;
    if (num_ranges > rem / 8U)
        return TBMP_ERR_TRUNCATED;

    TBmpRGBA zero_colour = tbmp_pixel_to_rgba(0U, fmt, pal, msks);

    /*
     * Use alpha == ZR_SENTINEL as a sentinel for "pixel still needs
     * a value from the values array".  We fix up any real pixels with
     * this alpha after the fill loop.
     */
    for (uint32_t i = 0; i < n; i++) {
        frame->pixels[i].r = 0;
        frame->pixels[i].g = 0;
        frame->pixels[i].b = 0;
        frame->pixels[i].a = ZR_SENTINEL;
    }

    /* Overwrite zero-range pixels with the zero colour. */
    for (uint32_t r = 0; r < num_ranges; r++) {
        ZR_NEED(8U);
        uint32_t start = tbmp_read_u32le(p);
        p += 4;
        rem -= 4;
        uint32_t length = tbmp_read_u32le(p);
        p += 4;
        rem -= 4;

        if (start >= n && length > 0)
            return TBMP_ERR_BAD_SPAN;
        if (length > n - start)
            return TBMP_ERR_BAD_SPAN;

        for (uint32_t k = 0; k < length; k++) {
            frame->pixels[start + k] = zero_colour;
        }
    }

    /* Read num_values then fill non-zero pixels. */
    ZR_NEED(4U);
    uint32_t num_values = tbmp_read_u32le(p);
    p += 4;
    rem -= 4;
    if (num_values > rem)
        return TBMP_ERR_TRUNCATED;

    uint32_t vi = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (frame->pixels[i].a == ZR_SENTINEL) {
            if (vi >= num_values)
                return TBMP_ERR_TRUNCATED;
            frame->pixels[i] =
                tbmp_pixel_to_rgba((uint32_t)p[vi++], fmt, pal, msks);
        }
    }

    /* Any remaining sentinel pixels (invalid data) get zero. */
    for (uint32_t i = 0; i < n; i++) {
        if (frame->pixels[i].a == ZR_SENTINEL) {
            frame->pixels[i] = zero_colour;
        }
    }

#undef ZR_NEED
    return TBMP_OK;
}

/*
 * decode_rle - Mode 2 RLE decoder.
 *
 * Wire format: stream of (u8 count, value[vbytes]) pairs until data
 * exhausted.  count == 0 is a NOP.  The value field is vbytes wide where
 * vbytes = tbmp_value_field_bytes(bit_depth).  Any pixels not covered are
 * filled with the zero colour.
 */
static TBmpError decode_rle(const TBmpImage *img, TBmpFrame *frame) {
    const TBmpHead *h = &img->head;
    uint32_t n = total_pixels(h);
    TBmpPixelFormat fmt = (TBmpPixelFormat)h->pixel_format;
    const TBmpPalette *pal = img->has_palette ? &img->palette : NULL;
    const TBmpMasks *msks = img->has_masks ? &img->masks : NULL;

    const uint8_t *p = img->data;
    const uint8_t *end = img->data + img->data_len;
    uint8_t vbytes = tbmp_value_field_bytes(h->bit_depth);
    uint32_t pixel_out = 0;

    /* Each entry: 1 (count) + vbytes (value). */
    while (p + 1 + vbytes <= end) {
        uint8_t count = p[0];
        uint32_t raw = read_value_field(p + 1, vbytes);
        p += 1U + vbytes;

        if (count == 0)
            continue;

        TBmpRGBA colour = tbmp_pixel_to_rgba(raw, fmt, pal, msks);
        for (uint8_t k = 0; k < count; k++) {
            if (pixel_out >= n)
                return TBMP_ERR_BAD_SPAN;
            frame->pixels[pixel_out++] = colour;
        }
    }

    /* Pad any remaining pixels with the zero colour. */
    TBmpRGBA zero = tbmp_pixel_to_rgba(0U, fmt, pal, msks);
    while (pixel_out < n) {
        frame->pixels[pixel_out++] = zero;
    }

    return TBMP_OK;
}

/*
 * decode_span - Mode 3 Span decoder.
 *
 * Wire format:
 *   u32  num_spans
 *   [index: u32, length: u32, value: vbytes] x num_spans
 *
 * vbytes = tbmp_value_field_bytes(bit_depth).
 * Sets pixels[index .. index+length-1] = value.  Background is zero colour.
 */
static TBmpError decode_span(const TBmpImage *img, TBmpFrame *frame) {
    const TBmpHead *h = &img->head;
    uint32_t n = total_pixels(h);
    TBmpPixelFormat fmt = (TBmpPixelFormat)h->pixel_format;
    const TBmpPalette *pal = img->has_palette ? &img->palette : NULL;
    const TBmpMasks *msks = img->has_masks ? &img->masks : NULL;

    TBmpRGBA bg = tbmp_pixel_to_rgba(0U, fmt, pal, msks);
    for (uint32_t i = 0; i < n; i++)
        frame->pixels[i] = bg;

    const uint8_t *p = img->data;
    size_t rem = img->data_len;
    uint8_t vbytes = tbmp_value_field_bytes(h->bit_depth);
    /* Entry size: 4 (index) + 4 (length) + vbytes (value). */
    size_t entry_size = 8U + vbytes;

#define SP_NEED(bytes)                                                         \
    do {                                                                       \
        if ((bytes) > rem)                                                     \
            return TBMP_ERR_TRUNCATED;                                         \
    } while (0)

    SP_NEED(4U);
    uint32_t num_spans = tbmp_read_u32le(p);
    p += 4;
    rem -= 4;

    if (num_spans > rem / entry_size)
        return TBMP_ERR_TRUNCATED;

    for (uint32_t s = 0; s < num_spans; s++) {
        SP_NEED(entry_size);
        uint32_t index = tbmp_read_u32le(p);
        p += 4;
        rem -= 4;
        uint32_t length = tbmp_read_u32le(p);
        p += 4;
        rem -= 4;
        uint32_t raw = read_value_field(p, vbytes);
        p += vbytes;
        rem -= vbytes;

        if (index >= n)
            return TBMP_ERR_BAD_SPAN;
        if (length > n - index)
            return TBMP_ERR_BAD_SPAN;

        TBmpRGBA colour = tbmp_pixel_to_rgba(raw, fmt, pal, msks);
        for (uint32_t k = 0; k < length; k++) {
            frame->pixels[index + k] = colour;
        }
    }

#undef SP_NEED
    return TBMP_OK;
}

/*
 * decode_sparse_pixel - Mode 4 Sparse Pixel decoder.
 *
 * Wire format:
 *   u32  num_pixels
 *   [x: u16, y: u16, value: vbytes] x num_pixels
 *
 * vbytes = tbmp_value_field_bytes(bit_depth).
 * Explicitly listed pixels; background is the zero colour.
 */
static TBmpError decode_sparse_pixel(const TBmpImage *img, TBmpFrame *frame) {
    const TBmpHead *h = &img->head;
    uint32_t n = total_pixels(h);
    TBmpPixelFormat fmt = (TBmpPixelFormat)h->pixel_format;
    const TBmpPalette *pal = img->has_palette ? &img->palette : NULL;
    const TBmpMasks *msks = img->has_masks ? &img->masks : NULL;

    TBmpRGBA bg = tbmp_pixel_to_rgba(0U, fmt, pal, msks);
    for (uint32_t i = 0; i < n; i++)
        frame->pixels[i] = bg;

    const uint8_t *p = img->data;
    size_t rem = img->data_len;
    uint8_t vbytes = tbmp_value_field_bytes(h->bit_depth);
    /* Entry size: 2 (x) + 2 (y) + vbytes (value). */
    size_t entry_size = 4U + vbytes;

#define SPX_NEED(bytes)                                                        \
    do {                                                                       \
        if ((bytes) > rem)                                                     \
            return TBMP_ERR_TRUNCATED;                                         \
    } while (0)

    SPX_NEED(4U);
    uint32_t num_pixels = tbmp_read_u32le(p);
    p += 4;
    rem -= 4;

    if (num_pixels > rem / entry_size)
        return TBMP_ERR_TRUNCATED;

    for (uint32_t i = 0; i < num_pixels; i++) {
        SPX_NEED(entry_size);
        uint16_t x = tbmp_read_u16le(p);
        p += 2;
        rem -= 2;
        uint16_t y = tbmp_read_u16le(p);
        p += 2;
        rem -= 2;
        uint32_t raw = read_value_field(p, vbytes);
        p += vbytes;
        rem -= vbytes;

        if (x >= h->width || y >= h->height)
            return TBMP_ERR_BAD_PIXEL_COORD;

        uint32_t idx = (uint32_t)y * h->width + x;
        frame->pixels[idx] = tbmp_pixel_to_rgba(raw, fmt, pal, msks);
    }

#undef SPX_NEED
    return TBMP_OK;
}

/*
 * decode_block_sparse - Mode 5 Block-Sparse decoder.
 *
 * Wire format:
 *   u16  block_width
 *   u16  block_height
 *   u32  num_blocks
 *   [block_index: u32, block_data_size: u32, block_data: u8[]] x num_blocks
 *
 * block_index: tile number in row-major order (left to right, top to bottom).
 * block_data:  raw packed-pixel tile of block_width x block_height pixels.
 * Background is the zero colour.
 */
static TBmpError decode_block_sparse(const TBmpImage *img, TBmpFrame *frame) {
    const TBmpHead *h = &img->head;
    TBmpPixelFormat fmt = (TBmpPixelFormat)h->pixel_format;
    const TBmpPalette *pal = img->has_palette ? &img->palette : NULL;
    const TBmpMasks *msks = img->has_masks ? &img->masks : NULL;

    uint32_t n = total_pixels(h);
    TBmpRGBA bg = tbmp_pixel_to_rgba(0U, fmt, pal, msks);
    for (uint32_t i = 0; i < n; i++)
        frame->pixels[i] = bg;

    const uint8_t *p = img->data;
    size_t rem = img->data_len;

#define BS_NEED(bytes)                                                         \
    do {                                                                       \
        if ((bytes) > rem)                                                     \
            return TBMP_ERR_TRUNCATED;                                         \
    } while (0)

    BS_NEED(8U);
    uint16_t bw = tbmp_read_u16le(p);
    p += 2;
    rem -= 2;
    uint16_t bh = tbmp_read_u16le(p);
    p += 2;
    rem -= 2;
    uint32_t num_blocks = tbmp_read_u32le(p);
    p += 4;
    rem -= 4;

    if (bw == 0 || bh == 0)
        return TBMP_ERR_BAD_BLOCK;

    /* Tiles per row/column of the full image (ceiling division). */
    uint32_t tiles_x = ((uint32_t)h->width + bw - 1U) / bw;
    uint32_t tiles_y = ((uint32_t)h->height + bh - 1U) / bh;
    if (tiles_x > 0 && tiles_y > UINT32_MAX / tiles_x)
        return TBMP_ERR_OVERFLOW;
    uint32_t total_tiles = tiles_x * tiles_y;

    for (uint32_t b = 0; b < num_blocks; b++) {
        BS_NEED(8U);
        uint32_t block_index = tbmp_read_u32le(p);
        p += 4;
        rem -= 4;
        uint32_t block_data_size = tbmp_read_u32le(p);
        p += 4;
        rem -= 4;

        if (block_index >= total_tiles)
            return TBMP_ERR_BAD_BLOCK;
        if (block_data_size > rem)
            return TBMP_ERR_TRUNCATED;

        const uint8_t *bdata = p;
        p += block_data_size;
        rem -= block_data_size;

        /* Tile origin in image coordinates. */
        uint32_t tile_col = block_index % tiles_x;
        uint32_t tile_row = block_index / tiles_x;
        uint32_t origin_x = tile_col * bw;
        uint32_t origin_y = tile_row * bh;

        /* Decode each pixel within the tile. */
        uint32_t tile_px = 0;
        for (uint32_t ty = 0; ty < bh; ty++) {
            uint32_t img_y = origin_y + ty;
            if (img_y >= h->height)
                break; /* partial last row */

            for (uint32_t tx = 0; tx < bw; tx++) {
                uint32_t img_x = origin_x + tx;
                if (img_x >= h->width) {
                    tile_px++;
                    continue; /* partial col */
                }

                uint32_t raw = 0;
                TBmpError e = extract_pixel_raw(bdata, block_data_size, tile_px,
                                                h->bit_depth, &raw);
                if (e != TBMP_OK)
                    return e;

                uint32_t out_idx = img_y * h->width + img_x;
                frame->pixels[out_idx] =
                    tbmp_pixel_to_rgba(raw, fmt, pal, msks);
                tile_px++;
            }
        }
    }

#undef BS_NEED
    return TBMP_OK;
}

/* Public entry point. */

TBmpError tbmp_decode(const TBmpImage *img, TBmpFrame *frame) {
    if (img == NULL || frame == NULL)
        return TBMP_ERR_NULL_PTR;
    if (frame->pixels == NULL)
        return TBMP_ERR_NULL_PTR;

    const TBmpHead *h = &img->head;
    if (h->width == 0 || h->height == 0)
        return TBMP_ERR_ZERO_DIMENSIONS;

    /* Overflow check: width x height must fit in uint32_t. */
    if ((uint32_t)h->width > UINT32_MAX / (uint32_t)h->height)
        return TBMP_ERR_OVERFLOW;

    frame->width = h->width;
    frame->height = h->height;

    switch ((TBmpEncoding)h->encoding) {
    case TBMP_ENC_RAW:
        return decode_raw(img, frame);
    case TBMP_ENC_ZERO_RANGE:
        return decode_zero_range(img, frame);
    case TBMP_ENC_RLE:
        return decode_rle(img, frame);
    case TBMP_ENC_SPAN:
        return decode_span(img, frame);
    case TBMP_ENC_SPARSE_PIXEL:
        return decode_sparse_pixel(img, frame);
    case TBMP_ENC_BLOCK_SPARSE:
        return decode_block_sparse(img, frame);
    default:
        return TBMP_ERR_BAD_ENCODING;
    }
}
