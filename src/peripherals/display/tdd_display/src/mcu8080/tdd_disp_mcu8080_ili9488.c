/**
 * @file tdd_disp_mcu8080_ili9488.c
 * @brief ILI9488 / ILI9341 LCD driver implementation via MCU 8080 parallel interface
 *
 * Supports auto-detection and driving of ILI9488 (320x480), ILI9341 (240x320),
 * and ILI9481 (320x480) panels via 8-bit MCU 8080 parallel hardware bus.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_ili9488.h"
#include "tdl_display_driver.h"
#include "tdl_display_manage.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"

/* Platform device headers */
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
#define MCU8080_LCD_RST_PIN       IO_PORTB_00
#define MCU8080_LCD_RS_PIN        IO_PORTC_09
#define MCU8080_LCD_BL_PIN        IO_PORTB_08
#define MCU8080_LCD_CHECK_D6_PIN  IO_PORTC_07
#define MCU8080_LCD_CHECK_D7_PIN  IO_PORTC_08

#define REGFLAG_DELAY             0xFF

typedef enum {
    MCU8080_LCD_ILI9341 = 0,
    MCU8080_LCD_ILI9481 = 1,
    MCU8080_LCD_ILI9488 = 2,
} MCU8080_LCD_TYPE_E;

typedef struct {
    uint8_t cmd;
    uint8_t cnt;
    uint8_t dat[16];
} LCD_INIT_CODE_T;

typedef struct {
    bool                opened;
    void               *bus_hdl;
    MCU8080_LCD_TYPE_E  lcd_type;
    uint16_t            width;
    uint16_t            height;
    TUYA_GPIO_LEVEL_E   bl_active_level;
} MCU8080_DISP_CTRL_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static MCU8080_DISP_CTRL_T sg_mcu8080_disp = {0};

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
static inline void __mcu8080_write_cmd(void *bus_hdl, uint8_t cmd)
{
    tkl_gpio_write(MCU8080_LCD_RS_PIN, TUYA_GPIO_LEVEL_LOW);
    dev_write(bus_hdl, &cmd, 1);
}

static inline void __mcu8080_write_data8(void *bus_hdl, uint8_t dat)
{
    tkl_gpio_write(MCU8080_LCD_RS_PIN, TUYA_GPIO_LEVEL_HIGH);
    dev_write(bus_hdl, &dat, 1);
}

static inline void __mcu8080_write_data_dma(void *bus_hdl, uint8_t *dat, uint32_t len)
{
    tkl_gpio_write(MCU8080_LCD_RS_PIN, TUYA_GPIO_LEVEL_HIGH);
    /* Write in 16KB chunks using synchronous hardware-polled DMA */
    uint32_t chunk_size = 16384;
    for (uint32_t off = 0; off < len; off += chunk_size) {
        uint32_t cur = (len - off > chunk_size) ? chunk_size : (len - off);
        dev_write(bus_hdl, dat + off, cur);
    }
}

static void __mcu8080_send_init_seq(void *bus_hdl, const LCD_INIT_CODE_T *code, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (code[i].cmd == REGFLAG_DELAY) {
            tal_system_sleep(code[i].cnt);
        } else {
            __mcu8080_write_cmd(bus_hdl, code[i].cmd);
            for (uint32_t j = 0; j < code[i].cnt; j++) {
                __mcu8080_write_data8(bus_hdl, code[i].dat[j]);
            }
        }
    }
}

static void __mcu8080_set_window(void *bus_hdl, uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye)
{
    __mcu8080_write_cmd(bus_hdl, 0x2A);
    __mcu8080_write_data8(bus_hdl, (uint8_t)(xs >> 8));
    __mcu8080_write_data8(bus_hdl, (uint8_t)(xs & 0xFF));
    __mcu8080_write_data8(bus_hdl, (uint8_t)(xe >> 8));
    __mcu8080_write_data8(bus_hdl, (uint8_t)(xe & 0xFF));

    __mcu8080_write_cmd(bus_hdl, 0x2B);
    __mcu8080_write_data8(bus_hdl, (uint8_t)(ys >> 8));
    __mcu8080_write_data8(bus_hdl, (uint8_t)(ys & 0xFF));
    __mcu8080_write_data8(bus_hdl, (uint8_t)(ye >> 8));
    __mcu8080_write_data8(bus_hdl, (uint8_t)(ye & 0xFF));

    __mcu8080_write_cmd(bus_hdl, 0x2C);
}

