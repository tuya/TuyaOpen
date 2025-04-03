/**
 * @file app_led.c
 * @brief app_led module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "app_led.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E led_pin;
    TUYA_GPIO_LEVEL_E active_level;
    MUTEX_HANDLE mutex;
    uint8_t status;
} APP_LED_S;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_LED_S sg_led = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_led_set(uint8_t is_on)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_led.mutex) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_led.mutex);

    TUYA_GPIO_LEVEL_E level = is_on ? sg_led.active_level : !sg_led.active_level;
    sg_led.status = is_on;
    TUYA_CALL_ERR_LOG(tkl_gpio_write(sg_led.led_pin, level));

    tal_mutex_unlock(sg_led.mutex);

    return rt;
}

uint8_t app_led_get(void)
{
    return sg_led.status;
}

OPERATE_RET app_led_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E active_level)
{
    OPERATE_RET rt = OPRT_OK;

    if (pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_INVALID_PARM;
    }

    sg_led.led_pin = pin;
    sg_led.active_level = active_level;

    if (NULL == sg_led.mutex) {
        tal_mutex_create_init(&sg_led.mutex);
        TUYA_CHECK_NULL_RETURN(sg_led.mutex, OPRT_COM_ERROR);
    }

    TUYA_GPIO_BASE_CFG_T out_pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_HIGH};
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(sg_led.led_pin, &out_pin_cfg));

    TUYA_CALL_ERR_RETURN(app_led_set(0));

    return rt;
}
