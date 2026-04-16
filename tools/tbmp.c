/* tBMP general command-line tool.
 *
 * Converts between PNM image files (P1-P6) and the tBMP embedded graphics
 * format.  All six PNM variants are supported:
 *
 *   P1  ASCII PBM  (bilevel)
 *   P2  ASCII PGM  (grayscale)
 *   P3  ASCII PPM  (colour)
 *   P4  binary PBM (bilevel, 1 bit per pixel packed)
 *   P5  binary PGM (grayscale, 8 bit per pixel)
 *   P6  binary PPM (colour, 8 bit per pixel)
 *
 * P1-P4 are parsed by the hand-rolled pnm_load() below.
 * P5 and P6 are handled by stb_image (faster, already present).
 * Other formats accepted by stb_image (PNG, BMP, JPEG, ...) also work
 * for encode input.
 *
 * Output images (decode) are written as PPM P6 (colour) or PGM P5
 * (grayscale) depending on the tBMP pixel format.
 *
 * Usage:
 *   tbmp encode <input> <output.tbmp> [OPTIONS]
 *   tbmp decode <input.tbmp> <output.ppm>
 *
 * Encode options:
 *   --format   <name>  Pixel format. Choices:
 *                        rgba8888 (default), rgb888, rgb565, rgb555,
 *                        rgb444, rgb332
 *   --encoding <name>  Encoding mode. Choices:
 *                        raw (default), rle, zerorange, span
 *
 * Decode:
 *   Writes a PPM P6 or PGM P5 file depending on the tBMP pixel format.
 *   If the tBMP file contains a META section each key/value pair is
 *   printed to stdout.
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STB_IMAGE_WRITE_IMPLEMENTATION

/* Suppress warnings from vendored headers that we do not control. */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "vendor/stb_image.h"
#include "vendor/stb_image_write.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "tbmp_decode.h"
#include "tbmp_meta.h"
#include "tbmp_msgpack.h"
#include "tbmp_reader.h"
#include "tbmp_types.h"
#include "tbmp_ui.h"
#include "tbmp_write.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PNM loader for P1-P4 (formats stb_image does not support).
 *
 * Reads the file at path into a heap-allocated TBmpRGBA array.
 * Sets *out_w, *out_h, *out_channels (1 for bilevel/gray, 3 for colour).
 * Returns NULL on error.  Caller must free() the returned buffer.
 *
 * maxval > 255 (16-bit PGM/PPM) is rejected; only 8-bit data is accepted.
 */

/*
 * pnm_skip - advance *p past whitespace and '#' comment lines.
 */
static void pnm_skip(const char **p, const char *end) {
    while (*p < end) {
        if (**p == '#') {
            while (*p < end && **p != '\n')
                (*p)++;
        } else if (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') {
            (*p)++;
        } else {
            break;
        }
    }
}

/*
 * pnm_uint: parse one non-negative decimal integer from *p.
 * Returns -1 on failure.
 */
static long pnm_uint(const char **p, const char *end) {
    pnm_skip(p, end);
    if (*p >= end || **p < '0' || **p > '9')
        return -1;
    long v = 0;
    while (*p < end && **p >= '0' && **p <= '9') {
        v = v * 10 + (**p - '0');
        (*p)++;
    }
    return v;
}

/*
 * pnm_load - load a P1/P2/P3/P4 PNM file into an RGBA buffer.
 *
 * path         : input file path.
 * out_w        : receives image width.
 * out_h        : receives image height.
 * out_channels : receives 1 (bilevel/gray) or 3 (colour).
 *
 * Returns heap-allocated TBmpRGBA array, or NULL on error.
 */
static TBmpRGBA *pnm_load(const char *path, int *out_w, int *out_h,
                          int *out_channels) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    rewind(f);
    if (fsz <= 0) {
        fprintf(stderr, "error: empty file '%s'\n", path);
        fclose(f);
        return NULL;
    }
    char *buf = (char *)malloc((size_t)fsz + 1);
    if (!buf) {
        fprintf(stderr, "error: out of memory\n");
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)fsz, f) != (size_t)fsz) {
        fprintf(stderr, "error: read failed for '%s'\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[fsz] = '\0';

    const char *p = buf;
    const char *end = buf + fsz;

    /* Magic: "P1", "P2", "P3", or "P4". */
    if (end - p < 2 || p[0] != 'P' ||
        (p[1] != '1' && p[1] != '2' && p[1] != '3' && p[1] != '4')) {
        fprintf(stderr, "error: '%s' is not a P1/P2/P3/P4 PNM file\n", path);
        free(buf);
        return NULL;
    }
    int type = p[1] - '0'; /* 1, 2, 3, or 4 */
    p += 2;

    long width = pnm_uint(&p, end);
    long height = pnm_uint(&p, end);
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "error: invalid PNM dimensions in '%s'\n", path);
        free(buf);
        return NULL;
    }

    long maxval = 1; /* PBM (P1/P4) has no maxval field */
    if (type == 2 || type == 3) {
        maxval = pnm_uint(&p, end);
        if (maxval <= 0 || maxval > 255) {
            fprintf(stderr,
                    "error: unsupported maxval %ld in '%s' (only 8-bit)\n",
                    maxval, path);
            free(buf);
            return NULL;
        }
    }

    /* For binary P4: one mandatory whitespace byte before pixel data. */
    if (type == 4) {
        if (p >= end) {
            fprintf(stderr, "error: truncated P4 file '%s'\n", path);
            free(buf);
            return NULL;
        }
        p++; /* consume the single whitespace separator */
    }

    size_t npix = (size_t)width * (size_t)height;
    TBmpRGBA *rgba = (TBmpRGBA *)malloc(npix * sizeof(TBmpRGBA));
    if (!rgba) {
        fprintf(stderr, "error: out of memory\n");
        free(buf);
        return NULL;
    }

    if (type == 1) {
        /* P1: ASCII bilevel, '0'=white, '1'=black. */
        for (size_t i = 0; i < npix; i++) {
            long bit = pnm_uint(&p, end);
            if (bit < 0) {
                fprintf(stderr, "error: truncated P1 data in '%s'\n", path);
                free(rgba);
                free(buf);
                return NULL;
            }
            uint8_t v = (bit == 0) ? 255 : 0;
            rgba[i].r = v;
            rgba[i].g = v;
            rgba[i].b = v;
            rgba[i].a = 255;
        }
        *out_channels = 1;

    } else if (type == 2) {
        /* P2: ASCII grayscale. */
        for (size_t i = 0; i < npix; i++) {
            long val = pnm_uint(&p, end);
            if (val < 0) {
                fprintf(stderr, "error: truncated P2 data in '%s'\n", path);
                free(rgba);
                free(buf);
                return NULL;
            }
            /* Scale to 0-255. */
            uint8_t v = (uint8_t)((val * 255L) / maxval);
            rgba[i].r = v;
            rgba[i].g = v;
            rgba[i].b = v;
            rgba[i].a = 255;
        }
        *out_channels = 1;

    } else if (type == 3) {
        /* P3: ASCII colour. */
        for (size_t i = 0; i < npix; i++) {
            long r = pnm_uint(&p, end);
            long g = pnm_uint(&p, end);
            long b = pnm_uint(&p, end);
            if (r < 0 || g < 0 || b < 0) {
                fprintf(stderr, "error: truncated P3 data in '%s'\n", path);
                free(rgba);
                free(buf);
                return NULL;
            }
            rgba[i].r = (uint8_t)((r * 255L) / maxval);
            rgba[i].g = (uint8_t)((g * 255L) / maxval);
            rgba[i].b = (uint8_t)((b * 255L) / maxval);
            rgba[i].a = 255;
        }
        *out_channels = 3;

    } else {
        /* P4: binary bilevel, 1 bit per pixel, MSB first, rows padded to
         * byte boundary.  '0' bit = white, '1' bit = black. */
        size_t row_bytes = ((size_t)width + 7) / 8;
        size_t expected = row_bytes * (size_t)height;
        if ((size_t)(end - p) < expected) {
            fprintf(stderr, "error: truncated P4 data in '%s'\n", path);
            free(rgba);
            free(buf);
            return NULL;
        }
        const uint8_t *bits = (const uint8_t *)p;
        for (long row = 0; row < height; row++) {
            for (long col = 0; col < width; col++) {
                size_t byte_idx = (size_t)row * row_bytes + (size_t)col / 8;
                int bit_idx = 7 - (int)(col % 8);
                int bit = (bits[byte_idx] >> bit_idx) & 1;
                size_t i = (size_t)row * (size_t)width + (size_t)col;
                uint8_t v = (bit == 0) ? 255 : 0;
                rgba[i].r = v;
                rgba[i].g = v;
                rgba[i].b = v;
                rgba[i].a = 255;
            }
        }
        *out_channels = 1;
    }

    *out_w = (int)width;
    *out_h = (int)height;
    free(buf);
    return rgba;
}

