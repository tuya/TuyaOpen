/**
 * @file tuya_ota.c
 * @brief Implementation of Tuya's Over-The-Air (OTA) update functionality.
 *
 * This file contains the implementation of the OTA update process for Tuya
 * devices. It includes mechanisms for receiving OTA update notifications,
 * downloading firmware updates, reporting update status and progress, and
 * applying the firmware updates. The OTA process ensures that devices can be
 * remotely updated with new firmware versions, improving functionality and
 * security.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_iot.h"
#include "cJSON.h"
#include "tal_api.h"
#include "tuya_cloud_com_defs.h"
#include "tuya_endpoint.h"
#include "iotdns.h"
#include "mix_method.h"

typedef struct {
    tuya_ota_config_t config;
    tuya_ota_msg_t msg;
    tuya_ota_event_t event;
    uint8_t channel;
    uint8_t progress_percent;
    THREAD_HANDLE upgrade_thrd;
    TKL_HASH_HANDLE sha256;
} tuya_ota_t;

int tuya_ota_upgrade_status_report(tuya_ota_t *handle, int status);
int tuya_ota_upgrade_progress_report(tuya_ota_t *handle, int percent);

static tuya_ota_t *s_ota_ctx;

static void file_download_event_cb(http_download_event_id_t id, http_download_event_t *event)
{
    tuya_ota_t *ota = (tuya_ota_t *)event->user_data;
    tuya_ota_event_cb_t event_cb = ota->config.event_cb;
    tuya_iot_client_t *client = ota->config.client;
    uint8_t file_hmac[32];
    uint8_t self_hmac[32];
    uint8_t file_sha256[32 * 2 + 1] = {0};

    switch (id) {
    case DL_EVENT_START:
        PR_DEBUG("DL_EVENT_START");
        tuya_ota_upgrade_status_report(ota, TUS_UPGRDING);
        tal_sha256_create_init(&ota->sha256);
        tal_sha256_starts_ret(ota->sha256, 0);
        break;

    case DL_EVENT_ON_FILESIZE:
        PR_DEBUG("DL_EVENT_ON_FILESIZE");
        if (0 == ota->channel) {
            tal_ota_start_notify(event->file_size, TUYA_OTA_FULL, TUYA_OTA_PATH_AIR);
        } else if (event_cb) {
            ota->event.id = TUYA_OTA_EVENT_START;
            ota->event.file_size = event->file_size;
            ota->event.user_data = ota->config.user_data;
            event_cb(&ota->msg, &ota->event);
        }
        break;

    case DL_EVENT_ON_DATA: {
        PR_DEBUG("DL_EVENT_ON_DATA:%d", event->data_len);
        PR_DEBUG("event->file_size %d, offset:%d, last remain %d", event->file_size, event->offset, event->remain_len);
        if (0 == ota->channel) {
            TUYA_OTA_DATA_T ota_pack;

            ota_pack.total_len = event->file_size;
            ota_pack.offset = event->offset;
            ota_pack.data = event->data;
            ota_pack.len = event->data_len;
            ota_pack.pri_data = NULL;
            tal_ota_data_process(&ota_pack, (uint32_t *)&event->remain_len);
            if (event->remain_len) {
                tal_sha256_update_ret(ota->sha256, event->data, event->data_len - event->remain_len);
            } else {
                tal_sha256_update_ret(ota->sha256, event->data, event->data_len);
            }
        } else if (event_cb) {
            ota->event.id = TUYA_OTA_EVENT_ON_DATA;
            ota->event.data = event->data;
            ota->event.data_len = event->data_len;
            ota->event.offset = event->offset;
            event_cb(&ota->msg, &ota->event);
        }
        uint8_t percent = event->offset * 100 / event->file_size;
        if (percent - ota->progress_percent > 5) {
            PR_DEBUG("File Download Percent: %d%%", percent);
            tuya_ota_upgrade_progress_report(ota, percent);
            ota->progress_percent = percent;
        }
        break;
    }

    case DL_EVENT_FINISH:
        PR_DEBUG("DL_EVENT_FINISH");
        PR_DEBUG("File Download Percent: %d%%", 100);
        tal_sha256_finish_ret(ota->sha256, file_hmac);
        tal_sha256_free(ota->sha256);
        hex2str((uint8_t *)file_sha256, file_hmac, 32);
        tal_sha256_mac((const uint8_t *)client->activate.seckey, strlen(client->activate.seckey), file_sha256, 32 * 2,
                       file_hmac);
        ascs2hex(self_hmac, (uint8_t *)(ota->msg.fw_hmac), FW_HMAC_LEN);
        if ((memcmp(self_hmac, file_hmac, 32) == 0)) {
            PR_DEBUG("file hmac check success");
            tuya_ota_upgrade_progress_report(ota, 100);
            tuya_ota_upgrade_status_report(ota, TUS_UPGRD_FINI);
            if (0 == ota->channel) {
                tal_ota_end_notify(TRUE);
            } else if (event_cb) {
                ota->event.id = TUYA_OTA_EVENT_FINISH;
                event_cb(&ota->msg, &ota->event);
            }
        }
        break;

    case DL_EVENT_FAULT:
        PR_DEBUG("DL_EVENT_FAULT");
        tuya_ota_upgrade_status_report(ota, TUS_UPGRD_EXEC);
        if (event_cb) {
            ota->event.id = TUYA_OTA_EVENT_FAULT;
            event_cb(&ota->msg, &ota->event);
        }
        break;

    default:
        break;
    }
}

/**
 * @brief Initializes the Tuya OTA (Over-The-Air) module.
 *
 * This function initializes the Tuya OTA module with the provided
 * configuration.
 *
 * @param config Pointer to the Tuya OTA configuration structure.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_ota_init(tuya_ota_config_t *config)
{
    if (s_ota_ctx) {
        return OPRT_OK;
    }

    s_ota_ctx = tal_malloc(sizeof(tuya_ota_t));
    if (NULL == s_ota_ctx) {
        return OPRT_MALLOC_FAILED;
    }
    memset(s_ota_ctx, 0, sizeof(tuya_ota_t));
    memcpy(&s_ota_ctx->config, config, sizeof(tuya_ota_config_t));

    return OPRT_OK;
}

static void ota_process_thread_func(void *arg)
{
    tuya_ota_t *ota = (tuya_ota_t *)arg;

    //! get ota cert
    uint8_t *cert = NULL;
    uint16_t cert_len = 0;

    tuya_iotdns_query_domain_certs(ota->msg.fw_url, &cert, &cert_len);

    http_download_config_t download_cfg;
    download_cfg.file_size = ota->msg.file_size;
    download_cfg.range_length = ota->config.range_size;
    download_cfg.timeout_ms = ota->config.timeout_ms;
    download_cfg.cacert = cert;
    download_cfg.cacert_len = cert_len;
    download_cfg.url = ota->msg.fw_url;
    download_cfg.event_handler = file_download_event_cb;
    download_cfg.user_data = ota;

    http_file_download(&download_cfg);
    tal_free(cert);
}

/**
 * @brief Starts the OTA (Over-The-Air) upgrade process.
 *
 * This function is used to initiate the OTA upgrade process for a device.
 *
 * @param upgrade A pointer to a cJSON object containing the upgrade
 * information.
 *
 * @return An integer value indicating the status of the OTA upgrade process.
 *         - 0: Success
 *         - Other values: Error codes indicating the reason for failure
 */
