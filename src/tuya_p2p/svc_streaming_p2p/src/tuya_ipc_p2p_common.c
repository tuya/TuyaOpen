#include <stddef.h>
#include <stdio.h>
#include <string.h>
//#include "uni_log.h"
//#include "gw_intf.h"
#include "cJSON.h"
#include "tuya_ipc_p2p_common.h"
//#include "cloud_httpc.h"
//#include "tuya_ws_db.h"
//#include "mqc_app.h"
//#include "tuya_tls.h"
//#include "iot_httpc.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "tal_time_service.h"

#include "tal_kv.h"
#include "tuya_iot.h"
#include "atop_service.h"

#define P2P_AUTH_INFO_UPDATE_RETRY_CNT (20)

// Force HTTPS POST 2.0
#define TI_IPC_P2P_CONFIG_GET "tuya.device.ipc.p2p.config.get"
// Force HTTPS POST 1.0
#define TI_IPC_PASSWORD_UPDATE "tuya.device.ipc.password.update"

static BOOL_T sg_p2p_passwd_update_flag = FALSE;
static BOOL_T sg_p2p_pwd_cloud_synced = FALSE;

OPERATE_RET httpc_ipc_p2p_cfg_get_v20(const char *gw_id, const int p2p_type, cJSON **result)
{
    // HTTPC_NULL_CHECK(gw_id);
    // HTTPC_NULL_CHECK(result);

    TIME_T timestamp = 0;
    timestamp = tal_time_get_posix();

    char *post_data = malloc(64);
    if (post_data == NULL) {
        printf("Malloc Fail.\n");
        return OPRT_MALLOC_FAILED;
    }
    memset(post_data, 0, 64);
    snprintf(post_data, 64, "{\"type\":%d,\"t\":\"%d\"}", p2p_type, timestamp);

    OPERATE_RET op_ret = OPRT_OK;
    // op_ret = iot_httpc_common_post_no_remalloc(
    //                            TI_IPC_P2P_CONFIG_GET, "2.0",
    //                            NULL, gw_id,
    //                            post_data, 64, NULL,
    //                            result);

    tuya_iot_client_t *iot_client = tuya_iot_client_get();
    op_ret = atop_service_comm_post_simple(TI_IPC_P2P_CONFIG_GET, "2.0", post_data, NULL, result);

    free(post_data);
    return op_ret;
}

OPERATE_RET httpc_ipc_p2p_passwd_update_v10(const char *gw_id, const char *p2p_passwd, cJSON **result)
{
    // HTTPC_NULL_CHECK(gw_id);
    // HTTPC_NULL_CHECK(p2p_passwd);
    // HTTPC_NULL_CHECK(result);

    TIME_T timestamp = 0;
    timestamp = tal_time_get_posix();

    char *post_data = Malloc(128);
    if (post_data == NULL) {
        PR_ERR("Malloc Fail.");
        return OPRT_MALLOC_FAILED;
    }
    memset(post_data, 0, 128);
    snprintf(post_data, 128, "{\"password\":\"%s\",\"t\":\"%d\"}", p2p_passwd, timestamp);

    OPERATE_RET op_ret = OPRT_OK;
    // op_ret = httpc_common_post_no_remalloc(
    //                            TI_IPC_PASSWORD_UPDATE, "1.0",
    //                            NULL, gw_id,
    //                            post_data, 128, NULL,
    //                            result);

    op_ret = atop_service_comm_post_simple(TI_IPC_PASSWORD_UPDATE, "1.0", post_data, NULL, result);

    Free(post_data);
    return op_ret;
}

/* tutk:1   ppcs:2 */
OPERATE_RET httpc_ipc_p2p_cfg_get(const int p2p_type, cJSON **result)
{
    // GW_CNTL_S *gw_cntl = get_gw_cntl();
    OPERATE_RET op_ret = OPRT_OK;
    op_ret = httpc_ipc_p2p_cfg_get_v20(NULL /*gw_cntl->gw_if.id*/, p2p_type, result);
    return op_ret;
}

