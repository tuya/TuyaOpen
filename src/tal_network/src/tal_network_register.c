/**
 * @file tal_network_register.c
 * @brief Registry of socket ops backends, plus the route the data plane follows.
 *
 * Holds the one copy of "which backend and which source address are in force"
 * (see tal_net_route.h) and the table of backends a build has available.
 *
 * See "Locking discipline" below before adding or removing a lock here: the lock
 * protects the consistency of a pair of fields, not the visibility of any single
 * one of them.
 *
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_mutex.h"

#include "tal_network_register.h"
#include "tal_net_route.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    /* The active route. Its src_ip is kept here rather than in
     * tal_net_provider_t.ipaddr because several connections can share one
     * provider type (on T5AI both wifi and cellular are TKL), so it is a
     * property of the active link, not of the provider. Updates that move both
     * fields at once go through s_route_lock; reads of a single field do not
     * need it. */
    tal_net_route_t route;
    /* Available backends, indexed by TAL_NET_PROVIDER_*. Written once by the
     * static initializer below and never again, so readers need no lock. */
    tal_net_provider_t *providers[TAL_NET_PROVIDER_MAX];
} tal_net_provider_registry_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
/* The one backend this build links, keyed off TAL_NET_PROVIDER_DEFAULT so the
 * ENABLE_LIBLWIP/OPERATING_SYSTEM test itself lives in exactly one place. */
#if TAL_NET_PROVIDER_DEFAULT == TAL_NET_PROVIDER_POSIX
extern tal_net_provider_t tal_net_provider_posix;
#define TAL_NET_PROVIDER_DEFAULT_OBJ tal_net_provider_posix
#else
extern tal_net_provider_t tal_net_provider_tkl;
#define TAL_NET_PROVIDER_DEFAULT_OBJ tal_net_provider_tkl
#endif

/* Statically initialized, not filled in by tal_net_provider_init(): early socket
 * users can reach tal_net_provider_ops() before init runs, and they must
 * find a working backend there. */
tal_net_provider_registry_t tal_net_provider_registry = {
    .route                               = {.provider = TAL_NET_PROVIDER_DEFAULT, .src_ip = 0},
    .providers[TAL_NET_PROVIDER_DEFAULT] = &TAL_NET_PROVIDER_DEFAULT_OBJ,
};

/*
 * Locking discipline
 * ------------------
 * s_route_lock exists to keep the PAIR (provider, src_ip) consistent. It is not
 * there to make either field individually visible, and it is not needed for that.
 * Hence:
 *
 *   - tal_net_route_set() and tal_net_route_get() take it. Moving or reading both
 *     fields as one unit is their entire contract: a link switch must never be
 *     observed as the new backend paired with the address of the link that just
 *     went away.
 *   - The single-field readers do NOT take it. Each returns one naturally atomic
 *     word that cannot tear, and none of them looks at the other field, so the
 *     lock would buy nothing at all. tal_net_provider_ops() matters most
 *     here: TAL_NET_EXEC_OP in tal_network.c calls it from every socket
 *     primitive - send, recv, recvfrom, select, fd_isset and some thirty more,
 *     several of them from inside tight select loops. A mutex per call would put
 *     a kernel-level operation under every socket operation and, on an RTOS, add
 *     a fresh source of priority inversion. That path was a plain array read
 *     before this state was consolidated and it stays one.
 *
 * So do not "fix" the readers below by taking the lock in them: that reintroduces
 * the hot-path cost without making anything more correct. A caller that genuinely
 * needs the two fields to agree with each other should call tal_net_route_get(),
 * which is exactly what it is for.
 *
 * The handle stays NULL until tal_net_provider_init() arms it. A mutex cannot be
 * created at static initialization time, and the route must stay usable before
 * init, so the writers fall back to unguarded access while the handle is NULL -
 * which is what this code did before the lock existed. Nothing is lost by it:
 * only the single-threaded startup path runs that early, and the writer (netmgr)
 * does not exist until well after init.
 */
static MUTEX_HANDLE s_route_lock = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __route_lock(void)
{
    if (NULL != s_route_lock) {
        tal_mutex_lock(s_route_lock);
    }
}

static void __route_unlock(void)
{
    if (NULL != s_route_lock) {
        tal_mutex_unlock(s_route_lock);
    }
}

OPERATE_RET tal_net_provider_init(void)
{
    /* The backend table and the default route come from the static initializer
     * above, so there is nothing to publish here. What is left is arming the
     * route lock, which cannot be done statically. */
    if (NULL != s_route_lock) {
        return OPRT_OK;
    }

    return tal_mutex_create_init(&s_route_lock);
}

OPERATE_RET tal_net_route_set(const tal_net_route_t *route)
{
    if (NULL == route) {
        return OPRT_INVALID_PARM;
    }

    if (route->provider >= TAL_NET_PROVIDER_MAX) {
        return OPRT_INVALID_PARM;
    }

    /* In range is not the same as backed by anything. providers[] is statically
     * initialised with exactly one non-NULL entry and is immutable afterwards, so
     * publishing any other provider leaves tal_net_provider_ops() returning
     * NULL and every socket primitive in tal_network.c failing - with no symptom
     * that points here.
     *
     * Refused rather than logged, because this translation unit has no log
     * dependency and should not grow one for a caller error. The distinct return
     * code is what lets the caller say something useful; netmgr does. */
    if (NULL == tal_net_provider_registry.providers[route->provider]) {
        return OPRT_NOT_SUPPORTED;
    }

    __route_lock();
    tal_net_provider_registry.route = *route;
    __route_unlock();

    return OPRT_OK;
}

OPERATE_RET tal_net_route_get(tal_net_route_t *route)
{
    if (NULL == route) {
        return OPRT_INVALID_PARM;
    }

    __route_lock();
    *route = tal_net_provider_registry.route;
    __route_unlock();

    return OPRT_OK;
}

TUYA_IP_ADDR_T tal_net_route_src_ip(void)
{
    /* One word, read unlocked on purpose - see the locking discipline above. A
     * caller that needs this address to agree with the provider it belongs to
     * must use tal_net_route_get() instead. */
    return tal_net_provider_registry.route.src_ip;
}

TAL_NETWORK_OPS_T *tal_net_provider_ops(void)
{
    /* The hot path: every socket primitive in tal_network.c lands here. Both
     * reads below are unlocked by design - provider is a single byte that cannot
     * tear, providers[] is immutable after static initialization, and neither
     * has to agree with src_ip. */
    uint8_t provider = tal_net_provider_registry.route.provider;

    /* Every writer validates provider against TAL_NET_PROVIDER_MAX, so it always
     * indexes in range. */
    tal_net_provider_t *entry = tal_net_provider_registry.providers[provider];
    if (NULL == entry) {
        return NULL;
    }

    return &entry->ops;
}
