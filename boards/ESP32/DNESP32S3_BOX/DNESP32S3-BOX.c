/**
 * @file dnesp32s3-box.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "tdd_disp_esp_st7789_80.h"
#include "board_com_api.h"

#include "xl9555.h"

#include "tdd_audio_8311_codec.h"
#include "tdd_audio_atk_no_codec.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "string.h"
#include "tal_memory.h"
#include "tdl_button_driver.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* Audio sample rates */
#define I2S_INPUT_SAMPLE_RATE  (16000)
#define I2S_OUTPUT_SAMPLE_RATE (16000)

/* I2C port and GPIOs */
#define I2C_NUM    (0)
#define I2C_SCL_IO (45)
#define I2C_SDA_IO (48)

/* I2S port and GPIOs */
#define I2S_NUM    (0)
#define I2S_MCK_IO (-1)
#define I2S_BCK_IO (21)
#define I2S_WS_IO  (13)
#define I2S_DO_IO  (14)
#define I2S_DI_IO  (47)

#define GPIO_OUTPUT_PA (-1)

/* Audio codec */
#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8311_ADDR   (0x30)

/* LCD (ST7789 over i80 8-bit bus) */
#define LCD_I80_CS  (1)
#define LCD_I80_DC  (2)
#define LCD_I80_RD  (41)
#define LCD_I80_WR  (42)
#define LCD_I80_RST (-1)

#define LCD_I80_D0 (40)
#define LCD_I80_D1 (39)
#define LCD_I80_D2 (38)
#define LCD_I80_D3 (12)
#define LCD_I80_D4 (11)
#define LCD_I80_D5 (10)
#define LCD_I80_D6 (9)
#define LCD_I80_D7 (46)

#define DISPLAY_WIDTH      (320)
#define DISPLAY_HEIGHT     (240)
#define DISPLAY_SWAP_XY    true
#define DISPLAY_MIRROR_X   true
#define DISPLAY_MIRROR_Y   false
#define DISPLAY_SWAP_BYTES 1

/* XL9555 IO expander */
#define IO_EXPANDER_XL9555_ADDR (0x20)

#define EX_IO_AP_INT   (0x0001 << 0)
#define EX_IO_QMA_INT  (0x0001 << 1)
#define EX_IO_BEEP     (0x0001 << 2)
#define EX_IO_KEY_1    (0x0001 << 3)
#define EX_IO_KEY_0    (0x0001 << 4)
#define EX_IO_SPK_CTRL (0x0001 << 5)
#define EX_IO_CTP_RST  (0x0001 << 6)
#define EX_IO_LCD_BL   (0x0001 << 7)
#define EX_IO_LED_R    (0x0001 << 8)
#define EX_IO_CTP_INT  (0x0001 << 9)
#define EX_IO_1_2      (0x0001 << 10)
#define EX_IO_1_3      (0x0001 << 11)
#define EX_IO_1_4      (0x0001 << 12)
#define EX_IO_1_5      (0x0001 << 13)
#define EX_IO_1_6      (0x0001 << 14)
#define EX_IO_1_7      (0x0001 << 15)

