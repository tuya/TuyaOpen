/**
 * @file desk_buddy_test.c
 * @brief Asynchronous hardware self-test implementation.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "desk_buddy_test.h"

#include <stdio.h>
#include <string.h>

#include "tal_api.h"

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
#include "tdl_camera_manage.h"
#endif

#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
#include "tkl_fs.h"
#endif

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
#include "tdl_encoder_manage.h"
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdl_led_manage.h"
#endif

#if defined(ENABLE_MOTION) && (ENABLE_MOTION == 1)
#include "tdl_motion_manage.h"
#endif

#define CAMERA_TEST_WIDTH        320U
#define CAMERA_TEST_HEIGHT       240U
#define CAMERA_TEST_FPS          15U
#define CAMERA_TEST_FRAMES       3U
#define CAMERA_TEST_TIMEOUT_MS   5000U
#define TEST_CANCEL_POLL_MS      10U

#ifndef DESK_BUDDY_SD_MOUNT_PATH
#define DESK_BUDDY_SD_MOUNT_PATH "/sdcard"
#endif

static QUEUE_HANDLE s_request_queue = NULL;
static QUEUE_HANDLE s_result_queue = NULL;
static THREAD_HANDLE s_worker_thread = NULL;
static MUTEX_HANDLE s_state_mutex = NULL;
static MUTEX_HANDLE s_encoder_mutex = NULL;
static volatile DESK_BUDDY_TEST_ID_E s_pending_id = DESK_BUDDY_TEST_MAX;
static volatile bool s_cancel_requested = false;

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
static TDL_ENCODER_HANDLE s_encoder = NULL;
#endif

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
static volatile uint32_t s_camera_frames;
static volatile uint32_t s_camera_errors;

static OPERATE_RET __camera_frame_cb(TDL_CAMERA_HANDLE_T camera,
                                     TDL_CAMERA_FRAME_T *frame)
{
    (void)camera;

    if (frame == NULL || frame->data == NULL || frame->data_len == 0U) {
        ++s_camera_errors;
        return OPRT_INVALID_PARM;
    }

    ++s_camera_frames;
    return OPRT_OK;
}
#endif

static bool __is_valid_id(DESK_BUDDY_TEST_ID_E id)
{
    return id >= 0 && id < DESK_BUDDY_TEST_MAX;
}

static bool __cancel_requested(DESK_BUDDY_TEST_ID_E id)
{
    return s_pending_id == id && s_cancel_requested;
}

static bool __sleep_cancelable(DESK_BUDDY_TEST_ID_E id, uint32_t duration_ms)
{
    uint32_t start_ms = (uint32_t)tal_system_get_millisecond();

    while ((uint32_t)tal_system_get_millisecond() - start_ms < duration_ms) {
        if (__cancel_requested(id)) {
            return true;
        }
        tal_system_sleep(TEST_CANCEL_POLL_MS);
    }

    return __cancel_requested(id);
}

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
static OPERATE_RET __close_encoder(void)
{
    TDL_ENCODER_HANDLE encoder;

    if (s_encoder_mutex == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_encoder_mutex);
    encoder = s_encoder;
    s_encoder = NULL;
    tal_mutex_unlock(s_encoder_mutex);

    if (encoder == NULL) {
        return OPRT_OK;
    }

    return tdl_encoder_close(encoder);
}
#endif

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
static void __test_camera(DESK_BUDDY_TEST_ID_E id, const char *name,
                          DESK_BUDDY_TEST_RESULT_T *result)
{
    TDL_CAMERA_HANDLE_T camera;
    TDL_CAMERA_CFG_T cfg = {0};
    OPERATE_RET rt;
    OPERATE_RET close_rt;
    uint32_t start_ms;
    bool canceled = false;

    if (name == NULL || name[0] == '\0') {
        result->status = DESK_BUDDY_TEST_STATUS_SKIPPED;
        snprintf(result->detail, sizeof(result->detail), "相机未启用");
        return;
    }

    if (__cancel_requested(id)) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "测试已停止");
        return;
    }

    camera = tdl_camera_find_dev((char *)name);
    if (camera == NULL) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "未找到设备: %s", name);
        return;
    }

    s_camera_frames = 0U;
    s_camera_errors = 0U;
    cfg.fps = CAMERA_TEST_FPS;
    cfg.width = CAMERA_TEST_WIDTH;
    cfg.height = CAMERA_TEST_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = __camera_frame_cb;

    rt = tdl_camera_dev_open(camera, &cfg);
    if (rt != OPRT_OK) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "相机打开失败: %d", rt);
        return;
    }

    start_ms = (uint32_t)tal_system_get_millisecond();
    while (s_camera_frames < CAMERA_TEST_FRAMES &&
           (uint32_t)tal_system_get_millisecond() - start_ms <
               CAMERA_TEST_TIMEOUT_MS) {
        if (__cancel_requested(id)) {
            canceled = true;
            break;
        }
        tal_system_sleep(TEST_CANCEL_POLL_MS);
    }

    close_rt = tdl_camera_dev_close(camera);
    if (close_rt != OPRT_OK) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "相机关闭失败: %d", close_rt);
        return;
    }

    if (canceled) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "相机已停止并关闭");
        return;
    }

    if (s_camera_frames < CAMERA_TEST_FRAMES) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "仅采集 %u 帧",
                 (unsigned)s_camera_frames);
        return;
    }

    result->status = DESK_BUDDY_TEST_STATUS_PASS;
    snprintf(result->detail, sizeof(result->detail), "%ux%u, 共 %u 帧，相机已关闭",
             (unsigned)CAMERA_TEST_WIDTH, (unsigned)CAMERA_TEST_HEIGHT,
             (unsigned)s_camera_frames);
}
#endif

#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
static void __test_sd_card(DESK_BUDDY_TEST_ID_E id,
                           DESK_BUDDY_TEST_RESULT_T *result)
{
    static const char test_path[] = DESK_BUDDY_SD_MOUNT_PATH "/desk_buddy_test.txt";
    char write_buf[64];
    char read_buf[64];
    TUYA_FILE file = NULL;
    int32_t value;
    int32_t io_rt;
    int32_t unmount_rt = 0;
    bool mounted = false;
    bool canceled = false;
    OPERATE_RET status = DESK_BUDDY_TEST_STATUS_FAIL;

    if (__cancel_requested(id)) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "测试已停止");
        return;
    }

    value = (int32_t)tal_system_get_random(0xFFFFFFFFU);
    snprintf(write_buf, sizeof(write_buf), "desk-buddy:%d", value);

    if (tkl_fs_mount(DESK_BUDDY_SD_MOUNT_PATH, DEV_SDCARD) != 0) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "存储卡挂载失败");
        return;
    }
    mounted = true;

    file = tkl_fopen(test_path, "w");
    if (file == NULL) {
        snprintf(result->detail, sizeof(result->detail), "写入文件打开失败");
        goto __TEST_SD_EXIT;
    }

    io_rt = tkl_fwrite(write_buf, (int32_t)strlen(write_buf), file);
    (void)tkl_fclose(file);
    file = NULL;
    if (io_rt != (int32_t)strlen(write_buf)) {
        snprintf(result->detail, sizeof(result->detail), "写入失败: %d", io_rt);
        goto __TEST_SD_EXIT;
    }

    memset(read_buf, 0, sizeof(read_buf));
    file = tkl_fopen(test_path, "r");
    if (file == NULL) {
        snprintf(result->detail, sizeof(result->detail), "读取文件打开失败");
        goto __TEST_SD_EXIT;
    }

    io_rt = tkl_fread(read_buf, sizeof(read_buf) - 1U, file);
    (void)tkl_fclose(file);
    file = NULL;
    if (io_rt <= 0 || strcmp(write_buf, read_buf) != 0) {
        snprintf(result->detail, sizeof(result->detail), "数据校验失败: %d", io_rt);
        goto __TEST_SD_EXIT;
    }

    status = DESK_BUDDY_TEST_STATUS_PASS;
    snprintf(result->detail, sizeof(result->detail), "读写通过: %d 字节，已卸载", io_rt);

__TEST_SD_EXIT:
    if (file != NULL) {
        (void)tkl_fclose(file);
    }
    (void)tkl_fs_remove(test_path);
    unmount_rt = tkl_fs_unmount(DESK_BUDDY_SD_MOUNT_PATH);
    canceled = __cancel_requested(id);

    if (canceled) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "存储卡已停止并卸载");
    } else if (status == DESK_BUDDY_TEST_STATUS_PASS && mounted &&
               unmount_rt != 0) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "存储卡卸载失败: %d", unmount_rt);
    } else {
        result->status = status;
    }
}
#endif

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
static void __test_encoder(DESK_BUDDY_TEST_ID_E id,
                           DESK_BUDDY_TEST_RESULT_T *result)
{
    TDL_ENCODER_CFG_T cfg = {
        .click_ms = 300U,
        .long_press_ms = 1500U,
        .long_hold_ms = 100U,
    };
    TDL_ENCODER_HANDLE encoder = NULL;
    int32_t position = 0;
    uint8_t pressed = 0U;
    bool close_encoder = false;
    OPERATE_RET rt;

    if (__cancel_requested(id)) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "测试已停止");
        return;
    }

    tal_mutex_lock(s_encoder_mutex);
    encoder = s_encoder;
    tal_mutex_unlock(s_encoder_mutex);

    if (encoder == NULL) {
        encoder = tdl_encoder_find_dev(ENCODER_NAME);
        if (encoder == NULL) {
            result->status = DESK_BUDDY_TEST_STATUS_FAIL;
            snprintf(result->detail, sizeof(result->detail), "未找到编码器");
            return;
        }

        rt = tdl_encoder_open(&encoder, &cfg);
        if (rt != OPRT_OK) {
            result->status = DESK_BUDDY_TEST_STATUS_FAIL;
            snprintf(result->detail, sizeof(result->detail), "编码器打开失败: %d", rt);
            return;
        }
    }

    tal_mutex_lock(s_encoder_mutex);
    s_encoder = encoder;

    if (__cancel_requested(id)) {
        s_encoder = NULL;
        close_encoder = true;
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "编码器已停止并关闭");
    } else {
        rt = tdl_encoder_read_position(encoder, &position);
        if (rt == OPRT_OK) {
            rt = tdl_encoder_read_button(encoder, &pressed);
        }

        if (__cancel_requested(id)) {
            s_encoder = NULL;
            close_encoder = true;
            result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
            snprintf(result->detail, sizeof(result->detail), "编码器已停止并关闭");
        } else if (rt != OPRT_OK) {
            s_encoder = NULL;
            close_encoder = true;
            result->status = DESK_BUDDY_TEST_STATUS_FAIL;
            snprintf(result->detail, sizeof(result->detail), "编码器读取失败: %d", rt);
        } else {
            result->status = DESK_BUDDY_TEST_STATUS_PASS;
            snprintf(result->detail, sizeof(result->detail), "位置 %d, 按键%s",
                     position, pressed != 0U ? "按下" : "松开");
        }
    }
    tal_mutex_unlock(s_encoder_mutex);

    if (close_encoder) {
        (void)tdl_encoder_close(encoder);
    }
}
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
static void __test_led(DESK_BUDDY_TEST_ID_E id,
                       DESK_BUDDY_TEST_RESULT_T *result)
{
    TDL_LED_HANDLE_T led1 = tdl_led_find_dev(LED_NAME);
    TDL_LED_HANDLE_T led2 = NULL;
    OPERATE_RET open_rt1;
    OPERATE_RET open_rt2 = OPRT_OK;
    OPERATE_RET close_rt1;
    OPERATE_RET close_rt2 = OPRT_OK;
    bool canceled = false;

    if (led1 == NULL) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "未找到灯设备");
        return;
    }

#if defined(ENABLE_LED_2) && (ENABLE_LED_2 == 1)
    led2 = tdl_led_find_dev(LED_NAME_2);
    if (led2 == NULL) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "未找到第二路灯设备");
        return;
    }
#endif

    if (__cancel_requested(id)) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "测试已停止");
        return;
    }

    open_rt1 = tdl_led_open(led1);
    if (open_rt1 == OPRT_OK && led2 != NULL) {
        open_rt2 = tdl_led_open(led2);
    }
    if (open_rt1 != OPRT_OK || open_rt2 != OPRT_OK) {
        if (open_rt1 == OPRT_OK) {
            (void)tdl_led_close(led1);
        }
        if (open_rt2 == OPRT_OK && led2 != NULL) {
            (void)tdl_led_close(led2);
        }
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "灯打开失败: %d/%d",
                 open_rt1, open_rt2);
        return;
    }

    (void)tdl_led_set_status(led1, TDL_LED_ON);
    if (led2 != NULL) {
        (void)tdl_led_set_status(led2, TDL_LED_OFF);
    }
    canceled = __sleep_cancelable(id, 700U);

    if (!canceled) {
        (void)tdl_led_set_status(led1, TDL_LED_OFF);
        if (led2 != NULL) {
            (void)tdl_led_set_status(led2, TDL_LED_ON);
        }
        canceled = __sleep_cancelable(id, 700U);
    }

    (void)tdl_led_set_status(led1, TDL_LED_OFF);
    if (led2 != NULL) {
        (void)tdl_led_set_status(led2, TDL_LED_OFF);
        close_rt2 = tdl_led_close(led2);
    }
    close_rt1 = tdl_led_close(led1);

    if (close_rt1 != OPRT_OK || close_rt2 != OPRT_OK) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "灯关闭失败: %d/%d",
                 close_rt1, close_rt2);
    } else if (canceled) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "灯已停止并关闭");
    } else {
        result->status = DESK_BUDDY_TEST_STATUS_PASS;
        snprintf(result->detail, sizeof(result->detail), "两路灯依次点亮后已关闭");
    }
}
#endif

#if defined(ENABLE_MOTION) && (ENABLE_MOTION == 1)
static void __test_motion(DESK_BUDDY_TEST_ID_E id,
                          DESK_BUDDY_TEST_RESULT_T *result)
{
    TDL_MOTION_HANDLE motion;
    TDL_MOTION_OPEN_CFG_T open_cfg = {
        .sample_cfg = {
            .accel_range = TDL_MOTION_ACCEL_RANGE_2G,
            .gyro_range = TDL_MOTION_GYRO_RANGE_2000DPS,
            .sample_rate_hz = 100U,
            .low_power = false,
        },
        .events_enabled = false,
    };
    TDL_MOTION_DATA_T data;
    OPERATE_RET rt;
    int index;
    bool canceled = false;

    if (__cancel_requested(id)) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "测试已停止");
        return;
    }

    motion = tdl_motion_find_dev(MOTION_NAME);
    if (motion == NULL) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "未找到加速度计");
        return;
    }

    rt = tdl_motion_open(&motion, &open_cfg);
    if (rt != OPRT_OK) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "加速度计打开失败: %d", rt);
        return;
    }

    for (index = 0; index < 5; ++index) {
        if (__cancel_requested(id)) {
            canceled = true;
            break;
        }

        memset(&data, 0, sizeof(data));
        rt = tdl_motion_read(motion, &data);
        if (rt == OPRT_OK && data.accel_valid) {
            break;
        }
        tal_system_sleep(TEST_CANCEL_POLL_MS);
    }
    (void)tdl_motion_close(motion);

    if (canceled) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "加速度计已停止并关闭");
        return;
    }

    if (rt != OPRT_OK || !data.accel_valid) {
        result->status = DESK_BUDDY_TEST_STATUS_FAIL;
        snprintf(result->detail, sizeof(result->detail), "加速度读取失败: %d", rt);
        return;
    }

    result->status = DESK_BUDDY_TEST_STATUS_PASS;
    snprintf(result->detail, sizeof(result->detail), "加速度 %d, %d, %d，已关闭",
             (int)data.accel_mps2.x, (int)data.accel_mps2.y,
             (int)data.accel_mps2.z);
}
#endif

static void __run_test(DESK_BUDDY_TEST_ID_E id,
                       DESK_BUDDY_TEST_RESULT_T *result)
{
    memset(result, 0, sizeof(*result));
    result->id = id;

    if (__cancel_requested(id)) {
        result->status = DESK_BUDDY_TEST_STATUS_STOPPED;
        snprintf(result->detail, sizeof(result->detail), "测试已停止");
        return;
    }

    switch (id) {
#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
    case DESK_BUDDY_TEST_FRONT_CAMERA:
        __test_camera(id, CAMERA_NAME, result);
        break;
#if defined(ENABLE_CAMERA_2) && (ENABLE_CAMERA_2 == 1)
    case DESK_BUDDY_TEST_REAR_CAMERA:
        __test_camera(id, CAMERA_NAME_2, result);
        break;
#endif
#endif
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    case DESK_BUDDY_TEST_SD_CARD:
        __test_sd_card(id, result);
        break;
#endif
#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
    case DESK_BUDDY_TEST_ENCODER:
        __test_encoder(id, result);
        break;
#endif
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    case DESK_BUDDY_TEST_LED:
        __test_led(id, result);
        break;
#endif
#if defined(ENABLE_MOTION) && (ENABLE_MOTION == 1)
    case DESK_BUDDY_TEST_MOTION:
        __test_motion(id, result);
        break;
#endif
    default:
        result->status = DESK_BUDDY_TEST_STATUS_SKIPPED;
        snprintf(result->detail, sizeof(result->detail), "当前功能未启用");
        break;
    }
}

static void __test_worker(void *arg)
{
    DESK_BUDDY_TEST_ID_E id;
    DESK_BUDDY_TEST_RESULT_T result;

    (void)arg;

    while (1) {
        if (tal_queue_fetch(s_request_queue, &id, QUEUE_WAIT_FOREVER) != OPRT_OK) {
            continue;
        }

        __run_test(id, &result);

        tal_mutex_lock(s_state_mutex);
        if (s_pending_id == id) {
            s_pending_id = DESK_BUDDY_TEST_MAX;
            s_cancel_requested = false;
        }
        tal_mutex_unlock(s_state_mutex);

        (void)tal_queue_post(s_result_queue, &result, 0U);
    }
}

OPERATE_RET desk_buddy_test_init(void)
{
    OPERATE_RET rt;
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 8192U,
        .priority = THREAD_PRIO_2,
        .thrdname = "desk_buddy_test",
    };

    if (s_request_queue != NULL) {
        return OPRT_OK;
    }

    rt = tal_mutex_create_init(&s_state_mutex);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tal_mutex_create_init(&s_encoder_mutex);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tal_queue_create_init(&s_request_queue,
                               sizeof(DESK_BUDDY_TEST_ID_E), 8);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tal_queue_create_init(&s_result_queue,
                               sizeof(DESK_BUDDY_TEST_RESULT_T), 8);
    if (rt != OPRT_OK) {
        return rt;
    }

    return tal_thread_create_and_start(&s_worker_thread, NULL, NULL,
                                       __test_worker, NULL, &thread_cfg);
}

OPERATE_RET desk_buddy_test_request(DESK_BUDDY_TEST_ID_E id)
{
    OPERATE_RET rt;

    if (s_request_queue == NULL || s_state_mutex == NULL ||
        !__is_valid_id(id)) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_state_mutex);
    if (s_pending_id != DESK_BUDDY_TEST_MAX) {
        tal_mutex_unlock(s_state_mutex);
        return OPRT_RESOURCE_NOT_READY;
    }
    s_pending_id = id;
    s_cancel_requested = false;
    tal_mutex_unlock(s_state_mutex);

    rt = tal_queue_post(s_request_queue, &id, 0U);
    if (rt != OPRT_OK) {
        tal_mutex_lock(s_state_mutex);
        if (s_pending_id == id) {
            s_pending_id = DESK_BUDDY_TEST_MAX;
            s_cancel_requested = false;
        }
        tal_mutex_unlock(s_state_mutex);
    }

    return rt;
}

OPERATE_RET desk_buddy_test_stop(DESK_BUDDY_TEST_ID_E id)
{
    if (s_state_mutex == NULL || !__is_valid_id(id)) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_state_mutex);
    if (s_pending_id == id) {
        s_cancel_requested = true;
    }
    tal_mutex_unlock(s_state_mutex);

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
    if (id == DESK_BUDDY_TEST_ENCODER) {
        return __close_encoder();
    }
#endif

    return OPRT_OK;
}

bool desk_buddy_test_poll_result(DESK_BUDDY_TEST_RESULT_T *result)
{
    if (s_result_queue == NULL || result == NULL) {
        return false;
    }

    return tal_queue_fetch(s_result_queue, result, 0U) == OPRT_OK;
}

OPERATE_RET desk_buddy_test_get_encoder_state(int32_t *position, uint8_t *pressed)
{
#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
    TDL_ENCODER_HANDLE encoder;
    OPERATE_RET rt;

    if (position == NULL || pressed == NULL || s_encoder_mutex == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_encoder_mutex);
    encoder = s_encoder;
    if (encoder == NULL) {
        tal_mutex_unlock(s_encoder_mutex);
        return OPRT_NOT_FOUND;
    }

    rt = tdl_encoder_read_position(encoder, position);
    if (rt == OPRT_OK) {
        rt = tdl_encoder_read_button(encoder, pressed);
    }
    tal_mutex_unlock(s_encoder_mutex);
    return rt;
#else
    (void)position;
    (void)pressed;
    return OPRT_NOT_SUPPORTED;
#endif
}
