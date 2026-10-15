/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "tuya_ble_cfg.h"
#include "ble_trace_stats.h"
#include "tuya_ble_hci.h"
#include "ble_hs_priv.h"
#include "ble_monitor_priv.h"
#include "tuya_hs_port.h"
#include "ble_svc_gap.h"
#include "tal_log.h"
#include "tkl_hci.h"
#include "tal_system.h"

#define BLE_HS_HCI_EVT_COUNT ((TUYA_BLE_HCI_EVT_HI_BUF_COUNT) + (TUYA_BLE_HCI_EVT_LO_BUF_COUNT))

static void ble_hs_event_rx_hci_ev(struct tuya_ble_event *ev);
#if TY_HS_BLE_CONNECT
static void ble_hs_event_tx_notify(struct tuya_ble_event *ev);
#endif
static void ble_hs_event_reset(struct tuya_ble_event *ev);
static void ble_hs_event_start_stage1(struct tuya_ble_event *ev);
static void ble_hs_event_start_stage2(struct tuya_ble_event *ev);
static void ble_hs_timer_sched(int32_t ticks_from_now);

struct os_mempool ble_hs_hci_ev_pool;
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
static os_membuf_t ble_hs_hci_os_event_buf[OS_MEMPOOL_SIZE(BLE_HS_HCI_EVT_COUNT, sizeof(struct tuya_ble_event))];
#endif
/** OS event - triggers tx of pending notifications and indications. */
static struct tuya_ble_event ble_hs_ev_tx_notifications;

/** OS event - triggers a full reset. */
static struct tuya_ble_event ble_hs_ev_reset;

static struct tuya_ble_event ble_hs_ev_start_stage1;
static struct tuya_ble_event ble_hs_ev_start_stage2;

uint8_t    ble_hs_sync_state;
uint8_t    ble_hs_enabled_state = BLE_HS_ENABLED_STATE_OFF;
static int ble_hs_reset_reason;

#define BLE_HS_SYNC_RETRY_TIMEOUT_MS 100 /* ms */
#define MAX_QUEUE_NUM                16

// static void *ble_hs_parent_task;

/**
 * Handles unresponsive timeouts and periodic retries in case of resource
 * shortage.
 */
static struct tuya_ble_callout ble_hs_timer;

/* Shared queue that the host uses for work items. */
static tuya_ble_eventq *ble_hs_evq;

static struct ble_mqueue ble_hs_rx_q;

static tuya_ble_mutex ble_hs_mutex;

/** These values keep track of required ATT and GATT resources counts.  They
 * increase as services are added, and are read when the ATT server and GATT
 * server are started.
 */
uint16_t ble_hs_max_attrs;
uint16_t ble_hs_max_services;
uint16_t ble_hs_max_client_configs;

#if (TY_HS_BLE_HS_DEBUG)
static uint8_t ble_hs_dbg_mutex_locked;
#endif

STATS_SECT_DECL(ble_hs_stats) ble_hs_stats;
STATS_NAME_START(ble_hs_stats)
STATS_NAME(ble_hs_stats, conn_create)
STATS_NAME(ble_hs_stats, conn_delete)
STATS_NAME(ble_hs_stats, hci_cmd)
STATS_NAME(ble_hs_stats, hci_event)
STATS_NAME(ble_hs_stats, hci_invalid_ack)
STATS_NAME(ble_hs_stats, hci_unknown_event)
STATS_NAME(ble_hs_stats, hci_timeout)
STATS_NAME(ble_hs_stats, reset)
STATS_NAME(ble_hs_stats, sync)
STATS_NAME(ble_hs_stats, pvcy_add_entry)
STATS_NAME(ble_hs_stats, pvcy_add_entry_fail)
STATS_NAME_END(ble_hs_stats)

tuya_ble_eventq *ble_hs_evq_get(void)
{
    return ble_hs_evq;
}

void ble_hs_evq_set(tuya_ble_eventq *evq)
{
    ble_hs_evq = evq;
}

