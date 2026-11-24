/**
 * @file board_com_api.c
 * @brief Board hardware registration for GD32H779I DESK BUDDY.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "board_com_api.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_memory.h"
#include "tkl_pinmux.h"
#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdd_disp_st77916.h"
#endif
#if defined(ENABLE_DESK_BUDDY_AUDIO) && (ENABLE_DESK_BUDDY_AUDIO == 1)
#include "tdd_audio_ty_uart.h"
#endif
#if defined(ENABLE_DESK_BUDDY_BUTTON) && (ENABLE_DESK_BUDDY_BUTTON == 1)
#include "tdd_button_gpio.h"
#endif

#define BOARD_LCD_WIDTH                 360U
#define BOARD_LCD_HEIGHT                360U
#define BOARD_LCD_QSPI_PORT             TUYA_QSPI_NUM_0
/* PLL0Q is 375 MHz; Quad-SPI supports /8 minimum, so 46.875 MHz
 * is the highest available SCK without changing the clock tree. */
#define BOARD_LCD_QSPI_CLK_HZ           46875000U

#if defined(ENABLE_DESK_BUDDY_AUDIO) && (ENABLE_DESK_BUDDY_AUDIO == 1)
/* Voice module UART2 is cross-connected to GD32 UART3:
 * voice TX -> PD0/UART3_RX, voice RX <- PD1/UART3_TX. */
#define BOARD_AUDIO_UART_PORT          TUYA_UART_NUM_3
#define BOARD_AUDIO_UART_RX_PIN        TUYA_GPIO_NUM_48 /* PD0 */
#define BOARD_AUDIO_UART_TX_PIN        TUYA_GPIO_NUM_49 /* PD1 */
#define BOARD_AUDIO_SPK_FLOW_PIN          TUYA_GPIO_NUM_15 /* PA15 */
#define BOARD_AUDIO_BAUDRATE           1000000U
#endif
#define BOARD_LCD_CS_PIN                TUYA_GPIO_NUM_36 /* PC4 */
#define BOARD_LCD_RST_PIN               TUYA_GPIO_NUM_94 /* PF14 */
#define BOARD_LCD_BL_PIN                TUYA_GPIO_NUM_118 /* PH6 */

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
#include "tdd_encoder_gpio.h"

#define BOARD_ENCODER_A_PIN             TUYA_GPIO_NUM_121 /* PH9 */
#define BOARD_ENCODER_B_PIN             TUYA_GPIO_NUM_28  /* PB12 */
#define BOARD_ENCODER_BUTTON_PIN        TUYA_GPIO_NUM_0   /* PA0 */
#endif

#if defined(ENABLE_DESK_BUDDY_BUTTON) && (ENABLE_DESK_BUDDY_BUTTON == 1)
#define BOARD_BUTTON_PIN                TUYA_GPIO_NUM_0   /* PA0 */
#endif

#if defined(ENABLE_MOTION) && (ENABLE_MOTION == 1)
#include "tdd_motion_sc7a20.h"

#define BOARD_MOTION_I2C_PORT           TUYA_I2C_NUM_2
#define BOARD_MOTION_I2C_SCL_PIN        TUYA_GPIO_NUM_119 /* PH7 */
#define BOARD_MOTION_I2C_SDA_PIN        TUYA_GPIO_NUM_120 /* PH8 */
#define BOARD_MOTION_INT1_PIN           TUYA_GPIO_NUM_92  /* PF12 */
#define BOARD_MOTION_I2C_ADDR           0x19U
#define BOARD_MOTION_I2C_SPEED_HZ       400000U
#endif

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) && \
    defined(ENABLE_DVP) && (ENABLE_DVP == 1)
#include "tdd_camera_ov5640.h"

