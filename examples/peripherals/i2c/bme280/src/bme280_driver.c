/**
 * bme280_driver.c
 *
 * TuyaOpen-compatible BME280/BMP280 driver - integer implementation
 * 
 * https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf
 * https://www.bosch-sensortec.com/media/boschsensortec/downloads/product_flyer/bst-bmp280-fl000.pdf
 *
 * Requires Kconfig-generated macros:
 *  BME280_I2C_SCL_PIN
 *  BME280_I2C_SDA_PIN
 *  BME280_I2C_BUS    (0..TUYA_I2C_NUM_MAX-1)
 *  BME280_I2C_ADDR   (0x76 or 0x77)
 *
 * - Public API:
 *     OPERATE_RET bme280_init(void);
 *     OPERATE_RET bme280_config(BME280_OSR_T osrs_t, BME280_OSR_T osrs_p, BME280_OSR_T osrs_h, BME280_FILTER_T filter);
 *     OPERATE_RET bme280_read(BME280_DATA_T *out);
 *
 * - Units 32-bit, Temperature is signed:
 *     temperature_x10 : °C × 10
 *     pressure_hpa_x10: hPa × 10
 *     humidity_x10    : % × 10
 *
 * - Robust I²C helper with simple retry
 * - Safe 64-bit arithmetic for humidity & pressure compensation
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "bme280_driver.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "tkl_output.h"
#include "tal_api.h"

/* BME280 register map */
#define REG_ID          0xD0
#define REG_RESET       0xE0
#define REG_CTRL_HUM    0xF2
#define REG_STATUS      0xF3
#define REG_CTRL_MEAS   0xF4
#define REG_CONFIG      0xF5
#define REG_DATA_START  0xF7
#define REG_CALIB00     0x88
#define REG_CALIB26     0xE1

/* Chip IDs */
#define CHIP_ID_BMP280  0x58
#define CHIP_ID_BME280  0x60

/* sanity */
#ifndef BME280_I2C_BUS
  #error "BME280_I2C_BUS must be defined"
#endif
#ifndef BME280_I2C_SCL_PIN
  #error "BME280_I2C_SCL_PIN must be defined"
#endif
#ifndef BME280_I2C_SDA_PIN
  #error "BME280_I2C_SDA_PIN must be defined"
#endif
#ifndef BME280_I2C_ADDR
  #error "BME280_I2C_ADDR must be defined"
#endif

/* map bus id macro for tkl_i2c calls */
#if (BME280_I2C_BUS == 0)
  #define _I2C_BUS_ID TUYA_I2C_NUM_0
  #define _PINMUX_SCL_FUNC TUYA_IIC0_SCL
  #define _PINMUX_SDA_FUNC TUYA_IIC0_SDA
#elif (BME280_I2C_BUS == 1)
  #define _I2C_BUS_ID TUYA_I2C_NUM_1
  #define _PINMUX_SCL_FUNC TUYA_IIC1_SCL
  #define _PINMUX_SDA_FUNC TUYA_IIC1_SDA
#else
  #define _I2C_BUS_ID ((TUYA_I2C_NUM_E)BME280_I2C_BUS)
  #define _PINMUX_SCL_FUNC TUYA_IIC0_SCL
  #define _PINMUX_SDA_FUNC TUYA_IIC0_SDA
#endif

/* calibration structure */
static struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;

    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;

    int32_t  t_fine;
} s_calib;

/* driver state */
static bool s_inited = false;
static bool s_has_humidity = false;

/* Low-level I2C helpers: simple retry wrapper */
#define I2C_TRIES 3

static inline OPERATE_RET _i2c_master_send_retry(const void *data, uint32_t size, BOOL_T xfer_pending)
{
    OPERATE_RET rt;
    for (int i = 0; i < I2C_TRIES; ++i) {
        rt = tkl_i2c_master_send(_I2C_BUS_ID, BME280_I2C_ADDR, data, size, xfer_pending);
        if (rt == OPRT_OK) return rt;
        tal_system_sleep(5); /* small backoff */
    }
    return rt;
}

static inline OPERATE_RET _i2c_master_recv_retry(void *data, uint32_t size, BOOL_T xfer_pending)
{
    OPERATE_RET rt;
    for (int i = 0; i < I2C_TRIES; ++i) {
        rt = tkl_i2c_master_receive(_I2C_BUS_ID, BME280_I2C_ADDR, data, size, xfer_pending);
        if (rt == OPRT_OK) return rt;
        tal_system_sleep(5);
    }
    return rt;
}

