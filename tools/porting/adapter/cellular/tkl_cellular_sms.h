/**
 * @file tkl_cellular_sms.h
 * @author www.tuya.com
 * @brief Cellular module SMS API implementation interface.
 *
 * @copyright Copyright (c) tuya.inc 2021
 */

#ifndef __TKL_CELLULAR_SMS_H__
#define __TKL_CELLULAR_SMS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum length of phone number when sending and receiving SMS
 */
#define TUYA_SMS_PHONENUM_LEN_MAX    24

/**
 * @brief Maximum length of SMS content
 */
#define TUYA_SMS_MSG_LEN_MAX    160

/**
 * @brief SMS content encoding format
 */
typedef enum{
    TUYA_SMS_ISO8859_1,
    TUYA_SMS_UTF8,
    TUYA_SMS_UTF16BE,
    TUYA_SMS_UTF16LE,
    TUYA_SMS_GSM,
    TUYA_SMS_CP936   //GBK encoding
} TUYA_CELLULAR_SMS_ENCODE_E;

/**
 * @brief Send SMS structure definition
 */
typedef struct
{
    char phone[TUYA_SMS_PHONENUM_LEN_MAX];  /*< Phone number to receive SMS */
    uint16_t msg_len;                        /*< SMS length */
    uint16_t msg[TUYA_SMS_MSG_LEN_MAX];     /*< SMS content */
    TUYA_CELLULAR_SMS_ENCODE_E   sms_encode;
} TUYA_CELLULAR_SMS_SEND_T;

/**
 * @brief SMS timestamp information structure definition
 */
typedef struct
{
    uint16_t year;  /*< Year */
    uint8_t month;  /*< Month */
    uint8_t day;    /*< Day */
    uint8_t hour;   /*< Hour */
    uint8_t minute; /*< Minute */
    uint8_t second; /*< Second */
    uint8_t zone;   /*< Time zone */
} TUYA_CELLULAR_SMS_TIMESTAMP_T;

/**
 * @brief Receive SMS structure definition
 */
typedef struct
{
    char phone[TUYA_SMS_PHONENUM_LEN_MAX];  /*< Phone number that sent the SMS */
    TUYA_CELLULAR_SMS_TIMESTAMP_T  date;       /*< SMS receive time */
    int msg_len;                             /*< SMS content length */
    char msg[TUYA_SMS_MSG_LEN_MAX];          /*< SMS content */
} TUYA_CELLULAR_SMS_RECV_T;

/**
 * @brief SMS receive callback function interface prototype
 * @note Do not call blocking functions in the callback function. The msg structure memory is
 *       allocated and released by the system.
 * @param simId SIM card ID
 * @param msg SMS structure
 * @return None
 */
typedef void (*TUYA_CELLULAR_SMS_CB)(uint8_t sim_id, TUYA_CELLULAR_SMS_RECV_T* msg);

/**
 * @brief Send SMS
 *
 * @param simId SIM card ID
 * @param smsMsg SMS to send
 *
 * @return  0 send success, others send failure
 */
OPERATE_RET tkl_cellular_sms_send(uint8_t sim_id, TUYA_CELLULAR_SMS_SEND_T* sms_msg);

/**
 * @brief Register SMS receive callback function
 * @note This function needs to be registered before SIM card activation, otherwise it may cause exceptions or SMS loss.
 *
 * @param callback SMS receive callback function
 *
 * @return  0 registration success, others registration failure
 */
OPERATE_RET tkl_cellular_sms_recv_cb_register(TUYA_CELLULAR_SMS_CB callback);

/**
 * @brief Set SMS receive mute
 * @note By default, when receiving SMS, a notification sound will be played through the speaker. Through this function, you can set
 *       to not play notification sound when receiving SMS.
 *
 * @param mute TRUE mute, FALSE enable notification sound when receiving SMS
 *
 * @return 0 setting success, others setting failure
 */
OPERATE_RET tkl_cellular_sms_mute(bool mute);

/**
 * SMS content encoding conversion
 *
 * When \a from_charset or \a to_charset is unknown or unsupported,
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
void *tkl_cellular_sms_convert_str(const void *from, int from_size,
    TUYA_CELLULAR_SMS_ENCODE_E from_chset, TUYA_CELLULAR_SMS_ENCODE_E to_chset, int *to_size);


/**
 * @brief Set default SMS receive encoding
 * @param chset TUYA_CELLULAR_SMS_ENCODE_E type
 *
 * @return 0 setting success, others setting failure
 */
OPERATE_RET tkl_cellular_sms_set_charactor(TUYA_CELLULAR_SMS_ENCODE_E chset);

#ifdef __cplusplus
}
#endif

#endif
