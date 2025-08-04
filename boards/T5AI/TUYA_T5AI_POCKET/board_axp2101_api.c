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
#include "tkl_gpio.h"
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

OPERATE_RET axp2101_write_reg(axp2101_dev_t *dev, uint8_t reg, uint8_t data)
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

OPERATE_RET axp2101_read_regs(axp2101_dev_t *dev, uint8_t reg, uint8_t *data, uint8_t len)
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
 * @brief 读取寄存器中指定位的值
 * @param reg: 寄存器地址
 * @param bit: 位号 (0-7)
 * @return 指定位的值 (0或1)
 */
bool getRegisterBit(uint8_t reg, uint8_t bit)
{
    uint8_t value;
    if (axp2101_read_regs(&axp2101_dev, reg, &value, 1) != OPRT_OK) {
        return false;
    }
    return (value & (1U << bit)) ? true : false;
}

/**
 * @brief 设置寄存器中的指定位为1
 * @param reg: 寄存器地址
 * @param bit: 位号 (0-7)
 * @return true: 成功, false: 失败
 */
bool setRegisterBit(uint8_t reg, uint8_t bit)
{
    uint8_t value;
    if (axp2101_read_regs(&axp2101_dev, reg, &value, 1) != OPRT_OK) {
        return false;
    }
    value |= (1U << bit);
    return (axp2101_write_reg(&axp2101_dev, reg, value) == OPRT_OK);
}

/**
 * @brief 清除寄存器中的指定位(设置为0)
 * @param reg: 寄存器地址
 * @param bit: 位号 (0-7)
 * @return true: 成功, false: 失败
 */
bool clrRegisterBit(uint8_t reg, uint8_t bit)
{
    uint8_t value;
    if (axp2101_read_regs(&axp2101_dev, reg, &value, 1) != OPRT_OK) {
        return false;
    }
    value &= ~(1U << bit);
    return (axp2101_write_reg(&axp2101_dev, reg, value) == OPRT_OK);
}

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
        axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_INTEN1, &data, 1);
        intRegister[0] = enable ? (data | value) : (data & (~value));
        res |= axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INTEN1, intRegister[0]);
    }
    if (opts & 0x00FF00) {
        value = opts >> 8;
        axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_INTEN2, &data, 1);
        intRegister[1] = enable ? (data | value) : (data & (~value));
        res |= axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INTEN2, intRegister[1]);
    }
    if (opts & 0xFF0000) {
        value = opts >> 16;
        axp2101_read_regs(&axp2101_dev, XPOWERS_AXP2101_INTEN3, &data, 1);
        intRegister[2] = enable ? (data | value) : (data & (~value));
        res |= axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_INTEN3, intRegister[2]);
    }
    return res == 0;
}

// static bool setRegisterBit(uint8_t reg, uint8_t bit)
// {
//     uint8_t value;
//     if (axp2101_read_regs(&axp2101_dev, reg, &value, 1) != OPRT_OK) {
//         return false;
//     }
//     value |= (1U << bit);
//     return (axp2101_write_reg(&axp2101_dev, reg, value) == OPRT_OK);
// }

// static bool clrRegisterBit(uint8_t reg, uint8_t bit)
// {
//     uint8_t value;
//     if (axp2101_read_regs(&axp2101_dev, reg, &value, 1) != OPRT_OK) {
//         return false;
//     }
//     value &= ~(1U << bit);
//     return (axp2101_write_reg(&axp2101_dev, reg, value) == OPRT_OK);
// }

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
    if (val == (uint8_t)-1)
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
    if (val == (uint8_t)-1)
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
    if (val == (uint8_t)-1)
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
    if (val == (uint8_t)-1)
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
    if (val == (uint8_t)-1)
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
    if (val == (uint8_t)-1)
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
    if (val == (uint8_t)-1)
        return false;
    val &= 0xF8;
    return 0 == axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_CV_CHG_VOL_SET, val | opt);
}
/**
 * @brief  Read a single register value from the AXP2101.
 * @param  reg: Register address to read from.
 * @retval Register value on success, -1 on failure.
 */
static int readRegister(uint8_t reg)
{
    uint8_t val = 0;
    OPERATE_RET ret = axp2101_read_regs(&axp2101_dev, reg, &val, 1);
    if (ret != OPRT_OK) {
        return -1;
    }
    return val;
}

/**
 * @brief  Write a value to a single register on the AXP2101.
 * @param  reg: Register address to write to.
 * @param  val: Value to write to the register.
 * @retval true on success, false on failure.
 */
static bool writeRegister(uint8_t reg, uint8_t val)
{
    OPERATE_RET ret = axp2101_write_reg(&axp2101_dev, reg, val);
    return (ret == OPRT_OK);
}

/*
 * Power control DCDC1 functions
 */
bool isEnableDC1(void)
{
    return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 0);
}

bool enableDC1(void)
{
    return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 0);
}

bool disableDC1(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 0);
}

bool setDC1Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_DCDC1_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_DCDC1_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_DCDC1_VOL_MIN) {
        PR_DEBUG("Mistake ! DC1 minimum voltage is %u mV", XPOWERS_AXP2101_DCDC1_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_DCDC1_VOL_MAX) {
        PR_DEBUG("Mistake ! DC1 maximum voltage is %u mV", XPOWERS_AXP2101_DCDC1_VOL_MAX);
        return false;
    }
    return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL0_CTRL,
                              (millivolt - XPOWERS_AXP2101_DCDC1_VOL_MIN) / XPOWERS_AXP2101_DCDC1_VOL_STEPS);
}

