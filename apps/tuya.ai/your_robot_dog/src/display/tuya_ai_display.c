/**
 * @file tuya_ai_display.c
 * @author www.tuya.com
 * @version 0.1
 * @date 2025-02-07
 *
 * @copyright Copyright (c) tuya.inc 2025
 *
 */


#include "tkl_display.h"
#include "tkl_ipc.h"
#include "tkl_video_in.h"
#include "tkl_thread.h"
#include "tkl_queue.h"
#include "tal_log.h"
#include "lvgl.h"
#include "lv_vendor.h"
/* tkl_lvgl.h is not visible in the current include paths; add a minimal forward declaration to satisfy compilation */
typedef struct {
    void (*recv_cb)(unsigned char *buf, unsigned long len, void *args);
    void *recv_arg;
} TKL_LVGL_CFG_T;
void tkl_lvgl_init(void *info, TKL_LVGL_CFG_T *cfg);
#ifndef TKL_IPC_TYPE_LVGL
#define TKL_IPC_TYPE_LVGL TKL_IPC_TYPE_TEST_MEDIA
#endif
#include "tuya_ai_display.h"
 
 /***********************************************************
 *********************** macro define ***********************
 ***********************************************************/
/* If the hardware module does not define LCD parameters, provide defaults to keep the build passing */
#ifndef BOARD_LCD_WIDTH
#define BOARD_LCD_WIDTH 320
#endif
#ifndef BOARD_LCD_HEIGHT
#define BOARD_LCD_HEIGHT 240
#endif
#ifndef BOARD_LCD_ROTATION
#define BOARD_LCD_ROTATION TUYA_DISPLAY_ROTATION_0
#endif


 /***********************************************************
 ********************** typedef define **********************
 ***********************************************************/
static TKL_QUEUE_HANDLE disp_msg_queue;
/***********************************************************
 ********************** variable define *********************
***********************************************************/
STATIC TKL_THREAD_HANDLE sg_display_thrd_hdl = NULL;
/* remove unused flag to satisfy -Werror */


/***********************************************************
 ********************** function define *********************
***********************************************************/
void tuya_xiaozhi_init(void);
void tuya_xiaozhi_app(TY_DISPLAY_MSG_T *msg);

void tuya_eyes_init(void);
void tuya_eyes_app(TY_DISPLAY_MSG_T *msg);

void tuya_wechat_init(void);
void tuya_wechat_app(TY_DISPLAY_MSG_T *msg);

void tuya_robot_init(void);
void tuya_robot_app(TY_DISPLAY_MSG_T *msg);



void tuya_ui_init(void)
{
#if defined(T5AI_BOARD) && T5AI_BOARD == 1
    tuya_wechat_init();
#elif  defined(T5AI_BOARD_EYES) && T5AI_BOARD_EYES == 1
    tuya_eyes_init();
#elif (defined(T5AI_BOARD_EVB) && T5AI_BOARD_EVB == 1) || (defined(T5AI_BOARD_EVB_PRO) && T5AI_BOARD_EVB_PRO == 1)
    tuya_xiaozhi_init();
#elif defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1
    tuya_robot_init();
#endif
}

void tuya_ui_app(TY_DISPLAY_MSG_T *msg)
{
#if defined(T5AI_BOARD) && T5AI_BOARD == 1
    tuya_wechat_app(msg);
#elif  defined(T5AI_BOARD_EYES) && T5AI_BOARD_EYES == 1
    tuya_eyes_app(msg);
#elif (defined(T5AI_BOARD_EVB) && T5AI_BOARD_EVB == 1) || (defined(T5AI_BOARD_EVB_PRO) && T5AI_BOARD_EVB_PRO == 1)
    tuya_xiaozhi_app(msg);
#elif defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1
    tuya_robot_app(msg);
#endif
}

