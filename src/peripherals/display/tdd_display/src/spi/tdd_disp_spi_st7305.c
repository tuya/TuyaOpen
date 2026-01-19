/**
 * @file tdd_disp_spi_st7305.c
 * @brief ST7305 monochrome LCD driver implementation with SPI interface
 *
 * This file provides the implementation for ST7305 monochrome LCD displays using SPI interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for ST7305 displays. The ST7305 is designed for monochrome displays
 * commonly used in industrial and embedded applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_memory.h"

#include "tdl_display_manage.h"

#include "tdd_display_spi.h"
#include "tdd_disp_st7305.h"

/***********************************************************
***********************MACRO define**********************
***********************************************************/
#define GET_ROUND_UP_TO_MULTI_OF_3(num) (((num) % 3 == 0) ? (num) : ((num) + (3 - (num) % 3)))

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    DISP_SPI_BASE_CFG_T cfg;
    TDL_DISP_FRAME_BUFF_T *convert_fb; // Frame buffer for conversion
} DISP_ST7305_DEV_T;

/***********************************************************
***********************const define**********************
***********************************************************/
static uint8_t ST7305_INIT_SEQ[] = {
    3,  0,   0xD6, 0x13, 0x02,                                                 // NVM Load Control
    2,  0,   0xD1, 0x01,                                                       // Booster Enable
    3,  0,   0xC0, 0X12, 0X0A,                                                 // Gate Voltage Setting
    5,  0,   0xC1, 0X73, 0X3E, 0X3C, 0X3C,                                     // VSHP Setting (4.8V)
    5,  0,   0xC2, 0X00, 0X21, 0X23, 0X23,                                     // VSLP Setting (0.98V)
    5,  0,   0xC4, 0X32, 0X5C, 0X5A, 0X5A,                                     // VSHN Setting (-3.6V)
    5,  0,   0xC5, 0X32, 0X35, 0X37, 0X37,                                     // VSLN Setting (0.22V)
    3,  0,   0xD8, 0x80, 0xE9,                                                 // OSC Setting
    2,  0,   0xB2, 0x12,                                                       // Frame Rate Control
    11, 0,   0xB3, 0xE5, 0xF6, 0x17, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x71, // Update Period Gate
    9,  0,   0xB4, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45,             // Update Period Gate in LPM
    4,  0,   0x62, 0x32, 0x03, 0x1F,                                           // Gate Timing Control
    2,  0,   0xB7, 0x13,                                                       // Source EQ Enable
    2,  0,   0xB0, 0x60,                                                       // Gate Line Setting 384 line = 96 * 4
    1,  120, 0x11,                                                             // Sleep out
    2,  0,   0xC9, 0x00,                                                       // Source Voltage Select
    2,  0,   0x36, 0x48, // Memory Data Access Control (MX=1, DO=1)
    2,  0,   0x3A, 0x11, // Data Format Select (3 write for 24bit)
    2,  0,   0xB9, 0x20, // Gamma Mode Setting (Mono)
    2,  0,   0xB8, 0x29, // Panel Setting (1-Dot inversion, Frame inversion, One Line Interlace)
    2,  0,   0xD0, 0xFF, // Auto power down OFF
    1,  0,   0x38,       // HPM (High Power Mode
    1,  0,   0x20,       // Display Inversion OFF
    2,  0,   0xBB, 0x4F, // Enable Clear RAM
    1,  10,  0x29,       // Display ON
    0                    // Terminate list
};

static uint8_t *sg_disp_init_seq = ST7305_INIT_SEQ;

/***********************************************************
***********************function define**********************
***********************************************************/
// Pixel data structure is as follows:
// P0 P2 P4 P6
// P1 P3 P5 P7

