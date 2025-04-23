/**
 * @file tdd_pixel_basic.c
 * @author www.tuya.com
 * @brief tdd_pixel_basic module is used to provid chip basic driver api
 * @version 0.1
 * @date 2022-07-14
 *
 * @copyright Copyright (c) tuya.inc 2022
 *
 */
#include <string.h>

#include "tal_memory.h"

#include "tdd_pixel_basic.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define COLOR_PRIMARY_MAX            5

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
* @brief       rgb转成spi数据
*
* @param[in]   color_data          颜色数据
* @param[in]   chip_ic_0           0码
* @param[in]   chip_ic_1           1码
* @param[out]  spi_data_buf        转化后的spi数据
*
* @return none
*/
void tdd_rgb_transform_spi_data(unsigned char color_data, unsigned char chip_ic_0, \
                                         unsigned char chip_ic_1,  unsigned char *spi_data_buf)
{
    unsigned char i = 0; 
    
    for (i = 0; i < 8; i++) {
        spi_data_buf[i] =  (color_data & 0x80) ? chip_ic_1 : chip_ic_0;
        color_data <<= 1;       
    }

    return;
}

/**
* @brief        调整颜色线序
*
* @param[in]   data_buf             发送缓存长度
* @param[out]  spi_buf              发送控制参数缓存
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tdd_rgb_line_seq_transform(unsigned short *data_buf, unsigned short *spi_buf, RGB_ORDER_MODE_E rgb_order)
{
    if (NULL == spi_buf || NULL == data_buf) {
        return OPRT_INVALID_PARM;
    }

    switch(rgb_order){
        case RGB_ORDER:
            *spi_buf       = *(data_buf);
            *(spi_buf + 1) = *(data_buf + 1);
            *(spi_buf + 2) = *(data_buf + 2);
            break;

        case RBG_ORDER:
            *spi_buf       = *(data_buf);
            *(spi_buf + 1) = *(data_buf + 2);
            *(spi_buf + 2) = *(data_buf + 1);
            break;

        case GRB_ORDER:
            *spi_buf       = *(data_buf + 1);
            *(spi_buf + 1) = *data_buf ;
            *(spi_buf + 2) = *(data_buf + 2);
            break;

        case GBR_ORDER:
            *spi_buf       = *(data_buf + 1);
            *(spi_buf + 1) = *(data_buf + 2) ;
            *(spi_buf + 2) = *(data_buf);
            break;
        
        case BRG_ORDER:
            *spi_buf       = *(data_buf + 2);
            *(spi_buf + 1) = *data_buf ;
            *(spi_buf + 2) = *(data_buf + 1);
            break;
        
        case BGR_ORDER:
            *spi_buf       = *(data_buf + 2);
            *(spi_buf + 1) = *(data_buf + 1) ;
            *(spi_buf + 2) = *(data_buf);
            break;
        default:
            break;
    }

    return OPRT_OK;
}

/**
* @brief      创建存放发送控制参数的缓存
*
* @param[in]   tx_buff_len          发送缓存长度
* @param[out]  p_pixel_tx           发送控制参数缓存
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
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
* @brief      释放存放发送控制参数的缓存
*
* @param[]   tx_ctrl           发送控制参数缓存
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tdd_pixel_tx_ctrl_release( DRV_PIXEL_TX_CTRL_T *tx_ctrl)
{
    if (NULL == tx_ctrl) {
        return OPRT_INVALID_PARM;
    }

    tal_free(tx_ctrl);

	return OPRT_OK;
}

/**
* @brief      BK 平台 SPI 驱动幻彩灯带需要特殊处理，这里为了能够跨平台实现该接口
*
* @param[in]   none
*
* @return none
*/
__attribute__((weak)) void tkl_spi_set_spic_flag(void)
{
    return;
}
