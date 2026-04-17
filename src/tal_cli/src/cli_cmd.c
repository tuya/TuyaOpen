/**
 * @file cli_cmd.c
 * @brief CLI command implementations for sys, kv, and fs groups.
 * @version 0.1
 * @date 2026-04-08
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */

#include "netmgr.h"
#include "tal_api.h"
#include "tal_cli.h"
#include "tal_fs.h"
#include "tal_kv.h"
#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tuya_iot.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#include "tal_wifi.h"
#endif

#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

extern void netmgr_cmd(int argc, char *argv[]);
extern void tal_thread_dump_watermark(void);

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define CLI_LINE_SIZE             256
#define CLI_VALUE_SIZE            512
#define CLI_DEFAULT_TEXT_LIMIT    512
#define CLI_DEFAULT_HEX_LIMIT     512
#define CLI_FS_LS_MAX_DEPTH       3
#define CLI_FS_DEFAULT_PATH       "/"
#define CLI_WIFI_SCAN_MAX_AP_NUM  20

/* ---------------------------------------------------------------------------
 * Forward declarations
 * --------------------------------------------------------------------------- */
#if defined(CLI_CMD_SYS)
static void cmd_sys_status(int argc, char *argv[]);
static void cmd_sys_heap(int argc, char *argv[]);
static void cmd_sys_thread(int argc, char *argv[]);
static void cmd_sys_version(int argc, char *argv[]);
static void cmd_sys_tick(int argc, char *argv[]);
static void cmd_sys_set_log_level(int argc, char *argv[]);
static void cmd_sys_reboot(int argc, char *argv[]);
static void cmd_sys_iot_stop(int argc, char *argv[]);
static void cmd_sys_iot_restart(int argc, char *argv[]);
static void cmd_sys_iot_reset(int argc, char *argv[]);
static void cmd_sys_iot_get_devid(int argc, char *argv[]);
static void cmd_sys_netmgr(int argc, char *argv[]);
#if defined(PLATFORM_LINUX) && (PLATFORM_LINUX == 1)
static void cmd_sys_exec(int argc, char *argv[]);
#endif
static void cmd_sys_iot_report_dp(int argc, char *argv[]);
static void cmd_sys_timer_count(int argc, char *argv[]);
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
static void cmd_sys_wifi_info(int argc, char *argv[]);
static void cmd_sys_wifi_scan(int argc, char *argv[]);
#endif
#endif /* CLI_CMD_SYS */

#if defined(CLI_CMD_FS)
static void cmd_fs_ls(int argc, char *argv[]);
static void cmd_fs_stat(int argc, char *argv[]);
static void cmd_fs_cat(int argc, char *argv[]);
static void cmd_fs_hexdump(int argc, char *argv[]);
static void cmd_fs_write(int argc, char *argv[]);
static void cmd_fs_append(int argc, char *argv[]);
static void cmd_fs_rm(int argc, char *argv[]);
static void cmd_fs_mkdir(int argc, char *argv[]);
static void cmd_fs_mv(int argc, char *argv[]);
#endif /* CLI_CMD_FS */

#if defined(CLI_CMD_KV)
static void cmd_kv_get(int argc, char *argv[]);
static void cmd_kv_set(int argc, char *argv[]);
static void cmd_kv_del(int argc, char *argv[]);
static void cmd_kv_list(int argc, char *argv[]);
#endif /* CLI_CMD_KV */

#if defined(ENABLE_SERIAL_CLI_CMD) && (ENABLE_SERIAL_CLI_CMD == 1)
static OPERATE_RET cli_fs_list_dir_recursive_(const char *path, int depth, int max_depth, uint32_t *count);
static void cli_fs_build_tree_prefix_(int depth, char *out, size_t out_size);
#endif /* ENABLE_SERIAL_CLI_CMD && (ENABLE_SERIAL_CLI_CMD == 1) */

#if defined(ENABLE_SERIAL_CLI_CMD) && (ENABLE_SERIAL_CLI_CMD == 1)
/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */
/**
 * @brief Echo a formatted line to CLI.
 * @param[in] fmt printf-style format string
 * @param[in] ... format arguments
 * @return none
 */
static void cli_echof_(const char *fmt, ...)
{
    char    line[CLI_LINE_SIZE] = {0};
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    tal_cli_echo(line);
}

/**
 * @brief Print command usage and up to three examples.
 * @param[in] usage usage string
 * @param[in] example1 first example, optional
 * @param[in] example2 second example, optional
 * @param[in] example3 third example, optional
 * @return none
 */
static void cli_print_usage_(const char *usage, const char *example1, const char *example2, const char *example3)
{
    if (usage != NULL) {
        cli_echof_("Usage: %s", usage);
    }
    if (example1 != NULL) {
        cli_echof_("  e.g. %s", example1);
    }
    if (example2 != NULL) {
        cli_echof_("  e.g. %s", example2);
    }
    if (example3 != NULL) {
        cli_echof_("  e.g. %s", example3);
    }
}

/**
 * @brief Join CLI arguments into one space-separated string.
 * @param[in] argc argument count
 * @param[in] argv argument array
 * @param[in] start first index to join
 * @param[out] out output buffer
 * @param[in] out_size output buffer size
 * @return true if at least one argument was joined, false otherwise
 */
