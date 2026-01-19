/**
 * @file tuya_app_main.c
 * @author www.tuya.com
 * @brief tuya_app_main module is used to
 * @version 0.1
 * @date 2022-10-28
 *
 * @copyright Copyright (c) tuya.inc 2022
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tuya_cloud_types.h"
#include "tuya_device_cfg.h"
#include "tuya_svc_netmgr.h"
#if defined(ENABLE_WIFI_SERVICE) && (ENABLE_WIFI_SERVICE == 1)
#include "tuya_iot_wifi_api.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "tuya_iot_base_api.h"
#endif
#include "tuya_iot_com_api.h"
#include "tuya_ws_db.h"

#include "tal_system.h"
#include "tal_log.h"
#include "base_event.h"
#include "mf_test.h"
#include "mqc_app.h"
#if defined(ENABLE_LWIP) && (ENABLE_LWIP == 1)
#include "lwip_init.h"
#endif

#include "tal_uart.h"
#include "tuya_ai_toy.h"
#include "tuya_device_cfg.h"
#include "tuya_ai_battery.h"

// e-Paper 墨水屏时钟显示
#include "epd_clock.h"
#include "epd_pet.h"    // 虚拟电子宠物

#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
#include "tal_cellular.h"
#include "tuya_svc_cellular.h"
#include "tuya_iot_internal_api.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#define PID         "nhjetawhwodzbki6"      // TUYA T5AI-EVB xiaozhi box
// #define PID         "zbwbmdyemfa4ipkw"       // AI-Chat Multi-Mode PID
// #define PID         "yr4ybissxrezmu2u"       // AI-Chat Demo PID
// #define PID 		   "a3gahyytd3g8oatg"       // T5AI_BOARD_CELLULAR 
// #define PID 		   "y0k6ydkxphvv5g7a"       // AI-ROBOT-DOG Demo PID 

// #define UUID        "your uuid"
// #define AUTHKEY     "your authkey"

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
extern void tuya_ble_enable_debug(bool enable);
extern VOID_T tuya_ai_camera_init(VOID_T);

/***********************************************************
***********************variable define**********************
***********************************************************/
/* app thread handle */
STATIC THREAD_HANDLE ty_app_thread = NULL;

#if defined(T5AI_BOARD_CELLULAR ) && (T5AI_BOARD_CELLULAR  == 1)
#define TI_META_SAVE    "tuya.device.meta.save"
BOOL_T is_cellular_ccid_reported = FALSE;
#endif
/***********************************************************
***********************function define**********************
***********************************************************/

#if (defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1))
// qrcode打印
extern INT_T qrcode_exec(INT_T argc, CHAR_T **argv);
STATIC INT_T __qrcode_printf(CHAR_T *msg)
{
    CHAR_T *qrcode_argv[] = {
        "qrcode_exec", "-m", "3", "-t", "ansiutf8", msg
    };

    return qrcode_exec(sizeof(qrcode_argv)/sizeof(qrcode_argv[0]), qrcode_argv);
}

// TuyaOS获取到短链接之后调用此接口输出qrcode打印
STATIC VOID __qrcode_active_shourturl_cb(CONST CHAR_T *shorturl)
{
    if (NULL == shorturl) {
        return;
    }

    TAL_PR_DEBUG("shorturl : %s", shorturl);
    // ty_cJSON *item = ty_cJSON_Parse(shorturl);
    // __qrcode_printf(ty_cJSON_GetObjectItem(item, "shortUrl")->valuestring);
    // ty_cJSON_Delete(item);

    return;
}

OPERATE_RET httpc_put_iccid(IN CHAR_T iccid[21])
{
    OPERATE_RET op_ret = OPRT_OK;
    INT_T buffer_len = 72;
    CHAR_T *post_data = Malloc(buffer_len);
    if(post_data == NULL)
    {
        TAL_PR_ERR("Malloc Fail");
        return OPRT_MALLOC_FAILED;
    }

    memset(post_data, 0, buffer_len);
    snprintf(post_data, buffer_len, "{\"metas\":{\"catIccId\":\"%s\"}}", iccid);

    op_ret = iot_httpc_common_post_simple(TI_META_SAVE, "1.0",post_data, NULL,NULL);
    Free(post_data);
    return op_ret;
}

