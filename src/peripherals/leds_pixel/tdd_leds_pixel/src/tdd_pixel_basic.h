/**
 * @file tdd_pixel_basic.h
 * @author www.tuya.com
 * @brief tdd_pixel_basic module is used to provid chip basic driver api
 * @version 0.1
 * @date 2022-07-14
 *
 * @copyright Copyright (c) tuya.inc 2022
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
***********************typedef define***********************
***********************************************************/

typedef struct {
    unsigned char *tx_buffer;   // 数据 -> 数据流转换成SPI数据后的buf
    unsigned int tx_buffer_len; // 数据长度 -> 数据流转换成SPI数据后的buf的长度
} DRV_PIXEL_TX_CTRL_T;

/***********************************************************
********************function declaration********************
***********************************************************/

void tdd_rgb_transform_spi_data(unsigned char color_data, unsigned char chip_ic_0, unsigned char chip_ic_1,
                                unsigned char *spi_data_buf);


OPERATE_RET tdd_rgb_line_seq_transform(unsigned short *data_buf, unsigned short *spi_buf, RGB_ORDER_MODE_E rgb_order);


OPERATE_RET tdd_pixel_create_tx_ctrl(unsigned int tx_buff_len, DRV_PIXEL_TX_CTRL_T **p_pixel_tx);

OPERATE_RET tdd_pixel_tx_ctrl_release( DRV_PIXEL_TX_CTRL_T *tx_ctrl);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_PIXEL_BASIC_H__ */