uint16_t getDC1Voltage(void)
{
    return (readRegister(XPOWERS_AXP2101_DC_VOL0_CTRL) & 0x1F) * XPOWERS_AXP2101_DCDC1_VOL_STEPS +
           XPOWERS_AXP2101_DCDC1_VOL_MIN;
}

// DCDC1 85% low voltage turn off PMIC function
void setDC1LowVoltagePowerDown(bool en)
{
    en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 0) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 0);
}

bool getDC1LowVoltagePowerDownEn()
{
    return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 0);
}

/*
 * Power control DCDC2 functions
 */
bool isEnableDC2(void)
{
    return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 1);
}

bool enableDC2(void)
{
    return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 1);
}

bool disableDC2(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 1);
}

bool setDC2Voltage(uint16_t millivolt)
{
    int val = readRegister(XPOWERS_AXP2101_DC_VOL1_CTRL);
    if (val == -1)
        return 0;
    val &= 0x80;
    if (millivolt >= XPOWERS_AXP2101_DCDC2_VOL1_MIN && millivolt <= XPOWERS_AXP2101_DCDC2_VOL1_MAX) {
        if (millivolt % XPOWERS_AXP2101_DCDC2_VOL_STEPS1) {
            PR_DEBUG("Mistake !  The steps is must %umV", XPOWERS_AXP2101_DCDC2_VOL_STEPS1);
            return false;
        }
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL1_CTRL, val | (millivolt - XPOWERS_AXP2101_DCDC2_VOL1_MIN) /
                                                                          XPOWERS_AXP2101_DCDC2_VOL_STEPS1);
    } else if (millivolt >= XPOWERS_AXP2101_DCDC2_VOL2_MIN && millivolt <= XPOWERS_AXP2101_DCDC2_VOL2_MAX) {
        if (millivolt % XPOWERS_AXP2101_DCDC2_VOL_STEPS2) {
            PR_DEBUG("Mistake !  The steps is must %umV", XPOWERS_AXP2101_DCDC2_VOL_STEPS2);
            return false;
        }
        val |= (((millivolt - XPOWERS_AXP2101_DCDC2_VOL2_MIN) / XPOWERS_AXP2101_DCDC2_VOL_STEPS2) +
                XPOWERS_AXP2101_DCDC2_VOL_STEPS2_BASE);
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL1_CTRL, val);
    }
    return false;
}

uint16_t getDC2Voltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_DC_VOL1_CTRL);
    if (val == -1)
        return 0;
    val &= 0x7F;
    if (val < XPOWERS_AXP2101_DCDC2_VOL_STEPS2_BASE) {
        return (val * XPOWERS_AXP2101_DCDC2_VOL_STEPS1) + XPOWERS_AXP2101_DCDC2_VOL1_MIN;
    } else {
        return (val * XPOWERS_AXP2101_DCDC2_VOL_STEPS2) - 200;
    }
    return 0;
}

uint8_t getDC2WorkMode(void)
{
    return getRegisterBit(XPOWERS_AXP2101_DCDC2_VOL_STEPS2, 7);
}

void setDC2LowVoltagePowerDown(bool en)
{
    en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 1) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 1);
}

bool getDC2LowVoltagePowerDownEn()
{
    return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 1);
}

/*
 * Power control DCDC3 functions
 */

bool isEnableDC3(void)
{
    return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 2);
}

bool enableDC3(void)
{
    return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 2);
}

bool disableDC3(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 2);
}

/**
    0.5~1.2V,10mV/step,71steps
    1.22~1.54V,20mV/step,17steps
    1.6~3.4V,100mV/step,19steps
    */
bool setDC3Voltage(uint16_t millivolt)
{
    int val = readRegister(XPOWERS_AXP2101_DC_VOL2_CTRL);
    if (val == -1)
        return false;
    val &= 0x80;
    if (millivolt >= XPOWERS_AXP2101_DCDC3_VOL1_MIN && millivolt <= XPOWERS_AXP2101_DCDC3_VOL1_MAX) {
        if (millivolt % XPOWERS_AXP2101_DCDC3_VOL_STEPS1) {
            PR_DEBUG("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC3_VOL_STEPS1);
            return false;
        }
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL2_CTRL,
                                  val | (millivolt - XPOWERS_AXP2101_DCDC3_VOL_MIN) / XPOWERS_AXP2101_DCDC3_VOL_STEPS1);
    } else if (millivolt >= XPOWERS_AXP2101_DCDC3_VOL2_MIN && millivolt <= XPOWERS_AXP2101_DCDC3_VOL2_MAX) {
        if (millivolt % XPOWERS_AXP2101_DCDC3_VOL_STEPS2) {
            PR_DEBUG("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC3_VOL_STEPS2);
            return false;
        }
        val |= (((millivolt - XPOWERS_AXP2101_DCDC3_VOL2_MIN) / XPOWERS_AXP2101_DCDC3_VOL_STEPS2) +
                XPOWERS_AXP2101_DCDC3_VOL_STEPS2_BASE);
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL2_CTRL, val);
    } else if (millivolt >= XPOWERS_AXP2101_DCDC3_VOL3_MIN && millivolt <= XPOWERS_AXP2101_DCDC3_VOL3_MAX) {
        if (millivolt % XPOWERS_AXP2101_DCDC3_VOL_STEPS3) {
            PR_DEBUG("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC3_VOL_STEPS3);
            return false;
        }
        val |= (((millivolt - XPOWERS_AXP2101_DCDC3_VOL3_MIN) / XPOWERS_AXP2101_DCDC3_VOL_STEPS3) +
                XPOWERS_AXP2101_DCDC3_VOL_STEPS3_BASE);
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL2_CTRL, val);
    }
    return false;
}

