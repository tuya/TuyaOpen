/**
 * @file http_manager.h
 * @brief http_manager module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __HTTP_MANAGER_H__
#define __HTTP_MANAGER_H__

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "tuya_ai_types.h"
#include "http_inf.h"
#include "httpc.h"
#include "http_parser.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#ifndef MAX_HTTP_SESSION_NUM
#define MAX_HTTP_SESSION_NUM 16 // max number of active session
#endif
#define INVALID_HTTP_SESSION_ID 0xFFFFffff // invalid HTTP session handle
#define MAX_HTTP_URL_LEN        1024       // max len of url

/***********************************************************
***********************typedef define***********************
***********************************************************/

typedef enum {
    HTTP_VER_1_0,
    HTTP_VER_1_1,
} http_ver_t;

typedef void (*HTTP_HEAD_ADD_CB)(http_session_t session, void *data);

typedef enum http_method http_method_t;

typedef struct {
    void *pri_data;
} http_custom_content_ctx_s;

typedef struct {
    /** The Type of HTTP Request */
    http_method_t type;
    /** The target resource for the HTTP Request. A complete URL is also
     * accepted.
     */
    const char *resource;
    /** initialzied redirect count, default is zero */
    unsigned char redirect_cnt;
    /** The HTTP Protocol Version */
    http_ver_t version;
    /** Pointer to data buffer. NULL if GET request */
    const char *content;
    /** The length of the data pointed to by \a content above. This is
     * don't-care if the content is set to NULL
     */
    int content_len;
    HTTP_HEAD_ADD_CB add_head_cb;
    void *add_head_data;
    unsigned int download_offset;
    unsigned int download_size;

    http_custom_content_ctx_s *p_custom_content_ctx;
} http_req_t;

/**
 * Structure used to give back http header response fields to the caller.
 */
typedef struct {
    /** The value of the protocol field in the first line of the HTTP
        header response. e.g. "HTTP". */
    const char *protocol;
    /** HTTP version */
    http_ver_t version;
    /** The status code returned as a part of the first line of the
        HTTP response e.g. 200 if success
    */
    int status_code;
    /** The ASCII string present in the first line of HTTP response. It
        is the verbose representation of status code. e.g. "OK" if
        status_code is 200
    */
    const char *reason_phrase;
    /** HTTP "Location" header field value */
    const char *location;
    /** HTTP "Server" header field value */
    const char *server;
    /** Accept-Ranges */
    const char *p_accept_ranges;
    /** Last-Modified header field value in POSIX time format */
    time_t modify_time;
    /** The value of "Content-Type" header field. e.g. "text/html" */
    const char *content_type;
    /** The value of "Content-Encoding" header field e.g. "gzip" */
    const char *content_encoding;
    /** If "Keep-Alive" field is present or if the value of
        "Connection" field is "Keep-Alive" then this member is set to
        'true'. It is set to 'false' in other cases
    */
    bool_t keep_alive_ack;
    /** If "Keep-Alive" field is present in the response, this member
        contains the value of the "timeout" sub-field of this
        field. This is the time the server will allow an idle
        connection to remain open before it is closed.
    */
    int keep_alive_timeout;
    /** If "Keep-Alive" field is present in the response, this member
        contains the value of the "max" sub-field of this field. The
        max parameter indicates the maximum number of requests that a
        client will make, or that a server will allow to be made on the
        persistent connection.
    */
    int keep_alive_max;
    /** This will be 'true' if "Transfer-Encoding" field is set to
        "chunked". Note that this is only for information and the API
        http_read_content() transparently handles chunked reads.
    */
    bool_t chunked;
    /** Value of the "Content-Length" field. If "Transfer-Encoding" is
        set to chunked then this value will be zero.
    */
    unsigned int content_length;
} http_resp_t;

/**
 * @brief Definition of HTTP session state
 */
typedef enum {
    // HTTP_FREE = 0,   // session is free
    HTTP_DISCONNECT = 1, // session is disconnected
    HTTP_CONNECTING,     // session is connecting
    HTTP_CONNECTED,      // session is connected and ready to send/recv
    HTTP_UPLOADING,      // session has sent data and is ready to recv
} E_HTTP_SESSION_STATE;

/**
 * The OR of zero or more flags below is passed to the API
 * http_prepare_req(). If the a flag is passed the corresponding HTTP
 * header field is added to the HTTP header. The values added will be
 * default ones.
 */
typedef enum {
    HDR_ADD_DEFAULT_USER_AGENT = 0x0001,
} http_hdr_field_sel_t;

