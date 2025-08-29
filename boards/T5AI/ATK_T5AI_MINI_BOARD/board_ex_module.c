/**
 * @file board_ex_module.c
 * @version 0.1
 * @date 2025-07-01
 */

#include "tal_api.h"

#include "tkl_gpio.h"
#include "board_ex_module.h"

#if defined (ATK_T5AI_MINI_BOARD_EX_MODULE_LCD) && (ATK_T5AI_MINI_BOARD_EX_MODULE_LCD ==1)
#include "tdd_display_rgb.h"
#include "tdd_touch_gt1151.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct{
    uint32_t id;        /* Panel ID */
    uint32_t width;     /* width in pixels */
    uint32_t height;    /* height in pixels */
    uint32_t fmt;       /* Pixel format */
    uint32_t dir;       /* Display direction */
    uint16_t hsw;       /* Horizontal sync width */
    uint16_t vsw;       /* Vertical sync width */
    uint16_t hbp;       /* Horizontal back porch */
    uint16_t vbp;       /* Vertical back porch */
    uint16_t hfp;       /* Horizontal front porch */
    uint16_t vfp;       /* Vertical front porch */
    uint32_t pclk_hz;   /* Pixel clock frequency in Hz */
    uint32_t clk_edge;  /* Clock edge */
}ATK_LCD_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined (ATK_T5AI_MINI_BOARD_EX_MODULE_LCD) && (ATK_T5AI_MINI_BOARD_EX_MODULE_LCD ==1)
static const ATK_LCD_INFO_T cATK_MD0430R_480272 = {
    .id       = ATK_MD0430R_480272_ID,
    .width    = ATK_MD0430R_480272_WIDTH,
    .height   = ATK_MD0430R_480272_HEIGHT,
    .fmt      = ATK_MD0430R_480272_FMT,
    .dir      = ATK_MD0430R_480272_ROTATION,
    .hsw      = ATK_MD0430R_480272_HSW,
    .vsw      = ATK_MD0430R_480272_VSW,
    .hbp      = ATK_MD0430R_480272_HBP,
    .vbp      = ATK_MD0430R_480272_VBP,
    .hfp      = ATK_MD0430R_480272_HFP,
    .vfp      = ATK_MD0430R_480272_VFP,
    .pclk_hz  = ATK_MD0430R_480272_CLK,
    .clk_edge = ATK_MD0430R_480272_CLK_EDGE,
};

static const ATK_LCD_INFO_T cATK_MD0430R_800480 = {
    .id      = ATK_MD0430R_800480_ID,
    .width   = ATK_MD0430R_800480_WIDTH,
    .height  = ATK_MD0430R_800480_HEIGHT,
    .fmt     = ATK_MD0430R_800480_FMT,
    .dir     = ATK_MD0430R_800480_ROTATION,
    .hsw     = ATK_MD0430R_800480_HSW,
    .vsw     = ATK_MD0430R_800480_VSW,
    .hbp     = ATK_MD0430R_800480_HBP,
    .vbp     = ATK_MD0430R_800480_VBP,
    .hfp     = ATK_MD0430R_800480_HFP,
    .vfp     = ATK_MD0430R_800480_VFP,
    .pclk_hz = ATK_MD0430R_800480_CLK,
    .clk_edge = ATK_MD0430R_800480_CLK_EDGE,
};
#endif
/***********************************************************
***********************function define**********************
***********************************************************/
#if defined (ATK_T5AI_MINI_BOARD_EX_MODULE_LCD) && (ATK_T5AI_MINI_BOARD_EX_MODULE_LCD ==1)
/**
 * @brief Reads the lcd information from the RGB LCD display by checking the GPIO levels.
 *
 * This function initializes the GPIO pins for the RGB channels, reads their levels,
 * and determines the lcd information based on the combination of these levels.
 *
 * @return Returns the lcd information if recognized, or NULL if not recognized.
 */
