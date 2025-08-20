/**
 * @file tkl_cellular_vbat.h
 * @author www.tuya.com
 * @brief Cellular module battery function API implementation interface.
 *
 * @copyright Copyright (c) tuya.inc 2021
 */

#ifndef __TKL_CELLULAR_VBAT_H__
#define __TKL_CELLULAR_VBAT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Battery and charger message definition
 */
typedef enum
{
    TKL_CELLULAR_VBAT_CHG_START = 0x01, /*< Start charging */
    TKL_CELLULAR_VBAT_CHG_FINISH,       /*< Finish charging */
    TKL_CELLULAR_VBAT_CHG_WARNING,      /*< Low battery, remind user to charge */
    TKL_CELLULAR_VBAT_CHG_SHUTDOWN,     /*< Very low battery, remind user to shutdown */
    TKL_CELLULAR_VBAT_CHG_BATT_OFF,     /*< Battery removed */
    TKL_CELLULAR_VBAT_CHG_CAPACITY,     /*< Battery capacity change notification */
    TKL_CELLULAR_VBAT_CHG_DISCONNECT,   /*< Charger disconnected */
    TKL_CELLULAR_VBAT_CHG_FAULT,        /*< Charging fault */
    TKL_CELLULAR_VBAT_CHG_MAX
} TKL_CELLULAR_VBAT_CHG_MSG_T;

/**
 * @brief Charger status definition
 */
typedef enum
{
    TKL_CELLULAR_CHG_STATE_UNCONNECTED = 0x01, /*< Charger not connected */
    TKL_CELLULAR_CHG_STATE_CHARGING,           /*< Charger connected, charging */
    TKL_CELLULAR_CHG_STATE_CHARGE_OVER,        /*< Charger connected, charging completed */
    TKL_CELLULAR_CHG_STATE_MAX
} TKL_CELLULAR_VBAT_CHG_STATE_E;

/**
 * @brief Battery and charger message callback function prototype definition
 * @param msg Battery and charger message, see @TKL_CELLULAR_VBAT_CHG_MSG_T definition
 * @return None
 */
typedef void (*TKL_CELLULAR_VBAT_CHARGE_CB)(TKL_CELLULAR_VBAT_CHG_MSG_T msg);

/**
 * @brief Get battery remaining power percentage
 *
 * @param rsoc Remaining power percentage
 *
 * @return OPRT_OK get success, others get failure
 */
OPERATE_RET tkl_cellular_vbat_get_rsoc(uint8_t* rsoc);

/**
 * @brief Get battery voltage
 *
 * @param voltage Current battery voltage, unit mV
 *
 * @return OPRT_OK get success, others get failure
 */
OPERATE_RET tkl_cellular_vbat_get_voltage(uint32_t* voltage);

/**
 * @brief Set whether to enable NTC detection of battery temperature
 *
 * @param enable NTC battery temperature detection on/off
 *
 * @return OPRT_OK setting success, others setting failure
 */
OPERATE_RET tkl_cellular_vbat_ntc_enable(bool enable);

/**
 * @brief Set battery charging current in constant current charging phase
 *
 * @param current Charging current, unit milliampere (mA)
 *
 * @return OPRT_OK setting success, others setting failure
 */
OPERATE_RET tkl_cellular_vbat_set_charge_current(uint32_t current);

/**
 * @brief Get charger status
 *
 * @param None
 *
 * @return Charger status, see @TKL_CELLULAR_VBAT_CHG_STATE_E definition
 */
TKL_CELLULAR_VBAT_CHG_STATE_E tkl_cellular_vbat_get_charger_state(void);

/**
 * @brief Register battery and charger message callback handler function
 *
 * @param callback Callback function
 *
 * @return 0 registration success, others registration failure
 */
OPERATE_RET tkl_cellular_vbat_cb_register(TKL_CELLULAR_VBAT_CHARGE_CB callback);

/**
 * @brief Whether to enable battery low voltage shutdown function
 *
 * @param enable TRUE enable, FALSE disable
 *
 * @return OPRT_OK setting success, others setting failure
 */
OPERATE_RET tkl_cellular_vbat_low_volt_poweroff_enable(bool enable);

#ifdef __cplusplus
}
#endif

#endif