/* read registers with register address send (no stop), then read */
static OPERATE_RET _i2c_read_regs(uint8_t reg, uint8_t *buf, uint32_t len)
{
    OPERATE_RET rt;
    rt = _i2c_master_send_retry(&reg, 1, TRUE);
    if (rt != OPRT_OK) {
        PR_ERR("bme280: i2c send reg 0x%02X failed: %d", reg, rt);
        return rt;
    }
    rt = _i2c_master_recv_retry(buf, len, FALSE);
    if (rt != OPRT_OK) {
        PR_ERR("bme280: i2c read reg 0x%02X len %lu failed: %d", reg, len, rt);
    }
    return rt;
}

/* write single register (reg, val) */
static OPERATE_RET _i2c_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg, val };
    OPERATE_RET rt = _i2c_master_send_retry(tx, sizeof(tx), FALSE);
    if (rt != OPRT_OK) {
        PR_ERR("bme280: i2c write reg 0x%02X=0x%02X failed: %d", reg, val, rt);
    }
    return rt;
}

/* ------------------ Calibration read ------------------ */

static OPERATE_RET _read_calibration(void)
{
    OPERATE_RET rt;
    uint8_t buf[24];

    /* read 0x88..0x9F (24 bytes) */
    rt = _i2c_read_regs(REG_CALIB00, buf, sizeof(buf));
    if (rt != OPRT_OK) return rt;

    s_calib.dig_T1 = (uint16_t)((buf[1] << 8) | buf[0]);
    s_calib.dig_T2 = (int16_t)((buf[3] << 8) | buf[2]);
    s_calib.dig_T3 = (int16_t)((buf[5] << 8) | buf[4]);

    s_calib.dig_P1 = (uint16_t)((buf[7] << 8) | buf[6]);
    s_calib.dig_P2 = (int16_t)((buf[9] << 8) | buf[8]);
    s_calib.dig_P3 = (int16_t)((buf[11] << 8) | buf[10]);
    s_calib.dig_P4 = (int16_t)((buf[13] << 8) | buf[12]);
    s_calib.dig_P5 = (int16_t)((buf[15] << 8) | buf[14]);
    s_calib.dig_P6 = (int16_t)((buf[17] << 8) | buf[16]);
    s_calib.dig_P7 = (int16_t)((buf[19] << 8) | buf[18]);
    s_calib.dig_P8 = (int16_t)((buf[21] << 8) | buf[20]);
    s_calib.dig_P9 = (int16_t)((buf[23] << 8) | buf[22]);

    PR_NOTICE("bme280: T calib: T1=%u, T2=%d, T3=%d",
              s_calib.dig_T1, s_calib.dig_T2, s_calib.dig_T3);
    PR_NOTICE("bme280: P calib: P1=%u, P2=%d, P3=%d, P4=%d, P5=%d",
              s_calib.dig_P1, s_calib.dig_P2, s_calib.dig_P3,
              s_calib.dig_P4, s_calib.dig_P5);

    if (!s_has_humidity) {
        /* For BMP280 ignore humidity bytes */
        return OPRT_OK;
    }

    /* read H1 at 0xA1 */
    rt = _i2c_read_regs(0xA1, &s_calib.dig_H1, 1);
    if (rt != OPRT_OK) return rt;

    /* read 0xE1..0xE7 (7 bytes) */
    uint8_t hbuf[7];
    rt = _i2c_read_regs(REG_CALIB26, hbuf, sizeof(hbuf));
    if (rt != OPRT_OK) return rt;

    s_calib.dig_H2 = (int16_t)((hbuf[1] << 8) | hbuf[0]);
    s_calib.dig_H3 = hbuf[2];

    /* H4 and H5 are 12-bit signed values spread across bytes — assemble and sign-extend */
    {
        int16_t h4 = (int16_t)((hbuf[3] << 4) | (hbuf[4] & 0x0F));
        if (h4 & 0x0800) h4 |= 0xF000;
        s_calib.dig_H4 = h4;

        int16_t h5 = (int16_t)((hbuf[5] << 4) | ((hbuf[4] >> 4) & 0x0F));
        if (h5 & 0x0800) h5 |= 0xF000;
        s_calib.dig_H5 = h5;
    }

    s_calib.dig_H6 = (int8_t)hbuf[6];

    PR_NOTICE("bme280: H calib: H1=%u, H2=%d, H3=%u, H4=%d, H5=%d, H6=%d",
              s_calib.dig_H1, s_calib.dig_H2, s_calib.dig_H3,
              s_calib.dig_H4, s_calib.dig_H5, s_calib.dig_H6);

    return OPRT_OK;
}

