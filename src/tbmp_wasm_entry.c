#include "tbmp_types.h"
#include "tbmp_decode.h"
#include "tbmp_meta.h"
#include "tbmp_msgpack.h"
#include "tbmp_pixel.h"
#include "tbmp_reader.h"
#include "tbmp_rotate.h"
#include "tbmp_write.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * tBMP WebAssembly API Implementation.
 *
 * This file provides a JavaScript-friendly API for browsers by wrapping
 * the core C library functions with internal state management.
 */

/* Internal image state. */
static TBmpImage g_image = {0};
static TBmpFrame g_frame = {0};
static uint8_t *g_inputBuf = NULL;
static uint8_t *g_pixelBuf = NULL;

/* Rotation buffer for transform operations. */
static uint8_t *g_rotateBuf = NULL;
static size_t g_rotateCap = 0;

/* Current image dimensions. */
static int g_errorCode = 0;
static int g_width = 0;
static int g_height = 0;

/*
 * tbmp_reset - reset internal state for loading a new image.
 * Frees allocated buffers but keeps them for reuse.
 */
void tbmp_reset(void) {
    if (g_inputBuf) {
        free(g_inputBuf);
        g_inputBuf = NULL;
    }
    if (g_pixelBuf) {
        free(g_pixelBuf);
        g_pixelBuf = NULL;
    }
    g_errorCode = 0;
    g_width = g_height = 0;
    /* Fully reset image state, including palette and masks */
    memset(&g_image, 0, sizeof(g_image));
    memset(&g_image.palette, 0, sizeof(g_image.palette));
    memset(&g_image.masks, 0, sizeof(g_image.masks));
    g_image.has_palette = 0;
    g_image.has_masks = 0;
    g_image.extra = NULL;
    g_image.extra_len = 0;
    g_image.meta = NULL;
    g_image.meta_len = 0;
    g_frame.width = g_frame.height = 0;
    g_frame.pixels = NULL;
}

/*
 * tbmp_load - load and decode a tBMP file into internal state.
 *
 * data : pointer to tBMP file data (non-NULL).
 * len  : byte length of data.
 *
 * Returns: 0 on success, negative TBmpError on failure.
 */
int tbmp_load(uint8_t *data, int len) {
    tbmp_reset();

    g_inputBuf = (uint8_t *)malloc(len);
    memcpy(g_inputBuf, data, len);

    g_errorCode = tbmp_open(g_inputBuf, len, &g_image);
    if (g_errorCode != 0)
        return g_errorCode;

    uint32_t w = g_image.head.width;
    uint32_t h = g_image.head.height;
    uint32_t pxLen = w * h * 4;

    g_pixelBuf = (uint8_t *)malloc(pxLen);
    g_frame.width = w;
    g_frame.height = h;
    g_frame.pixels = (TBmpRGBA *)g_pixelBuf;

    g_errorCode = tbmp_decode(&g_image, &g_frame);

    if (g_errorCode == 0) {
        g_width = w;
        g_height = h;
    }
    return g_errorCode;
}

/* Image information getters. */

int tbmp_width(void) {
    return g_width;
}

int tbmp_height(void) {
    return g_height;
}

int tbmp_pixel_format(void) {
    return g_image.head.pixel_format;
}

int tbmp_encoding(void) {
    return g_image.head.encoding;
}

int tbmp_bit_depth(void) {
    return g_image.head.bit_depth;
}

int tbmp_error(void) {
    return g_errorCode;
}

int tbmp_has_palette(void) {
    return g_image.has_palette;
}

int tbmp_has_masks(void) {
    return g_image.has_masks;
}

int tbmp_has_extra(void) {
    return g_image.extra && g_image.extra_len;
}

int tbmp_has_meta(void) {
    return g_image.meta && g_image.meta_len;
}

/*
 * tbmp_pixels_len - get byte length of pixel buffer.
 *
 * Returns: width * height * 4 bytes.
 */