#if (TY_HS_BLE_HS_DEBUG)
int ble_hs_locked_by_cur_task(void)
{
    return 1;
}
#endif

/**
 * Indicates whether the host's parent task is currently running.
 */
int ble_hs_is_parent_task(void)
{
    return !tuya_ble_os_started(); // || tuya_ble_check_current_task_id(ble_hs_parent_task);
}

/**
 * Locks the BLE host mutex.  Nested locks allowed.
 */
void ble_hs_lock_nested(void)
{
    int rc;

#if (TY_HS_BLE_HS_DEBUG)
    if (!tuya_ble_os_started()) {
        ble_hs_dbg_mutex_locked = 1;
        return;
    }
#endif
    rc = tuya_ble_mutex_lock(ble_hs_mutex, 0xffffffff);
    BLE_HS_DBG_ASSERT_EVAL(rc == 0 || rc == OS_NOT_STARTED);
}

/**
 * Unlocks the BLE host mutex.  Nested locks allowed.
 */
void ble_hs_unlock_nested(void)
{
    int rc;

#if (TY_HS_BLE_HS_DEBUG)
    if (!tuya_ble_os_started()) {
        ble_hs_dbg_mutex_locked = 0;
        return;
    }
#endif
    rc = tuya_ble_mutex_unlock(ble_hs_mutex);
    BLE_HS_DBG_ASSERT_EVAL(rc == 0 || rc == OS_NOT_STARTED);
}

/**
 * Locks the BLE host mutex.  Nested locks not allowed.
 */
void ble_hs_lock(void)
{
    BLE_HS_DBG_ASSERT(!ble_hs_locked_by_cur_task());
#if (TY_HS_BLE_HS_DEBUG)
    if (!tuya_ble_os_started()) {
        BLE_HS_DBG_ASSERT(!ble_hs_dbg_mutex_locked);
    }
#endif

    ble_hs_lock_nested();
}

/**
 * Unlocks the BLE host mutex.  Nested locks not allowed.
 */
void ble_hs_unlock(void)
{
#if (TY_HS_BLE_HS_DEBUG)
    if (!tuya_ble_os_started()) {
        BLE_HS_DBG_ASSERT(ble_hs_dbg_mutex_locked);
    }
#endif

    ble_hs_unlock_nested();
}

void ble_hs_process_rx_data_queue(void)
{
    struct os_mbuf *om;

    while ((om = ble_mqueue_get(&ble_hs_rx_q)) != NULL) {
#if BLE_MONITOR
        ble_monitor_send_om(BLE_MONITOR_OPCODE_ACL_RX_PKT, om);
#endif
        int ret = ble_hs_hci_evt_acl_process(om);
        if (ret) {
            PR_ERR("ACL PROCESS ERR");
        }
    }
}

static int ble_hs_wakeup_tx_conn(struct ble_hs_conn *conn)
{
    struct os_mbuf_pkthdr *omp;
    struct os_mbuf        *om;
    int                    rc;

    while ((omp = STAILQ_FIRST(&conn->bhc_tx_q)) != NULL) {
        STAILQ_REMOVE_HEAD(&conn->bhc_tx_q, omp_next);

        om = OS_MBUF_PKTHDR_TO_MBUF(omp);
        rc = ble_hs_hci_acl_tx_now(conn, &om);
        if (rc == BLE_HS_EAGAIN) {
            /* Controller is at capacity.  This packet will be the first to
             * get transmitted next time around.
             */
            STAILQ_INSERT_HEAD(&conn->bhc_tx_q, OS_MBUF_PKTHDR(om), omp_next);
            return BLE_HS_EAGAIN;
        }
    }

    return 0;
}

/**
 * Schedules the transmission of all queued ACL data packets to the controller.
 */
