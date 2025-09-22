/**
 * @file tdd_ir_driver_T1.c
 * @brief Infrared driver implementation for T1 microcontroller platform.
 *
 * This file provides the hardware-specific implementation of the infrared
 * driver for the T1 microcontroller platform used in Tuya IoT devices.
 * The implementation leverages the T1's hardware capabilities including
 * PWM generation, timer resources, and GPIO functionality to provide
 * reliable infrared communication services.
 *
 * T1 platform-specific features:
 * - Hardware PWM support for IR carrier frequency generation
 * - Dedicated timer resources for precise IR timing control
 * - GPIO interrupt capabilities for IR signal reception
 * - Platform-optimized timing calculations and corrections
 * - Low-power mode integration for energy-efficient IR operations
 * - Hardware-specific register configurations and optimizations
 *
 * The driver abstracts the T1 hardware complexity while providing
 * a standardized interface compatible with the TDL IR management
 * system, ensuring consistent IR functionality across different
 * Tuya device implementations using the T1 platform.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tdd_ir_driver.h"

#include "tal_memory.h"
#include "tal_system.h"
#include "tal_log.h"

#include "tkl_pwm.h"
#include "tkl_timer.h"
#include "tkl_gpio.h"
#include "tkl_output.h"

#define EN_TIMER_CAPTURE    0

#if EN_TIMER_CAPTURE
#include "BkDriverPwm.h"
#include "pwm_pub.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define IR_SEND_TIMEOUT_US          (1*1000*1000U)
#define IR_SEND_FREQ_DEFAULT        (38000U)

#define IR_SEND_TIM_HIGH_ERR_VALUE  (-55)
#define IR_SEND_TIM_LOW_ERR_VALUE   (-115)

#define DEF_TIMER_OVERFLOW_MS       (500*1000U) // 500ms

/***********************************************************
***********************typedef define***********************
***********************************************************/

typedef struct {
    IR_MODE_E                       driver_mode;
    /* tdl callback */
    IR_DRV_OUTPUT_FINISH_CB         output_finish_cb;
    IR_DRV_RECV_CB                  recv_value_cb;
    /* tdl handle */
    void                            *handle;
} TDL_IR_T;

typedef struct {
    unsigned char                   is_enable;
    /* ir receive use capture */
    int                             recv_pwm_id;

    volatile unsigned char          is_receiving;
    volatile unsigned char          irq_edge_mode;
    volatile unsigned int           last_time;
    volatile unsigned int           overflow_cnt;
    unsigned int                    irq_enable_time;
} TDD_IR_RECV_T;

typedef struct {
    IR_DRIVER_TYPE_E                driver_type;
    TDL_IR_T                        tdl_data;
    uint32_t                        inter_code_delay;

    /* ir send data */
    TUYA_PWM_NUM_E                  send_pwm_id;

    /* ir recv data */
    TDD_IR_RECV_T                   tdd_recv;

    /* ir drive hardware config information */
    IR_DRV_CFG_T                    hw_cfg;
}IR_DRV_INFO_T;

/* Register interface */
typedef struct{
    volatile unsigned int val_32k;
}T1_REG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/* tdl adapter interface */
static int __tdd_ir_drv_open(IR_DRV_HANDLE_T drv_hdl, unsigned char mode, IR_TDL_TRANS_CB ir_tdl_cb, void *args);
static int __tdd_ir_drv_close(IR_DRV_HANDLE_T drv_hdl, unsigned char mode);
static int __tdd_ir_drv_output(IR_DRV_HANDLE_T drv_hdl, unsigned int freq, unsigned char is_active, unsigned int time_us);
static int __tdd_ir_drv_status_notification(IR_DRV_HANDLE_T drv_hdl, IR_DRIVER_STATE_E state, void *args);