int tbmp_pixels_len(void) {
    return g_width * g_height * 4;
}
int tbmp_height(void) {
    return g_height;
}
int tbmp_pixel_format(void) {
    return g_image.head.pixel_format;
}
int tbmp_encoding(void) {
    return g_image.head.encoding;
}

int tbmp_bit_depth(void) {
    return g_image.head.bit_depth;
}

int tbmp_error(void) {
    return g_errorCode;
}

int tbmp_has_palette(void) {
    return g_image.has_palette;
}

int tbmp_has_masks(void) {
    return g_image.has_masks;
}

int tbmp_has_extra(void) {
    return g_image.extra && g_image.extra_len;
}

int tbmp_has_meta(void) {
    return g_image.meta && g_image.meta_len;
}

int tbmp_pixels_len(void) {
    return g_width * g_height * 4;
}

uint8_t *tbmp_pixels_ptr(void) {
    return g_pixelBuf;
}

int tbmp_pixels_copy(uint8_t *addr, int maxLen) {
    uint32_t pxLen = g_width * g_height * 4;
    int copyLen = (int)pxLen < maxLen ? pxLen : maxLen;
    if (g_pixelBuf && copyLen > 0) {
        memcpy(addr, g_pixelBuf, copyLen);
    }
    return copyLen;
}

static char g_jsonBuf[65536];

int tbmp_image_info_json(char *addr, int maxLen) {
    if (!g_width)
        return 0;
    int n = snprintf(
        g_jsonBuf, sizeof(g_jsonBuf),
        "{\"width\":%u,\"height\":%u,\"pixel_format\":%u,\"encoding\":%u}",
        g_width, g_height, g_image.head.pixel_format, g_image.head.encoding);
    if (n > 0 && n < maxLen) {
        memcpy(addr, g_jsonBuf, n + 1);
    }
    return n;
}

// Helper: Convert a MessagePack map (with string keys, simple values) to JSON
static int msgpack_map_to_json(const uint8_t *data, uint32_t len, char *out,
                               int maxLen) {
    TBmpMpReader r;
    tbmp_mp_reader_init(&r, data, len);
    TBmpMpTag tag = tbmp_mp_read_tag(&r);
    if (tbmp_mp_reader_error(&r) || tag.type != TBMP_MP_MAP)
        return snprintf(out, maxLen, "null");
    int pos = 0;
    pos += snprintf(out + pos, maxLen - pos, "{");
    for (uint32_t i = 0; i < tag.v.len && pos < maxLen - 10; i++) {
        if (i)
            pos += snprintf(out + pos, maxLen - pos, ",");
        // Key
        TBmpMpTag keyTag = tbmp_mp_read_tag(&r);
        if (tbmp_mp_reader_error(&r) || keyTag.type != TBMP_MP_STR)
            return snprintf(out, maxLen, "null");
        char key[64] = {0};
        uint32_t klen = keyTag.v.len < 63 ? keyTag.v.len : 63;
        tbmp_mp_read_bytes(&r, key, klen);
        key[klen] = 0;
        tbmp_mp_done_str(&r);
        // Value
        TBmpMpTag valTag = tbmp_mp_read_tag(&r);
        if (tbmp_mp_reader_error(&r))
            return snprintf(out, maxLen, "null");
        if (valTag.type == TBMP_MP_STR) {
            char sval[128] = {0};
            uint32_t slen = valTag.v.len < 127 ? valTag.v.len : 127;
            tbmp_mp_read_bytes(&r, sval, slen);
            sval[slen] = 0;
            tbmp_mp_done_str(&r);
            pos +=
                snprintf(out + pos, maxLen - pos, "\"%s\":\"%s\"", key, sval);
        } else if (valTag.type == TBMP_MP_UINT) {
            pos += snprintf(out + pos, maxLen - pos, "\"%s\":%llu", key,
                            (unsigned long long)valTag.v.u);
        } else if (valTag.type == TBMP_MP_INT) {
            pos += snprintf(out + pos, maxLen - pos, "\"%s\":%lld", key,
                            (long long)valTag.v.i);
        } else if (valTag.type == TBMP_MP_BOOL) {
            pos += snprintf(out + pos, maxLen - pos, "\"%s\":%s", key,
                            valTag.v.b ? "true" : "false");
        } else {
            pos += snprintf(out + pos, maxLen - pos, "\"%s\":null", key);
        }
    }
    pos += snprintf(out + pos, maxLen - pos, "}");
    return pos;
}

