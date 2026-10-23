/**
 * @file tdd_display_emi_wl82.c
 * @brief JieLi WL82 (AC791N) 8-bit EMI parallel LCD TDD driver implementation
 *
 * Supports auto-detection and driving of ILI9341 (240x320), ILI9488 (320x480),
 * and ILI9481 (320x480) panels via JieLi 8-bit EMI parallel hardware bus.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_display_emi_wl82.h"
#include "tdl_display_driver.h"
#include "tdl_display_manage.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"

/* JieLi SDK headers have macro overlaps with TuyaOpen utilities */
#undef BIT
#undef LIST_HEAD
#undef LIST_HEAD_INIT
#undef INIT_LIST_HEAD
#include "device/device.h"
#include "asm/emi.h"
#include "asm/gpio.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define WL82_LCD_RST_PIN       IO_PORTB_00
#define WL82_LCD_RS_PIN        IO_PORTC_09
#define WL82_LCD_BL_PIN        IO_PORTB_08
#define WL82_LCD_CHECK_D6_PIN  IO_PORTC_07
#define WL82_LCD_CHECK_D7_PIN  IO_PORTC_08

#define REGFLAG_DELAY          0xFF

typedef enum {
    WL82_LCD_ILI9341 = 0,
    WL82_LCD_ILI9481 = 1,
    WL82_LCD_ILI9488 = 2,
} WL82_LCD_TYPE_E;

typedef struct {
    uint8_t cmd;
    uint8_t cnt;
    uint8_t dat[16];
} LCD_INIT_CODE_T;

typedef struct {
    bool                opened;
    void               *emi_hdl;
    WL82_LCD_TYPE_E     lcd_type;
    uint16_t            width;
    uint16_t            height;
    TUYA_GPIO_LEVEL_E   bl_active_level;
} WL82_DISP_CTRL_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static WL82_DISP_CTRL_T sg_wl82_disp = {0};

/* ILI9488 init commands */
static const LCD_INIT_CODE_T c_ili9488_init_seq[] = {
    {0x21, 1, {0x00}},
    {0x13, 1, {0x00}},
    {0x35, 1, {0x00}},
    {0xB1, 2, {0x90, 0x11}},
    {0x36, 1, {0xD8}},
    {0xC1, 1, {0x41}},
    {0xF7, 4, {0xA9, 0x51, 0x2C, 0x82}},
    {0xC1, 1, {0x41}},
    {0xC0, 2, {0x0F, 0x0F}},
    {0xC2, 1, {0x22}},
    {0xC5, 3, {0x00, 0x53, 0x80}},
    {0xB4, 1, {0x02}},
    {0xB7, 1, {0xC6}},
    {0xB6, 2, {0x02, 0x42}},
    {0xBE, 2, {0x00, 0x04}},
    {0xE9, 1, {0x00}},
    {0x3A, 1, {0x55}},
    {0x11, 0, {0x00}},
    {REGFLAG_DELAY, 200, {0x00}},
    {0x29, 0, {0x00}},
};

/* ILI9481 init commands */
static const LCD_INIT_CODE_T c_ili9481_init_seq[] = {
    {0x01, 0, {0x00}},
    {REGFLAG_DELAY, 200, {0x00}},
    {0x13, 1, {0x00}},
    {0x35, 1, {0x00}},
    {0x44, 2, {0x01, 0x50}},
    {0xC5, 1, {0x04}},
    {0xC5, 1, {0x07}},
    {0xE4, 1, {0xA0}},
    {0xD0, 3, {0x05, 0x40, 0x08}},
    {0xD1, 3, {0x00, 0x00, 0x10}},
    {0xD2, 2, {0x01, 0x00}},
    {0xC0, 5, {0x00, 0x3B, 0x00, 0x02, 0x11}},
    {0xC8, 12, {0x00, 0x26, 0x21, 0x00, 0x00, 0x1F, 0x65, 0x23, 0x77, 0x00, 0x0F, 0x00}},
    {0x2A, 4, {0x00, 0x00, 0x01, 0x3F}},
    {0x2B, 4, {0x00, 0x00, 0x01, 0xDF}},
    {0x36, 1, {0x4C}},
    {0x3A, 1, {0x55}},
    {0x11, 0, {0x00}},
    {REGFLAG_DELAY, 200, {0x00}},
    {0x29, 0, {0x00}},
};

