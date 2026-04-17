#include "test_helpers.h"
#include "test_suites.h"
#include "tbmp_reader.h"
#include "tbmp_decode.h"
#include "tbmp_pixel.h"
#include "tbmp_write.h"

#include <stdlib.h>

void test_writer_roundtrip(void) {
    SUITE("Writer/RoundTrip");

    /* Encode a 2x2 RGBA8888 image, then decode it and verify. */
    {
        TBmpRGBA src_pixels[4] = {
            {255, 0, 0, 255},
            {0, 255, 0, 255},
            {0, 0, 255, 255},
            {255, 255, 255, 128},
        };
        TBmpFrame src = {2, 2, src_pixels};

        TBmpWriteParams params;
        tbmp_write_default_params(&params);
        params.encoding = TBMP_ENC_RAW;
        params.pixel_format = TBMP_FMT_RGBA_8888;
        params.bit_depth = 32;

        size_t buf_needed = tbmp_write_needed_size(&src, &params);
        CHECK_GT(buf_needed, 0U);

        uint8_t *enc_buf = malloc(buf_needed);
        CHECK_NE(enc_buf, NULL);
        if (!enc_buf)
            return;

        size_t written = 0;
        CHECK_OK(tbmp_write(&src, &params, enc_buf, buf_needed, &written));
        CHECK_GT(written, 0U);
        CHECK_LE(written, buf_needed);

        /* Decode the encoded buffer */
        TBmpImage img;
        CHECK_OK(tbmp_open(enc_buf, written, &img));
        CHECK_EQ(img.head.width, 2);
        CHECK_EQ(img.head.height, 2);

        TBmpRGBA out_pixels[4];
        TBmpFrame out_frame = {0, 0, out_pixels};
        CHECK_OK(tbmp_decode(&img, &out_frame));

        CHECK_EQ(out_pixels[0].r, 255);
        CHECK_EQ(out_pixels[0].g, 0);
        CHECK_EQ(out_pixels[1].g, 255);
        CHECK_EQ(out_pixels[2].b, 255);
        CHECK_EQ(out_pixels[3].a, 128);

        free(enc_buf);
    }

    /* Encode with palette (INDEX_8 RAW) */
    {
        TBmpPalette pal;
        pal.count = 4;
        pal.entries[0] = (TBmpRGBA){0, 0, 0, 255};
        pal.entries[1] = (TBmpRGBA){255, 0, 0, 255};
        pal.entries[2] = (TBmpRGBA){0, 255, 0, 255};
        pal.entries[3] = (TBmpRGBA){0, 0, 255, 255};

        /* Source: 4x1 where pixels match palette indices exactly */
        TBmpRGBA src_px[4] = {{0, 0, 0, 255},
                              {255, 0, 0, 255},
                              {0, 255, 0, 255},
                              {0, 0, 255, 255}};
        TBmpFrame src = {4, 1, src_px};

        TBmpWriteParams params;
        tbmp_write_default_params(&params);
        params.encoding = TBMP_ENC_RAW;
        params.pixel_format = TBMP_FMT_INDEX_8;
        params.bit_depth = 8;
        params.palette = &pal;

        size_t cap = tbmp_write_needed_size(&src, &params);
        uint8_t *enc_buf = malloc(cap);
        CHECK_NE(enc_buf, NULL);
        if (!enc_buf)
            return;

        size_t written = 0;
        CHECK_OK(tbmp_write(&src, &params, enc_buf, cap, &written));

        TBmpImage img;
        CHECK_OK(tbmp_open(enc_buf, written, &img));
        CHECK_EQ(img.has_palette, 1);
        CHECK_EQ(img.palette.count, 4U);

        TBmpRGBA out_px[4];
        TBmpFrame out = {0, 0, out_px};
        CHECK_OK(tbmp_decode(&img, &out));

        /* Palette round-trip: should match original colours */
        CHECK_EQ(out_px[0].r, 0);
        CHECK_EQ(out_px[1].r, 255);
        CHECK_EQ(out_px[2].g, 255);
        CHECK_EQ(out_px[3].b, 255);

        free(enc_buf);
    }

    /* RLE round-trip */
    {
        /* 4x1 of alternating red/green encoded as RLE */
        TBmpRGBA src_px[4] = {{255, 0, 0, 255},
                              {255, 0, 0, 255},
                              {0, 255, 0, 255},
                              {0, 255, 0, 255}};
        TBmpFrame src = {4, 1, src_px};

        TBmpWriteParams params;
        tbmp_write_default_params(&params);
        params.encoding = TBMP_ENC_RLE;
        params.pixel_format = TBMP_FMT_RGB_332;
        params.bit_depth = 8;

        size_t cap = tbmp_write_needed_size(&src, &params);
        uint8_t *enc_buf = malloc(cap);
        CHECK_NE(enc_buf, NULL);
        if (!enc_buf)
            return;

        size_t written = 0;
        CHECK_OK(tbmp_write(&src, &params, enc_buf, cap, &written));

        TBmpImage img;
        CHECK_OK(tbmp_open(enc_buf, written, &img));
        TBmpRGBA out_px[4];
        TBmpFrame out = {0, 0, out_px};
        CHECK_OK(tbmp_decode(&img, &out));

        /* After quantization to RGB332 and back, red channel should still be
     * non-zero for first two and zero for last two */
        CHECK_GT((int)out_px[0].r, 0);
        CHECK_GT((int)out_px[1].r, 0);
        CHECK_EQ(out_px[2].r, 0);
        CHECK_EQ(out_px[3].r, 0);
        CHECK_GT((int)out_px[2].g, 0);
        CHECK_GT((int)out_px[3].g, 0);

        free(enc_buf);
    }

    /* All encoding modes round-trip (INDEX_8 exact palette values). */
    {
        TBmpPalette pal;
        pal.count = 5;
        pal.entries[0] = (TBmpRGBA){0, 0, 0, 255};
        pal.entries[1] = (TBmpRGBA){255, 0, 0, 255};
        pal.entries[2] = (TBmpRGBA){0, 255, 0, 255};
        pal.entries[3] = (TBmpRGBA){0, 0, 255, 255};
        pal.entries[4] = (TBmpRGBA){255, 255, 255, 255};

        TBmpRGBA src_px[64];
        for (int i = 0; i < 64; i++) {
            switch (i % 5) {
            case 0:
                src_px[i] = pal.entries[0];
                break;
            case 1:
                src_px[i] = pal.entries[1];
                break;
            case 2:
                src_px[i] = pal.entries[2];
                break;
            case 3:
                src_px[i] = pal.entries[3];
                break;
            default:
                src_px[i] = pal.entries[4];
                break;
            }
        }

        TBmpFrame src = {8, 8, src_px};
        TBmpEncoding modes[] = {
            TBMP_ENC_RAW,          TBMP_ENC_ZERO_RANGE, TBMP_ENC_RLE,
            TBMP_ENC_SPAN,         TBMP_ENC_SPARSE_PIXEL,
            TBMP_ENC_BLOCK_SPARSE,
        };

        for (size_t mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++) {
            TBmpWriteParams params;
            tbmp_write_default_params(&params);
            params.encoding = modes[mi];
            params.pixel_format = TBMP_FMT_INDEX_8;
            params.bit_depth = 8;
            params.palette = &pal;

            size_t cap = tbmp_write_needed_size(&src, &params);
            CHECK_GT(cap, 0U);

            uint8_t *enc_buf = malloc(cap);
            CHECK_NE(enc_buf, NULL);
            if (!enc_buf)
                return;

            size_t written = 0;
            CHECK_OK(tbmp_write(&src, &params, enc_buf, cap, &written));
            CHECK_GT(written, 0U);

            TBmpImage img;
            CHECK_OK(tbmp_open(enc_buf, written, &img));
            CHECK_EQ(img.head.encoding, (uint8_t)modes[mi]);

            TBmpRGBA out_px[64];
            TBmpFrame out = {0, 0, out_px};
            CHECK_OK(tbmp_decode(&img, &out));
            CHECK_EQ(memcmp(src_px, out_px, sizeof(src_px)), 0);

            free(enc_buf);
        }
    }

    /* Auto-palette + dithering helpers */
    {
        TBmpRGBA src_px[16] = {
            {255, 0, 0, 255},   {220, 20, 20, 255}, {0, 255, 0, 255},
            {20, 220, 20, 255}, {0, 0, 255, 255},   {20, 20, 220, 255},
            {255, 255, 0, 255}, {220, 220, 20, 255},
            {0, 255, 255, 255}, {20, 220, 220, 255},
            {255, 0, 255, 255}, {220, 20, 220, 255},
            {64, 64, 64, 255},  {96, 96, 96, 255},   {192, 192, 192, 255},
            {240, 240, 240, 255},
        };
        TBmpFrame frame = {4, 4, src_px};

        TBmpPalette pal;
        CHECK_OK(tbmp_auto_palette_from_frame(&frame, 4, &pal));
        CHECK_EQ(pal.count, 4U);

        CHECK_OK(tbmp_dither_to_palette(&frame, &pal));
    }

    /* Encoding heuristic helper */
    {
        TBmpRGBA src_px[64];
        for (int i = 0; i < 64; i++)
            src_px[i] = (TBmpRGBA){255, 0, 0, 255};

        TBmpFrame frame = {8, 8, src_px};
        TBmpWriteParams params;
        tbmp_write_default_params(&params);
        params.pixel_format = TBMP_FMT_RGB_332;
        params.bit_depth = 8;

        TBmpEncoding picked = tbmp_pick_best_encoding(&frame, &params);
        CHECK_EQ(picked, TBMP_ENC_RLE);
    }
}
