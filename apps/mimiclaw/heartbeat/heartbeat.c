#include "heartbeat/heartbeat.h"

#include "bus/message_bus.h"
#include "mimi_config.h"

// #include "tal_fs.h"

#include <ctype.h>
#include <string.h>

static const char *TAG = "heartbeat";

#define HEARTBEAT_PROMPT                                                                                               \
    "Read " MIMI_HEARTBEAT_FILE                                                                                        \
    " and follow any instructions or tasks listed there. If nothing needs attention, reply with just: HEARTBEAT_OK"

static TIMER_ID s_heartbeat_timer = NULL;

static bool heartbeat_has_tasks(void)
{
    TUYA_FILE f = mimi_fopen(MIMI_HEARTBEAT_FILE, "r");
    if (!f) {
        return false;
    }

    char line[256]  = {0};
    bool found_task = false;

    while (mimi_fgets(line, sizeof(line), f)) {
        const char *p = line;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }

        if (*p == '\0' || *p == '\n' || *p == '\r') {
            continue;
        }

        if (*p == '#') {
            continue;
        }

        if ((*p == '-' || *p == '*') && *(p + 1) == ' ' && *(p + 2) == '[') {
            char mark = *(p + 3);
            if ((mark == 'x' || mark == 'X') && *(p + 4) == ']') {
                continue;
            }
        }

        found_task = true;
        break;
    }

    mimi_fclose(f);
    return found_task;
}

static bool heartbeat_send(void)
{
    if (!heartbeat_has_tasks()) {
        MIMI_LOGD(TAG, "no actionable tasks in HEARTBEAT.md");
        return false;
    }

    mimi_msg_t msg = {0};
    strncpy(msg.channel, MIMI_CHAN_SYSTEM, sizeof(msg.channel) - 1);
    strncpy(msg.chat_id, "heartbeat", sizeof(msg.chat_id) - 1);
    msg.content = strdup(HEARTBEAT_PROMPT);
    if (!msg.content) {
        MIMI_LOGE(TAG, "alloc heartbeat prompt failed");
        return false;
    }

    OPERATE_RET rt = message_bus_push_inbound(&msg);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "push heartbeat message failed: %d", rt);
        free(msg.content);
        return false;
    }

    MIMI_LOGI(TAG, "triggered agent heartbeat check");
    return true;
}

static void heartbeat_timer_callback(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;
    (void)heartbeat_send();
}

OPERATE_RET heartbeat_init(void)
{
    MIMI_LOGI(TAG, "heartbeat initialized (file=%s interval=%u s)", MIMI_HEARTBEAT_FILE,
              (unsigned)(MIMI_HEARTBEAT_INTERVAL_MS / 1000));
    return OPRT_OK;
}

OPERATE_RET heartbeat_start(void)
{
    if (s_heartbeat_timer) {
        MIMI_LOGW(TAG, "heartbeat already running");
        return OPRT_OK;
    }

    OPERATE_RET rt = tal_sw_timer_create(heartbeat_timer_callback, NULL, &s_heartbeat_timer);
    if (rt != OPRT_OK || !s_heartbeat_timer) {
        MIMI_LOGE(TAG, "create heartbeat timer failed: %d", rt);
        s_heartbeat_timer = NULL;
        return rt == OPRT_OK ? OPRT_COM_ERROR : rt;
    }

    rt = tal_sw_timer_start(s_heartbeat_timer, MIMI_HEARTBEAT_INTERVAL_MS, TAL_TIMER_CYCLE);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "start heartbeat timer failed: %d", rt);
        (void)tal_sw_timer_delete(s_heartbeat_timer);
        s_heartbeat_timer = NULL;
        return rt;
    }

    MIMI_LOGI(TAG, "heartbeat started (every %u min)", (unsigned)(MIMI_HEARTBEAT_INTERVAL_MS / 60000));
    return OPRT_OK;
}

void heartbeat_stop(void)
{
    if (s_heartbeat_timer) {
        (void)tal_sw_timer_stop(s_heartbeat_timer);
        (void)tal_sw_timer_delete(s_heartbeat_timer);
        s_heartbeat_timer = NULL;
        MIMI_LOGI(TAG, "heartbeat stopped");
    }
}

bool heartbeat_trigger(void)
{
    return heartbeat_send();
}
