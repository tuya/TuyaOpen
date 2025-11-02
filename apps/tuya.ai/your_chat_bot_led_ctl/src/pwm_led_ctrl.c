#include <stdlib.h>

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tuya_config.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tkl_pwm.h"
#include "tkl_gpio.h"
#include "tal_thread.h"
#include "tal_sw_timer.h"
#include "tuya_iot.h"
#include "tuya_cloud_com_defs.h"
#include "tuya_iot_dp.h"
#include "pwm_led_ctrl.h"

STATIC TUYA_PWM_BASE_CFG_T led_cfg = {0};
#define LED_PWM_VERTICAL            TUYA_PWM_NUM_5   //P9
#define LED_PWM_FREQ                100         // 100Hz,T=1/F=10ms
#define LONG_KEY_TIME               400         // ms
#define TOUCHE_KEY_PIN              TUYA_GPIO_NUM_29 // GPIO pin 10 long press contral LED

#define PWM_MIN_DUTY                0     // 
#define PWM_PWM_CYCLE               10000       // tkl_pwm cycle = 10000
#define PWM_MAX_DUTY                10000       // MAX duty = T * cycle = 100000
#define PWM_DUTY_STEP               100         // DUTY_STEP 100

#if USE_TUYA_KEY_LIB
STATIC VOID ai_toy_key_process(UINT_T port, PUSH_KEY_TYPE_E type, INT_T cnt) 
{
    static char *keystr[] = {
        "NORMAL_KEY",
        "SEQ_KEY",
        "LONG_KEY",
        "RELEASE_KEY",
    };
    TUYA_PWM_BASE_CFG_T *info = &led_cfg;

    PR_DEBUG("key process type: %s", keystr[type]);
    switch (type) {
        
    case NORMAL_KEY: {
        //! 开关
        if (info->duty == PWM_MIN_DUTY) {
            info->duty = PWM_MAX_DUTY; // 开启LED
            PR_DEBUG("LED ON");
        } else {
            info->duty = PWM_MIN_DUTY; // 关闭LED
            PR_DEBUG("LED OFF");
        }
        tkl_pwm_info_set(LED_PWM_VERTICAL, info);
    } break;

    case SEQ_KEY: {
        
        PR_DEBUG("audio_recorder mode %d", keystr[type]);
    } break;

    case LONG_KEY: {
        info->duty += PWM_DUTY_STEP;
        if(info->duty > PWM_MAX_DUTY) {
            info->duty = PWM_MAX_DUTY; // 重置为最小值  
        }
        PR_DEBUG("LED duty set to %d", info->duty);
        // 设置PWM占空比
        tkl_pwm_info_set(LED_PWM_VERTICAL, info);
    } break;
     
    case RELEASE_KEY: {
        
    } break;

    }
}
#endif // USE_TUYA_KEY_LIB
/**
 * @brief set led on or off
 *
 * @param[in] led_state: TRUE--on, FALSE--off
 * 
 * @return none
 */
int app_set_led_onoff(bool led_state)
{
    tuya_iot_client_t *client = tuya_iot_client_get();
    TUYA_PWM_BASE_CFG_T *info = &led_cfg;

    if (led_state) {
        info->duty = PWM_MAX_DUTY; // 开启LED
        PR_DEBUG("LED ON");
    } else {
        info->duty = PWM_MIN_DUTY; // 关闭LED
        PR_DEBUG("LED OFF");
    }
    tkl_pwm_info_set(LED_PWM_VERTICAL, info);

    PR_DEBUG("report onoff dp to cloud");

    dp_obj_t dp = {
        .id = SWITCH_DP_ID,
        .type = PROP_BOOL,
        .value.dp_bool = led_state,
    };
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp, 1, 0);
}

/**
 * @brief set led brightness value
 * @param[in] value: 0~100
 */
int app_set_led_brightness(INT_T value)
{
    TUYA_PWM_BASE_CFG_T *info = &led_cfg;
    tuya_iot_client_t *client = tuya_iot_client_get();

    if (value <= 100 && value >= 0) {
        info->duty = value * PWM_PWM_CYCLE / 100; // 设置LED亮度
        PR_DEBUG("LED duty set to %d", info->duty);
        tkl_pwm_info_set(LED_PWM_VERTICAL, info);
    } else {
        PR_DEBUG("LED bright value error");
    }
    PR_DEBUG("report bright value dp to cloud");
    dp_obj_t dp = {
        .id = BRIGHT_VALUE_DP_ID,
        .type = PROP_VALUE,
        .value.dp_value = value,
    };
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp, 1, 0);

}
/**
 * @brief touch_key thread task, judge whether the touch_key is pressed, long or short press
 *
 * @param[in] args: Parameters when the task is created
 * 
 * @return none
 */
