/**
 * @file m5pm1_driver.c
 * @brief M5PM1 power management IC driver implementation
 * @version 0.1
 * @date 2026-04-27
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */
#include "m5pm1_driver.h"

#include "tal_log.h"
#include "tal_system.h"
#include "tkl_i2c.h"
#include "tuya_error_code.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define M5PM1_REG_DEVICE_ID     (0x00)
#define M5PM1_REG_PWR_SRC       (0x04)
#define M5PM1_REG_WAKE_SRC      (0x05)
#define M5PM1_REG_PWR_CFG       (0x06)
#define M5PM1_REG_HOLD_CFG      (0x07)
#define M5PM1_REG_BATT_LVP      (0x08)
#define M5PM1_REG_I2C_CFG       (0x09)
#define M5PM1_REG_WDT_CNT       (0x0A)
#define M5PM1_REG_WDT_KEY       (0x0B)
#define M5PM1_REG_SYS_CMD       (0x0C)
#define M5PM1_REG_GPIO_MODE     (0x10)
#define M5PM1_REG_GPIO_OUT      (0x11)
#define M5PM1_REG_GPIO_IN       (0x12)
#define M5PM1_REG_GPIO_DRV      (0x13)
#define M5PM1_REG_GPIO_PUPD0    (0x14)
#define M5PM1_REG_GPIO_PUPD1    (0x15)
#define M5PM1_REG_GPIO_FUNC0    (0x16)
#define M5PM1_REG_GPIO_FUNC1    (0x17)
#define M5PM1_REG_GPIO_WAKE_EN  (0x18)
#define M5PM1_REG_GPIO_WAKE_CFG (0x19)
#define M5PM1_REG_TIM_CNT_0     (0x38)
#define M5PM1_REG_TIM_CFG       (0x3C)
#define M5PM1_REG_TIM_KEY       (0x3D)
#define M5PM1_REG_IRQ_STATUS1   (0x40)
#define M5PM1_REG_IRQ_STATUS2   (0x41)
#define M5PM1_REG_IRQ_STATUS3   (0x42)
#define M5PM1_REG_IRQ_MASK1     (0x43)
#define M5PM1_REG_IRQ_MASK2     (0x44)
#define M5PM1_REG_IRQ_MASK3     (0x45)

#define M5PM1_PWR_CFG_CHG_EN   (1U << 0)
#define M5PM1_PWR_CFG_DCDC_EN  (1U << 1)
#define M5PM1_PWR_CFG_LDO_EN   (1U << 2)
#define M5PM1_PWR_CFG_BOOST_EN (1U << 3)
#define M5PM1_PWR_CFG_LED_EN   (1U << 4)

#define M5PM1_HOLD_CFG_LDO   (1U << 5)
#define M5PM1_HOLD_CFG_BOOST (1U << 6)

#define M5PM1_I2C_SLEEP_MASK  (0x0F)
#define M5PM1_WDT_FEED_KEY    (0xA5)
#define M5PM1_SYS_CMD_KEY     (0xA0)
#define M5PM1_SYS_CMD_OFF     (0x01)
#define M5PM1_SYS_CMD_RESET   (0x02)
#define M5PM1_SYS_CMD_DL      (0x03)
#define M5PM1_TIM_RELOAD_KEY  (0xA5)
#define M5PM1_TIM_ARM         (1U << 3)
#define M5PM1_TIM_MAX_SECONDS (214748364UL)

#define M5PM1_IRQ_GPIO_VALID_MASK (0x1F)
#define M5PM1_IRQ_SYS_VALID_MASK  (0x3F)
#define M5PM1_IRQ_BTN_VALID_MASK  (0x07)
#define M5PM1_SYS_CMD_DELAY_MS    (120)

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    M5PM1_CFG_T cfg;
    bool initialized;
} M5PM1_DEV_T;

/* ---------------------------------------------------------------------------
 * File-scope variables
 * --------------------------------------------------------------------------- */
