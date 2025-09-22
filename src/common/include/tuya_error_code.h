/**
 * @file tuya_error_code.h
 * @brief Defines the error codes used in the Tuya SDK.
 *
 * The error codes in this file are used across various modules of the SDK to
 * indicate the result of operations. These error codes cover a range of issues
 * from network errors to memory allocation failures. Each error code has a
 * corresponding negative value, which is returned in function calls to indicate
 * a specific type of error.
 *
 * The error codes are divided into several main sections, each corresponding to
 * a module or functional area of the SDK:
 * - Global error codes (GLOBAL)
 * - Base operating system adapter error codes (BASE_OS_ADAPTER)
 * - Base utilities error codes (BASE_UTILITIES)
 * - Base security error codes (BASE_SECURITY)
 * - Base log management error codes (BASE_LOG_MNG)
 * - Base time queue error codes (BASE_TIMEQ)
 * - Base message queue error codes (BASE_MSGQ)
 * - Middleware HTTP error codes (MID_HTTP)
 * - Middleware MQTT error codes (MID_MQTT)
 *
 * Using these error codes can help developers quickly locate problems and
 * improve development efficiency.
 *
 *  @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */

#ifndef TUYA_ERROR_CODE_H
#define TUYA_ERROR_CODE_H

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off

/****************************************************************************
            the error code marco define for module GLOBAL
****************************************************************************/
#define OPRT_OK                       (0x0000)  // 0, Execution successful
#define OPRT_COM_ERROR                (-0x0001) //-1, Invalid parameter
#define OPRT_INVALID_PARM             (-0x0002) //-2, Invalid parameter
#define OPRT_MALLOC_FAILED            (-0x0003) //-3, Memory allocation failed
#define OPRT_NOT_SUPPORTED            (-0x0004) //-4, Not supported
#define OPRT_NETWORK_ERROR            (-0x0005) //-5, Network error
#define OPRT_NOT_FOUND                (-0x0006) //-6, Object not found
#define OPRT_CR_CJSON_ERR             (-0x0007) //-7, Failed to create JSON object
#define OPRT_CJSON_PARSE_ERR          (-0x0008) //-8, JSON parsing failed
#define OPRT_CJSON_GET_ERR            (-0x0009) //-9, Failed to get JSON object
#define OPRT_CR_MUTEX_ERR             (-0x000a) //-10, Failed to create mutex
#define OPRT_SOCK_ERR                 (-0x000b) //-11, Socket creation failed
#define OPRT_SET_SOCK_ERR             (-0x000c) //-12, Socket setting failed
#define OPRT_SOCK_CONN_ERR            (-0x000d) //-13, Socket connection failed
#define OPRT_SEND_ERR                 (-0x000e) //-14, Sending failed
#define OPRT_RECV_ERR                 (-0x000f) //-15, Receiving failed
#define OPRT_RECV_DA_NOT_ENOUGH       (-0x0010) //-16, Received data not enough
#define OPRT_KVS_WR_FAIL              (-0x0011) //-17, KV store write failed
#define OPRT_KVS_RD_FAIL              (-0x0012) //-18, KV store read failed
#define OPRT_CRC32_FAILED             (-0x0013) //-19, CRC check failed
#define OPRT_TIMEOUT                  (-0x0014) //-20, Timeout
#define OPRT_INIT_MORE_THAN_ONCE      (-0x0015) //-21, Initialized more than once
#define OPRT_INDEX_OUT_OF_BOUND       (-0x0016) //-22, Index out of bound
#define OPRT_RESOURCE_NOT_READY       (-0x0017) //-23, Resource not ready
#define OPRT_EXCEED_UPPER_LIMIT       (-0x0018) //-24, Exceed upper limit
#define OPRT_FILE_NOT_FIND            (-0x0019) //-25, File not found
#define OPRT_FILE_IS_FULL             (-0x001a) //-26, File is full
#define OPRT_FILE_OPEN_FAILED         (-0x001b) //-27, File open failed
#define OPRT_FILE_READ_FAILED         (-0x001c) //-28, File read failed
#define OPRT_FILE_WRITE_FAILED        (-0x001d) //-29, File write failed
#define OPRT_NOT_EXIST                (-0x001e) //-30, Not exist
#define OPRT_BUFFER_NOT_ENOUGH        (-0x001f) //-31, Buffer not enough
#define OPRT_ROUTER_NOT_FOUND         (-0x0020) //-32, Router not found
#define OPRT_AUTHENTICATION_NOT_FOUND (-0x0021) //-33, Authentication not found
#define OPRT_AUTHENTICATION_FAIL      (-0x0022) //-34, Authentication failed
#define OPRT_MSG_OUT_OF_LIMIT         (-0x0023) //-35, Message out of limit
#define OPRT_VERSION_FMT_ERR          (-0x0024) //-36, Version format error
#define OPRT_DIR_OPEN_FAILED          (-0x0025) //-37, DIR open failed
#define OPRT_DIR_READ_FAILED          (-0x0026) //-38, DIR read failed
#define OPRT_EOD                      (-0x0027) //-39, DIR END
#define OPRT_GLOBAL_ERRCODE_MAX_CNT   40

/****************************************************************************
            the error code marco define for module BASE_OS_ADAPTER
****************************************************************************/
#define OPRT_BASE_OS_ADAPTER_REG_NULL_ERROR  (-0x0100) //-256, System adapter registration failed
#define OPRT_BASE_OS_ADAPTER_ERRCODE_MAX_CNT 1

/****************************************************************************
            the error code marco define for module BASE_UTILITIES
****************************************************************************/
#define OPRT_BASE_UTILITIES_PARTITION_EMPTY     (-0x0200) //-512, No free linked list
#define OPRT_BASE_UTILITIES_PARTITION_FULL      (-0x0201) //-513, Linked list is full
#define OPRT_BASE_UTILITIES_PARTITION_NOT_FOUND (-0x0202) //-514, Linked list not traversed
#define OPRT_BASE_UTILITIES_ERRCODE_MAX_CNT     3

/****************************************************************************
            the error code marco define for module BASE_SECURITY
****************************************************************************/
#define OPRT_BASE_SECURITY_CRC32_FAILED    (-0x0300) //-768, CRC32 calculation failed
#define OPRT_BASE_SECURITY_ERRCODE_MAX_CNT 1

/****************************************************************************
            the error code marco define for module BASE_LOG_MNG
****************************************************************************/
#define OPRT_BASE_LOG_MNG_DONOT_FOUND_MODULE      (-0x0400) //-1024, Log module not found
#define OPRT_BASE_LOG_MNG_PRINT_LOG_LEVEL_HIGHER  (-0x0401) //-1025, Log level low
#define OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED    (-0x0402) //-1026, Log string formatting failed
#define OPRT_BASE_LOG_MNG_LOG_SEQ_OPEN_FILE_FAIL  (-0x0403) //-1027, Opening log sequence file failed
#define OPRT_BASE_LOG_MNG_LOG_SEQ_WRITE_FILE_FAIL (-0x0404) //-1028, Writing log sequence file failed
#define OPRT_BASE_LOG_MNG_LOG_SEQ_FILE_FULL       (-0x0405) //-1029, Log sequence file full
#define OPRT_BASE_LOG_MNG_LOG_SEQ_FILE_NOT_EXIST  (-0x0406) //-1030, Log sequence file does not exist
#define OPRT_BASE_LOG_MNG_LOG_SEQ_NAME_INVALIDE   (-0x0407) //-1031, Log sequence name invalid
#define OPRT_BASE_LOG_MNG_LOG_SEQ_CREATE_FAIL     (-0x0408) //-1032, Log sequence creation failed
#define OPRT_BASE_LOG_MNG_ERRCODE_MAX_CNT         9

/****************************************************************************
            the error code marco define for module BASE_TIMEQ
****************************************************************************/
#define OPRT_BASE_TIMEQ_TIMERID_EXIST          (-0x0600) //-1536, Timer ID already exists
#define OPRT_BASE_TIMEQ_TIMERID_NOT_FOUND      (-0x0601) //-1537, Specified timer ID not found
#define OPRT_BASE_TIMEQ_TIMERID_UNVALID        (-0x0602) //-1538, Timer ID invalid
#define OPRT_BASE_TIMEQ_GET_IDLE_TIMERID_ERROR (-0x0603) //-1539, Error getting idle timer ID
#define OPRT_BASE_TIMEQ_ERRCODE_MAX_CNT        4

/****************************************************************************
            the error code marco define for module BASE_MSGQ
****************************************************************************/
#define OPRT_BASE_MSGQ_NOT_FOUND       (-0x0700) //-1792, Message not found
#define OPRT_BASE_MSGQ_LIST_EMPTY      (-0x0701) //-1793, List is empty
#define OPRT_BASE_MSGQ_ERRCODE_MAX_CNT 2

