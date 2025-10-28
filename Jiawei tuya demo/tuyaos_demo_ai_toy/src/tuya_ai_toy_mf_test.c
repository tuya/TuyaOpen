#include "tuya_ai_toy_mf_test.h"
#include "prod_test.h"
#include "tuya_iot_base_api.h"
#include "tuya_cloud_com_defs.h"
#include "tal_log.h"
#include "tal_thread.h"
#include "tuya_ai_battery.h"
#include "tuya_ai_toy.h"
// #include "tuya_ai_proc.h"
#include <stdio.h>
#if defined(ENABLE_BT_SERVICE) && (ENABLE_BT_SERVICE==1)
#include "tuya_ble_sdk.h"
#endif
#include "tkl_audio.h"
#include "tuya_ringbuf.h"
#include "tuya_speaker_service.h"
#include "tuya_device_cfg.h"
// #include "t5_mp3_player.h"


#define PRODUCT_TEST_WIFI "ty_ai_mf_test"
#define PRODUCT_TEST_WIFI_1 "ty_ai_mf_test_1"

#define PROD_TEST_WEAK_SIGNAL       (-60)
#define PROD_TEST_TIMEOUT           (4 * 3600 * 1000) // 4 hours

STATIC CONST CHAR_T *s_WIFI_SCAN_SSID_LIST[] = {
    PRODUCT_TEST_WIFI_1,
};

#define AUDIO_SAMPLE_RATE          16000
#define SPK_SAMPLE_RATE            16000
#define AUDIO_SAMPLE_BITS          16
#define AUDIO_CHANNEL              1

TY_AI_TOY_CFG_T s_cfg;
STATIC TUYA_RINGBUFF_T stream_ringbuf;
STATIC UINT32_T stream_buf_size;   // 音频流buffer大小，单位字节
STATIC BOOL_T record_start = FALSE;
STATIC BOOL_T s_mf_init_flag = FALSE;
extern voltage_cap_map *bvc_map;

/**
 * @brief prod test sub cmd
 */
typedef USHORT_T PT_CMD_E;
/*0xF0 was product test cmd,  sub-cmd below*/
#define CMD_EXIT_OFFLINE_TEST  0xFF01 // 退出产测命令
#define CMD_ENTER_OFFLINE_TEST 0xFF02 // 进入产测命令
#define CMD_BOOL_TEST          0xFF03
#define CMD_RAW_TEST           0xFF04 // 通用透传测试命令

//#define CMD_INFORM_TEST_RESULT 0xFF07 // 通知测试结果命令
//#define CMD_JSON_UPDATE        0x000D // JSON更新命令 *通用固件没有这个功能*
//#define CMD_READ_RELAY_STATUS  0x0039 // 读继电器开关状态命令
//#define CMD_READ_DLTJ_POWER    0x003A // 读电量统计功率命令
//#define CMD_CLEAR_RECORD_INFO  0x003B // 清本地记录信息命令
//#define CMD_UPDATE_DEV_STATE   0x003C // 设备状态变化上报命令,暂不使用
//#define CMD_DEV_SELF_CHECK     0x003D // 设备自检命令,主要用于按键测试
#define CMD_LED_TEST           0x0001 // LED功能测试
#define CMD_RELAY_TEST         0x0002 // 继电器测试
#define CMD_KEY_TEST           0x0003 // 按键功能测试
#define CMD_MOTOR_TEST         0x0007 // 电机测试
#define CMD_WR_ELEC_STAT       0x0008 // 写入电量统计校准参数
#define CMD_ELEC_CHCK          0x0009 // 电量校准
#define CMD_AGING_TEST         0x000B // 老化测试
#define CMD_IR_SEND_TEST       0x000C // 红外发射
#define CMD_IR_RECV_TEST       0x000D // 红外接收
#define CMD_IR_MODE_CHANGE     0x000E // 红外工作模式切换,即退出接收模式
#define CMD_GET_DEV_MAC        0x0013 // 读取设备mac命令
#define CMD_COM_DATA_CFG       0x0014 // 成品通用数据配置
#define CMD_START_AUDIO        0x0028 // 开启音频测试
#define CMD_STOP_AUDIO         0x002A // 关闭音频测试
#define CMD_GET_WIFI_RSSI      0x002B // 读取wifi信号强度
#define CMD_GET_FIRM_INFO      0x002C // 读固件信息(固件版本,固件标识符...)
#define CMD_ANALOG_SENSOR_TEST 0x0030 // 模拟量传感器测试
#define CMD_BEEP_TEST          0x0031 // 喇叭/蜂鸣器测试
#define CMD_COM_SWITCH_TEST    0x0033 // 通用开关量测试
#define CMD_RECORD_TEST        0x0034 // 录音播音测试
#define CMD_GEAR_TEST          0x0036 // 挡位测试
#define CMD_COM_TEST           0x0035 // 通用功能测试，需要单独解析对PID获取 testitem:pid的处理
#define CMD_GET_BLE_RSSI       0x0100 // 获取BLE的RSSI，用于校验双模蓝牙的功能
#define CMD_GET_RF_RSSI        0x0101 // 获取RF的RSSI，用于校验RF遥控器功能
#define CMD_GET_COM_RSSI       0x0103 // 通用信号测试
/*cmd定义*/
#define CMD_UPGRADE_START 0x0F // 模组固件更新开始命令
#define CMD_UPGRADE_END   0x11 // 模组固件更新结束命令


/**
 * @brief prod test cmd type
 */
typedef enum {
    PT_CMD_READ,
    PT_CMD_WRITE,
    PT_CMD_NOTIFY
} PT_CMD_TYPE_E;

/**
 * @brief prod test vaule type
 */
typedef enum {
    PT_VALUE_INVAILD,
    PT_VALUE_STRING,
    PT_VALUE_INT,
    PT_VALUE_DOUBLE,
    PT_VALUE_HEX
} PT_VALUE_TYPE_E;

/**
 * @brief prod test cmd data info
 */
typedef struct {
    USHORT_T cmd;
    PT_CMD_TYPE_E cmd_type;
    PT_VALUE_TYPE_E value_type;
    UINT_T timeout; //unit:ms
    CHAR_T *ch;     //test channel
    CHAR_T *testitem;
    CHAR_T *value;
    BOOL_T default_ret;
} PT_CMD_DATA_S;

