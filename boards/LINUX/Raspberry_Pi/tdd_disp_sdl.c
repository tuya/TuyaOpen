/**
 * @file tdd_disp_sdl.c
 * @brief SDL2 based virtual display driver for LINUX/Raspberry_Pi.
 *
 * This driver registers a TDL display device which renders the framebuffer
 * into a desktop window using SDL2. It is intended for Raspberry Pi Desktop
 * to preview LVGL UI.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#if defined(ENABLE_SDL_DISPLAY) && (ENABLE_SDL_DISPLAY == 1)

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

#include "tdl_display_driver.h"
#include "tkl_gpio.h"

#include "tal_mutex.h"
#include "tal_semaphore.h"
#include "tal_thread.h"

#if defined(ENABLE_SDL_TP) && (ENABLE_SDL_TP == 1)
#include "tdl_tp_driver.h"
#endif

#ifndef TUYA_SDL_WHEEL_PIXEL_PER_STEP
#define TUYA_SDL_WHEEL_PIXEL_PER_STEP 40
#endif

#ifndef TUYA_SDL_WHEEL_SWIPE_STEP
#define TUYA_SDL_WHEEL_SWIPE_STEP 12
#endif

typedef struct {
    bool inited;
    uint16_t width;
    uint16_t height;

    /* Last known pointer position in window coordinate. */
    uint16_t x;
    uint16_t y;

    /* Current pressed state (mouse left button or finger down). */
    bool pressed;

    /* Accumulated wheel delta (SDL wheel.y: +up, -down). */
    int32_t wheel_y;

    MUTEX_HANDLE mutex;
} TDD_SDL_INPUT_T;

static TDD_SDL_INPUT_T sg_sdl_input;

static uint16_t __sdl_clamp_u16(int32_t v, uint16_t min_v, uint16_t max_v)
{
    if (v < (int32_t)min_v) {
        return min_v;
    }
    if (v > (int32_t)max_v) {
        return max_v;
    }
    return (uint16_t)v;
}

static int32_t __sdl_clamp_i32(int32_t v, int32_t min_v, int32_t max_v)
{
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

static void __sdl_input_reset_locked(uint16_t width, uint16_t height)
{
    sg_sdl_input.width = width;
    sg_sdl_input.height = height;
    sg_sdl_input.x = width / 2;
    sg_sdl_input.y = height / 2;
    sg_sdl_input.pressed = false;
    sg_sdl_input.wheel_y = 0;
}

static OPERATE_RET __sdl_input_init(uint16_t width, uint16_t height)
{
    OPERATE_RET rt = OPRT_OK;
    if (width == 0 || height == 0) {
        return OPRT_INVALID_PARM;
    }

    if (!sg_sdl_input.inited) {
        memset(&sg_sdl_input, 0, sizeof(sg_sdl_input));
        rt = tal_mutex_create_init(&sg_sdl_input.mutex);
        if (rt != OPRT_OK) {
            return rt;
        }
        sg_sdl_input.inited = true;
    }

    tal_mutex_lock(sg_sdl_input.mutex);
    __sdl_input_reset_locked(width, height);
    tal_mutex_unlock(sg_sdl_input.mutex);

    return OPRT_OK;
}

static void __sdl_input_deinit(void)
{
    if (!sg_sdl_input.inited) {
        return;
    }

    if (sg_sdl_input.mutex) {
        tal_mutex_release(sg_sdl_input.mutex);
        sg_sdl_input.mutex = NULL;
    }

    sg_sdl_input.inited = false;
}

static void __sdl_handle_event(const SDL_Event *event)
{
    if (!sg_sdl_input.inited || event == NULL) {
        return;
    }

    tal_mutex_lock(sg_sdl_input.mutex);

    switch (event->type) {
    case SDL_MOUSEMOTION:
        sg_sdl_input.x = __sdl_clamp_u16(event->motion.x, 0, sg_sdl_input.width - 1);
        sg_sdl_input.y = __sdl_clamp_u16(event->motion.y, 0, sg_sdl_input.height - 1);
        break;

    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            sg_sdl_input.pressed = true;
            sg_sdl_input.x = __sdl_clamp_u16(event->button.x, 0, sg_sdl_input.width - 1);
            sg_sdl_input.y = __sdl_clamp_u16(event->button.y, 0, sg_sdl_input.height - 1);
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            sg_sdl_input.pressed = false;
            sg_sdl_input.x = __sdl_clamp_u16(event->button.x, 0, sg_sdl_input.width - 1);
            sg_sdl_input.y = __sdl_clamp_u16(event->button.y, 0, sg_sdl_input.height - 1);
        }
        break;

    case SDL_MOUSEWHEEL:
        sg_sdl_input.wheel_y += event->wheel.y;
        break;

    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP: {
        const int32_t x = (int32_t)(event->tfinger.x * (float)sg_sdl_input.width);
        const int32_t y = (int32_t)(event->tfinger.y * (float)sg_sdl_input.height);
        sg_sdl_input.x = __sdl_clamp_u16(x, 0, sg_sdl_input.width - 1);
        sg_sdl_input.y = __sdl_clamp_u16(y, 0, sg_sdl_input.height - 1);
        sg_sdl_input.pressed = (event->type != SDL_FINGERUP);
        break;
    }

    default:
        break;
    }

    tal_mutex_unlock(sg_sdl_input.mutex);
}

