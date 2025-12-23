/**
 * @file nfc_emulate_main.c
 * @brief NFC Forum Tag Type 2 Emulation - Main entry point for Tuya platform
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"
#include "tkl_system.h"

#define BOARD_BUTTON_PIN_UP    TUYA_GPIO_NUM_22
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

static uint16_t      count            = 0;
static THREAD_HANDLE example_thrd_hdl = NULL;
static void          __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        count++;
        PR_NOTICE("%s: single click, count=%d", name, count);
    } break;

    default:
        break;
    }
}

// Forward declarations
extern int nfc_emulate_forum_tag2_main(char *uri, char *aar_package); // URL demo
extern int nfc_emulate_wifi_config_main(char *ssid, char *password);  // WiFi config demo

static void example_task(void *args)
{
    while (1) {
        switch (count % 4) {
        case 0:
            nfc_emulate_forum_tag2_main("https://tuyaopen.ai", "com.android.browser");
            PR_DEBUG("Demo: URL - https://tuyaopen.ai with AAR com.android.browser");
            break;

        case 1:
            nfc_emulate_forum_tag2_main("tel:+86123456789", NULL);
            PR_DEBUG("Demo: URL - tel:+86123456789 without AAR");
            break;

        case 2:
            nfc_emulate_forum_tag2_main("mailto:example@example.com", NULL);
            PR_DEBUG("Demo: URL - mailto:example@example.com without AAR");
            break;

        case 3:
            nfc_emulate_wifi_config_main("TuyaOpen", "12345678");
            PR_DEBUG("Demo: WiFi Configuration");
            break;

        default:
            break;
        }
        tal_system_sleep(20);
    }
}
/**
 * @brief user_main - NFC emulation entry point
 */
void user_main(void)
{
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("====================================");
    PR_NOTICE("NFC Forum Tag Type 2 Emulation Demo");
    PR_NOTICE("====================================");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s %s", __DATE__, __TIME__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("====================================");

    BUTTON_GPIO_CFG_T button_hw_cfg;

    memset(&button_hw_cfg, 0, sizeof(BUTTON_GPIO_CFG_T));

    button_hw_cfg.pin                = BOARD_BUTTON_PIN_UP;
    button_hw_cfg.level              = BOARD_BUTTON_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg);
    // button create
    TDL_BUTTON_CFG_T  button_cfg = {.long_start_valid_time     = 3000,
                                    .long_keep_timer           = 1000,
                                    .button_debounce_time      = 50,
                                    .button_repeat_valid_count = 2,
                                    .button_repeat_valid_time  = 500};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    tdl_button_create(BUTTON_NAME, &button_cfg, &button_hdl);

    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_DOWN, __button_function_cb);

    /* thread create and start */
    const THREAD_CFG_T thread_cfg = {
        .thrdname   = "example_task",
        .stackDepth = 8192,
        .priority   = THREAD_PRIO_2,
    };
    tal_thread_create_and_start(&example_thrd_hdl, NULL, NULL, example_task, NULL, &thread_cfg);
}

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
 * @brief  NFC task thread
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {8192, 4, "nfc_emulate"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