/* hardware */
static int __tdd_ir_send_hw_init(IR_DRV_INFO_T *drv_info);
static void __tdd_ir_send_hw_deinit(IR_DRV_INFO_T *drv_info);
static int __tdd_ir_recv_hw_init(IR_DRV_INFO_T *drv_info);
static void __tdd_ir_recv_hw_deinit(IR_DRV_INFO_T *drv_info);

static int __tdd_ir_pre_send_process(IR_DRV_HANDLE_T drv_hdl);
static int __tdd_ir_send_finish_process(IR_DRV_HANDLE_T drv_hdl);
static int __tdd_ir_pre_recv_process(IR_DRV_HANDLE_T drv_hdl);
static int __tdd_ir_recv_finish_process(IR_DRV_HANDLE_T drv_hdl);

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_IR_INTFS_T tdd_ir_intfs = {
    .open           = __tdd_ir_drv_open,
    .close          = __tdd_ir_drv_close,
    .output         = __tdd_ir_drv_output,
    .status_notif   = __tdd_ir_drv_status_notification
};

static T1_REG_T reg_val = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/*                 register function start                      */
/**
 * @brief t1 fiq enable
 *
 * @param[in] none: none
 *
 * @return none
 */
static void __tdd_t1_fiq_enable(void)
{
    *(volatile unsigned long *) (0x00802000 + 0x11 * 4) |= 0x00000002;
    return;
}

/**
 * @brief t1 fiq disable
 *
 * @param[in] none: none
 *
 * @return none
 */
static void __tdd_t1_fiq_disable(void)
{
    *(volatile unsigned long *) (0x00802000 + 0x11 * 4) &= 0xFFFFFFFD;
    return;
}

static void __tdd_t1_32k_enable(T1_REG_T *reg_val)
{
    if (0 != reg_val->val_32k) {
        *(volatile unsigned long *) (0x00802A00 + 0x13 * 4) |= reg_val->val_32k;
        reg_val->val_32k = 0;
    }
}

static void __tdd_t1_32k_disable(T1_REG_T *reg_val)
{
    if (0 == reg_val->val_32k) {
        reg_val->val_32k = (*(volatile unsigned long *) (0x00802A00 + 0x13 * 4)) & 0x7;
        *(volatile unsigned long *) (0x00802A00 + 0x13 * 4) &= 0xFFFFFFF8;
    }
}

/*                 register function end                        */

/*                 hardware function start                      
    * Do not use the TAL_PR_xxx, PR_xxx, TUYA_xxx interface, 
    * use "tkl_log_output"
*/

/**
 * @brief set pwm info and start pwm
 *
 * @param[in] pwm_id: pwm id
 * @param[in] pwm_freq: pwm frequency
 * @param[in] pwm_duty: pwm duty
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_pwm_start(int pwm_id, int pwm_freq, int pwm_duty)
{
    OPERATE_RET rt = OPRT_OK;

    if (0 == pwm_duty) {
        return tkl_pwm_stop(pwm_id);
    }

    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .polarity = TUYA_PWM_NEGATIVE,
        .duty = pwm_duty*100,
        .frequency = pwm_freq,
    };
    rt = tkl_pwm_info_set(pwm_id, &pwm_cfg);
    if (OPRT_OK != rt) {
        goto __EXIT;
    }

    rt = tkl_pwm_start(pwm_id);

__EXIT:
    return rt;
}

/**
 * @brief ir driver send callback
 *
 * @param[in] drv_handle: driver handle
 *
 * @return none
 */
static void __tdd_ir_timer_send_cb(void *drv_handle)
{
    IR_DRV_INFO_T *drv_info = NULL;

    drv_info = (IR_DRV_INFO_T *)drv_handle;
    if (NULL == drv_info) {
        tkl_log_output("send cb input is null\r\n");
        return;
    }

    /* stop timer */
    tkl_timer_stop(drv_info->hw_cfg.send_timer);

    if (NULL != drv_info->tdl_data.output_finish_cb) {
        drv_info->tdl_data.output_finish_cb(drv_info->tdl_data.handle);
    }

    return;
}

