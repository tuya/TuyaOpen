/**
 * @file app_launcher.h
 * @brief Gravity-driven app launcher menu for NiceMCU-T5-0.96ISP
 * @version 0.1
 * @date 2026-08-11
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __APP_LAUNCHER_H__
#define __APP_LAUNCHER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start gravity launcher (menu + chat/sand/settings)
 * @return OPRT_OK on success
 * @note Call after app_chat_bot_init() and board_register_hardware().
 *       Tilt L/R to switch apps; flick right to enter; flick left to back.
 */
OPERATE_RET app_launcher_start(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LAUNCHER_H__ */
