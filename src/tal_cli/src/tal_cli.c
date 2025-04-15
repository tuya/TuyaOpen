/**
 * @file tal_cli.c
 * @brief This file contains the implementation of the CLI (Command Line
 * Interface) for devices, providing functionalities such as command processing,
 * history management, and UART communication.
 *
 * The CLI allows for interaction with the device through a serial terminal,
 * offering a set of commands for device control, status queries, and system
 * configuration. It supports command auto-completion, command history, and
 * dynamic command registration, facilitating development and debugging
 * processes.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

/*============================ INCLUDES ======================================*/
#include <string.h>
#include "tuya_slist.h"
#include "tal_uart.h"
#include "tal_log.h"
#include "tal_cli.h"
#include "tal_thread.h"
#include "tal_memory.h"

/*============================ MACROS ========================================*/
#ifndef CLI_BUFFER_SIZE
#define CLI_BUFFER_SIZE 1000
#endif

#ifndef CLI_HISTORY_NUM
#define CLI_HISTORY_NUM 8
#endif

#ifndef CLI_ARGV_NUM
#define CLI_ARGV_NUM 8
#endif

#ifndef CLI_CMD_TABLE_NUM
#define CLI_CMD_TABLE_NUM 10
#endif

#ifndef CLI_CMD_NAME_MAX
#define CLI_CMD_NAME_MAX 20
#endif
/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
typedef struct {
    uint8_t num;
    cli_cmd_t *cmd;
} cli_cmd_table_t;

typedef struct {
    SLIST_HEAD next;
    cli_cmd_table_t table;
} cli_cmd_node_t;

typedef enum {
    CLI_NULL_KEY = '\0',
    CLI_ESC_KEY = 0x1b,
    CLI_ENTER_KEY = '\r',
    CLI_ENTER2_KEY = '\n',
    CLI_BACKSPACE_KEY = '\b',
    CLI_BACKSPACE2_KEY = '\177',
    CLI_TABLE_KEY = '\t',
    CLI_FUNC_TAG_KEY = 0x5b,
    CLI_UP_KEY = 'A',
    CLI_DOWN_KEY = 'B',
    CLI_RIGHT_KEY = 'C',
    CLI_LETF_KEY = 'D',
} cli_key_t;

typedef char history_data_t[CLI_BUFFER_SIZE + 1];

typedef struct {
    uint16_t write_index;
    uint16_t write_count;
    uint16_t read_index;
    uint16_t read_count;
    history_data_t data[CLI_HISTORY_NUM];
} cli_history_t;

typedef struct {
    TUYA_UART_NUM_E port_id;
    THREAD_HANDLE thread;
    char *prompt;
    uint8_t echo;
    uint16_t index;
    uint16_t insert;
    cli_history_t history;
    int argc;
    char *argv[CLI_ARGV_NUM];
    char buffer[CLI_BUFFER_SIZE + 1];
} cli_t;

/*============================ PROTOTYPES ====================================*/
static void cli_hello(int argc, char *argv[]);
static void cli_print_prompt(cli_t *cli);

/*============================ LOCAL VARIABLES ===============================*/
static cli_t *s_cli_handle = NULL;
static SLIST_HEAD s_cli_dynamic_table;
static cli_cmd_table_t s_cli_static_table[CLI_CMD_TABLE_NUM];

static const cli_cmd_t s_cli_cmd[] = {{
    .name = "hello",
    .help = "print helo world",
    .func = cli_hello,
}};

/*============================ IMPLEMENTATION ================================*/
static int32_t cli_out_put(TUYA_UART_NUM_E port_id, char *out_str, uint32_t len)
{
    return tal_uart_write(port_id, (const uint8_t *)out_str, len);
}

static void cli_print_string(cli_t *cli, char *string)
{
    cli_out_put(cli->port_id, "\r\n", 2);
    while ('\0' != *string) {
        cli_out_put(cli->port_id, string++, 1);
    }
}

static void cli_hello(int argc, char *argv[])
{
    cli_print_string(s_cli_handle, "helo world");
}

static cli_cmd_t *cli_cmd_find_with_name(char *name)
{
    int i, j;
    cli_cmd_t *cmd;

    if (NULL == name) {
        return NULL;
    }

    for (i = 0; i < CLI_CMD_TABLE_NUM; i++) {
        for (j = 0; j < s_cli_static_table[i].num; j++) {
            cmd = s_cli_static_table[i].cmd + j;
            if (0 != strcmp(cmd->name, name)) {
                continue;
            }
            return cmd;
        }
    }

    cli_cmd_node_t *node = NULL;
    SLIST_HEAD *pos = NULL;

    SLIST_FOR_EACH_ENTRY(node, cli_cmd_node_t, pos, &s_cli_dynamic_table, next)
    {
        for (i = 0; i < node->table.num; i++) {
            cmd = node->table.cmd + i;
            if (0 != strcmp(cmd->name, name)) {
                continue;
            }
            return cmd;
        }
    }

    return NULL;
}

static void cli_print_cmd(cli_t *cli, cli_cmd_t *cmd)
{
    uint8_t len;
    char name[CLI_CMD_NAME_MAX + 1];

    //! align
    memset(name, ' ', CLI_CMD_NAME_MAX);
    name[CLI_CMD_NAME_MAX] = '\0';
    len = strlen(cmd->name);
    len = len > CLI_CMD_NAME_MAX ? CLI_CMD_NAME_MAX : len;
    strncpy(name, cmd->name, len);
    cli_out_put(cli->port_id, "\r\n", 2);
    cli_out_put(cli->port_id, name, strlen(name));
    cli_out_put(cli->port_id, "\t", 1);
    cli_out_put(cli->port_id, cmd->help, strlen(cmd->help));
}

static void cli_print_all_cmd(cli_t *cli)
{
    int i, j;
    cli_cmd_t *cmd;

    for (i = 0; i < CLI_CMD_TABLE_NUM; i++) {
        for (j = 0; j < s_cli_static_table[i].num; j++) {
            cmd = s_cli_static_table[i].cmd + j;
            cli_print_cmd(cli, cmd);
        }
    }

    cli_cmd_node_t *node = NULL;
    SLIST_HEAD *pos = NULL;

    SLIST_FOR_EACH_ENTRY(node, cli_cmd_node_t, pos, &s_cli_dynamic_table, next)
    {
        for (i = 0; i < node->table.num; i++) {
            cmd = node->table.cmd + i;
            cli_print_cmd(cli, cmd);
        }
    }

    cli_print_prompt(cli);
}

static void cli_print_cmd_title(cli_t *cli)
{
    int i;

    cli_out_put(cli->port_id, "\r\ncmd", 5);
    for (i = 3; i < CLI_CMD_NAME_MAX; i++) {
        cli_out_put(cli->port_id, " ", 1);
    }
    cli_out_put(cli->port_id, "\thelp\r\n", 7);
    for (i = 0; i < 2 * CLI_CMD_NAME_MAX; i++) {
        cli_out_put(cli->port_id, "-", 1);
    }
}

