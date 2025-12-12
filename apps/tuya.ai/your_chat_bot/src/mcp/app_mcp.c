/**
 * @file app_mcp.c
 * @brief app_mcp module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_mcp.h"
#include "app_camera.h"

#include "tal_api.h"
#include "cJSON.h"

#include "ai_audio.h"
#include "wukong_ai_mcp_server.h"
#include <stdint.h>

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __get_device_info(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val,
                                     void *user_data)
{
    OPERATE_RET rt = OPRT_OK;

    cJSON *json = cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(json, OPRT_CR_CJSON_ERR);

    cJSON_AddStringToObject(json, "model", PROJECT_NAME);
    cJSON_AddStringToObject(json, "firmwareVersion", PROJECT_VERSION);

    wukong_mcp_return_value_set_json(ret_val, json);

    return rt;
}

// __set_volume
static OPERATE_RET __set_volume(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    int volume = 50; // default volume

    // Parse properties to get volume
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *prop = properties->properties[i];
        if (strcmp(prop->name, "volume") == 0 && prop->type == MCP_PROPERTY_TYPE_INTEGER) {
            volume = prop->default_val.int_val;
            break;
        }
    }

    // FIXME: Implement actual volume setting logic here
    ai_audio_set_volume(volume);
    PR_DEBUG("MCP set volume to %d", volume);

    // Set return value
    wukong_mcp_return_value_set_bool(ret_val, TRUE);

    return OPRT_OK;
}

#if defined(ENABLE_EX_MODULE_CAMERA) && (ENABLE_EX_MODULE_CAMERA == 1)
static OPERATE_RET __take_photo(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t *image_data     = NULL;
    uint32_t image_data_len = 0;

    TUYA_CALL_ERR_RETURN(app_camera_jpeg_capture(&image_data, &image_data_len, 3 * 1000));
    rt = wukong_mcp_return_value_set_image(ret_val, MCP_IMAGE_MIME_TYPE_JPEG, image_data, image_data_len);

    return rt;
}
#endif

static OPERATE_RET __app_mcp_init(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    wukong_mcp_server_init("Tuya MCP Server", "1.0");

    // device.info.get tool
    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("device.info.get",
                                           "Get device information such as model, and firmware version.",
                                           __get_device_info, NULL),
                       __ERR);

#if defined(ENABLE_EX_MODULE_CAMERA) && (ENABLE_EX_MODULE_CAMERA == 1)
    // device.camera.take_photo tool
    TUYA_CALL_ERR_GOTO(
        WUKONG_MCP_TOOL_ADD("device.camera.take_photo",
                            "Activates the device's camera to capture one or more photos.\n"
                            "Parameters:\n"
                            "- count (int): Number of photos to capture (1-10).\n"
                            "Response:\n"
                            "- Returns the captured photos encoded in Base64 format.",
                            __take_photo, NULL, MCP_PROP_STR("question", "The question prompting the photo capture."),
                            MCP_PROP_INT_DEF_RANGE("count", "Number of photos to capture (1-10).", 1, 1, 10)),
        __ERR);
#endif

    // device.audio.volume_set
    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("device.audio.volume_set",
                                           "Sets the device's volume level.\n"
                                           "Parameters:\n"
                                           "- volume (int): The volume level to set (0-100).\n"
                                           "Response:\n"
                                           "- Returns true if the volume was set successfully.",
                                           __set_volume, NULL,
                                           MCP_PROP_INT_RANGE("volume", "The volume level to set (0-100).", 0, 100)),
                       __ERR);

    PR_DEBUG("app_mcp_init success");
    return rt;

__ERR:
    app_mcp_deinit();
    return rt;
}

OPERATE_RET app_mcp_init(void)
{
    return tal_event_subscribe(EVENT_MQTT_CONNECTED, "app_mcp_init", __app_mcp_init, SUBSCRIBE_TYPE_ONETIME);
}

OPERATE_RET app_mcp_deinit(void)
{
    wukong_mcp_server_destroy();
    PR_DEBUG("APP MCP deinit success");
    return OPRT_OK;
}