/**
 * @brief ir send hardware init
 *
 * @param[in] drv_info: driver info structure
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_send_hw_init(IR_DRV_INFO_T *drv_info)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == drv_info) {
        return OPRT_INVALID_PARM;
    }

    if (drv_info->hw_cfg.send_pin == IR_INPUT_INVALID||\
        drv_info->tdl_data.driver_mode == IR_MODE_RECV_ONLY) {
        return OPRT_NOT_SUPPORTED;
    }

    /* pwm init */
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .polarity = TUYA_PWM_NEGATIVE,
        .duty = drv_info->hw_cfg.send_duty * 100,
        .frequency = IR_SEND_FREQ_DEFAULT,
    };
    rt = tkl_pwm_init(drv_info->send_pwm_id, &pwm_cfg);
    if (OPRT_OK != rt) {
        tkl_log_output("pwm init error\r\n");
        return rt;
    }

    /* timer init */
    TUYA_TIMER_BASE_CFG_T timer_cfg = {
        .mode = TUYA_TIMER_MODE_PERIOD,
        .cb = __tdd_ir_timer_send_cb,
        .args = drv_info
    };
    rt = tkl_timer_init(drv_info->hw_cfg.send_timer, &timer_cfg);
    if (OPRT_OK != rt) {
        tkl_log_output("timer init error\r\n");
        return rt;
    }

    return rt;
}

/**
 * @brief ir send hardware deinit
 *
 * @param[in] drv_info: driver info structure
 *
 * @return none
 */
static void __tdd_ir_send_hw_deinit(IR_DRV_INFO_T *drv_info)
{
    OPERATE_RET rt = OPRT_OK;
    
    if (NULL == drv_info) {
        tkl_log_output("send hardware deinit error, input invalid\r\n");
        return;
    }

    if (drv_info->hw_cfg.send_pin == IR_INPUT_INVALID||\
        drv_info->tdl_data.driver_mode == IR_MODE_RECV_ONLY) {
        return;
    }

    /* deinit pwm */
    rt = tkl_pwm_stop(drv_info->send_pwm_id);
    if (rt != OPRT_OK) {
        tkl_log_output("pwm stop error\r\n");
    }
    rt = tkl_pwm_deinit(drv_info->send_pwm_id);
    if (rt != OPRT_OK) {
        tkl_log_output("pwm deinit error\r\n");
    }

    /* deinit timer */
    rt = tkl_timer_stop(drv_info->hw_cfg.send_timer);
    if (rt != OPRT_OK) {
        tkl_log_output("timer stop error\r\n");
    }
    rt = tkl_timer_deinit(drv_info->hw_cfg.send_timer);
    if (rt != OPRT_OK) {
        tkl_log_output("timer deinit error\r\n");
    }

    return;
}


/**
 * @brief ir receive timer callback
 *
 * @param[in] args:
 *
 * @return none
 */
static void __tdd_timer_recv_cb(void *args)
{
    IR_DRV_INFO_T *drv_info = NULL;

    if (NULL == args) {
        return;
    }
    drv_info = (IR_DRV_INFO_T *)args;

    if (drv_info->tdd_recv.is_receiving == 1) {
        __tdd_t1_fiq_enable();
    }

    drv_info->tdd_recv.overflow_cnt++;

    return;
}

/**
 * @brief gpio irq callback function
 *
 * @param[in] drv_handle: driver handle
 *
 * @return none
 */
