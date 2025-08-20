/**
 * @file tkl_cellular_base.h
 * @author www.tuya.com
 * @brief Cellular module basic function API implementation interface.
 *
 * @copyright Copyright (c) tuya.inc 2021
 */
#ifndef __TKL_CELLULAR_H__
#define __TKL_CELLULAR_H__

#include "tuya_cloud_types.h"
#include "tkl_cellular_mds.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Basic IOCTL commands for cellular devices */
typedef enum
{
	CELL_IOCTL_SET_PLMN,						//Set PLMN
	CELL_IOCTL_GET_PLMN,						
	CELL_IOCTL_SET_BAND,						//Set frequency band
	CELL_IOCTL_GET_BAND,						//Get frequency band
// Set the detection time for cellular module without data transmission. If the set time is exceeded, the cellular module will actively release RRC. The purpose is to further optimize power consumption. The smaller the time, the more likely it is to cause data packet loss, use with caution.
	CELL_IOCTL_SET_RRC_RELEASE_TIME,          	//Set RRC release time 
	CELL_IOCTL_GET_RRC_RELEASE_TIME,          	//Get RRC release time
	CELL_IOCTL_GET_DATA_STATICS,              	//Get data statistics

	CELL_IOCTL_SET_PWRKEY_SHUTDOWN_TIME = 100,	//Set power key shutdown duration, when 0, disable this function	
	CELL_IOCTL_CLOSE_WAKEUP_MODULE ,			//Close the function of waking up the module through wakeup in low power mode, release the wakeup pin
	CELL_IOCTL_GET_USB_INSERT_STATUS,			//Get USB insertion status
	CELL_INIT_VIRTUAL_AT,               		//Initialize virtual AT service
	CELL_IOCTL_SEND_VIRTUAL_AT_CMD,				//Send virtual AT command	
	CELL_IOCTL_GET_SYS_VER,						//Get system version
	CELL_IOCTL_GET_MODULE,						//Get module model
	CELL_IOCTL_GET_RF_CALIBRATED,				//Get RF calibration status
	CELL_IOCTL_SET_NET_TYPE,
}CELL_IOCTRL_CMD_E;

/**
 * @brief SIM card status definition
 */
typedef enum
{
    TKL_NO_SIM,     /*< No SIM card */
    TKL_SIM_READY,  /*< SIM card recognized */
    TKL_SIM_INIT,   /*< SIM card initializing>*/
    TKL_SIM_WAIT_PIN,
    TKL_SIM_WAIT_PUK,
} TKL_SIM_STATE_E;

typedef enum{
    TUYA_TSIM_TYPE, /* Physical card */
    TUYA_VSIM_TYPE, /* Virtual card */
}TUYA_SIM_TYPE_E;

typedef enum {
  	DOUBLE_SIM_DOUBLE_CHANNEL = 1,
  	DOUBLE_SIM_SINGLE_CHANNEL = 2,
  	SINGLE_SIM_SINGLE_CHANNEL = 4,
}TKL_CELLULAR_ABILITY_E;

#define NEIGHBOUR_NUM   4
typedef struct {
	char mcc[3];
	char mnc[3];
	uint16_t lac;           //LAC
	int16_t  rx_pwr;
	uint32_t   cellid;        //20bits(eNodeID)+8bits(Cellid)
}TKL_CELL_INFO_T;

typedef struct
{
	char mcc[3];
	char mnc[3];
	TKL_CELL_INFO_T main;
	TKL_CELL_INFO_T neighbour[NEIGHBOUR_NUM];
}TKL_LBS_INFO_T;

typedef struct {
   int fd;
   char name[32+1];
   TUYA_UART_BASE_CFG_T cfg;
   uint8_t              sim_id;
   TUYA_SIM_TYPE_E      sim_type;
}TKL_CELL_INIT_PARAM_T;

/**
 * @brief SIM card status change notification function prototype
 * @param state SIM card status, see @TAL_SIM_STATE_E definition
 */
typedef void (*TKL_SIM_NOTIFY)(TKL_SIM_STATE_E status);

/** 
 * @brief Cellular network basic function initialization
 */
OPERATE_RET tkl_cellular_base_init(TKL_CELL_INIT_PARAM_T *param);

/**
 * @brief Cellular network registration status change notification function prototype
 * @param simId SIM card ID
 * @param state Cellular network status, see @TUYA_CELLULAR_MDS_STATUS_E definition, only need to report
 *              TUYA_CELLULAR_MDS_STATUS_IDLE, TUYA_CELLULAR_MDS_STATUS_REG, TUYA_CELLULAR_MDS_STATUS_CAMPED 3 events.
 */
typedef void (*TKL_REGISTION_NOTIFY)(TUYA_CELLULAR_MDS_STATUS_E st);