/* ------------------ Compensation algorithms ------------------ */
/* Temperature: return °C × 100 (to keep high precision), caller will convert to ×10 */
static int32_t _compensate_T(int32_t adc_T)
{
    /* Based on Bosch datasheet integer algorithm */
    int64_t var1, var2;
    var1 = ((((int64_t)adc_T >> 3) - ((int64_t)s_calib.dig_T1 << 1)) * (int64_t)s_calib.dig_T2) >> 11;
    var2 = (((((int64_t)adc_T >> 4) - (int64_t)s_calib.dig_T1) * (((int64_t)adc_T >> 4) - (int64_t)s_calib.dig_T1)) >> 12) * (int64_t)s_calib.dig_T3 >> 14;
    s_calib.t_fine = (int32_t)(var1 + var2);
    int32_t T = (int32_t)((s_calib.t_fine * 5 + 128) >> 8); /* °C × 100 */
    PR_DEBUG("bme280: raw_T=%ld, t_fine=%ld, T_x100=%ld", (long)adc_T, (long)s_calib.t_fine, (long)T);
    return T;
}

/* Pressure: return hPa × 10 (e.g., 101.3 hPa -> 1013) */
static uint32_t _compensate_P(int32_t adc_P)
{
    /* Use 64-bit math following Bosch algorithm and scale carefully */
    int64_t var1, var2, p;
    var1 = ((int64_t)s_calib.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)s_calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)s_calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)s_calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)s_calib.dig_P3) >> 8) + ((var1 * (int64_t)s_calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)s_calib.dig_P1) >> 33;

    if (var1 == 0) {
        PR_ERR("bme280: pressure compensation var1==0");
        return 0;
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)s_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)s_calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)s_calib.dig_P7) << 4);

    /* p is Pa × (1<<??) depending on shifts; reduce to Pa and convert to hPa × 10 */
    int64_t p_pa = p >> 8; /* approximate Pa */
    if (p_pa < 30000) p_pa = 30000;
    if (p_pa > 110000) p_pa = 110000;

    uint32_t pressure_hpa_x10 = (uint32_t)(p_pa / 10); /* e.g., 101325 Pa -> 10132 (hPa×10) */
    PR_DEBUG("bme280: raw_P=%ld, compensated_P=%lld Pa, hpa_x10=%lu", (long)adc_P, (long long)p_pa, (unsigned long)pressure_hpa_x10);
    return pressure_hpa_x10;
}

/* Humidity: return % × 10 */
static uint32_t _compensate_H(int32_t adc_H)
{
    /* Robust 64-bit implementation per Bosch datasheet but with larger intermediates to avoid precision loss */
    int64_t v_x1;
    int64_t t_fine = (int64_t)s_calib.t_fine;

    v_x1 = t_fine - 76800LL;

    int64_t term1 = ((((int64_t)adc_H << 14) - (((int64_t)s_calib.dig_H4) << 20) - (((int64_t)s_calib.dig_H5) * v_x1) + 16384LL) >> 15);

    int64_t t = v_x1;
    int64_t part2 = (((((t * (int64_t)s_calib.dig_H6) >> 10) * (((t * (int64_t)s_calib.dig_H3) >> 11) + 32768LL)) >> 10) + 2097152LL);
    part2 = ((part2 * (int64_t)s_calib.dig_H2) + 8192LL) >> 14;

    int64_t v = term1 * part2;

    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int64_t)s_calib.dig_H1) >> 4);

    if (v < 0) v = 0;
    if (v > 419430400LL) v = 419430400LL;

    /* Convert to % * 10: (v * 10) >> 22 */
    uint32_t humidity_x10 = (uint32_t)(((uint64_t)v * 10ULL) >> 22);
    return humidity_x10;
}

/* ------------------ Public API ------------------ */

