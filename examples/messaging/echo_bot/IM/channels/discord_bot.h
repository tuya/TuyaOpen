#ifndef __DISCORD_BOT_H__
#define __DISCORD_BOT_H__

#include "im_platform.h"

OPERATE_RET discord_bot_init(void);
OPERATE_RET discord_bot_start(void);
OPERATE_RET discord_send_message(const char *channel_id, const char *text);
OPERATE_RET discord_set_token(const char *token);
OPERATE_RET discord_set_channel_id(const char *channel_id);

#endif /* __DISCORD_BOT_H__ */
