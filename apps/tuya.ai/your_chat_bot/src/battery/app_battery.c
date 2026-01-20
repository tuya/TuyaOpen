/**
 * @file app_battery.c
 * @brief app_battery module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "app_battery.h"

#include "tal_api.h"

#include "tkl_adc.h"
#include "tkl_gpio.h"


/***********************************************************
************************macro define************************
***********************************************************/
#define GET_BATTERY_TIME_MS          (5 * 60 * 1000) // 5 minutes
#define BATTERY_CHARGE_CHECK_TIME_MS (1500)          // 1.5 seconds

#define ADC_BATTERY_ADC_PIN TUYA_GPIO_NUM_13
// ADC channel number get from tkl_adc.c file
#define ADC_BATTERY_ADC_CHANNEL 15

#define ADC_BATTERY_CHARGE_PIN TUYA_GPIO_NUM_30

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TUYA_ADC_BASE_CFG_T sg_adc_cfg = {
    .ch_list.data = 1 << ADC_BATTERY_ADC_CHANNEL,
    .ch_nums      = 1, // adc Number of channel lists
    .width        = 12,
    .mode         = TUYA_ADC_CONTINUOUS,
    .type         = TUYA_ADC_INNER_SAMPLE_VOL,
    .conv_cnt     = 1,
};

static TIMER_ID sg_battery_timer_id      = NULL;
static TIMER_ID sg_charge_check_timer_id = NULL;

volatile static bool sg_is_charging = false;

static uint8_t sg_battery_percentage = 50;

#define BVC_MAP_CNT 11
static const int32_t bvc_map[] = {
    2800, 3100, 3280, 3440, 3570, 3680, 3780, 3880, 3980, 4090, 4200,
};

/***********************************************************
***********************function define**********************
***********************************************************/

void __battery_charge_pin_init(void)
{
    OPERATE_RET       rt         = OPRT_OK;
    TUYA_GPIO_LEVEL_E read_level = 0;

    TUYA_GPIO_BASE_CFG_T in_pin_cfg = {
        .mode   = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
    };
    rt = tkl_gpio_init(ADC_BATTERY_CHARGE_PIN, &in_pin_cfg);
    if (OPRT_OK != rt) {
        return;
    }

    tkl_gpio_read(ADC_BATTERY_CHARGE_PIN, &read_level);
    PR_DEBUG("battery charge pin level: %d", read_level);

    // charge pin is low when charging
    sg_is_charging = (read_level == TUYA_GPIO_LEVEL_LOW) ? true : false;
    PR_DEBUG("battery is %s", sg_is_charging ? "charging" : "not charging");

    return;
}

void __battery_charge_pin_deinit(void)
{
    tkl_gpio_deinit(ADC_BATTERY_CHARGE_PIN);
    return;
}

static void __battery_status_process(void)
{
    OPERATE_RET rt            = OPRT_OK;
    int32_t     battery_value = 0;

    if (sg_is_charging) {
        PR_INFO("battery is charging");
        // TODO: Update DP

        // Update UI
        return;
    }

    TUYA_CALL_ERR_LOG(tkl_adc_read_voltage(TUYA_ADC_NUM_0, &battery_value, 1));
    if (OPRT_OK != rt) {
        PR_ERR("read battery adc failed");
        return;
    }
    battery_value = battery_value / 1000; // convert to mV

    PR_INFO("battery voltage: %d mV", battery_value);

    // 2M/510K
    battery_value = battery_value * 4; // voltage divider ratio

    // WAIT todo convert voltage to percentage
    sg_battery_percentage = 100;
    for (uint8_t i = 0; i < BVC_MAP_CNT - 1; i++) {
        if (battery_value >= bvc_map[i]) {
            sg_battery_percentage = i * 10;
        }
    }

    // TODO: update dp

    // Update UI

    return;
}

static void __battery_timer_cb(TIMER_ID timer_id, void *arg)
{
    __battery_status_process();
    return;
}

static void __charge_check_timer_cb(TIMER_ID timer_id, void *arg)
{
    TUYA_GPIO_LEVEL_E read_level          = 0;
    bool              prev_charging_state = sg_is_charging;

    tkl_gpio_read(ADC_BATTERY_CHARGE_PIN, &read_level);

    // charge pin is low when charging
    sg_is_charging = (read_level == TUYA_GPIO_LEVEL_LOW) ? true : false;

    // If charging state changed, trigger battery status update
    if (prev_charging_state != sg_is_charging) {
        PR_INFO("charging state changed: %s -> %s", prev_charging_state ? "charging" : "not charging",
                sg_is_charging ? "charging" : "not charging");
        __battery_status_process();
    }
}

OPERATE_RET app_battery_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("battery init");
    __battery_charge_pin_init();

    TUYA_CALL_ERR_RETURN(tkl_adc_init(TUYA_ADC_NUM_0, &sg_adc_cfg));

    // Create battery status timer
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__battery_timer_cb, NULL, &sg_battery_timer_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_battery_timer_id, GET_BATTERY_TIME_MS, TAL_TIMER_CYCLE));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_trigger(sg_battery_timer_id));

    // Create charge check timer
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__charge_check_timer_cb, NULL, &sg_charge_check_timer_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_charge_check_timer_id, BATTERY_CHARGE_CHECK_TIME_MS, TAL_TIMER_CYCLE));

    return rt;
}

void app_battery_get_status(uint8_t *percentage, bool *is_charging)
{
    if (NULL == percentage || NULL == is_charging) {
        return;
    }

    *percentage  = sg_battery_percentage;
    *is_charging = sg_is_charging;

    return;
}