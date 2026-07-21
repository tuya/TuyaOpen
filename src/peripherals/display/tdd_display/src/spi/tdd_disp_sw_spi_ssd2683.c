/**
 * @file tdd_disp_sw_spi_ssd2683.c
 * @brief SSD2683 E-Ink display driver (B/W, 400x300) over software (bit-banged) SPI.
 *
 * The controller command/refresh flow follows the vendor reference
 * (docs/4D2_BW_SSD2683_300x400_Code.c). The panel data lines on this hardware are
 * routed to general-purpose GPIO rather than a hardware SPI peripheral, so this
 * driver bit-bangs a 4-wire SPI bus (CLK/SDA/CS/DC) and reads back on SDA for the
 * on-chip temperature sensor.
 *
 * Hardware-tweakable details (flagged inline):
 *   - BUSY polarity: HIGH = ready (vendor polls BUSY until HIGH).
 *   - Pixel inversion in __epd_fb_convert() (matches UC8276 convention).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "tkl_gpio.h"

#include "tdl_display_manage.h"
#include "tdl_display_driver.h"
#include "tdd_disp_ssd2683.h"

/* [PERF] timing instrumentation — off by default. Set PERF_LOG_ENABLE to 1 to print. */
#define PERF_LOG_ENABLE 0
#define PERF_LOG(...)                                                                                                  \
    do {                                                                                                               \
        if (PERF_LOG_ENABLE) {                                                                                         \
            PR_NOTICE(__VA_ARGS__);                                                                                    \
        }                                                                                                              \
    } while (0)

/* ---------------------------------------------------------------------------
 * Fast GPIO for the bit-bang hot path (BK7258 / T5AI).
 *
 * tkl_gpio_write() walks tkl_gpio_write -> bk_gpio_set_output -> gpio_hal ->
 * gpio_ll (4 non-inlined calls + range checks) per edge. At ~720k edges for one
 * full-frame transfer that path alone measured ~775 ms (see the sw_spi_tx [PERF]
 * line). The BK7258 AON-GPIO block is one 32-bit config word per pin at
 * SOC_AON_GPIO_REG_BASE + pin*4, with bit[1] = output level (see gpio_struct.h).
 * We cache each hot pin's register pointer + its other (unchanging) config bits
 * once, then drive the level with a single 32-bit store inside the loop.
 *
 * The AON-GPIO base is SOC_AON_GPIO_REG_BASE from the platform's soc/reg_base.h,
 * but that header pulls sdkconfig.h, which is not on this component's include
 * path. The value is mirrored here instead: this display runs on the BK7258 AP
 * core, which is the secure world (CONFIG_SPE=1) -> base 0x44000400. (The
 * non-secure alias would be 0x54000400 = +SOC_S_NS_ADDR_DIFF.) Override
 * SSD2683_AON_GPIO_BASE if the build's security domain ever changes, or set
 * SSD2683_FAST_GPIO to 0 to fall back to tkl_gpio_write (non-BK platforms). */
#ifndef SSD2683_FAST_GPIO
#define SSD2683_FAST_GPIO 1
#endif

#if SSD2683_FAST_GPIO
#ifndef SSD2683_AON_GPIO_BASE
#define SSD2683_AON_GPIO_BASE 0x44000400u
#endif
#define SSD2683_GPIO_OUT_BIT (1u << 1) /* gpio_output, bit[1] of the per-pin cfg word */
static inline volatile uint32_t *__gpio_cfg_reg(TUYA_GPIO_NUM_E pin)
{
    return (volatile uint32_t *)(uintptr_t)(SSD2683_AON_GPIO_BASE + (uint32_t)pin * 4u);
}
#endif

typedef struct {
    uint16_t               width;
    uint16_t               height;
    TUYA_GPIO_NUM_E        clk_pin;
    TUYA_GPIO_NUM_E        sda_pin;
    TUYA_GPIO_NUM_E        cs_pin;
    TUYA_GPIO_NUM_E        dc_pin;
    TUYA_GPIO_NUM_E        rst_pin;
    TUYA_GPIO_NUM_E        busy_pin;
    TUYA_DISPLAY_IO_CTRL_T power;
    bool                   is_sleeping;
    bool                   is_use_partial;      /* false until the first full refresh has run */
    bool                   hot;                 /* TRUE: booster powered + inited; stays on (no 0x02) so
                                                   every refresh skips reset / 0x04 power-on */
    bool                   lut_is_fast;         /* TRUE: the fast (non-flashing) waveform LUT is loaded;
                                                   FALSE after a de-ghost full, so the next partial reloads it */
    uint8_t                temp_value;          /* cached 0xE6 booster value from the last temperature read */
    bool                   temp_valid;          /* TRUE once temp_value has been read at least once */
    uint32_t               temp_read_ms;        /* timestamp of the last temperature read (for re-cache) */
    uint16_t               full_refresh_period; /* force a full refresh every N fast ones (0 = always full) */
    uint16_t               partial_count;       /* fast refreshes since the last full refresh */
    TDL_DISP_FRAME_BUFF_T *fb;                  /* Panel-domain 1bpp buffer for the new frame (MSB first) */
    TDL_DISP_FRAME_BUFF_T *old_fb;              /* Panel-domain 1bpp buffer holding the on-screen frame */
#if SSD2683_FAST_GPIO
    volatile uint32_t *clk_reg; /* cached AON-GPIO cfg register for CLK */
    volatile uint32_t *sda_reg; /* cached AON-GPIO cfg register for SDA */
    uint32_t           clk_word; /* CLK cfg word with the output bit cleared */
    uint32_t           sda_word; /* SDA cfg word with the output bit cleared */
#endif
} DISP_SSD2683_DEV_T;

