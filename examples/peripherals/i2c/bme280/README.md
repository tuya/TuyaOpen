# BME280/BMP280 driver - integer implementation
**Sensor IC Datasheets:**  
https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf  
https://www.bosch-sensortec.com/media/boschsensortec/downloads/product_flyer/bst-bmp280-fl000.pdf

## ✅ 1. Initialization
```
#include "bme280_driver.h"
```
* Call `bme280_init`
    * Configures pinmux
    * Configures GPIO (SCL/SDA)
    * Configures I2C (tkl_i2c_init)
* Call `bme280_config()` - to set filters:

**Ultra low-power (fast)**

```c
bme280_config(BME280_OSRS_X1, BME280_OSRS_X1, BME280_OSRS_X1, BME280_FILTER_OFF);
```

**Balanced (recommended general purpose)**

```c
bme280_config(BME280_OSRS_X2, BME280_OSRS_X2, BME280_OSRS_X1, BME280_FILTER_4);
```

**High accuracy (slow but stable)**

```c
bme280_config(BME280_OSRS_X4, BME280_OSRS_X4, BME280_OSRS_X2, BME280_FILTER_16);
```

**Example init:**

```c
void sensor_init(void)
{
    PR_DEBUG("app_init");

    /* ----- 1. BME280 Init ----- */
    OPERATE_RET ret = bme280_init();
    if (ret != OPRT_OK) {
        PR_ERR("BME280 init error %d", ret);
    } else {
        PR_DEBUG("BME280 init OK");
    }
    
    /* ----- 2. BME280 Config ----- */
    ret = bme280_config(BME280_OSRS_X2, BME280_OSRS_X2, BME280_OSRS_X1, BME280_FILTER_4);
    if (ret != OPRT_OK) {
        PR_ERR("BME280 config error %d", ret);
    } else {
        PR_DEBUG("BME280 config OK");
    }
}
```

---

## ✅ 2. Read values in **periodic task** (timer or main loop - use bme280_read)

```c
BME280_DATA_T env;

if (bme280_read(&env) == OPRT_OK) {
    temp_x10 = env.temperature_x10;
    humi_x10 = env.humidity_x10;
    pressure_hpa_x10 = env.pressure_hpa_x10;
    PR_DEBUG("🌡️ BME280: T=%d.%d°C, H=%d.%d%%, P=%d.%dhPa",
            temp_x10/10, temp_x10%10, 
            humi_x10/10, humi_x10%10,
            pressure_hpa_x10/10, pressure_hpa_x10%10);
}
```

---

## ✅ 3. Kconfig settings
Run `tos.py config menu`  

<img width="244" height="90" alt="Image" src="https://github.com/user-attachments/assets/e81f70fd-50f1-463e-9a7a-184ef824c14b" />  

<img width="244" height="193" alt="Image" src="https://github.com/user-attachments/assets/a1d8a3a5-d3c1-4261-8341-44a3a22402af" />

## ✅ 4. Cloud
 Use scale:1 because all values as DPs are sending integers x 10

