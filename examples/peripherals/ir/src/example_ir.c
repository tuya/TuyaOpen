/**
 * @file example_ir.c
 * @brief Comprehensive infrared communication example implementation for Tuya IoT devices.
 *
 * This file provides a complete implementation example demonstrating infrared
 * communication capabilities using Tuya's IR peripheral drivers. It serves as
 * both an educational reference and a functional testing framework for developers
 * working with IR-enabled Tuya IoT devices. The implementation showcases best
 * practices for IR hardware configuration, protocol handling, and data communication.
 *
 * Key implementation features:
 * - Multi-platform hardware configuration support (BK7231N, RTL8720CF)
 * - Dual protocol support (NEC protocol and raw timecode transmission)
 * - Bidirectional IR communication with transmit and receive capabilities
 * - Configurable timing parameters and error tolerance settings
 * - Thread-based operation for non-blocking IR communication
 * - Comprehensive error handling and logging mechanisms
 * - Memory management for dynamic IR data structures
 * - Hardware abstraction layer integration for cross-platform compatibility
 *
 * The example demonstrates real-world IR applications including:
 * - Remote control simulation and testing
 * - IR sensor data collection and processing
 * - Device-to-device IR communication protocols
 * - Protocol debugging and timing analysis
 *
 * This implementation can be used as a starting point for developing custom
 * IR applications or as a testing tool for validating IR hardware functionality
 * across different Tuya IoT device platforms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tkl_output.h"

#include "tdd_ir_driver.h"
#include "tdl_ir_dev_manage.h"
#include "example_ir.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define USE_NEC_DEMO        EXAMPLE_IR_ENABLE_NEC   

#ifndef EXAMPLE_IR_SEND_TIMER
#define IR_DEV_SEND_TIMER   TUYA_TIMER_NUM_3
#else
#define IR_DEV_SEND_TIMER   EXAMPLE_IR_SEND_TIMER
#endif

#ifndef EXAMPLE_IR_RECV_TIMER
#define IR_DEV_RECV_TIMER   TUYA_TIMER_NUM_3
#else
#define IR_DEV_RECV_TIMER   EXAMPLE_IR_RECV_TIMER
#endif

#ifndef EXAMPLE_IR_TX_PIN
#define IR_DEV_SEND_PIN     TUYA_GPIO_NUM_24
#else
#define IR_DEV_SEND_PIN     EXAMPLE_IR_TX_PIN
#endif

#ifndef EXAMPLE_IR_RX_PIN
#define IR_DEV_RECV_PIN     TUYA_GPIO_NUM_26
#else
#define IR_DEV_RECV_PIN     EXAMPLE_IR_RX_PIN
#endif

#define TASK_GPIO_PRIORITY THREAD_PRIO_2
#define TASK_GPIO_SIZE     4096

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_ir_thrd_hdl = NULL;
static IR_HANDLE_T   sg_ir_dev_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __ir_example_task(void *args)
{
    OPERATE_RET rt = OPRT_OK;
    IR_DATA_U ir_send_buffer;
    IR_DATA_U *ir_recv_buffer = NULL;

    (void)args;

#if USE_NEC_DEMO
    ir_send_buffer.nec_data.addr = 0x807F;
    ir_send_buffer.nec_data.cmd = 0x1DE2;
    ir_send_buffer.nec_data.repeat_cnt = 1;
#else
    uint32_t data[] = {560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 1690, 1690, 1690};
    ir_send_buffer.timecode.data = data;
    ir_send_buffer.timecode.len = CNTSOF(data);
#endif

    for (;;) {
        TUYA_CALL_ERR_LOG(tdl_ir_dev_send(sg_ir_dev_hdl, 38000, ir_send_buffer, 1));

        rt = tdl_ir_dev_recv(sg_ir_dev_hdl, &ir_recv_buffer, 3000);
        if (OPRT_OK == rt && ir_recv_buffer) {
#if USE_NEC_DEMO
            PR_DEBUG("ir nec recv: addr:%04x, cmd:%04x, cnt:%d", ir_recv_buffer->nec_data.addr, \
                                                                     ir_recv_buffer->nec_data.cmd, \
                                                                    ir_recv_buffer->nec_data.repeat_cnt);
#else
            PR_DEBUG("ir timecode recv: addr:%p, len:%d", ir_recv_buffer, ir_recv_buffer->timecode.len);
            for (uint32_t i = 0; i < ir_recv_buffer->timecode.len; i ++) {
                PR_DEBUG_RAW("%d ", ir_recv_buffer->timecode.data[i]);
            }
            TAL_PR_DEBUG_RAW("\r\n");
#endif
            tdl_ir_dev_recv_release(sg_ir_dev_hdl, ir_recv_buffer);
            ir_recv_buffer = NULL;
        }

        tal_system_sleep(5000);
    }

    tal_thread_delete(sg_ir_thrd_hdl);
    sg_ir_thrd_hdl = NULL;

    return;
}

/**
 * @brief    register hardware 
 *
 * @param[in] : the name of the driver
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET reg_ir_hardware(char *device_name)
{
    IR_DRV_CFG_T ir_hw_cfg = {0};

    /* IR hardware config*/
    ir_hw_cfg.send_pin = IR_DEV_SEND_PIN; 
    ir_hw_cfg.recv_pin = IR_DEV_RECV_PIN;
    ir_hw_cfg.send_timer = IR_DEV_SEND_TIMER;
    ir_hw_cfg.recv_timer = IR_DEV_RECV_TIMER;
    ir_hw_cfg.send_duty = 50;
    return tdd_ir_driver_register(device_name, IR_DRV_SINGLE_TIMER, ir_hw_cfg);
}

/**
 * @brief    open driver
 *
 * @param[in] : the name of the driver
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET open_ir_driver(char *device_name)
{

    OPERATE_RET rt = OPRT_OK;
    IR_DEV_CFG_T ir_device_cfg = {0};

    TUYA_CALL_ERR_RETURN(tdl_ir_dev_find(device_name, &sg_ir_dev_hdl));

    /* ir device config */
    /* ir device common config*/
    ir_device_cfg.ir_mode = IR_MODE_SEND_RECV;
    ir_device_cfg.recv_queue_num = 3;
    ir_device_cfg.recv_buf_size = 1024;
    ir_device_cfg.recv_timeout = 300; // ms

#if USE_NEC_DEMO
    /* ir device nec protocol config */
    ir_device_cfg.prot_opt = IR_PROT_NEC;
    ir_device_cfg.prot_cfg.nec_cfg.is_nec_msb = 0;
    
    ir_device_cfg.prot_cfg.nec_cfg.lead_err   = 31;
    ir_device_cfg.prot_cfg.nec_cfg.logics_err = 46;
    ir_device_cfg.prot_cfg.nec_cfg.logic0_err = 46;
    ir_device_cfg.prot_cfg.nec_cfg.logic1_err = 40;
    ir_device_cfg.prot_cfg.nec_cfg.repeat_err = 24;
#else 
    /* ir device timecode protocol config */
    ir_device_cfg.prot_opt = IR_PROT_TIMECODE;
#endif

    return tdl_ir_dev_open(sg_ir_dev_hdl, &ir_device_cfg);
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main()
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    reg_ir_hardware("ir");
    open_ir_driver("ir");

    static THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = TASK_GPIO_SIZE;
    thrd_param.priority = TASK_GPIO_PRIORITY;
    thrd_param.thrdname = "ir";
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_ir_thrd_hdl, NULL, NULL, __ir_example_task, NULL, &thrd_param));

    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif