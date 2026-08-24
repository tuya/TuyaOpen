/**
 * @file netmgr.c
 * @brief Network manager implementation for managing network connections on
 * Tuya devices.
 *
 * This file contains the implementation of the network manager, which is
 * responsible for managing the network connections of Tuya devices. It supports
 * multiple network interfaces including WiFi, wired Ethernet, and Bluetooth.
 * The network manager initializes the network modules, manages network
 * connection states, and switches between different network types based on
 * availability and user configuration.
 *
 * netmgr no longer knows which technologies exist. Every link this build has is
 * one netconn_desc_t row in the registry (netconn_table.c), and netmgr_init()
 * registers whichever rows the caller's type mask selects. That is why there is
 * no `#ifdef ENABLE_<TECH>` left below: adding a link type is a table row plus a
 * driver, and this file does not change.
 *
 * The network manager plays a crucial role in ensuring that Tuya devices can
 * maintain a stable and reliable connection to the Tuya cloud services,
 * facilitating device control and data exchange.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     Refactored network manager to support management of multiple network connection types
 *
 */

#include "netmgr.h"
#include "netconn_registry.h"
#include "netmgr_priv.h"
#include "tal_api.h"
#include "tuya_slist.h"
#include "tuya_cloud_com_defs.h"
#include "tuya_error_code.h"
#include "tuya_lan.h"

/* The data plane, included here and not from netmgr.h: the control plane's public
 * header must not depend on it. tal_net_route.h is the one channel netmgr uses to
 * write the data plane; tal_network_register.h is here for tal_network_card_init()
 * and for TAL_NET_PROVIDER_DEFAULT. */
#include "tal_network_register.h"
#include "tal_net_route.h"

#ifdef ENABLE_BLUETOOTH
#include "ble_mgr.h"
#endif

/* Upper bound on links netmgr will register. netmgr_type_e is a bitmask and
 * NETCONN_AUTO already claims bit 0, so eight concurrent links is far past
 * anything a board has; the point of the bound is that the notify slots and the
 * teardown order can then live in statically sized storage, which is what makes
 * the cancel-by-callback in netmgr_deinit() safe (see the notify channel note
 * below). */
#define NETMGR_LINK_MAX 8

/* Bounded drain in netmgr_deinit(). 2s is generous for a handler whose slowest
 * step is one conn->get(NETCONN_CMD_IP) modem exchange. */
#define NETMGR_DRAIN_TIMEOUT_MS 2000
#define NETMGR_DRAIN_POLL_MS    10

/**
 * @brief One link's pending report, in registration order.
 *
 * Static storage on purpose - see __netmgr_notify_work().
 */
typedef struct {
    netmgr_type_e   type;    // which link this slot belongs to
    netmgr_status_e status;  // last reported status, advisory only
    BOOL_T          pending; // set by netmgr_notify_link(), cleared by the handler
} netmgr_report_t;

typedef struct {
    MUTEX_HANDLE lock; // mutex
    BOOL_T       inited;
    BOOL_T       stopping;  // netmgr_deinit() in progress: refuse new work
    BOOL_T       ble_owned; // this netmgr_init() is what brought the BLE stack up

    netmgr_type_e   type;   // network manage type
    netmgr_type_e   active; // the connect now used
    netmgr_status_e status; // the network status

    netmgr_conn_base_t *conn; // connections, sorted by descending priority

    uint32_t        link_num;                // registered links, and valid entries in report[]
    netmgr_report_t report[NETMGR_LINK_MAX]; // pending reports, in registration order

    BOOL_T   notify_queued; // a notify work item is queued and has not started
    uint32_t notify_busy;   // notify handlers currently running
} netmgr_t;

static netmgr_t s_netmgr = {0};

/* Locking contract for s_netmgr
 * =============================
 * s_netmgr.conn / .active / .status are reached from the notify handler, from
 * the tal_sw_timer thread, from netmgr_deinit() and from any caller of the
 * public netmgr_conn_get()/netmgr_conn_set(), so all of them are accessed under
 * s_netmgr.lock. The invariant that shapes every function below:
 *
 *     The lock protects field access on s_netmgr and nothing else. No driver
 *     callback - conn->open(), conn->close(), conn->get(), conn->set() - and no
 *     tal_event_publish() runs while the lock is held.
 *
 * Both halves of that matter:
 *
 * - Latency. A driver callback can block for a long time: netconn_cellular_get()
 *   servicing NETCONN_CMD_IP goes to tal_cellular_get_ip() and on into a modem
 *   AT exchange. Holding the lock across it would park every other caller on the
 *   same mutex for the duration.
 *
 * - Deadlock. Several of these callbacks re-enter netmgr synchronously:
 *   netconn_{wifi,wired,cellular}_set() fire base.event_cb() inline for
 *   NETCONN_CMD_PRI, the LINUX tkl_wired_set_status_cb() fires the status
 *   callback before it returns, and tuya_iot's __tuya_iot_link_type_change_cb()
 *   calls tuya_iot_reconnect(). s_netmgr.lock is not portably recursive:
 *   tkl_mutex_create_init() only maps to a recursive primitive where the port
 *   asks for one (the FreeRTOS ports gate it on configUSE_RECURSIVE_MUTEXES; the
 *   LINUX port always sets PTHREAD_MUTEX_RECURSIVE), so any of those would be a
 *   hard self-deadlock.
 *
 * The resulting shape: take the lock, resolve and snapshot what you need into
 * locals, drop the lock, then call outward. Keeping a netmgr_conn_base_t *
 * across the unlock is safe - the conn nodes are static globals that are never
 * freed, and netmgr_deinit() leaves them intact for the next netmgr_init().
 *
 * M2 removed the sharpest edge here: base.event_cb no longer runs the state
 * machine on the reporting thread, it only marks a slot and posts one work item
 * (see the notify channel below), so the whole machine runs on WORKQ_SYSTEM and
 * cannot re-enter netmgr from a driver at all. The rule above is kept anyway,
 * because the tal_sw_timer thread and public API callers still race the handler.
 *
 * One bounded carve-out: __get_active_conn() and __get_netmgr_status() call
 * conn->get(NETCONN_CMD_STATUS) while walking the list, which cannot be hoisted
 * out of the lock without snapshotting the whole list. All three drivers answer
 * that one command from their cached base.status with no TKL call, so it stays
 * bounded. A port that ever makes NETCONN_CMD_STATUS blocking breaks this.
 */

static TIMER_ID sg_lan_init_timer = NULL;

