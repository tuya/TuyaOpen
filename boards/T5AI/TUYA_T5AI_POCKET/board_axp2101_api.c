/**
 * @file board_AXP2101_api.c
 * @author Tuya Inc.
 * @brief AXP2101 power management IC driver implementation for TUYA_T5AI_POCKET board
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "board_axp2101_api.h"
#include "axp2101_driver.h"
#include "axp2101_reg.h"

#include "tal_system.h"
#include "tal_log.h"
#include "tuya_error_code.h"

#include "tkl_i2c.h"
#include "tkl_gpio.h"
/***********************************************************
***********************variable define**********************
***********************************************************/
// L511 4G module control pins
#define ENABLE_4G_MODULE_RST(level) tkl_gpio_write(RST_4G_MODULE_CTRL, level)        // high is valid work
#define ENABLE_SIM_VDD(level)       tkl_gpio_write(SIM_VDD_4G_MODULE_CTRL, (!level)) // low is valid work
/***********************************************************
***********************function define**********************
***********************************************************/

static void __board_axp2101_adc_enable(void)
{
    axp2101_disableTSPinMeasure();        // Disable TS pin to prevent interference
    axp2101_enableBattDetection();        // Enable battery detection
    axp2101_enableVbusVoltageMeasure();   // Enable VBUS voltage measurement
    axp2101_enableBattVoltageMeasure();   // Enable battery voltage measurement
    axp2101_enableSystemVoltageMeasure(); // Enable system voltage measurement
    axp2101_enableTemperatureMeasure();   // Enable temperature measurement
}

static void __board_axp2101_charge_init(void)
{
    axp2101_setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V20);  // 4.20V limit to support 4.6V input
    axp2101_setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_500MA); // 500mA current limit for lower voltage
    axp2101_setSysPowerDownVoltage(3300);                            // 3.30V system shutdown voltage

    axp2101_setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_200MA);         // 200mA precharge current
    axp2101_setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA); // 25mA termination current
    axp2101_setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_1000MA);    // 1000mA constant current (max)
    axp2101_setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);       // 4.2V target voltage
    axp2101_enableCellbatteryCharge();

    axp2101_enableChargingLed();
    axp2101_setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG); // Charging LED controlled by charger
}

static void __board_axp2101_power_on(void)
{
    // Disable all DCDC channels
    // axp2101_disablePowerOutput(XPOWERS_DCDC1);  // forbid disenable DCDC1
    axp2101_disablePowerOutput(XPOWERS_DCDC2);
    axp2101_disablePowerOutput(XPOWERS_DCDC3);
    axp2101_disablePowerOutput(XPOWERS_DCDC4);
    axp2101_disablePowerOutput(XPOWERS_DCDC5);

    // Disable all LDO channels
    axp2101_disablePowerOutput(XPOWERS_ALDO1);
    axp2101_disablePowerOutput(XPOWERS_ALDO2);
    axp2101_disablePowerOutput(XPOWERS_ALDO3);
    axp2101_disablePowerOutput(XPOWERS_ALDO4);

    axp2101_disablePowerOutput(XPOWERS_BLDO1);
    axp2101_disablePowerOutput(XPOWERS_BLDO2);

    axp2101_disablePowerOutput(XPOWERS_DLDO1);
    axp2101_disablePowerOutput(XPOWERS_DLDO2);

    axp2101_disablePowerOutput(XPOWERS_CPULDO);

    // Disable button battery
    axp2101_disablePowerOutput(XPOWERS_VBACKUP);

    // Only enable board power
    axp2101_setPowerChannelVoltage(XPOWERS_DCDC1, 3300);
    // axp2101_setPowerChannelVoltage(XPOWERS_DCDC2, 1500);
    // axp2101_setPowerChannelVoltage(XPOWERS_DCDC3, 3300);
    // axp2101_setPowerChannelVoltage(XPOWERS_DCDC4, 1800);
    axp2101_setPowerChannelVoltage(XPOWERS_DCDC5, 3300);
    axp2101_setPowerChannelVoltage(RTC_VDD, 1800);

    axp2101_setPowerChannelVoltage(VDD_CAM_2V8, 2800);
    axp2101_setPowerChannelVoltage(VDD_SD_3V3, 3300);
    axp2101_setPowerChannelVoltage(AVDD_CAM_2V8, 2800);
    axp2101_setPowerChannelVoltage(DVDD_CAM_1V8, 1800);
    // axp2101_setPowerChannelVoltage(VDD_JOYCON_1V1, 1100);

    axp2101_enablePowerOutput(XPOWERS_DCDC1);
    // axp2101_enablePowerOutput(XPOWERS_DCDC2);
    // axp2101_enablePowerOutput(XPOWERS_DCDC3);
    // axp2101_enablePowerOutput(XPOWERS_DCDC4);
    axp2101_enablePowerOutput(XPOWERS_DCDC5);
    axp2101_enablePowerOutput(RTC_VDD);

    axp2101_enablePowerOutput(VDD_CAM_2V8);
    axp2101_enablePowerOutput(VDD_SD_3V3);
    axp2101_enablePowerOutput(AVDD_CAM_2V8);
    axp2101_enablePowerOutput(DVDD_CAM_1V8);
    // axp2101_enablePowerOutput(VDD_JOYCON_1V1);

    PR_DEBUG("Enabled board DCDC and LDO out");
}

static void __board_axp2101_vbus_check(void)
{
    axp2101_print_chg_info();
    return;
}

static void __board_axp2101_power_info(void)
{
    axp2101_print_pwr_info();
    return;
}

OPERATE_RET board_axp2101_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(axp2101_init());

    __board_axp2101_adc_enable();  // Enable internal ADC detection
    __board_axp2101_vbus_check();  // check vbus info
    __board_axp2101_charge_init(); // Enable charging
    __board_axp2101_power_on();    // Enable all power outputs
    __board_axp2101_power_info();  // print pwr info

    axp2101_setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
    axp2101_setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);

    /*4G module RST init*/
    TUYA_GPIO_BASE_CFG_T pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_HIGH};
    TUYA_CALL_ERR_LOG(tkl_gpio_init(RST_4G_MODULE_CTRL, &pin_cfg));
    ENABLE_4G_MODULE_RST(1);

    /*4G module pwr on/off init*/
    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = TUYA_GPIO_LEVEL_LOW;
    TUYA_CALL_ERR_LOG(tkl_gpio_init(SIM_VDD_4G_MODULE_CTRL, &pin_cfg));
    ENABLE_SIM_VDD(1);

    // release i2c source
    // axp2101_deinit();

    return rt;
}
