#include "tbmp_msgpack.h"

#include <stdint.h>
#include <string.h> /* memcpy */

/*
Adapted from https://github.com/ludocode/mpack
*/

/* Internal helpers. */

/* rd_u8 - read one byte; set error and return 0 if exhausted. */
static uint8_t rd_u8(TBmpMpReader *r) {
    if (r->pos >= r->len) {
        r->error = true;
        return 0;
    }
    return r->data[r->pos++];
}

static uint16_t rd_u16be(TBmpMpReader *r) {
    uint8_t a = rd_u8(r), b = rd_u8(r);
    return (uint16_t)((uint16_t)a << 8 | b);
}

static uint32_t rd_u32be(TBmpMpReader *r) {
    uint8_t a = rd_u8(r), b = rd_u8(r), c = rd_u8(r), d = rd_u8(r);
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) |
           (uint32_t)d;
}

static uint64_t rd_u64be(TBmpMpReader *r) {
    uint32_t hi = rd_u32be(r), lo = rd_u32be(r);
    return ((uint64_t)hi << 32) | lo;
}

/* wr_u8 - write one byte; set error if buffer full. */
static void wr_u8(TBmpMpWriter *w, uint8_t v) {
    if (w->pos >= w->cap) {
        w->error = true;
        return;
    }
    w->buf[w->pos++] = v;
}

static void wr_u16be(TBmpMpWriter *w, uint16_t v) {
    wr_u8(w, (uint8_t)(v >> 8));
    wr_u8(w, (uint8_t)(v));
}

static void wr_u32be(TBmpMpWriter *w, uint32_t v) {
    wr_u8(w, (uint8_t)(v >> 24));
    wr_u8(w, (uint8_t)(v >> 16));
    wr_u8(w, (uint8_t)(v >> 8));
    wr_u8(w, (uint8_t)(v));
}

static void wr_u64be(TBmpMpWriter *w, uint64_t v) {
    wr_u32be(w, (uint32_t)(v >> 32));
    wr_u32be(w, (uint32_t)(v));
}

static void wr_bytes(TBmpMpWriter *w, const void *src, size_t n) {
    if (w->pos + n > w->cap) {
        w->error = true;
        return;
    }
    memcpy(w->buf + w->pos, src, n);
    w->pos += n;
}

/* Reader API. */

void tbmp_mp_reader_init(TBmpMpReader *r, const uint8_t *data, size_t len) {
    r->data = data;
    r->pos = 0;
    r->len = len;
    r->error = false;
}

bool tbmp_mp_reader_error(const TBmpMpReader *r) {
    return r->error;
}

