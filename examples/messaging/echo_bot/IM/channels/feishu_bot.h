#ifndef __FEISHU_BOT_H__
#define __FEISHU_BOT_H__

#include "im_platform.h"

OPERATE_RET feishu_bot_init(void);
OPERATE_RET feishu_bot_start(void);
OPERATE_RET feishu_send_message(const char *chat_id, const char *text);
OPERATE_RET feishu_set_app_id(const char *app_id);
OPERATE_RET feishu_set_app_secret(const char *app_secret);
OPERATE_RET feishu_set_allow_from(const char *allow_from_csv);

#endif /* __FEISHU_BOT_H__ */
