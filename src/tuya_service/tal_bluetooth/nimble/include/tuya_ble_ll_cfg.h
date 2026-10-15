/**
 * @file tuya_ble_ll_cfg.h
 * @brief Configuration options for Tuya's BLE lower layer.
 *
 * This header file contains macro definitions to configure various features of the BLE lower layer
 * used in Tuya's BLE SDK. It includes settings for hardware whitelist enablement, strict scheduling
 * periods, connection parameter request feature, control to host flow control, data length extension,
 * and LE 2M PHY feature support.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef _TUYA_BLE_LL_CFG_H_
#define _TUYA_BLE_LL_CFG_H_

#ifndef TY_HS_BLE_HW_WHITELIST_ENABLE
#define TY_HS_BLE_HW_WHITELIST_ENABLE (0)
#endif

#ifndef TY_HS_BLE_LL_ADD_STRICT_SCHED_PERIODS
#define TY_HS_BLE_LL_ADD_STRICT_SCHED_PERIODS (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_CONN_PARAM_REQ
#define TY_HS_BLE_LL_CFG_FEAT_CONN_PARAM_REQ (1)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_CTRL_TO_HOST_FLOW_CONTROL
#define TY_HS_BLE_LL_CFG_FEAT_CTRL_TO_HOST_FLOW_CONTROL (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_DATA_LEN_EXT
#define TY_HS_BLE_LL_CFG_FEAT_DATA_LEN_EXT (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_LE_2M_PHY
#define TY_HS_BLE_LL_CFG_FEAT_LE_2M_PHY (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_LE_CODED_PHY
#define TY_HS_BLE_LL_CFG_FEAT_LE_CODED_PHY (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_LE_CSA2
#define TY_HS_BLE_LL_CFG_FEAT_LE_CSA2 (1)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_LE_ENCRYPTION
#define TY_HS_BLE_LL_CFG_FEAT_LE_ENCRYPTION (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_LE_PING
#define TY_HS_BLE_LL_CFG_FEAT_LE_PING (TY_HS_BLE_LL_CFG_FEAT_LE_ENCRYPTION)
#endif

/* Value copied from BLE_EXT_ADV */
#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_EXT_ADV
#define TY_HS_BLE_LL_CFG_FEAT_LL_EXT_ADV (0)
#endif

/* Value copied from BLE_ISO */
#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_ISO
#define TY_HS_BLE_LL_CFG_FEAT_LL_ISO (0)
#endif

/* Value copied from BLE_ISO_TEST */
#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_ISO_TEST
#define TY_HS_BLE_LL_CFG_FEAT_LL_ISO_TEST (0)
#endif

/* Value copied from BLE_PERIODIC_ADV */
#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_PERIODIC_ADV
#define TY_HS_BLE_LL_CFG_FEAT_LL_PERIODIC_ADV (0)
#endif

/* Value copied from BLE_MAX_PERIODIC_SYNCS */
#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_PERIODIC_ADV_SYNC_CNT
#define TY_HS_BLE_LL_CFG_FEAT_LL_PERIODIC_ADV_SYNC_CNT (0)
#endif

/* Value copied from BLE_MAX_PERIODIC_SYNCS */
#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_PERIODIC_ADV_SYNC_LIST_CNT
#define TY_HS_BLE_LL_CFG_FEAT_LL_PERIODIC_ADV_SYNC_LIST_CNT (0)
#endif

/* Value copied from BLE_PERIODIC_ADV_SYNC_TRANSFER */
#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_PERIODIC_ADV_SYNC_TRANSFER
#define TY_HS_BLE_LL_CFG_FEAT_LL_PERIODIC_ADV_SYNC_TRANSFER (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_PRIVACY
#define TY_HS_BLE_LL_CFG_FEAT_LL_PRIVACY (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_LL_SCA_UPDATE
#define TY_HS_BLE_LL_CFG_FEAT_LL_SCA_UPDATE (0)
#endif

#ifndef TY_HS_BLE_LL_CFG_FEAT_SLAVE_INIT_FEAT_XCHG
#define TY_HS_BLE_LL_CFG_FEAT_SLAVE_INIT_FEAT_XCHG (1)
#endif