static M5PM1_DEV_T s_m5pm1 = {
    .cfg = {
        .i2c_port = TUYA_I2C_NUM_0,
        .i2c_addr = M5PM1_DEFAULT_ADDR,
    },
    .initialized = false,
};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Read one M5PM1 register.
 * @param[in] reg register address.
 * @param[out] data register value.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_read_reg(uint8_t reg, uint8_t *data)
{
    OPERATE_RET rt = OPRT_OK;

    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_i2c_master_send(s_m5pm1.cfg.i2c_port, s_m5pm1.cfg.i2c_addr, &reg, 1, FALSE);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tkl_i2c_master_receive(s_m5pm1.cfg.i2c_port, s_m5pm1.cfg.i2c_addr, data, 1, FALSE);
    if (rt != OPRT_OK) {
        return rt;
    }

    return OPRT_OK;
}

/**
 * @brief Write one M5PM1 register.
 * @param[in] reg register address.
 * @param[in] data register value.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};

    return tkl_i2c_master_send(s_m5pm1.cfg.i2c_port, s_m5pm1.cfg.i2c_addr, buf, sizeof(buf), FALSE);
}

/**
 * @brief Write a contiguous M5PM1 register block.
 * @param[in] reg first register address.
 * @param[in] data register data.
 * @param[in] len register data length.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_write_bytes(uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t buf[5] = {0};
    uint8_t i = 0;

    if ((data == NULL) || (len == 0) || (len > (sizeof(buf) - 1U))) {
        return OPRT_INVALID_PARM;
    }

    buf[0] = reg;
    for (i = 0; i < len; i++) {
        buf[i + 1U] = data[i];
    }

    return tkl_i2c_master_send(s_m5pm1.cfg.i2c_port, s_m5pm1.cfg.i2c_addr, buf, (uint16_t)(len + 1U), FALSE);
}

/**
 * @brief Check whether M5PM1 is initialized.
 * @return OPRT_OK when initialized, OPRT_COM_ERROR otherwise.
 */