/*****************************************************************************
 * Low-level bit-banged SPI
 *****************************************************************************/

static inline void __delay_ms(uint32_t ms)
{
    tal_system_sleep(ms);
}

/* Shift out one byte, MSB first. Data is latched on the CLK rising edge. */
static void __spi_shift_out(DISP_SSD2683_DEV_T *dev, uint8_t data)
{
#if SSD2683_FAST_GPIO
    /* Hot path: direct AON-GPIO stores, no per-edge call/validation overhead.
     * Same waveform as the tkl path: set SDA, then CLK low -> CLK high (latch). */
    volatile uint32_t *clk    = dev->clk_reg;
    volatile uint32_t *sda    = dev->sda_reg;
    const uint32_t     clk_lo = dev->clk_word;
    const uint32_t     clk_hi = dev->clk_word | SSD2683_GPIO_OUT_BIT;
    const uint32_t     sda_lo = dev->sda_word;
    const uint32_t     sda_hi = dev->sda_word | SSD2683_GPIO_OUT_BIT;

    for (uint8_t n = 0; n < 8; n++) {
        *sda = (data & 0x80) ? sda_hi : sda_lo;
        data <<= 1;
        *clk = clk_lo;
        *clk = clk_hi;
    }
#else
    for (uint8_t n = 0; n < 8; n++) {
        tkl_gpio_write(dev->sda_pin, (data & 0x80) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
        data <<= 1;
        tkl_gpio_write(dev->clk_pin, TUYA_GPIO_LEVEL_LOW);
        tkl_gpio_write(dev->clk_pin, TUYA_GPIO_LEVEL_HIGH);
    }
#endif
}

/* Write a command byte (DC low). CS is toggled to latch the start of a frame. */
static void __write_cmd(DISP_SSD2683_DEV_T *dev, uint8_t cmd)
{
    tkl_gpio_write(dev->cs_pin, TUYA_GPIO_LEVEL_HIGH);
    tkl_gpio_write(dev->cs_pin, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(dev->clk_pin, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(dev->dc_pin, TUYA_GPIO_LEVEL_LOW);
    __spi_shift_out(dev, cmd);
}

/* Write a data byte (DC high). CS is held low from the preceding command. */
static void __write_data(DISP_SSD2683_DEV_T *dev, uint8_t data)
{
    tkl_gpio_write(dev->dc_pin, TUYA_GPIO_LEVEL_HIGH);
    __spi_shift_out(dev, data);
}

/* Read one byte back on the SDA line (used for the temperature sensor). */
static uint8_t __read_byte(DISP_SSD2683_DEV_T *dev)
{
    uint8_t              value = 0;
    TUYA_GPIO_LEVEL_E    level;
    TUYA_GPIO_BASE_CFG_T in_cfg = {
        .mode   = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level  = TUYA_GPIO_LEVEL_HIGH,
    };
    TUYA_GPIO_BASE_CFG_T out_cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };

    tkl_gpio_init(dev->sda_pin, &in_cfg);

    tkl_gpio_write(dev->cs_pin, TUYA_GPIO_LEVEL_HIGH);
    tkl_gpio_write(dev->dc_pin, TUYA_GPIO_LEVEL_HIGH);
    tkl_gpio_write(dev->cs_pin, TUYA_GPIO_LEVEL_LOW);

    for (uint8_t n = 0; n < 8; n++) {
        tkl_gpio_write(dev->clk_pin, TUYA_GPIO_LEVEL_LOW);
        tkl_gpio_read(dev->sda_pin, &level);
        value = (uint8_t)((value << 1) | (level == TUYA_GPIO_LEVEL_HIGH ? 1 : 0));
        tkl_gpio_write(dev->clk_pin, TUYA_GPIO_LEVEL_HIGH);
        tkl_gpio_write(dev->clk_pin, TUYA_GPIO_LEVEL_LOW);
    }

    tkl_gpio_init(dev->sda_pin, &out_cfg); /* restore SDA as output */
    return value;
}

/* Busy level that means "ready". Both the vendor reference and the working
 * ESP32-S3 driver for this exact panel poll until BUSY is HIGH (LOW = operation
 * in progress, HIGH = done). */
#define SSD2683_BUSY_READY_LV TUYA_GPIO_LEVEL_HIGH
/* Max time to wait for one ready transition. Kept short so a wrong polarity
 * guess degrades to fixed delays instead of freezing for minutes. */
#define SSD2683_BUSY_TIMEOUT_MS 2000

/* Wait until BUSY reports ready (see SSD2683_BUSY_READY_LV). [PERF] tag + measured
 * wall time so each individual BUSY wait shows up separately in the log. */
static void __wait_busy_tag(DISP_SSD2683_DEV_T *dev, const char *tag)
{
    TUYA_GPIO_LEVEL_E level   = SSD2683_BUSY_READY_LV;
    uint32_t          timeout = 0;
    SYS_TIME_T        t0      = tal_system_get_millisecond();

    if (dev->busy_pin >= TUYA_GPIO_NUM_MAX) {
        __delay_ms(100);
        PERF_LOG("[PERF] busy(%s) no-pin fixed=%u ms", tag, (unsigned)(tal_system_get_millisecond() - t0));
        return;
    }

    while (timeout < SSD2683_BUSY_TIMEOUT_MS) {
        tkl_gpio_read(dev->busy_pin, &level);
        if (level == SSD2683_BUSY_READY_LV) {
            PERF_LOG("[PERF] busy(%s)=%u ms", tag, (unsigned)(tal_system_get_millisecond() - t0));
            return;
        }
        __delay_ms(2);
        timeout += 2;
    }
    PR_WARN("SSD2683: BUSY timeout (%s, pin level=%d, expected ready=%d) waited=%u ms", tag, level,
            SSD2683_BUSY_READY_LV, (unsigned)(tal_system_get_millisecond() - t0));
}

#define __wait_busy(dev) __wait_busy_tag((dev), __func__)

/* RST: HIGH(10ms) -> LOW(20ms) -> HIGH(10ms) */
static void __reset(DISP_SSD2683_DEV_T *dev)
{
    if (dev->rst_pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }
    tkl_gpio_write(dev->rst_pin, TUYA_GPIO_LEVEL_HIGH);
    __delay_ms(10);
    tkl_gpio_write(dev->rst_pin, TUYA_GPIO_LEVEL_LOW);
    __delay_ms(20);
    tkl_gpio_write(dev->rst_pin, TUYA_GPIO_LEVEL_HIGH);
    __delay_ms(10);
}

/*****************************************************************************
 * EPD operations
 *****************************************************************************/

/* Reverse bits in a byte (LVGL packs monochrome LSB-first; panel expects MSB-first) */
static inline uint8_t __reverse_bits(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

/* Interleave one "old" byte and one "new" byte (8 mono pixels each) into the two
 * 2-bit-per-pixel bytes the panel expects for a fast/partial refresh: each pixel
 * becomes (old<<1 | new). out0 carries pixels 0-3, out1 carries pixels 4-7.
 * Faithful port of the vendor reference bitInterleave() (docs/4D2_BW_SSD2683...). */
static void __bit_interleave(uint8_t old_b, uint8_t new_b, uint8_t *out0, uint8_t *out1)
{
    uint16_t result = 0;

    for (uint8_t i = 0; i < 8; i++) {
        result |= (uint16_t)(((old_b >> (7 - i)) & 0x01) << (2 * (7 - i) + 1));
        result |= (uint16_t)(((new_b >> (7 - i)) & 0x01) << (2 * (7 - i)));
    }

    *out0 = (uint8_t)(result >> 8); /* pixels 0-3 */
    *out1 = (uint8_t)(result);      /* pixels 4-7 */
}

/* Convert the LVGL monochrome frame buffer to the panel-domain 1bpp buffer. */
static void __epd_fb_convert(TDL_DISP_FRAME_BUFF_T *src_fb, TDL_DISP_FRAME_BUFF_T *dst_fb)
{
    uint32_t src_width_bytes = 0, dst_width_bytes = 0;

    if (NULL == src_fb || NULL == dst_fb) {
        PR_ERR("SSD2683: frame buffer is NULL");
        return;
    }

    src_width_bytes = (src_fb->width + 7) / 8;
    dst_width_bytes = (dst_fb->width + 7) / 8;

    for (uint16_t j = 0; j < dst_fb->height; j++) {
        for (uint16_t i = 0; i < dst_width_bytes; i++) {
            uint8_t src_byte = 0xFF;

            if (i < src_width_bytes && j < src_fb->height) {
                src_byte = src_fb->frame[j * src_width_bytes + i];
                src_byte = ~__reverse_bits(src_byte); /* flip here if colors are inverted */
            }

            dst_fb->frame[j * dst_width_bytes + i] = src_byte;
        }
    }
}

/* Initialize OTP / panel settings (after reset). */
static void __epd_init(DISP_SSD2683_DEV_T *dev)
{
    __reset(dev);
    __wait_busy(dev);

    __write_cmd(dev, SSD2683_PANEL_SETTING); /* 0x00 */
    __write_data(dev, 0x2F);
    __write_data(dev, 0x0E);

    __write_cmd(dev, SSD2683_BOOST_OTP); /* 0xE9 */
    __write_data(dev, 0x01);
    __wait_busy(dev);

    dev->is_sleeping = false;
    dev->hot         = false; /* RST left the booster off; the refresh's 0x04 re-powers it */
}

/* Map the on-chip temperature reading to the booster soft-start value. */
static uint8_t __temp_to_value(uint8_t temp)
{
    if (temp <= 5)
        return 232; /* -24 */
    else if (temp <= 10)
        return 235; /* -21 */
    else if (temp <= 20)
        return 238; /* -18 */
    else if (temp <= 30)
        return 241; /* -15 */
    else if (temp <= 127)
        return 244; /* -12 */
    return 232;
}

/* Border / cascade setup for a fast (non-flashing) refresh. Mirrors the vendor
 * tempetrue_value(): VCOM/border = 0x77, cascade = 0x00, then activate temperature. */
static void __epd_temp_fast(DISP_SSD2683_DEV_T *dev)
{
    __write_cmd(dev, SSD2683_VCOM_SETTING); /* 0x50 */
    __write_data(dev, 0x77);

    __write_cmd(dev, SSD2683_CASCADE_SETTING); /* 0xE0 */
    __write_data(dev, 0x00);

    __write_cmd(dev, SSD2683_ACT_TEMP); /* 0xA5 */
    __wait_busy(dev);
    __delay_ms(10);
}

// Re-read the on-chip temperature at most every TEMP_RECACHE_MS; the panel temperature
// drifts slowly, so the de-ghost full refresh reuses the cached booster value instead of
// paying the 0x40 read + BUSY wait every time. Reads on the first call too.
#define TEMP_RECACHE_MS (5 * 60 * 1000)
static void __epd_update_temp_if_due(DISP_SSD2683_DEV_T *dev)
{
    uint32_t now = (uint32_t)tal_system_get_millisecond();
    uint8_t  temp;

    if (dev->temp_valid && (now - dev->temp_read_ms) < TEMP_RECACHE_MS) {
        return;
    }

    __write_cmd(dev, SSD2683_TEMP_SENSOR_CMD); /* 0x40 */
    __wait_busy(dev);
    temp               = __read_byte(dev);
    dev->temp_value    = __temp_to_value(temp);
    dev->temp_read_ms  = now;
    dev->temp_valid    = true;
    PERF_LOG("[PERF] ssd2683 temp read=%d -> tempvalue=%d", (int)temp, (int)dev->temp_value);
}

/* Power the booster on (0x04). Marks the panel hot (ready for an immediate refresh). */
static void __epd_power_on(DISP_SSD2683_DEV_T *dev)
{
    __write_cmd(dev, SSD2683_POWER_ON); /* 0x04 */
    __wait_busy_tag(dev, "power_on");
    __delay_ms(10);
    dev->hot = true;
}

/* Trigger the display refresh waveform (0x12) and wait for it to complete. */
static void __epd_refresh_trigger(DISP_SSD2683_DEV_T *dev)
{
    __write_cmd(dev, SSD2683_DISPLAY_REFRESH); /* 0x12 */
    __write_data(dev, 0x00);
    __delay_ms(10);
    __wait_busy_tag(dev, "refresh_wave"); // <-- the actual e-paper waveform
}

/* Power the booster off (0x02), release CS / the EPD rail, and mark the panel cold so
 * the next refresh does a full reset + OTP re-init + temperature activation. */
static void __epd_power_off(DISP_SSD2683_DEV_T *dev)
{
    __write_cmd(dev, SSD2683_POWER_OFF); /* 0x02 */
    __write_data(dev, 0x00);
    __wait_busy_tag(dev, "power_off");
    __delay_ms(20);

    tkl_gpio_write(dev->cs_pin, TUYA_GPIO_LEVEL_HIGH);
    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(dev->power.pin, dev->power.active_level ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH);
    }
    dev->is_sleeping = true;
    dev->hot         = false;
}

/* Full (de-ghost) refresh from the panel-domain 1bpp buffer. The caller has just reset
 * the panel (required for a clean de-ghost), so this runs the cold cycle: load the
 * de-ghost LUT with the cached temperature, write the whole frame, power-cycle for the
 * flashing waveform. Ends cold so the next partial reloads the fast LUT post-reset (no
 * risky hot 0xA5). */
static void __epd_display_full(DISP_SSD2683_DEV_T *dev)
{
    uint32_t i;

    /* Load the de-ghost waveform LUT with the (cached) temperature compensation value. */
    __epd_update_temp_if_due(dev);
    __write_cmd(dev, SSD2683_CASCADE_SETTING); /* 0xE0 */
    __write_data(dev, 0x02);
    __write_cmd(dev, SSD2683_FORCE_TEMP); /* 0xE6 */
    __write_data(dev, dev->temp_value);
    __write_cmd(dev, SSD2683_ACT_TEMP); /* 0xA5 */
    __wait_busy(dev);
    __delay_ms(10);
    dev->lut_is_fast = false; /* de-ghost LUT now loaded; next partial reloads the fast LUT */

    /* Image data: each 1bpp byte is expanded to two 2-bit-per-pixel bytes (whole panel). */
    SYS_TIME_T t_tx0 = tal_system_get_millisecond();
    __write_cmd(dev, SSD2683_DATA_START_TX); /* 0x10 */
    for (i = 0; i < dev->fb->len; i++) {
        uint8_t data         = dev->fb->frame[i];
        uint8_t combin_byte0 = 0, combin_byte1 = 0;

        for (uint8_t b = 0; b < 8; b++) {
            if (b < 4) {
                combin_byte0 |= (uint8_t)(((data >> (7 - b)) & 0x01) << (8 - 2 * (b + 1)));
            } else {
                combin_byte1 |= (uint8_t)(((data >> (7 - b)) & 0x01) << (14 - 2 * b));
            }
        }
        __write_data(dev, combin_byte0);
        __write_data(dev, combin_byte1);
    }
    SYS_TIME_T t_tx1 = tal_system_get_millisecond();

    __epd_power_on(dev);     /* 0x04 */
    __epd_refresh_trigger(dev); /* 0x12 */
    __epd_power_off(dev);    /* 0x02 -> ends cold */
    PERF_LOG("[PERF] ssd2683 full: sw_spi_tx=%u ms (%u bytes) wave+busy=%u ms", (unsigned)(t_tx1 - t_tx0),
              (unsigned)(dev->fb->len * 2), (unsigned)(tal_system_get_millisecond() - t_tx1));
}

/* Byte-aligned bounding box of the bytes that differ between old_fb (on screen) and
 * fb (new frame). Returns FALSE if the two frames are identical. x/w land on 8-px
 * (1-byte) boundaries; y/h are per gate line. Mirrors the ESP32-S3 zectrix
 * analyze_frame_diff(). */
static BOOL_T __analyze_diff(DISP_SSD2683_DEV_T *dev, uint16_t *out_x, uint16_t *out_y, uint16_t *out_w,
                            uint16_t *out_h)
{
    const int bpr    = (dev->width + 7) / 8; /* bytes per row, 1bpp (400 -> 50) */
    int       min_bx = bpr, max_bx = -1;
    int       min_y = dev->height, max_y = -1;
    int       y, xb;

    for (y = 0; y < dev->height; y++) {
        const uint8_t *po = dev->old_fb->frame + y * bpr;
        const uint8_t *pn = dev->fb->frame + y * bpr;
        for (xb = 0; xb < bpr; xb++) {
            if (po[xb] != pn[xb]) {
                if (xb < min_bx) {
                    min_bx = xb;
                }
                if (xb > max_bx) {
                    max_bx = xb;
                }
                if (y < min_y) {
                    min_y = y;
                }
                if (y > max_y) {
                    max_y = y;
                }
            }
        }
    }

    if (max_bx < 0) {
        return FALSE; /* identical frames */
    }

    *out_x = (uint16_t)(min_bx * 8);
    *out_w = (uint16_t)((max_bx - min_bx + 1) * 8);
    if (*out_x + *out_w > dev->width) {
        *out_w = (uint16_t)(dev->width - *out_x);
    }
    *out_y = (uint16_t)min_y;
    *out_h = (uint16_t)(max_y - min_y + 1);
    return TRUE;
}

/* Set the SSD2683 partial RAM window (0x83). Coordinates are in pixels; each 10-bit
 * start/end address is sent as [9:8] then [7:0]. Trailing 0x01 selects partial mode.
 * Ref: ESP32-S3 zectrix EPD_SetPartialWindow(). */
static void __epd_set_partial_window(DISP_SSD2683_DEV_T *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t x_start = x;
    uint16_t x_end   = (uint16_t)(x + w - 1);
    uint16_t y_start = y;
    uint16_t y_end   = (uint16_t)(y + h - 1);

    __write_cmd(dev, SSD2683_PARTIAL_WINDOW); /* 0x83 */
    __write_data(dev, (uint8_t)((x_start >> 8) & 0x03));
    __write_data(dev, (uint8_t)(x_start & 0xFF));
    __write_data(dev, (uint8_t)((x_end >> 8) & 0x03));
    __write_data(dev, (uint8_t)(x_end & 0xFF));
    __write_data(dev, (uint8_t)((y_start >> 8) & 0x03));
    __write_data(dev, (uint8_t)(y_start & 0xFF));
    __write_data(dev, (uint8_t)((y_end >> 8) & 0x03));
    __write_data(dev, (uint8_t)(y_end & 0xFF));
    __write_data(dev, 0x01);
}

/* Partial (windowed, non-flashing) refresh: only the changed bounding box is sent and
 * scanned, so both the bit-bang payload and the gate-scan waveform shrink. Drives each
 * pixel from its old->new transition (old_fb -> fb). Ref: ESP32-S3 zectrix
 * EPD_DisplayPart().
 *
 * Hot keep-alive: the panel stays powered (dev->hot) so we skip the 0x04 power-on and do
 * NOT power off afterwards. The fast (non-flashing) waveform LUT also stays loaded across
 * partials, so we skip the temperature activation (0xE0/0xA5) too — UNLESS a de-ghost full
 * just ran (lut_is_fast cleared), in which case we reload the fast LUT. This removes the
 * per-frame reset + ~270ms power-cycle the user was seeing. */
static void __epd_display_partial(DISP_SSD2683_DEV_T *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    const int    bpr            = (dev->width + 7) / 8;
    const int    x_byte_start   = x / 8;
    const int    bytes_per_line = (w + 7) / 8;
    const BOOL_T was_hot        = dev->hot ? TRUE : FALSE;
    const BOOL_T need_fast_lut  = (!dev->hot || !dev->lut_is_fast) ? TRUE : FALSE;
    int          row, j;

    if (need_fast_lut) {
        __epd_temp_fast(dev); /* 0x50 / 0xE0=0x00 / 0xA5: load the fast (non-flashing) LUT */
        dev->lut_is_fast = true;
    } else {
        /* Fast LUT already loaded + powered: just refresh the border setting. */
        __write_cmd(dev, SSD2683_VCOM_SETTING); /* 0x50 */
        __write_data(dev, 0x77);
    }
    __epd_set_partial_window(dev, x, y, w, h);

    /* Image data: interleave old + new into 2-bit-per-pixel codes, windowed region only. */
    SYS_TIME_T t_tx0 = tal_system_get_millisecond();
    __write_cmd(dev, SSD2683_DATA_START_TX); /* 0x10 */
    __wait_busy(dev);
    for (row = y; row < y + h; row++) {
        const uint8_t *po = dev->old_fb->frame + row * bpr + x_byte_start;
        const uint8_t *pn = dev->fb->frame + row * bpr + x_byte_start;
        for (j = 0; j < bytes_per_line; j++) {
            uint8_t combin_byte0 = 0, combin_byte1 = 0;
            __bit_interleave(po[j], pn[j], &combin_byte0, &combin_byte1);
            __write_data(dev, combin_byte0);
            __write_data(dev, combin_byte1);
        }
    }
    SYS_TIME_T t_tx1 = tal_system_get_millisecond();

    if (!was_hot) {
        __epd_power_on(dev); /* cold (first / after a de-ghost full): 0x04 power the booster on */
    }
    __epd_refresh_trigger(dev); /* 0x12 */
    /* Keep powered: no 0x02, so dev->hot stays TRUE for the next partial. */

    PERF_LOG("[PERF] ssd2683 partial %s win=(%u,%u %ux%u): sw_spi_tx=%u ms (%u bytes) wave+busy=%u ms",
              need_fast_lut ? "lut-reload" : "hot", x, y, w, h, (unsigned)(t_tx1 - t_tx0),
              (unsigned)(bytes_per_line * 2 * h), (unsigned)(tal_system_get_millisecond() - t_tx1));
}

static void __epd_wake(DISP_SSD2683_DEV_T *dev)
{
    if (!dev->is_sleeping) {
        return;
    }

    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(dev->power.pin, dev->power.active_level);
        __delay_ms(100);
    }

    __epd_init(dev);
}

static void __epd_sleep(DISP_SSD2683_DEV_T *dev)
{
    if (dev->is_sleeping) {
        return;
    }

    if (dev->hot) {
        __epd_power_off(dev); /* the panel runs hot now: drop the booster before deep sleep */
    }

    __write_cmd(dev, SSD2683_DEEP_SLEEP); /* 0x07 */
    __write_data(dev, 0xA5);
    tkl_gpio_write(dev->cs_pin, TUYA_GPIO_LEVEL_HIGH);
    __delay_ms(100);

    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(dev->power.pin, dev->power.active_level ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH);
        __delay_ms(10);
    }

    dev->is_sleeping = true;
    dev->hot         = false;
}

