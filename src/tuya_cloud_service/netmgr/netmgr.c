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
 * The implementation utilizes conditional compilation to include support for
 * the different network types based on the device capabilities and
 * configuration. It defines a structure for managing the state of the network
 * connections and provides functions for initializing the network manager,
 * setting the active network type, and querying the current network status.
 *
 * The network manager plays a crucial role in ensuring that Tuya devices can
 * maintain a stable and reliable connection to the Tuya cloud services,
 * facilitating device control and data exchange.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     Refactored network manager to support management of multiple network connection types
 *
 */

#include "netmgr.h"
#include "tal_api.h"
#include "tuya_slist.h"
#include "tuya_cloud_com_defs.h"
#include "tuya_error_code.h"
#include "tuya_lan.h"

#ifdef ENABLE_WIFI
#include "netconn_wifi.h"
extern netmgr_conn_wifi_t s_netmgr_wifi;
#endif

#ifdef ENABLE_WIRED
#include "netconn_wired.h"
extern netmgr_conn_wired_t s_netmgr_wired;
#endif

#ifdef ENABLE_CELLULAR
#include "netconn_cellular.h"
extern netmgr_conn_cellular_t s_netmgr_cellular;
#endif

#ifdef ENABLE_BLUETOOTH
#include "ble_mgr.h"
#endif

typedef struct {
    MUTEX_HANDLE lock; // mutex
    BOOL_T inited;

    netmgr_type_e type;     // network manage type
    netmgr_type_e active;   // the connect now used
    netmgr_status_e status; // the network status

    netmgr_conn_base_t *conn; // connections
} netmgr_t;

static netmgr_t s_netmgr = {0};

/* Locking contract for s_netmgr
 * =============================
 * s_netmgr.conn / .active / .status are reached from the wifi and wired/cellular
 * driver callbacks, from the tal_sw_timer thread and from any caller of the
 * public netmgr_conn_get()/netmgr_conn_set(), so all of them are accessed under
 * s_netmgr.lock. The invariant that shapes every function below:
 *
 *     The lock protects field access on s_netmgr and nothing else. No driver
 *     callback - conn->open(), conn->get(), conn->set() - and no
 *     tal_event_publish() runs while the lock is held.
 *
 * Both halves of that matter:
 *
 * - Latency. A driver callback can block for a long time: netconn_cellular_get()
 *   servicing NETCONN_CMD_IP goes to tal_cellular_get_ip() and on into a modem
 *   AT exchange. Holding the lock across it would park every link-event callback
 *   on the same mutex for the duration.
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
 * unlinked or freed.
 *
 * One bounded carve-out: __get_active_conn() and __get_netmgr_status() call
 * conn->get(NETCONN_CMD_STATUS) while walking the list, which cannot be hoisted
 * out of the lock without snapshotting the whole list. All three drivers answer
 * that one command from their cached base.status with no TKL call, so it stays
 * bounded. A port that ever makes NETCONN_CMD_STATUS blocking breaks this.
 */

static TIMER_ID sg_lan_init_timer = NULL;

/**
 * @brief get active connection status and
 *
 * @note Caller must hold s_netmgr.lock: this walks the connection list.
 *
 * @return netconn_type_t: the connection should be used
 */
static netmgr_type_e __get_active_conn()
{
    netmgr_type_e active_type = NETCONN_AUTO;
    netmgr_conn_base_t *cur_conn = s_netmgr.conn;

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
            PR_TRACE("netmgr active connection [%s]", NETMGR_TYPE_TO_STR(cur_conn->type));
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

    // Snapshot under the lock, then act outside it: tuya_lan_init() is heavy and
    // has no business running with the netmgr state locked.
    tal_mutex_lock(s_netmgr.lock);
    status = s_netmgr.status;
    type   = (netmgr_type_e)s_netmgr.type;
    tal_mutex_unlock(s_netmgr.lock);

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

    PR_ERR("[%s] not found", NETMGR_TYPE_TO_STR(type));
    return NULL;
}

/**
 * @brief Read the link status of one registered connection.
 *
 * @note Caller must hold s_netmgr.lock.
 */
