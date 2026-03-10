#include "test_helpers.h"
#include "test_suites.h"
#include "tbmp_reader.h"
#include "tbmp_decode.h"

/* Suite: Encoding Mode 0 - RAW */
void test_decode_raw(void) {
    SUITE("Decode/RAW");

    uint8_t buf[256];
    TBmpImage img;
    TBmpRGBA pixels[16];
    TBmpFrame frame = {0, 0, pixels};

    /* 2x2 RGBA8888 */
    {
        uint8_t data[] = {
            0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
            0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF,
        };
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 2, 2, 32, 0,
                              9, 0, data, 16, NULL, 0, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        CHECK_EQ(frame.width, 2);
        CHECK_EQ(frame.height, 2);
        CHECK_EQ(pixels[0].r, 255);
        CHECK_EQ(pixels[0].g, 0);
        CHECK_EQ(pixels[1].r, 0);
        CHECK_EQ(pixels[1].g, 255);
        CHECK_EQ(pixels[2].r, 0);
        CHECK_EQ(pixels[2].b, 255);
        CHECK_EQ(pixels[3].r, 255);
        CHECK_EQ(pixels[3].g, 255);
    }

    /* 4x1 RGB565 */
    {
        uint8_t data[] = {
            0x00, 0xF8, /* red   */
            0xE0, 0x07, /* green */
            0x1F, 0x00, /* blue  */
            0xFF, 0xFF, /* white */
        };
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 1, 16, 0,
                              4 /*RGB565*/, 0, data, 8, NULL, 0, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        CHECK_EQ(pixels[0].r, 255);
        CHECK_EQ(pixels[0].g, 0);
        CHECK_EQ(pixels[1].g, 255);
        CHECK_EQ(pixels[1].r, 0);
        CHECK_EQ(pixels[2].b, 255);
        CHECK_EQ(pixels[2].r, 0);
        CHECK_EQ(pixels[3].r, 255);
        CHECK_EQ(pixels[3].g, 255);
    }

    /* 8x1 1-bit indexed - 8 pixels packed into 1 byte */
    {
        TBmpRGBA pal_e[2] = {{0, 0, 0, 255}, {255, 255, 255, 255}};
        uint8_t extra_buf[64];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_e, 2);

        uint8_t data[1] = {0xAA}; /* 0b10101010 */
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 8, 1, 1, 0,
                              0 /*INDEX_1*/, TBMP_FLAG_HAS_PALETTE, data, 1,
                              extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        /* Alternating: 1=white, 0=black */
        CHECK_EQ(pixels[0].r, 255); /* bit7=1 -> white */
        CHECK_EQ(pixels[1].r, 0);   /* bit6=0 -> black */
        CHECK_EQ(pixels[2].r, 255);
        CHECK_EQ(pixels[3].r, 0);
    }

    /* 4x1 4-bit indexed */
    {
        TBmpRGBA pal_e[4] = {{0, 0, 0, 255},
                             {255, 0, 0, 255},
                             {0, 255, 0, 255},
                             {0, 0, 255, 255}};
        uint8_t extra_buf[128];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_e, 4);

        /* Packed nibbles: 0x01 = px0=0, px1=1; 0x23 = px2=2, px3=3 */
        uint8_t data[2] = {0x01, 0x23};
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 1, 4, 0,
                              2 /*INDEX_4*/, TBMP_FLAG_HAS_PALETTE, data, 2,
                              extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        CHECK_EQ(pixels[0].r, 0);   /* pal[0] = black */
        CHECK_EQ(pixels[1].r, 255); /* pal[1] = red   */
        CHECK_EQ(pixels[2].g, 255); /* pal[2] = green */
        CHECK_EQ(pixels[3].b, 255); /* pal[3] = blue  */
    }
}

/* Suite: Encoding Mode 1 - Zero-Range */
void test_decode_zero_range(void) {
    SUITE("Decode/ZeroRange");

    uint8_t buf[512];
    TBmpImage img;
    TBmpRGBA pixels[16];
    TBmpFrame frame = {0, 0, pixels};

    /* 4x1 INDEX_8 with a palette of 4 colours.
   * Pixels: [1, 0, 0, 2] - zero-range covers [1..2], non-zeros: 1,2 */
    {
        TBmpRGBA pal_e[4] = {{0, 0, 0, 255},
                             {255, 0, 0, 255},
                             {0, 255, 0, 255},
                             {0, 0, 255, 255}};
        uint8_t extra_buf[128];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_e, 4);

        uint8_t data[64];
        uint8_t *p = data;

        /* num_zero_ranges = 1 */
        p[0] = 1;
        p[1] = p[2] = p[3] = 0;
        p += 4;
        /* range[0]: start=1, length=2 */
        p[0] = 1;
        p[1] = p[2] = p[3] = 0;
        p += 4;
        p[0] = 2;
        p[1] = p[2] = p[3] = 0;
        p += 4;
        /* num_values = 2 */
        p[0] = 2;
        p[1] = p[2] = p[3] = 0;
        p += 4;
        /* values: pixel[0]=1(red), pixel[3]=2(green) */
        p[0] = 1;
        p += 1;
        p[0] = 2;
        p += 1;

        size_t data_len = (size_t)(p - data);
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 1, 8,
                              1 /*ZERO_RANGE*/, 3 /*INDEX_8*/,
                              TBMP_FLAG_HAS_PALETTE, data, (uint32_t)data_len,
                              extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_GT(n, 0U);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));

        CHECK_EQ(pixels[0].r, 255); /* pal[1] = red   */
        CHECK_EQ(pixels[1].r, 0);   /* pal[0] = black */
        CHECK_EQ(pixels[2].r, 0);   /* pal[0] = black */
        CHECK_EQ(pixels[3].g, 255); /* pal[2] = green */
    }
}

/* Suite: Encoding Mode 2 - RLE */
void test_decode_rle(void) {
    SUITE("Decode/RLE");

    uint8_t buf[256];
    TBmpImage img;
    TBmpRGBA pixels[16];
    TBmpFrame frame = {0, 0, pixels};

    /* 6x1 INDEX_8 with a palette of 2 colours */
    {
        TBmpRGBA pal_e[2] = {{0, 0, 0, 255}, {255, 0, 0, 255}};
        uint8_t extra_buf[64];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_e, 2);

        /* RLE: (3,0) -> three blacks; (2,1) -> two reds; (1,0) -> one black */
        uint8_t data[] = {3, 0, 2, 1, 1, 0};
        size_t n =
            build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 6, 1, 8, 2 /*RLE*/,
                       3 /*INDEX_8*/, TBMP_FLAG_HAS_PALETTE, data, sizeof(data),
                       extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        CHECK_EQ(pixels[0].r, 0);
        CHECK_EQ(pixels[1].r, 0);
        CHECK_EQ(pixels[2].r, 0);
        CHECK_EQ(pixels[3].r, 255);
        CHECK_EQ(pixels[4].r, 255);
        CHECK_EQ(pixels[5].r, 0);
    }

    /* RLE with count=0 (NOP) */
    {
        uint8_t data[] = {0, 99, 4, 0xFF}; /* NOP then 4 white pixels */
        size_t n =
            build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 1, 8, 2,
                       7 /*RGB332*/, 0, data, sizeof(data), NULL, 0, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        /* 0xFF in RGB332 = R=7,G=7,B=3 -> all max -> all white */
        CHECK_EQ(pixels[0].r, 255);
        CHECK_EQ(pixels[3].r, 255);
    }
}