void ble_hs_wakeup_tx(void)
{
    struct ble_hs_conn *conn;
    int                 rc;

    ble_hs_lock();

    /* If there is a connection with a partially transmitted packet, it has to
     * be serviced first.  The controller is waiting for the remainder so it
     * can reassemble it.
     */
    for (conn = ble_hs_conn_first(); conn != NULL; conn = SLIST_NEXT(conn, bhc_next)) {

        if (conn->bhc_flags & BLE_HS_CONN_F_TX_FRAG) {
            rc = ble_hs_wakeup_tx_conn(conn);
            if (rc != 0) {
                goto done;
            }
            break;
        }
    }

    /* For each connection, transmit queued packets until there are no more
     * packets to send or the controller's buffers are exhausted.
     */
    for (conn = ble_hs_conn_first(); conn != NULL; conn = SLIST_NEXT(conn, bhc_next)) {

        rc = ble_hs_wakeup_tx_conn(conn);
        if (rc != 0) {
            goto done;
        }
    }

done:
    ble_hs_unlock();
}

static void ble_hs_clear_rx_queue(void)
{
    struct os_mbuf *om;

    while ((om = ble_mqueue_get(&ble_hs_rx_q)) != NULL) {
        os_mbuf_free_chain(om);
    }
}

int ble_hs_is_enabled(void)
{
    return ble_hs_enabled_state == BLE_HS_ENABLED_STATE_ON;
}

int ble_hs_synced(void)
{
    return ble_hs_sync_state == BLE_HS_SYNC_STATE_GOOD;
}

static int ble_hs_sync(void)
{
    uint32_t retry_tmo_ticks;
    int      rc;

    /* Set the sync state to "bringup."  This allows the parent task to send
     * the startup sequence to the controller.  No other tasks are allowed to
     * send any commands.
     */
    ble_hs_sync_state = BLE_HS_SYNC_STATE_BRINGUP;

    rc = ble_hs_startup_go();
    if (rc == 0) {
        ble_hs_sync_state = BLE_HS_SYNC_STATE_GOOD;
    } else {
        ble_hs_sync_state = BLE_HS_SYNC_STATE_BAD;
    }
    // BLE_HS_LOG_DEBUG("sync states rc = %d", rc);
    retry_tmo_ticks = tuya_ble_time_ms_to_ticks32(BLE_HS_SYNC_RETRY_TIMEOUT_MS);
    ble_hs_timer_sched(retry_tmo_ticks);

    if (rc == 0) {
#if 0 // We dont need IRK and SM.
        rc = ble_hs_misc_restore_irks();
        if (rc != 0) {
            BLE_HS_LOG(INFO, "Failed to restore IRKs from store; status=%d\n",
                       rc);
        }
#endif
        if (tuya_ble_hs_cfg.sync_cb != NULL) {
            tuya_ble_hs_cfg.sync_cb();
        }

        STATS_INC(ble_hs_stats, sync);
    }

    return rc;
}

static int ble_hs_reset(void)
{
    int rc;

    STATS_INC(ble_hs_stats, reset);

    ble_hs_sync_state = 0;

    /* Reset transport.  Assume success; there is nothing we can do in case of
     * failure.  If the transport failed to reset, the host will reset itself
     * again when it fails to sync with the controller.
     */
    (void)tkl_hci_reset();

    ble_hs_clear_rx_queue();

    // BLE_HS_LOG(INFO, "ble_hs_reset, ble_hs_reset_reason = %d", ble_hs_reset_reason);
    /* Clear adverising and scanning states. */
    ble_gap_reset_state(ble_hs_reset_reason);

    /* Clear configured addresses. */
    ble_hs_id_reset();

    if (tuya_ble_hs_cfg.reset_cb != NULL && ble_hs_reset_reason != 0) {
        tuya_ble_hs_cfg.reset_cb(ble_hs_reset_reason);
    }
    ble_hs_reset_reason = 0;
    rc                  = ble_hs_sync();
    return rc;
}

/**
 * Called when the host timer expires.  Handles unresponsive timeouts and
 * periodic retries in case of resource shortage.
 */