typedef OPERATE_RET(*PT_CMD_FUNC_CB)(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len);
typedef struct {
    USHORT_T cmd;
    PT_CMD_FUNC_CB func;
} PROD_TEST_OP_S;

STATIC TUYA_GPIO_NUM_E key_gpios[] = {TUYA_AI_TOY_AUDIO_TRIGGER_PIN, TUYA_AI_TOY_SPK_EN_PIN};
STATIC UCHAR_T key_ids[] = {0, 1};
STATIC TUYA_GPIO_LEVEL_E key_level[] = {TUYA_GPIO_LEVEL_HIGH, TUYA_GPIO_LEVEL_HIGH};
STATIC OPERATE_RET gpio_test_start(VOID);
STATIC OPERATE_RET led_test_start(UINT8_T type);
STATIC OPERATE_RET __mf_audio_init(VOID);
STATIC OPERATE_RET __mf_audio_uninit(VOID);
STATIC OPERATE_RET __mf_init(VOID);

STATIC OPERATE_RET __pt_enter(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    TAL_PR_DEBUG("prod test enter");
    return OPRT_OK;
}

STATIC OPERATE_RET __pt_exit(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    TAL_PR_DEBUG("prod test exit");
    return OPRT_OK;
}

//{"mac":"11100397*******"}
STATIC OPERATE_RET __pt_get_mac(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    NW_MAC_S dev_mac;
    memset(&dev_mac, 0, SIZEOF(NW_MAC_S));
    if (OPRT_OK != tal_wifi_get_mac(0, &dev_mac)) {
        TAL_PR_ERR("get mac err");
        return OPRT_COM_ERROR;
    }
    ty_cJSON *root = ty_cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(root, OPRT_COM_ERROR);

    UCHAR_T i = 0, k = 0;
    CHAR_T uDat[19] = {0};
    CHAR_T ubuf[14] = {0};
    for (i = 0; i < 12; i++) {
        k = i / 2;
        if (0 == i % 2) {
            uDat[i] = (dev_mac.mac[k] >> 4);
        } else {
            uDat[i] = (dev_mac.mac[k] & 0x0f);
        }
        sprintf((char *)ubuf + i, "%x", uDat[i]);
    }

    ty_cJSON_AddStringToObject(root, "mac", ubuf);
    CHAR_T *out = ty_cJSON_PrintUnformatted(root);
    ty_cJSON_Delete(root);
    TUYA_CHECK_NULL_RETURN(out, OPRT_COM_ERROR);

    TAL_PR_NOTICE("get mac %s", out);
    *ret_data = (UCHAR_T *)out;
    *ret_len = strlen(out);
    return OPRT_OK;
}

//{"ret":true,"rssi":xxxx}/{"ret":false}
STATIC OPERATE_RET __pt_get_wifi_rssi(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    ty_cJSON *root = ty_cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(root, OPRT_COM_ERROR);

    SCHAR_T rssi = -55;
    OPERATE_RET rt = gw_get_rssi(&rssi);
    if (OPRT_OK != rt) {
        TAL_PR_ERR("get rssi err %d", rt);
        return rt;
    }

    ty_cJSON_AddStringToObject(root, "ret", "true");
    ty_cJSON_AddNumberToObject(root, "rssi", rssi);

    CHAR_T *out = ty_cJSON_PrintUnformatted(root);
    ty_cJSON_Delete(root);
    TUYA_CHECK_NULL_RETURN(out, OPRT_COM_ERROR);

    TAL_PR_NOTICE("get wifi rssi %s", out);
    *ret_data = (UCHAR_T *)out;
    *ret_len = strlen(out);
    return OPRT_OK;
}


/**
 * @note 上位机下发内容：{"item":"ble","value":"TUYA_","high":-10.0,"low":-60.0,"timeout":4}，value可以为空，则用默认值
 *       交互成功：{"ret":true,"rssi":-50 }，交互失败：{"ret":false}
 */
STATIC OPERATE_RET __pt_get_ble_rssi(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *json = ty_cJSON_Parse((CHAR_T *)data);
    TUYA_CHECK_NULL_RETURN(json, OPRT_CJSON_PARSE_ERR);

#if defined(ENABLE_BT_SERVICE) && (ENABLE_BT_SERVICE == 1)
    ty_cJSON *jsontmp = ty_cJSON_GetObjectItem(json, "value");
    ty_bt_scan_info_t ble_scan;
    memset(&ble_scan, 0, sizeof(ble_scan));
    if (jsontmp == NULL) {
        strncpy(ble_scan.name, "ty_ai_mf_test", 16);
    } else {
        strncpy(ble_scan.name, jsontmp->valuestring, 16);
    }
    // parse timeout
    jsontmp = ty_cJSON_GetObjectItem(json, "timeout");
    if (jsontmp != NULL) {
        ble_scan.timeout_s = jsontmp->valueint;
    } else {
        ble_scan.timeout_s = 10;
    }
    ble_scan.scan_type = TY_BT_SCAN_BY_NAME;
    TAL_PR_DEBUG("ble scan name: %s", ble_scan.name);
    rt = tuya_sdk_bt_assign_scan(&ble_scan);
    if (OPRT_OK != rt) {
        TAL_PR_ERR("bt scan err");
        goto EXIT;
    }
    CHAR_T *out = tal_malloc(64);
    TUYA_CHECK_NULL_GOTO(out, EXIT);
    *ret_len = sprintf((char *)out, "{\"ret\":true,\"rssi\":%d}", ble_scan.rssi);
    *ret_data = (UCHAR_T *)out;

    TAL_PR_NOTICE("get ble rssi: %s", out);
#endif

EXIT:

    ty_cJSON_Delete(json);
    return rt;
}

// {"item":"ble","value":"ssid","low":xx,"high":xx,"timeout":xx}
STATIC OPERATE_RET __pt_get_com_rssi(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *json = ty_cJSON_Parse((CHAR_T *)data);
    TUYA_CHECK_NULL_RETURN(json, OPRT_CJSON_PARSE_ERR);
    TAL_PR_DEBUG("get com rssi %s", data);

    ty_cJSON *jsontmp = ty_cJSON_GetObjectItem(json, "item");
    TUYA_CHECK_NULL_GOTO(jsontmp, EXIT);
    if (strcmp(jsontmp->valuestring, "ble") == 0) {
        rt = __pt_get_ble_rssi(cmd, data, len, ret_data, ret_len);
    } else if (strcmp(jsontmp->valuestring, "wifi") == 0) {
        rt = __pt_get_wifi_rssi(cmd, data, len, ret_data, ret_len);
    } else {
        rt = OPRT_COM_ERROR;
    }

EXIT:    
    ty_cJSON_Delete(json);
    return rt;
}

