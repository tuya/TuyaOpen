/**
 * @file tdd_display_qspi.c
 * @brief TDD display QSPI interface implementation
 *
 * This file implements the QSPI (Quad SPI) interface functionality for the TDL display
 * system. It provides hardware abstraction for displays using QSPI interface, enabling
 * high-speed data transfer through quad SPI communication. The implementation handles
 * QSPI initialization, data transmission, and display controller communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"

#if defined(ENABLE_QSPI) && (ENABLE_QSPI == 1)
#include "tkl_qspi.h"
#include "tkl_gpio.h"

#include "tdd_display_qspi.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    QSPI_FRAME_REQUEST = 0,
    QSPI_FRAME_EXIT,
}QSPI_EVENT_E;

typedef struct {
	uint8_t                  task_running;
	TDL_DISP_FRAME_BUFF_T   *display_frame;
    MUTEX_HANDLE             mutex;
	SEM_HANDLE               task_sem;
	THREAD_HANDLE            task;
    QUEUE_HANDLE             queue;
    SEM_HANDLE               tx_sem;
} TDL_DISP_QSPI_INFO_T;

typedef struct {
    DISP_QSPI_BASE_CFG_T        cfg;
    const uint8_t              *init_seq;
    TDD_DISP_QPI_SET_WINDOW_CB  set_window_cb; // Callback to set the display window
} DISP_QSPI_DEV_T;

typedef struct {
	QSPI_EVENT_E            event;
	DISP_QSPI_DEV_T        *dev;
    TDL_DISP_FRAME_BUFF_T  *p_fb;
} QSPI_MSG_T;


/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_DISP_QSPI_INFO_T sg_display_qspi = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __disp_qspi_event_cb(TUYA_QSPI_NUM_E port, TUYA_QSPI_IRQ_EVT_E event)
{
    if(event == TUYA_QSPI_EVENT_TX) {      
        if(sg_display_qspi.tx_sem) {
            tal_semaphore_post(sg_display_qspi.tx_sem);
        }       
    }
}

static OPERATE_RET __disp_qspi_gpio_init(DISP_QSPI_BASE_CFG_T *p_cfg)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    OPERATE_RET rt = OPRT_OK;

    if (NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->rst_pin, &pin_cfg));

    return rt;
}

static OPERATE_RET __disp_qspi_init(TUYA_SPI_NUM_E port, uint32_t freq_hz)
{
    OPERATE_RET rt = OPRT_OK;

    /*spi init*/
    TUYA_QSPI_BASE_CFG_T qspi_cfg = {
        .role = TUYA_QSPI_ROLE_MASTER,
        .mode = TUYA_QSPI_MODE0,
        .type = TUYA_QSPI_TYPE_LCD,
        .freq_hz = freq_hz,
        .use_dma = true,
    };

    PR_NOTICE("spi init %d\r\n", qspi_cfg.freq_hz);
    TUYA_CALL_ERR_RETURN(tkl_qspi_init(port, &qspi_cfg));
    TUYA_CALL_ERR_RETURN(tkl_qspi_irq_init(port, __disp_qspi_event_cb));
    TUYA_CALL_ERR_RETURN(tkl_qspi_irq_enable(port));

    return rt;
}

static OPERATE_RET __disp_qspi_send_cmd(DISP_QSPI_BASE_CFG_T *p_cfg, uint8_t cmd, \
                                        uint8_t *data, uint32_t data_len)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_QSPI_CMD_T qspi_cmd = {0};

    if (NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    memset(&qspi_cmd, 0x00, SIZEOF(TUYA_QSPI_CMD_T));

    qspi_cmd.op        = TUYA_QSPI_WRITE;
    qspi_cmd.cmd[0]    = p_cfg->cmd_write_reg;
    qspi_cmd.cmd_lines = TUYA_QSPI_1WIRE;
    qspi_cmd.cmd_size  = 1;

    qspi_cmd.addr[0]    = 0x00;
    qspi_cmd.addr[1]    = cmd;
    qspi_cmd.addr[2]    = 0x00;
    qspi_cmd.addr_lines = TUYA_QSPI_1WIRE;
    qspi_cmd.addr_size  = 3;

    qspi_cmd.data       = data;
    qspi_cmd.data_lines = TUYA_QSPI_1WIRE;
    qspi_cmd.data_size  = data_len;

    qspi_cmd.dummy_cycle = 0;
    tkl_qspi_comand(p_cfg->port, &qspi_cmd);

    return rt;
}

