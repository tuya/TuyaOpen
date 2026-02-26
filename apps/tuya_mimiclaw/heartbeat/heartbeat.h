#pragma once

#include "mimi_base.h"

#include <stdbool.h>

OPERATE_RET heartbeat_init(void);
OPERATE_RET heartbeat_start(void);
void        heartbeat_stop(void);
bool        heartbeat_trigger(void);
