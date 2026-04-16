#include "test_helpers.h"
#include "test_suites.h"
#include "tbmp_meta.h"
#include "tbmp_reader.h"
#include "tbmp_write.h"

#include <string.h>

static int meta_equal(const TBmpMeta *a, const TBmpMeta *b) {
    if (a == NULL || b == NULL)
        return 0;
    if (a->count != b->count)
        return 0;

    for (uint32_t i = 0; i < a->count; i++) {
        const TBmpMetaEntry *ea = &a->entries[i];
        const TBmpMetaEntry *eb = &b->entries[i];

        if (strncmp(ea->key, eb->key, TBMP_META_KEY_MAX + 1U) != 0)
            return 0;
        if (ea->value.type != eb->value.type)
            return 0;

        switch (ea->value.type) {
        case TBMP_META_NIL:
            break;
        case TBMP_META_BOOL:
        case TBMP_META_UINT:
            if (ea->value.u != eb->value.u)
                return 0;
            break;
        case TBMP_META_INT:
            if (ea->value.i != eb->value.i)
                return 0;
            break;
        case TBMP_META_FLOAT:
            if (ea->value.f != eb->value.f)
                return 0;
            break;
        case TBMP_META_STR:
            if (strncmp((const char *)ea->value.s, (const char *)eb->value.s,
                        TBMP_META_STR_MAX + 1U) != 0)
                return 0;
            break;
        case TBMP_META_BIN:
            if (ea->value.bin_len != eb->value.bin_len)
                return 0;
            if (memcmp(ea->value.s, eb->value.s, ea->value.bin_len) != 0)
                return 0;
            break;
        default:
            return 0;
        }
    }

    return 1;
}