static OPERATE_RET __disp_qspi_send_frame(DISP_QSPI_BASE_CFG_T *p_cfg, TDL_DISP_FRAME_BUFF_T *p_fb)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_QSPI_CMD_T qspi_cmd = {0};

    if (NULL == p_cfg || NULL == p_fb) {
        return OPRT_INVALID_PARM;
    }

    memset(&qspi_cmd, 0x00, SIZEOF(TUYA_QSPI_CMD_T));

    tkl_qspi_force_cs_pin(p_cfg->port, 0);

    qspi_cmd.op = TUYA_QSPI_WRITE;

    qspi_cmd.cmd[0]    = p_cfg->pixel_pre_cmd.cmd;
    qspi_cmd.cmd_size  = 1;
    qspi_cmd.cmd_lines = p_cfg->pixel_pre_cmd.cmd_lines;

    for(uint32_t i = 0; i < p_cfg->pixel_pre_cmd.addr_size; i++) {
        qspi_cmd.addr[i] = p_cfg->pixel_pre_cmd.addr[i];
    }
    qspi_cmd.addr_size = p_cfg->pixel_pre_cmd.addr_size;
    qspi_cmd.addr_lines = p_cfg->pixel_pre_cmd.addr_lines;

    qspi_cmd.data_size = 0;
    qspi_cmd.dummy_cycle = 0;
    TUYA_CALL_ERR_RETURN(tkl_qspi_comand(p_cfg->port, &qspi_cmd));

    TUYA_CALL_ERR_RETURN(tkl_qspi_send(p_cfg->port, p_fb->frame, p_fb->len));//dma

    TUYA_CALL_ERR_RETURN(tal_semaphore_wait(sg_display_qspi.tx_sem, SEM_WAIT_FOREVER));

    tkl_qspi_force_cs_pin(p_cfg->port, 1);

    return rt;
}

static void __tdd_disp_reset(TUYA_GPIO_NUM_E rst_pin)
{
    if(rst_pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(20);

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(200);

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(120);
}

static void __tdd_disp_init_seq(DISP_QSPI_BASE_CFG_T *p_cfg, const uint8_t *init_seq)
{
    uint8_t *init_line = (uint8_t *)init_seq, *p_data = NULL;
    uint8_t data_len = 0, sleep_time = 0, cmd = 0;

    __tdd_disp_reset(p_cfg->rst_pin);

    while (*init_line) {
        data_len = init_line[0] - 1;
        sleep_time = init_line[1];
        cmd = init_line[2];

        if (data_len) {
            p_data = &init_line[3];
        } else {
            p_data = NULL;
        }

        if(cmd) {
            __disp_qspi_send_cmd(p_cfg, cmd, p_data, data_len);
        }

        if (sleep_time) {
            tal_system_sleep(sleep_time);
        }
        init_line += init_line[0] + 2;
    }
}

static void __display_qspi_task(void *args)
{
    QSPI_MSG_T msg = {0};
    sg_display_qspi.task_running = 1;

    while(sg_display_qspi.task_running) {
        OPERATE_RET ret = tal_queue_fetch(sg_display_qspi.queue, &msg, 15);//15ms
        if(ret == OPRT_OK) {
            switch(msg.event) {
                case QSPI_FRAME_REQUEST:
                    if(msg.dev->set_window_cb) {
                        msg.dev->set_window_cb(&msg.dev->cfg, 0, 0, msg.p_fb->width-1,  msg.p_fb->height-1);
                    }

                    __disp_qspi_send_frame(&msg.dev->cfg, msg.p_fb);
                    break;

                case QSPI_FRAME_EXIT:
                    sg_display_qspi.task_running = 0;
                    do {
                        ret = tal_queue_fetch(sg_display_qspi.queue, &msg, 0);//no wait
                    } while(ret == 0);
                    break;

                default:
                    break;

            }
        }
    }

    THREAD_HANDLE qspi_task1 = sg_display_qspi.task;
    tal_semaphore_post(sg_display_qspi.task_sem);
    tal_thread_delete(qspi_task1);
}


static OPERATE_RET __tdd_display_qspi_open(TDD_DISP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_QSPI_DEV_T *disp_qspi_dev = NULL;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }
    disp_qspi_dev = (DISP_QSPI_DEV_T *)device;

    TUYA_CALL_ERR_RETURN(__disp_qspi_init(disp_qspi_dev->cfg.port, disp_qspi_dev->cfg.freq_hz));
    TUYA_CALL_ERR_RETURN(__disp_qspi_gpio_init(&(disp_qspi_dev->cfg)));

    __tdd_disp_init_seq(&(disp_qspi_dev->cfg), disp_qspi_dev->init_seq);

    if(sg_display_qspi.mutex == NULL) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&(sg_display_qspi.mutex)));
    }

    if(sg_display_qspi.tx_sem == NULL) {
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(sg_display_qspi.tx_sem), 0, 1));
    }

    if(disp_qspi_dev->cfg.is_pixel_memory == 0) {

        if(sg_display_qspi.queue == NULL) {
            TUYA_CALL_ERR_RETURN(tal_queue_create_init(&(sg_display_qspi.queue), sizeof(QSPI_MSG_T), 32));
        }

        if(sg_display_qspi.task_sem == NULL) {
            TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(sg_display_qspi.task_sem), 0, 1));
        }

        if(sg_display_qspi.task == NULL) {
            THREAD_CFG_T thread_cfg = {4096, THREAD_PRIO_1, "qspi_task"};
            TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&(sg_display_qspi.task), \
                                                            NULL, NULL, __display_qspi_task,\
                                                            NULL, &thread_cfg));
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_display_qspi_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_QSPI_DEV_T *disp_qspi_dev = NULL;

    if (NULL == device || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    disp_qspi_dev = (DISP_QSPI_DEV_T *)device;

    tal_mutex_lock(sg_display_qspi.mutex);

    if(disp_qspi_dev->cfg.is_pixel_memory) {
        if(disp_qspi_dev->set_window_cb) {
            disp_qspi_dev->set_window_cb(&disp_qspi_dev->cfg, 0, 0, frame_buff->width-1,  frame_buff->height-1);
        }

        __disp_qspi_send_frame(&(disp_qspi_dev->cfg), frame_buff);
    }else {
        if(sg_display_qspi.task_running) {
            QSPI_MSG_T msg = {
                .event = QSPI_FRAME_REQUEST,
                .dev   = disp_qspi_dev,
                .p_fb  = frame_buff,
            };

            tal_queue_post(sg_display_qspi.queue, &msg , SEM_WAIT_FOREVER);
        }
    }

    tal_mutex_unlock(sg_display_qspi.mutex);

    return rt;
}