/* The lock-free gate on s_netmgr, and the only netmgr state read without the
 * mutex held.
 *
 * It exists because two entry points can be reached by threads netmgr cannot
 * account for, and neither can be made to hold the mutex before it decides
 * whether the mutex is still there:
 *
 *   - the report shim. Drivers read base.event_cb without any lock
 *     ("if (wifi->base.event_cb) wifi->base.event_cb(...)"), so a vendor task
 *     can read a non-NULL pointer, be preempted, and call in later. Setting
 *     conn->event_cb = NULL in netmgr_deinit() does not close that window, and
 *     the callback cannot be withdrawn at the TAL either: tal_wifi.h has no
 *     uninit and no way to retract the WIFI_EVENT_CB that tal_wifi_init()
 *     installed, and tal_wired_set_status_cb() does not accept a NULL.
 *   - the LAN timer callback, which runs on the tal_sw_timer thread and cannot
 *     be joined by tal_sw_timer_delete().
 *
 * Contract: set TRUE before netmgr_deinit() touches anything, and cleared only
 * by netmgr_init() once s_netmgr is fully seeded. TRUE means "do not read
 * s_netmgr, do not take the lock, return". Its initial value is TRUE so a
 * callback that arrives before the first netmgr_init() is dropped too.
 *
 * A caller can still read the gate as FALSE, be preempted, and reach
 * tal_mutex_lock() after netmgr_deinit() has finished - closing THAT would need
 * an atomic in-flight count the TAL does not offer. It is harmless, because
 * netmgr_deinit() never releases the mutex (see the note on it): the straggler
 * blocks on a live mutex, finds `stopping`, and returns.
 *
 * So the gate's job is the other failure mode, and there it is decisive: a late
 * report must not leave `pending` set for the next netmgr_init() to act on. That
 * is belt and braces with a structural guarantee - __netmgr_report_slot() only
 * hands out a slot while link_num > 0, and netmgr_deinit() zeroes link_num - so
 * it takes both to go wrong before any state leaks across a cycle.
 */
static volatile BOOL_T sg_netmgr_gate_closed = TRUE;

static void __netmgr_notify_work(void *data);

/**
 * @brief Log name for a link type, taken from its descriptor.
 *
 * Every netmgr-internal log line goes through here rather than through
 * NETMGR_TYPE_TO_STR(), so a new link type is named by the table row that
 * introduces it. The macro stays in netmgr.h because 44 files include it.
 *
 * @return never NULL, so it is safe as a "%s" argument.
 */
static const char *__netmgr_link_name(netmgr_type_e type)
{
    const netconn_desc_t *desc = netconn_registry_find(type);

    if (NULL != desc) {
        return desc->name;
    }

    // NETCONN_AUTO has no descriptor by definition; anything else here is a type
    // this build has no driver for.
    return (NETCONN_AUTO == type) ? "auto" : "unregistered";
}

/**
 * @brief get active connection status and
 *
 * @note Caller must hold s_netmgr.lock: this walks the connection list.
 *
 * @return netconn_type_t: the connection should be used
 */
static netmgr_type_e __get_active_conn()
{
    netmgr_type_e       active_type = NETCONN_AUTO;
    netmgr_conn_base_t *cur_conn    = s_netmgr.conn;

    if (NULL == cur_conn) {
        PR_ERR("no connection registered");
        return NETCONN_AUTO;
    }

    netmgr_status_e netmgr_status = NETMGR_LINK_DOWN;

    active_type = cur_conn->type;

    while (cur_conn) {
        netmgr_status = NETMGR_LINK_DOWN;
        cur_conn->get(NETCONN_CMD_STATUS, &netmgr_status);
        if (netmgr_status == NETMGR_LINK_UP) {
            // return the first connection which is up
            PR_TRACE("netmgr active connection [%s]", __netmgr_link_name(cur_conn->type));
            active_type = cur_conn->type;
            break;
        }
        cur_conn = cur_conn->next;
    }

    return active_type;
}

void __tuya_lan_init_tm_cb(TIMER_ID timer_id, void *arg)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_type_e   type   = NETCONN_AUTO;
    MUTEX_HANDLE    lock   = s_netmgr.lock;

    // netmgr_deinit() stops this timer without being able to join a callback that
    // is already inside it, so check the gate and snapshot the handle once.
    if (sg_netmgr_gate_closed || NULL == lock) {
        return;
    }

    // Snapshot under the lock, then act outside it: tuya_lan_init() is heavy and
    // has no business running with the netmgr state locked.
    tal_mutex_lock(lock);
    status = s_netmgr.status;
    type   = (netmgr_type_e)s_netmgr.type;
    tal_mutex_unlock(lock);

    if (status != NETMGR_LINK_UP) {
        return;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();
    if (client == NULL) {
        return;
    }

    if ((type & NETCONN_WIRED || type & NETCONN_WIFI) && client->is_activated) {
        PR_DEBUG("Start LAN initialization");
        tuya_lan_init(client);
        tal_sw_timer_stop(sg_lan_init_timer);
    }

    return;
}

/**
 * @brief Find a registered connection by type.
 *
 * @note Caller must hold s_netmgr.lock: this walks the connection list.
 *
 * @return NULL when @a type is NETCONN_AUTO or nothing matching is registered.
 */
static netmgr_conn_base_t *__get_conn_by_type(netmgr_type_e type)
{
    netmgr_conn_base_t *cur_conn = s_netmgr.conn;

    if (NETCONN_AUTO == type) {
        PR_ERR("type is NETCONN_AUTO");
        return NULL;
    }

    while (cur_conn) {
        if (cur_conn->type == type) {
            return cur_conn;
        }
        cur_conn = cur_conn->next;
    }

    PR_ERR("[%s] not found", __netmgr_link_name(type));
    return NULL;
}

/**
 * @brief Read the link status of one registered connection.
 *
 * @note Caller must hold s_netmgr.lock.
 */