static void __hw_reset(void)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg;
    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;

    tkl_gpio_init(MCU8080_LCD_RST_PIN, &gpio_cfg);
    tkl_gpio_write(MCU8080_LCD_RST_PIN, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(10);
    tkl_gpio_write(MCU8080_LCD_RST_PIN, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(20);
    tkl_gpio_write(MCU8080_LCD_RST_PIN, TUYA_GPIO_LEVEL_HIGH);
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
                    PR_NOTICE("MCU8080 LCD probe: detected GT911 tp, panel is ILI9341");
                    return true;
                }
            }
        }
    }
    return false;
}

static MCU8080_LCD_TYPE_E __probe_panel_type(void)
{
    if (__probe_gt911()) {
        return MCU8080_LCD_ILI9341;
    }

    /* Check PC07 (D6) and PC08 (D7) pull status */
    TUYA_GPIO_BASE_CFG_T in_cfg;
    in_cfg.mode = TUYA_GPIO_FLOATING;
    in_cfg.direct = TUYA_GPIO_INPUT;
    in_cfg.level = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(MCU8080_LCD_CHECK_D6_PIN, &in_cfg);
    tkl_gpio_init(MCU8080_LCD_CHECK_D7_PIN, &in_cfg);
    tal_system_sleep(5);

    TUYA_GPIO_LEVEL_E d6 = TUYA_GPIO_LEVEL_LOW, d7 = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_read(MCU8080_LCD_CHECK_D6_PIN, &d6);
    tkl_gpio_read(MCU8080_LCD_CHECK_D7_PIN, &d7);

    PR_NOTICE("MCU8080 LCD check pins: D6=%d, D7=%d", d6, d7);
    if (d6 == TUYA_GPIO_LEVEL_HIGH && d7 == TUYA_GPIO_LEVEL_HIGH) {
        PR_NOTICE("MCU8080 LCD probe: detected ILI9488 (320x480)");
        return MCU8080_LCD_ILI9488;
    }

    PR_NOTICE("MCU8080 LCD probe: detected ILI9481 (320x480)");
    return MCU8080_LCD_ILI9481;
}

static OPERATE_RET __mcu8080_disp_open(TDD_DISP_DEV_HANDLE_T device)
{
    MCU8080_DISP_CTRL_T *ctrl = (MCU8080_DISP_CTRL_T *)device;
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
    tkl_gpio_init(MCU8080_LCD_RS_PIN, &gpio_cfg);

    /* 2. Hardware reset */
    __hw_reset();

    /* 3. Probe panel type */
    ctrl->lcd_type = __probe_panel_type();
    if (ctrl->lcd_type == MCU8080_LCD_ILI9341) {
        ctrl->width = 240;
        ctrl->height = 320;
        ctrl->bl_active_level = TUYA_GPIO_LEVEL_HIGH;
    } else {
        ctrl->width = 320;
        ctrl->height = 480;
        /* Vendor AC791N DevKitBoard schematic: ILI9488 / ILI9481 backlight is active LOW */
        ctrl->bl_active_level = TUYA_GPIO_LEVEL_LOW;
    }

    /* 4. Open 8080 bus device ("emi") in synchronous polled mode */
    ctrl->bus_hdl = dev_open("emi", NULL);
    if (ctrl->bus_hdl == NULL) {
        PR_ERR("dev_open 8080 bus failed");
        return OPRT_COM_ERROR;
    }

    /* 5. Send init sequence */
    if (ctrl->lcd_type == MCU8080_LCD_ILI9341) {
        __mcu8080_send_init_seq(ctrl->bus_hdl, c_ili9341_init_seq, sizeof(c_ili9341_init_seq) / sizeof(c_ili9341_init_seq[0]));
    } else if (ctrl->lcd_type == MCU8080_LCD_ILI9488) {
        __mcu8080_send_init_seq(ctrl->bus_hdl, c_ili9488_init_seq, sizeof(c_ili9488_init_seq) / sizeof(c_ili9488_init_seq[0]));
    } else {
        __mcu8080_send_init_seq(ctrl->bus_hdl, c_ili9481_init_seq, sizeof(c_ili9481_init_seq) / sizeof(c_ili9481_init_seq[0]));
    }

    /* 6. Turn on backlight */
    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = ctrl->bl_active_level;
    tkl_gpio_init(MCU8080_LCD_BL_PIN, &gpio_cfg);
    tkl_gpio_write(MCU8080_LCD_BL_PIN, ctrl->bl_active_level);

    ctrl->opened = true;
    PR_NOTICE("MCU8080 LCD opened successfully: %dx%d, BL active=%d", ctrl->width, ctrl->height, ctrl->bl_active_level);
    return OPRT_OK;
}

