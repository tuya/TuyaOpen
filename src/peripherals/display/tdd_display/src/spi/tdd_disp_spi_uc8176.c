/**
 * @file tdd_disp_spi_uc8176.c
 * @brief UC8176 / IL0398 E-Ink display driver (400x300, Waveshare 4.2" V1)
 *
 * Follows Waveshare's own epd4in2 reference for the first revision of the
 * module. Everything about it is the mirror image of the SSD1683 part that
 * tdd_disp_spi_uc8276.c drives, which is why it needs its own file rather than
 * a flag:
 *
 *   BUSY      LOW means busy here, HIGH on the SSD1683. And the line only
 *             updates when the controller is asked for status, so every poll
 *             sends GET_STATUS first.
 *   Power     Nothing responds until POWER_ON (0x04) has run and been waited
 *             for; the SSD1683 wakes up on its own.
 *   Image     Two RAMs written back to back, the outgoing image to DTM1 and
 *             the incoming one to DTM2, then one refresh command.
 *
 * Only full refreshes. The part does have a partial mode (0x90..0x92), but on
 * the factory waveform it ghosts badly enough that the result is worse than
 * the four seconds a full refresh costs.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "tkl_gpio.h"

#include "tdd_display_spi.h"
#include "tdl_display_manage.h"
#include "tdd_disp_uc8176.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* Panel setting. Bit 4 selects black/white rather than the three-colour mode,
 * bits 3 and 2 the scan direction, bits 1 and 0 keep the booster on and the
 * part out of soft reset.
 *
 * Bit 5 is the one that matters here: set, the waveform comes from the LUT
 * registers; clear, from the panel's own OTP. This driver sends no LUT tables,
 * so it has to be clear. A logic-analyser capture of the earlier 0x3F -- bit 5
 * set, no tables loaded -- shows exactly what an empty waveform looks like:
 * POWER_ON pulls BUSY low for 40 ms as it should, then DISPLAY_REFRESH returns
 * without ever going busy, because there are no frames to drive. The published
 * references that do use 0x3F or 0xBF all upload the five tables straight
 * after; running on the factory waveform instead is what makes them optional. */
#define UC8176_PSR_VALUE 0x1F

/* VCOM DC level. 0x12 is what the current reference uses; the older one used
 * 0x28. Too low washes the black out, too high leaves a grey background. */
#define UC8176_VCOM_DC_VALUE 0x12

/* VCOM and data interval. 0x97 leaves a white border, 0x57 a black one. */
#define UC8176_VCOM_INTERVAL_VALUE 0x97

/* PLL / frame rate. 0x3C is 50 Hz, 0x3A 100 Hz. Slower is a cleaner image. */
#define UC8176_PLL_VALUE 0x3C

/* How long each stage may take before the driver gives up and says so. Power
 * on and refresh are the slow ones; a 4.2 inch full refresh is about 4 s. */
#define UC8176_BUSY_TIMEOUT_MS 15000

/* Scratch used to stream a constant byte to the panel without a second full
 * frame buffer. Runs on the caller's stack, so it stays small. */
#define UC8176_FILL_CHUNK 128

/* Setting the BUSY pad to TUYA_GPIO_NUM_MAX runs the panel blind, on fixed
 * delays long enough for the worst case. Slower, and with no way to notice a
 * panel that has stopped answering -- but it is how a broken BUSY wire is told
 * apart from everything else being broken: if the glass updates like this, the
 * command and data lines are fine and BUSY alone is at fault. */

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    DISP_SPI_BASE_CFG_T    cfg;
    TUYA_GPIO_NUM_E        busy_pin;
    TUYA_DISPLAY_IO_CTRL_T power;
    bool                   is_sleeping;
    TDL_DISP_FRAME_BUFF_T *fb; /* panel-order copy of the frame being shown */
} DISP_UC8176_DEV_T;

/***********************************************************
***********************function define**********************
***********************************************************/
/* LSB-first (what the TDL mono format and u8g2 produce) to MSB-first (what the
 * panel shifts out, leftmost pixel first). */
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

static inline void __send_cmd(DISP_UC8176_DEV_T *dev, uint8_t cmd)
{
    tdd_disp_spi_send_cmd(&dev->cfg, cmd);
}

static inline void __send_data(DISP_UC8176_DEV_T *dev, uint8_t data)
{
    tdd_disp_spi_send_data(&dev->cfg, &data, 1);
}

