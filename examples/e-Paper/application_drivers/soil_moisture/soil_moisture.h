/**
 * @file soil_moisture.h
 * @brief 土壤湿度传感器驱动模块
 * @details 使用ADC读取土壤湿度传感器的模拟信号，获取土壤湿度
 * @version 1.0
 * @date 2025-01-18
 */

#ifndef __SOIL_MOISTURE_H__
#define __SOIL_MOISTURE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================ 配置参数 ====================================*/

/**
 * @brief 土壤湿度传感器连接的GPIO引脚
 * @note GPIO 12 对应 ADC通道14
 */
#ifndef SOIL_MOISTURE_GPIO_PIN
#define SOIL_MOISTURE_GPIO_PIN      12
#endif

/**
 * @brief ADC采样位宽（12位 = 0~4095）
 */
#define SOIL_MOISTURE_ADC_WIDTH     12

/**
 * @brief ADC最大值（12位）
 */
#define SOIL_MOISTURE_ADC_MAX       4095

/**
 * @brief 参考电压（mV）
 * @note T5的ADC参考电压约为2.4V
 */
#define SOIL_MOISTURE_REF_VOLTAGE_MV 2400

/**
 * @brief 校准参数 - 干燥时的ADC值（已校准）
 * @note 您的传感器特性：干燥时ADC值低，湿润时ADC值高
 *       空气中ADC约386，设为380作为0%基准
 */
#define SOIL_MOISTURE_CAL_DRY       380     // 干燥时的ADC值（对应0%）

/**
 * @brief 校准参数 - 湿润时的ADC值（已校准）
 * @note 水中ADC约400，设为400作为100%基准
 */
#define SOIL_MOISTURE_CAL_WET       400     // 湿润时的ADC值（对应100%）

/*============================ 数据结构 ====================================*/

/**
 * @brief 土壤湿度传感器数据结构
 */
typedef struct {
    INT32_T raw_value;          /**< ADC原始值 (0~4095) */
    INT32_T voltage_mv;         /**< 电压值 (mV) */
    UINT8_T moisture_percent;   /**< 湿度百分比 (0~100%) */
} SOIL_MOISTURE_DATA_T;

/*============================ API函数声明 =================================*/

/**
 * @brief 初始化土壤湿度传感器
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET soil_moisture_init(VOID_T);

/**
 * @brief 反初始化土壤湿度传感器
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET soil_moisture_deinit(VOID_T);

/**
 * @brief 读取ADC原始值
 * @param[out] raw_value: ADC原始值指针 (0~4095)
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET soil_moisture_read_raw(INT32_T *raw_value);

/**
 * @brief 读取电压值（mV）
 * @param[out] voltage_mv: 电压值指针（毫伏）
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET soil_moisture_read_voltage(INT32_T *voltage_mv);

/**
 * @brief 读取完整的传感器数据
 * @param[out] data: 传感器数据结构指针
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET soil_moisture_read(SOIL_MOISTURE_DATA_T *data);

/**
 * @brief 获取土壤湿度百分比
 * @return 湿度 0~100%
 * @note 值越大表示土壤越湿润
 */
UINT8_T soil_moisture_get_percent(VOID_T);

/**
 * @brief 启动周期性读取（独立线程）
 * @param[in] interval_ms: 读取间隔（毫秒）
 * @param[in] callback: 数据回调函数，可为NULL
 * @return OPRT_OK: 成功, 其他: 失败
 */
typedef VOID_T (*SOIL_MOISTURE_CB)(SOIL_MOISTURE_DATA_T *data);
OPERATE_RET soil_moisture_start_periodic(UINT32_T interval_ms, SOIL_MOISTURE_CB callback);

/**
 * @brief 停止周期性读取
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET soil_moisture_stop_periodic(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __SOIL_MOISTURE_H__ */
