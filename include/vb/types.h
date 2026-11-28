#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef ptrdiff_t isize;

#if defined(__SIZEOF_INT128)
typedef __uint128_t u128;
typedef __int128_t i128;
#endif

typedef float f32;
typedef double f64;

#ifdef __cplusplus
static_assert(sizeof(f32) == 4, "f32 must be 32 bits");
static_assert(sizeof(f64) == 8, "f64 must be 64 bits");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
static_assert(sizeof(f32) == 4, "f32 must be 32 bits");
static_assert(sizeof(f64) == 8, "f64 must be 64 bits");
#endif
