#ifndef __IM_CONFIG_H__
#define __IM_CONFIG_H__

/*
 * IM component compile-time defaults.
 * Override any value by defining it in im_secrets.h (not tracked by VCS).
 */
#if __has_include("im_secrets.h")
#include "im_secrets.h"
#endif

/* ---- Secrets (override in im_secrets.h) ---- */

#ifndef IM_SECRET_TG_TOKEN
#define IM_SECRET_TG_TOKEN         ""
#endif
#ifndef IM_SECRET_DC_TOKEN
#define IM_SECRET_DC_TOKEN         ""
#endif
#ifndef IM_SECRET_DC_CHANNEL_ID
#define IM_SECRET_DC_CHANNEL_ID    ""
#endif
#ifndef IM_SECRET_FS_APP_ID
#define IM_SECRET_FS_APP_ID        ""
#endif
#ifndef IM_SECRET_FS_APP_SECRET
#define IM_SECRET_FS_APP_SECRET    ""
#endif
#ifndef IM_SECRET_FS_ALLOW_FROM
#define IM_SECRET_FS_ALLOW_FROM    ""
#endif
#ifndef IM_SECRET_CHANNEL_MODE
#define IM_SECRET_CHANNEL_MODE     "telegram"
#endif
#ifndef IM_SECRET_PROXY_HOST
#define IM_SECRET_PROXY_HOST       ""
#endif
#ifndef IM_SECRET_PROXY_PORT
#define IM_SECRET_PROXY_PORT       ""
#endif
#ifndef IM_SECRET_PROXY_TYPE
#define IM_SECRET_PROXY_TYPE       "http"
#endif

/* ---- Telegram ---- */

#ifndef IM_TG_API_HOST
#define IM_TG_API_HOST             "api.telegram.org"
#endif
#define IM_TG_POLL_TIMEOUT_S       30
#define IM_TG_MAX_MSG_LEN          4096
#define IM_TG_POLL_STACK           (12 * 1024)
#define IM_TG_POLL_PRIO            5
#define IM_TG_FAIL_BASE_MS         2000
#define IM_TG_FAIL_MAX_MS          60000

/* ---- Discord ---- */

#ifndef IM_DC_API_HOST
#define IM_DC_API_HOST             "discord.com"
#endif
#define IM_DC_API_BASE             "/api/v10"
#define IM_DC_MAX_MSG_LEN          2000
#define IM_DC_POLL_STACK           (12 * 1024)
#define IM_DC_FAIL_BASE_MS         2000
#define IM_DC_FAIL_MAX_MS          60000
#define IM_DC_LAST_MSG_SAVE_INTERVAL_MS  (5 * 1000)
#ifndef IM_DC_GATEWAY_HOST
#define IM_DC_GATEWAY_HOST         "gateway.discord.gg"
#endif
#ifndef IM_DC_GATEWAY_PATH
#define IM_DC_GATEWAY_PATH         "/?v=10&encoding=json"
#endif
#define IM_DC_GATEWAY_INTENTS      37377
#define IM_DC_GATEWAY_RX_BUF_SIZE  (64 * 1024)
#define IM_DC_GATEWAY_RECONNECT_MS 5000

/* ---- Feishu ---- */

#ifndef IM_FS_API_HOST
#define IM_FS_API_HOST             "open.feishu.cn"
#endif
#ifndef IM_FS_POLL_STACK
#define IM_FS_POLL_STACK           (16 * 1024)
#endif

/* ---- Message bus / outbound ---- */

#define IM_BUS_QUEUE_LEN           16
#define IM_OUTBOUND_STACK          (12 * 1024)
#define IM_OUTBOUND_PRIO           5

/* ---- NVS namespaces & keys ---- */

#define IM_NVS_TG                  "tg_config"
#define IM_NVS_DC                  "dc_config"
#define IM_NVS_FS                  "fs_config"
#define IM_NVS_BOT                 "bot_config"
#define IM_NVS_PROXY               "proxy_config"

#define IM_NVS_KEY_TG_TOKEN        "bot_token"
#define IM_NVS_KEY_DC_TOKEN        "bot_token"
#define IM_NVS_KEY_DC_CHANNEL_ID   "channel_id"
#define IM_NVS_KEY_DC_LAST_MSG_ID  "last_msg_id"
#define IM_NVS_KEY_FS_APP_ID       "app_id"
#define IM_NVS_KEY_FS_APP_SECRET   "app_secret"
#define IM_NVS_KEY_FS_ALLOW_FROM   "allow_from"
#define IM_NVS_KEY_CHANNEL_MODE    "channel_mode"
#define IM_NVS_KEY_PROXY_HOST      "host"
#define IM_NVS_KEY_PROXY_PORT      "port"
#define IM_NVS_KEY_PROXY_TYPE      "proxy_type"

#endif /* __IM_CONFIG_H__ */