static void __tdd_ir_irq_recv_cb(void *drv_handle)
{
    uint32_t cur_us = 0, out_us = 0;
    IR_DRV_INFO_T *drv_info = NULL;
    TDD_IR_RECV_T *tdd_recv = NULL;

    if (NULL == drv_handle) {
        return;
    }
    drv_info = (IR_DRV_INFO_T *)drv_handle;
    tdd_recv = (TDD_IR_RECV_T *)&drv_info->tdd_recv;

    tkl_gpio_irq_disable(drv_info->hw_cfg.recv_pin);
    tkl_gpio_deinit(drv_info->hw_cfg.recv_pin);

    if (tdd_recv->irq_enable_time == 0) {
        tkl_timer_start(drv_info->hw_cfg.recv_timer, DEF_TIMER_OVERFLOW_MS);
    }
    tkl_timer_get(drv_info->hw_cfg.recv_timer, &cur_us);

    if (tdd_recv->overflow_cnt > 0) {
        if (tdd_recv->irq_enable_time == 0) {
            out_us = cur_us + (DEF_TIMER_OVERFLOW_MS * (tdd_recv->overflow_cnt-1)) + (DEF_TIMER_OVERFLOW_MS - tdd_recv->last_time);
        } else {
            out_us = cur_us + (tdd_recv->irq_enable_time * (tdd_recv->overflow_cnt-1)) + (tdd_recv->irq_enable_time - tdd_recv->last_time);
        }
        tdd_recv->overflow_cnt = 0;
    } else {
        out_us = cur_us - tdd_recv->last_time;
    }
    tdd_recv->last_time = cur_us;

    // disable fiq, 32k, irq
    if (tdd_recv->is_receiving == 1 && tdd_recv->irq_enable_time != 0) {
        __tdd_t1_fiq_disable();
    }

    // tdd --data--> tdl
    if (drv_info->tdl_data.recv_value_cb) {
        drv_info->tdl_data.recv_value_cb(drv_handle, out_us, drv_info->tdl_data.handle);
    }

    /* enable next irq interrupt */
    if (TUYA_GPIO_IRQ_FALL == tdd_recv->irq_edge_mode) {
        tdd_recv->irq_edge_mode = TUYA_GPIO_IRQ_RISE;
    } else if (TUYA_GPIO_IRQ_RISE == tdd_recv->irq_edge_mode) {
        tdd_recv->irq_edge_mode = TUYA_GPIO_IRQ_FALL;
    }

    TUYA_GPIO_IRQ_T gpio_irq_cfg = {
        .mode = tdd_recv->irq_edge_mode,
        .cb = __tdd_ir_irq_recv_cb,
        .arg = drv_handle
    };
    tkl_gpio_irq_init(drv_info->hw_cfg.recv_pin, &gpio_irq_cfg);
    tkl_gpio_irq_enable(drv_info->hw_cfg.recv_pin);

    return;
}

static int __tdd_ir_recv_irq_timer_init(IR_DRV_INFO_T *drv_info)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_GPIO_IRQ_T gpio_irq_cfg = {
        .mode = TUYA_GPIO_IRQ_FALL,
        .cb = __tdd_ir_irq_recv_cb,
        .arg = (void *)drv_info,
    };
    drv_info->tdd_recv.irq_edge_mode = TUYA_GPIO_IRQ_FALL;
    rt = tkl_gpio_irq_init(drv_info->hw_cfg.recv_pin, &gpio_irq_cfg);
    if (OPRT_OK != rt) {
        tkl_log_output("gpio irq init err\r\n");
        return rt;
    }
    
    rt = tkl_gpio_irq_enable(drv_info->hw_cfg.recv_pin);
    if (OPRT_OK != rt) {
        tkl_log_output("enable gpio irq err\r\n");
    }
    
    /* timer init, don't start */
    TUYA_TIMER_BASE_CFG_T timer_cfg = {
        .mode = TUYA_TIMER_MODE_PERIOD,
        .cb = __tdd_timer_recv_cb,
        .args = drv_info
    };
    rt = tkl_timer_init(drv_info->hw_cfg.recv_timer, &timer_cfg);
    if (OPRT_OK != rt) {
        tkl_log_output("timer init err\r\n");
        return OPRT_COM_ERROR;
    }
    
    return rt;
}