static OPERATE_RET __get_netmgr_status(netmgr_type_e type, netmgr_status_e *status)
{
    OPERATE_RET rt = OPRT_OK;
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
        PR_ERR("netmgr type [%s] not supported", NETMGR_TYPE_TO_STR(type));
        return OPRT_NOT_SUPPORTED;
    }

    // A type nobody registered is not "link down": reporting OPRT_OK here would
    // hand the caller the default above and hide the misconfiguration.
    cur_conn = __get_conn_by_type(type);
    if (NULL == cur_conn) {
        PR_ERR("netmgr get status failed, conn [%s] not registered", NETMGR_TYPE_TO_STR(type));
        return OPRT_NOT_FOUND;
    }

    // get the connection status
    if (NULL == cur_conn->get) {
        PR_ERR("netmgr conn [%s] get status failed", NETMGR_TYPE_TO_STR(type));
        return OPRT_INVALID_PARM;
    }

    cur_conn->get(NETCONN_CMD_STATUS, status);
    PR_TRACE("netmgr conn [%s] status [%s]", NETMGR_TYPE_TO_STR(type), NETMGR_STATUS_TO_STR(*status));

    return rt;
}

/**
 * @brief connection event callback, called when connection event happed
 *
 * @param event the connection event
 */
/**
 * @brief Publish the active connection address down to tal_network.
 *
 * Outbound sockets bind to this so traffic leaves the interface netmgr picked.
 * Pushing it here means it tracks every link event - including a cellular redial
 * or DHCP renew that hands out a different address - whereas caching it at first
 * use would pin the first address seen for the life of the transport.
 *
 * @note Must be called with s_netmgr.lock released: it goes out to
 *       conn->get(NETCONN_CMD_IP), which on cellular is a blocking modem
 *       exchange. Both arguments are snapshots and the body touches no
 *       s_netmgr field of its own, so it needs no lock.
 */
static void __netmgr_sync_active_ip(netmgr_type_e type, netmgr_status_e status)
{
    TUYA_IP_ADDR_T addr = 0;
    NW_IP_S nw_ip = {0};

    if (NETMGR_LINK_DOWN != status && OPRT_OK == netmgr_conn_get(type, NETCONN_CMD_IP, &nw_ip) &&
        nw_ip.ip[0] != '\0') {
        addr = tal_net_str2addr(nw_ip.ip);
    }

    // 0 means "do not bind": better an unbound socket than one pinned to an
    // address the link no longer owns.
    tal_network_card_set_active_ip(addr);
}

/**
 * @brief Point tal_network at the card backing @a type.
 *
 * @note Caller must hold s_netmgr.lock.
 */
static void __netmgr_set_active_card(netmgr_type_e type)
{
    netmgr_conn_base_t *p_conn = __get_conn_by_type(type);

    // __get_conn_by_type() returns NULL for NETCONN_AUTO and for a type nothing
    // registered. Nothing to retarget then - keep the card we had rather than
    // dereference NULL. The caller still publishes its event.
    if (NULL == p_conn) {
        PR_ERR("netmgr conn [%s] not found, active card left unchanged", NETMGR_TYPE_TO_STR(type));
        return;
    }

    tal_network_card_set_active(p_conn->card_type);
}