static OPERATE_RET __get_netmgr_status(netmgr_type_e type, netmgr_status_e *status)
{
    OPERATE_RET         rt       = OPRT_OK;
    netmgr_conn_base_t *cur_conn = NULL;

    if (NULL == status) {
        PR_ERR("netmgr get status failed, status is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NETCONN_AUTO == type) {
        PR_ERR("netmgr get status failed, type is NETCONN_AUTO");
        return OPRT_INVALID_PARM;
    }

    *status = NETMGR_LINK_DOWN;

    if (!(s_netmgr.type & type)) {
        PR_ERR("netmgr type [%s] not supported", __netmgr_link_name(type));
        return OPRT_NOT_SUPPORTED;
    }

    // A type nobody registered is not "link down": reporting OPRT_OK here would
    // hand the caller the default above and hide the misconfiguration.
    cur_conn = __get_conn_by_type(type);
    if (NULL == cur_conn) {
        PR_ERR("netmgr get status failed, conn [%s] not registered", __netmgr_link_name(type));
        return OPRT_NOT_FOUND;
    }

    // get the connection status
    if (NULL == cur_conn->get) {
        PR_ERR("netmgr conn [%s] get status failed", __netmgr_link_name(type));
        return OPRT_INVALID_PARM;
    }

    cur_conn->get(NETCONN_CMD_STATUS, status);
    PR_TRACE("netmgr conn [%s] status [%s]", __netmgr_link_name(type), NETMGR_STATUS_TO_STR(*status));

    return rt;
}

/* Pushing the active route down to the data plane
 * ===============================================
 * The route is one value - which socket backend, plus the source address
 * outbound sockets bind to - and it goes down in one tal_net_route_set(). It
 * used to be two independent pushes, the backend from inside the lock and the
 * address from outside it, which left a window where the data plane already ran
 * the new backend while still bound to the address of the old one.
 *
 * M2 makes the notify handler the single writer of that value. Everything that
 * can move the route - a link event, a NETCONN_CMD_PRI change, a
 * NETCONN_CMD_IP set - reaches it through netmgr_notify_link(), so two
 * concurrent sources can no longer race over which consistent pair lands last.
 * netmgr_init() pushes once itself, before any handler can have run.
 *
 * The two halves are read from different places under different rules, so both
 * call sites follow the same three steps:
 *
 *   1. before taking the lock: tal_net_route_get() for the route currently
 *      installed, so a type that resolves to nothing keeps the backend it has;
 *   2. under the lock: __netmgr_snap_provider(), which walks s_netmgr.conn and
 *      hands back the connection it resolved;
 *   3. after dropping the lock: __netmgr_push_route(), which reads the address
 *      via conn->get(NETCONN_CMD_IP) - a blocking modem exchange on cellular,
 *      so it must not run under the lock - and installs both halves at once.
 */

/**
 * @brief Snapshot which socket backend the connection behind @a type uses.
 *
 * Only the provider half is resolved here; see the note above on why the source
 * address cannot be read at this point. The resolved connection is returned so
 * the caller can hand it to __netmgr_push_route() without a second lookup.
 *
 * @note Caller must hold s_netmgr.lock: this walks the connection list.
 *
 * @param[in]     type  the connection to resolve
 * @param[in,out] route provider is overwritten when @a type resolves and left
 *                      untouched otherwise, so seed it before calling
 *
 * @return the resolved connection, or NULL
 */
static netmgr_conn_base_t *__netmgr_snap_provider(netmgr_type_e type, tal_net_route_t *route)
{
    netmgr_conn_base_t *p_conn = __get_conn_by_type(type);

    // __get_conn_by_type() returns NULL for NETCONN_AUTO and for a type nothing
    // registered. Nothing to retarget then - keep the backend we had rather than
    // dereference NULL. The caller still pushes the route, which clears the
    // source address, and still publishes its event.
    if (NULL == p_conn) {
        PR_ERR("netmgr conn [%s] not found, active provider left unchanged", __netmgr_link_name(type));
        return NULL;
    }

    route->provider = p_conn->card_type;

    return p_conn;
}

/**
 * @brief Install the active route on the data plane, both halves in one call.
 *
 * Outbound sockets bind to route->src_ip so traffic leaves the interface netmgr
 * picked. Pushing it here means it tracks every link event - including a cellular
 * redial or DHCP renew that hands out a different address - whereas caching it at
 * first use would pin the first address seen for the life of the transport.
 *
 * The address is read through @a conn directly rather than through the public
 * netmgr_conn_get(). That is not a shortcut: the notify handler runs on
 * WORKQ_SYSTEM and can reach this before netmgr_init() has set s_netmgr.inited,
 * where netmgr_conn_get() answers OPRT_RESOURCE_NOT_READY and would silently
 * pin src_ip at 0. The conn pointer was resolved under the lock and the nodes
 * are static, so using it here is safe.
 *
 * @note Must be called with s_netmgr.lock released: it goes out to
 *       conn->get(NETCONN_CMD_IP), which on cellular is a blocking modem
 *       exchange. @a conn and @a status are snapshots and the body touches no
 *       s_netmgr field of its own, so it needs no lock.
 *
 * @param[in]     conn   the active connection, whose address is read here; NULL
 *                       is allowed and means "no address"
 * @param[in]     status its link status; only a link that is up is asked for one
 * @param[in,out] route  provider snapshotted under the lock; src_ip filled here
 */
static void __netmgr_push_route(netmgr_conn_base_t *conn, netmgr_status_e status, tal_net_route_t *route)
{
    NW_IP_S nw_ip = {0};

    // 0 means "do not bind": better an unbound socket than one pinned to an
    // address the link no longer owns.
    route->src_ip = 0;

    if (NULL != conn && NULL != conn->get && NETMGR_LINK_DOWN != status &&
        OPRT_OK == conn->get(NETCONN_CMD_IP, &nw_ip) && nw_ip.ip[0] != '\0') {
        route->src_ip = tal_net_str2addr(nw_ip.ip);
    }

    tal_net_route_set(route);
}

/**
 * @brief Recompute the active link, push the route, publish what changed.
 *
 * The whole netmgr state machine, and the only place that writes s_netmgr.active
 * / .status once init is done. Runs on WORKQ_SYSTEM only, from
 * __netmgr_notify_work(), so it is serialised with itself by construction.
 *
 * @note Must be called with s_netmgr.lock released.
 *
 * @param[in] lock the caller's snapshot of s_netmgr.lock. Taken as a parameter
 *                 rather than re-read here: the caller has already established
 *                 that the handle is live (it raised notify_busy under it), and
 *                 re-reading a field netmgr_deinit() nulls would reintroduce a
 *                 window this function does not need to have.
 */
static void __netmgr_reselect(MUTEX_HANDLE lock)
{
    BOOL_T              type_chg    = FALSE;
    BOOL_T              status_chg  = FALSE;
    netmgr_type_e       pub_active  = NETCONN_AUTO;
    netmgr_status_e     pub_status  = NETMGR_LINK_DOWN;
    netmgr_conn_base_t *active_base = NULL;
    tal_net_route_t     route       = {.provider = TAL_NET_PROVIDER_DEFAULT, .src_ip = 0};

    // Step 1 of the route push described above: read what is installed before
    // taking the lock. The initialiser only covers a route_get() that fails.
    tal_net_route_get(&route);

    tal_mutex_lock(lock);

    netmgr_status_e active_status = NETMGR_LINK_DOWN;
    netmgr_type_e   active_conn   = __get_active_conn();
    __get_netmgr_status(active_conn, &active_status);

    // both changed
    if (active_status != s_netmgr.status && active_conn != s_netmgr.active) {
        PR_DEBUG("netmgr conn type changed [%s] --> [%s], status changed %d --> %d",
                 __netmgr_link_name(s_netmgr.active), __netmgr_link_name(active_conn), s_netmgr.status, active_status);
        s_netmgr.status = active_status;
        s_netmgr.active = active_conn;
        type_chg        = TRUE;
        status_chg      = TRUE;
    } else if (active_status != s_netmgr.status) {
        // active_status changed
        PR_DEBUG("netmgr conn status changed [%s] --> [%s]", NETMGR_STATUS_TO_STR(s_netmgr.status),
                 NETMGR_STATUS_TO_STR(active_status));
        s_netmgr.status = active_status;
        status_chg      = TRUE;
    } else if (active_conn != s_netmgr.active) {
        // active_conn changed
        PR_DEBUG("netmgr conn type changed [%s] --> [%s]", __netmgr_link_name(s_netmgr.active),
                 __netmgr_link_name(active_conn));
        s_netmgr.active = active_conn;
        type_chg        = TRUE;
    }

    // Step 2: snapshot the backend behind the active connection. Done whichever
    // branch above ran, or none of them, because the route is pushed
    // unconditionally below and its provider half always has to be filled in;
    // unless the active connection just changed, this is the value already
    // installed and the push is a no-op on that half.
    active_base = __netmgr_snap_provider(active_conn, &route);

    pub_active = s_netmgr.active;
    pub_status = s_netmgr.status;

    tal_mutex_unlock(lock);

    // Step 3: one push, unconditionally, whichever branch above ran or none of
    // them: any of them can mean the source address changed (a same-connection
    // down/up cycle among them), and a connection re-reporting link-up with a new
    // address takes no branch at all yet still has to be picked up here.
    //
    // This assumes a connection reports link-up only once its address is usable.
    // That holds on T5AI, where WFE_CONNECTED is raised from EVENT_NETIF_GOT_IP4
    // rather than at association. A platform that reports link-up earlier would
    // land a stale address here.
    //
    // Runs after the unlock, on the locals decided above: it reaches
    // conn->get(NETCONN_CMD_IP), a blocking modem exchange on cellular. Still
    // ahead of the publishes, so subscribers already see the new route.
    __netmgr_push_route(active_base, active_status, &route);

    // Published from local copies, after the unlock: subscribers re-enter netmgr
    // synchronously (tuya_iot's __tuya_iot_link_type_change_cb calls
    // tuya_iot_reconnect()), and publishing under the lock would deadlock a
    // non-recursive mutex.
    if (type_chg) {
        tal_event_publish(EVENT_LINK_TYPE_CHG, (void *)&pub_active);
    }
    if (status_chg) {
        tal_event_publish(EVENT_LINK_STATUS_CHG, (void *)&pub_status);
    }
}

/***********************************************************
******************** reporting channel *********************
***********************************************************/

/* Why the reports are coalesced into one static-state work item
 * =============================================================
 * Three properties of the TAL force this shape; none of them is negotiable.
 *
 * - Coalescing is required, not an optimisation. tal_workq_schedule() does not
 *   deduplicate, and the queue is bounded (MAX_NODE_NUM_WORK_QUEUE, 100). A link
 *   that flaps faster than the handler drains would otherwise fill WORKQ_SYSTEM
 *   and start failing work for every other subsystem that shares it.
 *
 * - The handler must take no `data` pointer. tal_workqueue.c's
 *   __work_cancel_traverse() matches an item on callback OR data, so the only
 *   cancel that is precise is tal_workq_cancel(WORKQ_SYSTEM, handler, NULL) -
 *   cancelling by data would blank unrelated items that happen to share a
 *   pointer. Cancelling by callback alone is only safe because the report state
 *   is static and shared, so there is never more than one item to cancel and no
 *   allocation to leak when it is cancelled.
 *
 * - WORKQ_SYSTEM is reused, no thread is created. The handler blocks (see
 *   __netmgr_push_route()), which rules out WORKQ_HIGHTPRI - documented as
 *   "block operations are not allowed" - and netmgr_init() calls the idempotent
 *   tal_workq_init() itself rather than assuming the app already did.
 */

/**
 * @brief Find the report slot of one registered link.
 *
 * @note Caller must hold s_netmgr.lock.
 */
static netmgr_report_t *__netmgr_report_slot(netmgr_type_e type)
{
    uint32_t i = 0;

    for (i = 0; i < s_netmgr.link_num; i++) {
        if (s_netmgr.report[i].type == type) {
            return &s_netmgr.report[i];
        }
    }

    return NULL;
}

/**
 * @brief The one context the netmgr state machine runs in.
 *
 * Takes no data pointer on purpose - see the note above.
 */
static void __netmgr_notify_work(void *data)
{
    netmgr_type_e   rpt_type[NETMGR_LINK_MAX]   = {0};
    netmgr_status_e rpt_status[NETMGR_LINK_MAX] = {NETMGR_LINK_DOWN};
    uint32_t        rpt_num                     = 0;
    uint32_t        i                           = 0;
    MUTEX_HANDLE    lock                        = s_netmgr.lock;

    (void)data;

    // netmgr_deinit() closes the gate before it touches anything, so a straggler
    // dequeued during teardown returns without walking half-dismantled state. The
    // handle is only NULL before the very first netmgr_init().
    if (sg_netmgr_gate_closed || NULL == lock) {
        return;
    }

    tal_mutex_lock(lock);

    // Cleared before the slots are read, so a report arriving from here on posts
    // a fresh work item instead of being absorbed into a pass that has already
    // sampled it. A report that lands between the two lines is simply handled
    // twice, which is harmless: nothing below trusts the reported value.
    s_netmgr.notify_queued = FALSE;

    if (s_netmgr.stopping) {
        tal_mutex_unlock(lock);
        return;
    }

    // Raised before the unlock and lowered under the lock at the end, so
    // netmgr_deinit() can tell "a handler is inside" from "the queue is empty".
    s_netmgr.notify_busy++;

    for (i = 0; i < s_netmgr.link_num; i++) {
        if (s_netmgr.report[i].pending) {
            s_netmgr.report[i].pending = FALSE;
            rpt_type[rpt_num]          = s_netmgr.report[i].type;
            rpt_status[rpt_num]        = s_netmgr.report[i].status;
            rpt_num++;
        }
    }

    tal_mutex_unlock(lock);

    // Advisory: logged so a trace is readable, never acted on. Selection below
    // re-reads every link through conn->get(NETCONN_CMD_STATUS), which is what
    // makes coalescing lossless.
    for (i = 0; i < rpt_num; i++) {
        PR_DEBUG("netmgr link [%s] reported [%s]", __netmgr_link_name(rpt_type[i]),
                 NETMGR_STATUS_TO_STR(rpt_status[i]));
    }

    __netmgr_reselect(lock);

    tal_mutex_lock(lock);
    if (s_netmgr.notify_busy > 0) {
        s_netmgr.notify_busy--;
    }
    tal_mutex_unlock(lock);
}

OPERATE_RET netmgr_notify_link(netmgr_type_e type, netmgr_status_e status)
{
    OPERATE_RET      rt        = OPRT_OK;
    BOOL_T           need_post = FALSE;
    netmgr_report_t *slot      = NULL;
    MUTEX_HANDLE     lock      = s_netmgr.lock;

    // Before netmgr_init() seeded the state, or after netmgr_deinit() started
    // tearing it down: there is nothing to record the report in, so it is
    // dropped. Not an error - a driver has no recovery for this and never had
    // one. The gate is checked first and without the lock BECAUSE the caller may
    // be a vendor task holding a base.event_cb pointer it read before
    // netmgr_deinit() nulled it; see the gate's own comment for what this does
    // and does not guarantee.
    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_OK;
    }

    tal_mutex_lock(lock);

    // Re-checked under the lock: a caller that passed the gate just before
    // netmgr_deinit() closed it is now serialised behind step 1, and must not
    // mark a slot the teardown has already sampled.
    if (s_netmgr.stopping) {
        tal_mutex_unlock(lock);
        return OPRT_OK;
    }

    slot = __netmgr_report_slot(type);
    if (NULL == slot) {
        tal_mutex_unlock(lock);
        PR_ERR("netmgr link [%s] reported but not registered", __netmgr_link_name(type));
        return OPRT_NOT_FOUND;
    }

    slot->status  = status;
    slot->pending = TRUE;

    if (!s_netmgr.notify_queued) {
        s_netmgr.notify_queued = TRUE;
        need_post              = TRUE;
    }

    tal_mutex_unlock(lock);

    if (!need_post) {
        // Absorbed into the pass that is already queued. This is the common case
        // when a link flaps, and it is what keeps WORKQ_SYSTEM from filling up.
        return OPRT_OK;
    }

    rt = tal_workq_schedule(WORKQ_SYSTEM, __netmgr_notify_work, NULL);
    if (OPRT_OK != rt) {
        // Drop the "queued" mark but keep `pending` set, so the next report from
        // any link retries the post and this one is not lost.
        tal_mutex_lock(lock);
        s_netmgr.notify_queued = FALSE;
        tal_mutex_unlock(lock);
        PR_ERR("netmgr notify schedule failed, rt = %d", rt);
    }

    return rt;
}