static bool __sdl_input_get_touch(uint16_t *x, uint16_t *y, bool *pressed)
{
    if (!sg_sdl_input.inited) {
        return false;
    }

    tal_mutex_lock(sg_sdl_input.mutex);

    if (x) {
        *x = sg_sdl_input.x;
    }
    if (y) {
        *y = sg_sdl_input.y;
    }
    if (pressed) {
        *pressed = sg_sdl_input.pressed;
    }

    tal_mutex_unlock(sg_sdl_input.mutex);

    return true;
}

static int32_t __sdl_input_consume_wheel_y(void)
{
    if (!sg_sdl_input.inited) {
        return 0;
    }

    tal_mutex_lock(sg_sdl_input.mutex);
    int32_t v = sg_sdl_input.wheel_y;
    sg_sdl_input.wheel_y = 0;
    tal_mutex_unlock(sg_sdl_input.mutex);

    return v;
}


typedef struct {
    uint16_t width;
    uint16_t height;

    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    uint32_t     *conv_argb;
    uint32_t      conv_pixels;
    uint32_t      sdl_tid;
    bool          use_surface;
    THREAD_HANDLE thread;
    MUTEX_HANDLE  frame_mutex;
    SEM_HANDLE    ready_sem;
    SEM_HANDLE    frame_sem;
    SEM_HANDLE    exit_sem;
    bool          has_frame;
    bool          inited;
    bool          quit;
} TDD_DISP_SDL_T;

static uint32_t sg_sdl_flush_cnt = 0;

static void __sdl_thread_cb(void *args);
static void __sdl_present_locked(TDD_DISP_SDL_T *dev);

static void __sdl_poll_events(TDD_DISP_SDL_T *dev)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        __sdl_handle_event(&event);
        if (event.type == SDL_QUIT) {
            dev->quit = true;
        }

        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
            dev->quit = true;
        }
    }
}

static OPERATE_RET __sdl_open(TDD_DISP_DEV_HANDLE_T device)
{
    TDD_DISP_SDL_T *dev = (TDD_DISP_SDL_T *)device;
    OPERATE_RET rt = OPRT_OK;
    if (dev == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (dev->inited) {
        return OPRT_OK;
    }

    if (dev->frame_mutex == NULL) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&dev->frame_mutex));
    }
    if (dev->ready_sem == NULL) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&dev->ready_sem, 0, 1));
    }
    if (dev->frame_sem == NULL) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&dev->frame_sem, 0, 1));
    }
    if (dev->exit_sem == NULL) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&dev->exit_sem, 0, 1));
    }

    if (dev->conv_argb == NULL) {
        dev->conv_pixels = (uint32_t)dev->width * (uint32_t)dev->height;
        dev->conv_argb = (uint32_t *)tal_malloc(dev->conv_pixels * sizeof(uint32_t));
        if (!dev->conv_argb) {
            PR_ERR("alloc conv buffer failed: %u pixels", dev->conv_pixels);
            return OPRT_MALLOC_FAILED;
        }
    }

    dev->quit = false;
    dev->has_frame = false;
    dev->inited = false;

    if (dev->thread == NULL) {
        THREAD_CFG_T cfg = {0};
        cfg.stackDepth = 16384;
        cfg.priority = THREAD_PRIO_3;
        cfg.thrdname = "sdl_disp";
        TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&dev->thread, NULL, NULL, __sdl_thread_cb, dev, &cfg));
    }

    if (tal_semaphore_wait(dev->ready_sem, 5000) != OPRT_OK) {
        PR_ERR("wait sdl init timeout");
        return OPRT_COM_ERROR;
    }

    return dev->inited ? OPRT_OK : OPRT_COM_ERROR;
}

