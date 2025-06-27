/**
 * @file tdl_display_spi.c
 * @version 0.1
 * @date 2025-05-27
 */
#include "tal_api.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI==1)
#include "tkl_spi.h"
#include "tkl_gpio.h"
#include "tdl_display_manage.h"
#include "tdl_display_driver.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    SEM_HANDLE tx_sem;
} DISP_SPI_SYNC_T;

typedef struct {
    DISP_SPI_BASE_CFG_T       cfg;
    const uint8_t            *init_seq;
}DISP_SPI_DEV_T;

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static DISP_SPI_SYNC_T sg_disp_spi_sync[TUYA_SPI_NUM_MAX] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __disp_spi_isr_cb(TUYA_SPI_NUM_E port, TUYA_SPI_IRQ_EVT_E event)
{
    if(event == TUYA_SPI_EVENT_TX_COMPLETE) {
        if(sg_disp_spi_sync[port].tx_sem) {
            tal_semaphore_post(sg_disp_spi_sync[port].tx_sem);
        }
   }
}

static OPERATE_RET __disp_spi_gpio_init(DISP_SPI_BASE_CFG_T *p_cfg)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    OPERATE_RET rt = OPRT_OK;

    if(NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    pin_cfg.mode   = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level  = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->cs_pin, &pin_cfg));
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->dc_pin, &pin_cfg));
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->rst_pin, &pin_cfg));

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

    return rt;
}

static void __disp_device_reset(TUYA_GPIO_NUM_E rst_pin)
{
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

    while(left_len > 0) {
        send_len = (left_len > dma_max_size) ? dma_max_size : (left_len);
        TUYA_CALL_ERR_RETURN(tkl_spi_send(port, data + size - left_len, send_len));

        TUYA_CALL_ERR_RETURN(tal_semaphore_wait(sg_disp_spi_sync[port].tx_sem, 5000));

        left_len -= send_len;
    }

    return rt;
}

static void __disp_spi_set_window(DISP_SPI_BASE_CFG_T *p_cfg)
{
    uint8_t lcd_data[4];

    if(NULL == p_cfg) {
        return;
    }

    lcd_data[0] = 0;
    lcd_data[1] = 0;
    lcd_data[2] = (p_cfg->width >> 8) & 0xFF;
    lcd_data[3] = (p_cfg->width & 0xFF) - 1;
    tdl_disp_spi_send_cmd(p_cfg, p_cfg->cmd_caset);
    tdl_disp_spi_send_data(p_cfg, lcd_data, 4);

    lcd_data[0] = 0;
    lcd_data[1] = 0;
    lcd_data[2] = (p_cfg->height >> 8) & 0xFF;
    lcd_data[3] = (p_cfg->height & 0xFF) - 1;
    tdl_disp_spi_send_cmd(p_cfg, p_cfg->cmd_raset);
    tdl_disp_spi_send_data(p_cfg, lcd_data, 4);
}

OPERATE_RET tdl_disp_spi_init(DISP_SPI_BASE_CFG_T *p_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SPI_SYNC_T *spi_sync = NULL;

    if(NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    spi_sync = &sg_disp_spi_sync[p_cfg->port];
    if(NULL == spi_sync->tx_sem) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(spi_sync->tx_sem), 0, 1));
    }

    TUYA_CALL_ERR_RETURN(__disp_spi_init(p_cfg->port, p_cfg->spi_clk));
    TUYA_CALL_ERR_RETURN(__disp_spi_gpio_init(p_cfg));

    PR_NOTICE("SPI%d init success, clk: %d", p_cfg->port, p_cfg->spi_clk);

    return OPRT_OK;
}

OPERATE_RET tdl_disp_spi_send_cmd(DISP_SPI_BASE_CFG_T *p_cfg, uint8_t cmd)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(p_cfg->dc_pin, TUYA_GPIO_LEVEL_LOW);

    rt = __disp_spi_send(p_cfg->port, &cmd, 1);

    tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_HIGH);

    return rt;
}