#define BOARD_CAMERA_CN3_I2C_PORT   TUYA_I2C_NUM_2
#define BOARD_CAMERA_CN4_I2C_PORT   TUYA_I2C_NUM_3
#define BOARD_CAMERA_CN3_I2C_SCL    TUYA_GPIO_NUM_119 /* PH7 */
#define BOARD_CAMERA_CN3_I2C_SDA    TUYA_GPIO_NUM_120 /* PH8 */
#define BOARD_CAMERA_CN4_I2C_SCL    TUYA_GPIO_NUM_60  /* PD12 */
#define BOARD_CAMERA_CN4_I2C_SDA    TUYA_GPIO_NUM_61  /* PD13 */
#define BOARD_CAMERA_CN3_RST_PIN    TUYA_GPIO_NUM_98 /* PG2 */
#define BOARD_CAMERA_CN4_RST_PIN    TUYA_GPIO_NUM_10 /* PA10 */
#define BOARD_CAMERA_PWR_PIN        TUYA_GPIO_NUM_101 /* PG5 */


#endif

#if defined(ENABLE_TP) && (ENABLE_TP == 1)
#include "tdd_tp_cst816x.h"

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_2
#define BOARD_TP_I2C_SCL_PIN         TUYA_GPIO_NUM_119 /* PH7 */
#define BOARD_TP_I2C_SDA_PIN         TUYA_GPIO_NUM_120 /* PH8 */
#define BOARD_TP_RST_PIN             TUYA_GPIO_NUM_115 /* PH3 */
#define BOARD_TP_INTR_PIN            TUYA_GPIO_NUM_116 /* PH4 */
#define BOARD_TP_WIDTH               360U
#define BOARD_TP_HEIGHT              360U
#endif

#if defined(DESK_BUDDY_AUDIO_PLAYBACK_PCM) && \
    (DESK_BUDDY_AUDIO_PLAYBACK_PCM == 1)
#define BOARD_AUDIO_PLAYBACK_FORMAT    TDL_AUDIO_FRAME_FORMAT_PCM
#define BOARD_AUDIO_PLAYBACK_FRAME_MS  20U
#else
#define BOARD_AUDIO_PLAYBACK_FORMAT    TDL_AUDIO_FRAME_FORMAT_MP3
#define BOARD_AUDIO_PLAYBACK_FRAME_MS  0U
#endif

static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_DESK_BUDDY_AUDIO) && (ENABLE_DESK_BUDDY_AUDIO == 1) && \
    defined(AUDIO_CODEC_NAME)
    const TDD_AUDIO_TY_UART_CFG_T audio_cfg = {
        .uart_port = BOARD_AUDIO_UART_PORT,
        .spk_flow_pin = BOARD_AUDIO_SPK_FLOW_PIN,
        .spk_flow_active_level = TUYA_GPIO_LEVEL_HIGH,
        .baudrate = BOARD_AUDIO_BAUDRATE,
        .boot_delay_ms = 300U,
        .default_volume = 50U,
        .mic_gain_db = 26U,
        .capture = {
            .format = TDL_AUDIO_FRAME_FORMAT_PCM,
            .sample_rate = 16000U,
            .sample_ch_num = 1U,
            .sample_bits = 16U,
            .sample_tm_ms = 20U,
            .max_payload_size = 640U,
        },
        .playback = {
            .format = BOARD_AUDIO_PLAYBACK_FORMAT,
            .sample_rate = 16000U,
            .sample_ch_num = 1U,
            .sample_bits = 16U,
            .sample_tm_ms = BOARD_AUDIO_PLAYBACK_FRAME_MS,
            .max_payload_size = 1152U,
        },
    };

    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(BOARD_AUDIO_UART_RX_PIN,
                                               TUYA_UART3_RX));
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(BOARD_AUDIO_UART_TX_PIN,
                                               TUYA_UART3_TX));
    rt = tdd_audio_ty_uart_register(AUDIO_CODEC_NAME, &audio_cfg);
    if (rt == OPRT_OK) {
        PR_NOTICE("Tuya UART audio registered: UART3 PD0/PD1, SPK flow PA15");
    }
