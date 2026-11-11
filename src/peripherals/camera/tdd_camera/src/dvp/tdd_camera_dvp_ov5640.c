/**
 * @file tdd_camera_dvp_ov5640.c
 * @brief Portable OV5640 DVP sensor driver.
 *
 * DVP MCLK is configured by tkl_dvp_init() from sensor_clk. Sensor power,
 * reset and SCCB access use only TKL APIs so the driver remains portable.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) && \
    defined(ENABLE_DVP) && (ENABLE_DVP == 1)

#include "tal_api.h"
#include "tkl_gpio.h"
#include "tdd_camera_dvp_i2c.h"
#include "tdd_camera_ov5640.h"
#include "tdd_camera_ov5640_init_seq.h"

#define OV5640_ADDR7                    0x3CU
#define OV5640_ID                       0x5640U
#define OV5640_TEST_PATTERN             0U

static TUYA_GPIO_LEVEL_E __inactive_level(TUYA_GPIO_LEVEL_E active)
{
    return active == TUYA_GPIO_LEVEL_LOW ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW;
}

static void __ov5640_gpio_output(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T cfg = {
        .direct = TUYA_GPIO_OUTPUT,
        .level = level,
    };

    tkl_gpio_init(pin, &cfg);
    tkl_gpio_write(pin, level);
}

static OPERATE_RET __ov5640_write(DVP_I2C_CFG_T *i2c, uint16_t reg, uint8_t value)
{
    DVP_I2C_REG_CFG_T cfg = {
        .port = i2c->port,
        .addr = OV5640_ADDR7,
        .reg = reg,
        .is_16_reg = 1,
    };
    OPERATE_RET rt = OPRT_COM_ERROR;

    for (uint32_t attempt = 0U; attempt < 3U; ++attempt) {
        rt = tdd_dvp_i2c_write(&cfg, 1, &value);
        if (rt == OPRT_OK) {
            return OPRT_OK;
        }
        tal_system_sleep(2U);
    }
    return rt;
}

static OPERATE_RET __ov5640_read(DVP_I2C_CFG_T *i2c, uint16_t reg, uint8_t *value)
{
    DVP_I2C_REG_CFG_T cfg = {
        .port = i2c->port,
        .addr = OV5640_ADDR7,
        .reg = reg,
        .is_16_reg = 1,
    };
    OPERATE_RET rt = OPRT_COM_ERROR;

    for (uint32_t attempt = 0U; attempt < 3U; ++attempt) {
        rt = tdd_dvp_i2c_read(&cfg, 1, value);
        if (rt == OPRT_OK) {
            return OPRT_OK;
        }
        tal_system_sleep(2U);
    }
    return rt;
}

static OPERATE_RET __ov5640_write_table(DVP_I2C_CFG_T *i2c,
                                        const uint16_t table[][2], uint32_t count)
{
    for (uint32_t i = 0U; i < count; ++i) {
        OPERATE_RET rt = __ov5640_write(i2c, table[i][0], (uint8_t)table[i][1]);
        if (rt != OPRT_OK) {
            PR_ERR("ov5640 write failed reg=0x%04X value=0x%02X rt=%d",
                   table[i][0], (unsigned)table[i][1], rt);
            return rt;
        }
    }
    return OPRT_OK;
}

static OPERATE_RET __ov5640_reset(TUYA_CAMERA_IO_CTRL_T *pwr,
                                  TUYA_CAMERA_IO_CTRL_T *rst,
                                  void *arg)
{
    TUYA_GPIO_LEVEL_E rst_active;
    TUYA_GPIO_LEVEL_E rst_inactive;

    (void)arg;

    if (pwr == NULL || rst == NULL) {
        return OPRT_INVALID_PARM;
    }

    rst_active = rst->active_level;
    rst_inactive = __inactive_level(rst_active);
    PR_NOTICE("ov5640 reset begin pwr=%d/%d rst=%d/%d",
              pwr->pin, pwr->active_level, rst->pin, rst->active_level);

    if (pwr->pin < TUYA_GPIO_NUM_MAX) {
        __ov5640_gpio_output(pwr->pin, pwr->active_level);
    }
    if (rst->pin < TUYA_GPIO_NUM_MAX) {
        __ov5640_gpio_output(rst->pin, rst_active);
    }
    tal_system_sleep(100U);

    if (pwr->pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(pwr->pin, __inactive_level(pwr->active_level));
    }
    tal_system_sleep(10U);

    if (rst->pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(rst->pin, rst_inactive);
    }
    tal_system_sleep(100U);
    PR_NOTICE("ov5640 reset done");

    return OPRT_OK;
}

static OPERATE_RET __ov5640_init(DVP_I2C_CFG_T *i2c, void *arg)
{
    uint8_t high = 0U;
    uint8_t low = 0U;
    OPERATE_RET rt;

    (void)arg;

    if (i2c == NULL) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("ov5640 init begin i2c=%d", i2c->port);
    rt = __ov5640_read(i2c, 0x300AU, &high);
    if (rt == OPRT_OK) {
        rt = __ov5640_read(i2c, 0x300BU, &low);
    }
    if (rt != OPRT_OK || (((uint16_t)high << 8) | low) != OV5640_ID) {
        PR_ERR("ov5640 ID read failed rt=%d id=%02X%02X", rt, high, low);
        return OPRT_NOT_FOUND;
    }

    rt = __ov5640_write(i2c, 0x3103U, 0x11U);
    if (rt == OPRT_OK) {
        rt = __ov5640_write(i2c, 0x3008U, 0x82U);
    }
    if (rt != OPRT_OK) {
        return rt;
    }
    tal_system_sleep(10U);

    /* Base initialization only. Resolution, timing and output format are
     * selected by the TDD open sequence through set_ppi(). */
    rt = __ov5640_write_table(i2c, ov5640_init_reg_tbl,
                              CNTSOF(ov5640_init_reg_tbl));
    if (rt != OPRT_OK) {
        return rt;
    }
    tal_system_sleep(100U);
    PR_NOTICE("ov5640 base init done");
    return OPRT_OK;
}