STATIC VOID_T app_touch_key_task(VOID_T *args)
{
    TUYA_GPIO_LEVEL_E read_level = TUYA_GPIO_LEVEL_HIGH;
    TUYA_GPIO_LEVEL_E last_level = TUYA_GPIO_LEVEL_HIGH;
    UINT32_T time_start = 0, timer_end = 0;
    BOOL_T touch_key_long_pressed = FALSE;  // 按键状态标志
    TUYA_PWM_BASE_CFG_T *info = &led_cfg;
    bool led_state = false;

    for (;;) {
        // 读取当前按键状态
        tkl_gpio_read(TOUCHE_KEY_PIN, &read_level);        
        // 检测按键按下（从高电平变为低电平）
        if ( read_level == TUYA_GPIO_LEVEL_LOW) {
            tal_system_sleep(5);
            tkl_gpio_read(TOUCHE_KEY_PIN, &read_level);
            if (TUYA_GPIO_LEVEL_LOW != read_level) {
                continue; // jitter
            }

            time_start = tal_system_get_millisecond();
            while (TUYA_GPIO_LEVEL_LOW == read_level) {
                tal_system_sleep(30);
                tkl_gpio_read(TOUCHE_KEY_PIN, &read_level);
                timer_end = tal_system_get_millisecond();

                if (timer_end - time_start >= LONG_KEY_TIME) {
                    PR_DEBUG("--------------------long press");
                    touch_key_long_pressed = TRUE; // 设置长按标志
                    /* long press, remove device */
                    info->duty += PWM_DUTY_STEP;
                    if(info->duty > PWM_MAX_DUTY) {
                        info->duty = PWM_MAX_DUTY; // 重置为最大值                          
                        break;
                    }
                    PR_DEBUG("LED duty set to %d", info->duty);
                    // 设置PWM占空比
                    tkl_pwm_info_set(LED_PWM_VERTICAL, info);            
                }
            }

            if (last_level== TUYA_GPIO_LEVEL_HIGH && !touch_key_long_pressed && timer_end - time_start > 50) {
                PR_DEBUG("--------------------normal press");                
                //! 开关                
                if (info->duty == PWM_MIN_DUTY) {
                    // info->duty = PWM_MAX_DUTY; // 开启LED
                    PR_DEBUG("LED ON");
                    led_state = true;
                } else {
                    // info->duty = PWM_MIN_DUTY; // 关闭LED
                    PR_DEBUG("LED OFF");
                    led_state = false;
                }
                app_set_led_onoff(led_state);
            } else {
                // PR_DEBUG("time too short");
            }

            touch_key_long_pressed = FALSE; // 重置长按标志
        }
        
        // 更新上一次的按键状态
        last_level = read_level;
        
        // 降低CPU使用率
        tal_system_sleep(100);
    }

    return;
}


/**
 * @brief touch_key gpio init, creat touch_key thread
 *
 * @param[in] pin_id: touch_key pin id
 * 
 * @return none
 */
OPERATE_RET app_touch_key_init(TUYA_GPIO_NUM_E pin_id)
{
    OPERATE_RET rt = OPRT_OK;

    /* init touch_key gpio */
    TUYA_GPIO_BASE_CFG_T touch_key_cfg = {
        .mode = TUYA_GPIO_PULLDOWN,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_LOW
    };
    TUYA_CALL_ERR_LOG(tkl_gpio_deinit(pin_id)); // 确保GPIO被正确释放
    // 初始化GPIO
    TUYA_CALL_ERR_LOG(tkl_gpio_init(pin_id, &touch_key_cfg));

    /* start touch_key thread */
    THREAD_HANDLE touch_key_task_handle;
    THREAD_CFG_T thread_cfg = {
        .thrdname = "touch_key_task",
        .priority = THREAD_PRIO_6,
        .stackDepth = 2*1024
    };
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&touch_key_task_handle, NULL, NULL, app_touch_key_task, NULL, &thread_cfg));

    return rt;
}


OPERATE_RET app_led_contral_task(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    
    led_cfg.frequency = LED_PWM_FREQ;
    led_cfg.duty = PWM_MAX_DUTY; // 最小
    led_cfg.polarity = TUYA_PWM_NEGATIVE;

   

    // 初始化垂直PWM
    TUYA_CALL_ERR_RETURN(tkl_pwm_init(LED_PWM_VERTICAL, &led_cfg));
    TUYA_CALL_ERR_RETURN(tkl_pwm_start(LED_PWM_VERTICAL));

    PR_DEBUG("LED PWM chenel:%d with frequency: %dHz", LED_PWM_VERTICAL, LED_PWM_FREQ);

    app_touch_key_init(TOUCHE_KEY_PIN); // 初始化触摸按键
    // init gpio
#if USE_TUYA_KEY_LIB
        TUYA_GPIO_BASE_CFG_T touch_key_cfg = {
            .mode = TUYA_GPIO_PULLUP,
            .direct = TUYA_GPIO_INPUT,
            .level = TUYA_GPIO_LEVEL_LOW
        };
        TUYA_CALL_ERR_LOG(tkl_gpio_init(TOUCHE_KEY_PIN, &touch_key_cfg));

        KEY_USER_DEF_S trigger_pin;
        trigger_pin.port                = TOUCHE_KEY_PIN;
        trigger_pin.low_level_detect    = FALSE;
        trigger_pin.lp_tp               = LP_ONCE_TRIG;
        trigger_pin.long_key_time       = LONG_KEY_TIME;
        trigger_pin.seq_key_detect_time = 200;
        trigger_pin.call_back           = ai_toy_key_process;
        key_init(NULL, 0, 20);
        reg_proc_key(&trigger_pin);
        TUYA_CALL_ERR_LOG(tkl_gpio_irq_enable(TOUCHE_KEY_PIN));
#endif
    return OPRT_OK;
}