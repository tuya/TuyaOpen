/**
 * @file tdd_display_mipi.c
 * @brief TDD display MIPI-DSI interface implementation
 *
 * Frames handed over by the display manager are submitted to the controller
 * as-is and released through their own free_cb once the controller reports the
 * scanout done, so no framebuffer is allocated or copied here. The callback
 * runs in the controller interrupt: the in-flight table is protected with a
 * critical section, never the device mutex.
 *
 * open/flush/close are serialised with a mutex because the display manager
 * calls them from different tasks.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"

#include "tkl_gpio.h"
#include "tkl_mipi_dsi.h"

#include "tdd_display_mipi.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DISP_MIPI_FB_SLOTS_DEFAULT 4
#define DISP_MIPI_FB_SLOTS_MAX     4 /* matches TKL_MIPI_DSI_FLUSH_SLOTS */

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDL_DISP_FRAME_BUFF_T *owner;
    uint8_t               *buffer;
} TDD_DISP_MIPI_SLOT_T;

typedef struct {
    bool                 registered;
    bool                 opened;
    MUTEX_HANDLE         lock;
    TDD_DISP_MIPI_CFG_T  cfg;
    TUYA_DISPLAY_PIXEL_FMT_E fmt;
    uint8_t              slot_num;
    TDD_DISP_MIPI_SLOT_T slots[DISP_MIPI_FB_SLOTS_MAX];
    uint32_t             frame_drop;
} TDD_DISP_MIPI_INFO_T;

typedef enum {
    DISP_MIPI_SLOT_OK = 0,
    DISP_MIPI_SLOT_FULL,
    DISP_MIPI_SLOT_BUSY,
    DISP_MIPI_SLOT_ALIAS,
} DISP_MIPI_SLOT_RET_E;

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_DISP_MIPI_INFO_T sg_display_mipi;

/***********************************************************
***********************function define**********************
***********************************************************/
static TUYA_GPIO_LEVEL_E __tdd_display_mipi_idle_level(const TUYA_DISPLAY_IO_CTRL_T *io)
{
    return (io->active_level == TUYA_GPIO_LEVEL_HIGH) ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH;
}

static OPERATE_RET __tdd_display_mipi_io_init(const TUYA_DISPLAY_IO_CTRL_T *io, TUYA_GPIO_DRIVE_CAP_E drive_cap)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    if (io->pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_OK;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.mode      = TUYA_GPIO_PUSH_PULL;
    cfg.direct    = TUYA_GPIO_OUTPUT;
    cfg.level     = __tdd_display_mipi_idle_level(io);
    cfg.drive_cap = drive_cap;

    return tkl_gpio_init(io->pin, &cfg);
}

