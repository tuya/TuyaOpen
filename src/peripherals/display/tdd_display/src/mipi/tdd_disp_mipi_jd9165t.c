/**
 * @file tdd_disp_mipi_jd9165t.c
 * @brief JD9165T MIPI-DSI panel driver (1024x600, 4 lanes, 60 Hz).
 *
 * Panel data only: timing, default reset delays and the DCS init sequence.
 * Board wiring arrives through TDD_DISP_MIPI_BOARD_IO_T; link bring-up and
 * frame submission stay in the shared MIPI bus driver.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"

#include "tdd_disp_jd9165t.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define JD9165T_WIDTH          1024U
#define JD9165T_HEIGHT         600U
#define JD9165T_LANES          4U
#define JD9165T_FPS            60U

#define JD9165T_HSYNC_PULSE    2U
#define JD9165T_HSYNC_BACK     100U
#define JD9165T_HSYNC_FRONT    100U
#define JD9165T_VSYNC_PULSE    2U
#define JD9165T_VSYNC_BACK     12U
#define JD9165T_VSYNC_FRONT    95U

/***********************************************************
***********************variable define**********************
***********************************************************/
/* Entries with cmd == 0 are pure delays; every other entry is a DCS write. */
static const TDD_DISP_MIPI_INIT_CMD_T cJD9165T_INIT_CMDS[] = {
    {0xDF, 0, (const uint8_t []){0x90, 0x85, 0x15}, 3},
    {0x00, 1, NULL, 0},
    {0xDE, 0, (const uint8_t []){0x00}, 1},
    {0x00, 1, NULL, 0},
    {0xB6, 0, (const uint8_t []){0x11, 0x4F, 0x00, 0x5F, 0x0D, 0x16, 0xC0, 0xEE, 0x00, 0x5F, 0x0D, 0x8F}, 12},
    {0x00, 1, NULL, 0},
    {0xB7, 0, (const uint8_t []){0x00, 0x9C, 0x9C}, 3},
    {0x00, 1, NULL, 0},
    {0xBB, 0, (const uint8_t []){0x06, 0x54, 0x2C}, 3},
    {0x00, 1, NULL, 0},
    {0xC1, 0, (const uint8_t []){0x40, 0x01, 0x00}, 3},
    {0x00, 1, NULL, 0},
    {0xC3, 0, (const uint8_t []){0x0C, 0x01, 0x00, 0x0C, 0x18, 0xFF, 0x78, 0x12, 0xE8}, 9},
    {0x00, 1, NULL, 0},
    {0xC4, 0, (const uint8_t []){0x13, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x22}, 11},
    {0x00, 1, NULL, 0},
    {0xC8, 0, (const uint8_t []){0x6E, 0x60, 0x57, 0x46, 0x39, 0x36, 0x27, 0x2B, 0x15, 0x2E, 0x2C, 0x2C,
                                 0x49, 0x38, 0x42, 0x36, 0x36, 0x2F, 0x2A, 0x28, 0x27,
                                 0x6E, 0x60, 0x57, 0x46, 0x39, 0x36, 0x27, 0x2B, 0x15, 0x2E, 0x2C, 0x2C,
                                 0x49, 0x38, 0x42, 0x36, 0x36, 0x2F, 0x2A, 0x28, 0x27}, 42},
    {0x00, 1, NULL, 0},
    {0xCB, 0, (const uint8_t []){0x71, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x30}, 11},
    {0x00, 1, NULL, 0},
    {0xCE, 0, (const uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0xFF, 0xFF, 0xFF, 0xFC, 0xFF, 0xFF, 0xFF, 0xFC, 0x02}, 18},
    {0x00, 1, NULL, 0},
    {0xCF, 0, (const uint8_t []){0x00, 0x02, 0x00, 0x02, 0x02, 0x02, 0x7C, 0x03, 0x7C,
                                 0x00, 0x00, 0x05, 0x7C, 0x05, 0x7C}, 15},
    {0x00, 1, NULL, 0},
    {0xD0, 0, (const uint8_t []){0x00, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xD2, 0xD0, 0xDF,
                                 0xDF, 0xDD, 0xC2, 0xC0, 0xDB, 0xD9, 0xCE, 0xCC, 0xCA, 0xC8, 0xC6, 0xC4}, 23},
    {0x00, 1, NULL, 0},
    {0xD1, 0, (const uint8_t []){0x00, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xD3, 0xD1, 0xDF,
                                 0xDF, 0xDD, 0xC3, 0xC1, 0xDC, 0xDA, 0xCF, 0xCD, 0xCB, 0xC9, 0xC7, 0xC5}, 23},
    {0x00, 1, NULL, 0},
    {0xD2, 0, (const uint8_t []){0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x13, 0x11, 0x1F,
                                 0x1D, 0x1F, 0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x1A, 0x1C}, 23},
    {0x00, 1, NULL, 0},
    {0xD3, 0, (const uint8_t []){0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x12, 0x10, 0x1F,
                                 0x1D, 0x1F, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x19, 0x1B}, 23},
    {0x00, 1, NULL, 0},
    {0xD4, 0, (const uint8_t []){0x30, 0x00, 0x00, 0x0A, 0x00, 0x0C, 0x00, 0x04, 0xBA, 0x04, 0xBA,
                                 0x08, 0x08, 0x00, 0x00, 0x80, 0x01, 0xC0, 0x01, 0x00, 0x04, 0xBB,
                                 0x04, 0xBB, 0x00, 0x00, 0x08, 0x08, 0x11, 0x84, 0xCC, 0xC0, 0x05,
                                 0x03, 0x01, 0x00, 0x12, 0x04, 0x0A, 0x04, 0xCA, 0x00, 0x0F}, 43},
    {0x00, 1, NULL, 0},
    {0xD5, 0, (const uint8_t []){0x02, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7, 0x00,
                                 0x0E, 0x10, 0xF7, 0x07, 0x04, 0x18, 0x1E, 0x3C, 0x00, 0x00, 0x01,
                                 0x2F, 0x0F}, 24},
    {0x00, 1, NULL, 0},
    {0xD6, 0, (const uint8_t []){0x04, 0x02, 0x04, 0x02, 0x04, 0x04, 0x04, 0x04, 0x04, 0x02, 0x04, 0x04}, 12},
    {0x00, 1, NULL, 0},
    {0xDE, 0, (const uint8_t []){0x02}, 1},
    {0x00, 1, NULL, 0},
    {0xC5, 0, (const uint8_t []){0x4A, 0x00}, 2},
    {0x00, 1, NULL, 0},
    {0xD0, 0, (const uint8_t []){0x00, 0x00, 0x11, 0x00, 0x11, 0x26, 0x4A, 0xEC, 0x10, 0x59, 0x8D, 0x26,
                                 0x4A, 0xEC, 0x10, 0x59, 0x8D, 0x00, 0x66, 0xDD, 0x66, 0xDD, 0xEA, 0x86,
                                 0x20, 0xDC, 0x95, 0x41, 0xEA, 0x86, 0x20, 0xDC, 0x95, 0x41}, 34},
    {0x00, 1, NULL, 0},
    {0xD6, 0, (const uint8_t []){0x01, 0x01, 0x00, 0x10, 0x00, 0x48, 0x00, 0x01}, 8},
    {0x00, 1, NULL, 0},
    {0xDE, 0, (const uint8_t []){0x04}, 1},
    {0x00, 1, NULL, 0},
    {0xB0, 0, (const uint8_t []){0x31, 0x01}, 2},
    {0x00, 1, NULL, 0},
    {0xDE, 0, (const uint8_t []){0x00}, 1},
    {0x00, 1, NULL, 0},
    {0x11, 120, (const uint8_t []){0x00}, 1}, /* sleep out, then wait */
    {0x29, 0,   (const uint8_t []){0x00}, 1}, /* display on */
};

