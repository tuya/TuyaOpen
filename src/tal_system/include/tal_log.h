/**
 * @file tal_log.h
 * @brief Provides logging capabilities for Tuya IoT applications.
 *
 * This header file defines the logging interface for Tuya IoT applications,
 * including macros and functions for various levels of logging (error, warning,
 * info, debug, and trace). It supports conditional compilation of log levels,
 * custom log buffer sizes, and printf-style log messages. Additionally, it
 * provides mechanisms for hex dump logging, setting global log levels, and
 * managing output terminals for log messages.
 *
 * The logging functionality is designed to aid in the development and debugging
 * of Tuya IoT applications by providing comprehensive, flexible, and
 * configurable logging capabilities. It allows developers to control the
 * verbosity of log output, which can be directed to various output terminals,
 * such as serial ports or files, to suit the application's needs.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_LOG_H__
#define __TAL_LOG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/
/**
 * @brief Definition of log style
 */
typedef uint8_t TAL_LOG_DISPLAY_MODE_E;
#define TAL_LOG_DISPLAY_MODE_DEFAULT    (0)
#define TAL_LOG_DISPLAY_MODE_HIGH_LIGHT (1)
#define TAL_LOG_DISPLAY_MODE_UNDER_LINE (4)
#define TAL_LOG_DISPLAY_MODE_FLASH      (5)
#define TAL_LOG_DISPLAY_MODE_REVERSE    (7)

typedef uint8_t TAL_LOG_FONT_COLOR_E;
#define TAL_LOG_FONT_COLOR_BLACK   (30)
#define TAL_LOG_FONT_COLOR_RED     (31)
#define TAL_LOG_FONT_COLOR_GREEN   (32)
#define TAL_LOG_FONT_COLOR_YELLOW  (33)
#define TAL_LOG_FONT_COLOR_BLUE    (34)
#define TAL_LOG_FONT_COLOR_PURPLE  (35)
#define TAL_LOG_FONT_COLOR_CYAN    (36)
#define TAL_LOG_FONT_COLOR_WHITE   (37)
#define TAL_LOG_FONT_COLOR_DEFAULT (39)

typedef uint8_t TAL_LOG_BACKGROUND_COLOR_E;
#define TAL_LOG_BACKGROUND_COLOR_BLACK   (40)
#define TAL_LOG_BACKGROUND_COLOR_RED     (41)
#define TAL_LOG_BACKGROUND_COLOR_GREEN   (42)
#define TAL_LOG_BACKGROUND_COLOR_YELLOW  (43)
#define TAL_LOG_BACKGROUND_COLOR_BLUE    (44)
#define TAL_LOG_BACKGROUND_COLOR_PURPLE  (45)
#define TAL_LOG_BACKGROUND_COLOR_CYAN    (46)
#define TAL_LOG_BACKGROUND_COLOR_WHITE   (47)
#define TAL_LOG_BACKGROUND_COLOR_DEFAULT (49)

/**
 * @brief Definition of log level
 */
typedef enum {
    TAL_LOG_LEVEL_ERR,
    TAL_LOG_LEVEL_WARN,
    TAL_LOG_LEVEL_NOTICE,
    TAL_LOG_LEVEL_INFO,
    TAL_LOG_LEVEL_DEBUG,
    TAL_LOG_LEVEL_TRACE,
} TAL_LOG_LEVEL_E;

typedef TAL_LOG_LEVEL_E LOG_LEVEL;

#if defined(MAX_SIZE_OF_DEBUG_BUF)
#define DEF_LOG_BUF_LEN MAX_SIZE_OF_DEBUG_BUF
#else
#define DEF_LOG_BUF_LEN 4096
#endif

#ifdef ENABLE_PRINTF_CHECK
#define PRINTF_CHECK(formatArg, firstVarArg) __attribute__((format(printf, formatArg, firstVarArg)))
#else
#define PRINTF_CHECK(...)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define LOG_FMT_IS_CONST(fmt) __builtin_constant_p(fmt)
#else
#define LOG_FMT_IS_CONST(fmt) (FALSE)
#endif

PRINTF_CHECK(4, 5)
OPERATE_RET tal_log_print(const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *fmt, ...);
PRINTF_CHECK(5, 6)
OPERATE_RET tal_log_print_secure(BOOL_T is_const_fmt, const TAL_LOG_LEVEL_E level, const char *file, const int line,
                                 const char *fmt, ...);

// file name maybe define from complie parameter
#ifndef _THIS_FILE_NAME_
#define _THIS_FILE_NAME_ __FILE__
#endif