OPERATE_RET httpc_ipc_p2p_passwd_update(const char *p2p_passwd, cJSON **result)
{
    // GW_CNTL_S *gw_cntl = get_gw_cntl();
    OPERATE_RET op_ret = OPRT_OK;
    op_ret = httpc_ipc_p2p_passwd_update_v10(NULL /*gw_cntl->gw_if.id*/, p2p_passwd, result);
    return op_ret;
}

/***********************************************************
 *  Function: tuya_ipc_p2p_update_pw
 *  Note:Force update passwd to server
 *  Input:
 *  Output: none
 *  Return:
 ***********************************************************/
OPERATE_RET tuya_ipc_p2p_update_pw(char p2p_pw[])
{
    OPERATE_RET ret = OPRT_OK;
    cJSON *result = NULL;

    // PR_DEBUG("p2p passwd report %s", p2p_pw);
    ret = httpc_ipc_p2p_passwd_update(p2p_pw, &result);
    if (OPRT_OK != ret) {
        PR_DEBUG("passwd update failed");
    }
    cJSON_Delete(result);

    return ret;
}

/***********************************************************
 *  Function: tuya_ipc_p2p_get_pw
 *  Note:Get p2p_pw
 *  Input:
 *  Output: none
 *  Return:
 ***********************************************************/
OPERATE_RET tuya_ipc_p2p_get_pw(char p2p_pw[])
{
    uint8_t *old_pwd = NULL;
    uint32_t old_pwd_len = 0;
    cJSON *result = NULL;
    uint8_t new_pwd[P2P_PASSWD_LEN + 1] = {0};
    int rtyCnt = 0;

    OPERATE_RET ret = tal_kv_get("p2p_pwd", &(old_pwd), (size_t *)&old_pwd_len);
    if ((OPRT_OK != ret) || (0 == old_pwd[0])) {
        if (sg_p2p_passwd_update_flag == FALSE) {
            sg_p2p_passwd_update_flag = TRUE;

            // Loop to get pw
            while (rtyCnt < P2P_AUTH_INFO_UPDATE_RETRY_CNT) {
                TIME_T curtime = tal_time_get_posix();
                memset(new_pwd, 0x00, P2P_PASSWD_LEN + 1);

                snprintf((char *)new_pwd, P2P_PASSWD_LEN + 1, "ad%06x", (int)curtime & 0xFFFFFF);
                // PR_DEBUG("p2p passwd change to %s", new_pwd);
                if (OPRT_OK != httpc_ipc_p2p_passwd_update((const char *)new_pwd, &result)) {
                    cJSON_Delete(result);
                    PR_DEBUG("passwd update failed [%d]", rtyCnt);
                    rtyCnt++;
                    tal_system_sleep(500);
                    continue;
                }
                cJSON_Delete(result);
                break;
            }
            if (rtyCnt >= P2P_AUTH_INFO_UPDATE_RETRY_CNT) {
                PR_ERR("p2p passwd update failed");
                sg_p2p_passwd_update_flag = FALSE;
                return OPRT_COM_ERROR;
            } else {
                snprintf(p2p_pw, P2P_PASSWD_LEN + 1, "%s", (char *)new_pwd);
                tal_kv_set("p2p_pwd", (const uint8_t *)p2p_pw, P2P_PASSWD_LEN + 1);
            }
        } else {
            PR_DEBUG("p2p passwd wait for passwd update\n");
            int wait_times = P2P_AUTH_INFO_UPDATE_RETRY_CNT;
            do {
                OPERATE_RET ret = tal_kv_get("p2p_pwd", &(old_pwd), (size_t *)&old_pwd_len);
                if (ret == OPRT_OK) {
                    // PR_DEBUG("get p2p passwd = %s",old_pwd);
                    snprintf(p2p_pw, P2P_PASSWD_LEN + 1, "%s", (char *)old_pwd);
                    tal_kv_free(old_pwd);
                    break;
                } else {
                    tal_system_sleep(500);
                }
            } while (--wait_times);
        }
    } else {
        /*
         * KV already has password. Still push to cloud so App/localkey MD5
         * matches; stale cloud password caused first-connect auth failed.
         */
        snprintf(p2p_pw, P2P_PASSWD_LEN + 1, "%s", (char *)old_pwd);
        tal_kv_free(old_pwd);
        if (sg_p2p_pwd_cloud_synced == FALSE && sg_p2p_passwd_update_flag == FALSE) {
            OPERATE_RET sync_ret;
            sg_p2p_passwd_update_flag = TRUE;
            sync_ret = tuya_ipc_p2p_update_pw(p2p_pw);
            PR_DEBUG("p2p passwd cloud sync ret=%d", sync_ret);
            sg_p2p_passwd_update_flag = FALSE;
            if (sync_ret == OPRT_OK) {
                sg_p2p_pwd_cloud_synced = TRUE;
            }
        }
    }

    return OPRT_OK;
}

