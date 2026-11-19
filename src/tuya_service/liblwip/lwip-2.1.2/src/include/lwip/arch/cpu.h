
#ifndef __CPU_H__
#define __CPU_H__

#include "tuya_iot_config.h"

#if !defined(BYTE_ORDER)

#if defined(LITTLE_END) && (LITTLE_END == 1)
#define BYTE_ORDER LITTLE_ENDIAN
#else
#define BYTE_ORDER BIG_ENDIAN
#endif

#endif

#endif /* __CPU_H__ */
