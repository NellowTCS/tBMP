#ifndef TBMP_TEST_HELPERS_H
#define TBMP_TEST_HELPERS_H

#include "tbmp_types.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Minimal test framework. */

extern int g_pass;
extern int g_fail;
extern const char *g_current_suite;

#define SUITE(name)                                                            \
    do {                                                                       \
        g_current_suite = (name);                                              \
        printf("\n=== %s ===\n", name);                                        \
    } while (0)

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (expr) {                                                            \
            g_pass++;                                                          \
        } else {                                                               \
            g_fail++;                                                          \
            fprintf(stderr, "  FAIL  %s  line %d:  %s\n", g_current_suite,     \
                    __LINE__, #expr);                                          \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_OK(e) CHECK((e) == TBMP_OK)
#define CHECK_ERR(e, expected) CHECK((e) == (expected))

/*
 * build_tbmp - build a minimal valid tBMP byte buffer.
 *
 * Returns the number of bytes written, or 0 on failure.
 */

static inline size_t build_tbmp(uint8_t *buf, size_t cap, uint16_t version,
                                uint16_t width, uint16_t height,
                                uint8_t bit_depth, uint8_t encoding,
                                uint8_t pixel_format, uint8_t flags,
                                const uint8_t *data, uint32_t data_size,
                                const uint8_t *extra, uint32_t extra_size,
                                const uint8_t *meta_blob, uint32_t meta_size) {
    size_t total = 4U + 26U + data_size + extra_size + meta_size;
    if (total > cap)
        return 0;

    uint8_t *p = buf;
    memcpy(p, "tBMP", 4);
    p += 4;

    p[0] = (uint8_t)(version & 0xFF);
    p[1] = (uint8_t)(version >> 8);
    p += 2;

    p[0] = (uint8_t)(width & 0xFF);
    p[1] = (uint8_t)(width >> 8);
    p += 2;

    p[0] = (uint8_t)(height & 0xFF);
    p[1] = (uint8_t)(height >> 8);
    p += 2;

    p[0] = bit_depth;
    p[1] = encoding;
    p[2] = pixel_format;
    p[3] = flags;
    p += 4;

#define BT_PUT_U32(v)                                                          \
    do {                                                                       \
        p[0] = (uint8_t)((v)&0xFF);                                            \
        p[1] = (uint8_t)(((v) >> 8) & 0xFF);                                   \
        p[2] = (uint8_t)(((v) >> 16) & 0xFF);                                  \
        p[3] = (uint8_t)(((v) >> 24) & 0xFF);                                  \
        p += 4;                                                                \
    } while (0)

    BT_PUT_U32(data_size);
    BT_PUT_U32(extra_size);
    BT_PUT_U32(meta_size);
    BT_PUT_U32(0U); /* reserved */

#undef BT_PUT_U32

    if (data_size > 0) {
        memcpy(p, data, data_size);
        p += data_size;
    }
    if (extra_size > 0) {
        memcpy(p, extra, extra_size);
        p += extra_size;
    }
    if (meta_size > 0) {
        memcpy(p, meta_blob, meta_size);
        p += meta_size;
    }

    return (size_t)(p - buf);
}

/* build_palt_extra - build a PALT EXTRA chunk. */

static inline size_t build_palt_extra(uint8_t *buf, size_t cap,
                                      const TBmpRGBA *entries, uint32_t count) {
    size_t needed = 8U + 4U + count * 4U;
    if (needed > cap)
        return 0;

    uint8_t *p = buf;
    memcpy(p, "PALT", 4);
    p += 4;

    uint32_t body = 4U + count * 4U;
    p[0] = (uint8_t)(body & 0xFF);
    p[1] = (uint8_t)((body >> 8) & 0xFF);
    p[2] = (uint8_t)((body >> 16) & 0xFF);
    p[3] = (uint8_t)((body >> 24) & 0xFF);
    p += 4;

    p[0] = (uint8_t)(count & 0xFF);
    p[1] = (uint8_t)((count >> 8) & 0xFF);
    p[2] = (uint8_t)((count >> 16) & 0xFF);
    p[3] = (uint8_t)((count >> 24) & 0xFF);
    p += 4;

    for (uint32_t i = 0; i < count; i++) {
        p[0] = entries[i].r;
        p[1] = entries[i].g;
        p[2] = entries[i].b;
        p[3] = entries[i].a;
        p += 4;
    }

    return needed;
}

/* build_mask_extra - build a MASK EXTRA chunk. */

static inline size_t build_mask_extra(uint8_t *buf, size_t cap, TBmpMasks m) {
    if (cap < 8U + 16U)
        return 0;

    uint8_t *p = buf;
    memcpy(p, "MASK", 4);
    p += 4;

    p[0] = 16;
    p[1] = p[2] = p[3] = 0;
    p += 4;

#define BT_W32(v)                                                              \
    do {                                                                       \
        p[0] = (uint8_t)((v)&0xFF);                                            \
        p[1] = (uint8_t)(((v) >> 8) & 0xFF);                                   \
        p[2] = (uint8_t)(((v) >> 16) & 0xFF);                                  \
        p[3] = (uint8_t)(((v) >> 24) & 0xFF);                                  \
        p += 4;                                                                \
    } while (0)

    BT_W32(m.r);
    BT_W32(m.g);
    BT_W32(m.b);
    BT_W32(m.a);

#undef BT_W32

    return 24U;
}

#endif /* TBMP_TEST_HELPERS_H */
