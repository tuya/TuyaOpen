/**
 * @file m5pm1_driver.h
 * @brief M5PM1 power management IC driver
 * @version 0.1
 * @date 2026-04-27
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */
#ifndef __M5PM1_DRIVER_H__
#define __M5PM1_DRIVER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define M5PM1_DEFAULT_ADDR (0x6E)

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef enum {
    M5PM1_GPIO_NUM_0 = 0,
    M5PM1_GPIO_NUM_1,
    M5PM1_GPIO_NUM_2,
    M5PM1_GPIO_NUM_3,
    M5PM1_GPIO_NUM_4,
    M5PM1_GPIO_NUM_MAX,
} M5PM1_GPIO_NUM_E;

typedef enum {
    M5PM1_GPIO_FUNC_GPIO = 0,
    M5PM1_GPIO_FUNC_IRQ,
    M5PM1_GPIO_FUNC_WAKE,
    M5PM1_GPIO_FUNC_SPECIAL,
} M5PM1_GPIO_FUNC_E;

typedef enum {
    M5PM1_GPIO_MODE_INPUT = 0,
    M5PM1_GPIO_MODE_OUTPUT,
} M5PM1_GPIO_MODE_E;

typedef enum {
    M5PM1_GPIO_DRIVE_PUSHPULL = 0,
    M5PM1_GPIO_DRIVE_OPENDRAIN,
} M5PM1_GPIO_DRIVE_E;

typedef enum {
    M5PM1_GPIO_PULL_NONE = 0,
    M5PM1_GPIO_PULL_UP,
    M5PM1_GPIO_PULL_DOWN,
} M5PM1_GPIO_PULL_E;

typedef enum {
    M5PM1_GPIO_WAKE_FALLING = 0,
    M5PM1_GPIO_WAKE_RISING,
} M5PM1_GPIO_WAKE_EDGE_E;

typedef enum {
    M5PM1_PWR_SRC_5VIN = 0,
    M5PM1_PWR_SRC_5VINOUT,
    M5PM1_PWR_SRC_BAT,
    M5PM1_PWR_SRC_UNKNOWN,
} M5PM1_PWR_SRC_E;

typedef enum {
    M5PM1_WAKE_SRC_TIMER = (1U << 0),
    M5PM1_WAKE_SRC_VIN = (1U << 1),
    M5PM1_WAKE_SRC_PWRBTN = (1U << 2),
    M5PM1_WAKE_SRC_RSTBTN = (1U << 3),
    M5PM1_WAKE_SRC_CMD_RESET = (1U << 4),
    M5PM1_WAKE_SRC_EXT_GPIO = (1U << 5),
    M5PM1_WAKE_SRC_5VINOUT = (1U << 6),
    M5PM1_WAKE_SRC_ALL = 0x7F,
} M5PM1_WAKE_SRC_E;

typedef enum {
    M5PM1_CLEAN_NONE = 0,
    M5PM1_CLEAN_ONCE,
    M5PM1_CLEAN_ALL,
} M5PM1_CLEAN_E;

typedef enum {
    M5PM1_TIM_ACTION_STOP = 0,
    M5PM1_TIM_ACTION_FLAG,
    M5PM1_TIM_ACTION_REBOOT,
    M5PM1_TIM_ACTION_POWERON,
    M5PM1_TIM_ACTION_POWEROFF,
} M5PM1_TIM_ACTION_E;

typedef enum {
    M5PM1_IRQ_MASK_DISABLE = 0,
    M5PM1_IRQ_MASK_ENABLE,
} M5PM1_IRQ_MASK_E;

typedef enum {
    M5PM1_IRQ_DOMAIN_GPIO = 0,
    M5PM1_IRQ_DOMAIN_SYS,
    M5PM1_IRQ_DOMAIN_BTN,
} M5PM1_IRQ_DOMAIN_E;

typedef struct {
    TUYA_I2C_NUM_E i2c_port;
    uint8_t i2c_addr;
} M5PM1_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialize M5PM1.
 * @param[in] cfg M5PM1 bus configuration.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_init(const M5PM1_CFG_T *cfg);