static OPERATE_RET __tdd_display_qspi_close(TDD_DISP_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief Registers a QSPI display device.
 *
 * @param name Device name for identification.
 * @param spi Pointer to QSPI configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdd_disp_qspi_device_register(char *name, TDD_DISP_QSPI_CFG_T *qspi)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_QSPI_DEV_T *disp_qspi_dev = NULL;
    TDD_DISP_DEV_INFO_T disp_qspi_dev_info;

    if (NULL == name || NULL == qspi) {
        return OPRT_INVALID_PARM;
    }

    disp_qspi_dev = tal_malloc(sizeof(DISP_QSPI_DEV_T));
    if (NULL == disp_qspi_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(&disp_qspi_dev->cfg, &qspi->cfg, sizeof(DISP_QSPI_BASE_CFG_T));

    disp_qspi_dev->init_seq      = qspi->init_seq;
    disp_qspi_dev->set_window_cb = qspi->set_window_cb;

    disp_qspi_dev_info.type     = TUYA_DISPLAY_QSPI;
    disp_qspi_dev_info.width    = qspi->cfg.width;
    disp_qspi_dev_info.height   = qspi->cfg.height;
    disp_qspi_dev_info.fmt      = qspi->cfg.pixel_fmt;
    disp_qspi_dev_info.rotation = qspi->rotation;
    disp_qspi_dev_info.is_swap  = qspi->is_swap;

    memcpy(&disp_qspi_dev_info.bl, &qspi->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));
    memcpy(&disp_qspi_dev_info.power, &qspi->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    TDD_DISP_INTFS_T disp_qspi_intfs = {
        .open  = __tdd_display_qspi_open,
        .flush = __tdd_display_qspi_flush,
        .close = __tdd_display_qspi_close,
    };

    TUYA_CALL_ERR_RETURN(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_qspi_dev,\
                                                 &disp_qspi_intfs, &disp_qspi_dev_info));

    return OPRT_OK;
}

/**
 * @brief Sends a command with optional data to the display via QSPI interface.
 * 
 * @param p_cfg Pointer to the QSPI base configuration structure.
 * @param cmd The command byte to be sent.
 * @param data Pointer to the data buffer to be sent (can be NULL if no data).
 * @param data_len Length of the data buffer in bytes.
 * 
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdd_disp_qspi_send_cmd(DISP_QSPI_BASE_CFG_T *p_cfg, uint8_t cmd, \
                                        uint8_t *data, uint32_t data_len)
{
    return __disp_qspi_send_cmd(p_cfg, cmd, data, data_len);
}

#endif