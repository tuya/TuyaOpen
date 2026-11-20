/**
 * @file tdd_display_spi.c
 * @brief TDD display SPI interface implementation
 *
 * This file implements the SPI interface functionality for the TDL display system.
 * It provides hardware abstraction for displays using SPI interface, including SPI
 * initialization, data transmission, command handling, and display controller communication.
 * The implementation supports various SPI configurations and timing parameters.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"

#include "tkl_spi.h"
#include "tkl_gpio.h"
#include "tkl_system.h"

#include "tdd_display_spi.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    SEM_HANDLE    tx_sem;
    SEM_HANDLE    exit_sem;
    QUEUE_HANDLE  queue;
    THREAD_HANDLE spi_task;
    bool          is_task_running;    
}TDD_DISP_SPI_SYNC_T;

typedef enum {
    TDD_SPI_FRAME_REQUEST = 0,
    TDD_SPI_FRAME_EXIT,
} TDD_SPI_FRAME_EVENT_E;

typedef struct {
    TDD_SPI_FRAME_EVENT_E event;
    TDL_DISP_FRAME_BUFF_T *frame_buff;
}TDD_DISP_SPI_MSG_T;

typedef struct {
    DISP_SPI_BASE_CFG_T         cfg;
    const uint8_t              *init_seq;
}DISP_SPI_DEV_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static void __disp_spi_task(void *args);

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_DISP_SPI_SYNC_T sg_disp_spi_sync[TUYA_SPI_NUM_MAX] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __disp_spi_isr_cb(TUYA_SPI_NUM_E port, TUYA_SPI_IRQ_EVT_E event)
{
    if (event == TUYA_SPI_EVENT_TX_COMPLETE) {
        if (sg_disp_spi_sync[port].tx_sem) {
            tal_semaphore_post(sg_disp_spi_sync[port].tx_sem);
        }
    }
}

static OPERATE_RET __disp_spi_gpio_init(DISP_SPI_BASE_CFG_T *p_cfg)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    OPERATE_RET rt = OPRT_OK;

    if (NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = TUYA_GPIO_LEVEL_LOW;

    if(p_cfg->cs_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->cs_pin, &pin_cfg));
    }

    if(p_cfg->dc_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->dc_pin, &pin_cfg));
    }

    if(p_cfg->rst_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->rst_pin, &pin_cfg));
    }

    return rt;
}

static OPERATE_RET __disp_spi_gpio_deinit(DISP_SPI_BASE_CFG_T *p_cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    if(p_cfg->cs_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_CALL_ERR_LOG(tkl_gpio_deinit(p_cfg->cs_pin));
    }

    if(p_cfg->dc_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_CALL_ERR_LOG(tkl_gpio_deinit(p_cfg->dc_pin));
    }

    if(p_cfg->rst_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_CALL_ERR_LOG(tkl_gpio_deinit(p_cfg->rst_pin));
    }

    return rt;
}

static OPERATE_RET __disp_spi_init(TUYA_SPI_NUM_E port, uint32_t spi_clk)
{
    OPERATE_RET rt = OPRT_OK;

    /*spi init*/
    TUYA_SPI_BASE_CFG_T spi_cfg = {.mode = TUYA_SPI_MODE0,
                                   .freq_hz = spi_clk,
                                   .databits = TUYA_SPI_DATA_BIT8,
                                   .bitorder = TUYA_SPI_ORDER_MSB2LSB,
                                   .role = TUYA_SPI_ROLE_MASTER,
                                   .type = TUYA_SPI_AUTO_TYPE,
                                   .spi_dma_flags = 1};

    TUYA_CALL_ERR_RETURN(tkl_spi_init(port, &spi_cfg));
    TUYA_CALL_ERR_RETURN(tkl_spi_irq_init(port, __disp_spi_isr_cb));
    TUYA_CALL_ERR_RETURN(tkl_spi_irq_enable(port));

    PR_NOTICE("SPI%d init success, clk: %d", port, spi_clk);

    return rt;
}

static OPERATE_RET __disp_spi_manage_init(TUYA_SPI_NUM_E port, DISP_SPI_DEV_T *disp_spi_dev)
{
    OPERATE_RET rt = OPRT_OK;

    if(port >= TUYA_SPI_NUM_MAX || disp_spi_dev == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == sg_disp_spi_sync[port].tx_sem) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(sg_disp_spi_sync[port].tx_sem), 0, 1));
    }

    if(NULL == sg_disp_spi_sync[port].exit_sem) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(sg_disp_spi_sync[port].exit_sem), 0, 1));
    }

    if(NULL == sg_disp_spi_sync[port].queue) {
        tal_queue_create_init(&(sg_disp_spi_sync[port].queue), sizeof(TDD_DISP_SPI_MSG_T), 4);
    }

    if(NULL == sg_disp_spi_sync[port].spi_task) {
        THREAD_CFG_T thread_cfg = {4096, THREAD_PRIO_1, "spi_task"};
        TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&(sg_disp_spi_sync[port].spi_task), 
                                                         NULL, NULL, __disp_spi_task,
                                                         (const void *)disp_spi_dev, &thread_cfg));
    }

    return rt;
}


