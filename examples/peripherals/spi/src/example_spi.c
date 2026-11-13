/**
 * @file example_spi.c
 * @brief SPI blocking and interrupt-mode example.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */

#include <string.h>

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_spi.h"
#include "board_com_api.h"

#define SPI_TRANSFER_TIMEOUT_MS 1000U

#if EXAMPLE_SPI_IRQ_ENABLE
static volatile uint32_t s_spi_irq_done;
static volatile TUYA_SPI_IRQ_EVT_E s_spi_irq_event;

static void __spi_irq_callback(TUYA_SPI_NUM_E port, TUYA_SPI_IRQ_EVT_E event)
{
    if ((uint32_t)port == (uint32_t)EXAMPLE_SPI_PORT) {
        s_spi_irq_event = event;
        s_spi_irq_done = 1U;
    }
}

static void __spi_irq_prepare(void)
{
    s_spi_irq_done = 0U;
    s_spi_irq_event = (TUYA_SPI_IRQ_EVT_E)UINT32_MAX;
}

static OPERATE_RET __spi_irq_wait(TUYA_SPI_IRQ_EVT_E expected)
{
    uint32_t start_ms = (uint32_t)tal_system_get_millisecond();

    while (s_spi_irq_done == 0U) {
        if ((uint32_t)tal_system_get_millisecond() - start_ms >= SPI_TRANSFER_TIMEOUT_MS) {
            (void)tkl_spi_abort_transfer(EXAMPLE_SPI_PORT);
            return OPRT_TIMEOUT;
        }
        tal_system_sleep(1U);
    }

    if (s_spi_irq_event != expected) {
        PR_ERR("spi irq event mismatch: expected=%u actual=%u",
               (unsigned int)expected, (unsigned int)s_spi_irq_event);
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}
#endif

static OPERATE_RET __check_data(const char *operation, const uint8_t *expected,
                                const uint8_t *actual, uint32_t length)
{
    if (memcmp(expected, actual, length) == 0) {
        PR_NOTICE("spi %s PASS, length=%u", operation, (unsigned int)length);
        return OPRT_OK;
    }

    for (uint32_t i = 0U; i < length; ++i) {
        if (expected[i] != actual[i]) {
            PR_ERR("spi %s mismatch at index %u: expected=0x%02X actual=0x%02X",
                   operation, (unsigned int)i, (unsigned int)expected[i],
                   (unsigned int)actual[i]);
            break;
        }
    }
    return OPRT_COM_ERROR;
}

void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t send_buff[16] = "Hello Tuya SPI!";
    uint8_t transfer_rx[sizeof(send_buff)];
    uint8_t recv_buff[sizeof(send_buff)];

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    TUYA_CALL_ERR_LOG(board_register_hardware());

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    if ((uint32_t)EXAMPLE_SPI_PORT >= (uint32_t)TUYA_SPI_NUM_MAX) {
        rt = OPRT_INVALID_PARM;
        PR_ERR("invalid SPI port: %d", EXAMPLE_SPI_PORT);
        goto __EXIT;
    }

    TUYA_SPI_BASE_CFG_T spi_cfg = {
        .role = TUYA_SPI_ROLE_MASTER,
        .mode = TUYA_SPI_MODE0,
        .type = TUYA_SPI_AUTO_TYPE,
        .databits = TUYA_SPI_DATA_BIT8,
        .bitorder = TUYA_SPI_ORDER_MSB2LSB,
        .freq_hz = EXAMPLE_SPI_BAUDRATE,
        .spi_dma_flags = EXAMPLE_SPI_DMA ? 1U : 0U,
    };
    TUYA_CALL_ERR_GOTO(tkl_spi_init(EXAMPLE_SPI_PORT, &spi_cfg), __EXIT);

#if EXAMPLE_SPI_IRQ_ENABLE
    TUYA_CALL_ERR_GOTO(tkl_spi_irq_init(EXAMPLE_SPI_PORT, __spi_irq_callback), __EXIT);
    TUYA_CALL_ERR_GOTO(tkl_spi_irq_enable(EXAMPLE_SPI_PORT), __EXIT);
#endif

    while (1) {
#if EXAMPLE_SPI_IRQ_ENABLE
        __spi_irq_prepare();
#endif
        rt = tkl_spi_transfer(EXAMPLE_SPI_PORT, send_buff, transfer_rx, sizeof(send_buff));
#if EXAMPLE_SPI_IRQ_ENABLE
        if (rt == OPRT_OK) {
            rt = __spi_irq_wait(TUYA_SPI_EVENT_TRANSFER_COMPLETE);
        }
#endif
        if (rt == OPRT_OK) {
            rt = __check_data("transfer loopback", send_buff, transfer_rx, sizeof(send_buff));
        } else {
            PR_ERR("spi transfer failed, error code: %d", rt);
        }

#if EXAMPLE_SPI_IRQ_ENABLE
        __spi_irq_prepare();
#endif
        rt = tkl_spi_send(EXAMPLE_SPI_PORT, send_buff, sizeof(send_buff));
#if EXAMPLE_SPI_IRQ_ENABLE
        if (rt == OPRT_OK) {
            rt = __spi_irq_wait(TUYA_SPI_EVENT_TX_COMPLETE);
        }
#endif
        if (rt == OPRT_OK) {
            PR_NOTICE("spi send PASS, length=%u", (unsigned int)sizeof(send_buff));
        } else {
            PR_ERR("spi send failed, error code: %d", rt);
        }

#if EXAMPLE_SPI_IRQ_ENABLE
        __spi_irq_prepare();
#endif
        rt = tkl_spi_recv(EXAMPLE_SPI_PORT, recv_buff, sizeof(recv_buff));
#if EXAMPLE_SPI_IRQ_ENABLE
        if (rt == OPRT_OK) {
            rt = __spi_irq_wait(TUYA_SPI_EVENT_RX_COMPLETE);
        }
#endif
        if (rt == OPRT_OK) {
            PR_NOTICE("spi receive complete, length=%u, first=0x%02X",
                      (unsigned int)sizeof(recv_buff), (unsigned int)recv_buff[0]);
        } else {
            PR_ERR("spi receive failed, error code: %d", rt);
        }

        tal_system_sleep(500);
    }

__EXIT:
    PR_ERR("example spi error code: %d", rt);
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
}
#else
static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    (void)arg;
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