int tbmp_meta_json(char *addr, int maxLen) {
    if (!g_image.meta || !g_image.meta_len)
        return 0;
    TBmpMeta meta;
    if (tbmp_meta_parse(g_image.meta, g_image.meta_len, &meta) != 0)
        return 0;
    int pos = 0;
    addr[pos++] = '{';
    if (pos >= maxLen)
        return 0;
#define ADD_STR(k, v)                                                          \
    do {                                                                       \
        if ((v)[0] && pos < maxLen - 20) {                                     \
            if (pos > 1)                                                       \
                addr[pos++] = ',';                                             \
            pos += snprintf(addr + pos, maxLen - pos, "\"%s\":\"%s\"", k, v);  \
        }                                                                      \
    } while (0)
#define ADD_NUM(k, v)                                                          \
    do {                                                                       \
        if ((v) && pos < maxLen - 15) {                                        \
            if (pos > 1)                                                       \
                addr[pos++] = ',';                                             \
            pos += snprintf(addr + pos, maxLen - pos, "\"%s\":%u", k, (v));    \
        }                                                                      \
    } while (0)
    ADD_STR("title", meta.title);
    ADD_STR("author", meta.author);
    ADD_STR("description", meta.description);
    ADD_STR("created", meta.created);
    ADD_STR("software", meta.software);
    ADD_STR("license", meta.license);
    if (meta.has_dpi)
        ADD_NUM("dpi", meta.dpi);
    ADD_STR("colorspace", meta.colorspace);
    if (meta.tag_count > 0 && pos < maxLen - 20) {
        if (pos > 1)
            addr[pos++] = ',';
        pos += snprintf(addr + pos, maxLen - pos, "\"tags\":[");
        for (uint32_t i = 0; i < meta.tag_count && pos < maxLen - 10; i++) {
            if (i)
                addr[pos++] = ',';
            pos += snprintf(addr + pos, maxLen - pos, "\"%s\"", meta.tags[i]);
        }
        pos += snprintf(addr + pos, maxLen - pos, "]");
    }
    // Add custom metadata as a JSON array
    if (meta.custom_count > 0 && pos < maxLen - 20) {
        if (pos > 1)
            addr[pos++] = ',';
        pos += snprintf(addr + pos, maxLen - pos, "\"custom\":[");
        for (uint32_t i = 0; i < meta.custom_count && pos < maxLen - 10; i++) {
            if (i)
                addr[pos++] = ',';
            char custom_json[256];
            int n = msgpack_map_to_json(meta.custom[i].data, meta.custom[i].len,
                                        custom_json, sizeof(custom_json));
            if (n > 0 && n < (int)sizeof(custom_json)) {
                pos += snprintf(addr + pos, maxLen - pos, "%s", custom_json);
            } else {
                pos += snprintf(addr + pos, maxLen - pos, "null");
            }
        }
        pos += snprintf(addr + pos, maxLen - pos, "]");
    }

    if (pos < maxLen) {
        addr[pos++] = '}';
    }
    addr[pos] = 0;
    return pos;
}

