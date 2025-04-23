/**
 * @file tdd_pixel_ws2812_opt.c
 * @author www.tuya.com
 * @brief tdd_pixel_ws2812 module is used to
 * @version 0.1
 * @date 2022-08-03
 *
 * @copyright Copyright (c) tuya.inc 2022
 *
 */

#include <string.h>
#include "tuya_iot_config.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI)
#include "tal_log.h"
#include "tal_memory.h"
#include "tdd_pixel_type.h"
#include "tkl_spi.h"

#include "tdl_pixel_driver.h"
#include "tdd_pixel_basic.h"
#include "tdd_pixel_ws2812_opt.h"
#include "tdd_pixel_pwm.h"
/*********************************************************************
******************************macro define****************************
*********************************************************************/
/* SPI波特率 */
#define DRV_SPI_SPEED 2887500

#define COLOR_PRIMARY_NUM 3
#define COLOR_RESOLUTION  10000

// V2.0归零码4bit版
#define LED_DRVICE_IC_DATA_00 0X88 // 00
#define LED_DRVICE_IC_DATA_01 0X8e // 01
#define LED_DRVICE_IC_DATA_10 0Xe8 // 10
#define LED_DRVICE_IC_DATA_11 0Xee // 11

#define ONE_BYTE_LEN_4BIT 4
/*********************************************************************
****************************typedef define****************************
*********************************************************************/

/*********************************************************************
****************************variable define***************************
*********************************************************************/
static PIXEL_DRIVER_CONFIG_T driver_info;
static PIXEL_PWM_CFG_T *g_pwm_cfg = NULL;
/*********************************************************************
****************************function define***************************
*********************************************************************/
/**
 * @brief       rgb转成spi数据 4bit为单位
 *
 * @param[in]   color_data          颜色数据
 * @param[in]   chip_ic_0           0码
 * @param[in]   chip_ic_1           1码
 * @param[out]  spi_data_buf        转化后的spi数据
 *
 * @return none
 */
STATIC void __tdd_2812_4bit_rgb_transform_spi_data(unsigned char color_data, unsigned char *spi_data_buf)
{
    unsigned char i = 0;

    for (i = 0; i < ONE_BYTE_LEN_4BIT; i++) {
        if ((color_data & 0xc0) == 0) {
            spi_data_buf[i] = LED_DRVICE_IC_DATA_00;

        } else if ((color_data & 0xc0) == 0x40) {
            spi_data_buf[i] = LED_DRVICE_IC_DATA_01;

        } else if ((color_data & 0xc0) == 0x80) {
            spi_data_buf[i] = LED_DRVICE_IC_DATA_10;

        } else if ((color_data & 0xc0) == 0xc0) {
            spi_data_buf[i] = LED_DRVICE_IC_DATA_11;

        } else {
            TAL_PR_ERR("SPI Send 1/0 Bit error\r\n");
        }

        color_data = color_data << 2;
    }

    return;
}

/**
 * @function:__tdd_2812_driver_open
 * @brief: 打开（初始化）设备
 * @param[in]: pixel_num -> 像素点数
 * @param[out]: *handle  -> 设备句柄
 * @return: success -> 0  fail -> else
 */
STATIC OPERATE_RET __tdd_2812_driver_open(DRIVER_HANDLE_T *handle, unsigned short pixel_num)
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

    tx_buf_len = ONE_BYTE_LEN_4BIT * COLOR_PRIMARY_NUM * pixel_num;
    op_ret = tdd_pixel_create_tx_ctrl(tx_buf_len, &pixels_send);
    if (op_ret != OPRT_OK) {
        return op_ret;
    }

    if (NULL != g_pwm_cfg) {
      op_ret = tdd_pixel_pwm_open(g_pwm_cfg);
      if (op_ret != OPRT_OK) {
        return op_ret;
      }
    }

    *handle = pixels_send;

    return OPRT_OK;
}

/**
 * @function: __tdd_ws2812_driver_send_data
 * @brief: 将颜色数据（RGBCW）转换为当前芯片的线序并转换为SPI码流, 通过SPI发送
 * @param[in]: handle -> 设备句柄
 * @param[in]: *data_buf -> 颜色数据
 * @param[in]: buf_len -> 颜色数据长度
 * @return: success -> 0  fail -> else
 */
STATIC OPERATE_RET __tdd_ws2812_driver_send_data(DRIVER_HANDLE_T handle, unsigned short *data_buf,
                                                 unsigned int buf_len)
{
    OPERATE_RET ret = OPRT_OK;
    DRV_PIXEL_TX_CTRL_T *tx_ctrl = NULL;
    unsigned short swap_buf[COLOR_PRIMARY_NUM] = {0};
    unsigned int i = 0, j = 0, idx = 0;
    unsigned char color_nums = COLOR_PRIMARY_NUM;

    if (NULL == handle || NULL == data_buf || 0 == buf_len) {
        return OPRT_INVALID_PARM;
    }

    if (NULL != g_pwm_cfg) {
        if (g_pwm_cfg->pwm_ch_arr[PIXEL_PWM_CH_IDX_COLD] != PIXEL_PWM_ID_INVALID) {
            color_nums++;
        }
        if (g_pwm_cfg->pwm_ch_arr[PIXEL_PWM_CH_IDX_WARM] != PIXEL_PWM_ID_INVALID) {
            color_nums++;
        }
        LIGHT_RGBCW_U color = {.array = {0,0,0,0,0}};
        color.s.cold = data_buf[3];
        color.s.warm = data_buf[4];
        tdd_pixel_pwm_output(g_pwm_cfg ,&color);
    }

    tx_ctrl = (DRV_PIXEL_TX_CTRL_T *)handle;
    for (j = 0; j < buf_len / color_nums; j++) {
        memset(swap_buf, 0, sizeof(swap_buf));
        tdd_rgb_line_seq_transform(&data_buf[j * color_nums], swap_buf, driver_info.line_seq);
        for (i = 0; i < COLOR_PRIMARY_NUM; i++) {
            __tdd_2812_4bit_rgb_transform_spi_data((unsigned char)(swap_buf[i] * 255 / COLOR_RESOLUTION), &tx_ctrl->tx_buffer[idx]);
            idx += ONE_BYTE_LEN_4BIT;
        }
    }

    ret = tkl_spi_send(driver_info.port, tx_ctrl->tx_buffer, tx_ctrl->tx_buffer_len);

    return ret;
}

