#include "Sensor_Manager.h"
#include "AHT20.h"
#include "BH1750.h"
#include "OLED.h"
#include "tal_api.h"
#include "tkl_pinmux.h"
#include "tkl_system.h"
#include <stdio.h>
#include <string.h>

// 引脚定义
#define PIN_FAN       24
#define PIN_PIR       23
#define PIN_SERVO     26
#define PIN_LED       31
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   15

static SensorData_t g_sensor_data = {0};
static BOOL_T g_initialized = FALSE;

static VOID _oled_refresh_sensor_data(const SensorData_t *data)
{
    char line[22] = {0};

    if (data == NULL) {
        return;
    }

    OLED_Clear();
    OLED_ShowString(0, 0, "Smart Sensor");

    snprintf(line, sizeof(line), "T:%d H:%d", (int)data->temperature, (int)data->humidity);
    OLED_ShowString(1, 0, line);

    snprintf(line, sizeof(line), "AHT:R%ld %s", (long)data->temp_hum_err, data->temp_hum_ok ? "OK" : "NG");
    OLED_ShowString(2, 0, line);

    snprintf(line, sizeof(line), "L:%u R%ld", data->light, (long)data->light_err);
    OLED_ShowString(3, 0, line);

    snprintf(line, sizeof(line), "P:%s R%ld", data->person_detect ? "YES" : "NO", (long)data->pir_err);
    OLED_ShowString(4, 0, line);

    snprintf(line, sizeof(line), "Time:%lu", (unsigned long)(data->timestamp / 1000));
    OLED_ShowString(5, 0, line);
}

// PWM 辅助函数（简化版，实际需根据 SDK 配置 PWM 通道）
STATIC VOID _pwm_set_duty(UINT_T pin, UINT_T duty_percent) {
    // 注意：此处需要根据 TuyaOpen SDK 的 PWM API 具体实现
    // 如果 SDK 不支持动态 PWM，可暂时用 GPIO 模拟开关
    if (duty_percent > 0) {
        tkl_gpio_write(pin, TUYA_GPIO_LEVEL_HIGH);
    } else {
        tkl_gpio_write(pin, TUYA_GPIO_LEVEL_LOW);
    }
}

OPERATE_RET Sensor_Manager_Init(void) {
    if (g_initialized) return OPRT_OK;

    PR_NOTICE(">>> Sensor Manager Init Start");

    // 1. 传感器使用软件 I2C2，共用 GPIO14/15
    tkl_io_pinmux_config(I2C_SCL_PIN, TUYA_IIC2_SCL);
    tkl_io_pinmux_config(I2C_SDA_PIN, TUYA_IIC2_SDA);
    
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT
    };
    tkl_i2c_init(TUYA_I2C_NUM_2, &i2c_cfg);

    // 2. 传感器初始化
    OPERATE_RET aht_ret = AHT20_Init();
    OPERATE_RET bh_ret = BH1750_Init();
    PR_NOTICE("Sensor init result: AHT20=%ld BH1750=%ld", (long)aht_ret, (long)bh_ret);
    OLED_Init();
    // TODO: 如果有 CO2 驱动，在此处调用 CO2_Init()

    TUYA_GPIO_BASE_CFG_T gpio_cfg_out = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_LOW
    };
    
    TUYA_GPIO_BASE_CFG_T gpio_cfg_in = {
        .mode = TUYA_GPIO_FLOATING,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_LOW
    };

    tkl_gpio_init(PIN_FAN, &gpio_cfg_out);
    tkl_gpio_init(PIN_LED, &gpio_cfg_out);
    tkl_gpio_init(PIN_SERVO, &gpio_cfg_out);
    tkl_gpio_init(PIN_PIR, &gpio_cfg_in);

    OLED_ShowString(0, 0, "Smart Sensor");
    OLED_ShowString(2, 0, "Init Done");

    g_initialized = TRUE;
    PR_NOTICE(">>> Sensor Manager Init Done");
    return OPRT_OK;
}

// ... existing code ...

OPERATE_RET Sensor_Manager_Update(void) {
    if (!g_initialized) return OPRT_COM_ERROR;

    float temp = 0, hum = 0;
    OPERATE_RET temp_ret = AHT20_Read_Data_CRC(&temp, &hum);
    if (temp_ret == OPRT_OK) {
        g_sensor_data.temperature = temp;
        g_sensor_data.humidity = hum;
        g_sensor_data.temp_hum_ok = TRUE;
        g_sensor_data.temp_hum_err = 0;
    } else {
        g_sensor_data.temp_hum_ok = FALSE;
        g_sensor_data.temp_hum_err = temp_ret;
        PR_WARN("AHT20 read failed: %ld", (long)temp_ret);
    }

    uint16_t light = 0;
    OPERATE_RET light_ret = BH1750_Read_Light(&light);
    if (light_ret == OPRT_OK) {
        g_sensor_data.light = light;
        g_sensor_data.light_ok = TRUE;
        g_sensor_data.light_err = 0;
    } else {
        g_sensor_data.light_ok = FALSE;
        g_sensor_data.light_err = light_ret;
        PR_WARN("BH1750 read failed: %ld", (long)light_ret);
    }

    TUYA_GPIO_LEVEL_E pir_level = TUYA_GPIO_LEVEL_LOW;
    OPERATE_RET pir_ret = tkl_gpio_read(PIN_PIR, &pir_level);
    if (pir_ret == OPRT_OK) {
        g_sensor_data.person_detect = (pir_level == TUYA_GPIO_LEVEL_LOW) ? TRUE : FALSE;
        g_sensor_data.pir_ok = TRUE;
        g_sensor_data.pir_err = 0;
    } else {
        g_sensor_data.pir_ok = FALSE;
        g_sensor_data.pir_err = pir_ret;
        PR_WARN("PIR read failed: %ld", (long)pir_ret);
    }

    g_sensor_data.co2 = 0;
    g_sensor_data.co2_ok = FALSE;

    g_sensor_data.valid = TRUE;
    g_sensor_data.timestamp = tkl_system_get_millisecond();
    PR_NOTICE("Sensor ret: AHT=%ld BH=%ld PIR=%ld, value: T=%.1f H=%.1f L=%u P=%d",
              (long)temp_ret, (long)light_ret, (long)pir_ret,
              g_sensor_data.temperature, g_sensor_data.humidity, g_sensor_data.light, g_sensor_data.person_detect);
    _oled_refresh_sensor_data(&g_sensor_data);
    
    return OPRT_OK;
}


SensorData_t* Sensor_Manager_GetData(void) {
    return &g_sensor_data;
}

VOID Sensor_Ctrl_Fan(BOOL_T on) {
    tkl_gpio_write(PIN_FAN, on ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
}

VOID Sensor_Ctrl_Led(UINT_T brightness) {
    _pwm_set_duty(PIN_LED, brightness);
}

VOID Sensor_Ctrl_Servo(UINT_T angle) {
    // 舵机通常需要 50Hz PWM，占空比 2.5%-12.5% 对应 0-180度
    // 此处简化处理，实际需调用 tkl_pwm_start
    printf("Servo set to angle: %d\r\n", angle);
}