static void __sdl_present_locked(TDD_DISP_SDL_T *dev)
{
    if (dev == NULL || dev->window == NULL || dev->conv_argb == NULL) {
        return;
    }

    if (dev->use_surface) {
        SDL_Surface *surf = SDL_GetWindowSurface(dev->window);
        if (!surf || !surf->format) {
            PR_ERR("SDL_GetWindowSurface failed: %s", SDL_GetError());
            return;
        }

        if (SDL_MUSTLOCK(surf) && SDL_LockSurface(surf) != 0) {
            PR_ERR("SDL_LockSurface failed: %s", SDL_GetError());
            return;
        }

        int conv_rt = SDL_ConvertPixels((int)dev->width,
                                        (int)dev->height,
                                        SDL_PIXELFORMAT_ARGB8888,
                                        dev->conv_argb,
                                        (int)dev->width * 4,
                                        surf->format->format,
                                        surf->pixels,
                                        surf->pitch);

        if (SDL_MUSTLOCK(surf)) {
            SDL_UnlockSurface(surf);
        }

        if (conv_rt != 0) {
            PR_ERR("SDL_ConvertPixels failed: %s", SDL_GetError());
            return;
        }
        if (SDL_UpdateWindowSurface(dev->window) != 0) {
            PR_ERR("SDL_UpdateWindowSurface failed: %s", SDL_GetError());
            return;
        }
        return;
    }

    if (dev->renderer == NULL || dev->texture == NULL) {
        return;
    }

    void *tex_pixels = NULL;
    int tex_pitch = 0;
    if (SDL_LockTexture(dev->texture, NULL, &tex_pixels, &tex_pitch) != 0) {
        PR_ERR("SDL_LockTexture failed: %s", SDL_GetError());
        return;
    }

    const uint32_t w = (uint32_t)dev->width;
    const uint32_t h = (uint32_t)dev->height;
    const uint32_t row_bytes = w * 4;
    uint8_t *dst_row = (uint8_t *)tex_pixels;
    const uint8_t *src_row = (const uint8_t *)dev->conv_argb;
    for (uint32_t y = 0; y < h; y++) {
        memcpy(dst_row, src_row, row_bytes);
        dst_row += (uint32_t)tex_pitch;
        src_row += row_bytes;
    }
    SDL_UnlockTexture(dev->texture);

    SDL_RenderClear(dev->renderer);
    if (SDL_RenderCopy(dev->renderer, dev->texture, NULL, NULL) != 0) {
        PR_ERR("SDL_RenderCopy failed: %s", SDL_GetError());
    }
    SDL_RenderPresent(dev->renderer);
}