static void __disp_device_reset(TUYA_GPIO_NUM_E rst_pin)
{
    if(rst_pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(100);

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(100);

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(100);
}

static OPERATE_RET __disp_spi_send(TUYA_SPI_NUM_E port, uint8_t *data, uint32_t size)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t left_len = size, send_len = 0;
    uint32_t dma_max_size = tkl_spi_get_max_dma_data_length();

    if(NULL == sg_disp_spi_sync[port].tx_sem) {
        PR_ERR("tx sem not init, port:%d\r\n", port);
        return OPRT_COM_ERROR;
    }

    while (left_len > 0) {
        send_len = (left_len > dma_max_size) ? dma_max_size : (left_len);

        TUYA_CALL_ERR_RETURN(tkl_spi_send(port, data + size - left_len, send_len));

        rt = tal_semaphore_wait(sg_disp_spi_sync[port].tx_sem, 100); 
        if(rt != OPRT_OK) {
            PR_ERR("spi tx wait timeout, port:%d len:%d\r\n", port, send_len);
            return rt;
        }

        left_len -= send_len;
    }

    return rt;
}

static void __disp_spi_set_window(DISP_SPI_BASE_CFG_T *p_cfg, uint16_t x_start, uint16_t y_start,\
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
        tdd_disp_spi_send_cmd(p_cfg, p_cfg->cmd_caset);
        tdd_disp_spi_send_data(p_cfg, lcd_data, 4);
        x1 = x_start;
        x2 = x_end;
    }

    if(y1 != y_start || y2 != y_end) {
        lcd_data[0] = (y_start >> 8) & 0xFF;
        lcd_data[1] = (y_start & 0xFF);
        lcd_data[2] = (y_end >> 8) & 0xFF;
        lcd_data[3] = (y_end & 0xFF);
        tdd_disp_spi_send_cmd(p_cfg, p_cfg->cmd_raset);
        tdd_disp_spi_send_data(p_cfg, lcd_data, 4);
        y1 = y_start;
        y2 = y_end;
    }
}

static void __disp_spi_display_frame(DISP_SPI_DEV_T *disp_spi_dev, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    uint16_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;

    if(disp_spi_dev == NULL ||frame_buff == NULL) {
        PR_ERR("param null\r\n");
        return;
    }

    x0 = frame_buff->x_start;
    y0 = frame_buff->y_start;
    x1 = frame_buff->x_start + frame_buff->width - 1;
    y1 = frame_buff->y_start + frame_buff->height - 1;

    __disp_spi_set_window(&disp_spi_dev->cfg, x0, y0, x1, y1);

    tdd_disp_spi_send_cmd(&disp_spi_dev->cfg, disp_spi_dev->cfg.cmd_ramwr);
    tdd_disp_spi_send_data(&disp_spi_dev->cfg, frame_buff->frame, frame_buff->len);
}

static void __disp_spi_task(void *args)
{
    OPERATE_RET rt = 0;
    TDD_DISP_SPI_MSG_T msg = {0};
    DISP_SPI_DEV_T *disp_spi_dev = (DISP_SPI_DEV_T *)args;
    TUYA_SPI_NUM_E port = 0;

    if(disp_spi_dev == NULL) {
        PR_ERR("disp spi task args null\r\n");
        return;
    }

    port = disp_spi_dev->cfg.port;
    sg_disp_spi_sync[port].is_task_running = 1;
    PR_NOTICE("__disp_spi_task start, port:%d\r\n", port);

    while (sg_disp_spi_sync[port].is_task_running) {
        rt = tal_queue_fetch(sg_disp_spi_sync[port].queue, &msg, QUEUE_WAIT_FOREVER);
        if(rt != OPRT_OK) {
            continue;
        }

        switch(msg.event) {
        case TDD_SPI_FRAME_REQUEST: {
            __disp_spi_display_frame(disp_spi_dev, msg.frame_buff);
            if (msg.frame_buff != NULL && msg.frame_buff->free_cb) {
                msg.frame_buff->free_cb(msg.frame_buff);
            }
        }
        break;
        case TDD_SPI_FRAME_EXIT:{
            sg_disp_spi_sync[port].is_task_running = false;

            if(sg_disp_spi_sync[port].exit_sem) {
                while (tal_queue_fetch(sg_disp_spi_sync[port].queue, &msg, 0) == OPRT_OK) {
                    if (msg.frame_buff != NULL && msg.frame_buff->free_cb) {
                            msg.frame_buff->free_cb(msg.frame_buff);
                    }
                }
                tal_semaphore_post(sg_disp_spi_sync[port].exit_sem);
            }
        }
        break;
        default:
        break;
        }
    }
    
    PR_NOTICE("__disp_spi_task exit, port:%d\r\n", port);

    THREAD_HANDLE tmp_task = sg_disp_spi_sync[port].spi_task;
    sg_disp_spi_sync[port].spi_task = NULL;
    tal_thread_delete(tmp_task);
}


static OPERATE_RET __tdd_display_spi_open(TDD_DISP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SPI_DEV_T *disp_spi_dev = NULL;
    TUYA_SPI_NUM_E port = 0;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }
    disp_spi_dev = (DISP_SPI_DEV_T *)device;
    port = disp_spi_dev->cfg.port;

    PR_NOTICE("spi port :%d", port);

    TUYA_CALL_ERR_RETURN(__disp_spi_manage_init(port, disp_spi_dev));

    TUYA_CALL_ERR_RETURN(__disp_spi_init(port, disp_spi_dev->cfg.spi_clk));
    TUYA_CALL_ERR_RETURN(__disp_spi_gpio_init(&disp_spi_dev->cfg));

    tdd_disp_spi_init_seq(&(disp_spi_dev->cfg), disp_spi_dev->init_seq);

    return OPRT_OK;
}

static OPERATE_RET __tdd_display_spi_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SPI_DEV_T *disp_spi_dev = NULL;
    TUYA_SPI_NUM_E port = 0;


    if (NULL == device || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = (DISP_SPI_DEV_T *)device;
    port = disp_spi_dev->cfg.port;

    TDD_DISP_SPI_MSG_T msg = {TDD_SPI_FRAME_REQUEST, frame_buff};
    TUYA_CALL_ERR_RETURN(tal_queue_post(sg_disp_spi_sync[port].queue, &msg, SEM_WAIT_FOREVER));

    return rt;
}

static OPERATE_RET __tdd_display_spi_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_SPI_DEV_T *disp_spi_dev = NULL;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = (DISP_SPI_DEV_T *)device;
    
    __disp_spi_gpio_deinit(&disp_spi_dev->cfg);

    return OPRT_OK;
}

/**
 * @brief Initializes the SPI interface for display communication.
 *
 * This function sets up the SPI port and its associated semaphore for synchronization, 
 * and initializes the required GPIO pins for SPI-based display operations.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing port and clock settings.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if initialization fails.
 */
OPERATE_RET tdd_disp_spi_init(DISP_SPI_BASE_CFG_T *p_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_DISP_SPI_SYNC_T *spi_sync = NULL;

    if(NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    spi_sync = &sg_disp_spi_sync[p_cfg->port];
    if(NULL == spi_sync->tx_sem) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(spi_sync->tx_sem), 0, 1));
    }

    TUYA_CALL_ERR_RETURN(__disp_spi_init(p_cfg->port, p_cfg->spi_clk));
    TUYA_CALL_ERR_RETURN(__disp_spi_gpio_init(p_cfg));

    return OPRT_OK;
}

/**
 * @brief Sends a command over the SPI interface to the display device.
 *
 * This function pulls the chip select (CS) and data/command (DC) pins low to indicate 
 * command transmission, then sends the specified command byte via SPI.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param cmd The command byte to be sent to the display.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if sending the command fails.
 */
OPERATE_RET tdd_disp_spi_send_cmd(DISP_SPI_BASE_CFG_T *p_cfg, uint8_t cmd)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    if(p_cfg->cs_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_LOW);
    }

    if(p_cfg->dc_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(p_cfg->dc_pin, TUYA_GPIO_LEVEL_LOW);
    }

    rt = __disp_spi_send(p_cfg->port, &cmd, 1);

    if(p_cfg->cs_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_HIGH);
    }

    return rt;
}

/**
 * @brief Sends data over the SPI interface to the display device.
 *
 * This function pulls the chip select (CS) pin low and sets the data/command (DC) pin high 
 * to indicate data transmission, then sends the specified data buffer via SPI.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param data Pointer to the data buffer to be sent.
 * @param data_len Length of the data buffer in bytes.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if sending the data fails.
 */
