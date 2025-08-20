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
#include "tkl_cellular_keypad.h"
#include "tkl_cellular_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cellular module basic service API interface definition
 * After TAL implements the tkl_CellularSvcIntf_t interface, it needs to call tal_cellular_mds_register
 * to register the implemented object to tkl
 */
typedef struct {
    OPERATE_RET (*base_init)(TKL_CELL_INIT_PARAM_T *param);
    OPERATE_RET (*get_ability)(TKL_CELLULAR_ABILITY_E *ability);
    OPERATE_RET (*switch_sim)(uint8_t sim_id);
    OPERATE_RET (*register_sim_state_notify)(uint8_t simd_id, TKL_SIM_NOTIFY notify);
    OPERATE_RET (*sim_hotplug_enable)(uint8_t sim_id, bool enable);
    OPERATE_RET (*sim_get_status)(uint8_t sim_id, uint8_t *state);
    OPERATE_RET (*get_cfun_mode)(uint8_t sim_id, int *cfun);
    OPERATE_RET (*set_cfun_mode)(uint8_t sim_id, int cfun);
    OPERATE_RET (*get_imsi)(uint8_t sim_id, char imsi[15 + 1]);
    OPERATE_RET (*get_iccid)(uint8_t sim_id, char iccid[20 + 1]);
    OPERATE_RET (*get_imei)(uint8_t sim_id, char imei[15 + 1]);
    OPERATE_RET (*set_imei)(uint8_t sim_id, char imei[15 + 1]);
    OPERATE_RET (*get_rsrp)(uint8_t sim_id, int *rsrp);
    OPERATE_RET (*get_sinr)(uint8_t sim_id, int *sinr, int *bit_error);
    OPERATE_RET (*get_lbs)(uint8_t simid, TKL_LBS_INFO_T *lbs, bool neighbour, int timeout);
    bool (*rf_calibrated)(void);
    OPERATE_RET (*enable_sim_detect)(uint8_t simid, bool enable);
    int8_t (*get_default_simid)(void);
    OPERATE_RET (*ioctl)(int cmd, void *argv);
    OPERATE_RET (*get_epoch_sec)(uint64_t *epoch_sec);
    OPERATE_RET (*get_rssidbm)(uint8_t sim_id, int *rssi);
    OPERATE_RET (*get_rssi)(uint8_t sim_id, int *rssi);
    OPERATE_RET (*get_nettype)(uint8_t sim_id, TUYA_CELLULAR_RAT_E *net_type);
    // OPERATE_RET (*get_data_statics)(uint8_t cid, uint32_t *up, uint32_t *down);
    OPERATE_RET (*get_localtime)(struct tm *local_tm);
    OPERATE_RET (*get_timezone)(int *timezone);
    OPERATE_RET (*get_sn)(char sn[25]);
    OPERATE_RET (*register_dev_reg_notify)(uint8_t simd_id, TKL_REGISTION_NOTIFY fun);
} TKL_CELL_BASE_INTF_T;

/**
 * @brief register cellular base function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_BASE_INTF_T *tkl_cellular_base_desc_get();

/**
 * @brief Cellular module phone dialing API interface definition
 */
typedef struct {
    OPERATE_RET (*call_service_available)(uint8_t sim_id, bool *enable);
    OPERATE_RET (*call)(uint8_t sim_id, char call_number[TKL_CELLULAR_CALLNUM_LEN_MAX]);
    OPERATE_RET (*answer)(uint8_t sim_id);
    OPERATE_RET (*hungup)(uint8_t sim_id);
    OPERATE_RET (*cb_register)(TKL_CELLULAR_CALL_CB callback);
    OPERATE_RET (*set_callin_mute)(bool mute);
    OPERATE_RET (*set_volte)(uint8_t sim_id, bool enable);
    OPERATE_RET (*set_voice_mute)(bool mute);
    OPERATE_RET (*get_voice_mute)(bool *mute);
    OPERATE_RET (*set_voice_vol)(int vol);
    OPERATE_RET (*get_voice_vol)(int *vol);
    OPERATE_RET (*play_tone)(TUYA_TONE_TYPE_E tone, int duration);
    OPERATE_RET (*stop_tone)(void);
    OPERATE_RET (*dtmf2voice)(TUYA_TONE_TYPE_E dtmfTone, uint32_t duration);
    OPERATE_RET (*reg_KTDetect)(TKL_CELLULAR_CALL_KTDETECH_CB cb);
    OPERATE_RET (*ctrl_KTDetect)(bool enable);
    OPERATE_RET (*ioctl)(int cmd, void *arg);
} TKL_CELL_CALL_INTF_T;