/**
 * @brief The HTTP session's Request structure
 */
typedef struct {
    /** internal HTTP session */
    http_session_t s;

    /** persistent or not */
    bool is_persistent;

    /** hdr field flags */
    http_hdr_field_sel_t flags;

    /** url content */
    char url[MAX_HTTP_URL_LEN];

    /** http request */
    http_req_t req;

    /** session state, please refer to enum E_HTTP_SESSION_STATE */
    E_HTTP_SESSION_STATE state;
} S_HTTP_SESSION;

typedef S_HTTP_SESSION *SESSION_ID;

/**
 * @brief This API is used to create HTTP session
 *
 * @param[in] url URL of HTTP session, max len of URL is 256
 * @param[in] is_persistent Session is persistent or not
 *
 * @return SESSION_ID on success, NULL on error
 */
typedef SESSION_ID (*FUNC_HTTP_SESSION_CREATE)(IN CONST char *url, BOOL_T is_persistent);

/**
 * @brief This API is used to create HTTP session
 *
 * @param[in] url URL of HTTP session, max len of URL is 256
 * @param[in] is_persistent Session is persistent or not
 *
 * @return SESSION_ID on success, NULL on error
 */
typedef SESSION_ID (*FUNC_HTTP_SESSION_CREATE_TLS)(IN CONST char *url, BOOL_T is_persistent,
                                                   tuya_tls_config_t *config);

/**
 * @brief This API is used to send HTTP session request
 *
 * @param[in] session Session returned from the call to FUNC_HTTP_SESSION_CREATE
 * @param[in] req The http_req_t structure filled up with appropriate parameters
 * @param[in] field_flags The http_hdr_field_sel_t, OR of zero or more flags
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*FUNC_HTTP_SESSION_SEND)(IN CONST SESSION_ID session, IN CONST http_req_t *req,
                                              http_hdr_field_sel_t field_flags);

/**
 * @brief This API is used to recv response header from HTTP session
 *
 * @param[in] session Session returned from the call to FUNC_HTTP_SESSION_CREATE
 * @param[in,out] resp Pointer to a pointer of type http_resp_t .The
 * structure will be allocated by the callee. Note that the caller does
 * not need to free the structure allocated and returned from this
 * API. The allocation and free is done by the callee automatically
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*FUNC_HTTP_SESSION_RECEIVE)(SESSION_ID session, http_resp_t **resp);

/**
 * @brief This API is used to destroy HTTP session
 *
 * @param[in] session Session returned from the call to FUNC_HTTP_SESSION_CREATE
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*FUNC_HTTP_SESSION_DESTORY)(SESSION_ID session);

/**
 * @brief This API is used to recv response content from HTTP session.
 *
 * @param[in] session Session returned from the call to FUNC_HTTP_SESSION_CREATE
 * @param[in] pResp The http_resp_t structure returned from the call to FUNC_HTTP_SESSION_RECEIVE
 * @param[in, out] pDataOut Caller allocated buffer
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*FUNC_HTTP_SESSION_RECEIVE_DATA)(SESSION_ID session, http_resp_t *pResp, uint8_t **pDataOut);

/**
 * @brief The HTTP session manager structure.
 */
typedef struct {
    /** session resources */
    S_HTTP_SESSION *session[MAX_HTTP_SESSION_NUM];
    /** exclusive access to session resources*/
    MUTEX_HANDLE mutex;

    /** session manager inited or not */
    bool inited;

    /** handler to create HTTP session */
    FUNC_HTTP_SESSION_CREATE create_http_session;
    /** handler to create HTTPS session with TLS config */
    FUNC_HTTP_SESSION_CREATE_TLS create_http_session_tls;
    /** handler to send HTTP session request */
    FUNC_HTTP_SESSION_SEND send_http_request;
    /** handler to recv response header from HTTP session */
    FUNC_HTTP_SESSION_RECEIVE receive_http_response;
    /** handler to destroy HTTP session */
    FUNC_HTTP_SESSION_DESTORY destory_http_session;
    /** handler to recv response content from HTTP session */
    FUNC_HTTP_SESSION_RECEIVE_DATA receive_http_data;
} S_HTTP_MANAGER;

/**
 * @brief Retrieve instance of HTTP session manager
 *
 * @note HTTP manager is singleton, multiple call to this API will return the same instance
 *
 * @return NULL on error. Instance on success, please refer to S_HTTP_MANAGER
 */
S_HTTP_MANAGER *get_http_manager_instance(VOID_T);

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_MANAGER_H__ */
