/**
 * @file board_AXP2101_api.c
 * @author Tuya Inc.
 * @brief AXP2101 power management IC driver implementation for TUYA_T5AI_POCKET board
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "board_axp2101_api.h"
#include "board_axp2101_reg.h"

#include "tal_system.h"
#include "tal_log.h"
#include "tuya_error_code.h"

#include "tkl_i2c.h"

/***********************************************************
***********************variable define**********************
***********************************************************/
axp2101_dev_t axp2101_dev = {.i2c_port = TUYA_I2C_NUM_0, .i2c_addr = AXP2101_SLAVE_ADDRESS, .initialized = false};

typedef enum { PMU_GPIO0, PMU_GPIO1, PMU_GPIO2, PMU_GPIO3, PMU_GPIO4, PMU_GPIO5, PMU_TS_PIN } xpowers_axp192_num_t;

typedef struct {
    uint8_t mode;
} xpowers_gpio_t;

typedef enum {
    XPOWERS_AXP2101_PRECHARGE_0MA,
    XPOWERS_AXP2101_PRECHARGE_25MA,
    XPOWERS_AXP2101_PRECHARGE_50MA,
    XPOWERS_AXP2101_PRECHARGE_75MA,
    XPOWERS_AXP2101_PRECHARGE_100MA,
    XPOWERS_AXP2101_PRECHARGE_125MA,
    XPOWERS_AXP2101_PRECHARGE_150MA,
    XPOWERS_AXP2101_PRECHARGE_175MA,
    XPOWERS_AXP2101_PRECHARGE_200MA,
} xpowers_prechg_t;

typedef enum {
    XPOWERS_AXP2101_CHG_ITERM_0MA,
    XPOWERS_AXP2101_CHG_ITERM_25MA,
    XPOWERS_AXP2101_CHG_ITERM_50MA,
    XPOWERS_AXP2101_CHG_ITERM_75MA,
    XPOWERS_AXP2101_CHG_ITERM_100MA,
    XPOWERS_AXP2101_CHG_ITERM_125MA,
    XPOWERS_AXP2101_CHG_ITERM_150MA,
    XPOWERS_AXP2101_CHG_ITERM_175MA,
    XPOWERS_AXP2101_CHG_ITERM_200MA,
} xpowers_axp2101_chg_iterm_t;

#define XPOWERS_AXP192_DC1_VLOTAGE      (0x26)
#define XPOWERS_AXP192_LDO23OUT_VOL     (0x28)
#define XPOWERS_AXP192_GPIO0_CTL        (0x90)
#define XPOWERS_AXP192_GPIO0_VOL        (0x91)
#define XPOWERS_AXP192_GPIO1_CTL        (0X92)
#define XPOWERS_AXP192_GPIO2_CTL        (0x93)
#define XPOWERS_AXP192_GPIO012_SIGNAL   (0x94)
#define XPOWERS_AXP192_GPIO34_CTL       (0x95)
#define XPOWERS_AXP192_GPIO34_SIGNAL    (0x96)
#define XPOWERS_AXP192_GPIO012_PULLDOWN (0x97)
#define XPOWERS_AXP192_GPIO5_CTL        (0x9E)

// GPIO FUNCTIONS
#define INPUT          0x01
#define OUTPUT         0x03
#define PULLUP         0x04
#define INPUT_PULLUP   0x05
#define PULLDOWN       0x08
#define INPUT_PULLDOWN 0x09

xpowers_gpio_t gpio[6];
uint8_t statusRegister[XPOWERS_AXP2101_INTSTS_CNT];
#define CONFIG_PMU_IRQ 35
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __i2c_init(void)
{
    OPERATE_RET op_ret = OPRT_OK;
    TUYA_IIC_BASE_CFG_T cfg;

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    op_ret = tkl_i2c_init(TUYA_I2C_NUM_0, &cfg);
    if (op_ret != OPRT_OK) {
        PR_ERR("tkl_i2c_init failed: %d", op_ret);
        return op_ret;
    }
    return op_ret;
}

