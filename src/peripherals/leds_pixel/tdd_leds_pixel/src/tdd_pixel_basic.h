/**
 * @file tdd_pixel_basic.h
 * @brief TDD layer basic utilities header for LED pixel controllers
 *
 * This header file declares basic utility functions for LED pixel controllers,
 * including color data transformation to SPI format, RGB color channel reordering,
 * and transmission control buffer management. These functions provide common
 * operations that are shared across different LED pixel controller drivers.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_BASIC_H__
#define __TDD_PIXEL_BASIC_H__

#include "tdd_pixel_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define ONE_BYTE_LEN 8

/***********************************************************
****************************typedef define****************************
*********************************************************************/

typedef struct {
    unsigned char *tx_buffer;   // Data -> buffer after data stream is converted to SPI data
    unsigned int tx_buffer_len; // Data length -> length of buffer after data stream is converted to SPI data
} DRV_PIXEL_TX_CTRL_T;

/***********************************************************
****************************function define***************************
***********************************************************/

void tdd_rgb_transform_spi_data(unsigned char color_data, unsigned char chip_ic_0, unsigned char chip_ic_1,
                                unsigned char *spi_data_buf);

OPERATE_RET tdd_rgb_line_seq_transform(unsigned short *data_buf, unsigned short *spi_buf, RGB_ORDER_MODE_E rgb_order);

OPERATE_RET tdd_pixel_create_tx_ctrl(unsigned int tx_buff_len, DRV_PIXEL_TX_CTRL_T **p_pixel_tx);

OPERATE_RET tdd_pixel_tx_ctrl_release(DRV_PIXEL_TX_CTRL_T *tx_ctrl);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_PIXEL_BASIC_H__ */