STATIC OPERATE_RET __pt_get_firm_info(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    ty_cJSON *root = ty_cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(root, OPRT_COM_ERROR);

    CHAR_T fac[21] = {0};
    mf_test_facpin_get(fac);
    ty_cJSON_AddStringToObject(root, "ret", "true");
    ty_cJSON_AddStringToObject(root, "firmName", APP_BIN_NAME);
    ty_cJSON_AddStringToObject(root, "firmVer", USER_SW_VER);
    ty_cJSON_AddNumberToObject(root, "flashSize", 16);
    ty_cJSON_AddStringToObject(root, "fac_pin", fac);

    CHAR_T *out = ty_cJSON_PrintUnformatted(root);
    ty_cJSON_Delete(root);
    TUYA_CHECK_NULL_RETURN(out, OPRT_COM_ERROR);

    TAL_PR_NOTICE("get firmware info %s", out);
    *ret_data = (UCHAR_T *)out;
    *ret_len = strlen(out);
    return OPRT_OK;
}

/**
 * @brief 喇叭/蜂鸣器测试，0x0031
 * @note 上位机发送：{"state":"on"}/{"state":"off"}
 *       设备返回：{"ret":true}/{"ret":false}
 */
STATIC OPERATE_RET __pt_beep_test(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    OPERATE_RET rt = OPRT_OK;

    TAL_PR_NOTICE("beep test :%s", data);

    ty_cJSON *root_json = ty_cJSON_Parse((CHAR_T *)data);
    TUYA_CHECK_NULL_RETURN(root_json, OPRT_CJSON_PARSE_ERR);
    ty_cJSON *json_state = ty_cJSON_GetObjectItem(root_json, "state");
    TUYA_CHECK_NULL_GOTO(json_state, EXIT);

    PT_CMD_DATA_S cmd_data = {
        .cmd = cmd,
        .timeout = 5 * 1000,
        .value_type = PT_VALUE_STRING,
        .cmd_type = PT_CMD_NOTIFY,
        .testitem = NULL,
        .value = json_state->valuestring,
        .ch = NULL,
        .default_ret = TRUE,
    };

    // rt = ty_ai_prod_test_raw(&cmd_data, ret_data, ret_len, NULL);
    if (stream_ringbuf == NULL ||
        (strcmp(json_state->valuestring, "on") &&
         strcmp(json_state->valuestring, "off"))) {
        // response error data: {"ret":true}/{"ret":false}
        CHAR_T *ret_buf = tal_malloc(64);
        *ret_len = snprintf(ret_buf, 64, "{\"ret\":%s}", cmd_data.default_ret ? "true" : "false");
        *ret_data = (UCHAR_T *)ret_buf;
        goto EXIT;
    }

    if (strcmp(json_state->valuestring, "on") == 0) {
        TAL_PR_NOTICE("Speak on");
        // start play
        // do {
        //     CHAR_T buf[1024] = {0};
        //     INT_T read_size = t5_mp3_stream_read(buf, 1024);
        //     if (read_size <= 0) {
        //         break;
        //     }
        //     // put data to pcm
        //     TKL_AUDIO_FRAME_INFO_T frame;
        //     frame.pbuf = (CHAR_T *)buf;
        //     frame.used_size = read_size;
        //     rt = tkl_ao_put_frame(0, 0, NULL, &frame);
        //     if (rt != OPRT_OK) {
        //         TAL_PR_ERR("tkl_ao_put_frame failed, ret=%d", rt);
        //         continue;
        //     }
        // } while (1);
        // response success data: {"ret":true}
        CHAR_T *ret_buf = tal_malloc(64);
        *ret_len = snprintf(ret_buf, 64, "{\"ret\":true}");
        *ret_data = (UCHAR_T *)ret_buf;
    } else if (strcmp(json_state->valuestring, "off") == 0) {
        // stop beep
        // t5_mp3_stream_write(NULL, 0);
        // tuya_speaker_service_pl
        TAL_PR_DEBUG("Speak off");
        // response success data: {"ret":true}
        CHAR_T *ret_buf = tal_malloc(64);
        *ret_len = snprintf(ret_buf, 64, "{\"ret\":true}");
        *ret_data = (UCHAR_T *)ret_buf;
    } else {
        // response error data: {"ret":true}/{"ret":false}
        CHAR_T *ret_buf = tal_malloc(64);
        *ret_len = snprintf(ret_buf, 64, "{\"ret\":false}");
        *ret_data = (UCHAR_T *)ret_buf;
    }

EXIT:
    ty_cJSON_Delete(root_json);
    return rt;
}

