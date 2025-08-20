/**
 * @file tuay_hal_cellular_vbat.h
 * @author www.tuya.com
 * @brief 蜂窝模组电池功能API定义。
 *
 * @copyright Copyright (c) tuya.inc 2021
 */

#ifndef __TAL_HAL_CELLULAR_VBAT_H__
#define __TAL_HAL_CELLULAR_VBAT_H__

#if !defined(ENABLE_CELLULAR_PLUGIN) || ENABLE_CELLULAR_PLUGIN == 0
#include "tkl_cellular_base.h"
#include "tkl_cellular_vbat.h"
#else
// 包含at版本的蜂窝接口头文件
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取电池剩余电量百分比
 * @param rsoc 剩余电量百分比
 * @return OPRT_OK 获取成功 其它 获取失败
 */
OPERATE_RET tal_cellular_vbat_get_rsoc(uint8_t* rsoc);

/**
 * @brief 设置是否开启NTC检测电池温度
 * @param enable NTC检测电池温度开/关
 * @return OPRT_OK 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_vbat_ntc_enable(bool enable);

/**
 * @brief 设置恒流充电阶段，电池充电电流
 * @param current 充电电流，单位毫安mA
 * @return OPRT_OK 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_vbat_set_charge_current(uint32_t current);

/**
 * @brief 获取充电器状态
 * @return 充电器状态，查看 @TKL_CELLULAR_VBAT_CHG_STATE_E定义
 */
TKL_CELLULAR_VBAT_CHG_STATE_E tal_cellular_vbat_get_charger_state(void);

/**
 * @brief 注册电池及充电器消息回调处理函数
 * @param callback 回调函数
 * @return 0 注册成功 其它 注册失败
 */
OPERATE_RET tal_cellular_vbat_cb_register(TKL_CELLULAR_VBAT_CHARGE_CB callback);

/**
 * @brief 获取电池电压
 * @param voltage 当前电池电压，单位mV
 * @return OPRT_OK 获取成功 其它 获取失败
 */
OPERATE_RET tal_cellular_vbat_get_voltage(uint32_t* voltage);

/**
 * @brief 是否开启电池低电压关机功能
 * @param enable TRUE 开启，FLASE关闭
 * @return OPRT_OK 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_vbat_low_volt_poweroff_enable(bool enable);

#ifdef __cplusplus
}
#endif

#endif