static OPERATE_RET axp2101_write_reg(axp2101_dev_t *dev, uint8_t reg, uint8_t data)
{
    OPERATE_RET ret;
    uint8_t buf[2];

    if (!dev) {
        return OPRT_INVALID_PARM;
    }

    buf[0] = reg;
    buf[1] = data;

    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, buf, 2, FALSE);
    if (ret < 0) {
        return ret;
    }

    return OPRT_OK;
}

static OPERATE_RET axp2101_read_regs(axp2101_dev_t *dev, uint8_t reg, uint8_t *data, uint8_t len)
{
    OPERATE_RET ret;

    if (!dev || !data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, &reg, 1, FALSE);
    if (ret < 0) {
        return ret;
    }

    ret = tkl_i2c_master_receive(dev->i2c_port, dev->i2c_addr, data, len, FALSE);
    if (ret < 0) {
        return ret;
    }

    return OPRT_OK;
}
/**
 * @brief Set charging led mode.
 * @retval See xpowers_chg_led_mode_t enum for details.
 */
// ... existing code ...

/**
 * @brief  Read a single register value from the AXP2101.
 * @param  reg: Register address to read from.
 * @retval Register value on success, -1 on failure.
 */
// static int axp2101_read_regs(uint8_t reg)
// {
//     uint8_t val = 0;
//     OPERATE_RET ret = axp2101_read_regs(&axp2101_dev, reg, &val, 1);
//     if (ret != OPRT_OK) {
//         return -1;
//     }
//     return val;
// }

// /**
//  * @brief  Write a value to a single register on the AXP2101.
//  * @param  reg: Register address to write to.
//  * @param  val: Value to write to the register.
//  * @retval true on success, false on failure.
//  */
// static bool axp2101_write_reg(uint8_t reg, uint8_t val)
// {
//     OPERATE_RET ret = axp2101_write_reg(&axp2101_dev, reg, val);
//     return (ret == OPRT_OK);
// }

// ... existing code ...

void setChargingLedMode(uint8_t mode)
{
    uint8_t val;
    switch (mode) {
    case XPOWERS_CHG_LED_OFF:
    // clrRegisterBit(XPOWERS_AXP2101_CHGLED_SET_CTRL, 0);
    // break;
    case XPOWERS_CHG_LED_BLINK_1HZ:
    case XPOWERS_CHG_LED_BLINK_4HZ:
    case XPOWERS_CHG_LED_ON:
        if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_CHGLED_SET_CTRL, &val, 1) != OPRT_OK) {
            return;
        }
        val &= 0xC8;
        val |= 0x05; // use manual ctrl
        val |= (mode << 4);
        axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_CHGLED_SET_CTRL, val);
        break;
    case XPOWERS_CHG_LED_CTRL_CHG:
        if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_CHGLED_SET_CTRL, &val, 1) != OPRT_OK) {
            return;
        }
        val &= 0xF9;
        axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_CHGLED_SET_CTRL, val | 0x01); // use type A mode
        // axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_CHGLED_SET_CTRL, val | 0x02); // use type B mode
        break;
    default:
        break;
    }
}

uint8_t getChargingLedMode()
{
    uint8_t val;
    if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_CHGLED_SET_CTRL, &val, 1) != OPRT_OK) {
        return XPOWERS_CHG_LED_OFF;
    }
    val >>= 1;
    if ((val & 0x02) == 0x02) {
        val >>= 4;
        return val & 0x03;
    }
    return XPOWERS_CHG_LED_CTRL_CHG;
}

// ... existing code ...
/*
 * GPIO setting
 */
/*
 * GPIO setting
 */