/****************************************************************************
            the error code marco define for module MID_HTTP
****************************************************************************/
#define OPRT_MID_HTTP_BUF_NOT_ENOUGH      (-0x0800) //-2048, Buffer length insufficient
#define OPRT_MID_HTTP_URL_PARAM_OUT_LIMIT (-0x0801) //-2049, URL length exceeds limit
#define OPRT_MID_HTTP_OS_ERROR            (-0x0802) //-2050, OS error
#define OPRT_MID_HTTP_PR_REQ_ERROR        (-0x0803) //-2051, Prepare request error, please check if the network is normal
#define OPRT_MID_HTTP_SD_REQ_ERROR        (-0x0804) //-2052, Send request error, please check if the network is normal
#define OPRT_MID_HTTP_RD_ERROR            (-0x0805) //-2053, Read error, please check if the network is normal
#define OPRT_MID_HTTP_AD_HD_ERROR         (-0x0806) //-2054, Add header error
#define OPRT_MID_HTTP_GET_RESP_ERROR      (-0x0807) //-2055, Get response error
#define OPRT_MID_HTTP_AES_INIT_ERR        (-0x0808) //-2056, AES initialization error
#define OPRT_MID_HTTP_AES_OPEN_ERR        (-0x0809) //-2057, AES open error
#define OPRT_MID_HTTP_AES_SET_KEY_ERR     (-0x080a) //-2058, AES set key error
#define OPRT_MID_HTTP_AES_ENCRYPT_ERR     (-0x080b) //-2059, AES encryption error
#define OPRT_MID_HTTP_CR_HTTP_URL_H_ERR   (-0x080c) //-2060, Create HTTP URL header error
#define OPRT_MID_HTTP_HTTPS_HANDLE_FAIL   (-0x080d) //-2061, HTTPS handle error
#define OPRT_MID_HTTP_HTTPS_RESP_UNVALID  (-0x080e) //-2062, HTTPS invalid response
#define OPRT_MID_HTTP_NO_SUPPORT_RANGE    (-0x080f) //-2063, No support for range
#define OPRT_MID_HTTP_ERRCODE_MAX_CNT     16

/****************************************************************************
            the error code marco define for module MID_MQTT
****************************************************************************/
#define OPRT_MID_MQTT_DEF_ERR                   (-0x0900) //-2304, Definition failed
#define OPRT_MID_MQTT_INVALID_PARM              (-0x0901) //-2305, Invalid parameter
#define OPRT_MID_MQTT_MALLOC_FAILED             (-0x0902) //-2306, Memory allocation failed
#define OPRT_MID_MQTT_DNS_PARSED_FAILED         (-0x0903) //-2307, DNS resolution failed, please check if the network is normal
#define OPRT_MID_MQTT_SOCK_CREAT_FAILED         (-0x0904) //-2308, socket creation failed
#define OPRT_MID_MQTT_SOCK_SET_FAILED           (-0x0905) //-2309, socket set failed
#define OPRT_MID_MQTT_TCP_CONNECD_FAILED        (-0x0906) //-2310, tcp connection failed, please check if the network is normal
#define OPRT_MID_MQTT_TCP_TLS_CONNECD_FAILED    (-0x0907) //-2311, tcp tls connection failed, please check if the network is normal
#define OPRT_MID_MQTT_PACK_SEND_FAILED          (-0x0908) //-2312, packet sending failed, please check if the network is normal
#define OPRT_MID_MQTT_RECV_DATA_FORMAT_WRONG    (-0x0909) //-2313, received data format error
#define OPRT_MID_MQTT_MSGID_NOT_MATCH           (-0x090a) //-2314, received data msgid not found
#define OPRT_MID_MQTT_START_TM_MSG_ERR          (-0x090b) //-2315, start event msg error
#define OPRT_MID_MQTT_OVER_MAX_MESSAGE_LEN      (-0x090c) //-2316, over maximum message length
#define OPRT_MID_MQTT_PING_SEND_ERR             (-0x090d) //-2317, ping send failed, please check if the network is normal
#define OPRT_MID_MQTT_PUBLISH_TIMEOUT           (-0x090e) //-2318, publish timeout, please check if the network is normal
#define OPRT_MID_MQTT_ERRCODE_MAX_CNT           15

/****************************************************************************
            the error code marco define for module MID_TLS
****************************************************************************/
#define OPRT_MID_TLS_NET_SOCKET_ERROR               (-0x0a00) //-2560, Failed to open a socket
#define OPRT_MID_TLS_NET_CONNECT_ERROR              (-0x0a01) //-2561, The connection to the given server / port failed.
#define OPRT_MID_TLS_UNKNOWN_HOST_ERROR             (-0x0a02) //-2562, Failed to get an IP address for the given hostname.
#define OPRT_MID_TLS_CONNECTION_ERROR               (-0x0a03) //-2563, TLS connection failed, please check if the network is normal
#define OPRT_MID_TLS_DRBG_ENTROPY_ERROR             (-0x0a04) //-2564, mbedtls random seed generation failed
#define OPRT_MID_TLS_X509_ROOT_CRT_PARSE_ERROR      (-0x0a05) //-2565, X509 root certificate parsing failed
#define OPRT_MID_TLS_X509_DEVICE_CRT_PARSE_ERROR    (-0x0a06) //-2566, X509 device certificate parsing failed
#define OPRT_MID_TLS_CTR_DRBG_ENTROPY_SOURCE_ERROR  (-0x0a07) //-2567, The entropy source failed
#define OPRT_MID_TLS_PK_PRIVATE_KEY_PARSE_ERROR     (-0x0a08) //-2568, Private key parsing failed
#define OPRT_MID_TLS_ERRCODE_MAX_CNT                9

/****************************************************************************
            the error code marco define for module SVC_WIFI
****************************************************************************/
#define OPRT_SVC_WIFI_RECV_CONTINUE         (-0x0b00) //-2816, Continue receiving the network configuration package
#define OPRT_SVC_WIFI_DONOT_FOUND_MODULE    (-0x0b01) //-2817, Module not found
#define OPRT_SVC_WIFI_PEGASUS_DECODE_FAILED (-0x0b02) //-2818, Lightning package decoding error
#define OPRT_SVC_WIFI_NEED_FACTORY_RESET    (-0x0b03) //-2819, WiFi initialization configuration verification failed, need to restore factory settings
#define OPRT_SVC_WIFI_ERRCODE_MAX_CNT       4

/****************************************************************************
            the error code marco define for module SVC_MF_TEST
****************************************************************************/
#define OPRT_SVC_MF_TEST_UPDATE_DATA_LEN_EXECED (-0x0d00) //-3328, Upgrade data length exceeds processing limit
#define OPRT_SVC_MF_TEST_UPDATE_CRC_ERROR       (-0x0d01) //-3329, Upgrade CRC check failed
#define OPRT_SVC_MF_TEST_ERRCODE_MAX_CNT        2

/****************************************************************************
            the error code marco define for module SVC_DP
****************************************************************************/
#define OPRT_SVC_DP_ALREADY_PROCESS     (-0x0e00) //-3584, DP has been processed
#define OPRT_SVC_DP_ID_NOT_FOUND        (-0x0e01) //-3585, DP ID not found, please check if the reported DP ID is correct
#define OPRT_SVC_DP_TP_NOT_MATCH        (-0x0e02) //-3586, DP type does not match, please check if the reported DP type is correct
#define OPRT_SVC_DP_DEVICE_NOT_BINDED   (-0x0e03) //-3587, Device not bound
#define OPRT_SVC_DP_TYPE_PROP_ILLEGAL   (-0x0e04) //-3588, Type property illegal, enumeration value does not exist
#define OPRT_SVC_DP_NW_INVALID          (-0x0e05) //-3589, Network invalid, please check if the network is normal
#define OPRT_SVC_DP_SECURITY_VERIFY_ERR (-0x0e06) //-3590, Security verification failed
#define OPRT_SVC_DP_REPORT_FINISH       (-0x0e07) //-3591, DP report finished
#define OPRT_SVC_DP_ERRCODE_MAX_CNT     8

/****************************************************************************
            the error code marco define for module BASE_EVENT
****************************************************************************/
#define OPRT_BASE_EVENT_INVALID_EVENT_NAME (-0x0f00) //-3840, Invalid event name
#define OPRT_BASE_EVENT_INVALID_EVENT_DESC (-0x0f01) //-3841, Invalid event description
#define OPRT_BASE_EVENT_ERRCODE_MAX_CNT    2

/****************************************************************************
            the error code marco define for module SVC_TIMER_TASK
****************************************************************************/
#define OPRT_SVC_TIMER_TASK_LOAD_INVALID_CJSON          (-0x1000) //-4096, The data saved in K/V is in incorrect JSON format
#define OPRT_SVC_TIMER_TASK_LOAD_INVALID_DATA           (-0x1001) //-4097, The data saved in K/V lacks the cnt field
#define OPRT_SVC_TIMER_TASK_UPDATE_LAST_FETCH_INVALID   (-0x1002) //-4098, The data returned by the cloud lacks the lastFetchTime field
#define OPRT_SVC_TIMER_TASK_UPDATE_TIMER_CNT_INVALID    (-0x1003) //-4099, The data returned by the cloud lacks the count field
#define OPRT_SVC_TIMER_TASK_UPDATE_TIMER_CNT_EXCEED     (-0x1004) //-4100, The number of timing tasks returned by the cloud exceeds 30
#define OPRT_SVC_TIMER_TASK_ERRCODE_MAX_CNT             5