static void __tdd_ir_recv_irq_timer_deinit(IR_DRV_INFO_T *drv_info)
{
    if (drv_info->hw_cfg.recv_pin == IR_INPUT_INVALID||\
        drv_info->tdl_data.driver_mode == IR_MODE_SEND_ONLY) {
        return;
    }

    tkl_gpio_irq_disable(drv_info->hw_cfg.recv_pin);
    tkl_gpio_deinit(drv_info->hw_cfg.recv_pin);
    __tdd_t1_fiq_enable();  // enable fiq
    tkl_timer_stop(drv_info->hw_cfg.recv_timer);
    tkl_timer_deinit(drv_info->hw_cfg.recv_timer);

    return;
}

/**
 * @brief ir receive hardware init
 *
 * @param[in] drv_info: driver info structure
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_recv_hw_init(IR_DRV_INFO_T *drv_info)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == drv_info) {
        return OPRT_INVALID_PARM;
    }

    if ((IR_MODE_SEND_ONLY == drv_info->tdl_data.driver_mode) || \
        (0 == drv_info->tdd_recv.is_enable))
    {
        return OPRT_NOT_SUPPORTED;
    }

    if (IR_DRV_CAPTURE == drv_info->driver_type) {
        // rt = __tdd_ir_recv_capture_init(drv_info);
    } else {
        rt = __tdd_ir_recv_irq_timer_init(drv_info);
    }

    return rt;
}

/**
 * @brief ir receive hardware deinit
 *
 * @param[in] drv_info: driver info structure
 *
 * @return none
 */
static void __tdd_ir_recv_hw_deinit(IR_DRV_INFO_T *drv_info)
{
    if (NULL == drv_info) {
        tkl_log_output("ir receive hardware deinit error, input invalid\r\n");
        return;
    }

    if (IR_DRV_CAPTURE == drv_info->driver_type) {
        // __tdd_ir_recv_capture_deinit(drv_info);
    } else {
        __tdd_ir_recv_irq_timer_deinit(drv_info);
    }

    return;
}
/*                 hardware function end                        */

/**
 * @brief start send callback
 *
 * @param[in] drv_info: driver info structure
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_send_start(IR_DRV_INFO_T *drv_info)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == drv_info) {
        return OPRT_INVALID_PARM;
    }

    if (NULL != drv_info->tdl_data.output_finish_cb) {
        drv_info->tdl_data.output_finish_cb(drv_info->tdl_data.handle);
    }

    return rt;
}

/**
 * @brief ir pre-send process
 *
 * @param[in] drv_info: driver info structure
 *
 * @return none
 */
static int __tdd_ir_pre_send_process(IR_DRV_HANDLE_T drv_hdl)
{
    OPERATE_RET rt = OPRT_OK;
    IR_DRV_INFO_T *drv_info = (IR_DRV_INFO_T *)drv_hdl;

    if (IR_DRV_SINGLE_TIMER == drv_info->driver_type && \
        IR_MODE_SEND_RECV == drv_info->tdl_data.driver_mode) {
        __tdd_ir_recv_hw_deinit(drv_info);
    }

    rt = __tdd_ir_send_hw_init(drv_info);
    if (OPRT_OK != rt) {
        return rt;
    }

    /* close fiq */
    __tdd_t1_fiq_disable();
    __tdd_t1_32k_disable(&reg_val);

    rt = __tdd_ir_send_start(drv_info);
    if (OPRT_OK != rt) {
        tkl_log_output("tdd send start error\r\n");
        __tdd_ir_send_finish_process(drv_hdl);
    }

    return rt;
}

/**
 * @brief ir send finish process
 *
 * @param[in] drv_info: driver info structure
 * @param[in] is_success: 1: send success, 0: send fail
 *
 * @return none
 */
static int __tdd_ir_send_finish_process(IR_DRV_HANDLE_T drv_hdl)
{
    if (NULL == drv_hdl) {
        tkl_log_output("send finish process error, input invalid\r\n");
        return OPRT_INVALID_PARM;
    }

    IR_DRV_INFO_T *drv_info = (IR_DRV_INFO_T *)drv_hdl;

    /* open fiq, irq */
    __tdd_t1_fiq_enable();
    __tdd_t1_32k_enable(&reg_val);

    __tdd_ir_send_hw_deinit(drv_info);
    if (IR_DRV_SINGLE_TIMER == drv_info->driver_type && \
        IR_MODE_SEND_RECV == drv_info->tdl_data.driver_mode) {
        __tdd_ir_recv_hw_init(drv_info);
    }

    return OPRT_OK;
}