/***********************************************************
********************function declaration********************
***********************************************************/
int board_display_init(void);

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t is_es8311; // 1-ES8311, 0-NS4168
} DNSESP32S3_BOX_CONFIG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static DNSESP32S3_BOX_CONFIG_T sg_dnesp32s3_box = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static int __board_display_io_init(void);
static OPERATE_RET __io_expander_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    XL9555_HW_CFG_T xl9555_hw = {
        .i2c_port = I2C_NUM,
        .scl_io   = I2C_SCL_IO,
        .sda_io   = I2C_SDA_IO,
        .dev_addr = IO_EXPANDER_XL9555_ADDR,
    };
    rt = xl9555_init(&xl9555_hw);
    if (rt != OPRT_OK) {
        PR_ERR("xl9555_init failed: %d", rt);
        return rt;
    }

    uint32_t pin_out_mask = 0;
    pin_out_mask |= EX_IO_BEEP;
    pin_out_mask |= EX_IO_CTP_RST;
    pin_out_mask |= EX_IO_LCD_BL;
    pin_out_mask |= EX_IO_LED_R;
    pin_out_mask |= EX_IO_1_2;
    pin_out_mask |= EX_IO_1_3;
    pin_out_mask |= EX_IO_1_4;
    pin_out_mask |= EX_IO_1_5;
    pin_out_mask |= EX_IO_1_6;
    pin_out_mask |= EX_IO_1_7;
    rt = xl9555_set_dir(pin_out_mask, 0); // Set output direction
    if (rt != OPRT_OK) {
        PR_ERR("xl9555_set_dir out failed: %d", rt);
        return rt;
    }
    uint32_t pin_in_mask = ~pin_out_mask;
    rt = xl9555_set_dir(pin_in_mask, 1); // Set input direction
    if (rt != OPRT_OK) {
        PR_ERR("xl9555_set_dir in failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
typedef struct {
    uint32_t pin_mask;
    TUYA_GPIO_LEVEL_E active_level;
} XL9555_BUTTON_CFG_T;

static OPERATE_RET __tdd_create_xl9555_button(TDL_BUTTON_OPRT_INFO *dev)
{
    XL9555_BUTTON_CFG_T *cfg = NULL;

    if (dev == NULL || dev->dev_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    cfg = (XL9555_BUTTON_CFG_T *)dev->dev_handle;
    if (xl9555_set_dir(cfg->pin_mask, 1) != 0) {
        PR_ERR("xl9555_set_dir(input) failed");
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_delete_xl9555_button(TDL_BUTTON_OPRT_INFO *dev)
{
    if (dev == NULL || dev->dev_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_free(dev->dev_handle);
    dev->dev_handle = NULL;

    return OPRT_OK;
}

static OPERATE_RET __tdd_read_xl9555_button_value(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value)
{
    XL9555_BUTTON_CFG_T *cfg = NULL;
    uint32_t level_mask = 0;
    uint8_t raw_high = 0;

    if (dev == NULL || dev->dev_handle == NULL || value == NULL) {
        return OPRT_INVALID_PARM;
    }

    cfg = (XL9555_BUTTON_CFG_T *)dev->dev_handle;

    if (xl9555_get_level(cfg->pin_mask, &level_mask) != 0) {
        return OPRT_COM_ERROR;
    }

    raw_high = ((level_mask & cfg->pin_mask) != 0);
    if (cfg->active_level == TUYA_GPIO_LEVEL_HIGH) {
        *value = raw_high;
    } else {
        *value = (raw_high ? 0 : 1);
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_xl9555_button_register(char *name, uint32_t pin_mask, TUYA_GPIO_LEVEL_E active_level)
{
    XL9555_BUTTON_CFG_T *cfg = NULL;
    TDL_BUTTON_CTRL_INFO ctrl_info;
    TDL_BUTTON_DEVICE_INFO_T device_info;

    if (name == NULL) {
        return OPRT_INVALID_PARM;
    }

    cfg = (XL9555_BUTTON_CFG_T *)tal_malloc(sizeof(XL9555_BUTTON_CFG_T));
    if (cfg == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    cfg->pin_mask = pin_mask;
    cfg->active_level = active_level;

    memset(&ctrl_info, 0, sizeof(ctrl_info));
    ctrl_info.button_create = __tdd_create_xl9555_button;
    ctrl_info.button_delete = __tdd_delete_xl9555_button;
    ctrl_info.read_value = __tdd_read_xl9555_button_value;

    device_info.dev_handle = cfg;
    device_info.mode = BUTTON_TIMER_SCAN_MODE;

    return tdl_button_register(name, &ctrl_info, &device_info);
}
#endif

static OPERATE_RET __board_register_button(void)
{
#if !defined(ENABLE_BUTTON) || (ENABLE_BUTTON != 1)
    return OPRT_OK;
#else
    OPERATE_RET rt = OPRT_OK;

    /* Most XL9555 key circuits are pull-up + active-low. */
    TUYA_GPIO_LEVEL_E active_level = TUYA_GPIO_LEVEL_LOW;

    /* Button 1 */
    TUYA_CALL_ERR_RETURN(__tdd_xl9555_button_register(BUTTON_NAME, EX_IO_KEY_0, active_level));

    /* Button 2 (optional) */
#if defined(ENABLE_BUTTON_2) && (ENABLE_BUTTON_2 == 1)
    TUYA_CALL_ERR_RETURN(__tdd_xl9555_button_register(BUTTON_NAME_2, EX_IO_KEY_1, active_level));
#endif

    return rt;
#endif
}

static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    xl9555_set_dir(EX_IO_SPK_CTRL, 1);
    uint32_t read_level = 0;
    xl9555_get_level(EX_IO_SPK_CTRL, &read_level);
    PR_DEBUG("Speaker control level: 0x%04x", read_level);
    if (EX_IO_SPK_CTRL & read_level) {
        sg_dnesp32s3_box.is_es8311 = 1; // ES8311 codec
        PR_DEBUG("ES8311 codec is enabled");
    } else {
        sg_dnesp32s3_box.is_es8311 = 0; // NS4168 codec
        PR_DEBUG("NS4168 codec is enabled");
    }

    TDD_AUDIO_8311_CODEC_T cfg = {0};

    cfg.i2c_id = I2C_NUM;
    cfg.i2c_scl_io = I2C_SCL_IO;
    cfg.i2c_sda_io = I2C_SDA_IO;
    cfg.mic_sample_rate = I2S_INPUT_SAMPLE_RATE;
    cfg.spk_sample_rate = I2S_OUTPUT_SAMPLE_RATE;
    cfg.i2s_id = I2S_NUM;
    cfg.i2s_mck_io = I2S_MCK_IO;
    cfg.i2s_bck_io = I2S_BCK_IO;
    cfg.i2s_ws_io = I2S_WS_IO;
    cfg.i2s_do_io = I2S_DO_IO;
    cfg.i2s_di_io = I2S_DI_IO;
    cfg.gpio_output_pa = GPIO_OUTPUT_PA;
    cfg.es8311_addr = AUDIO_CODEC_ES8311_ADDR;
    cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    cfg.default_volume = 80;

    if (sg_dnesp32s3_box.is_es8311) {
        TUYA_CALL_ERR_RETURN(tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, cfg));
    } else {
        TDD_AUDIO_ATK_NO_CODEC_T NS4168_cfg = {0};
        memcpy(&NS4168_cfg, &cfg, sizeof(TDD_AUDIO_ATK_NO_CODEC_T));
        TUYA_CALL_ERR_RETURN(tdd_audio_atk_no_codec_register(AUDIO_CODEC_NAME, NS4168_cfg));
    }

    xl9555_set_dir(EX_IO_SPK_CTRL, 0);
    xl9555_set_level(EX_IO_SPK_CTRL, 1); // Enable speaker
#endif

    return rt;
}

/**
 * @brief Registers all the hardware peripherals (audio, button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__io_expander_init());
    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(board_display_init());
    TUYA_CALL_ERR_LOG(__board_display_io_init());

    return rt;
}

static int __board_display_io_init(void)
{
    xl9555_set_dir(EX_IO_LCD_BL, 0);
    xl9555_set_level(EX_IO_LCD_BL, 1);
    return 0;
}

int board_display_init(void)
{
    TDD_DISP_ESP_LCD_CFG_T cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = DISPLAY_SWAP_BYTES,
        .bl.type   = TUYA_DISP_BL_TP_NONE,
    };

    LCD_ST7789_80_HW_CFG_T hw = {
        .rd_io    = LCD_I80_RD,
        .dc_io    = LCD_I80_DC,
        .wr_io    = LCD_I80_WR,
        .cs_io    = LCD_I80_CS,
        .rst_io   = LCD_I80_RST,
        .data_io  = {LCD_I80_D0, LCD_I80_D1, LCD_I80_D2, LCD_I80_D3,
                     LCD_I80_D4, LCD_I80_D5, LCD_I80_D6, LCD_I80_D7},
        .swap_xy  = DISPLAY_SWAP_XY,
        .mirror_x = DISPLAY_MIRROR_X,
        .mirror_y = DISPLAY_MIRROR_Y,
    };

    return tdd_disp_esp_st7789_80_register(DISPLAY_NAME, &hw, &cfg);
}