static void __sdl_thread_cb(void *args)
{
    TDD_DISP_SDL_T *dev = (TDD_DISP_SDL_T *)args;
    if (dev == NULL) {
        return;
    }

    dev->sdl_tid = SDL_ThreadID();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        PR_ERR("SDL_Init failed: %s", SDL_GetError());
        dev->inited = false;
        (void)tal_semaphore_post(dev->ready_sem);
        (void)tal_semaphore_post(dev->exit_sem);
        return;
    }

    PR_NOTICE("SDL video driver: %s", SDL_GetCurrentVideoDriver());
    {
        const char *disp = getenv("DISPLAY");
        const char *wayland = getenv("WAYLAND_DISPLAY");
        const char *session = getenv("XDG_SESSION_TYPE");
        PR_NOTICE("env DISPLAY=%s WAYLAND_DISPLAY=%s XDG_SESSION_TYPE=%s",
                  disp ? disp : "(null)",
                  wayland ? wayland : "(null)",
                  session ? session : "(null)");
    }

    {
        const char *use_surface = getenv("TUYA_SDL_USE_SURFACE");
        dev->use_surface = (use_surface != NULL && strcmp(use_surface, "0") != 0);
        if (dev->use_surface) {
            PR_NOTICE("SDL present mode: window surface (TUYA_SDL_USE_SURFACE=%s)", use_surface);
        }
    }

    const char *title = "TuyaOpen";
#if defined(SDL_DISPLAY_TITLE)
    title = SDL_DISPLAY_TITLE;
#elif defined(CONFIG_SDL_DISPLAY_TITLE)
    title = CONFIG_SDL_DISPLAY_TITLE;
#endif

    dev->window = SDL_CreateWindow(title,
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   dev->width,
                                   dev->height,
                                   SDL_WINDOW_SHOWN);
    if (!dev->window) {
        PR_ERR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        dev->inited = false;
        (void)tal_semaphore_post(dev->ready_sem);
        (void)tal_semaphore_post(dev->exit_sem);
        return;
    }

    if (dev->use_surface) {
        SDL_Surface *surf = SDL_GetWindowSurface(dev->window);
        if (surf && surf->format) {
            PR_NOTICE("SDL window surface fmt=0x%x bpp=%u pitch=%d",
                      surf->format->format, surf->format->BitsPerPixel, surf->pitch);
        } else {
            PR_WARN("SDL_GetWindowSurface failed: %s", SDL_GetError());
        }
        dev->renderer = NULL;
        dev->texture = NULL;
    } else {
        dev->renderer = SDL_CreateRenderer(dev->window, -1, SDL_RENDERER_SOFTWARE);
        if (!dev->renderer) {
            PR_WARN("SDL_CreateRenderer (software) failed: %s, try accelerated", SDL_GetError());
            dev->renderer = SDL_CreateRenderer(dev->window, -1, SDL_RENDERER_ACCELERATED);
        }
        if (!dev->renderer) {
            PR_ERR("SDL_CreateRenderer failed: %s", SDL_GetError());
            SDL_DestroyWindow(dev->window);
            dev->window = NULL;
            SDL_Quit();
            dev->inited = false;
            (void)tal_semaphore_post(dev->ready_sem);
            (void)tal_semaphore_post(dev->exit_sem);
            return;
        }

        {
            SDL_RendererInfo info;
            if (SDL_GetRendererInfo(dev->renderer, &info) == 0) {
                PR_NOTICE("SDL renderer: %s flags=0x%x", info.name ? info.name : "(null)", info.flags);
            }
        }

        dev->texture = SDL_CreateTexture(dev->renderer,
                                         SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         dev->width,
                                         dev->height);
        if (!dev->texture) {
            PR_ERR("SDL_CreateTexture failed: %s", SDL_GetError());
            SDL_DestroyRenderer(dev->renderer);
            SDL_DestroyWindow(dev->window);
            dev->texture = NULL;
            dev->renderer = NULL;
            dev->window = NULL;
            SDL_Quit();
            dev->inited = false;
            (void)tal_semaphore_post(dev->ready_sem);
            (void)tal_semaphore_post(dev->exit_sem);
            return;
        }

        SDL_SetTextureBlendMode(dev->texture, SDL_BLENDMODE_NONE);
        SDL_RenderSetLogicalSize(dev->renderer, dev->width, dev->height);
        SDL_SetRenderDrawColor(dev->renderer, 0, 0, 0, 255);
        SDL_RenderClear(dev->renderer);
        SDL_RenderPresent(dev->renderer);
    }

    dev->quit = false;
    dev->inited = true;

    (void)__sdl_input_init(dev->width, dev->height);
    PR_NOTICE("SDL display opened: %ux%u", dev->width, dev->height);
    (void)tal_semaphore_post(dev->ready_sem);

    while (!dev->quit) {
        __sdl_poll_events(dev);
        if (dev->quit) {
            PR_NOTICE("SDL window closed, exiting...");
            exit(0);
        }

        (void)tal_semaphore_wait(dev->frame_sem, 16);

        tal_mutex_lock(dev->frame_mutex);
        if (dev->has_frame) {
            __sdl_present_locked(dev);
            dev->has_frame = false;
        }
        tal_mutex_unlock(dev->frame_mutex);
    }

    if (dev->texture) {
        SDL_DestroyTexture(dev->texture);
        dev->texture = NULL;
    }
    if (dev->renderer) {
        SDL_DestroyRenderer(dev->renderer);
        dev->renderer = NULL;
    }
    if (dev->window) {
        SDL_DestroyWindow(dev->window);
        dev->window = NULL;
    }

    __sdl_input_deinit();
    SDL_Quit();

    (void)tal_semaphore_post(dev->exit_sem);
}

