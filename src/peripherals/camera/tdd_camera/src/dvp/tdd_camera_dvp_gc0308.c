/**
 * @file tdd_camera_dvp_gc0308.c
 * @brief GC0308 camera sensor driver implementation for DVP interface
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_gpio.h"

#include "tdd_camera_dvp_i2c.h"
#include "tdd_camera_gc0308_init_seq.h"
#include "tdd_camera_gc0308.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define UNACTIVE_LEVEL(x)       (((x) == TUYA_GPIO_LEVEL_LOW) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW)

#define GC0308_WRITE_ADDRESS    (0x42)
#define GC0308_READ_ADDRESS     (0x43)
#define GC0308_CHIP_ID          (0x9b)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_CAMERA_PPI_E ppi;
    uint8_t          *seq_tab;
    uint32_t          tab_size;
} CAMERA_PPI_INIT_TAB_T;

const CAMERA_PPI_INIT_TAB_T cPPI_INIT_SEQ_LIST[] = {
    {TUYA_CAMERA_PPI_320X240, (uint8_t *)cGC0308_QVGA_320_240_TAB, CNTSOF(cGC0308_QVGA_320_240_TAB)},
    {TUYA_CAMERA_PPI_640X480, (uint8_t *)cGC0308_VGA_640_480_TAB, CNTSOF(cGC0308_VGA_640_480_TAB)},
};

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_DVP_SR_CFG_T sg_dvp_sensor = {
    .max_fps   = 30,
    .max_width = 640,
    .max_height = 480,
    .fmt = TUYA_FRAME_FMT_YUV422,
};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __dvp_gc0308_update_reg(TUYA_I2C_NUM_E port, const uint8_t sensor_reg_table[][2], uint32_t size)
{
    OPERATE_RET rt = OPRT_OK;
    DVP_I2C_REG_CFG_T i2c_reg_cfg;
    uint16_t val_len = sizeof(sensor_reg_table[0][0]);

    i2c_reg_cfg.port = port;
    i2c_reg_cfg.addr = (GC0308_WRITE_ADDRESS >> 1);
    i2c_reg_cfg.is_16_reg = 0;

    for (uint16_t idx = 0; idx < size; idx++) {
        i2c_reg_cfg.reg = sensor_reg_table[idx][0];
        rt = tdd_dvp_i2c_write(&i2c_reg_cfg, val_len, (uint8_t *)&sensor_reg_table[idx][1]);
        if (rt != OPRT_OK) {
            PR_ERR("gc0308 i2c write err: port=%d addr=0x%02x reg=0x%02x rt=%d idx=%d/%d",
                   port, i2c_reg_cfg.addr, i2c_reg_cfg.reg, rt, idx, size);
            return;
        }
    }
}

static OPERATE_RET __dvp_gc0308_reset(TUYA_CAMERA_IO_CTRL_T *rst_pin, void *arg)
{
    if (NULL == rst_pin) {
        return OPRT_OK;
    }

    (void)arg;

    if (rst_pin->pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {.direct = TUYA_GPIO_OUTPUT};

        gpio_cfg.level = UNACTIVE_LEVEL(rst_pin->active_level);
        tkl_gpio_init(rst_pin->pin, &gpio_cfg);

        tkl_gpio_write(rst_pin->pin, UNACTIVE_LEVEL(rst_pin->active_level));
        tal_system_sleep(20);
        tkl_gpio_write(rst_pin->pin, rst_pin->active_level);
        tal_system_sleep(100);
        tkl_gpio_write(rst_pin->pin, UNACTIVE_LEVEL(rst_pin->active_level));
        tal_system_sleep(100);
        PR_NOTICE("gc0308 reset pin %d lv:%d", rst_pin->pin, rst_pin->active_level);
    }

    return OPRT_OK;
}

static OPERATE_RET __dvp_gc0308_init(DVP_I2C_CFG_T *i2c, void *arg)
{
    if (NULL == i2c) {
        return OPRT_INVALID_PARM;
    }

    (void)arg;

    /* Verify chip ID */
    uint8_t chip_id = 0;
    DVP_I2C_REG_CFG_T id_cfg = {
        .port = i2c->port,
        .addr = (GC0308_WRITE_ADDRESS >> 1),
        .is_16_reg = 0,
        .reg = 0x00,
    };

    OPERATE_RET rt = tdd_dvp_i2c_read(&id_cfg, 1, &chip_id);
    PR_NOTICE("gc0308 chip id: 0x%02x (expected 0x%02x), rt=%d", chip_id, GC0308_CHIP_ID, rt);

    if (chip_id != GC0308_CHIP_ID) {
        PR_ERR("gc0308 chip id mismatch!");
        return OPRT_COM_ERROR;
    }

    __dvp_gc0308_update_reg(i2c->port, cGC0308_INIT_TAB, CNTSOF(cGC0308_INIT_TAB));

    PR_NOTICE("__dvp_gc0308_init done");

    return OPRT_OK;
}

static OPERATE_RET __dvp_gc0308_set_ppi(DVP_I2C_CFG_T *i2c, TUYA_CAMERA_PPI_E ppi, uint16_t fps, void *arg)
{
    uint32_t i = 0;
    CAMERA_PPI_INIT_TAB_T *ppi_init_tab = NULL;

    if (NULL == i2c) {
        return OPRT_INVALID_PARM;
    }

    (void)arg;

    for (i = 0; i < CNTSOF(cPPI_INIT_SEQ_LIST); i++) {
        if (ppi == cPPI_INIT_SEQ_LIST[i].ppi) {
            ppi_init_tab = (CAMERA_PPI_INIT_TAB_T *)&cPPI_INIT_SEQ_LIST[i];
            break;
        }
    }

    if (ppi_init_tab) {
        __dvp_gc0308_update_reg(i2c->port, (const uint8_t (*)[2])ppi_init_tab->seq_tab, ppi_init_tab->tab_size);
    } else {
        /* GC0308 max is VGA, fall back to VGA for unsupported PPI */
        PR_WARN("gc0308 unsupported ppi 0x%x, fallback to VGA", ppi);
        __dvp_gc0308_update_reg(i2c->port, cGC0308_VGA_640_480_TAB, CNTSOF(cGC0308_VGA_640_480_TAB));
    }

    PR_NOTICE("__dvp_gc0308_set_ppi ppi:0x%x, fps:%d", ppi, fps);

    return OPRT_OK;
}

OPERATE_RET tdd_camera_dvp_gc0308_register(char *name, TDD_DVP_SR_USR_CFG_T *cfg)
{
    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sg_dvp_sensor.usr_cfg, cfg, sizeof(TDD_DVP_SR_USR_CFG_T));

    TDD_DVP_SR_INTFS_T sr_intfs = {
        .arg     = NULL,
        .rst     = __dvp_gc0308_reset,
        .init    = __dvp_gc0308_init,
        .set_ppi = __dvp_gc0308_set_ppi,
    };

    return tdl_camera_dvp_device_register(name, &sg_dvp_sensor, &sr_intfs);
}