/***********************************************************
 *  Function: tuya_ipc_p2p_get_lk
 *  Note:Get p2p local_key
 *  Input:
 *  Output: none
 *  Return:
 ***********************************************************/
OPERATE_RET tuya_ipc_p2p_get_lk(char p2p_lk[])
{
    // GW_CNTL_S *gw_cntl = get_gw_cntl();
    uint32_t wait_lk = 10;
    do {
        if (strlen(tuya_iot_client_get()->activate.localkey) != 0) {
            strcpy(p2p_lk, tuya_iot_client_get()->activate.localkey);
            break;
        } else {
            PR_DEBUG("p2p get local key failed, wait: %d\n", wait_lk);
            tal_system_sleep(10);
        }
    } while (--wait_lk);
    // PR_DEBUG("get local_key = %s",p2p_lk);
    return OPRT_OK;
}
void tuya_ipc_p2p_get_name(char p2p_name[])
{
    strcpy(p2p_name, "admin");
}

/***********************************************************
 *  Function: tuya_ipc_p2p_get_id
 *  Note:Get p2p_id value, tutk only needs id value, separate interface for tutk
 *  Input: p_auth_param p2p info structure address
 *  Output: none
 *  Return:
 ***********************************************************/
OPERATE_RET tuya_ipc_p2p_get_id(char p2p_id[])
{
    if (NULL == p2p_id) {
        PR_ERR("input error");
        return OPRT_INVALID_PARM;
    }
    uint8_t *p_auth_str = NULL;
    uint32_t auth_param_len = 0;

    OPERATE_RET ret = tal_kv_get("p2p_auth_info", &p_auth_str, (size_t *)&auth_param_len);
    if ((ret != OPRT_OK) || (0 == p_auth_str[0])) {
        PR_ERR("read p2p_auth_info fails ..%d", ret);
        return OPRT_COM_ERROR;
    }

    // PR_DEBUG("load str:%s", p_auth_str);
    cJSON *p_authjson = cJSON_Parse((char *)p_auth_str);
    if (NULL == p_authjson) {
        PR_ERR("parse json fails");
        tal_kv_free(p_auth_str);
        return OPRT_CJSON_PARSE_ERR;
    }
    cJSON *p_child = NULL;

    p_child = cJSON_GetObjectItem(p_authjson, "p2pId");
    if (p_child != NULL) {
        strcpy(p2p_id, p_child->valuestring);
    }

    cJSON_Delete(p_authjson);
    tal_kv_free(p_auth_str);

    return OPRT_OK;
}

OPERATE_RET tuya_ipc_get_p2p_auth(TUYA_IPC_P2P_AUTH_T *pAuth)
{
    if (NULL == pAuth) {
        PR_ERR("Invalid Param");
        return OPRT_INVALID_PARM;
    }

    memset(pAuth, 0, SIZEOF(TUYA_IPC_P2P_AUTH_T));
    tuya_ipc_p2p_get_name(pAuth->p2p_name);
    tuya_ipc_p2p_get_pw(pAuth->p2p_passwd);
    tuya_ipc_p2p_get_lk(pAuth->gw_local_key);
    return OPRT_OK;
}

