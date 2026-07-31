/**
 * @file example_camera_web.c
 * @brief Stream the camera to a web browser as MJPEG over HTTP.
 *
 * Flow: connect to WiFi (STA, hardcoded SSID/password) -> open the camera in
 * JPEG mode -> run a tiny HTTP server. Opening http://<board-ip>/ in a browser
 * shows the live camera feed (multipart/x-mixed-replace MJPEG stream).
 *
 * Intended for boards with a camera but no display (e.g. Seeed XIAO ESP32S3
 * Sense). The camera must support JPEG output (checked via supported_fmts);
 * browsers render MJPEG natively, so no host-side software is required.
 *
 * @note SSID/password are hardcoded below for demo simplicity. Single client
 *       at a time (a second browser waits until the first disconnects).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include <string.h>

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_memory.h"
#include "tal_network.h"
#include "tal_wifi.h"
#include "tkl_output.h"

#include "board_com_api.h"
#include "tdl_camera_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* ---- EDIT THESE: WiFi credentials for your network ---- */
#define WIFI_SSID "your-ssid"
#define WIFI_PSWD "your-password"

#define HTTP_PORT       (80)
#define MJPEG_BOUNDARY  "tuyaframe"

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_CAMERA_HANDLE_T sg_camera_hdl = NULL;
static THREAD_HANDLE       sg_http_thrd  = NULL;

/* Single-slot "latest JPEG frame" shared between the camera callback (producer)
 * and the HTTP stream loop (consumer). The producer overwrites the slot, so a
 * slow client just drops stale frames instead of building a backlog. */
static MUTEX_HANDLE sg_frame_mutex = NULL;
static uint8_t     *sg_jpeg_buf    = NULL;
static uint32_t     sg_jpeg_cap    = 0; /* allocated capacity */
static uint32_t     sg_jpeg_len    = 0; /* bytes of the current frame */
static uint32_t     sg_frame_seq   = 0; /* bumped on each new frame */

/***********************************************************
***********************function define**********************
***********************************************************/
static int __send_all(int fd, const void *buf, int len)
{
    const uint8_t *p = (const uint8_t *)buf;
    int sent = 0;
    while (sent < len) {
        int r = tal_net_send(fd, p + sent, len - sent);
        if (r <= 0) {
            return -1;
        }
        sent += r;
    }
    return sent;
}

/* Camera callback: copy the encoded JPEG frame into the shared latest-frame
 * slot. Runs on the camera task; keep it short and non-blocking. */
static OPERATE_RET __on_jpeg_frame(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    (void)hdl;

    if (NULL == frame || NULL == frame->data || 0 == frame->data_len) {
        return OPRT_OK;
    }

    tal_mutex_lock(sg_frame_mutex);

    if (sg_jpeg_cap < frame->data_len) {
        uint8_t *nb = (uint8_t *)Malloc(frame->data_len);
        if (NULL == nb) {
            tal_mutex_unlock(sg_frame_mutex);
            return OPRT_OK; /* drop this frame */
        }
        if (sg_jpeg_buf) {
            Free(sg_jpeg_buf);
        }
        sg_jpeg_buf = nb;
        sg_jpeg_cap = frame->data_len;
    }

    memcpy(sg_jpeg_buf, frame->data, frame->data_len);
    sg_jpeg_len = frame->data_len;
    sg_frame_seq++;

    tal_mutex_unlock(sg_frame_mutex);
    return OPRT_OK;
}

static void __serve_index(int fd)
{
    static const char *page =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>XIAO ESP32S3 Camera</title></head>"
        "<body style=\"margin:0;height:100vh;background:#111;display:flex;"
        "align-items:center;justify-content:center\">"
        "<img src=\"/stream\" style=\"max-width:100%;max-height:100vh\"/>"
        "</body></html>";
    __send_all(fd, page, (int)strlen(page));
}

/* Stream the latest JPEG frames as multipart/x-mixed-replace (MJPEG). Returns
 * when the client disconnects (a send fails). */
static void __serve_stream(int fd)
{
    static const char *hdr =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=" MJPEG_BOUNDARY "\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n";
    if (__send_all(fd, hdr, (int)strlen(hdr)) < 0) {
        return;
    }

    uint8_t *snd_buf = NULL;
    uint32_t snd_cap = 0;
    uint32_t last_seq = 0;
    char part_hdr[96];

    for (;;) {
        uint32_t len = 0;

        tal_mutex_lock(sg_frame_mutex);
        if (0 == sg_jpeg_len || sg_frame_seq == last_seq) {
            tal_mutex_unlock(sg_frame_mutex);
            tal_system_sleep(10); /* no new frame yet */
            continue;
        }
        last_seq = sg_frame_seq;
        len = sg_jpeg_len;
        if (snd_cap < len) {
            uint8_t *nb = (uint8_t *)Malloc(len);
            if (NULL == nb) {
                tal_mutex_unlock(sg_frame_mutex);
                break;
            }
            if (snd_buf) {
                Free(snd_buf);
            }
            snd_buf = nb;
            snd_cap = len;
        }
        memcpy(snd_buf, sg_jpeg_buf, len);
        tal_mutex_unlock(sg_frame_mutex);

        int n = snprintf(part_hdr, sizeof(part_hdr),
                         "--" MJPEG_BOUNDARY "\r\nContent-Type: image/jpeg\r\n"
                         "Content-Length: %u\r\n\r\n", (unsigned)len);
        if (__send_all(fd, part_hdr, n) < 0) {
            break;
        }
        if (__send_all(fd, snd_buf, (int)len) < 0) {
            break;
        }
        if (__send_all(fd, "\r\n", 2) < 0) {
            break;
        }
    }

    if (snd_buf) {
        Free(snd_buf);
    }
}