STATIC THREAD_HANDLE __pt_record_thread;
STATIC BOOL_T s_pt_record_test_start = FALSE;
STATIC VOID_T __pt_record_thread_entry(VOID *arg)
{
    OPERATE_RET rt = OPRT_OK;
    UINT32_T duration = (UINT32_T)arg;

    TAL_PR_NOTICE("record test duration:%d", duration);

    // cache ringbuf audio data
    stream_buf_size = (duration) * AUDIO_SAMPLE_RATE * AUDIO_SAMPLE_BITS * AUDIO_CHANNEL / 8 / 1000;
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(stream_buf_size, OVERFLOW_PSRAM_STOP_TYPE, &stream_ringbuf), EXIT);

    TUYA_CALL_ERR_GOTO(__mf_audio_init(), EXIT);

    record_start = TRUE;
    TAL_PR_DEBUG("record start");

    tal_system_sleep(duration);

    CHAR_T *buf = tal_malloc(64);
    UINT32_T buf_len = snprintf(buf, 64, "{\"ret\":true}");
    mf_cmd_send(0xF0, (BYTE_T *)buf, buf_len);
    tal_free(buf);

    TAL_PR_DEBUG("record stop");
    record_start = FALSE;
    if (tuya_ring_buff_used_size_get(stream_ringbuf) > 0) {
        TAL_PR_ERR("record data is not empty");
        CHAR_T *buf = tal_malloc(3200);
        do {
            INT_T read_size = tuya_ring_buff_read(stream_ringbuf, buf, 3200);
            TAL_PR_DEBUG("read_size:%d", read_size);
            if (read_size <= 0) {
                break;
            }
            // put data to pcm
            TKL_AUDIO_FRAME_INFO_T frame;
            frame.pbuf = (CHAR_T *)buf;
            frame.used_size = read_size;
            rt = tkl_ao_put_frame(0, 0, NULL, &frame);
            if (rt != OPRT_OK) {
                TAL_PR_ERR("tkl_ao_put_frame failed, ret=%d", rt);
                continue;
            }
        } while (1);
        tal_free(buf);
        CHAR_T *ret_buf = tal_malloc(64);
        UINT32_T buf_len = snprintf(ret_buf, 64, "{\"ret\":true}");
        mf_cmd_send(0xF0, (BYTE_T *)buf, buf_len);
        tal_free(ret_buf);
    } else {
        TAL_PR_ERR("record data is empty");
        CHAR_T *ret_buf = tal_malloc(64);
        UINT32_T buf_len = snprintf(ret_buf, 64, "{\"ret\":false}");
        mf_cmd_send(0xF0, (BYTE_T *)buf, buf_len);
        tal_free(ret_buf);
    }

EXIT:
    __mf_audio_uninit();
    if (stream_ringbuf) {
        tuya_ring_buff_free(stream_ringbuf);
        stream_ringbuf = NULL;
    }
    s_pt_record_test_start = FALSE;
    return rt;
}

/**
 * @brief 录音播音测试，0x0034
 * @note 上位机发送：{"mic":0,"sec":3}；设备返回：{"ret":true }/{"ret":false}
 */
STATIC OPERATE_RET __pt_record_test(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_PR_NOTICE("record test :%s", data);

    ty_cJSON *root_json = ty_cJSON_Parse((CHAR_T *)data);
    TUYA_CHECK_NULL_RETURN(root_json, OPRT_CJSON_PARSE_ERR);
    ty_cJSON *json_mic = ty_cJSON_GetObjectItem(root_json, "mic");
    TUYA_CHECK_NULL_GOTO(json_mic, EXIT);
    ty_cJSON *json_sec = ty_cJSON_GetObjectItem(root_json, "sec");
    TUYA_CHECK_NULL_GOTO(json_mic, EXIT);
    json_sec = json_sec;

    if (!s_pt_record_test_start) {
        TAL_PR_DEBUG("__pt_record_thread_entry create!");
        UINT32_T duration = json_sec->valueint * 1000;
        // create thread to record
        THREAD_CFG_T thrd_param = {
            .priority = THREAD_PRIO_3,
            .stackDepth = 4096,
            .thrdname = "record",
        };
        rt = tal_thread_create_and_start(&__pt_record_thread, NULL, NULL, __pt_record_thread_entry, (VOID *)duration, &thrd_param);
        if (rt != OPRT_OK) {
            CHAR_T *ret_buf = tal_malloc(64);
            *ret_len = snprintf(ret_buf, 64, "{\"ret\":false}");
            *ret_data = (UCHAR_T *)ret_buf;
        } else {
            s_pt_record_test_start = TRUE;
        }
    } else {
        TAL_PR_DEBUG("__pt_record_thread_entry already exit, ignored!");
    }

    CHAR_T *ret_buf = tal_malloc(64);
    *ret_len = snprintf(ret_buf, 64, "{\"ret\":true}");
    *ret_data = (UCHAR_T *)ret_buf;
EXIT:
    ty_cJSON_Delete(root_json);
    return rt;
}

/**
* @brief module self item test handle
*/
typedef OPERATE_RET(*PT_MODULE_SELF_PROC_CB)(PT_CMD_DATA_S *cmd_data, UCHAR_T **ret_data, USHORT_T *ret_len, UCHAR_T *data_type);
typedef struct {
    CHAR_T *test_item;
    PT_MODULE_SELF_PROC_CB cb;
} PT_MODULE_SELF_HANDLE_S;

#define PT_TESTITEM_LOWPOWERMODE "LowPowerMode"
#define PT_TESTITEM_MANUAL_MIC_TEST "manual_mic_test"
#define PT_TESTITEM_PID_TEST "pid"
#define PT_TESTITEM_CHARGER_TEST "charger"
#define PT_TESTITEM_POWERVALUE_TEST "PowerValue"

static THREAD_HANDLE __ds_thread = NULL;
VOID_T __prod_deepsleep(VOID_T *args)
{
    tal_system_sleep(100);
    // call deepsleep func
    tal_cpu_sleep_mode_set(TRUE, TUYA_CPU_DEEP_SLEEP);
    while(1) tal_system_sleep(500);
}

STATIC OPERATE_RET __pt_entry_lowpower(PT_CMD_DATA_S *cmd_data, UCHAR_T **ret_data, USHORT_T *ret_len, UCHAR_T *data_type)
{
    CHAR_T *ret_buf = tal_malloc(128);
    TUYA_CHECK_NULL_RETURN(ret_buf, OPRT_MALLOC_FAILED);
    memset(ret_buf, 0, 128);

    char *enter_lp_succ_str = "{\"ret\":true}";
    memcpy(ret_buf, enter_lp_succ_str, strlen(enter_lp_succ_str));
    *ret_data = (UCHAR_T *)ret_buf;
    *ret_len = strlen(ret_buf);

    THREAD_CFG_T thrd_param = {0};
    thrd_param.priority = THREAD_PRIO_3;
    thrd_param.stackDepth = 4096;
    thrd_param.thrdname = "ds";
    tal_thread_create_and_start(&__ds_thread, NULL, NULL, __prod_deepsleep, NULL, &thrd_param);
    return OPRT_OK;
}