static OPERATE_RET __ov5640_set_ppi(DVP_I2C_CFG_T *i2c, TUYA_CAMERA_PPI_E ppi,
                                    uint16_t fps, void *arg)
{
    OPERATE_RET rt;

    (void)arg;
    (void)fps;

    if (i2c == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (ppi != TUYA_CAMERA_PPI_320X240) {
        return OPRT_NOT_SUPPORTED;
    }

    rt = __ov5640_write_table(i2c, ov5640_320x240_yuv422_24m_tbl,
                              CNTSOF(ov5640_320x240_yuv422_24m_tbl));
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x471DU, 0x00U);
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x380EU, 0x03U);
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x380FU, 0x84U);
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x3A02U, 0x03U);
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x3A03U, 0x84U);
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x3A14U, 0x03U);
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x3A15U, 0x84U);
    if (rt == OPRT_OK) {
        rt = __ov5640_write(i2c, 0x503DU,
                            OV5640_TEST_PATTERN ? 0x80U : 0x00U);
    }
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x4741U, 0x00U);
    if (rt == OPRT_OK) rt = __ov5640_write(i2c, 0x3008U, 0x02U);
    if (rt != OPRT_OK) {
        return rt;
    }

    tal_system_sleep(20U);
    PR_NOTICE("ov5640 mode ready: 320x240 YUV422, test_pattern=%u",
              OV5640_TEST_PATTERN);
    return OPRT_OK;
}

OPERATE_RET tdd_camera_dvp_ov5640_register(char *name, TDD_DVP_SR_USR_CFG_T *cfg)
{
    TDD_DVP_SR_INTFS_T intfs = {
        .arg = NULL,
        .rst = __ov5640_reset,
        .init = __ov5640_init,
        .set_ppi = __ov5640_set_ppi,
    };
    TDD_DVP_SR_CFG_T sensor_cfg = {
        .max_fps = 30,
        .max_width = 320,
        .max_height = 240,
        .fmt = TUYA_FRAME_FMT_YUV422,
        .yuv_order = TUYA_YUV422_YUYV,
        .sync_polarity = TUYA_DVP_SYNC_MODE_0,
    };

    if (name == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }
    memcpy(&sensor_cfg.usr_cfg, cfg, sizeof(*cfg));
    return tdl_camera_dvp_device_register(name, &sensor_cfg, &intfs);
}

#endif /* ENABLE_CAMERA && ENABLE_DVP */
