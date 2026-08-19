/**
 * @file xteink_x4_pro_touch.c
 * @brief Xteink X4 Pro GT911 capacitive touch + Home key driver.
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note Bring-up and polling semantics ported from the FreeInk SDK InputManager
 *       GT911 path (freeink-sdk/libs/hardware/InputManager/src/InputManager.cpp),
 *       confirmed on X4 Pro hardware:
 *         - shared I2C bus SDA39/SCL38 @400 kHz, address 0x5D (alt 0x14)
 *         - controller sits on an ACTIVE-LOW power rail (GPIO2, enabled by
 *           board rail init) — it never ACKs until the rail is on
 *         - INT=GPIO10, RST=GPIO4; the INT level sampled as RST rises selects
 *           the I2C address (LOW -> 0x5D, HIGH -> 0x14)
 *         - self-loads its internal config on the reset dance (no upload)
 *         - mounted portrait: swap X/Y first, map onto the panel axes, flip Y
 *         - capacitive Home key = status register 0x814E bit 0x10
 * @note All access goes through tkl_i2c / tkl_gpio; no ESP-IDF calls.
 */
#include "xteink_x4_pro_touch.h"

#include "board_config.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include <string.h>

#define GT911_REG_PRODUCT_ID 0x8140
#define GT911_REG_STATUS     0x814E
#define GT911_REG_POINTS     0x8150

#define GT911_STATUS_BUFFER_READY 0x80
#define GT911_STATUS_HOME_KEY     0x10
#define GT911_STATUS_COUNT_MASK   0x0F

#define GT911_MAX_POINTS   5U
#define GT911_POINT_RECORD 8U

#define TAG "x4pro_touch"

static BOOL_T   s_touch_inited = FALSE;
static uint8_t  s_touch_addr   = 0;
static uint16_t s_i2c_fail_streak = 0; /* consecutive poll read failures */

/**
 * @brief Initialize one output GPIO.
 * @param[in] pin GPIO number.
 * @param[in] level initial level.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __gpio_output_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = level;

    return tkl_gpio_init(pin, &cfg);
}

/**
 * @brief Initialize INT as input (release phase of the reset dance).
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __int_release(void)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PULLUP;
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.level  = TUYA_GPIO_LEVEL_HIGH;

    return tkl_gpio_init(X4PRO_TOUCH_PIN_INT, &cfg);
}

/**
 * @brief Set up the shared I2C bus (idempotent — the adapter reuses an
 *        already-created bus handle on the port).
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __bus_init(void)
{
    OPERATE_RET         rt = OPRT_OK;
    TUYA_IIC_BASE_CFG_T cfg;

    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(X4PRO_I2C_PIN_SDA, TUYA_IIC0_SDA));
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(X4PRO_I2C_PIN_SCL, TUYA_IIC0_SCL));

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.role       = TUYA_IIC_MODE_MASTER;
    cfg.speed      = TUYA_IIC_BUS_SPEED_400K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    return tkl_i2c_init(X4PRO_I2C_PORT, &cfg);
}

/**
 * @brief Read a GT911 register window (16-bit register address).
 * @param[in] reg register address.
 * @param[out] buf destination buffer.
 * @param[in] len number of bytes to read.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __read_reg(uint16_t reg, uint8_t *buf, uint32_t len)
{
    OPERATE_RET rt           = OPRT_OK;
    uint8_t     reg_addr[2]  = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFFU)};

    TUYA_CALL_ERR_RETURN(tkl_i2c_master_send(X4PRO_I2C_PORT, s_touch_addr, reg_addr, sizeof(reg_addr), FALSE));
    return tkl_i2c_master_receive(X4PRO_I2C_PORT, s_touch_addr, buf, len, FALSE);
}

/**
 * @brief Clear the status register (mandatory after each consumed frame).
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __clear_status(void)
{
    uint8_t pkt[3] = {0x81, 0x4E, 0x00};

    return tkl_i2c_master_send(X4PRO_I2C_PORT, s_touch_addr, pkt, sizeof(pkt), FALSE);
}

/**
 * @brief Probe both candidate I2C addresses.
 * @return true when one address ACKed (stored in s_touch_addr).
 */
static bool __probe_candidates(void)
{
    const uint8_t candidates[2] = {X4PRO_TOUCH_I2C_ADDR, X4PRO_TOUCH_I2C_ADDRAlt};
    uint8_t       i;

    for (i = 0; i < 2U; i++) {
        if (candidates[i] == 0U) {
            continue;
        }
        if (OPRT_OK == tkl_i2c_master_send(X4PRO_I2C_PORT, candidates[i], NULL, 0, FALSE)) {
            s_touch_addr = candidates[i];
            return true;
        }
    }

    return false;
}

