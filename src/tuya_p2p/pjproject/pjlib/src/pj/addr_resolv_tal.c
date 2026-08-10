/**
 * @file addr_resolv_tal.c
 * @brief Host resolution on TuyaOpen TAL — replaces addr_resolv_sock.c
 *
 * TAL exposes a single IPv4 resolver (tal_net_gethostbyname), which is what
 * the TKL/lwIP and POSIX backends both implement. That keeps pjlib free of
 * <netdb.h> and of the lwIP DNS headers.
 *
 * @copyright Copyright (c) Tuya Inc.
 */
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ip_helper.h>
#include <pj/log.h>
#include <pj/string.h>

#include "tal_api.h"
#include "tal_network.h"
#include "tal_ip_addr.h"

#define THIS_FILE "addr_resolv_tal.c"

/* Storage backing the pj_hostent returned by pj_gethostbyname(). */
static char g_resolved_name[PJ_MAX_HOSTNAME];
static pj_in_addr g_resolved_addr;
static char *g_resolved_addr_list[2];
static char *g_resolved_aliases[1];

/**
 * @brief Resolve a host name to a single IPv4 address
 *
 * @param[in] name host name, may already be a dotted-quad literal
 * @param[out] he filled with one address on success
 *
 * @note The result points into static storage, matching what the C library
 * gethostbyname() contract allows and what upstream pjlib relies on.
 *
 * @return PJ_SUCCESS on success, PJ_ERESOLVE on failure
 */
PJ_DEF(pj_status_t) pj_gethostbyname(const pj_str_t *hostname, pj_hostent *phe)
{
    char name[PJ_MAX_HOSTNAME];
    TUYA_IP_ADDR_T addr;

    PJ_ASSERT_RETURN(hostname && hostname->slen && phe, PJ_EINVAL);

    if (hostname->slen >= (pj_ssize_t)sizeof(name))
        return PJ_ENAMETOOLONG;

    pj_memcpy(name, hostname->ptr, hostname->slen);
    name[hostname->slen] = '\0';

    pj_bzero(&addr, sizeof(addr));
    if (tal_net_gethostbyname(name, &addr) != OPRT_OK) {
        PJ_LOG(4, (THIS_FILE, "unable to resolve %s", name));
        return PJ_ERESOLVE;
    }

    pj_ansi_snprintf(g_resolved_name, sizeof(g_resolved_name), "%s", name);
    g_resolved_addr.s_addr = pj_htonl((pj_uint32_t)TUYA_IP_ADDR_GET_IP4(addr));

    g_resolved_addr_list[0] = (char *)&g_resolved_addr;
    g_resolved_addr_list[1] = NULL;
    g_resolved_aliases[0] = NULL;

    phe->h_name = g_resolved_name;
    phe->h_aliases = g_resolved_aliases;
    phe->h_addrtype = PJ_AF_INET;
    phe->h_length = sizeof(pj_in_addr);
    phe->h_addr_list = g_resolved_addr_list;

    return PJ_SUCCESS;
}

/**
 * @brief Resolve a host name into a list of socket addresses
 *
 * @param[in] af address family, only PJ_AF_INET is supported
 * @param[in] nodename host name to resolve
 * @param[in,out] count in: array capacity, out: entries filled
 * @param[out] ai resolved addresses
 *
 * @note TAL resolves to a single IPv4 address, so at most one entry is
 * returned even when the caller offers a larger array.
 *
 * @return PJ_SUCCESS on success, otherwise an error
 */
PJ_DEF(pj_status_t) pj_getaddrinfo(int af, const pj_str_t *nodename, unsigned *count, pj_addrinfo ai[])
{
    pj_hostent he;
    pj_status_t status;

    PJ_ASSERT_RETURN(nodename && count && *count && ai, PJ_EINVAL);
    PJ_ASSERT_RETURN(af == PJ_AF_INET || af == PJ_AF_INET6 || af == PJ_AF_UNSPEC, PJ_EAFNOTSUP);

    if (af == PJ_AF_INET6) {
        *count = 0;
        return PJ_EIPV6NOTSUP;
    }

    status = pj_gethostbyname(nodename, &he);
    if (status != PJ_SUCCESS) {
        *count = 0;
        return status;
    }

    pj_bzero(&ai[0], sizeof(ai[0]));
    pj_ansi_snprintf(ai[0].ai_canonname, sizeof(ai[0].ai_canonname), "%s", he.h_name);
    ai[0].ai_addr.addr.sa_family = PJ_AF_INET;
    pj_memcpy(&ai[0].ai_addr.ipv4.sin_addr, he.h_addr_list[0], sizeof(pj_in_addr));

    *count = 1;

    return PJ_SUCCESS;
}
