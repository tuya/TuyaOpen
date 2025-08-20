
#include "tal_cellular_call.h"
#include "tkl_init_cellular.h"

/**
 * @brief 查询呼叫服务是否可用
 * @note 该函数用于查询呼叫服务是否可用，只有在呼叫服务可用时，才能使用使用
 *       其它呼叫服务接口。
 * @param simId sim卡ID
 * @param enalbe 呼叫服务是否可用
 * @return 0 成功，其他失败
 */
OPERATE_RET tal_cellular_call_service_available(uint8_t sim_id, bool *enable)
{
    if (tkl_cellular_call_desc_get()->call_service_available == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->call_service_available(sim_id, enable);
}

/**
 * @brief 呼叫拨号接口函数
 * @param simId sim卡ID号
 * @param callNUm 呼叫号码，字符串形式
 * @return  0 发起呼叫成功 其它 失败
 */
OPERATE_RET tal_cellular_call(uint8_t sim_id, char callNum[TKL_CELLULAR_CALLNUM_LEN_MAX])
{
    if(tkl_cellular_call_desc_get()->call == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->call(sim_id, callNum);
}

/**
 * @brief 外部呼叫到来时，应答接听接口函数
 * @return   0 应答成功 其它 失败
 */
OPERATE_RET tal_cellular_call_answer(uint8_t sim_id)
{
    if(tkl_cellular_call_desc_get()->answer == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->answer(sim_id);
}

/**
 * @brief 呼叫通话后，挂机接口函数
 * @param 无
 * @return  0 挂机成功 其它 失败
 */
OPERATE_RET tal_cellular_call_hungup(uint8_t sim_id)
{
    if(tkl_cellular_call_desc_get()->hungup == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->hungup(sim_id);
}

/**
 * @brief 注册用户定义的呼叫回调处理函数
 * @param callback 呼叫回调处理函数
 * @return 0 注册成功 其它 注册失败
 */
OPERATE_RET tal_cellular_call_cb_register(TKL_CELLULAR_CALL_CB callback)
{
    if(tkl_cellular_call_desc_get()->cb_register == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->cb_register(callback);
}

/**
 * @brief 设置呼入时铃声静音
 * @param mute TRUE 静音 FALSE 非静音
 * @return 0 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_call_set_callin_mute(bool mute)
{
    if(tkl_cellular_call_desc_get()->set_callin_mute == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->set_callin_mute(mute);
}

/**
 * @brief 设置通话功能
 * @param simId sim卡ID号
 * @param enable TRUE 使能 FALSE 禁止
 * @return 0 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_call_set_volte(uint8_t simId, bool enable)
{
    if(tkl_cellular_call_desc_get()->set_volte == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->set_volte(simId, enable);
}

/**
 * @brief 设置通话时声音静音
 * @param mute TRUE 静音 FALSE 非静音
 * @return 0 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_call_set_voice_mute(bool mute)
{
    if(tkl_cellular_call_desc_get()->set_voice_mute == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->set_voice_mute(mute);
}

/**
 * @brief 获取通话时声音静音状态
 * @param mute TRUE 静音 FALSE 非静音
 * @return 0 获取成功 其它 获取失败
 */
OPERATE_RET tal_cellular_call_get_voice_mute(bool *mute)
{
    if(tkl_cellular_call_desc_get()->get_voice_mute == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->get_voice_mute(mute);
}

/**
 * @brief 设置通话音量
 * @param vol 音量值
 * @return 0 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_call_set_volume(int vol)
{
    if(tkl_cellular_call_desc_get()->set_voice_vol == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->set_voice_vol(vol);
}

/**
 * @brief 获取通话音量
 * @param vol 音量值
 * @return 0 获取成功 其它 获取失败
 */
OPERATE_RET tal_cellular_call_get_volume(int *vol)
{
    if(tkl_cellular_call_desc_get()->get_voice_vol == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->get_voice_vol(vol);
}

/**
 * @brief 播放通话提示音
 * @param tone 提示音类型
 * @param duration 提示音持续时间
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_call_play_tone(TUYA_TONE_TYPE_E tone, int duration)
{
    if(tkl_cellular_call_desc_get()->play_tone == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->play_tone(tone, duration);
}

/**
 * @brief 停止电话的拨号音
 * @param 无
 * @return 0 成功，其他失败
 */
OPERATE_RET tal_cellular_call_stop_tone(void)
{
    if(tkl_cellular_call_desc_get()->stop_tone == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->stop_tone();
}

/**
 * @brief 将DTMF数字按键音频发送到语音通道
 * @note 1、该函数用于实现10086之类的语音交互，语音提示后，用户操作按键，调用该函数
 *       将按键音频发送到语音通道，该函数只能在通话状态后调用，否则将引起系统异常。
 *       2、dtmfTone只能是TKL_TONE_DTMF_0 ~ TKL_TONE_DTMF_STAR中的一种。
 *
 * @param dtmfTone 拨号按键
 * @param duration 音频持续时长
 *
 * @return 0 成功
 *        -1 dtmfTone 类型错误
 *        -2 分配内存失败
 *    OPRT_TIMEOUT 获取信号量超时
 *    OPRT_BASE_OS_ADAPTER_REG_NULL_ERROR 函数未适配
 */
OPERATE_RET tal_cellular_call_dtmf2voice(TUYA_TONE_TYPE_E dtmfTone, uint32_t duration)
{
    if(tkl_cellular_call_desc_get()->dtmf2voice == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->dtmf2voice(dtmfTone, duration);
}

/**
 * @brief 注册DMTF侦测回调函数
 * @param cb :  回调函数
 * @return 0 成功，其他失败
 */
OPERATE_RET tal_cellular_call_reg_dtmfdetect(TKL_CELLULAR_CALL_KTDETECH_CB cb)
{
    if(tkl_cellular_call_desc_get()->reg_KTDetect == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->reg_KTDetect(cb);
}

/**
 * @brief 控制DTMF侦测功能是否使能，系统默认未使能。（开启这个功能，音频相关业务过程中，会增加系统负荷）
 * @param enable : TRUE 使能 FALSE 禁止
 * @return 0 成功，其他失败
 */
OPERATE_RET tal_cellular_call_ctrl_dtmfdetect(bool enable)
{
    if (NULL == tkl_cellular_call_desc_get()) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->ctrl_KTDetect(enable);
}

/**
 * @brief 呼叫服务ioctl接口
 * @param cmd 命令
 * @param argv 参数
 * @return 0 成功，其他失败
 */
OPERATE_RET tal_cellular_call_ioctl(int cmd, void *argv)
{
    if(tkl_cellular_call_desc_get()->ioctl == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return tkl_cellular_call_desc_get()->ioctl(cmd, argv);
}

