/**
 * @file example_u8g2_i2c.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pinmux.h"

#include "u8g2_port.h"
#include "u8g2.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static u8g2_t u8g2;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief user_main
 *
 * @return int
 */
int user_main()
{
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


    // 3. Initialize U8g2 (Core: bind low-level adapter functions + display driver)
    u8g2_Setup_ssd1306_i2c_128x64_vcomh0_1(
        &u8g2,                  // U8g2 object
        U8G2_R0,                // Rotation direction
        u8x8_byte_tkl_i2c,      // I2C communication function (user implemented)
        u8x8_gpio_and_delay_tkl // GPIO + delay function (user implemented)
    );

	u8x8_SetPort(u8g2_GetU8x8(&u8g2), TUYA_I2C_NUM_0); // Set I2C port number

    if (OPRT_OK != u8g2_alloc_buffer(&u8g2)) {
        PR_ERR("Failed to allocate u8g2 buffer\r\n");
        return -1;
    }

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0); // Disable power save mode

    // 5. Draw test content using page mode (_1 mode requires page mode to display full screen)
    // _1 mode: Buffer is only 8 pixels high, needs 8 transmissions to display full 64-pixel high screen
    // Use u8g2_FirstPage() + u8g2_NextPage() loop to implement page mode display
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    
    u8g2_FirstPage(&u8g2);
    do {
        // Draw content on each page
        // Note: Coordinates are relative to the entire screen, u8g2 will automatically clip to current page range
        u8g2_DrawStr(&u8g2, 30, 20, "U8g2 Demo");
        u8g2_DrawStr(&u8g2, 30, 40, "Page Mode");
        u8g2_DrawStr(&u8g2, 30, 60, "I2c  Bus");
        
        //Draw a rectangular frame
        u8g2_DrawFrame(&u8g2, 1, 1, u8g2_GetDisplayWidth(&u8g2)-2, u8g2_GetDisplayHeight(&u8g2)-2);
        
    } while (u8g2_NextPage(&u8g2)); // Loop until all pages are sent

    while (1) {
        tal_system_sleep(10);
    }
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
    THREAD_CFG_T thrd_param;

    memset(&thrd_param, 0, sizeof(THREAD_CFG_T));
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";

    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif