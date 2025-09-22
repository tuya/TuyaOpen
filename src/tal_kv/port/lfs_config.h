#ifndef __LFS_CONFIG_H_
#define __LFS_CONFIG_H_

// System includes
#include "tal_log.h"
#include "tal_memory.h"
#include <inttypes.h>

#define LFS_NO_ASSERT
#define LFS_NO_DEBUG
#define LFS_ERROR PR_ERR
#define LFS_WARN  PR_WARN

#ifndef LFS_NO_ASSERT
#include <assert.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Macros, may be replaced by system specific wrappers. Arguments to these
// macros must not have side-effects as the macros can be removed for a smaller
// code footprint

// Logging functions
#ifndef LFS_TRACE
#ifdef LFS_YES_TRACE
#define LFS_TRACE_(fmt, ...) printf("%s:%d:trace: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define LFS_TRACE(...)       LFS_TRACE_(__VA_ARGS__, "")
#else
#define LFS_TRACE(...)
#endif
#endif

#ifndef LFS_DEBUG
#ifndef LFS_NO_DEBUG
#define LFS_DEBUG_(fmt, ...) printf("%s:%d:debug: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define LFS_DEBUG(...)       LFS_DEBUG_(__VA_ARGS__, "")
#else
#define LFS_DEBUG(...)
#endif
#endif

#ifndef LFS_WARN
#ifndef LFS_NO_WARN
#define LFS_WARN_(fmt, ...) printf("%s:%d:warn: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define LFS_WARN(...)       LFS_WARN_(__VA_ARGS__, "")
#else
#define LFS_WARN(...)
#endif
#endif

#ifndef LFS_ERROR
#ifndef LFS_NO_ERROR
#define LFS_ERROR_(fmt, ...) printf("%s:%d:error: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define LFS_ERROR(...)       LFS_ERROR_(__VA_ARGS__, "")
#else
#define LFS_ERROR(...)
#endif
#endif

// Runtime assertions
#ifndef LFS_ASSERT
#ifndef LFS_NO_ASSERT
#define LFS_ASSERT(test) assert(test)
#else
#define LFS_ASSERT(test)
#endif
#endif

// Builtin functions, these may be replaced by more efficient
// toolchain-specific implementations. LFS_NO_INTRINSICS falls back to a more
// expensive basic C implementation for debugging purposes

// Min/max functions for unsigned 32-bit numbers
static inline uint32_t lfs_max(uint32_t a, uint32_t b)
{
    return (a > b) ? a : b;
}

