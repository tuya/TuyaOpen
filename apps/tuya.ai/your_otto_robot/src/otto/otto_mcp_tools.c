/**
 * @file otto_mcp_tools.c
 * @brief Otto robot motion/control MCP tools (replaces cloud DP motion control)
 * @version 1.0
 * @date 2025-05-22
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "otto_mcp_tools.h"
#include "otto_robot_main.h"
#include "otto_robot_dp_profile.h"
#include "ai_mcp_server.h"
#include "tal_api.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define OTTO_MCP_ACTION_ID_MAX  12

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Map action name to internal ActionType id (V1 ptz_control strings)
 * @param[in] action_str Action name from MCP
 * @param[out] action_out Internal action id
 * @return true if mapped, false if unknown
 */
STATIC BOOL_T __otto_mcp_action_from_name(CONST CHAR_T *action_str, UINT32_T *action_out)
{
    if (action_str == NULL || action_out == NULL) {
        return FALSE;
    }
    if (strcmp(action_str, "front") == 0) {
        *action_out = 0;
    } else if (strcmp(action_str, "back") == 0) {
        *action_out = 1;
    } else if (strcmp(action_str, "left") == 0) {
        *action_out = 2;
    } else if (strcmp(action_str, "right") == 0) {
        *action_out = 3;
    } else if (strcmp(action_str, "none") == 0) {
        *action_out = 4;
    } else if (strcmp(action_str, "swing") == 0) {
        *action_out = 5;
    } else if (strcmp(action_str, "up_down") == 0) {
        *action_out = 6;
    } else if (strcmp(action_str, "bend") == 0) {
        *action_out = 7;
    } else if (strcmp(action_str, "jitter") == 0) {
        *action_out = 8;
    } else if (strcmp(action_str, "moonwalker") == 0) {
        *action_out = 9;
    } else if (strcmp(action_str, "jump") == 0) {
        *action_out = 10;
    } else if (strcmp(action_str, "show") == 0) {
        *action_out = 11;
    } else if (strcmp(action_str, "hand") == 0) {
        *action_out = 12;
    } else {
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Find integer property value by name
 * @param[in] properties MCP property list from tools/call
 * @param[in] name Property name
 * @param[in] default_val Value when property missing
 * @return Parsed integer or default
 */
STATIC INT_T __otto_mcp_get_int_prop(CONST MCP_PROPERTY_LIST_T *properties, CONST CHAR_T *name, INT_T default_val)
{
    INT_T i = 0;

    if (properties == NULL || name == NULL) {
        return default_val;
    }
    for (i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *prop = properties->properties[i];
        if (prop != NULL && strcmp(prop->name, name) == 0 &&
            prop->type == MCP_PROPERTY_TYPE_INTEGER) {
            return prop->default_val.int_val;
        }
    }
    return default_val;
}

/**
 * @brief Find string property value by name
 * @param[in] properties MCP property list from tools/call
 * @param[in] name Property name
 * @return String pointer or NULL
 */
STATIC CONST CHAR_T *__otto_mcp_get_str_prop(CONST MCP_PROPERTY_LIST_T *properties, CONST CHAR_T *name)
{
    INT_T i = 0;

    if (properties == NULL || name == NULL) {
        return NULL;
    }
    for (i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *prop = properties->properties[i];
        if (prop != NULL && strcmp(prop->name, name) == 0 &&
            prop->type == MCP_PROPERTY_TYPE_STRING && prop->default_val.str_val != NULL) {
            return prop->default_val.str_val;
        }
    }
    return NULL;
}

/**
 * @brief MCP tool: execute Otto robot action
 * @param[in] properties action (string) or action_id (int 0-12)
 * @param[out] ret_val MCP return value
 * @param[in] user_data Unused
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __otto_mcp_robot_action(CONST MCP_PROPERTY_LIST_T *properties,
                                           MCP_RETURN_VALUE_T *ret_val, VOID_T *user_data)
{
    UINT32_T action_id = 0;
    CONST CHAR_T *action_str = NULL;
    BOOL_T has_action = FALSE;

    (VOID)user_data;

    action_str = __otto_mcp_get_str_prop(properties, "action");
    if (action_str != NULL) {
        if (!__otto_mcp_action_from_name(action_str, &action_id)) {
            PR_ERR("otto_mcp unknown action: %s", action_str);
            ai_mcp_return_value_set_bool(ret_val, FALSE);
            return OPRT_INVALID_PARM;
        }
        has_action = TRUE;
    } else {
        action_id = (UINT32_T)__otto_mcp_get_int_prop(properties, "action_id", -1);
        if (action_id <= OTTO_MCP_ACTION_ID_MAX) {
            has_action = TRUE;
        }
    }

    if (!has_action) {
        PR_ERR("otto_mcp action: missing action or action_id");
        ai_mcp_return_value_set_bool(ret_val, FALSE);
        return OPRT_INVALID_PARM;
    }

    PR_DEBUG("otto_mcp run action id=%u", action_id);
    otto_robot_dp_proc_thread(action_id);
    ai_mcp_return_value_set_bool(ret_val, TRUE);
    return OPRT_OK;
}

/**
 * @brief MCP tool: set walk speed (0=slow, 1=normal, 2=fast)
 * @param[in] properties speed_level (int 0-2)
 * @param[out] ret_val MCP return value
 * @param[in] user_data Unused
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __otto_mcp_robot_speed_set(CONST MCP_PROPERTY_LIST_T *properties,
                                              MCP_RETURN_VALUE_T *ret_val, VOID_T *user_data)
{
    INT_T speed_level = 0;

    (VOID)user_data;

    speed_level = __otto_mcp_get_int_prop(properties, "speed_level", -1);
    if (speed_level < 0 || speed_level > 2) {
        PR_ERR("otto_mcp speed_level invalid: %d", speed_level);
        ai_mcp_return_value_set_bool(ret_val, FALSE);
        return OPRT_INVALID_PARM;
    }

    otto_robot_speed_dp_proc((UINT32_T)speed_level);
    PR_DEBUG("otto_mcp speed_level=%d", speed_level);
    ai_mcp_return_value_set_bool(ret_val, TRUE);
    return OPRT_OK;
}

/**
 * @brief MCP tool: set step count for locomotion actions
 * @param[in] properties steps (int 1-30)
 * @param[out] ret_val MCP return value
 * @param[in] user_data Unused
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __otto_mcp_robot_step_set(CONST MCP_PROPERTY_LIST_T *properties,
                                             MCP_RETURN_VALUE_T *ret_val, VOID_T *user_data)
{
    UINT32_T steps = 0;

    (VOID)user_data;

    steps = (UINT32_T)__otto_mcp_get_int_prop(properties, "steps", 0);
#if defined(OTTO_PRODUCT_ST7789_V1) && (OTTO_PRODUCT_ST7789_V1 == 1)
    if (steps < OTTO_STEP_MIN) {
        steps = OTTO_STEP_MIN;
    }
    if (steps > OTTO_STEP_MAX) {
        steps = OTTO_STEP_MAX;
    }
#else
    if (steps < 1) {
        steps = 1;
    }
    if (steps > 30) {
        steps = 30;
    }
#endif

    otto_robot_step_dp_proc(steps);
    PR_DEBUG("otto_mcp steps=%u", steps);
    ai_mcp_return_value_set_bool(ret_val, TRUE);
    return OPRT_OK;
}

/**
 * @brief Register Otto MCP tools (server must already be initialized)
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __otto_mcp_tools_register(VOID_T)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_GOTO(AI_MCP_TOOL_ADD(
        "otto_robot_action",
        "Execute an Otto robot motion. Use when the user asks the robot to move, dance, "
        "or perform a gesture.\n"
        "Parameters:\n"
        "- action (string, preferred): front, back, left, right, none, swing, up_down, "
        "bend, jitter, moonwalker, jump, show, hand.\n"
        "- action_id (int, optional): same as cloud ptz_control enum index 0-12.\n"
        "show preempts the current motion; other actions queue (max 8).\n"
        "Response: true if the action was accepted.",
        __otto_mcp_robot_action,
        NULL,
        MCP_PROP_STR("action",
                     "Motion name: front, back, left, right, none, swing, up_down, bend, "
                     "jitter, moonwalker, jump, show, hand."),
        MCP_PROP_INT_DEF_RANGE("action_id",
                               "ptz_control enum index (0-12), used if action string omitted.",
                               0, 0, 12)
    ), err);

    TUYA_CALL_ERR_GOTO(AI_MCP_TOOL_ADD(
        "otto_robot_speed_set",
        "Set Otto walk/motion speed.\n"
        "Parameters:\n"
        "- speed_level (int): 0=slow, 1=normal, 2=fast.\n"
        "Response: true if set successfully.",
        __otto_mcp_robot_speed_set,
        NULL,
        MCP_PROP_INT_RANGE("speed_level", "Speed: 0=slow, 1=normal, 2=fast.", 0, 2)
    ), err);

    TUYA_CALL_ERR_GOTO(AI_MCP_TOOL_ADD(
        "otto_robot_step_set",
        "Set step count for walk/turn motions (1-30 on V1 product).\n"
        "Parameters:\n"
        "- steps (int): step count.\n"
        "Response: true if set successfully.",
        __otto_mcp_robot_step_set,
        NULL,
        MCP_PROP_INT_DEF_RANGE("steps", "Number of steps for locomotion.", 3, 1, 30)
    ), err);

    PR_DEBUG("Otto MCP tools registered");
    return OPRT_OK;

err:
    PR_ERR("Otto MCP tools register failed: %d", rt);
    return rt;
}

/**
 * @brief MQTT connected handler: register tools after ai_mcp server init
 * @param[in] data Unused
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __otto_mcp_on_mqtt_connected(VOID_T *data)
{
    (VOID)data;
    return __otto_mcp_tools_register();
}

/**
 * @brief Initialize Otto MCP tools (subscribe after ai_mcp_init)
 * @return OPRT_OK on success
 */
OPERATE_RET otto_mcp_tools_init(VOID_T)
{
    return tal_event_subscribe(EVENT_MQTT_CONNECTED, "otto_mcp_tools", __otto_mcp_on_mqtt_connected,
                               SUBSCRIBE_TYPE_ONETIME);
}