/* Suite: Encoding Mode 3 - Span */
void test_decode_span(void) {
    SUITE("Decode/Span");

    uint8_t buf[512];
    TBmpImage img;
    TBmpRGBA pixels[16];
    TBmpFrame frame = {0, 0, pixels};

    /* 5x1 INDEX_8: two spans */
    {
        TBmpRGBA pal_e[3] = {
            {0, 0, 0, 255}, {255, 0, 0, 255}, {0, 0, 255, 255}};
        uint8_t extra_buf[64];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_e, 3);

        /* span0: index=0, length=2, value=1(red)
     * span1: index=3, length=2, value=2(blue) */
        uint8_t data[64];
        uint8_t *p = data;
        p[0] = 2;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* num_spans=2 */
        /* span0 */
        p[0] = 0;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* index=0 */
        p[0] = 2;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* length=2 */
        p[0] = 1;
        p += 1; /* value=1 */
        /* span1 */
        p[0] = 3;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* index=3 */
        p[0] = 2;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* length=2 */
        p[0] = 2;
        p += 1; /* value=2 */

        size_t data_len = (size_t)(p - data);
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 5, 1, 8,
                              3 /*SPAN*/, 3 /*INDEX_8*/, TBMP_FLAG_HAS_PALETTE,
                              data, (uint32_t)data_len, extra_buf,
                              (uint32_t)extra_len, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));
        CHECK_EQ(pixels[0].r, 255); /* red   */
        CHECK_EQ(pixels[1].r, 255); /* red   */
        CHECK_EQ(pixels[2].r, 0);   /* black (background) */
        CHECK_EQ(pixels[3].b, 255); /* blue  */
        CHECK_EQ(pixels[4].b, 255); /* blue  */
    }

    /* Span with out-of-bounds index -> error */
    {
        uint8_t data[64];
        uint8_t *p = data;
        p[0] = 1;
        p[1] = p[2] = p[3] = 0;
        p += 4;
        p[0] = 4;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* index=4 -> OOB for 4 pixels */
        p[0] = 2;
        p[1] = p[2] = p[3] = 0;
        p += 4;
        p[0] = 0;
        p += 1;

        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 1, 8, 3,
                              7 /*RGB332*/, 0, data, (uint32_t)(p - data), NULL,
                              0, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        TBmpError e = tbmp_decode(&img, &frame);
        CHECK_ERR(e, TBMP_ERR_BAD_SPAN);
    }
}

/* Suite: Encoding Mode 4 - Sparse Pixel */
void test_decode_sparse_pixel(void) {
    SUITE("Decode/SparsePixel");

    uint8_t buf[512];
    TBmpImage img;
    TBmpRGBA pixels[16];
    TBmpFrame frame = {0, 0, pixels};

    /* 4x4 INDEX_8 with a few explicit pixels */
    {
        TBmpRGBA pal_e[3] = {
            {0, 0, 0, 255}, {255, 0, 0, 255}, {0, 255, 0, 255}};
        uint8_t extra_buf[64];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_e, 3);

        /* 3 sparse pixels: (0,0)=1, (3,3)=2, (2,1)=1 */
        uint8_t data[64];
        uint8_t *p = data;
        p[0] = 3;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* num_pixels=3 */
        /* pixel0: x=0,y=0,value=1 */
        p[0] = 0;
        p[1] = 0;
        p += 2;
        p[0] = 0;
        p[1] = 0;
        p += 2;
        p[0] = 1;
        p += 1;
        /* pixel1: x=3,y=3,value=2 */
        p[0] = 3;
        p[1] = 0;
        p += 2;
        p[0] = 3;
        p[1] = 0;
        p += 2;
        p[0] = 2;
        p += 1;
        /* pixel2: x=2,y=1,value=1 */
        p[0] = 2;
        p[1] = 0;
        p += 2;
        p[0] = 1;
        p[1] = 0;
        p += 2;
        p[0] = 1;
        p += 1;

        size_t data_len = (size_t)(p - data);
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 4, 8,
                              4 /*SPARSE*/, 3 /*INDEX_8*/,
                              TBMP_FLAG_HAS_PALETTE, data, (uint32_t)data_len,
                              extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));

        /* (0,0) = index 0*4+0 = 0 -> red */
        CHECK_EQ(pixels[0].r, 255);
        /* (3,3) = index 3*4+3 = 15 -> green */
        CHECK_EQ(pixels[15].g, 255);
        /* (2,1) = index 1*4+2 = 6 -> red */
        CHECK_EQ(pixels[6].r, 255);
        /* (1,0) = index 0*4+1 = 1 -> black (background) */
        CHECK_EQ(pixels[1].r, 0);
        CHECK_EQ(pixels[1].g, 0);
        CHECK_EQ(pixels[1].b, 0);
    }

    /* Out-of-bounds coordinate -> error */
    {
        uint8_t data[64];
        uint8_t *p = data;
        p[0] = 1;
        p[1] = p[2] = p[3] = 0;
        p += 4;
        p[0] = 5;
        p[1] = 0;
        p += 2; /* x=5 OOB for width=4 */
        p[0] = 0;
        p[1] = 0;
        p += 2;
        p[0] = 1;
        p += 1;
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 4, 8, 4,
                              7 /*RGB332*/, 0, data, (uint32_t)(p - data), NULL,
                              0, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_ERR(tbmp_decode(&img, &frame), TBMP_ERR_BAD_PIXEL_COORD);
    }
}

