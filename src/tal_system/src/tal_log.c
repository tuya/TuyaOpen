/**
 * @file tal_log.c
 * @brief Implements logging functionalities for Tuya IoT applications.
 *
 * This source file provides the implementation of the logging system for Tuya
 * IoT applications, facilitating the output of log messages with varying levels
 * of severity. It supports dynamic log level adjustment, multiple output
 * destinations (such as terminal or file), and custom log message formatting.
 * The system is designed to aid in debugging and monitoring the application's
 * behavior in development and production environments.
 *
 * Key features include:
 * - Configurable log levels ranging from debug to critical errors.
 * - Support for multiple log output destinations through callback registration.
 * - Thread-safe log message output using mutexes.
 * - Integration with Tuya's IoT SDK for memory management and system utilities.
 *
 * The logging system is implemented using a linked list to manage output
 * destinations and utilizes Tuya's abstracted system functionalities, such as
 * mutexes for thread safety and memory management for dynamic allocation of log
 * nodes.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include "tal_log.h"
#include "tuya_list.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "tal_time_service.h"
#include "tal_memory.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define LOG_LEVEL_MIN 0
#define LOG_LEVEL_MAX 5

typedef struct {
    LIST_HEAD node;
    char *name;
    TAL_LOG_OUTPUT_CB out_term;
} LOG_OUT_NODE_S;

typedef struct {
    TAL_LOG_DISPLAY_MODE_E display_mode;
    TAL_LOG_FONT_COLOR_E font_color;
    TAL_LOG_BACKGROUND_COLOR_E background_color;
} LOG_TEXT_STYLE_S;

typedef struct {
    BOOL_T enable_color;
    LOG_TEXT_STYLE_S style[LOG_LEVEL_MAX + 1];
} LOG_COLOR_S;

typedef struct {
    LOG_LEVEL curLogLevel;
    LIST_HEAD listHead;
    LIST_HEAD log_list;
    MUTEX_HANDLE mutex;

    LOG_COLOR_S log_color;

    int log_buf_len;
    BOOL_T ms_level;
    char *log_buf;
} LOG_MANAGE, *P_LOG_MANAGE;

#define DEF_OUTPUT_NAME "def_output"

/***********************************************************
*************************variable define********************
***********************************************************/
const char *sLevelStr[] = {"E", "W", "N", "I", "D", "T"};
P_LOG_MANAGE pLogManage = NULL;

const LOG_TEXT_STYLE_S sDefaultStyle[LOG_LEVEL_MAX + 1] = {
    {TAL_LOG_DISPLAY_MODE_DEFAULT, TAL_LOG_FONT_COLOR_RED, TAL_LOG_BACKGROUND_COLOR_DEFAULT},
    {TAL_LOG_DISPLAY_MODE_DEFAULT, TAL_LOG_FONT_COLOR_YELLOW, TAL_LOG_BACKGROUND_COLOR_DEFAULT},
    {TAL_LOG_DISPLAY_MODE_DEFAULT, TAL_LOG_FONT_COLOR_BLUE, TAL_LOG_BACKGROUND_COLOR_DEFAULT},
    {TAL_LOG_DISPLAY_MODE_DEFAULT, TAL_LOG_FONT_COLOR_CYAN, TAL_LOG_BACKGROUND_COLOR_DEFAULT},
    {TAL_LOG_DISPLAY_MODE_DEFAULT, TAL_LOG_FONT_COLOR_GREEN, TAL_LOG_BACKGROUND_COLOR_DEFAULT},
    {TAL_LOG_DISPLAY_MODE_DEFAULT, TAL_LOG_FONT_COLOR_WHITE, TAL_LOG_BACKGROUND_COLOR_DEFAULT}};

/***********************************************************
*************************function define********************
***********************************************************/
/**
 * @brief Initializes the TAL log system.
 *
 * This function initializes the TAL log system with the specified log level,
 * buffer length, and output callback function.
 *
 * @param level The log level to set. Must be between LOG_LEVEL_MIN and
 * LOG_LEVEL_MAX.
 * @param buf_len The length of the log buffer.
 * @param output The output callback function to be called when a log message is
 * generated.
 * @return OPERATE_RET Returns OPRT_OK if the initialization is successful.
 * Returns OPRT_INVALID_PARM if any of the parameters are invalid. Returns
 * OPRT_MALLOC_FAILED if memory allocation fails. Returns an error code if
 * adding the output terminal fails.
 */
