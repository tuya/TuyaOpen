/**
 * @file tkl_cellular_keypad.h
 * @author www.tuya.com
 * @brief Cellular module keypad function API implementation interface.
 *
 * @copyright Copyright (c) tuya.inc 2022
 */

#ifndef __TKL_CELLULAR_KEYPAD_H__
#define __TKL_CELLULAR_KEYPAD_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief tuya cellular module keypad logic definition
 */
typedef enum {
    TUYA_KEY_MAP_POWER = 0, ///< power key
    TUYA_KEY_MAP_0,         ///< '0'
    TUYA_KEY_MAP_1,         ///< '1'
    TUYA_KEY_MAP_2,         ///< '2'
    TUYA_KEY_MAP_3,         ///< '3'
    TUYA_KEY_MAP_4,         ///< '4'
    TUYA_KEY_MAP_5,         ///< '5'
    TUYA_KEY_MAP_6,         ///< '6'
    TUYA_KEY_MAP_7,         ///< '7'
    TUYA_KEY_MAP_8,         ///< '8'
    TUYA_KEY_MAP_9,         ///< '9'
    TUYA_KEY_MAP_STAR,      ///< '*'
    TUYA_KEY_MAP_SHARP,     ///< '#'
    TUYA_KEY_MAP_LEFT,      ///< left
    TUYA_KEY_MAP_RIGHT,     ///< right
    TUYA_KEY_MAP_UP,        ///< up
    TUYA_KEY_MAP_DOWN,      ///< down
    TUYA_KEY_MAP_OK,        ///< ok
    TUYA_KEY_MAP_CANCLE,    ///< cancel
    TUYA_KEY_MAP_SOFT_L,    ///< left soft key
    TUYA_KEY_MAP_SOFT_R,    ///< rigfht soft key
    TUYA_KEY_MAP_SIM1,      ///< customized
    TUYA_KEY_MAP_SIM2,      ///< customized
    TUYA_KEY_MAP_MAX_COUNT  ///< total count
} TUYA_KEYMAP_E;

/**
 * @brief tuya cellular module key state definition
 */
typedef enum {
    TUYA_KEY_PRESS = (1 << 0),   ///< key pressed
    TUYA_KEY_RELEASE = (1 << 1), ///< key released
} TUYA_KEYSTATE_E;

/**
 * @brief tuya cellular key listener definition
 */
typedef void*  TUYA_CELLULAR_KEY_LISTENER;

/**
 * @brief tuya cat1 key event process callback defintion
 */
typedef void (*TUYA_CELLULAR_KEY_CB)(TUYA_KEYMAP_E keyId, TUYA_KEYSTATE_E state, void *ctx);

/**
 * @brief init tuya cellular keypad service.
 *
 * @note
 *      if keypad service init failed, system will be crash.
 */
OPERATE_RET tkl_cellular_keypad_init(void *param);

/**
 * @brief add key event listener to keypad service.
 *
 * @param keyId   key id
 * @param cb      user defined key event callback function
 * @param ctx     argument for @cb
 * @return
 *        listener instance or NULL
 */
TUYA_CELLULAR_KEY_LISTENER tkl_cellular_keypad_key_listener_add(TUYA_KEYMAP_E keyId, TUYA_CELLULAR_KEY_CB cb, void *ctx);

/**
 * @brief delete key event listener from keypad service.
 *
 * @param listener    listener to delete
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_keypad_key_listener_delete(TUYA_CELLULAR_KEY_LISTENER listener);

/**
 * @brief get current key state.
 *
 * @param keyId[in]    key id
 * @param state[out]    @TUYA_KEYSTATE_E
 *     TUYA_KEY_PRESS   the key is pressed
 *     TUYA_KEY_RELEASE the key is released
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_keypad_key_state_get(TUYA_KEYMAP_E keyId,TUYA_KEYSTATE_E *state);

/**
 * @brief keypad ioctl.
 *
 * @param cmd
 * @param argv
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_keypad_key_ioctl(int cmd,void *argv);

#ifdef __cplusplus
}
#endif

#endif