static const ATK_LCD_INFO_T *__read_lcd_info(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t idx = 0;
    TUYA_GPIO_LEVEL_E r_level = TUYA_GPIO_LEVEL_NONE;   /* Variable to hold GPIO level */

    TUYA_GPIO_BASE_CFG_T out_pin_cfg = { /* GPIO configuration */
        .mode = TUYA_GPIO_PULLUP,        /* Push-pull mode */
        .direct = TUYA_GPIO_INPUT,       /* Input direction */
        .level = TUYA_GPIO_LEVEL_NONE    /* Initial level, will be set later */
    };
 
    TUYA_CALL_ERR_LOG(tkl_gpio_init(BOARD_GPIO_LCD_ID0, &out_pin_cfg));    /* Initialize GPIO for Red channel */
    TUYA_CALL_ERR_LOG(tkl_gpio_init(BOARD_GPIO_LCD_ID1, &out_pin_cfg));    /* Initialize GPIO for Green channel */
    TUYA_CALL_ERR_LOG(tkl_gpio_init(BOARD_GPIO_LCD_ID2, &out_pin_cfg));    /* Initialize GPIO for Blue channel */

    TUYA_CALL_ERR_LOG(tkl_gpio_read(BOARD_GPIO_LCD_ID0,&r_level));         /* Read the level of Red channel GPIO */
    idx = r_level;
    TUYA_CALL_ERR_LOG(tkl_gpio_read(BOARD_GPIO_LCD_ID1,&r_level));         /* Read the level of Green channel GPIO */
    idx |= r_level << 1;
    TUYA_CALL_ERR_LOG(tkl_gpio_read(BOARD_GPIO_LCD_ID2,&r_level));         /* Read the level of Blue channel GPIO */
    idx |= r_level << 2;

    switch (idx) {
        case 0: {
            return &cATK_MD0430R_480272;                      /* ATK-MD0430R-480272 */
        }
        case 4:{
            return &cATK_MD0430R_800480;                      /* ATK-MD0430R-800480 */
        }
        default: {
            PR_ERR("not find lcd info");
            return NULL;
        }
    }
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    ATK_LCD_INFO_T *p_lcd_info = (ATK_LCD_INFO_T *)__read_lcd_info();
    TDD_DISP_RGB_CFG_T display_cfg;

    if(NULL == p_lcd_info) {
        return OPRT_NOT_FOUND;
    }

    memset(&display_cfg, 0, sizeof(TDD_DISP_RGB_CFG_T));

    display_cfg.cfg.width     = p_lcd_info->width;
    display_cfg.cfg.height    = p_lcd_info->height;
    display_cfg.cfg.pixel_fmt = p_lcd_info->fmt;

    display_cfg.cfg.clk               = p_lcd_info->pclk_hz;
    display_cfg.cfg.out_data_clk_edge = p_lcd_info->clk_edge;
    display_cfg.cfg.hsync_back_porch  = p_lcd_info->hbp;
    display_cfg.cfg.hsync_front_porch = p_lcd_info->hfp;
    display_cfg.cfg.vsync_back_porch  = p_lcd_info->vbp;
    display_cfg.cfg.vsync_front_porch = p_lcd_info->vfp;
    display_cfg.cfg.hsync_pulse_width = p_lcd_info->hsw;
    display_cfg.cfg.vsync_pulse_width = p_lcd_info->vsw;

    display_cfg.rotation  = p_lcd_info->dir;

    display_cfg.bl.type              = BOARD_LCD_BL_TYPE;
    display_cfg.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_rgb_device_register(DISPLAY_NAME, &display_cfg));                      /* Register the RGB display device */

    TUYA_CALL_ERR_RETURN(
        tdd_disp_rgb_device_register(DISPLAY_NAME, &display_cfg)); /* Register the RGB display device */

    TDD_TOUCH_GT1151_INFO_T touch_cfg = {
        .i2c_cfg =
            {
                .port = BOARD_TOUCH_I2C_PORT,
                .scl_pin = BOARD_TOUCH_I2C_SCL_PIN,
                .sda_pin = BOARD_TOUCH_I2C_SDA_PIN,
            },
        .tp_cfg =
            {
                .x_max = display_cfg.cfg.width,
                .y_max = display_cfg.cfg.height,
                .flags =
                    {
                        .mirror_x = 0,
                        .mirror_y = 0,
                        .swap_xy = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_touch_i2c_gt1151_register(DISPLAY_NAME, &touch_cfg));
#endif

    return rt;
}

#else
static OPERATE_RET __board_register_display(void)
{
    return OPRT_OK;
}

#endif

OPERATE_RET board_register_ex_module(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__board_register_display());

    return rt;
}