OPERATE_RET tuya_ipc_get_p2p_auth_proc()
{
    OPERATE_RET ret = OPRT_OK;
    cJSON *result = NULL;

    ret = httpc_ipc_p2p_cfg_get(TUYA_P2P, &result);

    char *tmp_str = cJSON_PrintUnformatted(result);
    if (NULL == tmp_str) {
        PR_ERR("get p2p auth failed");
        cJSON_Delete(result);
        return OPRT_COM_ERROR;
    }

    if (ret == OPRT_OK) {
        // PR_DEBUG("SY P2P AUTH:%s",tmp_str);
        tal_kv_set("p2p_auth_info", (uint8_t *)tmp_str, strlen(tmp_str) + 1);
        cJSON_free(tmp_str);
    }

    cJSON_Delete(result);

    return ret;
}
/***********************************************************
 *  Function: tuya_ipc_check_p2p_auth_update
 *  Note:Check if p2p needs update
 *  Input:
 *  Output: none
 *  Return:
 ***********************************************************/
OPERATE_RET tuya_ipc_check_p2p_auth_update(void)
{
    PR_DEBUG("check p2p auth update or not");
    // After power-on, first check if p2p info needs update, judgment condition (whether there is p2p related info in
    // configuration)

    uint8_t *p_auth_str = NULL;
    size_t auth_param_len = 0;
    uint8_t *p_type = NULL;
    int p2p_type = 0;
    char str_p2p_type[P2P_TYPE_LEN] = {0};
    size_t p2p_type_len = 0;
    int rtyCnt = 0;
    BOOL_T isNeedReLoad = FALSE;

    OPERATE_RET ret = tal_kv_get("p2p_auth_info", &p_auth_str, &auth_param_len);
    OPERATE_RET ret2 = tal_kv_get("p2p_type", &p_type, &p2p_type_len);

    if ((OPRT_OK != ret) || (OPRT_OK != ret2)) {
        isNeedReLoad = TRUE;
    } else {
        if ((0 == p_auth_str[0]) || (0 == p_type[0])) {
            isNeedReLoad = TRUE;
        } else {
            // Compatible with old fields of TUYA_P2P
            if (0 == strcmp((char *)p_type, "tutk")) {
                p2p_type = 1;
            } else if (0 == strcmp((char *)p_type, "ppcs")) {
                p2p_type = 2;
            } else if (0 == strcmp((char *)p_type, "mqtt_p2p")) {
                p2p_type = 4;
            } else if (0 == strcmp((char *)p_type, "ppcs+mqtt_p2p")) {
                p2p_type = 6;
            } else {
                p2p_type = atoi((char *)p_type);
            }
            snprintf(str_p2p_type, 4, "%d", p2p_type);

            if (p2p_type != TUYA_P2P) {
                isNeedReLoad = TRUE;
            }
            if (0 != strcmp(str_p2p_type, (char *)p_type)) {
                isNeedReLoad = TRUE;
            }
        }
    }
    tal_kv_free(p_type);
    tal_kv_free(p_auth_str);

    if (TRUE == isNeedReLoad) {
        char new_str_p2p_type[P2P_TYPE_LEN] = {0};
        snprintf(new_str_p2p_type, P2P_TYPE_LEN, "%d", TUYA_P2P);
        PR_DEBUG("update p2p_auth_info from service, type %d", TUYA_P2P);
        while (1) {
            if (OPRT_OK == tuya_ipc_get_p2p_auth_proc()) {
                PR_DEBUG("get p2p auth info from service");
                break;
            }
            rtyCnt++;
            if (rtyCnt % 5 == 0) {
                PR_ERR("get p2p auth retry cnt[%d]", rtyCnt);
            }
            tal_system_sleep(1000);
        }
        tal_kv_set("p2p_type", (uint8_t *)new_str_p2p_type, strlen(new_str_p2p_type) + 1);
    } else {
        PR_DEBUG("no need update p2p_auth_info from service");
    }

    return OPRT_OK;
}

