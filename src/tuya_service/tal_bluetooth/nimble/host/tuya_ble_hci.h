/**
 * @file tuya_ble_hci.h
 * @brief HCI transport interface for Tuya BLE SDK.
 *
 * This file defines the HCI (Host Controller Interface) transport layer interface
 * for the Tuya BLE SDK. It includes definitions for buffer sizes, counts, and
 * types for HCI commands, events, and ACL (Asynchronous Connection-Less) data.
 * Additionally, it provides an interface for setting a callback for when an ACL
 * data packet is freed. This is crucial for managing memory and ensuring efficient
 * communication between the host stack and the controller stack within Tuya's BLE solutions.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef _TUYA_HCI_TRANSPORT_H_
#define _TUYA_HCI_TRANSPORT_H_

#include <inttypes.h>
#include "tuya_ble_mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

struct os_mbuf;

/**************************     Tuya Ble HCI Transport           **********************************/
/*
                            --------   Tuya HOST STACK    --------

                             HCI CMD                 ACL DATA
                                I           O           IO
                                |           |           |
                                |           |           |
                                |           |           |
                                O           I           IO
                                        HCI EVENT    ACL DATA

                            --------  CONTROLLER STACK   --------
*/
/***********************    Tuya Ble HCI Transport Interface        *****************************/
#ifndef TUYA_BLE_HCI_CMD_BUF_COUNT
#define TUYA_BLE_HCI_CMD_BUF_COUNT (8)
#endif

#ifndef TUYA_BLE_HCI_EVT_HI_BUF_COUNT
#define TUYA_BLE_HCI_EVT_HI_BUF_COUNT (24)
#endif

#ifndef TUYA_BLE_HCI_EVT_LO_BUF_COUNT
#define TUYA_BLE_HCI_EVT_LO_BUF_COUNT (8)
#endif

// ACL Data Buffer Count
#ifndef TUYA_BLE_ACL_BUF_COUNT
#define TUYA_BLE_ACL_BUF_COUNT (12)
#endif

// ACL Data Buffer Size
#if TUYA_BK_HOST_ALLACATION
#ifndef TUYA_BLE_ACL_BUF_SIZE
#define TUYA_BLE_ACL_BUF_SIZE (80)
#endif
#else
#ifndef TUYA_BLE_ACL_BUF_SIZE
#define TUYA_BLE_ACL_BUF_SIZE (255)
#endif
#endif

#if TUYA_BK_HOST_ALLACATION
#ifndef TUYA_BLE_HCI_EVT_BUF_SIZE
#define TUYA_BLE_HCI_EVT_BUF_SIZE (50)
#endif
#else
#ifndef TUYA_BLE_HCI_EVT_BUF_SIZE
#define TUYA_BLE_HCI_EVT_BUF_SIZE (70)
#endif
#endif

#if TUYA_BK_HOST_ALLACATION
#ifndef TUYA_BLE_HCI_CMD_SZ
#define TUYA_BLE_HCI_CMD_SZ 50
#endif
#else
#ifndef TUYA_BLE_HCI_CMD_SZ
#define TUYA_BLE_HCI_CMD_SZ (260)
#endif
#endif

#ifndef TUYA_BLE_HCI_DATA_HDR_SZ
#define TUYA_BLE_HCI_DATA_HDR_SZ (4)
#endif

#define TUYA_BLE_HCI_BUF_EVT_LO (1)
#define TUYA_BLE_HCI_BUF_EVT_HI (2)

/* Host-to-controller command. */
#define TUYA_BLE_HCI_BUF_CMD (3)
/* Tuya Dyna Ram Use. */
// Dyna Event Use
#define TUYA_BLE_HCI_BUF_EVT (4)

/**
 * Configures a callback to get executed whenever an ACL data packet is freed.
 * The function is called immediately before the free occurs.
 *
 * @param cb                    The callback to configure.
 * @param arg                   An optional argument to pass to the callback.
 *
 * @retval 0                    success
 * @retval Other                fail
 *                              BLE_ERR_UNSUPPORTED if the transport does not
 *                                  support this operation.
 */
int tuya_ble_hci_set_acl_free_cb(os_mempool_put_fn *cb, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* H_HCI_TRANSPORT_ */
