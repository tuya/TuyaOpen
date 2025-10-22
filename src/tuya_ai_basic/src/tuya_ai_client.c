/**
 * @file tuya_ai_client.c
 * @brief This file contains the implementation of Tuya AI client core functionality,
 * including connection management, network communication and protocol handling.
 *
 * The Tuya AI client module provides the underlying communication framework for AI services,
 * handling network connections, data transmission and protocol processing. It implements
 * automatic reconnection mechanisms and ping-pong keepalive for stable connections.
 *
 * Key features include:
 * - Configurable reconnection attempts (AI_RECONN_TIME_NUM)
 * - Customizable ping timeout settings (AT_PING_TIMEOUT)
 * - Thread stack size configuration (AI_CLIENT_STACK_SIZE)
 * - Secure communication through tal_security integration
 * - Asynchronous task processing via work queue service
 * - Network state management with netmgr integration
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_log.h"
#include "uni_random.h"
#include "tal_system.h"
#include "tal_hash.h"
#include "tal_thread.h"
#include "tal_security.h"
#include "tal_sw_timer.h"
#include "tal_workq_service.h"
#include "tal_memory.h"
#include "tuya_ai_client.h"
#include "tuya_ai_biz.h"
#include "netmgr.h"

#define AI_RECONN_TIME_NUM 7
#define AT_PING_TIMEOUT    6

#ifndef AI_CLIENT_STACK_SIZE
#define AI_CLIENT_STACK_SIZE 4096
#endif

typedef struct {
    uint32_t min;
    uint32_t max;
} AI_RECONN_TIME_T;

typedef enum {
    AI_STATE_IDLE,
    AI_STATE_SETUP,
    AI_STATE_CONNECT,
    AI_STATE_CLIENT_HELLO,
    AI_STATE_AUTH_REQ,
    AI_STATE_AUTH_RESP,
    AI_STATE_RUNNING,

    AI_STATE_END
} AI_CLIENT_STATE_E;

typedef struct {
    uint32_t reconn_cnt;
    AI_RECONN_TIME_T reconn[AI_RECONN_TIME_NUM];
    THREAD_HANDLE thread;
    TIMER_ID tid;
    AI_CLIENT_STATE_E state;
    uint32_t heartbeat_interval;
    DELAYED_WORK_HANDLE alive_work;
    TIMER_ID alive_timeout_timer;
    uint8_t heartbeat_lost_cnt;
    AI_BASIC_DATA_HANDLE cb;
} AI_BASIC_CLIENT_T;

static AI_BASIC_CLIENT_T *ai_basic_client = NULL;

static uint32_t __ai_get_random_value(uint32_t min, uint32_t max)
{
    return min + uni_random() % (max - min + 1);
}

static void __ai_client_set_state(AI_CLIENT_STATE_E state)
{
    PR_NOTICE("***** ai client state %d -> %d *****", ai_basic_client->state, state);
    ai_basic_client->state = state;
}

static OPERATE_RET __ai_connect()
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_basic_connect();
    if (OPRT_OK != rt) {
        PR_ERR("connect failed, rt:%d", rt);
        return rt;
    }
    ai_basic_client->reconn_cnt = 0;
    __ai_client_set_state(AI_STATE_CLIENT_HELLO);
    return rt;
}

static OPERATE_RET __ai_client_hello()
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_basic_client_hello();
    if (OPRT_OK != rt) {
        PR_ERR("send client hello failed, rt:%d", rt);
        return rt;
    }
    __ai_client_set_state(AI_STATE_AUTH_REQ);
    return rt;
}

static OPERATE_RET __ai_auth_req(void)
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_basic_auth_req();
    if (OPRT_OK != rt) {
        PR_ERR("send auth req failed, rt:%d", rt);
        return rt;
    }
    __ai_client_set_state(AI_STATE_AUTH_RESP);
    return rt;
}

static OPERATE_RET __ai_auth_resp(void)
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_auth_resp();
    if (OPRT_OK != rt) {
        PR_ERR("recv auth resp failed, rt:%d", rt);
        return rt;
    }
    ai_basic_client->heartbeat_lost_cnt = 0;
    tal_workq_start_delayed(ai_basic_client->alive_work, (ai_basic_client->heartbeat_interval * 1000), LOOP_ONCE);
    __ai_client_set_state(AI_STATE_RUNNING);
    tal_event_publish(EVENT_AI_CLIENT_RUN, NULL);
    return rt;
}

static void __ai_conn_refresh(TIMER_ID timerID, void *pTimerArg)
{
    tuya_ai_basic_refresh_req();
    return;
}

static OPERATE_RET __ai_conn_close(void)
{
    OPERATE_RET rt = OPRT_OK;
    tal_event_publish(EVENT_AI_CLIENT_CLOSE, NULL);
    tuya_ai_client_stop_ping();
    tuya_ai_basic_conn_close(AI_CODE_CLOSE_BY_CLIENT);
    __ai_client_set_state(AI_STATE_IDLE);
    return rt;
}

static void __ai_client_handle_err()
{
    if (ai_basic_client->state == AI_STATE_SETUP) {
        uint32_t sleep_random = 0;
        uint32_t size = AI_RECONN_TIME_NUM - 1;
        sleep_random = __ai_get_random_value(ai_basic_client->reconn[ai_basic_client->reconn_cnt].min,
                                             ai_basic_client->reconn[ai_basic_client->reconn_cnt].max);
        PR_NOTICE("connect to cloud failed, get config and connect after %d s", sleep_random);
        tal_system_sleep(sleep_random * 1000);
        if (ai_basic_client->reconn_cnt >= size) {
            ai_basic_client->reconn_cnt = size;
        } else {
            ai_basic_client->reconn_cnt++;
        }
    } else if ((ai_basic_client->state == AI_STATE_CONNECT) || (ai_basic_client->state == AI_STATE_AUTH_RESP)) {
        tal_system_sleep(1000);
        __ai_client_set_state(AI_STATE_SETUP);
    } else if (ai_basic_client->state == AI_STATE_RUNNING) {
        PR_NOTICE("ai client running error, reconnect");
        __ai_conn_close();
    } else {
        tal_system_sleep(1000);
    }
}

static void __ai_stop_alive_time()
{
    ai_basic_client->heartbeat_lost_cnt = 0;
    tal_sw_timer_stop(ai_basic_client->alive_timeout_timer);
    return;
}

static void __ai_start_expire_tid()
{
    uint64_t expire = tuya_ai_basic_get_atop_cfg()->expire;
    uint64_t current = tal_time_get_posix();
    if (expire <= current) {
        PR_ERR("expire time is invalid, expire:%llu, current:%llu", expire, current);
        return;
    }
    tal_sw_timer_stop(ai_basic_client->tid);
    tal_sw_timer_start(ai_basic_client->tid, (expire - current - 10) * 1000, TAL_TIMER_ONCE); // 10s before expire
    PR_NOTICE("connect refresh success,expire:%llu current:%llu next %d s", expire, current, expire - current - 10);
}

static void __ai_handle_refresh_resp(char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PAYLOAD_HEAD_T *packet = (AI_PAYLOAD_HEAD_T *)data;
    if (packet->attribute_flag != AI_HAS_ATTR) {
        PR_ERR("refresh resp packet has no attribute");
        return;
    }

    uint32_t attr_len = 0;
    memcpy(&attr_len, data + sizeof(AI_PAYLOAD_HEAD_T), sizeof(attr_len));
    attr_len = UNI_NTOHL(attr_len);
    uint32_t offset = sizeof(AI_PAYLOAD_HEAD_T) + sizeof(attr_len);
    rt = tuya_ai_refresh_resp(data + offset, attr_len);
    if (OPRT_OK != rt) {
        PR_ERR("refresh resp failed, rt:%d", rt);
        return;
    }

    __ai_start_expire_tid();
    return;
}

static void __ai_handle_conn_close(char *data, uint32_t len)
{
    PR_NOTICE("recv conn close by server");
    AI_PAYLOAD_HEAD_T *packet = (AI_PAYLOAD_HEAD_T *)data;
    if (packet->attribute_flag != AI_HAS_ATTR) {
        PR_ERR("refresh resp packet has no attribute");
        return;
    }

    uint32_t attr_len = 0;
    memcpy(&attr_len, data + sizeof(AI_PAYLOAD_HEAD_T), sizeof(attr_len));
    attr_len = UNI_NTOHL(attr_len);
    uint32_t offset = sizeof(AI_PAYLOAD_HEAD_T) + sizeof(attr_len);
    tuya_ai_parse_conn_close(data + offset, attr_len);
    tal_event_publish(EVENT_AI_CLIENT_CLOSE, NULL);
    __ai_client_set_state(AI_STATE_SETUP);
    return;
}

static void __ai_handle_pong(char *data, uint32_t len)
{
    tuya_ai_pong(data, len);
    tal_workq_start_delayed(ai_basic_client->alive_work, (ai_basic_client->heartbeat_interval * 1000), LOOP_ONCE);
    PR_NOTICE("ai pong");
}

static OPERATE_RET __ai_running(void)
{
    OPERATE_RET rt = OPRT_OK;
    char *de_buf = NULL;
    uint32_t de_len = 0;
    AI_FRAG_FLAG frag = AI_PACKET_NO_FRAG;

    rt = tuya_ai_basic_pkt_read(&de_buf, &de_len, &frag);
    if (OPRT_RESOURCE_NOT_READY == rt) {
        return OPRT_OK;
    } else if ((OPRT_OK != rt) || (de_buf == NULL)) {
        AI_PROTO_D("recv and parse data failed, rt:%d", rt);
        return rt;
    }
    __ai_stop_alive_time();
    if ((frag == AI_PACKET_NO_FRAG) || (frag == AI_PACKET_FRAG_START)) {
        AI_PACKET_PT pkt_type = tuya_ai_basic_get_pkt_type(de_buf);
        AI_PROTO_D("ai recv data type:%d, %d", pkt_type, de_len);
        if (pkt_type == AI_PT_PONG) {
            __ai_handle_pong(de_buf, de_len);
        } else if (pkt_type == AI_PT_CONN_REFRESH_RESP) {
            __ai_handle_refresh_resp(de_buf, de_len);
        } else if (pkt_type == AI_PT_CONN_CLOSE) {
            __ai_handle_conn_close(de_buf, de_len);
        } else {
            if (ai_basic_client->cb) {
                ai_basic_client->cb(de_buf, de_len, frag);
            }
        }
    } else {
        if (ai_basic_client->cb) {
            ai_basic_client->cb(de_buf, de_len, frag);
        }
    }

    tuya_ai_basic_pkt_free(de_buf);
    return rt;
}

static OPERATE_RET __ai_idle()
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    if (status != NETMGR_LINK_UP) {
        return OPRT_COM_ERROR;
    }

    __ai_client_set_state(AI_STATE_SETUP);
    return OPRT_OK;
}

static OPERATE_RET __ai_setup()
{
    OPERATE_RET rt = OPRT_OK;

    rt = tuya_ai_basic_atop_req();
    if (OPRT_OK != rt) {
        return rt;
    }

    __ai_start_expire_tid();
    tal_workq_stop_delayed(ai_basic_client->alive_work);
    __ai_client_set_state(AI_STATE_CONNECT);
    return OPRT_OK;
}

static void __ai_basic_client_deinit(void)
{
    if (ai_basic_client->thread) {
        tal_thread_delete(ai_basic_client->thread);
        ai_basic_client->thread = NULL;
    }
    if (ai_basic_client->tid) {
        tal_sw_timer_delete(ai_basic_client->tid);
        ai_basic_client->tid = NULL;
    }
    if (ai_basic_client->alive_timeout_timer) {
        tal_sw_timer_delete(ai_basic_client->alive_timeout_timer);
        ai_basic_client->alive_timeout_timer = NULL;
    }
    if (ai_basic_client->alive_work) {
        tal_workq_stop_delayed(ai_basic_client->alive_work);
    }
    Free(ai_basic_client);
    ai_basic_client = NULL;
    return;
}

static void __ai_client_thread_cb(void *args)
{
    OPERATE_RET rt = OPRT_OK;
    while (tal_thread_get_state(ai_basic_client->thread) == THREAD_STATE_RUNNING) {
        switch (ai_basic_client->state) {
        case AI_STATE_IDLE:
            rt = __ai_idle();
            break;
        case AI_STATE_SETUP:
            rt = __ai_setup();
            break;
        case AI_STATE_CONNECT:
            rt = __ai_connect();
            break;
        case AI_STATE_CLIENT_HELLO:
            rt = __ai_client_hello();
            break;
        case AI_STATE_AUTH_REQ:
            rt = __ai_auth_req();
            break;
        case AI_STATE_AUTH_RESP:
            rt = __ai_auth_resp();
            break;
        case AI_STATE_RUNNING:
            rt = __ai_running();
            break;
        default:
            break;
        }
        if (OPRT_OK != rt) {
            __ai_client_handle_err();
        }
    }

    __ai_basic_client_deinit();
    PR_NOTICE("ai client thread exit");
}

static OPERATE_RET __ai_client_create_task(void)
{
    OPERATE_RET rt = OPRT_OK;
    THREAD_CFG_T thrd_param = {0};
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "ai_client_thread";
    thrd_param.stackDepth = AI_CLIENT_STACK_SIZE;
#if defined(AI_STACK_IN_PSRAM) && (AI_STACK_IN_PSRAM == 1)
    thrd_param.psram_mode = 1;
#endif

    rt = tal_thread_create_and_start(&ai_basic_client->thread, NULL, NULL, __ai_client_thread_cb, NULL, &thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("ai client thread create err, rt:%d", rt);
    }
    return rt;
}

static void __ai_ping(void *data)
{
    OPERATE_RET rt = OPRT_OK;
    tal_sw_timer_start(ai_basic_client->alive_timeout_timer, AT_PING_TIMEOUT * 1000, TAL_TIMER_ONCE);
    rt = tuya_ai_basic_ping();
    if (OPRT_OK != rt) {
        PR_ERR("send ping to cloud failed, rt:%d", rt);
    }
}

static void __ai_alive_timeout(TIMER_ID timer_id, void *data)
{
    PR_ERR("alive timeout");
    ai_basic_client->heartbeat_lost_cnt++;
    if (ai_basic_client->heartbeat_lost_cnt >= 3) {
        PR_ERR("ping lost >= 3, close tcp connection");
        __ai_conn_close();
    } else {
        AI_PROTO_D("start ping, %p", ai_basic_client->alive_work);
        tal_workq_start_delayed(ai_basic_client->alive_work, 10, LOOP_ONCE);
        AI_PROTO_D("start ping success");
    }
}

void tuya_ai_client_reg_cb(AI_BASIC_DATA_HANDLE cb)
{
    if (ai_basic_client) {
        ai_basic_client->cb = cb;
        AI_PROTO_D("register ai client cb success");
    }
}

uint8_t tuya_ai_client_is_ready(void)
{
    if (ai_basic_client) {
        if (ai_basic_client->state == AI_STATE_RUNNING) {
            return true;
        }
    }
    return false;
}

void tuya_ai_client_stop_ping(void)
{
    if (ai_basic_client) {
        tal_workq_stop_delayed(ai_basic_client->alive_work);
        __ai_stop_alive_time();
    }
}

void tuya_ai_client_start_ping(void)
{
    __ai_ping(NULL);
}

OPERATE_RET tuya_ai_client_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_basic_client) {
        return OPRT_OK;
    }
    ai_basic_client = Malloc(sizeof(AI_BASIC_CLIENT_T));
    TUYA_CHECK_NULL_RETURN(ai_basic_client, OPRT_MALLOC_FAILED);

    memset(ai_basic_client, 0, sizeof(AI_BASIC_CLIENT_T));
    ai_basic_client->heartbeat_interval = 30;
    AI_RECONN_TIME_T reconn[AI_RECONN_TIME_NUM] = {{5, 10},   {10, 20},   {20, 40},  {40, 80},
                                                   {80, 160}, {160, 320}, {320, 640}};
    memcpy(ai_basic_client->reconn, reconn, sizeof(reconn));
    tuya_ai_biz_init();
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_conn_refresh, NULL, &ai_basic_client->tid), EXIT);
    TUYA_CALL_ERR_GOTO(__ai_client_create_task(), EXIT);
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_alive_timeout, NULL, &ai_basic_client->alive_timeout_timer), EXIT);
    TUYA_CALL_ERR_GOTO(tal_workq_init_delayed(WORKQ_HIGHTPRI, __ai_ping, NULL, &ai_basic_client->alive_work), EXIT);
    PR_NOTICE("ai client init success");
    return rt;

EXIT:
    PR_ERR("ai client init failed");
    __ai_basic_client_deinit();
    return OPRT_COM_ERROR;
}
