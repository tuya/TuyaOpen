#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"
#include "tkl_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

// 传感器数据结构
typedef struct {
    float temperature;
    float humidity;
    uint16_t light;      // 光照 (Lux)
    uint16_t light_raw;
    uint16_t co2;        // CO2 (PPM)
    BOOL_T person_detect;// 人体感应
    BOOL_T temp_hum_ok;
    BOOL_T light_ok;
    BOOL_T pir_ok;
    BOOL_T co2_ok;
    INT_T temp_hum_err;
    INT_T light_err;
    INT_T pir_err;
    BOOL_T valid;
    uint32_t timestamp;
} SensorData_t;

// 初始化函数
OPERATE_RET Sensor_Manager_Init(void);

// 数据更新函数（建议在定时器中调用）
OPERATE_RET Sensor_Manager_Update(void);

// 获取数据指针
SensorData_t* Sensor_Manager_GetData(void);

// 执行器控制接口
VOID Sensor_Ctrl_Fan(BOOL_T on);
VOID Sensor_Ctrl_Led(UINT_T brightness); // 0-100
VOID Sensor_Ctrl_Servo(UINT_T angle);    // 0-180

#ifdef __cplusplus
}
#endif

#endif