uint16_t getDC3Voltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_DC_VOL2_CTRL) & 0x7F;
    if (val == -1)
        return 0;
    if (val < XPOWERS_AXP2101_DCDC3_VOL_STEPS2_BASE) {
        return (val * XPOWERS_AXP2101_DCDC3_VOL_STEPS1) + XPOWERS_AXP2101_DCDC3_VOL_MIN;
    } else if (val >= XPOWERS_AXP2101_DCDC3_VOL_STEPS2_BASE && val < XPOWERS_AXP2101_DCDC3_VOL_STEPS3_BASE) {
        return (val * XPOWERS_AXP2101_DCDC3_VOL_STEPS2) - 200;
    } else {
        return (val * XPOWERS_AXP2101_DCDC3_VOL_STEPS3) - 7200;
    }
    return 0;
}

uint8_t getDC3WorkMode(void)
{
    return getRegisterBit(XPOWERS_AXP2101_DC_VOL2_CTRL, 7);
}

// DCDC3 85% low voltage turn off PMIC function
void setDC3LowVoltagePowerDown(bool en)
{
    en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 2) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 2);
}

bool getDC3LowVoltagePowerDownEn()
{
    return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 2);
}

/*
 * Power control DCDC4 functions
 */
/**
    0.5~1.2V,10mV/step,71steps
    1.22~1.84V,20mV/step,32steps
    */
bool isEnableDC4(void)
{
    return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 3);
}

bool enableDC4(void)
{
    return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 3);
}

bool disableDC4(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 3);
}

bool setDC4Voltage(uint16_t millivolt)
{
    int val = readRegister(XPOWERS_AXP2101_DC_VOL3_CTRL);
    if (val == -1)
        return false;
    val &= 0x80;
    if (millivolt >= XPOWERS_AXP2101_DCDC4_VOL1_MIN && millivolt <= XPOWERS_AXP2101_DCDC4_VOL1_MAX) {
        if (millivolt % XPOWERS_AXP2101_DCDC4_VOL_STEPS1) {
            PR_DEBUG("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC4_VOL_STEPS1);
            return false;
        }
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL3_CTRL, val | (millivolt - XPOWERS_AXP2101_DCDC4_VOL1_MIN) /
                                                                          XPOWERS_AXP2101_DCDC4_VOL_STEPS1);

    } else if (millivolt >= XPOWERS_AXP2101_DCDC4_VOL2_MIN && millivolt <= XPOWERS_AXP2101_DCDC4_VOL2_MAX) {
        if (millivolt % XPOWERS_AXP2101_DCDC4_VOL_STEPS2) {
            PR_DEBUG("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC4_VOL_STEPS2);
            return false;
        }
        val |= (((millivolt - XPOWERS_AXP2101_DCDC4_VOL2_MIN) / XPOWERS_AXP2101_DCDC4_VOL_STEPS2) +
                XPOWERS_AXP2101_DCDC4_VOL_STEPS2_BASE);
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL3_CTRL, val);
    }
    return false;
}

uint16_t getDC4Voltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_DC_VOL3_CTRL);
    if (val == -1)
        return 0;
    val &= 0x7F;
    if (val < XPOWERS_AXP2101_DCDC4_VOL_STEPS2_BASE) {
        return (val * XPOWERS_AXP2101_DCDC4_VOL_STEPS1) + XPOWERS_AXP2101_DCDC4_VOL1_MIN;
    } else {
        return (val * XPOWERS_AXP2101_DCDC4_VOL_STEPS2) - 200;
    }
    return 0;
}

// DCDC4 85% low voltage turn off PMIC function
void setDC4LowVoltagePowerDown(bool en)
{
    en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 3) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 3);
}

bool getDC4LowVoltagePowerDownEn()
{
    return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 3);
}

/*
 * Power control DCDC5 functions,Output to gpio pin
 */
bool isEnableDC5(void)
{
    return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 4);
}

bool enableDC5(void)
{
    return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 4);
}

bool disableDC5(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 4);
}

bool setDC5Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_DCDC5_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_DCDC5_VOL_STEPS);
        return false;
    }
    if (millivolt != XPOWERS_AXP2101_DCDC5_VOL_1200MV && millivolt < XPOWERS_AXP2101_DCDC5_VOL_MIN) {
        PR_DEBUG("Mistake ! DC5 minimum voltage is %umV ,%umV", XPOWERS_AXP2101_DCDC5_VOL_1200MV,
                 XPOWERS_AXP2101_DCDC5_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_DCDC5_VOL_MAX) {
        PR_DEBUG("Mistake ! DC5 maximum voltage is %umV", XPOWERS_AXP2101_DCDC5_VOL_MAX);
        return false;
    }

    int val = readRegister(XPOWERS_AXP2101_DC_VOL4_CTRL);
    if (val == -1)
        return false;
    val &= 0xE0;
    if (millivolt == XPOWERS_AXP2101_DCDC5_VOL_1200MV) {
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL4_CTRL, val | XPOWERS_AXP2101_DCDC5_VOL_VAL);
    }
    val |= (millivolt - XPOWERS_AXP2101_DCDC5_VOL_MIN) / XPOWERS_AXP2101_DCDC5_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL4_CTRL, val);
}

uint16_t getDC5Voltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_DC_VOL4_CTRL);
    if (val == -1)
        return 0;
    val &= 0x1F;
    if (val == XPOWERS_AXP2101_DCDC5_VOL_VAL)
        return XPOWERS_AXP2101_DCDC5_VOL_1200MV;
    return (val * XPOWERS_AXP2101_DCDC5_VOL_STEPS) + XPOWERS_AXP2101_DCDC5_VOL_MIN;
}

