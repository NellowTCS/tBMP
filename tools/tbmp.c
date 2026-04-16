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
#include "tbmp_reader.h"
#include "tbmp_types.h"
#include "tbmp_write.h"

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

/* print_meta - display decoded META entries. */
static void print_meta(const TBmpImage *img) {
    if (img->meta_len == 0 || img->meta == NULL)
        return;

    TBmpMeta meta;
    if (tbmp_meta_parse(img->meta, img->meta_len, &meta) != TBMP_OK) {
        fprintf(stderr,
                "warning: META section present but could not be parsed\n");
        return;
    }
    if (meta.count == 0)
        return;

    printf("META (%u entr%s):\n", meta.count, meta.count == 1 ? "y" : "ies");
    for (uint32_t i = 0; i < meta.count; i++) {
        const TBmpMetaEntry *e = &meta.entries[i];
        printf("  %-24s = ", e->key);
        switch (e->value.type) {
        case TBMP_META_NIL:
            printf("nil");
            break;
        case TBMP_META_BOOL:
            printf("%s", e->value.u ? "true" : "false");
            break;
        case TBMP_META_UINT:
            printf("%llu", (unsigned long long)e->value.u);
            break;
        case TBMP_META_INT:
            printf("%lld", (long long)e->value.i);
            break;
        case TBMP_META_FLOAT:
            printf("%g", e->value.f);
            break;
        case TBMP_META_STR:
            printf("\"%s\"", (const char *)e->value.s);
            break;
        case TBMP_META_BIN:
            printf("<binary %u bytes>", e->value.bin_len);
            break;
        default:
            printf("?");
            break;
        }
        printf("\n");
    }
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

/* print_extra - list EXTRA chunk tags and lengths; return unknown count. */
static uint32_t print_extra(const TBmpImage *img) {
    if (img->extra == NULL || img->extra_len == 0) {
        printf("EXTRA: none\n");
        return 0;
    }

    printf("EXTRA (%zu bytes):\n", img->extra_len);
    size_t pos = 0;
    uint32_t idx = 0;
    uint32_t unknown = 0;
    while (pos < img->extra_len) {
        if (img->extra_len - pos < 8U) {
            printf("  warning: trailing %zu bytes in EXTRA\n",
                   img->extra_len - pos);
            break;
        }

        const uint8_t *tag = img->extra + pos;
        uint32_t len = tbmp_read_u32le(img->extra + pos + 4U);
        char tag_text[5] = {(char)tag[0], (char)tag[1], (char)tag[2],
                            (char)tag[3], '\0'};
        pos += 8U;

        if ((size_t)len > img->extra_len - pos) {
            printf("  warning: chunk %u (%s) length %u exceeds remaining data\n",
                   idx, tag_text, len);
            break;
        }

        printf("  [%u] %s len=%u", idx, tag_text, len);
        if (memcmp(tag, "PALT", 4) == 0) {
            printf(" (palette)\n");
        } else if (memcmp(tag, "MASK", 4) == 0) {
            printf(" (masks)\n");
        } else {
            printf(" (unknown)\n");
            unknown++;
        }

        pos += (size_t)len;
        idx++;
    }

    return unknown;
}

/* Subcommand: encode. */
static int cmd_encode(int argc, char **argv) {
    /* argv[0]="encode", argv[1]=input, argv[2]=output, argv[3..]=options */
    if (argc < 3) {
        fprintf(stderr, "usage: tbmp encode <input> <output.tbmp>"
                        " [--format NAME] [--encoding NAME]\n");
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

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
    TBmpRGBA *pixels = img_load(in_path, &w, &h, &src_channels, &free_with_stb);
    if (!pixels)
        return 1;

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
    TBmpError err = tbmp_write(&frame, &params, out_buf, cap, &written);
    if (err != TBMP_OK) {
        fprintf(stderr, "error: encode failed: %s\n", tbmp_error_str(err));
        free(out_buf);
        img_free(pixels);
        return 1;
    }

    /* Write output file. */
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s' for writing\n", out_path);
        free(out_buf);
        img_free(pixels);
        return 1;
    }
    if (fwrite(out_buf, 1, written, f) != written) {
        fprintf(stderr, "error: write failed for '%s'\n", out_path);
        fclose(f);
        free(out_buf);
        img_free(pixels);
        return 1;
    }
    fclose(f);

    printf("encoded: %dx%d (src channels: %d) %s/%s -> %s (%zu bytes)\n", w, h,
           src_channels, fmt_name, enc_name, out_path, written);

    free(out_buf);
    img_free(pixels);
#undef img_free
    return 0;
}

/* Subcommand: decode. */
static int cmd_decode(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: tbmp decode <input.tbmp> <output.ppm>\n");
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    uint8_t *raw = NULL;
    size_t raw_len = 0;
    if (read_file_all(in_path, &raw, &raw_len) != 0)
        return 1;

    /* Parse the tBMP header. */
    TBmpImage img;
    TBmpError err = tbmp_open(raw, raw_len, &img);
    if (err != TBMP_OK) {
        fprintf(stderr, "error: cannot parse '%s': %s\n", in_path,
                tbmp_error_str(err));
        free(raw);
        return 1;
    }

    /* Print META if present. */
    print_meta(&img);

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

    err = tbmp_decode(&img, &frame);
    if (err != TBMP_OK) {
        fprintf(stderr, "error: decode failed: %s\n", tbmp_error_str(err));
        free(pixels);
        free(raw);
        return 1;
    }

    int rc = 0;

    if (is_grayscale_format(img.head.pixel_format)) {
        /* Write PGM P5: use R channel as luminance. */
        FILE *pg = fopen(out_path, "wb");
        if (!pg) {
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
        }
        fclose(pg);
    } else {
        /* Write PPM P6. */
        FILE *pp = fopen(out_path, "wb");
        if (!pp) {
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
        }
        fclose(pp);
    }

    if (rc == 0)
        printf("decoded: %ux%u -> %s\n", img.head.width, img.head.height,
               out_path);

    free(pixels);
    free(raw);
    return rc;
}

/* Subcommand: dump-rgba. */
static int cmd_dump_rgba(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
                "usage: tbmp dump-rgba <input.tbmp> <output.rgba>\n");
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

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

    err = tbmp_decode(&img, &frame);
    if (err != TBMP_OK) {
        fprintf(stderr, "error: decode failed: %s\n", tbmp_error_str(err));
        free(pixels);
        free(raw);
        return 1;
    }

    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s' for writing\n", out_path);
        free(pixels);
        free(raw);
        return 1;
    }

    size_t out_size = npix * 4U;
    if (fwrite((const uint8_t *)pixels, 1, out_size, f) != out_size) {
        fprintf(stderr, "error: write failed for '%s'\n", out_path);
        fclose(f);
        free(pixels);
        free(raw);
        return 1;
    }
    fclose(f);

    printf("dumped RGBA: %ux%u -> %s (%zu bytes)\n", img.head.width,
           img.head.height, out_path, out_size);

    free(pixels);
    free(raw);
    return 0;
}