OPERATE_RET tal_log_init(const TAL_LOG_LEVEL_E level, const int buf_len, const TAL_LOG_OUTPUT_CB output)
{
    if (level < LOG_LEVEL_MIN || level > LOG_LEVEL_MAX || 0 == buf_len || NULL == output) {
        return OPRT_INVALID_PARM;
    }

    if (!pLogManage) {
        OPERATE_RET op_ret = OPRT_OK;

        P_LOG_MANAGE tmp_log_mng = (P_LOG_MANAGE)tal_malloc(sizeof(LOG_MANAGE) + buf_len + 1);
        if (!tmp_log_mng) {
            return OPRT_MALLOC_FAILED;
        }
        tmp_log_mng->log_buf_len = buf_len;
        tmp_log_mng->log_buf = (char *)(tmp_log_mng + 1);
        op_ret = tal_mutex_create_init(&tmp_log_mng->mutex);
        if (OPRT_OK != op_ret) {
            tal_free(tmp_log_mng);
            return op_ret;
        }
        INIT_LIST_HEAD(&(tmp_log_mng->listHead));
        INIT_LIST_HEAD(&(tmp_log_mng->log_list));
        tmp_log_mng->curLogLevel = level;
        tmp_log_mng->ms_level = FALSE;
        pLogManage = tmp_log_mng;

        // set default log style
        tmp_log_mng->log_color.enable_color = TRUE;
        memcpy(tmp_log_mng->log_color.style, sDefaultStyle, (LOG_LEVEL_MAX + 1) * sizeof(LOG_TEXT_STYLE_S));

        op_ret = tal_log_add_output_term(DEF_OUTPUT_NAME, output);
        if (OPRT_OK != op_ret) {
            tal_mutex_release(tmp_log_mng->mutex);
            tal_free(tmp_log_mng);
            return op_ret;
        }
    } else {
        pLogManage->curLogLevel = level;
    }

    return OPRT_OK;
}

void __output_logManage_buf(void)
{
    P_LIST_HEAD pPos;
    LOG_OUT_NODE_S *output_node;
    tuya_list_for_each(pPos, &(pLogManage->log_list))
    {
        output_node = tuya_list_entry(pPos, LOG_OUT_NODE_S, node);
        if (output_node->out_term) {
            output_node->out_term(pLogManage->log_buf);
        }
    }
}

OPERATE_RET __find_out_term_node(const char *name, LOG_OUT_NODE_S **node)
{
    P_LIST_HEAD pPos;
    LOG_OUT_NODE_S *output_node;

    tuya_list_for_each(pPos, &(pLogManage->log_list))
    {
        output_node = tuya_list_entry(pPos, LOG_OUT_NODE_S, node);
        if (!strcmp(output_node->name, name)) {
            *node = output_node;
            return OPRT_OK;
        }
    }

    return OPRT_COM_ERROR;
}

static BOOL_T __log_fmt_has_forbidden_spec(const char *fmt)
{
    if (!fmt) {
        return FALSE;
    }

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == '%') {
                fmt++;
                continue;
            }

            while (*fmt && (*fmt == ' ' || *fmt == '#' || *fmt == '+' || *fmt == '-' || *fmt == '0' || *fmt == '\'' ||
                            *fmt == 'I')) {
                fmt++;
            }
            while (*fmt && isdigit((unsigned char)(*fmt))) {
                fmt++;
            }
            if (*fmt == '.') {
                fmt++;
                while (*fmt && isdigit((unsigned char)(*fmt))) {
                    fmt++;
                }
            }
            while (*fmt && (*fmt == 'h' || *fmt == 'l' || *fmt == 'j' || *fmt == 'z' || *fmt == 't' || *fmt == 'L')) {
                fmt++;
            }
            if (*fmt == 'n') {
                return TRUE;
            }
        } else {
            fmt++;
        }
    }

    return FALSE;
}