#define PR_ERR(fmt, ...)                                                                                                 \
    tal_log_print_secure(LOG_FMT_IS_CONST(fmt), TAL_LOG_LEVEL_ERR, _THIS_FILE_NAME_, __LINE__, fmt, ##__VA_ARGS__)
#define PR_WARN(fmt, ...)                                                                                                \
    tal_log_print_secure(LOG_FMT_IS_CONST(fmt), TAL_LOG_LEVEL_WARN, _THIS_FILE_NAME_, __LINE__, fmt, ##__VA_ARGS__)
#define PR_NOTICE(fmt, ...)                                                                                              \
    tal_log_print_secure(LOG_FMT_IS_CONST(fmt), TAL_LOG_LEVEL_NOTICE, _THIS_FILE_NAME_, __LINE__, fmt, ##__VA_ARGS__)
#define PR_INFO(fmt, ...)                                                                                                \
    tal_log_print_secure(LOG_FMT_IS_CONST(fmt), TAL_LOG_LEVEL_INFO, _THIS_FILE_NAME_, __LINE__, fmt, ##__VA_ARGS__)
#define PR_DEBUG(fmt, ...)                                                                                               \
    tal_log_print_secure(LOG_FMT_IS_CONST(fmt), TAL_LOG_LEVEL_DEBUG, _THIS_FILE_NAME_, __LINE__, fmt, ##__VA_ARGS__)
#define PR_TRACE(fmt, ...)                                                                                               \
    tal_log_print_secure(LOG_FMT_IS_CONST(fmt), TAL_LOG_LEVEL_TRACE, _THIS_FILE_NAME_, __LINE__, fmt, ##__VA_ARGS__)

#define PR_HEXDUMP_ERR(title, buf, size)                                                                               \
    tal_log_hex_dump(TAL_LOG_LEVEL_ERR, _THIS_FILE_NAME_, __LINE__, title, 8, buf, size)
#define PR_HEXDUMP_WARN(title, buf, size)                                                                              \
    tal_log_hex_dump(TAL_LOG_LEVEL_WARN, _THIS_FILE_NAME_, __LINE__, title, 8, buf, size)
#define PR_HEXDUMP_NOTICE(title, buf, size)                                                                            \
    tal_log_hex_dump(TAL_LOG_LEVEL_NOTICE, _THIS_FILE_NAME_, __LINE__, title, 8, buf, size)
#define PR_HEXDUMP_INFO(title, buf, size)                                                                              \
    tal_log_hex_dump(TAL_LOG_LEVEL_INFO, _THIS_FILE_NAME_, __LINE__, title, 8, buf, size)
#define PR_HEXDUMP_DEBUG(title, buf, size)                                                                             \
    tal_log_hex_dump(TAL_LOG_LEVEL_DEBUG, _THIS_FILE_NAME_, __LINE__, title, 8, buf, size)
#define PR_HEXDUMP_TRACE(title, buf, size)                                                                             \
    tal_log_hex_dump(TAL_LOG_LEVEL_TRACE, _THIS_FILE_NAME_, __LINE__, title, 8, buf, size)
#define PR_HEX_DUMP(title, width, buf, size)                                                                           \
    tal_log_hex_dump(TAL_LOG_LEVEL_NOTICE, __FILE__, __LINE__, title, width, buf, size)

#define PR_DEBUG_RAW(fmt, ...) tal_log_print_raw(fmt, ##__VA_ARGS__)
#define PR_TRACE_ENTER()       PR_TRACE("enter [%s]", (const char *)__func__)
#define PR_TRACE_LEAVE()       PR_TRACE(("leave [%s]", (const char *)__func__))

/***********************************************************************
 ********************* struct ******************************************
 **********************************************************************/
// prototype of log output function
typedef void (*TAL_LOG_OUTPUT_CB)(const char *str);

/***********************************************************************
 ********************* variable ****************************************
 **********************************************************************/

/***********************************************************************
 ********************* function ****************************************
 **********************************************************************/

/**
 * @brief initialize log management.
 *
 * @param[in] level , set log level
 * @param[in] buf_len , set log buffer size
 * @param[in] output , log print function pointer
 *
 * @note This API is used for initializing log management.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_init(const TAL_LOG_LEVEL_E level, const int buf_len, const TAL_LOG_OUTPUT_CB output);

/**
 * @brief add one output terminal.
 *
 * @param[in] name , terminal name
 * @param[in] term , output function pointer
 *
 * @note This API is used for adding one output terminal.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_add_output_term(const char *name, const TAL_LOG_OUTPUT_CB term);

/**
 * @brief delete one output terminal.
 *
 * @param[in] name , terminal name
 *
 * @note This API is used for delete one output terminal.
 *
 * @return NONE
 */
void tal_log_del_output_term(const char *name);

/**
 * @brief set global log level.
 *
 * @param[in] curLogLevel , log level
 *
 * @note This API is used for setting global log level.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_set_level(const TAL_LOG_LEVEL_E level);

/**
 * @brief set log time whether show in millisecond.
 *
 * @param[in] if_ms_level, whether log time include millisecond
 *
 * @note This API is used for setting log time whether include milisecond.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_set_ms_info(BOOL_T if_ms_level);

/**
 * @brief get global log level.
 *
 * @param[in] pCurLogLevel, global log level
 *
 * @note This API is used for getting global log level.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_get_level(TAL_LOG_LEVEL_E *level);

/**
 * @brief add one module's log level
 *
 * @param[in] module_name, module name
 * @param[in] logLevel, this module's log level
 *
 * @note This API is used for adding one module's log level.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_add_module_level(const char *module_name, const TAL_LOG_LEVEL_E level);

/**
 * @brief This API is used for adding one module's log level.
 *
 * @param[in] module_name: module_name
 * @param[in] level: level
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_set_module_level(const char *module_name, TAL_LOG_LEVEL_E level);
/**
 * @brief get one module's log level
 *
 * @param[in] pModuleName, module name
 * @param[in] logLevel, this module's log level
 *
 * @note This API is used for getting one module's log level.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_get_module_level(const char *module_name, TAL_LOG_LEVEL_E *level);

/**
 * @brief delete one module's log level
 *
 * @param[in] pModuleName, module name
 *
 * @note This API is used for deleting one module's log level.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_delete_module_level(const char *module_name);

PRINTF_CHECK(1, 2)

/**
 * @brief This API is used for print only user log info.
 *
 * @param[in] pFmt: format string
 * @param[in] ...: parameter
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_log_print_raw(const char *pFmt, ...);

/**
 * @brief Print the user-provided string, internally escaping '%' to '%%' to avoid format parsing.
 *
 * @param[in] level log level
 * @param[in] file file name
 * @param[in] line line number
 * @param[in] prefix fixed prefix, can be NULL or empty string
 * @param[in] user_str user string to be output
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_log_print_escape(const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *prefix,
                                 const char *user_str);

/**
 * @brief destroy log management
 *
 * @param[in] pFmt, format string
 * @param[in] ..., parameter
 *
 * @note This API is used for destroy log management.
 *
 * @return NONE
 */
void tal_log_release(void);

/**
 * @brief print a buffer in hex format
 *
 * @param[in] title, buffer title for print
 * @param[in] width, one line width
 * @param[in] buf, buffer address
 * @param[in] size, buffer size
 *
 * @note This API is used for print one buffer.
 *
 * @return NONE
 */
void tal_log_hex_dump(const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *title, uint8_t width,
                      uint8_t *buf, uint16_t size);

/**
 * @brief Sets the enable status of log color.
 *
 * This function sets the enable status of log color. If the enable parameter is set to TRUE,
 * log color will be enabled. If the enable parameter is set to FALSE, log color will be disabled.
 *
 * @param enable The enable status of log color. Set to TRUE to enable log color, FALSE to disable log color.
 *
 * @return NONE
 */
void tal_log_color_enable_set(BOOL_T enable);

/**
 * @brief Sets the color configuration for a specific log level.
 *
 * This function sets the color configuration for a specific log level, including the display mode, font color, and
 * background color.
 *
 * @param level The log level to set the color configuration for.
 * @param display_mode The display mode to set for the log level.
 * @param font_color The font color to set for the log level.
 * @param background_color The background color to set for the log level.
 *
 * @return NONE
 */
void tal_log_color_set(const TAL_LOG_LEVEL_E level, TAL_LOG_DISPLAY_MODE_E display_mode,
                       TAL_LOG_FONT_COLOR_E font_color, TAL_LOG_BACKGROUND_COLOR_E background_color);

/**
 * @brief Variant of tal_log_print_raw that accepts a va_list.
 *
 * @param pFmt Format string
 * @param ap   Initialized va_list (will not be modified except read)
 * @return OPRT_OK on success or error code
 */
 OPERATE_RET tal_log_vprint_raw(const char *pFmt, va_list ap);

/**
 * @brief Prints a colored log message with the specified display mode, font color, and background color.
 *
 * This function prints a log message with the specified display mode, font color, and background color.
 * The log message is formatted using a format string and optional arguments, similar to the printf function.
 *
 * @param display_mode The display mode of the log message.
 * @param font_color The font color of the log message.
 * @param background_color The background color of the log message.
 * @param pFmt The format string for the log message.
 * @param ... Optional arguments for the format string.
 *
 * @return The result of the operation. Returns OPRT_INVALID_PARM if pLogManage is NULL,
 *         OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED if the format string failed to be formatted,
 *         or the number of characters written to the log buffer otherwise.
 */
OPERATE_RET tal_log_color_print_raw(TAL_LOG_DISPLAY_MODE_E display_mode, TAL_LOG_FONT_COLOR_E font_color,
                                    TAL_LOG_BACKGROUND_COLOR_E background_color, const char *pFmt, ...);

#ifdef __cplusplus
}
#endif /* __TAL_LOG_H__ */

#endif