static void ble_hs_timer_exp(struct tuya_ble_event *ev)
{
    int32_t ticks_until_next;

    BLE_HS_LOG(INFO, "ble_hs_timer_exp:");
    switch (ble_hs_sync_state) {
    case BLE_HS_SYNC_STATE_GOOD:
#if TY_HS_BLE_CONNECT
        ticks_until_next = ble_gattc_timer();
        ble_hs_timer_sched(ticks_until_next);
        ticks_until_next = ble_l2cap_sig_timer();
        ble_hs_timer_sched(ticks_until_next);
        ticks_until_next = ble_sm_timer();
        ble_hs_timer_sched(ticks_until_next);
        ticks_until_next = ble_hs_conn_timer();
        ble_hs_timer_sched(ticks_until_next);
#endif
        ticks_until_next = ble_gap_timer();
        ble_hs_timer_sched(ticks_until_next);
        break;

    case BLE_HS_SYNC_STATE_BAD:
        BLE_HS_LOG(DEBUG, "BLE_HS_SYNC_STATE_BAD");
        ble_hs_reset();
        break;

    case BLE_HS_SYNC_STATE_BRINGUP:
    default:
        /* The timer should not be set in this state. */
        BLE_HS_LOG(INFO, "ble_hs_sync_state:%d", ble_hs_sync_state);
        TUYA_HS_ASSERT(0);
        break;
    }
}

static void ble_hs_timer_reset(uint32_t ticks)
{
    int rc;

    if (!ble_hs_is_enabled()) {
        tuya_ble_callout_stop(&ble_hs_timer);
        BLE_HS_LOG(INFO, "ble_hs_timer stop:%d ", ticks);
    } else {
        rc = tuya_ble_callout_reset(&ble_hs_timer, ticks);
        BLE_HS_LOG(INFO, "ble_hs_timer reset:%d ", ticks);
        BLE_HS_DBG_ASSERT_EVAL(rc == 0);
    }
}

static void ble_hs_timer_sched(int32_t ticks_from_now)
{
    uint32_t abs_time;

    if (ticks_from_now == BLE_HS_FOREVER) {
        return;
    }

    /* Reset timer if it is not currently scheduled or if the specified time is
     * sooner than the previous expiration time.
     */
    abs_time = tuya_ble_tick_count_get() + ticks_from_now;
    if (!tuya_ble_callout_is_active(&ble_hs_timer) ||
        ((int32_t)(abs_time - tuya_ble_callout_get_ticks(&ble_hs_timer))) < 0) {
        ble_hs_timer_reset(ticks_from_now);
    }
}

void ble_hs_timer_resched(void)
{
    /* Reschedule the timer to run immediately.  The timer callback will query
     * each module for an up-to-date expiration time.
     */
    ble_hs_timer_reset(0);
}

void ble_hs_dyna_timer_resched(uint16_t time)
{
    /* Reschedule the timer to run.  The timer callback will query
     * each module for an up-to-date expiration time.
     */
    ble_hs_timer_reset(time);
}

static void ble_hs_sched_start_stage2(void)
{
    tuya_ble_eventq_put((tuya_ble_eventq *)ble_hs_evq_get(), &ble_hs_ev_start_stage2);
}

void ble_hs_sched_start(void)
{
    tuya_ble_eventq_put(tuya_port_get_dflt_eventq(), &ble_hs_ev_start_stage1);
}

static void ble_hs_event_rx_hci_ev(struct tuya_ble_event *ev)
{
    const struct ble_hci_ev *hci_ev;
    int                      rc;

    hci_ev = tuya_ble_event_get_arg(ev);
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
    rc = os_memblock_put(&ble_hs_hci_ev_pool, ev);
#else
    rc = tuya_ble_hci_mp_num_buf_free(&ble_hs_hci_ev_pool, (uint8_t *)ev);
#endif
    BLE_HS_DBG_ASSERT_EVAL(rc == 0);

#if BLE_MONITOR
    ble_monitor_send(BLE_MONITOR_OPCODE_EVENT_PKT, hci_ev, hci_ev->length + sizeof(*hci_ev));
#endif

    ble_hs_hci_evt_process(hci_ev);
}