static OPERATE_RET __sdl_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    TDD_DISP_SDL_T *dev = (TDD_DISP_SDL_T *)device;

    if (dev == NULL || frame_buff == NULL || frame_buff->frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!dev->inited) {
        OPERATE_RET rt = __sdl_open(device);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    if (sg_sdl_flush_cnt == 0) {
        PR_NOTICE("SDL first flush: %ux%u fmt=%d len=%u", frame_buff->width, frame_buff->height, frame_buff->fmt, frame_buff->len);
    }
    sg_sdl_flush_cnt++;

    if (frame_buff->fmt != TUYA_PIXEL_FMT_RGB565) {
        PR_WARN("SDL display only supports RGB565 currently, fmt=%d", frame_buff->fmt);
        return OPRT_NOT_SUPPORTED;
    }

    /* Sample a few pixels to see if we're just getting all-black frames. */
    if (sg_sdl_flush_cnt < 5) {
        const uint16_t *p = (const uint16_t *)frame_buff->frame;
        uint32_t total_px = (uint32_t)frame_buff->width * (uint32_t)frame_buff->height;
        uint32_t step = total_px / 256;
        if (step < 1) {
            step = 1;
        }

        uint32_t nonzero = 0;
        uint32_t sampled = 0;
        for (uint32_t i = 0; i < total_px; i += step) {
            sampled++;
            if (p[i] != 0x0000) {
                nonzero++;
            }
        }
        PR_NOTICE("SDL flush sample: nonzero=%u/%u first_px=0x%04x", nonzero, sampled, p[0]);
    }

    tal_mutex_lock(dev->frame_mutex);

    const uint16_t *src = (const uint16_t *)frame_buff->frame;
    uint32_t *dst = dev->conv_argb;
    uint32_t total_px = (uint32_t)frame_buff->width * (uint32_t)frame_buff->height;
    if (dev->conv_argb == NULL || dev->conv_pixels < total_px) {
        tal_mutex_unlock(dev->frame_mutex);
        PR_ERR("conv buffer not ready: have %u need %u", dev->conv_pixels, total_px);
        return OPRT_COM_ERROR;
    }

    for (uint32_t i = 0; i < total_px; i++) {
        uint16_t v = src[i];
        uint32_t r5 = (v >> 11) & 0x1F;
        uint32_t g6 = (v >> 5) & 0x3F;
        uint32_t b5 = (v >> 0) & 0x1F;
        uint32_t r8 = (r5 << 3) | (r5 >> 2);
        uint32_t g8 = (g6 << 2) | (g6 >> 4);
        uint32_t b8 = (b5 << 3) | (b5 >> 2);
        dst[i] = 0xFF000000u | (r8 << 16) | (g8 << 8) | (b8 << 0);
    }

    dev->has_frame = true;
    tal_mutex_unlock(dev->frame_mutex);
    (void)tal_semaphore_post(dev->frame_sem);

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __sdl_close(TDD_DISP_DEV_HANDLE_T device)
{
    TDD_DISP_SDL_T *dev = (TDD_DISP_SDL_T *)device;
    if (dev == NULL) {
        return OPRT_INVALID_PARM;
    }

    dev->quit = true;
    dev->has_frame = false;
    if (dev->frame_sem) {
        (void)tal_semaphore_post(dev->frame_sem);
    }

    if (dev->thread) {
        (void)tal_semaphore_wait(dev->exit_sem, 2000);
        tal_thread_delete(dev->thread);
        dev->thread = NULL;
    }

    if (dev->ready_sem) {
        tal_semaphore_release(dev->ready_sem);
        dev->ready_sem = NULL;
    }
    if (dev->frame_sem) {
        tal_semaphore_release(dev->frame_sem);
        dev->frame_sem = NULL;
    }
    if (dev->exit_sem) {
        tal_semaphore_release(dev->exit_sem);
        dev->exit_sem = NULL;
    }
    if (dev->frame_mutex) {
        tal_mutex_release(dev->frame_mutex);
        dev->frame_mutex = NULL;
    }

    if (dev->conv_argb) {
        tal_free(dev->conv_argb);
        dev->conv_argb = NULL;
        dev->conv_pixels = 0;
    }

    dev->window = NULL;
    dev->renderer = NULL;
    dev->texture = NULL;
    dev->inited = false;
    return OPRT_OK;
}

OPERATE_RET tdd_disp_sdl_register(char *name, uint16_t width, uint16_t height)
{
    if (name == NULL || width == 0 || height == 0) {
        return OPRT_INVALID_PARM;
    }

    TDD_DISP_SDL_T *dev = (TDD_DISP_SDL_T *)tal_malloc(sizeof(TDD_DISP_SDL_T));
    if (dev == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(TDD_DISP_SDL_T));
    dev->width = width;
    dev->height = height;

    TDD_DISP_INTFS_T intfs = {
        .open  = __sdl_open,
        .flush = __sdl_flush,
        .close = __sdl_close,
    };

    TDD_DISP_DEV_INFO_T dev_info = {
        .type     = TUYA_DISPLAY_RGB,
        .width    = width,
        .height   = height,
        .is_swap  = false,
        .has_vram = true,
        .fmt      = TUYA_PIXEL_FMT_RGB565,
        .rotation = TUYA_DISPLAY_ROTATION_0,
        .bl       = { .type = TUYA_DISP_BL_TP_NONE },
        .power    = { .pin = TUYA_GPIO_NUM_MAX, .active_level = TUYA_GPIO_LEVEL_HIGH },
    };

    OPERATE_RET rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        tal_free(dev);
        PR_ERR("tdl_disp_device_register(%s) failed: %d", name, rt);
        return rt;
    }

    PR_NOTICE("SDL display device registered: %s (%ux%u)", name, width, height);
    return OPRT_OK;
}