STATIC OPERATE_RET __pt_entry_pid_test(PT_CMD_DATA_S *cmd_data, UCHAR_T **ret_data, USHORT_T *ret_len, UCHAR_T *data_type)
{
    OPERATE_RET rt = OPRT_OK;
    CHAR_T fac[21] = {0};
    CHAR_T *out_data = NULL, *ret_buf = NULL;
    USHORT_T out_len = 0, malloc_ret_len = 0;

    // get pid
    rt = mf_test_facpin_get(fac);
    if (OPRT_OK != rt) {
        TAL_PR_ERR("get pid err");
        return rt;
    }
    out_len = strlen(fac);
    malloc_ret_len = 64 + strlen(cmd_data->testitem) + strlen(cmd_data->value) + out_len;
    ret_buf = tal_malloc(malloc_ret_len);
    TUYA_CHECK_NULL_RETURN(ret_buf, OPRT_MALLOC_FAILED);
    memset(ret_buf, 0, malloc_ret_len);
    *ret_len = sprintf(ret_buf, "{\"ret\":true,\"testItem\":\"%s\",\"type\":\"%s\",\"value\":\"%s\"}", cmd_data->testitem, cmd_data->value, fac);
    *ret_data = (UCHAR_T *)ret_buf;
    TAL_PR_NOTICE("ret len: %d, data:%s", *ret_len, ret_buf);
    return rt;
}

#if defined(TUYA_AI_TOY_BATTERY_ENABLE) || TUYA_AI_TOY_BATTERY_ENABLE == 1

STATIC OPERATE_RET __pt_entry_charger_test(PT_CMD_DATA_S *cmd_data, UCHAR_T **ret_data, USHORT_T *ret_len, UCHAR_T *data_type)
{
    OPERATE_RET rt = OPRT_OK;
    CHAR_T *ret_buf = NULL;
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_NONE;
    // BOOL_T is_chargeing = FALSE;

    tkl_gpio_read(TUYA_AI_TOY_CHARGE_PIN, &level);
    // is_chargeing = (level == TUYA_GPIO_LEVEL_LOW);

    ret_buf = tal_malloc(128);
    TUYA_CHECK_NULL_RETURN(ret_buf, OPRT_MALLOC_FAILED);
    memset(ret_buf, 0, 128);
    *ret_len = snprintf(ret_buf, 128, "{\"ret\":true,\"testItem\":\"%s\",\"type\":\"%s\",\"value\":\"%d\"}", cmd_data->testitem, cmd_data->value, level==TUYA_GPIO_LEVEL_LOW?1:0);
    *ret_data = (UCHAR_T *)ret_buf;
    TAL_PR_NOTICE("ret len: %d, data:%s", *ret_len, ret_buf);
    return rt;
}

STATIC UINT8_T s_battery_capacity = 0xff;
// static int _battery_callback(uint8_t current_capacity)
// {
//     s_battery_capacity = current_capacity;
//     return 0;
// }
STATIC OPERATE_RET __pt_entry_powervalue_test(PT_CMD_DATA_S *cmd_data, UCHAR_T **ret_data, USHORT_T *ret_len, UCHAR_T *data_type)
{
    OPERATE_RET rt = OPRT_OK;
    CHAR_T *ret_buf = NULL;
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_NONE;
    UINT8_T battery_capacity = 0XFF;
    // battery monitor init
    tuya_ai_toy_battery_init();
    STATIC int cnt = 0;
    while (cnt < 10) {
        s_battery_capacity = tuya_ai_battery_get_capacity();
        if (s_battery_capacity != 0xff) {
            battery_capacity = s_battery_capacity;
            break;
        }
        tal_system_sleep(100);
    }

    ret_buf = tal_malloc(128);
    TUYA_CHECK_NULL_GOTO(ret_buf, EXIT);
    memset(ret_buf, 0, 128);
    *ret_len = snprintf(ret_buf, 128, "{\"ret\":true,\"testItem\":\"%s\",\"type\":\"%s\",\"value\":\"%d\"}", cmd_data->testitem, cmd_data->value, battery_capacity);
    *ret_data = (UCHAR_T *)ret_buf;
    TAL_PR_NOTICE("ret len: %d, data:%s", *ret_len, ret_buf);

EXIT:
    tuya_ai_toy_battery_uninit();
    return rt;
}
#endif

STATIC PT_MODULE_SELF_HANDLE_S pt_self_test_arr[] = {
    { PT_TESTITEM_LOWPOWERMODE,     __pt_entry_lowpower},
    { PT_TESTITEM_PID_TEST,         __pt_entry_pid_test},
#if defined(TUYA_AI_TOY_BATTERY_ENABLE) && (TUYA_AI_TOY_BATTERY_ENABLE == 1)
    { PT_TESTITEM_CHARGER_TEST,     __pt_entry_charger_test},
    { PT_TESTITEM_POWERVALUE_TEST,  __pt_entry_powervalue_test},
#endif
};

OPERATE_RET ty_ai_prod_test_raw(PT_CMD_DATA_S *cmd_data, UCHAR_T **ret_data, USHORT_T *ret_len, UCHAR_T* data_type)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(cmd_data, OPRT_INVALID_PARM);

    UCHAR_T idx = 0;
    for (idx = 0; idx < CNTSOF(pt_self_test_arr); idx++) {
        if (cmd_data->testitem && (0 == strcmp(cmd_data->testitem, pt_self_test_arr[idx].test_item))) {
            rt = pt_self_test_arr[idx].cb(cmd_data, ret_data, ret_len, data_type);
            return rt;
        }
    }
    return OPRT_INVALID_PARM;
}

/**
 * @brief LED功能测试，0x0001
 * @note 上位机发送：1字节；0x00：全亮；0x01：全灭；0x02：交替闪烁（500ms）；
 *       设备返回：{"ret":true}/{"ret":false}；设备侧只反馈io操作执行结果。闪烁状态人为判断。
 */
//先发01全灭,之后发02(交替闪烁),模组回了之后弹出界面让人工确认,如果没回发6次02命令,人工确认后发01
STATIC OPERATE_RET __pt_led_test(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    CHAR_T value[16] = {0};
    sprintf(value, "%02x", *data);
    UCHAR_T led_type = atoi(value);
    TAL_PR_NOTICE("led type:%d", led_type);

    led_test_start(led_type);

    CHAR_T *ret_buf = tal_malloc(16);
    if (ret_buf) {
        *ret_len = sprintf((CHAR_T *)ret_buf, "{\"ret\":true}");
        *ret_data = (UCHAR_T *)ret_buf;
    }
    return;
}

