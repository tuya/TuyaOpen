/*
 * Copyright (C) 2026 Tuya Inc.
 *
 * pjlib OS profile for TuyaOpen — unified LINUX host + MCU (TAL/TKL).
 * Socket backend differs (POSIX vs lwIP); OS core is always os_core_tuyaos.c.
 */
#ifndef __PJ_COMPAT_OS_TUYAOS_H__
#define __PJ_COMPAT_OS_TUYAOS_H__

#define PJ_OS_NAME "tuyaos"

#define PJ_HAS_ASSERT_H       1
#define PJ_HAS_CTYPE_H        1
#define PJ_HAS_ERRNO_H        1
#define PJ_HAS_LINUX_SOCKET_H 0
#define PJ_HAS_SETJMP_H       1
#define PJ_HAS_STDARG_H       1
#define PJ_HAS_STDDEF_H       1
#define PJ_HAS_STDIO_H        1
#define PJ_HAS_STDLIB_H       1
#define PJ_HAS_STRING_H       1
#define PJ_HAS_SYS_TYPES_H    1
#define PJ_HAS_TIME_H         1
#define PJ_HAS_UNISTD_H       1
#define PJ_HAS_LIMITS_H       1

#define PJ_HAS_MSWSOCK_H  0
#define PJ_HAS_WINSOCK_H  0
#define PJ_HAS_WINSOCK2_H 0

/*
 * Sockets go through TAL on every target (sock_tal.c, sock_select_tal.c,
 * addr_resolv_tal.c, ip_helper_tal.c), so pjlib pulls in no BSD socket
 * headers and needs no chip network stack on the include path. TAL picks
 * the POSIX or TKL backend underneath.
 */
#define PJ_HAS_ARPA_INET_H    0
#define PJ_HAS_NETDB_H        0
#define PJ_HAS_NETINET_IN_H   0
#define PJ_HAS_NETINET_TCP_H  0
#define PJ_HAS_SYS_IOCTL_H    0
#define PJ_HAS_SYS_SELECT_H   0
#define PJ_HAS_SYS_SOCKET_H   0
#define PJ_HAS_LWIP_SOCKETS   0
#define PJ_HAS_SEMAPHORE_H    0
#define PJ_SELECT_NEEDS_NFDS  1
#define PJ_HAS_SYS_TIME_H     1

/* pj_sockaddr never reaches the OS now, so keep the plain BSD layout. */
#define PJ_SOCKADDR_HAS_LEN 0

/* sock_tal.c implements the address text conversions itself. */
#define PJ_SOCK_HAS_INET_ATON 0
#define PJ_SOCK_HAS_INET_NTOP 0
#define PJ_SOCK_HAS_INET_PTON 0

/* pj_in_addr / pj_in6_addr are declared by pjlib instead of <netinet/in.h>. */
#define PJ_TUYAOS_OWN_IN_ADDR 1

#if defined(__linux__)
#define PJ_HAS_MALLOC_H    1
#define PJ_HAS_SYS_TIMEB_H 1
#define PJ_HAS_LOCALTIME_R 1
#else
#define PJ_HAS_MALLOC_H    0
#define PJ_HAS_SYS_TIMEB_H 0
#define PJ_HAS_LOCALTIME_R 0
#endif

/* Name reported by pj_gethostname() where no POSIX host database exists */
#ifndef PJ_TUYAOS_HOSTNAME
#define PJ_TUYAOS_HOSTNAME "tuyaos"
#endif

#define PJ_HAS_ERRNO_VAR 1

/*
 * Without <sys/socket.h> there is no SO_ERROR to read, so pjlib uses its own
 * portable connect-completion probe (pj_sock_getpeername) in the ioqueue.
 */
#define PJ_HAS_SO_ERROR 0

#ifndef EAGAIN
#define EAGAIN 11
#endif
#ifndef EINPROGRESS
#define EINPROGRESS 115
#endif

#define PJ_BLOCKING_ERROR_VAL         EAGAIN
#define PJ_BLOCKING_CONNECT_ERROR_VAL EINPROGRESS

#ifndef PJ_HAS_THREADS
#define PJ_HAS_THREADS (1)
#endif

#define PJ_HAS_HIGH_RES_TIMER 1
#define PJ_HAS_MALLOC         1
#ifndef PJ_OS_HAS_CHECK_STACK
#define PJ_OS_HAS_CHECK_STACK 0
#endif
#define PJ_NATIVE_STRING_IS_UNICODE 0

#define PJ_ATOMIC_VALUE_TYPE long

#define PJ_EMULATE_RWMUTEX 1

#define PJ_THREAD_SET_STACK_SIZE  1
#define PJ_THREAD_ALLOCATE_STACK  0

#define PJ_HAS_SOCKLEN_T 1

/* One ioqueue backend (select) on all platforms — no epoll fork */
#ifndef PJ_HAS_LINUX_EPOLL
#define PJ_HAS_LINUX_EPOLL 0
#endif

#endif /* __PJ_COMPAT_OS_TUYAOS_H__ */