#if TY_HS_BLE_CONNECT
static void ble_hs_event_tx_notify(struct tuya_ble_event *ev)
{
    ble_gatts_tx_notifications();
}
#endif

static void ble_hs_event_rx_data(struct tuya_ble_event *ev)
{
    // PR_INFO("ble_hs_acl_rx_data\n");
    ble_hs_process_rx_data_queue();
}

static void ble_hs_event_reset(struct tuya_ble_event *ev)
{
    PR_INFO("ble_hs_event_reset\n");
    ble_hs_reset();
}

/**
 * Implements the first half of the start process.  This just enqueues another
 * event on the host parent task's event queue.
 *
 * Starting is done in two stages to allow the application time to configure
 * the event queue to use after system initialization but before the host
 * starts.
 */
static void ble_hs_event_start_stage1(struct tuya_ble_event *ev)
{
    // PR_DEBUG("ble host stage 1 start\n");
    ble_hs_sched_start_stage2();
}

/**
 * Implements the second half of the start process.  This actually starts the
 * host.
 *
 * Starting is done in two stages to allow the application time to configure
 * the event queue to use after system initialization but before the host
 * starts.
 */
static void ble_hs_event_start_stage2(struct tuya_ble_event *ev)
{
    int rc;
    PR_DEBUG("ble host stage 2 start\n");
    rc = ble_hs_start();
    if (rc != 0) {
        PR_ERR("HS START ERR %d", rc);
    }
}

void ble_hs_enqueue_hci_event(uint8_t *hci_evt)
{
    if (ble_hs_evq->q_num >= (MAX_QUEUE_NUM - 5) && hci_evt[0] == 0x3e && hci_evt[2] == 0x02) {
        // PR_INFO("que_num >10,discard LE_ADVERTISING_REPORT evt\n\r");
        tuya_ble_hci_buf_free(TUYA_BLE_HCI_BUF_EVT, (uint8_t *)hci_evt);
        return;
    }
    struct tuya_ble_event *ev;
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
    ev = os_memblock_get(&ble_hs_hci_ev_pool);
#else
    extern void *tuya_ble_hci_dyna_buf_alloc(int type, struct os_mempool *dynapool);
    ev = (struct tuya_ble_event *)tuya_ble_hci_dyna_buf_alloc(0, &ble_hs_hci_ev_pool);
#endif
    if (ev == NULL) {
        BLE_HS_LOG_DEBUG("cannot get hci_evt pool, wait");
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
        tuya_ble_hci_buf_free(hci_evt);
#else
        tuya_ble_hci_buf_free(TUYA_BLE_HCI_BUF_EVT, (uint8_t *)hci_evt);
#endif
        tal_system_sleep(5);
    } else {
        tuya_ble_event_set_ev(ev, ble_hs_event_rx_hci_ev, hci_evt);
        tuya_ble_eventq_put(ble_hs_evq, ev);
    }
}

/**
 * Schedules for all pending notifications and indications to be sent in the
 * host parent task.
 */
void ble_hs_notifications_sched(void)
{
#if !(TY_HS_BLE_HS_REQUIRE_OS)
    if (!tuya_ble_os_started()) {
        ble_gatts_tx_notifications();
        return;
    }
#endif

    tuya_ble_eventq_put(ble_hs_evq, &ble_hs_ev_tx_notifications);
}

void ble_hs_sched_reset(int reason)
{
    BLE_HS_DBG_ASSERT(ble_hs_reset_reason == 0);
    BLE_HS_LOG_DEBUG("Reset!!!*(0x%02x)", reason);

    ble_hs_reset_reason = reason;
    tuya_ble_eventq_put(ble_hs_evq, &ble_hs_ev_reset);
}

void ble_hs_hw_error(uint8_t hw_code)
{
    BLE_HS_LOG_DEBUG("Controller Report Fail!!!*(0x%02x)", hw_code);
    ble_hs_sched_reset(BLE_HS_HW_ERR(hw_code));
}

