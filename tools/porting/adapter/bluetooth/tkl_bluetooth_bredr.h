/**
 * @file tkl_bluetooth_bredr.h
 * @brief This is tkl_bluetooth_bredr file
 * @version 1.0
 * @date 2022-08-10
 *
 * @copyright Copyright 2021-2031 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_BLUETOOTH_BREDR_H__
#define __TKL_BLUETOOTH_BREDR_H__

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Unified Bluetooth BREDR Interface, primarily used in the following scenarios:
 * 1. As a Bluetooth speaker device (Sink), playing audio from the mobile phone (Source) on the device. Examples: Tuya
 * Smart Speaker, Tuya WiFi Voice Speaker, etc.
 * 2. As a Bluetooth headset (Sink), playing audio from the mobile phone or mobile terminal (Sink) on the device.
 * Example: Tuya Bluetooth Headset
 *
 * Note: The main application scenario for this interface is the Sink side, as there are currently no Source
 * requirements. We will focus on the Sink side interface specifications and do not need to consider the audio data
 * stream. For example, sending speaker data to the next audio playback device is not within our consideration, as the
 * protocol does not support point-to-multipoint connections, so this is not considered for the time being.
 *
 * Purpose of standardizing the Bluetooth BREDR interface:
 * 1. Separate business logic from the driver interface to maximize the standardization of upper and lower layer
 * behavior.
 * 2. Better expand business and driver needs as required, avoiding unnecessary interface imports and applications.
 * 3. Better meet the needs of TuyaOS in Bluetooth business.
 * 4. Lightweight interface for easier understanding.
 */

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/
typedef enum {
    TUYA_BT_BREDR_STACK_INIT = 0x01, /**< Init Bluetooth BR-EDR Stack, return  refer@Stack Error Code.result */

    TUYA_BT_BREDR_STACK_DEINIT, /**< Deinit Bluetooth BR-EDR Stack */

    TUYA_BT_BREDR_STACK_RESET, /**< Reset Bluetooth BR-EDR Stack */

    TUYA_BT_BREDR_GAP_EVT_CONNECT, /**< General Connected */

    TUYA_BT_BREDR_GAP_EVT_DISCONNECT, /**< General Disconnected */

    TUYA_BT_BREDR_GAP_EVT_PAIR, /**< General Pairing */

    TUYA_BT_BREDR_GAP_INFO_INQUIRY, /**< Report Device Info inquiry */

    TUYA_BT_BREDR_STREAM_STATUS, /**< Report BR-EDR Stream Status */

    TUYA_BT_BREDR_PHONE_STATUS, /**< Report Phone Status */
} TUYA_BT_BREDR_EVENT_TYPE_E;

typedef enum {
    TUYA_BT_BREDR_MODE_IDLE = 0x00,                /**< device is idle mode */
    TUYA_BT_BREDR_MODE_SCAN_INQUIRY_ONLY,          /**< device is only inquiry_scan mode */
    TUYA_BT_BREDR_MODE_SCAN_PAGE_ONLY,             /**< device is only page_scan mode */
    TUYA_BT_BREDR_MODE_SCAN_INQUIRY_AND_SCAN_PAGE, /**< device is both inquiry_scan and page_scan mode */
} TUYA_BT_SCAN_MODE_E;

typedef enum {
    TUYA_BT_PAIR_BOND_START = 0x01, /**< Indicatet the pairing procedure running*/
    TUYA_BT_PAIR_BOND_REQUEST, /**< After the phone request pairing with any passkey, we need to input the passkey*/
    TUYA_BT_PAIR_BOND_CONFIRM, /**< After the phone request pairing with one passkey, we can show it and do confirm */
    TUYA_BT_PAIR_BOND_SUCCESS, /**< Pairing successfully*/
    TUYA_BT_PAIR_BOND_FAIL,    /**< Pairing fail*/
} TUYA_BT_PAIR_EVENT_TYPE_E;