int tbmp_palette_json(char *addr, int maxLen) {
    if (!g_image.has_palette || !g_image.palette.count)
        return 0;
    memset(addr, 0, maxLen); // Clear output buffer to prevent stacking
    TBmpPalette *p = &g_image.palette;
    char *out = addr;
    int pos = 0;
    pos += snprintf(out + pos, maxLen - pos, "[");
    int truncated = 0;
    for (uint32_t i = 0; i < p->count; i++) {
        if (pos >= maxLen - 30) {
            truncated = 1;
            break;
        }
        if (i > 0)
            pos += snprintf(out + pos, maxLen - pos, ",");
        pos += snprintf(
            out + pos, maxLen - pos, "{\"r\":%u,\"g\":%u,\"b\":%u,\"a\":%u}",
            p->entries[i].r, p->entries[i].g, p->entries[i].b, p->entries[i].a);
    }
    if (pos < maxLen)
        out[pos] = ']';
    // Always null-terminate
    if (pos + 1 < maxLen)
        out[pos + 1] = 0;
    else
        out[maxLen - 1] = 0;
    return pos + 1;
}

int tbmp_masks_json(char *addr, int maxLen) {
    if (!g_image.has_masks)
        return 0;
    TBmpMasks *m = &g_image.masks;
    int n = snprintf(g_jsonBuf, sizeof(g_jsonBuf),
                     "{\"r\":%u,\"g\":%u,\"b\":%u,\"a\":%u}", m->r, m->g, m->b,
                     m->a);
    if (n > 0 && n < maxLen) {
        memcpy(addr, g_jsonBuf, n + 1);
    }
    return n;
}

int tbmp_extra_json(char *addr, int maxLen) {
    if (!g_image.extra || !g_image.extra_len)
        return 0;
    int n = g_image.extra_len < maxLen - 1 ? g_image.extra_len : maxLen - 1;
    memcpy(addr, g_image.extra, n);
    addr[n] = 0;
    return n;
}

void tbmp_cleanup(void) {
    if (g_inputBuf) {
        free(g_inputBuf);
        g_inputBuf = NULL;
    }
    if (g_pixelBuf) {
        free(g_pixelBuf);
        g_pixelBuf = NULL;
    }
    if (g_rotateBuf) {
        free(g_rotateBuf);
        g_rotateBuf = NULL;
        g_rotateCap = 0;
    }
    g_errorCode = 0;
    g_width = g_height = 0;
    memset(&g_image, 0, sizeof(g_image));
    memset(&g_frame, 0, sizeof(g_frame));
}

static const char *g_errorStrings[] = {"OK",
                                       "INVALID_SIGNATURE",
                                       "INVALID_VERSION",
                                       "INVALID_HEADER",
                                       "INVALID_PIXEL_FORMAT",
                                       "INVALID_ENCODING",
                                       "DECODE_ERROR",
                                       "MEMORY_ERROR",
                                       "UNSUPPORTED_FEATURE"};

int tbmp_error_string(int err, char *addr, int maxLen) {
    const char *msg =
        (err >= 0 && err < 9) ? g_errorStrings[err] : "UNKNOWN_ERROR";
    int n = strlen(msg);
    if (n < maxLen) {
        memcpy(addr, msg, n + 1);
    }
    return n;
}

/* Write API */

static size_t g_outputLen = 0;

int tbmp_encode(uint8_t *pixels, int width, int height, int encoding,
                int pixel_format, int bit_depth, uint8_t *out, int out_cap) {
    if (!pixels || width <= 0 || height <= 0 || !out || out_cap <= 0)
        return TBMP_ERR_NULL_PTR;

    TBmpFrame frame;
    frame.width = (uint16_t)width;
    frame.height = (uint16_t)height;
    frame.pixels = (TBmpRGBA *)pixels;

    TBmpWriteParams params;
    tbmp_write_default_params(&params);
    params.encoding = (TBmpEncoding)encoding;
    params.pixel_format = (TBmpPixelFormat)pixel_format;
    params.bit_depth = (uint8_t)bit_depth;

    size_t needed = tbmp_write_needed_size(&frame, &params);
    if (needed == 0)
        return TBMP_ERR_BAD_DIMENSIONS;
    if ((size_t)needed > (size_t)out_cap)
        return TBMP_ERR_OUT_OF_MEMORY;

    int rc = tbmp_write(&frame, &params, out, out_cap, &g_outputLen);
    return rc;
}

