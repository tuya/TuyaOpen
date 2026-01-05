/**
 * @file atk_t5ai_disp_rgb_md0430r.c
 * @brief atk_t5ai_disp_rgb_md0430r module is used to display rgb lcd md0430r.
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_gpio.h"

#include "tdd_display_rgb.h"
#include "atk_t5ai_disp_md0430r.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define PIN_TRIG_LV(pin) ((pin) == TUYA_GPIO_LEVEL_LOW ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW)

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_DISP_RGB_CFG_T sg_disp_rgb = {
    .cfg =
        {
            .clk = 26000000,
            .out_data_clk_edge = TUYA_RGB_DATA_IN_FALLING_EDGE,
            .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
            .hsync_pulse_width = 4,
            .hsync_back_porch  = 4,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch  = 8,
            .vsync_front_porch = 8,
        },
};


static TUYA_DISPLAY_IO_CTRL_T sg_lcd_rst;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __atk_t5ai_disp_rgb_md0430r_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T gpio_cfg;

    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = PIN_TRIG_LV(sg_lcd_rst.active_level);
    tkl_gpio_init(sg_lcd_rst.pin, &gpio_cfg);

    tal_system_sleep(20);

    tkl_gpio_write(sg_lcd_rst.pin, sg_lcd_rst.active_level);
    tal_system_sleep(200);

    tkl_gpio_write(sg_lcd_rst.pin, PIN_TRIG_LV(sg_lcd_rst.active_level));
    tal_system_sleep(120);

    return rt;
}

OPERATE_RET atk_t5ai_disp_rgb_md0430r_register(char *name, ATK_T5AI_DISP_MD0430R_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_rgb.init_cb = __atk_t5ai_disp_rgb_md0430r_init;

    sg_disp_rgb.cfg.width     = dev_cfg->width;
    sg_disp_rgb.cfg.height    = dev_cfg->height;
    sg_disp_rgb.cfg.pixel_fmt = TUYA_PIXEL_FMT_RGB565;
    sg_disp_rgb.rotation      = dev_cfg->rotation;
    sg_disp_rgb.is_swap       = false;

    memcpy(&sg_disp_rgb.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_rgb.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));
    memcpy(&sg_lcd_rst, &dev_cfg->rst, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    return tdd_disp_rgb_device_register(name, &sg_disp_rgb);
}