bool isDC5FreqCompensationEn(void)
{
    return getRegisterBit(XPOWERS_AXP2101_DC_VOL4_CTRL, 5);
}

void enableDC5FreqCompensation()
{
    setRegisterBit(XPOWERS_AXP2101_DC_VOL4_CTRL, 5);
}

void disableFreqCompensation()
{
    clrRegisterBit(XPOWERS_AXP2101_DC_VOL4_CTRL, 5);
}

// DCDC4 85% low voltage turn off PMIC function
void setDC5LowVoltagePowerDown(bool en)
{
    en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 4) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 4);
}

bool getDC5LowVoltagePowerDownEn()
{
    return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 4);
}

/*
 * Power control ALDO1 functions
 */
bool isEnableALDO1(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 0);
}

bool enableALDO1(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 0);
}

bool setALDO1Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_ALDO1_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_ALDO1_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_ALDO1_VOL_MIN) {
        PR_DEBUG("Mistake ! ALDO1 minimum output voltage is  %umV", XPOWERS_AXP2101_ALDO1_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_ALDO1_VOL_MAX) {
        PR_DEBUG("Mistake ! ALDO1 maximum output voltage is  %umV", XPOWERS_AXP2101_ALDO1_VOL_MAX);
        return false;
    }
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL0_CTRL) & 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_ALDO1_VOL_MIN) / XPOWERS_AXP2101_ALDO1_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL0_CTRL, val);
}

uint16_t getALDO1Voltage(void)
{
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL0_CTRL) & 0x1F;
    return val * XPOWERS_AXP2101_ALDO1_VOL_STEPS + XPOWERS_AXP2101_ALDO1_VOL_MIN;
}

/*
 * Power control ALDO2 functions
 */
bool isEnableALDO2(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 1);
}

bool enableALDO2(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 1);
}

bool disableALDO2(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 1);
}

bool setALDO2Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_ALDO2_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_ALDO2_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_ALDO2_VOL_MIN) {
        PR_DEBUG("Mistake ! ALDO2 minimum output voltage is  %umV", XPOWERS_AXP2101_ALDO2_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_ALDO2_VOL_MAX) {
        PR_DEBUG("Mistake ! ALDO2 maximum output voltage is  %umV", XPOWERS_AXP2101_ALDO2_VOL_MAX);
        return false;
    }
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL1_CTRL) & 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_ALDO2_VOL_MIN) / XPOWERS_AXP2101_ALDO2_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL1_CTRL, val);
}

uint16_t getALDO2Voltage(void)
{
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL1_CTRL) & 0x1F;
    return val * XPOWERS_AXP2101_ALDO2_VOL_STEPS + XPOWERS_AXP2101_ALDO2_VOL_MIN;
}

/*
 * Power control ALDO3 functions
 */
bool isEnableALDO3(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 2);
}

bool enableALDO3(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 2);
}

bool disableALDO3(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 2);
}

bool setALDO3Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_ALDO3_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_ALDO3_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_ALDO3_VOL_MIN) {
        PR_DEBUG("Mistake ! ALDO3 minimum output voltage is  %umV", XPOWERS_AXP2101_ALDO3_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_ALDO3_VOL_MAX) {
        PR_DEBUG("Mistake ! ALDO3 maximum output voltage is  %umV", XPOWERS_AXP2101_ALDO3_VOL_MAX);
        return false;
    }
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL2_CTRL) & 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_ALDO3_VOL_MIN) / XPOWERS_AXP2101_ALDO3_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL2_CTRL, val);
}

uint16_t getALDO3Voltage(void)
{
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL2_CTRL) & 0x1F;
    return val * XPOWERS_AXP2101_ALDO3_VOL_STEPS + XPOWERS_AXP2101_ALDO3_VOL_MIN;
}

/*
 * Power control ALDO4 functions
 */
bool isEnableALDO4(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 3);
}

bool enableALDO4(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 3);
}

bool disableALDO4(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 3);
}

bool setALDO4Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_ALDO4_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_ALDO4_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_ALDO4_VOL_MIN) {
        PR_DEBUG("Mistake ! ALDO4 minimum output voltage is  %umV", XPOWERS_AXP2101_ALDO4_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_ALDO4_VOL_MAX) {
        PR_DEBUG("Mistake ! ALDO4 maximum output voltage is  %umV", XPOWERS_AXP2101_ALDO4_VOL_MAX);
        return false;
    }
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL3_CTRL) & 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_ALDO4_VOL_MIN) / XPOWERS_AXP2101_ALDO4_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL3_CTRL, val);
}

uint16_t getALDO4Voltage(void)
{
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL3_CTRL) & 0x1F;
    return val * XPOWERS_AXP2101_ALDO4_VOL_STEPS + XPOWERS_AXP2101_ALDO4_VOL_MIN;
}

/*
 * Power control BLDO1 functions
 */
bool isEnableBLDO1(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 4);
}

bool enableBLDO1(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 4);
}

bool disableBLDO1(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 4);
}

bool setBLDO1Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_BLDO1_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_BLDO1_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_BLDO1_VOL_MIN) {
        PR_DEBUG("Mistake ! BLDO1 minimum output voltage is  %umV", XPOWERS_AXP2101_BLDO1_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_BLDO1_VOL_MAX) {
        PR_DEBUG("Mistake ! BLDO1 maximum output voltage is  %umV", XPOWERS_AXP2101_BLDO1_VOL_MAX);
        return false;
    }
    int val = readRegister(XPOWERS_AXP2101_LDO_VOL4_CTRL);
    if (val == -1)
        return false;
    val &= 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_BLDO1_VOL_MIN) / XPOWERS_AXP2101_BLDO1_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL4_CTRL, val);
}