static void __netmgr_event_cb(netmgr_type_e type, netmgr_status_e status)
{
    // status unused
    (void)status;

    BOOL_T          type_chg   = FALSE;
    BOOL_T          status_chg = FALSE;
    netmgr_type_e   pub_active = NETCONN_AUTO;
    netmgr_status_e pub_status = NETMGR_LINK_DOWN;

    if (!(s_netmgr.type & type)) {
        return;
    }

    tal_mutex_lock(s_netmgr.lock);

    netmgr_status_e active_status = NETMGR_LINK_DOWN;
    netmgr_type_e   active_conn   = __get_active_conn();
    __get_netmgr_status(active_conn, &active_status);

    // both changed
    if (active_status != s_netmgr.status && active_conn != s_netmgr.active) {
        PR_DEBUG("netmgr conn type changed [%s] --> [%s], status changed %d --> %d",
                 NETMGR_TYPE_TO_STR(s_netmgr.active), NETMGR_TYPE_TO_STR(active_conn), s_netmgr.status, active_status);
        s_netmgr.status = active_status;
        s_netmgr.active = active_conn;
        __netmgr_set_active_card(active_conn);
        type_chg   = TRUE;
        status_chg = TRUE;
    } else if (active_status != s_netmgr.status) {
        // active_status changed
        PR_DEBUG("netmgr conn status changed [%s] --> [%s]", NETMGR_STATUS_TO_STR(s_netmgr.status),
                 NETMGR_STATUS_TO_STR(active_status));
        s_netmgr.status = active_status;
        status_chg      = TRUE;
    } else if (active_conn != s_netmgr.active) {
        // active_conn changed
        PR_DEBUG("netmgr conn type changed [%s] --> [%s]", NETMGR_TYPE_TO_STR(s_netmgr.active),
                 NETMGR_TYPE_TO_STR(active_conn));
        s_netmgr.active = active_conn;
        __netmgr_set_active_card(active_conn);
        type_chg = TRUE;
    }

    pub_active = s_netmgr.active;
    pub_status = s_netmgr.status;

    tal_mutex_unlock(s_netmgr.lock);

    // Refreshed unconditionally, whichever branch above ran or none of them: any
    // of them can mean the source address changed (a same-connection down/up
    // cycle among them), and a connection re-reporting link-up with a new address
    // takes no branch at all yet still has to be picked up here.
    //
    // This assumes a connection reports link-up only once its address is usable.
    // That holds on T5AI, where WFE_CONNECTED is raised from EVENT_NETIF_GOT_IP4
    // rather than at association. A platform that reports link-up earlier would
    // land a stale address here.
    //
    // Runs after the unlock, on the locals decided above: it reaches
    // conn->get(NETCONN_CMD_IP), a blocking modem exchange on cellular. Still
    // ahead of the publishes, so subscribers already see the new bound address.
    __netmgr_sync_active_ip(active_conn, active_status);

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

    return;
}

OPERATE_RET __netmgr_conn_register(netmgr_type_e type, netmgr_conn_base_t *conn)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_base_t *cur_conn  = NULL;
    netmgr_conn_base_t *prev_conn = NULL;

    if (NULL == conn) {
        PR_ERR("netmgr [%s] register failed, conn is NULL", NETMGR_TYPE_TO_STR(type));
        return OPRT_INVALID_PARM;
    }

    conn->event_cb = __netmgr_event_cb;

    tal_mutex_lock(s_netmgr.lock);

    // check if the connection already registered
    cur_conn = s_netmgr.conn;
    while (cur_conn) {
        if (type == cur_conn->type) {
            PR_DEBUG("netmgr [%s] already registered", NETMGR_TYPE_TO_STR(type));
            tal_mutex_unlock(s_netmgr.lock);
            return OPRT_INVALID_PARM;
        }
        cur_conn = cur_conn->next;
    }
    PR_DEBUG("netmgr [%s] register start", NETMGR_TYPE_TO_STR(type));

    // First insert the new connection
    if (NULL == s_netmgr.conn) {
        s_netmgr.conn = conn;
        conn->next = NULL;
        PR_DEBUG("netmgr [%s] is the first connection", NETMGR_TYPE_TO_STR(type));
        goto __EXIT;
    }

    // Insert the new connection in the linked list based on priority
    cur_conn = s_netmgr.conn;
    while (cur_conn) {
        if (cur_conn->pri < conn->pri) {
            if (prev_conn == NULL) {
                // insert at the head
                s_netmgr.conn = conn;
                conn->next = cur_conn;
            } else {
                // insert in the middle
                prev_conn->next = conn;
                conn->next = cur_conn;
            }
            break;
        }

        prev_conn = cur_conn;
        cur_conn = cur_conn->next;
    }

    // If we reached the end of the list, insert at the tail
    if (cur_conn == NULL) {
        if (prev_conn == NULL) {
            // This should not happen as we already handled empty list case above
            s_netmgr.conn = conn;
            conn->next = NULL;
        } else {
            prev_conn->next = conn;
            conn->next = NULL;
        }
    }

__EXIT:
    tal_mutex_unlock(s_netmgr.lock);

    // open() runs with the lock released on purpose: it installs the driver's
    // status callback and some ports fire that callback inline (the LINUX
    // tkl_wired_set_status_cb() calls it before returning), which lands in
    // __netmgr_event_cb() and would self-deadlock on a non-recursive mutex.
    // The list is already fully linked at this point, so the re-entrant pass
    // sees consistent state; it just runs before s_netmgr.inited is set, which
    // is the same window the code had before.
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
    OPERATE_RET rt = OPRT_OK;
    netmgr_type_e   active = NETCONN_AUTO;
    netmgr_status_e status = NETMGR_LINK_DOWN;

    TUYA_CALL_ERR_RETURN(tal_network_card_init());

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&s_netmgr.lock));
    s_netmgr.status = NETMGR_LINK_DOWN;
    s_netmgr.type = type;

