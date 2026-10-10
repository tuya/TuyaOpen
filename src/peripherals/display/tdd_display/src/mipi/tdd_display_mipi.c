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
    TDD_DISP_MIPI_CFG_T   cfg;
    bool                  opened;
} MIPI_DISP_CTRL_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static MIPI_DISP_CTRL_T sg_mipi_disp = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
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
     * stay valid until the next frame-end notification. */
    if (tkl_mipi_dsi_base_addr_set((uint32_t)(uintptr_t)frame_buff->frame) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }

    return tkl_mipi_dsi_display_transfer_start();
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
    /* The controller scans the caller's buffer, so the application holds the
     * frame buffer that is on screen. */
    dev_info.has_vram = true;
    dev_info.fmt = cfg->fmt;
    dev_info.rotation = cfg->rotation;
    dev_info.bl = cfg->bl;
    dev_info.power = cfg->power;

    return tdl_disp_device_register(name, &sg_mipi_disp, &intfs, &dev_info);
}