/**
 * @brief The connection event callback every driver is given.
 *
 * base.event_cb used to be __netmgr_event_cb() and ran the whole state machine
 * on the reporting thread. It is a one-liner now, which is the migration lever:
 * the thread model changed without a single edit to any driver.
 */
static void __netmgr_event_shim(netmgr_type_e type, netmgr_status_e status)
{
    (void)netmgr_notify_link(type, status);
}

/**
 * @brief Register one link from its descriptor.
 *
 * The descriptor is the source of truth for priority and socket provider:
 * conn->pri and conn->card_type are overwritten from it here, which is what
 * lets a board retune either without patching a driver.
 *
 * @param[in] desc the registry row to register
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
static OPERATE_RET __netmgr_conn_register(const netconn_desc_t *desc)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_base_t *conn      = NULL;
    netmgr_conn_base_t *cur_conn  = NULL;
    netmgr_conn_base_t *prev_conn = NULL;

    if (NULL == desc || NULL == desc->conn || NULL == desc->name) {
        PR_ERR("netmgr register failed, descriptor incomplete");
        return OPRT_INVALID_PARM;
    }

    conn = desc->conn;

    // Descriptor wins over the driver's static initialiser. Both base fields
    // degrade to caches of it, which is the ownership rule netconn_registry.h
    // states.
    conn->pri       = desc->default_pri;
    conn->card_type = desc->provider;
    conn->event_cb  = __netmgr_event_shim;

    tal_mutex_lock(s_netmgr.lock);

    if (s_netmgr.link_num >= NETMGR_LINK_MAX) {
        tal_mutex_unlock(s_netmgr.lock);
        PR_ERR("netmgr [%s] register failed, more than %d links", desc->name, NETMGR_LINK_MAX);
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    // check if the connection already registered
    cur_conn = s_netmgr.conn;
    while (cur_conn) {
        if (desc->type == cur_conn->type) {
            PR_DEBUG("netmgr [%s] already registered", desc->name);
            tal_mutex_unlock(s_netmgr.lock);
            return OPRT_INVALID_PARM;
        }
        cur_conn = cur_conn->next;
    }
    PR_DEBUG("netmgr [%s] register start, pri %d", desc->name, conn->pri);

    // Claim the report slot before the link goes on the list: conn->open() below
    // can make the driver report immediately (the LINUX tal_wired_set_status_cb()
    // fires before it returns) and netmgr_notify_link() needs the slot to exist.
    s_netmgr.report[s_netmgr.link_num].type    = desc->type;
    s_netmgr.report[s_netmgr.link_num].status  = NETMGR_LINK_DOWN;
    s_netmgr.report[s_netmgr.link_num].pending = FALSE;
    s_netmgr.link_num++;

    // First insert the new connection
    if (NULL == s_netmgr.conn) {
        s_netmgr.conn = conn;
        conn->next    = NULL;
        PR_DEBUG("netmgr [%s] is the first connection", desc->name);
        goto __EXIT;
    }

    // Insert the new connection in the linked list based on priority
    cur_conn = s_netmgr.conn;
    while (cur_conn) {
        if (cur_conn->pri < conn->pri) {
            if (prev_conn == NULL) {
                // insert at the head
                s_netmgr.conn = conn;
                conn->next    = cur_conn;
            } else {
                // insert in the middle
                prev_conn->next = conn;
                conn->next      = cur_conn;
            }
            break;
        }

        prev_conn = cur_conn;
        cur_conn  = cur_conn->next;
    }

    // If we reached the end of the list, insert at the tail
    if (cur_conn == NULL) {
        if (prev_conn == NULL) {
            // This should not happen as we already handled empty list case above
            s_netmgr.conn = conn;
            conn->next    = NULL;
        } else {
            prev_conn->next = conn;
            conn->next      = NULL;
        }
    }

__EXIT:
    tal_mutex_unlock(s_netmgr.lock);

    // open() runs with the lock released on purpose: it installs the driver's
    // status callback and some ports fire that callback inline (the LINUX
    // tkl_wired_set_status_cb() calls it before returning). That now lands in
    // __netmgr_event_shim(), which only marks the slot and posts a work item, so
    // it can no longer re-enter the state machine on this thread; keeping the
    // call outside the lock still matters because open() can block for a long
    // time (tal_cellular_init()).
    if (NULL != conn->open) {
        rt = conn->open(NULL);
    }

    return rt;
}

/**
 * @brief Initializes the network manager.
 *
 * This function initializes the network manager based on the specified type.
 *
 * @param type The type of network manager to initialize.
 * @return The result of the initialization operation.
 */
