

#ifndef __TUYA_LOG_H__
#define __TUYA_LOG_H__

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "tuya_media_service_rtc.h"

#define tuya_p2p_log_trace(...) tuya_p2p_log_log(TUYA_P2P_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define tuya_p2p_log_debug(...) tuya_p2p_log_log(TUYA_P2P_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define tuya_p2p_log_info(...)  tuya_p2p_log_log(TUYA_P2P_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define tuya_p2p_log_warn(...)  tuya_p2p_log_log(TUYA_P2P_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define tuya_p2p_log_error(...) tuya_p2p_log_log(TUYA_P2P_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define tuya_p2p_log_fatal(...) tuya_p2p_log_log(TUYA_P2P_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

void tuya_p2p_log_log(int level, const char *file, int line, const char *fmt, ...);

#endif
