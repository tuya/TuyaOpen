/**
 * @file wifi_backup_internal.h
 * @brief Internal definitions shared between wifi_backup_fsm.c and wifi_backup_proto.c.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __WIFI_BACKUP_INTERNAL_H__
#define __WIFI_BACKUP_INTERNAL_H__

#include "tuya_wifi_backup.h"
#include "tal_api.h"
#include "tal_sw_timer.h"
#include "tal_workq_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define WIFI_BACKUP_KV_KEY    "wifi_bak"
#define WIFI_BACKUP_TID_LEN   32
#define WIFI_BACKUP_DEVID_LEN 32

#define WIFI_BACKUP_STATUS_IDLE    0
#define WIFI_BACKUP_STATUS_SWITCH  1
#define WIFI_BACKUP_STATUS_CONNECT 2

#define WIFI_BACKUP_AP_EXIST_FLAG 1
#define WIFI_BACKUP_AP_PRIOR_FLAG 2

#define WIFI_BACKUP_SWITCH_INTERVAL_MS  1000
#define WIFI_BACKUP_CONNECT_INTERVAL_MS 2000
#define WIFI_BACKUP_SWITCH_TIMEOUT_MS   (45 * 1000)
#define WIFI_BACKUP_CONNECT_TIMEOUT_MS  (35 * 1000)

#define WIFI_BACKUP_LOCAL_RECONNECT_TIME 60
#define WIFI_BACKUP_CLOUD_RECONNECT_TIME 300

#define WIFI_BACKUP_OFFLINE_TICK_MS 1000

#define WIFI_BACKUP_B64_LEN(n) ((((n) / 3 + ((n) % 3 != 0)) * 4) + 1)
#define WIFI_BACKUP_HASH_LEN   32
#define WIFI_BACKUP_PUBKEY_LEN 65

#define WIFI_BACKUP_FSM_STATE_INIT  0
#define WIFI_BACKUP_FSM_STATE_CHECK 1
#define WIFI_BACKUP_FSM_STATE_STOP  2
#define WIFI_BACKUP_FSM_STATE_DONE  3

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char    ssid[WIFI_SSID_LEN + 1];
    char    passwd[WIFI_PASSWD_LEN + 1];
    uint8_t hash[WIFI_BACKUP_HASH_LEN];
    int8_t  rssi;
    uint8_t flag;
} wifi_backup_ap_t;

typedef struct {
    uint8_t                  state;
    uint32_t                 timeout;
    uint8_t                  recover;
    uint8_t                  retry;
    uint32_t                 fail_seq;
    tuya_wifi_backup_trans_t trans;
    wifi_backup_ap_t         ap;
    char                     tid[WIFI_BACKUP_TID_LEN + 1];
    char                     devid[WIFI_BACKUP_DEVID_LEN + 1];
} wifi_backup_switch_fsm_t;

typedef struct {
    uint8_t          state;
    uint32_t         timeout;
    uint8_t          index;
    uint8_t          count;
    wifi_backup_ap_t ap[TUYA_WIFI_BACKUP_AP_NUM];
} wifi_backup_connect_fsm_t;

typedef struct {
    MUTEX_HANDLE              lock;
    bool                      inited;
    uint8_t                   status;
    uint8_t                   count;
    wifi_backup_ap_t          list[TUYA_WIFI_BACKUP_AP_NUM];
    uint8_t                   ecdh_secret[WIFI_BACKUP_HASH_LEN];
    bool                      ecdh_ready;
    char                      orig_ssid[WIFI_SSID_LEN + 1];
    char                      orig_passwd[WIFI_PASSWD_LEN + 1];
    wifi_backup_switch_fsm_t  switch_fsm;
    wifi_backup_connect_fsm_t connect_fsm;
    tuya_wfpt_state_cb_t     *state_cb;
    tuya_wifi_backup_trans_t  trans;
    DELAYED_WORK_HANDLE       fsm_work;
    TIMER_ID                  offline_timer;
    uint32_t                  local_num;
    uint32_t                  cloud_num;
} wifi_backup_mgr_t;


extern wifi_backup_mgr_t g_wifi_backup_mgr;
extern int (*g_wifi_backup_conf_attr)(tuya_wfpt_attr_t *attr);

/* Shared core helpers (wifi_backup_fsm.c) */
void              wifi_backup_lock(void);
void              wifi_backup_unlock(void);
void              wifi_backup_str_copy(char *dst, uint32_t dst_size, const char *src);
bool              wifi_backup_cur_ap_get(char *ssid, uint32_t ssid_size, char *passwd, uint32_t passwd_size);
bool              wifi_backup_is_same_ap_as_current(const char *ssid, const char *passwd);
void              wifi_backup_ap_hash(wifi_backup_ap_t *ap);
wifi_backup_ap_t *wifi_backup_list_find(const uint8_t *hash);
void              wifi_backup_list_remove_at(uint8_t index);
void              wifi_backup_list_push(const char *ssid, const char *passwd, bool front);
void              wifi_backup_list_keep(const uint8_t (*hashes)[WIFI_BACKUP_HASH_LEN], uint8_t num);
void              wifi_backup_list_clear(void);
OPERATE_RET       wifi_backup_kv_save(void);
OPERATE_RET       wifi_backup_switch_start(const char *tid, const char *devid, const char *ssid, const char *passwd,
                                           tuya_wifi_backup_trans_t trans);

/* Protocol helpers (wifi_backup_proto.c) */
void wifi_backup_proto_register(void);
void wifi_backup_resp_simple(tuya_wifi_backup_trans_t trans, const char *req_type, const char *tid, const char *devid,
                             int res_code);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_BACKUP_INTERNAL_H__ */