static int cli_table_key(cli_t *cli)
{
    cli_cmd_t *cmd;
    cli_cmd_t *match_cmd = NULL;

    int i, j;
    int count = 0;

    //! print all cmd
    if (0 == cli->index) {
        cli_print_cmd_title(cli);
        cli_print_all_cmd(cli);
        return OPRT_OK;
    }

    //! match one or more
    for (i = 0; i < CLI_CMD_TABLE_NUM; i++) {
        for (j = 0; j < s_cli_static_table[i].num; j++) {
            cmd = s_cli_static_table[i].cmd + j;
            if (0 != strncmp(cmd->name, cli->buffer, cli->index)) {
                continue;
            }
            match_cmd = cmd;
            count++;
        }
    }

    cli_cmd_node_t *node = NULL;
    SLIST_HEAD *pos = NULL;

    SLIST_FOR_EACH_ENTRY(node, cli_cmd_node_t, pos, &s_cli_dynamic_table, next)
    {
        for (i = 0; i < node->table.num; i++) {
            cmd = node->table.cmd + i;
            if (0 != strncmp(cmd->name, cli->buffer, cli->index)) {
                continue;
            }
            match_cmd = cmd;
            count++;
        }
    }

    if (count > 1) { //! print more
        cli_print_cmd_title(cli);
        for (i = 0; i < CLI_CMD_TABLE_NUM; i++) {
            for (j = 0; j < s_cli_static_table[i].num; j++) {
                cmd = s_cli_static_table[i].cmd + j;
                if (0 == strncmp(cmd->name, cli->buffer, cli->index)) {
                    cli_print_cmd(cli, cmd);
                }
            }
        }
        SLIST_FOR_EACH_ENTRY(node, cli_cmd_node_t, pos, &s_cli_dynamic_table, next)
        {
            for (i = 0; i < node->table.num; i++) {
                cmd = node->table.cmd + i;
                if (0 == strncmp(cmd->name, cli->buffer, cli->index)) {
                    cli_print_cmd(cli, cmd);
                }
            }
        }
    } else if (count) { //! print one
        strcpy(cli->buffer, match_cmd->name);
        cli->index = strlen(cli->buffer);
        cli->insert = cli->index;
    } else {
        cli->insert = cli->index;
    }

    cli_print_prompt(cli);
    cli_out_put(cli->port_id, cli->buffer, cli->index);

    return OPRT_OK;
}

static int cli_histroy_data_perv(cli_t *cli, history_data_t **data)
{
    cli_history_t *history = &cli->history;

    if (history->read_count == 0) {
        return false;
    }
    if (history->read_count != history->write_count) {
        if (0 == history->read_index) {
            history->read_index = CLI_HISTORY_NUM;
        }
        history->read_index--;
    }
    history->read_count--;
    *data = (history_data_t *)history->data[history->read_index];

    return true;
}

static int cli_histroy_data_next(cli_t *cli, history_data_t **data)
{
    cli_history_t *history = &cli->history;

    if (history->write_count <= (history->read_count + 1)) {
        return false;
    }
    if (CLI_HISTORY_NUM == ++history->read_index) {
        history->read_index = 0;
    }
    history->read_count++;

    *data = (history_data_t *)history->data[history->read_index];

    return true;
}

static int cli_histroy_data_save(cli_t *cli)
{
    history_data_t *history_data = NULL;
    cli_history_t *history = &cli->history;

    history->read_count = history->write_count;
    history->read_index = history->write_index - 1;

    if (cli_histroy_data_perv(cli, &history_data)) {
        if (0 == strcmp(cli->buffer, (char *)history_data)) {
            history->read_count = history->write_count;
            history->read_index = history->write_index - 1;
            return true;
        }
    }

    if (CLI_HISTORY_NUM <= history->write_index) {
        history->write_index = 0;
    }
    history->read_index = history->write_index;
    if (CLI_HISTORY_NUM > history->write_count) {
        history->write_count++;
    }
    history->read_count = history->write_count;

    memcpy(history->data[history->write_index++], cli->buffer, cli->index + 1);

    return true;
}

static int cli_key_detect(TUYA_UART_NUM_E port_id, char *data, cli_key_t *key)
{
    char ch;
    enum {
        CHECK_KEY,
        CHECK_FUNC_TAG,
        CHECK_FUNC_KEY,
    } state = CHECK_KEY;

    for (;;) {
        tal_uart_read(port_id, (uint8_t *)&ch, 1);

        switch (state) {

        case CHECK_KEY:
            if (CLI_ENTER_KEY == ch || CLI_ENTER2_KEY == ch || CLI_BACKSPACE_KEY == ch || CLI_BACKSPACE2_KEY == ch || CLI_TABLE_KEY == ch) {
                *key = ch;
                return OPRT_OK;
            } else if (CLI_ESC_KEY == ch) {
                state = CHECK_FUNC_TAG;
            } else {
                *data = ch;
                *key = CLI_NULL_KEY;
                return OPRT_OK;
            }
            break;

        case CHECK_FUNC_TAG:
            if (CLI_FUNC_TAG_KEY == ch) {
                state = CHECK_FUNC_KEY;
            } else {
                state = CHECK_KEY;
            }
            break;

        case CHECK_FUNC_KEY:
            if (CLI_UP_KEY == ch || CLI_DOWN_KEY == ch || CLI_LETF_KEY == ch || CLI_RIGHT_KEY == ch) {
                *key = ch;
                return OPRT_OK;
            } else {
                state = CHECK_KEY;
            }
            break;
        }
    }

    return OPRT_OK;
}

