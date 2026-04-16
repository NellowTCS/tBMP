#include "tbmp_imgio.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_LINEAR
#define STBI_NO_HDR

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Suppress warnings from vendored headers that we do not control. */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "stb_image.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static int pnm_skip(FILE *f) {
    int c = fgetc(f);
    while (c != EOF) {
        if (isspace((unsigned char)c)) {
            c = fgetc(f);
            continue;
        }
        if (c == '#') {
            while (c != EOF && c != '\n') c = fgetc(f);
            c = fgetc(f);
            continue;
        }
        ungetc(c, f);
        return 1;
    }
    return 0;
}

static int pnm_uint(FILE *f, int *out) {
    if (!pnm_skip(f)) return 0;
    int c = fgetc(f);
    if (c == EOF || !isdigit((unsigned char)c)) return 0;
    long v = 0;
    while (c != EOF && isdigit((unsigned char)c)) {
        v = v * 10 + (c - '0');
        if (v > 1 << 20) return 0;
        c = fgetc(f);
    }
    if (c != EOF) ungetc(c, f);
    *out = (int)v;
    return 1;
}

static TBmpRGBA *pnm_load(const char *path, int *w, int *h, int *ch) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    char m0 = 0, m1 = 0;
    if (fread(&m0, 1, 1, f) != 1 || fread(&m1, 1, 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    if (m0 != 'P' || (m1 != '6' && m1 != '3' && m1 != '2' && m1 != '5' &&
                      m1 != '1' && m1 != '4')) {
        fclose(f);
        return NULL;
    }

    int W = 0, H = 0, maxv = 1;
    if (!pnm_uint(f, &W) || !pnm_uint(f, &H)) {
        fclose(f);
        return NULL;
    }
    if (W <= 0 || H <= 0 || W > 100000 || H > 100000) {
        fclose(f);
        return NULL;
    }

    int ascii = (m1 == '1' || m1 == '2' || m1 == '3');
    int bw = (m1 == '1' || m1 == '4');
    int gray = (m1 == '2' || m1 == '5');
    int color = (m1 == '3' || m1 == '6');
    int C = color ? 3 : (gray ? 1 : 1);

    if (!bw) {
        if (!pnm_uint(f, &maxv) || maxv <= 0 || maxv > 255) {
            fclose(f);
            return NULL;
        }
    }

    int c = fgetc(f);
    if (c == '\r') {
        int d = fgetc(f);
        if (d != '\n') ungetc(d, f);
    }

    TBmpRGBA *pix = (TBmpRGBA *)malloc((size_t)W * H * sizeof(TBmpRGBA));
    if (!pix) {
        fclose(f);
        return NULL;
    }

    if (!ascii) {
        size_t n = (size_t)W * H * C;
        uint8_t *buf = (uint8_t *)malloc(n);
        if (!buf) {
            free(pix);
            fclose(f);
            return NULL;
        }
        if (fread(buf, 1, n, f) != n) {
            free(buf);
            free(pix);
            fclose(f);
            return NULL;
        }
        for (int i = 0; i < W * H; ++i) {
            uint8_t r = 0, g = 0, b = 0;
            if (bw) {
                uint8_t v = buf[i] ? 0 : 255;
                r = g = b = v;
            } else if (gray) {
                uint8_t v = (uint8_t)((buf[i] * 255) / maxv);
                r = g = b = v;
            } else {
                r = (uint8_t)((buf[i * 3 + 0] * 255) / maxv);
                g = (uint8_t)((buf[i * 3 + 1] * 255) / maxv);
                b = (uint8_t)((buf[i * 3 + 2] * 255) / maxv);
            }
            pix[i].r = r;
            pix[i].g = g;
            pix[i].b = b;
            pix[i].a = 255;
        }
        free(buf);
    } else {
        for (int i = 0; i < W * H; ++i) {
            int v0 = 0, v1 = 0, v2 = 0;
            uint8_t r = 0, g = 0, b = 0;
            if (bw) {
                if (!pnm_uint(f, &v0)) {
                    free(pix);
                    fclose(f);
                    return NULL;
                }
                uint8_t v = v0 ? 0 : 255;
                r = g = b = v;
            } else if (gray) {
                if (!pnm_uint(f, &v0)) {
                    free(pix);
                    fclose(f);
                    return NULL;
                }
                uint8_t v = (uint8_t)((v0 * 255) / maxv);
                r = g = b = v;
            } else {
                if (!pnm_uint(f, &v0) || !pnm_uint(f, &v1) || !pnm_uint(f, &v2)) {
                    free(pix);
                    fclose(f);
                    return NULL;
                }
                r = (uint8_t)((v0 * 255) / maxv);
                g = (uint8_t)((v1 * 255) / maxv);
                b = (uint8_t)((v2 * 255) / maxv);
            }
            pix[i].r = r;
            pix[i].g = g;
            pix[i].b = b;
            pix[i].a = 255;
        }
    }

    fclose(f);
    *w = W;
    *h = H;
    *ch = C;
    return pix;
}

TBmpRGBA *tbmp_cli_img_load(const char *path, int *out_w, int *out_h,
                            int *out_channels, int *out_uses_stb) {
    int w = 0, h = 0, ch = 0;
    TBmpRGBA *pix = pnm_load(path, &w, &h, &ch);
    if (pix) {
        *out_w = w;
        *out_h = h;
        *out_channels = ch;
        *out_uses_stb = 0;
        return pix;
    }

    int n = 0;
    unsigned char *data = stbi_load(path, &w, &h, &n, 4);
    if (!data) return NULL;

    size_t count = (size_t)w * h;
    TBmpRGBA *out = (TBmpRGBA *)data;
    *out_w = w;
    *out_h = h;
    *out_channels = n;
    *out_uses_stb = 1;
    (void)count;
    return out;
}

void tbmp_cli_img_free(TBmpRGBA *pixels, int uses_stb) {
    if (!pixels) return;
    if (uses_stb) {
        stbi_image_free((void *)pixels);
    } else {
        free(pixels);
    }
}
