#ifndef __BME280_DRIVER_H__
#define __BME280_DRIVER_H__

#include <stdint.h>
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BME280_OSRS_SKIP = 0,
    BME280_OSRS_X1   = 1,
    BME280_OSRS_X2   = 2,
    BME280_OSRS_X4   = 3,
    BME280_OSRS_X8   = 4,
    BME280_OSRS_X16  = 5
} BME280_OSR_T;

typedef enum {
    BME280_FILTER_OFF = 0,
    BME280_FILTER_2   = 1,
    BME280_FILTER_4   = 2,
    BME280_FILTER_8   = 3,
    BME280_FILTER_16  = 4
} BME280_FILTER_T;

typedef enum {
    BME280_STBY_0_5_MS   = 0,
    BME280_STBY_62_5_MS  = 1,
    BME280_STBY_125_MS   = 2,
    BME280_STBY_250_MS   = 3,
    BME280_STBY_500_MS   = 4,
    BME280_STBY_1000_MS  = 5,
    BME280_STBY_10_MS    = 6,
    BME280_STBY_20_MS    = 7
} BME280_STBY_T;

typedef struct {
    int32_t  temperature_x10;   /* °C * 10 */
    uint32_t pressure_hpa_x10;  /* hPa * 10 */
    uint32_t humidity_x10;      /* RH * 10 (0 for BMP280) */
} BME280_DATA_T;

/* API */
OPERATE_RET bme280_init(void);

OPERATE_RET bme280_config(
    BME280_OSR_T osrs_t,
    BME280_OSR_T osrs_p,
    BME280_OSR_T osrs_h,
    BME280_FILTER_T filter);

OPERATE_RET bme280_read(BME280_DATA_T *out);

/* conversion helpers */
int32_t  bme280_to_celsius_x10(int32_t raw_temp_x10);
uint32_t bme280_to_hpa_x10(uint32_t pressure_hpa_x10);
uint32_t bme280_to_humidity_x10(uint32_t raw_hum_x10);

#ifdef __cplusplus
}
#endif

#endif /* __BME280_DRIVER_H__ */