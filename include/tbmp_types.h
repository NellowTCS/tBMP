#ifndef TBMP_TYPES_H
#define TBMP_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Four-byte magic at offset 0. */
#define TBMP_MAGIC "tBMP"
#define TBMP_MAGIC_LEN 4U

/* Supported version values stored in TBmpHead::version (little-endian). */
#define TBMP_VERSION_1_0 UINT16_C(0x0100) /* 1.0 - basic tBMP            */

/* Maximum sane image dimension (64 K - 1 pixels per axis). */
#define TBMP_MAX_DIM UINT16_C(0xFFFF)

/* Maximum palette entries for an 8-bit indexed image. */
#define TBMP_MAX_PALETTE 256U

/* TBmpHead::flags bitfield. */
#define TBMP_FLAG_HAS_PALETTE UINT8_C(0x01) /* bit 0 */
#define TBMP_FLAG_HAS_MASKS UINT8_C(0x02)   /* bit 1 */

/* Encoding modes stored in TBmpHead::encoding. */
typedef enum TBmpEncoding {
    TBMP_ENC_RAW = 0,          /* literal packed pixel bits               */
    TBMP_ENC_ZERO_RANGE = 1,   /* non-zero pixels + zero-range table      */
    TBMP_ENC_RLE = 2,          /* (count, value) run-length pairs         */
    TBMP_ENC_SPAN = 3,         /* (pixel_index, length, value) triples    */
    TBMP_ENC_SPARSE_PIXEL = 4, /* (x, y, value) explicit pixel list       */
    TBMP_ENC_BLOCK_SPARSE = 5, /* tile-indexed block data                 */
    TBMP_ENC_MAX_ = 6          /* sentinel, not a valid encoding          */
} TBmpEncoding;

/* Pixel formats stored in TBmpHead::pixel_format. */
typedef enum TBmpPixelFormat {
    TBMP_FMT_INDEX_1 = 0,   /* 1-bit palette index                      */
    TBMP_FMT_INDEX_2 = 1,   /* 2-bit palette index                      */
    TBMP_FMT_INDEX_4 = 2,   /* 4-bit palette index                      */
    TBMP_FMT_INDEX_8 = 3,   /* 8-bit palette index                      */
    TBMP_FMT_RGB_565 = 4,   /* 16-bit packed RGB                        */
    TBMP_FMT_RGB_555 = 5,   /* 15-bit packed RGB (1 bit padding)        */
    TBMP_FMT_RGB_444 = 6,   /* 12-bit packed RGB (4 bits padding)       */
    TBMP_FMT_RGB_332 = 7,   /* 8-bit packed RGB                         */
    TBMP_FMT_RGB_888 = 8,   /* 24-bit RGB                               */
    TBMP_FMT_RGBA_8888 = 9, /* 32-bit RGBA                              */
    TBMP_FMT_CUSTOM = 10,   /* bit-mask defined format (MASK chunk req.)*/
    TBMP_FMT_MAX_ = 11      /* sentinel                                 */
} TBmpPixelFormat;

/* Return codes used throughout the library. */
typedef enum TBmpError {
    TBMP_OK = 0,
    TBMP_ERR_NULL_PTR = -1,
    TBMP_ERR_BAD_MAGIC = -2,
    TBMP_ERR_BAD_VERSION = -3,
    TBMP_ERR_BAD_BIT_DEPTH = -4,
    TBMP_ERR_BAD_ENCODING = -5,
    TBMP_ERR_BAD_PIXEL_FORMAT = -6,
    TBMP_ERR_BAD_DIMENSIONS = -7,
    TBMP_ERR_TRUNCATED = -8,        /* buffer too small for declared sizes */
    TBMP_ERR_OVERFLOW = -9,         /* arithmetic overflow in size calc    */
    TBMP_ERR_NO_PALETTE = -10,      /* indexed format but no palette       */
    TBMP_ERR_NO_MASKS = -11,        /* CUSTOM format but no MASK chunk     */
    TBMP_ERR_BAD_PALETTE = -12,     /* palette_count exceeds max           */
    TBMP_ERR_BAD_SPAN = -13,        /* span index/length overflows canvas  */
    TBMP_ERR_BAD_BLOCK = -14,       /* block_index out of range            */
    TBMP_ERR_BAD_PIXEL_COORD = -15, /* sparse pixel coord out of range     */
    TBMP_ERR_OUT_OF_MEMORY = -16,   /* caller-supplied buffer too small    */
    TBMP_ERR_ZERO_DIMENSIONS = -17, /* width or height is zero             */
    TBMP_ERR_INCONSISTENT = -18,    /* data_size inconsistent with encoding*/
} TBmpError;

/*
 * TBmpHead mirrors the tBMP.ksy wire layout (little-endian).  Callers MUST
 * use the tbmp_read_* helpers rather than casting raw pointers so that
 * unaligned-access targets (e.g. Cortex-M0) remain safe.
 */