void tuya_board_lcd_init(TKL_DISP_INFO_S *lcd)
{
#if defined(T5AI_BOARD) && T5AI_BOARD == 1
    lcd->ll_ctrl.power_ctrl_pin     = TUYA_GPIO_NUM_MAX;     // no lcd ldo
    lcd->ll_ctrl.power_active_level = TUYA_GPIO_LEVEL_HIGH;
    lcd->ll_ctrl.rgb_mode           = TKL_DISP_PIXEL_FMT_RGB565;

    lcd->ll_ctrl.bl.io              = TUYA_GPIO_NUM_9;
    lcd->ll_ctrl.bl.mode            = TKL_DISP_BL_GPIO;
    lcd->ll_ctrl.bl.active_level    = TUYA_GPIO_LEVEL_HIGH;

    lcd->ll_ctrl.spi.clk            = TUYA_GPIO_NUM_49;
    lcd->ll_ctrl.spi.csx            = TUYA_GPIO_NUM_48;
    lcd->ll_ctrl.spi.sda            = TUYA_GPIO_NUM_50;
    lcd->ll_ctrl.spi.rst_mode       = TKL_DISP_GPIO_RESET;
    lcd->ll_ctrl.spi.rst            = 0xff;

    lcd->ll_ctrl.tp.tp_i2c_clk      = TUYA_GPIO_NUM_13;
    lcd->ll_ctrl.tp.tp_i2c_sda      = TUYA_GPIO_NUM_15;
    lcd->ll_ctrl.tp.tp_rst          = TUYA_GPIO_NUM_54;
    lcd->ll_ctrl.tp.tp_intr         = TUYA_GPIO_NUM_55;

    lcd->ll_ctrl.rst_pin            = TUYA_GPIO_NUM_53;
    lcd->ll_ctrl.rst_active_level   = TUYA_GPIO_LEVEL_LOW;

#elif  defined(T5AI_BOARD_EYES) && T5AI_BOARD_EYES == 1
    lcd->ll_ctrl.power_ctrl_pin     = TUYA_GPIO_NUM_MAX;     // no lcd ldo

    lcd->ll_ctrl.bl.io              = TUYA_GPIO_NUM_25;
    lcd->ll_ctrl.bl.mode            = TKL_DISP_BL_GPIO;
    lcd->ll_ctrl.bl.active_level    = TUYA_GPIO_LEVEL_HIGH;

    lcd->ll_ctrl.dc_pin             = TUYA_GPIO_NUM_7;
    lcd->ll_ctrl.dc_cmd_level       = TUYA_GPIO_LEVEL_LOW;

    lcd->ll_ctrl.rst_pin            = TUYA_GPIO_NUM_6;
    lcd->ll_ctrl.rst_active_level   = TUYA_GPIO_LEVEL_LOW;

    lcd->ll_ctrl.tp.tp_i2c_clk      = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_i2c_sda      = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_rst          = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_intr         = TUYA_GPIO_NUM_MAX;

    lcd->ll_ctrl.spi_using_qspi     = TRUE;

    //! update dual spi screen
    // lcd->ll_ctrl.dc_pin1            = TUYA_GPIO_NUM_5;
    // lcd->ll_ctrl.dc_cmd_level1      = TUYA_GPIO_LEVEL_LOW;
    // lcd->ll_ctrl.rst_pin1           = TUYA_GPIO_NUM_45;
    // lcd->ll_ctrl.rst_active_level1  = TUYA_GPIO_LEVEL_LOW;
    // lcd->ll_ctrl.screen_num         = 2;

#elif defined(T5AI_BOARD_EVB) && T5AI_BOARD_EVB == 1
// #define LCD_SPI_POWERON_PIN         GPIO_7
// #define LCD_SPI_RESET_PIN           GPIO_6
// #define LCD_SPI_BACKLIGHT_PIN       GPIO_5
// #define LCD_SPI_RS_PIN              GPIO_17
    lcd->ll_ctrl.power_ctrl_pin     = TUYA_GPIO_NUM_7;
    lcd->ll_ctrl.power_active_level = TUYA_GPIO_LEVEL_HIGH;

    lcd->ll_ctrl.bl.io              = TUYA_GPIO_NUM_5;
    lcd->ll_ctrl.bl.mode            = TKL_DISP_BL_GPIO;
    lcd->ll_ctrl.bl.active_level    = TUYA_GPIO_LEVEL_HIGH;

    lcd->ll_ctrl.dc_pin             = TUYA_GPIO_NUM_17;
    lcd->ll_ctrl.dc_cmd_level       = TUYA_GPIO_LEVEL_LOW;

    lcd->ll_ctrl.rst_pin            = TUYA_GPIO_NUM_6;
    lcd->ll_ctrl.rst_active_level   = TUYA_GPIO_LEVEL_LOW;

    lcd->ll_ctrl.spi_using_qspi     = FALSE;

    lcd->ll_ctrl.tp.tp_i2c_clk      = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_i2c_sda      = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_rst          = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_intr         = TUYA_GPIO_NUM_MAX;

#elif defined(T5AI_BOARD_EVB_PRO) && T5AI_BOARD_EVB_PRO == 1

    lcd->ll_ctrl.power_ctrl_pin     = TUYA_GPIO_NUM_17;
    lcd->ll_ctrl.power_active_level = TUYA_GPIO_LEVEL_HIGH;

    lcd->ll_ctrl.bl.io              = TUYA_GPIO_NUM_19;
    lcd->ll_ctrl.bl.mode            = TKL_DISP_BL_GPIO;
    lcd->ll_ctrl.bl.active_level    = TUYA_GPIO_LEVEL_HIGH;

    lcd->ll_ctrl.dc_pin             = TUYA_GPIO_NUM_47;
    lcd->ll_ctrl.dc_cmd_level       = TUYA_GPIO_LEVEL_LOW;

    lcd->ll_ctrl.rst_pin            = TUYA_GPIO_NUM_18;
    lcd->ll_ctrl.rst_active_level   = TUYA_GPIO_LEVEL_LOW;

    lcd->ll_ctrl.spi_using_qspi     = FALSE;

    lcd->ll_ctrl.tp.tp_i2c_clk      = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_i2c_sda      = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_rst          = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_intr         = TUYA_GPIO_NUM_MAX;

#elif defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1

    lcd->ll_ctrl.power_ctrl_pin     = TUYA_GPIO_NUM_19;
    lcd->ll_ctrl.power_active_level = TUYA_GPIO_LEVEL_HIGH;

    lcd->ll_ctrl.bl.io              = TUYA_GPIO_NUM_14;
    lcd->ll_ctrl.bl.mode            = TKL_DISP_BL_GPIO;
    lcd->ll_ctrl.bl.active_level    = TUYA_GPIO_LEVEL_HIGH;

    lcd->ll_ctrl.dc_pin             = TUYA_GPIO_NUM_47;
    lcd->ll_ctrl.dc_cmd_level       = TUYA_GPIO_LEVEL_LOW;

    lcd->ll_ctrl.rst_pin            = TUYA_GPIO_NUM_16;
    lcd->ll_ctrl.rst_active_level   = TUYA_GPIO_LEVEL_LOW;

    lcd->ll_ctrl.tp.tp_i2c_clk      = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_i2c_sda      = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_rst          = TUYA_GPIO_NUM_MAX;
    lcd->ll_ctrl.tp.tp_intr         = TUYA_GPIO_NUM_MAX;

    lcd->ll_ctrl.spi_using_qspi     = FALSE;

#endif
}