int tbmp_write_needed(int width, int height, int encoding, int pixel_format,
                      int bit_depth) {
    if (width <= 0 || height <= 0)
        return 0;

    TBmpFrame frame;
    frame.width = (uint16_t)width;
    frame.height = (uint16_t)height;
    frame.pixels = NULL;

    TBmpWriteParams params;
    tbmp_write_default_params(&params);
    params.encoding = (TBmpEncoding)encoding;
    params.pixel_format = (TBmpPixelFormat)pixel_format;
    params.bit_depth = (uint8_t)bit_depth;

    return (int)tbmp_write_needed_size(&frame, &params);
}

/* Rotate API */

int tbmp_rotate_90(void) {
    if (!g_frame.pixels || !g_width || !g_height)
        return TBMP_ERR_NULL_PTR;

    size_t newCap = (size_t)g_height * g_width * 4;
    if (newCap > g_rotateCap) {
        if (g_rotateBuf)
            free(g_rotateBuf);
        g_rotateBuf = malloc(newCap);
        if (!g_rotateBuf)
            return TBMP_ERR_OUT_OF_MEMORY;
        g_rotateCap = newCap;
    }

    TBmpFrame src = {g_width, g_height, g_frame.pixels};
    TBmpFrame dst = {g_height, g_width, (TBmpRGBA *)g_rotateBuf};

    int rc = tbmp_rotate90(&src, &dst);
    if (rc != 0)
        return rc;

    memcpy(g_frame.pixels, g_rotateBuf, newCap);
    uint16_t tmp = g_width;
    g_width = g_height;
    g_height = tmp;
    g_frame.width = g_width;
    g_frame.height = g_height;

    return TBMP_OK;
}

int tbmp_rotate_180(void) {
    if (!g_frame.pixels || !g_width || !g_height)
        return TBMP_ERR_NULL_PTR;

    size_t newCap = (size_t)g_width * g_height * 4;
    if (newCap > g_rotateCap) {
        if (g_rotateBuf)
            free(g_rotateBuf);
        g_rotateBuf = malloc(newCap);
        if (!g_rotateBuf)
            return TBMP_ERR_OUT_OF_MEMORY;
        g_rotateCap = newCap;
    }

    TBmpFrame src = {g_width, g_height, g_frame.pixels};
    TBmpFrame dst = {g_width, g_height, (TBmpRGBA *)g_rotateBuf};

    int rc = tbmp_rotate180(&src, &dst);
    if (rc != 0)
        return rc;

    memcpy(g_frame.pixels, g_rotateBuf, newCap);
    return TBMP_OK;
}

int tbmp_rotate_270(void) {
    if (!g_frame.pixels || !g_width || !g_height)
        return TBMP_ERR_NULL_PTR;

    size_t newCap = (size_t)g_height * g_width * 4;
    if (newCap > g_rotateCap) {
        if (g_rotateBuf)
            free(g_rotateBuf);
        g_rotateBuf = malloc(newCap);
        if (!g_rotateBuf)
            return TBMP_ERR_OUT_OF_MEMORY;
        g_rotateCap = newCap;
    }

    TBmpFrame src = {g_width, g_height, g_frame.pixels};
    TBmpFrame dst = {g_height, g_width, (TBmpRGBA *)g_rotateBuf};

    int rc = tbmp_rotate270(&src, &dst);
    if (rc != 0)
        return rc;

    memcpy(g_frame.pixels, g_rotateBuf, newCap);
    uint16_t tmp = g_width;
    g_width = g_height;
    g_height = tmp;
    g_frame.width = g_width;
    g_frame.height = g_height;

    return TBMP_OK;
}

