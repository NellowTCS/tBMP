#include "test_helpers.h"
#include "test_suites.h"
#include "tbmp_reader.h"
#include "tbmp_decode.h"

#include <stdint.h>
#include <string.h>

void test_reader(void) {
    SUITE("Reader");

    uint8_t buf[256];

    /* NULL pointer guards */
    {
        TBmpImage img;
        CHECK_ERR(tbmp_open(NULL, 100, &img), TBMP_ERR_NULL_PTR);
        CHECK_ERR(tbmp_open(buf, 100, NULL), TBMP_ERR_NULL_PTR);
    }

    /* Too short */
    {
        TBmpImage img;
        memset(buf, 0, sizeof(buf));
        CHECK_ERR(tbmp_open(buf, 3, &img), TBMP_ERR_TRUNCATED);
    }

    /* Bad magic (buffer large enough to reach the magic check) */
    {
        TBmpImage img;
        memset(buf, 0, sizeof(buf));
        memcpy(buf, "xBMP", 4);
        CHECK_ERR(tbmp_open(buf, 32, &img), TBMP_ERR_BAD_MAGIC);
    }

    /* Bad version */
    {
        TBmpImage img;
        size_t n = build_tbmp(buf, sizeof(buf), 0x0200 /*bad*/, 4, 4, 8, 0,
                              3 /*INDEX_8*/, TBMP_FLAG_HAS_PALETTE, NULL, 0,
                              NULL, 0, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_ERR(tbmp_open(buf, n, &img), TBMP_ERR_BAD_VERSION);
    }

    /* Zero width */
    {
        TBmpImage img;
        size_t n =
            build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 0 /*width*/, 4, 32,
                       0, 9 /*RGBA8888*/, 0, NULL, 0, NULL, 0, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_ERR(tbmp_open(buf, n, &img), TBMP_ERR_ZERO_DIMENSIONS);
    }

    /* Bad bit_depth */
    {
        TBmpImage img;
        uint8_t data[16] = {0};
        size_t n =
            build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 2, 2, 7 /*invalid*/,
                       0, 9, 0, data, 16, NULL, 0, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_ERR(tbmp_open(buf, n, &img), TBMP_ERR_BAD_BIT_DEPTH);
    }

    /* Bad encoding */
    {
        TBmpImage img;
        uint8_t data[4] = {0};
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 1, 1, 8,
                              99 /*invalid*/, 9, 0, data, 4, NULL, 0, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_ERR(tbmp_open(buf, n, &img), TBMP_ERR_BAD_ENCODING);
    }

    /* Truncated data section */
    {
        TBmpImage img;
        uint8_t data[4] = {0xAB, 0xCD, 0xEF, 0x00};
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 2, 2, 8, 0,
                              3 /*INDEX_8*/, TBMP_FLAG_HAS_PALETTE, data, 4,
                              NULL, 0, NULL, 0);
        /* data_size=4 but we pass fewer bytes to open */
        CHECK_ERR(tbmp_open(buf, n - 1, &img), TBMP_ERR_TRUNCATED);
    }

    /* Indexed format without palette -> error */
    {
        TBmpImage img;
        uint8_t data[4] = {0};
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 2, 2, 8, 0,
                              3 /*INDEX_8*/, 0 /*no palette flag*/, data, 4,
                              NULL, 0, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_ERR(tbmp_open(buf, n, &img), TBMP_ERR_NO_PALETTE);
    }

    /* Valid minimal RGBA8888 RAW */
    {
        TBmpImage img;
        uint8_t data[16] = {
            0xFF, 0x00, 0x00, 0xFF, /* red   */
            0x00, 0xFF, 0x00, 0xFF, /* green */
            0x00, 0x00, 0xFF, 0xFF, /* blue  */
            0xFF, 0xFF, 0xFF, 0xFF  /* white */
        };
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 2, 2, 32, 0,
                              9 /*RGBA8888*/, 0, data, 16, NULL, 0, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_EQ(img.head.width, 2);
        CHECK_EQ(img.head.height, 2);
        CHECK_EQ(img.head.bit_depth, 32);
        CHECK_EQ(img.data_len, 16U);
        CHECK_EQ(img.has_palette, 0);
        CHECK_EQ(img.has_masks, 0);
    }

    /* Valid with palette */
    {
        TBmpImage img;
        TBmpRGBA pal_entries[2] = {{255, 0, 0, 255}, {0, 255, 0, 255}};
        uint8_t extra_buf[64];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_entries, 2);
        CHECK_GT(extra_len, 0U);

        /* 4 pixels, 1-bit indexed -> 1 byte */
        uint8_t data[1] = {0xA0}; /* 0b10100000 */
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 2, 2, 1, 0,
                              0 /*INDEX_1*/, TBMP_FLAG_HAS_PALETTE, data, 1,
                              extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_EQ(img.has_palette, 1);
        CHECK_EQ(img.palette.count, 2U);
        CHECK_EQ(img.palette.entries[0].r, 255);
        CHECK_EQ(img.palette.entries[1].g, 255);
    }

    /* Valid with MASK chunk */
    {
        TBmpImage img;
        uint8_t extra_buf[64];
        TBmpMasks m = {0xF800U, 0x07E0U, 0x001FU, 0x0000U};
        size_t extra_len = build_mask_extra(extra_buf, sizeof(extra_buf), m);
        CHECK_GT(extra_len, 0U);

        uint8_t data[2] = {0x00, 0xF8}; /* 1 pixel of u16 */
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 1, 1, 16, 0,
                              10 /*CUSTOM*/, TBMP_FLAG_HAS_MASKS, data, 2,
                              extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_EQ(img.has_masks, 1);
        CHECK_EQ(img.masks.r, 0xF800U);
        CHECK_EQ(img.masks.g, 0x07E0U);
        CHECK_EQ(img.masks.b, 0x001FU);
    }
}