static const TDD_DISP_MIPI_PANEL_T cJD9165T_PANEL = {
    .width             = JD9165T_WIDTH,
    .height            = JD9165T_HEIGHT,
    .lanes             = JD9165T_LANES,
    .fps               = JD9165T_FPS,
    .pixel_fmt         = TUYA_PIXEL_FMT_RGB565,
    .hsync_pulse_width = JD9165T_HSYNC_PULSE,
    .hsync_back_porch  = JD9165T_HSYNC_BACK,
    .hsync_front_porch = JD9165T_HSYNC_FRONT,
    .vsync_pulse_width = JD9165T_VSYNC_PULSE,
    .vsync_back_porch  = JD9165T_VSYNC_BACK,
    .vsync_front_porch = JD9165T_VSYNC_FRONT,
    .reset_delay_ms    = {10, 50, 150},
    .init_cmds         = cJD9165T_INIT_CMDS,
    .init_cmds_num     = sizeof(cJD9165T_INIT_CMDS) / sizeof(cJD9165T_INIT_CMDS[0]),
    .has_vram          = false,
};

static const TDD_DISP_MIPI_INIT_CMD_T *sg_init_cmds;
static uint16_t                        sg_init_cmds_num;

/***********************************************************
***********************function define**********************
***********************************************************/
static bool __tdd_disp_mipi_jd9165t_fmt_to_tkl(TUYA_DISPLAY_PIXEL_FMT_E fmt, DISPLAY_PIXEL_FORMAT_E *out)
{
    switch (fmt) {
    case TUYA_PIXEL_FMT_RGB565:
        *out = TY_PIXEL_FMT_RGB565;
        return true;
    case TUYA_PIXEL_FMT_RGB888:
        *out = TY_PIXEL_FMT_RGB888;
        return true;
    default:
        return false;
    }
}