static void __tdd_display_mipi_io_write(const TUYA_DISPLAY_IO_CTRL_T *io, bool active)
{
    TUYA_GPIO_LEVEL_E level;
    OPERATE_RET       rt;

    if (io->pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    level = active ? io->active_level : __tdd_display_mipi_idle_level(io);
    rt    = tkl_gpio_write(io->pin, level);
    if (rt != OPRT_OK) {
        PR_ERR("mipi gpio %d write failed: %d", io->pin, rt);
    }
}

static void __tdd_display_mipi_io_deinit(const TUYA_DISPLAY_IO_CTRL_T *io)
{
    if (io->pin < TUYA_GPIO_NUM_MAX) {
        (void)tkl_gpio_deinit(io->pin);
    }
}

static DISP_MIPI_SLOT_RET_E __tdd_display_mipi_frame_add(TDL_DISP_FRAME_BUFF_T *frame)
{
    DISP_MIPI_SLOT_RET_E ret = DISP_MIPI_SLOT_FULL;
    uint32_t             irq_mask;

    irq_mask = tal_system_enter_critical();
    for (uint8_t i = 0; i < sg_display_mipi.slot_num; i++) {
        if (sg_display_mipi.slots[i].owner == frame) {
            ret = DISP_MIPI_SLOT_BUSY;
            goto exit;
        }
        if (sg_display_mipi.slots[i].owner != NULL && sg_display_mipi.slots[i].buffer == frame->frame) {
            ret = DISP_MIPI_SLOT_ALIAS;
            goto exit;
        }
    }
    for (uint8_t i = 0; i < sg_display_mipi.slot_num; i++) {
        if (sg_display_mipi.slots[i].owner == NULL) {
            sg_display_mipi.slots[i].owner  = frame;
            sg_display_mipi.slots[i].buffer = frame->frame;
            ret                             = DISP_MIPI_SLOT_OK;
            break;
        }
    }

exit:
    tal_system_exit_critical(irq_mask);

    return ret;
}

static TDL_DISP_FRAME_BUFF_T *__tdd_display_mipi_frame_take(uint8_t *buffer)
{
    TDL_DISP_FRAME_BUFF_T *frame = NULL;
    uint32_t               irq_mask;

    irq_mask = tal_system_enter_critical();
    for (uint8_t i = 0; i < sg_display_mipi.slot_num; i++) {
        if (sg_display_mipi.slots[i].owner != NULL && sg_display_mipi.slots[i].buffer == buffer) {
            frame                           = sg_display_mipi.slots[i].owner;
            sg_display_mipi.slots[i].owner  = NULL;
            sg_display_mipi.slots[i].buffer = NULL;
            break;
        }
    }
    tal_system_exit_critical(irq_mask);

    return frame;
}

/* Controller is done with this buffer: give it back through its own free_cb. */
static void __tdd_display_mipi_frame_release(void *buffer)
{
    TDL_DISP_FRAME_BUFF_T *frame = __tdd_display_mipi_frame_take((uint8_t *)buffer);

    if (frame != NULL && frame->free_cb != NULL) {
        frame->free_cb(frame);
    }
}

static void __tdd_display_mipi_frames_drain(void)
{
    TDL_DISP_FRAME_BUFF_T *frames[DISP_MIPI_FB_SLOTS_MAX];
    uint8_t                num = 0;
    uint32_t               irq_mask;

    irq_mask = tal_system_enter_critical();
    for (uint8_t i = 0; i < sg_display_mipi.slot_num; i++) {
        if (sg_display_mipi.slots[i].owner != NULL) {
            frames[num++]                   = sg_display_mipi.slots[i].owner;
            sg_display_mipi.slots[i].owner  = NULL;
            sg_display_mipi.slots[i].buffer = NULL;
        }
    }
    tal_system_exit_critical(irq_mask);

    for (uint8_t i = 0; i < num; i++) {
        if (frames[i]->free_cb != NULL) {
            frames[i]->free_cb(frames[i]);
        }
    }
}

static void __tdd_display_mipi_reset(const TDD_DISP_MIPI_CFG_T *cfg)
{
    if (cfg->reset.pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    __tdd_display_mipi_io_write(&cfg->reset, false);
    tal_system_sleep(cfg->reset_delay_ms[0]);

    __tdd_display_mipi_io_write(&cfg->reset, true);
    tal_system_sleep(cfg->reset_delay_ms[1]);

    __tdd_display_mipi_io_write(&cfg->reset, false);
    tal_system_sleep(cfg->reset_delay_ms[2]);
}

static OPERATE_RET __tdd_display_mipi_send_init_cmds(const TDD_DISP_MIPI_CFG_T *cfg)
{
    OPERATE_RET rt;

    for (uint16_t i = 0; i < cfg->init_cmds_num; i++) {
        const TDD_DISP_MIPI_INIT_CMD_T *cmd = &cfg->init_cmds[i];

        if (cmd->cmd != 0U) {
            rt = tkl_mipi_dsi_tx_cmd(cmd->cmd, cmd->data, cmd->len);
            if (rt != OPRT_OK) {
                PR_ERR("panel init cmd 0x%02x failed: %d", cmd->cmd, rt);
                return rt;
            }
        }

        if (cmd->delay_ms != 0U) {
            tal_system_sleep(cmd->delay_ms);
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_display_mipi_open(TDD_DISP_DEV_HANDLE_T dev)
{
    TDD_DISP_MIPI_INFO_T *info = (TDD_DISP_MIPI_INFO_T *)dev;
    OPERATE_RET           rt;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(info->lock);

    if (info->opened) {
        tal_mutex_unlock(info->lock);
        return OPRT_OK;
    }

    /* Reset is idle before the panel rail comes up, so power-on is a clean edge. */
    rt = __tdd_display_mipi_io_init(&info->cfg.reset, info->cfg.gpio_drive_cap);
    if (rt != OPRT_OK) {
        goto err;
    }

    rt = __tdd_display_mipi_io_init(&info->cfg.power, info->cfg.gpio_drive_cap);
    if (rt != OPRT_OK) {
        goto err;
    }
    __tdd_display_mipi_io_write(&info->cfg.power, true);

    rt = tkl_mipi_dsi_set_input_mode(info->cfg.input_decompress ? TRUE : FALSE);
    if (rt != OPRT_OK) {
        PR_ERR("set input mode failed: %d", rt);
        goto err;
    }

    rt = tkl_mipi_dsi_init(&info->cfg.cfg);
    if (rt != OPRT_OK) {
        PR_ERR("dsi init failed: %d", rt);
        goto err;
    }

    __tdd_display_mipi_reset(&info->cfg);

    rt = __tdd_display_mipi_send_init_cmds(&info->cfg);
    if (rt != OPRT_OK) {
        (void)tkl_mipi_dsi_deinit();
        goto err;
    }

    info->opened = true;
    tal_mutex_unlock(info->lock);

    PR_NOTICE("mipi panel up: %ux%u, %u lanes", (unsigned int)info->cfg.cfg.width,
              (unsigned int)info->cfg.cfg.height, (unsigned int)info->cfg.cfg.n_lanes);

    return OPRT_OK;

err:
    __tdd_display_mipi_io_write(&info->cfg.power, false);
    tal_mutex_unlock(info->lock);
    return rt;
}

static bool __tdd_display_mipi_frame_valid(const TDD_DISP_MIPI_INFO_T *info, const TDL_DISP_FRAME_BUFF_T *frame)
{
    uint8_t  bpp;
    uint32_t need;

    switch (info->fmt) {
    case TUYA_PIXEL_FMT_RGB565:
        bpp = 2;
        break;
    case TUYA_PIXEL_FMT_RGB888:
        bpp = 3;
        break;
    default:
        return false;
    }

    if (frame->fmt != info->fmt || frame->width != info->cfg.cfg.width || frame->height != info->cfg.cfg.height) {
        return false;
    }

    need = (uint32_t)frame->width * frame->height * bpp;

    return frame->len >= need;
}

static OPERATE_RET __tdd_display_mipi_flush(TDD_DISP_DEV_HANDLE_T dev, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    TDD_DISP_MIPI_INFO_T      *info  = (TDD_DISP_MIPI_INFO_T *)dev;
    tkl_mipi_dsi_flush_frame_t frame = {0};
    TDL_DISP_FRAME_BUFF_T     *pending;
    uint8_t                   *buffer;
    bool                       owned = false;
    OPERATE_RET                rt;
    DISP_MIPI_SLOT_RET_E       slot_rt;

    if (info == NULL || frame_buff == NULL || frame_buff->frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(info->lock);

    if (false == info->opened) {
        rt = OPRT_RESOURCE_NOT_READY;
        goto exit_unlock;
    }

    buffer = frame_buff->frame;

    if (false == __tdd_display_mipi_frame_valid(info, frame_buff)) {
        PR_ERR("frame %ux%u fmt %d does not match the panel", frame_buff->width, frame_buff->height,
               frame_buff->fmt);
        rt = OPRT_NOT_SUPPORTED;
        goto exit_release;
    }

    if (frame_buff->free_cb != NULL) {
        slot_rt = __tdd_display_mipi_frame_add(frame_buff);
        if (slot_rt != DISP_MIPI_SLOT_OK) {
            sg_display_mipi.frame_drop++;
            PR_ERR("mipi slot rejected (%d), %u frames dropped", slot_rt, (unsigned int)sg_display_mipi.frame_drop);
            rt = OPRT_COM_ERROR;
            goto exit_release;
        }
        owned = true;
    }

    frame.buffer  = buffer;
    frame.free_cb = owned ? __tdd_display_mipi_frame_release : NULL;

    rt = tkl_mipi_dsi_flush(&frame);
    if (rt != OPRT_OK) {
        PR_ERR("dsi flush failed: %d", rt);
        /* Do not interpret the error code: the table says whether the frame is
         * still ours. If TKL released it, the entry is already gone. */
        pending = __tdd_display_mipi_frame_take(buffer);
        if (pending != NULL && pending->free_cb != NULL) {
            pending->free_cb(pending);
        }
    }

    tal_mutex_unlock(info->lock);
    return rt;

exit_release:
    if (frame_buff->free_cb != NULL) {
        frame_buff->free_cb(frame_buff);
    }
exit_unlock:
    tal_mutex_unlock(info->lock);
    return rt;
}

static OPERATE_RET __tdd_display_mipi_close(TDD_DISP_DEV_HANDLE_T dev)
{
    TDD_DISP_MIPI_INFO_T *info = (TDD_DISP_MIPI_INFO_T *)dev;
    OPERATE_RET           rt;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(info->lock);

    if (false == info->opened) {
        tal_mutex_unlock(info->lock);
        return OPRT_OK;
    }

    info->opened = false;

    rt = tkl_mipi_dsi_deinit();
    if (rt != OPRT_OK) {
        PR_ERR("dsi deinit failed: %d", rt);
    }

    __tdd_display_mipi_frames_drain();

    __tdd_display_mipi_io_write(&info->cfg.power, false);
    __tdd_display_mipi_io_deinit(&info->cfg.power);
    __tdd_display_mipi_io_deinit(&info->cfg.reset);

    tal_mutex_unlock(info->lock);

    return OPRT_OK;
}

static OPERATE_RET __tdd_display_mipi_fmt_to_tdl(DISPLAY_PIXEL_FORMAT_E fmt, TUYA_DISPLAY_PIXEL_FMT_E *out)
{
    switch (fmt) {
    case TY_PIXEL_FMT_RGB565:
        *out = TUYA_PIXEL_FMT_RGB565;
        return OPRT_OK;
    case TY_PIXEL_FMT_RGB888:
        *out = TUYA_PIXEL_FMT_RGB888;
        return OPRT_OK;
    default:
        return OPRT_NOT_SUPPORTED;
    }
}

static OPERATE_RET __tdd_display_mipi_check_cfg(const TDD_DISP_MIPI_CFG_T *mipi)
{
    if (mipi->cfg.width == 0 || mipi->cfg.height == 0 || mipi->cfg.clk == 0) {
        return OPRT_INVALID_PARM;
    }

    if (mipi->cfg.n_lanes < 1 || mipi->cfg.n_lanes > 4) {
        return OPRT_INVALID_PARM;
    }

    if (mipi->init_cmds_num > 0 && mipi->init_cmds == NULL) {
        return OPRT_INVALID_PARM;
    }

    return OPRT_OK;
}

/**
 * @brief Registers a MIPI-DSI display device with the display management system.
 */
OPERATE_RET tdd_disp_mipi_device_register(char *name, const TDD_DISP_MIPI_CFG_T *mipi)
{
    TDD_DISP_DEV_INFO_T dev_info;
    TDD_DISP_INTFS_T    intfs;
    TUYA_DISPLAY_PIXEL_FMT_E fmt;
    OPERATE_RET         rt;

    if (name == NULL || mipi == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (sg_display_mipi.registered) {
        PR_ERR("mipi display already registered");
        return OPRT_COM_ERROR;
    }

    rt = __tdd_display_mipi_check_cfg(mipi);
    if (rt != OPRT_OK) {
        PR_ERR("invalid mipi cfg");
        return rt;
    }

    rt = __tdd_display_mipi_fmt_to_tdl(mipi->cfg.pixel_fmt, &fmt);
    if (rt != OPRT_OK) {
        PR_ERR("unsupported pixel format: %d", mipi->cfg.pixel_fmt);
        return rt;
    }

    memset(&dev_info, 0, sizeof(dev_info));
    dev_info.type     = TUYA_DISPLAY_MIPI_DSI;
    dev_info.width    = mipi->cfg.width;
    dev_info.height   = mipi->cfg.height;
    dev_info.rotation = mipi->rotation;
    dev_info.is_swap  = mipi->is_swap;
    dev_info.has_vram = mipi->has_vram;
    dev_info.fmt      = fmt;
    dev_info.bl       = mipi->bl;
    dev_info.power    = mipi->power;

    memset(&intfs, 0, sizeof(intfs));
    intfs.open  = __tdd_display_mipi_open;
    intfs.flush = __tdd_display_mipi_flush;
    intfs.close = __tdd_display_mipi_close;

    memset(&sg_display_mipi, 0, sizeof(sg_display_mipi));
    sg_display_mipi.cfg  = *mipi;
    sg_display_mipi.fmt  = fmt;
    sg_display_mipi.slot_num = (0 != mipi->max_inflight) ? mipi->max_inflight : DISP_MIPI_FB_SLOTS_DEFAULT;
    if (sg_display_mipi.slot_num > DISP_MIPI_FB_SLOTS_MAX) {
        sg_display_mipi.slot_num = DISP_MIPI_FB_SLOTS_MAX;
    }

    rt = tal_mutex_create_init(&sg_display_mipi.lock);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)&sg_display_mipi, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        (void)tal_mutex_release(sg_display_mipi.lock);
        sg_display_mipi.lock = NULL;
        return rt;
    }

    sg_display_mipi.registered = true;

    return OPRT_OK;
}
