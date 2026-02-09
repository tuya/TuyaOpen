/**
 * @file tdd_disp_spi_uc8276.c
 * @brief UC8276 E-Ink display driver (400x300, 4.2" V2)
 *
 * Based on Waveshare epd4in2_V2 reference implementation.
 * BUSY: HIGH=busy, LOW=idle | Reset: HIGH(100ms)->LOW(2ms)->HIGH(100ms)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "tkl_gpio.h"

#include "tdd_display_spi.h"
#include "tdl_display_manage.h"
#include "tdd_disp_uc8276.h"

typedef struct {
    DISP_SPI_BASE_CFG_T    cfg;
    TUYA_GPIO_NUM_E        busy_pin;
    TUYA_DISPLAY_IO_CTRL_T power;           /* Power control pin */
    bool                   is_sleeping;     /* Track sleep state */
    bool                   is_use_partial;  /* Use partial refresh flag */
    bool                   partial_mode;    /* Partial refresh mode flag */
    TDL_DISP_FRAME_BUFF_T *fb;              /* Current frame buffer */
} DISP_UC8276_DEV_T;

/*****************************************************************************
 * Low-level EPD functions
 *****************************************************************************/

/* Reverse bits in a byte (LSB<->MSB) for LVGL compatibility */
static inline uint8_t __reverse_bits(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

static inline void __delay_ms(uint32_t ms)
{
    tal_system_sleep(ms);
}

static inline void __send_cmd(DISP_UC8276_DEV_T *dev, uint8_t cmd)
{
    tdd_disp_spi_send_cmd(&dev->cfg, cmd);
}

static inline void __send_data(DISP_UC8276_DEV_T *dev, uint8_t data)
{
    tdd_disp_spi_send_data(&dev->cfg, &data, 1);
}

static inline void __send_data_buf(DISP_UC8276_DEV_T *dev, const uint8_t *data, uint32_t len)
{
    tdd_disp_spi_send_data(&dev->cfg, (uint8_t *)data, len);
}

/* Wait for BUSY pin to go LOW (idle) with timeout */
static void __wait_busy(DISP_UC8276_DEV_T *dev)
{
    TUYA_GPIO_LEVEL_E level;
    uint32_t          timeout = 0;

    if (dev->busy_pin >= TUYA_GPIO_NUM_MAX) {
        __delay_ms(3000);
        return;
    }

    while (timeout < 60000) {
        tkl_gpio_read(dev->busy_pin, &level);
        if (level == TUYA_GPIO_LEVEL_LOW) {
            return;
        }
        __delay_ms(10);
        timeout += 10;
    }
    PR_ERR("EPD: BUSY timeout");
}

/* Hardware reset sequence */
static void __reset(DISP_UC8276_DEV_T *dev)
{
    if (dev->cfg.rst_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(dev->cfg.rst_pin, TUYA_GPIO_LEVEL_HIGH);
        __delay_ms(100);
        tkl_gpio_write(dev->cfg.rst_pin, TUYA_GPIO_LEVEL_LOW);
        __delay_ms(2);
        tkl_gpio_write(dev->cfg.rst_pin, TUYA_GPIO_LEVEL_HIGH);
        __delay_ms(100);
    }
}

/* Full display update */
static void __update_display(DISP_UC8276_DEV_T *dev)
{
    __send_cmd(dev, 0x22);
    __send_data(dev, 0xF7);
    __send_cmd(dev, 0x20);
    __wait_busy(dev);
}

/* Partial display update (fast, no erase, may have ghosting) */
static void __update_display_partial(DISP_UC8276_DEV_T *dev)
{
    __send_cmd(dev, 0x21);
    __send_data(dev, 0x00);
    __send_data(dev, 0x00);
    __send_cmd(dev, 0x22);
    __send_data(dev, 0xFC);
    __send_cmd(dev, 0x20);
    __wait_busy(dev);
}

/*****************************************************************************
 * EPD operations
 *****************************************************************************/

/* Forward declarations */
static void __epd_partial_out(DISP_UC8276_DEV_T *dev);
static void __epd_partial_in(DISP_UC8276_DEV_T *dev);

static void __epd_set_windows(DISP_UC8276_DEV_T *dev, uint16_t x_start, uint16_t y_start,
                             uint16_t x_end, uint16_t y_end)
{
    PR_NOTICE("EPD: set window x[%d-%d], y[%d-%d]", x_start, x_end, y_start, y_end);

    __send_cmd(dev, 0x44); // RAM X range: 0-49 (50 bytes = 400 pixels)
    __send_data(dev, x_start/8);
    __send_data(dev, x_end/8);

    __send_cmd(dev, 0x45); // Set row address
    __send_data(dev, y_start & 0xFF);
    __send_data(dev, (y_start >> 8) & 0xFF);
    __send_data(dev, y_end & 0xFF);
    __send_data(dev, (y_end >> 8) & 0xFF);
}

static void __epd_fb_convert(TDL_DISP_FRAME_BUFF_T *src_fb, TDL_DISP_FRAME_BUFF_T *dst_fb)
{
    uint32_t src_width_bytes = 0, dst_width_bytes = 0;

    if(NULL == src_fb || NULL == dst_fb) {
        PR_ERR("EPD: frame buffer is NULL");
        return;
    }

    src_width_bytes = (src_fb->width + 7) / 8;
    dst_width_bytes = (dst_fb->width + 7) / 8;

    for (uint16_t j = 0; j < dst_fb->height; j++) {
        for (uint16_t i = 0; i < dst_width_bytes; i++) {
            uint8_t src_byte = 0xFF;

            if(i < src_width_bytes && j < src_fb->height) {
                src_byte = src_fb->frame[j * src_width_bytes + i];
                src_byte = ~__reverse_bits(src_byte);
            }else {
                src_byte = 0xFF;
            }
        
            dst_fb->frame[j * dst_width_bytes + i] = src_byte;
        }
    }

}


static void __epd_init(DISP_UC8276_DEV_T *dev)
{
    __reset(dev);
    __wait_busy(dev);

    __send_cmd(dev, 0x12); // Software reset
    __wait_busy(dev);

    __send_cmd(dev, 0x21); // Display update control
    __send_data(dev, 0x40);
    __send_data(dev, 0x00);

    __send_cmd(dev, 0x3C); // Border waveform
    __send_data(dev, 0x05);

    __send_cmd(dev, 0x11); // Data entry mode: X+, Y+
    __send_data(dev, 0x03);

    __epd_set_windows(dev, 0, 0, dev->cfg.width - 1, dev->cfg.height - 1);

    __send_cmd(dev, 0x4E); // RAM X counter
    __send_data(dev, 0x00);

    __send_cmd(dev, 0x4F); // RAM Y counter
    __send_data(dev, 0x00);
    __send_data(dev, 0x00);

    __wait_busy(dev);
}

static void __epd_clear(DISP_UC8276_DEV_T *dev)
{
    if(NULL == dev) {
        PR_ERR("EPD: dev is NULL");
        return;
    }

    const uint32_t size = (dev->cfg.width / 8) * dev->cfg.height;

    __epd_partial_out(dev);

    __send_cmd(dev, 0x24);
    for (uint32_t i = 0; i < size; i++) {
        __send_data(dev, 0xFF);
    }

    __send_cmd(dev, 0x26);
    for (uint32_t i = 0; i < size; i++) {
        __send_data(dev, 0xFF);
    }

    __update_display(dev);
}

__attribute__((unused)) static void __epd_partial_in(DISP_UC8276_DEV_T *dev)
{
    if (dev->partial_mode) {
        return;
    }
    dev->partial_mode = true;
}

__attribute__((unused)) static void __epd_partial_out(DISP_UC8276_DEV_T *dev)
{
    if (!dev->partial_mode) {
        return;
    }
    dev->partial_mode = false;
}

static void __epd_display(DISP_UC8276_DEV_T *dev, TDL_DISP_FRAME_BUFF_T *fb)
{
    if(NULL == dev) {
        PR_ERR("EPD: dev is NULL");
        return;
    }

    if (dev->is_use_partial) {
        /* Partial refresh mode: fast, no erase, may have ghosting */
        /* Set RAM area directly */

        /* Set RAM entry mode (x increase, y increase) */
        __send_cmd(dev, 0x11);  // Data entry mode
        __send_data(dev, 0x03); // x increase, y increase: normal mode

        /* Set RAM address range for partial refresh (full screen) */
        __epd_set_windows(dev, fb->x_start, fb->y_start,
                         fb->x_start + fb->width - 1,
                         fb->y_start + fb->height - 1);

        /* Set RAM address counter to start position */
        __send_cmd(dev, 0x4E); // RAM X counter
        __send_data(dev, 0x00);
        __send_cmd(dev, 0x4F);  // RAM Y counter
        __send_data(dev, 0x00); // Y counter low
        __send_data(dev, 0x00); // Y counter high

        __send_cmd(dev, 0x24); // DTM1 - Data transmission for partial refresh
        __send_data_buf(dev, fb->frame, fb->len);

        __update_display_partial(dev);
    } else {
        /* Full refresh mode: clear screen buffer first to remove ghosting */
        __epd_partial_out(dev);
        
        /* Set RAM entry mode (x increase, y increase) */
        __send_cmd(dev, 0x11);  // Data entry mode
        __send_data(dev, 0x03); // x increase, y increase: normal mode

        /* Set RAM address range for partial refresh (full screen) */
        __epd_set_windows(dev, fb->x_start, fb->y_start,
                         fb->x_start + fb->width - 1,
                         fb->y_start + fb->height - 1);

        /* Set RAM address counter to start position */
        __send_cmd(dev, 0x4E); // RAM X counter
        __send_data(dev, 0x00);
        __send_cmd(dev, 0x4F);  // RAM Y counter
        __send_data(dev, 0x00); // Y counter low
        __send_data(dev, 0x00); // Y counter high

        __send_cmd(dev, 0x24); // DTM1 - Data transmission for partial refresh
        for (uint32_t i = 0; i < fb->len; i++) {
            __send_data(dev, 0xFF);
        }

        __update_display_partial(dev);

        /* Now write the actual image data */
        __send_cmd(dev, 0x24);
        __send_data_buf(dev, fb->frame, fb->len);

        __send_cmd(dev, 0x26);
        __send_data_buf(dev, fb->frame, fb->len);

        __update_display(dev);
    }
}

/* Wake EPD from sleep (power on and reinitialize) */
static void __epd_wake(DISP_UC8276_DEV_T *dev)
{
    if (!dev->is_sleeping) {
        return; /* Already awake */
    }

    /* Power on if power control pin is available */
    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(dev->power.pin, TUYA_GPIO_LEVEL_HIGH);
        __delay_ms(100);
    }

    /* Reinitialize display after power on */
    __epd_init(dev);
    dev->is_sleeping  = false;
    dev->partial_mode = false;
}

