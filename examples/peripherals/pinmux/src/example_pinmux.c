/**
 * @file example_pinmux.c
 * @brief Board-independent TKL pinmux example.
 *
 * The selected pins and I2C port come from Kconfig. This example does not
 * initialize I2C or access a board peripheral, so it can be reused by every
 * platform that provides the TKL pinmux interface.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pinmux.h"
#include "tuya_cloud_types.h"

static void __run_pinmux_example(void)
{
    TUYA_MUL_PIN_CFG_T pins[] = {
        {
            .pin = (TUYA_PIN_NAME_E)EXAMPLE_PINMUX_I2C_SCL_PIN,
            .pin_func = (TUYA_PIN_FUNC_E)(TUYA_IIC0_SCL + EXAMPLE_PINMUX_I2C_PORT * 2U),
        },
        {
            .pin = (TUYA_PIN_NAME_E)EXAMPLE_PINMUX_I2C_SDA_PIN,
            .pin_func = (TUYA_PIN_FUNC_E)(TUYA_IIC0_SDA + EXAMPLE_PINMUX_I2C_PORT * 2U),
        },
    };
    OPERATE_RET rt;

    PR_NOTICE("Configure I2C%d: SCL=GPIO%d, SDA=GPIO%d", EXAMPLE_PINMUX_I2C_PORT,
              EXAMPLE_PINMUX_I2C_SCL_PIN, EXAMPLE_PINMUX_I2C_SDA_PIN);
    rt = tkl_multi_io_pinmux_config(pins, sizeof(pins) / sizeof(pins[0]));
    if (rt == OPRT_OK) {
        PR_NOTICE("I2C pinmux configured");
    } else if (rt == OPRT_NOT_SUPPORTED) {
        PR_NOTICE("I2C pinmux is not supported by this platform");
    } else {
        PR_ERR("I2C pinmux failed (%d)", rt);
    }
}

void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("TKL pinmux example starts");
    __run_pinmux_example();
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
}
#else
static THREAD_HANDLE sg_app_thread;

static void __app_task(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(sg_app_thread);
    sg_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_1,
        .thrdname = "pinmux_test",
    };
    (void)tal_thread_create_and_start(&sg_app_thread, NULL, NULL, __app_task, NULL, &cfg);
}
#endif