int iot_gw_reset_cb(void *rst_tp)
{
    PR_DEBUG("__begin");
    // Clear p2p_auth_info information

    char new_auth[P2P_ID_LEN + 1];
    memset(new_auth, 0x00, sizeof(new_auth));
    if (OPRT_OK != tal_kv_set("p2p_auth_info", (uint8_t *)new_auth, strlen(new_auth) + 1)) {
        PR_ERR("reset  p2p_auth_info failed");
    }
    char new_pwd[P2P_PASSWD_LEN + 1];
    memset(new_pwd, 0x00, sizeof(new_pwd));
    if (OPRT_OK != tal_kv_set("p2p_pwd", (uint8_t *)new_pwd, strlen(new_pwd) + 1)) {
        PR_ERR("reset  p2p_pwd failed");
    }

    uint8_t new_type[P2P_TYPE_LEN + 1];
    memset(new_type, 0x00, sizeof(new_type));
    if (OPRT_OK != tal_kv_set("p2p_type", new_type, strlen((char *)new_type) + 1)) {
        PR_ERR("reset  p2p_type failed");
    }
    return OPRT_OK;
}

// Called frequently, add variable control
static BOOL_T sg_p2p_passwd_flag = FALSE;
BOOL_T iot_permit_mqtt_connect_cb(void)
{
    uint8_t *old_pwd = NULL;
    size_t old_pwd_len = 0;
    uint8_t new_pwd[P2P_PASSWD_LEN + 1] = {0};
    cJSON *result = NULL;
    OPERATE_RET ret = 0;
    static uint32_t fail_cnt = 0;

    if (TRUE == sg_p2p_passwd_flag) {
        return TRUE;
    }
    ret = tal_kv_get("p2p_pwd", &(old_pwd), &old_pwd_len);
    if ((OPRT_OK != ret) || (0 == old_pwd[0])) {
        if (sg_p2p_passwd_update_flag == FALSE) {
            sg_p2p_passwd_update_flag = TRUE;

            TIME_T curtime = tal_time_get_posix();
            memset(new_pwd, 0x00, P2P_PASSWD_LEN + 1);

            snprintf((char *)new_pwd, P2P_PASSWD_LEN + 1, "ad%06x", (int)curtime & 0xFFFFFF);
            // PR_DEBUG("p2p passwd change to %s", new_pwd);
            if (OPRT_OK != httpc_ipc_p2p_passwd_update((char *)new_pwd, &result)) {
                PR_DEBUG("passwd update failed %d\n", fail_cnt);
                sg_p2p_passwd_flag = FALSE;
                sg_p2p_passwd_update_flag = FALSE;
                ret = -1;
                if (fail_cnt++ > 20) {
                    // set_gw_ext_stat(EXT_NET_FAIL);
                }
            } else {
                tal_kv_set("p2p_pwd", (uint8_t *)new_pwd, P2P_PASSWD_LEN + 1);
                sg_p2p_passwd_flag = TRUE;
                ret = 0;
                fail_cnt = 0;
                // set_gw_ext_stat(EXT_NORMAL_S);
            }
            if (result) {
                cJSON_Delete(result);
            }
        }
    } else {
        // PR_DEBUG("get p2p passwd = %s",old_pwd);
        tal_kv_free(old_pwd);
        sg_p2p_passwd_flag = TRUE;
        ret = 0;
    }
    return ret == 0 ? TRUE : FALSE;
}

// OPERATE_RET mqc_p2p_data_rept_v41(IN const char *devid,IN const char * pData, IN const int len)
// {
//     if (NULL == pData || NULL == devid) {
//         PR_ERR("input failed");
//         return OPRT_COM_ERROR;
//     }
//     return mqc_prot_data_rept_seq(PRO_RTC_REQ, pData, 0, 0, NULL, NULL);
// }
