/**
 * @file tdd_camera_dvp_ov2640.c
 * @brief OV2640 camera sensor driver implementation
 *
 * This file implements the OV2640 camera sensor driver for DVP interface,
 * including sensor initialization, reset control, and PPI (pixels per inch)
 * configuration. It provides register update functions and sensor-specific
 * initialization sequences.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_gpio.h"

#include "tdd_camera_ov2640_init_seq.h"
#include "tdd_camera_ov2640.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define UNACTIVE_LEVEL(x) 	    (((x) == TUYA_GPIO_LEVEL_LOW)? TUYA_GPIO_LEVEL_HIGH: TUYA_GPIO_LEVEL_LOW)

#define OV2640_WRITE_ADDRESS (0x60)
#define OV2640_READ_ADDRESS  (0x61)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_CAMERA_PPI_E ppi;
    uint8_t          *seq_tab;
    uint32_t          tab_size;
}CAMERA_PPI_INIT_TAB_T;

typedef struct {
    TUYA_CAMERA_PPI_E ppi;
    uint16_t          fps;
    uint8_t          *seq_tab;
    uint32_t          tab_size;
}CAMERA_FPS_INIT_TAB_T;

const CAMERA_PPI_INIT_TAB_T cPPI_INIT_SEQ_LIST[] = {
    {TUYA_CAMERA_PPI_240X240,  (uint8_t *)cOV2640_240_240_TAB,  CNTSOF(cOV2640_240_240_TAB)},
    {TUYA_CAMERA_PPI_480X480,  (uint8_t *)cOV2640_480_480_TAB,  CNTSOF(cOV2640_480_480_TAB)},
    {TUYA_CAMERA_PPI_1280X720, (uint8_t *)cOV2640_1280_720_TAB, CNTSOF(cOV2640_1280_720_TAB)},
};

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_DVP_SR_CFG_T sg_dvp_sensor = {
	.max_fps = 30,
	.max_width = 1600,
	.max_height = 1200,
	.fmt = TUYA_FRAME_FMT_YUV422,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Update OV2640 sensor registers via I2C
 * @param port I2C port number
 * @param sensor_reg_table Pointer to sensor register table (register address and value pairs)
 * @param size Number of register entries in the table
 */
static void __dvp_ov2640_update_reg(TUYA_I2C_NUM_E port, const uint8_t sensor_reg_table[][2], uint32_t size)
{
    OPERATE_RET rt = OPRT_OK;
    DVP_I2C_REG_CFG_T i2c_reg_cfg;
    uint16_t val_len = sizeof(sensor_reg_table[0][0]);

    i2c_reg_cfg.port = port;
    i2c_reg_cfg.addr = (OV2640_WRITE_ADDRESS >> 1);
    i2c_reg_cfg.is_16_reg = 0;

    for (uint16_t idx = 0; idx < size; idx++) {
        i2c_reg_cfg.reg = sensor_reg_table[idx][0];
        rt = tdd_dvp_i2c_write(&i2c_reg_cfg, val_len, (uint8_t *)&sensor_reg_table[idx][1]);
        if (rt != OPRT_OK) {
            PR_ERR("ov2640 i2c write err");
            return;
        }
    }
}

/**
 * @brief Reset OV2640 camera sensor
 * @param rst_pin Pointer to reset pin control structure
 * @param arg User argument (unused)
 * @return OPRT_OK on success
 */
static OPERATE_RET __dvp_ov2640_reset(TUYA_CAMERA_IO_CTRL_T *rst_pin, void *arg)
{
    if(NULL == rst_pin) {
        return OPRT_OK;
    }

    (void)arg;

    if(rst_pin->pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {.direct = TUYA_GPIO_OUTPUT};

        gpio_cfg.level = UNACTIVE_LEVEL(rst_pin->active_level);
		tkl_gpio_init(rst_pin->pin, &gpio_cfg);

        tkl_gpio_write(rst_pin->pin, UNACTIVE_LEVEL(rst_pin->active_level));
        tal_system_sleep(20);
        tkl_gpio_write(rst_pin->pin, rst_pin->active_level);
        tal_system_sleep(100);
        tkl_gpio_write(rst_pin->pin, UNACTIVE_LEVEL(rst_pin->active_level));
        tal_system_sleep(100);
        PR_NOTICE("ov2640 reset pin %d lv:%d", rst_pin->pin, rst_pin->active_level);
    }

    return OPRT_OK;
}

/**
 * @brief Initialize OV2640 camera sensor
 * @param i2c Pointer to I2C configuration structure
 * @param arg User argument (unused)
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid
 */
static OPERATE_RET __dvp_ov2640_init(DVP_I2C_CFG_T *i2c, void *arg)
{
    if(NULL == i2c) {
        return OPRT_INVALID_PARM;
    }

    (void)arg;

    __dvp_ov2640_update_reg(i2c->port, cOV2640_INIT_TAB, CNTSOF(cOV2640_INIT_TAB));

    PR_NOTICE("__dvp_ov2640_init");

    return OPRT_OK;
}

/**
 * @brief Set OV2640 camera sensor PPI (resolution) and FPS
 * @param i2c Pointer to I2C configuration structure
 * @param ppi Camera PPI enumeration (resolution)
 * @param fps Frame rate per second
 * @param arg User argument (unused)
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid
 */
static OPERATE_RET __dvp_ov2640_set_ppi(DVP_I2C_CFG_T *i2c, TUYA_CAMERA_PPI_E ppi, uint16_t fps, void *arg)
{
    uint32_t i = 0; 
    CAMERA_PPI_INIT_TAB_T *ppi_init_tab = NULL;

    if(NULL == i2c) {
        return OPRT_INVALID_PARM;
    }

    (void)arg;

    for(i=0; i<CNTSOF(cPPI_INIT_SEQ_LIST); i++) {
        if(ppi == cPPI_INIT_SEQ_LIST[i].ppi) {
            ppi_init_tab = (CAMERA_PPI_INIT_TAB_T *)&cPPI_INIT_SEQ_LIST[i];
            break;
        }
    }

    if(ppi_init_tab) {
        __dvp_ov2640_update_reg(i2c->port, (const uint8_t (*)[2])ppi_init_tab->seq_tab, ppi_init_tab->tab_size);
    }

    PR_NOTICE("__dvp_ov2640_set_ppi ppi:%x, fps:%d", ppi, fps);

    return OPRT_OK;
}

/**
 * @brief Register OV2640 camera sensor device
 * @param name Camera device name
 * @param cfg Pointer to DVP sensor user configuration structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid
 */
OPERATE_RET tdd_camera_dvp_ov2640_register(char *name, TDD_DVP_SR_USR_CFG_T *cfg)
{
    if(NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sg_dvp_sensor.usr_cfg, cfg, sizeof(TDD_DVP_SR_USR_CFG_T));

    TDD_DVP_SR_INTFS_T sr_intfs = {
        .arg      = NULL,
        .rst      = __dvp_ov2640_reset,
        .init     = __dvp_ov2640_init,
        .set_ppi  = __dvp_ov2640_set_ppi,
    };

    return tdl_camera_dvp_device_register(name, &sg_dvp_sensor, &sr_intfs);
}
