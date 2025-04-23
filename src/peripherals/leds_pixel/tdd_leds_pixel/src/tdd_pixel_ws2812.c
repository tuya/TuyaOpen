/**
 * @file tdd_pixel_ws2812.c
 * @author www.tuya.com
 * @brief tdd_pixel_ws2812 module is used to
 * @version 0.1
 * @date 2022-03-03
 *
 * @copyright Copyright (c) tuya.inc 2022
 *
 */
#include "string.h"
#include "tuya_iot_config.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI)
#include "tal_log.h"
#include "tkl_spi.h"

#include "tdl_pixel_driver.h"
#include "tdd_pixel_basic.h"
#include "tdd_pixel_ws2812.h"
/*********************************************************************
******************************macro define****************************
*********************************************************************/
/* SPI波特率 */
#define DRV_SPI_SPEED 6600000

/* SPI 0、1码对应的数据 */
#define DRVICE_DATA_0 0XC0   //11000000
#define DRVICE_DATA_1 0XF0   //11110000

#define COLOR_PRIMARY_NUM 3
#define COLOR_RESOLUTION  255

/*********************************************************************
****************************typedef define****************************
*********************************************************************/

/*********************************************************************
****************************variable define***************************
*********************************************************************/
static PIXEL_DRIVER_CONFIG_T driver_info;
/*********************************************************************
****************************function define***************************
*********************************************************************/
/**
 * @function:tdd_2812_driver_open
 * @brief: 打开（初始化）设备
 * @param[in]: pixel_num -> 像素点数
 * @param[out]: *handle  -> 设备句柄
 * @return: success -> 0  fail -> else
 */
OPERATE_RET tdd_2812_driver_open(OUT DRIVER_HANDLE_T *handle, unsigned short pixel_num)
{
    OPERATE_RET op_ret = OPRT_OK;
    TUYA_SPI_BASE_CFG_T spi_cfg = {0};
    DRV_PIXEL_TX_CTRL_T *pixels_send = NULL;
    unsigned int tx_buf_len = 0;

    if (NULL == handle || (0 == pixel_num)) {
        return OPRT_INVALID_PARM;
    }
    extern void tkl_spi_set_spic_flag(void);
    tkl_spi_set_spic_flag();
    spi_cfg.role = TUYA_SPI_ROLE_MASTER;
    spi_cfg.mode = TUYA_SPI_MODE0;
    spi_cfg.type = TUYA_SPI_SOFT_TYPE;
    spi_cfg.databits = TUYA_SPI_DATA_BIT8;
    spi_cfg.freq_hz = DRV_SPI_SPEED;
    spi_cfg.spi_dma_flags = TRUE;
    op_ret = tkl_spi_init(driver_info.port, &spi_cfg);
    if (op_ret != OPRT_OK) {
        TAL_PR_ERR("tkl_spi_init fail op_ret:%d", op_ret);
        return op_ret;
    }

    tx_buf_len = ONE_BYTE_LEN * COLOR_PRIMARY_NUM * pixel_num;
    op_ret = tdd_pixel_create_tx_ctrl(tx_buf_len, &pixels_send);
    if (op_ret != OPRT_OK) {
        return op_ret;
    }

    *handle = pixels_send;

    return OPRT_OK;
}

/**
 * @function: tdd_ws2812_driver_send_data
 * @brief: 将颜色数据（RGBCW）转换为当前芯片的线序并转换为SPI码流, 通过SPI发送
 * @param[in]: handle -> 设备句柄
 * @param[in]: *data_buf -> 颜色数据
 * @param[in]: buf_len -> 颜色数据长度
 * @return: success -> 0  fail -> else
 */
OPERATE_RET tdd_ws2812_driver_send_data(DRIVER_HANDLE_T handle, unsigned short *data_buf, unsigned int buf_len)
{
    OPERATE_RET ret = OPRT_OK;
    DRV_PIXEL_TX_CTRL_T *tx_ctrl = NULL;
    unsigned short swap_buf[COLOR_PRIMARY_NUM] = {0};
    unsigned int i = 0, j = 0, idx = 0;

    if (NULL == handle || NULL == data_buf || 0 == buf_len) {
        return OPRT_INVALID_PARM;
    }

    tx_ctrl = (DRV_PIXEL_TX_CTRL_T *)handle;

    for (j = 0; j < buf_len / COLOR_PRIMARY_NUM; j++) {
        memset(swap_buf, 0, sizeof(swap_buf));
        tdd_rgb_line_seq_transform(&data_buf[j * COLOR_PRIMARY_NUM], swap_buf, driver_info.line_seq);
        for (i = 0; i < COLOR_PRIMARY_NUM; i++) {
            tdd_rgb_transform_spi_data((unsigned char)swap_buf[i], DRVICE_DATA_0, DRVICE_DATA_1,
                                       &tx_ctrl->tx_buffer[idx]);
            idx += ONE_BYTE_LEN;
        }
    }

    ret = tkl_spi_send(driver_info.port, tx_ctrl->tx_buffer, tx_ctrl->tx_buffer_len);

    return ret;
}

/**
 * @function: tdd_ws2812_driver_close
 * @brief: 关闭设备（资源释放）
 * @param[in]: *handle -> 设备句柄
 * @return: success -> 0  fail -> else
 */
OPERATE_RET tdd_ws2812_driver_close(DRIVER_HANDLE_T *handle)
{
    OPERATE_RET ret = OPRT_OK;
    DRV_PIXEL_TX_CTRL_T *tx_ctrl = NULL;

    if ((NULL == handle) || (*handle == NULL)) {
        return OPRT_INVALID_PARM;
    }

    tx_ctrl = (DRV_PIXEL_TX_CTRL_T *)(*handle);

    ret = tkl_spi_deinit(driver_info.port);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("spi deinit err:%d", ret);
    }
    ret = tdd_pixel_tx_ctrl_release(tx_ctrl);
    *handle = NULL;

    return ret;
}

/**
 * @function:tdd_ws2812_driver_register
 * @brief: 注册设备
 * @param[in]: *driver_name -> 设备名
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_ws2812_driver_register(CHAR_T *driver_name, PIXEL_DRIVER_CONFIG_T *init_param)
{
    OPERATE_RET ret = OPRT_OK;
    PIXEL_DRIVER_INTFS_T intfs = {0};
    PIXEL_ATTR_T arrt = {0};

    intfs.open = tdd_2812_driver_open;
    intfs.output = tdd_ws2812_driver_send_data;
    intfs.close = tdd_ws2812_driver_close;

    arrt.color_tp = PIXEL_COLOR_TP_RGB;
    arrt.color_maximum = COLOR_RESOLUTION;

    ret = tdl_pixel_driver_register(driver_name, &intfs, &arrt, NULL);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("pixel drv init err:%d", ret);
        return ret;
    }
    memcpy(&driver_info, init_param, sizeof(PIXEL_DRIVER_CONFIG_T));
    return OPRT_OK;
}
#endif