/* Configure all control GPIO as push-pull outputs (CLK/SDA/CS/DC/RST) + BUSY input. */
static void __epd_gpio_init(DISP_SSD2683_DEV_T *dev)
{
    TUYA_GPIO_BASE_CFG_T out_cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = TUYA_GPIO_LEVEL_HIGH,
    };
    TUYA_GPIO_BASE_CFG_T in_cfg = {
        .mode   = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level  = TUYA_GPIO_LEVEL_HIGH,
    };

    tkl_gpio_init(dev->clk_pin, &out_cfg);
    tkl_gpio_init(dev->cs_pin, &out_cfg);
    out_cfg.level = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(dev->sda_pin, &out_cfg);
    tkl_gpio_init(dev->dc_pin, &out_cfg);
    if (dev->rst_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_init(dev->rst_pin, &out_cfg);
    }
    if (dev->busy_pin < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_init(dev->busy_pin, &in_cfg);
    }

#if SSD2683_FAST_GPIO
    /* Cache the hot pins' registers + their non-output config bits now that
     * tkl_gpio_init() has configured CLK/SDA as push-pull outputs. */
    dev->clk_reg  = __gpio_cfg_reg(dev->clk_pin);
    dev->sda_reg  = __gpio_cfg_reg(dev->sda_pin);
    dev->clk_word = (*dev->clk_reg) & ~SSD2683_GPIO_OUT_BIT;
    dev->sda_word = (*dev->sda_reg) & ~SSD2683_GPIO_OUT_BIT;