int8_t pinMode(uint8_t pin, uint8_t mode)
{
    uint8_t val = 0;
    switch (pin) {
    case PMU_GPIO0:
        /*
         * 000: NMOS open-drain output
         * 001: Universal input function
         * 010: Low noise LDO
         * 011: reserved
         * 100: ADC input
         * 101: Low output
         * 11X: Floating
         * * */
        if (mode == INPUT || mode == INPUT_PULLDOWN) {
            // Read current register value
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO0_CTL, &val, 1) != OPRT_OK) {
                return -1;
            }

            // Clear lower 3 bits and set to 001 for universal input function
            val = (val & 0xF8) | 0x01;
            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO0_CTL, val) != OPRT_OK) {
                return -1;
            }

            // Set pull-down mode
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO012_PULLDOWN, &val, 1) != OPRT_OK) {
                return -1;
            }

            val = val & 0xFE; // Clear bit 0
            if (mode == INPUT_PULLDOWN) {
                val |= 0x01; // Set bit 0 for pull-down
            }

            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO012_PULLDOWN, val) != OPRT_OK) {
                return -1;
            }
        }
        break;

    case PMU_GPIO1:
        /*
         * 000: NMOS open-drain output
         * 001: Universal input function
         * 010: PWM1 output, high level is VINT, not Can add less than 100K pull-down resistance
         * 011: reserved
         * 100: ADC input
         * 101: Low output
         * 11X: Floating
         * * */
        if (mode == INPUT || mode == INPUT_PULLDOWN) {
            // Read current register value
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO1_CTL, &val, 1) != OPRT_OK) {
                return -1;
            }

            // Clear lower 3 bits and set to 001 for universal input function
            val = (val & 0xF8) | 0x01;
            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO1_CTL, val) != OPRT_OK) {
                return -1;
            }

            // Set pull-down mode
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO012_PULLDOWN, &val, 1) != OPRT_OK) {
                return -1;
            }

            val = val & 0xFD; // Clear bit 1
            if (mode == INPUT_PULLDOWN) {
                val |= 0x02; // Set bit 1 for pull-down
            }

            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO012_PULLDOWN, val) != OPRT_OK) {
                return -1;
            }
        }
        break;

    case PMU_GPIO2:
        /*
         * 000: NMOS open-drain output
         * 001: Universal input function
         * 010: PWM2 output, high level is VINT, not Can add less than 100K pull-down resistance
         * 011: reserved
         * 100: ADC input
         * 101: Low output
         * 11X: Floating
         * */
        if (mode == INPUT || mode == INPUT_PULLDOWN) {
            // Read current register value
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO2_CTL, &val, 1) != OPRT_OK) {
                return -1;
            }

            // Clear lower 3 bits and set to 001 for universal input function
            val = (val & 0xF8) | 0x01;
            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO2_CTL, val) != OPRT_OK) {
                return -1;
            }

            // Set pull-down mode
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO012_PULLDOWN, &val, 1) != OPRT_OK) {
                return -1;
            }

            val = val & 0xFB; // Clear bit 2
            if (mode == INPUT_PULLDOWN) {
                val |= 0x04; // Set bit 2 for pull-down
            }

            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO012_PULLDOWN, val) != OPRT_OK) {
                return -1;
            }
        }
        break;

    case PMU_GPIO3:
        /*
         * 00: External charging control
         * 01: NMOS open-drain output port 3
         * 10: Universal input port 3
         * 11: ADC input
         * * */
        if (mode == INPUT) {
            // Read current register value
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO34_CTL, &val, 1) != OPRT_OK) {
                return -1;
            }

            // Clear lower 2 bits and set to 10 for universal input
            val = (val & 0xFC) | 0x02;
            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO34_CTL, val) != OPRT_OK) {
                return -1;
            }
        }
        break;

    case PMU_GPIO4:
        /*
         * 00: External charging control
         * 01: NMOS open-drain output port 4
         * 10: Universal input port 4
         * 11: undefined
         * * */
        if (mode == INPUT) {
            // Read current register value
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO34_CTL, &val, 1) != OPRT_OK) {
                return -1;
            }

            // Clear bits 2-3 and set to 10 for universal input
            val = (val & 0xF3) | 0x08;
            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO34_CTL, val) != OPRT_OK) {
                return -1;
            }
        }
        break;

    case PMU_GPIO5:
        if (mode == INPUT) {
            // Read current register value
            if (axp2101_read_regs(&axp2101_dev, XPOWERS_AXP192_GPIO5_CTL, &val, 1) != OPRT_OK) {
                return -1;
            }

            // Clear bit 6 and set to 1 for input mode
            val = (val & 0xBF) | 0x40;
            if (axp2101_write_reg(&axp2101_dev, XPOWERS_AXP192_GPIO5_CTL, val) != OPRT_OK) {
                return -1;
            }
        }
        break;

    default:
        break;
    }
    return 0;
}
/*
 * Interrupt control functions
 */
