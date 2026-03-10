#include "test_helpers.h"
#include "test_suites.h"
#include "tbmp_reader.h"
#include "tbmp_decode.h"
#include "tbmp_write.h"

#include <string.h>

void test_edge_cases(void) {
    SUITE("EdgeCases");

    uint8_t buf[512];
    TBmpImage img;
    TBmpRGBA pixels[4];
    TBmpFrame frame = {0, 0, pixels};

    /* 1x1 RGBA8888 */
    {
        uint8_t data[4] = {0xAB, 0xCD, 0xEF, 0x80};
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 1, 1, 32, 0,
                              9, 0, data, 4, NULL, 0, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        CHECK_EQ(pixels[0].r, 0xAB);
        CHECK_EQ(pixels[0].g, 0xCD);
        CHECK_EQ(pixels[0].b, 0xEF);
        CHECK_EQ(pixels[0].a, 0x80);
    }

    /* Output buffer too small for write */
    {
        TBmpRGBA px[4] = {
            {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
        TBmpFrame f = {2, 2, px};
        TBmpWriteParams params;
        tbmp_write_default_params(&params);

        uint8_t tiny[4] = {0};
        size_t wlen = 0;
        TBmpError e = tbmp_write(&f, &params, tiny, sizeof(tiny), &wlen);
        CHECK_ERR(e, TBMP_ERR_OUT_OF_MEMORY);
    }

    /* Decode with NULL pixels -> error */
    {
        uint8_t data[4] = {0};
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 1, 1, 32, 0,
                              9, 0, data, 4, NULL, 0, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        TBmpFrame null_frame = {0, 0, NULL};
        CHECK_ERR(tbmp_decode(&img, &null_frame), TBMP_ERR_NULL_PTR);
    }

    /* tbmp_error_str returns non-NULL strings */
    {
        CHECK_NE(tbmp_error_str(TBMP_OK), NULL);
        CHECK_NE(tbmp_error_str(TBMP_ERR_NULL_PTR), NULL);
        CHECK_NE(tbmp_error_str(TBMP_ERR_BAD_MAGIC), NULL);
        CHECK_NE(tbmp_error_str(TBMP_ERR_TRUNCATED), NULL);
        CHECK_NE(tbmp_error_str(TBMP_ERR_BAD_SPAN), NULL);
        CHECK_NE(tbmp_error_str((TBmpError)9999), NULL);
    }

    /* MAX_PALETTE exceeded in PALT chunk -> error */
    {
        uint8_t extra_buf[32];
        /* Craft a PALT chunk claiming 512 entries but providing none */
        memcpy(extra_buf, "PALT", 4);
        /* body length = 4 + 512*4 = 2052 */
        uint32_t fake_len = 4U + 512U * 4U;
        extra_buf[4] = (uint8_t)(fake_len & 0xFF);
        extra_buf[5] = (uint8_t)((fake_len >> 8) & 0xFF);
        extra_buf[6] = (uint8_t)((fake_len >> 16) & 0xFF);
        extra_buf[7] = (uint8_t)((fake_len >> 24) & 0xFF);
        /* palette_count = 512 */
        extra_buf[8] = 0;
        extra_buf[9] = 2;
        extra_buf[10] = 0;
        extra_buf[11] = 0;

        /* Total extra_size we declare = 12 bytes (truncated) */
        uint8_t data[4] = {0};
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 1, 1, 8, 0,
                              3 /*INDEX_8*/, TBMP_FLAG_HAS_PALETTE, data, 4,
                              extra_buf, 12, NULL, 0);
        TBmpError e = tbmp_open(buf, n, &img);
        /* Should fail: either TRUNCATED or BAD_PALETTE */
        CHECK_NE(e, TBMP_OK);
    }

    /* v1.0 file is accepted */
    {
        uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 1, 1, 32, 0,
                              9, 0, data, 4, NULL, 0, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_EQ(img.head.version, TBMP_VERSION_1_0);
    }

    /* 2-bit indexed, 4 pixels */
    {
        TBmpRGBA pal_e[4] = {{0, 0, 0, 255},
                             {255, 0, 0, 255},
                             {0, 255, 0, 255},
                             {0, 0, 255, 255}};
        uint8_t extra_buf[128];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_e, 4);

        /* 4 pixels packed into 1 byte: 0b00011011 = 0,1,2,3 */
        uint8_t data[1] = {0x1B}; /* 0b00011011 */
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 1, 2, 0,
                              1 /*INDEX_2*/, TBMP_FLAG_HAS_PALETTE, data, 1,
                              extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        CHECK_EQ(pixels[0].r, 0);   /* pal[0]=black */
        CHECK_EQ(pixels[1].r, 255); /* pal[1]=red   */
        CHECK_EQ(pixels[2].g, 255); /* pal[2]=green */
        CHECK_EQ(pixels[3].b, 255); /* pal[3]=blue  */
    }
}
