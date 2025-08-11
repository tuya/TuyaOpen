#ifndef __TUYA_COMMON_TYPES_H__
#define __TUYA_COMMON_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef int OPERATE_RET;
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
typedef unsigned int UINT_T;
typedef unsigned int *PUINT_T;
typedef int BOOL_T;
typedef BOOL_T *PBOOL_T;
typedef long long int INT64_T;
typedef INT64_T *PINT64_T;
typedef unsigned long long int UINT64_T;
typedef UINT64_T *PUINT64_T;
typedef unsigned int UINT32_T;
typedef unsigned int *PUINT32_T;
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
typedef ULONG_T TIME_MS;
typedef ULONG_T TIME_S;
typedef unsigned int TIME_T;
typedef double DOUBLE_T;
typedef unsigned short WORD_T;
typedef WORD_T *PWORD_T;
typedef unsigned int DWORD_T;
typedef DWORD_T *PDWORD_T;

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

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

#ifndef SIZEOF
#define SIZEOF sizeof
#endif

#ifdef __cplusplus
}
#endif

#endif