/*
 * Always returns 4-channel RGBA data.  *free_with_stb is set to 1 if the
 * caller must free with stbi_image_free(), 0 if with free().
 */
static TBmpRGBA *img_load(const char *path, int *out_w, int *out_h,
                          int *out_channels, int *free_with_stb) {
    /* Peek at the magic bytes to decide which loader to use. */
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return NULL;
    }
    char magic[2] = {0, 0};
    size_t nr = fread(magic, 1, 2, f);
    fclose(f);
    if (nr < 2) {
        fprintf(stderr, "error: cannot read '%s'\n", path);
        return NULL;
    }

    if (magic[0] == 'P' && magic[1] >= '1' && magic[1] <= '4') {
        *free_with_stb = 0;
        return pnm_load(path, out_w, out_h, out_channels);
    }

    /* P5/P6 and all other formats: let stb_image handle it. */
    *free_with_stb = 1;
    uint8_t *raw = stbi_load(path, out_w, out_h, out_channels, 4);
    if (!raw) {
        fprintf(stderr, "error: cannot load '%s': %s\n", path,
                stbi_failure_reason());
        return NULL;
    }
    return (TBmpRGBA *)raw;
}

/* Argument parsing helpers. */
static const char *arg_get(int argc, char **argv, const char *flag,
                           const char *default_val) {
    for (int i = 0; i < argc - 1; i++) {
        if (strcmp(argv[i], flag) == 0)
            return argv[i + 1];
    }
    return default_val;
}

static int read_file_all(const char *path, uint8_t **out, size_t *out_len);

static int append_meta_tag(TBmpMeta *meta, const char *tag, size_t len) {
    if (meta->tag_count >= TBMP_META_MAX_TAGS) {
        fprintf(stderr, "error: too many tags (max %u)\n", TBMP_META_MAX_TAGS);
        return 1;
    }
    if (len == 0 || len > TBMP_META_TAG_MAX) {
        fprintf(stderr, "error: invalid tag length %zu (max %u)\n", len,
                TBMP_META_TAG_MAX);
        return 1;
    }

    memcpy(meta->tags[meta->tag_count], tag, len);
    meta->tags[meta->tag_count][len] = '\0';
    meta->tag_count++;
    return 0;
}

static int parse_tags_csv(TBmpMeta *meta, const char *csv) {
    const char *p = csv;
    if (csv == NULL || csv[0] == '\0') {
        fprintf(stderr, "error: --tags requires at least one value\n");
        return 1;
    }

    while (*p != '\0') {
        const char *start = p;
        while (*p != ',' && *p != '\0')
            p++;
        const char *end = p;

        while (start < end && isspace((unsigned char)*start))
            start++;
        while (end > start && isspace((unsigned char)end[-1]))
            end--;

        if (append_meta_tag(meta, start, (size_t)(end - start)) != 0)
            return 1;

        if (*p == ',')
            p++;
    }

    return 0;
}

static void mp_skip_value(TBmpMpReader *r) {
    TBmpMpTag tag = tbmp_mp_read_tag(r);
    if (tbmp_mp_reader_error(r))
        return;

    switch (tag.type) {
    case TBMP_MP_NIL:
    case TBMP_MP_BOOL:
    case TBMP_MP_UINT:
    case TBMP_MP_INT:
    case TBMP_MP_FLOAT:
    case TBMP_MP_DOUBLE:
        break;
    case TBMP_MP_STR:
        tbmp_mp_skip_bytes(r, tag.v.len);
        tbmp_mp_done_str(r);
        break;
    case TBMP_MP_BIN:
        tbmp_mp_skip_bytes(r, tag.v.len);
        tbmp_mp_done_bin(r);
        break;
    case TBMP_MP_EXT:
        tbmp_mp_skip_bytes(r, tag.v.len);
        break;
    case TBMP_MP_ARRAY:
        for (uint32_t i = 0; i < tag.v.len; i++)
            mp_skip_value(r);
        tbmp_mp_done_array(r);
        break;
    case TBMP_MP_MAP:
        for (uint32_t i = 0; i < tag.v.len * 2U; i++)
            mp_skip_value(r);
        tbmp_mp_done_map(r);
        break;
    default:
        break;
    }
}

