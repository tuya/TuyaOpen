/**
 * @file sock_tal.c
 * @brief pj socket backend on TuyaOpen TAL — replaces sock_bsd.c
 *
 * One code path for every target: TAL dispatches to the POSIX backend on
 * LINUX and to TKL (lwIP) on MCU, so pjlib needs no BSD socket headers and
 * no chip tree include paths.
 *
 * Scope is IPv4, matching PJ_HAS_IPV6 == 0. IPv6 entry points return
 * PJ_EIPV6NOTSUP rather than asserting.
 *
 * Address convention: pj_sockaddr keeps network byte order (sin_addr,
 * sin_port), TAL takes and returns host byte order. Every crossing goes
 * through pj_sockaddr_to_tal() / tal_to_pj_sockaddr().
 *
 * @copyright Copyright (c) Tuya Inc.
 */
#include <pj/sock.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>

#include "tal_api.h"
#include "tal_network.h"
#include "tal_ip_addr.h"
#include "tal_local_ip.h"

#define THIS_FILE "sock_tal.c"

/*
 * Constants exported to the rest of pjlib.
 *
 * sock_bsd.c published the native values because it handed them straight to
 * the OS. Here every option is translated in pj_sock_setsockopt(), so the
 * values only have to be stable and unique within pjlib.
 */
const pj_uint16_t PJ_AF_UNSPEC = 0;
const pj_uint16_t PJ_AF_UNIX = 1;
const pj_uint16_t PJ_AF_INET = 2;
const pj_uint16_t PJ_AF_INET6 = 10;
const pj_uint16_t PJ_AF_PACKET = 17;
const pj_uint16_t PJ_AF_IRDA = 23;

const pj_uint16_t PJ_SOCK_STREAM = 1;
const pj_uint16_t PJ_SOCK_DGRAM = 2;
const pj_uint16_t PJ_SOCK_RAW = 3;
const pj_uint16_t PJ_SOCK_RDM = 4;

const pj_uint16_t PJ_SOL_SOCKET = 0xF000;
const pj_uint16_t PJ_SOL_IP = 0xF001;
const pj_uint16_t PJ_SOL_TCP = 0xF002;
const pj_uint16_t PJ_SOL_UDP = 0xF003;
const pj_uint16_t PJ_SOL_IPV6 = 0xF004;

const pj_uint16_t PJ_SO_TYPE = 1;
const pj_uint16_t PJ_SO_RCVBUF = 2;
const pj_uint16_t PJ_SO_SNDBUF = 3;
const pj_uint16_t PJ_SO_REUSEADDR = 4;
const pj_uint16_t PJ_SO_NOSIGPIPE = 5;
const pj_uint16_t PJ_SO_PRIORITY = 6;
const pj_uint16_t PJ_TCP_NODELAY = 7;

const pj_uint16_t PJ_IP_TOS = 8;
const pj_uint16_t PJ_IPTOS_LOWDELAY = 0x10;
const pj_uint16_t PJ_IPTOS_THROUGHPUT = 0x08;
const pj_uint16_t PJ_IPTOS_RELIABILITY = 0x04;
const pj_uint16_t PJ_IPTOS_MINCOST = 0x02;
const pj_uint16_t PJ_IPV6_TCLASS = 9;

const pj_uint16_t PJ_IP_MULTICAST_IF = 10;
const pj_uint16_t PJ_IP_MULTICAST_TTL = 11;
const pj_uint16_t PJ_IP_MULTICAST_LOOP = 12;
const pj_uint16_t PJ_IP_ADD_MEMBERSHIP = 13;
const pj_uint16_t PJ_IP_DROP_MEMBERSHIP = 14;

/*
 * Byte order helpers. sock_bsd.c forwarded these to the C library; without
 * the socket headers pjlib provides them from its own endian configuration.
 */
static pj_uint16_t tal_swap16(pj_uint16_t v)
{
    return (pj_uint16_t)(((v & 0x00FFu) << 8) | ((v & 0xFF00u) >> 8));
}

static pj_uint32_t tal_swap32(pj_uint32_t v)
{
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) | ((v & 0x00FF0000u) >> 8) |
           ((v & 0xFF000000u) >> 24);
}

PJ_DEF(pj_uint16_t) pj_ntohs(pj_uint16_t netshort)
{
#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN != 0
    return tal_swap16(netshort);
#else
    return netshort;
#endif
}

PJ_DEF(pj_uint16_t) pj_htons(pj_uint16_t hostshort)
{
    return pj_ntohs(hostshort);
}

