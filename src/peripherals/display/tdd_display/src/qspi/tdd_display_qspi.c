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
typedef struct {
    MUTEX_HANDLE                mutex;
    DISP_QSPI_BASE_CFG_T        cfg;
    const uint8_t              *init_seq;
} DISP_QSPI_DEV_T;

typedef enum {
    TDD_QSPI_FRAME_REQUEST = 0,
    TDD_QSPI_FRAME_EXIT,
} TDD_QSPI_FRAME_EVENT_E;

typedef struct {
	TDD_QSPI_FRAME_EVENT_E  event;
    TDL_DISP_FRAME_BUFF_T  *frame_buff;
} TDD_DISP_QSPI_MSG_T;

typedef struct {
    TUYA_SPI_NUM_E          port;
    SEM_HANDLE              tx_sem;
    SEM_HANDLE              exit_sem;
    QUEUE_HANDLE            queue;
    THREAD_HANDLE           task;
    uint8_t                 is_task_running;
    uint8_t                 is_period_flush;
    TDD_DISP_DEV_HANDLE_T   device;   
    TDL_DISP_FRAME_BUFF_T  *display_fb; 
}TDD_DISP_QSPI_SYNC_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static void __display_qspi_task(void *args);

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_DISP_QSPI_SYNC_T sg_disp_qspi_sync[TUYA_QSPI_NUM_MAX] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __disp_qspi_event_cb(TUYA_QSPI_NUM_E port, TUYA_QSPI_IRQ_EVT_E event)
{
    if(event == TUYA_QSPI_EVENT_TX) {      
        if(sg_disp_qspi_sync[port].tx_sem) {
            tal_semaphore_post(sg_disp_qspi_sync[port].tx_sem);
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
        .dma_data_lines = TUYA_QSPI_4WIRE,
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
    qspi_cmd.cmd[0]    = p_cfg->cmd_ramwr;
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

static void __disp_qspi_set_window(DISP_QSPI_BASE_CFG_T *p_cfg, uint16_t x_start, uint16_t y_start,\
                            uint16_t x_end, uint16_t y_end)
{
    uint8_t lcd_data[4];
    static uint16_t x1=0, x2=0, y1=0, y2=0;

    if (NULL == p_cfg) {
        return;
    }

    x_start += p_cfg->x_offset;
    x_end   += p_cfg->x_offset;
    y_start += p_cfg->y_offset;
    y_end   += p_cfg->y_offset;

    if(x1 != x_start || x2 != x_end) {
        lcd_data[0] = (x_start >> 8) & 0xFF;
        lcd_data[1] = (x_start & 0xFF);
        lcd_data[2] = (x_end >> 8) & 0xFF;
        lcd_data[3] = (x_end & 0xFF);
        __disp_qspi_send_cmd(p_cfg, p_cfg->cmd_caset, lcd_data, sizeof(lcd_data));
        x1 = x_start;
        x2 = x_end;
    }

    if(y1 != y_start || y2 != y_end) {
        lcd_data[0] = (y_start >> 8) & 0xFF;
        lcd_data[1] = (y_start & 0xFF);
        lcd_data[2] = (y_end >> 8) & 0xFF;
        lcd_data[3] = (y_end & 0xFF);
        __disp_qspi_send_cmd(p_cfg, p_cfg->cmd_raset, lcd_data, sizeof(lcd_data));
        y1 = y_start;
        y2 = y_end;
    }
}

static OPERATE_RET __disp_qspi_send_frame(DISP_QSPI_BASE_CFG_T *p_cfg, TDL_DISP_FRAME_BUFF_T *p_fb)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_QSPI_CMD_T qspi_cmd = {0};

    if (NULL == p_cfg || NULL == p_fb || p_cfg->port >= TUYA_QSPI_NUM_MAX) {
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

    TUYA_CALL_ERR_RETURN(tkl_qspi_send(p_cfg->port, p_fb->frame, p_fb->len));
    TUYA_CALL_ERR_RETURN(tal_semaphore_wait(sg_disp_qspi_sync[p_cfg->port].tx_sem, SEM_WAIT_FOREVER));

    tkl_qspi_force_cs_pin(p_cfg->port, 1);

    return rt;
}

static void __disp_qspi_display_frame(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *p_fb)
{
    DISP_QSPI_DEV_T *disp_qspi_dev = NULL;

    if (NULL == device || NULL == p_fb) {
        return;
    }
    disp_qspi_dev = (DISP_QSPI_DEV_T *)device;

    __disp_qspi_set_window(&disp_qspi_dev->cfg, p_fb->x_start, p_fb->y_start, p_fb->width-1, p_fb->height-1);

    __disp_qspi_send_frame(&disp_qspi_dev->cfg, p_fb);
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

static OPERATE_RET __disp_qspi_sync_init(TUYA_SPI_NUM_E port, \
                                         TDD_DISP_DEV_HANDLE_T device, \
                                         uint8_t is_period_flush)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_DISP_QSPI_SYNC_T *qspi_sync = NULL;

    if(port >= TUYA_SPI_NUM_MAX || device == NULL) {
        return OPRT_INVALID_PARM;
    }

    qspi_sync = &sg_disp_qspi_sync[port];

    qspi_sync->port = port;
    qspi_sync->device = device;

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(qspi_sync->tx_sem), 0, 1));

    if(NULL == qspi_sync->exit_sem) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(qspi_sync->exit_sem), 0, 1));
    }

    if(NULL == qspi_sync->queue) {
        tal_queue_create_init(&(qspi_sync->queue), sizeof(TDD_DISP_QSPI_MSG_T), 4);
    }

    if(NULL == qspi_sync->task) {
        THREAD_CFG_T thread_cfg = {4096, THREAD_PRIO_0, "qspi_task"};
        TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&(qspi_sync->task), \
                                                         NULL, NULL, __display_qspi_task,\
                                                         (const void *)qspi_sync, &thread_cfg));
    }

    return rt;
}

static void __display_qspi_task(void *args)
{
    OPERATE_RET rt = 0;
    TDD_DISP_QSPI_MSG_T msg = {0};
    TDD_DISP_QSPI_SYNC_T *qspi_sync = (TDD_DISP_QSPI_SYNC_T *)args;
    uint32_t wait_queue_time = QUEUE_WAIT_FOREVER;

    if(NULL == qspi_sync) {
        PR_ERR("qspi_sync is null");
        return;
    }

    wait_queue_time = (qspi_sync->is_period_flush) ? 15 : QUEUE_WAIT_FOREVER;

    qspi_sync->is_task_running = 1;

    while(qspi_sync->is_task_running) {
        rt = tal_queue_fetch(qspi_sync->queue, &msg, wait_queue_time);
        if(rt !=  OPRT_OK){
            if(qspi_sync->is_period_flush) {
                msg.event = TDD_QSPI_FRAME_REQUEST;   
                msg.frame_buff = qspi_sync->display_fb;
            }else {
                continue;
            }
        }

        switch(msg.event) {
            case TDD_QSPI_FRAME_REQUEST:
                __disp_qspi_display_frame(qspi_sync->device, msg.frame_buff);

                if(qspi_sync->is_period_flush) {
                    if(qspi_sync->display_fb != msg.frame_buff) {
                        TDL_DISP_FRAME_BUFF_T *tmp_fb = NULL;
                        tmp_fb = qspi_sync->display_fb;
                        qspi_sync->display_fb = msg.frame_buff;
                        if(tmp_fb != NULL && tmp_fb->free_cb) {
                            tmp_fb->free_cb(tmp_fb);
                        }
                    }
                }else {
                    if (msg.frame_buff != NULL && msg.frame_buff->free_cb) {
                        msg.frame_buff->free_cb(msg.frame_buff);
                    }
                }

                break;
            case TDD_QSPI_FRAME_EXIT:
                qspi_sync->is_task_running = 0;
                if(qspi_sync->exit_sem) {
                    while (tal_queue_fetch(qspi_sync->queue, &msg, 0) == OPRT_OK) {
                        if (msg.frame_buff != NULL && msg.frame_buff->free_cb) {
                                msg.frame_buff->free_cb(msg.frame_buff);
                        }
                    }
                    tal_semaphore_post(qspi_sync->exit_sem);
                }
            default:
                break;

        }
    }

    THREAD_HANDLE tmp_task = qspi_sync->task;
    qspi_sync->task = NULL;
    tal_thread_delete(tmp_task);
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

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&(disp_qspi_dev->mutex)));

    TUYA_CALL_ERR_RETURN(__disp_qspi_sync_init(disp_qspi_dev->cfg.port, device,
                                              (disp_qspi_dev->cfg.has_vram) ? 0 : 1 ));

    return OPRT_OK;
}

static OPERATE_RET __tdd_display_qspi_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_QSPI_DEV_T *disp_qspi_dev = NULL;
    TUYA_QSPI_NUM_E port = 0;

    if (NULL == device || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    disp_qspi_dev = (DISP_QSPI_DEV_T *)device;
    port = disp_qspi_dev->cfg.port;

    tal_mutex_lock(disp_qspi_dev->mutex);

    TDD_DISP_QSPI_MSG_T msg = {TDD_QSPI_FRAME_REQUEST, frame_buff};
    TUYA_CALL_ERR_RETURN(tal_queue_post(sg_disp_qspi_sync[port].queue, &msg, SEM_WAIT_FOREVER));

    tal_mutex_unlock(disp_qspi_dev->mutex);

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

    disp_qspi_dev_info.type     = TUYA_DISPLAY_QSPI;
    disp_qspi_dev_info.width    = qspi->cfg.width;
    disp_qspi_dev_info.height   = qspi->cfg.height;
    disp_qspi_dev_info.fmt      = qspi->cfg.pixel_fmt;
    disp_qspi_dev_info.rotation = qspi->rotation;
    disp_qspi_dev_info.is_swap  = qspi->is_swap;
    disp_qspi_dev_info.has_vram = qspi->cfg.has_vram;

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