static int validate_custom_map_blob(const uint8_t *buf, size_t len) {
    TBmpMpReader r;
    tbmp_mp_reader_init(&r, buf, len);
    TBmpMpTag tag = tbmp_mp_read_tag(&r);
    if (tbmp_mp_reader_error(&r) || tag.type != TBMP_MP_MAP)
        return 1;

    for (uint32_t i = 0; i < tag.v.len; i++) {
        TBmpMpTag key = tbmp_mp_read_tag(&r);
        if (tbmp_mp_reader_error(&r) || key.type != TBMP_MP_STR)
            return 1;
        tbmp_mp_skip_bytes(&r, key.v.len);
        tbmp_mp_done_str(&r);
        if (tbmp_mp_reader_error(&r))
            return 1;
        mp_skip_value(&r);
        if (tbmp_mp_reader_error(&r))
            return 1;
    }

    tbmp_mp_done_map(&r);
    return tbmp_mp_reader_error(&r) || r.pos != len;
}

static int parse_encode_meta_flags(int argc, char **argv, TBmpMeta *meta,
                                   int *has_meta) {
    memset(meta, 0, sizeof(*meta));
    *has_meta = 0;

    for (int i = 3; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--title") == 0 || strcmp(a, "--author") == 0 ||
            strcmp(a, "--description") == 0 || strcmp(a, "--created") == 0 ||
            strcmp(a, "--software") == 0 || strcmp(a, "--license") == 0 ||
            strcmp(a, "--tags") == 0 || strcmp(a, "--dpi") == 0 ||
            strcmp(a, "--colorspace") == 0 || strcmp(a, "--custom-map") == 0) {
            *has_meta = 1;
        }

        if (strcmp(a, "--title") == 0 || strcmp(a, "--author") == 0 ||
            strcmp(a, "--description") == 0 || strcmp(a, "--created") == 0 ||
            strcmp(a, "--software") == 0 || strcmp(a, "--license") == 0 ||
            strcmp(a, "--tags") == 0 || strcmp(a, "--dpi") == 0 ||
            strcmp(a, "--colorspace") == 0 || strcmp(a, "--custom-map") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: %s requires a value\n", a);
                return 1;
            }
            i++;
        } else {
            continue;
        }

        if (strcmp(a, "--title") == 0) {
            if (strlen(argv[i]) > TBMP_META_FIELD_MAX)
                return fprintf(stderr, "error: --title too long\n"), 1;
            strcpy(meta->title, argv[i]);
        } else if (strcmp(a, "--author") == 0) {
            if (strlen(argv[i]) > TBMP_META_FIELD_MAX)
                return fprintf(stderr, "error: --author too long\n"), 1;
            strcpy(meta->author, argv[i]);
        } else if (strcmp(a, "--description") == 0) {
            if (strlen(argv[i]) > TBMP_META_FIELD_MAX)
                return fprintf(stderr, "error: --description too long\n"), 1;
            strcpy(meta->description, argv[i]);
        } else if (strcmp(a, "--created") == 0) {
            if (strlen(argv[i]) > TBMP_META_FIELD_MAX)
                return fprintf(stderr, "error: --created too long\n"), 1;
            strcpy(meta->created, argv[i]);
        } else if (strcmp(a, "--software") == 0) {
            if (strlen(argv[i]) > TBMP_META_FIELD_MAX)
                return fprintf(stderr, "error: --software too long\n"), 1;
            strcpy(meta->software, argv[i]);
        } else if (strcmp(a, "--license") == 0) {
            if (strlen(argv[i]) > TBMP_META_FIELD_MAX)
                return fprintf(stderr, "error: --license too long\n"), 1;
            strcpy(meta->license, argv[i]);
        } else if (strcmp(a, "--colorspace") == 0) {
            if (strlen(argv[i]) > TBMP_META_FIELD_MAX)
                return fprintf(stderr, "error: --colorspace too long\n"), 1;
            strcpy(meta->colorspace, argv[i]);
        } else if (strcmp(a, "--tags") == 0) {
            if (parse_tags_csv(meta, argv[i]) != 0)
                return 1;
        } else if (strcmp(a, "--dpi") == 0) {
            char *endp = NULL;
            errno = 0;
            unsigned long v = strtoul(argv[i], &endp, 10);
            if (errno != 0 || endp == argv[i] || *endp != '\0' ||
                v > UINT32_MAX) {
                fprintf(stderr, "error: invalid --dpi value '%s'\n", argv[i]);
                return 1;
            }
            meta->has_dpi = 1;
            meta->dpi = (uint32_t)v;
        } else if (strcmp(a, "--custom-map") == 0) {
            if (meta->custom_count >= TBMP_META_MAX_CUSTOM_ITEMS) {
                fprintf(stderr, "error: too many --custom-map items (max %u)\n",
                        TBMP_META_MAX_CUSTOM_ITEMS);
                return 1;
            }

            uint8_t *raw = NULL;
            size_t raw_len = 0;
            if (read_file_all(argv[i], &raw, &raw_len) != 0)
                return 1;

            if (raw_len > TBMP_META_CUSTOM_ITEM_MAX) {
                fprintf(stderr,
                        "error: --custom-map '%s' is too large (%zu bytes, max "
                        "%u)\n",
                        argv[i], raw_len, TBMP_META_CUSTOM_ITEM_MAX);
                free(raw);
                return 1;
            }
            if (validate_custom_map_blob(raw, raw_len) != 0) {
                fprintf(
                    stderr,
                    "error: --custom-map '%s' must be a MessagePack map blob\n",
                    argv[i]);
                free(raw);
                return 1;
            }

            meta->custom[meta->custom_count].len = (uint32_t)raw_len;
            memcpy(meta->custom[meta->custom_count].data, raw, raw_len);
            meta->custom_count++;
            free(raw);
        }
    }

    if (*has_meta) {
        uint8_t tmp[8192];
        size_t tmp_len = 0;
        int rc = tbmp_meta_encode(meta, tmp, sizeof(tmp), &tmp_len);
        if (rc != TBMP_OK) {
            fprintf(
                stderr,
                "error: structured metadata is incomplete/invalid (code %d)\n",
                rc);
            return 1;
        }
    }

    return 0;
}