static inline void __send_data_buf(DISP_UC8176_DEV_T *dev, const uint8_t *data, uint32_t len)
{
    tdd_disp_spi_send_data(&dev->cfg, (uint8_t *)data, len);
}

/**
 * @brief Stream one repeated byte to the panel, without a second frame buffer.
 */
static void __send_data_fill(DISP_UC8176_DEV_T *dev, uint8_t value, uint32_t len)
{
    uint8_t chunk[UC8176_FILL_CHUNK];

    memset(chunk, value, sizeof(chunk));

    while (len > 0) {
        uint32_t n = (len > sizeof(chunk)) ? sizeof(chunk) : len;
        __send_data_buf(dev, chunk, n);
        len -= n;
    }
}

/**
 * @brief Wait for the panel to go idle, which on this part means BUSY HIGH.
 *
 * The BUSY output is only refreshed when the controller is polled for status,
 * so GET_STATUS goes out on every pass. A read that fails counts as still busy
 * rather than as idle: treating an unreadable pad as "ready" would send the
 * next command into a panel that is not listening.
 *
 * @param what       what is being waited for, for the log.
 * @param blind_ms   how long to sleep instead when there is no BUSY pad to
 *                   read. Must cover the worst case for this stage, since
 *                   nothing else will.
 * @param must_block true for a stage the panel cannot possibly finish between
 *                   two polls. Returning immediately from one of those means
 *                   the line is not telling the truth, which is worth saying
 *                   out loud -- otherwise a panel that answers nothing looks
 *                   exactly like one that answers instantly.
 */
static void __wait_busy(DISP_UC8176_DEV_T *dev, const char *what, uint32_t blind_ms, bool must_block)
{
    uint32_t waited   = 0;
    bool     was_busy = false;

    if (dev->busy_pin >= TUYA_GPIO_NUM_MAX) {
        __delay_ms(blind_ms);
        return;
    }

    while (waited < UC8176_BUSY_TIMEOUT_MS) {
        TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_LOW;

        __send_cmd(dev, UC8176_GET_STATUS);

        if (OPRT_OK == tkl_gpio_read(dev->busy_pin, &level) && TUYA_GPIO_LEVEL_HIGH == level) {
            if (must_block && !was_busy) {
                PR_WARN("EPD: %s left BUSY high the whole time -- pad %d is not reading the panel, "
                        "so every wait below is a no-op and the glass never updates",
                        what, dev->busy_pin);
            }
            __delay_ms(20);
            return;
        }

        was_busy = true;
        __delay_ms(10);
        waited += 10;
    }

    PR_ERR("EPD: BUSY still low after %u ms waiting for %s", UC8176_BUSY_TIMEOUT_MS, what);
}