/**
 * @brief ir pre-receive process
 *
 * @param[in] drv_info: driver info struct
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_pre_recv_process(IR_DRV_HANDLE_T drv_hdl)
{
    IR_DRV_INFO_T *drv_info = NULL;

    if (NULL == drv_hdl) {
        return OPRT_INVALID_PARM;
    }

    drv_info = (IR_DRV_INFO_T *)drv_hdl;

    drv_info->tdd_recv.overflow_cnt = 0;
    drv_info->tdd_recv.is_receiving = 1;

    /* close fiq */
    __tdd_t1_fiq_disable();

    if (drv_info->tdd_recv.irq_enable_time != 0) {
        tkl_timer_start(drv_info->hw_cfg.recv_timer, drv_info->tdd_recv.irq_enable_time);
    }

    return OPRT_OK;
}

/**
 * @brief ir receive finish process
 *
 * @param[in] drv_info: driver info struct
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_recv_finish_process(IR_DRV_HANDLE_T drv_hdl)
{
    IR_DRV_INFO_T *drv_info = NULL;

    if (NULL == drv_info) {
        return OPRT_INVALID_PARM;
    }
    drv_info = (IR_DRV_INFO_T *)drv_hdl;

    drv_info->tdd_recv.is_receiving = 0;
    drv_info->tdd_recv.overflow_cnt = 0;
    drv_info->tdd_recv.last_time = 0;

    // stop timer
    if (drv_info->tdd_recv.irq_enable_time != 0) {
        tkl_timer_stop(drv_info->hw_cfg.recv_timer);
        tkl_timer_deinit(drv_info->hw_cfg.recv_timer);
    }

    /* open fiq, close */
    __tdd_t1_fiq_enable();

    __tdd_ir_recv_hw_deinit(drv_info);
    __tdd_ir_recv_hw_init(drv_info);

    return OPRT_OK;
}

/**
 * @brief driver status notification function
 *
 * @param[in] drv_hdl: driver handle
 * @param[in] device_state: device state
 * @param[in] args: 
 *
 * @return none
 */
static int __tdd_ir_drv_status_notification(IR_DRV_HANDLE_T drv_hdl, IR_DRIVER_STATE_E state, void *args)
{
    if (NULL == drv_hdl) {
        return OPRT_INVALID_PARM;
    }
    IR_DRV_INFO_T *drv_info = drv_hdl;

    switch (state) {
        case IR_DRV_PRE_SEND_STATE:
            __tdd_ir_pre_send_process(drv_hdl);
        break;
        case IR_DRV_SEND_FINISH_STATE:
            __tdd_ir_send_finish_process(drv_hdl);
        break;
        case IR_DRV_PRE_RECV_STATE:
            __tdd_ir_pre_recv_process(drv_hdl);
        break;
        case IR_DRV_RECV_FINISH_STATE:
            __tdd_ir_recv_finish_process(drv_hdl);
        break;
        case IR_DRV_SEND_HW_RESET:
            __tdd_ir_send_hw_deinit(drv_hdl);
            __tdd_ir_send_hw_init(drv_hdl);
        break;
        case IR_DRV_RECV_HW_INIT:
            drv_info->tdd_recv.is_enable = 1;
            __tdd_ir_recv_hw_init(drv_hdl);
        break;
        case IR_DRV_RECV_HW_DEINIT:
            __tdd_ir_recv_hw_deinit(drv_hdl);
            drv_info->tdd_recv.is_enable = 0;
        break;
        case IR_DRV_IRQ_ENABLE_TIME_SET:
            if (NULL == args) {
                return OPRT_INVALID_PARM;
            }
            drv_info->tdd_recv.irq_enable_time = *((unsigned int *)args);
            __tdd_ir_recv_hw_deinit(drv_hdl);
            __tdd_ir_recv_hw_init(drv_hdl);
        break;
        default:break;
    }

    return OPRT_OK;
}