/**
 * @brief Enable or disable M5PM1 battery charging.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_charge_enable(bool enable);

/**
 * @brief Enable or disable M5PM1 5V DCDC output.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_dcdc_enable(bool enable);

/**
 * @brief Enable or disable M5PM1 3.3V LDO output.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_ldo_enable(bool enable);

/**
 * @brief Enable or disable M5PM1 BOOST/Grove 5V output.
 * @param[in] enable true to enable output mode, false to leave the port as input mode.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_boost_enable(bool enable);

/**
 * @brief Set M5PM1 LED_EN default level.
 * @param[in] high true for high level, false for low level.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_led_en_level(bool high);

/**
 * @brief Get current M5PM1 power source.
 * @param[out] src power source.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_get_power_source(M5PM1_PWR_SRC_E *src);

/**
 * @brief Read M5PM1 wake source flags.
 * @param[out] src wake source bitmask.
 * @param[in] clean read-and-clear behavior.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_get_wake_source(uint8_t *src, M5PM1_CLEAN_E clean);

/**
 * @brief Clear selected M5PM1 wake source flags.
 * @param[in] mask wake source bits to clear.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_clear_wake_source(uint8_t mask);

/**
 * @brief Configure raw M5PM1 power configuration bits.
 * @param[in] mask bit mask to update.
 * @param[in] value masked value to write.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_power_config(uint8_t mask, uint8_t value);

/**
 * @brief Read raw M5PM1 power configuration bits.
 * @param[out] config power configuration register value.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_get_power_config(uint8_t *config);

/**
 * @brief Clear selected M5PM1 power configuration bits.
 * @param[in] mask bit mask to clear.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_clear_power_config(uint8_t mask);

/**
 * @brief Set M5PM1 battery low-voltage protection threshold.
 * @param[in] mv threshold in millivolts, 2000 to 4000.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_battery_lvp(uint16_t mv);

/**
 * @brief Configure an M5PM1 GPIO function.
 * @param[in] pin GPIO number.
 * @param[in] func GPIO function.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_func(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_FUNC_E func);

/**
 * @brief Configure an M5PM1 GPIO direction.
 * @param[in] pin GPIO number.
 * @param[in] mode GPIO direction.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_mode(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_MODE_E mode);

/**
 * @brief Configure an M5PM1 GPIO drive mode.
 * @param[in] pin GPIO number.
 * @param[in] drive GPIO drive mode.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_drive(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_DRIVE_E drive);

/**
 * @brief Set an M5PM1 GPIO output level.
 * @param[in] pin GPIO number.
 * @param[in] high true for high level, false for low level.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_output(M5PM1_GPIO_NUM_E pin, bool high);

/**
 * @brief Get an M5PM1 GPIO input level.
 * @param[in] pin GPIO number.
 * @param[out] high true when input is high, false when input is low.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_get_input(M5PM1_GPIO_NUM_E pin, bool *high);

/**
 * @brief Configure an M5PM1 GPIO pull resistor.
 * @param[in] pin GPIO number.
 * @param[in] pull pull mode.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_pull(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_PULL_E pull);

/**
 * @brief Enable or disable an M5PM1 GPIO wake source.
 * @param[in] pin GPIO number.
 * @param[in] enable true to enable wake, false to disable wake.
 * @return OPRT_OK on success, error code on failure.
 * @note GPIO1 does not support wake. GPIO0/GPIO2 and GPIO3/GPIO4 are mutually exclusive wake pairs.
 */
OPERATE_RET m5pm1_gpio_set_wake_enable(M5PM1_GPIO_NUM_E pin, bool enable);

/**
 * @brief Configure M5PM1 GPIO wake edge.
 * @param[in] pin GPIO number.
 * @param[in] edge wake edge.
 * @return OPRT_OK on success, error code on failure.
 * @note GPIO1 does not support wake edge configuration.
 */
OPERATE_RET m5pm1_gpio_set_wake_edge(M5PM1_GPIO_NUM_E pin, M5PM1_GPIO_WAKE_EDGE_E edge);

