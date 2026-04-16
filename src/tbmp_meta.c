#include "tbmp_meta.h"
#include "tbmp_msgpack.h"

#include <string.h>

/* Skip one complete MessagePack value to keep reader in sync. */
static void skip_value(TBmpMpReader *r) {
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

    case TBMP_MP_ARRAY: {
        uint32_t cnt = tag.v.len;
        for (uint32_t i = 0; i < cnt; i++)
            skip_value(r);
        tbmp_mp_done_array(r);
        break;
    }

    case TBMP_MP_MAP: {
        uint32_t cnt = tag.v.len;
        for (uint32_t i = 0; i < cnt * 2U; i++)
            skip_value(r);
        tbmp_mp_done_map(r);
        break;
    }

    default:
        break;
    }
}

typedef enum TBmpStructuredKey {
    TBMP_STRUCT_KEY_TITLE = 0,
    TBMP_STRUCT_KEY_AUTHOR,
    TBMP_STRUCT_KEY_DESCRIPTION,
    TBMP_STRUCT_KEY_CREATED,
    TBMP_STRUCT_KEY_SOFTWARE,
    TBMP_STRUCT_KEY_LICENSE,
    TBMP_STRUCT_KEY_TAGS,
    TBMP_STRUCT_KEY_DPI,
    TBMP_STRUCT_KEY_COLORSPACE,
    TBMP_STRUCT_KEY_CUSTOM,
    TBMP_STRUCT_KEY_UNKNOWN
} TBmpStructuredKey;

static TBmpStructuredKey structured_key_id(const char *key) {
    if (strcmp(key, "title") == 0)
        return TBMP_STRUCT_KEY_TITLE;
    if (strcmp(key, "author") == 0)
        return TBMP_STRUCT_KEY_AUTHOR;
    if (strcmp(key, "description") == 0)
        return TBMP_STRUCT_KEY_DESCRIPTION;
    if (strcmp(key, "created") == 0)
        return TBMP_STRUCT_KEY_CREATED;
    if (strcmp(key, "software") == 0)
        return TBMP_STRUCT_KEY_SOFTWARE;
    if (strcmp(key, "license") == 0)
        return TBMP_STRUCT_KEY_LICENSE;
    if (strcmp(key, "tags") == 0)
        return TBMP_STRUCT_KEY_TAGS;
    if (strcmp(key, "dpi") == 0)
        return TBMP_STRUCT_KEY_DPI;
    if (strcmp(key, "colorspace") == 0)
        return TBMP_STRUCT_KEY_COLORSPACE;
    if (strcmp(key, "custom") == 0)
        return TBMP_STRUCT_KEY_CUSTOM;
    return TBMP_STRUCT_KEY_UNKNOWN;
}

static int read_key(TBmpMpReader *r, char *dst, uint32_t dst_max) {
    TBmpMpTag key_tag = tbmp_mp_read_tag(r);
    if (tbmp_mp_reader_error(r))
        return TBMP_META_ERR_TRUNCATED;
    if (key_tag.type != TBMP_MP_STR)
        return TBMP_META_ERR_BAD_KEY;

    if (key_tag.v.len > dst_max)
        return TBMP_META_ERR_LENGTH_OUT_OF_RANGE;

    memset(dst, 0, (size_t)dst_max + 1U);
    tbmp_mp_read_bytes(r, dst, key_tag.v.len);
    tbmp_mp_done_str(r);
    if (tbmp_mp_reader_error(r))
        return TBMP_META_ERR_TRUNCATED;

    return TBMP_OK;
}

static int read_nonempty_string(TBmpMpReader *r, TBmpMpTag tag, char *dst,
                                uint32_t dst_max) {
    if (tag.type != TBMP_MP_STR)
        return TBMP_META_ERR_TYPE_MISMATCH;
    if (tag.v.len == 0 || tag.v.len > dst_max)
        return TBMP_META_ERR_LENGTH_OUT_OF_RANGE;

    memset(dst, 0, (size_t)dst_max + 1U);
    tbmp_mp_read_bytes(r, dst, tag.v.len);
    tbmp_mp_done_str(r);
    if (tbmp_mp_reader_error(r))
        return TBMP_META_ERR_TRUNCATED;

    return TBMP_OK;
}

static int parse_tags(TBmpMpReader *r, TBmpMpTag tag, TBmpMeta *out) {
    if (tag.type != TBMP_MP_ARRAY)
        return TBMP_META_ERR_TYPE_MISMATCH;
    if (tag.v.len == 0 || tag.v.len > TBMP_META_MAX_TAGS)
        return TBMP_META_ERR_LENGTH_OUT_OF_RANGE;

    out->tag_count = tag.v.len;
    for (uint32_t i = 0; i < out->tag_count; i++) {
        TBmpMpTag t = tbmp_mp_read_tag(r);
        int rc = read_nonempty_string(r, t, out->tags[i], TBMP_META_TAG_MAX);
        if (rc != TBMP_OK)
            return rc;
    }

    tbmp_mp_done_array(r);
    return tbmp_mp_reader_error(r) ? TBMP_META_ERR_TRUNCATED : TBMP_OK;
}