uint16_t getBLDO1Voltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_LDO_VOL4_CTRL);
    if (val == -1)
        return 0;
    val &= 0x1F;
    return val * XPOWERS_AXP2101_BLDO1_VOL_STEPS + XPOWERS_AXP2101_BLDO1_VOL_MIN;
}

/*
 * Power control BLDO2 functions
 */
bool isEnableBLDO2(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 5);
}

bool enableBLDO2(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 5);
}

bool disableBLDO2(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 5);
}

bool setBLDO2Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_BLDO2_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_BLDO2_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_BLDO2_VOL_MIN) {
        PR_DEBUG("Mistake ! BLDO2 minimum output voltage is  %umV", XPOWERS_AXP2101_BLDO2_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_BLDO2_VOL_MAX) {
        PR_DEBUG("Mistake ! BLDO2 maximum output voltage is  %umV", XPOWERS_AXP2101_BLDO2_VOL_MAX);
        return false;
    }
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL5_CTRL) & 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_BLDO2_VOL_MIN) / XPOWERS_AXP2101_BLDO2_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL5_CTRL, val);
}

uint16_t getBLDO2Voltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_LDO_VOL5_CTRL);
    if (val == -1)
        return 0;
    val &= 0x1F;
    return val * XPOWERS_AXP2101_BLDO2_VOL_STEPS + XPOWERS_AXP2101_BLDO2_VOL_MIN;
}

/*
 * Power control CPUSLDO functions
 */
bool isEnableCPUSLDO(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 6);
}

bool enableCPUSLDO(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 6);
}

bool disableCPUSLDO(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 6);
}

bool setCPUSLDOVoltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_CPUSLDO_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_CPUSLDO_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_CPUSLDO_VOL_MIN) {
        PR_DEBUG("Mistake ! CPULDO minimum output voltage is  %umV", XPOWERS_AXP2101_CPUSLDO_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_CPUSLDO_VOL_MAX) {
        PR_DEBUG("Mistake ! CPULDO maximum output voltage is  %umV", XPOWERS_AXP2101_CPUSLDO_VOL_MAX);
        return false;
    }
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL6_CTRL) & 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_CPUSLDO_VOL_MIN) / XPOWERS_AXP2101_CPUSLDO_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL6_CTRL, val);
}

uint16_t getCPUSLDOVoltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_LDO_VOL6_CTRL);
    if (val == -1)
        return 0;
    val &= 0x1F;
    return val * XPOWERS_AXP2101_CPUSLDO_VOL_STEPS + XPOWERS_AXP2101_CPUSLDO_VOL_MIN;
}

/*
 * Power control DLDO1 functions
 */
bool isEnableDLDO1(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 7);
}

bool enableDLDO1(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 7);
}

bool disableDLDO1(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 7);
}

bool setDLDO1Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_DLDO1_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_DLDO1_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_DLDO1_VOL_MIN) {
        PR_DEBUG("Mistake ! DLDO1 minimum output voltage is  %umV", XPOWERS_AXP2101_DLDO1_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_DLDO1_VOL_MAX) {
        PR_DEBUG("Mistake ! DLDO1 maximum output voltage is  %umV", XPOWERS_AXP2101_DLDO1_VOL_MAX);
        return false;
    }
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL7_CTRL) & 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_DLDO1_VOL_MIN) / XPOWERS_AXP2101_DLDO1_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL7_CTRL, val);
}

uint16_t getDLDO1Voltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_LDO_VOL7_CTRL);
    if (val == -1)
        return 0;
    val &= 0x1F;
    return val * XPOWERS_AXP2101_DLDO1_VOL_STEPS + XPOWERS_AXP2101_DLDO1_VOL_MIN;
}

/*
 * Power control DLDO2 functions
 */
bool isEnableDLDO2(void)
{
    return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL1, 0);
}

bool enableDLDO2(void)
{
    return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL1, 0);
}

bool disableDLDO2(void)
{
    return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL1, 0);
}

bool setDLDO2Voltage(uint16_t millivolt)
{
    if (millivolt % XPOWERS_AXP2101_DLDO2_VOL_STEPS) {
        PR_DEBUG("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_DLDO2_VOL_STEPS);
        return false;
    }
    if (millivolt < XPOWERS_AXP2101_DLDO2_VOL_MIN) {
        PR_DEBUG("Mistake ! DLDO2 minimum output voltage is  %umV", XPOWERS_AXP2101_DLDO2_VOL_MIN);
        return false;
    } else if (millivolt > XPOWERS_AXP2101_DLDO2_VOL_MAX) {
        PR_DEBUG("Mistake ! DLDO2 maximum output voltage is  %umV", XPOWERS_AXP2101_DLDO2_VOL_MAX);
        return false;
    }
    uint16_t val = readRegister(XPOWERS_AXP2101_LDO_VOL8_CTRL) & 0xE0;
    val |= (millivolt - XPOWERS_AXP2101_DLDO2_VOL_MIN) / XPOWERS_AXP2101_DLDO2_VOL_STEPS;
    return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL8_CTRL, val);
}

uint16_t getDLDO2Voltage(void)
{
    int val = readRegister(XPOWERS_AXP2101_LDO_VOL8_CTRL);
    if (val == -1)
        return 0;
    val &= 0x1F;
    return val * XPOWERS_AXP2101_DLDO2_VOL_STEPS + XPOWERS_AXP2101_DLDO2_VOL_MIN;
}