TBmpMpTag tbmp_mp_read_tag(TBmpMpReader *r) {
    TBmpMpTag tag;
    tag.type = TBMP_MP_UNKNOWN;
    tag.v.u = 0;

    if (r->error)
        return tag;

    uint8_t b = rd_u8(r);
    if (r->error)
        return tag;

    /* positive fixint (0x00-0x7f) */
    if (b <= 0x7f) {
        tag.type = TBMP_MP_UINT;
        tag.v.u = b;
        return tag;
    }
    /* negative fixint (0xe0-0xff) */
    if (b >= 0xe0) {
        tag.type = TBMP_MP_INT;
        tag.v.i = (int8_t)b;
        return tag;
    }
    /* fixmap (0x80-0x8f) */
    if ((b & 0xf0) == 0x80) {
        tag.type = TBMP_MP_MAP;
        tag.v.len = b & 0x0f;
        return tag;
    }
    /* fixarray (0x90-0x9f) */
    if ((b & 0xf0) == 0x90) {
        tag.type = TBMP_MP_ARRAY;
        tag.v.len = b & 0x0f;
        return tag;
    }
    /* fixstr (0xa0-0xbf) */
    if ((b & 0xe0) == 0xa0) {
        tag.type = TBMP_MP_STR;
        tag.v.len = b & 0x1f;
        return tag;
    }

    switch (b) {
    case 0xc0:
        tag.type = TBMP_MP_NIL;
        break;
    case 0xc2:
        tag.type = TBMP_MP_BOOL;
        tag.v.b = false;
        break;
    case 0xc3:
        tag.type = TBMP_MP_BOOL;
        tag.v.b = true;
        break;

    case 0xc4: /* bin 8  */
        tag.type = TBMP_MP_BIN;
        tag.v.len = rd_u8(r);
        break;
    case 0xc5: /* bin 16 */
        tag.type = TBMP_MP_BIN;
        tag.v.len = rd_u16be(r);
        break;
    case 0xc6: /* bin 32 */
        tag.type = TBMP_MP_BIN;
        tag.v.len = rd_u32be(r);
        break;

    case 0xc7: /* ext 8  */ {
        uint32_t n = rd_u8(r);
        rd_u8(r);
        tag.type = TBMP_MP_EXT;
        tag.v.len = n;
    } break;
    case 0xc8: /* ext 16 */ {
        uint32_t n = rd_u16be(r);
        rd_u8(r);
        tag.type = TBMP_MP_EXT;
        tag.v.len = n;
    } break;
    case 0xc9: /* ext 32 */ {
        uint32_t n = rd_u32be(r);
        rd_u8(r);
        tag.type = TBMP_MP_EXT;
        tag.v.len = n;
    } break;

    case 0xca: { /* float 32 */
        uint32_t bits = rd_u32be(r);
        float fv;
        memcpy(&fv, &bits, sizeof(fv));
        tag.type = TBMP_MP_FLOAT;
        tag.v.f = fv;
        break;
    }
    case 0xcb: { /* float 64 */
        uint64_t bits = rd_u64be(r);
        double dv;
        memcpy(&dv, &bits, sizeof(dv));
        tag.type = TBMP_MP_DOUBLE;
        tag.v.d = dv;
        break;
    }

    case 0xcc:
        tag.type = TBMP_MP_UINT;
        tag.v.u = rd_u8(r);
        break;
    case 0xcd:
        tag.type = TBMP_MP_UINT;
        tag.v.u = rd_u16be(r);
        break;
    case 0xce:
        tag.type = TBMP_MP_UINT;
        tag.v.u = rd_u32be(r);
        break;
    case 0xcf:
        tag.type = TBMP_MP_UINT;
        tag.v.u = rd_u64be(r);
        break;

    case 0xd0:
        tag.type = TBMP_MP_INT;
        tag.v.i = (int8_t)rd_u8(r);
        break;
    case 0xd1:
        tag.type = TBMP_MP_INT;
        tag.v.i = (int16_t)rd_u16be(r);
        break;
    case 0xd2:
        tag.type = TBMP_MP_INT;
        tag.v.i = (int32_t)rd_u32be(r);
        break;
    case 0xd3:
        tag.type = TBMP_MP_INT;
        tag.v.i = (int64_t)rd_u64be(r);
        break;

    case 0xd4: /* fixext 1  */
        rd_u8(r);
        rd_u8(r);
        tag.type = TBMP_MP_EXT;
        tag.v.len = 1;
        break;
    case 0xd5: /* fixext 2  */
        rd_u8(r);
        rd_u16be(r);
        tag.type = TBMP_MP_EXT;
        tag.v.len = 2;
        break;
    case 0xd6: /* fixext 4  */ {
        rd_u8(r);
        rd_u32be(r);
        tag.type = TBMP_MP_EXT;
        tag.v.len = 4;
    } break;
    case 0xd7: /* fixext 8  */ {
        uint64_t tmp;
        rd_u8(r);
        tmp = rd_u64be(r);
        (void)tmp;
        tag.type = TBMP_MP_EXT;
        tag.v.len = 8;
    } break;
    case 0xd8: /* fixext 16 */ {
        /* 16 payload bytes + 1 type byte already consumed; skip the 16 payload bytes */
        rd_u8(r); /* type byte */
        tbmp_mp_skip_bytes(r, 16);
        tag.type = TBMP_MP_EXT;
        tag.v.len = 16;
        break;
    }

    case 0xd9:
        tag.type = TBMP_MP_STR;
        tag.v.len = rd_u8(r);
        break;
    case 0xda:
        tag.type = TBMP_MP_STR;
        tag.v.len = rd_u16be(r);
        break;
    case 0xdb:
        tag.type = TBMP_MP_STR;
        tag.v.len = rd_u32be(r);
        break;

    case 0xdc:
        tag.type = TBMP_MP_ARRAY;
        tag.v.len = rd_u16be(r);
        break;
    case 0xdd:
        tag.type = TBMP_MP_ARRAY;
        tag.v.len = rd_u32be(r);
        break;

    case 0xde:
        tag.type = TBMP_MP_MAP;
        tag.v.len = rd_u16be(r);
        break;
    case 0xdf:
        tag.type = TBMP_MP_MAP;
        tag.v.len = rd_u32be(r);
        break;

    default:
        r->error = true;
        break;
    }

    return tag;
}

void tbmp_mp_read_bytes(TBmpMpReader *r, void *dst, size_t count) {
    if (r->error || count == 0)
        return;
    if (r->pos + count > r->len) {
        r->error = true;
        return;
    }
    memcpy(dst, r->data + r->pos, count);
    r->pos += count;
}

void tbmp_mp_skip_bytes(TBmpMpReader *r, size_t count) {
    if (r->error || count == 0)
        return;
    if (r->pos + count > r->len) {
        r->error = true;
        return;
    }
    r->pos += count;
}

