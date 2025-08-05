/**
 * @file board_AXP2101_api.c
 * @author Tuya Inc.
 * @brief AXP2101 power management IC driver implementation for TUYA_T5AI_POCKET board
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "board_axp2101_api.h"
#include "tdl_axp2101_driver.h"
#include "tdl_axp2101_reg.h"

#include "tal_system.h"
#include "tal_log.h"
#include "tuya_error_code.h"

#include "tkl_i2c.h"
#include "tkl_gpio.h"
/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

static void __board_charge_init(void)
{
    tdl_axp2101_setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V20);  // 4.20V limit to support 4.6V input
    tdl_axp2101_setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_500MA); // 500mA current limit for lower voltage
    tdl_axp2101_setSysPowerDownVoltage(2600);                            // 2.6V system shutdown voltage

    tdl_axp2101_disableTSPinMeasure();        // Disable TS pin to prevent interference
    tdl_axp2101_enableBattDetection();        // Enable battery detection
    tdl_axp2101_enableVbusVoltageMeasure();   // Enable VBUS voltage measurement
    tdl_axp2101_enableBattVoltageMeasure();   // Enable battery voltage measurement
    tdl_axp2101_enableSystemVoltageMeasure(); // Enable system voltage measurement

    tdl_axp2101_setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_200MA);         // 200mA precharge current
    tal_axp2101_setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA); // 25mA termination current
    tdl_axp2101_setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_1000MA);    // 1000mA constant current (max)
    tdl_axp2101_setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);       // 4.2V target voltage
    tdl_axp2101_enableCellbatteryCharge();
}

static void __board_all_pwron(void)
{
    tdl_axp2101_setPowerChannelVoltage(XPOWERS_DCDC1, 3300);
    tdl_axp2101_setPowerChannelVoltage(XPOWERS_DCDC2, 1500);
    tdl_axp2101_setPowerChannelVoltage(XPOWERS_DCDC3, 3300);
    tdl_axp2101_setPowerChannelVoltage(XPOWERS_DCDC4, 1800);
    tdl_axp2101_setPowerChannelVoltage(XPOWERS_DCDC5, 3300);
    tdl_axp2101_setPowerChannelVoltage(RTC_VDD, 1800);

    tdl_axp2101_setPowerChannelVoltage(VDD_CAM_2V8, 2800);
    tdl_axp2101_setPowerChannelVoltage(VDD_SD_3V3, 3300);
    tdl_axp2101_setPowerChannelVoltage(AVDD_CAM_2V8, 2800);
    tdl_axp2101_setPowerChannelVoltage(DVDD_CAM_1V8, 1800);
    tdl_axp2101_setPowerChannelVoltage(VDD_JOYCON_1V1, 1100);

    tdl_axp2101_enablePowerOutput(XPOWERS_DCDC1);
    tdl_axp2101_enablePowerOutput(XPOWERS_DCDC2);
    tdl_axp2101_enablePowerOutput(XPOWERS_DCDC3);
    tdl_axp2101_enablePowerOutput(XPOWERS_DCDC4);
    tdl_axp2101_enablePowerOutput(XPOWERS_DCDC5);
    tdl_axp2101_enablePowerOutput(RTC_VDD);

    tdl_axp2101_enablePowerOutput(VDD_CAM_2V8);
    tdl_axp2101_enablePowerOutput(VDD_SD_3V3);
    tdl_axp2101_enablePowerOutput(AVDD_CAM_2V8);
    tdl_axp2101_enablePowerOutput(DVDD_CAM_1V8);
    tdl_axp2101_enablePowerOutput(VDD_JOYCON_1V1);

    PR_DEBUG("Enabled DCDC and LDO out");
}

void __board_vbus_check(void)
{
    tdl_axp2101_print_chg_info();
    return;
}

void __board_pwr_info(void)
{
    tdl_axp2101_print_pwr_info();
    return;
}

OPERATE_RET board_axp2101_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tdl_axp2101_init());

    __board_vbus_check();  // check vbus info
    __board_charge_init(); // Enable charging
    __board_all_pwron();   // Enable all power outputs
    __board_pwr_info();    // print pwr info

    /*4G module RST init, high is valid*/
    TUYA_GPIO_BASE_CFG_T pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_HIGH};
    TUYA_CALL_ERR_LOG(tkl_gpio_init(RST_4G_MODULE_CTRL, &pin_cfg));
    tkl_gpio_write(RST_4G_MODULE_CTRL, TUYA_GPIO_LEVEL_HIGH);

    /*4G module pwr on/off init, low is valid*/
    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = TUYA_GPIO_LEVEL_LOW;
    TUYA_CALL_ERR_LOG(tkl_gpio_init(SIM_VDD_4G_MODULE_CTRL, &pin_cfg));
    tkl_gpio_write(SIM_VDD_4G_MODULE_CTRL, TUYA_GPIO_LEVEL_LOW);

    tdl_axp2101_deinit(); // release i2c source

    return rt;
}
