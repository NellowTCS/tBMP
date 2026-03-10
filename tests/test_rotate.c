#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "test_helpers.h"
#include "test_suites.h"
#include "tbmp_rotate.h"

#include <math.h>
#include <stdlib.h>

/* Rotation: exact 90/180/270 */
void test_rotate_exact(void) {
    SUITE("Rotate/Exact");

    /* Build a 2x3 source frame:
   *   [A B]
   *   [C D]
   *   [E F]
   */
    TBmpRGBA src_pixels[6];
    TBmpFrame src = {2, 3, src_pixels};
    src_pixels[0] = (TBmpRGBA){1, 0, 0, 255}; /* A */
    src_pixels[1] = (TBmpRGBA){2, 0, 0, 255}; /* B */
    src_pixels[2] = (TBmpRGBA){3, 0, 0, 255}; /* C */
    src_pixels[3] = (TBmpRGBA){4, 0, 0, 255}; /* D */
    src_pixels[4] = (TBmpRGBA){5, 0, 0, 255}; /* E */
    src_pixels[5] = (TBmpRGBA){6, 0, 0, 255}; /* F */

    /* --- 90° CW ---
   * Expected 3x2 output:
   *   [E C A]
   *   [F D B]
   */
    {
        TBmpRGBA dst_pixels[6];
        TBmpFrame dst = {0, 0, dst_pixels};
        CHECK_OK(tbmp_rotate90(&src, &dst));
        CHECK_EQ(dst.width, 3);
        CHECK_EQ(dst.height, 2);
        /* Row 0: E(5), C(3), A(1) */
        CHECK_EQ(dst_pixels[0].r, 5);
        CHECK_EQ(dst_pixels[1].r, 3);
        CHECK_EQ(dst_pixels[2].r, 1);
        /* Row 1: F(6), D(4), B(2) */
        CHECK_EQ(dst_pixels[3].r, 6);
        CHECK_EQ(dst_pixels[4].r, 4);
        CHECK_EQ(dst_pixels[5].r, 2);
    }

    /* --- 180° ---
   * Expected 2x3 output (reversed):
   *   [F E]
   *   [D C]
   *   [B A]
   */
    {
        TBmpRGBA dst_pixels[6];
        TBmpFrame dst = {0, 0, dst_pixels};
        CHECK_OK(tbmp_rotate180(&src, &dst));
        CHECK_EQ(dst.width, 2);
        CHECK_EQ(dst.height, 3);
        CHECK_EQ(dst_pixels[0].r, 6); /* F */
        CHECK_EQ(dst_pixels[1].r, 5); /* E */
        CHECK_EQ(dst_pixels[4].r, 2); /* B */
        CHECK_EQ(dst_pixels[5].r, 1); /* A */
    }

    /* --- 270° CW (= 90° CCW) ---
   * Expected 3x2 output:
   *   [B D F]
   *   [A C E]
   */
    {
        TBmpRGBA dst_pixels[6];
        TBmpFrame dst = {0, 0, dst_pixels};
        CHECK_OK(tbmp_rotate270(&src, &dst));
        CHECK_EQ(dst.width, 3);
        CHECK_EQ(dst.height, 2);
        CHECK_EQ(dst_pixels[0].r, 2); /* B */
        CHECK_EQ(dst_pixels[1].r, 4); /* D */
        CHECK_EQ(dst_pixels[2].r, 6); /* F */
        CHECK_EQ(dst_pixels[3].r, 1); /* A */
        CHECK_EQ(dst_pixels[4].r, 3); /* C */
        CHECK_EQ(dst_pixels[5].r, 5); /* E */
    }

    /* --- Rotate 180 twice = identity --- */
    {
        TBmpRGBA tmp[6], out[6];
        TBmpFrame t1 = {0, 0, tmp};
        TBmpFrame t2 = {0, 0, out};
        CHECK_OK(tbmp_rotate180(&src, &t1));
        CHECK_OK(tbmp_rotate180(&t1, &t2));
        for (int i = 0; i < 6; i++) {
            CHECK_EQ(out[i].r, src_pixels[i].r);
        }
    }

    /* --- Null pointer guards --- */
    {
        TBmpRGBA dummy[4];
        TBmpFrame d = {2, 2, dummy};
        CHECK_ERR(tbmp_rotate90(NULL, &d), TBMP_ERR_NULL_PTR);
        CHECK_ERR(tbmp_rotate90(&src, NULL), TBMP_ERR_NULL_PTR);
        TBmpFrame no_px = {2, 2, NULL};
        CHECK_ERR(tbmp_rotate90(&src, &no_px), TBMP_ERR_NULL_PTR);
    }
}

/* Suite: Rotation - arbitrary angle */
void test_rotate_arbitrary(void) {
    SUITE("Rotate/Arbitrary");

    /* 1x1 image - rotation should produce a 1x1 result regardless of angle */
    {
        TBmpRGBA src_px = {100, 150, 200, 255};
        TBmpFrame src = {1, 1, &src_px};

        TBmpRGBA dst_px[4];
        TBmpFrame dst = {0, 0, dst_px};
        TBmpRGBA bg = {0, 0, 0, 255};

        CHECK_OK(tbmp_rotate(&src, &dst, 0.0, bg, TBMP_ROTATE_NEAREST));
        CHECK_EQ(dst_px[0].r, 100);
        CHECK_EQ(dst_px[0].g, 150);
    }

    /* Output dims for 90° rotation of a square should be the same */
    {
        uint16_t ow, oh;
        tbmp_rotate_output_dims(4, 4, M_PI / 2.0, &ow, &oh);
        /* For a 4x4 image rotated 90°, output should be ~4x4 */
        CHECK_GE(ow, 4U);
        CHECK_GE(oh, 4U);
    }

    /* Rotate a 3x3 by 0 radians -> identity */
    {
        TBmpRGBA src_pix[9];
        TBmpFrame src = {3, 3, src_pix};
        for (int i = 0; i < 9; i++)
            src_pix[i] = (TBmpRGBA){(uint8_t)(i * 10), 0, 0, 255};

        uint16_t ow, oh;
        tbmp_rotate_output_dims(3, 3, 0.0, &ow, &oh);
        TBmpRGBA *dst_pix = malloc((size_t)ow * oh * sizeof(TBmpRGBA));
        CHECK_NE(dst_pix, NULL);
        if (dst_pix) {
            TBmpFrame dst = {0, 0, dst_pix};
            TBmpRGBA bg = {0, 0, 0, 255};
            CHECK_OK(tbmp_rotate(&src, &dst, 0.0, bg, TBMP_ROTATE_NEAREST));
            /* Centre pixel should still be src_pix[4] */
            uint32_t cx = dst.width / 2;
            uint32_t cy = dst.height / 2;
            TBmpRGBA centre = dst_pix[cy * dst.width + cx];
            CHECK_EQ(centre.r, 40); /* src_pix[4].r = 40 */
            free(dst_pix);
        }
    }
}
