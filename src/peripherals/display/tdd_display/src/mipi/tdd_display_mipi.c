/**
 * @file tdd_display_mipi.c
 * @brief MIPI-DSI display TDD driver
 *
 * Sits between TDL (which the application talks to by device name) and the
 * platform TKL MIPI-DSI backend. The panel is initialised and scanned out by the
 * platform side; this layer only forwards the application's frame buffer to it
 * and owns the device lifecycle.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_display_mipi.h"

#include "tkl_mipi_dsi.h"
#include "tal_log.h"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_DISP_MIPI_CFG_T     cfg;
    bool                    opened;
    /* The buffer the controller is scanning, and the one whose transfer will
     * retire it. The frame-end notification runs in interrupt context, so it
     * only moves a pointer here; the release happens in flush(), in task
     * context, where calling the caller's free callback is safe. */
    TDL_DISP_FRAME_BUFF_T  *scanning;
    TDL_DISP_FRAME_BUFF_T  *retiring;
    TDL_DISP_FRAME_BUFF_T  *retired;
} MIPI_DISP_CTRL_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static MIPI_DISP_CTRL_T sg_mipi_disp = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
/* Called from the platform's frame-end path, which is an ISR. It must not
 * release anything - the caller's free callback may take locks or touch the
 * heap. It only records which buffer just left the screen. */
static void __mipi_disp_frame_end_cb(TUYA_MIPI_DSI_EVENT_E event)
{
    if (event != MIPI_DSI_OUTPUT_FINISH) {
        return;
    }
    sg_mipi_disp.retired = sg_mipi_disp.retiring;
    sg_mipi_disp.retiring = NULL;
}

static void __mipi_disp_release(TDL_DISP_FRAME_BUFF_T *fb)
{
    if (fb != NULL && fb->free_cb != NULL) {
        fb->free_cb(fb);
    }
}
static OPERATE_RET __mipi_disp_open(TDD_DISP_DEV_HANDLE_T device)
{
    MIPI_DISP_CTRL_T *ctrl = (MIPI_DISP_CTRL_T *)device;
    TUYA_MIPI_DSI_BASE_CFG_T dsi_cfg = {0};

    if (ctrl == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (ctrl->opened) {
        return OPRT_OK;
    }

    dsi_cfg.width = ctrl->cfg.width;
    dsi_cfg.height = ctrl->cfg.height;
    dsi_cfg.fmt = ctrl->cfg.fmt;
    dsi_cfg.rotation = ctrl->cfg.rotation;
    dsi_cfg.is_swap = ctrl->cfg.is_swap;

    /* The platform backend opens the vendor LCD device, which matches the panel
     * by the name the board profile selected. */
    if (tkl_mipi_dsi_init(&dsi_cfg) != OPRT_OK) {
        PR_ERR("mipi display: tkl_mipi_dsi_init failed");
        return OPRT_COM_ERROR;
    }
    (void)tkl_mipi_dsi_ppi_set(ctrl->cfg.width, ctrl->cfg.height);
    (void)tkl_mipi_dsi_pixel_mode_set(ctrl->cfg.fmt);
    (void)tkl_mipi_dsi_irq_cb_register(__mipi_disp_frame_end_cb);

    ctrl->scanning = NULL;
    ctrl->retiring = NULL;
    ctrl->retired = NULL;
    ctrl->opened = true;
    return OPRT_OK;
}

static OPERATE_RET __mipi_disp_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    MIPI_DISP_CTRL_T *ctrl = (MIPI_DISP_CTRL_T *)device;

    if (ctrl == NULL || frame_buff == NULL || frame_buff->frame == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!ctrl->opened) {
        return OPRT_COM_ERROR;
    }

    /* The panel scans out of the caller's buffer, so the address is handed over
     * directly rather than copied into one the platform owns. The buffer must
     * stay valid until the next frame-end notification.
     *
     * Submitting this frame is what retires the one on screen, so the previous
     * buffer is recorded as retiring first and released once the transfer has
     * actually swapped. Releasing it before the swap would hand back memory the
     * controller is still reading. */
    ctrl->retiring = ctrl->scanning;
    ctrl->scanning = frame_buff;

    if (tkl_mipi_dsi_base_addr_set((uint32_t)(uintptr_t)frame_buff->frame) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }

    OPERATE_RET rt = tkl_mipi_dsi_display_transfer_start();

    /* The notification arrives during the transfer's wait. Releasing here, in
     * task context, keeps the caller's free callback out of the ISR. */
    __mipi_disp_release(ctrl->retired);
    ctrl->retired = NULL;

    return rt;
}

static OPERATE_RET __mipi_disp_close(TDD_DISP_DEV_HANDLE_T device)
{
    MIPI_DISP_CTRL_T *ctrl = (MIPI_DISP_CTRL_T *)device;

    if (ctrl == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!ctrl->opened) {
        return OPRT_OK;
    }

    (void)tkl_mipi_dsi_display_transfer_stop();

    /* Scan-out has stopped, so nothing is being read any more. Release both the
     * frame still on screen and any the callback had already marked, or the last
     * one or two buffers never return to the pool. */
    __mipi_disp_release(ctrl->retired);
    __mipi_disp_release(ctrl->retiring);
    __mipi_disp_release(ctrl->scanning);
    ctrl->retired = NULL;
    ctrl->retiring = NULL;
    ctrl->scanning = NULL;

    ctrl->opened = false;
    return OPRT_OK;
}

OPERATE_RET tdd_disp_mipi_device_register(char *name, TDD_DISP_MIPI_CFG_T *cfg)
{
    TDD_DISP_INTFS_T intfs = {0};
    TDD_DISP_DEV_INFO_T dev_info = {0};

    if (name == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (sg_mipi_disp.opened) {
        return OPRT_COM_ERROR;
    }

    sg_mipi_disp.cfg = *cfg;
    sg_mipi_disp.opened = false;

    intfs.open = __mipi_disp_open;
    intfs.flush = __mipi_disp_flush;
    intfs.close = __mipi_disp_close;

    dev_info.type = TUYA_DISPLAY_MIPI_DSI;
    dev_info.width = cfg->width;
    dev_info.height = cfg->height;
    dev_info.is_swap = cfg->is_swap;
    /* False: the controller scans the caller's buffer directly and keeps no
     * store of its own. The upper layer therefore has to hold a second buffer to
     * draw the next frame into while the submitted one is on screen. Declaring
     * true makes the LVGL full-frame port allocate a single buffer, which leaves
     * nothing to draw into after the first submit - the pool then runs dry and
     * flushes time out. */
    dev_info.has_vram = false;
    dev_info.fmt = cfg->fmt;
    dev_info.rotation = cfg->rotation;
    dev_info.bl = cfg->bl;
    dev_info.power = cfg->power;

    return tdl_disp_device_register(name, &sg_mipi_disp, &intfs, &dev_info);
}