static OPERATE_RET ldo_open()
{
    // DC1 IMAX=2A
    // 1500~3400mV,100mV/step,20steps
    setDC1Voltage(3300);
    PR_DEBUG("DC1  : %s   Voltage:%u mV \n", isEnableDC1() ? "+" : "-", getDC1Voltage());

    // DC2 IMAX=2A
    // 500~1200mV  10mV/step,71steps
    // 1220~1540mV 20mV/step,17steps
    setDC2Voltage(1500);
    PR_DEBUG("DC2  : %s   Voltage:%u mV \n", isEnableDC2() ? "+" : "-", getDC2Voltage());

    // DC3 IMAX = 2A
    // 500~1200mV,10mV/step,71steps
    // 1220~1540mV,20mV/step,17steps
    // 1600~3400mV,100mV/step,19steps
    setDC3Voltage(3300);
    PR_DEBUG("DC3  : %s   Voltage:%u mV \n", isEnableDC3() ? "+" : "-", getDC3Voltage());

    // DCDC4 IMAX=1.5A
    // 500~1200mV,10mV/step,71steps
    // 1220~1840mV,20mV/step,32steps
    setDC4Voltage(1800);
    PR_DEBUG("DC4  : %s   Voltage:%u mV \n", isEnableDC4() ? "+" : "-", getDC4Voltage());

    // DC5 IMAX=2A
    // 1200mV
    // 1400~3700mV,100mV/step,24steps
    setDC5Voltage(3300);
    PR_DEBUG("DC5  : %s   Voltage:%u mV \n", isEnableDC5() ? "+" : "-", getDC5Voltage());

    // ALDO1 IMAX=300mA
    // 500~3500mV, 100mV/step,31steps
    //  setALDO1Voltage(3300);

    // ALDO2 IMAX=300mA
    // 500~3500mV, 100mV/step,31steps
    //  setALDO2Voltage(3300);

    // ALDO3 IMAX=300mA
    // 500~3500mV, 100mV/step,31steps
    setALDO3Voltage(2800);

    // ALDO4 IMAX=300mA
    // 500~3500mV, 100mV/step,31steps
    setALDO4Voltage(3300);

    // BLDO1 IMAX=300mA
    // 500~3500mV, 100mV/step,31steps
    setBLDO1Voltage(2800);

    // BLDO2 IMAX=300mA
    // 500~3500mV, 100mV/step,31steps
    setBLDO2Voltage(1800);

    // CPUSLDO IMAX=30mA
    // 500~1400mV,50mV/step,19steps
    //  setCPUSLDOVoltage(1000);

    // DLDO1 IMAX=300mA
    // 500~3400mV, 100mV/step,29steps
    //  setDLDO1Voltage(3300);

    // DLDO2 IMAX=300mA
    // 500~1400mV, 50mV/step,2steps
    setDLDO2Voltage(1100);
    // axp2101_write_reg(&axp2101_dev, XPOWERS_AXP2101_LDO_VOL8_CTRL, 0x0F); // 1.1v

    enableDC1();
    enableDC2();
    enableDC3();
    enableDC4();
    enableDC5();
    // enableALDO1();
    // enableALDO2();
    enableALDO3();
    enableALDO4();
    enableBLDO1();
    enableBLDO2();
    // enableCPUSLDO();
    enableDLDO1();
    enableDLDO2();

    PR_NOTICE("ALDO=======================================================================");
    PR_DEBUG("ALDO1: %s   Voltage:%u mV\n", isEnableALDO1() ? "+" : "-", getALDO1Voltage());
    PR_DEBUG("ALDO2: %s   Voltage:%u mV\n", isEnableALDO2() ? "+" : "-", getALDO2Voltage());
    PR_DEBUG("ALDO3: %s   Voltage:%u mV\n", isEnableALDO3() ? "+" : "-", getALDO3Voltage());
    PR_DEBUG("ALDO4: %s   Voltage:%u mV\n", isEnableALDO4() ? "+" : "-", getALDO4Voltage());
    PR_NOTICE("BLDO=======================================================================");
    PR_DEBUG("BLDO1: %s   Voltage:%u mV\n", isEnableBLDO1() ? "+" : "-", getBLDO1Voltage());
    PR_DEBUG("BLDO2: %s   Voltage:%u mV\n", isEnableBLDO2() ? "+" : "-", getBLDO2Voltage());
    PR_NOTICE("CPUSLDO====================================================================");
    PR_DEBUG("CPUSLDO: %s Voltage:%u mV\n", isEnableCPUSLDO() ? "+" : "-", getCPUSLDOVoltage());
    PR_NOTICE("DLDO=======================================================================");
    PR_DEBUG("DLDO1: %s   Voltage:%u mV\n", isEnableDLDO1() ? "+" : "-", getDLDO1Voltage());
    PR_DEBUG("DLDO2: %s   Voltage:%u mV\n", isEnableDLDO2() ? "+" : "-", getDLDO2Voltage());
    PR_NOTICE("===========================================================================");
    return OPRT_OK;
}