int ble_hs_start(void)
{
    int rc;

    ble_hs_lock();
    switch (ble_hs_enabled_state) {
    case BLE_HS_ENABLED_STATE_ON:
        rc = BLE_HS_EALREADY;
        break;

    case BLE_HS_ENABLED_STATE_STOPPING:
        rc = BLE_HS_EBUSY;
        break;

    case BLE_HS_ENABLED_STATE_OFF:
        ble_hs_enabled_state = BLE_HS_ENABLED_STATE_ON;
        rc                   = 0;
        break;

    default:
        TUYA_HS_ASSERT(0);
        rc = BLE_HS_EUNKNOWN;
        break;
    }
    ble_hs_unlock();

    if (rc != 0) {
        return rc;
    }

    // tuya_ble_check_current_task_id(ble_hs_parent_task);

#if (TY_HS_SELFTEST)
    /* Stop the timer just in case the host was already running (e.g., unit
     * tests).
     */
    tuya_ble_callout_stop(&ble_hs_timer);
#endif

#if TY_HS_BLE_CONNECT
    rc = ble_gatts_start();
    if (rc != 0) {
        return rc;
    }
#endif
    ble_hs_sync();

    return 0;
}

/**
 * Called when a data packet is received from the controller.  This function
 * consumes the supplied mbuf, regardless of the outcome.
 *
 * @param om                    The incoming data packet, beginning with the
 *                                  HCI ACL data header.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int ble_hs_rx_data(uint8_t *acl_pkt, uint16_t lens)
{
    int             rc;
    struct os_mbuf *om;
    uint8_t        *pdu_pkt = acl_pkt;
    uint16_t        len;

    len = get_le16(pdu_pkt + 2);
    if (len > TUYA_BLE_ACL_BUF_SIZE) {
        PR_ERR("ble_hs_rx_data LEN ERR  len:%d", len);
        return BLE_ERR_UNSPECIFIED;
    }
    // tuya_ble_raw_print("ble_hs_rx_data", 16, acl_pkt, len + TUYA_BLE_HCI_DATA_HDR_SZ);
    om = tuya_ble_hci_acl_buf_alloc();
    if (!om) {
        PR_ERR("OM ALLOC ERR");
        return BLE_ERR_MEM_CAPACITY;
    }

    memcpy(om->om_data, pdu_pkt, len + TUYA_BLE_HCI_DATA_HDR_SZ);
    OS_MBUF_PKTLEN(om) = TUYA_BLE_HCI_DATA_HDR_SZ + len;
    om->om_len         = TUYA_BLE_HCI_DATA_HDR_SZ + len;

    /* If flow control is enabled, mark this packet with its corresponding
     * connection handle.
     */
    ble_hs_flow_track_data_mbuf(om);

    rc = ble_mqueue_put(&ble_hs_rx_q, ble_hs_evq, om); // om_pkthdr_len
    if (rc != 0) {
        os_mbuf_free_chain(om);
        PR_ERR("RX_Q PUT ERR:%d", rc);
        return BLE_HS_EOS;
    }

    return 0;
}

/**
 * Enqueues an ACL data packet for transmission.  This function consumes the
 * supplied mbuf, regardless of the outcome.
 *
 * @param om                    The outgoing data packet, beginning with the
 *                                  HCI ACL data header.
 *
 * @return                      0 on success; nonzero on failure.
 */
int ble_hs_tx_data(struct os_mbuf *om)
{
#if BLE_MONITOR
    ble_monitor_send_om(BLE_MONITOR_OPCODE_ACL_TX_PKT, om);
#endif

    return tuya_ble_hs_acl_tx(om);
}

void ble_hs_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    BLE_ASSERT_ACTIVE();
    /* Create memory pool of OS events */
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
    rc = os_mempool_init(&ble_hs_hci_ev_pool, BLE_HS_HCI_EVT_COUNT, sizeof(struct tuya_ble_event),
                         ble_hs_hci_os_event_buf, "ble_hs_hci_ev_pool");
    BLE_PANIC_ASSERT(rc == 0);
