#pragma once

#include "mimi_base.h"

OPERATE_RET memory_store_init(void);
OPERATE_RET memory_read_long_term(char *buf, size_t size);
OPERATE_RET memory_write_long_term(const char *content);
OPERATE_RET memory_append_today(const char *note);
OPERATE_RET memory_read_recent(char *buf, size_t size, int days);
