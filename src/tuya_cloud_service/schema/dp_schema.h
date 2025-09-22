/**
 * @file dp_schema.h
 * @brief Header file for defining data point (DP) schemas in Tuya SDK.
 *
 * This file contains definitions and structures for representing and managing
 * data point (DP) schemas within the Tuya IoT platform. It includes definitions
 * for DP property types (e.g., boolean, value, string, enumeration, bitmap) and
 * DP command types (e.g., LAN, MQTT) used in device-to-cloud and
 * device-to-device communication. These schemas are essential for defining the
 * data structure and communication protocol for smart devices integrated with
 * the Tuya IoT platform.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __DP_SCHEMA_H__
#define __DP_SCHEMA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "cJSON.h"
#include "tal_mutex.h"

#define DEV_ID_LEN 25

/**
 * @brief  Definition of dp property type
 */
// typedef uint8_t DP_PROP_TP_E;
typedef uint8_t dp_prop_tp_t;
#define PROP_BOOL   0
#define PROP_VALUE  1
#define PROP_STR    2
#define PROP_ENUM   3
#define PROP_BITMAP 4

/**
 * @brief tuya sdk dp cmd type
 */
typedef uint8_t dp_cmd_type_t;
#define DP_CMD_LAN      0 // cmd from LAN
#define DP_CMD_MQ       1 // cmd from MQTT
#define DP_CMD_BT       5 // cmd from BT
#define DP_CMD_AI_SKILL 9 // cmd from AI skill

/**
 * @brief  Definition of dp report type
 */
typedef uint8_t dp_rept_type_t;
#define T_OBJ_REPT      0  // dp is value,str,enum,bool,bitmap
#define T_RAW_REPT      1  // raw type
#define T_STAT_REPT     2  // stat type
#define T_RE_TRANS_REPT 10 // repeat report

/**
 * @brief  Definition of dp type
 */
typedef uint8_t dp_type_t;
#define T_OBJ  0 // dp is value,str,enum,bool,bitmap
#define T_RAW  1 // raw type
#define T_FILE 2 // file type

/**
 * @brief  Definition of dp mode
 */
typedef uint8_t dp_mode_t;
#define M_RW 0 // cloud/app can read and write
#define M_WR 1 // cloud/app can only write to device
#define M_RO 2 // cloud/app can only read from device

/**
 * @brief tuya sdk dp trans type
 */
typedef uint8_t dp_trans_type_t;
#define DTT_SCT_UNC   0 // unicast
#define DTT_SCT_BNC   1 // boardcast
#define DTT_SCT_MNC   2 // multicast
#define DTT_SCT_SCENE 3 // scene

/**
 * @brief  Definition of map type
 */
typedef uint8_t UP_MAP_TYPE_E;
#define UP_CLEANER_MAP      0
#define UP_CLEANER_PATH     1
#define UP_CLEANER_MAP_PATH 2

/**
 * @brief  Definition of dp trigger type
 */
typedef uint8_t dp_trig_t;
#define TRIG_PULSE  0 // upload when value changes
#define TRIG_DIRECT 1 // upload without value change check

/**
 * @brief  Definition of dp positive upload policy
 */
typedef uint8_t dp_psv_t;
#define PSV_FALSE  0 /* disabled */
#define PSV_TRUE   1 /* app hasn't query yet */
#define PSV_F_ONCE 2 /* app has queryed, shoulb report */

/**
 * @brief  Definition of dp statistics type
 */
typedef uint8_t dp_stat_tp_t;
#define DST_NONE  0 // no need statistics
#define DST_INC   1 // dp statistics increase
#define DST_TOTAL 2 // dp statistics total

//
/**
 * @brief  dp transport channel priority
 */
typedef uint8_t dp_route_t;
#define ROUTE_DEFAULT   0
#define ROUTE_BLE_FIRST 1
#define ROUTE_FORCE_BT  2 // dp transport force by ble

/**
 * @brief Definiton of DP status
 */
typedef uint8_t dp_pv_stat_t;

#define PV_STAT_INVALID 0
#define PV_STAT_LOCAL   1
#define PV_STAT_ULING   2
#define PV_STAT_CLOUD   3

