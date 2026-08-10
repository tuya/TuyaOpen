/**
 * @file app_lowpower.h
 * @brief Idle low-power (power-management) integration layer for the chatbot app.
 *
 * Wraps the tuya_pm scheme/consumer/gate wiring that lets the device idle from ACTIVE into
 * WiFi AP keep-alive (ULP_ONLINE) or deep sleep in key/push-to-talk mode. All entry points
 * are no-ops when ENABLE_APP_LOWPOWER is off, so the caller (app_chat_bot) needs no gating.
 *
 * @version 0.1
 * @date 2025-03-25
 */

#ifndef __APP_LOWPOWER_H__
#define __APP_LOWPOWER_H__

#include "tuya_cloud_types.h"
#include "ai_user_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring up the idle low-power stack: pm chain + mic/display consumers + mode/link
 *        gate locks. Call AFTER ai_chat_init. No-op unless ENABLE_APP_LOWPOWER.
 */
void app_lowpower_init(void);

/**
 * @brief Feed chatbot user events into power management (activity / key-mode gate).
 *        No-op unless ENABLE_APP_LOWPOWER.
 *
 * @param[in] event the chatbot user event.
 */
void app_lowpower_feed_event(AI_NOTIFY_EVENT_T *event);

/**
 * @brief Cloud online/offline gate: releasing it (when online) allows idle descent; pinning
 *        it (when offline) keeps ACTIVE. No-op unless ENABLE_APP_LOWPOWER.
 *
 * @param[in] online TRUE if cloud-associated/online.
 */
void app_lowpower_set_online(BOOL_T online);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LOWPOWER_H__ */