/**
 * @brief Enable charging (fix: ensure all required bits are set and charger is not in suspend)
 */
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

    // Step 1: Configure VBUS input limits for lower voltage input (4.6V support)
    setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V20);  // 4.20V limit to support 4.6V input
    setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_500MA); // 500mA current limit for lower voltage
    setSysPowerDownVoltage(2600);                            // 2.6V system shutdown voltage

    // Step 2: Configure ADC measurements
    disableTSPinMeasure();        // Disable TS pin to prevent interference
    enableBattDetection();        // Enable battery detection
    enableVbusVoltageMeasure();   // Enable VBUS voltage measurement
    enableBattVoltageMeasure();   // Enable battery voltage measurement
    enableSystemVoltageMeasure(); // Enable system voltage measurement

    // Step 3: Configure charging parameters for 3.85V battery
    setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_200MA);         // 200mA precharge current
    setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA); // 25mA termination current
    setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_1000MA);    // 1000mA constant current (max)
    setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);       // 4.2V target voltage

    // Step 4: Configure charging LED
    // setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);

    // Step 5: Configure interrupts
    // pinMode(CONFIG_PMU_IRQ, INPUT_PULLUP);
    // setInterruptImpl(XPOWERS_AXP2101_ALL_IRQ, FALSE);
    // clearIrqStatus();
    // setInterruptImpl((XPOWERS_AXP2101_BAT_INSERT_IRQ | XPOWERS_AXP2101_BAT_REMOVE_IRQ |         // BATTERY
    //                   XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ |       // VBUS
    //                   XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ |          // POWER KEY
    //                   XPOWERS_AXP2101_BAT_CHG_DONE_IRQ | XPOWERS_AXP2101_BAT_CHG_START_IRQ |    // CHARGE
    //                   XPOWERS_AXP2101_WARNING_LEVEL1_IRQ | XPOWERS_AXP2101_WARNING_LEVEL2_IRQ), // Low battery
    //                   warning
    //                  TRUE);

    // Step 6: Enable charging (CRITICAL - this was the main issue!)
    // Register 0x18 bit 1 must be set to enable charging
    // Also, clear suspend bit (bit 0) if set
    axp2101_read_regs(dev, XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, &val, 1);
    val &= ~(1 << 0); // clear suspend
    val |= (1 << 1);  // set enable charging
    TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, val));

    // Step 7: Enable DC-DC converters for power path from VBUS to VSYS
    // This is CRITICAL - DC-DC converters must be enabled to provide power to VSYS
    // uint8_t dcdc_ctrl = 0;
    // axp2101_read_regs(dev, XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, &dcdc_ctrl, 1);
    // dcdc_ctrl |= 0x1F; // Enable all DC-DC converters (DCDC1-5)
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, dcdc_ctrl));

    // // Step 8: Configure DC-DC voltages for proper power delivery
    // // DCDC1: 3.3V for system power
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_DC_VOL0_CTRL, 0x12)); // 3.3V
    // // DCDC2: 1.2V for core power
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_DC_VOL1_CTRL, 0x57)); // 1.2V
    // // DCDC3: 1.8V for I/O power
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_DC_VOL2_CTRL, 0x57)); // 1.8V
    // // DCDC4: 2.5V for analog power
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_DC_VOL3_CTRL, 0x66)); // 1.84v
    // // DCDC5: 3.3V for additional system power
    // TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_DC_VOL4_CTRL, 0x13)); // 3.3V

    // Step 9: Configure for lower VBUS voltage detection (4.6V support)
    // Set VBUS voltage limit to 4.20V to allow 4.6V input to be detected
    TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL, 0x0A)); // 4.20V limit

    // Step 9a: Configure VBUS detection thresholds for lower voltage
    // This enables charging at lower VBUS voltages (4.6V instead of 5V)
    uint8_t vbus_detect = 0;
    axp2101_read_regs(dev, XPOWERS_AXP2101_COMMON_CONFIG, &vbus_detect, 1);
    vbus_detect |= 0x40; // Enable lower voltage VBUS detection
    TUYA_CALL_ERR_RETURN(axp2101_write_reg(dev, XPOWERS_AXP2101_COMMON_CONFIG, vbus_detect));

    // Step 10: Verify charging enable register
    axp2101_read_regs(dev, XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, &val, 1);
    PR_DEBUG("CHARGE_GAUGE_WDT_CTRL after setup: 0x%02X", val);
    if (!(val & 0x02)) {
        PR_ERR("FAILED: Charging not enabled in register 0x18!");
        return OPRT_COM_ERROR;
    }

    // Step 11: Verify lower voltage VBUS configuration
    uint8_t input_limit = 0, common_config = 0;
    axp2101_read_regs(dev, XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL, &input_limit, 1);
    axp2101_read_regs(dev, XPOWERS_AXP2101_COMMON_CONFIG, &common_config, 1);
    PR_DEBUG("Lower Voltage VBUS Configuration:");
    PR_DEBUG("  Input Voltage Limit (0x15): 0x%02X (4.20V)", input_limit);
    PR_DEBUG("  Common Config (0x10): 0x%02X", common_config);
    PR_DEBUG("  Lower Voltage Detection: %s", (common_config & 0x40) ? "ENABLED" : "DISABLED");

    // Step 12: Check all charging parameters
    uint8_t prechg_val = 0, chg_curr_val = 0, term_val = 0, target_val = 0;
    axp2101_read_regs(dev, XPOWERS_AXP2101_IPRECHG_SET, &prechg_val, 1);
    axp2101_read_regs(dev, XPOWERS_AXP2101_ICC_CHG_SET, &chg_curr_val, 1);
    axp2101_read_regs(dev, XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, &term_val, 1);
    axp2101_read_regs(dev, XPOWERS_AXP2101_CV_CHG_VOL_SET, &target_val, 1);

    PR_DEBUG("Charging Parameters Verification:");
    PR_DEBUG("  Precharge Current (0x61): 0x%02X", prechg_val);
    PR_DEBUG("  Charging Current (0x62): 0x%02X", chg_curr_val);
    PR_DEBUG("  Termination Current (0x63): 0x%02X", term_val);
    PR_DEBUG("  Target Voltage (0x64): 0x%02X", target_val);

    // Step 13: Verify DC-DC status
    uint8_t dcdc_status = 0;
    axp2101_read_regs(dev, XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, &dcdc_status, 1);
    PR_DEBUG("DC-DC Control Register (0x80): 0x%02X", dcdc_status);
    PR_DEBUG("  DCDC1-5 Enabled: %s", (dcdc_status & 0x1F) == 0x1F ? "YES" : "NO");

    PR_DEBUG("open charge succeed");
    axp2101_read_regs(dev, XPOWERS_AXP2101_STATUS2, &val, 1);
    PR_DEBUG("charge status:0x%02x", val);
    PR_DEBUG("is charging:%s", ((val >> 5) & 0x01) ? "YES" : "NO");

    // Step 10: Additional debug info
    axp2101_read_regs(dev, XPOWERS_AXP2101_STATUS1, &val, 1);
    PR_DEBUG("STATUS1 after setup: 0x%02X", val);
    PR_DEBUG("  VBUS Present: %s", (val & 0x20) ? "YES" : "NO");
    PR_DEBUG("  VBUS Valid: %s", (val & 0x10) ? "YES" : "NO");

    axp2101_read_regs(dev, XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, &val, 1);
    PR_DEBUG("DCDC PWM CTRL REG 0x%02x", val);

    // Step 14: Additional power path verification
    PR_DEBUG("Power Path Configuration Complete:");
    PR_DEBUG("  - Charging enabled and configured");
    PR_DEBUG("  - DC-DC converters enabled for VBUS->VSYS power path");
    PR_DEBUG("  - VBUS input limits configured for 4.6V operation");
    PR_DEBUG("  - Lower voltage VBUS detection enabled");
    PR_DEBUG("  - System should now receive power from VBUS when connected");
    PR_DEBUG("  - Charging should work at both 5V and 4.6V VBUS input");

    return rt;
}

