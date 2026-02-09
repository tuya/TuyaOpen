/**
 * @file example_u8g2_disp.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pinmux.h"

#include "board_com_api.h"

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
    OPERATE_RET rt = OPRT_OK;

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

    TUYA_CALL_ERR_RETURN(board_register_hardware());

    TUYA_CALL_ERR_RETURN(u8g2_Setup_tdl_display_f(&u8g2, DISPLAY_NAME));

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0); // Disable power save mode

    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    
    u8g2_DrawStr(&u8g2, 30, 20, "U8g2 Demo");
    u8g2_DrawStr(&u8g2, 30, 40, "Full Mode");
    u8g2_DrawStr(&u8g2, 30, 60, "Tdl  Disp");

    //Draw a rectangular frame
    u8g2_DrawFrame(&u8g2, 1, 1, u8g2_GetDisplayWidth(&u8g2)-2, u8g2_GetDisplayHeight(&u8g2)-2);

    u8g2_SendBuffer(&u8g2); // Send buffer to the display
        
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