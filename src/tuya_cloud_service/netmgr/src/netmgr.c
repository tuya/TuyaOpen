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

/* The four M3 contracts. This file is the whole consumer side of all of them:
 * netmgr_policy.h and netmgr_retry.h are implemented elsewhere and only called
 * from here, while netmgr_probe.h and netmgr_event.h are implemented here except
 * for the one passive backend instance netmgr_probe.c owns. */
#include "netmgr_policy.h"
#include "netmgr_probe.h"
#include "netmgr_retry.h"
#include "netmgr_event.h"

#include "tal_api.h"
#include "tuya_slist.h"
#include "tuya_cloud_com_defs.h"
#include "tuya_error_code.h"
#include "tuya_lan.h"

/* The data plane, included here and not from netmgr.h: the control plane's public
 * header must not depend on it. tal_net_route.h is the one channel netmgr uses to
 * write the data plane; tal_net_provider.h is here for tal_net_provider_init()
 * and for TAL_NET_PROVIDER_DEFAULT. */
#include "tal_net_provider.h"
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
 * How long to wait before retrying a notify work item the workqueue refused.
 *
 * Short, because the state machine is not running while a post is outstanding and
 * the route may be wrong for the whole interval. Not zero, because the only
 * reason tal_workq_schedule() fails is a full queue or a failed allocation, and
 * retrying either of those immediately is how a busy device stays busy.
 */
#define NETMGR_NOTIFY_RETRY_MS 200

/**
 * @brief One link's pending report and everything netmgr knows about it, in
 *        registration order.
 *
 * Static storage on purpose - see __netmgr_notify_work().
 *
 * M3 makes the index into this array the link's IDENTITY, not just the place its
 * report lands: netmgr_link_view_t.reg_index is documented as "the index into
 * s_netmgr.report[]", so the registration order this array already had becomes
 * the secondary ranking key. That is why the per-link state machine lives here
 * rather than in a parallel array - one index, one link, one struct.
 */
typedef struct {
    netmgr_type_e   type;    // which link this slot belongs to
    netmgr_status_e status;  // last reported status, advisory only
    BOOL_T          pending; // set by netmgr_notify_link(), cleared by the handler

    /* The registry row this link was registered from, resolved once. It carries
     * both halves the state machine needs - desc->conn for the live fields and
     * desc->caps / desc->ctrl for the descriptor ones - so a pass needs neither
     * the __get_conn_by_type() list walk nor a netconn_registry_find() lookup per
     * link. Safe to cache: the table is `static const` and immutable from the
     * moment netmgr_init() takes it, and the conn nodes it points at are static
     * globals that are never freed. */
    const netconn_desc_t *desc;

    /* The per-link state machine of netmgr_policy.h. */
    netmgr_link_state_e state;

    /**
     * When up_debounce_ms will have elapsed for this link, 0 for "eligible now".
     * Handed to the ranking function verbatim as netmgr_link_view_t.eligible_at_ms.
     */
    uint32_t eligible_at_ms;

    /**
     * down_grace_ms deadline while the ACTIVE link is being held up through a
     * brief drop, 0 when no grace is running. The link's state stays up for the
     * duration, which is how grace stays invisible to the ranking function.
     */
    uint32_t grace_at_ms;

    /**
     * verify_timeout_ms deadline while this link is ACTIVE and UNVERIFIED, 0
     * when it is not being timed. Rebased when the link becomes active.
     */
    uint32_t verify_at_ms;

    /** Revalidation back-off for a DEGRADED link. */
    netmgr_retry_t reval;

    /** Accumulated probe verdicts. See netmgr_probe.h. */
    netmgr_probe_stat_t probe;

    /**
     * conn->pri as of the previous pass, so a NETCONN_CMD_PRI change can be
     * NAMED (NETMGR_CHG_REASON_PRI_CHANGED) rather than inferred. One byte per
     * link buys the one reason netmgr_event.h says was unreachable before M3.
     */
    uint8_t last_pri;
} netmgr_report_t;

typedef struct {
    MUTEX_HANDLE lock; // mutex
    BOOL_T       inited;
    BOOL_T       stopping;  // netmgr_deinit() in progress: refuse new work
    BOOL_T       ble_owned; // this netmgr_init() is what brought the BLE stack up

    netmgr_type_e   type;   // network manage type
    netmgr_type_e   active; // the connect now used
    netmgr_status_e status; // the network status

    netmgr_conn_base_t *conn; // connections, in registration order

    uint32_t        link_num;                // registered links, and valid entries in report[]
    netmgr_report_t report[NETMGR_LINK_MAX]; // pending reports, in registration order

    BOOL_T   notify_queued; // a notify work item is queued and has not started
    uint32_t notify_busy;   // notify handlers currently running

    /**
     * Mutual exclusion for __netmgr_settle(), and the reason it is needed even
     * though every POSTED pass is serialised by the single WORKQ_SYSTEM thread:
     * netmgr_init() calls the pass directly, on its caller's thread. Without this
     * flag that pass can run against a work item queued moments earlier by a
     * driver reporting from inside conn->open() - which on LINUX is the normal
     * case, not a rare one, because tkl_wired_set_status_cb() fires its callback
     * before returning.
     *
     * `settle_missed` is the re-run bit. A pass that finds the flag taken records
     * its reason in req_reason and returns; the pass that owns the flag re-posts
     * on the way out. Re-posting rather than looping in place keeps the whole
     * mechanism single-exit, which is what makes the flag safe to hold across the
     * body at all.
     */
    BOOL_T settle_busy;
    BOOL_T settle_missed;

    /** When s_netmgr.active last changed, the base for policy.min_dwell_ms. */
    uint32_t active_since_ms;

    /**
     * The route the last __netmgr_push_route() installed, and its generation.
     *
     * Kept so a pass can tell "the route moved" from "the same route was
     * re-announced": the first bumps @ref route_epoch and reports
     * NETMGR_CHG_REASON_ADDR_CHANGED, the second is silent. netmgr_probe.h's
     * epoch is exactly this counter, which is why it lives next to the value it
     * counts changes of rather than on its own.
     */
    tal_net_route_t route;
    uint32_t        route_epoch;

    /** The pin as of the previous pass, so PINNED/UNPINNED can be named. */
    netmgr_type_e last_pin;

    /**
     * The reason a netmgr_reselect_request() gave, held until the next pass
     * consumes it, or NETMGR_CHG_REASON_NONE.
     *
     * It is a field rather than a parameter because the requester and the pass run
     * on different threads: the request only marks and posts, exactly like a
     * driver report, so the cause has to survive the hand-off to WORKQ_SYSTEM.
     * Folded by rank like everything else, so a request cannot mask a link event
     * that lands in the same coalesced pass.
     */
    netmgr_change_reason_e req_reason;

    /** TRUE once tuya_lan_init() has been called for this init cycle. */
    BOOL_T lan_started;
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
 * One bounded carve-out: __netmgr_links_step() calls conn->get(NETCONN_CMD_STATUS)
 * once per link while building the snapshot, which cannot be hoisted out of the
 * lock without snapshotting the whole list first. All three drivers answer that
 * one command from their cached base.status with no TKL call, so it stays
 * bounded. A port that ever makes NETCONN_CMD_STATUS blocking breaks this.
 *
 * What M3 adds to the contract, and why none of it widens the lock
 * ---------------------------------------------------------------
 * Three new things run WITH the lock held, and each is admissible for a reason
 * that is written down in its own contract rather than assumed here:
 *
 *   - netmgr_policy_select(), i.e. the built-in ranking or a product hook.
 *     netmgr_policy.h requires it to be pure arithmetic over its snapshot: no
 *     driver access, no publish, no blocking, no netmgr re-entry. That is why the
 *     input is a flat array of netmgr_link_view_t and not a set of accessors -
 *     the type makes the rule enforceable rather than advisory;
 *   - netmgr_policy_get() and netmgr_policy_pin_get(). netmgr_policy.c is
 *     deliberately lock-free so that these two are safe to call from inside this
 *     lock; that property is load-bearing, not incidental, and it is what keeps
 *     the module at exactly one mutex;
 *   - the netmgr_retry_* arithmetic. netmgr_retry.h states it plainly: the
 *     instances live inside s_netmgr and are therefore covered by this lock, and
 *     the arithmetic is short and non-blocking.
 *
 * Two things that would be tempting to do under the lock and are NOT:
 *
 *   - arming the shared deadline timer. tal_sw_timer_start() takes the timer
 *     manager's own mutex and posts its semaphore, so doing it here would order
 *     two mutexes against each other for no gain. It happens after the unlock,
 *     from the pass's locals, and the handler is single-context so nothing can
 *     race it;
 *   - tuya_lan_init(). It is heavy, it reaches the cloud layer, and
 *     tuya_iot_client_get() is a cloud-layer call. Same rule as conn->open().
 *
 * A SECOND, INDEPENDENT reason not to publish under the lock
 * ----------------------------------------------------------
 * The original reason is deadlock on a non-recursive mutex, above: subscribers
 * re-enter netmgr synchronously. M3 adds a reason that survives even if that one
 * ever stops applying, and it is recorded here so nobody moves a publish back
 * inside the lock on the grounds that "the first reason no longer holds".
 *
 * It is lock ORDER. tal_event_publish() dispatches with the event module's own
 * mutex held, and the probe backend's subscribers call netmgr_probe_report(),
 * which takes s_netmgr.lock. So the dispatch path is
 *
 *     event mutex  ->  s_netmgr.lock
 *
 * Publishing from inside s_netmgr.lock would create the opposite order,
 *
 *     s_netmgr.lock  ->  event mutex
 *
 * and two threads taking them in opposite orders is a classic inversion. Keeping
 * every tal_event_publish() outside the lock means netmgr only ever walks the
 * first ordering.
 */

/* The single shared deadline. Every debounce, grace, verify and revalidation
 * deadline in the module collapses into this one timer, armed at the nearest one
 * pending across every link - see the deadline note in netmgr_policy.h for why
 * one timer rather than one per link per kind.
 *
 * It replaces the 500 ms LAN poll timer one for one, so netmgr still owns exactly
 * one tal_sw_timer and the module total stays at two (this one and the wifi
 * driver's reconnect timer). */
static TIMER_ID sg_netmgr_deadline_timer = NULL;

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
 *   - the shared deadline timer callback, which runs on the tal_sw_timer thread
 *     and cannot be joined by tal_sw_timer_delete(). This used to be the LAN poll
 *     timer callback; the hazard is identical and so is the guard.
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

/* The replacement ranking hook and its context.
 *
 * netmgr_policy.h declares netmgr_policy_select_cb_set() but netmgr_policy.c
 * deliberately does not define it, and its file comment says why: the hook is
 * invoked under s_netmgr.lock and its answer has to be VALIDATED against the live
 * candidate set before it is honoured, so the pointer belongs with the code that
 * does both. Kept outside s_netmgr because it must survive netmgr_deinit()'s
 * memset - a product installs it once, before netmgr_init(), the same way it
 * installs a registry table. */
static netmgr_policy_select_cb_t sg_select_cb  = NULL;
static void                     *sg_select_ctx = NULL;

/* The reachability backend, and whether a product chose one.
 *
 * Two variables rather than one because NULL is a legitimate CHOICE:
 * netmgr_probe.h says passing NULL to netmgr_probe_backend_set() selects "no
 * probing at all", which is distinct from "nobody called it", where netmgr_init()
 * installs netmgr_probe_backend_mqtt. A single pointer cannot express both.
 *
 * netmgr_probe_backend_set() is implemented here and not in netmgr_probe.c
 * because its contract is "rejected after netmgr_init()", and s_netmgr.inited is
 * the only thing that knows. netmgr_probe.c owns exactly one symbol of this
 * header, netmgr_probe_backend_mqtt; everything else in netmgr_probe.h touches
 * s_netmgr and is therefore below. */
static const netmgr_probe_backend_t *sg_probe_backend        = NULL;
static BOOL_T                        sg_probe_backend_chosen = FALSE;

/** The backend actually started, so deinit stops the one init started. */
static const netmgr_probe_backend_t *sg_probe_running = NULL;

static void __netmgr_notify_work(void *data);

static OPERATE_RET __netmgr_notify_post(MUTEX_HANDLE lock);

/**
 * @brief The monotonic millisecond base every deadline in this file shares.
 *
 * tal_system_get_millisecond() is tick-derived on every platform in the tree, so
 * it is monotonic and unaffected by an SNTP step - which matters, because
 * tal_sw_timer.c computes its own expiry from tal_time_get_system_time(), the
 * WALL clock. Deadlines are therefore compared here and only ever handed to
 * tal_sw_timer_start() as a duration, never as an absolute time.
 *
 * Truncated to 32 bits deliberately: SYS_TIME_T is 64-bit on some ports and
 * 32-bit on others, and netmgr_policy.h fixes the shared base at uint32_t with
 * wrap-correct comparisons (see __eligible_in_ms() and __netmgr_retry_reached()).
 * Taking the low half on every port is what makes the two agree.
 *
 * @return the current time in milliseconds, never 0 - see __netmgr_stamp()
 */
static uint32_t __netmgr_now_ms(void)
{
    return (uint32_t)tal_system_get_millisecond();
}

/**
 * @brief Make a timestamp storable in a field whose 0 means "not armed".
 *
 * Every deadline field in netmgr_report_t uses 0 as the unarmed sentinel, and the
 * clock legitimately reads 0 once per wrap (and at boot on some ports). Nudging
 * by one millisecond costs nothing and removes the case.
 */
static uint32_t __netmgr_stamp(uint32_t ms)
{
    return (0 == ms) ? 1 : ms;
}

/**
 * @brief Milliseconds until @a deadline_ms, 0 when unarmed or already reached.
 *
 * The same three-way contract netmgr_retry_remain_ms() states, so the caller
 * folds every deadline in the module with one rule: "ignore zero, take the
 * minimum of the rest". Signed difference, so it is correct across the 49.7-day
 * wrap of the base above.
 */
static uint32_t __netmgr_remain_ms(uint32_t deadline_ms, uint32_t now_ms)
{
    int32_t remain = 0;

    if (0 == deadline_ms) {
        return 0;
    }

    remain = (int32_t)(deadline_ms - now_ms);

    return (remain > 0) ? (uint32_t)remain : 0;
}

/**
 * @brief Fold one deadline into the pass minimum, ignoring the unarmed 0.
 */
static void __netmgr_fold(uint32_t *next_ms, uint32_t remain_ms)
{
    if (0 == remain_ms) {
        return;
    }

    if (0 == *next_ms || remain_ms < *next_ms) {
        *next_ms = remain_ms;
    }
}

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

/* The LAN gate, and the two layers of the old one
 * ===============================================
 * M2 decided whether to run the LAN service in two places, and both were wrong.
 *
 * Layer one was in netmgr_init():
 *
 *     #if !defined(ENABLE_CELLULAR) || (ENABLE_CELLULAR == 0)
 *         tal_sw_timer_create(__tuya_lan_init_tm_cb, NULL, &sg_lan_init_timer);
 *         tal_sw_timer_start(sg_lan_init_timer, 500, TAL_TIMER_CYCLE);
 *     #endif
 *
 * a COMPILE-time switch on whether the image contains a cellular driver. So a
 * wifi+4G build lost LAN on its wifi link as well, which is not what anyone meant
 * - it is what happens when a per-link question is answered per-image. Deleted.
 *
 * Layer two was inside the callback: `type & NETCONN_WIRED || type & NETCONN_WIFI`
 * where `type` is s_netmgr.type, the CONFIGURED mask netmgr_init() was called
 * with. On a wifi+4G board that mask holds both bits permanently, so the test
 * answered "yes, LAN" whichever link was actually carrying traffic. The question
 * is about the ACTIVE link, and it is now asked of the active link's descriptor:
 * netconn_registry_find(active)->caps & NETCONN_CAP_LAN.
 *
 * The 500 ms poll is gone with them. The gate is evaluated at the end of every
 * reselect, where `active` and the status have just been decided, plus once per
 * EVENT_MQTT_CONNECTED - which is the trigger for the one input that changes with
 * no link event behind it, client->is_activated. A first-time device provisions,
 * activates, connects MQTT, and that event brings us back here with is_activated
 * finally TRUE.
 *
 * What this deliberately does NOT do: stop LAN when the route moves to a link
 * without NETCONN_CAP_LAN, and restart it on the way back. Three independent
 * reasons, any one of them sufficient:
 *
 *   - tuya_lan_disable() blocks for up to 3000 ms waiting for LAN's socket loop
 *     thread to wind down (the tuya_sock_loop_is_inited() poll in tuya_lan.c).
 *     Called from here it would park the system work queue for that long and blow
 *     netmgr_deinit()'s 2000 ms drain budget;
 *   - it is not needed for correctness. Both LAN server sockets bind to the
 *     wildcard address, and LAN reads the active address per packet through
 *     netmgr_conn_get(), so a link switch heals itself;
 *   - a stop/restart cycle can leave LAN inert until reboot. When that 3000 ms
 *     wait times out, tuya_lan_disable() deliberately leaks s_lan_mgr rather than
 *     free it under a live reader callback, and tuya_lan_init()'s
 *     `if (s_lan_mgr) return OPRT_OK;` guard then makes every later restart
 *     "succeed" without registering a socket. That path is survivable as a rare
 *     abnormality; driving it once per link switch would make it routine.
 *
 * A fourth reason used to head this list and no longer holds. While the socket
 * loop was a process-wide singleton, tuya_lan_disable() closed reader fds out
 * from under the AI monitor, which shared the loop with no reference count, and
 * the monitor then dereferenced a NULL g_sloop - known_gaps.md section 3. Each
 * owner now creates its own loop via tuya_sock_loop_create(), so that crash is
 * gone, and with it the old fourth reason that stopping LAN was "broken TODAY".
 * Stopping is no longer a crash; it is still the three things above.
 *
 * So the gate governs STARTING only. A device that boots on cellular never opens
 * the LAN ports at all, which is the whole of what the `#if` was reaching for.
 */

/**
 * @brief Start the LAN service if the active link is one it belongs on.
 *
 * @note Must be called with s_netmgr.lock RELEASED. It takes the lock itself for
 *       the two snapshots and drops it around tuya_lan_init(), which is heavy and
 *       reaches into the cloud layer - the same rule as conn->open().
 *
 * @param[in] lock the caller's snapshot of s_netmgr.lock, already established live
 */
static void __netmgr_lan_gate(MUTEX_HANDLE lock)
{
    const netconn_desc_t *desc   = NULL;
    netmgr_type_e         active = NETCONN_AUTO;
    netmgr_status_e       status = NETMGR_LINK_DOWN;
    tuya_iot_client_t    *client = NULL;

    tal_mutex_lock(lock);
    if (s_netmgr.stopping || s_netmgr.lan_started) {
        tal_mutex_unlock(lock);
        return;
    }
    active = s_netmgr.active;
    status = s_netmgr.status;
    tal_mutex_unlock(lock);

    // NETMGR_LINK_UP covers NETMGR_LINK_STATE_DEGRADED, and that is correct
    // rather than sloppy: a link whose cloud is unreachable is exactly the link
    // whose LAN path is worth having.
    if (NETMGR_LINK_UP != status) {
        return;
    }

    // Layer two, replaced. NULL means the active link has no descriptor, which a
    // registered link cannot have, so it is treated as "no LAN" rather than as an
    // implicit yes.
    desc = netconn_registry_find(active);
    if (NULL == desc || 0 == (desc->caps & NETCONN_CAP_LAN)) {
        return;
    }

    client = tuya_iot_client_get();
    if (NULL == client || !client->is_activated) {
        // Not an error and not a permanent answer: EVENT_MQTT_CONNECTED brings us
        // back once activation has happened.
        return;
    }

    // Marked before the call, so a second pass cannot start LAN twice, and rolled
    // back on failure so a later link change retries. Safe as a
    // test-set-then-act because every caller is the one WORKQ_SYSTEM context.
    tal_mutex_lock(lock);
    if (s_netmgr.lan_started) {
        tal_mutex_unlock(lock);
        return;
    }
    s_netmgr.lan_started = TRUE;
    tal_mutex_unlock(lock);

    PR_DEBUG("Start LAN initialization on [%s]", desc->name);
    if (OPRT_OK != tuya_lan_init(client)) {
        tal_mutex_lock(lock);
        s_netmgr.lan_started = FALSE;
        tal_mutex_unlock(lock);
        PR_ERR("netmgr LAN init failed on [%s], retrying at the next link change", desc->name);
    }
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

/* __get_netmgr_status() used to live here: it read one link's status through
 * conn->get(NETCONN_CMD_STATUS) after screening it against s_netmgr.type. Both of
 * its callers are gone. The state machine below reads every link's driver status
 * once per pass while it is already walking them, so a per-link lookup that walks
 * the list again is pure cost, and the screen it did - "is this type in the
 * configured mask" - was unreachable for a registered link, because registration
 * only ever happens for a type the mask selected. */

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

    route->provider = p_conn->provider;

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

    /* OPRT_NOT_SUPPORTED here means the route named a socket backend this build
     * does not contain - a board set netconn_desc_t.provider to something other
     * than TAL_NET_PROVIDER_DEFAULT. The data plane refuses it, so the previous
     * route stays installed and sockets keep working; without this line the
     * board author would be left with a link that ranks fine and traffic that
     * does not move, and nothing naming the reason. */
    if (OPRT_NOT_SUPPORTED == tal_net_route_set(route)) {
        PR_ERR("netmgr route rejected: provider %u has no backend in this build, link [%s] not routable",
               (unsigned int)route->provider, (NULL != conn) ? __netmgr_link_name(conn->type) : "?");
    }
}

/***********************************************************
******************** link state machine ********************
***********************************************************/

/* What replaced __get_active_conn()
 * ================================
 * Selection used to be "the first link in list order that is up", with the order
 * baked into the container by an insertion sort that ran once at registration.
 * netmgr_policy.h lists the three defects that follow from that; all three are
 * fixed by the same move, which is that ranking now happens from a SNAPSHOT built
 * fresh on every pass:
 *
 *   1. __netmgr_link_step() advances one link's netmgr_link_state_e from what its
 *      driver reports plus what the probe accumulator holds plus which deadlines
 *      have expired. This is the only place any of those three become a state;
 *   2. the pass fills a netmgr_link_view_t per link, in registration order, so
 *      reg_index is the array index and needs no storage of its own;
 *   3. netmgr_policy_select() names the winner from that array. conn->pri is read
 *      per pass, which is the whole of the NETCONN_CMD_PRI fix - there is no
 *      sorted structure left to disagree with it.
 *
 * Everything below runs under s_netmgr.lock, and every part of it is either
 * arithmetic or the one bounded conn->get(NETCONN_CMD_STATUS) the locking
 * contract carves out.
 */

/**
 * @brief Which revalidation table is in force, sentinel resolved.
 *
 * NETMGR_POLICY_DEFAULT_INIT sets `.revalidate = {NULL, 0}`, and read literally
 * that is a table of no entries, which netmgr_policy.h says means "a DEGRADED
 * link is never re-verified". Taken together those two make the DEFAULT policy
 * one under which a link that is demoted once can never come back - a wifi link
 * behind an AP that recovers stays DEGRADED for the life of the boot.
 *
 * That is not what either header intends: netmgr_policy_t.revalidate documents
 * its default as "netmgr_retry_table_revalidate, {30, 60, 120, 300, 600}
 * seconds". So the sentinel is resolved HERE, at the point of use:
 *
 *   entry == NULL            -> the built-in netmgr_retry_table_revalidate
 *   entry != NULL, count > 0 -> the product's table
 *   entry != NULL, count = 0 -> never re-verify, which stays expressible
 *
 * A product that really wants revalidation off therefore points entry at
 * anything non-NULL and sets count to 0. The wording in both headers was
 * corrected to say so.
 */
static const netmgr_retry_table_t *__netmgr_reval_table(const netmgr_policy_t *pol)
{
    return (NULL == pol->revalidate.entry) ? &netmgr_retry_table_revalidate : &pol->revalidate;
}

/**
 * @brief Arm the next revalidation deadline for a link that just went DEGRADED.
 *
 * Wraps netmgr_retry_fail() for one reason, and it is a deliberate divergence
 * from that function rather than a workaround. netmgr_retry.c arms an EMPTY table
 * at `now`, so netmgr_retry_due() answers TRUE on the next poll - correct for its
 * other consumer, the wifi association back-off, where "no table" has to mean
 * "retry immediately" or a driver with no table would never redial. For
 * REVALIDATION the same value means the opposite: netmgr_policy_t.revalidate
 * documents count 0 as "never re-verified", and arming at `now` would promote the
 * link straight back out of DEGRADED on the next pass, i.e. switch demotion off
 * altogether.
 *
 * Two consumers, two readings of count 0, so the difference lives in the
 * consumer. Leaving the context unarmed is exactly "never": netmgr_retry_due()
 * answers FALSE on a zero deadline, forever.
 */
static void __netmgr_reval_arm(netmgr_retry_t *reval, const netmgr_retry_table_t *table, uint32_t now_ms)
{
    if (NULL == table->entry || 0 == table->count) {
        netmgr_retry_reset(reval);
        return;
    }

    (void)netmgr_retry_fail(reval, now_ms);
}

/**
 * @brief How strongly a reason explains a pass; lower wins.
 *
 * netmgr_event.h says the reason reported is "the one that changed the OUTCOME",
 * and a coalesced pass routinely has several inputs. This is that tie-break,
 * written down once so it is reviewable rather than emerging from the order of a
 * chain of ifs:
 *
 *   an explicit operator action  >  evidence about a link  >  configuration
 *     >  what a driver observed  >  a deadline expiring  >  the address moving
 *
 * "Configuration" is the two reasons a caller CAUSED without observing anything:
 * NETMGR_CHG_REASON_PRI_CHANGED, which netmgr detects by comparison, and
 * NETMGR_CHG_REASON_POLICY, which arrives through netmgr_reselect_request().
 *
 * NETMGR_CHG_REASON_INIT and _DEINIT are not ranked: they are facts about the
 * CALL rather than about a link, so netmgr_init() and netmgr_deinit() force them.
 */
static uint8_t __netmgr_reason_rank(netmgr_change_reason_e reason)
{
    switch (reason) {
    case NETMGR_CHG_REASON_PINNED:
    case NETMGR_CHG_REASON_UNPINNED:
        return 1;
    case NETMGR_CHG_REASON_PROBE_BAD:
        return 2;
    case NETMGR_CHG_REASON_PROBE_TIMEOUT:
        return 3;
    case NETMGR_CHG_REASON_PROBE_GOOD:
        return 4;
    case NETMGR_CHG_REASON_REVALIDATE:
        return 5;
    case NETMGR_CHG_REASON_PRI_CHANGED:
        return 6;
    case NETMGR_CHG_REASON_POLICY:
        return 7;
    case NETMGR_CHG_REASON_LINK_DOWN:
        return 8;
    case NETMGR_CHG_REASON_LINK_UP:
        return 9;
    case NETMGR_CHG_REASON_GRACE:
        return 10;
    case NETMGR_CHG_REASON_DEBOUNCE:
        return 11;
    case NETMGR_CHG_REASON_DWELL:
        return 12;
    case NETMGR_CHG_REASON_ADDR_CHANGED:
        return 13;
    default:
        return 0xFF;
    }
}

/**
 * @brief Keep @a cand as the pass's reason if it outranks what is there.
 */
static void __netmgr_reason_take(netmgr_change_reason_e *reason, netmgr_type_e *subject, netmgr_change_reason_e cand,
                                 netmgr_type_e cand_subject)
{
    if (__netmgr_reason_rank(cand) < __netmgr_reason_rank(*reason)) {
        *reason  = cand;
        *subject = cand_subject;
    }
}

/**
 * @brief Advance one link's state machine by one pass.
 *
 * @note Caller must hold s_netmgr.lock. Reaches conn->get(NETCONN_CMD_STATUS),
 *       which every driver answers from its cached base.status - the one bounded
 *       carve-out the locking contract allows.
 *
 * Two of the six netmgr_link_state_e values are NOT entered here, and that is a
 * property of the TAL rather than an omission:
 *
 *   - NETMGR_LINK_STATE_CONNECTING needs netmgr to observe a MANAGED driver
 *     BEGIN an attempt. No driver reports that - netconn_wifi.c calls
 *     base.event_cb() only on WFE_CONNECTED and on failure - and inventing a
 *     report verb would mean editing netconn_wifi.h, which is on the global
 *     public include path;
 *   - NETMGR_LINK_STATE_BACKOFF needs netmgr to own the retry deadline for a
 *     link that is DOWN. It does not: wifi keeps its own conn.timer, and OBSERVE
 *     and SUSTAINED links have nothing to retry (netconn_registry.h). The
 *     revalidation deadline is NOT this state, because a DEGRADED link is up at
 *     L3 and BACKOFF maps to NETMGR_LINK_DOWN, which is exactly the mapping the
 *     DEGRADED note forbids.
 *
 * Both stay in the enum: they are correctly handled everywhere (both fail
 * NETMGR_LINK_STATE_IS_UP and both map to NETMGR_LINK_DOWN), so a later change
 * that gives a driver the missing verb needs no edit outside this function.
 *
 * @param[in,out] slot    the link, indexed by registration order
 * @param[in]     pol     the policy in force
 * @param[in]     active  the link that HAS been active, i.e. before this pass
 * @param[in]     now_ms  the pass timestamp, shared by every link
 * @param[in,out] reason  the pass reason, folded by rank
 * @param[in,out] subject the link @a reason is about
 */
static void __netmgr_link_step(netmgr_report_t *slot, const netmgr_policy_t *pol, netmgr_type_e active, uint32_t now_ms,
                               netmgr_change_reason_e *reason, netmgr_type_e *subject)
{
    netmgr_conn_base_t *conn      = (NULL != slot->desc) ? slot->desc->conn : NULL;
    netmgr_status_e     drv       = NETMGR_LINK_DOWN;
    BOOL_T              was_up    = NETMGR_LINK_STATE_IS_UP(slot->state);
    BOOL_T              held      = FALSE;
    uint8_t             threshold = 0;

    if (NULL != conn && NULL != conn->get) {
        conn->get(NETCONN_CMD_STATUS, &drv);
    }

    // A priority change is named rather than inferred. last_pri is seeded at
    // registration, so the first pass never reports one.
    if (NULL != conn && conn->pri != slot->last_pri) {
        slot->last_pri = conn->pri;
        __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_PRI_CHANGED, slot->type);
    }

    if (NETMGR_LINK_UP == drv) {
        slot->grace_at_ms = 0;

        if (!was_up) {
            // A fresh up-streak. Everything the previous streak accumulated is
            // evidence about an association that no longer exists, so it goes:
            // a link that reassociates starts from UNVERIFIED with a clean
            // counter and a back-off at the top of its table.
            slot->state           = NETMGR_LINK_STATE_UNVERIFIED;
            slot->eligible_at_ms  = (0 != pol->up_debounce_ms) ? __netmgr_stamp(now_ms + pol->up_debounce_ms) : 0;
            slot->verify_at_ms    = 0;
            slot->probe.last      = NETMGR_PROBE_UNKNOWN;
            slot->probe.source    = NETMGR_PROBE_SRC_NONE;
            slot->probe.bad_count = 0;
            netmgr_retry_reset(&slot->reval);
            __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_LINK_UP, slot->type);
        } else if (0 != slot->eligible_at_ms && 0 == __netmgr_remain_ms(slot->eligible_at_ms, now_ms)) {
            // up_debounce_ms ran out: the link ripens into a candidate.
            slot->eligible_at_ms = 0;
            __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_DEBOUNCE, slot->type);
        }
    } else if (was_up) {
        // down_grace_ms applies to the ACTIVE link only, because a standby link
        // that drops has no route and no MQTT session to protect. It is expressed
        // by holding the link's state UP for the duration, which is what keeps it
        // invisible to netmgr_policy_select_default() - by the time a snapshot
        // exists the grace has already had its whole effect.
        if (slot->type == active && 0 != pol->down_grace_ms) {
            if (0 == slot->grace_at_ms) {
                slot->grace_at_ms = __netmgr_stamp(now_ms + pol->down_grace_ms);
            }

            if (0 != __netmgr_remain_ms(slot->grace_at_ms, now_ms)) {
                held = TRUE;
            } else {
                slot->grace_at_ms = 0;
                __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_GRACE, slot->type);
            }
        }

        if (!held) {
            slot->state          = NETMGR_LINK_STATE_DOWN;
            slot->eligible_at_ms = 0;
            slot->verify_at_ms   = 0;
            slot->grace_at_ms    = 0;
            netmgr_retry_reset(&slot->reval);
            __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_LINK_DOWN, slot->type);
        }
    } else {
        // Down and already known to be down. Nothing to report and no deadline
        // to keep: netmgr has no verb to dial this link with, whatever its
        // control level, so there is nothing to wait for.
        slot->state       = NETMGR_LINK_STATE_DOWN;
        slot->grace_at_ms = 0;
    }

    if (!NETMGR_LINK_STATE_IS_UP(slot->state)) {
        return;
    }

    if (!pol->probe_enable) {
        // Verdicts are dropped and DEGRADED is unreachable. Promoting a link that
        // an earlier policy demoted is part of that: a state must not outlive the
        // flag that produced it, or turning probing off would leave a permanently
        // demoted link behind with nothing able to clear it.
        if (NETMGR_LINK_STATE_DEGRADED == slot->state) {
            slot->state = NETMGR_LINK_STATE_UNVERIFIED;
        }
        slot->verify_at_ms = 0;
        return;
    }

    /* The documented minimum, enforced. netmgr_policy.h requires this to be
     * greater than 1 and nothing was holding it to that: netmgr_policy_set()
     * stores the field verbatim, and the old mapping here turned an unset 0 into
     * exactly the value the requirement forbids.
     *
     * It matters because netmgr_probe.h's survivability argument rests on it - a
     * mis-attributed BAD is tolerable only because it costs at most one increment
     * out of several. At a threshold of 1 a single false BAD demotes a healthy
     * link, and the false BADs are enumerated rather than hypothetical: three
     * deliberate tuya_mqtt_stop() call sites publish EVENT_MQTT_DISCONNECTED with
     * a current epoch, so they survive the staleness check.
     *
     * Clamped here and not in netmgr_policy_set() because that module holds no
     * state and validates nothing against live data, and because clamping there
     * would make netmgr_policy_get() hand back something the product never set. */
    threshold = (pol->probe_bad_threshold < NETMGR_PROBE_BAD_THRESHOLD_MIN) ? NETMGR_PROBE_BAD_THRESHOLD_MIN
                                                                            : pol->probe_bad_threshold;

    if (NETMGR_PROBE_GOOD == slot->probe.last) {
        // Believed immediately and unconditionally - see NETMGR_PROBE_GOOD. One
        // GOOD also forgets the whole back-off streak, so a link that recovers
        // and fails again waits 30 s and not 600 s.
        if (NETMGR_LINK_STATE_ONLINE != slot->state) {
            slot->state = NETMGR_LINK_STATE_ONLINE;
            __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_PROBE_GOOD, slot->type);
        }
        netmgr_retry_reset(&slot->reval);
    } else if (NETMGR_LINK_STATE_DEGRADED == slot->state) {
        // Revalidation. Promoting the link back to UNVERIFIED is what makes it
        // win ranking again, which moves the route, which makes tuya_iot
        // reconnect - so it is rate limited by the table.
        //
        // Note what is NOT done here: the next deadline is not armed. Arming is
        // the DEMOTION's job, and doing it in both places would advance the
        // attempt counter twice per revalidation cycle - the sequence would come
        // out 30, 120, 300, 600 and skip every other entry of
        // {30, 60, 120, 300, 600}. The stale deadline left behind is harmless:
        // netmgr_retry_remain_ms() reports an already-reached deadline as 0 so it
        // contributes nothing to the shared timer, netmgr_retry_due() is only
        // consulted in this DEGRADED branch which the link has just left, and the
        // next demotion re-arms from the attempt counter this preserves.
        if (netmgr_retry_due(&slot->reval, now_ms)) {
            slot->state           = NETMGR_LINK_STATE_UNVERIFIED;
            slot->probe.last      = NETMGR_PROBE_UNKNOWN;
            slot->probe.source    = NETMGR_PROBE_SRC_NONE;
            slot->probe.bad_count = 0;
            slot->verify_at_ms    = 0;
            __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_REVALIDATE, slot->type);
        }
    } else if (slot->probe.bad_count >= threshold) {
        slot->state = NETMGR_LINK_STATE_DEGRADED;
        // Bound once, and re-bound only when the policy's table actually changed,
        // so the attempt counter survives a demote/promote cycle. That is what
        // makes {30, 60, 120, 300, 600} a growing sequence rather than a 30 s
        // loop.
        if (slot->reval.table.entry != __netmgr_reval_table(pol)->entry ||
            slot->reval.table.count != __netmgr_reval_table(pol)->count) {
            netmgr_retry_bind(&slot->reval, __netmgr_reval_table(pol));
        }
        __netmgr_reval_arm(&slot->reval, __netmgr_reval_table(pol), now_ms);
        PR_WARN("netmgr link [%s] degraded, %d bad from %s", __netmgr_link_name(slot->type), slot->probe.bad_count,
                NETMGR_PROBE_SRC_TO_STR(slot->probe.source));
        __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_PROBE_BAD, slot->type);
    } else if (NETMGR_LINK_STATE_ONLINE == slot->state && NETMGR_PROBE_BAD == slot->probe.last) {
        // A BAD against a link that was already ONLINE demotes it to UNVERIFIED -
        // NOT to DEGRADED. The distinction is the whole point, so it is worth
        // stating both halves.
        //
        // Why this path has to exist at all: the case the contracts describe is a
        // link that was NEVER verified, and verify_timeout_ms covers it. The more
        // common field case is a link that WAS verified and whose WAN then died,
        // and nothing covered that. Trace it: one BAD arrives, the count is 1 and
        // probe_bad_threshold is 3, so no demotion; every later reconnect ATTEMPT
        // is silent, because mqtt_client_connect() closes its transporter and
        // returns without calling on_disconnected (mqtt_client_wrapper.c:218-223);
        // and verify_timeout_ms does not apply, because the link is ONLINE rather
        // than UNVERIFIED. So the link sat at ONLINE forever and the 4G bearer
        // never took over - M3's headline feature, missing in the direction that
        // matters most.
        //
        // Why UNVERIFIED is not a demotion: UNVERIFIED and ONLINE share the
        // NOT-SUSPECT tier (see NETMGR_LINK_STATE_UNVERIFIED on why separating
        // them would deadlock a passive probe), so this move cannot change the
        // ranking, cannot move the route and publishes nothing. All it does is
        // restart the verification clock, which __netmgr_settle() re-arms for the
        // active link below. If a GOOD arrives inside verify_timeout_ms the link
        // returns to ONLINE and nothing happened; if none does, the timeout
        // demotes it properly, with NETMGR_PROBE_SRC_TIMEOUT and
        // NETMGR_CHG_REASON_PROBE_TIMEOUT naming the cause.
        //
        // It also uses no new state and no new knob, which is why it is preferred
        // over teaching the threshold to survive an intervening GOOD.
        slot->state = NETMGR_LINK_STATE_UNVERIFIED;
        PR_DEBUG("netmgr link [%s] unverified again, bad from %s", __netmgr_link_name(slot->type),
                 NETMGR_PROBE_SRC_TO_STR(slot->probe.source));
    }

    // verify_timeout_ms, checked only for the link that has BEEN active: a
    // passive backend can only ever observe the active link, so a standby link
    // staying UNVERIFIED forever is correct rather than a gap. The deadline
    // itself is seeded by __netmgr_settle() once the pass has chosen an active
    // link, which is why only the expiry is here.
    if (slot->type == active && NETMGR_LINK_STATE_UNVERIFIED == slot->state && 0 != slot->verify_at_ms &&
        0 == __netmgr_remain_ms(slot->verify_at_ms, now_ms)) {
        slot->verify_at_ms = 0;
        slot->probe.last   = NETMGR_PROBE_BAD;
        slot->probe.source = NETMGR_PROBE_SRC_TIMEOUT;
        if (slot->probe.bad_total < 0xFFFF) {
            slot->probe.bad_total++;
        }
        slot->state = NETMGR_LINK_STATE_DEGRADED;
        if (slot->reval.table.entry != __netmgr_reval_table(pol)->entry ||
            slot->reval.table.count != __netmgr_reval_table(pol)->count) {
            netmgr_retry_bind(&slot->reval, __netmgr_reval_table(pol));
        }
        __netmgr_reval_arm(&slot->reval, __netmgr_reval_table(pol), now_ms);
        PR_WARN("netmgr link [%s] degraded, no verdict in %dms", __netmgr_link_name(slot->type),
                pol->verify_timeout_ms);
        __netmgr_reason_take(reason, subject, NETMGR_CHG_REASON_PROBE_TIMEOUT, slot->type);
    }
}