/**
 * @function: __tdd_ws2812_driver_close
 * @brief: 关闭设备（资源释放）
 * @param[in]: *handle -> 设备句柄
 * @return: success -> 0  fail -> else
 */
STATIC OPERATE_RET __tdd_ws2812_driver_close(DRIVER_HANDLE_T *handle)
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

    // ret = tdd_pixel_pwm_close(g_pwm_cfg);
    *handle = NULL;

    return ret;
}

OPERATE_RET __tdd_ws2812_driver_config(DRIVER_HANDLE_T handle, unsigned char cmd, void *arg)
{
    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }
    
    switch(cmd) {
        case DRV_CMD_GET_PWM_HARDWARE_CFG: {
            if (NULL == arg) {
                return OPRT_INVALID_PARM;
            }
            if (NULL == g_pwm_cfg) {
                return OPRT_NOT_SUPPORTED;
            }
            PIXEL_PWM_CFG_T *pwm_cfg = (PIXEL_PWM_CFG_T *)arg;
            pwm_cfg->active_level = g_pwm_cfg->active_level;
            pwm_cfg->pwm_freq = g_pwm_cfg->pwm_freq;
            memcpy((UCHAR_T *)pwm_cfg->pwm_ch_arr, (UCHAR_T *)g_pwm_cfg->pwm_ch_arr, SIZEOF(g_pwm_cfg->pwm_ch_arr));
            memcpy((UCHAR_T *)pwm_cfg->pwm_pin_arr, (UCHAR_T *)g_pwm_cfg->pwm_pin_arr, SIZEOF(g_pwm_cfg->pwm_pin_arr));
            break;
        }
        case DRV_CMD_SET_RGB_ORDER_CFG: {
            if (NULL == arg) {
                return OPRT_INVALID_PARM;
            }
            RGB_ORDER_MODE_E *new_rgb_order = (RGB_ORDER_MODE_E *)arg;
            driver_info.line_seq = *new_rgb_order;
            break;
        }
        default:
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}

/**
 * @function:tdd_ws2812_driver_register
 * @brief: 注册设备
 * @param[in]: *driver_name -> 设备名
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_ws2812_opt_driver_register(CHAR_T *driver_name, PIXEL_DRIVER_CONFIG_T *init_param, PIXEL_PWM_CFG_T *pwm_cfg)
{
    OPERATE_RET ret = OPRT_OK;
    PIXEL_DRIVER_INTFS_T intfs = {0};
    PIXEL_ATTR_T arrt = {0};

    intfs.open = __tdd_2812_driver_open;
    intfs.output = __tdd_ws2812_driver_send_data;
    intfs.close = __tdd_ws2812_driver_close;
    intfs.config = __tdd_ws2812_driver_config;

    arrt.color_tp = PIXEL_COLOR_TP_RGB;
    arrt.color_maximum = COLOR_RESOLUTION;
    arrt.white_color_control = FALSE;

    if (NULL != pwm_cfg) {
        g_pwm_cfg = (PIXEL_PWM_CFG_T *) tal_malloc(SIZEOF(PIXEL_PWM_CFG_T));
        g_pwm_cfg->active_level = pwm_cfg->active_level;
        g_pwm_cfg->pwm_freq = pwm_cfg->pwm_freq;
        memcpy((UCHAR_T *)g_pwm_cfg->pwm_ch_arr, (UCHAR_T *)pwm_cfg->pwm_ch_arr, SIZEOF(pwm_cfg->pwm_ch_arr));
        memcpy((UCHAR_T *)g_pwm_cfg->pwm_pin_arr, (UCHAR_T *)pwm_cfg->pwm_pin_arr, SIZEOF(pwm_cfg->pwm_pin_arr));
        if (g_pwm_cfg->pwm_ch_arr[PIXEL_PWM_CH_IDX_COLD] != PIXEL_PWM_ID_INVALID) {
            arrt.color_tp |= COLOR_C_BIT;
        }
        if (g_pwm_cfg->pwm_ch_arr[PIXEL_PWM_CH_IDX_WARM] != PIXEL_PWM_ID_INVALID) {
            arrt.color_tp |= COLOR_W_BIT;
        }
        arrt.white_color_control = TRUE;
    } 

    ret = tdl_pixel_driver_register(driver_name, &intfs, &arrt, NULL);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("pixel drv init err:%d", ret);
        return ret;
    }
    memcpy(&driver_info, init_param, sizeof(PIXEL_DRIVER_CONFIG_T));
    return OPRT_OK;
}
#endif