static OPERATE_RET __mcu8080_disp_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    MCU8080_DISP_CTRL_T *ctrl = (MCU8080_DISP_CTRL_T *)device;
    if (ctrl == NULL || frame_buff == NULL || frame_buff->frame == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!ctrl->opened || ctrl->bus_hdl == NULL) {
        return OPRT_COM_ERROR;
    }

    uint16_t xs = frame_buff->x_start;
    uint16_t xe = frame_buff->x_start + frame_buff->width - 1;
    uint16_t ys = frame_buff->y_start;
    uint16_t ye = frame_buff->y_start + frame_buff->height - 1;

    __mcu8080_set_window(ctrl->bus_hdl, xs, xe, ys, ye);

    uint32_t len = (uint32_t)frame_buff->width * frame_buff->height * 2;
    __mcu8080_write_data_dma(ctrl->bus_hdl, (uint8_t *)frame_buff->frame, len);

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __mcu8080_disp_close(TDD_DISP_DEV_HANDLE_T device)
{
    MCU8080_DISP_CTRL_T *ctrl = (MCU8080_DISP_CTRL_T *)device;
    if (ctrl == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!ctrl->opened) {
        return OPRT_OK;
    }

    /* Turn off backlight */
    TUYA_GPIO_LEVEL_E bl_off = (ctrl->bl_active_level == TUYA_GPIO_LEVEL_HIGH) ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_write(MCU8080_LCD_BL_PIN, bl_off);

    if (ctrl->bus_hdl) {
        dev_close(ctrl->bus_hdl);
        ctrl->bus_hdl = NULL;
    }
    ctrl->opened = false;
    return OPRT_OK;
}

OPERATE_RET tdd_disp_mcu8080_ili9488_register(char *name)
{
    if (name == NULL) {
        return OPRT_INVALID_PARM;
    }

    static TDD_DISP_INTFS_T intfs = {
        .open = __mcu8080_disp_open,
        .close = __mcu8080_disp_close,
        .flush = __mcu8080_disp_flush,
    };

    /* Pre-probe panel type to configure default geometry for TDL registration */
    __hw_reset();
    MCU8080_LCD_TYPE_E type = __probe_panel_type();
    sg_mcu8080_disp.lcd_type = type;
    if (type == MCU8080_LCD_ILI9341) {
        sg_mcu8080_disp.width = 240;
        sg_mcu8080_disp.height = 320;
        sg_mcu8080_disp.bl_active_level = TUYA_GPIO_LEVEL_HIGH;
    } else {
        sg_mcu8080_disp.width = 320;
        sg_mcu8080_disp.height = 480;
        sg_mcu8080_disp.bl_active_level = TUYA_GPIO_LEVEL_LOW;
    }

    TDD_DISP_DEV_INFO_T dev_info = {
        .type     = TUYA_DISPLAY_8080,
        .width    = sg_mcu8080_disp.width,
        .height   = sg_mcu8080_disp.height,
        .fmt      = TUYA_PIXEL_FMT_RGB565,
        .rotation = TUYA_DISPLAY_ROTATION_0,
        .is_swap  = true,
        .has_vram = true,
        .bl       = {
            .type = TUYA_DISP_BL_TP_GPIO,
            .gpio = {
                .pin = MCU8080_LCD_BL_PIN,
                .active_level = sg_mcu8080_disp.bl_active_level,
            },
        },
        .power    = {
            .pin = TUYA_GPIO_NUM_MAX,
            .active_level = TUYA_GPIO_LEVEL_LOW,
        },
    };

    return tdl_disp_device_register(name, &sg_mcu8080_disp, &intfs, &dev_info);
}

/* Backward compatibility alias */
OPERATE_RET tdd_disp_emi_wl82_register(char *name)
{
    return tdd_disp_mcu8080_ili9488_register(name);
}