#if defined(ENABLE_SDL_TP) && (ENABLE_SDL_TP == 1)

typedef struct {
    uint16_t width;
    uint16_t height;

    /* Fake swipe gesture state (generated from wheel). */
    bool swipe_active;
    bool swipe_releasing;
    int32_t swipe_remain_px;

    uint16_t swipe_x;
    uint16_t swipe_y;
} TDD_TP_SDL_T;

static OPERATE_RET __tp_open(TDD_TP_DEV_HANDLE_T device)
{
    (void)device;
    return OPRT_OK;
}

static void __tp_update_wheel_swipe(TDD_TP_SDL_T *dev)
{
    /*
     * SDL wheel.y: +up, -down.
     * For a touch "swipe": finger moves down -> content moves down (scroll up).
     * So map wheel.y directly to finger delta-Y.
     */
    const int32_t wheel_y = __sdl_input_consume_wheel_y();
    if (wheel_y == 0) {
        return;
    }

    uint16_t x = dev->width / 2;
    uint16_t y = dev->height / 2;
    bool pressed = false;
    (void)__sdl_input_get_touch(&x, &y, &pressed);

    dev->swipe_active = true;
    dev->swipe_releasing = false;
    dev->swipe_remain_px += wheel_y * TUYA_SDL_WHEEL_PIXEL_PER_STEP;
    dev->swipe_x = x;
    dev->swipe_y = y;
}

