#include "tal_cellular_keypad.h"
#include "tkl_init_cellular.h"

/**
 * @brief init tuya cellular keypad service.
 * @note if keypad service init failed, system will be crash.
 */
OPERATE_RET tal_cellular_keypad_init(void *param)
{
    if(tkl_cellular_keypad_desc_get()->init == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_keypad_desc_get()->init(param);
}

/**
 * @brief add key event listener to keypad service.
 * @param keyId   key id
 * @param cb      user defined key event callback function
 * @param ctx     argument for @cb
 * @return listener instance or NULL
 */
TUYA_CELLULAR_KEY_LISTENER tal_cellular_keypad_key_listener_add(TUYA_KEYMAP_E keyId, TUYA_CELLULAR_KEY_CB cb, void *ctx)
{
    if(tkl_cellular_keypad_desc_get()->key_listener_add == NULL) {
        return NULL;
    }
    return tkl_cellular_keypad_desc_get()->key_listener_add(keyId, cb, ctx);
}

/**
 * @brief delete key event listener from keypad service.
 * @param listener    listener to delete
 * @return true: success  false: failed
 */
OPERATE_RET tal_cellular_keypad_key_listener_delete(TUYA_CELLULAR_KEY_LISTENER listener)
{
    if(tkl_cellular_keypad_desc_get()->key_listener_delete == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_keypad_desc_get()->key_listener_delete(listener);
}

/**
 * @brief get current key state.
 * @param keyId    key id
 * @return
 *     -1               fail
 *     TUYA_KEY_PRESS   the key is pressed
 *     TUYA_KEY_RELEASE the key is released
 */
OPERATE_RET tal_cellular_keypad_key_state_get(TUYA_KEYMAP_E keyId,TUYA_KEYSTATE_E *state)
{
    if(tkl_cellular_keypad_desc_get()->key_state_get == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_keypad_desc_get()->key_state_get(keyId, state);
}

/**
 * @brief keypad ioctl.
 * @param cmd
 * @param argv
 * @return OPRT_OK success, other failed
 */
OPERATE_RET tal_cellular_keypad_key_ioctl(int cmd,void *argv)
{
    if(tkl_cellular_keypad_desc_get()->key_ioctl == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_keypad_desc_get()->key_ioctl(cmd, argv);
}