static TBmpPixelFormat parse_format(const char *name, uint8_t *out_depth) {
    struct {
        const char *n;
        TBmpPixelFormat f;
        uint8_t d;
    } map[] = {
        {"rgba8888", TBMP_FMT_RGBA_8888, 32}, {"rgb888", TBMP_FMT_RGB_888, 24},
        {"rgb565", TBMP_FMT_RGB_565, 16},     {"rgb555", TBMP_FMT_RGB_555, 16},
        {"rgb444", TBMP_FMT_RGB_444, 16},     {"rgb332", TBMP_FMT_RGB_332, 8},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(name, map[i].n) == 0) {
            *out_depth = map[i].d;
            return map[i].f;
        }
    }
    fprintf(stderr, "error: unknown format '%s'\n", name);
    *out_depth = 0;
    return TBMP_FMT_MAX_;
}

static TBmpEncoding parse_encoding(const char *name) {
    struct {
        const char *n;
        TBmpEncoding e;
    } map[] = {
        {"raw", TBMP_ENC_RAW},
        {"rle", TBMP_ENC_RLE},
        {"zerorange", TBMP_ENC_ZERO_RANGE},
        {"span", TBMP_ENC_SPAN},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(name, map[i].n) == 0)
            return map[i].e;
    }
    fprintf(stderr, "error: unknown encoding '%s'\n", name);
    return TBMP_ENC_MAX_;
}

static const char *format_name(TBmpPixelFormat fmt) {
    switch (fmt) {
    case TBMP_FMT_INDEX_1:
        return "index1";
    case TBMP_FMT_INDEX_2:
        return "index2";
    case TBMP_FMT_INDEX_4:
        return "index4";
    case TBMP_FMT_INDEX_8:
        return "index8";
    case TBMP_FMT_RGB_565:
        return "rgb565";
    case TBMP_FMT_RGB_555:
        return "rgb555";
    case TBMP_FMT_RGB_444:
        return "rgb444";
    case TBMP_FMT_RGB_332:
        return "rgb332";
    case TBMP_FMT_RGB_888:
        return "rgb888";
    case TBMP_FMT_RGBA_8888:
        return "rgba8888";
    case TBMP_FMT_CUSTOM:
        return "custom";
    default:
        return "unknown";
    }
}

static const char *encoding_name(TBmpEncoding enc) {
    switch (enc) {
    case TBMP_ENC_RAW:
        return "raw";
    case TBMP_ENC_ZERO_RANGE:
        return "zerorange";
    case TBMP_ENC_RLE:
        return "rle";
    case TBMP_ENC_SPAN:
        return "span";
    case TBMP_ENC_SPARSE_PIXEL:
        return "sparse";
    case TBMP_ENC_BLOCK_SPARSE:
        return "block-sparse";
    default:
        return "unknown";
    }
}

/* print_meta - display structured META fields and return field count. */
static uint32_t print_meta(const TBmpImage *img) {
    if (img->meta_len == 0 || img->meta == NULL)
        return 0;

    TBmpMeta meta;
    if (tbmp_meta_parse(img->meta, img->meta_len, &meta) != TBMP_OK) {
        tbmp_ui_status_warn("META section present but could not be parsed");
        return 0;
    }
    tbmp_ui_box_kv("title", "%s", meta.title);
    tbmp_ui_box_kv("author", "%s", meta.author);
    tbmp_ui_box_kv("description", "%s", meta.description);
    tbmp_ui_box_kv("created", "%s", meta.created);
    tbmp_ui_box_kv("software", "%s", meta.software);
    tbmp_ui_box_kv("license", "%s", meta.license);

    if (meta.has_dpi)
        tbmp_ui_box_kv("dpi", "%u", meta.dpi);
    if (meta.colorspace[0] != '\0')
        tbmp_ui_box_kv("colorspace", "%s", meta.colorspace);

    {
        char tags_line[256];
        size_t pos = 0;
        tags_line[0] = '\0';
        for (uint32_t i = 0; i < meta.tag_count; i++) {
            int n = snprintf(tags_line + pos, sizeof(tags_line) - pos, "%s%s",
                             (i == 0) ? "" : ", ", meta.tags[i]);
            if (n < 0)
                break;
            if ((size_t)n >= sizeof(tags_line) - pos) {
                pos = sizeof(tags_line) - 1U;
                break;
            }
            pos += (size_t)n;
        }
        tbmp_ui_box_kv("tags", "%s", tags_line);
    }

    tbmp_ui_box_kv("custom", "%u item(s)", meta.custom_count);
    for (uint32_t i = 0; i < meta.custom_count; i++) {
        tbmp_ui_box_linef("custom[%u]: msgpack map (%u bytes)", i,
                          meta.custom[i].len);
    }

    return 7U + (meta.has_dpi ? 1U : 0U) +
           ((meta.colorspace[0] != '\0') ? 1U : 0U) + meta.tag_count +
           meta.custom_count;
}

/*
 * is_grayscale_format - return 1 if fmt should be written as PGM P5.
 * The hook is here for future palette/grayscale format additions.
 */
static int is_grayscale_format(TBmpPixelFormat fmt) {
    (void)fmt;
    return 0;
}

/* read_file_all - load entire file into heap buffer; caller frees *out. */
static int read_file_all(const char *path, uint8_t **out, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return 1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: seek failed for '%s'\n", path);
        fclose(f);
        return 1;
    }

    long fsz = ftell(f);
    if (fsz <= 0) {
        fprintf(stderr, "error: empty file '%s'\n", path);
        fclose(f);
        return 1;
    }
    rewind(f);

    uint8_t *buf = (uint8_t *)malloc((size_t)fsz);
    if (!buf) {
        fprintf(stderr, "error: out of memory\n");
        fclose(f);
        return 1;
    }

    if (fread(buf, 1, (size_t)fsz, f) != (size_t)fsz) {
        fprintf(stderr, "error: read failed for '%s'\n", path);
        free(buf);
        fclose(f);
        return 1;
    }
    fclose(f);

    *out = buf;
    *out_len = (size_t)fsz;
    return 0;
}

