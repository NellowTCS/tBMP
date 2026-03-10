#include "test_helpers.h"
#include "test_suites.h"
#include "tbmp_pixel.h"

void test_pixel_formats(void) {
    SUITE("PixelFormats");

    /* RGB565 - known values */
    {
        /* 0xF800 = R:31, G:0, B:0 -> red */
        TBmpRGBA c = tbmp_pixel_to_rgba(0xF800U, TBMP_FMT_RGB_565, NULL, NULL);
        CHECK_EQ(c.r, 255);
        CHECK_EQ(c.g, 0);
        CHECK_EQ(c.b, 0);
        CHECK_EQ(c.a, 255);
    }
    {
        /* 0x07E0 = R:0, G:63, B:0 -> green */
        TBmpRGBA c = tbmp_pixel_to_rgba(0x07E0U, TBMP_FMT_RGB_565, NULL, NULL);
        CHECK_EQ(c.r, 0);
        CHECK_EQ(c.g, 255);
        CHECK_EQ(c.b, 0);
    }
    {
        /* 0x001F = R:0, G:0, B:31 -> blue */
        TBmpRGBA c = tbmp_pixel_to_rgba(0x001FU, TBMP_FMT_RGB_565, NULL, NULL);
        CHECK_EQ(c.r, 0);
        CHECK_EQ(c.g, 0);
        CHECK_EQ(c.b, 255);
    }
    {
        /* 0xFFFF -> white */
        TBmpRGBA c = tbmp_pixel_to_rgba(0xFFFFU, TBMP_FMT_RGB_565, NULL, NULL);
        CHECK_EQ(c.r, 255);
        CHECK_EQ(c.g, 255);
        CHECK_EQ(c.b, 255);
    }

    /* RGB555 */
    {
        /* 0x7C00 = R:31, G:0, B:0 -> red */
        TBmpRGBA c = tbmp_pixel_to_rgba(0x7C00U, TBMP_FMT_RGB_555, NULL, NULL);
        CHECK_EQ(c.r, 255);
        CHECK_EQ(c.g, 0);
        CHECK_EQ(c.b, 0);
    }

    /* RGB888 */
    {
        TBmpRGBA c =
            tbmp_pixel_to_rgba(0x123456U, TBMP_FMT_RGB_888, NULL, NULL);
        CHECK_EQ(c.r, 0x12);
        CHECK_EQ(c.g, 0x34);
        CHECK_EQ(c.b, 0x56);
        CHECK_EQ(c.a, 255);
    }

    /* RGBA8888 */
    {
        TBmpRGBA c =
            tbmp_pixel_to_rgba(0x12345678U, TBMP_FMT_RGBA_8888, NULL, NULL);
        CHECK_EQ(c.r, 0x12);
        CHECK_EQ(c.g, 0x34);
        CHECK_EQ(c.b, 0x56);
        CHECK_EQ(c.a, 0x78);
    }

    /* RGB332 - red = top 3 bits */
    {
        TBmpRGBA c = tbmp_pixel_to_rgba(0xE0U, TBMP_FMT_RGB_332, NULL, NULL);
        CHECK_EQ(c.r, 255);
        CHECK_EQ(c.g, 0);
        CHECK_EQ(c.b, 0);
    }

    /* RGB444 */
    {
        TBmpRGBA c = tbmp_pixel_to_rgba(0xF00U, TBMP_FMT_RGB_444, NULL, NULL);
        CHECK_EQ(c.r, 255);
        CHECK_EQ(c.g, 0);
        CHECK_EQ(c.b, 0);
    }

    /* Palette lookup */
    {
        TBmpPalette pal;
        pal.count = 3;
        TBmpRGBA e0 = {10, 20, 30, 255};
        TBmpRGBA e1 = {40, 50, 60, 255};
        TBmpRGBA e2 = {70, 80, 90, 128};
        pal.entries[0] = e0;
        pal.entries[1] = e1;
        pal.entries[2] = e2;

        TBmpRGBA c0 = tbmp_palette_lookup(&pal, 0);
        CHECK_EQ(c0.r, 10);
        TBmpRGBA c1 = tbmp_palette_lookup(&pal, 1);
        CHECK_EQ(c1.g, 50);
        TBmpRGBA c2 = tbmp_palette_lookup(&pal, 2);
        CHECK_EQ(c2.a, 128);

        /* Out-of-range -> black */
        TBmpRGBA co = tbmp_palette_lookup(&pal, 3);
        CHECK_EQ(co.r, 0);
        CHECK_EQ(co.a, 0);

        /* NULL palette -> black */
        TBmpRGBA cn = tbmp_palette_lookup(NULL, 0);
        CHECK_EQ(cn.r, 0);
    }

    /* CUSTOM masked */
    {
        /* Emulate RGB565 via custom masks: R=0xF800, G=0x07E0, B=0x001F, A=0 */
        TBmpMasks masks = {0xF800U, 0x07E0U, 0x001FU, 0x0000U};
        TBmpRGBA c = tbmp_pixel_to_rgba(0xF800U, TBMP_FMT_CUSTOM, NULL, &masks);
        CHECK_EQ(c.r, 255);
        CHECK_EQ(c.g, 0);
        CHECK_EQ(c.b, 0);
        CHECK_EQ(c.a, 255); /* no alpha mask -> opaque */
    }
}
