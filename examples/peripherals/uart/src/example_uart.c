/**
 * @file example_uart.c
 * @version 0.1
 * @date 2025-06-30
 */

#include "tal_api.h"

#include "tkl_output.h"
#include "tkl_pinmux.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define USR_UART_NUM      TUYA_UART_NUM_0
#define READ_BUFFER_SIZE  1024
#define START_TEXT       "Please input text: \r\n" 

#define SCANF_ENTER_KEY   '\r'
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static char sg_read_buffer[READ_BUFFER_SIZE];
// static uint32_t sg_read_index = 0;
/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    int read_len = 0;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

#if USR_UART_NUM == TUYA_UART_NUM_2
    /* UART2 pinmux support PIN30, 31 and PIN40, 41*/
    tkl_io_pinmux_config(TUYA_IO_PIN_40, TUYA_UART2_RX);
    tkl_io_pinmux_config(TUYA_IO_PIN_41, TUYA_UART2_TX);
#endif

    /* UART 0 init */
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = 115200;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size = 256;
    cfg.open_mode = O_BLOCK;
    TUYA_CALL_ERR_GOTO(tal_uart_init(USR_UART_NUM, &cfg), __EXIT);

    tal_uart_write(USR_UART_NUM, (const uint8_t*)START_TEXT, sizeof(START_TEXT));

    while(1) {
        read_len = tal_uart_read(USR_UART_NUM, (uint8_t *)sg_read_buffer, READ_BUFFER_SIZE);
        if(read_len <= 0) {
            tal_system_sleep(10);
            continue;
        }

        tal_uart_write(USR_UART_NUM, (const uint8_t*)sg_read_buffer, read_len);
        tal_system_sleep(10);
    }


__EXIT:
    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif