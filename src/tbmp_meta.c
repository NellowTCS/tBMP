#include "tbmp_meta.h"
#include "tbmp_msgpack.h"

#include <string.h> /* memset, memcpy, strncmp */

/* Portable bounded string length, safe on non-NUL-terminated buffers. */
static size_t tbmp_strnlen(const char *s, size_t max) {
    size_t n = 0;
    while (n < max && s[n] != '\0')
        n++;
    return n;
}

/* Internal helper: skip one complete MessagePack value.
 *
 * Used when a value type we do not store (array, nested map, ext) is
 * encountered; we must consume its bytes so the reader stays in sync. */
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
        /* scalar: tag was the whole value */
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

/* Public: tbmp_meta_parse. */

int tbmp_meta_parse(const uint8_t *blob, size_t len, TBmpMeta *out) {
    if (out == NULL)
        return TBMP_ERR_NULL_PTR;
    if (blob == NULL && len > 0)
        return TBMP_ERR_NULL_PTR;

    memset(out, 0, sizeof(*out));

    if (len == 0)
        return TBMP_OK; /* empty META: zero entries, success */

    TBmpMpReader r;
    tbmp_mp_reader_init(&r, blob, len);

    /* Expect the root object to be a map. */
    TBmpMpTag root = tbmp_mp_read_tag(&r);
    if (tbmp_mp_reader_error(&r))
        return TBMP_META_ERR_TRUNCATED;
    if (root.type != TBMP_MP_MAP) {
        return TBMP_META_ERR_NOT_A_MAP;
    }

    uint32_t map_count = root.v.len;
    int warn_trunc = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        if (tbmp_mp_reader_error(&r))
            break;

        /* Read key (must be a string). */
        TBmpMpTag key_tag = tbmp_mp_read_tag(&r);
        if (tbmp_mp_reader_error(&r))
            return TBMP_META_ERR_TRUNCATED;
        if (key_tag.type != TBMP_MP_STR)
            return TBMP_META_ERR_BAD_KEY;

        uint32_t key_len = key_tag.v.len;

        /* Read key bytes into a temp buffer; truncate silently if over limit. */
        char key_buf[TBMP_META_KEY_MAX + 1U];
        memset(key_buf, 0, sizeof(key_buf));
        if (key_len > TBMP_META_KEY_MAX) {
            tbmp_mp_read_bytes(&r, key_buf, TBMP_META_KEY_MAX);
            tbmp_mp_skip_bytes(&r, key_len - TBMP_META_KEY_MAX);
        } else {
            tbmp_mp_read_bytes(&r, key_buf, key_len);
        }
        tbmp_mp_done_str(&r);

        if (tbmp_mp_reader_error(&r))
            return TBMP_META_ERR_TRUNCATED;

        /* Determine target entry slot (drop if over max). */
        TBmpMetaEntry *entry = NULL;
        if (out->count < TBMP_META_MAX_ENTRIES) {
            entry = &out->entries[out->count];
            memset(entry, 0, sizeof(*entry));
            memcpy(entry->key, key_buf, TBMP_META_KEY_MAX);
            entry->key[TBMP_META_KEY_MAX] = '\0';
        }
        /* If entry is NULL we still consume the value to stay in sync. */

        /* Read value. */
        TBmpMpTag val_tag = tbmp_mp_read_tag(&r);
        if (tbmp_mp_reader_error(&r))
            return TBMP_META_ERR_TRUNCATED;

        TBmpMpType vtype = val_tag.type;

        if (entry != NULL) {
            switch (vtype) {
            case TBMP_MP_NIL:
                entry->value.type = TBMP_META_NIL;
                break;

            case TBMP_MP_BOOL:
                entry->value.type = TBMP_META_BOOL;
                entry->value.u = val_tag.v.b ? 1U : 0U;
                break;

            case TBMP_MP_UINT:
                entry->value.type = TBMP_META_UINT;
                entry->value.u = val_tag.v.u;
                break;

            case TBMP_MP_INT:
                entry->value.type = TBMP_META_INT;
                entry->value.i = val_tag.v.i;
                break;

            case TBMP_MP_FLOAT:
                entry->value.type = TBMP_META_FLOAT;
                entry->value.f = (double)val_tag.v.f;
                break;

            case TBMP_MP_DOUBLE:
                entry->value.type = TBMP_META_FLOAT;
                entry->value.f = val_tag.v.d;
                break;

            case TBMP_MP_STR: {
                uint32_t slen = val_tag.v.len;
                entry->value.type = TBMP_META_STR;
                uint32_t copy = slen;
                if (copy > TBMP_META_STR_MAX) {
                    copy = TBMP_META_STR_MAX;
                    warn_trunc = 1;
                }
                tbmp_mp_read_bytes(&r, entry->value.s, copy);
                if (slen > copy)
                    tbmp_mp_skip_bytes(&r, slen - copy);
                tbmp_mp_done_str(&r);
                entry->value.s[copy] = '\0';
                entry->value.bin_len = copy;
                break;
            }

            case TBMP_MP_BIN: {
                uint32_t blen = val_tag.v.len;
                entry->value.type = TBMP_META_BIN;
                uint32_t copy = blen;
                if (copy > TBMP_META_STR_MAX) {
                    copy = TBMP_META_STR_MAX;
                    warn_trunc = 1;
                }
                tbmp_mp_read_bytes(&r, entry->value.s, copy);
                if (blen > copy)
                    tbmp_mp_skip_bytes(&r, blen - copy);
                tbmp_mp_done_bin(&r);
                entry->value.bin_len = copy;
                break;
            }

            default:
                /* Unsupported type (array, map, ext): store NIL, skip bytes. */
                entry->value.type = TBMP_META_NIL;
                switch (vtype) {
                case TBMP_MP_ARRAY: {
                    uint32_t cnt = val_tag.v.len;
                    for (uint32_t j = 0; j < cnt; j++)
                        skip_value(&r);
                    tbmp_mp_done_array(&r);
                    break;
                }
                case TBMP_MP_MAP: {
                    uint32_t cnt = val_tag.v.len;
                    for (uint32_t j = 0; j < cnt * 2U; j++)
                        skip_value(&r);
                    tbmp_mp_done_map(&r);
                    break;
                }
                case TBMP_MP_EXT:
                    tbmp_mp_skip_bytes(&r, val_tag.v.len);
                    break;
                default:
                    break;
                }
                break;
            }

            if (!tbmp_mp_reader_error(&r)) {
                out->count++;
            }
        } else {
            /* Entry slot full: skip value to keep reader in sync. */
            switch (vtype) {
            case TBMP_MP_STR:
                tbmp_mp_skip_bytes(&r, val_tag.v.len);
                tbmp_mp_done_str(&r);
                break;
            case TBMP_MP_BIN:
                tbmp_mp_skip_bytes(&r, val_tag.v.len);
                tbmp_mp_done_bin(&r);
                break;
            case TBMP_MP_EXT:
                tbmp_mp_skip_bytes(&r, val_tag.v.len);
                break;
            case TBMP_MP_ARRAY: {
                uint32_t cnt = val_tag.v.len;
                for (uint32_t j = 0; j < cnt; j++)
                    skip_value(&r);
                tbmp_mp_done_array(&r);
                break;
            }
            case TBMP_MP_MAP: {
                uint32_t cnt = val_tag.v.len;
                for (uint32_t j = 0; j < cnt * 2U; j++)
                    skip_value(&r);
                tbmp_mp_done_map(&r);
                break;
            }
            default:
                break; /* scalars: tag already consumed */
            }
        }
    }

    tbmp_mp_done_map(&r);
    if (tbmp_mp_reader_error(&r))
        return TBMP_META_ERR_TRUNCATED;

    return warn_trunc ? TBMP_META_WARN_STR_TRUNC : TBMP_OK;
}