#endif

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1) && defined(DISPLAY_NAME)
    DISP_QSPI_DEVICE_CFG_T display_cfg = {
        .width = BOARD_LCD_WIDTH,
        .height = BOARD_LCD_HEIGHT,
        .x_offset = 0U,
        .y_offset = 0U,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation = TUYA_DISPLAY_ROTATION_0,
        .rst_pin = BOARD_LCD_RST_PIN,
        .cs_pin = BOARD_LCD_CS_PIN,
        .port = BOARD_LCD_QSPI_PORT,
        .spi_clk = BOARD_LCD_QSPI_CLK_HZ,
        .mode = TUYA_QSPI_MODE0,
        .data_lines = TUYA_QSPI_4WIRE,
        .te_pin = TUYA_GPIO_NUM_MAX,
        .te_active_level = TUYA_GPIO_LEVEL_HIGH,
        .bl = {
            .type = TUYA_DISP_BL_TP_GPIO,
            .gpio = {
                .pin = BOARD_LCD_BL_PIN,
                .active_level = TUYA_GPIO_LEVEL_HIGH,
            },
        },
        .power = {
            .pin = TUYA_GPIO_NUM_MAX,
            .active_level = TUYA_GPIO_LEVEL_HIGH,
        },
        .use_dma = true,
        .te_sync_enable = false,
        .te_wait_div = 1U,
    };

    rt = tdd_disp_qspi_st77916_register(DISPLAY_NAME, &display_cfg);
#endif

    return rt;
}

static OPERATE_RET __board_register_tp(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_TP) && (ENABLE_TP == 1) && defined(DISPLAY_NAME)
    TDD_TP_CST816X_INFO_T tp_cfg = {
        .rst_pin = BOARD_TP_RST_PIN,
        .intr_pin = BOARD_TP_INTR_PIN,
        .i2c_cfg = {
            .port = BOARD_TP_I2C_PORT,
            .scl_pin = BOARD_TP_I2C_SCL_PIN,
            .sda_pin = BOARD_TP_I2C_SDA_PIN,
        },
        .tp_cfg = {
            .x_max = BOARD_TP_WIDTH,
            .y_max = BOARD_TP_HEIGHT,
            .flags = {
                .swap_xy = 1,
                .mirror_x = 1,
                .mirror_y = 0,
            },
        },
    };

    rt = tdd_tp_i2c_cst816x_register(DISPLAY_NAME, &tp_cfg);
#endif

    return rt;
}

static OPERATE_RET __board_register_encoder(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1) && defined(ENCODER_NAME)
    TDD_ENCODER_GPIO_CFG_T encoder_cfg = {
        .pin_a = BOARD_ENCODER_A_PIN,
        .pin_b = BOARD_ENCODER_B_PIN,
        .pin_button = BOARD_ENCODER_BUTTON_PIN,
        .pin_mode = TUYA_GPIO_PULLDOWN,
        .button_pin_mode = TUYA_GPIO_PULLDOWN,
        .button_active_level = TUYA_GPIO_LEVEL_HIGH,
        .button_enabled = true,
        .debounce_ms = 10U,
        .steps_per_detent = 4U,
        .reverse = true,
        .irq_enabled = false,
    };

    rt = tdd_encoder_gpio_register(ENCODER_NAME, &encoder_cfg);
#endif

    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_DESK_BUDDY_BUTTON) && (ENABLE_DESK_BUDDY_BUTTON == 1) && \
    defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1) && defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_cfg = {0};

    button_cfg.pin = BOARD_BUTTON_PIN;
    button_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    button_cfg.mode = BUTTON_TIMER_SCAN_MODE;
    button_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLDOWN;

    rt = tdd_gpio_button_register(BUTTON_NAME, &button_cfg);
    if (rt == OPRT_OK) {
        PR_NOTICE("Desk Buddy button registered as %s on PA0", BUTTON_NAME);
    }
#endif

    return rt;
}

