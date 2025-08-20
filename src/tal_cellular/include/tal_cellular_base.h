/**
 * @file tal_cellular.h
 * @brief
 * @version 0.1
 * @date 2025-08-21
 *
 * @copyright Copyright (c) 2021-2022 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */

#ifndef _TAL_CELLULAR_H_
#define _TAL_CELLULAR_H_

#include "tuya_cloud_types.h"

#if !defined(ENABLE_CELLULAR_PLUGIN) || ENABLE_CELLULAR_PLUGIN == 0
#include "tkl_cellular_base.h"
#include "tkl_cellular_comm.h"
#else
// 包含at版本的蜂窝接口头文件
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    CELLULAR_SINGAL_QUAL_UNKNOWN    = 0,
    CELLULAR_SINGAL_QUAL_POOR,
    CELLULAR_SINGAL_QUAL_MIDDLE,
    CELLULAR_SINGAL_QUAL_GOOD,
    CELLULAR_SINGAL_QUAL_GREAT,
}TUYA_CELLULAR_SINGAL_QAUL_E;

/**
 * @brief 蜂窝初始化
 * @return 0 成功  其它 失败
 */
OPERATE_RET tal_cellular_base_init(TKL_CELL_INIT_PARAM_T *cfg);

/**
 * @brief 获取当前设备的通讯能力
 * @param ability @TKL_CELLULAR_ABILITY_E 类型
 * @return 0 成功  其它 失败
 */
OPERATE_RET tal_cellular_get_ability(TKL_CELLULAR_ABILITY_E *ability);

/**
 * @brief 切换当前使能的SIM卡。
 * @param simid SIM卡ID.(0~1)
 * @return 0 成功  其它 失败
 */
OPERATE_RET tal_cellular_switch_sim(uint8_t sim_id);

/**
 * @brief 注册SIM状态变化通知函数
 * @param fun 状态变化通知函数
 * @return 0 成功  其它 失败
 */
OPERATE_RET tal_cellular_register_sim_state_notify(uint8_t sim_id, TKL_SIM_NOTIFY fun);

/**
 * @brief 使能或禁止sim卡热拔插
 *
 * @param simId sim卡ID
 * @param enable TRUE 使能 FALSE 禁止
 *
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_sim_hotplug(uint8_t sim_id, bool enable);

/**
 * @brief 获取SIM卡的状态
 * @param simId sim卡ID
 * @param state 1：正常，0：异常
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_sim_get_status(uint8_t sim_id, uint8_t *state);

/**
 * @brief 获取蜂窝设备当前的通信功能设置
 *
 * @param cfun 获取的通信功能
 *
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_fun_mode(uint8_t simd_id,int * cfun);

/**
 * @brief 设置蜂窝设备的通信功能模式
 *
 * @param cfun 通信功能，取值含义如下：
 *            1：全功能模式
 *            4：飞行模式
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_set_fun_mode(uint8_t simd_id,int cfun);

/**
 * @brief 获取SIM卡中的国际移动用户识别码
 * @param simid,SIM卡id号(0,1,2...)
 * @param imsi识别码，为16字节的字符串
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_imsi(uint8_t sim_id,char imsi[15 + 1]);

/**
 * @brief 获取SIM卡的ICCID
 * @param simid
 * @param ICCID识别码，为20字节的字符串
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_iccid(uint8_t sim_id,char iccid[20 + 1]);

/**
 * @brief 获取SIM卡所在通道设备的IMEI号
 * @param simid
 * @param IMEI识别码，为15字节的字符串
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_imei(uint8_t sim_id, char imei[15 + 1]);

/**
 * @brief 设置设备的IMEI号
 * @param simid
 * @param IMEI识别码，为15字节的字符串
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_set_imei(uint8_t sim_id,char imei[15 + 1]);

/**
 * @brief 获取SIM卡所在通道蜂窝设备的信号接收功率——单位dbm
 * @param simid
 * @param rsrp 返回实际的信号强度(dbm)
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_rsrp(uint8_t sim_id,int *rsrp);

/**
 * @brief 获取蜂窝设备SIM卡所在通道的信号噪声比及误码率
 * @param simid
 * @param sinr 干扰信噪比
 * @param bit_error (0~7,99) 99无网络
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_sinr(uint8_t sim_id,int *sinr,int *bit_error);

/**
 * @brief SIM卡所在通道LBS的基站信息)
 * @param sim_id
 * @param lbs 返回基站信息
 * @param neighbour 是否搜索临近基站信息
 * @param timeout 搜索临近基站信息超时时间(一般需要4秒左右)
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_lbs(uint8_t sim_id,TKL_LBS_INFO_T *lbs,bool neighbour,int timeout);

/**
 * @brief 获取当前设备的射频校准状态
 * @return TRUE正常，FALSE异常
 */
bool tal_cellular_rf_calibrated(void);

/**
 * @brief 使能或禁止sim卡gpio检测
 * @param sim_id sim卡ID
 * @param enable TRUE 使能 FALSE 禁止
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_enable_sim_detect(uint8_t sim_id, bool enable);

/**
 * @brief 获取默认的SIM ID
 * @return 小于0失败，其他SIM ID
 */
int8_t tal_cellular_base_get_default_simid(void);

/**
 * brief 关闭长按POWER KEY关机功能。
 * @param disable TRUE 关闭关机功能 FALSE 打开关机功能
 * @return 0 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_powerkey_off_disable(bool disable);

/**
 * @brief 蜂窝基础的通用控制功能，一般作为平台提供一些特殊的能力接口
 *
 * @param cmd 参考CELL_IOCTRL_CMD
 * @param argc argv数组大小
 * @param argv 平台自定义
 *
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_ioctl(int cmd,void* argv);

/**
 * @brief get the epoch time in second
 *
 * The time is seconds from 1970-01-01 UTC. To avoid overflow, the data
 * type is 64 bits.
 * - signed 32bits will overflow at year 2038
 * - unsigned 32bits will overflow at year 2106
 *
 * Epoch time is not monoclinic time. For example, when system time is
 * synchronized with network, there may exist a jump (forward or backward)
 * of epoch time.
 *
 * In 2 cases, this system time may be not reliable:
 * - During boot, and before RTC is initialized.
 * - During wakeup, and the elapsed sleep time hasn't compensated.
 *
 * @return      epoch time in seconds
 */
OPERATE_RET tal_cellular_get_epoch_sec(uint64_t *epoch_sec);

/**
 * @brief 获取指定的SIM 卡所在网络的rssi dBm值
 *
 * @param sim_id sim id
 * @param rssi_dBm 获取到的rssi dBm值
 *
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_rssidbm(uint8_t sim_id,int  *rssi_dBm);

/**
 * @brief 获取指定的SIM 卡所在网络的rssi值
 * @param sim_id sim id
 * @param rssi_dBm 获取到的rssi(0~31)
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_rssi(uint8_t sim_id,int  *rssi);

/**
 * @brief 获取指定的SIM 卡所在网络的网络类型
 * @param sim_id sim id
 * @param net_type 网络类型
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_get_nettype(uint8_t sim_id,TUYA_CELLULAR_RAT_E *net_type);

/**
 * @brief 获取指定的SIM 卡所在网络信号质量(5个等级)
 * @param sim_id sim id
 * @return TUYA_CELLULAR_SINGAL_QAUL_E 类型
 */
TUYA_CELLULAR_SINGAL_QAUL_E tal_cellular_get_singal_quailty(uint8_t sim_id);

/**
 * @brief 获取设备本次启动后，指定的CID 所在网络的数据上行及下线统计
 *
 * @param cid 数据承载通道(1~7),系统默认使用通道1
 * @param rx  数据下行统计，单位BYTE
 * @param tx  数据上行统计，单位BYTE
 * @return TUYA_CELLULAR_SINGAL_QAUL_E 类型
 */
OPERATE_RET tal_cellular_get_data_statistics(uint8_t cid,uint32_t *rx,uint32_t *tx);

/**
 * @brief 获取蜂窝设备的本地时间。（本时间为通过基站时间同步的时间）
 *
 * @param local_tm   struct tm类型时间指针
 * @return 0 其他失败
 */
OPERATE_RET tal_cellular_get_localtime(struct tm *local_tm);

/**
 * @brief 获取通过基站网络信息同步获取到的时区
 *
 * @return 时区，单位秒
 */
OPERATE_RET tal_cellular_get_timezone(int *timezone);

/**
 * @brief 获取蜂窝的模块NV中的SN号，不是所有模块中的都会内置SN。
 * @param sim_id (预留参数)
 * @param sn 获取的SN号
 * @return 0 其他失败
 */
OPERATE_RET tal_cellular_get_sn(char sn[25]);

/**
 * @brief 设置模组底层网络注册事件的回调
 * @note 主要底层上报TUYA_CELLULAR_MDS_STATUS_IDLE，TUYA_CELLULAR_MDS_STATUS_REG,TUYA_CELLULAR_MDS_STATUS_CAMPED 3个事件。
 * 分别表示搜网中，注网成功，停止搜网
 *
 * @param fun 回调函数
 * @return OPERATE_RET 操作结果，成功返回OPRT_OK，失败返回错误码
 */
OPERATE_RET tal_cellular_register_dev_reg_notify(uint8_t simd_id,TKL_REGISTION_NOTIFY fun);

/**
 * @brief 初始化虚拟AT服务
 * @param resp_callback AT应答数据回调函数
 * @return 0 其他失败
 */
OPERATE_RET tal_cellular_init_virtual_at(TKL_VIRTAT_RESP resp_callback);

/**
 * @brief 发送AT命令
 * @param at_cmd at命令以\r\n为结束符
 * @return 0 其他失败
 */
OPERATE_RET tal_cellular_send_virtual_at(char *at_cmd);

/**
 * @brief 设置蜂窝的模块没有数据收发的检测时间，如果在超过的设置时间，蜂窝就主动释放RRC。
 *        目的是进一步优化功耗。时间越小，有可能造成数据的丢包，慎重使用。
 *
 * @param time 时间，范围0~20。步进为500ms。0则关闭优化功能。
 * @return 0 其他失败
 */
OPERATE_RET tal_cellular_set_rrc_release_time(uint32_t time);

/**
 * @brief 获取蜂窝的模块没有数据收发的检测时间。
 * @return 范围0~20。步进为500ms
 */
uint32_t tal_cellular_get_rrc_release_time(void);

/**
 * @brief 设置模组的搜网的PLMN
 * @note 设置模组搜索指定的运营商，如果搜索失败，则自动再自动搜索运营商
 *
 * @param plmn 数字编码的运营商，如中国移动，"46000"
 * @return OPERATE_RET 操作结果，成功返回OPRT_OK，失败返回错误码
 */
OPERATE_RET tal_cellular_comm_set_plmn(char *plmn);

#ifdef __cplusplus
  }
#endif
#endif /* TAL_CELLULAR_BASE */