static void cli_print_prompt(cli_t *cli)
{
    cli_out_put(cli->port_id, "\r\n", 2);
    cli_out_put(cli->port_id, cli->prompt, strlen(cli->prompt));
}

static int cli_parse_buffer(char *buffer, int *argc, char **argv)
{
    enum {
        PARSE_SPACE,
        PARSE_ARG,
    } state = PARSE_SPACE;
    char *readchar = buffer;
    char *writechar = buffer;

    *argc = 0;

    do {
        switch (state) {

        case PARSE_SPACE:
            if (' ' == *readchar) {
                readchar++;
            } else if ('\0' == *readchar) {
                *writechar = '\0';
                goto __exit;
            } else {
                state = PARSE_ARG;
            }
            break;

        case PARSE_ARG:
            if (' ' == *readchar) {
                *writechar++ = '\0';
                readchar++;
                (*argc)++;
                state = PARSE_SPACE;
            } else if ('\0' == *readchar) {
                *writechar = '\0';
                (*argc)++;
                goto __exit;
            } else {
                *writechar++ = *readchar++;
                ;
            }
            break;
        }
    } while (true);

__exit:

    if (CLI_ARGV_NUM < *argc) {
        *argc = CLI_ARGV_NUM;
    }

    int i;
    for (i = 0; i < *argc; i++) {
        argv[i] = buffer;
        buffer += strlen(buffer) + 1;
    }

    return OPRT_OK;
}

static int cli_cmd_exec(int argc, char **argv)
{
    cli_cmd_t *cmd;

    cmd = cli_cmd_find_with_name(argv[0]);
    if (cmd) {
        cmd->func(argc, argv);
        return OPRT_OK;
    }

    return OPRT_NOT_FOUND;
}

static void cli_enter_key(cli_t *cli)
{
    int result;

    if (0 == cli->index) {
        cli_print_prompt(cli);
        return;
    }
    cli->buffer[cli->index] = 0;
    cli_histroy_data_save(cli);
    cli_parse_buffer(cli->buffer, &cli->argc, cli->argv);
    cli_out_put(cli->port_id, "\r\n", 2);
    result = cli_cmd_exec(cli->argc, cli->argv);
    if (OPRT_OK != result) {
        cli_print_string(cli, "No command or file name");
    }
    cli_print_prompt(cli);
    cli->index = 0;
    cli->insert = 0;
    memset(cli->buffer, 0, sizeof(cli->buffer));
}

static void cli_backspace_key(cli_t *cli)
{
    char ch = '\b';

    if (0 == cli->index) {
        return;
    } else if (cli->index > cli->insert) {
        cli->index--;
        cli->insert--;
        memmove(&cli->buffer[cli->insert], &cli->buffer[cli->insert + 1], cli->index - cli->insert);
        cli->buffer[cli->index] = '\0';
        cli_out_put(cli->port_id, &ch, 1);
        cli_out_put(cli->port_id, &cli->buffer[cli->insert], cli->index - cli->insert);
        cli_out_put(cli->port_id, " \b", 2);
        int i;
        for (i = 0; i < (cli->index - cli->insert); i++) {
            cli_out_put(cli->port_id, &ch, 1);
        }
    } else {
        cli->index--;
        cli->insert--;
        cli->buffer[cli->insert] = '\0';
        cli_out_put(cli->port_id, "\b \b", 3);
    }
}

