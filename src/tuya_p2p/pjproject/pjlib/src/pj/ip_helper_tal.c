/**
 * @file ip_helper_tal.c
 * @brief Local IP enumeration on TuyaOpen TAL — replaces ip_helper_generic.c
 *
 * ip_helper_generic.c walked getifaddrs()/SIOCGIFCONF or netlink, none of
 * which exist on an RTOS. TuyaOpen already knows the active station or wired
 * address, so a single interface is reported and pjlib keeps no dependency
 * on <net/if.h> or the chip network stack.
 *
 * @copyright Copyright (c) Tuya Inc.
 */
#include <pj/ip_helper.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/string.h>

#include "tal_local_ip.h"

#define THIS_FILE "ip_helper_tal.c"

/**
 * @brief Enumerate local IP interfaces
 *
 * @param[in] af address family, PJ_AF_INET or PJ_AF_UNSPEC
 * @param[in,out] count in: array capacity, out: entries filled
 * @param[out] ifs filled with the local interface addresses
 *
 * @note TuyaOpen exposes one routable IPv4 per device (station or wired),
 * so at most one entry is reported.
 *
 * @return PJ_SUCCESS on success, PJ_ENOTFOUND when no address is up
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af, unsigned *count, pj_sockaddr ifs[])
{
    unsigned int nbo = 0;

    PJ_ASSERT_RETURN(count && *count > 0 && ifs, PJ_EINVAL);

    if (af == PJ_AF_INET6) {
        *count = 0;
        return PJ_EIPV6NOTSUP;
    }

    if (tal_compat_get_sta_ipv4_nbo(&nbo) != 0 || nbo == 0) {
        *count = 0;
        return PJ_ENOTFOUND;
    }

    pj_bzero(&ifs[0], sizeof(ifs[0]));
    ifs[0].addr.sa_family = PJ_AF_INET;
    ifs[0].ipv4.sin_addr.s_addr = (pj_uint32_t)nbo;

    *count = 1;

    return PJ_SUCCESS;
}

/**
 * @brief Enumerate local IP interfaces with options
 *
 * @param[in] opt enumeration options, NULL for defaults
 * @param[in,out] count in: array capacity, out: entries filled
 * @param[out] ifs filled with the local interface addresses
 *
 * @note The IPv6 deprecation filter has no meaning here; it is ignored
 * because only IPv4 is reported.
 *
 * @return PJ_SUCCESS on success, otherwise an error
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface2(const pj_enum_ip_option *opt, unsigned *count, pj_sockaddr ifs[])
{
    int af = PJ_AF_UNSPEC;

    if (opt)
        af = opt->af;

    return pj_enum_ip_interface(af, count, ifs);
}

/**
 * @brief Enumerate the IP routing table
 *
 * @param[in,out] count set to 0 on return
 * @param[out] routes unused
 *
 * @note TuyaOpen exposes no routing table; pjlib only uses this to pick a
 * source address and falls back to pj_enum_ip_interface() when it fails.
 *
 * @return PJ_ENOTSUP
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *count, pj_ip_route_entry routes[])
{
    PJ_ASSERT_RETURN(count && routes, PJ_EINVAL);

    PJ_UNUSED_ARG(routes);

    *count = 0;

    return PJ_ENOTSUP;
}
