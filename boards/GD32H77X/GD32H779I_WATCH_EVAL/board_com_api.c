/**
 * @file board_com_api.c
 * @brief Board hardware registration for GD32H779I Watch EVAL.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "board_com_api.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "tdd_disp_st77916.h"

#define BOARD_LCD_WIDTH                 360U
#define BOARD_LCD_HEIGHT                360U
#define BOARD_LCD_QSPI_PORT             TUYA_QSPI_NUM_0
#define BOARD_LCD_QSPI_CLK_HZ           37500000U
#define BOARD_LCD_CS_PIN                TUYA_GPIO_NUM_36 /* PC4 */
#define BOARD_LCD_RST_PIN               TUYA_GPIO_NUM_94 /* PF14 */
#define BOARD_LCD_BL_PIN                TUYA_GPIO_NUM_118 /* PH6 */

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) && \
    defined(ENABLE_DVP) && (ENABLE_DVP == 1)
#include "tdd_camera_ov5640.h"

#define BOARD_CAMERA_CN3_I2C_PORT   TUYA_I2C_NUM_2
#define BOARD_CAMERA_CN4_I2C_PORT   TUYA_I2C_NUM_3
#define BOARD_CAMERA_CN3_RST_PIN    TUYA_GPIO_NUM_98 /* PG2 */
#define BOARD_CAMERA_CN4_RST_PIN    TUYA_GPIO_NUM_10 /* PA10 */
#define BOARD_CAMERA_PWR_PIN        TUYA_GPIO_NUM_101 /* PG5 */

#endif

#if defined(ENABLE_TP) && (ENABLE_TP == 1)
#include "tdd_tp_cst816x.h"

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_2
#define BOARD_TP_I2C_SCL_PIN         TUYA_IO_PIN_MAX
#define BOARD_TP_I2C_SDA_PIN         TUYA_IO_PIN_MAX
#define BOARD_TP_RST_PIN             TUYA_GPIO_NUM_115 /* PH3 */
#define BOARD_TP_INTR_PIN            TUYA_GPIO_NUM_MAX
#define BOARD_TP_WIDTH               360U
#define BOARD_TP_HEIGHT              360U
#endif

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1) && defined(DISPLAY_NAME)
    DISP_QSPI_DEVICE_CFG_T display_cfg = {
        .width = BOARD_LCD_WIDTH,
        .height = BOARD_LCD_HEIGHT,
        .x_offset = 0U,
        .y_offset = 0U,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation = TUYA_DISPLAY_ROTATION_0,
        .rst_pin = BOARD_LCD_RST_PIN,
        .cs_pin = BOARD_LCD_CS_PIN,
        .port = BOARD_LCD_QSPI_PORT,
        .spi_clk = BOARD_LCD_QSPI_CLK_HZ,
        .mode = TUYA_QSPI_MODE0,
        .data_lines = TUYA_QSPI_1WIRE,
        .te_pin = TUYA_GPIO_NUM_MAX,
        .te_active_level = TUYA_GPIO_LEVEL_HIGH,
        .bl = {
            .type = TUYA_DISP_BL_TP_GPIO,
            .gpio = {
                .pin = BOARD_LCD_BL_PIN,
                .active_level = TUYA_GPIO_LEVEL_HIGH,
            },
        },
        .power = {
            .pin = TUYA_GPIO_NUM_MAX,
            .active_level = TUYA_GPIO_LEVEL_HIGH,
        },
        .use_dma = true,
        .te_sync_enable = false,
        .te_wait_div = 1U,
    };

    rt = tdd_disp_qspi_st77916_register(DISPLAY_NAME, &display_cfg);
#endif

    return rt;
}

static OPERATE_RET __board_register_tp(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_TP) && (ENABLE_TP == 1) && defined(DISPLAY_NAME)
    TDD_TP_CST816X_INFO_T tp_cfg = {
        .rst_pin = BOARD_TP_RST_PIN,
        .intr_pin = BOARD_TP_INTR_PIN,
        .i2c_cfg = {
            .port = BOARD_TP_I2C_PORT,
            .scl_pin = BOARD_TP_I2C_SCL_PIN,
            .sda_pin = BOARD_TP_I2C_SDA_PIN,
        },
        .tp_cfg = {
            .x_max = BOARD_TP_WIDTH,
            .y_max = BOARD_TP_HEIGHT,
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        },
    };

    rt = tdd_tp_i2c_cst816x_register(DISPLAY_NAME, &tp_cfg);
#endif

    return rt;
}

static OPERATE_RET __board_register_camera_one(char *name, TUYA_I2C_NUM_E i2c_port,
                                               TUYA_GPIO_NUM_E rst_pin)
{
    TDD_DVP_SR_USR_CFG_T camera_cfg = {
        .pwr = {
            .pin = BOARD_CAMERA_PWR_PIN,
            .active_level = TUYA_GPIO_LEVEL_HIGH,
        },
        .rst = {
            .pin = rst_pin,
            .active_level = TUYA_GPIO_LEVEL_LOW,
        },
        .i2c = {
            .port = i2c_port,
            .clk = TUYA_IO_PIN_MAX,
            .sda = TUYA_IO_PIN_MAX,
        },
        .clk = 25000000U,
    };

    PR_NOTICE("register camera %s", name);
    return tdd_camera_dvp_ov5640_register(name, &camera_cfg);
}

static OPERATE_RET __board_register_camera(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) && \
    defined(ENABLE_DVP) && (ENABLE_DVP == 1)
    TUYA_GPIO_BASE_CFG_T reset_held = {
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_LOW,
    };

    /* CN3/CN4 share DVP and PWDN. Keep both sensors in reset and release only
     * the device selected by its reset callback when it is opened. */
    tkl_gpio_init(BOARD_CAMERA_CN3_RST_PIN, &reset_held);
    tkl_gpio_init(BOARD_CAMERA_CN4_RST_PIN, &reset_held);

    rt = __board_register_camera_one(CAMERA_NAME, BOARD_CAMERA_CN3_I2C_PORT,
                                     BOARD_CAMERA_CN3_RST_PIN);
    if (rt != OPRT_OK) {
        return rt;
    }

#if defined(ENABLE_CAMERA_2) && (ENABLE_CAMERA_2 == 1)
    rt = __board_register_camera_one(CAMERA_NAME_2, BOARD_CAMERA_CN4_I2C_PORT,
                                     BOARD_CAMERA_CN4_RST_PIN);
#endif
#endif

    return rt;
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    rt = __board_register_display();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __board_register_tp();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __board_register_camera();
    if (rt != OPRT_OK) {
        return rt;
    }

    return rt;
}
