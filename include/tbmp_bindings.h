/*
 * Minimal header for Rust bindings - only includes tBMP types
 * This avoids pulling in system headers that cause warnings
 */

#ifndef TBMP_BINDINGS_H
#define TBMP_BINDINGS_H

/* Define fixed-width integers ourselves to avoid system headers */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long int64_t;

/* Include the public API */
#include "tbmp_types.h"
#include "tbmp_reader.h"
#include "tbmp_decode.h"
#include "tbmp_pixel.h"
#include "tbmp_write.h"
#include "tbmp_rotate.h"
#include "tbmp_meta.h"

#endif /* TBMP_BINDINGS_H */