/****************************************************************************
            the error code marco define for module SVC_HTTP
****************************************************************************/
#define OPRT_SVC_HTTP_NOT_ENCRYPT_RET                       (-0x1100) //-4352, Result not encrypted, please check if you have access permission
#define OPRT_SVC_HTTP_FILL_URL_H_ERR                        (-0x1101) //-4353, Construct header error
#define OPRT_SVC_HTTP_FILL_URL_FULL_ERR                     (-0x1102) //-4354, Construct entire URL error
#define OPRT_SVC_HTTP_FILL_DATA_ERR                         (-0x1103) //-4355, Construct data error
#define OPRT_SVC_HTTP_URL_CFG_AI_SPEAKER_ERR                (-0x1104) //-4356, AI speaker configuration failed
#define OPRT_SVC_HTTP_URL_CFG_URL_ERR                       (-0x1105) //-4357, httpUrl/mqttUrl field missing
#define OPRT_SVC_HTTP_URL_CFG_URL2IP_ERR                    (-0x1106) //-4358, httpUrl/mqttUrl resolution error
#define OPRT_SVC_HTTP_URL_CFG_URL2IP_SELF_ERR               (-0x1107) //-4359, httpsSelfUrl/mqttsSelfUrl resolution error
#define OPRT_SVC_HTTP_URL_CFG_URL2IP_VERIFY_ERR             (-0x1108) //-4360, httpsVerifyUrl/mqttsVerifyUrl resolution error
#define OPRT_SVC_HTTP_URL_CFG_URL2IP_PSK_ERR                (-0x1109) //-4361, httpsPSKUrl/mqttsPSKUrl resolution error
#define OPRT_SVC_HTTP_RECV_ERR                              (-0x110a) //-4362, Receive data error, please check if the network is normal
#define OPRT_SVC_HTTP_RECV_DA_NOT_ENOUGH                    (-0x110b) //-4363, Received data not enough
#define OPRT_SVC_HTTP_API_VERIFY_FAILED                     (-0x110c) //-4364, Data verification error
#define OPRT_SVC_HTTP_GW_NOT_EXIST                          (-0x110d) //-4365, Gateway information does not exist
#define OPRT_SVC_HTTP_API_TOKEN_EXPIRE                      (-0x110e) //-4366, TOKEN expired
#define OPRT_SVC_HTTP_DEV_RESET_FACTORY                     (-0x110f) //-4367, Device needs to be reset to factory settings
#define OPRT_SVC_HTTP_DEV_NEED_REGISTER                     (-0x1110) //-4368, Device not registered
#define OPRT_SVC_HTTP_ORDER_EXPIRE                          (-0x1111) //-4369, Order expired
#define OPRT_SVC_HTTP_NOT_EXISTS                            (-0x1112) //-4370, Does not exist
#define OPRT_SVC_HTTP_SIGNATURE_ERROR                       (-0x1113) //-4371, Signature error
#define OPRT_SVC_HTTP_API_VERSION_WRONG                     (-0x1114) //-4372, API version wrong
#define OPRT_SVC_HTTP_DEVICE_REMOVED                        (-0x1115) //-4373, Device removed
#define OPRT_SVC_HTTP_DEV_ALREADY_BIND                      (-0x1116) //-4374, Device already bound
#define OPRT_SVC_HTTP_REMOTE_API_RUN_UNKNOW_FAILED          (-0x1117) //-4375, Unrecognizable API
#define OPRT_SVC_HTTP_FORMAT_STRING_FAILED                  (-0x1118) //-4376, String formatting error
#define OPRT_SVC_HTTP_API_DECODE_FAILED                     (-0x1119) //-4377, Data decryption failed
#define OPRT_SVC_HTTP_SERV_VRFY_FAIL                        (-0x111a) //-4378, Server verification failed
#define OPRT_SVC_HTTP_DEVICE_IS_SUB_NODE_OR_IN_ELECTION     (-0x111b) //-4379, Dynamic mesh device is in election on the cloud
#define OPRT_SVC_HTTP_EXT_MOD_NOT_SUPPORT_AUTO_ACT          (-0x111c) //-4380, Extension module does not support automatic activation
#define OPRT_SVC_HTTP_ERRCODE_MAX_CNT                       29

/****************************************************************************
            the error code marco define for module SVC_LAN
****************************************************************************/
#define OPRT_SVC_LAN_SOCKET_FAULT        (-0x1500) //-5376, socket error, APP actively disconnected or network abnormal
#define OPRT_SVC_LAN_SEND_ERR            (-0x1501) //-5377, socket send error, please check if the network is normal
#define OPRT_SVC_LAN_NO_CLIENT_CONNECTED (-0x1502) //-5378, no LAN device connections to report
#define OPRT_SVC_LAN_ERRCODE_MAX_CNT     3

/****************************************************************************
            the error code macro define for module SVC_LAN_LINKAGE
****************************************************************************/
#define OPRT_SVC_LAN_LINKAGE_SOCK_CREAT_ERR  (-0x1600) //-5632, socket creation failed
#define OPRT_SVC_LAN_LINKAGE_SET_SOCK_ERR    (-0x1601) //-5633, socket set failed
#define OPRT_SVC_LAN_LINKAGE_SOCK_CONN_ERR   (-0x1602) //-5634, socket connection failed, please check if the network is normal
#define OPRT_SVC_LAN_LINKAGE_SEND_ERR        (-0x1603) //-5635, send failed, please check if the network is normal
#define OPRT_SVC_LAN_LINKAGE_RECV_ERR        (-0x1604) //-5636, receive failed, please check if the network is normal
#define OPRT_SVC_LAN_LINKAGE_ERRCODE_MAX_CNT 5

/****************************************************************************
            the error code marco define for module SVC_MQTT
****************************************************************************/
#define OPRT_SVC_MQTT_CMD_NOT_EXEC    (-0x1700) //-5888, Command not executed
#define OPRT_SVC_MQTT_CMD_OUT_OF_TIME (-0x1701) //-5889, Command not executed within the specified time
#define OPRT_SVC_MQTT_GW_MQ_OFFLILNE  (-0x1702) //-5890, MQTT offline, please check if the network is normal
#define OPRT_SVC_MQTT_ERRCODE_MAX_CNT 3

/****************************************************************************
            the error code macro define for module SVC_PEGASUS
****************************************************************************/
#define OPRT_SVC_PEGASUS_DECODE_FAILED      (-0x1800) //-6144, Decoding failed
#define OPRT_SVC_PEGASUS_DONOT_FOUND_MODULE (-0x1801) //-6145, Module not found
#define OPRT_SVC_PEGASUS_ERRCODE_MAX_CNT    2

/****************************************************************************
            the error code macro define for module SVC_UPGRADE
****************************************************************************/
#define OPRT_SVC_UPGRADE_APP_NOT_READY     (-0x1900) //-6400, Application not ready
#define OPRT_SVC_UPGRADE_NO_VALID_FIRMWARE (-0x1901) //-6401, Upgrade information field verification failed
#define OPRT_SVC_UPGRADE_LAN_OTA_FINISH    (-0x1902) //-6402, LAN OTA completed
#define OPRT_SVC_UPGRADE_HMAC_NOT_MATCH    (-0x1903) //-6403, HMAC does not match
#define OPRT_SVC_UPGRADE_ERRCODE_MAX_CNT   4

/****************************************************************************
            the error code macro define for module SVC_API_IOT
****************************************************************************/
#define OPRT_SVC_API_IOT_DISCONNECTED_WITH_ROUTER (-0x1a00) //-6656, Disconnected with router
#define OPRT_SVC_API_IOT_DEV_NOT_BIND             (-0x1a01) //-6657, Device not bound
#define OPRT_SVC_API_IOT_ERRCODE_MAX_CNT          2

/****************************************************************************
            the error code marco define for module SVC_CELLULAR
****************************************************************************/
#define OPRT_SVC_CELLULAR_CALL_FAILED          (-0x1b00) //-6912, Initiate call failed
#define OPRT_SVC_CELLULAR_CALL_VOICE_START_ERR (-0x1b01) //-6913, Start call voice failed
#define OPRT_SVC_CELLULAR_CALL_ANSWER_ERR      (-0x1b02) //-6914, Call answer failed
#define OPRT_SVC_CELLULAR_CALL_GET_INFO_ERR    (-0x1b03) //-6915, Get call information failed
#define OPRT_SVC_CELLULAR_CALL_HUNGUP_ERR      (-0x1b04) //-6916, Call hang up failed
#define OPRT_SVC_CELLULAR_SMS_SEND_ERR         (-0x1b05) //-6917, SMS send failed
#define OPRT_SVC_CELLULAR_SMS_SEND_TIMEOUT     (-0x1b06) //-6918, SMS send timeout
#define OPRT_SVC_CELLULAR_SMS_ENCODE_ERR       (-0x1b07) //-6919, SMS encode error
#define OPRT_SVC_CELLULAR_SMS_SIZE_ERR         (-0x1b08) //-6920, SMS size error
#define OPRT_SVC_CELLULAR_ERRCODE_MAX_CNT      9

/****************************************************************************
            the error code marco define for module SVC_DEVOS
****************************************************************************/
#define OPRT_SVC_DEVOS_NOT_EXISTS         (-0x1c00) //-7168, Not exists
#define OPRT_SVC_DEVOS_SCMA_INVALID       (-0x1c01) //-7169, SCMA invalid
#define OPRT_SVC_DEVOS_DEV_DP_CNT_INVALID (-0x1c02) //-7170, Device DP count invalid, cannot exceed 255
#define OPRT_SVC_DEVOS_NO_AUTHENTICATION  (-0x1c03) //-7171, No authentication
#define OPRT_SVC_DEVOS_ROUTER_NOT_FIND    (-0x1c04) //-7172, Router not found
#define OPRT_SVC_DEVOS_ERRCODE_MAX_CNT    5

/****************************************************************************
            the error code marco define for module BASE_DB
****************************************************************************/
#define OPRT_BASE_DB_FLASH_NOT_ENOUGH_PAGE (-0x1d00) //-7424, Not enough flash pages
#define OPRT_BASE_DB_ERRCODE_MAX_CNT       1

