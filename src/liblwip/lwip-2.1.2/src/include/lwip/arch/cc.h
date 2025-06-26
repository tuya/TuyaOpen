#ifndef __CC_H__
#define __CC_H__

#include "cpu.h"

typedef unsigned char u8_t;
typedef signed char s8_t;
typedef unsigned short u16_t;
typedef signed short s16_t;
typedef unsigned int u32_t;
typedef signed long s32_t;
typedef u32_t mem_ptr_t;
typedef int sys_prot_t;

#define U16_F "d"
#define S16_F "d"
#define X16_F "x"
#define U32_F "d"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "uz"

/* define compiler specific symbols */
#if defined(__ICCARM__)
#if !defined(__IARSTDLIB__)
#define _STRING
#ifndef memcmp
#define memcmp(dst, src, sz) _memcmp(dst, src, sz)
#endif
#ifndef memset
#define memset(dst, val, sz) _memset(dst, val, sz)
#endif
#ifndef memcpy
#define memcpy(dst, src, sz) _memcpy(dst, src, sz)
#endif
#endif // __IARSTDLIB__

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_USE_INCLUDES

#elif defined(__CC_ARM)

#define PACK_STRUCT_BEGIN __packed
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#elif defined(__GNUC__)
#include <sys/types.h>
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((__packed__))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_USE_INCLUDES

#elif defined(__TASKING__)

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#endif

#define LWIP_PLATFORM_ASSERT(x)

#define LWIP_NO_STDINT_H 1

#if defined(__GNUC__) && (__GNUC__ >= 6)
//#define LWIP_TIMEVAL_PRIVATE        1

//#define IN_ADDR_T_DEFINED
#endif

#define LWIP_NO_CTYPE_H 1

#define LWIP_LIBC_HAVE_SSIZE_T 1

#define TYALIGNED(n)                                                 __attribute__((aligned(n)))
#define TYSHAREDRAM                                                  __attribute__((section("SHAREDRAM")))
#define LWIP_DECLARE_MEMORY_ALIGNED_TYSHAREDRAM(variable_name, size) u8_t variable_name[size] TYALIGNED(4) TYSHAREDRAM

#endif /* __CC_H__ */