/* Put EPD to sleep (power off) */
static void __epd_sleep(DISP_UC8276_DEV_T *dev)
{
    if (dev->is_sleeping) {
        return; /* Already sleeping */
    }

    /* Send deep sleep command */
    __send_cmd(dev, 0x10);
    __send_data(dev, 0x01);
    __delay_ms(100);

    /* Power off if power control pin is available */
    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(dev->power.pin, TUYA_GPIO_LEVEL_LOW);
        __delay_ms(10);
    }

    dev->is_sleeping = true;
}

/*****************************************************************************
 * TDD Driver Interface
 *****************************************************************************/

static OPERATE_RET __tdd_disp_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_UC8276_DEV_T *dev = (DISP_UC8276_DEV_T *)device;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    /* Initialize SPI and control pins */
    tdd_disp_spi_init(&dev->cfg);
    __delay_ms(100);

    /* Initialize BUSY pin as input with pull-up */
    if (dev->busy_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {
            .mode   = TUYA_GPIO_PULLUP,
            .direct = TUYA_GPIO_INPUT,
            .level  = TUYA_GPIO_LEVEL_HIGH,
        };
        tkl_gpio_init(dev->busy_pin, &gpio_cfg);
    }

    /* Initialize power control pin if available */
    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {
            .mode   = TUYA_GPIO_PULLUP,
            .direct = TUYA_GPIO_OUTPUT,
            .level  = TUYA_GPIO_LEVEL_HIGH,
        };
        tkl_gpio_init(dev->power.pin, &gpio_cfg);
        __delay_ms(50);
    }

    /* Wake display from sleep (power on and initialize) */
    __epd_wake(dev);
    __epd_clear(dev);

    uint32_t frame_len = ((dev->cfg.width+7) / 8) * dev->cfg.height;

    dev->fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == dev->fb) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev->fb->frame, 0xFF, frame_len);

    dev->fb->fmt    = TUYA_PIXEL_FMT_MONOCHROME;
    dev->fb->width  = dev->cfg.width;
    dev->fb->height = dev->cfg.height;

    PR_NOTICE("EPD: initialized");
    return OPRT_OK;
}