// Corresponds to one byte of data:
// BIT7 BIT5 BIT3 BIT1
// BIT6 BIT4 BIT2 BIT0
static void __tdd_st7305_convert(TDL_DISP_FRAME_BUFF_T *src_fb, TDL_DISP_FRAME_BUFF_T *dst_fb)
{
    uint16_t k = 0, i = 0, j = 0, y = 0;
    uint8_t b1 = 0, b2 = 0, mix = 0;
    uint32_t src_width_bytes = 0, offset = 0, dst_width_bytes = 0;

    if (NULL == src_fb || NULL == dst_fb) {
        return;
    }

    offset = GET_ROUND_UP_TO_MULTI_OF_3((dst_fb->width + 3) / 4) - (dst_fb->width / 4);
    src_width_bytes = src_fb->width / 8;
    dst_width_bytes = dst_fb->width / 8;

    for (i = 0; i < dst_fb->height; i += 2) {
        if (i >= (src_fb->height)) {
            break; // Skip rows outside the source framebuffer
        }

        k += offset;
        for (j = 0; j < dst_width_bytes; j += 3) {

            for (y = 0; y < 3; y++) {
                if((j + y) >= dst_width_bytes) {
                    break; // Avoid out of bounds
                }

                if ((j + y) >= src_width_bytes) {
                    k += 2;
                    continue; // Avoid out of bounds
                }

                b1 = src_fb->frame[i * src_width_bytes + j + y];
                b2 = src_fb->frame[(i + 1) * src_width_bytes + j + y];

                // First 4 bits
                mix = 0;
                mix = ((b1 & 0x01) << 7) | ((b2 & 0x01) << 6) | ((b1 & 0x02) << 4) | ((b2 & 0x02) << 3) |
                      ((b1 & 0x04) << 1) | ((b2 & 0x04)) | ((b1 & 0x08) >> 2) | ((b2 & 0x08) >> 3);
                dst_fb->frame[k++] = mix;

                // Second 4 bits
                b1 >>= 4;
                b2 >>= 4;
                mix = 0;
                mix = ((b1 & 0x01) << 7) | ((b2 & 0x01) << 6) | ((b1 & 0x02) << 4) | ((b2 & 0x02) << 3) |
                      ((b1 & 0x04) << 1) | ((b2 & 0x04)) | ((b1 & 0x08) >> 2) | ((b2 & 0x08) >> 3);
                dst_fb->frame[k++] = mix;
            }
        }


    }
}

static void __disp_spi_st7305_set_addr(DISP_SPI_BASE_CFG_T *p_cfg)
{
    uint8_t data[2];

    data[0] = p_cfg->x_offset;
    data[1] = p_cfg->x_offset + (p_cfg->width + 11) / (4 * 3) - 1;
    tdd_disp_spi_send_cmd(p_cfg, p_cfg->cmd_caset);
    tdd_disp_spi_send_data(p_cfg, data, sizeof(data));

    data[0] = p_cfg->y_offset;
    data[1] = p_cfg->y_offset + (p_cfg->height + 1) / 2 - 1; // Height is divided by 2 for ST7305
    tdd_disp_spi_send_cmd(p_cfg, p_cfg->cmd_raset);
    tdd_disp_spi_send_data(p_cfg, data, sizeof(data));
}

static OPERATE_RET __tdd_disp_spi_st7305_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_ST7305_DEV_T *disp_spi_dev = NULL;
    uint8_t gate_line = 0;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }
    disp_spi_dev = (DISP_ST7305_DEV_T *)device;

    gate_line = (disp_spi_dev->cfg.height + 3) / 4;

    tdd_disp_modify_init_seq_param(sg_disp_init_seq, 0xB0, gate_line, 0); // Set gate line count

    tdd_disp_spi_init(&(disp_spi_dev->cfg));

    tdd_disp_spi_init_seq(&(disp_spi_dev->cfg), (const uint8_t *)sg_disp_init_seq);
    PR_DEBUG("[ST7305] Initialize display device successful.");

    return OPRT_OK;
}