OPERATE_RET netmgr_init(netmgr_type_e type)
{
    OPERATE_RET           rt          = OPRT_OK;
    const netconn_desc_t *table       = NULL;
    uint32_t              count       = 0;
    uint32_t              i           = 0;
    netmgr_type_e         active      = NETCONN_AUTO;
    netmgr_status_e       status      = NETMGR_LINK_DOWN;
    netmgr_conn_base_t   *active_base = NULL;
    tal_net_route_t       route       = {.provider = TAL_NET_PROVIDER_DEFAULT, .src_ip = 0};

    TUYA_CALL_ERR_RETURN(tal_network_card_init());

    // The state machine runs on WORKQ_SYSTEM from here on, so make sure it
    // exists. tal_workq_init() is idempotent and every app already calls it;
    // netmgr no longer depends on that being true.
    TUYA_CALL_ERR_RETURN(tal_workq_init());

    // Created once per process, not once per init: netmgr_deinit() deliberately
    // does not release it (see the note on netmgr_deinit()), so a re-init reuses
    // the handle the previous teardown left behind.
    if (NULL == s_netmgr.lock) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&s_netmgr.lock));
    }
    s_netmgr.status   = NETMGR_LINK_DOWN;
    s_netmgr.type     = type;
    s_netmgr.stopping = FALSE;

    // Open the gate only now that s_netmgr is seeded and the mutex exists, and
    // before the registration loop - a driver can report from inside its own
    // open() (the LINUX tal_wired_set_status_cb() does) and that report has to be
    // recorded, not dropped.
    sg_netmgr_gate_closed = FALSE;

    // Every link this build has is a registry row. Which ones get registered is
    // the caller's type mask and nothing else - no technology is named here.
    table = netconn_registry_get_table(&count);
    if (NULL == table) {
        PR_ERR("netmgr has no link driver in this build");
        netmgr_deinit();
        return OPRT_NOT_SUPPORTED;
    }

    for (i = 0; i < count; i++) {
        if (!(type & table[i].type)) {
            continue;
        }
        // Registration failure of one link is not fatal for the others, which is
        // the behaviour the per-technology blocks had (their return values were
        // dropped). The "no link came up" check below is what catches a build
        // where nothing worked.
        __netmgr_conn_register(&table[i]);
    }

    tal_mutex_lock(s_netmgr.lock);
    s_netmgr.active = __get_active_conn();
    active          = s_netmgr.active;
    tal_mutex_unlock(s_netmgr.lock);

    if (active == NETCONN_AUTO) {
        PR_ERR("No connection available, please check your configuration");
        // Used to return here holding a created mutex and whatever links had
        // already been opened.
        netmgr_deinit();
        return OPRT_INVALID_PARM;
    }

    s_netmgr.inited = TRUE;

    // A link already up when we get here publishes no event, so seed the route.
    // Same three steps as __netmgr_reselect(): installed route first, provider
    // snapshotted under the lock, one push once the lock is released - the push
    // reaches conn->get(NETCONN_CMD_IP). Seeding the provider here as well as the
    // address is new: the address used to go down on its own, leaving the backend
    // at whatever tal_network_card_init() defaulted to until the first link event.
    // The route is one value now, so both halves are seeded together.
    tal_net_route_get(&route);

    tal_mutex_lock(s_netmgr.lock);
    status      = s_netmgr.status;
    active_base = __netmgr_snap_provider(active, &route);
    tal_mutex_unlock(s_netmgr.lock);

    __netmgr_push_route(active_base, status, &route);

    // Cellular not support LAN
    // M3 replaces this with `netconn_registry_find(active)->caps & NETCONN_CAP_LAN`
    // - the bit is already declared, consuming it is out of M2's scope. Note what
    // the guard costs meanwhile: a wifi+4G build loses LAN on its wifi link too.