OPERATE_RET cellular_http_upload_iccid(VOID_T)
{
    OPERATE_RET op_ret;
    CHAR_T iccid[TAL_CELLULAR_CCID_LEN+1] = { 0 };
    
    if (is_cellular_ccid_reported) {
        return OPRT_OK;
    }

    op_ret = tal_cellular_get_ccid(iccid);
    if (OPRT_OK != op_ret) {
        return OPRT_COM_ERROR;
    }

    if ('\0' == iccid[0]) {
        return OPRT_COM_ERROR;
    }
 
    op_ret = httpc_put_iccid(iccid);
    if (OPRT_OK != op_ret) {
        return OPRT_COM_ERROR;
    }

    is_cellular_ccid_reported = TRUE;
    TAL_PR_NOTICE("cellular report ccid %s to Tuya cloud", iccid);
    return op_ret;
}
#endif

/**
 * @brief SOC device upgrade entry
 *
 * @param[in] fw: firmware info
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h".
 */
STATIC OPERATE_RET __soc_dev_rev_upgrade_info_cb(IN CONST FW_UG_S *fw)
{
    TAL_PR_DEBUG("SOC Rev Upgrade Info");
    TAL_PR_DEBUG("fw->tp:%d", fw->tp);
    TAL_PR_DEBUG("fw->fw_url:%s", fw->fw_url);
    TAL_PR_DEBUG("fw->fw_hmac:%s", fw->fw_hmac);
    TAL_PR_DEBUG("fw->sw_ver:%s", fw->sw_ver);
    TAL_PR_DEBUG("fw->file_size:%u", fw->file_size);

    return OPRT_OK;
}

/**
 * @brief SOC device cloud state change callback
 *
 * @param[in] status: current status
 *
 * @return none
 */
STATIC VOID_T __soc_dev_status_changed_cb(IN CONST GW_STATUS_E status)
{
    TAL_PR_DEBUG("SOC TUYA-Cloud Status:%d", status);
    return;
}


/**
 * @brief SOC device DP query entry
 *
 * @param[in] dp_qry: DP query list
 *
 * @return none
 */
STATIC VOID_T __soc_dev_dp_query_cb(IN CONST TY_DP_QUERY_S *dp_qry)
{
    UINT32_T index = 0;

    TAL_PR_DEBUG("SOC Rev DP Query Cmd");
    if (dp_qry->cid != NULL) {
        TAL_PR_ERR("soc not have cid.%s", dp_qry->cid);
    }

    if (dp_qry->cnt == 0) {
        TAL_PR_DEBUG("soc rev all dp query");
    } else {
        TAL_PR_DEBUG("soc rev dp query cnt:%d", dp_qry->cnt);
        for (index = 0; index < dp_qry->cnt; index++) {
            TAL_PR_DEBUG("rev dp query:%d", dp_qry->dpid[index]);
            // UserTODO
        }
    }

    return;
}

/**
 * @brief SOC device format command data delivery entry
 *
 * @param[in] dp: obj dp info
 *
 * @return none
 */
STATIC VOID_T __soc_dev_obj_dp_cmd_cb(IN CONST TY_RECV_OBJ_DP_S *dp)
{

    TAL_PR_DEBUG("SOC Rev DP Obj Cmd t1:%d t2:%d CNT:%u", dp->cmd_tp, dp->dtt_tp, dp->dps_cnt);

    // invoke ai toy dp command callback
    ty_ai_toy_dp_cmd_cb(dp);

    return;
}

/**
 * @brief SOC device transparently transmits command data delivery entry
 *
 * @param[in] dp: raw dp info
 *
 * @return none
 */
STATIC VOID_T __soc_dev_raw_dp_cmd_cb(IN CONST TY_RECV_RAW_DP_S *dp)
{
    TAL_PR_DEBUG("SOC Rev DP Raw Cmd t1:%d t2:%d dpid:%d len:%u", dp->cmd_tp, dp->dtt_tp, dp->dpid, dp->len);

    return;
}