typedef struct TBmpHead {
    uint16_t version;     /* TBMP_VERSION_1_0                         */
    uint16_t width;       /* image width  in pixels (1..65535)        */
    uint16_t height;      /* image height in pixels (1..65535)        */
    uint8_t bit_depth;    /* bits per pixel: 1,2,4,8,16,24,32         */
    uint8_t encoding;     /* TBmpEncoding                             */
    uint8_t pixel_format; /* TBmpPixelFormat                          */
    uint8_t flags;        /* TBMP_FLAG_* bitfield                     */
    uint32_t data_size;   /* byte length of DATA section              */
    uint32_t extra_size;  /* byte length of EXTRA section (0=absent)  */
    uint32_t meta_size;   /* byte length of META section  (0=absent)  */
    uint32_t reserved;    /* must be zero on write, ignored on read   */
} TBmpHead;

/*
 * TBMP_HEAD_WIRE_SIZE is the byte size of the header on the wire:
 *
 *   version      u16   2
 *   width        u16   2
 *   height       u16   2
 *   bit_depth    u8    1
 *   encoding     u8    1
 *   pixel_format u8    1
 *   flags        u8    1
 *   data_size    u32   4
 *   extra_size   u32   4
 *   meta_size    u32   4
 *   reserved     u32   4
 *                    = 26 bytes total
 *
 * sizeof(TBmpHead) may be larger due to alignment padding.  Always use
 * TBMP_HEAD_WIRE_SIZE when reading/writing the binary format.
 */
#define TBMP_HEAD_WIRE_SIZE 26U

/* Minimum valid encoded size: 4 (magic) + 26 (header) = 30 bytes. */
#define TBMP_MIN_FILE_SIZE (TBMP_MAGIC_LEN + TBMP_HEAD_WIRE_SIZE)

/* In-memory decoded pixel; always RGBA8888. */
typedef struct TBmpRGBA {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} TBmpRGBA;

/* In-memory palette. */
typedef struct TBmpPalette {
    uint32_t count;                     /* number of valid entries        */
    TBmpRGBA entries[TBMP_MAX_PALETTE]; /* RGBA8888 palette entries       */
} TBmpPalette;

/* In-memory color masks (for TBMP_FMT_CUSTOM). */
typedef struct TBmpMasks {
    uint32_t r;
    uint32_t g;
    uint32_t b;
    uint32_t a;
} TBmpMasks;

/*
 * TBmpImage is returned by tbmp_open().  All pointers alias into the
 * caller-supplied buffer; no heap allocation is performed.
 */
typedef struct TBmpImage {
    TBmpHead head; /* copy of the parsed header           */

    const uint8_t *data; /* pointer into caller buffer, DATA    */
    size_t data_len;     /* == head.data_size                   */

    const uint8_t *extra; /* pointer into caller buffer, EXTRA   */
    size_t extra_len;     /* == head.extra_size                  */

    const uint8_t *meta; /* pointer into caller buffer, META    */
    size_t meta_len;     /* == head.meta_size                   */

    int has_palette;     /* non-zero if palette was found       */
    TBmpPalette palette; /* decoded palette (if has_palette)    */

    int has_masks;   /* non-zero if masks were found        */
    TBmpMasks masks; /* decoded masks  (if has_masks)       */
} TBmpImage;

/* Decoded frame with a caller-owned, heap-free pixel buffer. */
typedef struct TBmpFrame {
    uint16_t width;
    uint16_t height;
    TBmpRGBA *pixels; /* caller must provide width*height TBmpRGBA slots */
} TBmpFrame;

/*
 * META section: a raw MessagePack blob containing a single top-level map
 * whose keys are UTF-8 strings and whose values are typed scalars.
 * tbmp_meta_parse() decodes it into TBmpMeta without heap allocation.
 */

/* Maximum top-level string length (without NUL). */
#define TBMP_META_FIELD_MAX 127U

/* Maximum key string length inside custom map entries. */
#define TBMP_META_KEY_MAX 63U

/* Maximum single tag length (without NUL). */
#define TBMP_META_TAG_MAX 31U

/* Maximum number of tags in structured metadata. */
#define TBMP_META_MAX_TAGS 16U

/* Maximum number of custom map entries in `custom` array. */
#define TBMP_META_MAX_CUSTOM_ITEMS 16U

/* Maximum encoded bytes per custom map item. */
#define TBMP_META_CUSTOM_ITEM_MAX 256U

/* One raw MessagePack map blob from structured `custom` array. */
typedef struct TBmpMetaCustomItem {
    uint32_t len;
    uint8_t data[TBMP_META_CUSTOM_ITEM_MAX];
} TBmpMetaCustomItem;

/* Strict structured META representation. */
typedef struct TBmpMeta {
    char title[TBMP_META_FIELD_MAX + 1U];
    char author[TBMP_META_FIELD_MAX + 1U];
    char description[TBMP_META_FIELD_MAX + 1U];
    char created[TBMP_META_FIELD_MAX + 1U];
    char software[TBMP_META_FIELD_MAX + 1U];
    char license[TBMP_META_FIELD_MAX + 1U];

    uint8_t has_dpi;
    uint32_t dpi;

    char colorspace[TBMP_META_FIELD_MAX + 1U];

    uint32_t tag_count;
    char tags[TBMP_META_MAX_TAGS][TBMP_META_TAG_MAX + 1U];

    uint32_t custom_count;
    TBmpMetaCustomItem custom[TBMP_META_MAX_CUSTOM_ITEMS];
} TBmpMeta;

#ifdef __cplusplus
}
#endif

#endif /* TBMP_TYPES_H */