/**
 * @brief Enable or disable M5PM1 GPIO power hold during sleep.
 * @param[in] pin GPIO number.
 * @param[in] enable true to hold, false to release hold.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_set_power_hold(M5PM1_GPIO_NUM_E pin, bool enable);

/**
 * @brief Read M5PM1 GPIO power hold state.
 * @param[in] pin GPIO number.
 * @param[out] enable true when hold is enabled.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_gpio_get_power_hold(M5PM1_GPIO_NUM_E pin, bool *enable);

/**
 * @brief Enable or disable M5PM1 LDO power hold during sleep.
 * @param[in] enable true to hold, false to release hold.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_ldo_set_power_hold(bool enable);

/**
 * @brief Read M5PM1 LDO power hold state.
 * @param[out] enable true when hold is enabled.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_ldo_get_power_hold(bool *enable);

/**
 * @brief Enable or disable M5PM1 BOOST power hold during sleep.
 * @param[in] enable true to hold, false to release hold.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_boost_set_power_hold(bool enable);

/**
 * @brief Read M5PM1 BOOST power hold state.
 * @param[out] enable true when hold is enabled.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_boost_get_power_hold(bool *enable);

/**
 * @brief Set M5PM1 watchdog timeout.
 * @param[in] timeout_sec timeout in seconds, 0 disables watchdog.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_wdt_set(uint8_t timeout_sec);

/**
 * @brief Feed M5PM1 watchdog.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_wdt_feed(void);

/**
 * @brief Read M5PM1 watchdog countdown.
 * @param[out] count countdown in seconds.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_wdt_get_count(uint8_t *count);

/**
 * @brief Configure M5PM1 timer.
 * @param[in] seconds timer count in seconds.
 * @param[in] action timer action.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_timer_set(uint32_t seconds, M5PM1_TIM_ACTION_E action);

/**
 * @brief Stop and clear M5PM1 timer.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_timer_clear(void);

/**
 * @brief Run an M5PM1 system command.
 * @param[in] cmd command value.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_sys_cmd(uint8_t cmd);

/**
 * @brief Request M5PM1 shutdown.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_shutdown(void);

/**
 * @brief Request M5PM1 reboot.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_reboot(void);

/**
 * @brief Request M5PM1 download mode.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_enter_download_mode(void);

/**
 * @brief Read M5PM1 IRQ status.
 * @param[in] domain IRQ domain.
 * @param[out] status IRQ status bitmask.
 * @param[in] clean read-and-clear behavior.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_irq_get_status(M5PM1_IRQ_DOMAIN_E domain, uint8_t *status, M5PM1_CLEAN_E clean);

/**
 * @brief Clear all IRQ status bits in an M5PM1 IRQ domain.
 * @param[in] domain IRQ domain.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_irq_clear_all(M5PM1_IRQ_DOMAIN_E domain);

/**
 * @brief Set all IRQ mask bits in an M5PM1 IRQ domain.
 * @param[in] domain IRQ domain.
 * @param[in] mask mask control.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_irq_set_mask_all(M5PM1_IRQ_DOMAIN_E domain, M5PM1_IRQ_MASK_E mask);

/**
 * @brief Read raw IRQ mask bits in an M5PM1 IRQ domain.
 * @param[in] domain IRQ domain.
 * @param[out] mask IRQ mask bits.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_irq_get_mask_bits(M5PM1_IRQ_DOMAIN_E domain, uint8_t *mask);

/**
 * @brief Set M5PM1 I2C idle sleep timeout.
 * @param[in] seconds timeout in seconds, 0 disables sleep, 1 to 15 enables sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_set_i2c_sleep_time(uint8_t seconds);

/**
 * @brief Read M5PM1 I2C idle sleep timeout.
 * @param[out] seconds timeout in seconds.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET m5pm1_get_i2c_sleep_time(uint8_t *seconds);

#ifdef __cplusplus
}
#endif
#endif /* __M5PM1_DRIVER_H__ */