#if 0
static void __print_frame_buffer_bits(uint8_t *frame, uint32_t width, uint32_t height, const char *title)
{
    uint32_t width_bytes = (width + 7) / 8;
    uint32_t total_bytes = width_bytes * height;
    uint32_t byte_idx, bit_idx;
    uint32_t row, col;
    char line_buf[256];
    uint32_t pos;
    
    PR_NOTICE("========== %s ==========", title);
    PR_NOTICE("Width: %d, Height: %d, Width_bytes: %d, Total_bytes: %d", width, height, width_bytes, total_bytes);
    
    /* Print first few rows for debugging */
    for(row = 0; row < (height<64?height:64); row++) {
        pos = 0;
        for(col = 0; col < (width<128?width:128) && pos < sizeof(line_buf) - 1; col++) {
            byte_idx = row * width_bytes + (col / 8);
            bit_idx = col % 8;
            if(byte_idx < total_bytes) {
                uint8_t bit = (frame[byte_idx] >> bit_idx) & 0x01;
                line_buf[pos++] = (bit ? '1' : '0');
            }
        }
        line_buf[pos] = '\0';
        bk_printf("%s\r\n", line_buf);
    }
    
    PR_NOTICE("");
    PR_NOTICE("================================");
}
#endif

static OPERATE_RET __tdd_disp_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_UC8276_DEV_T *dev = (DISP_UC8276_DEV_T *)device;

    if (NULL == device || NULL == frame_buff || NULL == dev->fb) {
        return OPRT_INVALID_PARM;
    }

    __epd_fb_convert(frame_buff, dev->fb);

    __epd_display(dev, dev->fb);

    if (false == dev->is_use_partial) {
        dev->is_use_partial = true;
    }

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_disp_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_UC8276_DEV_T *dev = (DISP_UC8276_DEV_T *)device;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    /* Put display to sleep (power off) */
    __epd_sleep(dev);

    dev->is_use_partial = false;

    if (dev->fb) {
        tdl_disp_free_frame_buff(dev->fb);
        dev->fb = NULL;
    }

    return OPRT_OK;
}