/**
 * @brief  app process when device reset
 *
 * @param[in] type: gateway reset type
 *
 * @return none
 */
STATIC VOID_T __soc_dev_reset_inform_cb(GW_RESET_TYPE_E type)
{
    TAL_PR_DEBUG("reset type %d", type);

    return;
}

/**
 * @brief SOC external network status change callback
 *
 * @param[in/out] data
 * @return STATIC
 */
STATIC OPERATE_RET __soc_dev_net_status_cb(VOID *data)
{
    STATIC BOOL_T s_syn_all_status = FALSE;

    TAL_PR_DEBUG("network status changed!");
    if (tuya_svc_netmgr_linkage_is_up(LINKAGE_TYPE_DEFAULT)) {
        TAL_PR_DEBUG("linkage status changed, current status is up");
        if (get_mqc_conn_stat()) {
            TAL_PR_DEBUG("mqtt is connected!");

            if (FALSE == s_syn_all_status) {
                s_syn_all_status = TRUE;
            }
            // UserTODO
#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
            cellular_http_upload_iccid();
#endif
        }
    } else {
        TAL_PR_DEBUG("linkage status changed, current status is down");

        // UserTODO
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __soc_dev_reset_cb(VOID *data)
{
    __soc_dev_reset_inform_cb((GW_RESET_TYPE_E)data);
    tal_system_reset();
    return OPRT_OK;
}

/**
 * @brief mf uart init
 *
 * @param[in] baud: Baud rate
 * @param[in] bufsz: uart receive buffer size
 *
 * @return none
 */
VOID mf_uart_init_callback(UINT_T baud, UINT_T bufsz)
{
    TAL_UART_CFG_T cfg;
    memset(&cfg, 0, sizeof(TAL_UART_CFG_T));
    cfg.base_cfg.baudrate = baud;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.rx_buffer_size = bufsz;

    tal_uart_init(TUYA_UART_NUM_0, &cfg);

    return;
}

/**
 * @brief mf uart free
 *
 * @param[in] none
 *
 * @return none
 */
VOID mf_uart_free_callback(VOID)
{
    tal_uart_deinit(TUYA_UART_NUM_0);
    return;
}

/**
 * @brief mf uart send function
 *
 * @param[in] data: send data
 * @param[in] len: send data length
 *
 * @return none
 */
VOID mf_uart_send_callback(IN BYTE_T *data, IN CONST UINT_T len)
{
    tal_uart_write(TUYA_UART_NUM_0, data, len);
    return;
}

/**
 * @brief mf uart receive function
 *
 * @param[in] buf: receive buffer
 * @param[in] len: receive buffer max length
 *
 * @return receive data length
 */
UINT_T mf_uart_recv_callback(OUT BYTE_T *buf, IN CONST UINT_T len)
{
    return tal_uart_read(TUYA_UART_NUM_0, buf, len);
}

/**
 * @brief Product test callback function
 *
 * @param[in] cmd: Command
 * @param[in] data: data
 * @param[out] ret_data: Resulting data
 * @param[out] ret_len: Resulting data length
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h".
 */
OPERATE_RET mf_user_product_test_callback(USHORT_T cmd, UCHAR_T *data, UINT_T len, OUT UCHAR_T **ret_data, OUT USHORT_T *ret_len)
{
    /* USER todo */
    //gpio  test  refer to tuyaos_demo_examples -> src/examples/service_mf_test

    return OPRT_OK;
}

/**
 * @brief mf configure write callback functions
 *
 * @param[in] none
 *
 * @return none
 */
VOID mf_user_callback(VOID)
{
    return ;
}

/**
 * @brief Callback function before entering the production test
 *
 * @param[in] none
 *
 * @return none
 */
VOID mf_user_enter_mf_callback(VOID)
{
    return ;
}

/**
 * @brief SOC device initialization
 *
 * @param[in] none
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h".
 */
OPERATE_RET __soc_device_init(VOID_T)
{
    OPERATE_RET rt = OPRT_OK;

    ty_subscribe_event(EVENT_RESET, "quickstart", __soc_dev_reset_cb, SUBSCRIBE_TYPE_EMERGENCY);
    ty_subscribe_event(EVENT_LINK_UP, "quickstart", __soc_dev_net_status_cb, SUBSCRIBE_TYPE_NORMAL);
    ty_subscribe_event(EVENT_LINK_DOWN, "quickstart", __soc_dev_net_status_cb, SUBSCRIBE_TYPE_NORMAL);
    ty_subscribe_event(EVENT_MQTT_CONNECTED, "quickstart", __soc_dev_net_status_cb, SUBSCRIBE_TYPE_NORMAL);

#if (defined(UUID) && defined(AUTHKEY))
#ifndef ENABLE_KV_FILE
    ws_db_init_mf();
#endif
    /* Set authorization information
     * Note that if you use the default authorization information of the code, there may be problems of multiple users and conflicts,
     * so try to use all the authorizations purchased from the tuya iot platform.
     * Buying guide: https://developer.tuya.com/cn/docs/iot/lisence-management?id=Kb4qlem97idl0.
     * You can also apply for two authorization codes for free in the five-step hardware development stage of the Tuya IoT platform.
     * Authorization information can also be written through the production testing tool.
     * When the production testing function is started and the authorization is burned with the Tuya Cloud module tool,
     * please comment out this piece of code.
     */
#ifdef ENABLE_WIFI_SERVICE
    WF_GW_PROD_INFO_S prod_info = {UUID, AUTHKEY};
    TUYA_CALL_ERR_RETURN(tuya_iot_set_wf_gw_prod_info(&prod_info));
#else
    GW_PROD_INFO_S prod_info = {UUID, AUTHKEY};
    TUYA_CALL_ERR_RETURN(tuya_iot_set_gw_prod_info(&prod_info));
#endif

#else
    /*authorization is burned with the Tuya Cloud module tool
     *If you want to get the specific details, such as GPIO TEST,
     *please refer to the tuyaos_demo_examples -> src/examples/service_mf_test.*/
    MF_IMPORT_INTF_S intf = {0};

    intf.uart_init = mf_uart_init_callback;
    intf.uart_free = mf_uart_free_callback;
    intf.uart_send = mf_uart_send_callback;
    intf.uart_recv = mf_uart_recv_callback;

    intf.mf_user_product_test = mf_user_product_test_callback;
    intf.user_callback = mf_user_callback;
    intf.user_enter_mf_callback = mf_user_enter_mf_callback;

    TUYA_CALL_ERR_RETURN(mf_init(&intf, APP_BIN_NAME, USER_SW_VER, TRUE));

    TAL_PR_NOTICE("mf_init successfully");
#endif

    // TODO: OEM firmware: need recovery para form json config
    TY_AI_TOY_CFG_T ai_toy_cfg  = TY_AI_TOY_CFG_DEFAULT;

#if defined(T5AI_BOARD_EVB) && T5AI_BOARD_EVB == 1    
    ty_ai_toy_mf_test_init(&ai_toy_cfg);
    TAL_PR_NOTICE("ty_ai_toy_mf_test_init");
#endif

#if (defined(ENABLE_PRODUCT_AUTOTEST) && (ENABLE_PRODUCT_AUTOTEST == 1))
    if (prodtest_ssid_scan(500)) {
        TAL_PR_NOTICE("prodtest_ssid_scan");
        return;
    }
    TAL_PR_NOTICE("prodtest_ssid_scan ignored");
#endif

    // set wifi dtim 3
    tal_cpu_set_lp_mode(TRUE);
    tal_wifi_set_lps_dtim(3);
    tal_cpu_lp_disable();
    tal_wifi_lp_disable();

    /* Initialize TuyaOS product information */
    TY_IOT_CBS_S iot_cbs = {0};
    iot_cbs.gw_status_cb    = __soc_dev_status_changed_cb;
    iot_cbs.gw_ug_cb        = __soc_dev_rev_upgrade_info_cb;
    iot_cbs.gw_reset_cb     = __soc_dev_reset_inform_cb;
    iot_cbs.dev_obj_dp_cb   = __soc_dev_obj_dp_cmd_cb;
    iot_cbs.dev_raw_dp_cb   = __soc_dev_raw_dp_cmd_cb;
    iot_cbs.dev_dp_query_cb = __soc_dev_dp_query_cb;
#if (defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1))
    iot_cbs.active_shorturl = __qrcode_active_shourturl_cb;
#endif
#ifdef ENABLE_WIFI_SERVICE
#ifdef PID
    TUYA_CALL_ERR_RETURN(tuya_iot_wf_soc_dev_init(GWCM_OLD, WF_START_AP_FIRST, &iot_cbs, PID, USER_SW_VER));
#else
    tuya_iot_oem_set(TRUE);
    TUYA_CALL_ERR_RETURN(tuya_iot_wf_soc_dev_init_param(GWCM_OLD, WF_START_AP_FIRST, &iot_cbs, GFW_FIRMWARE_KEY, GFW_FIRMWARE_KEY, USER_SW_VER));
#endif
#ifdef ENABLE_WIRED
    // init wired linkage
    TUYA_CALL_ERR_RETURN(tuya_svc_wired_init());
#endif
#else
    TUYA_CALL_ERR_RETURN(tuya_iot_soc_init(&iot_cbs, PID, USER_SW_VER));
#endif

    tuya_ble_enable_debug(false);
    
    /* AI toy initialization */
    TUYA_CALL_ERR_RETURN(log_seq_set_enable(FALSE));    // disable log sequence, 减少flash写入操作
    TUYA_CALL_ERR_RETURN(ty_ai_toy_init(&ai_toy_cfg));

    return 0;
}

#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
VOID pre_device_init(VOID)
{
    tuya_svc_cellular_init();
    mqc_set_connection_switch(TRUE);
    tuya_svc_netcfg_qrcode_init();

    LINKAGE_TYPE_E linkage_pri[LINKAGE_TYPE_MAX] = { 0 };
    uint8_t cnt = 0;
    linkage_pri[cnt++] = LINKAGE_TYPE_WIRED;
    linkage_pri[cnt++] = LINKAGE_TYPE_WIFI;
    linkage_pri[cnt] = LINKAGE_TYPE_CAT1;
    tuya_svc_netmgr_linkage_set_priority(linkage_pri, cnt);
}

STATIC VOID_T boot_cellular_module(VOID_T)
{
    TUYA_GPIO_BASE_CFG_T cfg;
    
    memset(&cfg, 0, sizeof(cfg));
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level = TUYA_GPIO_LEVEL_HIGH;
 
    tkl_gpio_init(TUYA_GPIO_NUM_24, &cfg);
    tkl_gpio_write(TUYA_GPIO_NUM_24, TUYA_GPIO_LEVEL_HIGH);
    
    memset(&cfg, 0, sizeof(cfg));
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level = TUYA_GPIO_LEVEL_LOW;

    tkl_gpio_init(TUYA_GPIO_NUM_9, &cfg);
    tkl_gpio_write(TUYA_GPIO_NUM_9, TUYA_GPIO_LEVEL_LOW);
}
#endif

STATIC VOID_T user_main(VOID_T)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
    boot_cellular_module();
#endif

    /* Initialization, because DB initialization takes a long time,
     * which affects the startup efficiency of some devices,
     * so special processing is performed during initialization to delay initialization of DB
     */
#if OPERATING_SYSTEM == SYSTEM_LINUX
    rt= system("mkdir -p ./tuya_db_files/");
    TUYA_CALL_ERR_LOG(tuya_iot_init_params("./tuya_db_files/", NULL));
#else
    TY_INIT_PARAMS_S init_param = {0};
    init_param.init_db = TRUE;
    strcpy(init_param.sys_env, TARGET_PLATFORM);
    TUYA_CALL_ERR_LOG(tuya_iot_init_params(NULL, &init_param));
#endif

    TAL_PR_NOTICE("sdk_info:%s", tuya_iot_get_sdk_info());                        /* print SDK information */
    TAL_PR_NOTICE("name:%s:%s", APP_BIN_NAME, USER_SW_VER);                       /* print the firmware name and version */
    TAL_PR_NOTICE("firmware compiled at %s %s", __DATE__, __TIME__);              /* print firmware compilation time */
    TAL_PR_NOTICE("system reset reason:[%d]", tal_system_get_reset_reason(NULL)); /* print system reboot causes */

    tal_log_set_manage_attr(TAL_LOG_LEVEL_DEBUG);
    // tal_log_set_manage_attr(TAL_LOG_LEVEL_INFO);

#if defined(ENABLE_TUYA_UI) && ENABLE_TUYA_UI == 1
    tuya_ai_display_init();
#endif

#if defined(ENABLE_TUYA_CAMERA) && ENABLE_TUYA_CAMERA == 1
    tuya_ai_camera_init();
#endif

    /* Initialization device */
    TAL_PR_DEBUG("device_init in");
    TUYA_CALL_ERR_LOG(__soc_device_init());

#if defined(TUYA_AI_TOY_BATTERY_ENABLE) && (TUYA_AI_TOY_BATTERY_ENABLE == 1)
    TUYA_CALL_ERR_LOG(tuya_ai_toy_battery_init());
#endif

    /* e-Paper 墨水屏时钟显示初始化 */
    TAL_PR_NOTICE("Starting e-Paper clock display...");
    TUYA_CALL_ERR_LOG(epd_clock_start());

    /* e-Paper 虚拟宠物初始化 */
    TAL_PR_NOTICE("Starting virtual pet...");
    TUYA_CALL_ERR_LOG(epd_pet_start());

#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
    TAL_CELLULAR_BASE_CFG_T cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.apn, "");
    tal_cellular_init(&cfg);
#endif

    return;
}

