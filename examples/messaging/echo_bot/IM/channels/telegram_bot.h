#ifndef __TELEGRAM_BOT_H__
#define __TELEGRAM_BOT_H__

#include "im_platform.h"

OPERATE_RET telegram_bot_init(void);
OPERATE_RET telegram_bot_start(void);
OPERATE_RET telegram_send_message(const char *chat_id, const char *text);
OPERATE_RET telegram_set_token(const char *token);

#endif /* __TELEGRAM_BOT_H__ */