static int parse_custom(TBmpMpReader *r, TBmpMpTag tag, const uint8_t *blob,
                        TBmpMeta *out) {
    if (tag.type != TBMP_MP_ARRAY)
        return TBMP_META_ERR_TYPE_MISMATCH;
    if (tag.v.len > TBMP_META_MAX_CUSTOM_ITEMS)
        return TBMP_META_ERR_LENGTH_OUT_OF_RANGE;

    out->custom_count = tag.v.len;
    for (uint32_t i = 0; i < out->custom_count; i++) {
        size_t item_start = r->pos;
        TBmpMpTag item = tbmp_mp_read_tag(r);
        if (tbmp_mp_reader_error(r))
            return TBMP_META_ERR_TRUNCATED;
        if (item.type != TBMP_MP_MAP)
            return TBMP_META_ERR_TYPE_MISMATCH;

        uint32_t pair_count = item.v.len;
        for (uint32_t j = 0; j < pair_count; j++) {
            char tmp_key[TBMP_META_KEY_MAX + 1U];
            int rc = read_key(r, tmp_key, TBMP_META_KEY_MAX);
            if (rc != TBMP_OK)
                return rc;
            skip_value(r);
            if (tbmp_mp_reader_error(r))
                return TBMP_META_ERR_TRUNCATED;
        }

        tbmp_mp_done_map(r);
        if (tbmp_mp_reader_error(r))
            return TBMP_META_ERR_TRUNCATED;

        size_t item_len = r->pos - item_start;
        if (item_len == 0 || item_len > TBMP_META_CUSTOM_ITEM_MAX)
            return TBMP_META_ERR_LENGTH_OUT_OF_RANGE;

        out->custom[i].len = (uint32_t)item_len;
        memcpy(out->custom[i].data, blob + item_start, item_len);
    }

    tbmp_mp_done_array(r);
    return tbmp_mp_reader_error(r) ? TBMP_META_ERR_TRUNCATED : TBMP_OK;
}

static int validate_required_fields(const TBmpMeta *meta) {
    if (meta->title[0] == '\0' || meta->author[0] == '\0' ||
        meta->description[0] == '\0' || meta->created[0] == '\0' ||
        meta->software[0] == '\0' || meta->license[0] == '\0' ||
        meta->tag_count == 0) {
        return TBMP_META_ERR_REQUIRED_MISSING;
    }
    return TBMP_OK;
}

int tbmp_meta_parse(const uint8_t *blob, size_t len, TBmpMeta *out) {
    if (out == NULL)
        return TBMP_ERR_NULL_PTR;
    if (blob == NULL && len > 0)
        return TBMP_ERR_NULL_PTR;

    memset(out, 0, sizeof(*out));

    if (len == 0)
        return TBMP_META_ERR_REQUIRED_MISSING;

    TBmpMpReader r;
    tbmp_mp_reader_init(&r, blob, len);

    TBmpMpTag root = tbmp_mp_read_tag(&r);
    if (tbmp_mp_reader_error(&r))
        return TBMP_META_ERR_TRUNCATED;
    if (root.type != TBMP_MP_MAP)
        return TBMP_META_ERR_NOT_A_MAP;

    uint32_t seen_mask = 0U;

    for (uint32_t i = 0; i < root.v.len; i++) {
        char key[TBMP_META_KEY_MAX + 1U];
        int rc = read_key(&r, key, TBMP_META_KEY_MAX);
        if (rc != TBMP_OK)
            return rc;

        TBmpStructuredKey key_id = structured_key_id(key);
        if (key_id == TBMP_STRUCT_KEY_UNKNOWN)
            return TBMP_META_ERR_SCHEMA_UNKNOWN_KEY;

        uint32_t bit = 1U << (uint32_t)key_id;
        if ((seen_mask & bit) != 0U)
            return TBMP_META_ERR_SCHEMA_DUPLICATE_KEY;
        seen_mask |= bit;

        TBmpMpTag value = tbmp_mp_read_tag(&r);
        if (tbmp_mp_reader_error(&r))
            return TBMP_META_ERR_TRUNCATED;

        switch (key_id) {
        case TBMP_STRUCT_KEY_TITLE:
            rc = read_nonempty_string(&r, value, out->title,
                                      TBMP_META_FIELD_MAX);
            break;
        case TBMP_STRUCT_KEY_AUTHOR:
            rc = read_nonempty_string(&r, value, out->author,
                                      TBMP_META_FIELD_MAX);
            break;
        case TBMP_STRUCT_KEY_DESCRIPTION:
            rc = read_nonempty_string(&r, value, out->description,
                                      TBMP_META_FIELD_MAX);
            break;
        case TBMP_STRUCT_KEY_CREATED:
            rc = read_nonempty_string(&r, value, out->created,
                                      TBMP_META_FIELD_MAX);
            break;
        case TBMP_STRUCT_KEY_SOFTWARE:
            rc = read_nonempty_string(&r, value, out->software,
                                      TBMP_META_FIELD_MAX);
            break;
        case TBMP_STRUCT_KEY_LICENSE:
            rc = read_nonempty_string(&r, value, out->license,
                                      TBMP_META_FIELD_MAX);
            break;
        case TBMP_STRUCT_KEY_TAGS:
            rc = parse_tags(&r, value, out);
            break;
        case TBMP_STRUCT_KEY_DPI:
            if (value.type == TBMP_MP_UINT) {
                if (value.v.u > UINT32_MAX)
                    rc = TBMP_META_ERR_LENGTH_OUT_OF_RANGE;
                else {
                    out->has_dpi = 1U;
                    out->dpi = (uint32_t)value.v.u;
                    rc = TBMP_OK;
                }
            } else if (value.type == TBMP_MP_INT) {
                if (value.v.i < 0 || value.v.i > (int64_t)UINT32_MAX)
                    rc = TBMP_META_ERR_TYPE_MISMATCH;
                else {
                    out->has_dpi = 1U;
                    out->dpi = (uint32_t)value.v.i;
                    rc = TBMP_OK;
                }
            } else {
                rc = TBMP_META_ERR_TYPE_MISMATCH;
            }
            break;
        case TBMP_STRUCT_KEY_COLORSPACE:
            rc = read_nonempty_string(&r, value, out->colorspace,
                                      TBMP_META_FIELD_MAX);
            break;
        case TBMP_STRUCT_KEY_CUSTOM:
            rc = parse_custom(&r, value, blob, out);
            break;
        case TBMP_STRUCT_KEY_UNKNOWN:
        default:
            rc = TBMP_META_ERR_SCHEMA_UNKNOWN_KEY;
            break;
        }

        if (rc != TBMP_OK)
            return rc;
    }

    tbmp_mp_done_map(&r);
    if (tbmp_mp_reader_error(&r))
        return TBMP_META_ERR_TRUNCATED;

    return validate_required_fields(out);
}

