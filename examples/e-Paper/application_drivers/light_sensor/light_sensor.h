/**
 * @file light_sensor.h
 * @brief 光敏电阻传感器驱动模块
 * @details 使用ADC读取光敏电阻的模拟信号，获取环境光照强度
 * @version 1.0
 * @date 2025-12-12
 */

#ifndef __LIGHT_SENSOR_H__
#define __LIGHT_SENSOR_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================ 配置参数 ====================================*/

/**
 * @brief 光敏电阻连接的GPIO引脚
 * @note GPIO 13 对应 ADC通道15
 */
#ifndef LIGHT_SENSOR_GPIO_PIN
#define LIGHT_SENSOR_GPIO_PIN       13
#endif

/**
 * @brief ADC采样位宽（12位 = 0~4095）
 */
#define LIGHT_SENSOR_ADC_WIDTH      12

/**
 * @brief ADC最大值（12位）
 */
#define LIGHT_SENSOR_ADC_MAX        4095

/**
 * @brief 参考电压（mV）
 * @note T5的ADC参考电压约为2.4V
 */
#define LIGHT_SENSOR_REF_VOLTAGE_MV 2400

/**
 * @brief 校准参数 - 强光时的ADC值（光照最强时）
 * @note 光敏电阻特性：光照越强 → 阻值越低 → ADC值越低
 *       调整后：raw=97 显示约60%
 */
#define LIGHT_SENSOR_CAL_MIN        30

/**
 * @brief 校准参数 - 暗环境时的ADC值（光照最弱时）
 * @note 调整后使 raw=97 对应约65%
 */
#define LIGHT_SENSOR_CAL_MAX        200

/**
 * @brief 是否反转光照百分比（1=反转，光强时数值高）
 * @note 因为光敏电阻在分压电路上端，光照越强ADC值越低
 *       反转后：光照越强 → 百分比越高(100%)
 */
#define LIGHT_SENSOR_INVERT         1

/*============================ 数据结构 ====================================*/

/**
 * @brief 光敏传感器数据结构
 */
typedef struct {
    INT32_T raw_value;      /**< ADC原始值 (0~4095) */
    INT32_T voltage_mv;     /**< 电压值 (mV) */
    UINT8_T light_percent;  /**< 光照强度百分比 (0~100%) */
} LIGHT_SENSOR_DATA_T;

/*============================ API函数声明 =================================*/

/**
 * @brief 初始化光敏传感器
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET light_sensor_init(VOID_T);

/**
 * @brief 反初始化光敏传感器
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET light_sensor_deinit(VOID_T);

/**
 * @brief 读取ADC原始值
 * @param[out] raw_value: ADC原始值指针 (0~4095)
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET light_sensor_read_raw(INT32_T *raw_value);

/**
 * @brief 读取电压值（mV）
 * @param[out] voltage_mv: 电压值指针（毫伏）
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET light_sensor_read_voltage(INT32_T *voltage_mv);

/**
 * @brief 读取完整的传感器数据
 * @param[out] data: 传感器数据结构指针
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET light_sensor_read(LIGHT_SENSOR_DATA_T *data);

/**
 * @brief 获取光照强度百分比
 * @return 光照强度 0~100%
 * @note 值越大表示光照越强
 */
UINT8_T light_sensor_get_light_percent(VOID_T);

/**
 * @brief 启动周期性读取（独立线程）
 * @param[in] interval_ms: 读取间隔（毫秒）
 * @param[in] callback: 数据回调函数，可为NULL
 * @return OPRT_OK: 成功, 其他: 失败
 */
typedef VOID_T (*LIGHT_SENSOR_CB)(LIGHT_SENSOR_DATA_T *data);
OPERATE_RET light_sensor_start_periodic(UINT32_T interval_ms, LIGHT_SENSOR_CB callback);

/**
 * @brief 停止周期性读取
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET light_sensor_stop_periodic(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __LIGHT_SENSOR_H__ */

