/**
 * @file tkl_init_nbiot.h
 * @brief Common process - tkl init nbiot description
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_INIT_NBIOT_H__
#define __TKL_INIT_NBIOT_H__

#include "tkl_nbiot_ctcc.h"
#include "tkl_nbiot_cmcc.h"
#include "tkl_nbiot_psm.h"
#include "tkl_nbiot_ril.h"
#include "tkl_nbiot_misc.h"
#include "tkl_nbiot_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // ctcc
    OPERATE_RET (*ctcc_lwm2m_register)(NBIOT_LWM2M_REGISTER_T *reg_params);
    OPERATE_RET (*ctcc_lwm2m_deregister)(void);
    OPERATE_RET (*ctcc_data_send)(uint8_t *data, uint32_t data_len);
    OPERATE_RET (*ctcc_lifetime_update)(uint32_t lifetime);
    // cmcc
    OPERATE_RET (*cmcc_register)(NBIOT_LWM2M_REGISTER_T *reg_params);
    OPERATE_RET (*cmcc_deregister)(void);
    OPERATE_RET (*cmcc_data_send)(uint8_t *data, uint32_t data_len);
    OPERATE_RET (*cmcc_lifetime_update)(uint32_t lifetime);
    // psm
    OPERATE_RET (*psm_plat_config)(void);
    OPERATE_RET (*psm_sleep_notify_reg)(TKL_NB_PSM_NOTIFY_CB_T cb);
    OPERATE_RET (*psm_create_sleeplock)(const PCHAR_T lock_name, uint8_t *handle);
    OPERATE_RET (*psm_acquire_sleeplock)(uint8_t handle);
    OPERATE_RET (*psm_release_sleeplock)(uint8_t handle);
    void (*psm_force_sleep)(void);
    OPERATE_RET (*psm_get_poweron_result)(TKL_NB_POWER_ON_RESULT_E *result);
    OPERATE_RET (*psm_get_wakeup_source)(TKL_NB_WAKEUP_SOURCE_E *src);
    OPERATE_RET (*psm_rtc_timer_create)(uint32_t time_period_sec, BOOL_T is_periodic, TKL_NB_SLP_TIMER_CB_T cb);
    OPERATE_RET (*psm_rtc_timer_start)(void);
    OPERATE_RET (*psm_rtc_timer_stop)(void);
    OPERATE_RET (*psm_rtc_timer_delete)(void);
    BOOL_T (*psm_usb_is_active)(void);
    OPERATE_RET (*psm_wakeup_pin_cfg)(int pin_num, TKL_NB_WAKEUP_CB wakeup_cb);
    // ril
    OPERATE_RET (*ril_init)(void);
    OPERATE_RET (*ril_register_event_callback)(TKL_RIL_EVENT_SEND_T evt_cb, TKL_RIL_EVENT_ID_T *evt_id);
    OPERATE_RET (*ril_attach_event_inquire)(void);
    BOOL_T (*ril_is_sim_ready)(void);
    OPERATE_RET (*ril_get_cesq)(TKL_RIL_CESQ_T *info);
    OPERATE_RET (*ril_get_t3324)(uint32_t *t3324);
    OPERATE_RET (*ril_get_t3412)(uint32_t *t3412);
    OPERATE_RET (*ril_set_t3412)(uint32_t req_time);
    OPERATE_RET (*ril_set_t3324)(uint32_t req_time);
    OPERATE_RET (*ril_set_apn)(PCHAR_T apn_name, PCHAR_T pdp_type);
    OPERATE_RET (*ril_get_apn)(PCHAR_T apn);
    OPERATE_RET (*ril_get_rssi)(int *out_rssi);
    OPERATE_RET (*ril_get_imsi)(PCHAR_T imsi);
    OPERATE_RET (*ril_get_iccid)(PCHAR_T iccid);
    OPERATE_RET (*ril_get_plmn_info)(TKL_RIL_PLMN_T *plmn_info);
    OPERATE_RET (*ril_set_cfun)(uint8_t val);
    OPERATE_RET (*ril_get_cfun)(int *val);
    OPERATE_RET (*ril_get_rf_flag)(int *flag);
    OPERATE_RET (*ril_set_imei)(PCHAR_T imei);
    OPERATE_RET (*ril_get_imei)(PCHAR_T buf, uint8_t len);
    OPERATE_RET (*ril_release_rrc)(void);
    OPERATE_RET (*ril_set_area_attach_info)(TKL_RIL_ATTACH_INFO_T *operator);
    OPERATE_RET (*ril_set_band)(PCHAR_T band_array_str);
    OPERATE_RET (*ril_get_band)(PCHAR_T pbuf, uint8_t len);
    OPERATE_RET (*ril_psm_mode_enable)(uint8_t enable);
    OPERATE_RET (*ril_edrx_mode_enable)(uint8_t enable);
    OPERATE_RET (*ril_get_serving_cellid)(PCHAR_T pbuf, uint8_t len);
    OPERATE_RET (*ril_get_neightbor_cellid)(TKL_RIL_NEIGHBOR_CELL_INFO_T **pbuf, int *out_num);
    // misc
    OPERATE_RET (*misc_chip_type)(TKL_MISC_CHIP_TYPE_E *out_type);
    BOOL_T (*misc_is_atcmd_serial_port_on)(void);
    OPERATE_RET (*misc_atcmd_serial_port_off)(void);
    OPERATE_RET (*misc_atcmd_serial_port_on)(void);
    OPERATE_RET (*misc_get_app_offset)(uint32_t *out_addr);
    OPERATE_RET (*misc_get_app_len)(uint32_t *out_len);
    OPERATE_RET (*misc_fota_update_start)(void);
    OPERATE_RET (*misc_fota_status_acquire)(void);
    OPERATE_RET (*misc_fota_update_end)(void);
    OPERATE_RET (*misc_get_fota_offset)(uint32_t *out_addr, uint8_t *_1st_data, uint32_t _1st_data_size);
    OPERATE_RET (*misc_get_fota_len)(uint32_t *out_len);
    OPERATE_RET (*misc_get_fota_update_info_offset)(uint32_t *out_addr);
    OPERATE_RET (*misc_get_fota_trigger_flag_offset)(uint32_t *out_addr);
} TKL_NBIOT_DESC_T;

/**
 * @brief register gpio description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_NBIOT_DESC_T *tkl_nbiot_desc_get(void);

#ifdef __cplusplus
}
#endif

#endif