static void cli_up_key(cli_t *cli)
{
    int i;
    char ch;
    history_data_t *history_data;

    if (cli_histroy_data_perv(cli, &history_data)) {
        ch = '\r';
        cli_out_put(cli->port_id, &ch, 1);
        ch = ' ';
        for (i = 0; i < cli->index + strlen(cli->prompt); i++) {
            cli_out_put(cli->port_id, &ch, 1);
        }
        ch = '\r';
        cli_out_put(cli->port_id, &ch, 1);
        cli_out_put(cli->port_id, cli->prompt, strlen(cli->prompt));
        cli_out_put(cli->port_id, (char *)history_data, strlen((char *)history_data));
        strcpy(cli->buffer, (char *)history_data);
        cli->index = strlen(cli->buffer);
        cli->buffer[cli->index] = '\0';
        cli->insert = cli->index;
    }
}

static void cli_down_key(cli_t *cli)
{
    int i;
    char ch;
    history_data_t *history_data;

    if (cli_histroy_data_next(cli, &history_data)) {
        ch = '\r';
        cli_out_put(cli->port_id, &ch, 1);
        ch = ' ';
        for (i = 0; i < cli->index + strlen(cli->prompt); i++) {
            cli_out_put(cli->port_id, &ch, 1);
        }
        ch = '\r';
        cli_out_put(cli->port_id, &ch, 1);
        cli_out_put(cli->port_id, cli->prompt, strlen(cli->prompt));
        cli_out_put(cli->port_id, (char *)history_data, strlen((char *)history_data));
        strcpy(cli->buffer, (char *)history_data);
        cli->index = strlen(cli->buffer);
        cli->buffer[cli->index] = '\0';
        cli->insert = cli->index;
    }
}

static void cli_letf_key(cli_t *cli)
{
    char ch = '\b';

    if (cli->insert) {
        cli_out_put(cli->port_id, &ch, 1);
        cli->insert--;
    }
}

static void cli_right_key(cli_t *cli)
{
    char ch;

    if (cli->insert < cli->index) {
        ch = cli->buffer[cli->insert];
        cli_out_put(cli->port_id, &ch, 1);
        cli->insert++;
    }
}

static void cli_key_app(cli_t *cli, cli_key_t key)
{
    switch (key) {

    case CLI_TABLE_KEY:
        cli_table_key(cli);
        break;

    case CLI_ENTER_KEY:
    case CLI_ENTER2_KEY:
        cli_enter_key(cli);
        break;

    case CLI_BACKSPACE_KEY:
    case CLI_BACKSPACE2_KEY:
        cli_backspace_key(cli);
        break;

    case CLI_UP_KEY:
        cli_up_key(cli);
        break;

    case CLI_DOWN_KEY:
        cli_down_key(cli);
        break;

    case CLI_LETF_KEY:
        cli_letf_key(cli);
        break;

    case CLI_RIGHT_KEY:
        cli_right_key(cli);
        break;

    default:
        break;
    }
}

static void cli_task(void *parameter)
{
    cli_t *cli;
    char data;
    cli_key_t key;

    cli = (cli_t *)parameter;
    cli->prompt = "tuya>";
    cli_print_prompt(cli);
    cli->echo = 1;

    for (;;) {
        cli_key_detect(cli->port_id, &data, &key);
        if (CLI_NULL_KEY != key) {
            cli_key_app(cli, key);
            continue;
        }
        if (!((32 <= data) && (127 >= data))) {
            continue;
        }
        if (CLI_BUFFER_SIZE - 1 < cli->index) {
            continue;
        }
        if (cli->insert != cli->index) {
            memmove(&cli->buffer[cli->insert + 1], &cli->buffer[cli->insert], cli->index - cli->insert);
            cli->buffer[cli->insert] = data;
            cli->index++;
            cli_out_put(cli->port_id, &cli->buffer[cli->insert], cli->index - cli->insert);
            int i;
            char ch = '\b';
            cli->insert++;
            for (i = 0; i < (cli->index - cli->insert); i++) {
                cli_out_put(cli->port_id, &ch, 1);
            }
            continue;
        } else {
            cli->buffer[cli->index++] = data;
            cli->insert = cli->index;
        }
        if (cli->echo) {
            cli_out_put(cli->port_id, &data, 1);
        }
    }
}

