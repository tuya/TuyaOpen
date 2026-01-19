/**
 * @file app_system_info.c
 * @brief app_system_info module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "app_system_info.h"

#include "tal_api.h"

#include "tkl_adc.h"
#include "tkl_gpio.h"

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "app_display.h"
#endif

#include "netmgr.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tal_wifi.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define GET_BATTERY_TIME_MS          (5 * 60 * 1000) // 5 minutes
#define BATTERY_CHARGE_CHECK_TIME_MS (1500)          // 1.5 seconds

#define ADC_BATTERY_ADC_PIN TUYA_GPIO_NUM_28
// ADC channel number get from tkl_adc.c file
#define ADC_BATTERY_ADC_CHANNEL 4

#define ADC_BATTERY_CHARGE_PIN TUYA_GPIO_NUM_21

#define PRINTF_FREE_HEAP_TTIME (10 * 1000)
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
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
#endif

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static TIMER_ID s_wifi_status_timer_id = NULL;
#endif

static TIMER_ID sg_printf_heap_tm;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __printf_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;

    uint32_t free_heap = tal_system_get_free_heap_size();
    PR_INFO("Free heap size:%d", free_heap);
}

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#ifndef PLATFORM_T5
static uint8_t __robot_wifi_status_from_rssi(int8_t rssi)
{
    if (rssi >= -60) {
        return 1; // GOOD
    } else if (rssi >= -70) {
        return 2; // FAIR
    } else {
        return 3; // WEAK
    }
}
#endif

static uint8_t __robot_get_wifi_status(void)
{
    netmgr_status_e net_status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net_status);
    if (net_status != NETMGR_LINK_UP) {
        return 0; // DISCONNECTED
    }

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#ifndef PLATFORM_T5
    // get rssi
    int8_t rssi = 0;
    // BUG: Getting RSSI causes a crash on T5 platform
    if (OPRT_OK == tal_wifi_station_get_conn_ap_rssi(&rssi)) {
        return __robot_wifi_status_from_rssi(rssi);
    }
#endif
    // Connected but RSSI not available (or disabled on platform)
    return 1;
#else
    return 1;
#endif
}

static void __robot_wifi_status_timer_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;

    uint8_t wifi_status = __robot_get_wifi_status();
    robot_ui_wifi_update(wifi_status);
}

#endif /* ENABLE_COMP_AI_DISPLAY */

#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
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
    OPERATE_RET rt                 = OPRT_OK;
    int32_t     adc_pin_voltage_mv = 0;

    if (sg_is_charging) {
        PR_INFO("battery is charging");
        // TODO: Update DP

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
        robot_ui_battery_update(true, sg_battery_percentage);
#endif
        return;
    }

    rt = tkl_adc_read_voltage(TUYA_ADC_NUM_0, &adc_pin_voltage_mv, 1);
    if (OPRT_OK != rt) {
        PR_ERR("read battery adc failed");
        return;
    }

    // tkl_adc_read_voltage() returns mV.
    PR_INFO("battery adc(pin) voltage: %d mV", adc_pin_voltage_mv);

    // 2M/510K divider (approx 4x). Convert ADC pin voltage to estimated battery voltage.
    int32_t battery_voltage_mv = adc_pin_voltage_mv * 4;

    // Convert voltage to percentage (0..100). bvc_map is in mV.
    sg_battery_percentage = 0;
    for (uint8_t i = 0; i < BVC_MAP_CNT; i++) {
        if (battery_voltage_mv >= bvc_map[i]) {
            sg_battery_percentage = i * 10;
        }
    }

    PR_INFO("battery est(bat) voltage: %d mV, percent=%u", battery_voltage_mv, sg_battery_percentage);

    // TODO: update dp

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    robot_ui_battery_update(false, sg_battery_percentage);
#endif
    (void)0;

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
#endif

OPERATE_RET app_system_info(void)
{
    OPERATE_RET rt = OPRT_OK;
#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
    PR_DEBUG("battery init");
    PR_INFO("battery cfg: adc_pin=%d adc_channel=%d charge_pin=%d", ADC_BATTERY_ADC_PIN, ADC_BATTERY_ADC_CHANNEL,
            ADC_BATTERY_CHARGE_PIN);
    __battery_charge_pin_init();

    TUYA_CALL_ERR_RETURN(tkl_adc_init(TUYA_ADC_NUM_0, &sg_adc_cfg));

    // Create battery status timer
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__battery_timer_cb, NULL, &sg_battery_timer_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_battery_timer_id, GET_BATTERY_TIME_MS, TAL_TIMER_CYCLE));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_trigger(sg_battery_timer_id));

    // Create charge check timer
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__charge_check_timer_cb, NULL, &sg_charge_check_timer_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_charge_check_timer_id, BATTERY_CHARGE_CHECK_TIME_MS, TAL_TIMER_CYCLE));
#endif

    #if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    if (s_wifi_status_timer_id == NULL) {
        tal_sw_timer_create(__robot_wifi_status_timer_cb, NULL, &s_wifi_status_timer_id);
        tal_sw_timer_start(s_wifi_status_timer_id, 1000, TAL_TIMER_CYCLE);
        tal_sw_timer_trigger(s_wifi_status_timer_id);
    }
    #endif

    tal_sw_timer_create(__printf_free_heap_tm_cb, NULL, &sg_printf_heap_tm);
    tal_sw_timer_start(sg_printf_heap_tm, PRINTF_FREE_HEAP_TTIME, TAL_TIMER_CYCLE);

    return rt;
}

void app_battery_get_status(uint8_t *percentage, bool *is_charging)
{
    if (NULL == percentage || NULL == is_charging) {
        return;
    }

#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
    *percentage  = sg_battery_percentage;
    *is_charging = sg_is_charging;
#else
    *percentage  = 0;
    *is_charging = false;
#endif
}