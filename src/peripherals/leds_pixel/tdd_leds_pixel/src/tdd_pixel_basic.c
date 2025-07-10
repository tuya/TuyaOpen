/**
 * @file tdd_pixel_basic.c
 * @brief TDD layer basic utilities for LED pixel controllers
 *
 * This source file provides basic utility functions for LED pixel controllers,
 * including color data transformation to SPI format, RGB color channel reordering,
 * and transmission control buffer management. These functions are used by various
 * LED pixel controller drivers to handle common operations like data format conversion
 * and memory management for efficient LED strip communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <string.h>

#include "tal_memory.h"

#include "tdd_pixel_basic.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define COLOR_PRIMARY_MAX 5

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @function:tdd_rgb_transform_spi_data
 * @brief: R/G/B 8bit to spi data
 * @param[in]   color_data          color data
 * @param[in]   chip_ic_0           0 code
 * @param[in]   chip_ic_1           1 code
 * @param[out]  spi_data_buf        Converted SPI data
 * @return: none
 */
void tdd_rgb_transform_spi_data(unsigned char color_data, unsigned char chip_ic_0, unsigned char chip_ic_1,
                                unsigned char *spi_data_buf)
{
    unsigned char i = 0;

    for (i = 0; i < 8; i++) {
        spi_data_buf[i] = (color_data & 0x80) ? chip_ic_1 : chip_ic_0;
        color_data <<= 1;
    }

    return;
}

/**
 * @brief        Adjust color line sequence
 *
 * @param[in]   data_buf             Send buffer length
 * @param[out]  spi_buf              Send control parameter buffer
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tdd_rgb_line_seq_transform(unsigned short *data_buf, unsigned short *spi_buf, RGB_ORDER_MODE_E rgb_order)
{
    if (NULL == spi_buf || NULL == data_buf) {
        return OPRT_INVALID_PARM;
    }

    switch (rgb_order) {
    case RGB_ORDER:
        *spi_buf = *(data_buf);
        *(spi_buf + 1) = *(data_buf + 1);
        *(spi_buf + 2) = *(data_buf + 2);
        break;

    case RBG_ORDER:
        *spi_buf = *(data_buf);
        *(spi_buf + 1) = *(data_buf + 2);
        *(spi_buf + 2) = *(data_buf + 1);
        break;

    case GRB_ORDER:
        *spi_buf = *(data_buf + 1);
        *(spi_buf + 1) = *data_buf;
        *(spi_buf + 2) = *(data_buf + 2);
        break;

    case GBR_ORDER:
        *spi_buf = *(data_buf + 1);
        *(spi_buf + 1) = *(data_buf + 2);
        *(spi_buf + 2) = *(data_buf);
        break;

    case BRG_ORDER:
        *spi_buf = *(data_buf + 2);
        *(spi_buf + 1) = *data_buf;
        *(spi_buf + 2) = *(data_buf + 1);
        break;

    case BGR_ORDER:
        *spi_buf = *(data_buf + 2);
        *(spi_buf + 1) = *(data_buf + 1);
        *(spi_buf + 2) = *(data_buf);
        break;
    default:
        break;
    }

    return OPRT_OK;
}

/**
 * @function:tdd_pixel_create_tx_ctrl
 * @brief: Create a buffer to store sending control parameters
 * @param[in]   tx_buff_len         length of buffer
 * @param[out]  p_pixel_tx          the point of DRV_PIXEL_TX_CTRL_T
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_pixel_create_tx_ctrl(unsigned int tx_buff_len, DRV_PIXEL_TX_CTRL_T **p_pixel_tx)
{
    DRV_PIXEL_TX_CTRL_T *tx_ctrl = NULL;
    unsigned int len = 0;

    if (0 == tx_buff_len) {
        return OPRT_INVALID_PARM;
    }

    len = sizeof(DRV_PIXEL_TX_CTRL_T) + tx_buff_len;
    tx_ctrl = (DRV_PIXEL_TX_CTRL_T *)tal_malloc(len);
    if (NULL == tx_ctrl) {
        return OPRT_MALLOC_FAILED;
    }
    memset((unsigned char *)tx_ctrl, 0, len);

    tx_ctrl->tx_buffer = (unsigned char *)(tx_ctrl + 1);
    tx_ctrl->tx_buffer_len = tx_buff_len;

    *p_pixel_tx = tx_ctrl;

    return OPRT_OK;
}
/**
 * @function:tdd_pixel_tx_ctrl_release
 * @brief: Release the buffer for storing sending control parameters
 * @param[in]   tx_ctrl             the point of DRV_PIXEL_TX_CTRL_T
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_pixel_tx_ctrl_release(DRV_PIXEL_TX_CTRL_T *tx_ctrl)
{
    if (NULL == tx_ctrl) {
        return OPRT_INVALID_PARM;
    }

    tal_free(tx_ctrl);

    return OPRT_OK;
}

/**
 * @brief      BK platform SPI driver for colorful LED strips requires special handling, this interface is implemented
 * here for cross-platform compatibility
 *
 * @param[in]   none
 *
 * @return none
 */
__attribute__((weak)) void tkl_spi_set_spic_flag(void)
{
    return;
}