#endif
}

/*****************************************************************************
 * TDD Driver Interface
 *****************************************************************************/

static OPERATE_RET __tdd_disp_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_SSD2683_DEV_T *dev = (DISP_SSD2683_DEV_T *)device;
    uint32_t            frame_len;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    if (dev->power.pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T pwr_cfg = {
            .mode   = TUYA_GPIO_PUSH_PULL,
            .direct = TUYA_GPIO_OUTPUT,
            .level  = dev->power.active_level,
        };
        tkl_gpio_init(dev->power.pin, &pwr_cfg);
        __delay_ms(50);
    }

    __epd_gpio_init(dev);
    __delay_ms(10);

    dev->is_sleeping = true;
    __epd_wake(dev);

    frame_len = ((dev->width + 7) / 8) * dev->height;
    dev->fb   = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == dev->fb) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev->fb->frame, 0xFF, frame_len);
    dev->fb->fmt    = TUYA_PIXEL_FMT_MONOCHROME;
    dev->fb->width  = dev->width;
    dev->fb->height = dev->height;
    dev->fb->len    = frame_len;

    /* Old-frame mirror used by the fast (non-flashing) refresh path */
    dev->old_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == dev->old_fb) {
        tdl_disp_free_frame_buff(dev->fb);
        dev->fb = NULL;
        return OPRT_MALLOC_FAILED;
    }
    dev->old_fb->len = frame_len;

    /* Clear to white on power-up with a full refresh, then sync old<-new so the
     * first fast refresh diffs against the correct on-screen content. */
    dev->is_use_partial = false;
    dev->partial_count  = 0;
    __epd_display_full(dev);
    memcpy(dev->old_fb->frame, dev->fb->frame, frame_len);

    PR_NOTICE("SSD2683: initialized (%dx%d, full_refresh_period=%d)", dev->width, dev->height,
              dev->full_refresh_period);
    return OPRT_OK;
}