#ifdef ENABLE_WIRED
    if (type & NETCONN_WIRED) {
        __netmgr_conn_register(NETCONN_WIRED, (netmgr_conn_base_t *)&s_netmgr_wired);
    }
#endif

#ifdef ENABLE_CELLULAR
    if (type & NETCONN_CELLULAR) {
        __netmgr_conn_register(NETCONN_CELLULAR, (netmgr_conn_base_t *)&s_netmgr_cellular);
    }
#endif

#ifdef ENABLE_WIFI
    if (type & NETCONN_WIFI) {
        __netmgr_conn_register(NETCONN_WIFI, (netmgr_conn_base_t *)&s_netmgr_wifi);
    }
#endif
    tal_mutex_lock(s_netmgr.lock);
    s_netmgr.active = __get_active_conn();
    active          = s_netmgr.active;
    tal_mutex_unlock(s_netmgr.lock);

    if (active == NETCONN_AUTO) {
        PR_ERR("No connection available, please check your configuration");
        return OPRT_INVALID_PARM;
    }

    s_netmgr.inited = TRUE;

    // A link already up when we get here publishes no event, so seed the address.
    // Snapshot, then sync with the lock released - the sync reaches conn->get().
    tal_mutex_lock(s_netmgr.lock);
    status = s_netmgr.status;
    tal_mutex_unlock(s_netmgr.lock);

    __netmgr_sync_active_ip(active, status);

    // Cellular not support LAN
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
    ble_cfg.client = tuya_iot_client_get();
    snprintf(ble_cfg.device_name, sizeof(ble_cfg.device_name), "TYBLE");
    tuya_ble_init(&ble_cfg);
#endif

    return rt;
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
    OPERATE_RET rt = OPRT_OK;
    netmgr_conn_base_t *cur_conn = NULL;
    netmgr_type_e       active   = NETCONN_AUTO;

    // Checked before the lock: the handle only exists once netmgr_init() ran.
    if (!s_netmgr.inited) {
        return OPRT_RESOURCE_NOT_READY;
    }

    PR_DEBUG("netmgr conn %s set %d", NETMGR_TYPE_TO_STR(type), cmd);

    tal_mutex_lock(s_netmgr.lock);
    if (NETCONN_AUTO == type) {
        // get the active connection
        type = s_netmgr.active;
    }
    cur_conn = __get_conn_by_type(type);
    active   = s_netmgr.active;
    tal_mutex_unlock(s_netmgr.lock);

    // No match used to fall out of the loop as OPRT_OK, so a set against an
    // unregistered link silently did nothing.
    if (NULL == cur_conn) {
        PR_ERR("netmgr conn [%s] set failed, not registered", NETMGR_TYPE_TO_STR(type));
        return OPRT_NOT_FOUND;
    }

    TUYA_CHECK_NULL_RETURN(cur_conn->set, OPRT_INVALID_PARM);

    // Deliberately outside the lock: for NETCONN_CMD_PRI the drivers call
    // base.event_cb() inline (netconn_wifi_set/netconn_wired_set/
    // netconn_cellular_set), which re-enters __netmgr_event_cb(). The conn nodes
    // are static and never unlinked, so keeping the pointer across the unlock is
    // safe.
    rt = cur_conn->set(cmd, param);

    // Setting the address changes it without any link event, so refresh
    // what outbound sockets bind to. Only meaningful for the active
    // connection; a standby one is not what traffic leaves through.
    if (OPRT_OK == rt && NETCONN_CMD_IP == cmd && type == active) {
        netmgr_status_e status = NETMGR_LINK_DOWN;

        tal_mutex_lock(s_netmgr.lock);
        status = s_netmgr.status;
        tal_mutex_unlock(s_netmgr.lock);

        __netmgr_sync_active_ip(type, status);
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
    OPERATE_RET rt = OPRT_OK;
    netmgr_conn_base_t *cur_conn = NULL;

    // Checked before the lock: the handle only exists once netmgr_init() ran.
    if (!s_netmgr.inited) {
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
        PR_ERR("netmgr conn [%s] get failed, not registered", NETMGR_TYPE_TO_STR(type));
        return OPRT_NOT_FOUND;
    }

    TUYA_CHECK_NULL_RETURN(cur_conn->get, OPRT_INVALID_PARM);

    // Outside the lock, per the contract at the top of the file: on cellular
    // NETCONN_CMD_IP is a blocking modem exchange, and every link-event callback
    // would otherwise queue behind it on s_netmgr.lock.
    rt = cur_conn->get(cmd, param);
    if (OPRT_OK != rt) {
        PR_ERR("netmgr conn %s get failed, cmd %d, rt = %d", NETMGR_TYPE_TO_STR(type), cmd, rt);
        return rt;
    }

    return rt;
}

