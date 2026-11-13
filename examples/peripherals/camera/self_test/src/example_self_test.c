/**
 * @file example_self_test.c
 * @brief Generic TDL camera lifecycle and capture self-test.
 *
 * The example operates only on generic camera device names. Board-specific
 * power, reset, MCLK and shared-bus handling remain in the board layer.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "crc32i.h"
#include "board_com_api.h"
#include "tdl_camera_manage.h"

#define TEST_FRAME_COUNT       SELF_TEST_FRAME_COUNT
#define TEST_TIMEOUT_MS        SELF_TEST_TIMEOUT_MS
#define TEST_WIDTH             SELF_TEST_WIDTH
#define TEST_HEIGHT            SELF_TEST_HEIGHT
#define TEST_FPS               SELF_TEST_FPS

typedef struct {
    volatile uint32_t frames;
    volatile uint32_t errors;
    volatile uint32_t last_crc32;
    volatile uint32_t last_length;
    volatile uint32_t last_nonzero;
    volatile uint8_t last_min;
    volatile uint8_t last_max;
} SELF_TEST_STATE_T;

static SELF_TEST_STATE_T sg_self_test;

static void __self_test_state_reset(void)
{
    memset((void *)&sg_self_test, 0, sizeof(sg_self_test));
    sg_self_test.last_min = 0xFFU;
}

static OPERATE_RET __self_test_frame_cb(TDL_CAMERA_HANDLE_T camera, TDL_CAMERA_FRAME_T *frame)
{
    uint32_t nonzero = 0U;
    uint8_t min_value = 0xFFU;
    uint8_t max_value = 0U;

    (void)camera;

    if (frame == NULL || frame->data == NULL || frame->data_len == 0U) {
        ++sg_self_test.errors;
        return OPRT_INVALID_PARM;
    }

    for (uint32_t i = 0U; i < frame->data_len; ++i) {
        uint8_t value = frame->data[i];
        nonzero += value != 0U;
        min_value = value < min_value ? value : min_value;
        max_value = value > max_value ? value : max_value;
    }

    sg_self_test.last_crc32 = hash_crc32i_total(frame->data, frame->data_len);
    sg_self_test.last_length = frame->data_len;
    sg_self_test.last_nonzero = nonzero;
    sg_self_test.last_min = min_value;
    sg_self_test.last_max = max_value;
    ++sg_self_test.frames;

    PR_NOTICE("frame %u: %ux%u fmt=%d len=%u crc=0x%08X range=%u..%u nonzero=%u",
              (unsigned)sg_self_test.frames, frame->width, frame->height, frame->fmt,
              (unsigned)frame->data_len, (unsigned)sg_self_test.last_crc32,
              (unsigned)min_value, (unsigned)max_value, (unsigned)nonzero);
    return OPRT_OK;
}

static OPERATE_RET __self_test_run_camera(const char *name)
{
    TDL_CAMERA_HANDLE_T camera;
    TDL_CAMERA_DEV_INFO_T info = {0};
    TDL_CAMERA_CFG_T cfg = {0};
    OPERATE_RET rt;
    uint32_t start_ms;

    PR_NOTICE("==== camera self-test: %s ====", name);
    camera = tdl_camera_find_dev((char *)name);
    if (camera == NULL) {
        PR_ERR("camera device %s not found", name);
        return OPRT_NOT_FOUND;
    }

    rt = tdl_camera_dev_get_info(camera, &info);
    if (rt != OPRT_OK) {
        PR_ERR("camera %s get info failed: %d", name, rt);
        return rt;
    }
    PR_NOTICE("device=%s max=%ux%u max_fps=%u formats=0x%X yuv_order=%d",
              name, info.max_width, info.max_height, info.max_fps,
              info.supported_fmts, info.yuv_order);

    __self_test_state_reset();
    cfg.fps = TEST_FPS;
    cfg.width = TEST_WIDTH;
    cfg.height = TEST_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = __self_test_frame_cb;

    rt = tdl_camera_dev_open(camera, &cfg);
    if (rt != OPRT_OK) {
        PR_ERR("camera %s open failed: %d", name, rt);
        return rt;
    }

    start_ms = (uint32_t)tal_system_get_millisecond();
    while (sg_self_test.frames < TEST_FRAME_COUNT &&
           (uint32_t)tal_system_get_millisecond() - start_ms < TEST_TIMEOUT_MS) {
        tal_system_sleep(10U);
    }

    rt = tdl_camera_dev_close(camera);
    if (sg_self_test.frames < TEST_FRAME_COUNT) {
        PR_ERR("camera %s capture timeout: frames=%u errors=%u", name,
               (unsigned)sg_self_test.frames, (unsigned)sg_self_test.errors);
        return rt != OPRT_OK ? rt : OPRT_TIMEOUT;
    }

    PR_NOTICE("camera %s result: frames=%u errors=%u elapsed=%ums last_crc=0x%08X",
              name, (unsigned)sg_self_test.frames, (unsigned)sg_self_test.errors,
              (unsigned)((uint32_t)tal_system_get_millisecond() - start_ms),
              (unsigned)sg_self_test.last_crc32);
    return rt;
}

void user_main(void)
{
    OPERATE_RET rt;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    PR_NOTICE("generic camera self-test start");

    rt = board_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return;
    }

    (void)__self_test_run_camera(CAMERA_NAME);

#if defined(ENABLE_CAMERA_2) && (ENABLE_CAMERA_2 == 1)
    if (strcmp(CAMERA_NAME, CAMERA_NAME_2) != 0) {
        (void)__self_test_run_camera(CAMERA_NAME_2);
        (void)__self_test_run_camera(CAMERA_NAME);
        (void)__self_test_run_camera(CAMERA_NAME_2);
    }
#else
    (void)__self_test_run_camera(CAMERA_NAME);
#endif

    PR_NOTICE("generic camera self-test finished");
    while (1) {
        tal_system_sleep(1000U);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
}
#else
static THREAD_HANDLE s_app_thread;

static void __app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_1,
        .thrdname = "self_test_app",
    };
    tal_thread_create_and_start(&s_app_thread, NULL, NULL, __app_thread, NULL, &cfg);
}
#endif