static OPERATE_RET __tdd_disp_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_SSD2683_DEV_T *dev = (DISP_SSD2683_DEV_T *)device;

    if (NULL == device || NULL == frame_buff || NULL == dev->fb) {
        return OPRT_INVALID_PARM;
    }

    // [PERF] cvt = 1bpp->2bpp CPU pack; refresh = the panel sequence (window/data + the
    // BUSY-wait waveform). Wake/reset only happens on a cold refresh now (see below).
    SYS_TIME_T t_cvt0 = tal_system_get_millisecond();
    __epd_fb_convert(frame_buff, dev->fb); /* CPU only, no panel IO */
    SYS_TIME_T t_ref0 = tal_system_get_millisecond();

    /* First frame, "always full" config, or the periodic de-ghost frame -> full refresh;
     * otherwise a windowed partial refresh of just the changed bounding box (diff old_fb
     * vs fb). Identical frames refresh nothing.
     *
     * Partial refreshes keep the panel powered (hot) between frames, so __epd_wake() is a
     * no-op (no reset) and the partial skips the power-cycle. The de-ghost full refresh
     * always runs cold: force the panel back to a clean reset first. */
    const char *mode = "partial";
    BOOL_T      do_full =
        (!dev->is_use_partial || dev->full_refresh_period == 0 || dev->partial_count >= dev->full_refresh_period);
    uint16_t dx = 0, dy = 0, dw = 0, dh = 0;
    BOOL_T   has_diff = FALSE;

    if (!do_full) {
        has_diff = __analyze_diff(dev, &dx, &dy, &dw, &dh);
    }

    if (do_full) {
        mode = "FULL";
        /* The de-ghost full refresh needs a clean power-off + reset + re-init first (the
         * panel reloads the full waveform LUT from a known state). Partials reuse the
         * loaded fast LUT, so they keep running hot without a reset. */
        if (dev->hot) {
            __epd_power_off(dev); /* graceful 0x02 before the reset */
        }
        __epd_wake(dev);         /* reset + OTP init */
        __epd_display_full(dev); /* loads de-ghost LUT, then 0x04 / 0x12 / 0x02 (ends cold) */
        dev->partial_count  = 0;
        dev->is_use_partial = true;
    } else if (has_diff) {
        if (!dev->hot) {
            __epd_wake(dev); /* first refresh after a cold start only */
        }
        __epd_display_partial(dev, dx, dy, dw, dh);
        dev->partial_count++;
    } else {
        mode = "skip"; /* nothing changed on the panel */
    }
    PERF_LOG("[PERF] ssd2683 %s refresh: cvt=%u refresh=%u ms (total=%u)", mode, (unsigned)(t_ref0 - t_cvt0),
              (unsigned)(tal_system_get_millisecond() - t_ref0), (unsigned)(tal_system_get_millisecond() - t_cvt0));

    /* old_fb now mirrors what is on screen */
    memcpy(dev->old_fb->frame, dev->fb->frame, dev->fb->len);

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_disp_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_SSD2683_DEV_T *dev = (DISP_SSD2683_DEV_T *)device;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    __epd_sleep(dev);

    if (dev->fb) {
        tdl_disp_free_frame_buff(dev->fb);
        dev->fb = NULL;
    }
    if (dev->old_fb) {
        tdl_disp_free_frame_buff(dev->old_fb);
        dev->old_fb = NULL;
    }

    return OPRT_OK;
}

