/**
 * @file lckfb_t5ai_ex_module.c
 * @version 0.1
 * @date 2025-07-01
 */

#include "tal_api.h"
#include "tkl_pinmux.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"

#include "lckfb_t5ai_ex_module.h"

/***********************************************************
************************macro define************************
***********************************************************/

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
**************************************************************/

/* ---- LCD ---- */
#if defined (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320) && (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320 ==1)
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_SPI_DEVICE_CFG_T display_cfg;

    tkl_io_pinmux_config(BOARD_TP_I2C_SCL_PIN, TUYA_IIC1_SCL);
    tkl_io_pinmux_config(BOARD_TP_I2C_SDA_PIN, TUYA_IIC1_SDA);

    memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display_cfg.bl.type                   = BOARD_LCD_BL_TYPE;
    display_cfg.bl.pwm.id                 = BOARD_LCD_BL_PWM_ID;
    display_cfg.bl.pwm.cfg.frequency      = BOARD_LCD_BL_PWM_FREQ;
    display_cfg.bl.pwm.cfg.duty           = BOARD_LCD_BL_PWM_CYCLE;
    display_cfg.bl.pwm.cfg.cycle          = BOARD_LCD_BL_PWM_CYCLE;
    display_cfg.bl.pwm.cfg.polarity       = TUYA_PWM_POSITIVE;
    display_cfg.bl.pwm.cfg.count_mode     = TUYA_PWM_CNT_UP;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port    = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin  = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin  = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin          = BOARD_LCD_POWER_PIN;
    display_cfg.power.active_level = BOARD_LCD_POWER_ACTIVE_LV;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7789_register(DISPLAY_NAME, &display_cfg));

