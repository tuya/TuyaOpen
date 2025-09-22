/**
 * @file     tkl_hci.h
 * @brief    Host-Controller Interface, Sync with Tuya Host Stack
 * @version
 * @date     2021-10-11
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_HCI_H__
#define __TKL_HCI_H__

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************     TKL Bluetooth HCI Transport        **********************************/
/*
                        --------  TAL Bluetooth HOST STACK        --------

                             HCI CMD                 ACL DATA
                                I           O           IO
                                |           |           |
                                |           |           |
                                |           |           |
                                O           I           IO
                                        HCI EVENT    ACL DATA

                        -------- TKL Bluetooth CONTROLLER STACK    --------
*/
/*************************    TKL Bluetooth HCI Transport Interface  *********************************/

/**< HCI Callback Register function definition */
typedef OPERATE_RET (*TKL_HCI_FUNC_CB)(uint8_t *p_buf, uint16_t buf_len);

/**
 * @brief   Function for initializing the bluetooth host-controller interface
 * @param   void
 * @return  SUCCESS             Initialized successfully.
 *          ERROR
 * */
OPERATE_RET tkl_hci_init(void);

/**
 * @brief   Function for de-initializing the bluetooth host-controller interface
 * @param   void
 * @return  SUCCESS             De-initialized successfully.
 *          ERROR
 * */
OPERATE_RET tkl_hci_deinit(void);

/**
 * @brief   [Linux/Android] Function for reseting the bluetooth host-controller interface
 *          Try to recover socket or reopen uart/usb interface.
 * @param   void
 * @return  SUCCESS             Reset successfully.
 *          ERROR
 * @note    [Special Interface] If running in RTOS, we may not support this feature.
 *          And you can report OPRT_NOT_SUPPORT if you dont need it.
 * */
OPERATE_RET tkl_hci_reset(void);

/**
 * @brief   Send HCI-Command Packet to controller from host.
 *          The HCI Command packet is used to send commands to the Controller from
 *          the Host. The format of the HCI Command packet is shown @Rule
 *          Controllers shall be able to accept HCI Command packets with up to 255 bytes
 *          of data excluding the HCI Command packet header. The HCI Command packet
 *          header is the first 3 octets of the packet.
 *
 * @param   p_buf               Follow Core Spec. Refer to @Rule
 *          length              Indicate the length of the buffer. it can be "opcode + 1(len) + Parameter len";
 * @return  SUCCESS             Initialized successfully.
 *          ERROR
 *
 * @Spec    BLUETOOTH CORE SPECIFICATION Version 5.2 | Vol 4, Part E, 5-4.1
 * @Rule        2 bytes              1 byte              1 byte           N bytes
 *          OpCode(OCF+OGF) + Parameter Total Length + Parameter 0 ... + Parameter N
 * @Note    The OpCode Group Field (OGF), OpCode Command Field (OCF).
 * */
OPERATE_RET tkl_hci_cmd_packet_send(const uint8_t *p_buf, uint16_t buf_len);

/**
 * @brief   Send HCI-Command Packet to controller from host.
 *           HCI ACL Data packets are used to exchange data between the Host and Controller.
 *           Hosts and Controllers shall be able to accept HCI ACL Data packets with up to
 *           27 bytes of data excluding the HCI ACL Data packet header on
 *           Connection_Handles associated with an LE-U logical link.The HCI ACL Data
 *           packet header is the first 4 octets of the packet.
 *
 * @param   p_buf               Follow Core Spec. Refer to @Rule
 *          length              Indicate the length of the buffer. it can be "Handle + PB Flag
 *                              + PC Flag + Data Total Length";
 * @return  SUCCESS             Initialized successfully.
 *          ERROR
 *
 * @Spec    BLUETOOTH CORE SPECIFICATION Version 5.2 | Vol 4, Part E, 5-4.2
 * @Rule                                  2 bytes                             2 bytes         N bytes
 *          (Connection Handle + PB Flag(12-14bit) + PC Flag(14-16bit)) + Data Total Length + Data
 * @Note    PB Flag: Packet_Boundary_Flag; PB Flag: Broadcast_Flag;
 * */
OPERATE_RET tkl_hci_acl_packet_send(const uint8_t *p_buf, uint16_t buf_len);

/**
 * @brief   Register the hci callback, while receiving "hci-event" or "acl-packet" data from controller,
 *          we will post these data into host stack.
 *          hci_evt_cb: The Host shall be able to accept HCI Event packets with up to 255 octets of data
 *          excluding the HCI Event packet header
 *          acl_pkt_cb: Refer to @tkl_hci_acl_packet_send
 *
 * @param   hci_evt_cb          Indicate the HCI Event callback.
 *          acl_pkt_cb          Indicate the ACL packet callback.
 * @return  SUCCESS             Initialized successfully.
 *          ERROR
 *
 * @Spec    BLUETOOTH CORE SPECIFICATION Version 5.2 | Vol 4, Part E, 5-4.4
 *          For More Event And Commnad Details:
 *          BLUETOOTH CORE SPECIFICATION Version 5.2 | Vol 4, Part E, 7-x
 *
 * @Rule    hci_evt_cb:
               1 byte        1 byte                1 byte                       N bytes
 *          Event Code + Parameter Total Length+ Event Parameter 0 + ... + Event Parameter N
 * */
OPERATE_RET tkl_hci_callback_register(const TKL_HCI_FUNC_CB hci_evt_cb, const TKL_HCI_FUNC_CB acl_pkt_cb);

#ifdef __cplusplus
}
#endif

#endif
