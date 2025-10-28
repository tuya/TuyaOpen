/*
 * tuya_ai_battery.c
 * Copyright (C) 2025 cc <cc@tuya>
 *
 * Distributed under terms of the MIT license.
 */

#include "tuya_ai_battery.h"
#include "tal_log.h"
#include "tal_thread.h"
#include "tal_sw_timer.h"
#include "tkl_pinmux.h"
#include "tkl_adc.h"
#include "uni_log.h"
#include "base_event.h"
#include "smart_frame.h"
#include "tuya_ai_display.h"
#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
#include "tal_cellular.h"
#endif

#if defined(TUYA_AI_TOY_BATTERY_ENABLE) || TUYA_AI_TOY_BATTERY_ENABLE == 1

#define ADC_CONV_TIMES  8
#define ADC_TASK_DELAY_TIME  20
#define CAPACITY_KEEP_MAX_TIME  (3 * 60 * 1000)
#define CAPACITY_CONFIDENCE_INTERVAL        12
#define CAPACITY_CONFIDENCE_INTERVAL_DELAY  5

STATIC ai_battery_conf_t batt_conf = {
    .mode = AI_BATTERY_MODE_MAX,
    .cb = NULL,
};
STATIC INT32_T adc_chan = 4;
STATIC uint8_t last_capacity = 0xff;
STATIC TIMER_ID sw_timer_id = NULL;
STATIC THREAD_HANDLE __vol_conv_thread_handle = NULL;
STATIC uint8_t *battery_capacity = NULL;
STATIC BOOL_T is_running = FALSE;
STATIC BOOL_T is_stop = TRUE;

STATIC BOOL_T s_is_battery_low = FALSE;
STATIC BOOL_T s_is_charging = FALSE;

STATIC uint8_t last_report_capacity = 0;

// 803035-800-1C电池电压与容量映射表
voltage_cap_map bvc_map[] = {
    {.v = 4200, .c = 100},  // 满电截止电压（充电末端）
    {.v = 4090, .c =  90},  // 恒流放电末端（对应90%容量）
    {.v = 3980, .c =  80},  // 放电平台起始点
    {.v = 3880, .c =  70},  // 主要放电区段
    {.v = 3780, .c =  60},  // 容量下降转折点
    {.v = 3680, .c =  50},  // 容量中值基准点
    {.v = 3570, .c =  40},  // 放电速率加快区
    {.v = 3440, .c =  30},  // 低电量预警阈值
    {.v = 3280, .c =  20},  // 深度放电起始
    {.v = 3100, .c =  10},  // 低压保护触发点
    {.v = 2800, .c =   0}   // 放电截止电压
};


/**
 * @brief get map size
 *
 * @return size
 */
int tuya_ai_vol2cap_map_size(void)
{
    return sizeof(bvc_map) / sizeof(bvc_map[0]);
}

/**
 * @brief 
 * 
 * @param arr 
 * @param n 
 * @return int 
 */
static int findCandidate(uint8_t arr[], int n)
{
    int count = 0;
    int candidate = -1;

    for (int i = 0; i < n; i++) {
        if (count == 0) {
            candidate = arr[i];
            count = 1;
        } else if (arr[i] == candidate) {
            count++;
        } else {
            count--;
        }
    }

    return candidate;
}

static int isValidCandidate(uint8_t arr[], int n, uint8_t candidate)
{
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (arr[i] == candidate) {
            count++;
        }
    }
    return count >= (n * 2 / 3);
}

/**
 * @brief adc value to capacity
 *
 * @param[in] :
 *
 * @return none
 */
uint8_t __voltage_conv_cap(int voltage, voltage_cap_map *map, uint8_t size)
{
    // lookup map
    for (int i = 0; i < size; i++) {
        if (voltage > map[i].v) {
            return map[i].c;
        }
    }
    return 0;
}