static void __handle_client(int fd)
{
    char req[256] = {0};
    int r = tal_net_recv(fd, req, sizeof(req) - 1);
    if (r <= 0) {
        return;
    }
    req[r] = '\0';

    if (NULL != strstr(req, "GET /stream")) {
        __serve_stream(fd);
    } else {
        __serve_index(fd);
    }
}

static void __http_server_task(void *arg)
{
    (void)arg;

    int listen_fd = tal_net_socket_create(PROTOCOL_TCP);
    if (listen_fd < 0) {
        PR_ERR("create listen socket failed");
        goto __EXIT;
    }
    tal_net_set_reuse(listen_fd);

    if (tal_net_bind(listen_fd, TY_IPADDR_ANY, HTTP_PORT) != 0) {
        PR_ERR("bind port %d failed", HTTP_PORT);
        tal_net_close(listen_fd);
        goto __EXIT;
    }
    tal_net_listen(listen_fd, 2);
    PR_NOTICE("HTTP server listening on port %d", HTTP_PORT);

    for (;;) {
        TUYA_IP_ADDR_T client_ip = 0;
        uint16_t client_port = 0;
        int sock_fd = tal_net_accept(listen_fd, &client_ip, &client_port);
        if (sock_fd < 0) {
            tal_system_sleep(20);
            continue;
        }
        PR_NOTICE("client %s:%d connected", tal_net_addr2str(client_ip), client_port);
        /* MJPEG sends many small boundary/header writes; disable Nagle so they
         * are not coalesced (~40ms delay), which otherwise causes visible lag. */
        tal_net_disable_nagle(sock_fd);
        __handle_client(sock_fd);
        tal_net_close(sock_fd);
        PR_NOTICE("client disconnected");
    }

__EXIT:
    {
        THREAD_HANDLE self = sg_http_thrd;
        sg_http_thrd = NULL;
        tal_thread_delete(self);
    }
}

static OPERATE_RET __camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;
    TDL_CAMERA_DEV_INFO_T info;

    sg_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (NULL == sg_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    /* This example streams MJPEG, so the camera must produce JPEG. */
    memset(&info, 0, sizeof(info));
    TUYA_CALL_ERR_RETURN(tdl_camera_dev_get_info(sg_camera_hdl, &info));
    if (info.supported_fmts != 0 && !(info.supported_fmts & TDL_CAMERA_FMT_JPEG)) {
        PR_ERR("camera does not support JPEG (supported_fmts=0x%x), cannot MJPEG-stream",
               info.supported_fmts);
        return OPRT_NOT_SUPPORTED;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.fps    = EXAMPLE_CAMERA_FPS;
    cfg.width  = EXAMPLE_CAMERA_WIDTH;
    cfg.height = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_JPEG;
    cfg.get_encoded_frame_cb = __on_jpeg_frame;

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_camera_hdl, &cfg));
    PR_NOTICE("camera opened %dx%d @%dfps JPEG", EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT,
              EXAMPLE_CAMERA_FPS);
    return OPRT_OK;
}

/* WiFi event callback: once connected (and IP acquired), start the HTTP server. */
static void __wifi_event_cb(WF_EVENT_E event, void *arg)
{
    (void)arg;

    switch (event) {
    case WFE_CONNECTED: {
        NW_IP_S ip = {0};
        if (OPRT_OK == tal_wifi_get_ip(WF_STATION, &ip)) {
            PR_NOTICE("=========================================");
            PR_NOTICE(" WiFi up. Open  http://%s/  in a browser", ip.ip);
            PR_NOTICE("=========================================");
        }
        if (NULL == sg_http_thrd) {
            THREAD_CFG_T cfg = {
                .thrdname   = "cam_http",
                .stackDepth = 1024 * 4,
                .priority   = THREAD_PRIO_2,
            };
            tal_thread_create_and_start(&sg_http_thrd, NULL, NULL, __http_server_task, NULL, &cfg);
        }
        break;
    }
    case WFE_CONNECT_FAILED:
        PR_ERR("WiFi connect failed");
        break;
    case WFE_DISCONNECTED:
        PR_WARN("WiFi disconnected");
        break;
    default:
        break;
    }
}

void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("------ camera web stream example start ------");
    PR_NOTICE("Board: %s", PLATFORM_BOARD);

    tal_mutex_create_init(&sg_frame_mutex);

    /* Register board peripherals (camera) and start capturing JPEG frames. */
    TUYA_CALL_ERR_LOG(board_register_hardware());
    TUYA_CALL_ERR_LOG(__camera_init());

    /* Connect to WiFi in station mode (raw tal_wifi, no cloud/tuya_iot). The
     * HTTP server starts from __wifi_event_cb on WFE_CONNECTED. */
    TUYA_CALL_ERR_LOG(tal_wifi_init(__wifi_event_cb));
    TUYA_CALL_ERR_LOG(tal_wifi_set_work_mode(WWM_STATION));
    PR_NOTICE("connecting to WiFi ssid: %s ...", WIFI_SSID);
    TUYA_CALL_ERR_LOG(tal_wifi_station_connect((int8_t *)WIFI_SSID, (int8_t *)WIFI_PSWD));
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
    while (1) {
        tal_system_sleep(500);
    }
}
#else
static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
