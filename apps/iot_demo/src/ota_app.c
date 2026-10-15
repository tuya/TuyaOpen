/**
 * @file ota_app.c
 * @brief 应用层 OTA 实现（linux 写文件版）：3 个回调 + 注册注入
 *
 * 原为 port/src/tkl_ota.c（ops 卡模式：port 组装 tal_ota_ops_platform 全局
 * 实例，core 的 tal_api.c 经 extern 强引用——不需要 OTA 的平台也必须给空
 * 桩）。2026-10 OTA 改为 core 回调注入制（tal_ota_register_cbs，见
 * tal_ota.h）：实现移到应用层，本 demo 在此提供并注册；不注册的应用即不
 * 支持 OTA（tal_ota_* 返回 OPRT_NOT_SUPPORTED）。
 *
 * 本实现把升级包写入 ./tuyadb/ota（演示/链路验证用途；注意 cwd 需存在
 * tuyadb/ 目录），end_notify 按 reset 复位进程。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"

#include <stdio.h>
#include <stdlib.h>

#include "ota_app.h"

static FILE *s_upgrade_fd = NULL;

/**
 * @brief ota start notify（注册回调：TAL_OTA_CBS_T.start_notify）
 *
 * @param[in] image_size:  image size
 * @param[in] type:        ota type
 * @param[in] path:        ota path
 *
 * @note This API is used for ota start notify
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
static OPERATE_RET ota_start_notify(uint32_t image_size, TUYA_OTA_TYPE_E type, TUYA_OTA_PATH_E path)
{
    (void)image_size;
    (void)type;
    (void)path;

    FILE *p_upgrade_fd = (FILE *)s_upgrade_fd;

    if (p_upgrade_fd) {
        fclose(p_upgrade_fd);
    }

    char ota_path[255];

    sprintf(ota_path, "./tuyadb/ota");
    s_upgrade_fd = fopen(ota_path, "w+b");

    if (NULL == s_upgrade_fd) {
        printf("open upgrade file fail. upgrade fail %s\r\n", ota_path);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/**
 * @brief ota data process（注册回调：TAL_OTA_CBS_T.data_process）
 *
 * @param[in] pack:       point to ota pack
 * @param[out] remain_len: ota pack remain len
 *
 * @note This API is used for ota data process
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
static OPERATE_RET ota_data_process(TUYA_OTA_DATA_T *pack, uint32_t *remain_len)
{
    printf("Rev File Data Total_len %d, Offset:%u Len:%u\r\n", pack->total_len, pack->offset, pack->len);
    FILE *p_upgrade_fd = (FILE *)s_upgrade_fd;

    /* 句柄防护：start_notify 失败（fopen 失败）时句柄为 NULL，而 core 忽略
     * start 返回值（tuya_ota.c:61）仍会送 DL_EVENT_ON_DATA 到本接口——不防
     * 则 fwrite(NULL) 崩溃。对齐 esp32 版 INIT 门的语义：报错、不写
     * *remain_len（保持调用方传入值） */
    if (NULL == p_upgrade_fd) {
        printf("ota data process: upgrade file not opened\r\n");
        return OPRT_COM_ERROR;
    }

    if (pack->offset + pack->len != pack->total_len) {
        fwrite(pack->data, 1, pack->len - 100, p_upgrade_fd);
        *remain_len = 100;
    } else {
        fwrite(pack->data, 1, pack->len, p_upgrade_fd);
        *remain_len = 0;
    }

    return OPRT_OK;
}

/**
 * @brief ota end notify（注册回调：TAL_OTA_CBS_T.end_notify）
 *
 * @param[in] reset:  ota reset
 *
 * @note This API is used for ota end notify
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
static OPERATE_RET ota_end_notify(BOOL_T reset)
{
    FILE *p_upgrade_fd = (FILE *)s_upgrade_fd;

    /* 注：core 的失败路径不调用本接口——全 core 唯一调用点是成功路径的
     * end_notify(TRUE)（tuya_ota.c:118），DL_EVENT_FAULT / hmac 校验失败
     * 分支无通知。reset=FALSE 分支当前不可达，保留为防御性清理语义。
     *
     * reset 参数当前恒为 TRUE（成功结束），本实现打印并复位进程（exit(1)，
     * linux 无重启机制的最接近模拟）。若 core 将来补失败通知（reset=FALSE），
     * 本分支已就绪：关闭句柄、不退出。 */
    if (p_upgrade_fd) {
        fclose(p_upgrade_fd);
        s_upgrade_fd = NULL;
    }

    if (reset) {
        printf("SOC Upgrade File Download Success\r\n");
        // UserTODO
        exit(1);
    } else {
        printf("SOC Upgrade File Download Fail.ret = %d\r\n", reset);
    }

    return OPRT_OK;
}

/* ---------------------------------------------------------------------------
 * OTA 回调注册：应用层（本 demo）经 tal_ota_register_cbs 注入 core；
 * 不调用 ota_app_register() 的应用即不支持 OTA（tal_ota_* 返回
 * OPRT_NOT_SUPPORTED），无需任何空实现桩。
 * --------------------------------------------------------------------------- */
static const TAL_OTA_CBS_T s_ota_cbs = {
    .start_notify = ota_start_notify,
    .data_process = ota_data_process,
    .end_notify   = ota_end_notify,
};

void ota_app_register(void)
{
    tal_ota_register_cbs(&s_ota_cbs);
    PR_NOTICE("ota callbacks registered (write to ./tuyadb/ota)");
}