int reset_netconfig_init(VOID);
/**
* @brief  task thread
*
* @param[in] arg:Parameters when creating a task
* @return none
*/
STATIC VOID_T tuya_app_thread(VOID_T *arg)
{
    tuya_base_utilities_init();
#if defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)
    pre_device_init();
 #endif   
    /* Initialization LWIP first!!! */
#if defined(ENABLE_LWIP) && (ENABLE_LWIP == 1)
    TUYA_LwIP_Init();
#endif

    reset_netconfig_init();

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

/**
 * @brief user entry function
 *
 * @param[in] none:
 *
 * @return none
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
INT_T main(INT_T argc, CHAR_T **argv)
#else
VOID_T tuya_app_main(VOID)
#endif
{
    extern VOID_T tkl_system_psram_malloc_force_set(BOOL_T enable);
    tkl_system_psram_malloc_force_set(TRUE);

    THREAD_CFG_T thrd_param = {4096, THREAD_PRIO_2, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
#if OPERATING_SYSTEM == SYSTEM_LINUX
    while (1) {
        tal_system_sleep(1000);
    }
#endif
}

/* uncomment following comment to disable asr or enbale ty vad */
#if 1
/*
    * @brief  tuya_asr_enable
    * @param  none
    * 
    * @return BOOL_T return TRUE if enable tuya KWS, FALSE if enable user KWS or disable KWS.
    * @note   this function is weak, user can override it
    *         if user want to disable asr, just return FALSE
 */
BOOL_T tuya_asr_enable(VOID_T)
{
    return TRUE;
}

/*
    * @brief  tuya_vad_enable
    * @param  none
    * 
    * @return uint8_t return 1 if enable ty vad
    * @note   this function is weak, user can override it
    *         if user want to disable vad, just return FALSE
 */
uint8_t tuya_vad_enable(void)
{
    return FALSE;
}

VOID _tuya_asr_init(VOID_T)
{
    extern int tuya_wakeupword_default_init(void);     //! 默认启用涂鸦唤醒算法
    tuya_wakeupword_default_init();
}

#endif