#ifndef TY_HS_BLE_LL_CONN_INIT_MAX_TX_BYTES
#define TY_HS_BLE_LL_CONN_INIT_MAX_TX_BYTES (TY_HS_BLE_LL_MAX_PKT_SIZE)
#endif

#ifndef TY_HS_BLE_LL_CONN_INIT_MIN_WIN_OFFSET
#define TY_HS_BLE_LL_CONN_INIT_MIN_WIN_OFFSET (0)
#endif

#ifndef TY_HS_BLE_LL_CONN_INIT_SLOTS
#define TY_HS_BLE_LL_CONN_INIT_SLOTS (4)
#endif

#ifndef TY_HS_BLE_LL_DEBUG_GPIO_HCI_CMD
#define TY_HS_BLE_LL_DEBUG_GPIO_HCI_CMD (-1)
#endif

#ifndef TY_HS_BLE_LL_DEBUG_GPIO_HCI_EV
#define TY_HS_BLE_LL_DEBUG_GPIO_HCI_EV (-1)
#endif

#ifndef TY_HS_BLE_LL_DEBUG_GPIO_SCHED_ITEM_CB
#define TY_HS_BLE_LL_DEBUG_GPIO_SCHED_ITEM_CB (-1)
#endif

#ifndef TY_HS_BLE_LL_DEBUG_GPIO_SCHED_RUN
#define TY_HS_BLE_LL_DEBUG_GPIO_SCHED_RUN (-1)
#endif

#ifndef TY_HS_BLE_LL_DIRECT_TEST_MODE
#define TY_HS_BLE_LL_DIRECT_TEST_MODE (0)
#endif

/* Value copied from BLE_LL_DIRECT_TEST_MODE */
#ifndef TY_HS_BLE_LL_DTM
#define TY_HS_BLE_LL_DTM (0)
#endif

#ifndef TY_HS_BLE_LL_DTM_EXTENSIONS
#define TY_HS_BLE_LL_DTM_EXTENSIONS (0)
#endif

#ifndef TY_HS_BLE_LL_EXT_ADV_AUX_PTR_CNT
#define TY_HS_BLE_LL_EXT_ADV_AUX_PTR_CNT (0)
#endif

#ifndef TY_HS_BLE_LL_MASTER_SCA
#define TY_HS_BLE_LL_MASTER_SCA (4)
#endif

#ifndef TY_HS_BLE_LL_MAX_PKT_SIZE
#define TY_HS_BLE_LL_MAX_PKT_SIZE (251)
#endif

#ifndef TY_HS_BLE_LL_MFRG_ID
#define TY_HS_BLE_LL_MFRG_ID (0xFFFF)
#endif

#ifndef TY_HS_BLE_LL_NUM_COMP_PKT_ITVL_MS
#define TY_HS_BLE_LL_NUM_COMP_PKT_ITVL_MS (2000)
#endif

#ifndef TY_HS_BLE_LL_NUM_SCAN_DUP_ADVS
#define TY_HS_BLE_LL_NUM_SCAN_DUP_ADVS (8)
#endif

#ifndef TY_HS_BLE_LL_NUM_SCAN_RSP_ADVS
#define TY_HS_BLE_LL_NUM_SCAN_RSP_ADVS (8)
#endif

#ifndef TY_HS_BLE_LL_OUR_SCA
#define TY_HS_BLE_LL_OUR_SCA (60)
#endif

#ifndef TY_HS_BLE_LL_PRIO
#define TY_HS_BLE_LL_PRIO (0)
#endif

#ifndef TY_HS_BLE_LL_RESOLV_LIST_SIZE
#define TY_HS_BLE_LL_RESOLV_LIST_SIZE (4)
#endif

#ifndef TY_HS_BLE_LL_RFMGMT_ENABLE_TIME
#define TY_HS_BLE_LL_RFMGMT_ENABLE_TIME (1500)
#endif

#ifndef TY_HS_BLE_LL_RNG_BUFSIZE
#define TY_HS_BLE_LL_RNG_BUFSIZE (32)
#endif

/* Value copied from BLE_LL_OUR_SCA */
#ifndef TY_HS_BLE_LL_SCA
#define TY_HS_BLE_LL_SCA (60)
#endif

