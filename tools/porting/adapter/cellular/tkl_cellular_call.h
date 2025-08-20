/**
 * @file tkl_cellular_call.h
 * @author www.tuya.com
 * @brief Cellular module phone dialing API implementation interface.
 *
 * @copyright Copyright (c) tuya.inc 2021
 */

#ifndef __TKL_CELLULAR_CALL_H__
#define __TKL_CELLULAR_CALL_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tone type definition
 */
typedef enum
{
    TUYA_TONE_DTMF_0 = 0,                /*< DTMF '0' */
    TUYA_TONE_DTMF_1,                    /*< DTMF '1' */
    TUYA_TONE_DTMF_2,                    /*< DTMF '2' */
    TUYA_TONE_DTMF_3,                    /*< DTMF '3' */
    TUYA_TONE_DTMF_4,                    /*< DTMF '4' */
    TUYA_TONE_DTMF_5,                    /*< DTMF '5' */
    TUYA_TONE_DTMF_6,                    /*< DTMF '6' */
    TUYA_TONE_DTMF_7,                    /*< DTMF '7' */
    TUYA_TONE_DTMF_8,                    /*< DTMF '8' */
    TUYA_TONE_DTMF_9,                    /*< DTMF '9' */
    TUYA_TONE_DTMF_A,                    /*< DTMF 'A' */
    TUYA_TONE_DTMF_B,                    /*< DTMF 'B' */
    TUYA_TONE_DTMF_C,                    /*< DTMF 'C' */
    TUYA_TONE_DTMF_D,                    /*< DTMF 'D' */
    TUYA_TONE_DTMF_SHARP,                /*< DTMF '#' */
    TUYA_TONE_DTMF_STAR,                 /*< DTMF '*' */
} TUYA_TONE_TYPE_E;

/**
 * @brief Maximum length of phone number
 * @note The phone number is in string format, this length includes the '\0' character.
 */
#define TKL_CELLULAR_CALLNUM_LEN_MAX  24

/**
 * @brief Call callback status
 */
typedef enum
{
    TUYA_CALL_IND_CALLIN = 1,       /*< Incoming call indication */
    TUYA_CALL_IND_RELEASE,          /*< Call release indication */
    TUYA_CALL_IND_CALL_RSP_OK,      /*< Outgoing call response success indication */
    TUYA_CALL_IND_CALL_RSP_FAIL,    /*< Outgoing call response failure indication */
    TUYA_CALL_IND_RING,             /*< Outgoing call ringing indication */
    TUYA_CALL_IND_ACCEPT_CALL_OK,   /*< Incoming call answer response success indication */
    TUYA_CALL_IND_ACCEPT_CALL_FAIL, /*< Incoming call answer response failure indication */
    TUYA_CALL_IND_RELEASE_RSP,      /*< Call release response indication */
    TUYA_CALL_IND_DTMF_KEY,         /*< Get the key value corresponding to the received DTMF*/
    TUYA_CALL_IND_MAX
} TUYA_CELLULAR_CALL_IND_STATE_E;

typedef enum { 
    CELL_CALL_CMD_SET_MIC_GAIN,
    CELL_CALL_CMD_GET_MIC_GAIN,
    CELL_CALL_CMD_SET_SIDETONE_GAIN,
    CELL_CALL_CMD_GET_SIDETONE_GAIN,
}TUYA_CELLULAR_CALL_IOCTL_CMD_E;

/**
 * @brief Call callback handler function prototype
 * @param state Call status
 * @param simId SIM card ID
 * @param callNum Phone number
 * @return None
 */
typedef void (*TKL_CELLULAR_CALL_CB)(TUYA_CELLULAR_CALL_IND_STATE_E state, uint8_t simId, char callNum[TKL_CELLULAR_CALLNUM_LEN_MAX]);

/**
 * @brief During a call, the other party's key input detection callback function definition, used to detect the other party's DTMF input
 * @param dtmf
 * @return None
 */
typedef void (*TKL_CELLULAR_CALL_KTDETECH_CB)(TUYA_TONE_TYPE_E dtmf);

