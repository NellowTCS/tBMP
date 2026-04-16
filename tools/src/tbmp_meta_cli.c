#include "tbmp_meta_cli.h"

#include "tbmp_meta.h"
#include "tbmp_msgpack.h"
#include "tbmp_ui.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int is_meta_flag(const char *arg) {
    return strcmp(arg, "--title") == 0 || strcmp(arg, "--author") == 0 ||
           strcmp(arg, "--description") == 0 ||
           strcmp(arg, "--created") == 0 || strcmp(arg, "--software") == 0 ||
           strcmp(arg, "--license") == 0 || strcmp(arg, "--tags") == 0 ||
           strcmp(arg, "--dpi") == 0 || strcmp(arg, "--colorspace") == 0 ||
           strcmp(arg, "--custom-map") == 0 ||
           strcmp(arg, "--meta-file") == 0;
}

int tbmp_cli_parse_encode_meta_flags(int argc, char **argv, TBmpMeta *meta,
                                     int *has_meta) {
    memset(meta, 0, sizeof(*meta));
    *has_meta = 0;

    for (int i = 3; i < argc; i++) {
        const char *a = argv[i];

        if (is_meta_flag(a)) {
            *has_meta = 1;
        }

        if (is_meta_flag(a)) {
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
        } else if (strcmp(a, "--meta-file") == 0) {
            uint8_t *raw = NULL;
            size_t raw_len = 0;
            if (read_file_all(argv[i], &raw, &raw_len) != 0)
                return 1;

            TBmpMeta parsed;
            int rc = tbmp_meta_parse(raw, raw_len, &parsed);
            free(raw);

            if (rc != TBMP_OK) {
                fprintf(stderr,
                        "error: --meta-file '%s' is not valid structured "
                        "metadata (code %d)\n",
                        argv[i], rc);
                return 1;
            }

            *meta = parsed;
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

uint32_t tbmp_cli_print_meta(const TBmpImage *img) {
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
