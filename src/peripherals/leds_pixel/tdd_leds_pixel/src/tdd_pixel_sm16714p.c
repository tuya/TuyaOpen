/**
 * @file tdd_pixel_sm16714p.c
 * @author www.tuya.com
 * @brief tdd_pixel_sm16714p module is used to driving sm16714p chip
 * @version 0.1
 * @date 2022-03-08
 *
 * @copyright Copyright (c) tuya.inc 2022
 *
 */
#include <string.h>
#include "tuya_iot_config.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI)
#include "tal_log.h"
#include "tkl_spi.h"

#include "tdl_pixel_driver.h"
#include "tdd_pixel_basic.h"
#include "tdd_pixel_sm16714p.h"

/*********************************************************************
******************************macro define****************************
*********************************************************************/
#define ONE_COLOR_GAIN_LEN 5

#define ELE_GAIN_LEVEL_MAX 31
#define ELE_GAIN_RED       15
#define ELE_GAIN_GREEN     15
#define ELE_GAIN_BLUE      15
#define ELE_GAIN_WARM      15

#define DRV_SPI_SPEED 8000000 /* SPI波特率 */

#define DRVICE_DATA_0 0XC0 /* SPI 0、1码对应的数据 */
#define DRVICE_DATA_1 0XFE

#define COLOR_PRIMARY_NUM 4 // 4路
#define COLOR_RESOLUTION  255
/*********************************************************************
****************************typedef define****************************
*********************************************************************/

/*********************************************************************
****************************variable define***************************
*********************************************************************/
static char g_gain_arr[COLOR_PRIMARY_NUM] = {
    ELE_GAIN_RED,
    ELE_GAIN_GREEN,
    ELE_GAIN_BLUE,
    ELE_GAIN_WARM,
};
static PIXEL_DRIVER_CONFIG_T driver_info;
/*********************************************************************
****************************function define***************************
*********************************************************************/

static VOID __tdd_sm16714p_ele_gain_transform(unsigned char *spi_data)
{
    unsigned char i = 0, j = 0, gain_data = 0;

    for (i = 0; i < COLOR_PRIMARY_NUM; i++) {
        gain_data = g_gain_arr[i];

        for (j = 0; j < ONE_COLOR_GAIN_LEN; j++) { // sm16714p 一个通道的颜色增益用5bit表示
            spi_data[0] = (gain_data & 0x10) ? DRVICE_DATA_1 : DRVICE_DATA_0;
            gain_data <<= 1;
            spi_data++;
        }
    }

    return;
}

/**
 * @function:tdd_sm16714p_driver_open
 * @brief: 打开（初始化）设备
 * @param[in]: inform_cb -> spi码流发送完成回调
 * @param[in]: pixel_num -> 像素点数
 * @param[out]: *handle  -> 设备句柄
 * @return: success -> 0  fail -> else
 */
OPERATE_RET tdd_sm16714p_driver_open(DRIVER_HANDLE_T *handle, unsigned short pixel_num)
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
    // 32bytes固定电流增益字节 + 实际像素点数所占字节
    tx_buf_len = ONE_BYTE_LEN * (COLOR_PRIMARY_NUM * pixel_num + COLOR_PRIMARY_NUM * ONE_COLOR_GAIN_LEN);
    op_ret = tdd_pixel_create_tx_ctrl(tx_buf_len, &pixels_send);
    if (op_ret != OPRT_OK) {
        return op_ret;
    }

    *handle = pixels_send;

    return OPRT_OK;
}

/**
 * @function: tdd_sm16714p_driver_send_data
 * @brief: 将颜色数据（RGBCW）转换为当前芯片的线序并转换为SPI码流, 通过SPI发送
 * @param[in]: handle -> 设备句柄
 * @param[in]: *data_buf -> 颜色数据
 * @param[in]: buf_len -> 颜色数据长度
 * @return: success -> 0  fail -> else
 */
OPERATE_RET tdd_sm16714p_driver_send_data(DRIVER_HANDLE_T handle, unsigned short *data_buf,
                                          unsigned int buf_len)
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
    //添加增益
    __tdd_sm16714p_ele_gain_transform(&tx_ctrl->tx_buffer[idx]);

    ret = tkl_spi_send(driver_info.port, tx_ctrl->tx_buffer, tx_ctrl->tx_buffer_len);
    return ret;
}
/**
 * @function: tdd_sm16714p_driver_close
 * @brief: 关闭设备（资源释放）
 * @param[in]: *handle -> 设备句柄
 * @return: success -> 0  fail -> else
 */
OPERATE_RET tdd_sm16714p_driver_close(DRIVER_HANDLE_T *handle)
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
 * @function:tdd_sm16714p_driver_register
 * @brief: 注册设备
 * @param[in]: *driver_name -> 设备名
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_sm16714p_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param)
{
    OPERATE_RET ret = OPRT_OK;
    PIXEL_DRIVER_INTFS_T intfs;
    PIXEL_ATTR_T arrt = {0};

    intfs.open = tdd_sm16714p_driver_open;
    intfs.output = tdd_sm16714p_driver_send_data;
    intfs.close = tdd_sm16714p_driver_close;

    arrt.color_tp = PIXEL_COLOR_TP_RGBW;
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