OPERATE_RET tdd_disp_spi_send_data(DISP_SPI_BASE_CFG_T *p_cfg, uint8_t *data, uint32_t data_len)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == p_cfg || NULL == data || data_len == 0) {
        return OPRT_INVALID_PARM;
    }

    if(p_cfg->cs_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_LOW);
    }

    if(p_cfg->dc_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(p_cfg->dc_pin, TUYA_GPIO_LEVEL_HIGH);
    }

    rt = __disp_spi_send(p_cfg->port, data, data_len);

    if(p_cfg->cs_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_HIGH);
    }

    return rt;
}

/**
 * @brief Executes the display initialization sequence over SPI.
 *
 * This function processes a command-based initialization sequence, sending commands 
 * and associated data to the display device to configure it during initialization.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param init_seq Pointer to the initialization sequence array (command/data format).
 *
 * @return None.
 */
void tdd_disp_spi_init_seq(DISP_SPI_BASE_CFG_T *p_cfg, const uint8_t *init_seq)
{
	uint8_t *init_line = (uint8_t *)init_seq, *p_data = NULL;
    uint8_t data_len = 0, sleep_time = 0, cmd = 0;

    __disp_device_reset(p_cfg->rst_pin);

    PR_NOTICE("init_seq:%p", init_seq);

    while (*init_line) {
        data_len   = init_line[0] - 1;
        sleep_time = init_line[1];
        cmd        = init_line[2];

        if(data_len) {
            p_data = &init_line[3];
        }else {
            p_data = NULL;
        }

        tdd_disp_spi_send_cmd(p_cfg, cmd);
	    tdd_disp_spi_send_data(p_cfg, p_data, data_len);

        if(sleep_time) {
            tal_system_sleep(sleep_time);
        }

        init_line += init_line[0] + 2;
    }

    PR_NOTICE("Display SPI:%d init sequence completed\r\n", p_cfg->port);
}

/**
 * @brief Modifies a parameter in the display initialization sequence for a specific command.
 *
 * This function searches for the specified command in the initialization sequence and 
 * updates the parameter at the given index. If the index is out of bounds, an error is logged.
 *
 * @param init_seq Pointer to the initialization sequence array.
 * @param init_cmd The command whose parameter needs to be modified.
 * @param param The new parameter value to set.
 * @param idx The index of the parameter to modify within the command's data block.
 *
 * @return None.
 */
void tdd_disp_modify_init_seq_param(uint8_t *init_seq, uint8_t init_cmd, uint8_t param, uint8_t idx)
{
	uint8_t *init_line = init_seq;
    uint8_t data_len = 0, cmd = 0;

    if(NULL == init_seq) {
        return;
    }

    while (*init_line) {
        data_len   = init_line[0] - 1;
        cmd        = init_line[2];

        if(init_cmd == cmd) {
            if(idx < data_len) {
                init_line[3 + idx] = param;
            }else {
                PR_ERR("Index %d out of bounds for command 0x%02X with param length %d", idx, init_cmd, data_len);
            }
            return;
        }

        init_line += init_line[0] + 2;
    }
}


/**
 * @brief Registers an RGB display device with the display management system.
 *
 * This function creates and initializes a new RGB display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param rgb Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_device_register(char *name, TDD_DISP_SPI_CFG_T *spi)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SPI_DEV_T *disp_spi_dev = NULL;
    TDD_DISP_DEV_INFO_T disp_spi_dev_info;

    if (NULL == name || NULL == spi) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = tal_malloc(sizeof(DISP_SPI_DEV_T));
    if (NULL == disp_spi_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(disp_spi_dev, 0x00, sizeof(DISP_SPI_DEV_T));

    memcpy(&disp_spi_dev->cfg, &spi->cfg, sizeof(DISP_SPI_BASE_CFG_T));

    disp_spi_dev->init_seq       = spi->init_seq;

    disp_spi_dev_info.type     = TUYA_DISPLAY_SPI;
    disp_spi_dev_info.width    = spi->cfg.width;
    disp_spi_dev_info.height   = spi->cfg.height;
    disp_spi_dev_info.fmt      = spi->cfg.pixel_fmt;
    disp_spi_dev_info.rotation = spi->rotation;
    disp_spi_dev_info.is_swap  = spi->is_swap;
    disp_spi_dev_info.has_vram = true;

    memcpy(&disp_spi_dev_info.bl, &spi->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));
    memcpy(&disp_spi_dev_info.power, &spi->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    TDD_DISP_INTFS_T disp_spi_intfs = {
        .open  = __tdd_display_spi_open,
        .flush = __tdd_display_spi_flush,
        .close = __tdd_display_spi_close,
    };

    TUYA_CALL_ERR_RETURN(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_spi_dev,\
                                                 &disp_spi_intfs, &disp_spi_dev_info));

    return OPRT_OK;
}