static OPERATE_RET __log_escape_percent(const char *src, char *dst, size_t dst_len)
{
    if (!src || !dst || dst_len == 0) {
        return OPRT_INVALID_PARM;
    }

    size_t used = 0;
    while (*src && used + 1 < dst_len) {
        if (*src == '%') {
            if (used + 2 >= dst_len) {
                break;
            }
            dst[used++] = '%';
            dst[used++] = '%';
            src++;
            continue;
        }
        dst[used++] = *src++;
    }
    dst[used] = '\0';

    if (*src != '\0') {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    return OPRT_OK;
}

/**
 * @brief Adds an output terminal for logging.
 *
 * This function adds an output terminal for logging with the specified name and
 * callback function.
 *
 * @param[in] name The name of the output terminal.
 * @param[in] term The callback function for the output terminal.
 *
 * @return The result of the operation.
 *     - OPRT_OK: Operation successful.
 *     - OPRT_INVALID_PARM: Invalid parameter.
 *     - OPRT_MALLOC_FAILED: Memory allocation failed.
 */
OPERATE_RET tal_log_add_output_term(const char *name, const TAL_LOG_OUTPUT_CB term)
{
    if (NULL == name || NULL == term || NULL == pLogManage) {
        return OPRT_INVALID_PARM;
    }

    LOG_OUT_NODE_S *output_node;
    OPERATE_RET ret = __find_out_term_node(name, &output_node);
    if (ret == OPRT_OK) {
        output_node->out_term = term;
        return OPRT_OK;
    }

    NEW_LIST_NODE(LOG_OUT_NODE_S, output_node);
    if (!output_node) {
        return OPRT_MALLOC_FAILED;
    }
    size_t name_len = strlen(name);
    output_node->name = tal_malloc(name_len + 1);
    if (!output_node->name) {
        tal_free(output_node);
        return OPRT_MALLOC_FAILED;
    }
    memcpy(output_node->name, name, name_len);
    output_node->name[name_len] = '\0';
    output_node->out_term = term;
    tuya_list_add(&(output_node->node), &(pLogManage->log_list));

    return OPRT_OK;
}

static int tal_log_strrchr(char *str, char ch)
{
    char *ta;

    ta = strrchr(str, ch);
    if (ta) {
        return (int)(ta - str);
    }

    return -1;
}

/**
 * @brief Deletes a log output terminal with the specified name.
 *
 * This function deletes a log output terminal from the log management system
 * based on the provided name. The function first checks if the name and the
 * log management system pointer are not NULL. If either of them is NULL, the
 * function returns without performing any action. Otherwise, it locks the
 * log management system mutex, searches for the output terminal node with
 * the specified name, and if found, removes it from the list and frees the
 * associated memory. Finally, it unlocks the mutex and returns.
 *
 * @param name The name of the log output terminal to be deleted.
 */
void tal_log_del_output_term(const char *name)
{
    if (NULL == name || NULL == pLogManage) {
        return;
    }
    tal_mutex_lock(pLogManage->mutex);
    LOG_OUT_NODE_S *output_node;
    OPERATE_RET ret = __find_out_term_node(name, &output_node);
    if (ret == OPRT_OK) {
        tuya_list_del(&(output_node->node));
        tal_free(output_node->name);
        tal_free(output_node);
    }
    tal_mutex_unlock(pLogManage->mutex);
    return;
}

/**
 * @brief Sets the millisecond information for the log.
 *
 * This function sets the millisecond information for the log. If the
 * `if_ms_level` parameter is set to TRUE, the millisecond information will be
 * included in the log; otherwise, it will be excluded.
 *
 * @param if_ms_level A boolean value indicating whether to include millisecond
 * information in the log.
 * @return The result of the operation. Returns OPRT_OK on success, or
 * OPRT_INVALID_PARM if `pLogManage` is NULL.
 */
OPERATE_RET tal_log_set_ms_info(BOOL_T if_ms_level)
{
    if (!pLogManage) {
        return OPRT_INVALID_PARM;
    }
    pLogManage->ms_level = if_ms_level;

    return OPRT_OK;
}

/**
 * @brief Sets the log level for the TAL system.
 *
 * This function sets the log level for the TAL system. The log level determines
 * the verbosity of the log messages that will be outputted.
 *
 * @param level The log level to set.
 * @return The result of the operation.
 *     - OPRT_OK: The log level was set successfully.
 *     - OPRT_INVALID_PARM: The provided log level is invalid.
 */
OPERATE_RET tal_log_set_level(const TAL_LOG_LEVEL_E level)
{
    if (!pLogManage) {
        return OPRT_INVALID_PARM;
    }

    if (level < LOG_LEVEL_MIN || level > LOG_LEVEL_MAX) {
        return OPRT_INVALID_PARM;
    }

    pLogManage->curLogLevel = level;

    return OPRT_OK;
}

/**
 * @brief Get the current log level.
 *
 * This function retrieves the current log level from the log manager.
 *
 * @param[out] level Pointer to a TAL_LOG_LEVEL_E variable to store the log
 * level.
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if
 * pLogManage is NULL.
 */
OPERATE_RET tal_log_get_level(TAL_LOG_LEVEL_E *level)
{
    if (!pLogManage) {
        return OPRT_INVALID_PARM;
    }
    *level = pLogManage->curLogLevel;

    return OPRT_OK;
}

/**
 * @brief Prints a log message with the specified log level, file name, line
 * number, and format string.
 *
 * This function is used to print log messages with different log levels. It
 * takes the log level, file name, line number, format string, and a variable
 * argument list as parameters. The log level determines the severity of the log
 * message. The file name and line number indicate the location where the log
 * message is printed. The format string specifies the format of the log
 * message, and the variable argument list contains the values to be formatted
 * and printed.
 *
 * @param logLevel The log level of the message.
 * @param pFile The name of the source file where the log message is printed.
 * @param line The line number in the source file where the log message is
 * printed.
 * @param pFmt The format string for the log message.
 * @param ap The variable argument list for the format string.
 * @return The result of the log printing operation.
 *     - OPRT_OK if the log message was printed successfully.
 *     - OPRT_INVALID_PARM if the log level is invalid or the log manager is not
 * initialized.
 *     - OPRT_BASE_LOG_MNG_PRINT_LOG_LEVEL_HIGHER if the log level is higher
 * than the current log level.
 *     - OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED if there was an error formatting
 * the log message.
 */
OPERATE_RET PrintLogV(LOG_LEVEL logLevel, char *pFile, uint32_t line, const char *pFmt, va_list ap)
{
    int len = 0;
    int cnt = 0;

    if (!pLogManage) {
        return OPRT_INVALID_PARM;
    }
    if (logLevel < LOG_LEVEL_MIN || logLevel > LOG_LEVEL_MAX) {
        return OPRT_INVALID_PARM;
    }
    LOG_LEVEL tmpLogLevel = pLogManage->curLogLevel;
    if (logLevel > tmpLogLevel) {
        return OPRT_BASE_LOG_MNG_PRINT_LOG_LEVEL_HIGHER;
    }
    const char *pTmpModuleName = "ty";
    const char *pTmpFilename = NULL;

    if (NULL == pFile) {
        pTmpFilename = "Null";
    } else {
        int pos = 0;
        pTmpFilename = pFile;
        pos = tal_log_strrchr((char *)pFile, '/');
        if (pos < 0) {
            pos = tal_log_strrchr((char *)pFile, '\\');
            if (pos >= 0) {
                pTmpFilename = pFile + pos + 1;
            }
        } else {
            pTmpFilename = pFile + pos + 1;
        }
    }
    tal_mutex_lock(pLogManage->mutex);

    // color prefix
    if (pLogManage->log_color.enable_color) {
        cnt = snprintf(pLogManage->log_buf, pLogManage->log_buf_len, "\033[%d;%d;%dm",
                       pLogManage->log_color.style[logLevel].display_mode,
                       pLogManage->log_color.style[logLevel].font_color,
                       pLogManage->log_color.style[logLevel].background_color);
        if (cnt <= 0) {
            goto ERR_EXIT;
        }
        len += cnt;
    }

    POSIX_TM_S tm;
    memset(&tm, 0, sizeof(tm));

    if (pLogManage->ms_level == FALSE) {
        tal_time_get_local_time_custom(0, &tm);
        cnt = snprintf(pLogManage->log_buf + len, pLogManage->log_buf_len - len,
                       "[%02d-%02d %02d:%02d:%02d %s %s][%s:%" PRIu32 "] ", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                       tm.tm_min, tm.tm_sec, pTmpModuleName, sLevelStr[logLevel], pTmpFilename, line);
    } else {
        SYS_TICK_T time_ms = tal_time_get_posix_ms();
        TIME_T sec = (TIME_T)(time_ms / 1000);
        uint32_t ms = (uint32_t)(time_ms % 1000);
        tal_time_get_local_time_custom(sec, &tm);
        cnt = snprintf(pLogManage->log_buf + len, pLogManage->log_buf_len - len,
                       "[%02d-%02d %02d:%02d:%02d:%" PRIu32 " %s %s][%s:%" PRIu32 "] ", tm.tm_mon + 1, tm.tm_mday,
                       tm.tm_hour, tm.tm_min, tm.tm_sec, ms, pTmpModuleName, sLevelStr[logLevel], pTmpFilename, line);
    }
    if (cnt <= 0) {
        goto ERR_EXIT;
    }
    len += cnt;

    // Check if there's enough space left for the formatted message
    int remaining = pLogManage->log_buf_len - len;
    if (remaining <= 0) {
        goto ERR_EXIT;
    }

    cnt = vsnprintf(pLogManage->log_buf + len, remaining, pFmt, ap);
    if (cnt < 0) {
        goto ERR_EXIT;
    }
    // vsnprintf returns the number of characters that would have been written
    // (excluding null terminator). If the return value >= buffer size, the string
    // was truncated and (buffer_size - 1) characters were actually written.
    if (cnt >= remaining) {
        cnt = remaining - 1; // Actual characters written (excluding null terminator)
    }
    len += cnt;

    char *p_suffix = (pLogManage->log_color.enable_color) ? "\033[0m\r\n" : "\r\n";
    if (len > (int)(pLogManage->log_buf_len - strlen(p_suffix) - 1)) { // 1 -> "\0"
        len = pLogManage->log_buf_len - strlen(p_suffix) - 1;
    }
    cnt = snprintf(pLogManage->log_buf + len, pLogManage->log_buf_len - len, "%s", p_suffix);
    if (cnt <= 0) {
        goto ERR_EXIT;
    }
    len += cnt;
    pLogManage->log_buf[len] = '\0';

    __output_logManage_buf();
    tal_mutex_unlock(pLogManage->mutex);

    return OPRT_OK;

ERR_EXIT:
    tal_mutex_unlock(pLogManage->mutex);
    return OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED;
}

/**
 * @brief Prints a log message with the specified log level, file, line number,
 * and format string.
 *
 * This function is used to print log messages with different log levels. It
 * takes the log level, file name, line number, and a format string as input
 * parameters. It then formats the log message using the provided format string
 * and any additional arguments, and prints the formatted message to the log
 * output.
 *
 * @param level The log level of the message.
 * @param file The name of the source file where the log message is being
 * printed.
 * @param line The line number in the source file where the log message is being
 * printed.
 * @param fmt The format string for the log message.
 * @param ... Additional arguments to be formatted and included in the log
 * message.
 * @return The result of the log printing operation.
 */
OPERATE_RET tal_log_print(const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *fmt, ...)
{
    OPERATE_RET opRet = 0;
    if (NULL == fmt) {
        return OPRT_INVALID_PARM;
    }
    if (__log_fmt_has_forbidden_spec(fmt)) {
        return OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED;
    }
    va_list ap;
    va_start(ap, fmt);
    opRet = PrintLogV(level, (char *)file, line, fmt, ap);
    va_end(ap);

    return opRet;
}

OPERATE_RET tal_log_print_secure(BOOL_T is_const_fmt, const TAL_LOG_LEVEL_E level, const char *file, const int line,
                                 const char *fmt, ...)
{
    if (NULL == fmt) {
        return OPRT_INVALID_PARM;
    }

    if (is_const_fmt) {
        if (__log_fmt_has_forbidden_spec(fmt)) {
            return OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED;
        }
        va_list ap;
        va_start(ap, fmt);
        OPERATE_RET ret = PrintLogV(level, (char *)file, line, fmt, ap);
        va_end(ap);
        return ret;
    }

    return tal_log_print_escape(level, file, line, NULL, fmt);
}

static OPERATE_RET __PrintLogVRaw(const char *pFmt, va_list ap)
{
    int cnt = 0;
    cnt = vsnprintf(pLogManage->log_buf, pLogManage->log_buf_len, pFmt, ap);
    if (cnt <= 0) {
        return OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED;
    }
    __output_logManage_buf();

    return OPRT_OK;
}

/**
 * @brief Prints a log message with a variable number of arguments.
 *
 * This function prints a log message using the provided format string and
 * variable number of arguments.
 *
 * @param pFmt The format string for the log message.
 * @param ... The variable number of arguments to be formatted and printed.
 * @return The result of the log printing operation.
 *         - OPRT_INVALID_PARM if pLogManage is NULL.
 *         - The result of the __PrintLogVRaw function otherwise.
 */
OPERATE_RET tal_log_print_raw(const char *pFmt, ...)
{
    if (NULL == pLogManage) {
        return OPRT_INVALID_PARM;
    }
    if (NULL == pFmt) {
        return OPRT_INVALID_PARM;
    }
    if (__log_fmt_has_forbidden_spec(pFmt)) {
        return OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED;
    }

    OPERATE_RET opRet = 0;
    va_list ap;

    tal_mutex_lock(pLogManage->mutex);
    va_start(ap, pFmt);
    opRet = __PrintLogVRaw(pFmt, ap);
    va_end(ap);
    tal_mutex_unlock(pLogManage->mutex);

    return opRet;
}

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
                                 const char *user_str)
{
    if (NULL == user_str) {
        return OPRT_INVALID_PARM;
    }

    size_t src_len = strlen(user_str);
    size_t buf_len = src_len * 2 + 1;
    char *escaped = tal_malloc(buf_len);
    if (NULL == escaped) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET esc_ret = __log_escape_percent(user_str, escaped, buf_len);
    if (esc_ret != OPRT_OK && esc_ret != OPRT_BUFFER_NOT_ENOUGH) {
        tal_free(escaped);
        return esc_ret;
    }

    OPERATE_RET log_ret = OPRT_INVALID_PARM;
    if (prefix && prefix[0] != '\0') {
        log_ret = tal_log_print(level, file, line, "%s%s", prefix, escaped);
    } else {
        log_ret = tal_log_print(level, file, line, "%s", escaped);
    }

    tal_free(escaped);
    return log_ret;
}