PJ_DEF(pj_uint32_t) pj_ntohl(pj_uint32_t netlong)
{
#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN != 0
    return tal_swap32(netlong);
#else
    return netlong;
#endif
}

PJ_DEF(pj_uint32_t) pj_htonl(pj_uint32_t hostlong)
{
    return pj_ntohl(hostlong);
}

/*
 * Address text conversion. TAL offers str2addr()/addr2str(), but addr2str()
 * hands back a shared static buffer on some backends, so pjlib formats and
 * parses IPv4 itself. Both are pure and stack-only.
 */
static pj_bool_t tal_parse_ipv4(const char *s, pj_uint32_t *out_hbo)
{
    pj_uint32_t addr = 0;
    int octet = 0;
    int digits = 0;
    int value = 0;

    for (;; ++s) {
        if (pj_isdigit((int)(unsigned char)*s)) {
            if (++digits > 3)
                return PJ_FALSE;
            value = value * 10 + (*s - '0');
            if (value > 255)
                return PJ_FALSE;
        } else if (*s == '.' || *s == '\0') {
            if (digits == 0 || octet > 3)
                return PJ_FALSE;
            addr = (addr << 8) | (pj_uint32_t)value;
            ++octet;
            value = 0;
            digits = 0;
            if (*s == '\0')
                break;
        } else {
            return PJ_FALSE;
        }
    }

    if (octet != 4)
        return PJ_FALSE;

    *out_hbo = addr;
    return PJ_TRUE;
}

PJ_DEF(char *) pj_inet_ntoa(pj_in_addr inaddr)
{
    static char str[PJ_INET_ADDRSTRLEN];
    pj_uint32_t hbo = pj_ntohl(inaddr.s_addr);

    pj_ansi_snprintf(str, sizeof(str), "%u.%u.%u.%u", (unsigned)((hbo >> 24) & 0xFF), (unsigned)((hbo >> 16) & 0xFF),
                     (unsigned)((hbo >> 8) & 0xFF), (unsigned)(hbo & 0xFF));

    return str;
}

PJ_DEF(int) pj_inet_aton(const pj_str_t *cp, pj_in_addr *inp)
{
    char tempaddr[PJ_INET_ADDRSTRLEN];
    pj_uint32_t hbo;

    /* Callers rely on the output being INADDR_NONE on failure. */
    inp->s_addr = PJ_INADDR_NONE;

    PJ_ASSERT_RETURN(cp && cp->slen && inp, 0);

    /* May be called with a hostname just to test whether it is an address. */
    if (cp->slen >= PJ_INET_ADDRSTRLEN)
        return 0;

    pj_memcpy(tempaddr, cp->ptr, cp->slen);
    tempaddr[cp->slen] = '\0';

    if (!tal_parse_ipv4(tempaddr, &hbo))
        return 0;

    inp->s_addr = pj_htonl(hbo);
    return 1;
}

