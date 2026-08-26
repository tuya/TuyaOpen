/**
 * @file tuya_cloud_types.h
 * @brief Minimal type shim for host unit tests of netmgr_retry.c.
 *
 * netmgr_retry.h's only #include is "tuya_cloud_types.h", and netmgr_retry.c's
 * only #include is netmgr_retry.h. The real tuya_cloud_types.h pulls in
 * tuya_iot_config.h, which pulls in the build-generated tuya_kconfig.h - so
 * compiling against the real header would make this test depend on a full
 * `tos.py build`, which is the surest way to end up with a test nobody runs.
 *
 * This file defines EXACTLY the identifiers netmgr_retry.h and netmgr_retry.c
 * actually use and nothing else:
 *
 *   - uint32_t, int32_t   (via <stdint.h>)
 *   - NULL                (via <stddef.h>)
 *   - BOOL_T, TRUE, FALSE
 *
 * It is put on the include path with `-I tests/netmgr/shim`, BEFORE the real
 * tuya_cloud_types.h's directory, so the compiler finds this one first and
 * never touches tuya_iot_config.h or tuya_kconfig.h at all.
 *
 * This is an assumption, not a fact, and it can go stale: see the "Type shim"
 * section of tests/netmgr/README.md for what was checked, how, and what
 * breaks if a future tuya_cloud_types.h changes these definitions.
 */

#ifndef __TESTS_NETMGR_SHIM_TUYA_CLOUD_TYPES_H__
#define __TESTS_NETMGR_SHIM_TUYA_CLOUD_TYPES_H__

#include <stddef.h>
#include <stdint.h>

/*
 * Verified 2026-08-26 against every tuya_cloud_types.h in the tree
 * (platform/{LN882H,LINUX,T2,T5AI,BK7231X,T3}/.../tuya_cloud_types.h,
 * platform/ESP32/.../tuya_cloud_types.h, tools/porting/adapter/.../
 * tuya_cloud_types.h - eight files, five distinct md5sums, all agreeing on
 * this fragment):
 *
 *     typedef int BOOL_T;
 *     ...
 *     #ifndef FALSE
 *     #define FALSE 0
 *     #endif
 *     #ifndef TRUE
 *     #define TRUE 1
 *     #endif
 */
typedef int BOOL_T;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#endif /* __TESTS_NETMGR_SHIM_TUYA_CLOUD_TYPES_H__ */