/* ILI9341 init commands */
static const LCD_INIT_CODE_T c_ili9341_init_seq[] = {
    {0xCF, 3, {0x00, 0xAA, 0xE0}},
    {0xED, 4, {0x67, 0x03, 0x12, 0x81}},
    {0xE8, 3, {0x85, 0x11, 0x78}},
    {0xCB, 5, {0x39, 0x2C, 0x00, 0x34, 0x02}},
    {0xEA, 2, {0x00, 0x00}},
    {0xC0, 1, {0x21}},
    {0xC1, 1, {0x11}},
    {0xC5, 2, {0x24, 0x3C}},
    {0xC7, 1, {0xB7}},
    {0x36, 1, {0x48}},
    {0x3A, 1, {0x55}},
    {0xB1, 2, {0x00, 0x1F}},
    {0xF2, 1, {0x00}},
    {0x26, 1, {0x01}},
    {0xB7, 1, {0x06}},
    {0xE0, 15, {0x0F, 0x1B, 0x19, 0x0C, 0x0D, 0x07, 0x44, 0xA9, 0x2F, 0x08, 0x0D, 0x03, 0x10, 0x0B, 0x00}},
    {0xE1, 15, {0x00, 0x24, 0x26, 0x03, 0x12, 0x08, 0x3B, 0x57, 0x4F, 0x07, 0x12, 0x0C, 0x2F, 0x34, 0x0F}},
    {0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {0x00}},
    {0x29, 0, {0x00}},
};

/***********************************************************
***********************function define**********************
***********************************************************/
static inline void __emi_write_cmd(void *emi_hdl, uint8_t cmd)
{
    tkl_gpio_write(WL82_LCD_RS_PIN, TUYA_GPIO_LEVEL_LOW);
    dev_write(emi_hdl, &cmd, 1);
}

static inline void __emi_write_data8(void *emi_hdl, uint8_t dat)
{
    tkl_gpio_write(WL82_LCD_RS_PIN, TUYA_GPIO_LEVEL_HIGH);
    dev_write(emi_hdl, &dat, 1);
}

static inline void __emi_write_data_dma(void *emi_hdl, uint8_t *dat, uint32_t len)
{
    tkl_gpio_write(WL82_LCD_RS_PIN, TUYA_GPIO_LEVEL_HIGH);
    dev_write(emi_hdl, dat, len);
}

static void __emi_send_init_seq(void *emi_hdl, const LCD_INIT_CODE_T *code, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (code[i].cmd == REGFLAG_DELAY) {
            tal_system_sleep(code[i].cnt);
        } else {
            __emi_write_cmd(emi_hdl, code[i].cmd);
            for (uint32_t j = 0; j < code[i].cnt; j++) {
                __emi_write_data8(emi_hdl, code[i].dat[j]);
            }
        }
    }
}

static void __emi_set_window(void *emi_hdl, uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye)
{
    __emi_write_cmd(emi_hdl, 0x2A);
    __emi_write_data8(emi_hdl, (uint8_t)(xs >> 8));
    __emi_write_data8(emi_hdl, (uint8_t)(xs & 0xFF));
    __emi_write_data8(emi_hdl, (uint8_t)(xe >> 8));
    __emi_write_data8(emi_hdl, (uint8_t)(xe & 0xFF));

    __emi_write_cmd(emi_hdl, 0x2B);
    __emi_write_data8(emi_hdl, (uint8_t)(ys >> 8));
    __emi_write_data8(emi_hdl, (uint8_t)(ys & 0xFF));
    __emi_write_data8(emi_hdl, (uint8_t)(ye >> 8));
    __emi_write_data8(emi_hdl, (uint8_t)(ye & 0xFF));

    __emi_write_cmd(emi_hdl, 0x2C);
}