bool setInterruptImpl(uint32_t opts, bool enable)
{
    int res = 0;
    uint8_t data = 0, value = 0;
    uint8_t intRegister[XPOWERS_AXP2101_INTSTS_CNT];
    PR_DEBUG("%s - HEX:0x %lx \n", enable ? "ENABLE" : "DISABLE", opts);
    if (opts & 0x0000FF) {
        value = opts & 0xFF;
        // log_d("Write INT0: %x\n", value);
        axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_INTEN1, &data, 1);
        intRegister[0] = enable ? (data | value) : (data & (~value));
        res |= axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INTEN1, intRegister[0]);
    }
    if (opts & 0x00FF00) {
        value = opts >> 8;
        // log_d("Write INT1: %x\n", value);
        axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_INTEN2, &data, 1);
        intRegister[1] = enable ? (data | value) : (data & (~value));
        res |= axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INTEN2, intRegister[1]);
    }
    if (opts & 0xFF0000) {
        value = opts >> 16;
        // log_d("Write INT2: %x\n", value);
        axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_INTEN3, &data, 1);
        intRegister[2] = enable ? (data | value) : (data & (~value));
        res |= axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INTEN3, intRegister[2]);
    }
    return res == 0;
}
// ... existing code ...

static bool setRegisterBit(uint8_t reg, uint8_t bit)
{
    uint8_t value;
    if (axp2101_read_regs(&axp2101_dev, reg, &value, 1) != OPRT_OK) {
        return false;
    }
    value |= (1U << bit);
    return (axp2101_write_reg(&axp2101_dev, reg, value) == OPRT_OK);
}

static bool clrRegisterBit(uint8_t reg, uint8_t bit)
{
    uint8_t value;
    if (axp2101_read_regs(&axp2101_dev, reg, &value, 1) != OPRT_OK) {
        return false;
    }
    value &= ~(1U << bit);
    return (axp2101_write_reg(&axp2101_dev, reg, value) == OPRT_OK);
}

// ... existing code ...
/**
 * @brief  Clear interrupt controller state.
 */
void clearIrqStatus()
{
    for (int i = 0; i < XPOWERS_AXP2101_INTSTS_CNT; i++) {
        axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INTSTS1 + i, 0xFF);
        statusRegister[i] = 0;
    }
}
bool enableBattDetection(void)
{
    return setRegisterBit(XPOWERS_AXP2101_BAT_DET_CTRL, 0);
}
bool enableVbusVoltageMeasure(void)
{
    return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 2);
}
bool enableBattVoltageMeasure(void)
{
    return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 0);
}
bool enableSystemVoltageMeasure(void)
{
    return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 3);
}
bool disableTSPinMeasure(void)
{
    // TS pin is the external fixed input and doesn't affect the charger
    uint8_t value = 0;
    axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_TS_PIN_CTRL, &value, 1);
    value &= 0xF0;
    axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_TS_PIN_CTRL, value | 0x10);
    return clrRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 1);
}
/**
 * @brief  Set VBUS Voltage Input Limit.
 * @param  opt: View the related chip type xpowers_axp2101_vbus_vol_limit_t enumeration
 *              parameters in "XPowersParams.hpp"
 */
void setVbusVoltageLimit(uint8_t opt)
{
    uint8_t val = 0;
    axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL, &val, 1);
    if (val == -1)
        return;
    val &= 0xF0;
    axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL, val | (opt & 0x0F));
}
/**
 * @brief  Set VBUS Current Input Limit.
 * @param  opt: View the related chip type xpowers_axp2101_vbus_cur_limit_t enumeration
 *              parameters in "XPowersParams.hpp"
 * @retval true valid false invalid
 */