/**
 * @brief 按键功能测试，0x0003
 * @note 上位机发送：无；设备返回：{"keyID":n}；n表示按键值，例如{"keyID":0} 、{"keyID":1}、{"keyID":2}
 *       异步测试，检测到按键则返回；多次返回。
 */
STATIC OPERATE_RET __pt_key_test(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    return gpio_test_start();
}

/**
* @brief 通用bool量测试，0xFF03
* @note 上位机发送:{"testItem":"serialPort1"};设备返回:{"ret":true}/{"ret":false,"errCode":500000}
*/
STATIC OPERATE_RET __pt_bool_test(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    OPERATE_RET rt = OPRT_OK;

    TAL_PR_NOTICE("bool test %s", data);
    ty_cJSON *root_json = ty_cJSON_Parse((CHAR_T *)data);
    TUYA_CHECK_NULL_RETURN(root_json, OPRT_CJSON_PARSE_ERR);
    ty_cJSON *json_item = ty_cJSON_GetObjectItem(root_json, "testItem");
    TUYA_CHECK_NULL_GOTO(json_item, EXIT);

    PT_CMD_DATA_S cmd_data = {
        .cmd = cmd,
        .timeout = 5 * 1000,
        .value_type = PT_VALUE_INVAILD,
        .cmd_type = PT_CMD_READ,
        .testitem = json_item->valuestring,
        .value = NULL,
        .ch = NULL,
        .default_ret = TRUE,
    };

    rt = ty_ai_prod_test_raw(&cmd_data, ret_data, ret_len, NULL);

EXIT:
    ty_cJSON_Delete(root_json);
    return rt;
}

/**
 * @brief 通用数据获取测试，0x0035
 * @note 上位机发送：{"testItem":"PowerValue", "type":"int"}
 *       设备返回：{"ret":true,"testItem":"PowerValue", "type":"int", "value":"10" } /
 *                {"ret":false,"errCode":500000}
 */
STATIC OPERATE_RET __pt_com_data_test(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    OPERATE_RET rt = OPRT_OK;
    CHAR_T *out_data = NULL, *ret_buf = NULL;
    USHORT_T out_len = 0, malloc_ret_len = 0;
    CHAR_T data_type = 0;

    TAL_PR_NOTICE("com data test %s", data);

    ty_cJSON *root_json = ty_cJSON_Parse((CHAR_T *)data);
    TUYA_CHECK_NULL_RETURN(root_json, OPRT_CJSON_PARSE_ERR);
    ty_cJSON *json_item = ty_cJSON_GetObjectItem(root_json, "testItem");
    TUYA_CHECK_NULL_GOTO(json_item, EXIT);
    ty_cJSON *json_type = ty_cJSON_GetObjectItem(root_json, "type");
    TUYA_CHECK_NULL_GOTO(json_type, EXIT);

    PT_CMD_DATA_S cmd_data = {
        .cmd = cmd,
        .timeout = 10 * 1000,
        .value_type = PT_VALUE_STRING,
        .cmd_type = PT_CMD_READ,
        .testitem = json_item->valuestring,
        .value = json_type->valuestring,
        .ch = NULL,
        .default_ret = FALSE,
    };

    rt = ty_ai_prod_test_raw(&cmd_data, &out_data, &out_len, &data_type);
    if (OPRT_OK == rt) {
        *ret_data = (UCHAR_T *)out_data;
        *ret_len = out_len;
    }

EXIT:
    ty_cJSON_Delete(root_json);
    return rt;
}

STATIC CONST PROD_TEST_OP_S pt_cmd_proc_arr[] = {
    { CMD_ENTER_OFFLINE_TEST,      __pt_enter               },  // 进入测试模式
    { CMD_EXIT_OFFLINE_TEST,       __pt_exit                },  // 退出测试模式
    // {}, // 固件版本检测
    { CMD_GET_DEV_MAC,             __pt_get_mac             }, //module
    { CMD_GET_WIFI_RSSI,           __pt_get_wifi_rssi       }, //module
    { CMD_GET_COM_RSSI,            __pt_get_com_rssi        }, //com rssi
    { CMD_GET_FIRM_INFO,           __pt_get_firm_info       }, //module
    // { CMD_RELAY_TEST,              __pt_relay_test          },
    // { CMD_WR_ELEC_STAT,            __pt_wr_elec_stat        },
    // { CMD_MOTOR_TEST,              __pt_motor_test          },
    // { CMD_ELEC_CHCK,               __pt_elec_check          },
    // { CMD_AGING_TEST,              __pt_aging_test          },
    // { CMD_ANALOG_SENSOR_TEST,      __pt_analog_sensor_test  },
    // { CMD_BEEP_TEST,               __pt_beep_test           },
    // { CMD_COM_SWITCH_TEST,         __pt_com_switch_test     },
    { CMD_RECORD_TEST,             __pt_record_test         },
    // { CMD_GEAR_TEST,               __pt_gear_test           },
    // { CMD_GET_RF_RSSI,             __pt_rf_rssi_test        },
    // { CMD_COM_DATA_CFG,            __pt_data_config_test    },
    { CMD_LED_TEST,                __pt_led_test            },
    { CMD_KEY_TEST,                __pt_key_test            },
    // { CMD_IR_SEND_TEST,            __pt_ir_send_test        },
    // { CMD_IR_RECV_TEST,            __pt_ir_recv_test        },
    // { CMD_IR_MODE_CHANGE,          __pt_ir_mode_change_test },
    { CMD_BOOL_TEST,               __pt_bool_test           },
    { CMD_COM_TEST,                __pt_com_data_test       },
    // { CMD_RAW_TEST,                __pt_com_raw_test        },
    // { CMD_UPGRADE_START,           __pt_start_ota_test      },
    // { CMD_UPGRADE_END,             __pt_end_ota_test        },

    // { CMD_ENTER_OFFLINE_TEST,      __pt_enter               },
    // { CMD_EXIT_OFFLINE_TEST,       __pt_exit                },
    // { CMD_RAW_TEST,                __pt_com_raw_test        },
};

/**
 * @brief mf_user_product_test_cb 是成品产测回调接口
 * 
 * @return VOID_T 
 * 
 * @note 应用必须对其进行实现，如果不需要，则实现空函数
 * 
 */
