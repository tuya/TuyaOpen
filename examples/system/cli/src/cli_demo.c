/**
 * @file cli_demo.c
 * @brief Minimal TuyaOpen application that only starts the TAL CLI.
 *
 * This example deliberately does not initialize the IoT client, network
 * manager, Wi-Fi, Bluetooth, KV storage or provisioning services.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tal_cli.h"
#include "tkl_output.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define CLI_RANDOM_DEFAULT_COUNT 16U
#define CLI_RANDOM_DEFAULT_RANGE 256U
#define CLI_RANDOM_MAX_COUNT     1024U

static void cli_demo_echo_(const char *format, ...)
{
    char    line[160];
    va_list args;

    va_start(args, format);
    (void)vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    tal_cli_echo(line);
}

static bool cli_demo_parse_decimal_(const char *text, uint32_t minimum, uint32_t maximum, uint32_t *value)
{
    uint32_t parsed = 0U;

    if (text == NULL || text[0] == '\0' || value == NULL) {
        return false;
    }

    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        uint32_t digit;

        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        digit = (uint32_t)(*cursor - '0');
        if (parsed > maximum / 10U || (parsed == maximum / 10U && digit > maximum % 10U)) {
            return false;
        }
        parsed = parsed * 10U + digit;
    }

    if (parsed < minimum || parsed > maximum) {
        return false;
    }

    *value = parsed;
    return true;
}

static void cli_demo_random_(int argc, char *argv[])
{
    uint32_t count = CLI_RANDOM_DEFAULT_COUNT;
    uint32_t range = CLI_RANDOM_DEFAULT_RANGE;
    uint32_t check_range;

    if (argc < 1 || argc > 3 || argv == NULL) {
        cli_demo_echo_("Usage: sys_random [count [range]] (count=1..%u, range=0..%d)",
                       CLI_RANDOM_MAX_COUNT, INT_MAX);
        return;
    }
    if (argc >= 2 && !cli_demo_parse_decimal_(argv[1], 1U, CLI_RANDOM_MAX_COUNT, &count)) {
        cli_demo_echo_("sys_random: invalid count; expected 1..%u", CLI_RANDOM_MAX_COUNT);
        return;
    }
    if (argc >= 3 && !cli_demo_parse_decimal_(argv[2], 0U, (uint32_t)INT_MAX, &range)) {
        cli_demo_echo_("sys_random: invalid range; expected 0..%d", INT_MAX);
        return;
    }

    check_range = range == 0U ? 256U : range;
    cli_demo_echo_("sys_random: tal_system_get_random -> tkl_system_get_random");
    cli_demo_echo_("sys_random: start count=%u range=%u", count, range);

    for (uint32_t index = 0U; index < count; ++index) {
        int sample = tal_system_get_random(range);

        if (sample < 0 || (uint32_t)sample >= check_range) {
            cli_demo_echo_("sys_random: invalid result at sample %u: %d", index, sample);
            return;
        }
        cli_demo_echo_("sys_random[%u] = %d (0x%08X)", index, sample, (uint32_t)sample);
    }

    cli_demo_echo_("sys_random: completed %u samples", count);
}

static const cli_cmd_t s_cli_commands[] = {
    {.name = "sys_random", .help = "Test hardware random: [count [range]]", .func = cli_demo_random_},
};

void user_main(void)
{
    OPERATE_RET rt;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Minimal TAL CLI example");
    PR_NOTICE("Project: %s, platform: %s/%s", PROJECT_NAME, PLATFORM_CHIP, PLATFORM_BOARD);
    PR_NOTICE("No IoT, network manager, Wi-Fi, Bluetooth or provisioning startup");

    /* Do not use tal_cli_init(): it also registers the sys_iot/sys_netmgr table. */
    rt = tal_cli_init_with_uart(TUYA_UART_NUM_0);
    if (rt != OPRT_OK) {
        PR_ERR("tal_cli_init_with_uart failed: %d", rt);
        return;
    }

    rt = tal_cli_cmd_register(s_cli_commands, sizeof(s_cli_commands) / sizeof(s_cli_commands[0]));
    if (rt != OPRT_OK) {
        PR_ERR("tal_cli_cmd_register failed: %d", rt);
        return;
    }

    PR_NOTICE("CLI ready. Try: help, version, sys_random 32 256");

    while (1) {
        tal_system_sleep(1000U);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
}
#else
static THREAD_HANDLE s_app_thread;

static void cli_demo_app_thread_(void *arg)
{
    (void)arg;
    user_main();

    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T config = {0};

    config.stackDepth = 1024U * 4U;
    config.priority   = THREAD_PRIO_1;
    config.thrdname   = "cli_demo";
    tal_thread_create_and_start(&s_app_thread, NULL, NULL, cli_demo_app_thread_, NULL, &config);
}
#endif