typedef enum {
    // [AVRCP], Audio/Video Remote Control, 4.6.X Support level in CT&TG
    TUYA_BT_BREDR_RCP_START = 0x01, /**< Start to control Audio */
    TUYA_BT_BREDR_RCP_PLAY,         /**< Play Music */
    TUYA_BT_BREDR_RCP_STOP,         /**< Stop Playing Music */
    TUYA_BT_BREDR_RCP_PAUSE,        /**< Pause Music */
    TUYA_BT_BREDR_RCP_NEXT,         /**< Play Next Music */
    TUYA_BT_BREDR_RCP_PREV,         /**< Play Previous Music */
    TUYA_BT_BREDR_RCP_FORWARD,      /**< Forward Music */
    TUYA_BT_BREDR_RCP_REWIND,       /**< Rewind Music */
    TUYA_BT_BREDR_RCP_REPEAT,       /**< Repeat Music */
    TUYA_BT_BREDR_RCP_MUTE,         /**< Mute for Music, user_data: NULL */
    TUYA_BT_BREDR_RCP_VOLUME_UP,    /**< Volume up for Music, step: 1, user_data: NULL */
    TUYA_BT_BREDR_RCP_VOLUME_DOWN,  /**< Volume down for Music, step: 1, user_data: NULL */

    // [HFP], Hands-Free Profile
    TUYA_BT_BREDR_HFP_START,  /**< Start to control Phone */
    TUYA_BT_BREDR_HFP_ANSWER, /**< Answer the Phone */
    TUYA_BT_BREDR_HFP_HANGUP, /**< Hang Up the Phone */
    TUYA_BT_BREDR_HFP_REJECT, /**< Reject the Phone */
    TUYA_BT_BREDR_HFP_CALL,   /**< Call the Phone */

    // [HFP], Need to post the data
    TUYA_BT_BREDR_HFP_UPDATE_BATTERY, /**< Update Battery to the Phone */
    TUYA_BT_BREDR_HFP_VOLUME_UP,      /**< Volume up for the phone, step: 1, user_data: NULL */
    TUYA_BT_BREDR_HFP_VOLUME_DOWN,    /**< Volume down for the phone, step: 1, user_data: NULL */
    TUYA_BT_BREDR_HFP_SET_VOLUME,     /**< Set the volume for the phone, post volume percent into adapter.
                                              Eg: Set Volume into 50%[Range: 0-100],we should post 50(or hex: 0x32)*/
    TUYA_BT_BREDR_HFP_GET_VOLUME,     /**< Get the volume from the phone, user_data: NULL */

    TUYA_BT_BREDR_A2DP_START, /**< Start to play audio*/

    TUYA_BT_BREDR_AVRCP_DISCONNECT,  /**< Request Disconnect AVRCP Profile */
    TUYA_BT_BREDR_A2DP_DISCONNECT,   /**< Request Disconnect A2DP Profile */
    TUYA_BT_BREDR_HFP_DISCONNECT,    /**< Request Disconnect HFP Profile */
    TUYA_BT_BREDR_START_CONNECTION,  /**< Request Connect One Device*/
    TUYA_BT_BREDR_CANCEL_CONNECTION, /**< Cancel Connecting One Device*/
} TUYA_BT_BREDR_CONTROL_E;

/**< BLUETOOTH CORE SPECIFICATION Version 5.2 | Vol 2, Part B 2.2.1 Master-Slave definition */
typedef enum {
    TUYA_BT_BREDR_ROLE_MASTER = 0x01, /**< BR-EDR Role: Master, Not Central From Core Spec */
    TUYA_BT_BREDR_ROLE_SLAVE = 0x02,  /**< BR-EDR Role: Slave */
} TUYA_BT_BREDR_ROLE_E;

typedef enum {
    TUYA_BT_STREAM_STATUS_IDLE = 0x01,  /**< Current Audio Streaming Being in Idle Mode. */
    TUYA_BT_STREAM_STATUS_CONNECTED,    /**< A2DP Connected */
    TUYA_BT_STREAM_STATUS_DISCONNECTED, /**< A2DP Disonnected */
    TUYA_BT_STREAM_STATUS_START,        /**< Current Audio Streaming Has been started. */
    TUYA_BT_STREAM_STATUS_SUSPENDING,   /**< Current Audio Streaming Being in Suspend Mode. */
    TUYA_BT_STREAM_STATUS_STOP,         /**< Current Audio Streaming Has been stoped. */
} TUYA_BT_BREDR_STEAM_STATUS_E;