/* Public: tbmp_meta_find. */

const TBmpMetaEntry *tbmp_meta_find(const TBmpMeta *meta, const char *key) {
    if (meta == NULL || key == NULL)
        return NULL;
    for (uint32_t i = 0; i < meta->count; i++) {
        if (strncmp(meta->entries[i].key, key, TBMP_META_KEY_MAX) == 0)
            return &meta->entries[i];
    }
    return NULL;
}

/* Public: tbmp_meta_encode.
 *
 * Serialize a TBmpMeta into a caller-supplied buffer as a MessagePack map. */

int tbmp_meta_encode(const TBmpMeta *meta, uint8_t *out, size_t out_cap,
                     size_t *out_len) {
    if (out == NULL || out_len == NULL)
        return TBMP_ERR_NULL_PTR;
    *out_len = 0;

    uint32_t count = (meta != NULL) ? meta->count : 0U;

    TBmpMpWriter w;
    tbmp_mp_writer_init(&w, out, out_cap);

    tbmp_mp_start_map(&w, count);

    for (uint32_t i = 0; i < count; i++) {
        const TBmpMetaEntry *e = &meta->entries[i];

        /* Key. */
        tbmp_mp_write_cstr(&w, e->key);

        /* Value. */
        switch (e->value.type) {
        case TBMP_META_NIL:
            tbmp_mp_write_nil(&w);
            break;
        case TBMP_META_BOOL:
            tbmp_mp_write_bool(&w, e->value.u != 0U);
            break;
        case TBMP_META_UINT:
            tbmp_mp_write_uint(&w, e->value.u);
            break;
        case TBMP_META_INT:
            tbmp_mp_write_int(&w, e->value.i);
            break;
        case TBMP_META_FLOAT:
            tbmp_mp_write_double(&w, e->value.f);
            break;
        case TBMP_META_STR:
            tbmp_mp_write_str(&w, (const char *)e->value.s,
                              (uint32_t)tbmp_strnlen((const char *)e->value.s,
                                                     TBMP_META_STR_MAX));
            break;
        case TBMP_META_BIN:
            tbmp_mp_write_bin(&w, e->value.s, e->value.bin_len);
            break;
        default:
            tbmp_mp_write_nil(&w);
            break;
        }
    }

    if (tbmp_mp_writer_error(&w))
        return TBMP_ERR_OUT_OF_MEMORY;

    *out_len = tbmp_mp_writer_used(&w);
    return TBMP_OK;
}