static bool cli_join_args_(int argc, char *argv[], int start, char *out, size_t out_size)
{
    size_t offset = 0;

    if (out == NULL || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    if (argc <= start) {
        return false;
    }

    for (int i = start; i < argc && offset + 1 < out_size; i++) {
        int written = snprintf(out + offset, out_size - offset, "%s%s", (i == start) ? "" : " ", argv[i]);
        if (written < 0) {
            return false;
        }
        if ((size_t)written >= out_size - offset) {
            offset = out_size - 1;
            break;
        }
        offset += (size_t)written;
    }

    return true;
}

/**
 * @brief Check whether a string contains only digit characters.
 * @param[in] str input string
 * @return true if all characters are '0'-'9', false otherwise
 */
static bool cli_is_numeric_(const char *str)
{
    if (str == NULL || str[0] == '\0') {
        return false;
    }

    for (const char *p = str; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }

    return true;
}

/**
 * @brief Escape a string for safe JSON embedding.
 *        Escapes backslash and double-quote characters.
 * @param[in]  src      input string
 * @param[out] out      output buffer
 * @param[in]  out_size output buffer size
 * @return none
 */
static void cli_json_escape_(const char *src, char *out, size_t out_size)
{
    size_t pos = 0;

    if (src == NULL || out == NULL || out_size == 0) {
        return;
    }

    for (const char *p = src; *p != '\0' && pos + 1 < out_size; p++) {
        if (*p == '"' || *p == '\\') {
            if (pos + 2 >= out_size) {
                break;
            }
            out[pos++] = '\\';
        }
        out[pos++] = *p;
    }

    out[pos] = '\0';
}

/**
 * @brief Convert a boolean state to CLI text.
 * @param[in] value boolean input
 * @return textual representation
 */
static const char *cli_bool_to_str_(bool value)
{
    return value ? "true" : "false";
}

/**
 * @brief Convert TAL log level to CLI text.
 * @param[in] level log level enum
 * @return textual representation
 */
static const char *cli_log_level_to_str_(TAL_LOG_LEVEL_E level)
{
    switch (level) {
    case TAL_LOG_LEVEL_ERR:
        return "err";
    case TAL_LOG_LEVEL_WARN:
        return "warn";
    case TAL_LOG_LEVEL_NOTICE:
        return "notice";
    case TAL_LOG_LEVEL_INFO:
        return "info";
    case TAL_LOG_LEVEL_DEBUG:
        return "debug";
    case TAL_LOG_LEVEL_TRACE:
        return "trace";
    default:
        return "unknown";
    }
}

/**
 * @brief Parse CLI text into TAL log level.
 * @param[in] text input text
 * @param[out] level parsed log level
 * @return true on success, false on invalid text
 */
static bool cli_parse_log_level_(const char *text, TAL_LOG_LEVEL_E *level)
{
    if (text == NULL || level == NULL) {
        return false;
    }

    if (strcmp(text, "err") == 0) {
        *level = TAL_LOG_LEVEL_ERR;
    } else if (strcmp(text, "warn") == 0) {
        *level = TAL_LOG_LEVEL_WARN;
    } else if (strcmp(text, "notice") == 0) {
        *level = TAL_LOG_LEVEL_NOTICE;
    } else if (strcmp(text, "info") == 0) {
        *level = TAL_LOG_LEVEL_INFO;
    } else if (strcmp(text, "debug") == 0) {
        *level = TAL_LOG_LEVEL_DEBUG;
    } else if (strcmp(text, "trace") == 0) {
        *level = TAL_LOG_LEVEL_TRACE;
    } else {
        return false;
    }

    return true;
}

/**
 * @brief Format a MAC address for CLI display.
 * @param[in] mac MAC address structure
 * @param[out] out output text buffer
 * @param[in] out_size output buffer size
 * @return none
 */
static void cli_format_mac_(const NW_MAC_S *mac, char *out, size_t out_size)
{
    if (mac == NULL || out == NULL || out_size == 0) {
        return;
    }

    snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac->mac[0], mac->mac[1], mac->mac[2], mac->mac[3], mac->mac[4], mac->mac[5]);
}

/**
 * @brief Print current heap information.
 * @return none
 */
static void cli_print_heap_info_(void)
{
    cli_echof_("heap.free         %d", tal_system_get_free_heap_size());
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    cli_echof_("psram.free        %d", tal_psram_get_free_heap_size());
#endif
}

/**
 * @brief Print current WiFi connection details.
 * @return none
 */
static void cli_print_wifi_info_(void)
{
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    netconn_wifi_info_t wifi_info    = {0};
    uint8_t             bssid[6]     = {0};
    int8_t              wifi_rssi    = 0;
    OPERATE_RET         rt;

    rt = netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
    if (rt == OPRT_OK && wifi_info.ssid[0] != '\0') {
        cli_echof_("wifi.ssid        %s", wifi_info.ssid);
    } else if (rt == OPRT_OK) {
        cli_echof_("wifi.ssid        (empty)");
    } else {
        cli_echof_("wifi.ssid        unavailable (rt=%d)", rt);
    }

    if (tal_wifi_get_bssid(bssid) == OPRT_OK) {
        cli_echof_("wifi.bssid       %02X:%02X:%02X:%02X:%02X:%02X",
                     bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    }

    if (tal_wifi_station_get_conn_ap_rssi(&wifi_rssi) == OPRT_OK) {
        cli_echof_("wifi.rssi        %d dBm", wifi_rssi);
    }
#endif
}

/**
 * @brief Print current network information.
 * @return none
 */
static void cli_print_network_info_(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    NW_IP_S         ip     = {0};
    NW_MAC_S        mac    = {0};
    char            mac_text[32] = {0};
    OPERATE_RET     rt;

    rt = netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    if (rt == OPRT_OK) {
        cli_echof_("network.status   %s", NETMGR_STATUS_TO_STR(status));
    } else {
        cli_echof_("network.status   unavailable (rt=%d)", rt);
    }

    rt = netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_IP, &ip);
    if (rt == OPRT_OK) {
#if defined(ENABLE_IPv6) && (ENABLE_IPv6 == 1)
        if (IS_NW_IPV6_ADDR(&ip)) {
            cli_echof_("network.ip       %s", ip.addr.ip6.ip);
        } else {
            cli_echof_("network.ip       %s", ip.nwipstr);
            cli_echof_("network.mask     %s", ip.nwmaskstr);
            cli_echof_("network.gw       %s", ip.nwgwstr);
        }
#else
        cli_echof_("network.ip       %s", ip.ip);
        cli_echof_("network.mask     %s", ip.mask);
        cli_echof_("network.gw       %s", ip.gw);
        cli_echof_("network.dns      %s", ip.dns);
        cli_echof_("network.dhcp     %s", cli_bool_to_str_(ip.dhcpen == TRUE));
#endif
    }

    rt = netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_MAC, &mac);
    if (rt == OPRT_OK) {
        cli_format_mac_(&mac, mac_text, sizeof(mac_text));
        cli_echof_("network.mac      %s", mac_text);
    }

    cli_print_wifi_info_();
}

/**
 * @brief Build a masked display string for a KV text value.
 *        Shows first 4 and last 4 characters in plain text, with "***" in between.
 *        If the total visible length is <= 8, the full value is shown without masking.
 * @param[in]  value    text value (null-terminated or with trailing '\0' counted in length)
 * @param[in]  length   buffer length including any trailing '\0'
 * @param[out] out      output buffer for the masked string
 * @param[in]  out_size output buffer size
 * @return none
 */
static void cli_mask_kv_text_(const uint8_t *value, size_t length, char *out, size_t out_size)
{
    size_t text_len;
    const char *str;

    if (value == NULL || out == NULL || out_size == 0) {
        return;
    }

    str      = (const char *)value;
    text_len = length;
    if (text_len > 0 && value[text_len - 1] == '\0') {
        text_len--;
    }

    if (text_len <= 8) {
        snprintf(out, out_size, "%.*s", (int)text_len, str);
        return;
    }

    snprintf(out, out_size, "%.4s***%.*s", str, 4, str + text_len - 4);
}

/**
 * @brief Build a masked display string for a KV binary value (hex).
 *        Shows first 4 and last 4 bytes as hex with "***" in between.
 *        If length is <= 8, all bytes are shown without masking.
 * @param[in]  value    binary value buffer
 * @param[in]  length   buffer length
 * @param[out] out      output buffer for the masked hex string
 * @param[in]  out_size output buffer size
 * @return none
 */