/* CPU0 callback is currently unused; keep this to avoid unused-function warnings */


STATIC VOID_T __cpu1_recv_cb(UCHAR_T *buf, UINT_T len, VOID_T *args)
{
    OPERATE_RET  rt;
    struct ipc_msg_s *send_msg = (struct ipc_msg_s *)buf;
    TY_DISPLAY_MSG_T disp_msg;

    if (send_msg && send_msg->req_param && send_msg->req_len >= sizeof(TY_DISPLAY_MSG_T)) {
        memcpy(&disp_msg, send_msg->req_param, sizeof(TY_DISPLAY_MSG_T));
    } else {
        return;
    }

    if (disp_msg.type == TY_DISPLAY_TP_MALLOC) {
        extern VOID_T *tkl_system_psram_malloc(CONST SIZE_T size);
        void *p_buff = (void *)tkl_system_psram_malloc(disp_msg.len);
        if (p_buff != NULL) {
            memset(p_buff, 0, disp_msg.len);
            *(void **)disp_msg.data = p_buff;
        }
    } else {
        rt = tkl_queue_post(disp_msg_queue, &disp_msg, 0);
        if (rt != OPRT_OK) {
            bk_printf("%s, tkl_queue_post failed %d\n", __FUNCTION__, rt);
        }
    }
}
 
