/**
 * @file tal_cellular_sms.h
 * @author www.tuya.com
 * @brief 蜂窝模组短信服务API定义。
 *
 * @copyright Copyright (c) tuya.inc 2021
 */

#ifndef __TAL_CELLULAR_SMS_H__
#define __TAL_CELLULAR_SMS_H__

#include "tuya_cloud_types.h"

#if !defined(ENABLE_CELLULAR_PLUGIN) || ENABLE_CELLULAR_PLUGIN == 0
#include "tkl_cellular_base.h"
#include "tkl_cellular_sms.h"
#else
// 包含at版本的蜂窝接口头文件
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 发送短信
 *
 * @param simId sim卡ID
 * @param smsMsg 发送的短信
 * @return  0 发送成功 其它 发送失败
 */
OPERATE_RET tal_cellular_sms_send(uint8_t simId, TUYA_CELLULAR_SMS_SEND_T* smsMsg);

/**
 * @brief 注册短信接收回调函数
 * @note 该函数需在SIM卡激活前注册，否则可能引起异常或短信丢失。
 * @param callback 短信接收回调函数
 * @return  0 注册成功 其它 注册失败
 */
OPERATE_RET tal_cellular_sms_recv_cb_register(TUYA_CELLULAR_SMS_CB callback);

/**
 * @brief 设置短信接收时静音
 * @note 默认情况下，收到短信时，会通过扬声器播放提示音。通过该函数，可设置接收
 *       短信时，不播放提示音。
 * @param mute TRUE 静音 FALSE 短信接收时打开提示音
 * @return 0 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_sms_mute(bool mute);

/**
 * @brief 短信内容编码转换
 *
 * @note When \a from_charset or \a to_charset is unknown or unsupported,
 * return NULL.
 * The returned pointer is allocated inside, caller should free it.
 *
 * At most \a from_size byte count will be processed. When \a from_size
 * is -1, process till null chracter.
 *
 * null character will always be inserted into the output string.
 * Invalid characters are replaced silently.
 *
 * \a to_size can be NULL in case that the output byte count is not needed.
 *
 * @param from          input string
 * @param from_size     input string byte count
 * @param from_chset    input string charset
 * @param to_chset      output string charset
 * @param to_size       output string byte count
 * @return
 *      - NULL: invalid parameters
 *      - output string
 */
void *tal_cellular_sms_convert_str(const void *from, int from_size, TUYA_CELLULAR_SMS_ENCODE_E from_chset, TUYA_CELLULAR_SMS_ENCODE_E to_chset, int *to_size);

/**
 * @brief 设置短信默认接收短信编码
 * @param chset TUYA_CELLULAR_SMS_ENCODE_E类型
 * @return 0 设置成功 其它 设置失败
 */
OPERATE_RET tal_cellular_sms_set_charactor(TUYA_CELLULAR_SMS_ENCODE_E chset);

#ifdef __cplusplus
}
#endif

#endif