static void cli_mask_kv_binary_(const uint8_t *value, size_t length, char *out, size_t out_size)
{
    int   pos = 0;
    size_t head = (length < 4) ? length : 4;
    size_t tail = (length < 4) ? 0 : 4;

    if (value == NULL || out == NULL || out_size == 0) {
        return;
    }

    for (size_t i = 0; i < head && pos + 3 < (int)out_size; i++) {
        pos += snprintf(out + pos, out_size - (size_t)pos, "%02X ", value[i]);
    }

    if (length > 8) {
        if (pos + 4 < (int)out_size) {
            pos += snprintf(out + pos, out_size - (size_t)pos, "*** ");
        }
        for (size_t i = length - tail; i < length && pos + 3 < (int)out_size; i++) {
            pos += snprintf(out + pos, out_size - (size_t)pos, "%02X ", value[i]);
        }
    } else if (length > head) {
        for (size_t i = head; i < length && pos + 3 < (int)out_size; i++) {
            pos += snprintf(out + pos, out_size - (size_t)pos, "%02X ", value[i]);
        }
    }

    if (pos > 0 && out[pos - 1] == ' ') {
        out[pos - 1] = '\0';
    }
}

/**
 * @brief Print a binary preview for a KV value.
 * @param[in] value binary value buffer
 * @param[in] length buffer length
 * @return none
 */
static void cli_print_kv_binary_preview_(const uint8_t *value, size_t length)
{
    char  line[CLI_LINE_SIZE] = {0};
    int   pos                 = 0;
    size_t preview_len        = (length < 16) ? length : 16;

    pos += snprintf(line + pos, sizeof(line) - pos, "value(hex)        ");
    for (size_t i = 0; i < preview_len && pos < (int)sizeof(line); i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02X%s", value[i], (i + 1 == preview_len) ? "" : " ");
    }

    if (length > preview_len && pos < (int)sizeof(line)) {
        (void)snprintf(line + pos, sizeof(line) - pos, " ...");
    }

    tal_cli_echo(line);
}

/**
 * @brief Check whether a KV value is printable text.
 * @param[in] value value buffer
 * @param[in] length buffer length
 * @return true if value can be shown as text, false otherwise
 */
