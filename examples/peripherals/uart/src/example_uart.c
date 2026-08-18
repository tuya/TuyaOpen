/**
 * @file example_uart.c
 * @version 0.1
 * @date 2025-06-30
 */

#include "tal_api.h"

#include "tkl_output.h"
#include "tkl_pinmux.h"
#include "board_com_api.h"
#if defined(EXAMPLE_LOW_POWER) && (EXAMPLE_LOW_POWER == 1)
#include "tkl_sleep.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tkl_wifi.h"
#endif
#if defined(ENABLE_BLUETOOTH) && (ENABLE_BLUETOOTH == 1)
#include "tkl_bluetooth.h"
#endif
#endif
/***********************************************************
************************macro define************************
***********************************************************/
#define USR_UART_NUM      ((TUYA_UART_NUM_E)EXAMPLE_UART_NUM)
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

    /* The board states its own wiring here - which pads the peripherals were routed to,
     * what leds and buttons exist. Everything below can then ask for "uart N" without
     * knowing, or caring, which pins that turns out to be on this board. */
    board_register_hardware();

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

#if defined(EXAMPLE_UART_PINMUX) && (EXAMPLE_UART_PINMUX == 1)
    /* the port's default pins are not the ones this board wired up */
    tkl_io_pinmux_config(EXAMPLE_UART_RX_PIN, TUYA_UART0_RX + EXAMPLE_UART_NUM * 4);
    tkl_io_pinmux_config(EXAMPLE_UART_TX_PIN, TUYA_UART0_TX + EXAMPLE_UART_NUM * 4);
#endif

#if defined(EXAMPLE_LOW_POWER) && (EXAMPLE_LOW_POWER == 1)
    /* Ahead of tal_uart_init() on purpose. On a board whose log shares a port with the one
     * this example opens, anything printed after that call lands in the middle of the user
     * data - so all of this, prints included, has to happen first. Nothing here depends on
     * the port being open.
     *
     * The read further down blocks, so the core is idle between bytes and free to sleep.
     * Getting it to actually do so takes two unrelated things, and skipping either one leaves
     * the sleep request silently ignored with nothing under test:
     *
     *   - a radio stack that is up registers a claim on the cpu for its whole lifetime, so an
     *     application with no use for it has to hand it back explicitly
     *   - some platforms additionally gate cpu sleep behind a wifi low power flag that only
     *     tkl_wifi_set_lp_mode() sets, and only for a dtim of 10, 20 or 30
     *
     * Each is harmless where it is not needed, so ask for both. */
#if defined(ENABLE_BLUETOOTH) && (ENABLE_BLUETOOTH == 1)
    PR_NOTICE("ble  deinit -> %d", tkl_ble_stack_deinit(0));
#endif
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    PR_NOTICE("wifi lp     -> %d", tkl_wifi_set_lp_mode(TRUE, 10));
    PR_NOTICE("wifi down   -> %d", tkl_wifi_set_work_mode(WWM_POWERDOWN));
#endif
    PR_NOTICE("cpu sleep   -> %d", tkl_cpu_sleep_mode_set(TRUE, TUYA_CPU_SLEEP));
#endif

    /* uart init */
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = EXAMPLE_UART_BAUDRATE;
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