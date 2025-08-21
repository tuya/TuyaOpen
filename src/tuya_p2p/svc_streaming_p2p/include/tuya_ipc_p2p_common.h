/**
 * @ tuya_ipc_p2p_common.h
 * @brief p2p common define
 * @version 0.1
 * @date 2021-11-17
 *
 * @copyright Copyright (c) tuya.inc 2011
 *
 */

#ifndef __TUYA_IPC_P2P_COMMON_H__
#define __TUYA_IPC_P2P_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_common_types.h"

#define P2P_ID_LEN           25 /**P2P ID MAX LEN*/
#define P2P_NAME_LEN         8  /**P2P NAME LEN*/
#define P2P_PASSWD_LEN       8  /**P2P PASSWORD LEN*/
#define P2P_GW_LOCAL_KEY_LEN 16 /**GW KEY MAX LEN*/
#define P2P_TYPE_LEN         8  /**P2P TYPE MAX LEN*/
#define TUYA_P2P             4

/**
 * @struct TUYA_IPC_P2P_AUTH_T
 *
 * @brief p2p auth info
 */
typedef struct {
    CHAR_T p2p_id[P2P_ID_LEN + 1];                 /** p2p id*/
    CHAR_T p2p_name[P2P_NAME_LEN + 1];             /** p2p name*/
    CHAR_T p2p_passwd[P2P_PASSWD_LEN + 1];         /** p2p auth passeord*/
    CHAR_T gw_local_key[P2P_GW_LOCAL_KEY_LEN + 1]; /** p2p auth key*/
    VOID *p_reserved;                              /** reserved ptr*/
} TUYA_IPC_P2P_AUTH_T;

/**
 * @enum TRANSFER_AUDIO_SAMPLE_E
 *
 * @brief audio sample
 */
typedef enum {
    TY_AUDIO_SAMPLE_8K,  /** audio sample 8K*/
    TY_AUDIO_SAMPLE_11K, /** audio sample 11K*/
    TY_AUDIO_SAMPLE_12K, /** audio sample 12K*/
    TY_AUDIO_SAMPLE_16K, /** audio sample 16K*/
    TY_AUDIO_SAMPLE_22K, /** audio sample 22K*/
    TY_AUDIO_SAMPLE_24K, /** audio sample 24K*/
    TY_AUDIO_SAMPLE_32K, /** audio sample 32K*/
    TY_AUDIO_SAMPLE_44K, /** audio sample 44K*/
    TY_AUDIO_SAMPLE_48K, /** audio sample 48K*/
    TY_AUDIO_SAMPLE_96K, /** audio sample 96K*/
} TRANSFER_AUDIO_SAMPLE_E;

/**
 * @enum TRANSFER_AUDIO_DATABITS_E
 *
 * @brief audio databit
 */
typedef enum {
    TY_AUDIO_DATABITS_8,  /** 8 databit*/
    TY_AUDIO_DATABITS_16, /** 16 databit*/
} TRANSFER_AUDIO_DATABITS_E;

/**
 * @enum TRANSFER_AUDIO_CHANNEL_E
 *
 * @brief audio track
 */
typedef enum {
    TY_AUDIO_CHANNEL_MONO,
    TY_AUDIO_CHANNEL_STERO,
} TRANSFER_AUDIO_CHANNEL_E;

#define P2P_SESSION_DETECH_INTV  (10000)  /**session check time interval (ms)*/
#define P2P_SESSION_DETECH_COUNT (120)    /**session check cnt*/
#define P2P_LOGIN_DETECH_CNT     (180000) /**login checkout time interval*/

/**
 * @structP2P_SESSION_DETECH_T
 *
 * @brief debug cnt
 */
typedef struct {
    UINT_T lstCnt;    /** send cnt*/
    UINT_T lstCnt2;   /** recv cnt*/
    UINT_T staticCnt; /** detech cnt*/
} P2P_SESSION_DETECH_T;

/**
* @brief get p2p id
*
* @param[out] p2p_id:p2p id

* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tuya_ipc_p2p_get_id(INOUT CHAR_T p2p_id[]);

/**
* @brief check p2p auth update
*
* @param (VOID)

* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tuya_ipc_check_p2p_auth_update(VOID);

/**
* @brief get p2p auth info
*
* @param[out] pAuth:p2p auth info

* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tuya_ipc_get_p2p_auth(TUYA_IPC_P2P_AUTH_T *pAuth);

/**
* @brief get p2p auth info
*
* @param[out] p2p_pw:p2p password

* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tuya_ipc_p2p_update_pw(INOUT CHAR_T p2p_pw[]);

/**
 * @brief p2p log report
 *
 * @param[in] devid:device id
 * @param[in] pData:log data
 * @param[in] len:data len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET mqc_p2p_data_rept_v41(IN CONST CHAR_T *devid, IN CONST CHAR_T *pData, IN CONST INT_T len);

/**
 * @brief iot reset config callback
 *
 * @param[in] rst_tp:reset tp
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
INT_T iot_gw_reset_cb(VOID *rst_tp);

#ifdef __cplusplus
}
#endif

#endif