typedef union {
    int dp_value;       // valid when dp type is value
    uint32_t dp_enum;   // valid when dp type is enum
    char *dp_str;       // valid when dp type is str
    bool dp_bool;       // valid when dp type is bool
    uint32_t dp_bitmap; // valid when dp type is bitmap
} dp_value_t;

/**
 * @brief Definition of structured dp
 */
typedef struct {
    /** dp id */
    uint8_t id;
    /** dp type, see DP_PROP_TP_E */
    dp_prop_tp_t type;
    /** dp value, see TY_OBJ_DP_VALUE_U */
    dp_value_t value;
    /** dp happen time. if 0, mean now */
    TIME_T time_stamp;
} dp_obj_t;

/**
 * @brief Definition of structured dp
 */
typedef struct {
    /** dp id */
    uint8_t id;
    /** count of dp */
    uint16_t len;
    /** the dp data */
    uint8_t data[0];
} dp_raw_t;

/**
 * @brief Definition of recved structured dp
 */
typedef struct {
    /** see dp_cmd_type_t */
    dp_cmd_type_t cmd_tp;
    // /** see dp_trans_type_t */
    dp_trans_type_t dtt_tp;
    /** if(NULL == cid) then then the cid represents gwid */
    char *devid;
    /** count of dp */
    uint16_t dpscnt;
    /** the dp data */
    dp_obj_t dps[0];
} dp_obj_recv_t;

typedef struct {
    /** see dp_cmd_type_t */
    dp_cmd_type_t cmd_tp;
    // /** see dp_trans_type_t */
    dp_trans_type_t dtt_tp;
    /** if(NULL == cid) then then the cid represents gwid */
    char *devid;

    dp_raw_t dp;
} dp_raw_recv_t;

/**
 * @brief Definition of dp description
 */
typedef struct {
    /** dp id */
    uint8_t id;
    /** see dp_mode_t */
    dp_mode_t mode;
    /** see dp_psv_t */
    dp_psv_t passive;
    /** see dp_type_t */
    dp_type_t type;
    /** see DP_PROP_TP_E */
    dp_prop_tp_t prop_tp;
    /** see dp_trig_t */
    dp_trig_t trig;
    /** see dp_stat_tp_t */
    dp_stat_tp_t stat;
    /** see dp_route_t */
    dp_route_t route_t;
} dp_desc_t;

/**
 * @brief  Definition of dp prop
 */
typedef struct {
    /** min value */
    int min;
    /** max value */
    int max;
    /** step */
    int16_t step;
    /** scale */
    uint16_t scale;
    /** dp value */
    int value;
} dp_prop_int_t;

/**
 * @brief  Definition of dp prop
 */
typedef struct {
    /** enum count */
    int cnt;
    /** enum value */
    char **pp_enum;
    /** current value */
    int value;
} dp_prop_enum_t;

/**
 * @brief  Definition of dp prop
 */
typedef struct {
    /** max len */
    int max_len;
    /** cur len */
    int cur_len;
    /** mutex */
    void *dp_str_mutex;
    /** dp value */
    char *value;
} dp_prop_str_t;

/**
 * @brief  Definition of dp prop
 */
typedef struct {
    /** bool value */
    bool value;
} dp_prop_bool_t;

/**
 * @brief  Definition of dp prop
 */
typedef struct {
    /** max len */
    uint32_t max_len;
    /** value */
    uint32_t value;
} dp_prop_bitmap_t;

/**
 * @brief  Definition of dp prop value
 */
typedef union {
    /** see dp_prop_int_t */
    dp_prop_int_t prop_int;
    /** see dp_prop_enum_t */
    dp_prop_enum_t prop_enum;
    /** see dp_prop_str_t */
    dp_prop_str_t prop_str;
    /** see dp_prop_bool_t */
    dp_prop_bool_t prop_bool;
    /** see dp_prop_bitmap_t */
    dp_prop_bitmap_t prop_bitmap;
} dp_prop_vaule_t;

/**
 * @brief Definition of dp  control
 */