#if !defined(EXAMPLE_DISABLE_TOUCH) || (EXAMPLE_DISABLE_TOUCH == 0)
    TDD_TP_FT6336_INFO_T tp_cfg = {
        .rst_pin  = BOARD_TP_RST_PIN,
        .intr_pin = BOARD_TP_INT_PIN,
        .i2c_cfg =
            {
                .port    = BOARD_TP_I2C_PORT,
                .scl_pin = BOARD_TP_I2C_SCL_PIN,
                .sda_pin = BOARD_TP_I2C_SDA_PIN,
            },
        .tp_cfg =
            {
                .x_max = BOARD_LCD_WIDTH,
                .y_max = BOARD_LCD_HEIGHT,
                .flags =
                    {
                        .mirror_x = 0,
                        .mirror_y = 0,
                        .swap_xy  = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_ft6336_register(DISPLAY_NAME, &tp_cfg));
#else
    PR_NOTICE("[LCKFB_TP] touch panel is disabled by EXAMPLE_DISABLE_TOUCH");
#endif
#endif

    return rt;
}
#else
static OPERATE_RET __board_register_display(void)
{
    return OPRT_OK;
}
#endif


/* ---- Camera ---- */
#if defined (LCKFB_T5AI_TOUCH_CAMERA) && (LCKFB_T5AI_TOUCH_CAMERA ==1)
static OPERATE_RET __board_register_camera(void)
{
#if defined(CAMERA_NAME)
    OPERATE_RET rt = OPRT_OK;
    TDD_DVP_SR_USR_CFG_T camera_cfg = {
        .pwr = {
            .pin = BOARD_CAMERA_POWER_PIN,
        },
        .rst = {
            .pin = BOARD_CAMERA_RST_PIN,
            .active_level = BOARD_CAMERA_RST_ACTIVE_LV,
        },
        .i2c = {
            .port = BOARD_CAMERA_I2C_PORT,
            .clk  = BOARD_CAMERA_I2C_SCL,
            .sda  = BOARD_CAMERA_I2C_SDA,
        },
        .clk = BOARD_CAMERA_CLK,
    };

    tkl_io_pinmux_config(BOARD_CAMERA_I2C_SCL, TUYA_IIC1_SCL);
    tkl_io_pinmux_config(BOARD_CAMERA_I2C_SDA, TUYA_IIC1_SDA);

    TUYA_CALL_ERR_RETURN(tdd_camera_dvp_gc0308_register(CAMERA_NAME, &camera_cfg));
#endif

    return OPRT_OK;
}
#else
static OPERATE_RET __board_register_camera(void)
{
    return OPRT_OK;
}
#endif


/* ---- SC7A20 IMU sensor ---- */
#if defined(LCKFB_T5AI_TOUCH_SC7A20) && (LCKFB_T5AI_TOUCH_SC7A20 == 1)
static TDD_SC7A20_CFG_T s_sc7a20_cfg;
static SEM_HANDLE       s_sc7a20_int_sem  = NULL;
static THREAD_HANDLE    s_sc7a20_int_thrd = NULL;

/* INT1 ISR: post semaphore only — NO blocking/I2C calls in IRQ context. */
static void __board_sc7a20_int_isr(void *args)
{
    (void)args;
    tal_semaphore_post(s_sc7a20_int_sem);
}

/* INT1 worker: clear the latched interrupt (read INT1_SRC) and read accel.
 * Runs in thread context so blocking I2C access is safe. */
static void __board_sc7a20_int_task(void *args)
{
    uint8_t            src = 0;
    SC7A20_ACCEL_DATA_T mg;

    (void)args;

    while (1) {
        tal_semaphore_wait(s_sc7a20_int_sem, SEM_WAIT_FOREVER);

        /* reading INT1_SRC clears the latched INT1 */
        if (OPRT_OK == tdd_sc7a20_int_read_src(&s_sc7a20_cfg, &src)) {
            if (src & SC7A20_INT1_IA) {
                if (OPRT_OK == tdd_sc7a20_read_mg(&s_sc7a20_cfg, &mg)) {
                    PR_NOTICE("[SC7A20] INT1 activity: x=%d y=%d z=%d mg", mg.x, mg.y, mg.z);
                }
            }
        }
    }
}

static OPERATE_RET __board_register_sc7a20(void)
{
    OPERATE_RET rt = OPRT_OK;

    s_sc7a20_cfg.i2c_port  = BOARD_SC7A20_I2C_PORT;
    s_sc7a20_cfg.i2c_addr  = SC7A20_I2C_ADDR_SA0_HIGH;
    s_sc7a20_cfg.range     = SC7A20_RANGE_2G;
    s_sc7a20_cfg.odr       = SC7A20_ODR_100HZ;
    s_sc7a20_cfg.low_power = false;
    s_sc7a20_cfg.hpf_data  = false;

    /* I2C pinmux is board-specific (SCL/SDA routed to I2C1) */
    tkl_io_pinmux_config(BOARD_SC7A20_I2C_SCL_PIN, TUYA_IIC1_SCL);
    tkl_io_pinmux_config(BOARD_SC7A20_I2C_SDA_PIN, TUYA_IIC1_SDA);

    TUYA_CALL_ERR_RETURN(tdd_sc7a20_init(&s_sc7a20_cfg));

    /* configure INT1: activity detection (latched, active-high) */
    TDD_SC7A20_INT_CFG_T int_cfg = {
        .event     = SC7A20_INT_ACT,
        .threshold = 0x10,    /* ~250 mg at 2 g full-scale (1 LSB = 15.6 mg) */
        .duration  = 0x00,    /* immediate */
    };
    TUYA_CALL_ERR_RETURN(tdd_sc7a20_int_config(&s_sc7a20_cfg, &int_cfg));

    /* clear any latched INT1 before enabling the GPIO IRQ */
    uint8_t src = 0;
    (void)tdd_sc7a20_int_read_src(&s_sc7a20_cfg, &src);

    /* INT1 pin as input with pull-up (sensor drives it high on event) */
    TUYA_GPIO_BASE_CFG_T int_pin = {
        .mode   = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
    };
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(BOARD_SC7A20_INT1_PIN, &int_pin));

    /* semaphore + worker thread: defer I2C out of IRQ context */
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&s_sc7a20_int_sem, 0, 1));
    THREAD_CFG_T thrd_cfg = {
        .thrdname   = "sc7a20_int",
        .stackDepth = 2048,
        .priority   = THREAD_PRIO_2,
    };
    if (NULL == s_sc7a20_int_thrd) {
        TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&s_sc7a20_int_thrd, NULL, NULL, __board_sc7a20_int_task, NULL, &thrd_cfg));
    }

    /* enable INT1 GPIO interrupt: rising edge (INT1 goes high on event) */
    TUYA_GPIO_IRQ_T irq_cfg = {
        .mode = TUYA_GPIO_IRQ_RISE,
        .cb   = __board_sc7a20_int_isr,
        .arg  = NULL,
    };
    TUYA_CALL_ERR_RETURN(tkl_gpio_irq_init(BOARD_SC7A20_INT1_PIN, &irq_cfg));
    TUYA_CALL_ERR_RETURN(tkl_gpio_irq_enable(BOARD_SC7A20_INT1_PIN));

    PR_NOTICE("[SC7A20] INT1 activity-detection enabled on P%d", BOARD_SC7A20_INT1_PIN);
    return OPRT_OK;
}
#else
static OPERATE_RET __board_register_sc7a20(void)
{
    return OPRT_OK;
}
#endif


/* ---- SDIO ---- */
#if defined (LCKFB_T5AI_TOUCH_SDIO) && (LCKFB_T5AI_TOUCH_SDIO ==1)
static OPERATE_RET __board_sdio_pin_register(void)
{
    tkl_io_pinmux_config(TUYA_GPIO_NUM_14, TUYA_SDIO_CLK);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_15, TUYA_SDIO_CMD);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_16, TUYA_SDIO_DATA0);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_17, TUYA_SDIO_DATA1);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_18, TUYA_SDIO_DATA2);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_19, TUYA_SDIO_DATA3);

    return OPRT_OK;
}
#else
static OPERATE_RET __board_sdio_pin_register(void)
{
    return OPRT_OK;
}
#endif


OPERATE_RET board_register_ex_module(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__board_register_display());

    TUYA_CALL_ERR_RETURN(__board_register_camera());

    TUYA_CALL_ERR_RETURN(__board_sdio_pin_register());

    TUYA_CALL_ERR_RETURN(__board_register_sc7a20());

    return rt;
}
