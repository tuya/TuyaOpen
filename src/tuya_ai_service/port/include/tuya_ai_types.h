/**
 * @file tuya_ai_types.h
 * @brief tuya_ai_types module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_AI_TYPES_H__
#define __TUYA_AI_TYPES_H__

#include "ty_cJSON.h"

#include "mix_method.h"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#ifndef VOID
#define VOID void
#endif

#ifndef VOID_T
#define VOID_T void
#endif

#ifndef CONST
#define CONST const
#endif

#ifndef STATIC
#define STATIC static
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef long long DLONG_T;
typedef DLONG_T *PDLONG_T;
typedef float FLOAT_T;
typedef FLOAT_T *PFLOAT_T;
typedef signed int INT_T;
typedef int *PINT_T;
typedef void *PVOID_T;
typedef char CHAR_T;
typedef char *PCHAR_T;
typedef signed char SCHAR_T;
typedef unsigned char UCHAR_T;
typedef short SHORT_T;
typedef unsigned short USHORT_T;
typedef short *PSHORT_T;
typedef long LONG_T;
typedef unsigned long ULONG_T;
typedef long *PLONG_T;
typedef unsigned char BYTE_T;
typedef BYTE_T *PBYTE_T;
typedef uint32_t UINT_T;
typedef uint32_t *PUINT_T;
typedef int64_t INT64_T;
typedef INT64_T *PINT64_T;
typedef uint64_t UINT64_T;
typedef UINT64_T *PUINT64_T;
typedef uint32_t UINT32_T;
typedef uint32_t *PUINT32_T;
typedef int INT32_T;
typedef int *PINT32_T;
typedef short INT16_T;
typedef INT16_T *PINT16_T;
typedef unsigned short UINT16_T;
typedef UINT16_T *PUINT16_T;
typedef signed char INT8_T;
typedef INT8_T *PINT8_T;
typedef unsigned char UINT8_T;
typedef UINT8_T *PUINT8_T;
typedef double DOUBLE_T;
typedef unsigned short WORD_T;
typedef WORD_T *PWORD_T;
typedef unsigned int DWORD_T;
typedef DWORD_T *PDWORD_T;
typedef size_t SIZE_T;
/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_AI_TYPES_H__ */
