/**
 * @file tuya_wifi_ultra_lowpower.h
 * @brief Ultra Low Power WiFi initialization interface
 * @version 0.1
 * @date 2022-08-02
 *
 * @copyright Copyright (c) 2021-2022 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#ifndef _TUYA_WIFI_ULP_H_
#define _TUYA_WIFI_ULP_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Ultra Low Power WiFi mode.
 *
 * This function initializes the lpmgr reference counting manager,
 * subscribes to system events (MQTT connected, device state change, etc.),
 * and registers an initial TY_LP_DISABLE lock that is released after 10 seconds.
 *
 * @retval OPRT_OK    success
 * @retval Other      fail
 */
OPERATE_RET tuya_wifi_ulp_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _TUYA_WIFI_ULP_H_ */
