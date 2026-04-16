#ifndef TBMP_MSGPACK_H
#define TBMP_MSGPACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Value types (tag discriminant). */
typedef enum TBmpMpType {
    TBMP_MP_NIL = 0,
    TBMP_MP_BOOL = 1,
    TBMP_MP_UINT = 2,
    TBMP_MP_INT = 3,
    TBMP_MP_FLOAT = 4,  /* 32-bit float  */
    TBMP_MP_DOUBLE = 5, /* 64-bit double */
    TBMP_MP_STR = 6,
    TBMP_MP_BIN = 7,
    TBMP_MP_ARRAY = 8,
    TBMP_MP_MAP = 9,
    TBMP_MP_EXT = 10,
    TBMP_MP_UNKNOWN = 255
} TBmpMpType;

/* Tag: a parsed MessagePack header (no inline payload for str/bin/ext). */
typedef struct TBmpMpTag {
    TBmpMpType type;
    union {
        bool b;       /* BOOL                                     */
        uint64_t u;   /* UINT                                     */
        int64_t i;    /* INT                                      */
        float f;      /* FLOAT                                    */
        double d;     /* DOUBLE                                   */
        uint32_t len; /* STR/BIN/EXT/ARRAY/MAP: element or count  */
    } v;
} TBmpMpTag;

/* Reader: cursor over a read-only byte slice. */
typedef struct TBmpMpReader {
    const uint8_t *data;
    size_t pos;
    size_t len;
    bool error; /* set on any malformed/truncated input */
} TBmpMpReader;

/* Initialise a reader over a memory buffer. */
void tbmp_mp_reader_init(TBmpMpReader *r, const uint8_t *data, size_t len);

/* Returns true if a previous operation set the error flag. */
bool tbmp_mp_reader_error(const TBmpMpReader *r);

/*
 * Read the next tag.  On truncation or unknown format, sets error flag
 * and returns a tag with type=TBMP_MP_UNKNOWN.
 */
TBmpMpTag tbmp_mp_read_tag(TBmpMpReader *r);

/*
 * Copy exactly count bytes from the current position into dst.
 * Sets error flag if fewer bytes remain.
 */
void tbmp_mp_read_bytes(TBmpMpReader *r, void *dst, size_t count);

/*
 * Advance past count bytes without copying.
 * Sets error flag if fewer bytes remain.
 */
void tbmp_mp_skip_bytes(TBmpMpReader *r, size_t count);

/*
 * After reading the tag for a str/bin container, call this to signal
 * the reader that all container bytes have been consumed.
 * (No-op in this implementation; exists to mirror MPack API style.)
 */
void tbmp_mp_done_str(TBmpMpReader *r);
void tbmp_mp_done_bin(TBmpMpReader *r);
void tbmp_mp_done_array(TBmpMpReader *r);
void tbmp_mp_done_map(TBmpMpReader *r);

/* Writer: cursor into a caller-supplied output buffer. */
typedef struct TBmpMpWriter {
    uint8_t *buf;
    size_t cap;
    size_t pos;
    bool error; /* set when buffer runs out of space */
} TBmpMpWriter;

/* Initialise a writer targeting a fixed-size buffer. */
void tbmp_mp_writer_init(TBmpMpWriter *w, uint8_t *buf, size_t cap);

/* Returns true if a previous write overflowed the buffer. */
bool tbmp_mp_writer_error(const TBmpMpWriter *w);

/* Bytes written so far (valid even after an error). */
size_t tbmp_mp_writer_used(const TBmpMpWriter *w);

/* Scalar writers. */
void tbmp_mp_write_nil(TBmpMpWriter *w);
void tbmp_mp_write_bool(TBmpMpWriter *w, bool v);
void tbmp_mp_write_uint(TBmpMpWriter *w, uint64_t v);
void tbmp_mp_write_int(TBmpMpWriter *w, int64_t v);
void tbmp_mp_write_double(TBmpMpWriter *w, double v);

/* String / binary writers (write header + body in one call). */
void tbmp_mp_write_str(TBmpMpWriter *w, const char *data, uint32_t len);
void tbmp_mp_write_cstr(TBmpMpWriter *w, const char *cstr);
void tbmp_mp_write_bin(TBmpMpWriter *w, const void *data, uint32_t len);
void tbmp_mp_write_raw(TBmpMpWriter *w, const uint8_t *data, uint32_t len);

/* Container header writers. */
void tbmp_mp_start_array(TBmpMpWriter *w, uint32_t count);
void tbmp_mp_start_map(TBmpMpWriter *w, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_MSGPACK_H */