int tuya_ota_start(cJSON *upgrade)
{
    int rt = OPRT_OK;

    tuya_ota_t *ota = s_ota_ctx;

    ota->channel = cJSON_GetObjectItem(upgrade, "type")->valueint;
    ota->msg.file_size = atol(cJSON_GetObjectItem(upgrade, "size")->valuestring);
    strcpy(ota->msg.fw_url, cJSON_GetObjectItem(upgrade, "httpsUrl")->valuestring);
    strcpy(ota->msg.fw_hmac, cJSON_GetObjectItem(upgrade, "hmac")->valuestring);
    strcpy(ota->msg.fw_md5, cJSON_GetObjectItem(upgrade, "md5")->valuestring);

    THREAD_CFG_T thrd_param;
    thrd_param.priority = THREAD_PRIO_3;
    thrd_param.stackDepth = 4086;
    thrd_param.thrdname = "tuya_ota";
    rt = tal_thread_create_and_start(&(ota->upgrade_thrd), NULL, NULL, ota_process_thread_func, ota, &thrd_param);

    return rt;
}

/**
 * @brief Reports the upgrade status for Tuya OTA.
 *
 * This function is used to report the upgrade status for Tuya OTA.
 *
 * @param handle Pointer to the Tuya OTA handle.
 * @param status The upgrade status to be reported.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_ota_upgrade_status_report(tuya_ota_t *handle, int status)
{
    int ret = OPRT_OK;
    tuya_iot_client_t *client = handle->config.client;
    ret = matop_service_upgrade_status_update(&client->matop, handle->channel, status);
    return ret;
}

/**
 * @brief Reports the progress of the OTA upgrade.
 *
 * This function is used to report the progress of the OTA upgrade to the Tuya
 * Cloud.
 *
 * @param handle Pointer to the `tuya_ota_t` structure.
 * @param percent The progress percentage of the OTA upgrade.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_ota_upgrade_progress_report(tuya_ota_t *handle, int percent)
{
    tuya_iot_client_t *client = handle->config.client;
    return tuya_mqtt_upgrade_progress_report(&client->mqctx, handle->channel, percent);
}