STATIC OPERATE_RET ty_ai_toy_mf_test_cmd_proc(USHORT_T cmd, UCHAR_T *data, UINT_T len, UCHAR_T **ret_data, USHORT_T *ret_len)
{
    __mf_init();  // mf inif

    OPERATE_RET rt = OPRT_OK;
    TAL_PR_NOTICE("ty_gfw_mf_test_cmd_proc cmd:%d, data:%*s, len:%d", cmd, len, data, len);
    UCHAR_T idx = 0;
    for (idx = 0; idx < CNTSOF(pt_cmd_proc_arr); idx++) {
        if (cmd == (USHORT_T)pt_cmd_proc_arr[idx].cmd) {
            if (pt_cmd_proc_arr[idx].func) {
                return pt_cmd_proc_arr[idx].func(cmd, data, len, ret_data, ret_len);
            }
        }
    }
    TAL_PR_NOTICE("cmd not support");
    return OPRT_OK;
}

STATIC THREAD_HANDLE __led_thread = NULL;
STATIC UINT8_T led_type = 0;
STATIC UINT8_T led_status = 0;
typedef enum {
    FACTORY_TEST_STATUS_IDLE = 0,
    FACTORY_TEST_STATUS_START,
    FACTORY_TEST_STATUS_RUNNING,
    FACTORY_TEST_STATUS_STOP,
    FACTORY_TEST_STATUS_SIGNAL_ERR,
} FACTORY_TEST_STATUS_E;
STATIC FACTORY_TEST_STATUS_E s_factory_test_status = FACTORY_TEST_STATUS_IDLE;
STATIC BOOL_T s_factory_test = FALSE;
STATIC BOOL_T s_gpio_test = FALSE;
STATIC BOOL_T s_led_test = FALSE;
STATIC THREAD_HANDLE __factory_thread = NULL;

STATIC VOID_T __mf_factory_thread_entry(VOID *arg)
{
    OPERATE_RET rt = OPRT_OK;

    __mf_audio_init();
    // 播放音频
    // t5_mp3_player_init();
    TUYA_SPEAKER_SERVICE_CONFIG_S player_cfg = {0};
    tuya_speaker_service_init(&player_cfg);    

    SYS_TIME_T end_time = tal_system_get_millisecond() + PROD_TEST_TIMEOUT;

    // 播放提示音
    while (tal_system_get_millisecond() < end_time || tuya_speaker_service_is_playing()) {
        if (tuya_speaker_service_is_playing()) {
            TAL_PR_DEBUG("T5 is playing");
            tal_system_sleep(1000);
            continue;
        }
        TAL_PR_DEBUG("play alert");
        ty_ai_toy_alert(TOY_ALERT_TYPE_POWER_ON, TRUE);
    }
    TAL_PR_DEBUG("play alert end");
    tuya_speaker_service_deinit();
    __mf_audio_uninit();
    s_factory_test_status = FACTORY_TEST_STATUS_STOP;
    return OPRT_OK;
}

STATIC VOID_T __mf_thread_entry(VOID *arg)
{
    UINT32_T cnt = 0;

    ty_ble_init_for_mftest();

    // init led gpio
    TUYA_GPIO_BASE_CFG_T cfg;
    cfg.mode = TUYA_GPIO_PULLDOWN;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(TUYA_AI_TOY_LED_PIN, &cfg);


#if defined(TUYA_AI_TOY_BATTERY_ENABLE) || TUYA_AI_TOY_BATTERY_ENABLE == 1
    // 充电状态IO
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.mode = TUYA_GPIO_FLOATING;
    tkl_gpio_init(TUYA_AI_TOY_CHARGE_PIN, &cfg);
#endif

    // init key gpios
    for (UCHAR_T i = 0; i < CNTSOF(key_gpios); i++) {
        cfg.mode = TUYA_GPIO_PULLUP;
        cfg.direct = TUYA_GPIO_INPUT;
        cfg.level = TUYA_GPIO_LEVEL_HIGH;
        tkl_gpio_init(key_gpios[i], &cfg);
    }

    // set mf test flag
    s_mf_init_flag = TRUE;

    while (1) {
        if (s_factory_test) {
            if (FACTORY_TEST_STATUS_SIGNAL_ERR == s_factory_test_status) {
                TAL_PR_DEBUG("signal error");
                // turn off led
                led_test_start(0);
            } else if (FACTORY_TEST_STATUS_START == s_factory_test_status) {
                // start factory test
                TAL_PR_DEBUG("start factory test");
                // 设置led状态
                led_test_start(2);
                s_factory_test_status = FACTORY_TEST_STATUS_RUNNING;
                // create factory test thread
                THREAD_CFG_T thrd_param = {
                    .priority = THREAD_PRIO_3,
                    .stackDepth = 4096,
                    .thrdname = "mf_factory_thread"
                };
                tal_thread_create_and_start(&__factory_thread, NULL, NULL, __mf_factory_thread_entry, NULL, &thrd_param);
            } else if (FACTORY_TEST_STATUS_STOP == s_factory_test_status) {
                // stop factory test
                // 设置led状态
                TAL_PR_DEBUG("stop factory test");
                led_test_start(1);
                s_factory_test_status = FACTORY_TEST_STATUS_IDLE;
            }
        }

        if (s_led_test) {
            // led blink
            if (cnt % 5 == 0) {
                if (led_type == 2) {
                    led_status = !led_status;
                } else if (led_type == 1) {
                    led_status = 1;
                } else if (led_type == 0) {
                    led_status = 0;
                }
                // set led status
                tkl_gpio_write(TUYA_AI_TOY_LED_PIN, led_status);
            }
        }
        // gpio test
        if (s_gpio_test) {
            TUYA_GPIO_LEVEL_E read_level = TUYA_GPIO_LEVEL_HIGH;
            for (UCHAR_T i = 0; i < CNTSOF(key_gpios); i++) {
                tkl_gpio_read(key_gpios[i], &read_level);
                if (key_level[i] == read_level) {
                    continue;
                }

                if (TUYA_GPIO_LEVEL_LOW == read_level) {
                    TAL_PR_DEBUG("key %d pressed", key_gpios[i]);
                    CHAR_T buf[16] = {0};
                    UINT32_T len = sprintf(buf, "{\"keyID\":%d}", key_ids[i]);
                    mf_cmd_send(0xF0, (BYTE_T *)buf, len);
                } else {
                    TAL_PR_DEBUG("key %d released", key_gpios[i]);
                }
                key_level[i] = read_level;
            }
        }

        tal_system_sleep(100);
        cnt++;
    }
    return OPRT_OK;
}