void test_meta(void) {
    SUITE("META");

    /* tbmp_meta_parse: NULL out returns error */
    {
        uint8_t blob[1] = {0x80}; /* fixmap size 0 */
        CHECK_ERR(tbmp_meta_parse(blob, 1, NULL), TBMP_ERR_NULL_PTR);
    }

    /* tbmp_meta_parse: NULL blob with len>0 returns error */
    {
        TBmpMeta m;
        CHECK_ERR(tbmp_meta_parse(NULL, 4, &m), TBMP_ERR_NULL_PTR);
    }

    /* tbmp_meta_parse: empty blob (len=0) -> zero entries, OK */
    {
        TBmpMeta m;
        CHECK_OK(tbmp_meta_parse(NULL, 0, &m));
        CHECK_EQ(m.count, 0U);
    }

    /* tbmp_meta_parse: fixmap {} (0x80) -> zero entries */
    {
        uint8_t blob[1] = {0x80};
        TBmpMeta m;
        CHECK_OK(tbmp_meta_parse(blob, 1, &m));
        CHECK_EQ(m.count, 0U);
    }

    /* tbmp_meta_parse: root not a map -> TBMP_META_ERR_NOT_A_MAP */
    {
        uint8_t blob[1] = {0x01}; /* fixint 1, not a map */
        TBmpMeta m;
        CHECK_ERR(tbmp_meta_parse(blob, 1, &m), TBMP_META_ERR_NOT_A_MAP);
    }

    /* tbmp_meta_parse: fixmap with integer key -> TBMP_META_ERR_BAD_KEY */
    {
        /* {1: true} - key is int, not str */
        uint8_t blob[] = {0x81, 0x01, 0xc3};
        TBmpMeta m;
        CHECK_ERR(tbmp_meta_parse(blob, sizeof(blob), &m),
                  TBMP_META_ERR_BAD_KEY);
    }

    /* tbmp_meta_parse: truncated blob -> TBMP_META_ERR_TRUNCATED */
    {
        /* fixmap size 1 but no data follows */
        uint8_t blob[1] = {0x81};
        TBmpMeta m;
        CHECK_ERR(tbmp_meta_parse(blob, 1, &m), TBMP_META_ERR_TRUNCATED);
    }

    /* tbmp_meta_parse: malformed MessagePack payloads -> TRUNCATED */
    {
        /* map with key but missing value bytes */
        uint8_t missing_value[] = {0x81, 0xa1, 'k'};
        TBmpMeta m;
        CHECK_ERR(tbmp_meta_parse(missing_value, sizeof(missing_value), &m),
                  TBMP_META_ERR_TRUNCATED);

        /* map with str header declaring 3 bytes but only 2 provided */
        uint8_t short_str[] = {0x81, 0xa1, 'k', 0xa3, 'a', 'b'};
        CHECK_ERR(tbmp_meta_parse(short_str, sizeof(short_str), &m),
                  TBMP_META_ERR_TRUNCATED);

        /* map with bin8 length 4 but only 2 bytes provided */
        uint8_t short_bin[] = {0x81, 0xa1, 'b', 0xc4, 0x04, 0xde, 0xad};
        CHECK_ERR(tbmp_meta_parse(short_bin, sizeof(short_bin), &m),
                  TBMP_META_ERR_TRUNCATED);
    }

    /* tbmp_meta_encode + tbmp_meta_parse round-trip: all value types */
    {
        TBmpMeta src;
        memset(&src, 0, sizeof(src));

        /* nil */
        strncpy(src.entries[0].key, "nil_key", TBMP_META_KEY_MAX);
        src.entries[0].value.type = TBMP_META_NIL;

        /* bool true */
        strncpy(src.entries[1].key, "flag", TBMP_META_KEY_MAX);
        src.entries[1].value.type = TBMP_META_BOOL;
        src.entries[1].value.u = 1U;

        /* uint */
        strncpy(src.entries[2].key, "width", TBMP_META_KEY_MAX);
        src.entries[2].value.type = TBMP_META_UINT;
        src.entries[2].value.u = 640U;

        /* int (negative) */
        strncpy(src.entries[3].key, "offset", TBMP_META_KEY_MAX);
        src.entries[3].value.type = TBMP_META_INT;
        src.entries[3].value.i = -42;

        /* float */
        strncpy(src.entries[4].key, "scale", TBMP_META_KEY_MAX);
        src.entries[4].value.type = TBMP_META_FLOAT;
        src.entries[4].value.f = 1.5;

        /* str */
        strncpy(src.entries[5].key, "author", TBMP_META_KEY_MAX);
        src.entries[5].value.type = TBMP_META_STR;
        strncpy((char *)src.entries[5].value.s, "Alice", TBMP_META_STR_MAX);

        /* bin */
        strncpy(src.entries[6].key, "thumb", TBMP_META_KEY_MAX);
        src.entries[6].value.type = TBMP_META_BIN;
        src.entries[6].value.s[0] = 0xDE;
        src.entries[6].value.s[1] = 0xAD;
        src.entries[6].value.bin_len = 2U;

        src.count = 7U;

        /* Encode */
        uint8_t blob[512];
        size_t blob_len = 0;
        CHECK_OK(tbmp_meta_encode(&src, blob, sizeof(blob), &blob_len));
        CHECK_GT(blob_len, 0U);

        /* Parse back */
        TBmpMeta dst;
        CHECK_OK(tbmp_meta_parse(blob, blob_len, &dst));
        CHECK_EQ(dst.count, 7U);

        /* nil */
        const TBmpMetaEntry *e;
        e = tbmp_meta_find(&dst, "nil_key");
        CHECK_NE(e, NULL);
        if (e)
            CHECK_EQ(e->value.type, TBMP_META_NIL);

        /* bool */
        e = tbmp_meta_find(&dst, "flag");
        CHECK_NE(e, NULL);
        if (e) {
            CHECK_EQ(e->value.type, TBMP_META_BOOL);
            CHECK_EQ(e->value.u, 1U);
        }

        /* uint */
        e = tbmp_meta_find(&dst, "width");
        CHECK_NE(e, NULL);
        if (e) {
            CHECK_EQ(e->value.type, TBMP_META_UINT);
            CHECK_EQ(e->value.u, 640U);
        }

        /* int */
        e = tbmp_meta_find(&dst, "offset");
        CHECK_NE(e, NULL);
        if (e) {
            CHECK_EQ(e->value.type, TBMP_META_INT);
            CHECK_EQ(e->value.i, -42);
        }

        /* float - compare within epsilon */
        e = tbmp_meta_find(&dst, "scale");
        CHECK_NE(e, NULL);
        if (e) {
            CHECK_EQ(e->value.type, TBMP_META_FLOAT);
            CHECK(e->value.f > 1.49 && e->value.f < 1.51);
        }

        /* str */
        e = tbmp_meta_find(&dst, "author");
        CHECK_NE(e, NULL);
        if (e) {
            CHECK_EQ(e->value.type, TBMP_META_STR);
            CHECK_EQ(strcmp((const char *)e->value.s, "Alice"), 0);
        }

        /* bin */
        e = tbmp_meta_find(&dst, "thumb");
        CHECK_NE(e, NULL);
        if (e) {
            CHECK_EQ(e->value.type, TBMP_META_BIN);
            CHECK_EQ(e->value.bin_len, 2U);
            CHECK_EQ(e->value.s[0], 0xDE);
            CHECK_EQ(e->value.s[1], 0xAD);
        }

        /* Full struct round-trip compare (encode -> decode -> compare). */
        CHECK(meta_equal(&src, &dst));
    }

    /* tbmp_meta_validate_*: required keys, types, and length bounds */
    {
        TBmpMeta m;
        memset(&m, 0, sizeof(m));

        strncpy(m.entries[0].key, "title", TBMP_META_KEY_MAX);
        m.entries[0].value.type = TBMP_META_STR;
        strncpy((char *)m.entries[0].value.s, "Sprite Sheet",
                TBMP_META_STR_MAX);

        strncpy(m.entries[1].key, "author", TBMP_META_KEY_MAX);
        m.entries[1].value.type = TBMP_META_STR;
        strncpy((char *)m.entries[1].value.s, "Nellow", TBMP_META_STR_MAX);

        strncpy(m.entries[2].key, "build", TBMP_META_KEY_MAX);
        m.entries[2].value.type = TBMP_META_UINT;
        m.entries[2].value.u = 7U;

        strncpy(m.entries[3].key, "blob", TBMP_META_KEY_MAX);
        m.entries[3].value.type = TBMP_META_BIN;
        m.entries[3].value.s[0] = 0xAA;
        m.entries[3].value.s[1] = 0xBB;
        m.entries[3].value.s[2] = 0xCC;
        m.entries[3].value.bin_len = 3U;

        m.count = 4U;

        CHECK_OK(tbmp_meta_validate_required_key(&m, "title"));
        CHECK_ERR(tbmp_meta_validate_required_key(&m, "description"),
                  TBMP_META_ERR_REQUIRED_MISSING);

        CHECK_OK(tbmp_meta_validate_type(&m, "build", TBMP_META_UINT));
        CHECK_ERR(tbmp_meta_validate_type(&m, "build", TBMP_META_STR),
                  TBMP_META_ERR_TYPE_MISMATCH);
        CHECK_ERR(tbmp_meta_validate_type(&m, "missing", TBMP_META_STR),
                  TBMP_META_ERR_REQUIRED_MISSING);

        CHECK_OK(tbmp_meta_validate_length(&m, "title", 3U, 20U));
        CHECK_ERR(tbmp_meta_validate_length(&m, "title", 20U, 40U),
                  TBMP_META_ERR_LENGTH_OUT_OF_RANGE);
        CHECK_OK(tbmp_meta_validate_length(&m, "blob", 3U, 3U));
        CHECK_ERR(tbmp_meta_validate_length(&m, "build", 1U, 4U),
                  TBMP_META_ERR_TYPE_MISMATCH);
        CHECK_ERR(tbmp_meta_validate_length(&m, "author", 10U, 5U),
                  TBMP_META_ERR_LENGTH_OUT_OF_RANGE);

        CHECK_ERR(tbmp_meta_validate_required_key(NULL, "x"),
                  TBMP_ERR_NULL_PTR);
        CHECK_ERR(tbmp_meta_validate_type(&m, NULL, TBMP_META_STR),
                  TBMP_ERR_NULL_PTR);
        CHECK_ERR(tbmp_meta_validate_length(NULL, "title", 1U, 2U),
                  TBMP_ERR_NULL_PTR);
    }

    /* tbmp_meta_find: not found returns NULL */
    {
        TBmpMeta m;
        memset(&m, 0, sizeof(m));
        CHECK_EQ(tbmp_meta_find(&m, "missing"), NULL);
        CHECK_EQ(tbmp_meta_find(NULL, "key"), NULL);
        CHECK_EQ(tbmp_meta_find(&m, NULL), NULL);
    }

    /* tbmp_write / tbmp_open round-trip with META section */
    {
        TBmpMeta meta;
        memset(&meta, 0, sizeof(meta));
        strncpy(meta.entries[0].key, "version", TBMP_META_KEY_MAX);
        meta.entries[0].value.type = TBMP_META_UINT;
        meta.entries[0].value.u = 2U;
        meta.count = 1U;

        TBmpRGBA px = {100, 150, 200, 255};
        TBmpFrame fr;
        fr.width = 1;
        fr.height = 1;
        fr.pixels = &px;

        TBmpWriteParams p;
        tbmp_write_default_params(&p);
        p.meta = &meta;

        uint8_t buf[512];
        size_t written = 0;
        CHECK_OK(tbmp_write(&fr, &p, buf, sizeof(buf), &written));
        CHECK_GT(written, 0U);

        /* Parse the written file */
        TBmpImage img;
        CHECK_OK(tbmp_open(buf, written, &img));
        CHECK_GT(img.meta_len, 0U);
        CHECK_NE(img.meta, NULL);

        /* Parse the META blob */
        TBmpMeta parsed_meta;
        CHECK_OK(tbmp_meta_parse(img.meta, img.meta_len, &parsed_meta));
        CHECK_EQ(parsed_meta.count, 1U);
        const TBmpMetaEntry *e = tbmp_meta_find(&parsed_meta, "version");
        CHECK_NE(e, NULL);
        if (e) {
            CHECK_EQ(e->value.type, TBMP_META_UINT);
            CHECK_EQ(e->value.u, 2U);
        }
    }
}