/****************************************************************************
            the error code marco define for module LINK_CORE
****************************************************************************/
#define OPRT_LINK_CORE_NET_SOCKET_ERROR            (-0x1e00) //-7680, Failed to open a socket
#define OPRT_LINK_CORE_NET_CONNECT_ERROR           (-0x1e01) //-7681, The connection to the given server / port failed.
#define OPRT_LINK_CORE_UNKNOWN_HOST_ERROR          (-0x1e02) //-7682, Failed to get an IP address for the given hostname.
#define OPRT_LINK_CORE_TLS_CONNECTION_ERROR        (-0x1e03) //-7683, TLS connection failed
#define OPRT_LINK_CORE_DRBG_ENTROPY_ERROR          (-0x1e04) //-7684, mbedtls random seed generation failed
#define OPRT_LINK_CORE_X509_ROOT_CRT_PARSE_ERROR   (-0x1e05) //-7685, X509 root certificate parse failed
#define OPRT_LINK_CORE_X509_DEVICE_CRT_PARSE_ERROR (-0x1e06) //-7686, X509 device certificate parse failed
#define OPRT_LINK_CORE_PK_PRIVATE_KEY_PARSE_ERROR  (-0x1e07) //-7687, Private key parse failed
#define OPRT_LINK_CORE_HTTP_CLIENT_HEADER_ERROR    (-0x1e08) //-7688, HTTP header initialization failed
#define OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR      (-0x1e09) //-7689, HTTP request send failed
#define OPRT_LINK_CORE_HTTP_RESPONSE_BUFFER_EMPTY  (-0x1e0a) //-7690, HTTP buffer empty
#define OPRT_LINK_CORE_HTTP_GW_NOT_EXIST           (-0x1e0b) //-7691, Gateway does not exist, the device may have been deleted
#define OPRT_LINK_CORE_ERRCODE_MAX_CNT             12

/****************************************************************************
            the error code marco define for module SVC_BT
****************************************************************************/
#define OPRT_SVC_BT_API_TRSMITR_CONTINUE (-0x1f00) //-7936, Transmission not finished
#define OPRT_SVC_BT_API_TRSMITR_ERROR    (-0x1f01) //-7937, Transmission error
#define OPRT_SVC_BT_NETCFG_ERROR_ACK     (-0x1f02) //-7938, BT command error, send ack to app
#define OPRT_SVC_BT_ERRCODE_MAX_CNT      3

/****************************************************************************
            the error code macro define for module SVC_NETMGR
****************************************************************************/
#define OPRT_SVC_NETMGR_NEED_FACTORY_RESET      (-0x2000) //-8192, Network initialization configuration verification failed, need to restore factory settings
#define OPRT_SVC_NETMGR_ERRCODE_MAX_CNT         1

/****************************************************************************
            the error code macro define for module GW_BT_DEV
****************************************************************************/
#define OPRT_GW_BT_DEV_BLE_CONNECT_SUCCESS              (-0x3100) //-12544, BLE device connected successfully
#define OPRT_GW_BT_DEV_BLE_CONNECT_CNT_EXCEED           (-0x3101) //-12545, BLE device connection count exceeded the maximum limit
#define OPRT_GW_BT_DEV_BLE_CONNECT_HAL_FAILED           (-0x3102) //-12546, BLE device connection HAL layer interface returned failure
#define OPRT_GW_BT_DEV_BLE_CONNECT_HANDS_FAILED         (-0x3103) //-12547, BLE device connection handshake failed
#define OPRT_GW_BT_DEV_BLE_CONNECT_HANDS_TIMEOUT        (-0x3104) //-12548, BLE device connection handshake timeout, please check if the device is powered on normally, if it is too far away, or if it is connected by a mobile phone's Bluetooth
#define OPRT_GW_BT_DEV_BLE_CONNECT_GET_INFO_FAILED      (-0x3105) //-12549, BLE device connection get_info failed
#define OPRT_GW_BT_DEV_BLE_CONNECT_GET_LOGIN_KEY_FAILED (-0x3106) //-12550, BLE device connection get_login_key failed
#define OPRT_GW_BT_DEV_BLE_CONNECT_PAIR_DATA_LEN_ERR    (-0x3107) //-12551, BLE device connection pair_data_len_err failed
#define OPRT_GW_BT_DEV_BLE_CONNECT_SESSION_OPEN_TIMEOUT (-0x3108) //-12552, BLE device connection session_open timeout
#define OPRT_GW_BT_DEV_BLE_CONNECT_SEND_FAILED          (-0x3109) //-12553, BLE device connection data send failed
#define OPRT_GW_BT_DEV_BLE_LOCAL_RESET                  (-0x310a) //-12554, BLE device local reset, please confirm if the device has been manually reset
#define OPRT_GW_BT_DEV_BLE_SCAN_TIMEOUT                 (-0x310b) //-12555, BLE device broadcast packet scan timeout, please check if the device is powered on normally, if it is too far away, or if it is connected by a mobile phone's Bluetooth
#define OPRT_GW_BT_DEV_BLE_DISCONNECT_SUCCESS           (-0x310c) //-12556, BLE device disconnected successfully
#define OPRT_GW_BT_DEV_BEACON_DEV_NOT_FOUND             (-0x310d) //-12557, Device not found
#define OPRT_GW_BT_DEV_BEACON_DEV_KEY1_NOT_FOUND        (-0x310e) //-12558, Device KEY1 not found, the key for communication data  encryption
#define OPRT_GW_BT_DEV_BEACON_DEV_NOT_BIND              (-0x310f) //-12559, Device not bound, please rebind the device
#define OPRT_GW_BT_DEV_BEACON_DECODE_PAIR_MSG_FAILED    (-0x3110) //-12560, Device networking, gateway requests cloud to decrypt networking information failed
#define OPRT_GW_BT_DEV_BEACON_KEY_RESP_TIMEOUT          (-0x3111) //-12561, Device networking, device beaconkey response timeout
#define OPRT_GW_BT_DEV_BEACON_JOIN_NOTIFY_RESP_TIMEOUT  (-0x3112) //-12562, Device networking, device join notify response timeout
#define OPRT_GW_BT_DEV_ERRCODE_MAX_CNT                  19

/****************************************************************************
            the error code marco define for module GW_ZG_DEV
****************************************************************************/
#define OPRT_GW_ZG_DEV_ZB_HOST_TABLE_NULL                       (-0x3200) //-12800, The device does not exist in the host table and will be removed. The gateway has not stored information about this device.
#define OPRT_GW_ZG_DEV_ZB_SEQ_INVALID                           (-0x3201) //-12801, ZCL seq is invalid, the seq of the current packet is less than the seq of the previous packet.
#define OPRT_GW_ZG_DEV_ZB_HOST_TO_GATEWAY_ERR                   (-0x3202) //-12802, Error sending data from the host to the gateway component, local UDP data transmission error, it is recommended to restart the gateway.
#define OPRT_GW_ZG_DEV_ZB_DATA_TOO_BIG                          (-0x3203) //-12803, The length of Zigbee data is too large and has been discarded, it is recommended to check the data format.
#define OPRT_GW_ZG_DEV_ZB_GW_TABLE_NULL                         (-0x3204) //-12804, The gateway device table is not initialized, it is recommended to retry networking.
#define OPRT_GW_ZG_DEV_ZB_GW_RELIABLE_ERR                       (-0x3205) //-12805, Reliable reporting failed, it is recommended to check the data format.
#define OPRT_GW_ZG_DEV_ZB_GW_TRD_DEV_ERR                        (-0x3206) //-12806, Third-level device data reporting failed, it is recommended to check the data format.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_DEV_ID_ERR                    (-0x3207) //-12807, The device id for data distribution does not exist, it is recommended to re-network.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_UPDATE_SCE_LIST_ERR           (-0x3208) //-12808, The scene list for data distribution does not exist, it may not have verified that dp, it is recommended to recreate the scene.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_MALLOC_ERR                    (-0x3209) //-12809, GATEWAY layer memory allocation failed, insufficient memory, it is recommended to restart the gateway.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_DATA_TYPE_ERR                 (-0x320a) //-12810, Data distribution DP data type error, check the data format.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_TRD_DEV_DW_ERR                (-0x320b) //-12811, Third-level device data distribution failed, check the data format.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_COM_DATALEN_ERR               (-0x320c) //-12812, General docking data length error, check the data format.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_SEND_DATA_FAILED              (-0x320d) //-12813, GATEWAY layer failed to send data to HOST layer, check the data format.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_NCRYPT_DATA_FAILED            (-0x320e) //-12814, Data encryption failed.
#define OPRT_GW_ZG_DEV_ZB_GATWAY_STDRELIABLE_DW_FAILED          (-0x320f) //-12815, Standard transparent command sending failed, check the data format.
#define OPRT_GW_ZG_DEV_ZB_GATWAY_CMD_TYPE_NOT_SUPPORT           (-0x3210) //-12816, The command type for distribution is not supported, check the data format.
#define OPRT_GW_ZG_DEV_ZB_HOST_RCV_SLABSCMD_MSG_LEN_ERR         (-0x3211) //-12817, Data packet length error for distribution, check the data format.
#define OPRT_GW_ZG_DEV_ZB_HSOT_DEV_REJOIN                       (-0x3212) //-12818, When distributing data, the device is in the rejoin process, it is recommended to wait and retry.
#define OPRT_GW_ZG_DEV_ZB_HOST_CMD_TYPE_NOT_SUPPORT             (-0x3213) //-12819, Host command type is not supported, check the data format.
#define OPRT_GW_ZG_DEV_ZB_HOST_SEND_DATA_FAILED                 (-0x3214) //-12820, Host layer failed to send data, it is recommended to retry.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_DEV_BIND_ERR                  (-0x3215) //-12821, Binding error, it is recommended to re-network.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_SET_HB_STAT_FAILED            (-0x3216) //-12822, GATEWAY layer failed to set sub-device heartbeat status, the device may not be adapted.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_JOIN_STAT_FALSE               (-0x3217) //-12823, The gateway has not enabled networking, ensure the gateway is in networking mode.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_STAND_JOIN_ERR                (-0x3218) //-12824, Standard networking failed, the device may not be adapted.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_DEV_NOT_SUPPORT               (-0x3219) //-12825, Sub-device is not supported, the device may not be adapted.
#define OPRT_GW_ZG_DEV_ZB_HOST_RCV_JOIN_MSG_ERR                 (-0x321a) //-12826, Zigbee host layer received MSG length abnormal, check the data format.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_TRD_DEVID_ERR                 (-0x321b) //-12827, Third-level device ID error, it is recommended to re-network.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_THR_DEV_TPE_ERR               (-0x321c) //-12828, Third-level device type error, it is recommended to re-network.
#define OPRT_GW_ZG_DEV_ZB_GATEWAY_RS_PROFILE_NOT_SUPPORT        (-0x321d) //-12829, Receiving ZIGBEE PROFILE not supported, the device may not be adapted.
#define OPRT_GW_ZG_DEV_ZB_SUBDEV_LEAVE_SELF                     (-0x321e) //-12830, Sub-device actively leaves the network, the device may have been reset.
#define OPRT_GW_ZG_DEV_ERRCODE_MAX_CNT                          31