#if !defined(ENABLE_CELLULAR) || (ENABLE_CELLULAR == 0)
    tal_sw_timer_create(__tuya_lan_init_tm_cb, NULL, &sg_lan_init_timer);
    tal_sw_timer_start(sg_lan_init_timer, 500, TAL_TIMER_CYCLE);
#endif

#ifdef ENABLE_BLUETOOTH
    /* Always bring up the BLE stack here. For ULP, the app tears it down via
     * tuya_ble_deinit() once online (TUYA_EVENT_MQTT_CONNECTED) - that deinit is
     * what actually powers down the BT controller. Gating this init instead
     * would leave the controller powered (deinit becomes a NULL no-op) and pin
     * the idle floor, so the original's "don't start BLE if activated" does not
     * translate to TuyaOpen; init-then-deinit reaches the same off state. */
    tuya_ble_cfg_t ble_cfg = {0};
    ble_cfg.client         = tuya_iot_client_get();
    snprintf(ble_cfg.device_name, sizeof(ble_cfg.device_name), "TYBLE");
    tuya_ble_init(&ble_cfg);
    /* Remember that WE started it. tuya_iot_destroy() also calls
     * tuya_ble_deinit() and that call stays; netmgr_deinit() must not be a
     * second unconditional teardown of a stack it may not own. */
    s_netmgr.ble_owned = TRUE;
#endif

    return rt;
}

/**
 * @brief Tear down the network manager, undoing netmgr_init().
 *
 * WHY THIS DOES NOT RELEASE s_netmgr.lock
 * =======================================
 * Read this before "fixing" the apparent leak. Releasing the mutex here cannot
 * be made safe, and keeping it costs exactly one mutex for the life of the
 * process however many init/deinit cycles run - which is less than the
 * alternative it replaced, where every drain timeout leaked one.
 *
 * The reason is that two of netmgr's entry points are reached by threads it
 * cannot account for, and neither can hold the mutex before deciding whether the
 * mutex still exists:
 *
 *   - the report shim. Drivers read base.event_cb with no lock at all - the
 *     pattern is "if (x->base.event_cb) x->base.event_cb(...)" - so a vendor task
 *     can read a live pointer, be preempted, and call in an arbitrary time later.
 *     Setting conn->event_cb = NULL below does not close that window, and the
 *     callback cannot be withdrawn at the TAL either: tal_wifi.h has no uninit
 *     and no way to retract the WIFI_EVENT_CB that tal_wifi_init() installed,
 *     and tal_wired_set_status_cb() does not accept NULL (netmgr_priv.h records
 *     why).
 *   - the LAN timer callback, on the tal_sw_timer thread, which
 *     tal_sw_timer_delete() cannot join.
 *
 * Every guard available is "test a flag, then take the lock", so each one only
 * narrows the window between the test and the lock - sg_netmgr_gate_closed
 * included. Closing it properly needs an atomic in-flight count, which the TAL
 * does not offer portably. Retaining the mutex removes the question instead:
 * there is no freed mutex to lock, so a straggler that wins the race blocks
 * harmlessly, sees `stopping`, and returns.
 *
 * The drain below is still needed - a handler must not observe a half-dismantled
 * s_netmgr - but a drain timeout is no longer a use-after-free, only a handler
 * that finishes late.
 *
 * The gate keeps its other job: a report arriving after teardown must not leave
 * `pending` set for the next netmgr_init() to act on.
 *
 * @return OPRT_OK on success, including when there was nothing to tear down.
 *         OPRT_TIMEOUT when the drain did not complete; everything that can
 *         safely be torn down still is.
 */