static void __reset(DISP_UC8176_DEV_T *dev)
{
    if (dev->cfg.rst_pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    tkl_gpio_write(dev->cfg.rst_pin, TUYA_GPIO_LEVEL_HIGH);
    __delay_ms(200);
    tkl_gpio_write(dev->cfg.rst_pin, TUYA_GPIO_LEVEL_LOW);
    __delay_ms(4);
    tkl_gpio_write(dev->cfg.rst_pin, TUYA_GPIO_LEVEL_HIGH);
    __delay_ms(200);
}

static void __epd_init(DISP_UC8176_DEV_T *dev)
{
    __reset(dev);

    __send_cmd(dev, UC8176_POWER_SETTING);
    __send_data(dev, 0x03); /* VDS_EN, VDG_EN */
    __send_data(dev, 0x00); /* VCOM_HV, VGHL_LV */
    __send_data(dev, 0x2B); /* VDH */
    __send_data(dev, 0x2B); /* VDL */

    __send_cmd(dev, UC8176_BOOSTER_SOFT_START);
    __send_data(dev, 0x17);
    __send_data(dev, 0x17);
    __send_data(dev, 0x17);

    /* Nothing below this point is answered until the charge pump is up. */
    __send_cmd(dev, UC8176_POWER_ON);
    __wait_busy(dev, "power on", 300, false);

    __send_cmd(dev, UC8176_PANEL_SETTING);
    __send_data(dev, UC8176_PSR_VALUE);

    __send_cmd(dev, UC8176_PLL_CONTROL);
    __send_data(dev, UC8176_PLL_VALUE);

    __send_cmd(dev, UC8176_RESOLUTION_SETTING);
    __send_data(dev, (dev->cfg.width >> 8) & 0xFF);
    __send_data(dev, dev->cfg.width & 0xFF);
    __send_data(dev, (dev->cfg.height >> 8) & 0xFF);
    __send_data(dev, dev->cfg.height & 0xFF);

    __send_cmd(dev, UC8176_VCOM_DC_SETTING);
    __send_data(dev, UC8176_VCOM_DC_VALUE);

    __send_cmd(dev, UC8176_VCOM_DATA_INTERVAL);
    __send_data(dev, UC8176_VCOM_INTERVAL_VALUE);

    dev->is_sleeping = false;

    PR_NOTICE("EPD: UC8176 init done, %dx%d, psr 0x%02X", dev->cfg.width, dev->cfg.height, UC8176_PSR_VALUE);
}

static void __epd_refresh(DISP_UC8176_DEV_T *dev)
{
    __send_cmd(dev, UC8176_DISPLAY_REFRESH);
    __delay_ms(100);
    __wait_busy(dev, "refresh", 5000, true);
}

/**
 * @brief Convert a TDL monochrome frame into the panel's bit order.
 *
 * The source has the leftmost pixel in the least significant bit and a set bit
 * means ink; the panel wants the leftmost pixel in the most significant bit and
 * a set bit meaning white. So each byte is reversed and inverted.
 */
static void __epd_fb_convert(TDL_DISP_FRAME_BUFF_T *src_fb, TDL_DISP_FRAME_BUFF_T *dst_fb)
{
    uint32_t src_width_bytes = 0, dst_width_bytes = 0;

    if (NULL == src_fb || NULL == dst_fb) {
        PR_ERR("EPD: frame buffer is NULL");
        return;
    }

    src_width_bytes = (src_fb->width + 7) / 8;
    dst_width_bytes = (dst_fb->width + 7) / 8;

    for (uint16_t j = 0; j < dst_fb->height; j++) {
        for (uint16_t i = 0; i < dst_width_bytes; i++) {
            uint8_t src_byte = 0xFF; /* anything the source does not cover stays white */

            if (i < src_width_bytes && j < src_fb->height) {
                src_byte = (uint8_t)(~__reverse_bits(src_fb->frame[j * src_width_bytes + i]));
            }

            dst_fb->frame[j * dst_width_bytes + i] = src_byte;
        }
    }
}

/**
 * @brief Push one frame and refresh.
 *
 * DTM1 takes the image being replaced and DTM2 the one being drawn; the
 * waveform uses both to decide how to drive each pixel. The driver does not
 * keep the previous frame around, so DTM1 gets a white page -- which is what
 * the panel was cleared to, and what the reference sends.
 */
static void __epd_display(DISP_UC8176_DEV_T *dev, TDL_DISP_FRAME_BUFF_T *fb)
{
    if (NULL == dev || NULL == fb) {
        PR_ERR("EPD: dev or fb is NULL");
        return;
    }

    __send_cmd(dev, UC8176_DATA_START_TRANS_1);
    __send_data_fill(dev, 0xFF, fb->len);
    __delay_ms(10);

    __send_cmd(dev, UC8176_DATA_START_TRANS_2);
    __send_data_buf(dev, fb->frame, fb->len);
    __delay_ms(10);

    __epd_refresh(dev);
}

static void __epd_clear(DISP_UC8176_DEV_T *dev, uint32_t len)
{
    __send_cmd(dev, UC8176_DATA_START_TRANS_1);
    __send_data_fill(dev, 0xFF, len);
    __delay_ms(10);

    __send_cmd(dev, UC8176_DATA_START_TRANS_2);
    __send_data_fill(dev, 0xFF, len);
    __delay_ms(10);

    __epd_refresh(dev);
}

/**
 * @brief Bring the panel back from a sleep, supply first.
 *
 * __epd_sleep() may have cut the rail, so the init sequence would otherwise be
 * talking to a part that has no power.
 */
static void __epd_wake(DISP_UC8176_DEV_T *dev)
{
    if (!dev->is_sleeping) {
        return;
    }

    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(dev->power.pin, TUYA_GPIO_LEVEL_HIGH);
        __delay_ms(100);
    }

    __epd_init(dev);
}