/**
 * @brief Variant of tal_log_print_raw that accepts a va_list.
 *
 * @param pFmt Format string
 * @param ap   Initialized va_list (will not be modified except read)
 * @return OPRT_OK on success or error code
 */
OPERATE_RET tal_log_vprint_raw(const char *pFmt, va_list ap)
{
    if (NULL == pLogManage) {
        return OPRT_INVALID_PARM;
    }
    OPERATE_RET opRet = 0;
    tal_mutex_lock(pLogManage->mutex);
    opRet = __PrintLogVRaw(pFmt, ap);
    tal_mutex_unlock(pLogManage->mutex);
    return opRet;
}

/**
 * @brief Releases the memory allocated for the log management system.
 *
 * This function frees the memory allocated for the log management system and
 * all associated log nodes. It iterates through the log list and frees the
 * memory for each log node. After releasing the memory, it sets the pointer to
 * the log management system to NULL.
 */
void tal_log_release(void)
{
    if (!pLogManage) {
        return;
    }

    while (!tuya_list_empty(&(pLogManage->log_list))) {
        LOG_OUT_NODE_S *log_out_nd = NULL;
        log_out_nd = tuya_list_entry(&(pLogManage->log_list.next), LOG_OUT_NODE_S, node);
        tuya_list_del(&(log_out_nd->node));
        if (log_out_nd->name) {
            tal_free(log_out_nd->name);
        }
        tal_free(log_out_nd);
    }
    tal_free(pLogManage);
    pLogManage = NULL;
}