/****************************************************************************
            the error code marco define for module GW_BASE_LINKAGE
****************************************************************************/
#define OPRT_GW_BASE_LINKAGE_INIT_FAIL                          (-0x3300) //-13056, Linkage module initialization failed.
#define OPRT_GW_BASE_LINKAGE_COND_CHECK_FAIL                    (-0x3301) //-13057, Linkage condition not met.
#define OPRT_GW_BASE_LINKAGE_RULE_ID_NOT_FIND                   (-0x3302) //-13058, Linkage rule ID does not exist, please check if the rule exists.
#define OPRT_GW_BASE_LINKAGE_RULE_ID_DISABLE                    (-0x3303) //-13059, Linkage rule is not enabled, please check if the rule is enabled.
#define OPRT_GW_BASE_LINKAGE_RULE_IS_UPDATED                    (-0x3304) //-13060, Linkage rule is being updated, please try again later.
#define OPRT_GW_BASE_LINKAGE_FLOW_CNTL                          (-0x3305) //-13061, Linkage flow control, may trigger too frequently, please try again later, or there may be a dead loop, please check if there is a dead loop in the linkage.
#define OPRT_GW_BASE_LINKAGE_THREAD_CREATE_FAIL                 (-0x3306) //-13062, Linkage thread creation failed.
#define OPRT_GW_BASE_LINKAGE_COND_CHECK_NO_ID_MATCH             (-0x3307) //-13063, Linkage condition not met (no matching device ID).
#define OPRT_GW_BASE_LINKAGE_COND_CHECK_NO_DP_MATCH             (-0x3308) //-13064, Linkage condition not met (no matching condition DP).
#define OPRT_GW_BASE_LINKAGE_ERRCODE_MAX_CNT                    9

/****************************************************************************
            the error code marco define for module GW_ZG_SUB
****************************************************************************/
#define OPRT_GW_ZG_SUB_ERR0            (-0x3400) //-13312, Error 0
#define OPRT_GW_ZG_SUB_ERR1            (-0x3401) //-13313, Error 1
#define OPRT_GW_ZG_SUB_ERR2            (-0x3402) //-13314, Error 2: FIB restart
#define OPRT_GW_ZG_SUB_ERR3            (-0x3403) //-13315, Error 3: Upgrade restart
#define OPRT_GW_ZG_SUB_ERR4            (-0x3404) //-13316, Error 4: External IO exception restart
#define OPRT_GW_ZG_SUB_ERR5            (-0x3405) //-13317, Error 5: Low voltage restart, may be power failure restart, or power supply problem
#define OPRT_GW_ZG_SUB_ERR6            (-0x3406) //-13318, Error 6: Watchdog restart
#define OPRT_GW_ZG_SUB_ERR7            (-0x3407) //-13319, Error 7: Software initiated restart
#define OPRT_GW_ZG_SUB_ERR8            (-0x3408) //-13320, Error 8: Software crash restart
#define OPRT_GW_ZG_SUB_ERR9            (-0x3409) //-13321, Error 9: Flash exception restart
#define OPRT_GW_ZG_SUB_ERR10           (-0x340a) //-13322, Error 10: Irrecoverable hardware exception restart
#define OPRT_GW_ZG_SUB_ERR11           (-0x340b) //-13323, Error 11: Internal CPU access error in hardware
#define OPRT_GW_ZG_SUB_ERR12           (-0x340c) //-13324, Error 12: Power-off restart, may be power failure restart, or power supply problem
#define OPRT_GW_ZG_SUB_ERR13           (-0x340d) //-13325, Error 13
#define OPRT_GW_ZG_SUB_ERR14           (-0x340e) //-13326, Error 14
#define OPRT_GW_ZG_SUB_ERR15           (-0x340f) //-13327, Error 15
#define OPRT_GW_ZG_SUB_ERR16           (-0x3410) //-13328, Error 16: Restart
#define OPRT_GW_ZG_SUB_ERR17           (-0x3411) //-13329, Error 17: Disconnection
#define OPRT_GW_ZG_SUB_ERR18           (-0x3412) //-13330, Error 18: Active disconnection, please check if the customer triggered the disconnection locally
#define OPRT_GW_ZG_SUB_ERR19           (-0x3413) //-13331, Error 19: Passive disconnection, please check if it was removed by APP or remote control
#define OPRT_GW_ZG_SUB_ERR20           (-0x3414) //-13332, Error 20: IO exception
#define OPRT_GW_ZG_SUB_ERR21           (-0x3415) //-13333, Error 21: Memory leak
#define OPRT_GW_ZG_SUB_ERR22           (-0x3416) //-13334, Error 22: Short address change
#define OPRT_GW_ZG_SUB_ERR23           (-0x3417) //-13335, Error 23: Received factory reset
#define OPRT_GW_ZG_SUB_ERR24           (-0x3418) //-13336, Error 24: Received F0 command
#define OPRT_GW_ZG_SUB_ERR25           (-0x3419) //-13337, Error 25: Wake-up time too long
#define OPRT_GW_ZG_SUB_ERR26           (-0x341a) //-13338, Error 26: Received abnormal factory reset
#define OPRT_GW_ZG_SUB_ERRCODE_MAX_CNT 27

/****************************************************************************
            the error code macro define for module OS_ADAPTER_MUTEX
****************************************************************************/
#define OPRT_OS_ADAPTER_MUTEX_CREAT_FAILED    (-0x6500) //-25856, Creation failed
#define OPRT_OS_ADAPTER_MUTEX_LOCK_FAILED     (-0x6501) //-25857, Lock failed
#define OPRT_OS_ADAPTER_MUTEX_UNLOCK_FAILED   (-0x6502) //-25858, Unlock failed
#define OPRT_OS_ADAPTER_MUTEX_RELEASE_FAILED  (-0x6503) //-25859, Release failed
#define OPRT_OS_ADAPTER_MUTEX_ERRCODE_MAX_CNT 4

/****************************************************************************
            the error code macro define for module OS_ADAPTER_SEM
****************************************************************************/
#define OPRT_OS_ADAPTER_SEM_CREAT_FAILED    (-0x6600) //-26112, Creation failed
#define OPRT_OS_ADAPTER_SEM_WAIT_FAILED     (-0x6601) //-26113, Wait failed
#define OPRT_OS_ADAPTER_SEM_POST_FAILED     (-0x6602) //-26114, Post failed
#define OPRT_OS_ADAPTER_SEM_RELEASE_FAILED  (-0x6603) //-26115, Release failed
#define OPRT_OS_ADAPTER_SEM_WAIT_TIMEOUT    (-0x6604) //-26116, Wait timeout
#define OPRT_OS_ADAPTER_SEM_ERRCODE_MAX_CNT 5

/****************************************************************************
            the error code macro define for module OS_ADAPTER_QUEUE
****************************************************************************/
#define OPRT_OS_ADAPTER_QUEUE_CREAT_FAILED    (-0x6700) //-26368, Creation failed
#define OPRT_OS_ADAPTER_QUEUE_SEND_FAIL       (-0x6701) //-26369, Send failed
#define OPRT_OS_ADAPTER_QUEUE_RECV_FAIL       (-0x6702) //-26370, Receive failed
#define OPRT_OS_ADAPTER_QUEUE_ERRCODE_MAX_CNT 3

/****************************************************************************
            the error code macro define for module OS_ADAPTER_THRD
****************************************************************************/
#define OPRT_OS_ADAPTER_THRD_CREAT_FAILED      (-0x6800) //-26624, Creation failed
#define OPRT_OS_ADAPTER_THRD_RELEASE_FAILED    (-0x6801) //-26625, Release failed
#define OPRT_OS_ADAPTER_THRD_JUDGE_SELF_FAILED (-0x6802) //-26626, Judge self failed
#define OPRT_OS_ADAPTER_THRD_ERRCODE_MAX_CNT   3

