/**
 * @file board_com_api.c
 * @brief Board hardware registration for GD32H779I DESK BUDDY.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "board_com_api.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_pinmux.h"
#include "tdd_disp_st77916.h"

#define BOARD_LCD_WIDTH                 360U
#define BOARD_LCD_HEIGHT                360U
#define BOARD_LCD_QSPI_PORT             TUYA_QSPI_NUM_0
#define BOARD_LCD_QSPI_CLK_HZ           37500000U
#define BOARD_LCD_CS_PIN                TUYA_GPIO_NUM_36 /* PC4 */
#define BOARD_LCD_RST_PIN               TUYA_GPIO_NUM_94 /* PF14 */
#define BOARD_LCD_BL_PIN                TUYA_GPIO_NUM_118 /* PH6 */

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
#include "tdd_encoder_gpio.h"

#define BOARD_ENCODER_A_PIN             TUYA_GPIO_NUM_121 /* PH9 */
#define BOARD_ENCODER_B_PIN             TUYA_GPIO_NUM_28  /* PB12 */
#define BOARD_ENCODER_BUTTON_PIN        TUYA_GPIO_NUM_0   /* PA0 */
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
#include "tdl_camera_driver.h"

#define BOARD_CAMERA_CN3_I2C_PORT   TUYA_I2C_NUM_2
#define BOARD_CAMERA_CN4_I2C_PORT   TUYA_I2C_NUM_3
#define BOARD_CAMERA_CN3_I2C_SCL    TUYA_GPIO_NUM_119 /* PH7 */
#define BOARD_CAMERA_CN3_I2C_SDA    TUYA_GPIO_NUM_120 /* PH8 */
#define BOARD_CAMERA_CN4_I2C_SCL    TUYA_GPIO_NUM_60  /* PD12 */
#define BOARD_CAMERA_CN4_I2C_SDA    TUYA_GPIO_NUM_61  /* PD13 */
#define BOARD_CAMERA_CN3_RST_PIN    TUYA_GPIO_NUM_98 /* PG2 */
#define BOARD_CAMERA_CN4_RST_PIN    TUYA_GPIO_NUM_10 /* PA10 */
#define BOARD_CAMERA_PWR_PIN        TUYA_GPIO_NUM_101 /* PG5 */

#define BOARD_CAMERA_FRAME_ALIGN    32U
#define BOARD_CAMERA_FRAME_WIDTH    320U
#define BOARD_CAMERA_FRAME_HEIGHT   240U
#define BOARD_CAMERA_FRAME_COUNT    2U
#define BOARD_CAMERA_FRAME_BYTES    \
    (BOARD_CAMERA_FRAME_WIDTH * BOARD_CAMERA_FRAME_HEIGHT * 2U)
#define BOARD_CAMERA_FRAME_SLOT_SIZE \
    ((BOARD_CAMERA_FRAME_BYTES + BOARD_CAMERA_FRAME_ALIGN) & \
     ~(BOARD_CAMERA_FRAME_ALIGN - 1U))
#define BOARD_CAMERA_FRAME_POOL_SIZE (0x4C000U)

static uint8_t s_camera_frame_pool[BOARD_CAMERA_FRAME_POOL_SIZE]
    __attribute__((aligned(BOARD_CAMERA_FRAME_ALIGN)));
static size_t s_camera_frame_independent_used;
static size_t s_camera_frame_shared_next;
static bool s_camera_frame_shared_mode;

_Static_assert(BOARD_CAMERA_FRAME_COUNT * BOARD_CAMERA_FRAME_SLOT_SIZE <=
                   BOARD_CAMERA_FRAME_POOL_SIZE,
               "camera frame pool is too small");

#endif

#if defined(ENABLE_TP) && (ENABLE_TP == 1)
#include "tdd_tp_cst816x.h"

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_2
#define BOARD_TP_I2C_SCL_PIN         TUYA_IO_PIN_MAX
#define BOARD_TP_I2C_SDA_PIN         TUYA_IO_PIN_MAX
#define BOARD_TP_RST_PIN             TUYA_GPIO_NUM_115 /* PH3 */
#define BOARD_TP_INTR_PIN            TUYA_GPIO_NUM_MAX
#define BOARD_TP_WIDTH               360U
#define BOARD_TP_HEIGHT              360U
#endif

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
        .data_lines = TUYA_QSPI_1WIRE,
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

static void *__board_camera_frame_alloc(size_t size)
{
    void *ptr;
    size_t slot;
    bool fallback = false;

    if (size == 0U || size > BOARD_CAMERA_FRAME_SLOT_SIZE) {
        PR_ERR("camera frame request too large: request=%u max=%u",
               (unsigned)size, (unsigned)BOARD_CAMERA_FRAME_SLOT_SIZE);
        return NULL;
    }

    TAL_ENTER_CRITICAL();
    if (false == s_camera_frame_shared_mode &&
        BOARD_CAMERA_FRAME_SLOT_SIZE <= BOARD_CAMERA_FRAME_POOL_SIZE -
                                           s_camera_frame_independent_used) {
        ptr = &s_camera_frame_pool[s_camera_frame_independent_used];
        s_camera_frame_independent_used += BOARD_CAMERA_FRAME_SLOT_SIZE;
    } else {
        fallback = (false == s_camera_frame_shared_mode);
        s_camera_frame_shared_mode = true;
        slot = s_camera_frame_shared_next;
        s_camera_frame_shared_next =
            (s_camera_frame_shared_next + 1U) % BOARD_CAMERA_FRAME_COUNT;
        ptr = &s_camera_frame_pool[slot * BOARD_CAMERA_FRAME_SLOT_SIZE];
    }
    TAL_EXIT_CRITICAL();

    if (fallback) {
        PR_WARN("camera independent frame pool exhausted; use %u shared frames",
                (unsigned)BOARD_CAMERA_FRAME_COUNT);
    }

    return ptr;
}

static void __board_camera_frame_free(void *ptr)
{
    /* TDL keeps frame nodes for the registered camera lifetime, so this pool
     * is intentionally monotonic and allocations are reclaimed only by reset. */
    (void)ptr;
}

static void __board_register_camera_frame_pool(void)
{
    TDL_CAMERA_FRAME_MEM_OPS_T ops = {
        .alloc = __board_camera_frame_alloc,
        .free = __board_camera_frame_free,
    };

    tdl_camera_set_frame_mem_ops(&ops);
    PR_NOTICE("camera shared frame pool: %u frames x %u bytes",
              (unsigned)BOARD_CAMERA_FRAME_COUNT,
              (unsigned)BOARD_CAMERA_FRAME_SLOT_SIZE);
}

static OPERATE_RET __board_register_camera(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) && \
    defined(ENABLE_DVP) && (ENABLE_DVP == 1)
    TUYA_GPIO_BASE_CFG_T reset_held = {
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_LOW,
    };

    __board_register_camera_frame_pool();

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
