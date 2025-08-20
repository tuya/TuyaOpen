/**
 * @file tal_cellular_keypad.h
 * @author www.tuya.com
 * @brief 蜂窝模组键盘功能API实现接口。
 *
 * @copyright Copyright (c) tuya.inc 2022
 */

#ifndef __TAL_CELLULAR_KEYPAD_H__
#define __TAL_CELLULAR_KEYPAD_H__

#include "tuya_cloud_types.h"
#if !defined(ENABLE_CELLULAR_PLUGIN) || ENABLE_CELLULAR_PLUGIN == 0
#include "tkl_cellular_base.h"
#include "tkl_cellular_keypad.h"
#else
// 包含at版本的蜂窝接口头文件
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief init tuya cellular keypad service.
 * @note if keypad service init failed, system will be crash.
 */
OPERATE_RET tal_cellular_keypad_init(void *param);

/**
 * @brief add key event listener to keypad service.
 * @param keyId   key id
 * @param cb      user defined key event callback function
 * @param ctx     argument for @cb
 * @return listener instance or NULL
 */
TUYA_CELLULAR_KEY_LISTENER tal_cellular_keypad_key_listener_add(TUYA_KEYMAP_E keyId, TUYA_CELLULAR_KEY_CB cb, void *ctx);

/**
 * @brief delete key event listener from keypad service.
 * @param listener    listener to delete
 * @return true: success  false: failed
 */
OPERATE_RET tal_cellular_keypad_key_listener_delete(TUYA_CELLULAR_KEY_LISTENER listener);


/**
 * @brief get current key state.
 * @param keyId    key id
 * @return
 *     -1               fail
 *     TUYA_KEY_PRESS   the key is pressed
 *     TUYA_KEY_RELEASE the key is released
 */
OPERATE_RET tal_cellular_keypad_key_state_get(TUYA_KEYMAP_E keyId,TUYA_KEYSTATE_E *state);

/**
 * @brief keypad ioctl.
 * @param cmd
 * @param argv
 * @return OPRT_OK success, other failed
 */
OPERATE_RET tal_cellular_keypad_key_ioctl(int cmd,void *argv);

#ifdef __cplusplus
}
#endif

#endif
