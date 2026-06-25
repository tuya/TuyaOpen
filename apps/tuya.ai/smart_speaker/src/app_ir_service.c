/**
 * @file app_ir_service.c
 * @brief IR cloud service integration for the smart_speaker application.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "app_ir_service.h"

#if defined(ENABLE_TUYA_IR_CLOUD_SERVICE) && (ENABLE_TUYA_IR_CLOUD_SERVICE == 1)

#include "tal_log.h"
#include "tuya_iot.h"

#include "tuya_ir_cloud_service.h"

/***********************************************************
***********************variable define**********************
***********************************************************/

static uint8_t s_ir_lib_selected = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief cloud IR control notification (pre-send / send-finish)
 */
static void __app_ir_ctrl_notif_cb(IR_CODE_CTRL_STATUS_E ctrl_status, IR_CODE_CTRL_KEY_INFO_T key_info)
{
    PR_DEBUG("ir ctrl notif: status=%d, key_num=%d", ctrl_status, key_info.key_num);
}

/**
 * @brief cloud IR study state notification
 */
static void __app_ir_study_notif_cb(IR_CODE_STUDY_STATUS_E study_sta)
{
    PR_DEBUG("ir study notif: %d", study_sta);
}

OPERATE_RET app_ir_service_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    IR_CODE_NOTIF_CALLBACK_T notif = {0};

    /* the IR hardware driver is registered by the board (board_register_ex_module),
     * here we only start the cloud IR service on top of it. */
    notif.ctrl_cb  = __app_ir_ctrl_notif_cb;
    notif.study_cb = __app_ir_study_notif_cb;

    rt = tuya_ir_cloud_init((char *)IR_NAME, notif);
    if (OPRT_OK != rt) {
        PR_ERR("tuya ir cloud init failed, %d", rt);
        return rt;
    }

    PR_NOTICE("app ir service init success");
    return OPRT_OK;
}

void app_ir_service_on_mqtt_connected(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_ir_lib_selected) {
        return;
    }

    /* select the cloud fastlz IR code library (needs the device to be online) */
    rt = tuya_ir_lib_select(IR_LIB_TYPE_FASTLZ, NULL, IR_PRODUCT_TYPE_PUBLIC);
    if (OPRT_OK == rt) {
        s_ir_lib_selected = 1;
        PR_NOTICE("ir lib select success");
    } else {
        PR_WARN("ir lib select failed, %d", rt);
    }
}

OPERATE_RET app_ir_dp_process(dp_obj_recv_t *dpobj)
{
    if (NULL == dpobj) {
        return OPRT_INVALID_PARM;
    }

    for (uint32_t i = 0; i < dpobj->dpscnt; i++) {
        tuya_ir_cloud_dp_proc(&dpobj->dps[i]);
    }

    return OPRT_OK;
}

#else /* IR cloud service disabled */

OPERATE_RET app_ir_service_init(void)
{
    return OPRT_OK;
}

void app_ir_service_on_mqtt_connected(void)
{
}

OPERATE_RET app_ir_dp_process(dp_obj_recv_t *dpobj)
{
    (void)dpobj;
    return OPRT_OK;
}

#endif /* ENABLE_TUYA_IR_CLOUD_SERVICE */