static OPERATE_RET __tp_read(TDD_TP_DEV_HANDLE_T device, uint8_t max_num, TDL_TP_POS_T *point, uint8_t *point_num)
{
    TDD_TP_SDL_T *dev = (TDD_TP_SDL_T *)device;

    if (dev == NULL || point == NULL || point_num == NULL || max_num == 0) {
        return OPRT_INVALID_PARM;
    }

    uint16_t x = dev->width / 2;
    uint16_t y = dev->height / 2;
    bool pressed = false;
    (void)__sdl_input_get_touch(&x, &y, &pressed);

    /* Real touch/mouse drag has priority over wheel swipe. */
    if (pressed) {
        dev->swipe_active = false;
        dev->swipe_releasing = false;
        dev->swipe_remain_px = 0;

        point[0].x = __sdl_clamp_u16(x, 0, dev->width - 1);
        point[0].y = __sdl_clamp_u16(y, 0, dev->height - 1);
        *point_num = 1;
        return OPRT_OK;
    }

    __tp_update_wheel_swipe(dev);

    if (dev->swipe_active) {
        if (dev->swipe_releasing) {
            dev->swipe_active = false;
            dev->swipe_releasing = false;
            dev->swipe_remain_px = 0;
            *point_num = 0;
            return OPRT_OK;
        }

        const int32_t step = __sdl_clamp_i32(dev->swipe_remain_px, -TUYA_SDL_WHEEL_SWIPE_STEP, TUYA_SDL_WHEEL_SWIPE_STEP);
        dev->swipe_remain_px -= step;

        dev->swipe_y = __sdl_clamp_u16((int32_t)dev->swipe_y + step, 0, dev->height - 1);

        point[0].x = dev->swipe_x;
        point[0].y = dev->swipe_y;
        *point_num = 1;

        if (dev->swipe_remain_px == 0) {
            dev->swipe_releasing = true;
        }

        return OPRT_OK;
    }

    /* No press. */
    *point_num = 0;
    return OPRT_OK;
}

static OPERATE_RET __tp_close(TDD_TP_DEV_HANDLE_T device)
{
    (void)device;
    return OPRT_OK;
}

OPERATE_RET tdd_tp_sdl_register(char *name, uint16_t width, uint16_t height)
{
    if (name == NULL || width == 0 || height == 0) {
        return OPRT_INVALID_PARM;
    }

    TDD_TP_SDL_T *dev = (TDD_TP_SDL_T *)tal_malloc(sizeof(TDD_TP_SDL_T));
    if (dev == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    memset(dev, 0, sizeof(TDD_TP_SDL_T));
    dev->width = width;
    dev->height = height;

    TDD_TP_CONFIG_T tp_cfg = {
        .x_max = width,
        .y_max = height,
        .flags = {
            .mirror_x = 0,
            .mirror_y = 0,
            .swap_xy = 0,
        },
    };

    TDD_TP_INTFS_T intfs;
    memset(&intfs, 0, sizeof(intfs));
    intfs.open = __tp_open;
    intfs.read = __tp_read;
    intfs.close = __tp_close;

    OPERATE_RET rt = tdl_tp_device_register(name, (TDD_TP_DEV_HANDLE_T)dev, &tp_cfg, &intfs);
    if (rt != OPRT_OK) {
        tal_free(dev);
        PR_ERR("tdl_tp_device_register(%s) failed: %d", name, rt);
        return rt;
    }

    PR_NOTICE("SDL touchpad device registered: %s (%ux%u)", name, width, height);
    return OPRT_OK;
}

#else

OPERATE_RET tdd_tp_sdl_register(char *name, uint16_t width, uint16_t height)
{
    (void)name;
    (void)width;
    (void)height;
    return OPRT_NOT_SUPPORTED;
}

#endif /* ENABLE_SDL_TP */

#else

OPERATE_RET tdd_disp_sdl_register(char *name, uint16_t width, uint16_t height)
{
    (void)name;
    (void)width;
    (void)height;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tdd_tp_sdl_register(char *name, uint16_t width, uint16_t height)
{
    (void)name;
    (void)width;
    (void)height;
    return OPRT_NOT_SUPPORTED;
}

#endif