/**
 * @brief Query whether call service is available
 * @note This function is used to query whether call service is available. Only when call service is available,
 *       other call service interfaces can be used.
 * @param simId SIM card ID
 * @param enalbe Whether call service is available
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_service_available(uint8_t sim_id, bool *enable);

/**
 * @brief Call dialing interface function
 *
 * @param simId SIM card ID number
 * @param callNUm Call number, string format
 * @return  0 call initiated successfully, others failure
 */
OPERATE_RET tkl_cellular_call(uint8_t sim_id, char callNum[TKL_CELLULAR_CALLNUM_LEN_MAX]);

/**
 * @brief When an external call comes in, answer and pick up interface function
 * @return   0 answer success, others failure
 */
OPERATE_RET tkl_cellular_call_answer(uint8_t sim_id);

/**
 * @brief After a call conversation, hang up interface function
 * @return  0 hang up success, others failure
 */
OPERATE_RET tkl_cellular_call_hungup(uint8_t sim_id);

/**
 * @brief Register user-defined call callback handler function
 * @param callback Call callback handler function
 * @return 0 registration success, others registration failure
 */
OPERATE_RET tkl_cellular_call_cb_register(TKL_CELLULAR_CALL_CB callback);

/**
 * @brief Set incoming call ringtone mute
 * @param mute TRUE mute FALSE unmute
 * @return 0 setting success, others setting failure
 */
OPERATE_RET tkl_cellular_call_set_callin_mute(bool mute);

/**
 * @brief Start or turn off volte function
 * @param enable TRUE enable volte, false disable volte
 * @return 0 setting success, others setting failure
 */
OPERATE_RET tkl_cellular_call_set_volte(uint8_t sim_id,bool enable);

/**
 * @brief Play phone dial tone
 * @param tone Dial tone characteristic value
 * @param duration Duration of the tone playback
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_play_tone(TUYA_TONE_TYPE_E tone, int  duration);

/**
 * @brief Stop phone dial tone
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_stop_tone(void);

/**
 * @brief Send DTMF digit key audio to voice channel
 * @note 1. This function is used to implement voice interaction like 10086. After voice prompt, user operates keys, call this function
 *       to send key audio to voice channel. This function can only be called after call state, otherwise it will cause system exception.
 *       2. dtmfTone can only be one of TKL_TONE_DTMF_0 ~ TKL_TONE_DTMF_STAR.
 *
 * @param dtmfTone Dial key
 * @param duration Audio duration
 * @return 0 success
 *        -1 dtmfTone type error
 *        -2 memory allocation failed
 *    OPRT_TIMEOUT semaphore acquisition timeout
 *    OPRT_BASE_OS_ADAPTER_REG_NULL_ERROR function not adapted
 */
OPERATE_RET tkl_cellular_call_dtmf2voice(TUYA_TONE_TYPE_E dtmfTone, uint32_t duration);

/**
 * @brief Set the volume of voice call audio
 * @param vol (0~100)
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_set_voice_vol(int  vol);

/**
 * @brief Get the volume of voice call audio
 * @param vol (0~100)
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_get_voice_vol(int* vol);

/**
 * @brief Whether to set mute for uplink audio during call
 * When the current voice call is finished, the property will be kept.
 * When the current voice call ends, this property will be saved. In the next voice call process, if
 * it was set to mute last time, it will continue to be muted. But after restart, it will be restored to unmute by default
 * @param TRUE for mute uplink of voice call
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_set_voice_mute(bool mute);

/**
 * @brief Get the mute status of uplink sound during call.
 * @param mute : TRUE mute
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_get_voice_mute(bool *mute);

/**
 * @brief Register DMTF detection callback function
 * @param cb :  callback function
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_reg_KTDetect(TKL_CELLULAR_CALL_KTDETECH_CB cb);

/**
 * @brief Control whether DTMF detection function is enabled, system default is disabled. (Enabling this function will increase system load during audio-related business processes)
 * @param enable : TRUE enable FALSE disable
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_ctrl_KTDetect(bool enable);

/**
 * @brief Phone function ioctl interface
 * @param cmd Command
 * @param argv Parameters
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_call_ioctl(int cmd, void *argv);

#ifdef __cplusplus
}
#endif

#endif
