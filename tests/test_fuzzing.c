#include "test_helpers.h"
#include "test_suites.h"

#include "tbmp_decode.h"
#include "tbmp_reader.h"
#include "tbmp_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int file_exists_and_nonempty(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    return st.st_size > 0;
}

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static uint32_t count_equal_px(const TBmpRGBA *a, const TBmpRGBA *b,
                               size_t n) {
    uint32_t eq = 0;
    for (size_t i = 0; i < n; i++) {
        if (a[i].r == b[i].r && a[i].g == b[i].g && a[i].b == b[i].b &&
            a[i].a == b[i].a) {
            eq++;
        }
    }
    return eq;
}

void test_fuzzing(void) {
    SUITE("Fuzzing/FetchAndRoundTrip");

    /* Integration check: fetch API metadata and a seeded image to /tmp. */
    {
        const char *list_path = "/tmp/tbmp_picsum_list.json";
        const char *img_path = "/tmp/tbmp_picsum_seed.jpg";

        int rc_list = system(
            "curl -fsSL 'https://picsum.photos/v2/list?page=1&limit=8' "
            "-o /tmp/tbmp_picsum_list.json");
        CHECK_EQ(rc_list, 0);
        CHECK(file_exists_and_nonempty(list_path));

        int rc_img = system(
            "curl -fsSL 'https://picsum.photos/seed/tbmp-fuzz/96/96.jpg' "
            "-o /tmp/tbmp_picsum_seed.jpg");
        CHECK_EQ(rc_img, 0);
        CHECK(file_exists_and_nonempty(img_path));
    }

    /* Deterministic fuzzing across all encodings with per-mode thresholds. */
    {
        TBmpPalette pal;
        pal.count = 6;
        pal.entries[0] = (TBmpRGBA){0, 0, 0, 255};
        pal.entries[1] = (TBmpRGBA){255, 0, 0, 255};
        pal.entries[2] = (TBmpRGBA){0, 255, 0, 255};
        pal.entries[3] = (TBmpRGBA){0, 0, 255, 255};
        pal.entries[4] = (TBmpRGBA){255, 255, 255, 255};
        pal.entries[5] = (TBmpRGBA){255, 255, 0, 255};

        TBmpEncoding modes[] = {
            TBMP_ENC_RAW,          TBMP_ENC_ZERO_RANGE, TBMP_ENC_RLE,
            TBMP_ENC_SPAN,         TBMP_ENC_SPARSE_PIXEL,
            TBMP_ENC_BLOCK_SPARSE,
        };
        int min_permille[] = {
            1000, /* raw */
            1000, /* zero-range */
            1000, /* rle */
            1000, /* span */
            1000, /* sparse */
            1000, /* block-sparse */
        };

        uint32_t rng = 0xC0FFEE11u;
        for (size_t mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++) {
            for (int iter = 0; iter < 18; iter++) {
                uint16_t w = (uint16_t)(1u + (xorshift32(&rng) % 48u));
                uint16_t h = (uint16_t)(1u + (xorshift32(&rng) % 48u));
                size_t px_count = (size_t)w * (size_t)h;

                TBmpRGBA *src_px =
                    (TBmpRGBA *)malloc(px_count * sizeof(TBmpRGBA));
                CHECK_NE(src_px, NULL);
                if (!src_px)
                    return;

                for (size_t i = 0; i < px_count; i++) {
                    uint32_t idx = xorshift32(&rng) % pal.count;
                    src_px[i] = pal.entries[idx];
                }

                TBmpFrame src = {w, h, src_px};

                TBmpWriteParams p;
                tbmp_write_default_params(&p);
                p.pixel_format = TBMP_FMT_INDEX_8;
                p.bit_depth = 8;
                p.palette = &pal;
                p.encoding = modes[mi];

                size_t needed = tbmp_write_needed_size(&src, &p);
                CHECK_GT(needed, 0u);

                uint8_t *encoded = (uint8_t *)malloc(needed);
                CHECK_NE(encoded, NULL);
                if (!encoded) {
                    free(src_px);
                    return;
                }

                size_t out_len = 0;
                TBmpError we = tbmp_write(&src, &p, encoded, needed, &out_len);
                CHECK_OK(we);
                CHECK_GT(out_len, 0u);

                TBmpImage img;
                CHECK_OK(tbmp_open(encoded, out_len, &img));
                CHECK_EQ(img.head.width, w);
                CHECK_EQ(img.head.height, h);
                CHECK_EQ(img.head.encoding, (uint8_t)modes[mi]);

                TBmpRGBA *dst_px =
                    (TBmpRGBA *)malloc(px_count * sizeof(TBmpRGBA));
                CHECK_NE(dst_px, NULL);
                if (!dst_px) {
                    free(encoded);
                    free(src_px);
                    return;
                }

                TBmpFrame dst = {0, 0, dst_px};
                CHECK_OK(tbmp_decode(&img, &dst));
                CHECK_EQ(dst.width, w);
                CHECK_EQ(dst.height, h);

                uint32_t eq = count_equal_px(src_px, dst_px, px_count);
                uint32_t permille = (uint32_t)((eq * 1000u) / px_count);
                CHECK_GE((int)permille, min_permille[mi]);

                free(dst_px);
                free(encoded);
                free(src_px);
            }
        }
    }
}