/* write_file_all - write bytes to file with optional spinner progress. */
static int write_file_all(const char *path, const uint8_t *buf, size_t len,
                          const char *step_label) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s' for writing\n", path);
        return 1;
    }

    tbmp_ui_step_begin(step_label);

    TBmpUISpinner spinner;
    tbmp_ui_spinner_start(&spinner, "writing output");

    size_t off = 0;
    while (off < len) {
        size_t chunk = len - off;
        if (chunk > 65536U)
            chunk = 65536U;

        if (fwrite(buf + off, 1, chunk, f) != chunk) {
            fprintf(stderr, "error: write failed for '%s'\n", path);
            fclose(f);
            tbmp_ui_spinner_stop_error(&spinner, "write failed");
            tbmp_ui_step_end_fail();
            return 1;
        }

        off += chunk;
        tbmp_ui_spinner_tick(&spinner);
    }

    fclose(f);
    tbmp_ui_spinner_stop_success(&spinner, "output written");
    tbmp_ui_step_end_ok();
    return 0;
}

/* print_extra - list EXTRA chunk tags and lengths; return unknown count. */
static uint32_t print_extra(const TBmpImage *img) {
    if (img->extra == NULL || img->extra_len == 0) {
        tbmp_ui_box_line("none");
        return 0;
    }

    tbmp_ui_box_linef("total bytes: %zu", img->extra_len);
    size_t pos = 0;
    uint32_t idx = 0;
    uint32_t unknown = 0;
    while (pos < img->extra_len) {
        if (img->extra_len - pos < 8U) {
            tbmp_ui_box_linef("warning: trailing %zu bytes in EXTRA",
                              img->extra_len - pos);
            break;
        }

        const uint8_t *tag = img->extra + pos;
        uint32_t len = tbmp_read_u32le(img->extra + pos + 4U);
        char tag_text[5] = {(char)tag[0], (char)tag[1], (char)tag[2],
                            (char)tag[3], '\0'};
        pos += 8U;

        if ((size_t)len > img->extra_len - pos) {
            tbmp_ui_box_linef(
                "warning: chunk %u (%s) length %u exceeds remaining data", idx,
                tag_text, len);
            break;
        }

        if (memcmp(tag, "PALT", 4) == 0) {
            tbmp_ui_box_linef("[%u] %s len=%u (palette)", idx, tag_text, len);
        } else if (memcmp(tag, "MASK", 4) == 0) {
            tbmp_ui_box_linef("[%u] %s len=%u (masks)", idx, tag_text, len);
        } else {
            tbmp_ui_box_linef("[%u] %s len=%u (unknown)", idx, tag_text, len);
            unknown++;
        }

        pos += (size_t)len;
        idx++;
    }

    return unknown;
}

static void print_cmd_usage(const char *usage_line) {
    tbmp_ui_status_error("missing arguments");
    tbmp_ui_printlnf("usage: %s", usage_line);
}

static void print_general_usage(void) {
    if (tbmp_ui_is_ci()) {
        tbmp_ui_printlnf("tbmp - tBMP command-line toolkit");
        tbmp_ui_printlnf("Global options:");
        tbmp_ui_printlnf("  --ci  disable styled output (plain text mode)");
        tbmp_ui_printlnf("  --help  show this help");
        tbmp_ui_printlnf("");
        tbmp_ui_printlnf("  tbmp --ci <command> [args...]");
        tbmp_ui_printlnf("  tbmp encode <input> <output.tbmp> [--format NAME] "
                         "[--encoding NAME] [META_OPTS]");
        tbmp_ui_printlnf("  tbmp decode <input.tbmp> <output.ppm>");
        tbmp_ui_printlnf("  tbmp inspect <input.tbmp>");
        tbmp_ui_printlnf("  tbmp dump-rgba <input.tbmp> <output.rgba>");
        tbmp_ui_printlnf("");
        tbmp_ui_printlnf(
            "Input formats (encode): PBM/PGM/PPM P1-P6, PNG, BMP, JPEG, ...");
        tbmp_ui_printlnf("Pixel formats: rgba8888 (default), rgb888, rgb565, "
                         "rgb555, rgb444, rgb332");
        tbmp_ui_printlnf("  (grayscale/bilevel sources default to rgb332)");
        tbmp_ui_printlnf("Encodings: raw (default), rle, zerorange, span");
        tbmp_ui_printlnf("META_OPTS: --title --author --description --created "
                         "--software --license --tags --dpi --colorspace "
                         "--custom-map");
        tbmp_ui_printlnf(
            "inspect: print header/sections/palette/masks/meta/chunks");
        tbmp_ui_printlnf(
            "dump-rgba: dump decoded raw RGBA bytes (R,G,B,A per pixel)");
        return;
    }

    tbmp_ui_set_accent(TBMP_UI_ACCENT_CYAN);
    tbmp_ui_banner("tbmp :: tBMP command-line toolkit");

    tbmp_ui_section("Global Options");
    tbmp_ui_table_begin("Options");
    tbmp_ui_table_row("--ci", "disable styled output (plain text mode)");
    tbmp_ui_table_row("--help", "show this help");
    tbmp_ui_table_end();

    tbmp_ui_section("Commands");
    tbmp_ui_table_begin("Command Usage");
    tbmp_ui_table_row("global", "tbmp --ci <command> [args...]");
    tbmp_ui_table_row("encode", "tbmp encode <input> <output.tbmp> [--format "
                                "NAME] [--encoding NAME] [META_OPTS]");
    tbmp_ui_table_row("decode", "tbmp decode <input.tbmp> <output.ppm>");
    tbmp_ui_table_row("inspect", "tbmp inspect <input.tbmp>");
    tbmp_ui_table_row("dump-rgba", "tbmp dump-rgba <input.tbmp> <output.rgba>");
    tbmp_ui_table_end();

    tbmp_ui_section("Notes");
    tbmp_ui_box_begin("Format Notes");
    tbmp_ui_box_line(
        "Input formats (encode): PBM/PGM/PPM P1-P6, PNG, BMP, JPEG, ...");
    tbmp_ui_box_line("Pixel formats: rgba8888 (default), rgb888, rgb565, "
                     "rgb555, rgb444, rgb332");
    tbmp_ui_box_line("(grayscale/bilevel sources default to rgb332)");
    tbmp_ui_box_line("Encodings: raw (default), rle, zerorange, span");
    tbmp_ui_box_line("META_OPTS: --title --author --description --created "
                     "--software --license --tags --dpi --colorspace "
                     "--custom-map");
    tbmp_ui_box_line(
        "inspect: print header/sections/palette/masks/meta/chunks");
    tbmp_ui_box_line(
        "dump-rgba: dump decoded raw RGBA bytes (R,G,B,A per pixel)");
    tbmp_ui_box_end();
}