/**
 * @brief Reset dance with a defined INT address-select level.
 * @param[in] int_level INT level held while RST rises.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __reset_with_int_level(TUYA_GPIO_LEVEL_E int_level)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4PRO_TOUCH_PIN_INT, int_level));
    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4PRO_TOUCH_PIN_RST, TUYA_GPIO_LEVEL_LOW));
    tal_system_sleep(10);
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_TOUCH_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
    tal_system_sleep(10);
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_TOUCH_PIN_INT, int_level));
    tal_system_sleep(50);
    TUYA_CALL_ERR_RETURN(__int_release());
    tal_system_sleep(50);

    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_touch_init(void)
{
    OPERATE_RET rt      = OPRT_OK;
    uint8_t     id[4]   = {0};

    if (s_touch_inited) {
        return OPRT_OK;
    }

    /* Active-LOW rail enable: the controller never ACKs until it is powered.
     * GPIO1 (master peripheral rail) is asserted by the board rail init. */
    PR_NOTICE("[" TAG "] init: rail PWR=%d LOW (active-LOW enable), I2C port%d SDA=%d SCL=%d @400 kHz",
              (int)X4PRO_TOUCH_PIN_PWR, (int)X4PRO_I2C_PORT, (int)X4PRO_I2C_PIN_SDA, (int)X4PRO_I2C_PIN_SCL);
    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4PRO_TOUCH_PIN_PWR, TUYA_GPIO_LEVEL_LOW));
    tal_system_sleep(50);

    TUYA_CALL_ERR_RETURN(__bus_init());

    /* Address-select dance: try the primary select level first. */
    s_touch_addr = 0;
    PR_DEBUG("[" TAG "] reset dance 1/2: INT=%d LOW-select (expect addr 0x%02X)",
             (int)X4PRO_TOUCH_PIN_INT, X4PRO_TOUCH_I2C_ADDR);
    TUYA_CALL_ERR_RETURN(__reset_with_int_level(TUYA_GPIO_LEVEL_LOW));
    if (!__probe_candidates()) {
        PR_DEBUG("[" TAG "] reset dance 2/2: INT HIGH-select (expect addr 0x%02X)", X4PRO_TOUCH_I2C_ADDRAlt);
        TUYA_CALL_ERR_RETURN(__reset_with_int_level(TUYA_GPIO_LEVEL_HIGH));
        (void)__probe_candidates();
    }

    if (s_touch_addr == 0U) {
        PR_ERR("[" TAG "] GT911 not answering on 0x%02X/0x%02X", X4PRO_TOUCH_I2C_ADDR, X4PRO_TOUCH_I2C_ADDRAlt);
        return OPRT_COM_ERROR;
    }

    /* Sanity check: 4-byte product id ("911" on genuine parts). */
    if (OPRT_OK == __read_reg(GT911_REG_PRODUCT_ID, id, sizeof(id))) {
        PR_NOTICE("[" TAG "] GT911 up: id=%c%c%c%c addr=0x%02X", id[0], id[1], id[2], id[3], s_touch_addr);
    }

    (void)__clear_status();
    s_touch_inited = TRUE;
    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_touch_poll(X4PRO_TOUCH_STATE_T *state)
{
    uint8_t status = 0;
    uint8_t count  = 0;

    if (NULL == state) {
        return OPRT_INVALID_PARM;
    }

    (void)memset(state, 0, sizeof(*state));
    if (!s_touch_inited) {
        return OPRT_COM_ERROR;
    }

    if (OPRT_OK != __read_reg(GT911_REG_STATUS, &status, 1)) {
        /* Transient I2C failure: keep the latched state untouched. */
        s_i2c_fail_streak++;
        if (1U == s_i2c_fail_streak) {
            PR_WARN("[" TAG "] status read failed on 0x%02X (I2C) — keeping latched state", s_touch_addr);
        } else if (0U == (s_i2c_fail_streak % 500U)) {
            PR_WARN("[" TAG "] I2C still failing: %u consecutive status read errors", (unsigned)s_i2c_fail_streak);
        }
        return OPRT_OK;
    }
    if (s_i2c_fail_streak > 0U) {
        PR_NOTICE("[" TAG "] I2C recovered after %u failed reads", (unsigned)s_i2c_fail_streak);
        s_i2c_fail_streak = 0U;
    }

    state->home = ((status & GT911_STATUS_HOME_KEY) != 0U) ? TRUE : FALSE;

    if (0U == (status & GT911_STATUS_BUFFER_READY)) {
        return OPRT_OK;
    }

    count = status & GT911_STATUS_COUNT_MASK;
    if (count > 0U) {
        uint8_t stored = (count > GT911_MAX_POINTS) ? (uint8_t)GT911_MAX_POINTS : count;
        uint8_t points[GT911_MAX_POINTS * GT911_POINT_RECORD];

        if (OPRT_OK == __read_reg(GT911_REG_POINTS, points, (uint32_t)stored * GT911_POINT_RECORD)) {
            /* This panel's GT911 stores X at byte 0 of each 8-byte record. */
            const uint16_t raw_x = (uint16_t)points[0] | ((uint16_t)points[1] << 8);
            const uint16_t raw_y = (uint16_t)points[2] | ((uint16_t)points[3] << 8);
            uint16_t       panel_x;
            uint16_t       panel_y;

            /* Portrait-mounted digitizer: swap axes onto the panel frame... */
            panel_x = raw_y;
            panel_y = raw_x;

            if (panel_x >= X4PRO_EPD_WIDTH) {
                panel_x = (uint16_t)(X4PRO_EPD_WIDTH - 1U);
            }
            if (panel_y >= X4PRO_EPD_HEIGHT) {
                panel_y = (uint16_t)(X4PRO_EPD_HEIGHT - 1U);
            }

            /* ...then flip Y (confirmed by the corner-tap calibration). */
            panel_y = (uint16_t)((X4PRO_EPD_HEIGHT - 1U) - panel_y);

            state->pressed = TRUE;
            state->points  = stored;
            state->x       = panel_x;
            state->y       = panel_y;
        }
    }

    /* GT911 requires clearing 0x814E after each consumed frame. */
    (void)__clear_status();

    return OPRT_OK;
}
