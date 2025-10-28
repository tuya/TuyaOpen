/**
* @file tuya_key.h
* @brief TUYA KEY interface
* @version 0.1
* @date 2017-11-17
*
* @copyright Copyright 2019-2021 Tuya Inc. All Rights Reserved.
*/

#ifndef __TUYA_KEY_H
#define __TUYA_KEY_H

#include <stdint.h>
#include "tuya_cloud_types.h"


#ifdef __cplusplus
	extern "C" {
#endif

/**
 * @brief Definition of KEY type
 */
typedef enum {
    /** one shot */
    NORMAL_KEY = 0,
    /** continual shot */
    SEQ_KEY,
    /** long press */
    LONG_KEY,

    RELEASE_KEY,
}PUSH_KEY_TYPE_E;

/**
 * Definitioin of long pross type
 */
typedef enum {
    /** long press invalid */
    LP_INVALID = 0,
    /** long press once trigger */
    LP_ONCE_TRIG,
    /** long press more normal trigger */
    LP_MORE_NORMAL_TRIG,
    /** press key immedialtely trigger */
    FALLING_EDGE_TRIG,
    /** press key immedialtely trigger & LONG */
    FALLING_LONG_TRIG,
}KEY_LONG_PRESS_TP_E;

/**
 * @brief Handler of KEY event
 * 
 * @param[in] port PIN ID that got the event
 * @param[in] type Press type, see PUSH_KEY_TYPE_E
 * @param[in] cnt Count of continual. 
 * 
 * @note (cnt >= 2) ==> SEQ_KEY,
 * time < long_key_time && (cnt == 1) ==> NORMAL_KEY,
 * time >= long_key_time && (cnt == 1) ==> LONG_KEY
 */
typedef VOID(* KEY_CALLBACK)(UINT_T port,PUSH_KEY_TYPE_E type,INT_T cnt);

/**
 * @brief Definition of user defined key
 */
typedef struct {
    /** PIN ID */
    UINT_T port;
    /** whether low level trigger */
    BOOL_T low_level_detect;
    /** long press type, see KEY_LONG_PRESS_TP_E */
    KEY_LONG_PRESS_TP_E lp_tp;
    /** unit: ms, lp_tp == LP_ONCE_TRIG then valid and must >= 1000ms */
    USHORT_T long_key_time;
    /** unit:ms , 0:disable default:400ms */
    USHORT_T seq_key_detect_time;
    /** handler of KEY event */
    KEY_CALLBACK call_back;
}KEY_USER_DEF_S;

/**
 * @brief Register a new KEY
 * 
 * @param[in] key_ud User defined key, see KEY_USER_DEF_S
 * 
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET reg_proc_key(IN CONST KEY_USER_DEF_S *key_ud);

/**
 * @brief Initialize KEY process service
 * 
 * @param[in] p_tbl User defined key tables, see KEY_USER_DEF_S
 * @param[in] cnt Count of key tables
 * @param[in] timer_space Key detect interval (unit ms). No more than 100,
 * if 0 then use default value(20ms)
 * 
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET key_init(IN CONST KEY_USER_DEF_S *p_tbl,IN CONST INT_T cnt,\
                         IN CONST INT_T timer_space);

#ifdef __cplusplus
}
#endif

#endif