/**
 * @brief The link M2 would have called active when nothing at all is up.
 *
 * A neutrality shim, and it exists because one sentence in netmgr_policy.h is
 * wrong about the code it describes. netmgr_select_out_t.choice says NETCONN_AUTO
 * "is what s_netmgr.active already holds when nothing is up". It is not.
 * __get_active_conn() opened with
 *
 *     active_type = cur_conn->type;   // the HEAD of the priority-sorted list
 *
 * before its loop, and only returned NETCONN_AUTO when NOTHING was registered. So
 * on an M2 device with wifi registered and not yet associated, s_netmgr.active was
 * NETCONN_WIFI with status NETMGR_LINK_DOWN - which is the state every device is
 * in for the first seconds of every boot.
 *
 * Honouring NETCONN_AUTO there instead would change observable behaviour twice
 * over, and both would be regressions:
 *
 *   - netmgr_init() rejected NETCONN_AUTO with OPRT_INVALID_PARM, so every device
 *     that boots before its link associates would fail to initialise;
 *   - netmgr_conn_get(NETCONN_AUTO, ...) resolves AUTO through s_netmgr.active,
 *     and __get_conn_by_type(NETCONN_AUTO) is NULL, so the ~20 call sites that
 *     ask for the active link's status while offline would get OPRT_NOT_FOUND
 *     where they used to get OPRT_OK and NETMGR_LINK_DOWN. tuya_iot's
 *     network_check() is one of them.
 *
 * netmgr_policy_select_default() is right to answer NETCONN_AUTO - "no link can
 * carry traffic" is the truth, and the policy layer should not be told to lie -
 * so the M2 reading is reconstructed here instead, exactly: highest pri, ties to
 * the lowest reg_index, which for a registration-ordered array is the first
 * strict maximum. That is the head of the old sorted list, provably, because the
 * old insertion put a new link before the first strictly-lower priority and so
 * kept equals in arrival order.
 *
 * The status that goes with it is DOWN, from NETMGR_LINK_STATE_TO_STATUS, so the
 * link is nominal and nothing treats it as usable.
 */