/*****************************************************************************
 * Public API
 *****************************************************************************/

OPERATE_RET tdd_disp_spi_mono_uc8276_register(char *name, DISP_EINK_UC8276_CFG_T *dev_cfg)
{
    OPERATE_RET        rt       = OPRT_OK;
    DISP_UC8276_DEV_T *disp_dev = NULL;

    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    disp_dev = (DISP_UC8276_DEV_T *)tal_malloc(sizeof(DISP_UC8276_DEV_T));
    if (NULL == disp_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(disp_dev, 0, sizeof(DISP_UC8276_DEV_T));

    /* Configure device */
    disp_dev->cfg.width     = dev_cfg->width;
    disp_dev->cfg.height    = dev_cfg->height;
    disp_dev->cfg.x_offset  = 0;
    disp_dev->cfg.y_offset  = 0;
    disp_dev->cfg.pixel_fmt = TUYA_PIXEL_FMT_MONOCHROME;
    disp_dev->cfg.port      = dev_cfg->port;
    disp_dev->cfg.spi_clk   = dev_cfg->spi_clk;
    disp_dev->cfg.cs_pin    = dev_cfg->cs_pin;
    disp_dev->cfg.dc_pin    = dev_cfg->dc_pin;
    disp_dev->cfg.rst_pin   = dev_cfg->rst_pin;
    disp_dev->busy_pin      = dev_cfg->busy_pin;
    disp_dev->power         = dev_cfg->power;
    disp_dev->is_sleeping   = true;  /* Start in sleep state */
    disp_dev->partial_mode  = false; /* Start with full refresh mode */

    /* Fill device info */
    TDD_DISP_DEV_INFO_T disp_dev_info = {
        .type     = TUYA_DISPLAY_SPI,
        .width    = dev_cfg->width,
        .height   = dev_cfg->height,
        .fmt      = TUYA_PIXEL_FMT_MONOCHROME,
        .rotation = dev_cfg->rotation,
        .is_swap  = true,
        .has_vram = true,
    };
    disp_dev_info.power.pin   = TUYA_GPIO_NUM_MAX;
    disp_dev_info.bl.gpio.pin = TUYA_GPIO_NUM_MAX;
    memcpy(&disp_dev_info.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&disp_dev_info.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    TDD_DISP_INTFS_T disp_intfs = {
        .open  = __tdd_disp_open,
        .flush = __tdd_disp_flush,
        .close = __tdd_disp_close,
    };

    TUYA_CALL_ERR_GOTO(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_dev, &disp_intfs, &disp_dev_info),
                       err_exit);

    PR_NOTICE("EPD: %s registered (%dx%d)", name, dev_cfg->width, dev_cfg->height);

    return OPRT_OK;

err_exit:
    tal_free(disp_dev);
    return rt;
}