/* Subcommand: encode. */
static int cmd_encode(int argc, char **argv) {
    /* argv[0]="encode", argv[1]=input, argv[2]=output, argv[3..]=options */
    if (argc < 3) {
        print_cmd_usage("tbmp encode <input> <output.tbmp> [--format NAME] "
                        "[--encoding NAME] [--title T] [--author A] "
                        "[--description D] [--created C] [--software S] "
                        "[--license L] [--tags a,b] [--dpi N] "
                        "[--colorspace CS] [--custom-map FILE ...]");
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    tbmp_ui_set_accent(TBMP_UI_ACCENT_MAGENTA);
    tbmp_ui_banner("tbmp :: ENCODE");

    const char *fmt_name = arg_get(argc, argv, "--format", "rgba8888");
    const char *enc_name = arg_get(argc, argv, "--encoding", "raw");

    uint8_t bit_depth = 0;
    TBmpPixelFormat fmt = parse_format(fmt_name, &bit_depth);
    if (fmt == TBMP_FMT_MAX_)
        return 1;

    TBmpEncoding enc = parse_encoding(enc_name);
    if (enc == TBMP_ENC_MAX_)
        return 1;

    /* Load source image: pnm_load() for P1-P4, stb_image for everything
     * else (P5, P6, PNG, BMP, JPEG, ...).
     * Always returns 4-channel RGBA matching TBmpRGBA layout. */
    int w = 0, h = 0, src_channels = 0, free_with_stb = 0;
    tbmp_ui_step_begin("load input image");
    TBmpRGBA *pixels = img_load(in_path, &w, &h, &src_channels, &free_with_stb);
    if (!pixels) {
        tbmp_ui_step_end_fail();
        return 1;
    }
    tbmp_ui_step_end_ok();

    /* For bilevel (P1/P4) and grayscale (P2/P5) sources, default to rgb332
     * unless the user explicitly chose a format. */
    if (src_channels == 1 && strcmp(fmt_name, "rgba8888") == 0) {
        fmt_name = "rgb332";
        fmt = parse_format(fmt_name, &bit_depth);
    }

    TBmpFrame frame;
    frame.width = (uint16_t)w;
    frame.height = (uint16_t)h;
    frame.pixels = pixels;

    TBmpWriteParams params;
    tbmp_write_default_params(&params);
    params.encoding = enc;
    params.pixel_format = fmt;
    params.bit_depth = bit_depth;

    TBmpMeta meta;
    int has_meta = 0;
    if (parse_encode_meta_flags(argc, argv, &meta, &has_meta) != 0) {
        if (free_with_stb)
            stbi_image_free(pixels);
        else
            free(pixels);
        return 1;
    }
    if (has_meta)
        params.meta = &meta;

/* img_free - free pixel buffer using the correct allocator. */
#define img_free(ptr)                                                          \
    do {                                                                       \
        if (free_with_stb)                                                     \
            stbi_image_free(ptr);                                              \
        else                                                                   \
            free(ptr);                                                         \
    } while (0)

    /* Allocate output buffer. */
    size_t cap = tbmp_write_needed_size(&frame, &params);
    if (cap == 0) {
        fprintf(stderr, "error: could not compute output size\n");
        img_free(pixels);
        return 1;
    }
    uint8_t *out_buf = (uint8_t *)malloc(cap);
    if (!out_buf) {
        fprintf(stderr, "error: out of memory\n");
        img_free(pixels);
        return 1;
    }

    size_t written = 0;
    tbmp_ui_step_begin("encode tBMP payload");

    TBmpUISpinner spinner;
    tbmp_ui_spinner_start(&spinner, "encoding image");
    TBmpError err = tbmp_write(&frame, &params, out_buf, cap, &written);
    tbmp_ui_spinner_tick(&spinner);
    if (err != TBMP_OK) {
        tbmp_ui_spinner_stop_error(&spinner, "encode failed");
        tbmp_ui_step_end_fail();
        fprintf(stderr, "error: encode failed: %s\n", tbmp_error_str(err));
        free(out_buf);
        img_free(pixels);
        return 1;
    }
    tbmp_ui_spinner_stop_success(&spinner, "encode complete");
    tbmp_ui_step_end_ok();

    if (write_file_all(out_path, out_buf, written, "write output file") != 0) {
        free(out_buf);
        img_free(pixels);
        return 1;
    }

    tbmp_ui_status_ok("encoded successfully");
    tbmp_ui_box_begin("Encode Result");
    tbmp_ui_box_kv("input", "%s", in_path);
    tbmp_ui_box_kv("output", "%s", out_path);
    tbmp_ui_box_kv("size", "%dx%d", w, h);
    tbmp_ui_box_kv("source", "%d channels", src_channels);
    tbmp_ui_box_kv("mode", "%s / %s", fmt_name, enc_name);
    if (has_meta)
        tbmp_ui_box_kv("meta", "structured (%u tags, %u custom)",
                       meta.tag_count, meta.custom_count);
    tbmp_ui_box_kv("bytes", "%zu", written);
    tbmp_ui_box_end();

    free(out_buf);
    img_free(pixels);
#undef img_free
    return 0;
}

/* Subcommand: decode. */
static int cmd_decode(int argc, char **argv) {
    if (argc < 3) {
        print_cmd_usage("tbmp decode <input.tbmp> <output.ppm>");
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    tbmp_ui_set_accent(TBMP_UI_ACCENT_GREEN);
    tbmp_ui_banner("tbmp :: DECODE");

    uint8_t *raw = NULL;
    size_t raw_len = 0;
    tbmp_ui_step_begin("read input file");
    if (read_file_all(in_path, &raw, &raw_len) != 0) {
        tbmp_ui_step_end_fail();
        return 1;
    }
    tbmp_ui_step_end_ok();

    /* Parse the tBMP header. */
    TBmpImage img;
    tbmp_ui_step_begin("parse tBMP header");
    TBmpError err = tbmp_open(raw, raw_len, &img);
    if (err != TBMP_OK) {
        tbmp_ui_step_end_fail();
        fprintf(stderr, "error: cannot parse '%s': %s\n", in_path,
                tbmp_error_str(err));
        free(raw);
        return 1;
    }
    tbmp_ui_step_end_ok();

    /* Print META if present. */
    if (img.meta_len > 0 && img.meta != NULL) {
        tbmp_ui_box_begin("Metadata");
        (void)print_meta(&img);
        tbmp_ui_box_end();
    }

    /* Allocate RGBA pixel output buffer. */
    size_t npix = (size_t)img.head.width * (size_t)img.head.height;
    TBmpRGBA *pixels = (TBmpRGBA *)malloc(npix * sizeof(TBmpRGBA));
    if (!pixels) {
        fprintf(stderr, "error: out of memory\n");
        free(raw);
        return 1;
    }

    TBmpFrame frame;
    frame.width = img.head.width;
    frame.height = img.head.height;
    frame.pixels = pixels;

    tbmp_ui_step_begin("decode pixel data");
    TBmpUISpinner spinner;
    tbmp_ui_spinner_start(&spinner, "decoding image");
    err = tbmp_decode(&img, &frame);
    tbmp_ui_spinner_tick(&spinner);
    if (err != TBMP_OK) {
        tbmp_ui_spinner_stop_error(&spinner, "decode failed");
        tbmp_ui_step_end_fail();
        fprintf(stderr, "error: decode failed: %s\n", tbmp_error_str(err));
        free(pixels);
        free(raw);
        return 1;
    }
    tbmp_ui_spinner_stop_success(&spinner, "decode complete");
    tbmp_ui_step_end_ok();

    int rc = 0;

    tbmp_ui_step_begin("write output image");
    tbmp_ui_spinner_start(&spinner, "writing decoded image");

    if (is_grayscale_format(img.head.pixel_format)) {
        /* Write PGM P5: use R channel as luminance. */
        FILE *pg = fopen(out_path, "wb");
        if (!pg) {
            tbmp_ui_step_end_fail();
            fprintf(stderr, "error: cannot open '%s' for writing\n", out_path);
            free(pixels);
            free(raw);
            return 1;
        }
        fprintf(pg, "P5\n%u %u\n255\n", img.head.width, img.head.height);
        for (size_t i = 0; i < npix && rc == 0; i++) {
            uint8_t lum = pixels[i].r;
            if (fwrite(&lum, 1, 1, pg) != 1) {
                fprintf(stderr, "error: write failed for '%s'\n", out_path);
                rc = 1;
            }
            if ((i & 4095U) == 0U) {
                tbmp_ui_spinner_tick(&spinner);
            }
        }
        fclose(pg);
    } else {
        /* Write PPM P6. */
        FILE *pp = fopen(out_path, "wb");
        if (!pp) {
            tbmp_ui_step_end_fail();
            fprintf(stderr, "error: cannot open '%s' for writing\n", out_path);
            free(pixels);
            free(raw);
            return 1;
        }
        fprintf(pp, "P6\n%u %u\n255\n", img.head.width, img.head.height);
        for (size_t i = 0; i < npix && rc == 0; i++) {
            uint8_t rgb[3] = {pixels[i].r, pixels[i].g, pixels[i].b};
            if (fwrite(rgb, 1, 3, pp) != 3) {
                fprintf(stderr, "error: write failed for '%s'\n", out_path);
                rc = 1;
            }
            if ((i & 4095U) == 0U) {
                tbmp_ui_spinner_tick(&spinner);
            }
        }
        fclose(pp);
    }

    if (rc == 0)
        tbmp_ui_spinner_stop_success(&spinner, "write complete");
    else
        tbmp_ui_spinner_stop_error(&spinner, "write failed");

    if (rc == 0)
        tbmp_ui_step_end_ok();
    else
        tbmp_ui_step_end_fail();

    if (rc == 0) {
        tbmp_ui_status_ok("decoded successfully");
        tbmp_ui_box_begin("Decode Result");
        tbmp_ui_box_kv("input", "%s", in_path);
        tbmp_ui_box_kv("output", "%s", out_path);
        tbmp_ui_box_kv("size", "%ux%u", img.head.width, img.head.height);
        tbmp_ui_box_end();
    }

    free(pixels);
    free(raw);
    return rc;
}

/* Subcommand: dump-rgba. */
static int cmd_dump_rgba(int argc, char **argv) {
    if (argc < 3) {
        print_cmd_usage("tbmp dump-rgba <input.tbmp> <output.rgba>");
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    tbmp_ui_set_accent(TBMP_UI_ACCENT_YELLOW);
    tbmp_ui_banner("tbmp :: DUMP RGBA");

    uint8_t *raw = NULL;
    size_t raw_len = 0;
    tbmp_ui_step_begin("read input file");
    if (read_file_all(in_path, &raw, &raw_len) != 0) {
        tbmp_ui_step_end_fail();
        return 1;
    }
    tbmp_ui_step_end_ok();

    TBmpImage img;
    tbmp_ui_step_begin("parse tBMP header");
    TBmpError err = tbmp_open(raw, raw_len, &img);
    if (err != TBMP_OK) {
        tbmp_ui_step_end_fail();
        fprintf(stderr, "error: cannot parse '%s': %s\n", in_path,
                tbmp_error_str(err));
        free(raw);
        return 1;
    }
    tbmp_ui_step_end_ok();

    size_t npix = (size_t)img.head.width * (size_t)img.head.height;
    TBmpRGBA *pixels = (TBmpRGBA *)malloc(npix * sizeof(TBmpRGBA));
    if (!pixels) {
        fprintf(stderr, "error: out of memory\n");
        free(raw);
        return 1;
    }

    TBmpFrame frame;
    frame.width = img.head.width;
    frame.height = img.head.height;
    frame.pixels = pixels;

    tbmp_ui_step_begin("decode pixel data");
    TBmpUISpinner spinner;
    tbmp_ui_spinner_start(&spinner, "decoding image");
    err = tbmp_decode(&img, &frame);
    tbmp_ui_spinner_tick(&spinner);
    if (err != TBMP_OK) {
        tbmp_ui_spinner_stop_error(&spinner, "decode failed");
        tbmp_ui_step_end_fail();
        fprintf(stderr, "error: decode failed: %s\n", tbmp_error_str(err));
        free(pixels);
        free(raw);
        return 1;
    }
    tbmp_ui_spinner_stop_success(&spinner, "decode complete");
    tbmp_ui_step_end_ok();

    size_t out_size = npix * 4U;
    if (write_file_all(out_path, (const uint8_t *)pixels, out_size,
                       "write RGBA dump") != 0) {
        free(pixels);
        free(raw);
        return 1;
    }

    tbmp_ui_status_ok("raw RGBA dumped");
    tbmp_ui_box_begin("RGBA Dump");
    tbmp_ui_box_kv("input", "%s", in_path);
    tbmp_ui_box_kv("output", "%s", out_path);
    tbmp_ui_box_kv("size", "%ux%u", img.head.width, img.head.height);
    tbmp_ui_box_kv("bytes", "%zu", out_size);
    tbmp_ui_box_end();

    free(pixels);
    free(raw);
    return 0;
}

/* Subcommand: inspect. */
static int cmd_inspect(int argc, char **argv) {
    if (argc < 2) {
        print_cmd_usage("tbmp inspect <input.tbmp>");
        return 1;
    }

    const char *in_path = argv[1];

    tbmp_ui_set_accent(TBMP_UI_ACCENT_CYAN);
    tbmp_ui_banner("tbmp :: INSPECT");

    uint8_t *raw = NULL;
    size_t raw_len = 0;
    if (read_file_all(in_path, &raw, &raw_len) != 0)
        return 1;

    TBmpImage img;
    TBmpError err = tbmp_open(raw, raw_len, &img);
    if (err != TBMP_OK) {
        fprintf(stderr, "error: cannot parse '%s': %s\n", in_path,
                tbmp_error_str(err));
        free(raw);
        return 1;
    }

    tbmp_ui_box_begin("File");
    tbmp_ui_box_kv("path", "%s", in_path);
    tbmp_ui_box_end();

    tbmp_ui_box_begin("Header");
    tbmp_ui_box_kv("version", "0x%04X", img.head.version);
    tbmp_ui_box_kv("width", "%u", img.head.width);
    tbmp_ui_box_kv("height", "%u", img.head.height);
    tbmp_ui_box_kv("bit_depth", "%u", img.head.bit_depth);
    tbmp_ui_box_kv("encoding", "%u (%s)", img.head.encoding,
                   encoding_name((TBmpEncoding)img.head.encoding));
    tbmp_ui_box_kv("pixel_format", "%u (%s)", img.head.pixel_format,
                   format_name((TBmpPixelFormat)img.head.pixel_format));
    tbmp_ui_box_kv("flags", "0x%02X", img.head.flags);
    tbmp_ui_box_kv("data_size", "%u", img.head.data_size);
    tbmp_ui_box_kv("extra_size", "%u", img.head.extra_size);
    tbmp_ui_box_kv("meta_size", "%u", img.head.meta_size);
    tbmp_ui_box_end();

    tbmp_ui_box_begin("Sections");
    tbmp_ui_box_kv("DATA", "%s", img.data_len > 0 ? "yes" : "no");
    tbmp_ui_box_kv("EXTRA", "%s", img.extra_len > 0 ? "yes" : "no");
    tbmp_ui_box_kv("META", "%s", img.meta_len > 0 ? "yes" : "no");
    tbmp_ui_box_end();

    tbmp_ui_box_begin("Palette");
    if (img.has_palette) {
        tbmp_ui_box_kv("entries", "%u", img.palette.count);
    } else {
        tbmp_ui_box_line("none");
    }
    tbmp_ui_box_end();

    tbmp_ui_box_begin("Masks");
    if (img.has_masks) {
        tbmp_ui_box_kv("rgba", "r=0x%08X g=0x%08X b=0x%08X a=0x%08X",
                       img.masks.r, img.masks.g, img.masks.b, img.masks.a);
    } else {
        tbmp_ui_box_line("none");
    }
    tbmp_ui_box_end();

    tbmp_ui_box_begin("EXTRA Chunks");
    uint32_t unknown_chunks = print_extra(&img);
    tbmp_ui_box_end();

    int structured_meta_rc = TBMP_META_ERR_REQUIRED_MISSING;

    if (img.meta_len > 0 && img.meta != NULL) {
        structured_meta_rc =
            tbmp_meta_validate_structured_blob(img.meta, img.meta_len);
        tbmp_ui_box_begin("Metadata");
        (void)print_meta(&img);
        tbmp_ui_box_end();
    }

    tbmp_ui_box_begin("META");
    if (img.meta_len == 0 || img.meta == NULL) {
        tbmp_ui_box_line("not present");
    } else if (structured_meta_rc == TBMP_OK) {
        tbmp_ui_status_success("schema valid");
    } else {
        tbmp_ui_status_warn("schema invalid (code %d)", structured_meta_rc);
    }
    tbmp_ui_box_end();

    tbmp_ui_box_begin("Warnings");
    if (unknown_chunks > 0) {
        tbmp_ui_status_warn("unknown EXTRA chunks: %u", unknown_chunks);
    } else {
        tbmp_ui_status_ok("no warnings");
    }
    tbmp_ui_box_end();

    free(raw);
    return 0;
}

int main(int argc, char **argv) {
    int argi = 1;
    int ci_mode = 0;
    int show_help = 0;
    while (argi < argc && strncmp(argv[argi], "--", 2) == 0) {
        if (strcmp(argv[argi], "--ci") == 0) {
            ci_mode = 1;
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--help") == 0) {
            show_help = 1;
            argi++;
            continue;
        }
        fprintf(stderr, "error: unknown global option '%s'\n", argv[argi]);
        tbmp_ui_init(ci_mode);
        print_general_usage();
        return 1;
    }

    tbmp_ui_init(ci_mode);

    if (show_help) {
        print_general_usage();
        return 0;
    }

    if (argi >= argc) {
        print_general_usage();
        return 1;
    }

    if (strcmp(argv[argi], "encode") == 0)
        return cmd_encode(argc - argi, argv + argi);
    if (strcmp(argv[argi], "decode") == 0)
        return cmd_decode(argc - argi, argv + argi);
    if (strcmp(argv[argi], "inspect") == 0)
        return cmd_inspect(argc - argi, argv + argi);
    if (strcmp(argv[argi], "dump-rgba") == 0)
        return cmd_dump_rgba(argc - argi, argv + argi);

    fprintf(stderr, "error: unknown command '%s'\n", argv[argi]);
    print_general_usage();
    return 1;
}
