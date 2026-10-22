/**
 * @file lwipopts.h
 * @brief Minimal lwIP options used by the core build.
 *
 * The core only needs the BSD socket API declarations. The platform lwIP
 * library keeps its own configuration; this header describes the API surface
 * visible to core translation units.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#ifndef CORE_LWIPOPTS_H
#define CORE_LWIPOPTS_H

#define LWIP_NO_ARCH_CC_H   1
#define LWIP_NO_ARCH_CPU_H  1
#define LWIP_NO_ARCH_PERF_H 1

#define LWIP_SOCKET         1
#define LWIP_NETCONN        1
#define LWIP_COMPAT_SOCKETS 1
#define LWIP_SOCKET_SELECT  1
#define LWIP_SOCKET_POLL    1

#define LWIP_DNS  1
#define LWIP_TCP  1
#define LWIP_UDP  1
#define LWIP_RAW  1
#define LWIP_ICMP 1

#define LWIP_TIMEVAL_PRIVATE 0

#define MEMP_NUM_NETCONN 16

#endif /* CORE_LWIPOPTS_H */