OPERATE_RET netmgr_deinit(void)
{
    MUTEX_HANDLE        lock                   = s_netmgr.lock;
    netmgr_conn_base_t *conns[NETMGR_LINK_MAX] = {0};
    uint32_t            num                    = 0;
    uint32_t            i                      = 0;
    uint32_t            elapsed                = 0;
    uint32_t            busy                   = 0;
    BOOL_T              drained                = FALSE;
    BOOL_T              ble_owned              = FALSE;
    TIMER_ID            lan_timer              = NULL;

    // 0. Close the gate before anything else, and without the lock. From here on
    // the report shim and the LAN timer callback return immediately instead of
    // reaching into state this function is about to dismantle. It stays closed
    // until the next netmgr_init() reopens it.
    sg_netmgr_gate_closed = TRUE;

    // Idempotent, and safe when netmgr_init() never ran: no mutex means nothing
    // downstream of it was ever set up either, and the struct is already the
    // zeroed state a fresh netmgr_init() expects.
    if (NULL == lock) {
        lan_timer         = sg_lan_init_timer;
        sg_lan_init_timer = NULL;
        if (NULL != lan_timer) {
            tal_sw_timer_stop(lan_timer);
            tal_sw_timer_delete(lan_timer);
        }
        memset(&s_netmgr, 0, sizeof(s_netmgr));
        s_netmgr.stopping = TRUE;
        return OPRT_OK;
    }

    // 1. Stop accepting work. netmgr_conn_get/set() refuse on `inited`;
    // netmgr_notify_link() and the handler already refuse on the gate above and
    // re-check `stopping` under the lock, which catches a caller that passed the
    // gate a moment before it closed.
    tal_mutex_lock(lock);
    s_netmgr.inited    = FALSE;
    s_netmgr.stopping  = TRUE;
    ble_owned          = s_netmgr.ble_owned;
    s_netmgr.ble_owned = FALSE;
    tal_mutex_unlock(lock);

    // 2. Drop a work item that is queued but has not started. Cancelling by
    // callback with a NULL data is the only precise form - see the notify
    // channel note - and it is safe because the report state is static.
    tal_workq_cancel(WORKQ_SYSTEM, __netmgr_notify_work, NULL);

    // 3. Drain a handler that is already running: releasing the mutex while it is
    // inside, or about to take it, is a use-after-free.
    //
    // The first sleep is unconditional and deliberate. tal_workq_cancel() only
    // blanks the callback of items still in the queue - __work_cancel_traverse()
    // sets item->cb = NULL - and an item the workqueue thread already dequeued is
    // out of its reach; there is no tal_workqueue_flush() in the TAL. So a
    // handler can be sitting between "dequeued" and "notify_busy++" right now,
    // invisible to the counter. One poll interval gives it time to become
    // visible. That narrows the window, it does not close it, which is why step 6
    // nulls s_netmgr.lock before releasing the mutex: a straggler then sees NULL
    // and returns.
    for (elapsed = 0; elapsed <= NETMGR_DRAIN_TIMEOUT_MS; elapsed += NETMGR_DRAIN_POLL_MS) {
        tal_system_sleep(NETMGR_DRAIN_POLL_MS);

        tal_mutex_lock(lock);
        busy = s_netmgr.notify_busy;
        tal_mutex_unlock(lock);

        if (0 == busy) {
            drained = TRUE;
            break;
        }
    }

    // 4. Unlink every link, newest registration first, then close them outside
    // the lock. s_netmgr.report[] is in registration order; s_netmgr.conn is
    // sorted by priority, so the list is not the order to walk.
    tal_mutex_lock(lock);
    num = s_netmgr.link_num;
    for (i = 0; i < num; i++) {
        conns[i] = __get_conn_by_type(s_netmgr.report[num - 1 - i].type);
    }
    s_netmgr.conn     = NULL;
    s_netmgr.link_num = 0;
    s_netmgr.active   = NETCONN_AUTO;
    s_netmgr.status   = NETMGR_LINK_DOWN;
    memset(s_netmgr.report, 0, sizeof(s_netmgr.report));
    s_netmgr.notify_queued = FALSE;
    tal_mutex_unlock(lock);

    // close() is a driver callback, so it runs with the lock released, same rule
    // as conn->open(). The conn nodes are static globals a later netmgr_init()
    // reuses, so they are put back exactly as they were found. A status callback
    // firing from inside close() still reaches the shim, where `stopping` drops
    // it.
    for (i = 0; i < num; i++) {
        if (NULL == conns[i]) {
            continue;
        }
        if (NULL != conns[i]->close) {
            conns[i]->close();
        }
        conns[i]->event_cb = NULL;
        conns[i]->next     = NULL;
        conns[i]->status   = NETMGR_LINK_DOWN;
    }

    // BLE only when this netmgr_init() is what brought it up. tuya_iot_destroy()
    // also calls tuya_ble_deinit() and that call is not ours to remove; that
    // netmgr owns the BLE stack at all is a layering problem for another PR.
#ifdef ENABLE_BLUETOOTH
    if (ble_owned) {
        tuya_ble_deinit();
    }
#else
    (void)ble_owned;
#endif

    // 5. The LAN timer. Stopped and deleted outside the lock; a callback already
    // inside it cannot be joined, so it is the gate closed in step 0 that turns it
    // into a no-op - and the retained mutex that makes losing that race harmless
    // rather than fatal.
    lan_timer         = sg_lan_init_timer;
    sg_lan_init_timer = NULL;
    if (NULL != lan_timer) {
        tal_sw_timer_stop(lan_timer);
        tal_sw_timer_delete(lan_timer);
    }

    // 6. Zero the state - then put back the two fields that must survive it.
    //
    // The mutex handle is retained for the reason argued above; it is what the
    // next netmgr_init() reuses. `stopping` is restored because the memset would
    // otherwise clear it: a straggling handler already past the gate would then
    // find stopping == FALSE, walk an empty s_netmgr.conn and push a route with
    // src_ip 0 over whatever the data plane currently has. netmgr_init() clears
    // it again once it has seeded the state.
    memset(&s_netmgr, 0, sizeof(s_netmgr));
    s_netmgr.lock     = lock;
    s_netmgr.stopping = TRUE;

    if (drained) {
        return OPRT_OK;
    }

    // Not a leak and not a crash - the mutex is kept, so a late handler locks a
    // live mutex, finds `stopping` and returns. What it does mean: teardown
    // finished while a handler was still inside, so work that handler had already
    // started - a tal_net_route_set() in particular - can land after this
    // function has returned.
    PR_ERR("netmgr notify drain timed out after %dms, a handler is still in flight and will finish after teardown",
           NETMGR_DRAIN_TIMEOUT_MS);

    return OPRT_TIMEOUT;
}

/**
 * @brief Sets the connection configuration for the network manager.
 *
 * This function is used to set the connection configuration for the network
 * manager.
 *
 * @param type The type of network manager.
 * @param cmd The connection configuration type.
 * @param param A pointer to the connection configuration parameter.
 *
 * @return The result of the operation.
 */
OPERATE_RET netmgr_conn_set(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param)
{
    OPERATE_RET           rt       = OPRT_OK;
    netmgr_conn_base_t   *cur_conn = NULL;
    const netconn_desc_t *desc     = NULL;
    netmgr_type_e         active   = NETCONN_AUTO;
    netmgr_status_e       link_st  = NETMGR_LINK_DOWN;

    // Checked before the lock: the handle only exists once netmgr_init() ran. The
    // gate is checked alongside `inited` for the reason in its own comment -
    // `inited` is cleared under a mutex this caller has not taken yet.
    if (sg_netmgr_gate_closed || !s_netmgr.inited) {
        return OPRT_RESOURCE_NOT_READY;
    }

    tal_mutex_lock(s_netmgr.lock);
    if (NETCONN_AUTO == type) {
        // get the active connection
        type = s_netmgr.active;
    }
    cur_conn = __get_conn_by_type(type);
    active   = s_netmgr.active;
    if (NULL != cur_conn) {
        link_st = cur_conn->status;
    }
    tal_mutex_unlock(s_netmgr.lock);

    PR_DEBUG("netmgr conn %s set %d", __netmgr_link_name(type), cmd);

    // No match used to fall out of the loop as OPRT_OK, so a set against an
    // unregistered link silently did nothing.
    if (NULL == cur_conn) {
        PR_ERR("netmgr conn [%s] set failed, not registered", __netmgr_link_name(type));
        return OPRT_NOT_FOUND;
    }

    TUYA_CHECK_NULL_RETURN(cur_conn->set, OPRT_INVALID_PARM);

    // Attribute screening, one place instead of a `default:` arm repeated in
    // every driver. desc->set_mask is a bit-for-bit transcription of that
    // driver's switch, so this rejects exactly what the driver would have
    // rejected and nothing more. A registered link with no descriptor cannot
    // happen - registration reads the descriptor - so the NULL arm skips the
    // screen rather than inventing a refusal.
    // The range test comes first: NETCONN_ATTR_BIT() is a shift and a shift by 32
    // or more is undefined behaviour, so an out-of-range cmd must never reach it.
    // netconn_registry.h keeps the enum under 32 with a compile-time assert; this
    // guards a caller passing a value the enum does not contain.
    desc = netconn_registry_find(type);
    if (NULL != desc && ((uint32_t)cmd >= 32 || 0 == (desc->set_mask & NETCONN_ATTR_BIT(cmd)))) {
        PR_DEBUG("netmgr conn [%s] does not support set %d", desc->name, cmd);
        return OPRT_NOT_SUPPORTED;
    }

    // Deliberately outside the lock: for NETCONN_CMD_PRI the drivers call
    // base.event_cb() inline (netconn_wifi_set/netconn_wired_set/
    // netconn_cellular_set). That reaches __netmgr_event_shim() now, so it is a
    // slot write and a work post rather than a re-entry into the state machine.
    // The conn nodes are static and never freed, so keeping the pointer across
    // the unlock is safe.
    rt = cur_conn->set(cmd, param);

    // Setting the address changes it without any link event, so the route
    // outbound sockets follow has to be refreshed. Reported through the notify
    // channel rather than pushed here, which makes the handler the single writer
    // of tal_net_route_set() and closes the race between two concurrent sources
    // over which consistent (provider, src_ip) pair lands last.
    //
    // The cost: the route may not be installed yet when this returns. No caller
    // in the tree sets NETCONN_CMD_IP, so nothing observes the difference today;
    // a caller that needs the route in place must wait for
    // EVENT_LINK_STATUS_CHG rather than assume it.
    //
    // Only meaningful for the active connection; a standby one is not what
    // traffic leaves through.
    if (OPRT_OK == rt && NETCONN_CMD_IP == cmd && type == active) {
        netmgr_notify_link(type, link_st);
    }

    return rt;
}

