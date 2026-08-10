/**
 * @file tal_sock_compat.h
 * @brief Missing BSD socket constants for lwIP/TuyaOpen RTOS
 *
 * Included only from pj compat socket.h when PJ_HAS_LWIP_SOCKETS.
 * Does not affect LINUX / native POSIX builds.
 */
#ifndef __TAL_SOCK_COMPAT_H__
#define __TAL_SOCK_COMPAT_H__

#ifndef AF_UNIX
#define AF_UNIX 1
#endif
#ifndef SOCK_RDM
#define SOCK_RDM 4
#endif
#ifndef MSG_DONTROUTE
#define MSG_DONTROUTE 0x04
#endif
#ifndef MSG_OOB
#define MSG_OOB 0x01
#endif
#ifndef MSG_PEEK
#define MSG_PEEK 0x02
#endif
#ifndef MSG_WAITALL
#define MSG_WAITALL 0x100
#endif

/*
 * When LWIP_IPV6 is off, lwIP defines AF_INET6 as AF_UNSPEC (0).
 * pj treats sa_family == AF_INET6 as IPv6, so a zero-initialized sockaddr
 * is mis-detected as IPv6. Keep the standard AF_INET6 value for pj
 * bookkeeping only; creating real IPv6 sockets still fails at lwIP if
 * IPv6 is disabled.
 */
#if !defined(LWIP_IPV6) || (LWIP_IPV6 == 0)
#if defined(AF_INET6) && defined(AF_UNSPEC)
#if AF_INET6 == AF_UNSPEC
#undef AF_INET6
#define AF_INET6 10
#endif
#endif
#endif

#endif /* __TAL_SOCK_COMPAT_H__ */
