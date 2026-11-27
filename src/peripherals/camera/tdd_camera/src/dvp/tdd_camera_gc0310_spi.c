/**
 * @file tdd_camera_gc0310_spi.c
 * @brief GC0310 camera TDD for the JIELI SPI (J4) path
 *
 * The vendor stack configures GC0310 from its video1 sensor driver. This
 * driver does the same register work through the TDD I2C helper so the SPI
 * transport can run without opening video1 at all.
 *
 * Sensor side only; the transport lives in the shared TDD core
 * (tdd_camera_dvp.c).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_system.h"

#include "tdd_camera_dvp_i2c.h"
#include "tdd_camera_gc0310_init_seq.h"
#include "tdd_camera_gc0310_spi.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* SCCB slave address 0x42 in 8-bit form; tkl_i2c takes the 7-bit address. */
#define GC0310_I2C_ADDR    (0x42 >> 1)
#define GC0310_PAGE_REG    0xfe
#define GC0310_PAGE_ID     0xf0
#define GC0310_CHIP_ID     0xa310
#define GC0310_UNACTIVE(x) (((x) == TUYA_GPIO_LEVEL_LOW) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW)

/***********************************************************
***********************variable define**********************
***********************************************************/
static TUYA_I2C_NUM_E        sg_i2c_port = TUYA_I2C_NUM_MAX;
static TUYA_CAMERA_IO_CTRL_T sg_rst_pin  = {.pin = TUYA_GPIO_NUM_MAX, .active_level = TUYA_GPIO_LEVEL_LOW};

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __gc0310_write(uint8_t reg, uint8_t val)
{
    DVP_I2C_REG_CFG_T cfg = {
        .port      = sg_i2c_port,
        .addr      = GC0310_I2C_ADDR,
        .reg       = reg,
        .is_16_reg = 0,
    };

    return tdd_dvp_i2c_write(&cfg, 1, &val);
}

static OPERATE_RET __gc0310_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    DVP_I2C_REG_CFG_T cfg = {
        .port      = sg_i2c_port,
        .addr      = GC0310_I2C_ADDR,
        .reg       = reg,
        .is_16_reg = 0,
    };

    return tdd_dvp_i2c_read(&cfg, len, buf);
}

static OPERATE_RET __gc0310_write_table(void)
{
    for (uint32_t i = 0; i < GC0310_INIT_SEQ_LEN; i++) {
        OPERATE_RET rt = __gc0310_write(gc0310_init_seq[i].reg, gc0310_init_seq[i].val);

        if (rt != OPRT_OK) {
            PR_ERR("gc0310 i2c write failed: reg=0x%02x val=0x%02x rt=%d idx=%u/%u", gc0310_init_seq[i].reg,
                   gc0310_init_seq[i].val, rt, (unsigned)i, (unsigned)GC0310_INIT_SEQ_LEN);
            return rt;
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __gc0310_check_id(void)
{
    uint8_t  id[2] = {0};
    uint16_t value;

    /* This module powers up on page 0, where 0xf0/0xf1 read 0x00. */
    __gc0310_write(GC0310_PAGE_REG, GC0310_PAGE_ID);
    tal_system_sleep(2);

    if (__gc0310_read(GC0310_PAGE_ID, id, 2) != OPRT_OK) {
        PR_ERR("gc0310 id read failed");
        return OPRT_COM_ERROR;
    }

    value = (uint16_t)((id[0] << 8) | id[1]);
    PR_NOTICE("gc0310 chip id: 0x%04x (expected 0x%04x)", value, GC0310_CHIP_ID);

    return (value == GC0310_CHIP_ID) ? OPRT_OK : OPRT_COM_ERROR;
}

static void __gc0310_pulse_reset(void)
{
    if (sg_rst_pin.pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    tkl_gpio_write(sg_rst_pin.pin, sg_rst_pin.active_level);
    tal_system_sleep(10);
    tkl_gpio_write(sg_rst_pin.pin, GC0310_UNACTIVE(sg_rst_pin.active_level));
    /* The sensor NACKs its address for a few ms after reset is released. */
    tal_system_sleep(5);
}

static OPERATE_RET __dvp_gc0310_spi_reset(TUYA_CAMERA_IO_CTRL_T *rst_pin, void *arg)
{
    (void)arg;

    if (NULL == rst_pin) {
        return OPRT_OK;
    }

    sg_rst_pin = *rst_pin;

    if (rst_pin->pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {
            .direct = TUYA_GPIO_OUTPUT,
            .level  = GC0310_UNACTIVE(rst_pin->active_level),
        };

        tkl_gpio_init(rst_pin->pin, &gpio_cfg);
        tkl_gpio_write(rst_pin->pin, GC0310_UNACTIVE(rst_pin->active_level));
        tal_system_sleep(5);
        __gc0310_pulse_reset();
        PR_NOTICE("gc0310 reset pin %d lv:%d", rst_pin->pin, rst_pin->active_level);
    }

    return OPRT_OK;
}

static OPERATE_RET __dvp_gc0310_spi_init(DVP_I2C_CFG_T *i2c, void *arg)
{
    (void)arg;

    if (NULL == i2c) {
        return OPRT_INVALID_PARM;
    }

    sg_i2c_port = i2c->port;

    /* The board's reset callback already released the sensor; check the ID
     * before the long table so a wrong module fails quickly. */
    if (__gc0310_check_id() != OPRT_OK) {
        return OPRT_COM_ERROR;
    }

    OPERATE_RET rt = __gc0310_write_table();
    if (rt != OPRT_OK) {
        return rt;
    }

    PR_NOTICE("gc0310 spi init done");

    return OPRT_OK;
}

static OPERATE_RET __dvp_gc0310_spi_set_ppi(DVP_I2C_CFG_T *i2c, TUYA_CAMERA_PPI_E ppi, uint16_t fps, void *arg)
{
    (void)i2c;
    (void)arg;

    /* The SPI stream is fixed at the sensor's native VGA raster; the preview
     * size is decimated by tkl_dvp. A different PPI here cannot change the
     * 1288-byte row stride (measured on AC791). */
    PR_NOTICE("gc0310 spi fixed VGA (requested ppi 0x%x fps %u)", ppi, fps);

    return OPRT_OK;
}

OPERATE_RET tdd_camera_gc0310_spi_register(char *name, TDD_DVP_SR_USR_CFG_T *cfg)
{
    TDD_DVP_SR_CFG_T sensor = {
        .max_fps    = 15,
        .max_width  = 640,
        .max_height = 480,
        .fmt        = TUYA_FRAME_FMT_YUV422,
    };

    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sensor.usr_cfg, cfg, sizeof(TDD_DVP_SR_USR_CFG_T));

    TDD_DVP_SR_INTFS_T intfs = {
        .arg     = NULL,
        .rst     = __dvp_gc0310_spi_reset,
        .init    = __dvp_gc0310_spi_init,
        .set_ppi = __dvp_gc0310_spi_set_ppi,
    };

    return tdl_camera_dvp_device_register(name, &sensor, &intfs);
}