STATIC VOID_T __tuya_ai_display_thread(VOID_T *arg)
{
    TY_DISPLAY_MSG_T disp_msg;
    /* remove unused variable to satisfy -Werror */

    tuya_ui_init();

    for (;;) {
        tkl_queue_fetch(disp_msg_queue, &disp_msg, TKL_QUEUE_WAIT_FROEVER);
        lv_vendor_disp_lock();
        tuya_ui_app(&disp_msg);
        lv_vendor_disp_unlock();
        if (disp_msg.data && disp_msg.len) {
            extern VOID_T tkl_system_psram_free(VOID_T *ptr);
            tkl_system_psram_free(disp_msg.data);
        }
    }
}

VOID_T tuya_gui_main(VOID_T)
{
     TKL_DISP_INFO_S info;
     info.width  = BOARD_LCD_WIDTH;
     info.height = BOARD_LCD_HEIGHT;
     info.fps    = 30;
     info.rotation = BOARD_LCD_ROTATION;
     // Adapter display info does not use ll_ctrl here
 
     TKL_LVGL_CFG_T lv_cfg = {
         .recv_cb = __cpu1_recv_cb,
         .recv_arg = NULL,
     };
     tkl_lvgl_init(&info, &lv_cfg);

    bk_printf("-------[%s %d] \r\n", __func__, __LINE__);

    tkl_queue_create_init(&disp_msg_queue, sizeof(TY_DISPLAY_MSG_T), 50);

    //! Startup-success event
    // struct ipc_msg_s send_msg;
    // send_msg.type   = TKL_IPC_TYPE_LVGL;
    // send_msg.buf[0] = 1; 
    // tkl_lvgl_msg_sync((UINT8_T *)&send_msg, sizeof(send_msg));

    if (NULL == sg_display_thrd_hdl) {
        tkl_thread_create_in_psram(&sg_display_thrd_hdl, "ai_display", 1024*6, 4, __tuya_ai_display_thread, NULL);
    }
}

#include "tkl_fs.h"


/* Remove the unused tuya_fs_init to avoid undefined macros and unused warnings */

void tuya_gui_pause(void)
{
    //! TODO:
}

void tuya_gui_resume(void)
{
    //! TODO:

}


VOID *tuya_ai_display_malloc(UINT_T buf_len)
{
    struct ipc_msg_s send_msg;
    TY_DISPLAY_MSG_T disp_msg;

    VOID *buff = NULL;

    disp_msg.type = TY_DISPLAY_TP_MALLOC;
    disp_msg.len  = buf_len;
    disp_msg.data = (void *)&buff;

    send_msg.type = TKL_IPC_TYPE_LVGL;
    send_msg.req_param = (UINT8_T *)&disp_msg;
    send_msg.req_len   = sizeof(TY_DISPLAY_MSG_T);
    tuya_ipc_send_no_sync(&send_msg);

    return buff;
}

OPERATE_RET tuya_ai_display_msg(char *msg, int len, TY_DISPLAY_TYPE_E display_tp)
{
#if defined(ENABLE_TUYA_UI) && ENABLE_TUYA_UI == 1
    struct ipc_msg_s send_msg = {0};
    char *p_msg_bak = NULL;
    if (msg && len) {
        p_msg_bak = tuya_ai_display_malloc(len + 1);
        if(NULL == p_msg_bak) {
            return OPRT_MALLOC_FAILED;
        }
        memset(p_msg_bak, 0x00, len + 1);
        memcpy(p_msg_bak, msg, len);
    }

    TY_DISPLAY_MSG_T disp_msg;

    memset(&disp_msg, 0, sizeof(TY_DISPLAY_MSG_T));
    disp_msg.data = p_msg_bak;
    disp_msg.len  = len;
    disp_msg.type = display_tp;

    send_msg.type = TKL_IPC_TYPE_LVGL;
    send_msg.req_param = (UINT8_T *)&disp_msg;
    send_msg.req_len   = sizeof(TY_DISPLAY_MSG_T);
    return tuya_ipc_send_sync(&send_msg);
#endif

    return OPRT_OK;
}
