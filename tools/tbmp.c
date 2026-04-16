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

#include "tbmp_decode.h"
#include "tbmp_imgio.h"
#include "tbmp_meta.h"
#include "tbmp_meta_cli.h"
#include "tbmp_pngio.h"
#include "tbmp_reader.h"
#include "tbmp_types.h"
#include "tbmp_ui.h"
#include "tbmp_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void scan_extra_chunks(const TBmpImage *img, uint32_t *unknown_out,
                              int *malformed_out) {
    size_t pos = 0;
    uint32_t unknown = 0;
    int malformed = 0;

    while (pos < img->extra_len) {
        if (img->extra_len - pos < 8U) {
            malformed = 1;
            break;
        }

        const uint8_t *tag = img->extra + pos;
        uint32_t len = tbmp_read_u32le(img->extra + pos + 4U);
        pos += 8U;

        if ((size_t)len > img->extra_len - pos) {
            malformed = 1;
            break;
        }

        if (memcmp(tag, "PALT", 4) != 0 && memcmp(tag, "MASK", 4) != 0)
            unknown++;

        pos += (size_t)len;
    }

    *unknown_out = unknown;
    *malformed_out = malformed;
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
        tbmp_ui_printlnf("  tbmp export-png <input.tbmp> <output.png>");
        tbmp_ui_printlnf("  tbmp validate <input.tbmp> [--strict]");
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
                         "--custom-map --meta-file");
        tbmp_ui_printlnf(
            "inspect: print header/sections/palette/masks/meta/chunks");
        tbmp_ui_printlnf(
            "dump-rgba: dump decoded raw RGBA bytes (R,G,B,A per pixel)");
        tbmp_ui_printlnf(
            "export-png: decode and write PNG (debugging / preview)");
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
    tbmp_ui_table_row("export-png",
                      "tbmp export-png <input.tbmp> <output.png>");
    tbmp_ui_table_row("validate", "tbmp validate <input.tbmp> [--strict]");
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
                     "--custom-map --meta-file");
    tbmp_ui_box_line(
        "inspect: print header/sections/palette/masks/meta/chunks");
    tbmp_ui_box_line(
        "dump-rgba: dump decoded raw RGBA bytes (R,G,B,A per pixel)");
    tbmp_ui_box_line("export-png: decode and write PNG (debugging / preview)");
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
                        "[--colorspace CS] [--custom-map FILE ...] "
                        "[--meta-file META.msgpack]");
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

    /* Always returns 4-channel RGBA matching TBmpRGBA layout. */
    int w = 0, h = 0, src_channels = 0, free_with_stb = 0;
    tbmp_ui_step_begin("load input image");
    TBmpRGBA *pixels =
        tbmp_cli_img_load(in_path, &w, &h, &src_channels, &free_with_stb);
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
    if (tbmp_cli_parse_encode_meta_flags(argc, argv, &meta, &has_meta) != 0) {
        tbmp_cli_img_free(pixels, free_with_stb);
        return 1;
    }
    if (has_meta)
        params.meta = &meta;

    /* Allocate output buffer. */
    size_t cap = tbmp_write_needed_size(&frame, &params);
    if (cap == 0) {
        fprintf(stderr, "error: could not compute output size\n");
        tbmp_cli_img_free(pixels, free_with_stb);
        return 1;
    }
    uint8_t *out_buf = (uint8_t *)malloc(cap);
    if (!out_buf) {
        fprintf(stderr, "error: out of memory\n");
        tbmp_cli_img_free(pixels, free_with_stb);
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
        tbmp_cli_img_free(pixels, free_with_stb);
        return 1;
    }
    tbmp_ui_spinner_stop_success(&spinner, "encode complete");
    tbmp_ui_step_end_ok();

    if (write_file_all(out_path, out_buf, written, "write output file") != 0) {
        free(out_buf);
        tbmp_cli_img_free(pixels, free_with_stb);
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
    tbmp_cli_img_free(pixels, free_with_stb);
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
        (void)tbmp_cli_print_meta(&img);
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

/* Subcommand: export-png. */
static int cmd_export_png(int argc, char **argv) {
    if (argc < 3) {
        print_cmd_usage("tbmp export-png <input.tbmp> <output.png>");
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    tbmp_ui_set_accent(TBMP_UI_ACCENT_GREEN);
    tbmp_ui_banner("tbmp :: EXPORT PNG");

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

    tbmp_ui_step_begin("write PNG file");
    int rc =
        tbmp_cli_write_png_rgba(out_path, frame.width, frame.height,
                                (const uint8_t *)frame.pixels);
    if (rc != 0) {
        tbmp_ui_step_end_fail();
        free(pixels);
        free(raw);
        return 1;
    }
    tbmp_ui_step_end_ok();

    tbmp_ui_status_ok("png export complete");
    tbmp_ui_box_begin("PNG Export Result");
    tbmp_ui_box_kv("input", "%s", in_path);
    tbmp_ui_box_kv("output", "%s", out_path);
    tbmp_ui_box_kv("size", "%ux%u", img.head.width, img.head.height);
    tbmp_ui_box_end();

    free(pixels);
    free(raw);
    return 0;
}

/* Subcommand: validate. */
static int cmd_validate(int argc, char **argv) {
    if (argc < 2) {
        print_cmd_usage("tbmp validate <input.tbmp> [--strict]");
        return 1;
    }

    const char *in_path = argv[1];
    int strict_mode = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--strict") == 0) {
            strict_mode = 1;
        } else {
            fprintf(stderr, "error: unknown validate option '%s'\n", argv[i]);
            return 1;
        }
    }

    tbmp_ui_set_accent(TBMP_UI_ACCENT_CYAN);
    tbmp_ui_banner("tbmp :: VALIDATE");

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

    uint32_t unknown_extra = 0;
    int malformed_extra = 0;
    scan_extra_chunks(&img, &unknown_extra, &malformed_extra);

    int meta_rc = TBMP_OK;
    if (img.meta_len > 0 && img.meta != NULL)
        meta_rc = tbmp_meta_validate_structured_blob(img.meta, img.meta_len);

    int fail = 0;
    if (strict_mode) {
        if (malformed_extra)
            fail = 1;
        if (unknown_extra > 0)
            fail = 1;
        if (img.meta_len > 0 && meta_rc != TBMP_OK)
            fail = 1;
    }

    tbmp_ui_box_begin("Validation Result");
    tbmp_ui_box_kv("file", "%s", in_path);
    tbmp_ui_box_kv("mode", "%s", strict_mode ? "strict" : "basic");
    tbmp_ui_box_kv("parse", "%s", "ok");
    tbmp_ui_box_kv("unknown_extra", "%u", unknown_extra);
    tbmp_ui_box_kv("malformed_extra", "%s", malformed_extra ? "yes" : "no");
    if (img.meta_len > 0)
        tbmp_ui_box_kv("meta_schema", "%s",
                       (meta_rc == TBMP_OK) ? "valid" : "invalid");
    else
        tbmp_ui_box_kv("meta_schema", "%s", "not present");
    tbmp_ui_box_end();

    if (fail) {
        tbmp_ui_status_error("validation failed");
        free(raw);
        return 1;
    }

    tbmp_ui_status_success("validation passed");
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
        (void)tbmp_cli_print_meta(&img);
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
    if (strcmp(argv[argi], "export-png") == 0)
        return cmd_export_png(argc - argi, argv + argi);
    if (strcmp(argv[argi], "validate") == 0)
        return cmd_validate(argc - argi, argv + argi);
    if (strcmp(argv[argi], "inspect") == 0)
        return cmd_inspect(argc - argi, argv + argi);
    if (strcmp(argv[argi], "dump-rgba") == 0)
        return cmd_dump_rgba(argc - argi, argv + argi);

    fprintf(stderr, "error: unknown command '%s'\n", argv[argi]);
    print_general_usage();
    return 1;
}