static uint8_t __adc_conv_capacity(ai_battery_conf_t *bc)
{
    int i, sum = 0, max = 0, sample_cnt = 0;
    uint32_t cur_vol = 0, adc_sample = 0;
    INT32_T buf[ADC_CONV_TIMES];
    uint8_t capacity = 0;
    static uint32_t last_adc_sample = 0xffffffff;

    sum = 0;
    memset(buf, 0, sizeof(INT32_T) * ADC_CONV_TIMES);

    // 1, read adc, get voltage
    tkl_adc_read_data(0, buf, ADC_CONV_TIMES);

    // 2, get max voltage and sum
    for (i = 0; i < ADC_CONV_TIMES; i++) {
        sum += buf[i];

        if (max < buf[i])
            max = buf[i];

        // bk_printf("%d ", buf[i]);
    }
    // bk_printf("\r\n");

    // 3, 丢弃最大值，使用上次采样值
    if (last_adc_sample != 0xffffffff) {
        sum -= max;
        sum += last_adc_sample;
    }

    adc_sample = sum/ADC_CONV_TIMES;

    extern float bk_adc_data_calculate(uint16_t adc_val, uint8_t adc_chan);
    cur_vol = (uint32_t)(1000 * bk_adc_data_calculate(adc_sample, adc_chan));

    // 4, 3M / 1M 电阻串联分压
    cur_vol *= 4;

#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
    tal_cellular_get_volt(&cur_vol);
    adc_sample = cur_vol;
#endif

    // convert to capacity
    capacity = __voltage_conv_cap(cur_vol, bc->adc.map, bc->adc.map_size);
    // PR_DEBUG("battery sample: %u, cur: %u, vol: %u, cap: %u", last_adc_sample, adc_sample, cur_vol, capacity);
    last_adc_sample = adc_sample;

    return capacity;
}

/**
 * @brief software timer callback
 *
 * @param[in] :
 *
 * @return none
 */
STATIC VOID_T __timer_cb(TIMER_ID timer_id, VOID_T *arg)
{
    PR_NOTICE("--- aaa tal sw timer callback");
    ai_battery_conf_t *bc = (ai_battery_conf_t *)arg;
    if (bc->cb != NULL) {
        bc->cb(last_capacity);
    }
}

static void __battery_monitor_timer_stop(TIMER_ID timer_id)
{
    PR_NOTICE("stop and delete battery monitor timer");
    tal_sw_timer_stop(timer_id);
    tal_sw_timer_delete(timer_id);
}

static uint8_t __adc_confidence(ai_battery_conf_t *bc, uint8_t *out)
{
    int i, sample_cnt = 0;
    uint8_t max_capacity = 0, tmp = 0xff;
    uint8_t capacity[CAPACITY_CONFIDENCE_INTERVAL];

    for (i = 0; i < CAPACITY_CONFIDENCE_INTERVAL; i++) {
        capacity[i] = __adc_conv_capacity(bc);

        if (capacity[i] > max_capacity)
            max_capacity = capacity[i];

        tkl_system_sleep(CAPACITY_CONFIDENCE_INTERVAL_DELAY);
    }

    // 如果至少2/3采样值相同，则为可信数据, 否则取最大值
    uint8_t cap = findCandidate(capacity, CAPACITY_CONFIDENCE_INTERVAL);
    if (isValidCandidate(capacity, CAPACITY_CONFIDENCE_INTERVAL, cap)) {
        *out = cap;
        // PR_DEBUG("ValidCandidate: %u\r\n", cap);
        return 1;
    } else {
        *out = max_capacity;
        return 0;
    }

}

