/**
 * @file cli_cmd.c
 * @brief Command Line Interface (CLI) commands for Tuya IoT applications.
 *
 * This file implements a set of CLI commands for controlling and managing Tuya
 * IoT devices. It includes commands for switching device states, executing
 * system commands, managing key-value pairs, resetting and starting/stopping
 * the IoT process, and retrieving memory usage information. These commands
 * facilitate debugging, testing, and managing Tuya IoT applications directly
 * from a command line interface.
 *
 * Key functionalities provided in this file:
 * - Switching device states (on/off).
 * - Executing arbitrary system commands.
 * - Key-value pair management for device configuration.
 * - Resetting, starting, and stopping the IoT process.
 * - Retrieving current free heap memory size.
 *
 * This implementation leverages Tuya's Application Layer (TAL) APIs and IoT SDK
 * to provide a rich set of commands for device management and debugging. It is
 * designed to enhance the development and testing process of Tuya IoT
 * applications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tuya_iot.h"
#include <stdlib.h>

extern void tal_kv_cmd(int argc, char *argv[]);
extern void netmgr_cmd(int argc, char *argv[]);
extern void weather_get(int argc, char *argv[]);

/**
 * @brief excute system cmd
 *
 * @param argc
 * @param argv
 * @return void
 */
static void system_cmd(int argc, char *argv[])
{
    char cmd[256];

    if (argc < 2) {
        PR_INFO("usge: sys <cmd>");
        return;
    }

    size_t offset = 0;

    for (int i = 1; i < argc; i++) {
        int ret = snprintf(cmd + offset, sizeof(cmd) - offset, "%s ", argv[i]);
        if (ret < 0 || offset + ret >= sizeof(cmd)) {
            break;
        }
        offset += ret;
    }

    PR_DEBUG("system %s", cmd);
    system(cmd);
}

/**
 * @brief get free heap size cmd
 *
 * @param argc
 * @param argv
 */
static void mem(int argc, char *argv[])
{
    int free_heap = 0;
    free_heap = tal_system_get_free_heap_size();
    PR_NOTICE("cur free heap: %d", free_heap);
}

/**
 * @brief reset iot to unactive/unregister
 *
 * @param argc
 * @param argv
 */
static void reset(int argc, char *argv[])
{
    tuya_iot_reset(tuya_iot_client_get());
}

/**
 * @brief reset iot to unactive/unregister
 *
 * @param argc
 * @param argv
 */
static void start(int argc, char *argv[])
{
    tuya_iot_start(tuya_iot_client_get());
}

/**
 * @brief stop iot
 *
 * @param argc
 * @param argv
 */
static void stop(int argc, char *argv[])
{
    tuya_iot_stop(tuya_iot_client_get());
}

/**
 * @brief cli cmd list
 *
 */
static cli_cmd_t s_cli_cmd[] = {
    {.name = "weather", .func = weather_get, .help = "weather get"},
    {.name = "kv", .func = tal_kv_cmd, .help = "kv test"},
    {.name = "sys", .func = system_cmd, .help = "system  cmd"},
    {.name = "reset", .func = reset, .help = "reset iot"},
    {.name = "stop", .func = stop, .help = "stop iot"},
    {.name = "start", .func = start, .help = "start iot"},
    {.name = "mem", .func = mem, .help = "mem size"},
    {.name = "netmgr", .func = netmgr_cmd, .help = "netmgr cmd"},
};

/**
 * @brief
 *
 */
void tuya_app_cli_init(void)
{
    tal_cli_cmd_register(s_cli_cmd, sizeof(s_cli_cmd) / sizeof(s_cli_cmd[0]));
}