STATIC INT_T _audio_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    if (!record_start)
        return 1;

    // TAL_PR_DEBUG("audio frame put, size:%d, record_start:%d, stream_ringbuf=%p", pframe->used_size, record_start, stream_ringbuf);
    if (!stream_ringbuf) {
        TAL_PR_ERR("stream_ringbuf is NULL");
        return 1;
    }

    tuya_ring_buff_write(stream_ringbuf, pframe->pbuf, pframe->used_size);
    return 0;
}

STATIC OPERATE_RET __mf_audio_init(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_AUDIO_CONFIG_T config;
    config.enable = 0;
    config.ai_chn = 0;
    config.sample = AUDIO_SAMPLE_RATE;
    config.spk_sample = SPK_SAMPLE_RATE;
    config.datebits = AUDIO_SAMPLE_BITS;
    config.channel = AUDIO_CHANNEL;
    config.codectype = TKL_CODEC_AUDIO_PCM;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.spk_gpio = s_cfg.spk_en_pin;
    config.spk_gpio_polarity = TUYA_GPIO_LEVEL_LOW;
    config.put_cb = _audio_frame_put;

    TAL_PR_NOTICE("tkl_ai_init...");
    // open audio
    TUYA_CALL_ERR_GOTO(tkl_ai_init(&config, 0), err);

    TAL_PR_NOTICE("tkl_ai_start...");
    TUYA_CALL_ERR_GOTO(tkl_ai_start(0, 0), err);

    // set mic/spk volume
    tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 100);
    // tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, 70);

    return OPRT_OK;
err:
    tkl_ai_stop(TKL_AUDIO_TYPE_BOARD, 0);
    tkl_ai_uninit();
    return rt;
}

STATIC OPERATE_RET __mf_audio_uninit(VOID)
{
    tkl_ai_stop(TKL_AUDIO_TYPE_BOARD, 0);
    tkl_ai_uninit();
    return OPRT_OK;
}

STATIC OPERATE_RET __mf_init(VOID)
{
    // init led test
    if (!s_mf_init_flag) {
        // create led test thread
        THREAD_CFG_T thrd_param = {
            .priority = THREAD_PRIO_3,
            .stackDepth = 4096,
            .thrdname = "mf_thread"
        };
        OPERATE_RET rt = tal_thread_create_and_start(&__led_thread, NULL, NULL, __mf_thread_entry, NULL, &thrd_param);
        if (rt != OPRT_OK) {
            TAL_PR_ERR("create mf thread failed, ret=%d", rt);
            return rt;
        }
        while (!s_mf_init_flag) {
            tal_system_sleep(20);
        }
        TAL_PR_NOTICE("__mf_init done");
    }

    return OPRT_OK;
}

STATIC OPERATE_RET led_test_start(UINT8_T type)
{
    s_led_test = TRUE;
    led_type = type;
    return OPRT_OK;
}

STATIC OPERATE_RET gpio_test_start(VOID)
{
    s_gpio_test = TRUE;
    return OPRT_OK;
}


STATIC prodtest_ssid_info_t *__ty_app_find_target_ssid(CHAR_T *target_ssid, prodtest_ssid_info_t *wifi_arr, UINT8_T arr_cnt)
{
    int rt = 0;
    UINT8_T i = 0 ;

    if(NULL == target_ssid || NULL == wifi_arr || 0 == arr_cnt) {
        return NULL;
    }

    for(i = 0;  i<arr_cnt; i++) {
        TAL_PR_DEBUG("ssid:%s, target ssid:%s", wifi_arr[i].ssid, target_ssid);
        rt = strcmp(wifi_arr[i].ssid, target_ssid);
        TAL_PR_DEBUG("rt: %d", rt);
        if(0 == strcmp(wifi_arr[i].ssid, target_ssid)) {
            return &wifi_arr[i];
        }
    }

    return NULL;
}

//扫描到产测热点回调
STATIC OPERATE_RET ty_ai_toy_mf_user_ssid_info_cb(INT_T flag, prodtest_ssid_info_t *ssid_info, UCHAR_T info_count)
{
    prodtest_ssid_info_t *p_ssid_info = NULL;

    TAL_PR_NOTICE("ssid info count:%d", info_count);
    for (UCHAR_T i = 0; i < info_count; i++) {
        TAL_PR_NOTICE("ssid:%s, rssi:%d", ssid_info[i].ssid, ssid_info[i].rssi);
    }

    p_ssid_info = __ty_app_find_target_ssid(PRODUCT_TEST_WIFI_1, ssid_info, info_count);
    if (!p_ssid_info) {
        TAL_PR_ERR("can't find target ssid");
        return OPRT_COM_ERROR;
    }

    TAL_PR_DEBUG("target ssid:%s, rssi:%d", p_ssid_info->ssid, p_ssid_info->rssi);
    s_factory_test = TRUE;
    if (p_ssid_info->rssi < PROD_TEST_WEAK_SIGNAL) {
        s_factory_test_status = FACTORY_TEST_STATUS_SIGNAL_ERR;
    } else {
        __mf_init(); // mf inif
        s_factory_test_status = FACTORY_TEST_STATUS_START;
    }

    return OPRT_OK;
}

VOID ty_ai_toy_mf_test_init(TY_AI_TOY_CFG_T *cfg)
{
    if (cfg) {
        memcpy(&s_cfg, cfg, SIZEOF(TY_AI_TOY_CFG_T));
    }

    prodtest_app_cfg_t pt_cfg_param = {
        .gwcm_mode = GWCM_OLD_PROD,
        .ssid_list = s_WIFI_SCAN_SSID_LIST,
        .ssid_count = CNTSOF(s_WIFI_SCAN_SSID_LIST),
        .file_name = APP_BIN_NAME,
        .file_ver = USER_SW_VER,
        .app_cb = ty_ai_toy_mf_user_ssid_info_cb,
        .product_cb = ty_ai_toy_mf_test_cmd_proc,
        .prod_ssid = PRODUCT_TEST_WIFI,
    };
    prodtest_app_register(&pt_cfg_param);
    return OPRT_OK;
}
