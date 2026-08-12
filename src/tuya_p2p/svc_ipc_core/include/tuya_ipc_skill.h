/**
 * @file      tuya_ipc_skill.h
 * @brief     tuya ipc skill api
 * @version   1.0
 * @date      2021-11-19
 *
 * copyright  Copyright (c) tuya.inc 2021
 */
#ifndef __TUYA_IPC_SKILL_H__
#define __TUYA_IPC_SKILL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_cloud_types.h"

#define SKILL_INFO_LEN 512

/**
 * @enum  TUYA_IPC_SKILL_E
 * @brief tuya ipc skill enum
 */
typedef enum {
    TUYA_IPC_SKILL_CLOUDSTG,
    TUYA_IPC_SKILL_WEBRTC,
    TUYA_IPC_SKILL_LOWPOWER,
    TUYA_IPC_SKILL_AIDETECT,
    TUYA_IPC_SKILL_LOCALSTG,
    TUYA_IPC_SKILL_CLOUDGW,
    TUYA_IPC_SKILL_CLOUDGW_INFO,
    TUYA_IPC_SKILL_DOORBELLSTG,
    TUYA_IPC_SKILL_P2P,
    TUYA_IPC_SKILL_TMM,
    TUYA_IPC_SKILL_PX,
    TUYA_IPC_SKILL_PERSON_FLOW,
    TUYA_IPC_SKILL_UPNP,
    TUYA_IPC_SKILL_MAX_P2P_SESSIONS,
    TUYA_IPC_SKILL_CLOUDRULE,
    TUYA_IPC_SKILL_CLOUDRULE_GW,
    TUYA_IPC_SKILL_LOCAL_AI,
    TUYA_IPC_SKILL_MOBILE_NETWORK,
    TUYA_IPC_SKILL_MAX,
} TUYA_IPC_SKILL_E;

/**
 * @enum TUYA_IPC_LOCAL_AI_SKILL_E
 * @brief tuya ipc local ai skill enum
 */
typedef enum {
    TUYA_IPC_LOCAL_AI_SKILL_HUMAN = 1 << 0,   // Human detection
    TUYA_IPC_LOCAL_AI_SKILL_CAT = 1 << 1,     // Pet detection
    TUYA_IPC_LOCAL_AI_SKILL_CAR = 1 << 2,     // Vehicle detection
    TUYA_IPC_LOCAL_AI_SKILL_FACE = 1 << 3,    // Face detection
    TUYA_IPC_LOCAL_AI_SKILL_PACKAGE = 1 << 4, // Package detection
} TUYA_IPC_LOCAL_AI_SKILL_E;

/**
 * @union  TUYA_IPC_SKILL_PARAM_U
 * @brief  tuya ipc skill parameter
 */
typedef union {
    int value;
    char info[SKILL_INFO_LEN];
} TUYA_IPC_SKILL_PARAM_U;

/**
 * @brief      enable ipc skill
 * @param[in]  skill    skill defined in TUYA_IPC_SKILL_E
 * @param[in]  param    skill is set by param->value
 * @return     OPERATE_RET
 * - OPRT_OK   success
 * - Others    failed
 */
OPERATE_RET tuya_ipc_skill_enable(TUYA_IPC_SKILL_E skill, TUYA_IPC_SKILL_PARAM_U *param);

/**
 * @brief        fill ipc skills in skills buf
 * @param[inout] skills    buffer used to fill ipc skills
 * @return       void
 */
void tuya_ipc_http_fill_skills_cb(char *skills);

/**
 * @brief      upload ipc media info
 * @return     void
 */
OPERATE_RET tuya_ipc_upload_media_info(void);

/**
 * @brief      check whether it is low power
 * @return     BOOL_T
 * - TRUE      low power ipc
 * - FALSE     non low power ipc
 */
BOOL_T tuya_ipc_is_low_power(void);

/**
 * @brief      get ipc skill
 * @param[in]  skill    skill defined in TUYA_IPC_SKILL_E
 * @param[out]  param    skill result
 * @return     OPERATE_RET
 * - OPRT_OK   success
 * - Others    failed
 */
OPERATE_RET tuya_ipc_skill_get(TUYA_IPC_SKILL_E skill, int *result);
/**
 * @brief      get clow gw skill
 * @param[in]  device  device number
 * @param[in]  channel  device channel
 * @param[out]  skill_result    skill result
 * @return     OPERATE_RET
 * - OPRT_OK   success
 * - Others    failed
 */
OPERATE_RET tuya_ipc_cloud_gw_kills_get(uint32_t device, uint32_t channel, char *skill_result);

/**
 * check if void tuya_ipc_upload_skills(void) be called.
 */
BOOL_T tuya_ipc_get_if_skill_uploaded();

/**
 * @brief      set local ai skills
 * @param[in]  local_ai_skills    local ai skills, refer to TUYA_IPC_LOCAL_AI_SKILL_E
 * @return     OPERATE_RET
 * - OPRT_OK   success
 * - Others    failed
 */
OPERATE_RET tuya_ipc_set_local_ai_skills(uint32_t local_ai_skills);

#ifdef __cplusplus
}
#endif

#endif /*_TUYA_IPC_SKILL_H_*/