/**
 * @brief Logs a hexadecimal dump of a buffer.
 *
 * This function logs a hexadecimal dump of a buffer with the specified log
 * level, file, line, title, width, buffer, and size. The log level determines
 * the severity of the log message. The file and line parameters indicate the
 * source file and line number where the log message is generated. The title
 * parameter is a string that provides additional information about the log
 * message. The width parameter specifies the number of bytes to display per
 * line in the hexadecimal dump. The buf parameter is a pointer to the buffer to
 * be dumped. The size parameter specifies the size of the buffer in bytes.
 *
 * @param level The log level of the message.
 * @param file The source file where the log message is generated.
 * @param line The line number in the source file where the log message is
 * generated.
 * @param title Additional information about the log message.
 * @param width The number of bytes to display per line in the hexadecimal dump.
 * @param buf The buffer to be dumped.
 * @param size The size of the buffer in bytes.
 *
 * @return None.
 */
void tal_log_hex_dump(const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *title, uint8_t width,
                      uint8_t *buf, uint16_t size)
{
    uint16_t i = 0, j = 0;

    if (!pLogManage || level > pLogManage->curLogLevel) {
        return;
    }

    if (width >= 64) {
        width = 64;
    }
    tal_log_print(level, file, line, "%s %d <%p>", title, size, buf);

    for (i = 0; i < size; i += width) {
        tal_log_print_raw("%04lX | ", i);

        for (j = i; j < i + width; j++) {
            if (j < size) {
                tal_log_print_raw("%02X ", buf[j]);
            } else {
                tal_log_print_raw("   ");
            }
        }

        tal_log_print_raw("| ");

        for (j = i; j < i + width && j < size; j++) {
            if (isprint(buf[j])) {
                tal_log_print_raw("%c", buf[j]);
            } else {
                tal_log_print_raw(".");
            }
        }

        tal_log_print_raw("\r\n");
    }
    tal_log_print_raw("\r\n");
}

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
void tal_log_color_enable_set(BOOL_T enable)
{
    if (!pLogManage) {
        return;
    }
    pLogManage->log_color.enable_color = enable;
}

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
                       TAL_LOG_FONT_COLOR_E font_color, TAL_LOG_BACKGROUND_COLOR_E background_color)
{
    if (!pLogManage) {
        return;
    }
    if (level < LOG_LEVEL_MIN || level > LOG_LEVEL_MAX) {
        return;
    }
    pLogManage->log_color.style[level].display_mode = display_mode;
    pLogManage->log_color.style[level].font_color = font_color;
    pLogManage->log_color.style[level].background_color = background_color;
}

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
                                    TAL_LOG_BACKGROUND_COLOR_E background_color, const char *pFmt, ...)
{
    OPERATE_RET opRet = 0;
    va_list ap;
    int cnt = 0;
    int len = 0;

    if (NULL == pLogManage) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(pLogManage->mutex);
    va_start(ap, pFmt);
    if (pLogManage->log_color.enable_color) {
        cnt = snprintf(pLogManage->log_buf, pLogManage->log_buf_len, "\033[%d;%d;%dm", display_mode, font_color,
                       background_color);
        if (cnt <= 0) {
            opRet = OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED;
            goto __EXIT;
        }
        len += cnt;
    }

    cnt = vsnprintf(pLogManage->log_buf + len, pLogManage->log_buf_len - len, pFmt, ap);
    if (cnt <= 0) {
        opRet = OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED;
        goto __EXIT;
    }
    len += cnt;

    if (pLogManage->log_color.enable_color) {
        if (len > (int)(pLogManage->log_buf_len - 4 - 1)) { // 4 -> "\033[0m" 1 -> "\0"
            len = pLogManage->log_buf_len - 4 - 1;
        }
        cnt = snprintf(pLogManage->log_buf + len, pLogManage->log_buf_len - len, "\033[0m");
        if (cnt <= 0) {
            opRet = OPRT_BASE_LOG_MNG_FORMAT_STRING_FAILED;
            goto __EXIT;
        }
        len += cnt;
    }

    pLogManage->log_buf[len] = 0;

    __output_logManage_buf();

__EXIT:
    va_end(ap);
    tal_mutex_unlock(pLogManage->mutex);

    return opRet;
}