/****************************************************************************
            the error code marco define for module OS_ADAPTER
****************************************************************************/
#define OPRT_OS_ADAPTER_COM_ERROR             (-0x6900) //-26880, General exception
#define OPRT_OS_ADAPTER_INVALID_PARM          (-0x6901) //-26881, Invalid parameter
#define OPRT_OS_ADAPTER_MALLOC_FAILED         (-0x6902) //-26882, Memory allocation failed
#define OPRT_OS_ADAPTER_NOT_SUPPORTED         (-0x6903) //-26883, Operation not supported
#define OPRT_OS_ADAPTER_NETWORK_ERROR         (-0x6904) //-26884, Network error
#define OPRT_OS_ADAPTER_AP_NOT_FOUND          (-0x6905) //-26885, AP not found
#define OPRT_OS_ADAPTER_AP_SCAN_FAILED        (-0x6906) //-26886, AP scan failed
#define OPRT_OS_ADAPTER_AP_RELEASE_FAILED     (-0x6907) //-26887, AP release failed
#define OPRT_OS_ADAPTER_CHAN_SET_FAILED       (-0x6908) //-26888, Channel set failed
#define OPRT_OS_ADAPTER_CHAN_GET_FAILED       (-0x6909) //-26889, Channel get failed
#define OPRT_OS_ADAPTER_IP_GET_FAILED         (-0x690a) //-26890, IP get failed
#define OPRT_OS_ADAPTER_MAC_SET_FAILED        (-0x690b) //-26891, MAC set failed
#define OPRT_OS_ADAPTER_MAC_GET_FAILED        (-0x690c) //-26892, MAC get failed
#define OPRT_OS_ADAPTER_WORKMODE_SET_FAILED   (-0x690d) //-26893, Work mode set failed
#define OPRT_OS_ADAPTER_WORKMODE_GET_FAILED   (-0x690e) //-26894, Work mode get failed
#define OPRT_OS_ADAPTER_SNIFFER_SET_FAILED    (-0x690f) //-26895, SNIFFER set failed
#define OPRT_OS_ADAPTER_AP_START_FAILED       (-0x6910) //-26896, AP start failed
#define OPRT_OS_ADAPTER_AP_STOP_FAILED        (-0x6911) //-26897, AP stop failed
#define OPRT_OS_ADAPTER_APINFO_GET_FAILED     (-0x6912) //-26898, AP info get failed
#define OPRT_OS_ADAPTER_FAST_CONN_FAILED      (-0x6913) //-26899, Fast connect failed
#define OPRT_OS_ADAPTER_CONN_FAILED           (-0x6914) //-26900, Connect failed
#define OPRT_OS_ADAPTER_DISCONN_FAILED        (-0x6915) //-26901, Disconnect failed
#define OPRT_OS_ADAPTER_RSSI_GET_FAILED       (-0x6916) //-26902, RSSI get failed
#define OPRT_OS_ADAPTER_BSSID_GET_FAILED      (-0x6917) //-26903, BSSID get failed
#define OPRT_OS_ADAPTER_STAT_GET_FAILED       (-0x6918) //-26904, Status get failed
#define OPRT_OS_ADAPTER_CCODE_SET_FAILE       (-0x6919) //-26905, CCODE set failed
#define OPRT_OS_ADAPTER_MGNT_SEND_FAILED      (-0x691a) //-26906, Management packet send failed
#define OPRT_OS_ADAPTER_MGNT_REG_FAILED       (-0x691b) //-26907, Management packet callback registration failed
#define OPRT_OS_ADAPTER_WF_LPMODE_SET_FAILED  (-0x691c) //-26908, Wi-Fi low power mode set failed
#define OPRT_OS_ADAPTER_CPU_LPMODE_SET_FAILED (-0x691d) //-26909, CPU low power mode set failed
#define OPRT_OS_ADAPTER_ERRCODE_MAX_CNT       30

/****************************************************************************
            the error code macro define for module OS_ADAPTER_FLASH
****************************************************************************/
#define OPRT_OS_ADAPTER_FLASH_READ_FAILED     (-0x6a00) //-27136, Flash read failed
#define OPRT_OS_ADAPTER_FLASH_WRITE_FAILED    (-0x6a01) //-27137, Flash write failed
#define OPRT_OS_ADAPTER_FLASH_ERASE_FAILED    (-0x6a02) //-27138, Flash erase failed
#define OPRT_OS_ADAPTER_FLASH_ERRCODE_MAX_CNT 3

/****************************************************************************
            the error code macro define for module OS_ADAPTER_OTA
****************************************************************************/
#define OPRT_OS_ADAPTER_OTA_START_INFORM_FAILED (-0x6b00) //-27392, OTA start notification failed
#define OPRT_OS_ADAPTER_OTA_PKT_SIZE_FAILED     (-0x6b01) //-27393, OTA packet size invalid
#define OPRT_OS_ADAPTER_OTA_PROCESS_FAILED      (-0x6b02) //-27394, OTA process failed
#define OPRT_OS_ADAPTER_OTA_VERIFY_FAILED       (-0x6b03) //-27395, OTA verify failed
#define OPRT_OS_ADAPTER_OTA_END_INFORM_FAILED   (-0x6b04) //-27396, OTA end notification failed
#define OPRT_OS_ADAPTER_OTA_ERRCODE_MAX_CNT     5

/****************************************************************************
            the error code macro define for module OS_ADAPTER_WD
****************************************************************************/
#define OPRT_OS_ADAPTER_WD_WD_INIT_FAILED  (-0x6c00) //-27648, Watchdog initialization failed
#define OPRT_OS_ADAPTER_WD_ERRCODE_MAX_CNT 1

/****************************************************************************
            the error code marco define for module OS_ADAPTER_GPIO
****************************************************************************/
#define OPRT_OS_ADAPTER_GPIO_INOUT_SET_FAILED (-0x6d00) //-27904, GPIO INOUT setting failed
#define OPRT_OS_ADAPTER_GPIO_MODE_SET_FAILED  (-0x6d01) //-27905, GPIO mode setting failed
#define OPRT_OS_ADAPTER_GPIO_WRITE_FAILED     (-0x6d02) //-27906, GPIO write failed
#define OPRT_OS_ADAPTER_GPIO_IRQ_INIT_FAILED  (-0x6d03) //-27907, GPIO interrupt initialization failed
#define OPRT_OS_ADAPTER_GPIO_ERRCODE_MAX_CNT  4

/****************************************************************************
            the error code macro define for module OS_ADAPTER_UART
****************************************************************************/
#define OPRT_OS_ADAPTER_UART_INIT_FAILED     (-0x6e00) //-28160, UART initialization failed
#define OPRT_OS_ADAPTER_UART_DEINIT_FAILED   (-0x6e01) //-28161, UART release failed
#define OPRT_OS_ADAPTER_UART_SEND_FAILED     (-0x6e02) //-28162, UART send failed
#define OPRT_OS_ADAPTER_UART_READ_FAILED     (-0x6e03) //-28163, UART receive failed
#define OPRT_OS_ADAPTER_UART_ERRCODE_MAX_CNT 4

/****************************************************************************
            the error code macro define for module OS_ADAPTER_I2C
****************************************************************************/
#define OPRT_OS_ADAPTER_I2C_OPEN_FAILED     (-0x6f00) //-28416, I2C open failed
#define OPRT_OS_ADAPTER_I2C_CLOSE_FAILED    (-0x6f01) //-28417, I2C close failed
#define OPRT_OS_ADAPTER_I2C_READ_FAILED     (-0x6f02) //-28418, I2C read failed
#define OPRT_OS_ADAPTER_I2C_WRITE_FAILED    (-0x6f03) //-28419, I2C write failed
#define OPRT_OS_ADAPTER_I2C_INVALID_PARM    (-0x6f04) //-28420, Invalid parameter
#define OPRT_OS_ADAPTER_I2C_INIT_FAILED     (-0x6f05) //-28421, Initialization failed
#define OPRT_OS_ADAPTER_I2C_DEINIT_FAILED   (-0x6f06) //-28422, Release failed
#define OPRT_OS_ADAPTER_I2C_MODE_ERR        (-0x6f07) //-28423, Mode error
#define OPRT_OS_ADAPTER_I2C_BUSY            (-0x6f08) //-28424, Busy
#define OPRT_OS_ADAPTER_I2C_TIMEOUT         (-0x6f09) //-28425, Timeout
#define OPRT_OS_ADAPTER_I2C_MASTER_LOST_ARB (-0x6f0a) //-28426, Master arbitration lost
#define OPRT_OS_ADAPTER_I2C_BUS_ERR         (-0x6f0b) //-28427, Bus error
#define OPRT_OS_ADAPTER_I2C_TRANS_ERR       (-0x6f0c) //-28428, Transmission error
#define OPRT_OS_ADAPTER_I2C_ADDR_NO_ACK     (-0x6f0d) //-28429, No ACK
#define OPRT_OS_ADAPTER_I2C_IRQ_INIT_FAILED (-0x6f0e) //-28430, Interrupt initialization failed
#define OPRT_OS_ADAPTER_I2C_RESERVE1_ERR    (-0x6f0f) //-28431, RESERVE ERR1
#define OPRT_OS_ADAPTER_I2C_RESERVE2_ERR    (-0x6f10) //-28432, RESERVE ERR2
#define OPRT_OS_ADAPTER_I2C_RESERVE3_ERR    (-0x6f11) //-28433, RESERVE ERR3
#define OPRT_OS_ADAPTER_I2C_ERRCODE_MAX_CNT 18