static OPERATE_RET __m5pm1_check_init(void)
{
    if (!s_m5pm1.initialized) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/**
 * @brief Update selected bits in one M5PM1 register.
 * @param[in] reg register address.
 * @param[in] mask bit mask to update.
 * @param[in] value masked value to write.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_update_bits(uint8_t reg, uint8_t mask, uint8_t value)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(reg, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    reg_val &= (uint8_t)~mask;
    reg_val |= (uint8_t)(value & mask);

    return __m5pm1_write_reg(reg, reg_val);
}

/**
 * @brief Validate an M5PM1 GPIO number.
 * @param[in] pin GPIO number.
 * @return true when valid, false otherwise.
 */
static bool __m5pm1_is_valid_gpio(M5PM1_GPIO_NUM_E pin)
{
    return ((pin >= M5PM1_GPIO_NUM_0) && (pin < M5PM1_GPIO_NUM_MAX));
}

/**
 * @brief Check whether a GPIO wake source is enabled.
 * @param[in] pin GPIO number.
 * @param[out] enabled true when wake is enabled.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_gpio_get_wake_enabled(M5PM1_GPIO_NUM_E pin, bool *enabled)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    if (!__m5pm1_is_valid_gpio(pin) || (enabled == NULL)) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_GPIO_WAKE_EN, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    *enabled = ((reg_val & (uint8_t)(1U << pin)) != 0);
    return OPRT_OK;
}

/**
 * @brief Convert clean behavior into write-0-to-clear register write.
 * @param[in] reg status register address.
 * @param[in] status current status value.
 * @param[in] clean clean behavior.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_clean_status(uint8_t reg, uint8_t status, M5PM1_CLEAN_E clean)
{
    if (clean == M5PM1_CLEAN_NONE) {
        return OPRT_OK;
    }

    if (clean == M5PM1_CLEAN_ONCE) {
        if (status == 0) {
            return OPRT_OK;
        }

        return __m5pm1_write_reg(reg, (uint8_t)~status);
    }

    if (clean == M5PM1_CLEAN_ALL) {
        return __m5pm1_write_reg(reg, 0x00);
    }

    return OPRT_INVALID_PARM;
}

/**
 * @brief Get M5PM1 IRQ status register for a domain.
 * @param[in] domain IRQ domain.
 * @param[out] status_reg status register.
 * @param[out] mask_reg mask register.
 * @param[out] valid_mask valid bits mask.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_get_irq_regs(M5PM1_IRQ_DOMAIN_E domain, uint8_t *status_reg, uint8_t *mask_reg,
                                        uint8_t *valid_mask)
{
    if ((status_reg == NULL) || (mask_reg == NULL) || (valid_mask == NULL)) {
        return OPRT_INVALID_PARM;
    }

    switch (domain) {
    case M5PM1_IRQ_DOMAIN_GPIO:
        *status_reg = M5PM1_REG_IRQ_STATUS1;
        *mask_reg = M5PM1_REG_IRQ_MASK1;
        *valid_mask = M5PM1_IRQ_GPIO_VALID_MASK;
        break;
    case M5PM1_IRQ_DOMAIN_SYS:
        *status_reg = M5PM1_REG_IRQ_STATUS2;
        *mask_reg = M5PM1_REG_IRQ_MASK2;
        *valid_mask = M5PM1_IRQ_SYS_VALID_MASK;
        break;
    case M5PM1_IRQ_DOMAIN_BTN:
        *status_reg = M5PM1_REG_IRQ_STATUS3;
        *mask_reg = M5PM1_REG_IRQ_MASK3;
        *valid_mask = M5PM1_IRQ_BTN_VALID_MASK;
        break;
    default:
        return OPRT_INVALID_PARM;
    }

    return OPRT_OK;
}

/**
 * @brief Initialize M5PM1.
 * @param[in] cfg M5PM1 bus configuration.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_init(const M5PM1_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_IIC_BASE_CFG_T i2c_cfg = {0};
    uint8_t device_id = 0;

    if (cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    s_m5pm1.cfg = *cfg;

    i2c_cfg.role = TUYA_IIC_MODE_MASTER;
    i2c_cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    i2c_cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;
    rt = tkl_i2c_init(s_m5pm1.cfg.i2c_port, &i2c_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("m5pm1 i2c init failed:%d", rt);
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_DEVICE_ID, &device_id);
    if (rt != OPRT_OK) {
        PR_ERR("m5pm1 read device id failed:%d", rt);
        return rt;
    }

    s_m5pm1.initialized = true;
    PR_INFO("m5pm1 initialized, device id:0x%02x", device_id);

    return OPRT_OK;
}

/**
 * @brief Enable or disable M5PM1 battery charging.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_charge_enable(bool enable)
{
    return __m5pm1_update_bits(M5PM1_REG_PWR_CFG, M5PM1_PWR_CFG_CHG_EN,
                               enable ? M5PM1_PWR_CFG_CHG_EN : 0);
}

/**
 * @brief Enable or disable M5PM1 5V DCDC output.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_dcdc_enable(bool enable)
{
    return __m5pm1_update_bits(M5PM1_REG_PWR_CFG, M5PM1_PWR_CFG_DCDC_EN,
                               enable ? M5PM1_PWR_CFG_DCDC_EN : 0);
}

/**
 * @brief Enable or disable M5PM1 3.3V LDO output.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_ldo_enable(bool enable)
{
    return __m5pm1_update_bits(M5PM1_REG_PWR_CFG, M5PM1_PWR_CFG_LDO_EN,
                               enable ? M5PM1_PWR_CFG_LDO_EN : 0);
}

/**
 * @brief Enable or disable M5PM1 BOOST/Grove 5V output.
 * @param[in] enable true to enable output mode, false to leave the port as input mode.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_boost_enable(bool enable)
{
    return __m5pm1_update_bits(M5PM1_REG_PWR_CFG, M5PM1_PWR_CFG_BOOST_EN,
                               enable ? M5PM1_PWR_CFG_BOOST_EN : 0);
}

/**
 * @brief Set M5PM1 LED_EN default level.
 * @param[in] high true for high level, false for low level.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_led_en_level(bool high)
{
    return __m5pm1_update_bits(M5PM1_REG_PWR_CFG, M5PM1_PWR_CFG_LED_EN, high ? M5PM1_PWR_CFG_LED_EN : 0);
}

/**
 * @brief Get current M5PM1 power source.
 * @param[out] src power source.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_get_power_source(M5PM1_PWR_SRC_E *src)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    if (src == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_PWR_SRC, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    *src = (M5PM1_PWR_SRC_E)(reg_val & 0x07U);
    return OPRT_OK;
}

/**
 * @brief Read M5PM1 wake source flags.
 * @param[out] src wake source bitmask.
 * @param[in] clean read-and-clear behavior.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_get_wake_source(uint8_t *src, M5PM1_CLEAN_E clean)
{
    OPERATE_RET rt = OPRT_OK;

    if (src == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_WAKE_SRC, src);
    if (rt != OPRT_OK) {
        return rt;
    }

    return __m5pm1_clean_status(M5PM1_REG_WAKE_SRC, *src, clean);
}

/**
 * @brief Clear selected M5PM1 wake source flags.
 * @param[in] mask wake source bits to clear.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_clear_wake_source(uint8_t mask)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_WAKE_SRC, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    return __m5pm1_write_reg(M5PM1_REG_WAKE_SRC, (uint8_t)(reg_val & (uint8_t)~mask));
}

/**
 * @brief Configure raw M5PM1 power configuration bits.
 * @param[in] mask bit mask to update.
 * @param[in] value masked value to write.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_power_config(uint8_t mask, uint8_t value)
{
    return __m5pm1_update_bits(M5PM1_REG_PWR_CFG, mask, value);
}

/**
 * @brief Read raw M5PM1 power configuration bits.
 * @param[out] config power configuration register value.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_get_power_config(uint8_t *config)
{
    OPERATE_RET rt = OPRT_OK;

    if (config == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    return __m5pm1_read_reg(M5PM1_REG_PWR_CFG, config);
}

/**
 * @brief Clear selected M5PM1 power configuration bits.
 * @param[in] mask bit mask to clear.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_clear_power_config(uint8_t mask)
{
    return __m5pm1_update_bits(M5PM1_REG_PWR_CFG, mask, 0);
}

/**
 * @brief Set M5PM1 battery low-voltage protection threshold.
 * @param[in] mv threshold in millivolts, 2000 to 4000.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_battery_lvp(uint16_t mv)
{
    uint32_t value = 0;

    if ((mv < 2000U) || (mv > 4000U)) {
        return OPRT_INVALID_PARM;
    }

    if (__m5pm1_check_init() != OPRT_OK) {
        return OPRT_COM_ERROR;
    }

    value = ((uint32_t)(mv - 2000U) * 100U) / 781U;
    return __m5pm1_write_reg(M5PM1_REG_BATT_LVP, (uint8_t)value);
}

/**
 * @brief Configure an M5PM1 GPIO function.
 * @param[in] pin GPIO number.
 * @param[in] func GPIO function.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_func(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_FUNC_E func)
{
    uint8_t reg = 0;
    uint8_t shift = 0;

    if (!__m5pm1_is_valid_gpio(pin) || (func > M5PM1_GPIO_FUNC_SPECIAL)) {
        return OPRT_INVALID_PARM;
    }

    if (pin < M5PM1_GPIO_NUM_4) {
        reg = M5PM1_REG_GPIO_FUNC0;
        shift = (uint8_t)pin * 2;
    } else {
        reg = M5PM1_REG_GPIO_FUNC1;
        shift = 0;
    }

    return __m5pm1_update_bits(reg, (uint8_t)(0x03U << shift), (uint8_t)((uint8_t)func << shift));
}

/**
 * @brief Configure an M5PM1 GPIO direction.
 * @param[in] pin GPIO number.
 * @param[in] mode GPIO direction.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_mode(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_MODE_E mode)
{
    if (!__m5pm1_is_valid_gpio(pin) || (mode > M5PM1_GPIO_MODE_OUTPUT)) {
        return OPRT_INVALID_PARM;
    }

    return __m5pm1_update_bits(M5PM1_REG_GPIO_MODE, (uint8_t)(1U << pin),
                               (mode == M5PM1_GPIO_MODE_OUTPUT) ? (uint8_t)(1U << pin) : 0);
}

/**
 * @brief Configure an M5PM1 GPIO drive mode.
 * @param[in] pin GPIO number.
 * @param[in] drive GPIO drive mode.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_drive(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_DRIVE_E drive)
{
    if (!__m5pm1_is_valid_gpio(pin) || (drive > M5PM1_GPIO_DRIVE_OPENDRAIN)) {
        return OPRT_INVALID_PARM;
    }

    return __m5pm1_update_bits(M5PM1_REG_GPIO_DRV, (uint8_t)(1U << pin),
                               (drive == M5PM1_GPIO_DRIVE_OPENDRAIN) ? (uint8_t)(1U << pin) : 0);
}

/**
 * @brief Set an M5PM1 GPIO output level.
 * @param[in] pin GPIO number.
 * @param[in] high true for high level, false for low level.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_output(M5PM1_GPIO_NUM_E pin, bool high)
{
    if (!__m5pm1_is_valid_gpio(pin)) {
        return OPRT_INVALID_PARM;
    }

    return __m5pm1_update_bits(M5PM1_REG_GPIO_OUT, (uint8_t)(1U << pin), high ? (uint8_t)(1U << pin) : 0);
}

/**
 * @brief Get an M5PM1 GPIO input level.
 * @param[in] pin GPIO number.
 * @param[out] high true when input is high, false when input is low.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_get_input(M5PM1_GPIO_NUM_E pin, bool *high)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    if (!__m5pm1_is_valid_gpio(pin) || (high == NULL)) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_GPIO_IN, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    *high = ((reg_val & (uint8_t)(1U << pin)) != 0);
    return OPRT_OK;
}

/**
 * @brief Configure an M5PM1 GPIO pull resistor.
 * @param[in] pin GPIO number.
 * @param[in] pull pull mode.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_pull(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_PULL_E pull)
{
    uint8_t reg = 0;
    uint8_t shift = 0;

    if (!__m5pm1_is_valid_gpio(pin) || (pull > M5PM1_GPIO_PULL_DOWN)) {
        return OPRT_INVALID_PARM;
    }

    if (pin < M5PM1_GPIO_NUM_4) {
        reg = M5PM1_REG_GPIO_PUPD0;
        shift = (uint8_t)pin * 2U;
    } else {
        reg = M5PM1_REG_GPIO_PUPD1;
        shift = 0;
    }

    return __m5pm1_update_bits(reg, (uint8_t)(0x03U << shift), (uint8_t)((uint8_t)pull << shift));
}

/**
 * @brief Enable or disable an M5PM1 GPIO wake source.
 * @param[in] pin GPIO number.
 * @param[in] enable true to enable wake, false to disable wake.
 * @return OPRT_OK on success, error code on failure.
 * @note GPIO1 does not support wake. GPIO0/GPIO2 and GPIO3/GPIO4 are mutually exclusive wake pairs.
 */
OPERATE_RET m5pm1_gpio_set_wake_enable(M5PM1_GPIO_NUM_E pin, bool enable)
{
    OPERATE_RET rt = OPRT_OK;
    bool pair_enabled = false;

    if (!__m5pm1_is_valid_gpio(pin)) {
        return OPRT_INVALID_PARM;
    }

    if (!enable) {
        return __m5pm1_update_bits(M5PM1_REG_GPIO_WAKE_EN, (uint8_t)(1U << pin), 0);
    }

    if (pin == M5PM1_GPIO_NUM_1) {
        return OPRT_INVALID_PARM;
    }

    if (pin == M5PM1_GPIO_NUM_0) {
        rt = __m5pm1_gpio_get_wake_enabled(M5PM1_GPIO_NUM_2, &pair_enabled);
    } else if (pin == M5PM1_GPIO_NUM_2) {
        rt = __m5pm1_gpio_get_wake_enabled(M5PM1_GPIO_NUM_0, &pair_enabled);
    } else if (pin == M5PM1_GPIO_NUM_3) {
        rt = __m5pm1_gpio_get_wake_enabled(M5PM1_GPIO_NUM_4, &pair_enabled);
    } else {
        rt = __m5pm1_gpio_get_wake_enabled(M5PM1_GPIO_NUM_3, &pair_enabled);
    }
    if (rt != OPRT_OK) {
        return rt;
    }
    if (pair_enabled) {
        PR_ERR("m5pm1 wake source conflict on GPIO%d", pin);
        return OPRT_COM_ERROR;
    }

    return __m5pm1_update_bits(M5PM1_REG_GPIO_WAKE_EN, (uint8_t)(1U << pin), (uint8_t)(1U << pin));
}

/**
 * @brief Configure M5PM1 GPIO wake edge.
 * @param[in] pin GPIO number.
 * @param[in] edge wake edge.
 * @return OPRT_OK on success, error code on failure.
 * @note GPIO1 does not support wake edge configuration.
 */
OPERATE_RET m5pm1_gpio_set_wake_edge(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_WAKE_EDGE_E edge)
{
    if (!__m5pm1_is_valid_gpio(pin) || (edge > M5PM1_GPIO_WAKE_RISING) || (pin == M5PM1_GPIO_NUM_1)) {
        return OPRT_INVALID_PARM;
    }

    return __m5pm1_update_bits(M5PM1_REG_GPIO_WAKE_CFG, (uint8_t)(1U << pin),
                               (edge == M5PM1_GPIO_WAKE_RISING) ? (uint8_t)(1U << pin) : 0);
}

/**
 * @brief Enable or disable M5PM1 GPIO power hold during sleep.
 * @param[in] pin GPIO number.
 * @param[in] enable true to hold, false to release hold.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_power_hold(M5PM1_GPIO_NUM_E pin, bool enable)
{
    if (!__m5pm1_is_valid_gpio(pin)) {
        return OPRT_INVALID_PARM;
    }

    return __m5pm1_update_bits(M5PM1_REG_HOLD_CFG, (uint8_t)(1U << pin), enable ? (uint8_t)(1U << pin) : 0);
}

/**
 * @brief Read M5PM1 GPIO power hold state.
 * @param[in] pin GPIO number.
 * @param[out] enable true when hold is enabled.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_get_power_hold(M5PM1_GPIO_NUM_E pin, bool *enable)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    if (!__m5pm1_is_valid_gpio(pin) || (enable == NULL)) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_HOLD_CFG, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    *enable = ((reg_val & (uint8_t)(1U << pin)) != 0);
    return OPRT_OK;
}

/**
 * @brief Enable or disable M5PM1 LDO power hold during sleep.
 * @param[in] enable true to hold, false to release hold.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_ldo_set_power_hold(bool enable)
{
    return __m5pm1_update_bits(M5PM1_REG_HOLD_CFG, M5PM1_HOLD_CFG_LDO, enable ? M5PM1_HOLD_CFG_LDO : 0);
}

/**
 * @brief Read M5PM1 LDO power hold state.
 * @param[out] enable true when hold is enabled.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_ldo_get_power_hold(bool *enable)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    if (enable == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_HOLD_CFG, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    *enable = ((reg_val & M5PM1_HOLD_CFG_LDO) != 0);
    return OPRT_OK;
}

/**
 * @brief Enable or disable M5PM1 BOOST power hold during sleep.
 * @param[in] enable true to hold, false to release hold.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_boost_set_power_hold(bool enable)
{
    return __m5pm1_update_bits(M5PM1_REG_HOLD_CFG, M5PM1_HOLD_CFG_BOOST, enable ? M5PM1_HOLD_CFG_BOOST : 0);
}

/**
 * @brief Read M5PM1 BOOST power hold state.
 * @param[out] enable true when hold is enabled.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_boost_get_power_hold(bool *enable)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    if (enable == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_HOLD_CFG, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    *enable = ((reg_val & M5PM1_HOLD_CFG_BOOST) != 0);
    return OPRT_OK;
}

/**
 * @brief Set M5PM1 watchdog timeout.
 * @param[in] timeout_sec timeout in seconds, 0 disables watchdog.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_wdt_set(uint8_t timeout_sec)
{
    OPERATE_RET rt = __m5pm1_check_init();

    if (rt != OPRT_OK) {
        return rt;
    }

    return __m5pm1_write_reg(M5PM1_REG_WDT_CNT, timeout_sec);
}

/**
 * @brief Feed M5PM1 watchdog.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_wdt_feed(void)
{
    OPERATE_RET rt = __m5pm1_check_init();

    if (rt != OPRT_OK) {
        return rt;
    }

    return __m5pm1_write_reg(M5PM1_REG_WDT_KEY, M5PM1_WDT_FEED_KEY);
}

/**
 * @brief Read M5PM1 watchdog countdown.
 * @param[out] count countdown in seconds.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_wdt_get_count(uint8_t *count)
{
    OPERATE_RET rt = OPRT_OK;

    if (count == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    return __m5pm1_read_reg(M5PM1_REG_WDT_CNT, count);
}

/**
 * @brief Configure M5PM1 timer.
 * @param[in] seconds timer count in seconds.
 * @param[in] action timer action.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_timer_set(uint32_t seconds, M5PM1_TIM_ACTION_E action)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t data[4] = {0};

    if ((seconds > M5PM1_TIM_MAX_SECONDS) || (action > M5PM1_TIM_ACTION_POWEROFF)) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    data[0] = (uint8_t)(seconds & 0xFFU);
    data[1] = (uint8_t)((seconds >> 8) & 0xFFU);
    data[2] = (uint8_t)((seconds >> 16) & 0xFFU);
    data[3] = (uint8_t)((seconds >> 24) & 0x7FU);

    rt = __m5pm1_write_bytes(M5PM1_REG_TIM_CNT_0, data, sizeof(data));
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_write_reg(M5PM1_REG_TIM_CFG, (uint8_t)(M5PM1_TIM_ARM | (uint8_t)action));
    if (rt != OPRT_OK) {
        return rt;
    }

    return __m5pm1_write_reg(M5PM1_REG_TIM_KEY, M5PM1_TIM_RELOAD_KEY);
}

/**
 * @brief Stop and clear M5PM1 timer.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_timer_clear(void)
{
    OPERATE_RET rt = __m5pm1_check_init();

    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_write_reg(M5PM1_REG_TIM_CFG, 0);
    if (rt != OPRT_OK) {
        return rt;
    }

    return __m5pm1_write_reg(M5PM1_REG_TIM_KEY, M5PM1_TIM_RELOAD_KEY);
}

/**
 * @brief Run an M5PM1 system command.
 * @param[in] cmd command value.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_sys_cmd(uint8_t cmd)
{
    OPERATE_RET rt = __m5pm1_check_init();

    if (rt != OPRT_OK) {
        return rt;
    }
    if (cmd > M5PM1_SYS_CMD_DL) {
        return OPRT_INVALID_PARM;
    }

    tal_system_sleep(M5PM1_SYS_CMD_DELAY_MS);
    return __m5pm1_write_reg(M5PM1_REG_SYS_CMD, (uint8_t)(M5PM1_SYS_CMD_KEY | cmd));
}

/**
 * @brief Request M5PM1 shutdown.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_shutdown(void)
{
    return m5pm1_sys_cmd(M5PM1_SYS_CMD_OFF);
}

/**
 * @brief Request M5PM1 reboot.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_reboot(void)
{
    return m5pm1_sys_cmd(M5PM1_SYS_CMD_RESET);
}

/**
 * @brief Request M5PM1 download mode.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_enter_download_mode(void)
{
    return m5pm1_sys_cmd(M5PM1_SYS_CMD_DL);
}

/**
 * @brief Read M5PM1 IRQ status.
 * @param[in] domain IRQ domain.
 * @param[out] status IRQ status bitmask.
 * @param[in] clean read-and-clear behavior.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_irq_get_status(M5PM1_IRQ_DOMAIN_E domain, uint8_t *status, M5PM1_CLEAN_E clean)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t status_reg = 0;
    uint8_t mask_reg = 0;
    uint8_t valid_mask = 0;

    if (status == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_get_irq_regs(domain, &status_reg, &mask_reg, &valid_mask);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(status_reg, status);
    if (rt != OPRT_OK) {
        return rt;
    }
    *status &= valid_mask;

    return __m5pm1_clean_status(status_reg, *status, clean);
}

/**
 * @brief Clear all IRQ status bits in an M5PM1 IRQ domain.
 * @param[in] domain IRQ domain.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_irq_clear_all(M5PM1_IRQ_DOMAIN_E domain)
{
    uint8_t status_reg = 0;
    uint8_t mask_reg = 0;
    uint8_t valid_mask = 0;

    if (__m5pm1_check_init() != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    if (__m5pm1_get_irq_regs(domain, &status_reg, &mask_reg, &valid_mask) != OPRT_OK) {
        return OPRT_INVALID_PARM;
    }

    return __m5pm1_write_reg(status_reg, 0x00);
}

/**
 * @brief Set all IRQ mask bits in an M5PM1 IRQ domain.
 * @param[in] domain IRQ domain.
 * @param[in] mask mask control.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_irq_set_mask_all(M5PM1_IRQ_DOMAIN_E domain, M5PM1_IRQ_MASK_E mask)
{
    uint8_t status_reg = 0;
    uint8_t mask_reg = 0;
    uint8_t valid_mask = 0;

    if (mask > M5PM1_IRQ_MASK_ENABLE) {
        return OPRT_INVALID_PARM;
    }
    if (__m5pm1_check_init() != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    if (__m5pm1_get_irq_regs(domain, &status_reg, &mask_reg, &valid_mask) != OPRT_OK) {
        return OPRT_INVALID_PARM;
    }

    return __m5pm1_write_reg(mask_reg, (mask == M5PM1_IRQ_MASK_ENABLE) ? valid_mask : 0);
}

/**
 * @brief Read raw IRQ mask bits in an M5PM1 IRQ domain.
 * @param[in] domain IRQ domain.
 * @param[out] mask IRQ mask bits.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_irq_get_mask_bits(M5PM1_IRQ_DOMAIN_E domain, uint8_t *mask)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t status_reg = 0;
    uint8_t mask_reg = 0;
    uint8_t valid_mask = 0;

    if (mask == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_get_irq_regs(domain, &status_reg, &mask_reg, &valid_mask);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(mask_reg, mask);
    if (rt != OPRT_OK) {
        return rt;
    }

    *mask &= valid_mask;
    return OPRT_OK;
}

/**
 * @brief Set M5PM1 I2C idle sleep timeout.
 * @param[in] seconds timeout in seconds, 0 disables sleep, 1 to 15 enables sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_i2c_sleep_time(uint8_t seconds)
{
    if (seconds > M5PM1_I2C_SLEEP_MASK) {
        return OPRT_INVALID_PARM;
    }

    return __m5pm1_update_bits(M5PM1_REG_I2C_CFG, M5PM1_I2C_SLEEP_MASK, seconds);
}

/**
 * @brief Read M5PM1 I2C idle sleep timeout.
 * @param[out] seconds timeout in seconds.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_get_i2c_sleep_time(uint8_t *seconds)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t reg_val = 0;

    if (seconds == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __m5pm1_check_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __m5pm1_read_reg(M5PM1_REG_I2C_CFG, &reg_val);
    if (rt != OPRT_OK) {
        return rt;
    }

    *seconds = reg_val & M5PM1_I2C_SLEEP_MASK;
    return OPRT_OK;
}