/**
 * @brief Get the connection configuration for the specified network manager
 * type.
 *
 * This function retrieves the connection configuration for the specified
 * network manager type.
 * @param type The network manager type.
 * @param cmd The connection configuration type.
 * @param param A pointer to the parameter structure for the connection
 * configuration.
 *
 * @return The operation result status.
 */
OPERATE_RET netmgr_conn_get(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param)
{
    OPERATE_RET           rt       = OPRT_OK;
    netmgr_conn_base_t   *cur_conn = NULL;
    const netconn_desc_t *desc     = NULL;

    // Checked before the lock, gate included: see netmgr_conn_set().
    if (sg_netmgr_gate_closed || !s_netmgr.inited) {
        return OPRT_RESOURCE_NOT_READY;
    }

    tal_mutex_lock(s_netmgr.lock);
    if (NETCONN_AUTO == type) {
        // get the active connection
        type = s_netmgr.active;
    }
    cur_conn = __get_conn_by_type(type);
    tal_mutex_unlock(s_netmgr.lock);

    // Falling off the end of the list used to return OPRT_OK with *param never
    // written, so callers formatted uninitialised stack (tal_cli's ip command
    // printed exactly that).
    if (NULL == cur_conn) {
        PR_ERR("netmgr conn [%s] get failed, not registered", __netmgr_link_name(type));
        return OPRT_NOT_FOUND;
    }

    TUYA_CHECK_NULL_RETURN(cur_conn->get, OPRT_INVALID_PARM);

    // Same screening as netmgr_conn_set(), against desc->get_mask.
    desc = netconn_registry_find(type);
    if (NULL != desc && ((uint32_t)cmd >= 32 || 0 == (desc->get_mask & NETCONN_ATTR_BIT(cmd)))) {
        PR_DEBUG("netmgr conn [%s] does not support get %d", desc->name, cmd);
        return OPRT_NOT_SUPPORTED;
    }

    // Outside the lock, per the contract at the top of the file: on cellular
    // NETCONN_CMD_IP is a blocking modem exchange, and every other caller would
    // otherwise queue behind it on s_netmgr.lock.
    rt = cur_conn->get(cmd, param);
    if (OPRT_OK != rt) {
        PR_ERR("netmgr conn %s get failed, cmd %d, rt = %d", __netmgr_link_name(type), cmd, rt);
        return rt;
    }

    return rt;
}

/***********************************************************
******************* snapshot accessors *********************
***********************************************************/

OPERATE_RET netmgr_state_get(netmgr_state_t *state)
{
    MUTEX_HANDLE lock = s_netmgr.lock;

    if (NULL == state) {
        return OPRT_INVALID_PARM;
    }

    memset(state, 0, sizeof(*state));
    state->active = NETCONN_AUTO;
    state->status = NETMGR_LINK_DOWN;

    // Before netmgr_init() or after netmgr_deinit() there is nothing to lock, and
    // "not inited" is a legitimate answer rather than an error - it is what the
    // CLI prints "network not ready" from. Gated like every other unsynchronised
    // entry point: the CLI runs on its own thread.
    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_OK;
    }

    tal_mutex_lock(lock);
    state->configured = s_netmgr.type;
    state->active     = s_netmgr.active;
    state->status     = s_netmgr.status;
    state->inited     = s_netmgr.inited;
    state->link_num   = s_netmgr.link_num;
    tal_mutex_unlock(lock);

    return OPRT_OK;
}

/**
 * @brief Fill @a info from one conn node plus its descriptor.
 *
 * The live half is copied under the lock by the caller; the descriptor half is
 * filled here, after the unlock, because the registry table is `static const`
 * and immutable once netmgr_init() has taken it.
 */
static void __netmgr_link_info_desc(netmgr_link_info_t *info)
{
    const netconn_desc_t *desc = netconn_registry_find(info->type);

    if (NULL == desc) {
        // A registered link always has a row - registration reads it - so this is
        // defensive only. name must never be NULL per the contract.
        info->name = __netmgr_link_name(info->type);
        return;
    }

    info->name     = desc->name;
    info->caps     = desc->caps;
    info->ctrl     = desc->ctrl;
    info->set_mask = desc->set_mask;
    info->get_mask = desc->get_mask;
}

OPERATE_RET netmgr_link_info_at(uint32_t index, netmgr_link_info_t *info)
{
    MUTEX_HANDLE        lock     = s_netmgr.lock;
    netmgr_conn_base_t *cur_conn = NULL;
    uint32_t            i        = 0;

    if (NULL == info) {
        return OPRT_INVALID_PARM;
    }

    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_NOT_FOUND;
    }

    // Positions follow selection order, which is list order: index 0 is what
    // __get_active_conn() considers first. Walking off the end is OPRT_NOT_FOUND,
    // which is what lets a caller iterate until it stops being OPRT_OK.
    tal_mutex_lock(lock);
    cur_conn = s_netmgr.conn;
    for (i = 0; i < index && NULL != cur_conn; i++) {
        cur_conn = cur_conn->next;
    }
    if (NULL == cur_conn) {
        tal_mutex_unlock(lock);
        return OPRT_NOT_FOUND;
    }

    memset(info, 0, sizeof(*info));
    info->type     = cur_conn->type;
    info->pri      = cur_conn->pri;
    info->status   = cur_conn->status;
    info->provider = cur_conn->card_type;
    tal_mutex_unlock(lock);

    // Formatting-free descriptor lookup, outside the lock: the point of these
    // accessors is that no caller ever holds s_netmgr.lock while printing.
    __netmgr_link_info_desc(info);

    return OPRT_OK;
}

OPERATE_RET netmgr_link_info_get(netmgr_type_e type, netmgr_link_info_t *info)
{
    MUTEX_HANDLE        lock     = s_netmgr.lock;
    netmgr_conn_base_t *cur_conn = NULL;

    if (NULL == info || NETCONN_AUTO == type) {
        return OPRT_INVALID_PARM;
    }

    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_NOT_FOUND;
    }

    tal_mutex_lock(lock);
    cur_conn = s_netmgr.conn;
    while (NULL != cur_conn && cur_conn->type != type) {
        cur_conn = cur_conn->next;
    }
    if (NULL == cur_conn) {
        tal_mutex_unlock(lock);
        return OPRT_NOT_FOUND;
    }

    memset(info, 0, sizeof(*info));
    info->type     = cur_conn->type;
    info->pri      = cur_conn->pri;
    info->status   = cur_conn->status;
    info->provider = cur_conn->card_type;
    tal_mutex_unlock(lock);

    __netmgr_link_info_desc(info);

    return OPRT_OK;
}
