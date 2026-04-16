#include "test_helpers.h"
#include "test_suites.h"
#include "tbmp_meta.h"
#include "tbmp_msgpack.h"
#include "tbmp_reader.h"
#include "tbmp_write.h"

#include <string.h>

static void fill_valid_meta(TBmpMeta *m) {
    memset(m, 0, sizeof(*m));
    strcpy(m->title, "Forest Tiles");
    strcpy(m->author, "Nellow");
    strcpy(m->description, "Top-down biome tiles");
    strcpy(m->created, "2026-04-16T19:30:00Z");
    strcpy(m->software, "tbmp-cli 0.1");
    strcpy(m->license, "CC-BY-4.0");

    m->tag_count = 3;
    strcpy(m->tags[0], "tileset");
    strcpy(m->tags[1], "rpg");
    strcpy(m->tags[2], "forest");

    m->has_dpi = 1;
    m->dpi = 144;
    strcpy(m->colorspace, "sRGB");

    m->custom_count = 1;
    {
        TBmpMpWriter w;
        tbmp_mp_writer_init(&w, m->custom[0].data, TBMP_META_CUSTOM_ITEM_MAX);
        tbmp_mp_start_map(&w, 2);
        tbmp_mp_write_cstr(&w, "team");
        tbmp_mp_write_cstr(&w, "gfx");
        tbmp_mp_write_cstr(&w, "build");
        tbmp_mp_write_uint(&w, 7U);
        m->custom[0].len = (uint32_t)tbmp_mp_writer_used(&w);
    }
}