/* Subcommand: inspect. */
static int cmd_inspect(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: tbmp inspect <input.tbmp>\n");
        return 1;
    }

    const char *in_path = argv[1];

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

    printf("file: %s\n", in_path);
    printf("header:\n");
    printf("  version      : 0x%04X\n", img.head.version);
    printf("  width        : %u\n", img.head.width);
    printf("  height       : %u\n", img.head.height);
    printf("  bit_depth    : %u\n", img.head.bit_depth);
    printf("  encoding     : %u (%s)\n", img.head.encoding,
           encoding_name((TBmpEncoding)img.head.encoding));
    printf("  pixel_format : %u (%s)\n", img.head.pixel_format,
           format_name((TBmpPixelFormat)img.head.pixel_format));
    printf("  flags        : 0x%02X\n", img.head.flags);
    printf("  data_size    : %u\n", img.head.data_size);
    printf("  extra_size   : %u\n", img.head.extra_size);
    printf("  meta_size    : %u\n", img.head.meta_size);

    printf("sections:\n");
    printf("  DATA present : %s\n", img.data_len > 0 ? "yes" : "no");
    printf("  EXTRA present: %s\n", img.extra_len > 0 ? "yes" : "no");
    printf("  META present : %s\n", img.meta_len > 0 ? "yes" : "no");

    if (img.has_palette) {
        printf("palette:\n");
        printf("  entries      : %u\n", img.palette.count);
    } else {
        printf("palette: none\n");
    }

    if (img.has_masks) {
        printf("masks:\n");
        printf("  r=0x%08X g=0x%08X b=0x%08X a=0x%08X\n", img.masks.r,
               img.masks.g, img.masks.b, img.masks.a);
    } else {
        printf("masks: none\n");
    }

    uint32_t unknown_chunks = print_extra(&img);
    print_meta(&img);

    printf("warnings:\n");
    if (unknown_chunks > 0) {
        printf("  unknown EXTRA chunks: %u\n", unknown_chunks);
    } else {
        printf("  none\n");
    }

    free(raw);
    return 0;
}

/* main. */
static void usage(void) {
    fprintf(stderr,
            "tbmp - tBMP command-line toolkit\n"
            "\n"
            "  tbmp encode <input>      <output.tbmp>"
            " [--format NAME] [--encoding NAME]\n"
            "  tbmp decode <input.tbmp> <output.ppm>\n"
            "  tbmp inspect <input.tbmp>\n"
            "  tbmp dump-rgba <input.tbmp> <output.rgba>\n"
            "\n"
            "Input formats (encode): PBM/PGM/PPM P1-P6, PNG, BMP, JPEG, ...\n"
            "Pixel formats: rgba8888 (default), rgb888, rgb565, rgb555,"
            " rgb444, rgb332\n"
            "  (grayscale/bilevel sources default to rgb332)\n"
            "Encodings:     raw (default), rle, zerorange, span\n"
            "inspect:       print header/sections/palette/masks/meta/chunks\n"
            "dump-rgba:     dump decoded raw RGBA bytes (R,G,B,A per pixel)\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "encode") == 0)
        return cmd_encode(argc - 1, argv + 1);
    if (strcmp(argv[1], "decode") == 0)
        return cmd_decode(argc - 1, argv + 1);
    if (strcmp(argv[1], "inspect") == 0)
        return cmd_inspect(argc - 1, argv + 1);
    if (strcmp(argv[1], "dump-rgba") == 0)
        return cmd_dump_rgba(argc - 1, argv + 1);

    fprintf(stderr, "error: unknown command '%s'\n", argv[1]);
    usage();
    return 1;
}