static OPERATE_RET __tdd_disp_spi_st7305_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_ST7305_DEV_T *disp_spi_dev = NULL;

    if (NULL == device || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = (DISP_ST7305_DEV_T *)device;

    __tdd_st7305_convert(frame_buff, disp_spi_dev->convert_fb);

    __disp_spi_st7305_set_addr(&disp_spi_dev->cfg);

    tdd_disp_spi_send_cmd(&disp_spi_dev->cfg, disp_spi_dev->cfg.cmd_ramwr);
    tdd_disp_spi_send_data(&disp_spi_dev->cfg, disp_spi_dev->convert_fb->frame, disp_spi_dev->convert_fb->len);

    if(frame_buff && frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return rt;
}

static OPERATE_RET __tdd_disp_spi_st7305_close(TDD_DISP_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief Sets the initialization sequence for the ST7305 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_spi_mono_st7305_set_init_seq(uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an ST7305 monochrome display device using the SPI interface with the display management system.
 *
 * This function creates and initializes a new ST7305 display device instance, 
 * configures its frame buffer and hardware-specific settings, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_mono_st7305_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t frame_len = 0, width_bytes = 0;
    DISP_ST7305_DEV_T *disp_spi_dev = NULL;
    TDD_DISP_DEV_INFO_T disp_spi_dev_info;

    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = (DISP_ST7305_DEV_T *)tal_malloc(sizeof(DISP_ST7305_DEV_T));
    if (NULL == disp_spi_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(disp_spi_dev, 0x00, sizeof(DISP_ST7305_DEV_T));

    width_bytes = GET_ROUND_UP_TO_MULTI_OF_3((dev_cfg->width + 3) / 4);
    frame_len = width_bytes * (dev_cfg->height / 2);
    disp_spi_dev->convert_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == disp_spi_dev->convert_fb) {
        return OPRT_MALLOC_FAILED;
    }
    memset(disp_spi_dev->convert_fb->frame, 0x00, frame_len);
    disp_spi_dev->convert_fb->fmt = TUYA_PIXEL_FMT_MONOCHROME;
    disp_spi_dev->convert_fb->width = dev_cfg->width;
    disp_spi_dev->convert_fb->height = dev_cfg->height;

    disp_spi_dev->cfg.width     = dev_cfg->width;
    disp_spi_dev->cfg.height    = dev_cfg->height;
    disp_spi_dev->cfg.x_offset  = dev_cfg->x_offset;
    disp_spi_dev->cfg.y_offset  = dev_cfg->y_offset;
    disp_spi_dev->cfg.pixel_fmt = TUYA_PIXEL_FMT_MONOCHROME;
    disp_spi_dev->cfg.port      = dev_cfg->port;
    disp_spi_dev->cfg.spi_clk   = dev_cfg->spi_clk;
    disp_spi_dev->cfg.cs_pin    = dev_cfg->cs_pin;
    disp_spi_dev->cfg.dc_pin    = dev_cfg->dc_pin;
    disp_spi_dev->cfg.rst_pin   = dev_cfg->rst_pin;
    disp_spi_dev->cfg.cmd_caset = ST7305_CASET; // Column Address
    disp_spi_dev->cfg.cmd_raset = ST7305_RASET; // Row Address
    disp_spi_dev->cfg.cmd_ramwr = ST7305_RAMWR; // Memory Write

    disp_spi_dev_info.type      = TUYA_DISPLAY_SPI;
    disp_spi_dev_info.width     = dev_cfg->width;
    disp_spi_dev_info.height    = dev_cfg->height;
    disp_spi_dev_info.fmt       = TUYA_PIXEL_FMT_MONOCHROME;
    disp_spi_dev_info.rotation  = dev_cfg->rotation;
    disp_spi_dev_info.is_swap   = false;
    disp_spi_dev_info.has_vram  = true;

    memcpy(&disp_spi_dev_info.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&disp_spi_dev_info.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    TDD_DISP_INTFS_T disp_spi_intfs = {
        .open = __tdd_disp_spi_st7305_open,
        .flush = __tdd_disp_spi_st7305_flush,
        .close = __tdd_disp_spi_st7305_close,
    };

    TUYA_CALL_ERR_RETURN(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_spi_dev,\
                                                 &disp_spi_intfs, &disp_spi_dev_info));

    PR_NOTICE("tdd_disp_spi_st7305_register: %s", name);

    return OPRT_OK;
}
