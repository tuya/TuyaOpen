/**
 * @file example_qspi.c
 * @brief QSPI driver example - drives the bus so the transaction can be read off a scope.
 *
 * A QSPI transfer is not one stream of bits like plain SPI. It is split into phases - command,
 * address, dummy cycles, data - and each phase can independently use one, two or four data
 * lines. That is the whole point of the controller, and it is also what makes it easy to get
 * subtly wrong: a transfer can look fine byte-for-byte and still put the address out on the
 * wrong number of lines, or insert the wrong number of dummy cycles, and the flash on the other
 * end will simply return garbage.
 *
 * This example issues three transactions that are easy to tell apart on a logic analyser, so
 * the phases can be checked one at a time rather than all at once:
 *
 *   1. command only        - one byte, nothing else. Marks where a transaction starts.
 *   2. command + address + data, all on one line
 *   3. command + address on one line, data on four   - the usual shape for a quad flash write
 *
 * With no memory attached there is nothing to read back, so what this proves is framing and
 * timing: chip select brackets the whole transaction, the clock only runs inside it, and each
 * phase carries the bytes it should on the lines it should. Function - that a real device
 * understands what it is being told - needs a real device.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_qspi.h"
#include "board_com_api.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define TASK_QSPI_PRIORITY  THREAD_PRIO_2
#define TASK_QSPI_SIZE      4096

#define QSPI_PORT           ((TUYA_QSPI_NUM_E)EXAMPLE_QSPI_PORT)

/* 0x9F is "read jedec id" on just about every serial flash. Nothing here reads the answer -
 * it is used because it is one byte, universally recognised, and easy to spot on a trace. */
#define CMD_READ_ID         0x9F
/* 0x02 "page program", the command a quad write normally carries */
#define CMD_PAGE_PROGRAM    0x02

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_qspi_handle;

/* A pattern rather than text: alternating bits make the bit order and the line count obvious
 * at a glance, and 0x00/0xFF at the ends make the phase boundaries easy to find. */
static uint8_t sg_pattern[] = {0x00, 0xA5, 0x5A, 0xF0, 0x0F, 0xCC, 0x33, 0xFF};

/***********************************************************
***********************function define**********************
***********************************************************/

static const char *__wire_name(TUYA_QSPI_WIRE_MODE_E m)
{
    switch (m) {
    case TUYA_QSPI_1WIRE: return "1-wire";
    case TUYA_QSPI_2WIRE: return "2-wire";
    case TUYA_QSPI_4WIRE: return "4-wire";
    default:              return "?";
    }
}

/**
 * @brief issue one transaction and report what came back
 *
 * Every field of TUYA_QSPI_CMD_T that is left at zero switches its phase off, so the same
 * call covers "command only" and "command, address and data" without special cases.
 */
static void __qspi_do(const char *what, uint8_t cmd, uint32_t addr, uint8_t addr_size,
                      uint8_t *data, uint32_t data_size, TUYA_QSPI_WIRE_MODE_E data_lines)
{
    TUYA_QSPI_CMD_T c = {0};
    OPERATE_RET rt;

    c.op         = TUYA_QSPI_WRITE;
    c.cmd[0]     = cmd;
    c.cmd_size   = 1;
    c.cmd_lines  = TUYA_QSPI_1WIRE;

    if (addr_size) {
        /* most significant byte first, the order every serial flash expects */
        c.addr[0]   = (uint8_t)(addr >> 16);
        c.addr[1]   = (uint8_t)(addr >> 8);
        c.addr[2]   = (uint8_t)(addr);
        c.addr_size = addr_size;
        c.addr_lines = TUYA_QSPI_1WIRE;
    }

    if (data_size) {
        c.data       = data;
        c.data_size  = data_size;
        c.data_lines = data_lines;
    }

    rt = tkl_qspi_comand(QSPI_PORT, &c);
    PR_NOTICE("%-28s cmd 0x%02X, addr %u byte, data %u byte on %s -> %d",
              what, cmd, addr_size, data_size, __wire_name(data_lines), rt);
}

static void __example_qspi_task(void *param)
{
    OPERATE_RET rt = OPRT_OK;

    (void)param;

    TUYA_QSPI_BASE_CFG_T cfg = {
        .role    = TUYA_QSPI_ROLE_MASTER,
        .mode    = TUYA_QSPI_MODE0,
        .freq_hz = EXAMPLE_QSPI_FREQ_HZ,
        .use_dma = FALSE,
        .type    = TUYA_QSPI_TYPE_FLASH,
    };

    TUYA_CALL_ERR_GOTO(tkl_qspi_init(QSPI_PORT, &cfg), __EXIT);
    PR_NOTICE("qspi %d init at %d Hz", EXAMPLE_QSPI_PORT, EXAMPLE_QSPI_FREQ_HZ);

    while (1) {
        /* one byte and nothing else - the shortest transaction the controller can produce,
         * so it is unmistakable on a trace and marks where a round starts */
        __qspi_do("cmd only", CMD_READ_ID, 0, 0, NULL, 0, TUYA_QSPI_1WIRE);
        tal_system_sleep(50);

        /* all three phases on a single line, i.e. an ordinary spi transfer with structure */
        __qspi_do("cmd + addr + data, 1-wire", CMD_PAGE_PROGRAM, 0x123456, 3,
                  sg_pattern, sizeof(sg_pattern), TUYA_QSPI_1WIRE);
        tal_system_sleep(50);

#if defined(EXAMPLE_QSPI_QUAD) && (EXAMPLE_QSPI_QUAD == 1)
        /* command and address stay on one line, only the payload spreads over four - which is
         * how a quad flash write actually looks, and the case worth checking on a trace */
        __qspi_do("cmd + addr 1-wire, data 4", CMD_PAGE_PROGRAM, 0x123456, 3,
                  sg_pattern, sizeof(sg_pattern), TUYA_QSPI_4WIRE);
        tal_system_sleep(50);
#endif

        tal_system_sleep(1000);
    }

__EXIT:
    PR_NOTICE("QSPI task is finished, will delete");
    tal_thread_delete(sg_qspi_handle);
    return;
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /* The board states its own wiring here, so nothing below has to name a pin. */
    board_register_hardware();

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    THREAD_CFG_T qspi_param = {0};
    qspi_param.stackDepth = TASK_QSPI_SIZE;
    qspi_param.priority   = TASK_QSPI_PRIORITY;
    qspi_param.thrdname   = "qspi_task";
    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&sg_qspi_handle, NULL, NULL, __example_qspi_task, NULL, &qspi_param));

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

void tuya_app_main(void)
{
    user_main();
}
#endif