OPERATE_RET tdl_disp_spi_send_data(DISP_SPI_BASE_CFG_T *p_cfg, uint8_t *data, uint32_t data_len)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == p_cfg || NULL == data || data_len == 0) {
        return OPRT_INVALID_PARM;
    }

    tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(p_cfg->dc_pin, TUYA_GPIO_LEVEL_HIGH);

    rt = __disp_spi_send(p_cfg->port, data, data_len);

    tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_HIGH);

    return rt;
}

void tdl_disp_spi_init_seq(DISP_SPI_BASE_CFG_T *p_cfg, const uint8_t *init_seq)
{
	uint8_t *init_line = (uint8_t *)init_seq, *p_data = NULL;
    uint8_t data_len = 0, sleep_time = 0, cmd = 0;

    __disp_device_reset(p_cfg->rst_pin);

    while (*init_line) {
        data_len   = init_line[0] - 1;
        sleep_time = init_line[1];
        cmd        = init_line[2];

        if(data_len) {
            p_data = &init_line[3];
        }else {
            p_data = NULL;
        }

        tdl_disp_spi_send_cmd(p_cfg, cmd);
	    tdl_disp_spi_send_data(p_cfg, p_data, data_len);

        tal_system_sleep(sleep_time);
        init_line += init_line[0] + 2;
    }

    PR_NOTICE("Display SPI init sequence completed");
}

void tdl_disp_modify_init_seq_param(uint8_t *init_seq, uint8_t init_cmd, uint8_t param, uint8_t idx)
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

static OPERATE_RET __tdl_display_spi_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_SPI_DEV_T *disp_spi_dev = NULL;

    if(NULL == device) {
        return OPRT_INVALID_PARM;
    }
    disp_spi_dev = (DISP_SPI_DEV_T *)device;

    tdl_disp_spi_init(&(disp_spi_dev->cfg));

    tdl_disp_spi_init_seq(&(disp_spi_dev->cfg), disp_spi_dev->init_seq);

    return OPRT_OK;
}

static OPERATE_RET __tdl_display_spi_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SPI_DEV_T *disp_spi_dev = NULL;

    if(NULL == device || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = (DISP_SPI_DEV_T *)device;

    __disp_spi_set_window(&disp_spi_dev->cfg);

    tdl_disp_spi_send_cmd(&disp_spi_dev->cfg, disp_spi_dev->cfg.cmd_ramwr);
    tdl_disp_spi_send_data(&disp_spi_dev->cfg, frame_buff->frame, frame_buff->len);

    return rt;
}

static OPERATE_RET __tdl_display_spi_close(TDD_DISP_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tdl_disp_spi_device_register(char *name, TDD_DISP_SPI_CFG_T *spi)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SPI_DEV_T *disp_spi_dev = NULL;
    TDD_DISP_DEV_INFO_T disp_spi_dev_info;

    if(NULL == name || NULL == spi) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = tal_malloc(sizeof(DISP_SPI_DEV_T));
    if(NULL == disp_spi_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(disp_spi_dev, 0x00,sizeof(DISP_SPI_DEV_T));

    memcpy(&disp_spi_dev->cfg, &spi->cfg, sizeof(DISP_SPI_BASE_CFG_T));

    disp_spi_dev->init_seq = spi->init_seq;

    disp_spi_dev_info.type       = TUYA_DISPLAY_SPI;
    disp_spi_dev_info.width      = spi->cfg.width;
    disp_spi_dev_info.height     = spi->cfg.height;
    disp_spi_dev_info.fmt        = spi->cfg.pixel_fmt;
    disp_spi_dev_info.rotation   = spi->rotation;

    memcpy(&disp_spi_dev_info.bl, &spi->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));
    memcpy(&disp_spi_dev_info.power, &spi->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    TDD_DISP_INTFS_T disp_spi_intfs = {
        .open  = __tdl_display_spi_open,
        .flush = __tdl_display_spi_flush,
        .close = __tdl_display_spi_close,
    };

    TUYA_CALL_ERR_RETURN(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_spi_dev,
                                                  &disp_spi_intfs, &disp_spi_dev_info));

    return OPRT_OK;
}

#endif