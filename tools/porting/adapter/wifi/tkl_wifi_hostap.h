/**
 * @file tkl_wifi.h
 * @brief Common process - adapter the wi-fi hostap api
 * @version 0.1
 * @date 2020-11-09
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_WIFI_HOSTAP_H__
#define __TKL_WIFI_HOSTAP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief set ioctl commands to wlan driver for tuyaos wifi connection.
 * @param[in]       dev         number of sockt fd
 * @param[in]       vif_index   virtual interface index
 * @param[in]       cmd         ioctl command number
 * @param[in]       arg         ioctl parameters pointer
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_hostap_ioctl_inet(int dev, int vif_index, uint32_t cmd, uint32_t arg);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_WIFI_HOSTAP_H__