void test_meta(void) {
    SUITE("META");

    /* tbmp_meta_parse: NULL out returns error */
    {
        uint8_t blob[1] = {0x80};
        CHECK_ERR(tbmp_meta_parse(blob, 1, NULL), TBMP_ERR_NULL_PTR);
    }

    /* tbmp_meta_parse: strict structured parse and round-trip */
    {
        TBmpMeta src;
        fill_valid_meta(&src);

        uint8_t blob[1024];
        size_t blob_len = 0;
        CHECK_OK(tbmp_meta_encode(&src, blob, sizeof(blob), &blob_len));
        CHECK_GT(blob_len, 0U);

        TBmpMeta dst;
        CHECK_OK(tbmp_meta_parse(blob, blob_len, &dst));

        CHECK_EQ(strcmp(dst.title, src.title), 0);
        CHECK_EQ(strcmp(dst.author, src.author), 0);
        CHECK_EQ(strcmp(dst.description, src.description), 0);
        CHECK_EQ(strcmp(dst.created, src.created), 0);
        CHECK_EQ(strcmp(dst.software, src.software), 0);
        CHECK_EQ(strcmp(dst.license, src.license), 0);
        CHECK_EQ(dst.tag_count, src.tag_count);
        CHECK_EQ(strcmp(dst.tags[0], src.tags[0]), 0);
        CHECK_EQ(strcmp(dst.tags[1], src.tags[1]), 0);
        CHECK_EQ(strcmp(dst.tags[2], src.tags[2]), 0);
        CHECK_EQ(dst.has_dpi, src.has_dpi);
        CHECK_EQ(dst.dpi, src.dpi);
        CHECK_EQ(strcmp(dst.colorspace, src.colorspace), 0);
        CHECK_EQ(dst.custom_count, src.custom_count);
        CHECK_EQ(dst.custom[0].len, src.custom[0].len);
        CHECK_EQ(
            memcmp(dst.custom[0].data, src.custom[0].data, src.custom[0].len),
            0);

        CHECK_OK(tbmp_meta_validate_structured_blob(blob, blob_len));
    }

    /* Missing required key (tags) */
    {
        uint8_t blob[512];
        TBmpMpWriter w;
        tbmp_mp_writer_init(&w, blob, sizeof(blob));

        tbmp_mp_start_map(&w, 6);
        tbmp_mp_write_cstr(&w, "title");
        tbmp_mp_write_cstr(&w, "x");
        tbmp_mp_write_cstr(&w, "author");
        tbmp_mp_write_cstr(&w, "y");
        tbmp_mp_write_cstr(&w, "description");
        tbmp_mp_write_cstr(&w, "z");
        tbmp_mp_write_cstr(&w, "created");
        tbmp_mp_write_cstr(&w, "0");
        tbmp_mp_write_cstr(&w, "software");
        tbmp_mp_write_cstr(&w, "s");
        tbmp_mp_write_cstr(&w, "license");
        tbmp_mp_write_cstr(&w, "l");

        CHECK_ERR(
            tbmp_meta_validate_structured_blob(blob, tbmp_mp_writer_used(&w)),
            TBMP_META_ERR_REQUIRED_MISSING);
    }

    /* Unknown key */
    {
        uint8_t blob[512];
        TBmpMpWriter w;
        tbmp_mp_writer_init(&w, blob, sizeof(blob));

        tbmp_mp_start_map(&w, 8);
        tbmp_mp_write_cstr(&w, "title");
        tbmp_mp_write_cstr(&w, "x");
        tbmp_mp_write_cstr(&w, "author");
        tbmp_mp_write_cstr(&w, "y");
        tbmp_mp_write_cstr(&w, "description");
        tbmp_mp_write_cstr(&w, "z");
        tbmp_mp_write_cstr(&w, "created");
        tbmp_mp_write_cstr(&w, "0");
        tbmp_mp_write_cstr(&w, "software");
        tbmp_mp_write_cstr(&w, "s");
        tbmp_mp_write_cstr(&w, "license");
        tbmp_mp_write_cstr(&w, "l");
        tbmp_mp_write_cstr(&w, "tags");
        tbmp_mp_start_array(&w, 1);
        tbmp_mp_write_cstr(&w, "one");
        tbmp_mp_write_cstr(&w, "oops");
        tbmp_mp_write_cstr(&w, "bad");

        CHECK_ERR(
            tbmp_meta_validate_structured_blob(blob, tbmp_mp_writer_used(&w)),
            TBMP_META_ERR_SCHEMA_UNKNOWN_KEY);
    }

    /* tags must be array<string> */
    {
        uint8_t blob[512];
        TBmpMpWriter w;
        tbmp_mp_writer_init(&w, blob, sizeof(blob));

        tbmp_mp_start_map(&w, 7);
        tbmp_mp_write_cstr(&w, "title");
        tbmp_mp_write_cstr(&w, "x");
        tbmp_mp_write_cstr(&w, "author");
        tbmp_mp_write_cstr(&w, "y");
        tbmp_mp_write_cstr(&w, "description");
        tbmp_mp_write_cstr(&w, "z");
        tbmp_mp_write_cstr(&w, "created");
        tbmp_mp_write_cstr(&w, "0");
        tbmp_mp_write_cstr(&w, "software");
        tbmp_mp_write_cstr(&w, "s");
        tbmp_mp_write_cstr(&w, "license");
        tbmp_mp_write_cstr(&w, "l");
        tbmp_mp_write_cstr(&w, "tags");
        tbmp_mp_write_cstr(&w, "not-array");

        CHECK_ERR(
            tbmp_meta_validate_structured_blob(blob, tbmp_mp_writer_used(&w)),
            TBMP_META_ERR_TYPE_MISMATCH);
    }

    /* custom must be array<map<string,any>> */
    {
        uint8_t blob[512];
        TBmpMpWriter w;
        tbmp_mp_writer_init(&w, blob, sizeof(blob));

        tbmp_mp_start_map(&w, 8);
        tbmp_mp_write_cstr(&w, "title");
        tbmp_mp_write_cstr(&w, "x");
        tbmp_mp_write_cstr(&w, "author");
        tbmp_mp_write_cstr(&w, "y");
        tbmp_mp_write_cstr(&w, "description");
        tbmp_mp_write_cstr(&w, "z");
        tbmp_mp_write_cstr(&w, "created");
        tbmp_mp_write_cstr(&w, "0");
        tbmp_mp_write_cstr(&w, "software");
        tbmp_mp_write_cstr(&w, "s");
        tbmp_mp_write_cstr(&w, "license");
        tbmp_mp_write_cstr(&w, "l");
        tbmp_mp_write_cstr(&w, "tags");
        tbmp_mp_start_array(&w, 1);
        tbmp_mp_write_cstr(&w, "one");
        tbmp_mp_write_cstr(&w, "custom");
        tbmp_mp_start_array(&w, 1);
        tbmp_mp_write_cstr(&w, "no-map");

        CHECK_ERR(
            tbmp_meta_validate_structured_blob(blob, tbmp_mp_writer_used(&w)),
            TBMP_META_ERR_TYPE_MISMATCH);
    }

    /* tbmp_msgpack safety: null pointers and overflow-safe bounds checks */
    {
        TBmpMpReader r;
        tbmp_mp_reader_init(&r, NULL, 8U);
        CHECK(tbmp_mp_reader_error(&r));

        uint8_t data[4] = {1, 2, 3, 4};
        tbmp_mp_reader_init(&r, data, sizeof(data));
        tbmp_mp_read_bytes(&r, NULL, 1U);
        CHECK(tbmp_mp_reader_error(&r));

        tbmp_mp_reader_init(&r, data, sizeof(data));
        r.pos = (size_t)-2;
        r.len = (size_t)-1;
        tbmp_mp_skip_bytes(&r, 16U);
        CHECK(tbmp_mp_reader_error(&r));

        TBmpMpWriter w;
        tbmp_mp_writer_init(&w, NULL, 16U);
        CHECK(tbmp_mp_writer_error(&w));
    }

    /* tbmp_write / tbmp_open round-trip with structured META section */
    {
        TBmpMeta meta;
        fill_valid_meta(&meta);

        TBmpRGBA px = {100, 150, 200, 255};
        TBmpFrame fr;
        fr.width = 1;
        fr.height = 1;
        fr.pixels = &px;

        TBmpWriteParams p;
        tbmp_write_default_params(&p);
        p.meta = &meta;

        uint8_t buf[2048];
        size_t written = 0;
        CHECK_OK(tbmp_write(&fr, &p, buf, sizeof(buf), &written));
        CHECK_GT(written, 0U);

        TBmpImage img;
        CHECK_OK(tbmp_open(buf, written, &img));
        CHECK_GT(img.meta_len, 0U);
        CHECK_NE(img.meta, NULL);

        TBmpMeta parsed_meta;
        CHECK_OK(tbmp_meta_parse(img.meta, img.meta_len, &parsed_meta));
        CHECK_EQ(strcmp(parsed_meta.title, "Forest Tiles"), 0);
        CHECK_EQ(strcmp(parsed_meta.author, "Nellow"), 0);
        CHECK_EQ(parsed_meta.tag_count, 3U);
    }
}