static void __voltage_convert_task(void* arg)
{
    int ret;
    ai_battery_conf_t *bc = (ai_battery_conf_t *)arg;
    uint8_t capacity = 0, cap = 0xff;
    uint32_t sample_cnt = 0;
    uint32_t t = CAPACITY_CONFIDENCE_INTERVAL * CAPACITY_CONFIDENCE_INTERVAL_DELAY + ADC_TASK_DELAY_TIME;

    // 前几次采样不准
    for (int i = 0; i < 15; i++) {
        capacity = __adc_conv_capacity(bc);
        tkl_system_sleep(20);
    }

    // 首次上电，使用当前采样值作为初始电量
    // 非首次上电，即进出低功耗，初始化时候，保留上次采样值
    if (last_capacity == 0xff) {
        last_capacity = capacity;
    }

    // 初始化结束后，强制上报1次
    TAL_PR_DEBUG("force upload battery capacity, cap: %u", last_capacity);
    tuya_ai_battery_force_upload();

    is_running = TRUE;
    is_stop = FALSE;

    BOOL_T  is_charging  = FALSE;

    while (is_running) {
        // 充电时候，电量值设置0xff，结束充电后，根据实际电压赋值
        if (bc->is_charging_cb && bc->is_charging_cb() == TRUE) {
            if (!is_charging) {
                is_charging = TRUE;
                tuya_ai_display_msg(NULL, 0, TY_DISPLAY_TP_STAT_CHARGING);
            } 
            last_capacity = 0xff;
            tkl_system_sleep(ADC_TASK_DELAY_TIME);
            continue;
        } else {
            if (is_charging) {
                is_charging = FALSE;
                #ifdef ENABLE_TUYA_UI                   
                tuya_ai_display_msg(&last_capacity, 1, TY_DISPLAY_TP_STAT_BATTERY);
                #endif
            }
        }

        if (bc->mode == AI_BATTERY_MODE_ADC) {
            ret = __adc_confidence(bc, &capacity);
            if (last_capacity == 0xff) {
                last_capacity = capacity;
                battery_capacity[sample_cnt++] = capacity;
                PR_NOTICE("battery cap init: %u", capacity);
            } else if (ret) {
                // 可信数据
                battery_capacity[sample_cnt++] = capacity;
            }

            if (sample_cnt == CAPACITY_KEEP_MAX_TIME/t) {
                // 时间段内，采样次数有2/3 都相同，则认为值暂时有效，进行下一步判断
                cap = findCandidate(battery_capacity, sample_cnt);
                int is_valid = isValidCandidate(battery_capacity, sample_cnt, cap);
                sample_cnt = 0;
                if (is_valid) {
                    // 如果采样值较当前值大，使用当前值，容量只降不升
                    if (last_capacity < cap) {
                        PR_NOTICE("last_capacity < cap, %d %d", last_capacity, cap);
                        continue;
                    }
                    // 上一个判断可知，当前值大于采样值
                    // 差值大于等于20,则使用旧值
                    int diff = last_capacity - cap;
                    if (diff >= 20) {
                        PR_NOTICE("diff >= 20, %d %d", last_capacity, cap);
                        continue;
                    }

                    // 采样值较当前值小，差值小于10, 更新容量
                    // 不相等再打印
                    if( cap != last_capacity) {
                        last_capacity = cap;
                        #ifdef ENABLE_TUYA_UI   
                        tuya_ai_display_msg(&last_capacity, 1, TY_DISPLAY_TP_STAT_BATTERY);
                        #endif
                    }

                    PR_NOTICE("battery cap update: %u", last_capacity);
                }
            }
        } else if (bc->mode == AI_BATTERY_MODE_IIC) {
            // TODO

        } else {
            PR_ERR("%s, mode not support", __func__);
        }

        tkl_system_sleep(ADC_TASK_DELAY_TIME);
    }
    is_stop = TRUE;
    tkl_thread_release(__vol_conv_thread_handle);
}

void tuya_ai_battery_force_upload(void)
{
    tal_sw_timer_trigger(sw_timer_id);
}

UINT8_T tuya_ai_battery_get_capacity(VOID)
{
    tuya_ai_battery_force_upload();
    return last_report_capacity;
}

// 事件触发，主动上报电量
STATIC INT_T _event_active_cb(VOID_T *data)
{
    PR_NOTICE("active event");
    tuya_ai_battery_force_upload();
    return 0;
}
 
/**
 * @brief battery check init, creat battery monitor thread
 *
 * @param[in] conf, mode: how to get current battery capacity
 *                  cb: external action callback
 *
 * @return 0 is success, else is error.
 */
