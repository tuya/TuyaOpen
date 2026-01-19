/**
 * @file tuya_ai_client.c
 * @author tuya
 * @brief ai client
 * @version 0.1
 * @date 2025-03-02
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#include "gw_intf.h"
#include "uni_log.h"
#include "uni_random.h"
#include "tal_system.h"
#include "tal_hash.h"
#include "tal_thread.h"
#include "tal_security.h"
#include "tal_sw_timer.h"
#include "tal_time_service.h"
#include "tuya_svc_netmgr.h"
#include "tal_workq_service.h"
#include "tal_memory.h"
#include "tuya_ai_client.h"
#include "base_event.h"
#include "base_event_info.h"
#include "tuya_ai_biz.h"
#include "tal_semaphore.h"
#include "tuya_ai_mqtt.h"
#include "tuya_ai_private.h"

#define AI_RECONN_TIME_NUM 7
#ifndef AT_PING_TIMEOUT
#define AT_PING_TIMEOUT 6
#endif
#ifndef AI_HEARTBEAT_INTERVAL
#define AI_HEARTBEAT_INTERVAL 120
#endif

#ifndef AI_CLIENT_STACK_SIZE
#define AI_CLIENT_STACK_SIZE 4096
#endif

#define AI_IDLE_CHECK_TIME    (30 * 60 * 1000) // 30 minutes

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
    BOOL_T terminate;
    TIME_T start_time;
    BOOL_T idle_check_enable;
    TIMER_ID idle_check_timer;
    BOOL_T recv_biz_pkt;
} AI_BASIC_CLIENT_T;

STATIC AI_BASIC_CLIENT_T *ai_basic_client = NULL;

STATIC uint32_t __ai_get_random_value(uint32_t min, uint32_t max)
{
    return min + uni_random() % (max - min + 1);
}

STATIC VOID __ai_client_set_state(AI_CLIENT_STATE_E state)
{
    PR_NOTICE("***** ai client state %d -> %d *****", ai_basic_client->state, state);
    ai_basic_client->state = state;
}

STATIC OPERATE_RET __ai_connect()
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_basic_connect(tuya_ai_mq_ser_cfg_get());
    if (OPRT_OK != rt) {
        PR_ERR("connect failed, rt:%d", rt);
        return rt;
    }
    ai_basic_client->reconn_cnt = 0;
    __ai_client_set_state(AI_STATE_CLIENT_HELLO);
    return rt;
}

STATIC OPERATE_RET __ai_client_hello()
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_basic_client_hello(tuya_ai_mq_ser_cfg_get());
    if (OPRT_OK != rt) {
        PR_ERR("send client hello failed, rt:%d", rt);
        return rt;
    }
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    __ai_client_set_state(AI_STATE_AUTH_REQ);
#else
    __ai_client_set_state(AI_STATE_AUTH_RESP);
#endif
    return rt;
}

STATIC OPERATE_RET __ai_auth_req(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_basic_auth_req(tuya_ai_mq_ser_cfg_get());
    if (OPRT_OK != rt) {
        PR_ERR("send auth req failed, rt:%d", rt);
        return rt;
    }
    __ai_client_set_state(AI_STATE_AUTH_RESP);
    return rt;
}

STATIC OPERATE_RET __ai_auth_resp(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_auth_resp();
    if (OPRT_OK != rt) {
        return rt;
    }
    ai_basic_client->heartbeat_lost_cnt = 0;
    tuya_ai_client_start_ping();
    ai_basic_client->start_time = tal_time_get_posix();
    ai_basic_client->recv_biz_pkt = FALSE;
    ai_basic_client->idle_check_enable = FALSE;
    tal_sw_timer_start(ai_basic_client->idle_check_timer, AI_IDLE_CHECK_TIME, TAL_TIMER_ONCE);
    __ai_client_set_state(AI_STATE_RUNNING);
    ty_publish_event(EVENT_AI_CLIENT_RUN, NULL);
    return rt;
}

STATIC VOID __ai_conn_refresh(TIMER_ID timerID, void * pTimerArg)
{
    tuya_ai_basic_refresh_req();
    return;
}

STATIC OPERATE_RET __ai_conn_close(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    ty_publish_event(EVENT_AI_CLIENT_CLOSE, NULL);
    tuya_ai_client_stop_ping();
    tuya_ai_basic_conn_close(AI_CODE_CLOSE_BY_CLIENT);
    __ai_client_set_state(AI_STATE_IDLE);
    return rt;
}

STATIC VOID __ai_client_handle_err(OPERATE_RET rt)
{
    if (ai_basic_client->state == AI_STATE_SETUP) {
        uint32_t sleep_random = 0;
        uint32_t size = AI_RECONN_TIME_NUM - 1;
        sleep_random = __ai_get_random_value(ai_basic_client->reconn[ai_basic_client->reconn_cnt].min, ai_basic_client->reconn[ai_basic_client->reconn_cnt].max);
        PR_NOTICE("connect to cloud failed, sleep %d s", sleep_random);
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
        PR_NOTICE("ai client running error %d, reconnect %d", rt, tal_net_get_errno());
        __ai_conn_close();
    } else {
        tal_system_sleep(1000);
    }
}

STATIC VOID __ai_stop_alive_time()
{
    ai_basic_client->heartbeat_lost_cnt = 0;
    tal_sw_timer_stop(ai_basic_client->alive_timeout_timer);
    return;
}

STATIC VOID __ai_start_expire_tid()
{
    uint64_t expire = tuya_ai_mq_ser_cfg_get()->expire;
    uint64_t current = tal_time_get_posix();
    if (expire <= current) {
        PR_ERR("expire time is invalid, expire:%llu, current:%llu", expire, current);
        return;
    }
    tal_sw_timer_stop(ai_basic_client->tid);
    tal_sw_timer_start(ai_basic_client->tid, (expire - current - 10) * 1000, TAL_TIMER_ONCE); // 10s before expire
    PR_NOTICE("connect refresh success,expire:%llu current:%llu next %d s", expire, current, expire - current - 10);
}

STATIC VOID __ai_handle_refresh_resp(char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PAYLOAD_HEAD_T *packet = (AI_PAYLOAD_HEAD_T *)data;
    if (packet->attribute_flag != AI_HAS_ATTR) {
        PR_ERR("refresh resp packet has no attribute");
        return;
    }

    uint32_t attr_len = 0;
    memcpy(&attr_len, data + SIZEOF(AI_PAYLOAD_HEAD_T), SIZEOF(attr_len));
    attr_len = UNI_NTOHL(attr_len);
    uint32_t offset = SIZEOF(AI_PAYLOAD_HEAD_T) + SIZEOF(attr_len);
    rt = tuya_ai_refresh_resp(data + offset, attr_len, &(tuya_ai_mq_ser_cfg_get()->expire));
    if (OPRT_OK != rt) {
        PR_ERR("refresh resp failed, rt:%d", rt);
        return;
    }

    __ai_start_expire_tid();
    return;
}

STATIC VOID __ai_handle_conn_close(char *data, uint32_t len)
{
    PR_NOTICE("recv conn close by server");
    AI_PAYLOAD_HEAD_T *packet = (AI_PAYLOAD_HEAD_T *)data;
    if (packet->attribute_flag != AI_HAS_ATTR) {
        PR_ERR("refresh resp packet has no attribute");
        return;
    }

    uint32_t attr_len = 0;
    memcpy(&attr_len, data + SIZEOF(AI_PAYLOAD_HEAD_T), SIZEOF(attr_len));
    attr_len = UNI_NTOHL(attr_len);
    uint32_t offset = SIZEOF(AI_PAYLOAD_HEAD_T) + SIZEOF(attr_len);
    tuya_ai_parse_conn_close(data + offset, attr_len);
    ty_publish_event(EVENT_AI_CLIENT_CLOSE, NULL);
    __ai_client_set_state(AI_STATE_IDLE);
    return;
}

STATIC VOID __ai_handle_pong(char *data, uint32_t len)
{
    tuya_ai_pong(data, len);
    tuya_ai_client_start_ping();
    PR_NOTICE("ai pong");
}

STATIC VOID __ai_delay_dis_req(VOID)
{
    PR_NOTICE("recv delay disconnect pkt");
    if (ai_basic_client) {
        if (!ai_basic_client->idle_check_enable) {
            ai_basic_client->idle_check_enable = TRUE;
            ai_basic_client->recv_biz_pkt = FALSE;
            tal_sw_timer_start(ai_basic_client->idle_check_timer, AI_IDLE_CHECK_TIME, TAL_TIMER_ONCE);
        } else {
            PR_NOTICE("ai already in idle check mode");
        }
    }
}

STATIC VOID __ai_idle_check(TIMER_ID timer_id, VOID_T *data)
{
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    if (ai_basic_client == NULL) {
        PR_ERR("ai client is null");
        return;
    }
    TIME_T now_time = tal_time_get_posix();
    PR_NOTICE("ai idle check, enable:%d, recv pkt:%d", ai_basic_client->idle_check_enable, ai_basic_client->recv_biz_pkt);
    PR_NOTICE("ai client start time %d, now %d", ai_basic_client->start_time, now_time);
    if (ai_basic_client->idle_check_enable) {
        if (!ai_basic_client->recv_biz_pkt) {
            __ai_conn_close();
            return;
        } else {
            ai_basic_client->recv_biz_pkt = FALSE;
        }
    } else {
        uint32_t random_value = uni_random_range(6);
        uint32_t continue_run_time = (12 + random_value) * 60 * 60;
        if ((now_time - ai_basic_client->start_time) > continue_run_time) {
            PR_NOTICE("ai continue run large than %d hours, start idle check", continue_run_time);
            ai_basic_client->idle_check_enable = TRUE;
            ai_basic_client->recv_biz_pkt = FALSE;
        }
    }
    tal_sw_timer_start(ai_basic_client->idle_check_timer, AI_IDLE_CHECK_TIME, TAL_TIMER_ONCE);
#endif
    return;
}

STATIC OPERATE_RET __ai_running(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    char *de_buf = NULL;
    uint32_t de_len = 0;
    AI_FRAG_FLAG frag = AI_PACKET_NO_FRAG;
    uint32_t cnt = 0;

    rt = tuya_ai_basic_pkt_read(&de_buf, &de_len, &frag);
    if (OPRT_RESOURCE_NOT_READY == rt) {
        return OPRT_OK;
    } else if ((OPRT_OK != rt) || (de_buf == NULL)) {
        if ((rt == -1) && (cnt <= 3)) {
            if (tuya_svc_netmgr_get_status() == NETWORK_STATUS_MQTT) {
                cnt++;
                tal_system_sleep(1000);
                return OPRT_OK;
            }
        }
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
        } else if (pkt_type == AI_PT_DELAY_DISCONNECT) {
            __ai_delay_dis_req();
        } else {
            ai_basic_client->recv_biz_pkt = TRUE;
            if (ai_basic_client->cb) {
                ai_basic_client->cb(de_buf, de_len, frag);
            }
        }
    } else {
        ai_basic_client->recv_biz_pkt = TRUE;
        if (ai_basic_client->cb) {
            ai_basic_client->cb(de_buf, de_len, frag);
        }
    }

    tuya_ai_basic_pkt_free(de_buf);
    return rt;
}

STATIC OPERATE_RET __ai_idle()
{
    if ((tuya_svc_netmgr_get_status() != NETWORK_STATUS_MQTT) ||
        (tal_time_check_time_sync() != OPRT_OK)) {
        return OPRT_COM_ERROR;
    }
    __ai_client_set_state(AI_STATE_SETUP);
    return OPRT_OK;
}

STATIC OPERATE_RET __ai_setup()
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_basic_setup();
    if (OPRT_OK != rt) {
        return rt;
    }
    rt = tuya_ai_mq_ser_cfg_req();
    if (OPRT_OK != rt) {
        if (rt == OPRT_SVC_MQTT_GW_MQ_OFFLILNE) {
            __ai_client_set_state(AI_STATE_IDLE);
        }
        return rt;
    }
    __ai_start_expire_tid();
    tal_workq_stop_delayed(ai_basic_client->alive_work);
    __ai_client_set_state(AI_STATE_CONNECT);
    return OPRT_OK;
}

VOID tuya_ai_client_deinit(VOID)
{
    if (ai_basic_client) {
        ty_publish_event(EVENT_AI_CLIENT_CLOSE, NULL);
        ai_basic_client->terminate = TRUE;
    }
}

STATIC VOID __ai_client_free(VOID)
{
    tuya_ai_biz_deinit();
    tuya_ai_mq_deinit();
    if (ai_basic_client) {
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
        if (ai_basic_client->idle_check_timer) {
            tal_sw_timer_delete(ai_basic_client->idle_check_timer);
            ai_basic_client->idle_check_timer = NULL;
        }
        if (ai_basic_client->alive_work) {
            tal_workq_cancel_delayed(ai_basic_client->alive_work);
            ai_basic_client->alive_work = NULL;
        }
        OS_FREE(ai_basic_client);
        ai_basic_client = NULL;
        PR_NOTICE("ai client deinit");
    }
    tuya_ai_basic_disconnect();
    return;
}

STATIC VOID __ai_client_thread_cb(void* args)
{
    OPERATE_RET rt = OPRT_OK;
    while (!ai_basic_client->terminate && tal_thread_get_state(ai_basic_client->thread) == THREAD_STATE_RUNNING) {
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
            __ai_client_handle_err(rt);
        }
    }

    __ai_client_free();
    PR_NOTICE("ai client thread exit");
}

STATIC OPERATE_RET __ai_client_create_task(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    THREAD_CFG_T thrd_param = {0};
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "ai_client";
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

STATIC VOID __ai_ping(VOID *data)
{
    OPERATE_RET rt = OPRT_OK;
    tal_sw_timer_start(ai_basic_client->alive_timeout_timer, AT_PING_TIMEOUT * 1000, TAL_TIMER_ONCE);
    rt = tuya_ai_basic_ping();
    if (OPRT_OK != rt) {
        PR_ERR("send ping to cloud failed, rt:%d", rt);
    }
}

STATIC VOID __ai_alive_timeout(TIMER_ID timer_id, VOID_T *data)
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

VOID tuya_ai_client_reg_cb(AI_BASIC_DATA_HANDLE cb)
{
    if (ai_basic_client) {
        ai_basic_client->cb = cb;
        AI_PROTO_D("register ai client cb success");
    }
}

BOOL_T tuya_ai_client_is_ready(VOID)
{
    if (ai_basic_client) {
        if (!(ai_basic_client->terminate) && ai_basic_client->state == AI_STATE_RUNNING) {
            return TRUE;
        }
    }
    return FALSE;
}

VOID tuya_ai_client_stop_ping(VOID)
{
    if (ai_basic_client) {
        tal_workq_stop_delayed(ai_basic_client->alive_work);
        __ai_stop_alive_time();
    }
    AI_PROTO_D("ai stop ping");
}

VOID tuya_ai_client_start_ping(VOID)
{
    tal_workq_start_delayed(ai_basic_client->alive_work, (ai_basic_client->heartbeat_interval * 1000), LOOP_ONCE);
}

OPERATE_RET tuya_ai_client_init(AI_MQTT_RECV_CB cb)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_basic_client) {
        return OPRT_OK;
    }
    ai_basic_client = OS_MALLOC(SIZEOF(AI_BASIC_CLIENT_T));
    TUYA_CHECK_NULL_RETURN(ai_basic_client, OPRT_MALLOC_FAILED);

    memset(ai_basic_client, 0, SIZEOF(AI_BASIC_CLIENT_T));
    ai_basic_client->heartbeat_interval = AI_HEARTBEAT_INTERVAL;
    AI_RECONN_TIME_T reconn[AI_RECONN_TIME_NUM] = {
        {5, 10}, {10, 20}, {20, 40}, {40, 80}, {80, 160}, {160, 320}, {320, 640}
    };
    memcpy(ai_basic_client->reconn, reconn, SIZEOF(reconn));
    tuya_ai_mq_init(cb);
    tuya_ai_biz_init();
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_conn_refresh, NULL, &ai_basic_client->tid), EXIT);
    TUYA_CALL_ERR_GOTO(__ai_client_create_task(), EXIT);
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_alive_timeout, NULL, &ai_basic_client->alive_timeout_timer), EXIT);
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_idle_check, NULL, &ai_basic_client->idle_check_timer), EXIT);
    TUYA_CALL_ERR_GOTO(tal_workq_init_delayed(WORKQ_HIGHTPRI, __ai_ping, NULL, &ai_basic_client->alive_work), EXIT);
    PR_NOTICE("ai client init success, version:%d", AI_VERSION);
    return rt;

EXIT:
    PR_ERR("ai client init failed");
    __ai_client_free();
    return OPRT_COM_ERROR;
}