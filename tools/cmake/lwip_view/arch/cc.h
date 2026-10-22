/**
 * @file arch/cc.h
 * @brief Compiler abstraction for the core's lwIP header view.
 *
 * Header-only definitions used while compiling core translation units. The
 * platform lwIP library is built with its own compiler abstraction.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS
#define htons(x) __builtin_bswap16(x)
#define ntohs(x) __builtin_bswap16(x)
#define htonl(x) __builtin_bswap32(x)
#define ntohl(x) __builtin_bswap32(x)

#define LWIP_NOASSERT 1

typedef uint8_t  u8_t;
typedef int8_t   s8_t;
typedef uint16_t u16_t;
typedef int16_t  s16_t;
typedef uint32_t u32_t;
typedef int32_t  s32_t;

typedef int sys_prot_t;

#define S16_F "d"
#define U16_F "d"
#define X16_F "x"

#define S32_F "d"
#define U32_F "u"
#define X32_F "x"

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT   __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_DIAG(x)                                                                                          \
    do {                                                                                                               \
    } while (0)
#define LWIP_PLATFORM_ASSERT(x)                                                                                        \
    do {                                                                                                               \
    } while (0)

#endif /* __ARCH_CC_H__ */