/**
 * @brief Get the communication capability of the current device
 * @param ability @TKL_CELLULAR_ABILITY_E type
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_get_ability(TKL_CELLULAR_ABILITY_E *ability);

/**
 * @brief Switch the currently enabled SIM card.
 * @param simid SIM card ID.(0~1)
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_switch_sim(uint8_t sim_id);

/**
 * @brief Register SIM status change notification function
 * @param fun Status change notification function
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_register_sim_state_notify(uint8_t simd_id,TKL_SIM_NOTIFY fun);

/**
 * @brief Enable or disable SIM card hot plug
 * @param simId SIM card ID
 * @param enable TRUE enable FALSE disable
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_enable_sim_hotplug(uint8_t sim_id, bool enable);

/**
 * @brief Get the status of the SIM card
 * @param simId SIM card ID
 * @param state 1: normal, 0: abnormal, 2: initializing
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_sim_get_status(uint8_t sim_id, uint8_t *state);

/**
 * @brief Get the current communication function setting of the cellular device
 *
 * @param cfun The obtained communication function
 *
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_get_cfun_mode(uint8_t simd_id, int *cfun);

/**
 * @brief Set the communication function mode of the cellular device
 *
 * @param cfun Communication function, the value meanings are as follows:
 *            1: Full function mode
 *            4: Flight mode
 *
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_set_cfun_mode(uint8_t simd_id,int cfun);

/**
 * @brief Get the International Mobile Subscriber Identity from the SIM card
 *
 * @param simid, SIM card id number (0,1,2...)
 * @param imsi identification code, 16-byte string
 *
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_get_imsi(uint8_t sim_id,char imsi[15 + 1]);

/**
 * @brief Get the ICCID of the SIM card
 * @param simid
 * @param ICCID identification code, 20-byte string
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_get_iccid(uint8_t sim_id,char iccid[20 + 1]);

/**
 * @brief Get the IMEI number of the channel device where the SIM card is located
 * @param simid
 * @param IMEI identification code, 15-byte string
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_get_imei(uint8_t sim_id,char imei[15 + 1]);

/**
 * @brief Set the IMEI number of the device
 * @param simid
 * @param IMEI identification code, 15-byte string
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_set_imei(uint8_t sim_id,char imei[15 + 1]);

/**
 * @brief Get the signal receiving power of the cellular device in the channel where the SIM card is located - unit dbm
 * @param simid
 * @param rsrp Return the actual signal strength (dbm)
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_get_rsrp(uint8_t sim_id,int *rsrp);

/**
 * @brief Get the signal-to-noise ratio and bit error rate of the channel where the cellular device SIM card is located
 * @param simid
 * @param sinr (0~31)
 * @param bit_error (0~7,99) 99 no network
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_get_sinr(uint8_t sim_id,int *sinr,int *bit_error);

/**
 * @brief LBS base station information of the channel where the SIM card is located)
 * @param simid
 * @param lbs Return base station information
 * @param neighbour Whether to search for neighboring base station information
 * @param timeout Search for neighboring base station information timeout (generally takes about 4 seconds)
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_get_lbs(uint8_t sim_id,TKL_LBS_INFO_T *lbs,bool neighbour,int timeout);

/**
 * @brief Get the RF calibration status of the current device
 * @return TRUE normal, FALSE abnormal
 */
bool tkl_cellular_base_rf_calibrated(void);

/**
 * @brief Enable or disable SIM card GPIO detection
 * @param simId SIM card ID
 * @param enable TRUE enable FALSE disable
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_enable_sim_detect(uint8_t simid, bool enable);

/**
 * @brief Get the default SIM ID
 * @return Less than 0 failure, others SIM ID
 */
int8_t tkl_cellular_base_get_default_simid(void);

/**
 * @brief General control function for cellular basic functionality, usually used as a platform to provide some special capability interfaces
 * @param cmd Refer to CELL_IOCTRL_CMD
 * @param argv Platform-defined
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_base_ioctl(int cmd, void* argv);

/**
 * @brief Set the callback for module underlying network registration events
 * @note Mainly underlying reports TUYA_CELLULAR_MDS_STATUS_IDLE, TUYA_CELLULAR_MDS_STATUS_REG, TUYA_CELLULAR_MDS_STATUS_CAMPED 3 events.
 * Representing network searching, network registration success, and stop network searching respectively
 * @param fun Callback function
 * @return OPERATE_RET Operation result, returns OPRT_OK on success, returns error code on failure
 */
OPERATE_RET tkl_cellular_register_dev_reg_notify(uint8_t sim_id, TKL_REGISTION_NOTIFY fun);

#ifdef __cplusplus
}
#endif

#endif