#ifndef TY_HS_BLE_LL_SCHED_AUX_CHAIN_MAFS_DELAY
#define TY_HS_BLE_LL_SCHED_AUX_CHAIN_MAFS_DELAY (0)
#endif

#ifndef TY_HS_BLE_LL_SCHED_AUX_MAFS_DELAY
#define TY_HS_BLE_LL_SCHED_AUX_MAFS_DELAY (0)
#endif

#ifndef TY_HS_BLE_LL_SCHED_SCAN_AUX_PDU_LEN
#define TY_HS_BLE_LL_SCHED_SCAN_AUX_PDU_LEN (41)
#endif

#ifndef TY_HS_BLE_LL_SCHED_SCAN_SYNC_PDU_LEN
#define TY_HS_BLE_LL_SCHED_SCAN_SYNC_PDU_LEN (32)
#endif

#ifndef TY_HS_BLE_LL_STRICT_CONN_SCHEDULING
#define TY_HS_BLE_LL_STRICT_CONN_SCHEDULING (0)
#endif

#ifndef TY_HS_BLE_LL_SUPP_MAX_RX_BYTES
#define TY_HS_BLE_LL_SUPP_MAX_RX_BYTES (TY_HS_BLE_LL_MAX_PKT_SIZE)
#endif

#ifndef TY_HS_BLE_LL_SUPP_MAX_TX_BYTES
#define TY_HS_BLE_LL_SUPP_MAX_TX_BYTES (TY_HS_BLE_LL_MAX_PKT_SIZE)
#endif

#ifndef TY_HS_BLE_LL_SYSINIT_STAGE
#define TY_HS_BLE_LL_SYSINIT_STAGE (250)
#endif

#ifndef TY_HS_BLE_LL_SYSVIEW
#define TY_HS_BLE_LL_SYSVIEW (0)
#endif

#ifndef TY_HS_BLE_LL_TX_PWR_DBM
#define TY_HS_BLE_LL_TX_PWR_DBM (0)
#endif

#ifndef TY_HS_BLE_LL_USECS_PER_PERIOD
#define TY_HS_BLE_LL_USECS_PER_PERIOD (3250)
#endif

#ifndef TY_HS_BLE_LL_VND_EVENT_ON_ASSERT
#define TY_HS_BLE_LL_VND_EVENT_ON_ASSERT (0)
#endif

#ifndef TY_HS_BLE_LL_WHITELIST_SIZE
#define TY_HS_BLE_LL_WHITELIST_SIZE (8)
#endif

#ifndef TY_HS_OS_CPUTIME_FREQ
#define TY_HS_OS_CPUTIME_FREQ (32768)
#endif

#ifndef TY_HS_OS_CPUTIME_TIMER_NUM
#define TY_HS_OS_CPUTIME_TIMER_NUM (5)
#endif

#ifndef TY_HS_TIMER_5
#define TY_HS_TIMER_5 (1)
#endif

#ifndef TY_HS_BLE_XTAL_SETTLE_TIME
#define TY_HS_BLE_XTAL_SETTLE_TIME (0)
#endif

#ifndef TY_HS_BLE_PHY_DBG_TIME_ADDRESS_END_PIN
#define TY_HS_BLE_PHY_DBG_TIME_ADDRESS_END_PIN (-1)
#endif

#ifndef TY_HS_BLE_PHY_DBG_TIME_TXRXEN_READY_PIN
#define TY_HS_BLE_PHY_DBG_TIME_TXRXEN_READY_PIN (-1)
#endif

#ifndef TY_HS_BLE_PHY_DBG_TIME_WFR_PIN
#define TY_HS_BLE_PHY_DBG_TIME_WFR_PIN (-1)
#endif

#ifndef TY_HS_BLE_PHY_NRF52840_ERRATA_164
#define TY_HS_BLE_PHY_NRF52840_ERRATA_164 (0)
#endif

#ifndef TY_HS_BLE_PHY_NRF52840_ERRATA_191
#define TY_HS_BLE_PHY_NRF52840_ERRATA_191 (1)
#endif

#ifndef TY_HS_BLE_PHY_SYSVIEW
#define TY_HS_BLE_PHY_SYSVIEW (0)
#endif

#endif