/* Container-done calls: no-op (tracking not needed in this implementation). */
void tbmp_mp_done_str(TBmpMpReader *r) {
    (void)r;
}
void tbmp_mp_done_bin(TBmpMpReader *r) {
    (void)r;
}
void tbmp_mp_done_array(TBmpMpReader *r) {
    (void)r;
}
void tbmp_mp_done_map(TBmpMpReader *r) {
    (void)r;
}

/* Writer API. */

void tbmp_mp_writer_init(TBmpMpWriter *w, uint8_t *buf, size_t cap) {
    w->buf = buf;
    w->cap = cap;
    w->pos = 0;
    w->error = false;
}

bool tbmp_mp_writer_error(const TBmpMpWriter *w) {
    return w->error;
}

size_t tbmp_mp_writer_used(const TBmpMpWriter *w) {
    return w->pos;
}

void tbmp_mp_write_nil(TBmpMpWriter *w) {
    wr_u8(w, 0xc0);
}

void tbmp_mp_write_bool(TBmpMpWriter *w, bool v) {
    wr_u8(w, v ? 0xc3 : 0xc2);
}

void tbmp_mp_write_uint(TBmpMpWriter *w, uint64_t v) {
    if (v <= 0x7f) {
        wr_u8(w, (uint8_t)v);
    } else if (v <= 0xff) {
        wr_u8(w, 0xcc);
        wr_u8(w, (uint8_t)v);
    } else if (v <= 0xffff) {
        wr_u8(w, 0xcd);
        wr_u16be(w, (uint16_t)v);
    } else if (v <= 0xffffffff) {
        wr_u8(w, 0xce);
        wr_u32be(w, (uint32_t)v);
    } else {
        wr_u8(w, 0xcf);
        wr_u64be(w, v);
    }
}

void tbmp_mp_write_int(TBmpMpWriter *w, int64_t v) {
    if (v >= 0) {
        tbmp_mp_write_uint(w, (uint64_t)v);
        return;
    }
    if (v >= -32) {
        wr_u8(w, (uint8_t)(int8_t)v); /* negative fixint */
    } else if (v >= -128) {
        wr_u8(w, 0xd0);
        wr_u8(w, (uint8_t)(int8_t)v);
    } else if (v >= -32768) {
        wr_u8(w, 0xd1);
        wr_u16be(w, (uint16_t)(int16_t)v);
    } else if (v >= -2147483648LL) {
        wr_u8(w, 0xd2);
        wr_u32be(w, (uint32_t)(int32_t)v);
    } else {
        wr_u8(w, 0xd3);
        wr_u64be(w, (uint64_t)v);
    }
}

void tbmp_mp_write_double(TBmpMpWriter *w, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    wr_u8(w, 0xcb);
    wr_u64be(w, bits);
}

void tbmp_mp_write_str(TBmpMpWriter *w, const char *data, uint32_t len) {
    if (len <= 31) {
        wr_u8(w, (uint8_t)(0xa0 | len));
    } else if (len <= 0xff) {
        wr_u8(w, 0xd9);
        wr_u8(w, (uint8_t)len);
    } else if (len <= 0xffff) {
        wr_u8(w, 0xda);
        wr_u16be(w, (uint16_t)len);
    } else {
        wr_u8(w, 0xdb);
        wr_u32be(w, len);
    }
    wr_bytes(w, data, len);
}

void tbmp_mp_write_cstr(TBmpMpWriter *w, const char *cstr) {
    if (cstr == NULL) {
        tbmp_mp_write_nil(w);
        return;
    }
    /* Compute length up to a reasonable cap to stay embedded-safe. */
    size_t n = 0;
    while (n < 0xffffffffu && cstr[n] != '\0')
        n++;
    tbmp_mp_write_str(w, cstr, (uint32_t)n);
}

void tbmp_mp_write_bin(TBmpMpWriter *w, const void *data, uint32_t len) {
    if (len <= 0xff) {
        wr_u8(w, 0xc4);
        wr_u8(w, (uint8_t)len);
    } else if (len <= 0xffff) {
        wr_u8(w, 0xc5);
        wr_u16be(w, (uint16_t)len);
    } else {
        wr_u8(w, 0xc6);
        wr_u32be(w, len);
    }
    wr_bytes(w, data, len);
}

void tbmp_mp_start_map(TBmpMpWriter *w, uint32_t count) {
    if (count <= 15) {
        wr_u8(w, (uint8_t)(0x80 | count));
    } else if (count <= 0xffff) {
        wr_u8(w, 0xde);
        wr_u16be(w, (uint16_t)count);
    } else {
        wr_u8(w, 0xdf);
        wr_u32be(w, count);
    }
}