typedef struct {
    /** see dp_desc_t */
    dp_desc_t desc;
    /** see dp_prop_vaule_t */
    dp_prop_vaule_t prop;
    /** cache status, see dp_pv_stat_t */
    dp_pv_stat_t pv_stat;
    uint8_t uling_cnt;
    /** see DP_REPT_FLOW_CTRL */
    // DP_REPT_FLOW_CTRL rept_flow_ctrl;
    /** time stamp for dp sync */
    TIME_T time_stamp;
    /** sn for ble dp sync report */
    // uint32_t ble_send_sn;
} dp_node_t; // dp_obj_t

/**
 * @brief Definition of device actv attribute
 */

typedef struct {
    /** is attach dp info or not */
    bool attach_dp_if;
    /** need preprecess or not */
    bool preprocess;
} dp_prop_actv_t;

// typedef struct dev_cntl_n_s {

typedef struct {
    /** virtual id */
    char devid[DEV_ID_LEN + 1];
    /** device attribute, see DEV_ACTV_ATTR_S */
    dp_prop_actv_t actv;
    /** exclusive access to dp */
    MUTEX_HANDLE mutex;
    /** count of dp */
    uint8_t num;
    /** dp info */
    dp_node_t node[0];
} dp_schema_t;

/**
 * @brief Definition of schema other attribute
 */
typedef struct {
    /** need preprecess or not */
    bool preprocess;
} SCHEMA_OTHER_ATTR_S;

#define MAX_DP_NUM 64

#define DP_REPT_NO_FILTER_FLAG  (1 << 0)
#define DP_DUMP_STAT_LOCAL_FLAG (1 << 1)
#define DP_APPEND_HEADER_FLAG   (1 << 2)

typedef struct {
    char *devid;
    dp_cmd_type_t cmd;
    dp_trans_type_t dt_tp;
    cJSON *data_js;
    void *user_data;
} dp_recv_msg_t;

typedef struct {
    dp_rept_type_t rept_type;
    uint32_t flags;
    dp_raw_t *dp;
    uint8_t dpscnt;
    dp_obj_t *dps;
} dp_rept_in_t;

typedef struct {
    char *dpsjson;
    char *timejson;
} dp_rept_out_t;

typedef struct {
    uint8_t num;
    uint16_t len;
    uint16_t timelen;
    dp_schema_t *schema;
    uint8_t dpid[0];
} dp_rept_valid_t;

typedef void (*dp_recv_cb_t)(dp_type_t type, void *dp_data, void *user_data);

/**
 * @brief Finds a DP node in the given schema based on the ID.
 *
 * This function searches for a DP node with the specified ID in the given
 * schema.
 *
 * @param schema The schema to search in.
 * @param id The ID of the DP node to find.
 * @return A pointer to the found DP node, or NULL if not found.
 */
dp_node_t *dp_node_find(dp_schema_t *schema, int id);

/**
 * Finds a DP node by device ID and ID.
 *
 * This function searches for a DP node in the schema based on the given device
 * ID and ID.
 *
 * @param devid The device ID to search for.
 * @param id The ID of the DP node to search for.
 * @return A pointer to the found DP node, or NULL if not found.
 */
dp_node_t *dp_node_find_by_devid(char *devid, int id);

/**
 * @brief Finds the data point schema for a given device ID.
 *
 * This function searches for the data point schema associated with the
 * specified device ID.
 *
 * @param devid The device ID for which to find the data point schema.
 * @return A pointer to the `dp_schema_t` structure representing the data point
 * schema, or `NULL` if not found.
 */
dp_schema_t *dp_schema_find(const char *devid);

/**
 * @brief Sets the PV (Property Value) status for a specific ID in the given
 * schema.
 *
 * This function is used to set the PV status for a specific ID in the provided
 * schema.
 *
 * @param schema Pointer to the schema structure.
 * @param id The ID of the property.
 * @param pv_stat The PV status to be set.
 */

void dp_pv_stat_set(dp_schema_t *schema, uint8_t id, dp_pv_stat_t pv_stat);

/**
 * Retrieves the current status of a data point (DP) based on its ID.
 *
 * @param schema The pointer to the DP schema.
 * @param id The ID of the data point.
 * @return The current status of the data point.
 */
dp_pv_stat_t dp_pv_stat_get(dp_schema_t *schema, uint8_t id);