OPERATE_RET tdd_disp_mipi_jd9165t_set_init_cmds(const TDD_DISP_MIPI_INIT_CMD_T *cmds, uint16_t num)
{
    if (cmds == NULL || num == 0) {
        return OPRT_INVALID_PARM;
    }

    sg_init_cmds     = cmds;
    sg_init_cmds_num = num;

    return OPRT_OK;
}

OPERATE_RET tdd_disp_mipi_jd9165t_register(char *name, const TDD_DISP_MIPI_BOARD_IO_T *io)
{
    TDD_DISP_MIPI_CFG_T cfg;
    const TDD_DISP_MIPI_PANEL_T *panel = &cJD9165T_PANEL;
    uint32_t h_total;
    uint32_t v_total;
    uint32_t i;

    if (name == NULL || io == NULL) {
        return OPRT_INVALID_PARM;
    }

    memset(&cfg, 0, sizeof(cfg));

    h_total    = panel->width + panel->hsync_pulse_width + panel->hsync_back_porch + panel->hsync_front_porch;
    v_total    = panel->height + panel->vsync_pulse_width + panel->vsync_back_porch + panel->vsync_front_porch;
    cfg.cfg.clk = h_total * v_total * panel->fps;

    cfg.cfg.n_lanes           = panel->lanes;
    cfg.cfg.width             = panel->width;
    cfg.cfg.height            = panel->height;
    cfg.cfg.hsync_pulse_width = panel->hsync_pulse_width;
    cfg.cfg.vsync_pulse_width = panel->vsync_pulse_width;
    cfg.cfg.hsync_back_porch  = panel->hsync_back_porch;
    cfg.cfg.hsync_front_porch = panel->hsync_front_porch;
    cfg.cfg.vsync_back_porch  = panel->vsync_back_porch;
    cfg.cfg.vsync_front_porch = panel->vsync_front_porch;

    if (false == __tdd_disp_mipi_jd9165t_fmt_to_tkl(panel->pixel_fmt, &cfg.cfg.pixel_fmt)) {
        return OPRT_NOT_SUPPORTED;
    }

    cfg.reset               = io->reset;
    cfg.power               = io->power;
    cfg.bl                  = io->bl;
    cfg.rotation            = io->rotation;
    cfg.is_swap             = io->is_swap;
    cfg.input_decompress    = io->input_decompress;
    cfg.has_vram            = panel->has_vram;
    cfg.gpio_drive_cap      = io->gpio_drive_cap;
    cfg.max_inflight        = io->max_inflight;

    cfg.init_cmds     = (sg_init_cmds != NULL) ? sg_init_cmds : panel->init_cmds;
    cfg.init_cmds_num = (sg_init_cmds != NULL) ? sg_init_cmds_num : panel->init_cmds_num;

    for (i = 0; i < 3; i++) {
        cfg.reset_delay_ms[i] = (io->reset_delay_ms[i] != 0) ? io->reset_delay_ms[i] : panel->reset_delay_ms[i];
    }

    return tdd_disp_mipi_device_register(name, &cfg);
}