typedef enum {
    TUYA_BT_PHONE_STATUS_IDLE = 0x01,  /**< Current Device Being in Idle Mode. */
    TUYA_BT_PHONE_STATUS_CONNECTED,    /**< HFP Connected */
    TUYA_BT_PHONE_STATUS_DISCONNECTED, /**< HFP Disonnected */
    TUYA_BT_PHONE_INCOMING,            /**< Incoming phone, indicate the status for device */
    TUYA_BT_PHONE_OUTGOING,            /**< Outgoing phone, indicate the status for device */
    TUYA_BT_PHONE_ACTIVE,              /**< Currently, the device is being phone active */
    TUYA_BT_PHONE_HANGUP,              /**< Hang Up phone, indicate the status for device */
    TUYA_BT_PHONE_VOLOUME_CHANGED,     /**< Indicate the volume changed */
    TUYA_BT_PHONE_UPDATE_BATTERY,      /**< Update the battery */
} TUYA_BT_BREDR_PHONE_STATUS_E;

typedef enum {
    TUYA_BT_PAIR_MODE_NO_PAIRING,   /**< Pairing is not allowed */
    TUYA_BT_PAIR_MODE_WAIT_FOR_REQ, /**< Wait for a pairing request or slave security request */
    TUYA_BT_PAIR_MODE_INITIATE,     /**< Don't wait, initiate a pairing request or slave security request*/
} TUYA_BT_PAIR_MODE_E;

typedef enum {
    TUYA_BT_IO_CAP_DISPLAY_ONLY,       /**< Display Only Device */
    TUYA_BT_IO_CAP_DISPLAY_YES_NO,     /**< Display and Yes and No Capable */
    TUYA_BT_IO_CAP_KEYBOARD_ONLY,      /**< Keyboard Only */
    TUYA_BT_IO_CAP_NO_INPUT_NO_OUTPUT, /**< No Display or Input Device */
    TUYA_BT_IO_CAP_KEYBOARD_DISPLAY,   /**< Both Keyboard and Display Capable */
} TUYA_BT_PAIR_IO_CAP_E;

typedef enum {
    TUYA_BT_PAIR_REQUEST_CONFIRMATION, /**< Confirmation request then should send pair_enable*/
    TUYA_BT_PAIR_REQUEST_PASSKEY,      /**< passkey request then should enter passkey  */
    TUYA_BT_PAIR_REQUEST_PRESSKEY,     /**< */
    TUYA_BT_PAIR_REQUEST_PIN,          /**< pin request then should enter pair_enable */
} TUYA_BT_PAIR_REQUEST_T;

/***********************************************************************
 ********************* struct ******************************************
 **********************************************************************/
typedef struct {
    uint8_t type;    /**< Mac Address Type, Refer to @ TKL_BLE_GAP_ADDR_TYPE_PUBLIC or TKL_BLE_GAP_ADDR_TYPE_RANDOM*/
    uint8_t addr[6]; /**< Mac Address, Address size, 6 bytes */
} TUYA_BT_GAP_ADDR_T;

typedef struct {
    TUYA_BT_PAIR_MODE_E mode;     /**< Bond Manager Pairing Modes */
    TUYA_BT_PAIR_IO_CAP_E io_cap; /**< Bond Manager I/O Capabilities Refer to @TUYA_BT_PAIR_IO_CAP_E */

    uint8_t oob_data;
    uint8_t mitm;           /**< Man In The Middle mode enalbe/disable */
    BOOL_T ble_secure_conn; /**< BLE Secure Simple Pairing, also called Secure Connection mode. Enable or not */
    uint32_t passkey;       /**< Init passkey. */
} TUYA_BT_PAIR_INIT_PARAM_T;

typedef struct {
    TUYA_BT_GAP_ADDR_T addr; /**< Address of the remote device. */
    uint8_t link_key[16];    /**< security keys. */

    void *user_data;
} TUYA_BT_PAIR_BOND_INFO_T;

typedef struct {
    TUYA_BT_GAP_ADDR_T addr; /**< Address of the remote device. */
    uint32_t passkey;        /**< respond passkey. */

    void *user_data;
} TUYA_BT_PAIR_DEVICE_T;