OPERATE_RET bme280_init(void)
{
    OPERATE_RET rt;

    /* pinmux */
    tkl_io_pinmux_config((uint16_t)BME280_I2C_SCL_PIN, _PINMUX_SCL_FUNC);
    tkl_io_pinmux_config((uint16_t)BME280_I2C_SDA_PIN, _PINMUX_SDA_FUNC);

    /* init I2C */
    TUYA_IIC_BASE_CFG_T cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    rt = tkl_i2c_init((TUYA_I2C_NUM_E)BME280_I2C_BUS, &cfg);
    if (rt != OPRT_OK) {
        PR_ERR("bme280: tkl_i2c_init fail %d", rt);
        return rt;
    }

    /* soft reset */
    PR_NOTICE("bme280: performing soft reset...");
    _i2c_write_reg(REG_RESET, 0xB6);
    tal_system_sleep(10);

    /* read chip id */
    uint8_t chip_id;
    rt = _i2c_read_regs(REG_ID, &chip_id, 1);
    if (rt != OPRT_OK) {
        PR_ERR("bme280: cannot read chip ID");
        return OPRT_COM_ERROR;
    }

    if (chip_id == CHIP_ID_BME280) {
        s_has_humidity = true;
        PR_NOTICE("bme280: detected BME280 (with humidity)");
    } else if (chip_id == CHIP_ID_BMP280) {
        s_has_humidity = false;
        PR_NOTICE("bme280: detected BMP280 (no humidity)");
    } else {
        PR_ERR("bme280: unknown chip ID 0x%02X", chip_id);
        return OPRT_COM_ERROR;
    }

    /* configure humidity oversampling first for BME280 */
    if (s_has_humidity) {
        PR_NOTICE("bme280: configuring humidity control");
        rt = _i2c_write_reg(REG_CTRL_HUM, 0x01); /* humidity x1 */
        if (rt != OPRT_OK) return rt;
        tal_system_sleep(2);
    }

    /* configure ctrl_meas and config: temp x1, press x1, normal mode; filter off, standby minimal */
    PR_NOTICE("bme280: configuring measurement control");
    uint8_t ctrl = (1 << 5) | (1 << 2) | 0x03; /* osrs_t=1, osrs_p=1, mode=normal */
    rt = _i2c_write_reg(REG_CTRL_MEAS, ctrl);
    if (rt != OPRT_OK) return rt;
    rt = _i2c_write_reg(REG_CONFIG, 0x00);
    if (rt != OPRT_OK) return rt;

    tal_system_sleep(10); /* wait for first meas */

    /* read calibration */
    rt = _read_calibration();
    if (rt != OPRT_OK) {
        PR_ERR("bme280: read calibration failed");
        return OPRT_COM_ERROR;
    }

    s_inited = true;
    PR_NOTICE("bme280 init ok: %s addr=0x%02X bus=%d",
              s_has_humidity ? "BME280" : "BMP280",
              (unsigned)BME280_I2C_ADDR, BME280_I2C_BUS);
    return OPRT_OK;
}

OPERATE_RET bme280_config(BME280_OSR_T osrs_t, BME280_OSR_T osrs_p, BME280_OSR_T osrs_h, BME280_FILTER_T filter)
{
    if (!s_inited) return OPRT_COM_ERROR;

    if (s_has_humidity) {
        _i2c_write_reg(REG_CTRL_HUM, ((uint8_t)osrs_h) & 0x07);
    }

    uint8_t cfg = (((uint8_t)filter & 0x07) << 2);
    _i2c_write_reg(REG_CONFIG, cfg);

    uint8_t ctrl = ((((uint8_t)osrs_t) & 0x07) << 5) | ((((uint8_t)osrs_p) & 0x07) << 2) | 0x03;
    _i2c_write_reg(REG_CTRL_MEAS, ctrl);

    return OPRT_OK;
}

/* Read one sample, fill out BME280_DATA_T with units:
 * temperature_x10 (°C × 10), pressure_hpa_x10 (hPa × 10), humidity_x10 (% × 10)
 */
OPERATE_RET bme280_read(BME280_DATA_T *out)
{
    if (!s_inited) return OPRT_COM_ERROR;
    if (!out) return OPRT_INVALID_PARM;

    uint8_t buf[8];
    OPERATE_RET rt = _i2c_read_regs(REG_DATA_START, buf, sizeof(buf));
    if (rt != OPRT_OK) return OPRT_COM_ERROR;

    PR_DEBUG("bme280: raw data: %02X %02X %02X %02X %02X %02X %02X %02X",
             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

    /* extract raw adc values (20-bit for T/P, 16-bit for H) */
    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | ((buf[2] >> 4) & 0x0F);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | ((buf[5] >> 4) & 0x0F);

    PR_DEBUG("bme280: adc_T=%ld adc_P=%ld", (long)adc_T, (long)adc_P);

    int32_t temp_x100 = _compensate_T(adc_T); /* °C × 100 */
    out->temperature_x10 = temp_x100 / 10;   /* convert to ×10 */

    out->pressure_hpa_x10 = _compensate_P(adc_P); /* hPa × 10 */

    if (s_has_humidity) {
        int32_t adc_H = ((int32_t)buf[6] << 8) | buf[7];
        PR_DEBUG("bme280: adc_H=%ld t_fine=%ld", (long)adc_H, (long)s_calib.t_fine);
        out->humidity_x10 = _compensate_H(adc_H);
    } else {
        out->humidity_x10 = 0;
    }

    PR_DEBUG("bme280: compensated T=%ld P=%lu H=%lu",
             (long)out->temperature_x10,
             (unsigned long)out->pressure_hpa_x10,
             (unsigned long)out->humidity_x10);

    return OPRT_OK;
}

/* simple accessors (compatibility helpers) */
int32_t bme280_to_celsius_x10(int32_t raw_temp_x10) { return raw_temp_x10; }
uint32_t bme280_to_hpa_x10(uint32_t pressure_hpa_x10) { return pressure_hpa_x10; }
uint32_t bme280_to_humidity_x10(uint32_t x10) { return x10; }