/*****************************************************************************
 * Public API
 *****************************************************************************/

OPERATE_RET tdd_disp_sw_spi_mono_ssd2683_register(char *name, DISP_EINK_SSD2683_CFG_T *dev_cfg)
{
    OPERATE_RET         rt       = OPRT_OK;
    DISP_SSD2683_DEV_T *disp_dev = NULL;

    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    disp_dev = (DISP_SSD2683_DEV_T *)tal_malloc(sizeof(DISP_SSD2683_DEV_T));
    if (NULL == disp_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(disp_dev, 0, sizeof(DISP_SSD2683_DEV_T));

    disp_dev->width               = dev_cfg->width;
    disp_dev->height              = dev_cfg->height;
    disp_dev->clk_pin             = dev_cfg->clk_pin;
    disp_dev->sda_pin             = dev_cfg->sda_pin;
    disp_dev->cs_pin              = dev_cfg->cs_pin;
    disp_dev->dc_pin              = dev_cfg->dc_pin;
    disp_dev->rst_pin             = dev_cfg->rst_pin;
    disp_dev->busy_pin            = dev_cfg->busy_pin;
    disp_dev->power               = dev_cfg->power;
    disp_dev->is_sleeping         = true;
    disp_dev->full_refresh_period = dev_cfg->full_refresh_period;
    disp_dev->is_use_partial      = false;
    disp_dev->partial_count       = 0;

    TDD_DISP_DEV_INFO_T disp_dev_info = {
        .type     = TUYA_DISPLAY_SPI,
        .width    = dev_cfg->width,
        .height   = dev_cfg->height,
        .fmt      = TUYA_PIXEL_FMT_MONOCHROME,
        .rotation = dev_cfg->rotation,
        .is_swap  = true,
        .has_vram = true,
    };
    disp_dev_info.bl.gpio.pin = TUYA_GPIO_NUM_MAX;
    memcpy(&disp_dev_info.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    TDD_DISP_INTFS_T disp_intfs = {
        .open  = __tdd_disp_open,
        .flush = __tdd_disp_flush,
        .close = __tdd_disp_close,
    };

    TUYA_CALL_ERR_GOTO(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_dev, &disp_intfs, &disp_dev_info),
                       err_exit);

    PR_NOTICE("SSD2683: %s registered (%dx%d)", name, dev_cfg->width, dev_cfg->height);
    return OPRT_OK;

err_exit:
    tal_free(disp_dev);
    return rt;
}