/****************************************************************************
            the error code macro define for module OS_ADAPTER_BLE
****************************************************************************/
#define OPRT_OS_ADAPTER_BLE_HANDLE_ERROR             (-0x7000) //-28672, Ble handle error
#define OPRT_OS_ADAPTER_BLE_BUSY                     (-0x7001) //-28673, Ble busy
#define OPRT_OS_ADAPTER_BLE_TIMEOUT                  (-0x7002) //-28674, Ble timeout
#define OPRT_OS_ADAPTER_BLE_RESERVED1                (-0x7003) //-28675, Ble Reserved Error Code 1
#define OPRT_OS_ADAPTER_BLE_RESERVED2                (-0x7004) //-28676, Ble Reserved Error Code 2
#define OPRT_OS_ADAPTER_BLE_RESERVED3                (-0x7005) //-28677, Ble Reserved Error Code 3
#define OPRT_OS_ADAPTER_BLE_INIT_FAILED              (-0x7006) //-28678, Ble initialization failed
#define OPRT_OS_ADAPTER_BLE_DEINIT_FAILED            (-0x7007) //-28679, Ble release failed
#define OPRT_OS_ADAPTER_BLE_GATT_CONN_FAILED         (-0x7008) //-28680, Ble GATT connection failed
#define OPRT_OS_ADAPTER_BLE_GATT_DISCONN_FAILED      (-0x7009) //-28681, Ble GATT disconnection failed
#define OPRT_OS_ADAPTER_BLE_ADV_START_FAILED         (-0x700a) //-28682, Ble advertisement start failed
#define OPRT_OS_ADAPTER_BLE_ADV_STOP_FAILED          (-0x700b) //-28683, Ble advertisement stop failed
#define OPRT_OS_ADAPTER_BLE_SCAN_START_FAILED        (-0x700c) //-28684, Ble scan start failed
#define OPRT_OS_ADAPTER_BLE_SCAN_STAOP_FAILED        (-0x700d) //-28685, Ble scan stop failed
#define OPRT_OS_ADAPTER_BLE_SVC_DISC_FAILED          (-0x700e) //-28686, Ble service discovery failed
#define OPRT_OS_ADAPTER_BLE_CHAR_DISC_FAILED         (-0x700f) //-28687, Ble characteristic discovery failed
#define OPRT_OS_ADAPTER_BLE_DESC_DISC_FAILED         (-0x7010) //-28688, Ble descriptor discovery failed
#define OPRT_OS_ADAPTER_BLE_NOTIFY_FAILED            (-0x7011) //-28689, Ble Peripheral Notify failed
#define OPRT_OS_ADAPTER_BLE_INDICATE_FAILED          (-0x7012) //-28690, Ble Peripheral Indicate failed
#define OPRT_OS_ADAPTER_BLE_WRITE_FAILED             (-0x7013) //-28691, Ble Central write failed
#define OPRT_OS_ADAPTER_BLE_READ_FAILED              (-0x7014) //-28692, Ble Central read failed
#define OPRT_OS_ADAPTER_BLE_MTU_REQ_FAILED           (-0x7015) //-28693, Ble Central MTU request failed
#define OPRT_OS_ADAPTER_BLE_MTU_REPLY_FAILED         (-0x7016) //-28694, Ble Peripheral MTU response failed
#define OPRT_OS_ADAPTER_BLE_CONN_PARAM_UPDATE_FAILED (-0x7017) //-28695, Ble connection parameter update failed
#define OPRT_OS_ADAPTER_BLE_CONN_RSSI_GET_FAILED     (-0x7018) //-28696, Ble connection RSSI get failed
#define OPRT_OS_ADAPTER_BLE_MESH_INVALID_OPCODE      (-0x7019) //-28697, Ble Mesh invalid opcode
#define OPRT_OS_ADAPTER_BLE_MESH_INVALID_ELEMENT     (-0x701a) //-28698, Ble Mesh invalid element
#define OPRT_OS_ADAPTER_BLE_MESH_INVALID_MODEL       (-0x701b) //-28699, Ble Mesh invalid model
#define OPRT_OS_ADAPTER_BLE_MESH_INVALID_ADDR        (-0x701c) //-28700, Ble Mesh invalid source, virtual or destination address
#define OPRT_OS_ADAPTER_BLE_MESH_INVALID_INDEX       (-0x701d) //-28701, Ble Mesh invalid index, e.g., appkey index, netkey index, etc.
#define OPRT_OS_ADAPTER_BLE_MESH_NO_MEMORY           (-0x701e) //-28702, Ble Mesh no memory, e.g., sending too fast causing excessive buffering
#define OPRT_OS_ADAPTER_BLE_MESH_APPKEY_NOT_BOUND_MODEL (-0x701f) //-28703, Ble Mesh Appkey not bound error
#define OPRT_OS_ADAPTER_BLE_MESH_RESERVED1              (-0x7020) //-28704, Ble Mesh Reserved Error Code 1
#define OPRT_OS_ADAPTER_BLE_MESH_RESERVED2              (-0x7021) //-28705, Ble Mesh Reserved Error Code 2
#define OPRT_OS_ADAPTER_BLE_MESH_RESERVED3              (-0x7022) //-28706, Ble Mesh Reserved Error Code 3
#define OPRT_OS_ADAPTER_BLE_MESH_RESERVED4              (-0x7023) //-28707, Ble Mesh Reserved Error Code 4
#define OPRT_OS_ADAPTER_BLE_MESH_RESERVED5              (-0x7024) //-28708, Ble Mesh Reserved Error Code 5
#define OPRT_OS_ADAPTER_BLE_MESH_PROVISION_FAIL         (-0x7025) //-28709, Ble Mesh provisioning failed
#define OPRT_OS_ADAPTER_BLE_MESH_COMPO_GET_FAIL         (-0x7026) //-28710, Ble Mesh Composition data get failed
#define OPRT_OS_ADAPTER_BLE_MESH_MODEL_BIND_FAIL        (-0x7027) //-28711, Ble Mesh model bind failed
#define OPRT_OS_ADAPTER_BLE_MESH_APPKEY_ADD_FAIL        (-0x7028) //-28712, Ble Mesh Appkey add failed
#define OPRT_OS_ADAPTER_BLE_MESH_NETKEY_ADD_FAIL        (-0x7029) //-28713, Ble Mesh Netkey add failed
#define OPRT_OS_ADAPTER_BLE_MESH_APPKEY_BIND_FAIL       (-0x702a) //-28714, Ble Mesh Appkey bind failed
#define OPRT_OS_ADAPTER_BLE_ERRCODE_MAX_CNT             43

/****************************************************************************
            the error code marco define for module MID_TRANSPORT
****************************************************************************/
#define OPRT_MID_TRANSPORT_INVALID_PARM  (-0x7100) //-28928, Invalid parameter
#define OPRT_MID_TRANSPORT_MALLOC_FAILED (-0x7101) //-28929, Memory allocation failed
#define OPRT_MID_TRANSPORT_DNS_PARSED_FAILED             (-0x7102) //-28930, DNS parsing failed, please check if the network is normal
#define OPRT_MID_TRANSPORT_SOCK_CREAT_FAILED             (-0x7103) //-28931, Socket creation failed, please check if the socket configuration quantity is too small
#define OPRT_MID_TRANSPORT_SOCK_SET_REUSE_FAILED         (-0x7104) //-28932, Socket set reuse failed
#define OPRT_MID_TRANSPORT_SOCK_SET_DISABLE_NAGLE_FAILED (-0x7105) //-28933, Socket set disable nagle failed
#define OPRT_MID_TRANSPORT_SOCK_SET_KEEP_ALIVE_FAILED    (-0x7106) //-28934, Socket set keep alive failed
#define OPRT_MID_TRANSPORT_SOCK_SET_BLOCK_FAILED         (-0x7107) //-28935, Socket set block failed
#define OPRT_MID_TRANSPORT_SOCK_NET_BIND_FAILED          (-0x7108) //-28936, Socket net bind failed
#define OPRT_MID_TRANSPORT_SOCK_SET_TIMEOUT_FAILED       (-0x7109) //-28937, Socket set timeout failed
#define OPRT_MID_TRANSPORT_TCP_CONNECD_FAILED            (-0x710a) //-28938, TCP connection failed, please check if the network is normal
#define OPRT_MID_TRANSPORT_TCP_TLS_CONNECD_FAILED        (-0x710b) //-28939, TCP TLS connection failed, please check if the network is normal
#define OPRT_MID_TRANSPORT_PACK_SEND_FAILED              (-0x710c) //-28940, Packet sending failed, please check if the network is normal
#define OPRT_MID_TRANSPORT_ERRCODE_MAX_CNT 13

/****************************************************************************
            the error code macro define for module OS_ADAPTER_SPI
****************************************************************************/
#define OPRT_OS_ADAPTER_SPI_INVALID_PARM    (-0x7200) //-29184, Invalid parameter
#define OPRT_OS_ADAPTER_SPI_INIT_FAILED     (-0x7201) //-29185, Initialization failed
#define OPRT_OS_ADAPTER_SPI_DEINIT_FAILED   (-0x7202) //-29186, Deinitialization failed
#define OPRT_OS_ADAPTER_SPI_MODE_ERR        (-0x7203) //-29187, Mode error
#define OPRT_OS_ADAPTER_SPI_BUSY            (-0x7204) //-29188, Busy
#define OPRT_OS_ADAPTER_SPI_DATA_LOST       (-0x7205) //-29189, Data lost
#define OPRT_OS_ADAPTER_SPI_TIMEOUT         (-0x7206) //-29190, Timeout
#define OPRT_OS_ADAPTER_SPI_BUS_ERR         (-0x7207) //-29191, Bus error
#define OPRT_OS_ADAPTER_SPI_TRANS_ERR       (-0x7208) //-29192, Transmission error
#define OPRT_OS_ADAPTER_SPI_IRQ_ERR         (-0x7209) //-29193, Interrupt exception
#define OPRT_OS_ADAPTER_SPI_RESERVE1_ERR    (-0x720a) //-29194, RESERVE ERR1
#define OPRT_OS_ADAPTER_SPI_RESERVE2_ERR    (-0x720b) //-29195, RESERVE ERR2
#define OPRT_OS_ADAPTER_SPI_RESERVE3_ERR    (-0x720c) //-29196, RESERVE ERR3
#define OPRT_OS_ADAPTER_SPI_ERRCODE_MAX_CNT 13

/****************************************************************************
            the error code macro define for module OS_ADAPTER_PWM
****************************************************************************/
#define OPRT_OS_ADAPTER_PWM_INVALID_PARM    (-0x7300) //-29440, Invalid parameter
#define OPRT_OS_ADAPTER_PWM_INIT_FAILED     (-0x7301) //-29441, Initialization failed
#define OPRT_OS_ADAPTER_PWM_DEINIT_FAILED   (-0x7302) //-29442, Deinitialization failed
#define OPRT_OS_ADAPTER_PWM_MODE_ERR        (-0x7303) //-29443, Mode error
#define OPRT_OS_ADAPTER_PWM_BUSY            (-0x7304) //-29444, Busy
#define OPRT_OS_ADAPTER_PWM_IRQ_INIT_FAILED (-0x7305) //-29445, Interrupt initialization failed
#define OPRT_OS_ADAPTER_PWM_RESERVE1_ERR    (-0x7306) //-29446, RESERVE ERR1
#define OPRT_OS_ADAPTER_PWM_RESERVE2_ERR    (-0x7307) //-29447, RESERVE ERR2
#define OPRT_OS_ADAPTER_PWM_RESERVE3_ERR    (-0x7308) //-29448, RESERVE ERR3
#define OPRT_OS_ADAPTER_PWM_ERRCODE_MAX_CNT 9