static int cli_cmd_register(cli_cmd_t *cmd, uint8_t num)
{
    int i = 0;

    for (i = 0; i < CLI_CMD_TABLE_NUM; i++) {
        if (s_cli_static_table[i].cmd) {
            continue;
        }
        s_cli_static_table[i].cmd = cmd;
        s_cli_static_table[i].num = num;
        return OPRT_OK;
    }
    cli_cmd_node_t *node = tal_malloc(sizeof(cli_cmd_node_t));
    if (NULL == node) {
        return OPRT_MALLOC_FAILED;
    }
    node->table.cmd = cmd;
    node->table.num = num;
    tuya_slist_add_head(&s_cli_dynamic_table, &node->next);

    return OPRT_OK;
}

/**
 * @brief cli echo string
 *
 * @param[in] string context
 *
 * @return None
 *
 */
void tal_cli_echo(char *string)
{
    cli_print_string(s_cli_handle, string);
}

/**
 * @brief Registers a command in the CLI command table.
 *
 * This function registers a command in the CLI command table.
 *
 * @param cmd Pointer to the command structure to be registered.
 * @param num Number of commands to be registered.
 * @return Returns OPRT_INVALID_PARM if the cmd pointer is NULL or num is 0.
 *         Returns the result of the cli_cmd_register function otherwise.
 */
int tal_cli_cmd_register(const cli_cmd_t *cmd, uint8_t num)
{
    if (NULL == cmd || 0 == num) {
        return OPRT_INVALID_PARM;
    }

    return cli_cmd_register((cli_cmd_t *)cmd, num);
}

/**
 * @brief Initializes the CLI (Command Line Interface) with the specified UART
 * number.
 *
 * @param uart_num The UART number to be used for the CLI.
 * @return Returns OPRT_OK if the CLI is successfully initialized, otherwise
 * returns an error code.
 */
int tal_cli_init_with_uart(uint8_t uart_num)
{
    int result = OPRT_OK;

    if (s_cli_handle) {
        return OPRT_OK;
    }
    s_cli_handle = tal_malloc(sizeof(cli_t));
    if (NULL == s_cli_handle) {
        return OPRT_MALLOC_FAILED;
    }
    memset(s_cli_handle, 0, sizeof(cli_t));
    s_cli_handle->port_id = uart_num;
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = 115200;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size = 256;
    cfg.open_mode = O_BLOCK;
    result = tal_uart_init(uart_num, &cfg);
    if (OPRT_OK != result) {
        PR_ERR("uart init failed", result);
        goto __exit;
    }
    tal_cli_cmd_register((cli_cmd_t *)&s_cli_cmd, 1);

    THREAD_CFG_T param;

    param.priority = THREAD_PRIO_3;
    param.stackDepth = 3072;
    param.thrdname = "cli";

    result = tal_thread_create_and_start(&s_cli_handle->thread, NULL, NULL, cli_task, s_cli_handle, &param);
    if (OPRT_OK != result) {
        PR_ERR("tuya cli create thread failed %d", result);
        goto __exit;
    }

    return OPRT_OK;

__exit:
    tal_free(s_cli_handle);
    s_cli_handle = NULL;

    return OPRT_COM_ERROR;
}
/**
 * @brief Initializes the CLI module.
 *
 * This function initializes the CLI module by calling the
 * tal_cli_init_with_uart() function with the UART number set to
 * TUYA_UART_NUM_0.
 *
 * @return Returns OPRT_OK if the CLI is successfully initialized, otherwise
 * returns an error code.
 */
int tal_cli_init(void)
{
    return tal_cli_init_with_uart(TUYA_UART_NUM_0);
}