int __tuya_ai_battery_init(ai_battery_conf_t *conf)
{
    OPERATE_RET rt = OPRT_OK;

    if (conf == NULL) {
        PR_ERR("%s, config is NULL", __func__);
        return -1;
    }

    if (conf->mode == AI_BATTERY_MODE_ADC) {
        // adc
        if (conf->adc.map == NULL) {
            PR_ERR("%s, voltage/cap map is NULL", __func__);
            return -1;
        }
        adc_chan = tkl_io_pin_to_func(conf->adc.adc_gpio, TUYA_IO_TYPE_ADC);
        if (adc_chan == OPRT_NOT_SUPPORTED) {
            PR_ERR("%s, adc pin is not support", __func__);
            return -1;
        }
        PR_NOTICE("%s, adc pin: %d", __func__, conf->adc.adc_gpio);

        TUYA_ADC_BASE_CFG_T cfg;
        UINT32_T ch_data = 0;
        ch_data |= BIT(adc_chan & 0xFF);

        cfg.ch_list.data = ch_data;
        cfg.ch_nums = 1;
        cfg.type = TUYA_ADC_INNER_SAMPLE_VOL;
        cfg.width = 12;
        cfg.mode = TUYA_ADC_CONTINUOUS;
        cfg.conv_cnt = ADC_CONV_TIMES;
        tkl_adc_init(0, &cfg);

    } else if (conf->mode == AI_BATTERY_MODE_IIC) { 
        // i2c TODO
        if ((conf->i2c.init == NULL) || (conf->i2c.deinit == NULL) || (conf->i2c.get_capacity == NULL)) {
            PR_ERR("%s, i2c config error %p %p %p", __func__, conf->i2c.init, conf->i2c.deinit, conf->i2c.get_capacity);
            return -1;
        }

        // init i2c
        conf->i2c.init();
    } else {
        PR_ERR("%s, mode not support", __func__);
        return -2;
    }

    memcpy(&batt_conf, conf, sizeof(ai_battery_conf_t));

    uint32_t t = CAPACITY_CONFIDENCE_INTERVAL * CAPACITY_CONFIDENCE_INTERVAL_DELAY + ADC_TASK_DELAY_TIME;
    battery_capacity = (uint8_t *)tkl_system_psram_malloc(CAPACITY_KEEP_MAX_TIME/t);
    if (battery_capacity == NULL) {
        PR_ERR("%s, psram malloc failed", __func__);
        goto __EXIT;
    }
    memset(battery_capacity, 0, CAPACITY_KEEP_MAX_TIME/t);

    // create monitor timer
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__timer_cb, &batt_conf, &sw_timer_id), __EXIT);

    PR_DEBUG("battery monitor timer start");
    TUYA_CALL_ERR_LOG(tal_sw_timer_start(sw_timer_id, 60 * 1000, TAL_TIMER_CYCLE));
    ty_subscribe_event(EVENT_POST_ACTIVATE, "ai_toy", _event_active_cb, SUBSCRIBE_TYPE_ONETIME);


    tkl_thread_create_in_psram(&__vol_conv_thread_handle, "conv", 1024 * 2, THREAD_PRIO_2, __voltage_convert_task, &batt_conf);

    return 0;

__EXIT:
    return -2;
}

/**
 * @brief battery check deinit
 *
 * @return none
 */
void __tuya_ai_battery_deinit(void)
{
    // if not running, return
    if (!is_running)
        return;

    ty_unsubscribe_event(EVENT_POST_ACTIVATE, "ai_toy", _event_active_cb);

    // stop thread
    is_running = FALSE;
    while (!is_stop) {
        tkl_system_sleep(10);
    }
    TAL_PR_DEBUG("voltage convert thread exit");
    if (batt_conf.mode == AI_BATTERY_MODE_ADC) {
        tkl_adc_deinit(1);
    } else if (batt_conf.mode == AI_BATTERY_MODE_IIC) {
        batt_conf.i2c.init();
    }

    __battery_monitor_timer_stop(sw_timer_id);
    sw_timer_id = NULL;

    if (battery_capacity != NULL) {
        tkl_system_psram_free(battery_capacity);
        battery_capacity = NULL;
    }
    return;
}

STATIC TY_BATTERY_CB s_battery_cb = NULL;