/**
 * @brief tdd open interface 
 *
 * @param[in] drv_hdl: ir driver info handle
 * @param[in] mode: ir device open mode
 * @param[in] ir_tdl_cb: ir device callback struct
 * @param[in] tdl_hdl: 
 * 
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_drv_open(IR_DRV_HANDLE_T drv_hdl, unsigned char mode, IR_TDL_TRANS_CB ir_tdl_cb, void *tdl_hdl)
{
    OPERATE_RET rt = OPRT_OK;
    IR_DRV_INFO_T *drv_info = NULL;

    TUYA_CHECK_NULL_RETURN(drv_hdl, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(tdl_hdl, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(ir_tdl_cb.recv_cb, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(ir_tdl_cb.output_finish_cb, OPRT_INVALID_PARM);

    drv_info = (IR_DRV_INFO_T *)drv_hdl;

    drv_info->tdl_data.handle = tdl_hdl;
    drv_info->tdl_data.driver_mode = mode;
    drv_info->tdl_data.output_finish_cb = ir_tdl_cb.output_finish_cb;
    drv_info->tdl_data.recv_value_cb = ir_tdl_cb.recv_cb;

#if EN_TIMER_CAPTURE
    // TODO
#endif

    if (IR_MODE_RECV_ONLY == mode || IR_MODE_SEND_RECV == mode) {
        drv_info->tdd_recv.is_enable = 1;
        __tdd_ir_recv_hw_deinit(drv_info);
        TUYA_CALL_ERR_RETURN(__tdd_ir_recv_hw_init(drv_info));
    }

    return OPRT_OK;
}

/**
 * @brief close ir driver
 *
 * @param[in] drv_hdl: ir driver info handle
 * @param[in] mode: ir device open mode
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_drv_close(IR_DRV_HANDLE_T drv_hdl, unsigned char mode)
{
    IR_DRV_INFO_T *drv_info = NULL;

    TUYA_CHECK_NULL_RETURN(drv_hdl, OPRT_INVALID_PARM);

    drv_info = (IR_DRV_INFO_T *)drv_hdl;


    if (drv_info->tdl_data.driver_mode == IR_MODE_SEND_ONLY || \
        drv_info->tdl_data.driver_mode == IR_MODE_SEND_RECV) {
        __tdd_ir_send_hw_deinit(drv_info);
    }

    if (drv_info->tdl_data.driver_mode == IR_MODE_RECV_ONLY || \
        drv_info->tdl_data.driver_mode == IR_MODE_SEND_RECV) {
        __tdd_ir_recv_hw_deinit(drv_info);
    }

    return OPRT_OK;
}

/**
 * @brief ir device tdl send interface
 *
 * @param[in] drv_hdl: tdd driver handle
 * @param[in] freq: send carrier frequency
 * @param[in] is_active: 1: pwm, 0: low level
 * @param[in] time_us: time
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_ir_drv_output(IR_DRV_HANDLE_T drv_hdl, unsigned int freq, unsigned char is_active, unsigned int time_us)
{
    OPERATE_RET rt = OPRT_OK;
    IR_DRV_INFO_T *drv_info = NULL;
    unsigned int delay_time = 0;

    if (NULL == drv_hdl) {
        return OPRT_INVALID_PARM;
    }

    drv_info = (IR_DRV_INFO_T *)drv_hdl;

    if (is_active) {
        delay_time = ((time_us + IR_SEND_TIM_HIGH_ERR_VALUE)<=0 ? time_us : (time_us + IR_SEND_TIM_HIGH_ERR_VALUE));
        __tdd_ir_pwm_start(drv_info->send_pwm_id, freq, drv_info->hw_cfg.send_duty);
    } else {
        delay_time = ((time_us + IR_SEND_TIM_LOW_ERR_VALUE)<=0 ? time_us : (time_us + IR_SEND_TIM_LOW_ERR_VALUE));
        __tdd_ir_pwm_start(drv_info->send_pwm_id, freq, 0);
    }

    if (delay_time<50 || delay_time>IR_SEND_TIMEOUT_US) {
        rt = OPRT_COM_ERROR;
    } else {
        rt = tkl_timer_start(drv_info->hw_cfg.send_timer, delay_time);
    }

    return rt;
}

/**
 * @brief Convert gpio id to pwm id
 *
 * @param[in] gpio_id: gpio id, reference "tuya_cloud_types.h" file TUYA_GPIO_NUM_E
 * @param[out] pwm_id: pwm id, reference "tuya_cloud_types.h" file TUYA_PWM_NUM_E
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
static int __tdd_get_pwm_id(unsigned int gpio_id, TUYA_PWM_NUM_E *pwm_id)
{
    unsigned char i;
    const unsigned int gpio_pwm_map[6][2] = {
        {TUYA_GPIO_NUM_6,  TUYA_PWM_NUM_0},
        {TUYA_GPIO_NUM_7,  TUYA_PWM_NUM_1},
        {TUYA_GPIO_NUM_8,  TUYA_PWM_NUM_2},
        {TUYA_GPIO_NUM_9,  TUYA_PWM_NUM_3},
        {TUYA_GPIO_NUM_24, TUYA_PWM_NUM_4},
        {TUYA_GPIO_NUM_26, TUYA_PWM_NUM_5}
    };

    TUYA_CHECK_NULL_RETURN(pwm_id, OPRT_INVALID_PARM);

    for (i = 0; i < 6; i++) {
        if (gpio_pwm_map[i][0] == gpio_id) {
            *pwm_id = (TUYA_PWM_NUM_E)gpio_pwm_map[i][1];
            break;
        }
    }

    if (i >= 6) {
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}

/**
 * @brief register ir driver
 *
 * @param[in] dev_name: device name, maximum 16 bytes
 * @param[in] drv_cfg: driver config params
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
int tdd_ir_driver_register(char *dev_name, IR_DRIVER_TYPE_E driver_type, IR_DRV_CFG_T drv_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    IR_DRV_INFO_T *drv_info = NULL;

    if (NULL == dev_name || IR_DRV_TYPE_MAX <= driver_type) {
        return OPRT_INVALID_PARM ;
    }

#if !(defined(EN_TIMER_CAPTURE) && (EN_TIMER_CAPTURE==1))
    if (IR_DRV_CAPTURE == driver_type) {
        return OPRT_NOT_SUPPORTED;
    }
#endif

    drv_info = (IR_DRV_INFO_T *)tal_malloc(SIZEOF(IR_DRV_INFO_T));
    TUYA_CHECK_NULL_RETURN(drv_info, OPRT_MALLOC_FAILED);
    memset(drv_info, 0, SIZEOF(IR_DRV_INFO_T));

    /* send */
    if (IR_INPUT_INVALID != drv_cfg.send_pin) {
        TUYA_CALL_ERR_RETURN(__tdd_get_pwm_id(drv_cfg.send_pin, &drv_info->send_pwm_id));
    }

    /* receive */
#if (defined(EN_TIMER_CAPTURE) && (EN_TIMER_CAPTURE==1))
    if (IR_DRV_CAPTURE == driver_type) {
        // TODO:
    }
#endif

    /* other */
    drv_info->driver_type = driver_type;
    memcpy(&drv_info->hw_cfg, &drv_cfg, sizeof(IR_DRV_CFG_T));

    TUYA_CALL_ERR_RETURN(tdl_ir_dev_register(dev_name, (IR_DRV_HANDLE_T)drv_info, &tdd_ir_intfs));

    return rt;
}
