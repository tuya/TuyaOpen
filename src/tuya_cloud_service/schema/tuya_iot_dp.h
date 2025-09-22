/**
 * @file tuya_iot_dp.h
 * @brief Header file for Tuya IoT Data Point (DP) operations.
 *
 * This file provides the function declarations for Tuya IoT Data Point (DP)
 * operations, including parsing DPs, creating DP schemas, reporting object and
 * raw type DPs, and dumping DP objects. It serves as an interface for managing
 * and interacting with data points, which are the fundamental units of data
 * exchange between IoT devices and the Tuya cloud platform. These operations
 * are essential for the implementation of device control and status reporting
 * functionalities in IoT applications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_IOT_DP_H__
#define __TUYA_IOT_DP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_iot.h"

/**
 * @brief
 *
 * @param client
 * @param tp
 * @param cmd_js
 * @return int
 */
int tuya_iot_dp_parse(tuya_iot_client_t *client, dp_cmd_type_t tp, cJSON *cmd_js);

/**
 * @brief
 *
 * @param devid
 * @param schema_json
 * @param dp_schema_out
 * @return int
 */
int tuya_iot_dp_schema_create(char *devid, char *schema_json, dp_schema_t **dp_schema_out);

/**
 * @brief
 *
 * @param client
 * @param devid
 * @param dps
 * @param dpscnt
 * @param flags
 * @return int
 */
int tuya_iot_dp_obj_report(tuya_iot_client_t *client, const char *devid, dp_obj_t *dps, uint16_t dpscnt, int flags);

/**
 * @brief
 *
 * @param client
 * @param devid
 * @param dp
 * @param timeout
 * @return int
 */
int tuya_iot_dp_raw_report(tuya_iot_client_t *client, const char *devid, dp_raw_t *dp, uint32_t timeout);

/**
 * @brief
 *
 * @param client
 * @param devid
 * @param flags
 * @return char*
 */
char *tuya_iot_dp_obj_dump(tuya_iot_client_t *client, char *devid, int flags);

#ifdef __cplusplus
}
#endif
#endif