bool setVbusCurrentLimit(uint8_t opt)
{
    uint8_t val = 0;
    axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_INPUT_CUR_LIMIT_CTRL, &val, 1);
    if (val == -1)
        return false;
    val &= 0xF8;
    return 0 == axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INPUT_CUR_LIMIT_CTRL, val | (opt & 0x07));
}
// Set the minimum system operating voltage inside the PMU,
// below this value will shut down the PMU,Adjustment range 2600mV~3300mV
bool setSysPowerDownVoltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MIN) {
        PR_DEBUG("Mistake ! The minimum settable voltage of VSYS is %u mV", XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MAX) {
        PR_DEBUG("Mistake ! The maximum settable voltage of VSYS is %u mV", XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MAX);
        return false;
    }
    uint8_t val = 0;
    axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_VOFF_SET, &val, 1);
    if (val == -1)
        return false;
    val &= 0xF8;
    return 0 == axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_VOFF_SET,
                                  val | ((millivolt - XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MIN) /
                                         XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_STEPS));
}
/**
 * @brief 预充电充电电流限制
 * @note  Precharge current limit 25*N mA
 * @param  opt: 25 * opt
 * @retval None
 */
void setPrechargeCurr(xpowers_prechg_t opt)
{
    uint8_t val = 0;
    axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_IPRECHG_SET, &val, 1);
    if (val == -1)
        return;
    val &= 0xFC;
    axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_IPRECHG_SET, val | opt);
}
/**
 * @brief  充电终止电流限制
 * @note   Charging termination of current limit
 * @retval
 */
void setChargerTerminationCurr(xpowers_axp2101_chg_iterm_t opt)
{
    uint8_t val = 0;
    axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, &val, 1);
    if (val == -1)
        return;
    val &= 0xF0;
    axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, val | opt);
}
/**
 * @brief Set charge current.
 * @param  opt: See xpowers_axp2101_chg_curr_t enum for details.
 * @retval
 */
bool setChargerConstantCurr(uint8_t opt)
{
    if (opt > XPOWERS_AXP2101_CHG_CUR_1000MA)
        return false;
    uint8_t val = 0;
    axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_ICC_CHG_SET, &val, 1);
    if (val == -1)
        return false;
    val &= 0xE0;
    return 0 == axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_ICC_CHG_SET, val | opt);
}
/**
 * @brief Set charge target voltage.
 * @param  opt: See xpowers_axp2101_chg_vol_t enum for details.
 * @retval
 */