static inline uint32_t lfs_min(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

// Align to nearest multiple of a size
static inline uint32_t lfs_aligndown(uint32_t a, uint32_t alignment)
{
    return a - (a % alignment);
}

static inline uint32_t lfs_alignup(uint32_t a, uint32_t alignment)
{
    return lfs_aligndown(a + alignment - 1, alignment);
}

// Find the smallest power of 2 greater than or equal to a
static inline uint32_t lfs_npw2(uint32_t a)
{
#if !defined(LFS_NO_INTRINSICS) && (defined(__GNUC__) || defined(__CC_ARM))
    return 32 - __builtin_clz(a - 1);
#else
    uint32_t r = 0;
    uint32_t s;
    a -= 1;
    s = (a > 0xffff) << 4;
    a >>= s;
    r |= s;
    s = (a > 0xff) << 3;
    a >>= s;
    r |= s;
    s = (a > 0xf) << 2;
    a >>= s;
    r |= s;
    s = (a > 0x3) << 1;
    a >>= s;
    r |= s;
    return (r | (a >> 1)) + 1;
#endif
}

// Count the number of trailing binary zeros in a
// lfs_ctz(0) may be undefined
static inline uint32_t lfs_ctz(uint32_t a)
{
#if !defined(LFS_NO_INTRINSICS) && defined(__GNUC__)
    return __builtin_ctz(a);
#else
    return lfs_npw2((a & -a) + 1) - 1;
#endif
}

// Count the number of binary ones in a
static inline uint32_t lfs_popc(uint32_t a)
{
#if !defined(LFS_NO_INTRINSICS) && (defined(__GNUC__) || defined(__CC_ARM))
    return __builtin_popcount(a);
#else
    a = a - ((a >> 1) & 0x55555555);
    a = (a & 0x33333333) + ((a >> 2) & 0x33333333);
    return (((a + (a >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
#endif
}

// Find the sequence comparison of a and b, this is the distance
// between a and b ignoring overflow
static inline int lfs_scmp(uint32_t a, uint32_t b)
{
    return (int)(unsigned)(a - b);
}

// Convert between 32-bit little-endian and native order
static inline uint32_t lfs_fromle32(uint32_t a)
{
#if (defined(BYTE_ORDER) && defined(ORDER_LITTLE_ENDIAN) && BYTE_ORDER == ORDER_LITTLE_ENDIAN) ||                      \
    (defined(__BYTE_ORDER) && defined(__ORDER_LITTLE_ENDIAN) && __BYTE_ORDER == __ORDER_LITTLE_ENDIAN) ||              \
    (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return a;
#elif !defined(LFS_NO_INTRINSICS) &&                                                                                   \
    ((defined(BYTE_ORDER) && defined(ORDER_BIG_ENDIAN) && BYTE_ORDER == ORDER_BIG_ENDIAN) ||                           \
     (defined(__BYTE_ORDER) && defined(__ORDER_BIG_ENDIAN) && __BYTE_ORDER == __ORDER_BIG_ENDIAN) ||                   \
     (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
    return __builtin_bswap32(a);
#else
    return (((uint8_t *)&a)[0] << 0) | (((uint8_t *)&a)[1] << 8) | (((uint8_t *)&a)[2] << 16) |
           (((uint8_t *)&a)[3] << 24);
#endif
}

static inline uint32_t lfs_tole32(uint32_t a)
{
    return lfs_fromle32(a);
}

// Convert between 32-bit big-endian and native order
static inline uint32_t lfs_frombe32(uint32_t a)
{
#if !defined(LFS_NO_INTRINSICS) &&                                                                                     \
    ((defined(BYTE_ORDER) && defined(ORDER_LITTLE_ENDIAN) && BYTE_ORDER == ORDER_LITTLE_ENDIAN) ||                     \
     (defined(__BYTE_ORDER) && defined(__ORDER_LITTLE_ENDIAN) && __BYTE_ORDER == __ORDER_LITTLE_ENDIAN) ||             \
     (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
    return __builtin_bswap32(a);
#elif (defined(BYTE_ORDER) && defined(ORDER_BIG_ENDIAN) && BYTE_ORDER == ORDER_BIG_ENDIAN) ||                          \
    (defined(__BYTE_ORDER) && defined(__ORDER_BIG_ENDIAN) && __BYTE_ORDER == __ORDER_BIG_ENDIAN) ||                    \
    (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return a;
#else
    return (((uint8_t *)&a)[0] << 24) | (((uint8_t *)&a)[1] << 16) | (((uint8_t *)&a)[2] << 8) |
           (((uint8_t *)&a)[3] << 0);
#endif
}

static inline uint32_t lfs_tobe32(uint32_t a)
{
    return lfs_frombe32(a);
}

// Calculate CRC-32 with polynomial = 0x04c11db7
static __attribute__((unused)) uint32_t lfs_crc(uint32_t crc, const void *buffer, size_t size)
{
    static const uint32_t rtable[16] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
    };

    const uint8_t *data = (const uint8_t *)buffer;

    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 0)) & 0xf];
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 4)) & 0xf];
    }

    return crc;
}

// Allocate memory, only used if buffers are not provided to littlefs
// Note, memory must be 64-bit aligned
static inline void *lfs_malloc(size_t size)
{
#ifndef LFS_NO_MALLOC
    return tal_malloc(size);
#else
    (void)size;
    return NULL;
#endif
}

// Deallocate memory, only used if buffers are not provided to littlefs
static inline void lfs_free(void *p)
{
#ifndef LFS_NO_MALLOC
    tal_free(p);
#else
    (void)p;
#endif
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
