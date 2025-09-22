/**
 * @file tkl_init_cellular_base.h
 * @brief Common process - tkl init cellular description
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_INIT_CELLULAR_BASE_H__
#define __TKL_INIT_CELLULAR_BASE_H__

#include "tuya_cloud_types.h"
#include "tkl_cellular_base.h"
#include "tkl_cellular_call.h"
#include "tkl_cellular_mds.h"
#include "tkl_cellular_sms.h"
#include "tkl_cellular_vbat.h"
#include "tkl_cellular_player.h"
#include "tkl_cellular_keypad.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 蜂窝模组基础服务API接口定义
 * TAL在实现tkl_CellularSvcIntf_t接口后，需要调用tal_cellular_mds_register
 * 把实现的对象注册到tkl中
 */
typedef struct {

    OPERATE_RET (*base_init)(TKL_CELL_INIT_PARAM_T *param);
    /**
     * @brief 获取当前设备的通讯能力
     * @param ability @TKL_CELLULAR_ABILITY_E 类型
     * @return 0 成功  其它 失败
     */
    OPERATE_RET (*get_ability)(TKL_CELLULAR_ABILITY_E *ability);
    /**
     * @brief 切换当前使能的SIM卡。如果是双卡双待，或者单卡则不需要实现该接口
     * @param simid SIM卡ID.(0~1)
     * @return 0 成功  其它 失败
     */
    OPERATE_RET (*switch_sim)(uint8_t sim_id);
    /**
     * @brief 注册SIM状态变化通知函数
     * @param fun 状态变化通知函数
     * @return 0 成功  其它 失败
     */
    OPERATE_RET (*register_sim_state_notify)(uint8_t sim_id, TKL_SIM_NOTIFY notify);

    /**
     * @brief 使能或禁止sim卡热拔插
     * @param simId sim卡ID
     * @param enable TRUE 使能 FALSE 禁止
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*sim_hotplug_enable)(uint8_t sim_id, BOOL_T enable);

    /**
     * @brief 获取SIM卡的状态
     * @param simId sim卡ID
     * @param state 1：正常，0：异常
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*sim_get_status)(uint8_t sim_id, uint8_t *state);

    /**
     * @brief 获取蜂窝设备当前的通信功能设置
     * @param cfun 获取的通信功能
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*get_cfun_mode)(uint8_t sim_id, PINT_T cfun);

    /**
     * @brief 设置蜂窝设备的通信功能模式
     * @param cfun 通信功能，取值含义如下：
     *            1：全功能模式
     *            4：飞行模式
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*set_cfun_mode)(uint8_t sim_id, int cfun);

    /**
     * @brief 获取SIM卡中的国际移动用户识别码
     * @param simid
     * @param imsi识别码，为15字节的字符串
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*get_imsi)(uint8_t sim_id, char imsi[15 + 1]);

    /**
     * @brief 获取SIM卡的ICCID
     * @param simid
     * @param ICCID识别码，为20字节的字符串
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*get_iccid)(uint8_t sim_id, char iccid[20 + 1]);
    /**
     * @brief 获取SIM卡所在通道设备的IMEI号
     * @param simid
     * @param IMEI识别码，为15字节的字符串
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*get_imei)(uint8_t sim_id, char imei[15 + 1]);

    /**
     * @brief 设置SIM卡所在通道设备的IMEI号
     * @param IMEI识别码，为15字节的字符串
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*set_imei)(uint8_t sim_id, char imei[15 + 1]);

    /**
     * @brief 获取当前蜂窝设备的信号接收功率——单位dbm
     * @param simid
     * @param rsrp 返回实际的信号强度(dbm)
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*get_rsrp)(uint8_t sim_id, int *rsrp);

    /**
     * @brief 获取当前蜂窝设备的信号噪声比
     * @param simid
     * @param sinr (0~31)
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*get_sinr)(uint8_t sim_id, int *sinr, int *bit_error);

    /**
     * @brief 当前LBS的基站信息(只支持LTE)
     * @param simid
     * @param lbs 返回基站信息
     * @param neighbour 是否搜索临近基站信息
     * @param timeout 搜索临近基站信息超时时间(一般需要4秒左右)
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*get_lbs)(uint8_t simid, TKL_LBS_INFO_T *lbs, BOOL_T neighbour, int timeout);

    /**
     * @brief 获取当前设备的射频校准状态
     * @param
     * @return TRUE正常，FALSE异常
     */
    BOOL_T (*rf_calibrated)(void);

    /**
     * @brief 设置是否使能SIM检测功能
     * @param   enable TRUE 使能，FALSE 禁止
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*enable_sim_detect)(uint8_t simid, BOOL_T enable);

    /**
     * @brief 获取默认的SIM ID
     * @return 小于0失败，其他SIM ID
     */
    int8_t (*get_default_simid)(void);

    /**
     * @brief 平台提供一些特殊的能力接口
     *
     * @param cmd 参考CELL_IOCTRL_CMD
     * @param argv 平台自定义
     *
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*ioctl)(int cmd, void *argv);

} TKL_CELL_BASE_INTF_T;

/**
 * @brief register cellular base function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_BASE_INTF_T *tkl_cellular_base_desc_get();

/**
 * @brief 蜂窝模组电话拨号API接口定义
 */
typedef struct {
    /**
     * @brief 查询呼叫服务是否可用
     * @param simId
     * @return 1 呼叫服务可用 0 呼叫服务不可用
     */
    BOOL_T (*call_service_available)(uint8_t sim_id);

    /**
     * @brief 呼出拨号接口函数
     * @param simId sim卡ID号
     * @param callNum 呼叫号码，字符串形式
     * @return  0 发起呼叫成功 其它 失败
     */
    OPERATE_RET (*call)(uint8_t sim_id, char call_number[TKL_CELLULAR_CALLNUM_LEN_MAX]);

    /**
     * @brief 外部呼叫到来时，应答接听接口函数
     * @param sim_id
     * @return  0 应答成功 其它 失败
     */
    OPERATE_RET (*answer)(uint8_t sim_id);

    /**
     * @brief 呼叫通话结束后，挂机接口函数
     * @param sim_id
     * @return  0 挂机成功 其它 失败
     */
    OPERATE_RET (*hungup)(uint8_t sim_id);

    /**
     * @brief 注册呼叫回调处理接口函数
     * @param callback 呼叫回调处理函数
     * @return  0  注册成功 其它  注册失败
     */
    OPERATE_RET (*cb_register)(TKL_CELLULAR_CALL_CB callback);

    /**
     * @brief 设置呼入时铃声静音
     * @param mute TRUE 静音 FALSE 非静音
     * @return 0 设置成功 其它 设置失败
     */
    OPERATE_RET (*set_callin_mute)(BOOL_T mute);

    /**
     * @brief 启动或者关闭volte功能
     * @param sim_id
     * @param enable TRUE启用volte,false关闭volte
     * @return 0 设置成功 其它 设置失败
     */
    OPERATE_RET (*set_volte)(uint8_t sim_id, BOOL_T enable);

    /**
     * @brief 通话过程中，上行音频是否设置静音
     * When the current voice call is finished, the property will be kept.
     * 当前的语音呼叫结束后，这个属性会被保存下来。下次再进行语音通话的过程中，如果
     * 上次被设置为静音，则继续静音。但是重启后，会被默认恢复成非静音
     * @param TRUE for mute uplink of voice call
     * @return 0 成功，其他失败
     */
    OPERATE_RET (*set_voice_mute)(BOOL_T mute);

    /**
     * @brief 获取通话过程中的上行声音的静音状态。
     * @param mute : TRUE 静音
     * @return 0 成功，其他失败
     */
    OPERATE_RET (*get_voice_mute)(PBOOL_T mute);

    /**
     * @brief 设置语音通话音频的音量
     * @param vol （0~100）
     * @return 0 成功，其他失败
     */
    OPERATE_RET (*set_voice_vol)(int vol);

    /**
     * @brief 获取语音通话音频的音量
     * @param vol （0~100）
     * @return 0 成功，其他失败
     */
    OPERATE_RET (*get_voice_vol)(PINT_T vol);

    /**
     * @brief 播放电话的拨号音
     * @param tone 拨号音的特征值
     * @param duration 播放音的持续时间
     * @return 0 成功，其他失败
     */
    OPERATE_RET (*play_tone)(TUYA_TONE_TYPE_E tone, int duration);

    /**
     * @brief 停止电话的拨号音
     * @param 无
     * @return 0 成功，其他失败
     */
    OPERATE_RET (*stop_tone)(void);

    /**
     * @brief 将DTMF数字按键音频发送到语音通道
     * @note 1、该函数用于实现10086之类的语音交互，语音提示后，用户操作按键，调用该函数
     *       将按键音频发送到语音通道，该函数只能在通话状态后调用，否则将引起系统异常。
     *       2、dtmfTone只能是TKL_TONE_DTMF_0 ~ TKL_TONE_DTMF_STAR中的一种。
     * @param dtmfTone 拨号按键
     * @param duration 音频持续时长
     * @return 0 成功
     *        -1 dtmfTone 类型错误
     *        -2 分配内存失败
     *    OPRT_TIMEOUT 获取信号量超时
     *    OPRT_BASE_OS_ADAPTER_REG_NULL_ERROR 函数未适配
     */
    OPERATE_RET (*dtmf2voice)(TUYA_TONE_TYPE_E dtmfTone, uint32_t duration);

    OPERATE_RET (*reg_KTDetect)(TKL_CELLULAR_CALL_KTDETECH_CB cb);

    OPERATE_RET (*ctrl_KTDetect)(BOOL_T enable);

} TKL_CELL_CALL_INTF_T;

/**
 * @brief register cellular call function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_CALL_INTF_T *tkl_cellular_call_desc_get();

/**
 * @brief 蜂窝模组数据服务API接口定义
 * TAL在实现tkl_cellular_mds_intf_t接口后，需要调用tal_cellular_mds_register
 * 把实现的对象注册到tkl中
 */
typedef struct {

    /**
     * @brief 初始化蜂窝移动数据服务
     * @param simId sim卡ID
     * @return 0 成功
     */
    OPERATE_RET (*mds_init)(uint8_t sim_id);

    /**
     * @brief 获取蜂窝移动数据服务的鉴权状态
     * @param simId
     * @return 蜂窝移动数据鉴权状态，查看 @TUYA_CELLULAR_MDS_STATUS_E 定义
     */
    TUYA_CELLULAR_MDS_STATUS_E (*get_mds_status)(uint8_t simId);

    /**
     * @brief 蜂窝移动数据PDP激活
     * @param simId
     * @param apn 运营商APN设置
     * @param username 用户名
     * @param password 密码
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*pdp_active)(uint8_t sim_id, PCHAR_T apn, PCHAR_T username, PCHAR_T password);

    /**
     * @brief 蜂窝移动数据指定CID PDP激活
     *
     * @param simId sim卡ID
     * @param cid Specify the PDP Context Identifier
     * @param apn 运营商APN设置
     * @param username 用户名
     * @param password 密码
     *
     * @return 0 成功 其它 失败
     */
    OPERATE_RET(*adv_pdp_active)
    (uint8_t sim_id, uint8_t cid, TUYA_MDS_PDP_TYPE_E pdp_type, PCHAR_T apn, PCHAR_T username, PCHAR_T password);
    /**
     * @brief 蜂窝移动数据PDP去激活
     * @param simId
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*pdp_deactive)(uint8_t sim_id);

    /**
     * @brief 蜂窝移动数据指定CID PDP去激活
     * @param simId
     * @param cid Specify the PDP Context Identifier
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*adv_pdp_deactive)(uint8_t sim_id, uint8_t cid);

    /**
     * @brief 是否开启PDP自动激活
     * @note 开启该功能后，PDP激活失败时，系统会自动尝试重新激活PDP，该项
     *       功能默认开启。
     * @param simId
     * @param enable TRUE 开启 FALSE 关闭
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*pdp_auto_reactive_enable)(uint8_t sim_id, BOOL_T enable);

    /**
     * @brief 设置蜂窝网络当前的连接状态的回调函数
     * @param simId
     * @param st tuya_mds_notfiy定义的回调函数指针
     * @return 0 成功 其它 失败
     */
    OPERATE_RET (*registr_mds_net_notify)(uint8_t sim_id, TKL_MDS_NOTIFY notify);

    /**
     * @brief   Get device ip address.
     * @param   ip: The type of NW_IP_S
     * @return  OPRT_OK: success  Other: fail
     */
    OPERATE_RET (*get_ip)(uint8_t sim_id, NW_IP_S *ip);

    /**
     * @brief   Get device ip address.
     * @param   ip: The type of NW_IP_S
     * @return  OPRT_OK: success  Other: fail
     */
    OPERATE_RET (*adv_get_ip)(uint8_t sim_id, uint8_t cid, NW_IP_S *ip);
} TKL_CELL_MDS_INTF_T;

/**
 * @brief register cellular module data service function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_MDS_INTF_T *tkl_cellular_mds_desc_get();

/**
 * @brief 蜂窝模组短信功能API接口定义
 */
typedef struct {
    /**
     * @brief 发送短信接口函数原型
     * @param simId sim卡ID
     * @param smsMsg 需要发送短信
     * @return  0 发送成功 其它 发送失败
     */
    OPERATE_RET (*send)(uint8_t sim_id, TUYA_CELLULAR_SMS_SEND_T *msg);

    /**
     * @brief 短信接收回调函数注册原型
     * @param callback 短信接收回调函数
     * @return 0 注册成功 其它 注册失败
     */
    OPERATE_RET (*recv_cb_register)(TUYA_CELLULAR_SMS_CB callback);

    /**
     * @brief 短信接收时静音设置原型
     * @param mute TRUE 静音 FALSE 接收短信时开启声音提示
     * @return 0 设置成功 其它 设置失败
     */
    OPERATE_RET (*sms_mute)(BOOL_T mute);

    void *(*convert_str)(const void *from, int from_size, TUYA_CELLULAR_SMS_ENCODE_E from_chset,
                         TUYA_CELLULAR_SMS_ENCODE_E to_chset, int *to_size);
} TKL_CELL_SMS_INTF_T;

/**
 * @brief register cellular short message function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_SMS_INTF_T *tkl_cellular_sms_desc_get();

/**
 * @brief 蜂窝模组电池API接口定义
 */
typedef struct {
    /**
     * @brief 获取电池剩余电量百分比
     * @param rsoc 剩余电量百分比
     * @return 1 获取成功 其它 获取失败
     */
    OPERATE_RET (*get_rsoc)(PUINT8_T rsoc);

    /**
     * @brief 设置是否开启NTC检测电池温度
     *
     * @param enable NTC检测电池温度开/关
     *
     * @return OPRT_OK 设置成功 其它 设置失败
     */
    OPERATE_RET (*ntc_enable)(BOOL_T enable);

    /**
     * @brief 设置恒流充电阶段，电池充电电流
     *
     * @param current 充电电流，单位毫安（mA)
     *
     * @return OPRT_OK 设置成功 其它 设置失败
     */
    OPERATE_RET (*set_charge_current)(uint32_t current);

    /**
     * @brief 获取充电器状态
     * @param 无
     * @return 充电器状态
     */
    TKL_CELLULAR_VBAT_CHG_STATE_E (*get_charger_state)(void);

    /**
     * @brief 注册电池充电器消息回调处理函数
     * @param callback 回调函数
     * @return 0 注册成功 其它 注册失败
     */
    OPERATE_RET (*charge_cb_register)(TKL_CELLULAR_VBAT_CHARGE_CB callback);

    /**
     * @brief 获取电池电压
     * @param voltage 当前电池电压，单位mV
     * @return OPRT_OK 获取成功 其它 获取失败
     */
    OPERATE_RET (*get_voltage)(uint32_t *voltage);

    OPERATE_RET (*low_volt_poweroff_enable)(BOOL_T enable);
} TKL_CELL_VBAT_INTF_T;

/**
 * @brief register cellular battery function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_VBAT_INTF_T *tkl_cellular_vbat_desc_get();

/**
 * @brief 蜂窝模组音频接口
 */
typedef struct {
    OPERATE_RET(*audio_play)
    (TKL_AUDIO_PLAYER_TYPE_E type, TKL_AUDIO_STREAM_FORMAT_E format, PCHAR_T file_path, TKL_AUDIO_PLAYER_CB cb,
     PVOID_T cb_ctx);
    OPERATE_RET (*audio_pause)(void);
    OPERATE_RET (*audio_resume)(void);
    OPERATE_RET (*audio_stop)(void);
    OPERATE_RET (*audio_set_vol)(int vol);
    OPERATE_RET (*audio_get_vol)(PINT_T vol);
    OPERATE_RET (*audio_set_mute)(BOOL_T mute);
    OPERATE_RET (*audio_get_mute)(PBOOL_T mute);
    OPERATE_RET(*audio_mem_play)
    (TKL_AUDIO_PLAYER_TYPE_E type, TKL_AUDIO_STREAM_FORMAT_E format, PBYTE_T buff, uint32_t size,
     TKL_AUDIO_PLAYER_CB cb, PVOID_T cb_ctx);
    OPERATE_RET (*audio_set_output)(TKL_AUDEV_OUTPUT_TYPE chan);
    OPERATE_RET (*audio_get_output)(TKL_AUDEV_OUTPUT_TYPE *chan);
    OPERATE_RET (*audio_set_input)(TKL_AUDEV_INPUT_TYPE chan);
    OPERATE_RET (*audio_get_input)(TKL_AUDEV_INPUT_TYPE *chan);
    OPERATE_RET (*audio_get_status)(TKL_AUDIO_PLAYER_STATUS_E *status);
    OPERATE_RET(*audio_play_stream)
    (TKL_AUDIO_PLAYER_TYPE_E type, const TKL_AUDIO_PLAY_OPS_T *playOps, PVOID_T playCtx,
     const TKL_AUDIO_FRAME_T *frame);
    OPERATE_RET (*audio_stop_stream)(void);
} TKL_CELL_PLAYER_INTF_T;

/**
 * @brief register cellular player function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_PLAYER_INTF_T *tkl_cellular_player_desc_get();

/**
 * @brief 蜂窝模组音频接口
 */
typedef struct {
    OPERATE_RET (*init)(void *param);
    TUYA_CELLULAR_KEY_LISTENER (*key_listener_add)(TUYA_KEYMAP_E keyId, TUYA_CELLULAR_KEY_CB cb, void *ctx);
    OPERATE_RET (*key_listener_delete)(TUYA_CELLULAR_KEY_LISTENER listener);
    OPERATE_RET (*key_state_get)(TUYA_KEYMAP_E keyId, TUYA_KEYSTATE_E *state);
    OPERATE_RET (*key_ioctl)(int cmd, void *argv);
} TKL_CELL_KEYPAD_INTF_T;

/**
 * @brief register cellular player function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_KEYPAD_INTF_T *tkl_cellular_keypad_desc_get();

#ifdef __cplusplus
} // extern "C"
#endif

#endif
