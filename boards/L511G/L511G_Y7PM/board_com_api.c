/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and display peripherals.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "board_com_api.h"

#if defined(AUDIO_CODEC_NAME)
#include "tdd_audio.h"
#endif

#if defined(BUTTON_NAME)
#include "tdd_button_gpio.h"
#endif

#if defined(DISPLAY_NAME)
#if defined(BOARD_EINK_PANEL_UC8176) && (BOARD_EINK_PANEL_UC8176 == 1)
#include "tdd_disp_uc8176.h"
#else
#include "tdd_disp_uc8276.h"
#endif
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* The module's mic runs through the vendor recorder, which delivers 16-bit
 * mono PCM at 8k or 16k only (see tkl_ai_init). 16k is what the AI service
 * expects for ASR. */
#define BOARD_MIC_SAMPLE_RATE TKL_AUDIO_SAMPLE_16K

/* The speaker amplifier pin, or -1 when the board drives none. The codec driver
 * gets it through pad_hook(); this is the same value, for the playback path. */
#if defined(BOARD_SPEAKER_PA_PIN)
#define BOARD_SPEAKER_EN_PIN BOARD_SPEAKER_PA_PIN
#else
#define BOARD_SPEAKER_EN_PIN (-1)
#endif

/* Waveshare 4.2" e-paper, 400x300 mono. Which controller is behind the glass
 * is a board choice -- see BOARD_EINK_PANEL_* in Kconfig -- because the two
 * parts share neither their command set nor the sense of their BUSY line. The
 * two config structures happen to agree field for field, so aliasing the type
 * and the register call keeps one copy of the wiring below. */
#define BOARD_EINK_WIDTH  400
#define BOARD_EINK_HEIGHT 300

#if defined(DISPLAY_NAME)
#if defined(BOARD_EINK_PANEL_UC8176) && (BOARD_EINK_PANEL_UC8176 == 1)
#define BOARD_EINK_CFG_T      DISP_EINK_UC8176_CFG_T
#define board_eink_register   tdd_disp_spi_mono_uc8176_register
#define BOARD_EINK_PANEL_NAME "UC8176"
#else
#define BOARD_EINK_CFG_T      DISP_EINK_UC8276_CFG_T
#define board_eink_register   tdd_disp_spi_mono_uc8276_register
#define BOARD_EINK_PANEL_NAME "SSD1683"
#endif
#endif

#if defined(BOARD_BUTTON_ACTIVE_LOW) && (BOARD_BUTTON_ACTIVE_LOW == 1)
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_PULL      TUYA_GPIO_PULLUP
#else
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH
#define BOARD_BUTTON_PULL      TUYA_GPIO_PULLDOWN
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_T5AI_T cfg = {0};
    memset(&cfg, 0, sizeof(TDD_AUDIO_T5AI_T));

    /* No AEC on this module: the vendor recorder hands over raw PCM. */
    cfg.aec_enable = 0;

    cfg.ai_chn      = TKL_AI_0;
    cfg.sample_rate = BOARD_MIC_SAMPLE_RATE;
    cfg.data_bits   = TKL_AUDIO_DATABITS_16;
    cfg.channel     = TKL_AUDIO_CHANNEL_MONO;

    cfg.spk_sample_rate  = BOARD_MIC_SAMPLE_RATE;
    cfg.spk_pin          = BOARD_SPEAKER_EN_PIN;
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tdd_audio_register(AUDIO_CODEC_NAME, cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    /* Timer scan, not IRQ: pinMap in the platform's tkl_gpio.c records that
     * several pads -- pad 58 among them -- never deliver an edge interrupt. */
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin                = (TUYA_GPIO_NUM_E)BOARD_BUTTON_PIN,
        .level              = BOARD_BUTTON_ACTIVE_LV,
        .mode               = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = BOARD_BUTTON_PULL,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
}

