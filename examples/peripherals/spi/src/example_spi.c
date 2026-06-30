/**
 * @file example_spi.c
 * @brief SPI loopback test example.
 *
 * Short MOSI and MISO with a jumper wire; the example sends "Hello Tuya"
 * and verifies the received data matches. This tests the full SPI data path
 * including pinmux, DMA, and the tkl_spi abstraction layer.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_spi.h"
#include "tkl_pinmux.h"

#include <string.h>

/***********************************************************
*************************micro define***********************
***********************************************************/
#ifndef EXAMPLE_SPI_MOSI
#define EXAMPLE_SPI_MOSI 19
#endif

#ifndef EXAMPLE_SPI_MISO
#define EXAMPLE_SPI_MISO 20
#endif

#ifndef EXAMPLE_SPI_SCLK
#define EXAMPLE_SPI_SCLK 12
#endif

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
    uint8_t send_buff[] = {"Hello Tuya"};
    uint8_t recv_buff[sizeof(send_buff)];

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("SPI loopback test");
    PR_NOTICE("Short GPIO%d (MOSI) to GPIO%d (MISO) with a jumper wire",
              EXAMPLE_SPI_MOSI, EXAMPLE_SPI_MISO);

    /* configure pinmux for SPI pins */
    tkl_io_pinmux_config(EXAMPLE_SPI_MOSI, TUYA_SPI0_MOSI);
    tkl_io_pinmux_config(EXAMPLE_SPI_MISO, TUYA_SPI0_MISO);
    tkl_io_pinmux_config(EXAMPLE_SPI_SCLK, TUYA_SPI0_CLK);

    /* spi init */
    TUYA_SPI_BASE_CFG_T spi_cfg = {.mode = TUYA_SPI_MODE0,
                                   .freq_hz = EXAMPLE_SPI_BAUDRATE,
                                   .databits = TUYA_SPI_DATA_BIT8,
                                   .bitorder = TUYA_SPI_ORDER_MSB2LSB,
                                   .role = TUYA_SPI_ROLE_MASTER,
                                   .type = TUYA_SPI_AUTO_TYPE};
    TUYA_CALL_ERR_GOTO(tkl_spi_init(EXAMPLE_SPI_PORT, &spi_cfg), __EXIT);

    while (1) {
        memset(recv_buff, 0, sizeof(recv_buff));

        rt = tkl_spi_transfer(EXAMPLE_SPI_PORT, send_buff, recv_buff, sizeof(send_buff));
        if (rt != OPRT_OK) {
            PR_ERR("spi transfer failed: %d", rt);
        } else if (memcmp(send_buff, recv_buff, sizeof(send_buff)) == 0) {
            PR_NOTICE("LOOPBACK OK: sent=\"%s\" recv=\"%s\"", send_buff, recv_buff);
        } else {
            PR_ERR("LOOPBACK FAIL: sent=\"%s\" recv=\"%s\"", send_buff, recv_buff);
        }

        tal_system_sleep(1000);
    }

__EXIT:
    PR_ERR("example spi error code: %d", rt);
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