int tbmp_meta_validate_structured_blob(const uint8_t *blob, size_t len) {
    TBmpMeta meta;
    return tbmp_meta_parse(blob, len, &meta);
}

int tbmp_meta_encode(const TBmpMeta *meta, uint8_t *out, size_t out_cap,
                     size_t *out_len) {
    if (meta == NULL || out == NULL || out_len == NULL)
        return TBMP_ERR_NULL_PTR;

    int rc = validate_required_fields(meta);
    if (rc != TBMP_OK)
        return rc;

    TBmpMpWriter w;
    tbmp_mp_writer_init(&w, out, out_cap);

    uint32_t map_count = 7U;
    if (meta->has_dpi)
        map_count++;
    if (meta->colorspace[0] != '\0')
        map_count++;
    if (meta->custom_count > 0)
        map_count++;

    tbmp_mp_start_map(&w, map_count);

    tbmp_mp_write_cstr(&w, "title");
    tbmp_mp_write_cstr(&w, meta->title);

    tbmp_mp_write_cstr(&w, "author");
    tbmp_mp_write_cstr(&w, meta->author);

    tbmp_mp_write_cstr(&w, "description");
    tbmp_mp_write_cstr(&w, meta->description);

    tbmp_mp_write_cstr(&w, "created");
    tbmp_mp_write_cstr(&w, meta->created);

    tbmp_mp_write_cstr(&w, "software");
    tbmp_mp_write_cstr(&w, meta->software);

    tbmp_mp_write_cstr(&w, "license");
    tbmp_mp_write_cstr(&w, meta->license);

    tbmp_mp_write_cstr(&w, "tags");
    tbmp_mp_start_array(&w, meta->tag_count);
    for (uint32_t i = 0; i < meta->tag_count; i++)
        tbmp_mp_write_cstr(&w, meta->tags[i]);

    if (meta->has_dpi) {
        tbmp_mp_write_cstr(&w, "dpi");
        tbmp_mp_write_uint(&w, meta->dpi);
    }

    if (meta->colorspace[0] != '\0') {
        tbmp_mp_write_cstr(&w, "colorspace");
        tbmp_mp_write_cstr(&w, meta->colorspace);
    }

    if (meta->custom_count > 0) {
        tbmp_mp_write_cstr(&w, "custom");
        tbmp_mp_start_array(&w, meta->custom_count);
        for (uint32_t i = 0; i < meta->custom_count; i++) {
            if (meta->custom[i].len == 0 ||
                meta->custom[i].len > TBMP_META_CUSTOM_ITEM_MAX)
                return TBMP_META_ERR_LENGTH_OUT_OF_RANGE;
            tbmp_mp_write_raw(&w, meta->custom[i].data, meta->custom[i].len);
        }
    }

    if (tbmp_mp_writer_error(&w))
        return TBMP_ERR_OUT_OF_MEMORY;

    *out_len = tbmp_mp_writer_used(&w);
    return TBMP_OK;
}
