/**
 * @file tal_ip_addr.h
 * @brief IPv4 accessors for TUYA_IP_ADDR_T across platforms
 *
 * TUYA_IP_ADDR_T is a plain host-order uint32 when ENABLE_IPv6 is off and a
 * tagged union when it is on. The T5AI platform header ships accessor macros
 * for both shapes, the LINUX one does not, so define the scalar fallback here
 * and let a platform that provides its own win.
 *
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __TAL_IP_ADDR_H__
#define __TAL_IP_ADDR_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TUYA_IP_ADDR_MAKE_IP4
#define TUYA_IP_ADDR_MAKE_IP4(val) ((TUYA_IP_ADDR_T)(val))
#endif

#ifndef TUYA_IP_ADDR_GET_IP4
#define TUYA_IP_ADDR_GET_IP4(addr) (addr)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __TAL_IP_ADDR_H__ */