/**
 * @brief Executes a network manager command.
 *
 * This function is responsible for executing a network manager command.
 *
 * @param argc The number of command line arguments.
 * @param argv An array of command line arguments.
 */
void netmgr_cmd(int argc, char *argv[])
{
    if (!s_netmgr.inited) {
        PR_INFO("network not ready!");
        return;
    }

    if (argc > 5) {
        PR_INFO("usage: netmgr [wifi|wired|switch] [donw/up]");
        return;
    }

    netmgr_conn_base_t *p_conn = NULL;

    if (argc == 1) {
        // dump network connection
        tal_mutex_lock(s_netmgr.lock);
        PR_NOTICE("netmgr active %d, status %d", s_netmgr.active, s_netmgr.status);
        PR_NOTICE("---------------------------------------");
        if (s_netmgr.type & NETCONN_WIFI) {
            p_conn = __get_conn_by_type(NETCONN_WIFI);
            if (p_conn) {
                PR_NOTICE("type wifi pri %d status %s", p_conn->pri, NETMGR_STATUS_TO_STR(p_conn->status));
            }
        }
        if (s_netmgr.type & NETCONN_WIRED) {
            p_conn = __get_conn_by_type(NETCONN_WIRED);
            if (p_conn) {
                PR_NOTICE("type wire pri %d status %s", p_conn->pri, NETMGR_STATUS_TO_STR(p_conn->status));
            }
        }
        tal_mutex_unlock(s_netmgr.lock);
    } else {
        if (0 == strcmp(argv[1], "wifi")) {
#ifdef ENABLE_WIFI
            if (!(s_netmgr.type & NETCONN_WIFI)) {
                PR_INFO("usage: netmgr [wifi] [down/up/scan]");
            } else if (0 == strcmp(argv[2], "up")) {
                netconn_wifi_info_t wifi_info = {0};
                if (argc < 4) {
                    PR_INFO("usage: netmgr wifi up <ssid> <password>");
                    return;
                }
                if (strlen(argv[3]) > WIFI_SSID_LEN || strlen(argv[4]) > WIFI_PASSWD_LEN) {
                    PR_INFO("ssid or password too long");
                    return;
                }
                strncpy(wifi_info.ssid, argv[3], sizeof(wifi_info.ssid) - 1);
                wifi_info.ssid[sizeof(wifi_info.ssid) - 1] = '\0';
                strncpy(wifi_info.pswd, argv[4], sizeof(wifi_info.pswd) - 1);
                wifi_info.pswd[sizeof(wifi_info.pswd) - 1] = '\0';
                netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
            } else if (0 == strcmp(argv[2], "down")) {
                netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_CLOSE, NULL);
            } else if (0 == strcmp(argv[2], "scan")) {
                AP_IF_S *aplist;
                uint32_t num;
                tal_wifi_all_ap_scan(&aplist, &num);
            } else {
                PR_INFO("usage: netmgr [wifi] [down/up/scan]");
            }
#else
            PR_INFO("wifi disabled");
#endif
        } else if (0 == strcmp(argv[1], "wired")) {
#ifdef ENABLE_WIRED
            if (!(s_netmgr.type & NETCONN_WIRED)) {
                PR_INFO("usage: netmgr [wired] [donw/up]");
            } else if (0 == strcmp(argv[2], "up")) {
                // TBD..
            } else if (0 == strcmp(argv[2], "down")) {
                // TBD
            } else {
                PR_INFO("usage: netmgr wire [donw/up]");
            }
#else
            PR_INFO("wired disabled");
#endif
            return;
        } else if (0 == strcmp(argv[1], "switch")) {
            PR_DEBUG("netmgr switch not implemented yet");
        } else {
            PR_INFO("usage: netmgr [wifi|wired|switch] [down|up]");
        }
    }
}