/**
 * @brief Parses the received data and invokes the callback function.
 *
 * This function parses the received data using the provided `msg` parameter and
 * invokes the callback function `dp_recv_cb` to handle the parsed data.
 *
 * @param msg Pointer to the received message structure.
 * @param dp_recv_cb Callback function to handle the parsed data.
 * @return Returns an integer value indicating the status of the parsing
 * operation. A non-zero value indicates success, while a zero value indicates
 * failure.
 */
int dp_data_recv_parse(dp_recv_msg_t *msg, dp_recv_cb_t dp_recv_cb);

/**
 * @brief Performs a validity check on the given data point (DP) schema and
 * input.
 *
 * This function checks if the provided data point schema and input are valid
 * according to the specified rules.
 *
 * @param schema Pointer to the data point schema structure.
 * @param dpin Pointer to the data point input structure.
 * @param dpvalid Pointer to the data point validity structure.
 * @return Returns an integer value indicating the result of the validity check.
 *         - 0: The data point is valid.
 *         - Other values: The data point is invalid, and the value indicates
 * the specific error code.
 */
int dp_rept_valid_check(dp_schema_t *schema, dp_rept_in_t *dpin, dp_rept_valid_t *dpvalid);
/**
 * @brief Outputs the JSON representation of a device property (DP) report.
 *
 * This function takes a DP schema, input data, validation information, and
 * output data to generate the JSON representation of a DP report.
 *
 * @param schema The DP schema structure that defines the format of the DP
 * report.
 * @param dpin The input data for the DP report.
 * @param dpvalid The validation information for the DP report.
 * @param dpout The output data for the DP report.
 * @return Returns an integer value indicating the success or failure of the
 * operation.
 */
int dp_rept_json_output(dp_schema_t *schema, dp_rept_in_t *dpin, dp_rept_valid_t *dpvalid, dp_rept_out_t *dpout);

/**
 * Appends a JSON string to the given data point schema.
 *
 * @param schema The data point schema to append the JSON string to.
 * @param data The JSON string to append.
 * @param time The time associated with the JSON string.
 * @param type The type of the JSON string.
 * @param rept_seq The sequence number of the JSON string.
 * @param pp_out A pointer to the output string.
 * @return An integer indicating the success or failure of the operation.
 */
int dp_rept_json_append(dp_schema_t *schema, char *data, char *time, char *type, uint8_t rept_seq, char **pp_out);

/**
 * @brief Creates a new data point schema for a device.
 *
 * This function creates a new data point schema for a device identified by the
 * given device ID. The schema is defined by the provided JSON string.
 *
 * @param devid The device ID for which the schema is being created.
 * @param schema_json The JSON string defining the data point schema.
 * @param dp_schema_out A pointer to a variable that will hold the created data
 * point schema.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int dp_schema_create(char *devid, char *schema_json, dp_schema_t **dp_schema_out);
/**
 * @brief Deletes the data point schema for a specific device.
 *
 * This function deletes the data point schema associated with the specified
 * device.
 *
 * @param devid The device ID for which the data point schema needs to be
 * deleted.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int dp_schema_delete(char *devid);

/**
 * @brief Dumps all the JSON objects for the specified device ID.
 *
 * This function generates a JSON string containing all the data points (DPs)
 * for the specified device ID. The generated JSON string can be used for
 * debugging or analysis purposes.
 *
 * @param devid The device ID for which to dump the JSON objects.
 * @param flags Additional flags to control the behavior of the function
 * (optional).
 *
 * @return A pointer to the generated JSON string. The caller is responsible for
 * freeing the memory allocated for the string.
 */
char *dp_obj_dump_all_json(char *devid, int flags);

/**
 * @brief Dumps the status of a device object to a local JSON string.
 *
 * This function takes a device ID and retrieves the status of the device object
 * associated with that ID. It then converts the status into a JSON string
 * representation and returns it.
 *
 * @param devid The ID of the device.
 * @param outdpvalid A pointer to a variable that will hold the device object's
 * validity information.
 * @param outjson A pointer to a variable that will hold the generated JSON
 * string.
 * @param flags Additional flags to control the behavior of the function.
 *
 * @return An integer value indicating the success or failure of the function.
 *         0 indicates success, while a negative value indicates an error.
 */
int dp_obj_dump_stat_local_json(char *devid, dp_rept_valid_t **outdpvalid, char **outjson, int flags);

#ifdef __cplusplus
}
#endif
#endif