/* Arbitrary angle rotation */

int tbmp_rotate_output_size(int src_w, int src_h, double angle_rad, int *out_w,
                            int *out_h) {
    uint16_t w, h;
    tbmp_rotate_output_dims((uint16_t)src_w, (uint16_t)src_h, angle_rad, &w,
                            &h);
    if (out_w)
        *out_w = w;
    if (out_h)
        *out_h = h;
    return 0;
}

int tbmp_rotate_any(double angle_rad, int bg_r, int bg_g, int bg_b, int bg_a,
                    int filter) {
    if (!g_frame.pixels || !g_width || !g_height)
        return TBMP_ERR_NULL_PTR;

    uint16_t new_w, new_h;
    tbmp_rotate_output_dims(g_width, g_height, angle_rad, &new_w, &new_h);

    size_t newCap = (size_t)new_w * new_h * 4;
    if (newCap > g_rotateCap) {
        if (g_rotateBuf)
            free(g_rotateBuf);
        g_rotateBuf = malloc(newCap);
        if (!g_rotateBuf)
            return TBMP_ERR_OUT_OF_MEMORY;
        g_rotateCap = newCap;
    }

    TBmpFrame src = {g_width, g_height, g_frame.pixels};
    TBmpFrame dst = {new_w, new_h, (TBmpRGBA *)g_rotateBuf};
    TBmpRGBA bg = {(uint8_t)bg_r, (uint8_t)bg_g, (uint8_t)bg_b, (uint8_t)bg_a};
    TBmpRotateFilter fil =
        (filter == 1) ? TBMP_ROTATE_BILINEAR : TBMP_ROTATE_NEAREST;

    int rc = tbmp_rotate(&src, &dst, angle_rad, bg, fil);
    if (rc != 0)
        return rc;

    if (g_pixelBuf)
        free(g_pixelBuf);
    g_pixelBuf = g_rotateBuf;
    g_rotateBuf = NULL;
    g_rotateCap = 0;

    g_width = new_w;
    g_height = new_h;
    g_frame.width = new_w;
    g_frame.height = new_h;
    g_frame.pixels = (TBmpRGBA *)g_pixelBuf;

    g_rotateBuf = malloc(newCap);
    if (!g_rotateBuf)
        return TBMP_ERR_OUT_OF_MEMORY;
    g_rotateCap = newCap;

    return TBMP_OK;
}

/* Pixel conversion */

int tbmp_convert_pixel(int pixel_val, int fmt) {
    TBmpRGBA c =
        tbmp_pixel_to_rgba((uint32_t)pixel_val, (TBmpPixelFormat)fmt,
                           g_image.has_palette ? &g_image.palette : NULL,
                           g_image.has_masks ? &g_image.masks : NULL);
    return (int)((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) |
           ((uint32_t)c.b << 8) | (uint32_t)c.a;
}

/* Dithering */

int tbmp_dither(int num_colors) {
    if (!g_frame.pixels || !g_width || !g_height)
        return TBMP_ERR_NULL_PTR;
    if (num_colors <= 0 || num_colors > TBMP_MAX_PALETTE)
        return TBMP_ERR_BAD_PALETTE;

    TBmpPalette pal;
    int rc = tbmp_auto_palette_from_frame(&g_frame, (uint32_t)num_colors, &pal);
    if (rc != 0)
        return rc;

    return tbmp_dither_to_palette(&g_frame, &pal);
}

/* Validation */

int tbmp_validate(int width, int height, int bit_depth, int encoding,
                  int pixel_format) {
    TBmpHead head = {TBMP_VERSION_1_0,
                     (uint16_t)width,
                     (uint16_t)height,
                     (uint8_t)bit_depth,
                     (uint8_t)encoding,
                     (uint8_t)pixel_format,
                     0,
                     0,
                     0,
                     0,
                     0};
    return tbmp_validate_head(&head);
}