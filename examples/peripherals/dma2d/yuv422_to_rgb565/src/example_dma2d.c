/**
 * @file example_dma2d.c
 * @brief Validate the ESP32 PPA-backed tal_dma2d implementation.
 *
 * The test submits a YUV422(UYVY) to RGB565 conversion and a same-format
 * YUV422 block copy.  Both operations must complete through the asynchronous
 * tal_dma2d interface.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "tal_api.h"
#include "tal_dma2d.h"
#include "tkl_output.h"
#include "board_com_api.h"

#if defined(PLATFORM_ESP32)
#include "esp_heap_caps.h"
#endif

#define DMA2D_TEST_WIDTH  320
#define DMA2D_TEST_HEIGHT 240
#define DMA2D_TEST_BPP_YUV422 2
#define DMA2D_TEST_BPP_RGB565 2
#define DMA2D_TEST_ALIGN 64

static void *__test_alloc(size_t size)
{
#if defined(PLATFORM_ESP32)
    return heap_caps_aligned_alloc(DMA2D_TEST_ALIGN, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
#else
    return tal_malloc(size);
#endif
}

static void __test_free(void *ptr)
{
#if defined(PLATFORM_ESP32)
    heap_caps_free(ptr);
#else
    tal_free(ptr);
#endif
}

static void __fill_uyvy(uint8_t *buf, uint16_t width, uint16_t height)
{
    const size_t line_size = (size_t)width * DMA2D_TEST_BPP_YUV422;

    for (uint16_t y = 0; y < height; y++) {
        uint8_t *line = buf + (size_t)y * line_size;
        for (uint16_t x = 0; x < width; x += 2) {
            const uint8_t y0 = (uint8_t)(32 + ((uint32_t)x * 200U) / width);
            const uint8_t y1 = (uint8_t)(32 + ((uint32_t)(x + 1) * 200U) / width);

            /* UYVY: neutral chroma and a horizontal luminance ramp. */
            line[x * 2 + 0] = 128;
            line[x * 2 + 1] = y0;
            line[x * 2 + 2] = 128;
            line[x * 2 + 3] = y1;
        }
    }
}

static bool __buffer_changed(const uint8_t *buf, size_t size, uint8_t value)
{
    for (size_t i = 0; i < size; i++) {
        if (buf[i] != value) {
            return true;
        }
    }
    return false;
}

static OPERATE_RET __run_dma2d_test(void)
{
    const size_t yuv_size = (size_t)DMA2D_TEST_WIDTH * DMA2D_TEST_HEIGHT * DMA2D_TEST_BPP_YUV422;
    const size_t rgb_size = (size_t)DMA2D_TEST_WIDTH * DMA2D_TEST_HEIGHT * DMA2D_TEST_BPP_RGB565;
    uint8_t *src_buf = NULL;
    uint8_t *rgb_buf = NULL;
    uint8_t *copy_buf = NULL;
    TAL_DMA2D_HANDLE_T dma2d = NULL;
    TKL_DMA2D_FRAME_INFO_T src = {0};
    TKL_DMA2D_FRAME_INFO_T rgb = {0};
    TKL_DMA2D_FRAME_INFO_T copy = {0};
    OPERATE_RET rt = OPRT_OK;

    src_buf = __test_alloc(yuv_size);
    rgb_buf = __test_alloc(rgb_size);
    copy_buf = __test_alloc(yuv_size);
    if (src_buf == NULL || rgb_buf == NULL || copy_buf == NULL) {
        PR_ERR("DMA2D test buffer allocation failed");
        rt = OPRT_MALLOC_FAILED;
        goto __exit;
    }

    __fill_uyvy(src_buf, DMA2D_TEST_WIDTH, DMA2D_TEST_HEIGHT);
    memset(rgb_buf, 0xA5, rgb_size);
    memset(copy_buf, 0xA5, rgb_size);

    rt = tal_dma2d_init(&dma2d);
    if (rt != OPRT_OK) {
        PR_ERR("tal_dma2d_init failed: %d", rt);
        goto __exit;
    }

    src.type = TUYA_FRAME_FMT_YUV422;
    src.pbuf = src_buf;
    src.width = DMA2D_TEST_WIDTH;
    src.height = DMA2D_TEST_HEIGHT;

    rgb.type = TUYA_FRAME_FMT_RGB565;
    rgb.pbuf = rgb_buf;
    rgb.width = DMA2D_TEST_WIDTH;
    rgb.height = DMA2D_TEST_HEIGHT;

    PR_NOTICE("DMA2D test: YUV422(UYVY) -> RGB565");
    rt = tal_dma2d_convert(dma2d, &src, &rgb);
    if (rt != OPRT_OK) {
        PR_ERR("tal_dma2d_convert submit failed: %d", rt);
        goto __exit;
    }
    rt = tal_dma2d_wait_finish(dma2d, 5000);
    if (rt != OPRT_OK || !__buffer_changed(rgb_buf, rgb_size, 0xA5)) {
        PR_ERR("YUV422 -> RGB565 test failed: wait=%d first=0x%02x", rt, rgb_buf[0]);
        rt = OPRT_COM_ERROR;
        goto __exit;
    }
    PR_NOTICE("YUV422 -> RGB565 passed, first pixel=0x%02x%02x", rgb_buf[1], rgb_buf[0]);

    copy.type = TUYA_FRAME_FMT_RGB565;
    copy.pbuf = copy_buf;
    copy.width = DMA2D_TEST_WIDTH;
    copy.height = DMA2D_TEST_HEIGHT;

    PR_NOTICE("DMA2D test: RGB565 memcpy");
    rt = tal_dma2d_memcpy(dma2d, &rgb, &copy);
    if (rt != OPRT_OK) {
        PR_ERR("tal_dma2d_memcpy submit failed: %d", rt);
        goto __exit;
    }
    rt = tal_dma2d_wait_finish(dma2d, 5000);
    if (rt != OPRT_OK || memcmp(rgb_buf, copy_buf, rgb_size) != 0) {
        PR_ERR("RGB565 memcpy test failed: %d", rt);
        rt = OPRT_COM_ERROR;
        goto __exit;
    }
    PR_NOTICE("RGB565 memcpy passed");

__exit:
    if (dma2d != NULL) {
        tal_dma2d_deinit(dma2d);
    }
    __test_free(copy_buf);
    __test_free(rgb_buf);
    __test_free(src_buf);
    return rt;
}

void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    board_register_hardware();

    PR_NOTICE("Starting tal_dma2d hardware test on %s", PLATFORM_BOARD);
    if (__run_dma2d_test() == OPRT_OK) {
        PR_NOTICE("DMA2D hardware test PASSED");
    } else {
        PR_ERR("DMA2D hardware test FAILED");
    }

    while (1) {
        tal_system_sleep(1000);
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
static THREAD_HANDLE s_dma2d_thread = NULL;

static void __dma2d_test_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(s_dma2d_thread);
    s_dma2d_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T cfg = {
        .stackDepth = 1024 * 6,
        .priority = THREAD_PRIO_1,
        .thrdname = "dma2d_test",
    };

    tal_thread_create_and_start(&s_dma2d_thread, NULL, NULL, __dma2d_test_thread, NULL, &cfg);
}
#endif
