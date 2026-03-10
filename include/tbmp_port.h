/* tBMP portability and platform-abstraction macros.
 *
 * Include this header (or define its macros before including any tBMP header)
 * to control platform-specific behaviour.  All macros have safe defaults so
 * that nothing needs to change for a standard hosted C11 build.
 *
 * TBMP_NO_FLOAT
 *   Define to compile out all floating-point code paths.
 *   Affected: tbmp_rotate() returns TBMP_ERR_NOT_SUPPORTED;
 *             tbmp_rotate_output_dims() copies src dimensions unchanged.
 *   Unaffected: tbmp_rotate90/180/270(), all encoder/decoder paths,
 *               all pixel-format conversion paths.
 *
 * TBMP_ASSERT(expr)
 *   Override the assertion macro used by tBMP internals.  Defaults to a
 *   no-op on bare-metal targets (avoids pulling in <assert.h>).  Define to
 *   your platform assert if you want hard faults on logic errors.
 *
 * TBMP_STATIC_ASSERT(expr, msg)
 *   Compile-time assertion.  Defaults to C11 _Static_assert.  Override for
 *   C99 environments.
 *
 * Endianness: tBMP is always little-endian on the wire.  The reader/writer
 * use explicit byte-by-byte I/O and never cast pointers to wider types, so
 * no endianness macro is needed for correctness.
 *
 */

#ifndef TBMP_PORT_H
#define TBMP_PORT_H

#include <stdint.h>

#if defined(TBMP_NO_FLOAT)
#define TBMP_HAS_FLOAT 0
#else
#define TBMP_HAS_FLOAT 1
#endif

/* TBMP_ERR_NOT_SUPPORTED is returned by tbmp_rotate() when TBMP_NO_FLOAT is
 * defined.  The value -19 is one past the last code in tbmp_types.h. */
#define TBMP_ERR_NOT_SUPPORTED (-19)

/* Runtime assertion; no-op by default for bare-metal safety. */
#ifndef TBMP_ASSERT
#define TBMP_ASSERT(expr) ((void)(expr))
#endif

/* Compile-time assertion. */
#ifndef TBMP_STATIC_ASSERT
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define TBMP_STATIC_ASSERT(expr, msg) _Static_assert(expr, #msg)
#else
/* C99 fallback: declare an array of size 1 or -1. */
#define TBMP_STATIC_ASSERT(expr, msg)                                          \
    typedef char tbmp_static_assert_##msg[(expr) ? 1 : -1]
#endif
#endif

/* Inline hint. */
#ifndef TBMP_INLINE
#if defined(__GNUC__) || defined(__clang__)
#define TBMP_INLINE __attribute__((always_inline)) static inline
#else
#define TBMP_INLINE static inline
#endif
#endif

/* Compile-time wire-format size checks; catch padding surprises early. */
TBMP_STATIC_ASSERT(sizeof(uint8_t) == 1, uint8_must_be_1_byte);
TBMP_STATIC_ASSERT(sizeof(uint16_t) == 2, uint16_must_be_2_bytes);
TBMP_STATIC_ASSERT(sizeof(uint32_t) == 4, uint32_must_be_4_bytes);

#endif /* TBMP_PORT_H */