static bool cli_kv_value_is_text_(const uint8_t *value, size_t length)
{
    size_t text_len = length;

    if (value == NULL || length == 0) {
        return false;
    }

    if (value[length - 1] == '\0') {
        text_len = length - 1;
    }

    for (size_t i = 0; i < text_len; i++) {
        if (value[i] == '\n' || value[i] == '\r' || value[i] == '\t') {
            continue;
        }
        if (value[i] < 32 || value[i] > 126) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Join a parent path and child node name.
 * @param[in] dir parent directory
 * @param[in] name child name
 * @param[out] out output path buffer
 * @param[in] out_size output path buffer size
 * @return none
 */
static void cli_fs_join_path_(const char *dir, const char *name, char *out, size_t out_size)
{
    size_t dir_len;

    if (out == NULL || out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (dir == NULL || dir[0] == '\0') {
        snprintf(out, out_size, "%s", (name != NULL) ? name : "");
        return;
    }

    if (name == NULL || name[0] == '\0') {
        snprintf(out, out_size, "%s", dir);
        return;
    }

    dir_len = strlen(dir);
    if (dir_len > 0 && dir[dir_len - 1] == '/') {
        snprintf(out, out_size, "%s%s", dir, name);
    } else {
        snprintf(out, out_size, "%s/%s", dir, name);
    }
}

/**
 * @brief Build an ASCII tree prefix for one directory depth.
 * @param[in] depth current depth, first child level is 1
 * @param[out] out output buffer
 * @param[in] out_size output buffer size
 * @return none
 */
static void cli_fs_build_tree_prefix_(int depth, char *out, size_t out_size)
{
    size_t offset = 0;

    if (out == NULL || out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (depth <= 0) {
        return;
    }

    for (int i = 1; i < depth && offset + 3 < out_size; i++) {
        int written = snprintf(out + offset, out_size - offset, "|  ");
        if (written < 0 || (size_t)written >= out_size - offset) {
            out[out_size - 1] = '\0';
            return;
        }
        offset += (size_t)written;
    }

    if (offset + 4 < out_size) {
        (void)snprintf(out + offset, out_size - offset, "|- ");
    }
}

/**
 * @brief Recursively list directory entries up to a fixed depth.
 * @param[in] path directory path to scan
 * @param[in] depth current recursion depth, starting from 1
 * @param[in] max_depth maximum allowed recursion depth
 * @param[out] count accumulated entry count
 * @return OPRT_OK on success, error code on failure
 */
static OPERATE_RET cli_fs_list_dir_recursive_(const char *path, int depth, int max_depth, uint32_t *count)
{
    TUYA_DIR    dir = NULL;
    OPERATE_RET rt;

    if (path == NULL || count == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = tal_dir_open(path, &dir);
    if (rt != OPRT_OK || dir == NULL) {
        return (rt == OPRT_OK) ? OPRT_DIR_OPEN_FAILED : rt;
    }
    char *path_ptr = NULL;
    path_ptr = tal_malloc(CLI_VALUE_SIZE);
    if (path_ptr == NULL) {
        cli_echof_("ERR: malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    while (1) {
        TUYA_FILEINFO info                      = NULL;
        const char   *name                      = NULL;
        BOOL_T        is_dir                    = FALSE;
        
        char          tree_prefix[64]           = {0};
        bool          recurse                   = false;

        rt = tal_dir_read(dir, &info);
        if (rt == OPRT_EOD) {
            rt = OPRT_OK;
            break;
        }
        if (rt != OPRT_OK) {
            break;
        }
        if (info == NULL) {
            rt = OPRT_OK;
            break;
        }

        (void)tal_dir_name(info, &name);
        if (name == NULL || name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        (void)tal_dir_is_directory(info, &is_dir);
        cli_fs_join_path_(path, name, path_ptr, CLI_VALUE_SIZE);
        cli_fs_build_tree_prefix_(depth, tree_prefix, CLI_VALUE_SIZE);

        if (is_dir == TRUE) {
            cli_echof_("%s%s/", tree_prefix, name);
            recurse = (depth < max_depth);
        } else {
            cli_echof_("%s%s", tree_prefix, name);
        }

        (*count)++;

        if (recurse == true) {
            OPERATE_RET sub_rt = cli_fs_list_dir_recursive_(path_ptr, depth + 1, max_depth, count);
            if (sub_rt != OPRT_OK) {
                cli_echof_("%*sERR: tal_dir_open('%s') rt=%d", depth * 2, "", path_ptr, sub_rt);
            }
        }
    }

    (void)tal_dir_close(dir);
    tal_free(path_ptr);
    path_ptr = NULL;
    return rt;
}

/**
 * @brief Implement fs_write and fs_append shared logic.
 * @param[in] path file path
 * @param[in] mode fopen mode
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cli_fs_write_impl_(const char *path, const char *mode, int argc, char *argv[])
{
    TUYA_FILE file;
    char *content_ptr = NULL;
    int       written;

    if (argc < 3) {
        cli_print_usage_((strcmp(argv[0], "fs_append") == 0) ? "fs_append <file> <data...>" : "fs_write <file> <data...>",
                         (strcmp(argv[0], "fs_append") == 0) ? "fs_append /demo.txt world" : "fs_write /demo.txt hello",
                         NULL, NULL);
        return;
    }

    content_ptr = tal_malloc(CLI_VALUE_SIZE);
    if (content_ptr == NULL) {
        cli_echof_("ERR: malloc failed");
        return;
    }

    file = tal_fopen(path, mode);
    if (file == NULL) {
        cli_echof_("ERR: tal_fopen('%s','%s') failed", path, mode);
        tal_free(content_ptr);
        return;
    }

    (void)cli_join_args_(argc, argv, 2, content_ptr, CLI_VALUE_SIZE);
    written = tal_fwrite(content_ptr, strlen(content_ptr), file);
    (void)tal_fsync(file);
    (void)tal_fclose(file);

    if (written < 0) {
        cli_echof_("ERR: write failed n=%d", written);
        tal_free(content_ptr);
        return;
    }

    cli_echof_("OK: wrote %d bytes to %s", written, path);
    tal_free(content_ptr);
    content_ptr = NULL;
}

#endif /* ENABLE_SERIAL_CLI_CMD && (ENABLE_SERIAL_CLI_CMD == 1) */
#if defined(CLI_CMD_SYS)

/* ---------------------------------------------------------------------------
 * System commands
 * --------------------------------------------------------------------------- */
/**
 * @brief Show device runtime status.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_status(int argc, char *argv[])
{
    TAL_LOG_LEVEL_E     log_level    = TAL_LOG_LEVEL_INFO;
    char               *reason       = NULL;
    TUYA_RESET_REASON_E reset_reason;
    SYS_TICK_T          tick_count;
    SYS_TIME_T          uptime_ms;
    TIME_T              posix_time;
    POSIX_TM_S          tm           = {0};
    uint32_t            sec, min, hour, day;

    (void)argc;
    (void)argv;

    tick_count = tal_system_get_tick_count();
    uptime_ms  = tal_system_get_millisecond();
    posix_time = tal_time_get_posix();
    tal_time_get_local_time_custom(posix_time, &tm);

    sec  = (uint32_t)(uptime_ms / 1000);
    day  = sec / 86400;
    hour = (sec % 86400) / 3600;
    min  = (sec % 3600) / 60;
    sec  = sec % 60;

    tal_cli_echo("--- System status ---");

    cli_echof_("tick.count        %llu", (unsigned long long)tick_count);
    cli_echof_("uptime            %ud %uh %um %us (%llu ms)", day, hour, min, sec, (unsigned long long)uptime_ms);
    cli_echof_("posix.time        %llu", (unsigned long long)posix_time);
    cli_echof_("local.time        %04d-%02d-%02d %02d:%02d:%02d",
               tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec);

    if (tal_log_get_level(&log_level) == OPRT_OK) {
        cli_echof_("log.level         %s", cli_log_level_to_str_(log_level));
    }

    reset_reason = tal_system_get_reset_reason(&reason);
    cli_echof_("reset.reason      %d (%s)", (int)reset_reason, (reason != NULL) ? reason : "unknown");

    cli_print_heap_info_();
    cli_print_network_info_();
}

/**
 * @brief Show heap information.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_heap(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    tal_cli_echo("--- Heap status ---");
    cli_print_heap_info_();
}

/**
 * @brief Dump all thread watermark information.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_thread(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    tal_cli_echo("--- Thread watermark ---");
    tal_cli_echo("Note: output appears on log port");
    tal_thread_dump_watermark();
}

/**
 * @brief Show project, SDK, and platform version info.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_version(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    tal_cli_echo("--- Version info ---");
    cli_echof_("project.name      %s", PROJECT_NAME);
    cli_echof_("project.version   %s", PROJECT_VERSION);
    cli_echof_("build.date        %s", __DATE__);
    cli_echof_("build.time        %s", __TIME__);
    cli_echof_("open.version      %s", OPEN_VERSION);
    cli_echof_("open.commit       %s", OPEN_COMMIT);
    cli_echof_("platform.chip     %s", PLATFORM_CHIP);
    cli_echof_("platform.board    %s", PLATFORM_BOARD);
    cli_echof_("platform.commit   %s", PLATFORM_COMMIT);
}

/**
 * @brief Show current system tick and millisecond counters.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_tick(int argc, char *argv[])
{
    SYS_TICK_T tick_count;
    SYS_TIME_T uptime_ms;
    TIME_T     posix_time;
    POSIX_TM_S tm = {0};
    uint32_t   sec, min, hour, day;

    (void)argc;
    (void)argv;

    tick_count = tal_system_get_tick_count();
    uptime_ms  = tal_system_get_millisecond();
    posix_time = tal_time_get_posix();
    tal_time_get_local_time_custom(posix_time, &tm);

    sec  = (uint32_t)(uptime_ms / 1000);
    day  = sec / 86400;
    hour = (sec % 86400) / 3600;
    min  = (sec % 3600) / 60;
    sec  = sec % 60;

    cli_echof_("tick.count        %llu", (unsigned long long)tick_count);
    cli_echof_("uptime.ms         %llu", (unsigned long long)uptime_ms);
    cli_echof_("uptime            %ud %uh %um %us", day, hour, min, sec);
    cli_echof_("posix.time        %llu", (unsigned long long)posix_time);
    cli_echof_("local.time        %04d-%02d-%02d %02d:%02d:%02d",
               tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/**
 * @brief Set log level.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_set_log_level(int argc, char *argv[])
{
    TAL_LOG_LEVEL_E level = TAL_LOG_LEVEL_INFO;
    OPERATE_RET     rt;

    if (argc < 2) {
        cli_print_usage_("sys_set_log_level <err|warn|notice|info|debug|trace>",
                         "sys_set_log_level debug", NULL, NULL);
        return;
    }

    if (cli_parse_log_level_(argv[1], &level) == false) {
        cli_print_usage_("sys_set_log_level <err|warn|notice|info|debug|trace>",
                         "sys_set_log_level debug", NULL, NULL);
        return;
    }

    rt = tal_log_set_level(level);
    cli_echof_("%s: sys_set_log_level '%s' rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", argv[1], rt);
}

/**
 * @brief Reboot the device.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_reboot(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    tal_cli_echo("Rebooting device...");
    tal_system_sleep(100);
    tal_system_reset();
}

/**
 * @brief Stop Tuya IoT client.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_iot_stop(int argc, char *argv[])
{
    OPERATE_RET rt;

    (void)argc;
    (void)argv;

    rt = tuya_iot_stop(tuya_iot_client_get());
    cli_echof_("%s: sys_iot_stop rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", rt);
}

/**
 * @brief Restart Tuya IoT client.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_iot_restart(int argc, char *argv[])
{
    OPERATE_RET rt;

    (void)argc;
    (void)argv;

    rt = tuya_iot_stop(tuya_iot_client_get());
    if (rt != OPRT_OK) {
        cli_echof_("ERR: sys_iot_restart stop rt=%d", rt);
        return;
    }

    rt = tuya_iot_start(tuya_iot_client_get());
    cli_echof_("%s: sys_iot_restart rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", rt);
}

/**
 * @brief Reset Tuya IoT activation.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_iot_reset(int argc, char *argv[])
{
    OPERATE_RET rt;

    (void)argc;
    (void)argv;

    rt = tuya_iot_reset(tuya_iot_client_get());
    cli_echof_("%s: sys_iot_reset rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", rt);
}

/**
 * @brief Get Tuya device ID (assigned after cloud activation).
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 * @note Returns empty or NULL when the device has not been activated yet.
 */
static void cmd_sys_iot_get_devid(int argc, char *argv[])
{
    const char *devid = tuya_iot_devid_get(tuya_iot_client_get());

    (void)argc;
    (void)argv;

    if (devid != NULL && devid[0] != '\0') {
        cli_echof_("device_id: %s", devid);
    } else {
        tal_cli_echo("device_id: (not available — device not activated)");
    }
}

/**
 * @brief Forward arguments to the SDK netmgr CLI.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_netmgr(int argc, char *argv[])
{
    tal_cli_echo("Note: output appears on log port");
    if (argc >= 2 && strcmp(argv[1], "help") == 0) {
        tal_cli_echo("Usage:");
        tal_cli_echo("  sys_netmgr                           Show connection status");
        tal_cli_echo("  sys_netmgr wifi up <ssid> <pwd>      Connect to WiFi");
        tal_cli_echo("  sys_netmgr wifi down                 Disconnect WiFi");
        tal_cli_echo("  sys_netmgr wifi scan                 Scan nearby APs");
        tal_cli_echo("  sys_netmgr wired up|down             Wired link control (TBD)");
        tal_cli_echo("  sys_netmgr switch                    Switch active interface (TBD)");
        return;
    }
    netmgr_cmd(argc, argv);
}

#if defined(PLATFORM_LINUX) && (PLATFORM_LINUX == 1)
/**
 * @brief Execute a shell command (Linux only).
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_exec(int argc, char *argv[])
{
    char *command_ptr = NULL;
    int  status;

    if (argc < 2) {
        cli_print_usage_("sys_exec <cmd...>", "sys_exec ls /", NULL, NULL);
        return;
    }

    command_ptr = tal_malloc(CLI_VALUE_SIZE);
    if (command_ptr == NULL) {
        cli_echof_("ERR: malloc failed");
        return;
    }

    if (cli_join_args_(argc, argv, 1, command_ptr, CLI_VALUE_SIZE) == false) {
        cli_print_usage_("sys_exec <cmd...>", "sys_exec ls /", NULL, NULL);
        tal_free(command_ptr);
        return;
    }

    status = system(command_ptr);
    cli_echof_("sys_exec exit=%d", status);
    tal_free(command_ptr);
    command_ptr = NULL;
}
#endif

/**
 * @brief Report a Tuya IoT datapoint with user-specified ID, type, and value.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 * @note Syntax: sys_iot_report_dp <dp_id> <type> <value>
 *       type: bool  value: true | false
 *       type: int   value: integer number
 *       type: str   value: string (no quotes)
 */
static void cmd_sys_iot_report_dp(int argc, char *argv[])
{
    char *payload_ptr = NULL;
    OPERATE_RET rt;

    if (argc < 4) {
        cli_print_usage_("sys_iot_report_dp <dp_id> <bool|int|str> <value>",
                         "sys_iot_report_dp 1 bool true",
                         "sys_iot_report_dp 101 int 50",
                         "sys_iot_report_dp 2 str hello");
        return;
    }

    if (cli_is_numeric_(argv[1]) == false) {
        cli_echof_("ERR: dp_id must be numeric, got '%s'", argv[1]);
        return;
    }

    payload_ptr = tal_malloc(CLI_LINE_SIZE);
    if (payload_ptr == NULL) {
        cli_echof_("ERR: malloc failed");
        return;
    }

    if (strcmp(argv[2], "bool") == 0) {
        if (strcmp(argv[3], "true") == 0 || strcmp(argv[3], "1") == 0) {
            snprintf(payload_ptr, CLI_LINE_SIZE, "{\"%s\": true}", argv[1]);
        } else if (strcmp(argv[3], "false") == 0 || strcmp(argv[3], "0") == 0) {
            snprintf(payload_ptr, CLI_LINE_SIZE, "{\"%s\": false}", argv[1]);
        } else {
            cli_echof_("ERR: bool value must be true/false/1/0, got '%s'", argv[3]);
            tal_free(payload_ptr);
            return;
        }
    } else if (strcmp(argv[2], "int") == 0) {
        char *endptr = NULL;
        long val = strtol(argv[3], &endptr, 10);

        if (endptr == argv[3] || (endptr != NULL && *endptr != '\0')) {
            cli_echof_("ERR: invalid integer '%s'", argv[3]);
            tal_free(payload_ptr);
            return;
        }
        snprintf(payload_ptr, CLI_LINE_SIZE, "{\"%s\": %ld}", argv[1], val);
    } else if (strcmp(argv[2], "str") == 0) {
        char *joined_ptr = NULL;
        char *escaped_ptr = NULL;

        joined_ptr = tal_malloc(CLI_VALUE_SIZE);
        if (joined_ptr == NULL) {
            cli_echof_("ERR: malloc failed");
            tal_free(payload_ptr);
            return;
        }

        escaped_ptr = tal_malloc(CLI_VALUE_SIZE);
        if (escaped_ptr == NULL) {
            cli_echof_("ERR: malloc failed");
            tal_free(joined_ptr);
            tal_free(payload_ptr);
            return;
        }

        (void)cli_join_args_(argc, argv, 3, joined_ptr, CLI_VALUE_SIZE);
        cli_json_escape_(joined_ptr, escaped_ptr, CLI_VALUE_SIZE);
        snprintf(payload_ptr, CLI_LINE_SIZE, "{\"%s\": \"%s\"}", argv[1], escaped_ptr);
        tal_free(escaped_ptr);
        tal_free(joined_ptr);
        escaped_ptr = NULL;
        joined_ptr = NULL;
    } else {
        cli_echof_("ERR: unknown type '%s', use bool/int/str", argv[2]);
        tal_free(payload_ptr);
        return;
    }

    rt = tuya_iot_dp_report_json(tuya_iot_client_get(), payload_ptr);
    cli_echof_("%s: sys_iot_report_dp payload=%s rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", payload_ptr, rt);
    tal_free(payload_ptr);
    payload_ptr = NULL;
}

/**
 * @brief Show active software timer count.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_timer_count(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    cli_echof_("active sw timers: %d", tal_sw_timer_get_num());
}

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
/**
 * @brief Show current WiFi connection details.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_wifi_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    tal_cli_echo("--- WiFi info ---");
    cli_print_wifi_info_();
}

/**
 * @brief Scan nearby WiFi access points.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_sys_wifi_scan(int argc, char *argv[])
{
    AP_IF_S    *ap_list = NULL;
    uint32_t    ap_num  = 0;
    OPERATE_RET rt;

    (void)argc;
    (void)argv;

    rt = tal_wifi_all_ap_scan(&ap_list, &ap_num);
    if (rt != OPRT_OK || ap_list == NULL) {
        cli_echof_("ERR: tal_wifi_all_ap_scan rt=%d", rt);
        return;
    }

    if (ap_num > CLI_WIFI_SCAN_MAX_AP_NUM) {
        ap_num = CLI_WIFI_SCAN_MAX_AP_NUM;
    }
    cli_echof_("Found %u APs:", (unsigned)ap_num);
    for (uint32_t i = 0; i < ap_num; i++) {
        cli_echof_("  [%2u] %-32s  ch:%2d  rssi:%d  sec:%d",
                     i + 1, ap_list[i].ssid, ap_list[i].channel,
                     ap_list[i].rssi, ap_list[i].s_len);
    }

    tal_wifi_release_ap(ap_list);
}
#endif

#endif /* CLI_CMD_SYS */

#if defined(CLI_CMD_FS)
/* ---------------------------------------------------------------------------
 * Filesystem commands
 * --------------------------------------------------------------------------- */
/**
 * @brief List one directory.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_ls(int argc, char *argv[])
{
    const char *path = (argc >= 2) ? argv[1] : CLI_FS_DEFAULT_PATH;
    OPERATE_RET rt;
    uint32_t    count = 0;

    cli_echof_("Listing: %s (max depth=%d)", path, CLI_FS_LS_MAX_DEPTH);
    cli_echof_("%s/", path);
    rt = cli_fs_list_dir_recursive_(path, 1, CLI_FS_LS_MAX_DEPTH, &count);
    if (rt != OPRT_OK) {
        cli_echof_("ERR: tal_dir_open('%s') rt=%d", path, rt);
        return;
    }

    cli_echof_("Done. entries=%u", (unsigned)count);
}

/**
 * @brief Show file or directory metadata.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_stat(int argc, char *argv[])
{
    BOOL_T      exists  = FALSE;
    TUYA_DIR    dir     = NULL;
    bool        is_dir  = false;
    unsigned int mode   = 0;
    int         size;
    OPERATE_RET rt;
    OPERATE_RET mode_rt;

    if (argc < 2) {
        cli_print_usage_("fs_stat <path>", "fs_stat /demo.txt", NULL, NULL);
        return;
    }

    rt = tal_fs_is_exist(argv[1], &exists);
    if (rt != OPRT_OK) {
        cli_echof_("ERR: tal_fs_is_exist('%s') rt=%d", argv[1], rt);
        return;
    }

    if (exists == FALSE) {
        cli_echof_("NOT FOUND: %s", argv[1]);
        return;
    }

    if (tal_dir_open(argv[1], &dir) == OPRT_OK && dir != NULL) {
        is_dir = true;
        (void)tal_dir_close(dir);
    }

    size    = tal_fgetsize(argv[1]);
    mode_rt = tal_fs_mode(argv[1], &mode);

    cli_echof_("path: %s", argv[1]);
    cli_echof_("type: %s", is_dir ? "dir" : "file");
    if (!is_dir && size >= 0) {
        cli_echof_("size: %d", size);
    }
    if (mode_rt == OPRT_OK) {
        cli_echof_("mode: 0x%08x", mode);
    } else {
        cli_echof_("mode: (n/a) rt=%d", mode_rt);
    }
}

/**
 * @brief Print a text file.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_cat(int argc, char *argv[])
{
    const char *path;
    long        max_bytes;
    TUYA_FILE   file;
    long        total = 0;
    char        buf[128];

    if (argc < 2) {
        cli_print_usage_("fs_cat <file> [max]", "fs_cat /demo.txt", "fs_cat /demo.txt 256", NULL);
        return;
    }

    path      = argv[1];
    if (argc >= 3) {
        char *endptr = NULL;
        max_bytes = strtol(argv[2], &endptr, 10);
        if (endptr == argv[2] || (endptr != NULL && *endptr != '\0')) {
            cli_echof_("ERR: invalid max '%s'", argv[2]);
            return;
        }
    } else {
        max_bytes = CLI_DEFAULT_TEXT_LIMIT;
    }
    if (max_bytes <= 0) {
        tal_cli_echo("ERR: max_bytes must be > 0");
        return;
    }

    file = tal_fopen(path, "r");
    if (file == NULL) {
        cli_echof_("ERR: tal_fopen('%s') failed", path);
        return;
    }

    cli_echof_("=== %s ===", path);
    while (total < max_bytes) {
        char *line = tal_fgets(buf, (int)sizeof(buf), file);
        int   len;

        if (line == NULL) {
            break;
        }

        len = (int)strlen(line);
        if (len <= 0) {
            break;
        }

        if (total + len > max_bytes) {
            buf[max_bytes - total] = '\0';
        }

        tal_cli_echo(buf);
        total += len;
        if (total >= max_bytes) {
            break;
        }
    }

    if (tal_feof(file) != 1 && total >= max_bytes) {
        cli_echof_("[truncated] %ld bytes", total);
    }

    tal_cli_echo("=============");
    (void)tal_fclose(file);
}

/**
 * @brief Print a file as hex dump.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_hexdump(int argc, char *argv[])
{
    const char *path;
    long        max_bytes;
    TUYA_FILE   file;
    uint8_t     buf[16];
    long        offset = 0;

    if (argc < 2) {
        cli_print_usage_("fs_hexdump <file> [max]", "fs_hexdump /demo.bin", "fs_hexdump /demo.bin 128", NULL);
        return;
    }

    path      = argv[1];
    if (argc >= 3) {
        char *endptr = NULL;
        max_bytes = strtol(argv[2], &endptr, 10);
        if (endptr == argv[2] || (endptr != NULL && *endptr != '\0')) {
            cli_echof_("ERR: invalid max '%s'", argv[2]);
            return;
        }
    } else {
        max_bytes = CLI_DEFAULT_HEX_LIMIT;
    }
    if (max_bytes <= 0) {
        tal_cli_echo("ERR: max_bytes must be > 0");
        return;
    }

    file = tal_fopen(path, "r");
    if (file == NULL) {
        cli_echof_("ERR: tal_fopen('%s') failed", path);
        return;
    }

    while (offset < max_bytes) {
        int  want = (int)(((max_bytes - offset) > (long)sizeof(buf)) ? sizeof(buf) : (max_bytes - offset));
        int  read_bytes;
        char line[CLI_LINE_SIZE] = {0};
        int  pos = 0;

        read_bytes = tal_fread(buf, want, file);
        if (read_bytes <= 0) {
            break;
        }

        pos += snprintf(line + pos, sizeof(line) - pos, "%08lx  ", offset);
        for (int i = 0; i < (int)sizeof(buf); i++) {
            if (i < read_bytes) {
                pos += snprintf(line + pos, sizeof(line) - pos, "%02x ", buf[i]);
            } else {
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
            }
        }
        pos += snprintf(line + pos, sizeof(line) - pos, " |");
        for (int i = 0; i < read_bytes && pos + 2 < (int)sizeof(line); i++) {
            line[pos++] = (buf[i] >= 32 && buf[i] <= 126) ? (char)buf[i] : '.';
            line[pos]   = '\0';
        }
        if (pos + 2 < (int)sizeof(line)) {
            line[pos++] = '|';
            line[pos]   = '\0';
        }

        tal_cli_echo(line);
        offset += read_bytes;
        if (read_bytes < want) {
            break;
        }
    }

    if (offset >= max_bytes) {
        cli_echof_("[truncated] %ld bytes", offset);
    }

    (void)tal_fclose(file);
}

/**
 * @brief Overwrite a file with text content.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_write(int argc, char *argv[])
{
    cli_fs_write_impl_((argc >= 2) ? argv[1] : "", "w", argc, argv);
}

/**
 * @brief Append text content to a file.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_append(int argc, char *argv[])
{
    cli_fs_write_impl_((argc >= 2) ? argv[1] : "", "a", argc, argv);
}

/**
 * @brief Remove one file system path.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_rm(int argc, char *argv[])
{
    OPERATE_RET rt;

    if (argc < 2) {
        cli_print_usage_("fs_rm <path>", "fs_rm /demo.txt", NULL, NULL);
        return;
    }

    rt = tal_fs_remove(argv[1]);
    cli_echof_("%s: fs_rm rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", rt);
}

/**
 * @brief Create one directory.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_mkdir(int argc, char *argv[])
{
    OPERATE_RET rt;

    if (argc < 2) {
        cli_print_usage_("fs_mkdir <dir>", "fs_mkdir /logs", NULL, NULL);
        return;
    }

    rt = tal_fs_mkdir(argv[1]);
    cli_echof_("%s: fs_mkdir rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", rt);
}

/**
 * @brief Rename or move one file system path.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_fs_mv(int argc, char *argv[])
{
    OPERATE_RET rt;

    if (argc < 3) {
        cli_print_usage_("fs_mv <old> <new>", "fs_mv /old.txt /new.txt", NULL, NULL);
        return;
    }

    rt = tal_fs_rename(argv[1], argv[2]);
    cli_echof_("%s: fs_mv rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", rt);
}

#endif /* CLI_CMD_FS */

#if defined(CLI_CMD_KV)
/* ---------------------------------------------------------------------------
 * KV commands
 * --------------------------------------------------------------------------- */
/**
 * @brief Read one KV entry.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_kv_get(int argc, char *argv[])
{
    uint8_t    *value = NULL;
    size_t      length = 0;
    OPERATE_RET rt;

    if (argc < 2) {
        cli_print_usage_("kv_get <key>", "kv_get app_token", NULL, NULL);
        return;
    }

    rt = tal_kv_get(argv[1], &value, &length);
    if (rt != OPRT_OK || value == NULL) {
        cli_echof_("ERR: kv_get '%s' rt=%d", argv[1], rt);
        if (value != NULL) {
            tal_kv_free(value);
        }
        return;
    }

    cli_echof_("key: %s", argv[1]);
    cli_echof_("len: %u", (unsigned)length);
    char *masked_ptr = NULL;
    if (cli_kv_value_is_text_(value, length)) {
        masked_ptr = tal_malloc(CLI_VALUE_SIZE);
        if (masked_ptr == NULL) {
            cli_echof_("ERR: malloc failed");
            tal_kv_free(value);
            return;
        }

        cli_mask_kv_text_(value, length, masked_ptr, CLI_VALUE_SIZE);
        cli_echof_("value: %s", masked_ptr);
        tal_free(masked_ptr);
        masked_ptr = NULL;
    } else {
        masked_ptr = tal_malloc(CLI_VALUE_SIZE);
        if (masked_ptr == NULL) {
            cli_echof_("ERR: malloc failed");
            tal_kv_free(value);
            return;
        }

        cli_mask_kv_binary_(value, length, masked_ptr, CLI_VALUE_SIZE);
        cli_echof_("value(hex): %s", masked_ptr);
        tal_free(masked_ptr);
        masked_ptr = NULL;
    }
    if (masked_ptr != NULL) {
        tal_free(masked_ptr);
        masked_ptr = NULL;
    }
    tal_kv_free(value);
}

/**
 * @brief Write one string KV entry.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_kv_set(int argc, char *argv[])
{
    char        *value_ptr = NULL;
    OPERATE_RET rt;

    if (argc < 3) {
        cli_print_usage_("kv_set <key> <value...>", "kv_set app_token hello", NULL, NULL);
        return;
    }

    value_ptr = tal_malloc(CLI_VALUE_SIZE);
    if (value_ptr == NULL) {
        cli_echof_("ERR: malloc failed");
        return;
    }

    (void)cli_join_args_(argc, argv, 2, value_ptr, CLI_VALUE_SIZE);
    rt = tal_kv_set(argv[1], (const uint8_t *)value_ptr, strlen(value_ptr));
    cli_echof_("%s: kv_set rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", rt);
    tal_free(value_ptr);
    value_ptr = NULL;
}

/**
 * @brief Delete one KV entry.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_kv_del(int argc, char *argv[])
{
    OPERATE_RET rt;

    if (argc < 2) {
        cli_print_usage_("kv_del <key>", "kv_del app_token", NULL, NULL);
        return;
    }

    rt = tal_kv_del(argv[1]);
    cli_echof_("%s: kv_del rt=%d", (rt == OPRT_OK) ? "OK" : "ERR", rt);
}

/**
 * @brief List all KV entries.
 * @param[in] argc CLI argc
 * @param[in] argv CLI argv
 * @return none
 */
static void cmd_kv_list(int argc, char *argv[])
{
    lfs_t          *lfs = NULL;
    lfs_dir_t       dir;
    struct lfs_info info;
    uint32_t        count = 0;
    int             lfs_rt;

    (void)argc;
    (void)argv;

    lfs = tal_lfs_get();
    if (lfs == NULL) {
        tal_cli_echo("ERR: tal_lfs_get returned NULL");
        return;
    }

    memset(&dir, 0, sizeof(dir));
    memset(&info, 0, sizeof(info));

    lfs_rt = lfs_dir_open(lfs, &dir, "/");
    if (lfs_rt != LFS_ERR_OK) {
        cli_echof_("ERR: lfs_dir_open('/') rt=%d", lfs_rt);
        return;
    }

    tal_cli_echo("--- KV list ---");
    while ((lfs_rt = lfs_dir_read(lfs, &dir, &info)) > 0) {
        uint8_t *value  = NULL;
        size_t   length = 0;
        OPERATE_RET rt;

        if (info.name[0] == '\0' || strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
            continue;
        }

        if (info.type == LFS_TYPE_DIR) {
            continue;
        }

        rt = tal_kv_get(info.name, &value, &length);
        if (rt != OPRT_OK) {
            cli_echof_("[%u] key=%s len=(n/a) rt=%d", (unsigned)count, info.name, rt);
            count++;
            continue;
        }

        cli_echof_("[%u] key=%s len=%u", (unsigned)count, info.name, (unsigned)length);
        if (cli_kv_value_is_text_(value, length)) {
            char masked[CLI_LINE_SIZE] = {0};

            cli_mask_kv_text_(value, length, masked, sizeof(masked));
            cli_echof_("     value=%s", masked);
        } else {
            char masked[CLI_LINE_SIZE] = {0};

            cli_mask_kv_binary_(value, length, masked, sizeof(masked));
            cli_echof_("     value(hex)=%s", masked);
        }

        tal_kv_free(value);
        count++;
    }

    if (lfs_rt < 0) {
        cli_echof_("ERR: lfs_dir_read('/') rt=%d", lfs_rt);
    }

    (void)lfs_dir_close(lfs, &dir);
    cli_echof_("Done. entries=%u", (unsigned)count);
}

#endif /* CLI_CMD_KV */
/* ---------------------------------------------------------------------------
 * Command table
 * --------------------------------------------------------------------------- */
static cli_cmd_t s_cli_cmd[] = {
#if defined(CLI_CMD_SYS)
    {.name = "sys_status",       .help = "Device status, tick, uptime, network",.func = cmd_sys_status},
    {.name = "sys_heap",         .help = "Show free heap and PSRAM",             .func = cmd_sys_heap},
    {.name = "sys_thread",       .help = "Dump all thread watermark info",       .func = cmd_sys_thread},
    {.name = "sys_tick",         .help = "Show tick, uptime, posix, local time", .func = cmd_sys_tick},
    {.name = "sys_version",      .help = "Show app, SDK, and platform version",  .func = cmd_sys_version},
    {.name = "sys_set_log_level",.help = "Set log level (err/warn/notice/info/debug/trace)", .func = cmd_sys_set_log_level},
    {.name = "sys_reboot",       .help = "Reboot device",                        .func = cmd_sys_reboot},
    {.name = "sys_timer_count",  .help = "Show active software timer count",     .func = cmd_sys_timer_count},
    {.name = "sys_iot_stop",     .help = "Stop Tuya IoT client",                 .func = cmd_sys_iot_stop},
    {.name = "sys_iot_restart",  .help = "Restart Tuya IoT client",              .func = cmd_sys_iot_restart},
    {.name = "sys_iot_reset",    .help = "Reset Tuya IoT activation",            .func = cmd_sys_iot_reset},
    {.name = "sys_iot_get_devid",    .help = "Get Tuya device ID",                   .func = cmd_sys_iot_get_devid},
    {.name = "sys_iot_report_dp",.help = "Report DP (bool/int/str)",             .func = cmd_sys_iot_report_dp},
    {.name = "sys_netmgr",       .help = "Network manager (output on log port)", .func = cmd_sys_netmgr},
#if defined(PLATFORM_LINUX) && (PLATFORM_LINUX == 1)
    {.name = "sys_exec",         .help = "Execute shell command",                .func = cmd_sys_exec},
#endif
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    {.name = "sys_wifi_info",    .help = "Show current WiFi SSID/BSSID/RSSI",   .func = cmd_sys_wifi_info},
    {.name = "sys_wifi_scan",    .help = "Scan nearby WiFi APs",                .func = cmd_sys_wifi_scan},
#endif
#endif /* CLI_CMD_SYS */

#if defined(CLI_CMD_FS)
    {.name = "fs_ls",            .help = "List directory tree (depth <= 3)",    .func = cmd_fs_ls},
    {.name = "fs_stat",          .help = "Show exist/type/size",                .func = cmd_fs_stat},
    {.name = "fs_cat",           .help = "Print text file",                     .func = cmd_fs_cat},
    {.name = "fs_hexdump",       .help = "Hex dump file",                       .func = cmd_fs_hexdump},
    {.name = "fs_write",         .help = "Overwrite file",                      .func = cmd_fs_write},
    {.name = "fs_append",        .help = "Append to file",                      .func = cmd_fs_append},
    {.name = "fs_rm",            .help = "Remove file or directory",            .func = cmd_fs_rm},
    {.name = "fs_mkdir",         .help = "Create directory",                    .func = cmd_fs_mkdir},
    {.name = "fs_mv",            .help = "Rename or move path",                 .func = cmd_fs_mv},
#endif /* CLI_CMD_FS */

#if defined(CLI_CMD_KV)
    {.name = "kv_get",           .help = "Read a KV value",                     .func = cmd_kv_get},
    {.name = "kv_set",           .help = "Write a string KV value",             .func = cmd_kv_set},
    {.name = "kv_del",           .help = "Delete a KV entry",                   .func = cmd_kv_del},
    {.name = "kv_list",          .help = "List all KV entries",                 .func = cmd_kv_list},
#endif /* CLI_CMD_KV */
};

/* ---------------------------------------------------------------------------
 * Public functions
 * --------------------------------------------------------------------------- */
/**
 * @brief Register all unified CLI commands.
 * @return none
 */
void tuya_app_cli_init(void)
{
    OPERATE_RET rt = tal_cli_cmd_register(s_cli_cmd, sizeof(s_cli_cmd) / sizeof(s_cli_cmd[0]));

    if (rt != OPRT_OK) {
        PR_ERR("tal_cli_cmd_register failed: %d", rt);
    }
    PR_DEBUG("tuya_app_cli_init: tal_cli_cmd_register success");
}
