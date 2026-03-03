#include "tool_led.h"

#include "cJSON.h"
#include "tdl_led_manage.h"
#include "tdd_led_gpio.h"
#define BOARD_LED_PIN       TUYA_GPIO_NUM_1
#define BOARD_LED_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH
static const char *TAG = "tool_led";

static TDL_LED_HANDLE_T sg_led_hdl = NULL;

OPERATE_RET tool_led_init(void)
{
#if defined(LED_NAME) && defined(BOARD_LED_PIN)
    TDD_LED_GPIO_CFG_T led_gpio;

    led_gpio.pin   = BOARD_LED_PIN;
    led_gpio.level = BOARD_LED_ACTIVE_LV;
    led_gpio.mode  = TUYA_GPIO_PUSH_PULL;

    OPERATE_RET rt = tdd_led_gpio_register(LED_NAME, &led_gpio);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "led gpio register failed: %d", rt);
        return rt;
    }

    sg_led_hdl = tdl_led_find_dev(LED_NAME);
    if (!sg_led_hdl) {
        MIMI_LOGE(TAG, "led device not found: %s", LED_NAME);
        return OPRT_NOT_FOUND;
    }

    rt = tdl_led_open(sg_led_hdl);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "led open failed: %d", rt);
        return rt;
    }

    /* Start with LED off */
    tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);

    MIMI_LOGI(TAG, "led init ok, name=%s", LED_NAME);
#else
    MIMI_LOGW(TAG, "led not available: LED_NAME or BOARD_LED_PIN not defined");
#endif
    return OPRT_OK;
}

OPERATE_RET tool_led_control_execute(const char *input_json, char *output, size_t output_size)
{
    if (!input_json || !output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    output[0] = '\0';

    if (!sg_led_hdl) {
        snprintf(output, output_size, "Error: LED not initialized or not available on this board");
        return OPRT_NOT_FOUND;
    }

    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return OPRT_CJSON_PARSE_ERR;
    }

    const char *action = cJSON_GetStringValue(cJSON_GetObjectItem(root, "action"));
    if (!action || action[0] == '\0') {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: missing 'action' parameter");
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = OPRT_OK;

    if (strcmp(action, "on") == 0) {
        rt = tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
        if (rt == OPRT_OK) {
            snprintf(output, output_size, "LED turned on");
        } else {
            snprintf(output, output_size, "Error: failed to turn on LED (rt=%d)", rt);
        }

    } else if (strcmp(action, "off") == 0) {
        rt = tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
        if (rt == OPRT_OK) {
            snprintf(output, output_size, "LED turned off");
        } else {
            snprintf(output, output_size, "Error: failed to turn off LED (rt=%d)", rt);
        }

    } else if (strcmp(action, "toggle") == 0) {
        rt = tdl_led_set_status(sg_led_hdl, TDL_LED_TOGGLE);
        if (rt == OPRT_OK) {
            snprintf(output, output_size, "LED toggled");
        } else {
            snprintf(output, output_size, "Error: failed to toggle LED (rt=%d)", rt);
        }

    } else if (strcmp(action, "blink") == 0) {
        cJSON *count_j    = cJSON_GetObjectItem(root, "count");
        cJSON *interval_j = cJSON_GetObjectItem(root, "interval_ms");

        uint32_t count       = cJSON_IsNumber(count_j) ? (uint32_t)count_j->valuedouble : 5;
        uint32_t interval_ms = cJSON_IsNumber(interval_j) ? (uint32_t)interval_j->valuedouble : 300;

        TDL_LED_BLINK_CFG_T blink_cfg = {
            .cnt                    = count,
            .start_stat             = TDL_LED_ON,
            .first_half_cycle_time  = interval_ms,
            .latter_half_cycle_time = interval_ms,
        };

        rt = tdl_led_blink(sg_led_hdl, &blink_cfg);
        if (rt == OPRT_OK) {
            snprintf(output, output_size, "LED blinking %u times with %ums interval", (unsigned)count,
                     (unsigned)interval_ms);
        } else {
            snprintf(output, output_size, "Error: failed to blink LED (rt=%d)", rt);
        }

    } else if (strcmp(action, "flash") == 0) {
        cJSON *interval_j = cJSON_GetObjectItem(root, "interval_ms");

        uint32_t interval_ms = cJSON_IsNumber(interval_j) ? (uint32_t)interval_j->valuedouble : 1000;

        rt = tdl_led_flash(sg_led_hdl, interval_ms);
        if (rt == OPRT_OK) {
            snprintf(output, output_size, "LED flashing with %ums half cycle", (unsigned)interval_ms);
        } else {
            snprintf(output, output_size, "Error: failed to flash LED (rt=%d)", rt);
        }

    } else {
        snprintf(output, output_size, "Error: unknown action '%s'. Valid actions: on, off, toggle, blink, flash",
                 action);
        rt = OPRT_INVALID_PARM;
    }

    MIMI_LOGI(TAG, "led_control action=%s rt=%d", action, rt);
    cJSON_Delete(root);
    return rt;
}