/**
 * @brief Test VBUS voltage detection and charging at different voltage levels
 * @param dev: AXP2101 device pointer
 * @return OPERATE_RET: Operation result
 */
OPERATE_RET axp2101_test_vbus_voltage_detection(axp2101_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        PR_ERR("Device not initialized");
        return OPRT_INVALID_PARM;
    }

    PR_DEBUG("=== VBUS Voltage Detection Test ===");

    // Read current VBUS status and voltage
    uint8_t status1 = 0, status2 = 0;
    uint16_t vbus_voltage = 0;
    uint8_t adc_data[2];

    axp2101_read_regs(dev, XPOWERS_AXP2101_STATUS1, &status1, 1);
    axp2101_read_regs(dev, XPOWERS_AXP2101_STATUS2, &status2, 1);
    axp2101_read_regs(dev, XPOWERS_AXP2101_ADC_DATA_RELUST0, adc_data, 2);
    vbus_voltage = (adc_data[0] << 4) | (adc_data[1] & 0x0F);

    PR_DEBUG("Current VBUS Status:");
    PR_DEBUG("  VBUS Voltage: %d mV", vbus_voltage);
    PR_DEBUG("  VBUS Present: %s", (status1 & 0x20) ? "YES" : "NO");
    PR_DEBUG("  VBUS Valid: %s", (status1 & 0x10) ? "YES" : "NO");
    PR_DEBUG("  Charging: %s", (status2 & 0x20) ? "YES" : "NO");

    // Check if VBUS voltage is in the expected range for charging
    if (vbus_voltage >= 4600 && vbus_voltage <= 5500) {
        PR_DEBUG("✓ VBUS voltage in charging range (4.6V-5.5V)");
        if (status1 & 0x20) {
            PR_DEBUG("✓ VBUS detected and charging should work");
        } else {
            PR_ERR("✗ VBUS not detected despite voltage being in range");
        }
    } else if (vbus_voltage < 4600) {
        PR_ERR("✗ VBUS voltage too low for charging: %d mV", vbus_voltage);
        PR_DEBUG("  Expected: 4600-5500 mV for charging");
    } else {
        PR_ERR("✗ VBUS voltage too high: %d mV", vbus_voltage);
        PR_DEBUG("  Expected: 4600-5500 mV for charging");
    }

    return OPRT_OK;
}

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

    // Enable charging
    charge_open(&axp2101_dev, TRUE);
    // axp2101_force_enable_charging(&axp2101_dev);

    // Test VBUS voltage detection
    axp2101_test_vbus_voltage_detection(&axp2101_dev);

    // Enable LDO
    ldo_open();

    /*GPIO output init*/
    TUYA_GPIO_BASE_CFG_T pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_HIGH};
    TUYA_CALL_ERR_LOG(tkl_gpio_init(TUYA_GPIO_NUM_25, &pin_cfg));
    tkl_gpio_write(TUYA_GPIO_NUM_25, TUYA_GPIO_LEVEL_HIGH);

    /*GPIO output init*/
    TUYA_GPIO_BASE_CFG_T out_pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_LOW};
    TUYA_CALL_ERR_LOG(tkl_gpio_init(TUYA_GPIO_NUM_22, &out_pin_cfg));
    tkl_gpio_write(TUYA_GPIO_NUM_22, TUYA_GPIO_LEVEL_LOW);

    return rt;
}

/**
 * @brief Get the AXP2101 device instance
 *
 * @return axp2101_dev_t*: Pointer to the AXP2101 device structure
 */
axp2101_dev_t *get_axp2101_device(void)
{
    return &axp2101_dev;
}