static OPERATE_RET __board_register_motion(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_MOTION_SC7A20) && (ENABLE_MOTION_SC7A20 == 1) && defined(MOTION_NAME)
    TDD_MOTION_I2C_CFG_T motion_cfg = {
        .port = BOARD_MOTION_I2C_PORT,
        .scl_pin = BOARD_MOTION_I2C_SCL_PIN,
        .sda_pin = BOARD_MOTION_I2C_SDA_PIN,
        .addr = BOARD_MOTION_I2C_ADDR,
        .speed_hz = BOARD_MOTION_I2C_SPEED_HZ,
        .int1_pin = BOARD_MOTION_INT1_PIN,
        .int1_active_level = TUYA_GPIO_LEVEL_LOW,
        .int1_enabled = true,
    };

    rt = tdd_motion_sc7a20_register(MOTION_NAME, &motion_cfg);
#endif

    return rt;
}

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) && defined(ENABLE_DVP) && (ENABLE_DVP == 1)
static OPERATE_RET __board_register_camera_one(char *name, TUYA_I2C_NUM_E i2c_port,
                                               TUYA_GPIO_NUM_E scl_pin,
                                               TUYA_GPIO_NUM_E sda_pin,
                                               TUYA_GPIO_NUM_E rst_pin)
{
    TDD_DVP_SR_USR_CFG_T camera_cfg = {
        .pwr = {
            .pin = BOARD_CAMERA_PWR_PIN,
            .active_level = TUYA_GPIO_LEVEL_HIGH,
        },
        .rst = {
            .pin = rst_pin,
            .active_level = TUYA_GPIO_LEVEL_LOW,
        },
        .i2c = {
            .port = i2c_port,
            .clk = scl_pin,
            .sda = sda_pin,
        },
        .clk = 25000000U,
    };

    PR_NOTICE("register camera %s", name);
    return tdd_camera_dvp_ov5640_register(name, &camera_cfg);
}


#endif

static OPERATE_RET __board_register_camera(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) && \
    defined(ENABLE_DVP) && (ENABLE_DVP == 1)
    TUYA_GPIO_BASE_CFG_T reset_held = {
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_LOW,
    };

    /* CN3/CN4 share DVP and PWDN. Keep both sensors in reset and release only
     * the device selected by its reset callback when it is opened. */
    tkl_gpio_init(BOARD_CAMERA_CN3_RST_PIN, &reset_held);
    tkl_gpio_init(BOARD_CAMERA_CN4_RST_PIN, &reset_held);

    rt = __board_register_camera_one(CAMERA_NAME, BOARD_CAMERA_CN3_I2C_PORT,
                                     BOARD_CAMERA_CN3_I2C_SCL,
                                     BOARD_CAMERA_CN3_I2C_SDA,
                                     BOARD_CAMERA_CN3_RST_PIN);
    if (rt != OPRT_OK) {
        return rt;
    }

#if defined(ENABLE_CAMERA_2) && (ENABLE_CAMERA_2 == 1)
    rt = __board_register_camera_one(CAMERA_NAME_2, BOARD_CAMERA_CN4_I2C_PORT,
                                     BOARD_CAMERA_CN4_I2C_SCL,
                                     BOARD_CAMERA_CN4_I2C_SDA,
                                     BOARD_CAMERA_CN4_RST_PIN);
#endif
#endif

    return rt;
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* CH9114 channel 1 is wired to USART0 PF4/PF5. Keep the TKL chat UART
     * on the same route as the platform console; PA10 is camera reset here. */
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(TUYA_GPIO_NUM_84, TUYA_UART0_TX));
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(TUYA_GPIO_NUM_85, TUYA_UART0_RX));

    rt = __board_register_audio();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __board_register_display();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __board_register_tp();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __board_register_camera();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __board_register_button();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __board_register_encoder();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __board_register_motion();
    if (rt != OPRT_OK) {
        return rt;
    }

    return rt;
}