bool setChargeTargetVoltage(uint8_t opt)
{
    if (opt >= XPOWERS_AXP2101_CHG_VOL_MAX)
        return false;
    uint8_t val = 0;
    axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_CV_CHG_VOL_SET, &val, 1);
    if (val == -1)
        return false;
    val &= 0xF8;
    return 0 == axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_CV_CHG_VOL_SET, val | opt);
}
static OPERATE_RET charge_open(axp2101_dev_t *dev, bool enable)
{
    if (!dev || !dev->initialized) {
        PR_ERR("Device not initialized");
        return OPRT_INVALID_PARM;
    }
    uint8_t val = 0;

    OPERATE_RET rt = OPRT_OK;
    axp2101_read_regs(dev, XPOWERS_AXP2101_STATUS1, &val, 1);
    PR_DEBUG("vbus status: 0x%02x", val);
    if ((val & 0x20) == 0x20) {
        PR_DEBUG("vbus good!");
    }
    /*charge*/
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL, 0x0E)); //0x15 vbus input limit
    // 5v TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_INPUT_CUR_LIMIT_CTRL, 0x04)); //0x16 vbus input
    // limit 1500mA TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_VOFF_SET, 0x00)); //0x24 stop work
    // vol 2.6v

    setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_5V08);
    setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
    setSysPowerDownVoltage(2600);

    disableTSPinMeasure();
    // Enable internal ADC detection
    enableBattDetection();
    enableVbusVoltageMeasure();
    enableBattVoltageMeasure();
    enableSystemVoltageMeasure();
    setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);

    pinMode(CONFIG_PMU_IRQ, INPUT_PULLUP);
    setInterruptImpl(XPOWERS_AXP2101_ALL_IRQ, FALSE);
    clearIrqStatus();
    setInterruptImpl((XPOWERS_AXP2101_BAT_INSERT_IRQ | XPOWERS_AXP2101_BAT_REMOVE_IRQ |         // BATTERY
                      XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ |       // VBUS
                      XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ |          // POWER KEY
                      XPOWERS_AXP2101_BAT_CHG_DONE_IRQ | XPOWERS_AXP2101_BAT_CHG_START_IRQ |    // CHARGE
                      XPOWERS_AXP2101_WARNING_LEVEL1_IRQ | XPOWERS_AXP2101_WARNING_LEVEL2_IRQ), // Low battery warning
                     TRUE);

    setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_200MA);
    setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
    setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_1000MA);
    setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_IPRECHG_SET, 0x08)); //0x61 precharge current 200mA
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, 0x11)); //0x63 charge termination
    // current 25mA TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_ICC_CHG_SET, 0x0B)); //0x62 charge
    // current 500mA TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_CV_CHG_VOL_SET, 0x03)); //0x64 target
    // charge voltage 4.2V
    TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 0x02)); // 0x18 enable cell
                                                                                               // charge

    // pinMode(CONFIG_PMU_IRQ, INPUT);
    // setInterruptImpl(XPOWERS_AXP2101_ALL_IRQ, FALSE);
    // setInterruptImpl((XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   //BATTERY
    //                 XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   //VBUS
    //                 XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   //POWER KEY
    //                 XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ),       //CHARGE
    //                 TRUE);
    // clearIrqStatus();
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL, 0x0E)); //0x15 vbus input limit
    // 5v TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_INPUT_CUR_LIMIT_CTRL, 0x04)); //0x16 vbus input
    // limit 1500mA TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_VOFF_SET, 0x00)); //0x24 stop work
    // vol 2.6v TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_IPRECHG_SET, 0x08)); //0x61 precharge
    // current 200mA TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_ICC_CHG_SET, 0x0B)); //0x62 charge
    // current 500mA TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, 0x11)); //0x63
    // charge termination current 25mA TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_CV_CHG_VOL_SET,
    // 0x03)); //0x64 target charge voltage 4.2V

    // // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_CHGLED_SET_CTRL, 0x01)); //0x69 enable chg led

    // setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG); // use type A mode
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_BTN_BAT_CHG_VOL_SET, 0x07)); //0x6A button battery
    // charge voltage 3.3v TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 0x02));
    // //0x18 enable cell charge
    // // axp2101_write_reg(dev, XPOWERS_AXP2101_CHG_TIMEOUT_SET_CTRL, 0xD6); //0x67 charge timeout

    PR_DEBUG("open charge succeed");
    axp2101_read_regs(dev, XPOWERS_AXP2101_STATUS2, &val, 1);
    PR_DEBUG("charge status:0x%02x", val);
    PR_DEBUG("is charging:%s", ((val >> 5) == 0x01) ? "YES" : "NO");

    return rt;
}

// static void output_enable(axp2101_dev_t *dev, bool enable)
// {
//     if (TRUE == enable) {
//         axp2101_write_reg(dev, XPOWERS_AXP2101_FAST_PWRON_SET2, 0xCF); // Set fast power-on configuration
//         axp2101_write_reg(dev, XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 0x20); // Enable LDO2
//         axp2101_write_reg(dev, XPOWERS_AXP2101_LDO_VOL5_CTRL, 0x17);
//     }
// }

OPERATE_RET board_axp2101_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t chip_id = 0;

    TUYA_CALL_ERR_RETURN(__i2c_init());

    // 读取芯片ID寄存器
    rt = axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_IC_TYPE, &chip_id, 1);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to read AXP2101 chip ID: %d", rt);
        return rt;
    }

    // 检查芯片ID是否正确
    if (chip_id != XPOWERS_AXP2101_CHIP_ID) {
        PR_ERR("AXP2101 chip ID mismatch. Expected: 0x%02X, Read: 0x%02X", XPOWERS_AXP2101_CHIP_ID, chip_id);
        return OPRT_COM_ERROR;
    }

    axp2101_dev.initialized = true;
    PR_DEBUG("AXP2101 init succeed, chip ID: 0x%02X", chip_id);

    // axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 0x0F);
    charge_open(&axp2101_dev, TRUE); // Enable charging
    // output_enable(&axp2101_dev, TRUE);

    return rt;
}
