/**
 * @file tdd_disp_qspi_co5300.c

 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tdd_display_qspi.h"
#include "tdd_disp_co5300.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define CO5300_X_OFFSET         6
#define CO5300_Y_OFFSET         0

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
const uint8_t cCO5300_INIT_SEQ[] = {
    2,  0,   0xFE, 0x20,
    2,  0,   0x19, 0x10,
    2,  0,   0x1C, 0xa0,
    2,  0,   0xFE, 0x00,
    2,  0,   0xC4, 0x80,
    2,  0,   0x3A, 0x55,    
    2,  0,   0x35, 0x00,
    2,  0,   0x53, 0x20,
    2,  0,   0x51, 0xFF,
    2,  0,   0x63, 0xFF,    
    1, 200,  0x11,
    1,  0,   0x29,
    0, 
};

static TDD_DISP_QSPI_CFG_T sg_disp_qspi_cfg = {
    .cfg = {
        .refresh_method = QSPI_REFRESH_BY_FRAME,
        .pixel_pre_cmd = {
            .cmd = CO5300_WRITE_COLOR,
            .addr[0] = CO5300_ADDR_0,
            .addr[1] = CO5300_ADDR_1,
            .addr[2] = CO5300_ADDR_2,
            .addr_size  = CO5300_ADDR_LEN,
            .cmd_lines  = TUYA_QSPI_1WIRE,
            .addr_lines = TUYA_QSPI_1WIRE,
        },
        .is_pixel_memory = false,
        .cmd_write_reg   = CO5300_WRITE_REG,
    },
    .is_swap = true,
    .init_seq = cCO5300_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
void __tdd_disp_qspi_co5300_set_window(DISP_QSPI_BASE_CFG_T *p_cfg, uint16_t x_start, uint16_t y_start,\
                                           uint16_t x_end, uint16_t y_end)
{
    uint8_t lcd_data[4];
    static uint16_t x1=0, x2=0, y1=0, y2=0;

    if (NULL == p_cfg) {
        return;
    }

    x_start += CO5300_X_OFFSET;
    x_end   += CO5300_X_OFFSET;
    y_start += CO5300_Y_OFFSET;
    y_end   += CO5300_Y_OFFSET;

    if(x1 != x_start || x2 != x_end) {
        lcd_data[0] = (x_start >> 8) & 0xFF;
        lcd_data[1] = (x_start & 0xFF);
        lcd_data[2] = (x_end >> 8) & 0xFF;
        lcd_data[3] = (x_end & 0xFF);
        tdd_disp_qspi_send_cmd(p_cfg, CO5300_CASET, lcd_data, sizeof(lcd_data));
        x1 = x_start;
        x2 = x_end;
    }

    if(y1 != y_start || y2 != y_end) {
        lcd_data[0] = (y_start >> 8) & 0xFF;
        lcd_data[1] = (y_start & 0xFF);
        lcd_data[2] = (y_end >> 8) & 0xFF;
        lcd_data[3] = (y_end & 0xFF);
        tdd_disp_qspi_send_cmd(p_cfg, CO5300_RASET, lcd_data, sizeof(lcd_data));
        y1 = y_start;
        y2 = y_end;
    }
}

OPERATE_RET tdd_disp_qspi_co5300_set_bl(uint8_t value)
{
    if (value == 0) {
        value = 5;
    }
    return tdd_disp_qspi_send_cmd((DISP_QSPI_BASE_CFG_T*)&sg_disp_qspi_cfg, CO5300_BL, &value, 1);
}

OPERATE_RET tdd_disp_qspi_co5300_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_qspi_co5300_register: %s", name);

    sg_disp_qspi_cfg.cfg.width     = dev_cfg->width;
    sg_disp_qspi_cfg.cfg.height    = dev_cfg->height;
    sg_disp_qspi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_qspi_cfg.cfg.port      = dev_cfg->port;
    sg_disp_qspi_cfg.cfg.freq_hz   = dev_cfg->spi_clk;
    sg_disp_qspi_cfg.cfg.rst_pin   = dev_cfg->rst_pin;
    sg_disp_qspi_cfg.rotation      = dev_cfg->rotation;
    sg_disp_qspi_cfg.set_window_cb = __tdd_disp_qspi_co5300_set_window;


    memcpy(&sg_disp_qspi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_qspi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_qspi_device_register(name, &sg_disp_qspi_cfg);
}