static netmgr_type_e __netmgr_fallback_active(const netmgr_link_view_t *views, uint32_t count)
{
    const netmgr_link_view_t *best = NULL;
    uint32_t                  i    = 0;

    for (i = 0; i < count; i++) {
        if (NULL == best || views[i].pri > best->pri) {
            best = &views[i];
        }
    }

    return (NULL != best) ? best->type : NETCONN_AUTO;
}

void netmgr_policy_select(const netmgr_select_in_t *in, netmgr_select_out_t *out)
{
    uint32_t i     = 0;
    BOOL_T   valid = FALSE;

    if (NULL == out) {
        return;
    }

    // The contract with the hook: pre-initialised, so a hook that writes nothing
    // has answered "no link".
    out->choice     = NETCONN_AUTO;
    out->recheck_ms = 0;

    if (NULL == in) {
        return;
    }

    if (NULL == sg_select_cb) {
        netmgr_policy_select_default(in, out);
        return;
    }

    sg_select_cb(in, out, sg_select_ctx);

    // NETCONN_AUTO is a legitimate answer and is taken at face value: a hook is
    // entitled to say no link should carry traffic.
    if (NETCONN_AUTO == out->choice) {
        return;
    }

    // Everything else is VALIDATED, not trusted. A product hook must not be able
    // to route traffic over a link that is down or does not exist; the
    // eligibility floor is the one rule nothing overrides, the pin included, so a
    // hook does not get to override it either.
    for (i = 0; i < in->count && NULL != in->links; i++) {
        if (in->links[i].type == out->choice && NETMGR_LINK_STATE_IS_UP(in->links[i].state)) {
            valid = TRUE;
            break;
        }
    }

    if (!valid) {
        PR_ERR("netmgr policy hook chose [%s], which is not an eligible candidate; using the built-in ranking",
               __netmgr_link_name(out->choice));
        netmgr_policy_select_default(in, out);
    }
}

/**
 * @brief Run one whole pass: state machine, ranking, route, events, LAN, timer.
 *
 * The only place that writes s_netmgr.active / .status / .route_epoch once init is
 * done. Runs on WORKQ_SYSTEM from __netmgr_notify_work(), and once from
 * netmgr_init() on the caller's thread before any handler can exist, so it is
 * serialised with itself by construction.
 *
 * @note Must be called with s_netmgr.lock released.
 *
 * @param[in] lock   the caller's snapshot of s_netmgr.lock. Taken as a parameter
 *                   rather than re-read here: the caller has already established
 *                   that the handle is live (it raised notify_busy under it), and
 *                   re-reading a field netmgr_deinit() nulls would reintroduce a
 *                   window this function does not need to have.
 * @param[in] force  a reason that overrides the derived one and forces
 *                   EVENT_NETMGR_CHG, or NETMGR_CHG_REASON_NONE to derive it
 * @param[in] legacy FALSE to suppress EVENT_LINK_TYPE_CHG / EVENT_LINK_STATUS_CHG.
 *                   Only netmgr_init() passes FALSE, and only because M2's init
 *                   published neither: it assigned s_netmgr.active directly. The
 *                   one subscriber of EVENT_LINK_TYPE_CHG calls
 *                   tuya_iot_reconnect() synchronously, so publishing it from
 *                   inside netmgr_init() would be a new re-entry into a
 *                   half-built system.
 */