typedef struct {
    TUYA_BT_GAP_ADDR_T addr; /**< Disconnection handle on which the event occured.*/
    uint32_t reason;         /**< Disconnection Reason */
    void *user_data;
} TUYA_BT_DISCONNECT_EVT_T;

typedef struct {
    TUYA_BT_PAIR_REQUEST_T req; /**< pair request */
    uint32_t passkey;           /**< Init passkey. */
    void *user_data;
} TUYA_BT_PAIR_BOND_EVT_T;

typedef struct {
    TUYA_BT_GAP_ADDR_T addr; /**< Address of the remote device. */
    uint8_t *name;           /**< BT name of the remote device. */
    uint8_t name_len;
    void *user_data;
} TUYA_BT_PAIR_INQUIRY_EVT_T;

typedef struct {
    TUYA_BT_BREDR_STEAM_STATUS_E status; /**< Stream Status */

    void *p_endpoint;   /**< [Reserved] Stream Endpoint Pointer */
    void *p_connection; /**< [Reserved] Stream Connection Pointer */
    void *user_data;    /**< Stream User Data */
} TUYA_BT_BREDR_STEAM_T;

typedef struct {
    TUYA_BT_BREDR_PHONE_STATUS_E status; /**< Phone Status */
    uint8_t user_data_len;               /**< User Data Length */
    void *user_data;                     /**< Stream User Data */
} TUYA_BT_BREDR_PHONE_T;

typedef struct {
    TUYA_BT_BREDR_EVENT_TYPE_E type; /**< Tuya BR-EDR Event */
    int result;                      /**< Indiacte event result for bluetooth callback */

    union {
        TUYA_BT_PAIR_BOND_EVT_T pair;        /**< Pairing Event callback */
        TUYA_BT_PAIR_INQUIRY_EVT_T device;   /**< Information of device which is inquiry */
        TUYA_BT_DISCONNECT_EVT_T disconnect; /**< Disconnect Event callback */
        TUYA_BT_PAIR_BOND_INFO_T bond; /**< After paring successfully, we will report link key,  Version 5.2 | Vol 2,
                                          Part F, Figure 3.10 If fail, we will report NULL and report fail result */
        TUYA_BT_BREDR_STEAM_T audio;   /**< Tuya Bluetooth Audio Streaming Callback */
        TUYA_BT_BREDR_PHONE_T phone;   /**< Tuya Bluetooth Phone Callback */
    } profile_event;
} TUYA_BT_BREDR_EVENT_T;

/***********************************************************************
 ********************* variable ****************************************
 **********************************************************************/
/**< Tuya Bluetooth BR-EDR Callback Register function definition */
typedef void (*TUYA_BT_BREDR_EVT_FUNC_CB)(TUYA_BT_BREDR_EVENT_T *p_event);

/***********************************************************************
 ********************* function ****************************************
 **********************************************************************/