/**
 * @brief Close the load switch feeding the panel and let the rail settle.
 *
 * Deliberately not left to the display driver, even though its config has a
 * power pin. TuyaOpen's own TUYA_T5AI_EINK_NFC board -- the arrangement this
 * panel is known to work in -- keeps the switch under a separate power driver,
 * turns it on once before the display is registered, and passes
 * TUYA_GPIO_NUM_MAX as the display's power pin so the driver never touches it.
 *
 * The reason shows up on close: tdd_disp_spi_uc8276.c cuts the pin in
 * __epd_sleep(), and the application reopens the device every so often to clear
 * partial-refresh ghosting. Handing the driver the switch would turn each of
 * those into a power cycle of the panel, which is not what the working board
 * does. One GPIO does not justify pulling in tdd_power_soc here -- that driver
 * also wants a battery ADC and a charger pin, neither of which this board has.
 */
static void __board_eink_power_on(void)
{
#if defined(DISPLAY_NAME)
    if (BOARD_EINK_PWR_PIN >= TUYA_GPIO_NUM_MAX) {
        /* No switch: the panel is wired to a permanent 3V3. */
        return;
    }

    TUYA_GPIO_BASE_CFG_T pwr_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_HIGH, /* active high enable */
    };

    tkl_gpio_init((TUYA_GPIO_NUM_E)BOARD_EINK_PWR_PIN, &pwr_cfg);
    tkl_gpio_write((TUYA_GPIO_NUM_E)BOARD_EINK_PWR_PIN, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(50); /* let the rail come up before anything talks to it */
#endif
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    BOARD_EINK_CFG_T eink_cfg = {0};
    memset(&eink_cfg, 0, sizeof(BOARD_EINK_CFG_T));

    __board_eink_power_on();

    eink_cfg.width    = BOARD_EINK_WIDTH;
    eink_cfg.height   = BOARD_EINK_HEIGHT;
    eink_cfg.rotation = TUYA_DISPLAY_ROTATION_0;

    eink_cfg.port    = TUYA_SPI_NUM_1;
    eink_cfg.spi_clk = BOARD_EINK_SPI_CLK_HZ;

    /* SPI1 drives its own chip select on module pad 64: tkl_spi.c opens the
     * port with ARM_SPI_SS_MASTER_SW and pulls the line around every transfer.
     * The display driver must not also toggle a CS GPIO -- and could not, since
     * pad 64 is muxed to the SPI block and is not in tkl_gpio's pinMap. A pin
     * of TUYA_GPIO_NUM_MAX is how tdd_display_spi.c is told to leave CS alone. */
    eink_cfg.cs_pin   = TUYA_GPIO_NUM_MAX;
    eink_cfg.dc_pin   = (TUYA_GPIO_NUM_E)BOARD_EINK_DC_PIN;
    eink_cfg.rst_pin  = (TUYA_GPIO_NUM_E)BOARD_EINK_RST_PIN;
    eink_cfg.busy_pin = (TUYA_GPIO_NUM_E)BOARD_EINK_BUSY_PIN;

    /* The load switch is already closed, by __board_eink_power_on() above, and
     * stays that way. TUYA_GPIO_NUM_MAX keeps the display driver out of it --
     * and a zeroed struct would otherwise name GPIO 0, which the driver would
     * dutifully initialise. */
    eink_cfg.power.pin          = TUYA_GPIO_NUM_MAX;
    eink_cfg.power.active_level = TUYA_GPIO_LEVEL_HIGH;

    /* No front light on this panel. */
    eink_cfg.bl.type = TUYA_DISP_BL_TP_NONE;

    TUYA_CALL_ERR_RETURN(board_eink_register(DISPLAY_NAME, &eink_cfg));

    PR_NOTICE("eink: %s is a %s on SPI1, dc=%d rst=%d busy=%d pwr=%d @%dHz", DISPLAY_NAME, BOARD_EINK_PANEL_NAME,
              BOARD_EINK_DC_PIN, BOARD_EINK_RST_PIN, BOARD_EINK_BUSY_PIN, BOARD_EINK_PWR_PIN, BOARD_EINK_SPI_CLK_HZ);
#endif

    return rt;
}

/**
 * @brief Registers all the hardware peripherals (audio, button, display) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_audio());

    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_display());

    return rt;
}