static void __netmgr_settle(MUTEX_HANDLE lock, netmgr_change_reason_e force, BOOL_T legacy)
{
    netmgr_link_view_t     views[NETMGR_LINK_MAX];
    netmgr_select_in_t     in          = {0};
    netmgr_select_out_t    out         = {NETCONN_AUTO, 0};
    netmgr_policy_t        pol         = NETMGR_POLICY_DEFAULT_INIT;
    netmgr_change_t        chg         = {0};
    netmgr_change_reason_e reason      = NETMGR_CHG_REASON_NONE;
    netmgr_change_reason_e req_reason  = NETMGR_CHG_REASON_NONE;
    netmgr_type_e          subject     = NETCONN_AUTO;
    netmgr_type_e          pin         = NETCONN_AUTO;
    netmgr_type_e          pub_active  = NETCONN_AUTO;
    netmgr_status_e        pub_status  = NETMGR_LINK_DOWN;
    netmgr_conn_base_t    *active_base = NULL;
    tal_net_route_t        route       = {.provider = TAL_NET_PROVIDER_DEFAULT, .src_ip = 0};
    BOOL_T                 type_chg    = FALSE;
    BOOL_T                 status_chg  = FALSE;
    BOOL_T                 addr_chg    = FALSE;
    BOOL_T                 emit_switch = FALSE;
    BOOL_T                 missed       = FALSE;
    netmgr_type_e          skip_subject = NETCONN_AUTO;
    uint32_t               now_ms      = 0;
    uint32_t               next_ms     = 0;
    uint32_t               count       = 0;
    uint32_t               i           = 0;

    memset(views, 0, sizeof(views));

    // Take the exclusion before anything is read. A pass that cannot have the
    // flag must not sample state either, or it would publish a decision built
    // from a snapshot the owning pass is halfway through changing.
    tal_mutex_lock(lock);
    if (s_netmgr.settle_busy) {
        // The reason is not dropped, it is handed to the re-run. Without this a
        // skipped NETMGR_CHG_REASON_INIT would be reported as REASON_NONE.
        if (NETMGR_CHG_REASON_NONE != force) {
            __netmgr_reason_take(&s_netmgr.req_reason, &skip_subject, force, NETCONN_AUTO);
        }
        s_netmgr.settle_missed = TRUE;
        tal_mutex_unlock(lock);
        return;
    }
    s_netmgr.settle_busy = TRUE;
    tal_mutex_unlock(lock);

    // Step 1 of the route push described above: read what is installed before
    // taking the lock. The initialiser only covers a route_get() that fails.
    tal_net_route_get(&route);

    // Both are lock-free by construction in netmgr_policy.c, so they could equally
    // be called from inside the lock; they are read here so the whole pass sees
    // one consistent policy and one consistent pin, whatever else is happening.
    (void)netmgr_policy_get(&pol);
    (void)netmgr_policy_pin_get(&pin);

    // One timestamp for the whole pass. Reading the clock per link would let two
    // links compare their deadlines against different `now`s.
    now_ms = __netmgr_now_ms();

    tal_mutex_lock(lock);

    count          = s_netmgr.link_num;
    chg.old_active = s_netmgr.active;
    chg.old_status = s_netmgr.status;

    // Consume whatever netmgr_reselect_request() left, and clear it here rather
    // than after the fold: a request arriving from now on belongs to the NEXT
    // pass, not to one that has already sampled the state it is about. Held in a
    // local because it is only folded in at the end, once it is known whether the
    // pass changed anything - see below.
    req_reason          = s_netmgr.req_reason;
    s_netmgr.req_reason = NETMGR_CHG_REASON_NONE;

    // A pin change is an explicit operator action, so it outranks everything the
    // pass observes. netmgr_policy.c holds the pin and has no way to tell netmgr
    // it moved, so netmgr notices by comparison instead.
    if (pin != s_netmgr.last_pin) {
        __netmgr_reason_take(&reason, &subject,
                             (NETCONN_AUTO == pin) ? NETMGR_CHG_REASON_UNPINNED : NETMGR_CHG_REASON_PINNED, pin);
        s_netmgr.last_pin = pin;
    }

    for (i = 0; i < count; i++) {
        netmgr_report_t    *slot = &s_netmgr.report[i];
        netmgr_conn_base_t *conn = (NULL != slot->desc) ? slot->desc->conn : NULL;

        __netmgr_link_step(slot, &pol, chg.old_active, now_ms, &reason, &subject);

        views[i].type = slot->type;
        // Read per pass. THE NETCONN_CMD_PRI fix: there is no cached order left
        // that can disagree with the live value.
        views[i].pri            = (NULL != conn) ? conn->pri : 0;
        views[i].reg_index      = i;
        views[i].state          = slot->state;
        views[i].caps           = (NULL != slot->desc) ? slot->desc->caps : NETCONN_CAP_NONE;
        views[i].ctrl           = (NULL != slot->desc) ? slot->desc->ctrl : NETCONN_CTRL_OBSERVE;
        views[i].eligible_at_ms = slot->eligible_at_ms;
    }

    in.links  = views;
    in.count  = count;
    in.active = chg.old_active;
    in.pinned = pin;
    in.now_ms = now_ms;
    // Never in the future, which is what lets netmgr_policy.c compute the dwell
    // with an unsigned difference.
    in.active_since_ms = (0 != s_netmgr.active_since_ms) ? s_netmgr.active_since_ms : now_ms;
    in.policy          = pol;

    netmgr_policy_select(&in, &out);

    if (NETCONN_AUTO == out.choice) {
        out.choice = __netmgr_fallback_active(views, count);
    }

    chg.new_active = out.choice;
    chg.new_state  = NETMGR_LINK_STATE_DOWN;
    for (i = 0; i < count; i++) {
        if (views[i].type == out.choice) {
            chg.new_state = views[i].state;
            break;
        }
    }
    chg.new_status = (NETCONN_AUTO == out.choice) ? NETMGR_LINK_DOWN : NETMGR_LINK_STATE_TO_STATUS(chg.new_state);

    // A handover is reported whatever emit_up_switch says - netmgr_event.h makes
    // this the recommended way to consume it, because it needs no legacy event to
    // change meaning.
    chg.handover =
        (NETMGR_LINK_UP == chg.old_status && NETMGR_LINK_UP == chg.new_status && chg.old_active != chg.new_active)
            ? TRUE
            : FALSE;

    if (chg.new_active != s_netmgr.active) {
        PR_DEBUG("netmgr conn type changed [%s] --> [%s]", __netmgr_link_name(s_netmgr.active),
                 __netmgr_link_name(chg.new_active));
        s_netmgr.active          = chg.new_active;
        s_netmgr.active_since_ms = __netmgr_stamp(now_ms);
        type_chg                 = TRUE;
    }
    if (chg.new_status != s_netmgr.status) {
        PR_DEBUG("netmgr conn status changed [%s] --> [%s]", NETMGR_STATUS_TO_STR(s_netmgr.status),
                 NETMGR_STATUS_TO_STR(chg.new_status));
        s_netmgr.status = chg.new_status;
        status_chg      = TRUE;
    }

    // A switch nothing above explains happened because a deferral expired, and
    // min_dwell_ms is the only deferral in the module that defers a switch rather
    // than a candidacy. Ranked last, so it never displaces a real cause.
    if (type_chg && 0 != pol.min_dwell_ms) {
        __netmgr_reason_take(&reason, &subject, NETMGR_CHG_REASON_DWELL, chg.new_active);
    }

    // verify_timeout_ms is rebased against the link this pass chose, not the one
    // the pass started with: a link that has just become active has not yet had
    // its chance to be verified.
    for (i = 0; i < count; i++) {
        netmgr_report_t *slot = &s_netmgr.report[i];

        if (slot->type == chg.new_active && NETMGR_LINK_STATE_UNVERIFIED == slot->state && pol.probe_enable &&
            0 != pol.verify_timeout_ms) {
            if (0 == slot->verify_at_ms) {
                slot->verify_at_ms = __netmgr_stamp(now_ms + pol.verify_timeout_ms);
            }
        } else {
            slot->verify_at_ms = 0;
        }

        // Every deadline in the module folds into one minimum, with 0 meaning
        // "contributes nothing" throughout - which is why an already-due deadline
        // contributes 0 too: the pass that would act on it is this one.
        __netmgr_fold(&next_ms, __netmgr_remain_ms(slot->eligible_at_ms, now_ms));
        __netmgr_fold(&next_ms, __netmgr_remain_ms(slot->grace_at_ms, now_ms));
        __netmgr_fold(&next_ms, __netmgr_remain_ms(slot->verify_at_ms, now_ms));
        __netmgr_fold(&next_ms, netmgr_retry_remain_ms(&slot->reval, now_ms));
    }

    // The ranking function's own deadline, which is how it gets timing without
    // owning a timer.
    __netmgr_fold(&next_ms, out.recheck_ms);

    // Step 2: snapshot the backend behind the active connection. Done whichever
    // branch above ran, or none of them, because the route is pushed
    // unconditionally below and its provider half always has to be filled in;
    // unless the active connection just changed, this is the value already
    // installed and the push is a no-op on that half.
    active_base = __netmgr_snap_provider(chg.new_active, &route);

    pub_active  = s_netmgr.active;
    pub_status  = s_netmgr.status;
    emit_switch = (pol.emit_up_switch && chg.handover) ? TRUE : FALSE;

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
    __netmgr_push_route(active_base, pub_status, &route);

    // The route generation of netmgr_probe.h, bumped only when the installed
    // value actually MOVED. That is what lets a late verdict be attributed to the
    // route that carried its traffic, and it is also the one way netmgr can see
    // NETMGR_CHG_REASON_ADDR_CHANGED - a same-link address change takes no branch
    // above and published nothing at all before M3.
    //
    // THE BUMP MUST HAPPEN BEFORE THE PUBLISHES BELOW, and this is load-bearing
    // rather than tidy. EVENT_LINK_TYPE_CHG reaches tuya_iot's
    // __tuya_iot_link_type_change_cb() (tuya_iot.c:870), which calls
    // tuya_iot_reconnect(), which calls tuya_mqtt_stop() (tuya_iot.c:1078), which
    // publishes EVENT_MQTT_DISCONNECTED - and the passive probe backend turns that
    // into a BAD. That BAD is about the OLD route, and it carries the epoch the
    // backend saved at its CONNACK, so it is discarded exactly when the counter
    // has already advanced past it. Bumping after the publish instead would hand
    // the stale verdict the NEW epoch, and every single link switch would then
    // book a false BAD against the link it had just switched to. Not a rare race:
    // that chain runs on every switch, synchronously, without fail.
    tal_mutex_lock(lock);
    if (route.provider != s_netmgr.route.provider || route.src_ip != s_netmgr.route.src_ip) {
        s_netmgr.route = route;
        s_netmgr.route_epoch++;
        if (0 == s_netmgr.route_epoch) {
            // 0 is reserved for NETMGR_PROBE_EPOCH_ANY.
            s_netmgr.route_epoch = 1;
        }
        addr_chg = TRUE;
    }
    chg.epoch  = s_netmgr.route_epoch;
    chg.src_ip = s_netmgr.route.src_ip;
    tal_mutex_unlock(lock);

    if (addr_chg) {
        __netmgr_reason_take(&reason, &subject, NETMGR_CHG_REASON_ADDR_CHANGED, pub_active);
    }

    // The requested reason, folded LAST and only when the pass actually changed
    // something. Both halves matter.
    //
    // Guarded, because netmgr_event.h defines NETMGR_CHG_REASON_POLICY as "a
    // policy whose ranking picks DIFFERENTLY" - a netmgr_policy_set() that changes
    // no outcome must publish nothing, or every start-up call that installs the
    // same values a board already had would emit a spurious change event.
    //
    // Folded rather than assigned, so a request racing a real link event loses to
    // it: if the pass that finally runs is the one where wifi went DEGRADED, the
    // reason is PROBE_BAD, not POLICY. The subject stays NETCONN_AUTO, which is
    // what netmgr_event.h specifies for a change not attributable to one link.
    if ((type_chg || status_chg || addr_chg) && NETMGR_CHG_REASON_NONE != req_reason) {
        __netmgr_reason_take(&reason, &subject, req_reason, NETCONN_AUTO);
    }

    // A type or status change with no named cause still has to say something, and
    // "a driver reported" is the honest fallback: every other reason in the enum
    // names a mechanism that demonstrably did not fire this pass.
    if (NETMGR_CHG_REASON_NONE == reason && (type_chg || status_chg)) {
        reason  = (NETMGR_LINK_UP == pub_status) ? NETMGR_CHG_REASON_LINK_UP : NETMGR_CHG_REASON_LINK_DOWN;
        subject = pub_active;
    }

    if (NETMGR_CHG_REASON_NONE != force) {
        reason  = force;
        subject = pub_active;
    }

    // Published from local copies, after the unlock: subscribers re-enter netmgr
    // synchronously (tuya_iot's __tuya_iot_link_type_change_cb calls
    // tuya_iot_reconnect()), and publishing under the lock would deadlock a
    // non-recursive mutex.
    //
    // Order is fixed and is part of netmgr_event.h's contract: the two legacy
    // events first, unchanged in name, payload and condition, then
    // EVENT_NETMGR_CHG in addition and never instead.
    if (legacy && type_chg) {
        tal_event_publish(EVENT_LINK_TYPE_CHG, (void *)&pub_active);
    }
    if (legacy && (status_chg || emit_switch)) {
        // NETMGR_LINK_UP_SWITCH is produced HERE and only here, as an event
        // payload, and is deliberately never stored in s_netmgr.status. Storing it
        // would make the next pass see UP != UP_SWITH and publish a spurious
        // change back to UP, and it would leak a third value into
        // netmgr_state_get() and into the LAN gate. netmgr_policy.h's aside that
        // s_netmgr.status "stays in {DOWN, UP, UP_SWITH}" is the one place that
        // reads otherwise; keeping the field two-valued is strictly safer and
        // changes nothing a subscriber sees.
        //
        // Note the shape of the condition: a handover alone leaves status_chg
        // FALSE (UP -> UP), so with emit_up_switch set this ADDS an event that
        // never existed. That is exactly why the flag defaults to FALSE.
        netmgr_status_e legacy_status = emit_switch ? NETMGR_LINK_UP_SWITCH : pub_status;

        tal_event_publish(EVENT_LINK_STATUS_CHG, (void *)&legacy_status);
    }
    if (NETMGR_CHG_REASON_NONE != reason) {
        chg.reason  = reason;
        chg.subject = subject;
        PR_DEBUG("netmgr change [%s] subject [%s] active [%s] --> [%s] epoch %d", NETMGR_CHG_REASON_TO_STR(chg.reason),
                 __netmgr_link_name(chg.subject), __netmgr_link_name(chg.old_active),
                 __netmgr_link_name(chg.new_active), chg.epoch);
        tal_event_publish(EVENT_NETMGR_CHG, (void *)&chg);
    }

    // The LAN gate, evaluated where the decision it depends on was just made.
    __netmgr_lan_gate(lock);

    // The one shared deadline, armed last and outside the lock. Never armed with
    // 0: tal_sw_timer_start() treats a 0 interval as "keep the previous one",
    // which is the quirk netmgr_retry.c documents the wifi driver having depended
    // on by accident. Stopping is the honest way to say "no deadline".
    if (NULL != sg_netmgr_deadline_timer) {
        if (0 != next_ms) {
            tal_sw_timer_start(sg_netmgr_deadline_timer, next_ms, TAL_TIMER_ONCE);
        } else {
            tal_sw_timer_stop(sg_netmgr_deadline_timer);
        }
    }

    // Release the exclusion, and give a skipped pass its re-run. Posting is safe
    // from here whichever thread this is: on the WORKQ_SYSTEM thread it queues the
    // next item, and from netmgr_init() it queues one the workq picks up.
    tal_mutex_lock(lock);
    s_netmgr.settle_busy   = FALSE;
    missed                 = s_netmgr.settle_missed;
    s_netmgr.settle_missed = FALSE;
    tal_mutex_unlock(lock);

    if (missed) {
        (void)__netmgr_notify_post(lock);
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
 *
 * M3 adds three more producers - the shared deadline timer, netmgr_probe_report()
 * and the EVENT_MQTT_CONNECTED subscriber - and every one of them goes through
 * __netmgr_notify_post() rather than scheduling for itself, so the coalescing
 * above still bounds the queue at one item however many of them fire at once. The
 * state machine still runs in exactly one context.
 *
 * That is also why tal_workq_init_delayed()/tal_workq_start_delayed() are NOT
 * used for the shared deadline, although they look like exactly the right tool.
 * __delayed_work_cb() (tal_workqueue.c:335-340) calls tal_workqueue_schedule()
 * directly, BYPASSING this coalescing entirely, so a flapping link would queue one
 * item per expiry and burn through the bounded 100 slots - the precise failure the
 * whole channel exists to prevent. It also allocates a DELAYED_WORK_T on the heap
 * and still creates a tal_sw_timer underneath, so it costs strictly more than the
 * timer it would replace. Verified against the implementation, not assumed.
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
 * @brief Ask for one pass of the state machine, coalescing with any pending one.
 *
 * The single entry to the channel, extracted so that the four producers cannot
 * drift apart: a driver report, the shared deadline expiring, a probe verdict and
 * EVENT_MQTT_CONNECTED all mean the same thing to netmgr - "re-evaluate" - and
 * none of them carries information the pass does not re-read for itself.
 *
 * @note Must be called with s_netmgr.lock RELEASED. Callers that also have to
 *       record something first (netmgr_notify_link() marks a report slot,
 *       netmgr_probe_report() accumulates a verdict) take the lock for that and
 *       drop it before calling here. Two acquisitions rather than one, which
 *       costs an uncontended lock/unlock on a path that runs on link events
 *       rather than on packets, and buys one implementation of the
 *       claim-schedule-rollback sequence instead of four.
 *
 * @param[in] lock the caller's snapshot of s_netmgr.lock, already established live
 *
 * @return OPRT_OK when a pass is queued or one already was. Others when the work
 *         queue refused it, which netmgr_notify_link() propagates to keep the
 *         contract netconn_registry.h states for it.
 */
static OPERATE_RET __netmgr_notify_post(MUTEX_HANDLE lock)
{
    BOOL_T      need_post = FALSE;
    OPERATE_RET rt        = OPRT_OK;

    tal_mutex_lock(lock);
    if (!s_netmgr.stopping && !s_netmgr.notify_queued) {
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
        // Drop the "queued" mark and keep whatever the caller recorded, so a later
        // producer can retry the post. report[i].pending stays set, and a pass
        // re-reads every link through conn->get() anyway, so no state is lost.
        tal_mutex_lock(lock);
        s_netmgr.notify_queued = FALSE;
        tal_mutex_unlock(lock);

        // But a later producer is not guaranteed to exist, and this used to be the
        // only recovery - the comment here claimed "nothing is lost", which was
        // true of the DATA and false of the pass that acts on it. A link that flaps
        // once and settles produces no further report, and under the shipped
        // default policy every timing is 0, so the settle pass stops the shared
        // deadline rather than arming it. Nothing would have run the pass again.
        //
        // So the timer is armed here as a second, independent recovery path. The
        // two cover each other: a producer may never come, and this arm may itself
        // fail. If a settle pass does run first it re-arms or stops the timer from
        // its own deadline fold, which is the correct outcome - the retry existed
        // only until a pass happened.
        if (NULL != sg_netmgr_deadline_timer) {
            tal_sw_timer_start(sg_netmgr_deadline_timer, NETMGR_NOTIFY_RETRY_MS, TAL_TIMER_ONCE);
        }

        PR_ERR("netmgr notify schedule failed, rt = %d, retrying in %d ms", rt, NETMGR_NOTIFY_RETRY_MS);
    }

    return rt;
}

/**
 * @brief The shared deadline fired: ask for a pass, and do nothing else.
 *
 * Runs on the tal_sw_timer "sys_timer" thread, which is NOT the WORKQ_SYSTEM
 * thread the state machine runs on, so this must add no concurrency source of its
 * own. It does exactly what the LAN poll callback it replaced did and nothing
 * more: check the gate, post the coalesced work item, return. Which deadline
 * expired is not recorded and does not need to be - the pass re-derives every
 * deadline from its own timestamp.
 *
 * netmgr_deinit() stops this timer - and RETAINS it, it does not delete it, for
 * the reason given at step 5 there - without being able to join a callback
 * already inside it. So the gate check is what makes losing that race a no-op,
 * and the retained mutex is what makes it harmless rather than fatal. Retaining
 * the timer removes the third leg of the same problem: a callback that survives
 * into teardown can no longer be arming freed memory.
 */
static void __netmgr_deadline_tm_cb(TIMER_ID timer_id, void *arg)
{
    MUTEX_HANDLE lock = s_netmgr.lock;

    (void)timer_id;
    (void)arg;

    if (sg_netmgr_gate_closed || NULL == lock) {
        return;
    }

    (void)__netmgr_notify_post(lock);
}

/**
 * @brief MQTT came up: re-evaluate, for the LAN gate's sake.
 *
 * The one input to the LAN gate that changes with no link event behind it is
 * client->is_activated, and this is how netmgr learns it moved. A first-time
 * device provisions, activates, then connects MQTT; by the time this fires,
 * is_activated is TRUE and the gate can open the LAN ports. It is what pays for
 * deleting the 500 ms poll timer.
 *
 * Runs on the tuya_iot_yield() thread, which is also the MQTT keepalive pump, so
 * it records nothing and evaluates nothing - same discipline as
 * netmgr_probe_report(), and for the same reason.
 *
 * Subscribed under the name "netmgr", which is distinct from the probe backend's
 * own subscription to the same event: tal_event keys a subscriber on
 * (name, desc, cb), so the two coexist.
 */
static OPERATE_RET __netmgr_mqtt_connected_cb(void *data)
{
    MUTEX_HANDLE lock = s_netmgr.lock;

    (void)data;

    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_OK;
    }

    (void)__netmgr_notify_post(lock);

    return OPRT_OK;
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

    __netmgr_settle(lock, NETMGR_CHG_REASON_NONE, TRUE);

    tal_mutex_lock(lock);
    if (s_netmgr.notify_busy > 0) {
        s_netmgr.notify_busy--;
    }
    tal_mutex_unlock(lock);
}

OPERATE_RET netmgr_notify_link(netmgr_type_e type, netmgr_status_e status)
{
    netmgr_report_t *slot = NULL;
    MUTEX_HANDLE     lock = s_netmgr.lock;

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

    tal_mutex_unlock(lock);

    // `pending` is set whether or not the post succeeds, so a failed schedule is
    // retried by the next producer rather than losing this report.
    return __netmgr_notify_post(lock);
}

OPERATE_RET netmgr_reselect_request(netmgr_change_reason_e reason)
{
    MUTEX_HANDLE  lock = s_netmgr.lock;
    netmgr_type_e sink = NETCONN_AUTO;

    // Gated exactly like netmgr_notify_link(), and for one case that is not
    // hypothetical: netmgr_deinit() releases the pin through
    // netmgr_policy_pin(NETCONN_AUTO), which now comes back through here. The gate
    // closed in step 0 of teardown is what stops that from queueing work into a
    // netmgr being dismantled.
    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_OK;
    }

    if (NETMGR_CHG_REASON_NONE != reason) {
        // Recorded for the pass to report, folded by rank so it cannot mask a real
        // link event. Kept even if the post below fails: the next producer's pass
        // will still name the right cause.
        tal_mutex_lock(lock);
        if (!s_netmgr.stopping) {
            // `sink` only exists because __netmgr_reason_take() folds a
            // (reason, subject) pair and the pending subject is always
            // NETCONN_AUTO here: netmgr_event.h says a change not attributable to
            // one link reports AUTO, and neither POLICY nor a bare re-evaluate is
            // about one link. The pin is the case that looks like a counterexample
            // and is not - the pass detects a moved pin by comparison and names
            // its own subject, which is why netmgr_policy_pin() requests with
            // NETMGR_CHG_REASON_NONE.
            __netmgr_reason_take(&s_netmgr.req_reason, &sink, reason, NETCONN_AUTO);
        }
        tal_mutex_unlock(lock);
    }

    return __netmgr_notify_post(lock);
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
 * @brief Register one link from its descriptor, at the TAIL of the list.
 *
 * The descriptor is the source of truth for priority and socket provider:
 * conn->pri and conn->provider are overwritten from it here, which is what
 * lets a board retune either without patching a driver.
 *
 * Why this appends, where M2 inserted by descending priority
 * ---------------------------------------------------------
 * Ordering is a policy decision and it does not belong in the container. M2's
 * insertion sort ran ONCE, here, and nothing ever re-sorted, which is the whole
 * of a confirmed defect: netmgr_conn_set(NETCONN_CMD_PRI) fires base.event_cb()
 * and so runs a pass, but the pass walked a list whose order predated the new
 * priority, so changing a priority at runtime could not change the route.
 * netconn_wifi_set()'s comment - "set pri will cause status change to reneg the
 * active connection" - described an intent the code did not implement. Ranking
 * now happens per pass from conn->pri, so there is no cached order left to go
 * stale, and the list is free to mean the simplest thing it can.
 *
 * The list therefore becomes REGISTRATION ORDER, which is the same order
 * s_netmgr.report[] is in, so netmgr_link_view_t.reg_index is just the array
 * index - no new storage, and M2's tie-break survives verbatim. That tie-break
 * matters: netconn_table.c records that its row order is the pre-M2 registration
 * order and that preserving it is what makes the refactor provably rather than
 * probably behaviour neutral. Equal-priority links still come out in registration
 * order, and now they do so because __ranks_above() says so rather than because
 * an insertion loop happened to be stable.
 *
 * @param[in] desc the registry row to register
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
static OPERATE_RET __netmgr_conn_register(const netconn_desc_t *desc)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_base_t *conn     = NULL;
    netmgr_conn_base_t *cur_conn = NULL;
    netmgr_report_t    *slot     = NULL;

    if (NULL == desc || NULL == desc->conn || NULL == desc->name) {
        PR_ERR("netmgr register failed, descriptor incomplete");
        return OPRT_INVALID_PARM;
    }

    conn = desc->conn;

    // Descriptor wins over the driver's static initialiser. Both base fields
    // degrade to caches of it, which is the ownership rule netconn_registry.h
    // states.
    conn->pri       = desc->default_pri;
    conn->provider  = desc->provider;
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
    //
    // The slot is also where the link's whole M3 state lives, and it starts as a
    // zeroed one: NETMGR_LINK_STATE_DOWN is 0, no deadline is armed, and the probe
    // accumulator holds NETMGR_PROBE_UNKNOWN, which is the value
    // NETMGR_LINK_STATE_UNVERIFIED is defined to agree with. last_pri is seeded
    // from the live value so the first pass does not report a priority change that
    // did not happen.
    slot = &s_netmgr.report[s_netmgr.link_num];
    memset(slot, 0, sizeof(*slot));
    slot->type     = desc->type;
    slot->status   = NETMGR_LINK_DOWN;
    slot->pending  = FALSE;
    slot->desc     = desc;
    slot->state    = NETMGR_LINK_STATE_DOWN;
    slot->last_pri = conn->pri;
    netmgr_retry_bind(&slot->reval, &netmgr_retry_table_revalidate);
    s_netmgr.link_num++;

    // Append. The list is registration order now - see the note above - so the
    // priority insertion that used to be here is gone, and with it the only reason
    // this function had to know what a priority means.
    conn->next = NULL;
    if (NULL == s_netmgr.conn) {
        s_netmgr.conn = conn;
        PR_DEBUG("netmgr [%s] is the first connection", desc->name);
    } else {
        cur_conn = s_netmgr.conn;
        while (NULL != cur_conn->next) {
            cur_conn = cur_conn->next;
        }
        cur_conn->next = conn;
    }

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
    OPERATE_RET                   rt      = OPRT_OK;
    const netconn_desc_t         *table   = NULL;
    uint32_t                      count   = 0;
    uint32_t                      i       = 0;
    uint32_t                      links   = 0;
    const netmgr_probe_backend_t *backend = NULL;

    /* Idempotent, by early return rather than by a prohibition in the header.
     *
     * A second netmgr_init() against a LIVE netmgr used to run the whole body
     * again, and the end state was "netmgr has forgotten its route while every
     * driver is still up":
     *
     *   - s_netmgr.active, .status and .last_pin are reset below, so the pass that
     *     follows re-derives them from the drivers and republishes as if the links
     *     had just changed;
     *   - s_netmgr.route_epoch is put back to 1. This one is M3's own doing and it
     *     is the sharpest: a probe reporter holding an epoch from before the reset
     *     (netmgr_probe.c keeps one across an MQTT session) now presents a value
     *     the counter has to climb back to, so netmgr_probe_report() discards
     *     VALID verdicts as stale until it does;
     *   - a second backend->start() runs. Harmless for the built-in passive
     *     backend, whose start() is two idempotent subscribes, but netmgr_probe.h
     *     requires idempotence only of stop();
     *   - no link is reopened, which is the one part that was already safe:
     *     __netmgr_conn_register() rejects a type it has already registered before
     *     it reaches conn->open().
     *
     * Early return rather than a documented ban, for the same reason
     * netmgr_deinit() is idempotent: a ban relies on every caller having read it,
     * whereas this makes the misuse harmless. OPRT_OK because the caller asked for
     * an initialised netmgr and that is what it has - an error code would push
     * callers into treating a benign double call as a failure.
     *
     * What this deliberately gives up: re-initialising with a WIDER type mask can
     * no longer add links. Nothing in the tree does that - all eighteen callers
     * call netmgr_init() exactly once at start-up - and the legitimate re-init
     * path, netmgr_cli.c's `netmgr init` after a `netmgr deinit`, arrives with
     * `inited` already FALSE and is unaffected. A caller that really wants a
     * different mask must netmgr_deinit() first, which is what the CLI does. */
    if (s_netmgr.inited) {
        PR_WARN("netmgr already initialised, netmgr_init(0x%x) ignored; deinit first to change the type mask", type);
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(tal_net_provider_init());

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
    s_netmgr.active   = NETCONN_AUTO;
    s_netmgr.last_pin = NETCONN_AUTO;

    // Never 0 once netmgr_init() has run: 0 is NETMGR_PROBE_EPOCH_ANY, and a
    // reporter that passes it means "attribute this to whatever is active now".
    // Seeding at 1 rather than leaving the memset's 0 is what makes an epoch a
    // reporter saved genuinely comparable.
    s_netmgr.route_epoch = 1;
    memset(&s_netmgr.route, 0, sizeof(s_netmgr.route));

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
        // dropped). The "nothing registered" check below is what catches a build
        // where nothing worked.
        __netmgr_conn_register(&table[i]);
    }

    tal_mutex_lock(s_netmgr.lock);
    links = s_netmgr.link_num;
    tal_mutex_unlock(s_netmgr.lock);

    // The M2 test here was `__get_active_conn() == NETCONN_AUTO`, and it is worth
    // being precise about what that tested, because the obvious M3 translation
    // would be a serious regression. __get_active_conn() seeded its answer with
    // the HEAD of the list before looking at any status, so it returned
    // NETCONN_AUTO if and only if NOTHING WAS REGISTERED - never because nothing
    // was up. Translating it as "the policy chose no link" would fail
    // netmgr_init() on every device that boots before its wifi associates, which
    // is every device. So the condition is spelled as what it always meant.
    if (0 == links) {
        PR_ERR("No connection available, please check your configuration");
        // Used to return here holding a created mutex and whatever links had
        // already been opened.
        netmgr_deinit();
        return OPRT_INVALID_PARM;
    }

    // Set before the first pass, not after: __netmgr_settle() publishes
    // EVENT_NETMGR_CHG, and a subscriber calling back into netmgr_conn_get()
    // would otherwise be told OPRT_RESOURCE_NOT_READY by a netmgr that is in fact
    // ready.
    s_netmgr.inited = TRUE;

    // The shared deadline. Created before the first pass so that pass can arm it;
    // a creation failure is not fatal, it only means no deadline can be armed, and
    // __netmgr_settle() checks the handle for exactly that reason. Every deadline
    // in M3 is a refinement, so a device with none still routes traffic.
    if (NULL == sg_netmgr_deadline_timer) {
        if (OPRT_OK != tal_sw_timer_create(__netmgr_deadline_tm_cb, NULL, &sg_netmgr_deadline_timer)) {
            sg_netmgr_deadline_timer = NULL;
            PR_ERR("netmgr deadline timer create failed, debounce/grace/verify timing is disabled");
        }
    }

    // The LAN gate's one non-link input. See __netmgr_mqtt_connected_cb().
    TUYA_CALL_ERR_LOG(
        tal_event_subscribe(EVENT_MQTT_CONNECTED, "netmgr", __netmgr_mqtt_connected_cb, SUBSCRIBE_TYPE_NORMAL));

    // The reachability backend, started with the lock released - netmgr_probe.h
    // puts start()/stop() under the same rule as conn->open()/close(), and the
    // default backend's start() is two tal_event_subscribe() calls.
    //
    // A failure is logged and init CONTINUES, which the header states outright: a
    // device that cannot verify its link must still be able to use it, and making
    // this fatal would turn a diagnostic into a boot failure.
    //
    // The #ifdef is what lets src/tuya_cloud_service/CMakeLists.txt drop
    // netmgr_probe.c from a build that does not select ENABLE_NETMGR_PROBE. This
    // expression is the only reference to netmgr_probe_backend_mqtt that GENERATES
    // A RELOCATION anywhere outside netmgr_probe.c itself; every other mention in
    // the tree is a comment or the extern declaration in netmgr_probe.h, neither
    // of which makes the linker want the object. So without this guard, gating the
    // file out is an undefined reference rather than a smaller image. Stated as a
    // property rather than a hit count on purpose - a count is wrong as soon as
    // somebody edits a comment, and the property is what the linker acts on.
    //
    // Why gate it at all, when the object is small. Measured on T5AI with the
    // arm-none-eabi-10.3-2021.10 the tree ships: 408 B of .text across the four
    // functions, 272 B of PR_* format strings, 12 B for the backend descriptor,
    // 8 B of .bss - about 692 B of flash. That alone would not be worth an
    // #ifdef. The reason it is worth one: with ENABLE_NETMGR_PROBE off the policy
    // has probe_enable FALSE, and netmgr_probe_report() drops every verdict at
    // that gate. Keeping the backend linked would mean two live tal_event
    // subscriptions and two callbacks per MQTT session transition, all of it
    // computing values that are thrown away. Gating makes "off" mean off instead
    // of "on and ignored".
    //
    // sg_probe_backend_chosen still works with the file gone, which is the case
    // that must not break: a product supplying its own ACTIVE backend through
    // netmgr_probe_backend_set() gets it either way. Such a product still has to
    // turn probe_enable on - by selecting ENABLE_NETMGR_PROBE, or at runtime via
    // netmgr_policy_set() - or its verdicts meet the same gate.
#if defined(ENABLE_NETMGR_PROBE) && (ENABLE_NETMGR_PROBE == 1)
    backend = sg_probe_backend_chosen ? sg_probe_backend : &netmgr_probe_backend_mqtt;
#else
    backend = sg_probe_backend_chosen ? sg_probe_backend : NULL;
#endif
    if (NULL != backend) {
        sg_probe_running = backend;
        if (NULL != backend->start) {
            if (OPRT_OK != backend->start()) {
                PR_ERR("netmgr probe backend [%s] start failed, link verification is disabled",
                       (NULL != backend->name) ? backend->name : "?");
            }
        }
    }

    // The first pass. It replaces M2's hand-written seeding - __get_active_conn(),
    // then a route_get / snap_provider / push_route triple - with the same pass
    // every later event runs, so init cannot drift from steady state.
    //
    // Two things it does that M2's seeding did not, both deliberate:
    //
    //   - it computes s_netmgr.status from the drivers. M2 read the field, which
    //     was still NETMGR_LINK_DOWN unless a report had already been handled, so
    //     init's push_route() pinned src_ip at 0 whenever it won that race. This
    //     narrows the window rather than changing the semantics: the first handler
    //     pass pushed the real address moments later either way;
    //   - it publishes EVENT_NETMGR_CHG with NETMGR_CHG_REASON_INIT, which
    //     netmgr_event.h asks for so a subscriber that starts later can learn the
    //     initial value without polling.
    //
    // The two LEGACY events are suppressed (legacy = FALSE), because M2's init
    // published neither and EVENT_LINK_TYPE_CHG's one subscriber calls
    // tuya_iot_reconnect() synchronously.
    __netmgr_settle(s_netmgr.lock, NETMGR_CHG_REASON_INIT, FALSE);

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
 *   - the shared deadline timer callback, on the tal_sw_timer thread, which
 *     tal_sw_timer_delete() cannot join. This was the LAN poll timer before M3;
 *     the hazard did not change when the timer's job did.
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
    MUTEX_HANDLE                  lock                    = s_netmgr.lock;
    netmgr_conn_base_t           *conns[NETMGR_LINK_MAX]  = {0};
    const netconn_desc_t         *descs[NETMGR_LINK_MAX]  = {0};
    netmgr_status_e               was_up[NETMGR_LINK_MAX] = {NETMGR_LINK_DOWN};
    const netmgr_probe_backend_t *backend                 = NULL;
    netmgr_change_t               chg                     = {0};
    uint32_t                      num                     = 0;
    uint32_t                      i                       = 0;
    uint32_t                      elapsed                 = 0;
    uint32_t                      busy                    = 0;
    BOOL_T                        drained                 = FALSE;
    BOOL_T                        ble_owned               = FALSE;

    // 0. Close the gate before anything else, and without the lock. From here on
    // the report shim and the deadline timer callback return immediately instead
    // of reaching into state this function is about to dismantle. It stays closed
    // until the next netmgr_init() reopens it.
    sg_netmgr_gate_closed = TRUE;

    // Idempotent, and safe when netmgr_init() never ran: no mutex means nothing
    // downstream of it was ever set up either, and the struct is already the
    // zeroed state a fresh netmgr_init() expects.
    if (NULL == lock) {
        /* Stopped, not deleted - same rule as step 5 below. Unreachable with a
         * live timer today, since the handle only becomes non-NULL after the
         * mutex exists, but the two paths must not disagree about ownership. */
        if (NULL != sg_netmgr_deadline_timer) {
            tal_sw_timer_stop(sg_netmgr_deadline_timer);
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
    chg.old_active     = s_netmgr.active;
    chg.old_status     = s_netmgr.status;
    chg.epoch          = s_netmgr.route_epoch;
    chg.src_ip         = s_netmgr.route.src_ip;
    backend            = sg_probe_running;
    sg_probe_running   = NULL;
    tal_mutex_unlock(lock);

    // 1a. Stop the reachability backend, at the START of teardown as
    // netmgr_probe.h requires, and with the lock released as its start() was.
    // Doing it here rather than later is what stops new verdicts arriving into
    // state we are about to zero. stop() is required to be idempotent, so a
    // teardown that runs on netmgr_init()'s own error path is safe.
    if (NULL != backend && NULL != backend->stop) {
        backend->stop();
    }

    // 1b. Release the pin. It lives in netmgr_policy.c, so the memset at step 6
    // cannot clear it, and a pin surviving into the next netmgr_init() would
    // silently override that cycle's ranking with an instruction nobody gave it.
    // netmgr_policy_pin(NETCONN_AUTO) is the release path and cannot fail; it also
    // does not call back into netmgr, unlike the arm path.
    (void)netmgr_policy_pin(NETCONN_AUTO);

    // 1c. The LAN gate's event input. Unsubscribed here, symmetrically with
    // netmgr_init(), so an init/deinit cycle does not leak a subscription - the
    // exact leak netconn_wifi.c carried until M2.
    tal_event_unsubscribe(EVENT_MQTT_CONNECTED, "netmgr", __netmgr_mqtt_connected_cb);

    // 1d. NETMGR_CHG_REASON_DEINIT, published before the links close, which is
    // what netmgr_event.h specifies. Published from locals with the lock released,
    // like every other event in this file.
    chg.reason     = NETMGR_CHG_REASON_DEINIT;
    chg.subject    = chg.old_active;
    chg.new_active = NETCONN_AUTO;
    chg.new_status = NETMGR_LINK_DOWN;
    chg.new_state  = NETMGR_LINK_STATE_DOWN;
    chg.handover   = FALSE;
    tal_event_publish(EVENT_NETMGR_CHG, (void *)&chg);

    // 2. Drop a work item that is queued but has not started. Cancelling by
    // callback with a NULL data is the only precise form - see the notify
    // channel note - and it is safe because the report state is static.
    tal_workq_cancel(WORKQ_SYSTEM, __netmgr_notify_work, NULL);

    // 3. Drain a handler that is already running, so it finishes BEFORE step 4
    // unlinks the links and step 6 zeroes the state. The hazard the drain exists
    // for is a handler walking half-dismantled state, not a freed mutex - this
    // function does not free the mutex, and that is the whole point of retaining
    // it (netmgr_priv.h argues it out).
    //
    // The first sleep is unconditional and deliberate. tal_workq_cancel() only
    // blanks the callback of items still in the queue - __work_cancel_traverse()
    // sets item->cb = NULL - and an item the workqueue thread already dequeued is
    // out of its reach; there is no tal_workqueue_flush() in the TAL. So a
    // handler can be sitting between "dequeued" and "notify_busy++" right now,
    // invisible to the counter. One poll interval gives it time to become
    // visible.
    //
    // That narrows the window and does not close it. What makes losing it
    // survivable is two things that both OUTLIVE this function rather than
    // anything it tears down: the gate closed in step 0, which the handler tests
    // before it touches any state, and `stopping`, which step 6 restores after the
    // memset and the handler re-tests under the lock. A straggler therefore locks
    // a LIVE mutex, reads stopping == TRUE and returns.
    //
    // (An earlier version of this comment said the window was survivable "because
    // step 6 nulls s_netmgr.lock before releasing the mutex". Step 6 does the
    // opposite - it restores the handle - and no mutex is released anywhere. The
    // correction is recorded rather than swapped in silently, because that
    // sentence was offered as the justification for accepting this window, and
    // null-then-free is the design netmgr_priv.h rejects.)
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

    // 4. Unlink every link, then close them outside the lock, newest registration
    // first.
    //
    // Walking the list forward now GIVES registration order - that is what
    // __netmgr_conn_register() appending buys - so the loop that used to look each
    // link up by type through __get_conn_by_type() is a plain walk, and the
    // reversal moved to the close loop below. M2 needed the lookup because
    // s_netmgr.report[] was registration order while s_netmgr.conn was priority
    // order, so neither one alone was the order to tear down in.
    tal_mutex_lock(lock);
    num = s_netmgr.link_num;
    for (i = 0; i < num; i++) {
        descs[i] = s_netmgr.report[i].desc;
        conns[i] = (NULL != descs[i]) ? descs[i]->conn : NULL;
        // Snapshotted under the lock for the "left up" warning below: whether the
        // bearer was carrying traffic when we let go of it.
        was_up[i] = (NULL != conns[i]) ? conns[i]->status : NETMGR_LINK_DOWN;
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
    //
    // The return value used to be dropped, and dropping it moved a lie rather than
    // removing one. M2 made NETCONN_CMD_CLOSE honest - netconn_table.c leaves the
    // bit out of the wired and cellular masks, so the command now answers
    // OPRT_NOT_SUPPORTED instead of OPRT_OK - but conn->close() itself is still a
    // documented no-op on both, so teardown left a cellular bearer up and told
    // nobody. It is graded by control level, because that is what says whether
    // anything COULD have been done:
    //
    //   MANAGED   there is a disconnect verb, so a failure is a real failure
    //   SUSTAINED tal_cellular.h has no deinit; the bearer stays up by design
    //   OBSERVE   tal_wired.h has no disconnect at all
    //
    // For the latter two the honest report is not an error but a statement of
    // fact, and only worth making when the link was actually up.
    for (i = num; i > 0; i--) {
        uint32_t              idx  = i - 1;
        const netconn_desc_t *desc = descs[idx];
        netconn_ctrl_level_e  ctrl = (NULL != desc) ? desc->ctrl : NETCONN_CTRL_OBSERVE;
        const char           *name = (NULL != desc && NULL != desc->name) ? desc->name : "?";
        OPERATE_RET           crt  = OPRT_OK;

        if (NULL == conns[idx]) {
            continue;
        }
        if (NULL != conns[idx]->close) {
            crt = conns[idx]->close();
        }

        if (NETCONN_CTRL_MANAGED == ctrl) {
            if (OPRT_OK != crt) {
                PR_ERR("netmgr [%s] close failed, rt = %d", name, crt);
            }
        } else if (NETMGR_LINK_DOWN != was_up[idx]) {
            PR_WARN("netmgr [%s] link left up: a %s link has no teardown verb, the bearer outlives netmgr", name,
                    NETCONN_CTRL_TO_STR(ctrl));
        }

        conns[idx]->event_cb = NULL;
        conns[idx]->next     = NULL;
        conns[idx]->status   = NETMGR_LINK_DOWN;
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

    // 5. The shared deadline timer, in the slot the LAN poll timer used to occupy.
    // STOPPED, AND DELIBERATELY NOT DELETED, for the same reason the mutex below
    // is retained - and it is the same race, which is why the two must be treated
    // alike.
    //
    // __netmgr_settle() finishes by arming or stopping this timer, and it reads
    // the handle and calls into tal_sw_timer OUTSIDE the lock; that is stated
    // where it happens and it is what keeps a UART write off the lock. So a settle
    // pass can be between the NULL check and tal_sw_timer_start() right now.
    // Deleting the timer here frees it under that pass.
    //
    // Not hypothetical, and not covered by the gate: step 1 drains the notify work
    // item with a bounded budget and returns OPRT_TIMEOUT when the drain does not
    // finish, after which this function tears down anyway. That is precisely the
    // case the retained mutex exists for. An earlier version of this comment said
    // the gate in step 0 "turns it into a no-op" - the gate stops the CALLBACK
    // from queueing work, it does nothing about a pass already past the handle
    // read.
    //
    // Retaining costs one timer for the life of the process instead of one per
    // init/deinit cycle, and netmgr_init() already reuses it: its create is
    // guarded on the handle being NULL. A straggler that re-arms a retained timer
    // is harmless - the callback only calls __netmgr_notify_post(), which
    // re-checks `stopping` under the lock and queues nothing.
    if (NULL != sg_netmgr_deadline_timer) {
        tal_sw_timer_stop(sg_netmgr_deadline_timer);
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

    // Positions follow list order, which since M3 is REGISTRATION order rather
    // than selection order - selection is recomputed per pass from conn->pri and
    // no longer has a fixed order to expose. So index 0 is the first link
    // registered, i.e. the first row of netconn_table.c that this build's type
    // mask selected, and index i is netmgr_link_view_t.reg_index i. Position is
    // still stable for as long as nothing registers, which between netmgr_init()
    // and netmgr_deinit() means always. Walking off the end is OPRT_NOT_FOUND,
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
    info->provider = cur_conn->provider;
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
    info->provider = cur_conn->provider;
    tal_mutex_unlock(lock);

    __netmgr_link_info_desc(info);

    return OPRT_OK;
}

/***********************************************************
******************* policy plumbing ************************
***********************************************************/

/* What is implemented here rather than in netmgr_policy.c / netmgr_probe.c
 * =======================================================================
 * The seam is the same in both cases and it is stated in netmgr_policy.c's own
 * file comment: a function belongs to netmgr.c when it needs state netmgr.c owns
 * and the satellite modules are forbidden to hold.
 *
 *   netmgr_policy_select_cb_set()  the hook runs under s_netmgr.lock and its
 *                                  answer is validated against the live candidate
 *                                  set, so it lives with the code that does both
 *   netmgr_link_state_get()        reads the per-link state machine, and its own
 *                                  contract says it takes s_netmgr.lock
 *   netmgr_reselect_request()      posts to netmgr's work queue, which the
 *                                  satellites have no access to by design
 *                                  (declared in netmgr_priv.h, not a public API)
 *   netmgr_probe_backend_set()     "rejected after netmgr_init()", and
 *                                  s_netmgr.inited is the only thing that knows
 *   netmgr_probe_report()          records under s_netmgr.lock and posts the same
 *   netmgr_probe_report_simple()   coalesced work item netmgr_notify_link() posts
 *   netmgr_probe_epoch_get()       the epoch IS s_netmgr.route_epoch
 *   netmgr_probe_stat_get()        the accumulators live in s_netmgr.report[]
 *
 * netmgr_probe.c therefore owns exactly one symbol of netmgr_probe.h,
 * netmgr_probe_backend_mqtt, which is the only one that header assigns to it by
 * name ("Defined in netmgr_probe.c and installed by netmgr_init()").
 */

OPERATE_RET netmgr_policy_select_cb_set(netmgr_policy_select_cb_t cb, void *ctx)
{
    MUTEX_HANDLE lock = s_netmgr.lock;

    // Taken under the lock when there is one, because the pass reads both
    // variables while holding it and a hook installed with a mismatched ctx would
    // be worse than one installed a moment later. Before the first netmgr_init()
    // there is no lock and no pass, so a plain store is correct there.
    if (NULL != lock) {
        tal_mutex_lock(lock);
    }
    sg_select_cb  = cb;
    sg_select_ctx = ctx;
    if (NULL != lock) {
        tal_mutex_unlock(lock);
    }

    PR_DEBUG("netmgr policy ranking hook %s", (NULL != cb) ? "installed" : "cleared");

    return OPRT_OK;
}

OPERATE_RET netmgr_link_state_get(netmgr_type_e type, netmgr_link_state_e *state)
{
    MUTEX_HANDLE     lock = s_netmgr.lock;
    netmgr_report_t *slot = NULL;

    if (NULL == state) {
        return OPRT_INVALID_PARM;
    }

    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_NOT_FOUND;
    }

    tal_mutex_lock(lock);
    if (NETCONN_AUTO == type) {
        type = s_netmgr.active;
    }
    slot = __netmgr_report_slot(type);
    if (NULL != slot) {
        *state = slot->state;
    }
    tal_mutex_unlock(lock);

    // A type with no slot is not "state down": *state is left untouched, so
    // answering OPRT_OK would hand the caller its own uninitialised variable.
    // netmgr_policy_pin() reads this as "the pin is armed but the link cannot
    // carry traffic yet".
    if (NULL == slot) {
        return OPRT_NOT_FOUND;
    }

    // A PURE READ. It posted a pass in the first draft, so that a pin armed
    // immediately before would take effect - netmgr_policy_pin() arms the pin and
    // then calls this to report whether it is in force. That worked and it was the
    // wrong shape: an inspection API documented "for the CLI and for diagnostics"
    // must not carry a side effect, and a caller had no way to ask the question
    // without also asking for the action - a CLI dump that walked every link
    // queued a pass per row.
    //
    // netmgr_reselect_request() (netmgr_priv.h) is the explicit form, and
    // netmgr_policy_pin() calls it directly. Two callers that each say exactly
    // what they want, instead of one that gets both.
    return OPRT_OK;
}

/***********************************************************
********************* probe plumbing ***********************
***********************************************************/

uint32_t netmgr_probe_epoch_get(void)
{
    MUTEX_HANDLE lock  = s_netmgr.lock;
    uint32_t     epoch = NETMGR_PROBE_EPOCH_ANY;

    if (sg_netmgr_gate_closed || NULL == lock) {
        return NETMGR_PROBE_EPOCH_ANY;
    }

    tal_mutex_lock(lock);
    if (s_netmgr.inited) {
        epoch = s_netmgr.route_epoch;
    }
    tal_mutex_unlock(lock);

    return epoch;
}

OPERATE_RET netmgr_probe_report(const netmgr_probe_result_t *result)
{
    MUTEX_HANDLE     lock  = s_netmgr.lock;
    netmgr_report_t *slot  = NULL;
    BOOL_T           stale = FALSE;
    netmgr_policy_t  pol   = NETMGR_POLICY_DEFAULT_INIT;

    if (NULL == result || NETMGR_PROBE_UNKNOWN == result->verdict) {
        return OPRT_INVALID_PARM;
    }

    // Dropped, not failed, when there is nowhere to record it. A reporter is
    // stating a fact about the world, not requesting an action, and has no
    // recovery for any of these.
    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_OK;
    }

    (void)netmgr_policy_get(&pol);
    if (!pol.probe_enable) {
        return OPRT_OK;
    }

    tal_mutex_lock(lock);

    if (s_netmgr.stopping || !s_netmgr.inited) {
        tal_mutex_unlock(lock);
        return OPRT_OK;
    }

    // Epoch. A verdict is about the route that carried the traffic, not about the
    // route that happens to be installed when the report arrives, and the two
    // differ on EVERY link switch - not rarely. The chain is deterministic:
    // netmgr bumps the epoch, publishes EVENT_LINK_TYPE_CHG, tuya_iot's subscriber
    // calls tuya_iot_reconnect() -> tuya_mqtt_stop(), which publishes
    // EVENT_MQTT_DISCONNECTED, which the passive backend reports as BAD. That BAD
    // is about the link we just left. A backend that saved the epoch at its CONNACK
    // therefore hands us a stale one here, and it is discarded - which is the whole
    // point, and it is why the bump has to precede the publish.
    //
    // NETMGR_PROBE_EPOCH_ANY means "I have no epoch; attribute this to whatever is
    // active now", and is accepted at face value.
    if (NETMGR_PROBE_EPOCH_ANY != result->epoch && result->epoch != s_netmgr.route_epoch) {
        stale = TRUE;
    }

    if (!stale) {
        // Attribution is netmgr's, never the reporter's: a reporter knows whether
        // its packets arrived, not which link carried them, and letting it name one
        // would let it demote a link it never used.
        slot = __netmgr_report_slot(s_netmgr.active);
    }

    if (NULL != slot) {
        // ACCUMULATED, not overwritten - the one place this differs from
        // netmgr_notify_link(). A link report may be dropped in favour of the
        // settled state because the handler re-reads every driver; a verdict may
        // not, because there is nothing to re-read: the count of consecutive BADs
        // IS the state. So two BADs arriving before the pass runs count twice, and
        // a GOOD arriving before it runs still clears them.
        slot->probe.last   = result->verdict;
        slot->probe.source = result->source;

        if (NETMGR_PROBE_GOOD == result->verdict) {
            slot->probe.bad_count = 0;
            if (slot->probe.good_total < 0xFFFF) {
                slot->probe.good_total++;
            }
        } else {
            if (slot->probe.bad_count < 0xFF) {
                slot->probe.bad_count++;
            }
            if (slot->probe.bad_total < 0xFFFF) {
                slot->probe.bad_total++;
            }
        }
    }

    tal_mutex_unlock(lock);

    if (stale) {
        PR_DEBUG("netmgr probe %s from %s dropped, epoch %d is not %d", NETMGR_PROBE_VERDICT_TO_STR(result->verdict),
                 NETMGR_PROBE_SRC_TO_STR(result->source), result->epoch, netmgr_probe_epoch_get());
        return OPRT_OK;
    }

    if (NULL == slot) {
        // No link is active, so there is nothing the verdict can be about.
        return OPRT_OK;
    }

    // Record and post, never evaluate. This runs on the tuya_iot_yield() thread,
    // which is also the MQTT keepalive pump: running a pass here would put a
    // blocking conn->get(NETCONN_CMD_IP) modem exchange inside the keepalive path.
    (void)__netmgr_notify_post(lock);

    return OPRT_OK;
}

OPERATE_RET netmgr_probe_report_simple(netmgr_probe_verdict_e verdict, netmgr_probe_source_e source)
{
    netmgr_probe_result_t result = {
        .verdict = verdict,
        .source  = source,
        .epoch   = NETMGR_PROBE_EPOCH_ANY,
    };

    return netmgr_probe_report(&result);
}

OPERATE_RET netmgr_probe_backend_set(const netmgr_probe_backend_t *backend)
{
    // Rejected rather than silently ignored once the links are up, the same
    // discipline as netconn_registry_set_table(): a configuration call that
    // quietly reverts to the default is the failure mode these APIs exist to
    // prevent.
    if (s_netmgr.inited) {
        PR_ERR("netmgr probe backend set failed, netmgr is already initialised");
        return OPRT_COM_ERROR;
    }

    sg_probe_backend        = backend;
    sg_probe_backend_chosen = TRUE;

    PR_DEBUG("netmgr probe backend [%s] installed",
             (NULL != backend && NULL != backend->name) ? backend->name : "none");

    return OPRT_OK;
}

OPERATE_RET netmgr_probe_stat_get(netmgr_type_e type, netmgr_probe_stat_t *stat)
{
    MUTEX_HANDLE     lock = s_netmgr.lock;
    netmgr_report_t *slot = NULL;

    if (NULL == stat) {
        return OPRT_INVALID_PARM;
    }

    if (sg_netmgr_gate_closed || NULL == lock) {
        return OPRT_NOT_FOUND;
    }

    tal_mutex_lock(lock);
    slot = __netmgr_report_slot(type);
    if (NULL != slot) {
        *stat = slot->probe;
    }
    tal_mutex_unlock(lock);

    return (NULL != slot) ? OPRT_OK : OPRT_NOT_FOUND;
}