typedef enum {
    BATTERY_NO_CHARGE,
    BATTERY_CHARGEING,
    BATTERY_CHARGE_DONE,
} BATTERY_CHAEGE_STATUS;


BOOL_T __tuya_ai_toy_battery_is_charging(void)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_NONE;
    tkl_gpio_read(TUYA_AI_TOY_CHARGE_PIN, &level);
    // TAL_PR_NOTICE("battery_is_charging level: %d", level);

    return level == TUYA_GPIO_LEVEL_LOW;
}


STATIC int __tuya_ai_toy_battery_callback(uint8_t current_capacity)
{
    BOOL_T is_charging = __tuya_ai_toy_battery_is_charging();
    if (s_battery_cb) {
        s_battery_cb(current_capacity <= 20, is_charging);
    }
    if (!is_charging) {
        TAL_PR_NOTICE("%s, capacity: %d", __func__, current_capacity);
        // 上报dp
        TY_OBJ_DP_S batt_cap_dp_info[2];
        batt_cap_dp_info[0].dpid = 2;
        batt_cap_dp_info[0].type = PROP_VALUE;
        batt_cap_dp_info[0].value.dp_value = current_capacity;
        batt_cap_dp_info[0].time_stamp = 0;

        batt_cap_dp_info[1].dpid = 5;
        batt_cap_dp_info[1].type = PROP_ENUM;
        batt_cap_dp_info[1].value.dp_enum = BATTERY_NO_CHARGE;
        batt_cap_dp_info[1].time_stamp = 0;

        dev_report_dp_json_async_force(NULL, &batt_cap_dp_info, 2);

        last_report_capacity = current_capacity;

        // // 低电量亮红灯
        // if (current_capacity <= 20) {
        //     tkl_gpio_write(TUYA_AI_TOY_LED, 1);
        // } else {
        //     tkl_gpio_write(TUYA_AI_TOY_LED, 0);
        // }
    } else {
        TAL_PR_NOTICE("%s, in charge", __func__, current_capacity);
        TY_OBJ_DP_S batt_cap_dp_info;
        batt_cap_dp_info.dpid = 5;
        batt_cap_dp_info.type = PROP_ENUM;
        batt_cap_dp_info.value.dp_enum = BATTERY_CHARGEING;
        batt_cap_dp_info.time_stamp = 0;
        // 充电时候关闭低电量灯
        // __tuya_ai_toy_battery_led_off();

        dev_report_dp_json_async_force(NULL, &batt_cap_dp_info, 1);
    }
    return 0;
}

STATIC VOID _battery_cb(BOOL_T is_low, BOOL_T is_charging)
{
    TAL_PR_DEBUG("battery low = %d, charging = %d", is_low, is_charging);
    if (is_low && s_is_battery_low != is_low) {
        // low battery
        TAL_PR_NOTICE("low battery alert");
        s_is_battery_low = is_low;
        // ty_ai_event_post(s_ai_proc_handle, TY_AI_EVENT_BATTERY_LOW);
    }
}

OPERATE_RET tuya_ai_toy_battery_init(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    s_battery_cb = _battery_cb;

    ai_battery_conf_t conf = {
        .mode = AI_BATTERY_MODE_ADC,
        .adc = {
#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
			.adc_gpio = TUYA_GPIO_NUM_23,
#else					
            .adc_gpio = TUYA_AI_TOY_BATTERY_CAP_PIN,
#endif			
            .map = bvc_map,
            .map_size = tuya_ai_vol2cap_map_size(),
        },
        .cb = __tuya_ai_toy_battery_callback,
        .is_charging_cb = __tuya_ai_toy_battery_is_charging,
    };

    // 充电状态IO
    TUYA_GPIO_BASE_CFG_T cfg;
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.mode = TUYA_GPIO_FLOATING;
    tkl_gpio_init(TUYA_AI_TOY_CHARGE_PIN, &cfg);

    // battery monitor init
    __tuya_ai_battery_init(&conf);

    return rt;
}

OPERATE_RET tuya_ai_toy_battery_uninit(void)
{
    __tuya_ai_battery_deinit();
    return OPRT_OK;
}

#endif