static void __epd_sleep(DISP_UC8176_DEV_T *dev)
{
    if (dev->is_sleeping) {
        return;
    }

    __send_cmd(dev, UC8176_POWER_OFF);
    __wait_busy(dev, "power off", 300, false);

    __send_cmd(dev, UC8176_DEEP_SLEEP);
    __send_data(dev, 0xA5);
    __delay_ms(100);

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
    OPERATE_RET        rt        = OPRT_OK;
    DISP_UC8176_DEV_T *dev       = (DISP_UC8176_DEV_T *)device;
    uint32_t           frame_len = 0;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_LOG(tdd_disp_spi_init(&dev->cfg));
    __delay_ms(100);

    if (dev->busy_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {
            .mode   = TUYA_GPIO_PULLUP,
            .direct = TUYA_GPIO_INPUT,
            .level  = TUYA_GPIO_LEVEL_HIGH,
        };
        TUYA_CALL_ERR_LOG(tkl_gpio_init(dev->busy_pin, &gpio_cfg));
    }

    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {
            .mode   = TUYA_GPIO_PUSH_PULL,
            .direct = TUYA_GPIO_OUTPUT,
            .level  = dev->power.active_level,
        };
        TUYA_CALL_ERR_LOG(tkl_gpio_init(dev->power.pin, &gpio_cfg));
        __delay_ms(50);
    }

    /* Allocated before the panel is touched so the clear below can stream from
     * it instead of sending 15000 single-byte transfers. */
    frame_len = ((dev->cfg.width + 7) / 8) * dev->cfg.height;
    if (NULL == dev->fb) {
        dev->fb = tdl_disp_create_frame_buff(DISP_FB_TP_SRAM, frame_len);
        if (NULL == dev->fb) {
            return OPRT_MALLOC_FAILED;
        }
    }
    memset(dev->fb->frame, 0xFF, frame_len);
    dev->fb->fmt    = TUYA_PIXEL_FMT_MONOCHROME;
    dev->fb->width  = dev->cfg.width;
    dev->fb->height = dev->cfg.height;

    __epd_init(dev);
    __epd_clear(dev, frame_len);

    PR_NOTICE("EPD: UC8176 ready");

    return OPRT_OK;
}

static OPERATE_RET __tdd_disp_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_UC8176_DEV_T *dev = (DISP_UC8176_DEV_T *)device;

    if (NULL == device || NULL == frame_buff || NULL == dev->fb) {
        return OPRT_INVALID_PARM;
    }

    __epd_wake(dev);

    __epd_fb_convert(frame_buff, dev->fb);
    __epd_display(dev, dev->fb);

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_disp_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_UC8176_DEV_T *dev = (DISP_UC8176_DEV_T *)device;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    __epd_sleep(dev);

    if (dev->fb) {
        tdl_disp_free_frame_buff(dev->fb);
        dev->fb = NULL;
    }

    return OPRT_OK;
}

/*****************************************************************************
 * Public API
 *****************************************************************************/
OPERATE_RET tdd_disp_spi_mono_uc8176_register(char *name, DISP_EINK_UC8176_CFG_T *dev_cfg)
{
    OPERATE_RET        rt       = OPRT_OK;
    DISP_UC8176_DEV_T *disp_dev = NULL;

    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    disp_dev = (DISP_UC8176_DEV_T *)tal_malloc(sizeof(DISP_UC8176_DEV_T));
    if (NULL == disp_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(disp_dev, 0, sizeof(DISP_UC8176_DEV_T));

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
    disp_dev->is_sleeping   = true;

    TDD_DISP_DEV_INFO_T disp_dev_info = {
        .type     = TUYA_DISPLAY_SPI,
        .width    = dev_cfg->width,
        .height   = dev_cfg->height,
        .fmt      = TUYA_PIXEL_FMT_MONOCHROME,
        .rotation = dev_cfg->rotation,
        .is_swap  = true,
        .has_vram = true,
    };
    memcpy(&disp_dev_info.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&disp_dev_info.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    TDD_DISP_INTFS_T disp_intfs = {
        .open  = __tdd_disp_open,
        .flush = __tdd_disp_flush,
        .close = __tdd_disp_close,
    };

    TUYA_CALL_ERR_GOTO(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_dev, &disp_intfs, &disp_dev_info),
                       err_exit);

    PR_NOTICE("EPD: %s registered, UC8176 %dx%d", name, dev_cfg->width, dev_cfg->height);

    return OPRT_OK;

err_exit:
    tal_free(disp_dev);
    return rt;
}