PJ_DEF(pj_status_t) pj_inet_pton(int af, const pj_str_t *src, void *dst)
{
    char tempaddr[PJ_INET6_ADDRSTRLEN];
    pj_uint32_t hbo;

    PJ_ASSERT_RETURN(af == PJ_AF_INET || af == PJ_AF_INET6, PJ_EAFNOTSUP);
    PJ_ASSERT_RETURN(src && src->slen && dst, PJ_EINVAL);

    if (af != PJ_AF_INET)
        return PJ_EIPV6NOTSUP;

    ((pj_in_addr *)dst)->s_addr = PJ_INADDR_NONE;

    if (src->slen >= PJ_INET6_ADDRSTRLEN)
        return PJ_ENAMETOOLONG;

    pj_memcpy(tempaddr, src->ptr, src->slen);
    tempaddr[src->slen] = '\0';

    if (!tal_parse_ipv4(tempaddr, &hbo))
        return PJ_EINVAL;

    ((pj_in_addr *)dst)->s_addr = pj_htonl(hbo);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_inet_ntop(int af, const void *src, char *dst, int size)
{
    pj_uint32_t hbo;

    PJ_ASSERT_RETURN(src && dst && size, PJ_EINVAL);

    *dst = '\0';

    PJ_ASSERT_RETURN(af == PJ_AF_INET || af == PJ_AF_INET6, PJ_EAFNOTSUP);

    if (af != PJ_AF_INET)
        return PJ_EIPV6NOTSUP;

    if (size < PJ_INET_ADDRSTRLEN)
        return PJ_ETOOSMALL;

    hbo = pj_ntohl(((const pj_in_addr *)src)->s_addr);
    pj_ansi_snprintf(dst, size, "%u.%u.%u.%u", (unsigned)((hbo >> 24) & 0xFF), (unsigned)((hbo >> 16) & 0xFF),
                     (unsigned)((hbo >> 8) & 0xFF), (unsigned)(hbo & 0xFF));

    return PJ_SUCCESS;
}

PJ_DEF(const pj_str_t *) pj_gethostname(void)
{
    static char buf[PJ_MAX_HOSTNAME];
    static pj_str_t hostname;

    PJ_CHECK_STACK();

    if (hostname.ptr == NULL) {
        hostname.ptr = buf;
        /*
         * TuyaOS has no POSIX host database; pj only uses this as a label
         * (telnet CLI prompt, pj_gethostip fallback).
         */
        pj_ansi_snprintf(buf, sizeof(buf), "%s", PJ_TUYAOS_HOSTNAME);
        hostname.slen = pj_ansi_strlen(buf);
    }

    return &hostname;
}

/*
 * pj_sockaddr <-> TAL address conversion.
 */
static pj_status_t pj_sockaddr_to_tal(const pj_sockaddr_t *addr, int len, TUYA_IP_ADDR_T *out_addr,
                                      pj_uint16_t *out_port)
{
    const pj_sockaddr *sa = (const pj_sockaddr *)addr;

    PJ_ASSERT_RETURN(addr && len >= (int)sizeof(pj_sockaddr_in), PJ_EINVAL);

    if (sa->addr.sa_family != PJ_AF_INET)
        return PJ_EAFNOTSUP;

    *out_addr = TUYA_IP_ADDR_MAKE_IP4(pj_ntohl(sa->ipv4.sin_addr.s_addr));
    *out_port = pj_ntohs(sa->ipv4.sin_port);

    return PJ_SUCCESS;
}

static void tal_to_pj_sockaddr(TUYA_IP_ADDR_T addr, pj_uint16_t port, pj_sockaddr *out)
{
    pj_bzero(out, sizeof(*out));
    out->addr.sa_family = PJ_AF_INET;
    out->ipv4.sin_addr.s_addr = pj_htonl((pj_uint32_t)TUYA_IP_ADDR_GET_IP4(addr));
    out->ipv4.sin_port = pj_htons(port);
}

/*
 * Copy a filled pj_sockaddr into a caller buffer that may be shorter than
 * pj_sockaddr, honouring the in/out length convention of the pj API.
 */
static pj_status_t store_sockaddr(const pj_sockaddr *src, pj_sockaddr_t *dst, int *dstlen)
{
    int copy_len = (int)sizeof(pj_sockaddr_in);

    PJ_ASSERT_RETURN(dst && dstlen, PJ_EINVAL);

    if (*dstlen < copy_len)
        return PJ_ETOOSMALL;

    pj_memcpy(dst, src, copy_len);
    *dstlen = copy_len;

    return PJ_SUCCESS;
}

/*
 * Socket API.
 */
PJ_DEF(pj_status_t) pj_sock_socket(int af, int type, int proto, pj_sock_t *sock)
{
    TUYA_PROTOCOL_TYPE_E tal_type;
    int fd;

    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sock != NULL, PJ_EINVAL);
    PJ_UNUSED_ARG(proto);

    *sock = PJ_INVALID_SOCKET;

    if (af != PJ_AF_INET)
        return PJ_EAFNOTSUP;

    if (type == PJ_SOCK_DGRAM)
        tal_type = PROTOCOL_UDP;
    else if (type == PJ_SOCK_STREAM)
        tal_type = PROTOCOL_TCP;
    else if (type == PJ_SOCK_RAW)
        tal_type = PROTOCOL_RAW;
    else
        return PJ_EINVAL;

    fd = tal_net_socket_create(tal_type);
    if (fd < 0)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    *sock = (pj_sock_t)fd;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sock_bind(pj_sock_t sock, const pj_sockaddr_t *addr, int len)
{
    TUYA_IP_ADDR_T tal_addr;
    pj_uint16_t port;
    pj_status_t status;

    PJ_CHECK_STACK();

    status = pj_sockaddr_to_tal(addr, len, &tal_addr, &port);
    if (status != PJ_SUCCESS)
        return status;

    if (tal_net_bind((int)sock, tal_addr, port) != 0)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sock_bind_in(pj_sock_t sock, pj_uint32_t addr32, pj_uint16_t port)
{
    pj_sockaddr_in addr;

    PJ_CHECK_STACK();

    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = PJ_AF_INET;
    addr.sin_addr.s_addr = pj_htonl(addr32);
    addr.sin_port = pj_htons(port);

    return pj_sock_bind(sock, &addr, sizeof(pj_sockaddr_in));
}

PJ_DEF(pj_status_t) pj_sock_close(pj_sock_t sock)
{
    PJ_CHECK_STACK();

    if (tal_net_close((int)sock) != 0)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sock_shutdown(pj_sock_t sock, int how)
{
    PJ_CHECK_STACK();

    if (tal_net_shutdown((int)sock, how) != 0)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sock_listen(pj_sock_t sock, int backlog)
{
    PJ_CHECK_STACK();

    if (tal_net_listen((int)sock, backlog) != 0)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sock_accept(pj_sock_t serverfd, pj_sock_t *newsock, pj_sockaddr_t *addr, int *addrlen)
{
    TUYA_IP_ADDR_T tal_addr;
    uint16_t port = 0;
    int fd;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(newsock != NULL, PJ_EINVAL);

    pj_bzero(&tal_addr, sizeof(tal_addr));

    fd = tal_net_accept((int)serverfd, &tal_addr, &port);
    if (fd < 0)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    *newsock = (pj_sock_t)fd;

    if (addr && addrlen) {
        pj_sockaddr sa;

        tal_to_pj_sockaddr(tal_addr, port, &sa);
        return store_sockaddr(&sa, addr, addrlen);
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sock_connect(pj_sock_t sock, const pj_sockaddr_t *addr, int namelen)
{
    TUYA_IP_ADDR_T tal_addr;
    pj_uint16_t port;
    pj_status_t status;

    PJ_CHECK_STACK();

    status = pj_sockaddr_to_tal(addr, namelen, &tal_addr, &port);
    if (status != PJ_SUCCESS)
        return status;

    if (tal_net_connect((int)sock, tal_addr, port) != 0)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sock_getsockname(pj_sock_t sock, pj_sockaddr_t *addr, int *namelen)
{
    TUYA_IP_ADDR_T tal_addr;
    uint16_t port = 0;
    pj_sockaddr sa;
    pj_uint32_t host;

    PJ_CHECK_STACK();

    pj_bzero(&tal_addr, sizeof(tal_addr));

    if (tal_net_getsockname((int)sock, &tal_addr, &port) != OPRT_OK)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    host = (pj_uint32_t)TUYA_IP_ADDR_GET_IP4(tal_addr);

    /*
     * A socket bound to INADDR_ANY reports 0.0.0.0 (lwIP sometimes 127.0.0.1).
     * ICE needs a routable base address for its host candidates, so fall back
     * to the station/wired IPv4. The port stays as reported.
     */
    if (host == 0 || (host >> 24) == 127) {
        unsigned int nbo;

        if (tal_compat_get_sta_ipv4_nbo(&nbo) == 0)
            host = pj_ntohl((pj_uint32_t)nbo);
    }

    tal_to_pj_sockaddr(TUYA_IP_ADDR_MAKE_IP4(host), port, &sa);

    return store_sockaddr(&sa, addr, namelen);
}

PJ_DEF(pj_status_t) pj_sock_getpeername(pj_sock_t sock, pj_sockaddr_t *addr, int *namelen)
{
    TUYA_IP_ADDR_T tal_addr;
    uint16_t port = 0;
    pj_sockaddr sa;

    PJ_CHECK_STACK();

    pj_bzero(&tal_addr, sizeof(tal_addr));

    if (tal_net_getpeername((int)sock, &tal_addr, &port) != OPRT_OK)
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    tal_to_pj_sockaddr(tal_addr, port, &sa);

    return store_sockaddr(&sa, addr, namelen);
}

PJ_DEF(pj_status_t) pj_sock_send(pj_sock_t sock, const void *buf, pj_ssize_t *len, unsigned flags)
{
    int sent;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf && len && *len >= 0, PJ_EINVAL);

    /* TAL has no flags argument; pj only passes ioqueue bookkeeping bits. */
    PJ_UNUSED_ARG(flags);

    sent = tal_net_send((int)sock, buf, (uint32_t)(*len));
    if (sent < 0) {
        *len = -1;
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    }

    *len = sent;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t)
pj_sock_sendto(pj_sock_t sock, const void *buf, pj_ssize_t *len, unsigned flags, const pj_sockaddr_t *to, int tolen)
{
    TUYA_IP_ADDR_T tal_addr;
    pj_uint16_t port;
    pj_status_t status;
    int sent;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf && len && *len >= 0, PJ_EINVAL);
    PJ_UNUSED_ARG(flags);

    status = pj_sockaddr_to_tal(to, tolen, &tal_addr, &port);
    if (status != PJ_SUCCESS)
        return status;

    sent = tal_net_send_to((int)sock, buf, (uint32_t)(*len), tal_addr, port);
    if (sent < 0) {
        *len = -1;
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    }

    *len = sent;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sock_recv(pj_sock_t sock, void *buf, pj_ssize_t *len, unsigned flags)
{
    int recvd;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf && len && *len >= 0, PJ_EINVAL);
    PJ_UNUSED_ARG(flags);

    recvd = tal_net_recv((int)sock, buf, (uint32_t)(*len));
    if (recvd < 0) {
        *len = -1;
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    }

    *len = recvd;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t)
pj_sock_recvfrom(pj_sock_t sock, void *buf, pj_ssize_t *len, unsigned flags, pj_sockaddr_t *from, int *fromlen)
{
    TUYA_IP_ADDR_T tal_addr;
    uint16_t port = 0;
    int recvd;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf && len && *len >= 0, PJ_EINVAL);
    PJ_UNUSED_ARG(flags);

    pj_bzero(&tal_addr, sizeof(tal_addr));

    recvd = tal_net_recvfrom((int)sock, buf, (uint32_t)(*len), &tal_addr, &port);
    if (recvd < 0) {
        *len = -1;
        return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    }

    *len = recvd;

    if (from && fromlen) {
        pj_sockaddr sa;

        tal_to_pj_sockaddr(tal_addr, port, &sa);
        return store_sockaddr(&sa, from, fromlen);
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t)
pj_sock_setsockopt(pj_sock_t sock, pj_uint16_t level, pj_uint16_t optname, const void *optval, int optlen)
{
    int val = 0;
    OPERATE_RET rt = OPRT_NOT_SUPPORTED;

    PJ_CHECK_STACK();

    if (optval && optlen >= (int)sizeof(int))
        val = *(const int *)optval;

    /* The PJ_SO_* constants are runtime consts, so this is an if ladder. */
    if (level == PJ_SOL_SOCKET) {
        if (optname == PJ_SO_RCVBUF) {
            rt = tal_net_set_bufsize((int)sock, val, TRANS_RECV);
        } else if (optname == PJ_SO_SNDBUF) {
            rt = tal_net_set_bufsize((int)sock, val, TRANS_SEND);
        } else if (optname == PJ_SO_REUSEADDR) {
            rt = val ? tal_net_set_reuse((int)sock) : OPRT_OK;
        } else if (optname == PJ_SO_NOSIGPIPE) {
            /* TuyaOS never raises SIGPIPE, so this is a no-op. */
            rt = OPRT_OK;
        }
    } else if (level == PJ_SOL_TCP && optname == PJ_TCP_NODELAY) {
        rt = val ? tal_net_disable_nagle((int)sock) : OPRT_OK;
    }

    if (rt == OPRT_NOT_SUPPORTED) {
        /*
         * QoS/TOS and multicast are not exposed by TAL. pjlib treats a failed
         * socket option as non-fatal, so report it instead of faking success.
         */
        PJ_LOG(5, (THIS_FILE, "setsockopt level=%u opt=%u not supported by TAL", level, optname));
        return PJ_ENOTSUP;
    }

    return (rt == OPRT_OK) ? PJ_SUCCESS : PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
}

PJ_DEF(pj_status_t)
pj_sock_getsockopt(pj_sock_t sock, pj_uint16_t level, pj_uint16_t optname, void *optval, int *optlen)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(sock);

    PJ_ASSERT_RETURN(optval && optlen, PJ_EINVAL);

    /*
     * TAL exposes setters only. The one reader pjlib needs (SO_ERROR for
     * connect completion) is avoided by building with PJ_HAS_SO_ERROR 0,
     * which selects the portable pj_sock_getpeername() probe instead.
     */
    PJ_LOG(5, (THIS_FILE, "getsockopt level=%u opt=%u not supported by TAL", level, optname));

    return PJ_ENOTSUP;
}

PJ_DEF(pj_status_t) pj_sock_setsockopt_params(pj_sock_t sockfd, const pj_sockopt_params *params)
{
    unsigned int i = 0;
    pj_status_t retval = PJ_SUCCESS;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(params, PJ_EINVAL);

    for (i = 0; i < params->cnt && i < PJ_MAX_SOCKOPT_PARAMS; ++i) {
        pj_status_t status = pj_sock_setsockopt(sockfd, (pj_uint16_t)params->options[i].level,
                                                (pj_uint16_t)params->options[i].optname,
                                                params->options[i].optval, params->options[i].optlen);
        if (status != PJ_SUCCESS) {
            retval = status;
            PJ_PERROR(4, (THIS_FILE, status, "Warning: error applying sock opt %d", i));
        }
    }

    return retval;
}