#else
    rc = os_dynamempool_init(&ble_hs_hci_ev_pool, BLE_HS_HCI_EVT_COUNT, sizeof(struct tuya_ble_event),
                             "ble_hs_hci_ev_pool", TUYA_USE_DYNA_RAM_FLAG);
    BLE_PANIC_ASSERT(rc == 0);
#endif
    /* These get initialized here to allow unit tests to run without a zeroed
     * bss.
     */
    ble_hs_reset_reason  = 0;
    ble_hs_enabled_state = BLE_HS_ENABLED_STATE_OFF;
#if TY_HS_BLE_CONNECT
    tuya_ble_event_set_ev(&ble_hs_ev_tx_notifications, ble_hs_event_tx_notify, NULL);
#endif
    tuya_ble_event_set_ev(&ble_hs_ev_reset, ble_hs_event_reset, NULL);
    tuya_ble_event_set_ev(&ble_hs_ev_start_stage1, ble_hs_event_start_stage1, NULL);
    tuya_ble_event_set_ev(&ble_hs_ev_start_stage2, ble_hs_event_start_stage2, NULL);
    ble_hs_hci_init();
    rc = ble_hs_conn_init();
    BLE_PANIC_ASSERT(rc == 0);
#if (TY_HS_BLE_PERIODIC_ADV)
    rc = ble_hs_periodic_sync_init();
    BLE_PANIC_ASSERT(rc == 0);
#endif

#if TY_HS_BLE_CONNECT
    rc = ble_l2cap_init();
    BLE_PANIC_ASSERT(rc == 0);
    rc = ble_att_init();
    BLE_PANIC_ASSERT(rc == 0);
    rc = ble_att_svr_init();
    BLE_PANIC_ASSERT(rc == 0);
    rc = ble_gattc_init();
    BLE_PANIC_ASSERT(rc == 0);
    rc = ble_gatts_init();
    BLE_PANIC_ASSERT(rc == 0);
#endif
    rc = ble_gap_init();
    BLE_PANIC_ASSERT(rc == 0);
    ble_hs_stop_init();

    tuya_ble_callout_init(&ble_hs_timer, ble_hs_evq, ble_hs_timer_exp, NULL);
    ble_mqueue_init(&ble_hs_rx_q, ble_hs_event_rx_data, NULL);
    rc = stats_init_and_reg(STATS_HDR(ble_hs_stats), STATS_SIZE_INIT_PARMS(ble_hs_stats, STATS_SIZE_32),
                            STATS_NAME_INIT_PARMS(ble_hs_stats), "ble_hs");
    BLE_PANIC_ASSERT(rc == 0);
    rc = tuya_ble_mutex_init(&ble_hs_mutex);
    BLE_PANIC_ASSERT(rc == 0);

#if (TY_HS_BLE_HS_DEBUG)
    ble_hs_dbg_mutex_locked = 0;
#endif
    ble_hs_evq_set(tuya_port_get_dflt_eventq());
    /* Configure the HCI transport to communicate with a host. */
    tkl_hci_callback_register(ble_hs_hci_rx_evt, ble_hs_rx_data);
#if BLE_MONITOR
    rc = ble_monitor_init();
    BLE_PANIC_ASSERT(rc == 0);
#endif

    /* Enqueue the start event to the default event queue.  Using the default
     * queue ensures the event won't run until the end of main().  This allows
     * the application to configure this package in the meantime.
     */
#if (TY_HS_BLE_HS_AUTO_START)
    int ret = tuya_ble_eventq_put(tuya_port_get_dflt_eventq(), &ble_hs_ev_start_stage1);
    TUYA_HS_ASSERT(ret == 0);
#endif
#if BLE_MONITOR
    ble_monitor_new_index(0, (uint8_t[6]){}, "tuya_ble_host");
#endif
}