/* Suite: Encoding Mode 5 - Block-Sparse */
void test_decode_block_sparse(void) {
    SUITE("Decode/BlockSparse");

    uint8_t buf[512];
    TBmpImage img;
    TBmpRGBA pixels[64];
    TBmpFrame frame = {0, 0, pixels};

    /* 4x4 image, block_width=2, block_height=2 -> 4 tiles.
   * We only provide 2 blocks: tile 0 (top-left) and tile 3 (bottom-right). */
    {
        TBmpRGBA pal_e[3] = {
            {0, 0, 0, 255}, {255, 0, 0, 255}, {0, 0, 255, 255}};
        uint8_t extra_buf[64];
        size_t extra_len =
            build_palt_extra(extra_buf, sizeof(extra_buf), pal_e, 3);

        uint8_t data[256];
        uint8_t *p = data;

        /* block_width=2, block_height=2 */
        p[0] = 2;
        p[1] = 0;
        p += 2;
        p[0] = 2;
        p[1] = 0;
        p += 2;

        /* num_blocks = 2 */
        p[0] = 2;
        p[1] = p[2] = p[3] = 0;
        p += 4;

        /* block 0: block_index=0, 4 pixels INDEX_8 = {1,1,1,1} = all red */
        p[0] = 0;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* block_index=0 */
        p[0] = 4;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* block_data_size=4 */
        p[0] = p[1] = p[2] = p[3] = 1;
        p += 4; /* all palette[1]=red */

        /* block 1: block_index=3, 4 pixels = {2,2,2,2} = all blue */
        p[0] = 3;
        p[1] = p[2] = p[3] = 0;
        p += 4; /* block_index=3 */
        p[0] = 4;
        p[1] = p[2] = p[3] = 0;
        p += 4;
        p[0] = p[1] = p[2] = p[3] = 2;
        p += 4; /* all palette[2]=blue */

        size_t data_len = (size_t)(p - data);
        size_t n = build_tbmp(buf, sizeof(buf), TBMP_VERSION_1_0, 4, 4, 8,
                              5 /*BLOCK_SPARSE*/, 3 /*INDEX_8*/,
                              TBMP_FLAG_HAS_PALETTE, data, (uint32_t)data_len,
                              extra_buf, (uint32_t)extra_len, NULL, 0);
        CHECK_OK(tbmp_open(buf, n, &img));
        CHECK_OK(tbmp_decode(&img, &frame));

        /* Tile 0 covers pixels (0,0),(1,0),(0,1),(1,1) -> red */
        CHECK_EQ(pixels[0].r, 255); /* (0,0) */
        CHECK_EQ(pixels[1].r, 255); /* (1,0) */
        CHECK_EQ(pixels[4].r, 255); /* (0,1) */
        CHECK_EQ(pixels[5].r, 255); /* (1,1) */

        /* Tiles 1 and 2 are absent -> background (black) */
        CHECK_EQ(pixels[2].r, 0); /* (2,0) */
        CHECK_EQ(pixels[8].r, 0); /* (0,2) */

        /* Tile 3 covers pixels (2,2),(3,2),(2,3),(3,3) -> blue */
        CHECK_EQ(pixels[10].b, 255); /* (2,2) */
        CHECK_EQ(pixels[11].b, 255); /* (3,2) */
        CHECK_EQ(pixels[14].b, 255); /* (2,3) */
        CHECK_EQ(pixels[15].b, 255); /* (3,3) */
    }
}