/****************************************************************************
            the error code macro define for module OS_ADAPTER_ADC
****************************************************************************/
#define OPRT_OS_ADAPTER_ADC_INVALID_PARM    (-0x7400) //-29696, Invalid parameter
#define OPRT_OS_ADAPTER_ADC_INIT_FAILED     (-0x7401) //-29697, Initialization failed
#define OPRT_OS_ADAPTER_ADC_DEINIT_FAILED   (-0x7402) //-29698, Deinitialization failed
#define OPRT_OS_ADAPTER_ADC_READ_FAILED     (-0x7403) //-29699, ADC read failed
#define OPRT_OS_ADAPTER_ADC_MODE_ERR        (-0x7404) //-29700, Mode error
#define OPRT_OS_ADAPTER_ADC_BUSY            (-0x7405) //-29701, Busy
#define OPRT_OS_ADAPTER_ADC_TIMEOUT         (-0x7406) //-29702, Timeout
#define OPRT_OS_ADAPTER_ADC_IRQ_INIT_FAILED (-0x7407) //-29703, Interrupt initialization failed
#define OPRT_OS_ADAPTER_ADC_RESERVE1_ERR    (-0x7408) //-29704, RESERVE ERR1
#define OPRT_OS_ADAPTER_ADC_RESERVE2_ERR    (-0x7409) //-29705, RESERVE ERR2
#define OPRT_OS_ADAPTER_ADC_RESERVE3_ERR    (-0x740a) //-29706, RESERVE ERR3
#define OPRT_OS_ADAPTER_ADC_ERRCODE_MAX_CNT 11

/****************************************************************************
            the error code macro define for module OS_ADAPTER_DAC
****************************************************************************/
#define OPRT_OS_ADAPTER_DAC_INVALID_PARM    (-0x7500) //-29952, Invalid parameter
#define OPRT_OS_ADAPTER_DAC_INIT_FAILED     (-0x7501) //-29953, Initialization failed
#define OPRT_OS_ADAPTER_DAC_DEINIT_FAILED   (-0x7502) //-29954, Deinitialization failed
#define OPRT_OS_ADAPTER_DAC_WRITE_FAILED    (-0x7503) //-29955, DAC write failed
#define OPRT_OS_ADAPTER_DAC_MODE_ERR        (-0x7504) //-29956, Mode error
#define OPRT_OS_ADAPTER_DAC_BUSY            (-0x7505) //-29957, Busy
#define OPRT_OS_ADAPTER_DAC_TIMEOUT         (-0x7506) //-29958, Timeout
#define OPRT_OS_ADAPTER_DAC_IRQ_INIT_FAILED (-0x7507) //-29959, Interrupt initialization failed
#define OPRT_OS_ADAPTER_DAC_RESERVE1_ERR    (-0x7508) //-29960, RESERVE ERR1
#define OPRT_OS_ADAPTER_DAC_RESERVE2_ERR    (-0x7509) //-29961, RESERVE ERR2
#define OPRT_OS_ADAPTER_DAC_RESERVE3_ERR    (-0x750a) //-29962, RESERVE ERR3
#define OPRT_OS_ADAPTER_DAC_ERRCODE_MAX_CNT 11

/****************************************************************************
            the error code marco define for module OS_ADAPTER_I2S
****************************************************************************/
#define OPRT_OS_ADAPTER_I2S_INVALID_PARM    (-0x7600) //-30208, Invalid parameter
#define OPRT_OS_ADAPTER_I2S_INIT_FAILED     (-0x7601) //-30209, Initialization failed
#define OPRT_OS_ADAPTER_I2S_DEINIT_FAILED   (-0x7602) //-30210, Deinitialization failed
#define OPRT_OS_ADAPTER_I2S_WRITE_FAILED    (-0x7603) //-30211, I2S write failed
#define OPRT_OS_ADAPTER_I2S_READ_FAILED     (-0x7604) //-30212, I2S read failed
#define OPRT_OS_ADAPTER_I2S_MODE_ERR        (-0x7605) //-30213, Mode error
#define OPRT_OS_ADAPTER_I2S_BUSY            (-0x7606) //-30214, Busy
#define OPRT_OS_ADAPTER_I2S_TIMEOUT         (-0x7607) //-30215, Timeout
#define OPRT_OS_ADAPTER_I2S_DATA_LOST       (-0x7608) //-30216, Data lost
#define OPRT_OS_ADAPTER_I2S_TRANS_ERR       (-0x7609) //-30217, Transmission error
#define OPRT_OS_ADAPTER_I2S_IRQ_INIT_FAILED (-0x760a) //-30218, Interrupt initialization failed
#define OPRT_OS_ADAPTER_I2S_RESERVE1_ERR    (-0x760b) //-30219, RESERVE ERR1
#define OPRT_OS_ADAPTER_I2S_RESERVE2_ERR    (-0x760c) //-30220, RESERVE ERR2
#define OPRT_OS_ADAPTER_I2S_RESERVE3_ERR    (-0x760d) //-30221, RESERVE ERR3
#define OPRT_OS_ADAPTER_I2S_ERRCODE_MAX_CNT 14

/****************************************************************************
            the error code macro define for module GW_SUB_MGR
****************************************************************************/
#define OPRT_GW_SUB_MGR_INIT_FAIL               (-0x7700) //-30464, Sub-device management module initialization failed
#define OPRT_GW_SUB_MGR_BIND_THREAD_CREATE_FAIL (-0x7701) //-30465, Binding thread creation failed
#define OPRT_GW_SUB_MGR_EXCEED_MAX_CNT          (-0x7702) //-30466, Exceeded the maximum number of sub-devices
#define OPRT_GW_SUB_MGR_NODE_CREATE_FAIL        (-0x7703) //-30467, Sub-device node creation failed
#define OPRT_GW_SUB_MGR_CLOUD_HTTP_BIND_FAIL    (-0x7704) //-30468, Cloud HTTP interaction failed
#define OPRT_GW_SUB_MGR_ERRCODE_MAX_CNT         5

/****************************************************************************
            the error code macro define for module APP_TMP
****************************************************************************/
#define OPRT_APP_TMP_xxx             (-0x7800) //-30720, XXX
#define OPRT_APP_TMP_ERRCODE_MAX_CNT 1

/****************************************************************************
            the error code macro define for module IMM_COM
****************************************************************************/
#define OPRT_IMM_COM_MEM_PARTITION_FULL (-0x7900) //-30976, Memory buffer full
#define OPRT_IMM_COM_ERRCODE_MAX_CNT    1

/****************************************************************************
            the error code macro define for module IMM_CLOUD_STORAGE
****************************************************************************/
#define OPRT_IMM_CLOUD_STORAGE_ENCRYPT_KEY_UPDATED    (-0x7a00) //-31232, Storage buffer full
#define OPRT_IMM_CLOUD_STORAGE_STATUS_OFF             (-0x7a01) //-31233, Cloud storage off status
#define OPRT_IMM_CLOUD_STORAGE_UPLOAD_INTERRUPT_ERROR (-0x7a02) //-31234, Network report caused interruption
#define OPRT_IMM_CLOUD_STORAGE_EVENT_STATUS_INVALID   (-0x7a03) //-31235, Event status error
#define OPRT_IMM_CLOUD_STORAGE_ERRCODE_MAX_CNT        4

#define ERRCODE2STRING(errcode)    #errcode
#define TUYA_ERROR_STRING(errcode) ("[ErrCode: " ERRCODE2STRING(errcode) "]")

#define TUYA_CHECK_NULL_RETURN(x, y)                                                                                   \
    do {                                                                                                               \
        if (NULL == (x)) {                                                                                             \
            PR_ERR("%s null", #x);                                                                                     \
            return (y);                                                                                                \
        }                                                                                                              \
    } while (0)

#define TUYA_CHECK_NULL_GOTO(x, label)                                                                                 \
    do {                                                                                                               \
        if (NULL == (x)) {                                                                                             \
            PR_ERR("%s null", #x);                                                                                     \
            goto label;                                                                                                \
        }                                                                                                              \
    } while (0)

#define TUYA_CALL_ERR_LOG(func)                                                                                        \
    do {                                                                                                               \
        rt = (func);                                                                                                   \
        if (OPRT_OK != (rt)) {                                                                                         \
            PR_ERR("ret:%d", rt);                                                                                      \
        }                                                                                                              \
    } while (0)

#define TUYA_CALL_ERR_GOTO(func, label)                                                                                \
    do {                                                                                                               \
        rt = (func);                                                                                                   \
        if (OPRT_OK != (rt)) {                                                                                         \
            PR_ERR("ret:%d", rt);                                                                                      \
            goto label;                                                                                                \
        }                                                                                                              \
    } while (0)

#define TUYA_CALL_ERR_RETURN(func)                                                                                     \
    do {                                                                                                               \
        rt = (func);                                                                                                   \
        if (OPRT_OK != (rt)) {                                                                                         \
            PR_ERR("ret:%d", rt);                                                                                      \
            return (rt);                                                                                               \
        }                                                                                                              \
    } while (0)

#define TUYA_CALL_ERR_RETURN_VAL(func, y)                                                                              \
    do {                                                                                                               \
        rt = (func);                                                                                                   \
        if (OPRT_OK != (rt)) {                                                                                         \
            PR_ERR("ret:%d", rt);                                                                                      \
            return (y);                                                                                                \
        }                                                                                                              \
    } while (0)

// clang-format on

#ifdef __cplusplus
}
#endif
#endif