/**
 * @brief   Init the Bluetooth BR-EDR Interface.
 * @param   [in] role: Init the bt bredr role;
 *          [in] p_event: register callback;
 *          [in] user_data: Init the user data;
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_init(TUYA_BT_BREDR_ROLE_E role, TUYA_BT_BREDR_EVT_FUNC_CB p_event, void *user_data);

/**
 * @brief   De-Init the Bluetooth BR-EDR Interface.
 * @param   [in] role: De-init the bt bredr role;
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_deinit(TUYA_BT_BREDR_ROLE_E role);

/**
 * @brief   Reset the Bluetooth BR-EDR Interface.
 * @param   [in] role: Reset the bt bredr role;
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_reset(TUYA_BT_BREDR_ROLE_E role);

/**
 * @brief   Set the Bluetooth BR-EDR pair mode Interface.
 * @param   [in] pair: Bluetooth BR-EDR pair mode;
 *          [in] user_data: Init the user data;
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_pair_set(TUYA_BT_PAIR_INIT_PARAM_T pair);

/**
 * @brief   Enable the Bluetooth BR-EDR Interface.
 * @param   [in] mode: see TUYA_BT_SCAN_MODE_E
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_enable(TUYA_BT_SCAN_MODE_E mode);

/**
 * @brief   Enable the Bluetooth BR-EDR Interface.
 * @param   [in] enable: TRUE: Enable the bluetooth bredr page.
 *                       FALSE: Disable the bluetooth bredr page.
 *          [in] peer_addr: peer address for device which should be paging. if is NULL, will page the last device
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_page_enable(BOOL_T enable, TUYA_BT_GAP_ADDR_T *p_peer_addr);

/**
 * @brief   Enable the Bluetooth BR-EDR Interface.
 * @param   [in] enable: TRUE: Enable the bluetooth bredr inquiry.
 *                       FALSE: Disable the bluetooth bredr inquiry.
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_inquiry_enable(BOOL_T enable);

/**
 * @brief   Set the BT Address
 * @param   [in] p_peer_addr: set peer address for BT;
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_address_set(TUYA_BT_GAP_ADDR_T const *p_peer_addr);

/**
 * @brief   Get the BT Address
 * @param   [out] p_peer_addr: get peer address for BT;
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_address_get(TUYA_BT_GAP_ADDR_T *p_peer_addr);

/**
 * @brief   Set the BT GAP Name
 * @param   [in] p_peer_addr: set local bluetooth gap name for BT;
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_name_set(char *name);

/**
 * @brief   Get the BT GAP Name
 * @param   [in] p_peer_addr: get local bluetooth gap name for BT;
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_name_get(char *name);

/**
 * @brief   Request the pair while in BT-Master Mode.
 * @param   [in] mode: request security pairing mode for BT;
 *          [in] bond_info: post the major information for pairing.
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_paring_request(TUYA_BT_PAIR_DEVICE_T *p_device);

/**
 * @brief   Send paring passkey when in keyboard mode.
 * @param   [in] passkey: eg: 0x0001E240 means the passkey is 123456
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_paring_passkey_send(uint32_t passkey);

/**
 * @brief   Enable or Disable pair when pairing request
 * @param   [in] en: TRUE for enable and FALSE for disable pair
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_paring_enable_send(BOOL_T en);

/**
 * @brief   Delete the pair informations.
 * @param   [in] bond_info: delete the bond info.
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_paring_delete(TUYA_BT_PAIR_BOND_INFO_T *bond_info);

/**
 * @brief   check if device is paired
 * @param   [in] NULL
 *
 * @return  TRUE
 *          FALSE
 * */
BOOL_T tkl_bt_pairing_status_get(void);

/**
 * @brief   disconnect the link.
 * @param   [in] bond_info: disconnect the bond info.
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_gap_disconnect(TUYA_BT_PAIR_BOND_INFO_T *bond_info);

/**
 * @brief   Control the audio or phone, please refer to @TUYA_BT_BREDR_CONTROL_E.
 * @param   [in] crtl_event: control operations.
 *          [in] user_data: post the user-data for audio-control.
 *          [in] data_len: post the user-data length for audio-control.
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_control(TUYA_BT_BREDR_CONTROL_E crtl_event, uint8_t *user_data, uint16_t data_len);

/**
 * @brief   Control the bredr eq
 * @param   [in] eq_mode: control equalizer mode.
 *          [in] eq_data: post the eq-data for eq.
 *          [in] eq_data_len: post the eq-data length for eq.
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_equalizer_set(uint8_t eq_mode, uint8_t *eq_data, uint16_t eq_data_len);

/**
 * @brief   Swicth the bredr eq mode
 * @param   [in] eq_mode: Switch equalizer mode.
 *          [in] enable: Enable the current mode or not
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_equalizer_switch(uint8_t eq_mode, BOOL_T enable);

/**
 * @brief   Control the bredr noise
 * @param   [in] noise_mode: control noise mode.
 *          [in] noise_data: post the noise data.
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_noise_set(uint8_t noise_mode, uint16_t noise_data);

/**
 * @brief   Swicth the bredr noise mode
 * @param   [in] noise_mode: Switch noise mode.
 *          [in] enable: Enable the current mode or not
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_bt_bredr_noise_switch(uint8_t noise_mode, BOOL_T enable);

#ifdef __cplusplus
}
#endif

#endif /* __TKL_BLUETOOTH_BREDR_H__ */