/**
 * @brief register cellular call function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_CALL_INTF_T *tkl_cellular_call_desc_get();

/**
 * @brief Cellular module data service API interface definition
 * After TAL implements the tkl_cellular_mds_intf_t interface, it needs to call tal_cellular_mds_register
 * to register the implemented object to tkl
 */
typedef struct {
    OPERATE_RET (*mds_init)(uint8_t sim_id);
    TUYA_CELLULAR_MDS_STATUS_E (*get_mds_status)(uint8_t simId);
    OPERATE_RET (*pdp_active)(uint8_t sim_id, char *apn, char *username, char *password);
    OPERATE_RET (*adv_pdp_active)(uint8_t sim_id, uint8_t cid, TUYA_MDS_PDP_TYPE_E pdp_type, char *apn, char *username,
                                  char *password);
    OPERATE_RET (*pdp_deactive)(uint8_t sim_id);
    OPERATE_RET (*adv_pdp_deactive)(uint8_t sim_id, uint8_t cid);
    OPERATE_RET (*pdp_auto_reactive_enable)(uint8_t sim_id, bool enable);
    OPERATE_RET (*registr_mds_net_notify)(uint8_t sm_id, TKL_MDS_NOTIFY notify);
    OPERATE_RET (*get_ip)(uint8_t sim_id, NW_IP_S *ip);
} TKL_CELL_MDS_INTF_T;

/**
 * @brief register cellular module data service function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_MDS_INTF_T *tkl_cellular_mds_desc_get();

/**
 * @brief Cellular module SMS function API interface definition
 */
typedef struct {
    OPERATE_RET (*send)(uint8_t sim_id, TUYA_CELLULAR_SMS_SEND_T *msg);
    OPERATE_RET (*recv_cb_register)(TUYA_CELLULAR_SMS_CB callback);
    OPERATE_RET (*sms_mute)(bool mute);
    void *(*convert_str)(const void *from, int from_size, TUYA_CELLULAR_SMS_ENCODE_E from_chset,
                         TUYA_CELLULAR_SMS_ENCODE_E to_chset, int *to_size);
    OPERATE_RET (*set_charactor)(TUYA_CELLULAR_SMS_ENCODE_E chset);
} TKL_CELL_SMS_INTF_T;

/**
 * @brief register cellular short message function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_SMS_INTF_T *tkl_cellular_sms_desc_get();

/**
 * @brief Cellular module battery API interface definition
 */
typedef struct {
    OPERATE_RET (*get_rsoc)(uint8_t *rsoc);
    OPERATE_RET (*ntc_enable)(bool enable);
    OPERATE_RET (*set_charge_current)(uint32_t current);
    TKL_CELLULAR_VBAT_CHG_STATE_E (*get_charger_state)(void);
    OPERATE_RET (*charge_cb_register)(TKL_CELLULAR_VBAT_CHARGE_CB callback);
    OPERATE_RET (*get_voltage)(uint32_t *voltage);
    OPERATE_RET (*low_volt_poweroff_enable)(bool enable);
} TKL_CELL_VBAT_INTF_T;

/**
 * @brief register cellular battery function description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_CELL_VBAT_INTF_T *tkl_cellular_vbat_desc_get();

/**
 * @brief Cellular module audio interface
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