static void __hw_reset(void)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg;
    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;

    tkl_gpio_init(WL82_LCD_RST_PIN, &gpio_cfg);
    tkl_gpio_write(WL82_LCD_RST_PIN, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(10);
    tkl_gpio_write(WL82_LCD_RST_PIN, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(20);
    tkl_gpio_write(WL82_LCD_RST_PIN, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(120);
}

static bool __probe_gt911(void)
{
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };
    if (tkl_i2c_init(TUYA_I2C_NUM_0, &i2c_cfg) != OPRT_OK) {
        return false;
    }

    /* Probe GT911 product ID at 0x8140 via 7-bit addr 0x14 (0x28>>1) or 0x5D (0xBA>>1) */
    uint8_t reg_addr[2] = {0x81, 0x40};
    uint8_t id_buf[4] = {0};
    const uint16_t addrs[] = {0x14, 0x5D};

    for (int i = 0; i < 2; i++) {
        if (tkl_i2c_master_send(TUYA_I2C_NUM_0, addrs[i], reg_addr, 2, FALSE) == OPRT_OK) {
            if (tkl_i2c_master_receive(TUYA_I2C_NUM_0, addrs[i], id_buf, 4, FALSE) == OPRT_OK) {
                if (memcmp(id_buf, "911", 3) == 0 || memcmp(id_buf, "914", 3) == 0) {
                    PR_NOTICE("WL82 LCD probe: detected GT911 tp, panel is ILI9341");
                    return true;
                }
            }
        }
    }
    return false;
}

static WL82_LCD_TYPE_E __probe_panel_type(void)
{
    if (__probe_gt911()) {
        return WL82_LCD_ILI9341;
    }

    /* Check PC07 (D6) and PC08 (D7) pull status */
    TUYA_GPIO_BASE_CFG_T in_cfg;
    in_cfg.mode = TUYA_GPIO_FLOATING;
    in_cfg.direct = TUYA_GPIO_INPUT;
    in_cfg.level = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(WL82_LCD_CHECK_D6_PIN, &in_cfg);
    tkl_gpio_init(WL82_LCD_CHECK_D7_PIN, &in_cfg);
    tal_system_sleep(5);

    TUYA_GPIO_LEVEL_E d6 = TUYA_GPIO_LEVEL_LOW, d7 = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_read(WL82_LCD_CHECK_D6_PIN, &d6);
    tkl_gpio_read(WL82_LCD_CHECK_D7_PIN, &d7);

    PR_NOTICE("WL82 LCD check pins: D6=%d, D7=%d", d6, d7);
    if (d6 == TUYA_GPIO_LEVEL_HIGH && d7 == TUYA_GPIO_LEVEL_HIGH) {
        PR_NOTICE("WL82 LCD probe: detected ILI9488 (320x480)");
        return WL82_LCD_ILI9488;
    }

    PR_NOTICE("WL82 LCD probe: detected ILI9481 (320x480)");
    return WL82_LCD_ILI9481;
}

static OPERATE_RET __wl82_disp_open(TDD_DISP_DEV_HANDLE_T device)
{
    WL82_DISP_CTRL_T *ctrl = (WL82_DISP_CTRL_T *)device;
    if (ctrl == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (ctrl->opened) {
        return OPRT_OK;
    }

    /* 1. Init RS pin */
    TUYA_GPIO_BASE_CFG_T gpio_cfg;
    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(WL82_LCD_RS_PIN, &gpio_cfg);

    /* 2. Hardware reset */
    __hw_reset();

    /* 3. Probe panel type */
    ctrl->lcd_type = __probe_panel_type();
    if (ctrl->lcd_type == WL82_LCD_ILI9341) {
        ctrl->width = 240;
        ctrl->height = 320;
        ctrl->bl_active_level = TUYA_GPIO_LEVEL_HIGH;
    } else {
        ctrl->width = 320;
        ctrl->height = 480;
        ctrl->bl_active_level = TUYA_GPIO_LEVEL_LOW;
    }

    /* 4. Open EMI device */
    ctrl->emi_hdl = dev_open("emi", NULL);
    if (ctrl->emi_hdl == NULL) {
        PR_ERR("dev_open emi failed");
        return OPRT_COM_ERROR;
    }

    dev_ioctl(ctrl->emi_hdl, IOCTL_EMI_WRITE_NON_BLOCK, 1);

    /* 5. Send init sequence */
    if (ctrl->lcd_type == WL82_LCD_ILI9341) {
        __emi_send_init_seq(ctrl->emi_hdl, c_ili9341_init_seq, sizeof(c_ili9341_init_seq) / sizeof(c_ili9341_init_seq[0]));
    } else if (ctrl->lcd_type == WL82_LCD_ILI9488) {
        __emi_send_init_seq(ctrl->emi_hdl, c_ili9488_init_seq, sizeof(c_ili9488_init_seq) / sizeof(c_ili9488_init_seq[0]));
    } else {
        __emi_send_init_seq(ctrl->emi_hdl, c_ili9481_init_seq, sizeof(c_ili9481_init_seq) / sizeof(c_ili9481_init_seq[0]));
    }

    /* 6. Turn on backlight */
    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = ctrl->bl_active_level;
    tkl_gpio_init(WL82_LCD_BL_PIN, &gpio_cfg);
    tkl_gpio_write(WL82_LCD_BL_PIN, ctrl->bl_active_level);

    ctrl->opened = true;
    PR_NOTICE("WL82 EMI LCD opened successfully: %dx%d, BL active=%d", ctrl->width, ctrl->height, ctrl->bl_active_level);
    return OPRT_OK;
}

static OPERATE_RET __wl82_disp_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    WL82_DISP_CTRL_T *ctrl = (WL82_DISP_CTRL_T *)device;
    if (ctrl == NULL || frame_buff == NULL || frame_buff->frame == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!ctrl->opened || ctrl->emi_hdl == NULL) {
        return OPRT_COM_ERROR;
    }

    /* Wait for previous non-blocking transfer to finish */
    dev_ioctl(ctrl->emi_hdl, IOCTL_EMI_WRITE_NON_BLOCK_FLUSH, 0);

    uint16_t xs = frame_buff->x_start;
    uint16_t xe = frame_buff->x_start + frame_buff->width - 1;
    uint16_t ys = frame_buff->y_start;
    uint16_t ye = frame_buff->y_start + frame_buff->height - 1;

    __emi_set_window(ctrl->emi_hdl, xs, xe, ys, ye);

    uint32_t len = (uint32_t)frame_buff->width * frame_buff->height * 2;
    __emi_write_data_dma(ctrl->emi_hdl, (uint8_t *)frame_buff->frame, len);

    /* Wait for this transfer to complete */
    dev_ioctl(ctrl->emi_hdl, IOCTL_EMI_WRITE_NON_BLOCK_FLUSH, 0);

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __wl82_disp_close(TDD_DISP_DEV_HANDLE_T device)
{
    WL82_DISP_CTRL_T *ctrl = (WL82_DISP_CTRL_T *)device;
    if (ctrl == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!ctrl->opened) {
        return OPRT_OK;
    }

    /* Turn off backlight */
    TUYA_GPIO_LEVEL_E bl_off = (ctrl->bl_active_level == TUYA_GPIO_LEVEL_HIGH) ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_write(WL82_LCD_BL_PIN, bl_off);

    if (ctrl->emi_hdl) {
        dev_ioctl(ctrl->emi_hdl, IOCTL_EMI_WRITE_NON_BLOCK_FLUSH, 0);
        dev_close(ctrl->emi_hdl);
        ctrl->emi_hdl = NULL;
    }
    ctrl->opened = false;
    return OPRT_OK;
}

OPERATE_RET tdd_disp_emi_wl82_register(char *name)
{
    if (name == NULL) {
        return OPRT_INVALID_PARM;
    }

    static TDD_DISP_INTFS_T intfs = {
        .open = __wl82_disp_open,
        .close = __wl82_disp_close,
        .flush = __wl82_disp_flush,
    };

    /* Pre-probe panel type to configure default geometry for TDL registration */
    __hw_reset();
    WL82_LCD_TYPE_E type = __probe_panel_type();
    sg_wl82_disp.lcd_type = type;
    if (type == WL82_LCD_ILI9341) {
        sg_wl82_disp.width = 240;
        sg_wl82_disp.height = 320;
        sg_wl82_disp.bl_active_level = TUYA_GPIO_LEVEL_HIGH;
    } else {
        sg_wl82_disp.width = 320;
        sg_wl82_disp.height = 480;
        sg_wl82_disp.bl_active_level = TUYA_GPIO_LEVEL_LOW;
    }

    TDD_DISP_DEV_INFO_T dev_info = {
        .type     = TUYA_DISPLAY_8080,
        .width    = sg_wl82_disp.width,
        .height   = sg_wl82_disp.height,
        .fmt      = TUYA_PIXEL_FMT_RGB565,
        .rotation = TUYA_DISPLAY_ROTATION_0,
        .is_swap  = true,
        .has_vram = true,
        .bl       = {
            .type = TUYA_DISP_BL_TP_GPIO,
            .gpio = {
                .pin = WL82_LCD_BL_PIN,
                .active_level = sg_wl82_disp.bl_active_level,
            },
        },
        .power    = {
            .pin = TUYA_GPIO_NUM_MAX,
            .active_level = TUYA_GPIO_LEVEL_LOW,
        },
    };

    return tdl_disp_device_register(name, &sg_wl82_disp, &intfs, &dev_info);
}
