/**
 * @file xteink_x4_pro_epd.c
 * @brief Xteink X4 Pro SSD1677 E-Ink display driver.
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note NEWER X4 PRO BATCHES SWAP THE SSD1677 FOR AN ULTRACHIP PART (UC8179 or
 *       UC8279, per FreeInk XteinkDetect). SSD1677 commands are silently
 *       "accepted" on a write-only SPI bus by those parts but never complete an
 *       update (BUSY stuck forever) — the exact lab symptom. This driver
 *       therefore runs the FreeInk UC81xx bus probe (FLG 0x71 / VER 0x70
 *       half-duplex reads, bit-banged on the EPD GPIOs) before choosing a
 *       backend:
 *         - UC8179 / UC8279 (800x480): OEM sequences ported from FreeInk
 *           Uc8179Driver / Uc8279X4Driver (KW mode, DTM1=OLD / DTM2=NEW, PON /
 *           DRF / POF, BUSY idle-HIGH).
 *         - SSD1677 (probe negative): the X4 BSP incremental power model —
 *           booster AE C7 C3 C0 80, border init 0x80, SWRESET + fixed 10 ms
 *           settle, updates with rails held up between paints (0x22=0xC0|0x34
 *           full, 0x1C fast DU).
 * @note Bus access goes through tkl_spi / tkl_gpio exclusively. CS and DC are
 *       software-controlled GPIOs; the SPI peripheral's own MISO/CS are
 *       disabled by pinmuxing GPIO0 onto them (the adapter maps pin <= 0 to
 *       -1) — the default S3 FSPI MISO=13/CS=10 would collide with the EPD CS
 *       and the GT911 INT pin.
 */
#include "xteink_x4_pro_epd.h"

#include "board_config.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tkl_gpio.h"
#include "tkl_pinmux.h"
#include "tkl_spi.h"
#include "tuya_cloud_types.h"
#include <string.h>

#define CMD_SOFT_RESET            0x12
#define CMD_BOOSTER_SOFT_START    0x0C
#define CMD_DRIVER_OUTPUT_CONTROL 0x01
#define CMD_BORDER_WAVEFORM       0x3C
#define CMD_TEMP_SENSOR_CONTROL   0x18
#define CMD_DATA_ENTRY_MODE       0x11
#define CMD_SET_RAM_X_RANGE       0x44
#define CMD_SET_RAM_Y_RANGE       0x45
#define CMD_SET_RAM_X_COUNTER     0x4E
#define CMD_SET_RAM_Y_COUNTER     0x4F
#define CMD_WRITE_RAM_BW          0x24
#define CMD_WRITE_RAM_RED         0x26
#define CMD_AUTO_WRITE_BW_RAM     0x46
#define CMD_AUTO_WRITE_RED_RAM    0x47
#define CMD_DISPLAY_UPDATE_CTRL1  0x21
#define CMD_DISPLAY_UPDATE_CTRL2  0x22
#define CMD_MASTER_ACTIVATION     0x20
#define CMD_DEEP_SLEEP            0x10

#define CTRL1_NORMAL     0x00
#define CTRL1_BYPASS_RED 0x40

#define TEMP_SENSOR_INTERNAL   0x80
#define DATA_ENTRY_X_INC_Y_DEC 0x01

/* X4 Pro runs the incremental update sequences (see __refresh); SEQ_FULL is
 * kept for reference against the FreeInk vendor config only. */
#define SEQ_FULL 0xF7 /* vendor absolute full waveform (unused here)  */

#define BORDER_WAVEFORM_INIT 0x80
#define BORDER_WAVEFORM_RUN  0xC0
#define POWER_OFF_SEQUENCE   0x03

/* ── UC81xx (UC8179 / UC8279 800x480) command set — OEM sequences ported from
 * FreeInk Uc8179Driver.cpp / Uc8279X4Driver.cpp (via Ghidra RE of the stock
 * X4 Pro firmware). Completely different register map from the SSD1677. */
#define UC_CMD_PANEL_SETTING 0x00 /* PSR                                   */
#define UC_CMD_POWER_OFF     0x02 /* POF                                   */
#define UC_CMD_PFS           0x03 /* power-off sequence timing             */
#define UC_CMD_POWER_ON      0x04 /* PON                                   */
#define UC_CMD_BTST          0x06 /* booster soft-start (UC8179 only)      */
#define UC_CMD_DEEP_SLEEP    0x07 /* DSLP, check code 0xA5                 */
#define UC_CMD_DTM1          0x10 /* OLD plane (KW differential baseline)  */
#define UC_CMD_DISPLAY_REFRESH 0x12 /* DRF                                 */
#define UC_CMD_DTM2          0x13 /* NEW plane                             */
#define UC_CMD_PLL           0x30 /* frame rate (UC8279 only)              */
#define UC_CMD_CDI           0x50 /* VCOM/data interval (UC8179 only)      */
#define UC_CMD_TRES          0x61 /* resolution                            */
#define UC_CMD_GSST          0x65 /* gate/source start (4 bytes)           */
#define UC_CMD_PTIN          0x91 /* partial-in                            */
#define UC_CMD_PTOUT         0x92 /* partial-out                           */
#define UC_CMD_CCSET         0xE0 /* cascade/output enable                 */
#define UC_CMD_GATE_SCAN     0xE1 /* gate-scan selection                   */
#define UC_CMD_POWER_SAVE    0xE3 /* VCOM/source line periods (UC8179)     */
#define UC_CMD_TSSET         0xE5 /* forced temperature / frame-rate lever */

/* UC81xx boot probe registers (XteinkDetect port): */
#define UC_CMD_VER           0x70 /* reserved + CHIP_VER + LUT_VER[23:0]   */
#define UC_CMD_FLG           0x71 /* status; BUSY_N (D0) = 1 when idle     */
#define UC_CMD_RMTP          0xA2 /* bulk MTP read: 1 dummy byte + MTP[]   */

/* UC8179 vs UC8279 per the vendor reference: VER byte2 (LUT_VER) 0x02/0x68/
 * 0x69 = UC8279 800x480 variant; anything else (observed 0x01) = UC8179. */
#define UC_LUT_VER_UC8279_A  0x02
#define UC_LUT_VER_UC8279_B  0x68
#define UC_LUT_VER_UC8279_C  0x69

#define UC_TRES_HEIGHT       600U /* panel addressed 800x600 (480 visible) */
#define UC8279_GATE_OFFSET   120U /* visible gates start at 120 on UC8279  */

#define X4PRO_EPD_WIDTH_BYTES (X4PRO_EPD_WIDTH / 8U)
#define X4PRO_EPD_BUF_SIZE    (X4PRO_EPD_WIDTH_BYTES * X4PRO_EPD_HEIGHT)
#define X4PRO_EPD_SPI_CHUNK   4092U /* tkl_spi DMA bounce limit (4092) */

/* Bounded busy waits: a healthy DU update finishes in ~0.1-0.3 s and the OTP
 * full waveform in ~2 s. Anything longer is a stalled panel — fail fast so the
 * caller can recover instead of freezing the UI thread for half a minute. */
#define X4PRO_EPD_BUSY_TIMEOUT_FAST_MS 8000U
#define X4PRO_EPD_BUSY_TIMEOUT_FULL_MS 15000U

#define TAG "x4pro_epd"

typedef enum {
    EPD_CTRL_SSD1677 = 0, /* probe negative — original Solomon part        */
    EPD_CTRL_UC8179,      /* UltraChip batch, LUT_VER != 0x02/0x68/0x69    */
    EPD_CTRL_UC8279,      /* UltraChip batch, 800x480 variant              */
} EPD_CTRL_T;

static BOOL_T    s_epd_inited   = FALSE;
static BOOL_T    s_prev_valid   = FALSE;
static BOOL_T    s_screen_on    = FALSE; /* analog/clock rails up between updates */
static EPD_CTRL_T s_controller  = EPD_CTRL_SSD1677;
static uint8_t   s_prev_frame[X4PRO_EPD_BUF_SIZE];

/**
 * @brief Initialize one output GPIO.
 * @param[in] pin GPIO number.
 * @param[in] level initial level.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __gpio_output_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = level;

    return tkl_gpio_init(pin, &cfg);
}

/**
 * @brief Initialize one input GPIO (BUSY, active-high = default pulled down).
 * @param[in] pin GPIO number.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __gpio_input_init(TUYA_GPIO_NUM_E pin)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_FLOATING;
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.level  = TUYA_GPIO_LEVEL_LOW;

    return tkl_gpio_init(pin, &cfg);
}

/* ── UC81xx controller probe (port of FreeInk XteinkDetect) ─────────────────
 * Newer X4 Pro batches carry a UC8179/UC8279 instead of the SSD1677. The two
 * silicon families are identified by half-duplex register reads (FLG 0x71 /
 * VER 0x70) that only the UC81xx answers; an SSD1677 leaves the line floating.
 * Runs BEFORE the SPI bus is claimed, bit-banged on the EPD GPIOs. */

/**
 * @brief Coarse microsecond spin delay for the probe clock.
 * @param[in] us microseconds to delay (minimum granularity, may run slower).
 */
static void __probe_delay_us(uint32_t us)
{
    /* The UC81xx ID readback tolerates a slower-than-spec clock, so a
     * conservative spin (240 MHz -> ~12 iters/us) is timing-safe. */
    volatile uint32_t i;

    while (us-- > 0U) {
        for (i = 0; i < 12U; i++) {
        }
    }
}

/**
 * @brief Drive one probe pin as a push-pull output.
 */
static OPERATE_RET __probe_gpio_out(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = level;

    return tkl_gpio_init(pin, &cfg);
}

/**
 * @brief Release one probe pin as an input (pull-up for the bidirectional
 *        MOSI so a floating bus reads 0xFF instead of noise).
 */
static OPERATE_RET __probe_gpio_in(TUYA_GPIO_NUM_E pin, BOOL_T pullup)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = pullup ? TUYA_GPIO_PULLUP : TUYA_GPIO_FLOATING;
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.level  = pullup ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW;

    return tkl_gpio_init(pin, &cfg);
}

/**
 * @brief Bit-bang one byte out on SCLK/MOSI, MSB first.
 */
static void __probe_write_byte(uint8_t b)
{
    uint8_t i;

    for (i = 0; i < 8U; i++) {
        (void)tkl_gpio_write(X4PRO_EPD_PIN_MOSI, (b & 0x80U) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
        __probe_delay_us(1);
        (void)tkl_gpio_write(X4PRO_EPD_PIN_SCLK, TUYA_GPIO_LEVEL_HIGH);
        __probe_delay_us(1);
        (void)tkl_gpio_write(X4PRO_EPD_PIN_SCLK, TUYA_GPIO_LEVEL_LOW);
        b = (uint8_t)(b << 1);
    }
}

/**
 * @brief Bit-bang one byte in from MOSI. The controller shifts the next bit
 *        out on the SCL falling edge, so sample while the clock is low.
 */
static uint8_t __probe_read_byte(void)
{
    uint8_t           b = 0;
    uint8_t           i;
    TUYA_GPIO_LEVEL_E level;

    for (i = 0; i < 8U; i++) {
        __probe_delay_us(1);
        (void)tkl_gpio_read(X4PRO_EPD_PIN_MOSI, &level);
        b = (uint8_t)((b << 1) | ((level == TUYA_GPIO_LEVEL_HIGH) ? 1U : 0U));
        (void)tkl_gpio_write(X4PRO_EPD_PIN_SCLK, TUYA_GPIO_LEVEL_HIGH);
        __probe_delay_us(1);
        (void)tkl_gpio_write(X4PRO_EPD_PIN_SCLK, TUYA_GPIO_LEVEL_LOW);
    }

    return b;
}

/**
 * @brief One command + N-byte half-duplex read: command with DC low, then
 *        MOSI released to input with DC high while the controller drives.
 */
static void __probe_cmd_read(uint8_t cmd, uint8_t *out, uint8_t len)
{
    uint8_t i;

    (void)__probe_gpio_out(X4PRO_EPD_PIN_MOSI, TUYA_GPIO_LEVEL_LOW);
    (void)tkl_gpio_write(X4PRO_EPD_PIN_DC, TUYA_GPIO_LEVEL_LOW);
    (void)tkl_gpio_write(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_LOW);
    __probe_delay_us(1);
    __probe_write_byte(cmd);
    (void)tkl_gpio_write(X4PRO_EPD_PIN_DC, TUYA_GPIO_LEVEL_HIGH);
    (void)__probe_gpio_in(X4PRO_EPD_PIN_MOSI, TRUE);
    __probe_delay_us(1);
    for (i = 0; i < len; i++) {
        out[i] = __probe_read_byte();
    }
    (void)tkl_gpio_write(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH);
    (void)__probe_gpio_out(X4PRO_EPD_PIN_MOSI, TUYA_GPIO_LEVEL_LOW);
}

/**
 * @brief A released SDA reads back all-0x00 or all-0xFF through the pull-up;
 *        any variation in the five VER bytes means a real, driven response.
 */
static BOOL_T __probe_ver_is_floating(const uint8_t ver[5])
{
    int i;

    for (i = 1; i < 5; i++) {
        if (ver[i] != ver[0]) {
            return FALSE; /* any variation => a real, driven response */
        }
    }
    return TRUE;
}

/**
 * @brief One probe pass: reset pulse, FLG + VER read, UC81xx signature match.
 * @param[out] ver 5 VER bytes.
 * @param[out] flg FLG status byte (optional).
 * @param[in] rst_low_ms reset pulse width (1 ms screening / 50 ms doc timing).
 * @return TRUE when the part answers with the UC81xx signature.
 */
static BOOL_T __probe_pass(uint8_t ver[5], uint8_t *flg, uint32_t rst_low_ms)
{
    uint8_t flg_byte = 0;

    (void)__probe_gpio_out(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH);
    (void)__probe_gpio_out(X4PRO_EPD_PIN_SCLK, TUYA_GPIO_LEVEL_LOW);
    (void)__probe_gpio_out(X4PRO_EPD_PIN_DC, TUYA_GPIO_LEVEL_LOW);
    (void)__probe_gpio_out(X4PRO_EPD_PIN_MOSI, TUYA_GPIO_LEVEL_LOW);
    (void)__probe_gpio_in(X4PRO_EPD_PIN_BUSY, FALSE);

    /* BUSY polarity is unknown here (the controller is what we're trying to
     * identify), so a flat delay covers every UC81xx power-up instead of
     * gating on BUSY. The backend resets again afterwards. */
    (void)__probe_gpio_out(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(2);
    (void)tkl_gpio_write(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(rst_low_ms);
    (void)tkl_gpio_write(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(30);

    __probe_cmd_read(UC_CMD_FLG, &flg_byte, 1);
    __probe_cmd_read(UC_CMD_VER, ver, 5);
    if (NULL != flg) {
        *flg = flg_byte;
    }

    /* FLG must be a real, non-floating status with BUSY_N (bit0) asserted
     * (idle); VER must be an actually-driven (non-uniform) pattern. */
    if (flg_byte == 0x00U || flg_byte == 0xFFU) {
        return FALSE;
    }
    if ((flg_byte & 0x01U) != 0x01U) {
        return FALSE;
    }
    return !__probe_ver_is_floating(ver);
}

/**
 * @brief Two-pass probe with agreement; pick the controller backend.
 * @return EPD_CTRL_UC8179 / EPD_CTRL_UC8279 on a confirmed UltraChip part,
 *         EPD_CTRL_SSD1677 otherwise (the profile default).
 */
static EPD_CTRL_T __probe_controller(void)
{
    uint8_t  ver1[5]   = {0};
    uint8_t  ver2[5]   = {0};
    uint8_t  flg1      = 0;
    BOOL_T   confirmed = FALSE;
    BOOL_T   pass1;
    BOOL_T   pass2;
    uint8_t  lut_ver;

    pass1 = __probe_pass(ver1, &flg1, 1U);
    tal_system_sleep(2);
    pass2 = __probe_pass(ver2, NULL, pass1 ? 50U : 1U);

    confirmed = pass1 && pass2 && ((BOOL_T)(memcmp(ver1, ver2, 5) == 0));

    /* Field-observed fallback: some UC parts return a blank/unreadable
     * LUT_VER area (VER = FF FF FF FF FF), which the uniform-floating test
     * wrongly rejects. Require POSITIVE evidence then: the RMTP dump must
     * start with the 0xA5 MTP key, which only a programmed UC part drives. */
    if (!confirmed && pass1 && pass2 && __probe_ver_is_floating(ver1) && ver1[0] == 0xFFU &&
        flg1 != 0x00U && flg1 != 0xFFU && (flg1 & 0x01U) == 0x01U) {
        uint8_t mtp_head[2] = {0}; /* dummy byte + MTP[0] */

        __probe_cmd_read(UC_CMD_RMTP, mtp_head, sizeof(mtp_head));
        if (mtp_head[1] == 0xA5U) {
            confirmed = TRUE;
        }
    }

    /* Release the bus; the backend re-claims it through the SPI pinmux. */
    (void)tkl_gpio_deinit(X4PRO_EPD_PIN_SCLK);
    (void)tkl_gpio_deinit(X4PRO_EPD_PIN_MOSI);
    (void)tkl_gpio_deinit(X4PRO_EPD_PIN_CS);
    (void)tkl_gpio_deinit(X4PRO_EPD_PIN_DC);
    (void)tkl_gpio_deinit(X4PRO_EPD_PIN_RST);
    (void)tkl_gpio_deinit(X4PRO_EPD_PIN_BUSY);

    lut_ver = ver2[2];
    PR_NOTICE("[" TAG "] probe: VER=%02X %02X %02X %02X %02X FLG=0x%02X pass1=%d pass2=%d -> %s",
              ver1[0], ver1[1], ver1[2], ver1[3], ver1[4], (unsigned)flg1, (int)pass1, (int)pass2,
              confirmed ? ((lut_ver == UC_LUT_VER_UC8279_A || lut_ver == UC_LUT_VER_UC8279_B ||
                            lut_ver == UC_LUT_VER_UC8279_C) ? "UC8279" : "UC8179")
                        : "SSD1677 (default)");

    if (!confirmed) {
        return EPD_CTRL_SSD1677;
    }
    if (lut_ver == UC_LUT_VER_UC8279_A || lut_ver == UC_LUT_VER_UC8279_B || lut_ver == UC_LUT_VER_UC8279_C) {
        return EPD_CTRL_UC8279;
    }
    return EPD_CTRL_UC8179;
}

/**
 * @brief Configure the write-only SPI bus through the TuyaOpen wrappers.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __bus_init(void)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_SPI_BASE_CFG_T  cfg;

    /* Route the FSPI signals; pin 0 for MISO/CS marks them unused (-> -1). */
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(X4PRO_EPD_PIN_MOSI, TUYA_SPI0_MOSI));
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(X4PRO_EPD_PIN_SCLK, TUYA_SPI0_CLK));
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(TUYA_GPIO_NUM_0, TUYA_SPI0_MISO));
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(TUYA_GPIO_NUM_0, TUYA_SPI0_CS));

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.role         = TUYA_SPI_ROLE_MASTER;
    cfg.mode         = TUYA_SPI_MODE0;
    cfg.type         = TUYA_SPI_AUTO_TYPE;
    cfg.databits     = TUYA_SPI_DATA_BIT8;
    cfg.bitorder     = TUYA_SPI_ORDER_MSB2LSB;
    cfg.freq_hz      = X4PRO_EPD_SPI_FREQ_HZ;
    cfg.spi_dma_flags = 1; /* RAM writes are 48 kB — DMA mandatory */

    TUYA_CALL_ERR_RETURN(tkl_spi_init(X4PRO_EPD_SPI_PORT, &cfg));

    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4PRO_EPD_PIN_DC, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(__gpio_input_init(X4PRO_EPD_PIN_BUSY));

    return OPRT_OK;
}

/**
 * @brief Send bytes over the configured SPI bus, chunked to the DMA limit.
 * @param[in] data data pointer.
 * @param[in] len data length.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __spi_send(const uint8_t *data, uint32_t len)
{
    OPERATE_RET rt     = OPRT_OK;
    uint32_t    offset = 0;

    if (NULL == data || len == 0U) {
        return OPRT_INVALID_PARM;
    }

    while (offset < len) {
        uint32_t chunk = len - offset;
        if (chunk > X4PRO_EPD_SPI_CHUNK) {
            chunk = X4PRO_EPD_SPI_CHUNK;
        }

        TUYA_CALL_ERR_RETURN(
            tkl_spi_send(X4PRO_EPD_SPI_PORT, (void *)(uintptr_t)(data + offset), chunk));
        offset += chunk;
    }

    return OPRT_OK;
}

/**
 * @brief Send one command byte (DC low).
 * @param[in] cmd command byte.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __send_cmd(uint8_t cmd)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_DC, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(__spi_send(&cmd, 1));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));

    return OPRT_OK;
}

/**
 * @brief Send one data byte (DC high).
 * @param[in] data data byte.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __send_data(uint8_t data)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_DC, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(__spi_send(&data, 1));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));

    return OPRT_OK;
}

/**
 * @brief Send a data buffer while keeping chip-select asserted.
 * @param[in] data data pointer.
 * @param[in] len data length.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __send_data_buf(const uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_DC, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(__spi_send(data, len));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));

    return OPRT_OK;
}

/**
 * @brief Wait until the BUSY pin reaches its idle level.
 * @param[in] timeout_ms maximum time to wait.
 * @param[in] what sequence name, used in the stall diagnostic.
 * @param[in] idle_level idle level of the active controller: SSD1677 is
 *            busy-HIGH (idle LOW); UC81xx parts are busy-LOW (idle HIGH).
 * @return OPRT_OK on idle, timeout error otherwise.
 */
static OPERATE_RET __wait_busy(uint32_t timeout_ms, const char *what, TUYA_GPIO_LEVEL_E idle_level)
{
    OPERATE_RET       rt    = OPRT_OK;
    SYS_TIME_T        start = tal_system_get_millisecond();
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_HIGH;

    do {
        TUYA_CALL_ERR_RETURN(tkl_gpio_read(X4PRO_EPD_PIN_BUSY, &level));
        if (level == idle_level) {
            return OPRT_OK;
        }
        tal_system_sleep(1);
    } while ((uint32_t)(tal_system_get_millisecond() - start) < timeout_ms);

    /* Stall diagnostic: final BUSY level + the sequence that got stuck, so
     * the serial log alone can separate wiring issues from a bad model. */
    (void)tkl_gpio_read(X4PRO_EPD_PIN_BUSY, &level);
    PR_ERR("[" TAG "] BUSY stuck %s after %lu ms @ %s (BUSY=%d, want idle=%d, ctrl=%d)",
           (level == TUYA_GPIO_LEVEL_HIGH) ? "HIGH" : "LOW", (unsigned long)timeout_ms, what, (int)level,
           (int)idle_level, (int)s_controller);
    return OPRT_TIMEOUT;
}

/**
 * @brief Hardware reset sequence.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __reset(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
    tal_system_sleep(20);
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_LOW));
    tal_system_sleep(2);
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
    tal_system_sleep(20);

    return OPRT_OK;
}

/**
 * @brief Configure a display RAM region.
 * @param[in] x region x in pixels.
 * @param[in] y region y in pixels.
 * @param[in] w region width in pixels.
 * @param[in] h region height in pixels.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __set_ram_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    OPERATE_RET rt = OPRT_OK;

    /* Gates are physically reversed on this panel. */
    y = (uint16_t)(X4PRO_EPD_HEIGHT - y - h);

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DATA_ENTRY_MODE));
    TUYA_CALL_ERR_RETURN(__send_data(DATA_ENTRY_X_INC_Y_DEC));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SET_RAM_X_RANGE));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(x & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(x >> 8)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((x + w - 1U) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((x + w - 1U) >> 8)));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SET_RAM_Y_RANGE));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((y + h - 1U) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((y + h - 1U) >> 8)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(y & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(y >> 8)));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SET_RAM_X_COUNTER));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(x & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(x >> 8)));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SET_RAM_Y_COUNTER));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((y + h - 1U) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((y + h - 1U) >> 8)));

    return OPRT_OK;
}

/**
 * @brief Write a full framebuffer to one SSD1677 RAM plane.
 * @param[in] ram_cmd RAM write command.
 * @param[in] data framebuffer pointer.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __write_ram_buffer(uint8_t ram_cmd, const uint8_t *data)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__send_cmd(ram_cmd));
    TUYA_CALL_ERR_RETURN(__send_data_buf(data, X4PRO_EPD_BUF_SIZE));

    return OPRT_OK;
}

/**
 * @brief Run one SSD1677 refresh sequence (incremental X4 power model).
 *
 * Clock/analog enable bits are folded into the update command the first time
 * (0x22 |= 0xC0) and the rails then STAY up between paints: on this unit a
 * controller that self-powered down (vendor 0xF7/0xFC sequences, or a
 * standalone 0x22=0xC0 power-on) never de-asserted BUSY again, while the
 * always-on incremental path the X4 BSP drives this same panel class with
 * completes every update.
 *
 * @param[in] full true for the absolute full waveform, false for the fast
 *                 differential DU waveform.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __refresh(bool full)
{
    OPERATE_RET rt           = OPRT_OK;
    uint8_t     display_mode = 0;
    SYS_TIME_T  t0           = tal_system_get_millisecond();

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DISPLAY_UPDATE_CTRL1));
    TUYA_CALL_ERR_RETURN(__send_data(full ? CTRL1_BYPASS_RED : CTRL1_NORMAL));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_BORDER_WAVEFORM));
    TUYA_CALL_ERR_RETURN(__send_data(BORDER_WAVEFORM_RUN));

    if (!s_screen_on) {
        s_screen_on = TRUE;
        display_mode |= 0xC0; /* CLOCK_ON | ANALOG_ON */
        PR_DEBUG("[" TAG "] refresh: bringing clock/analog rails up (ctrl2 |= 0xC0)");
    }

    display_mode |= full ? 0x34 : 0x1C; /* OTP full waveform : DU partial */

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DISPLAY_UPDATE_CTRL2));
    TUYA_CALL_ERR_RETURN(__send_data(display_mode));
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_MASTER_ACTIVATION));
    TUYA_CALL_ERR_RETURN(__wait_busy(full ? X4PRO_EPD_BUSY_TIMEOUT_FULL_MS
                                          : X4PRO_EPD_BUSY_TIMEOUT_FAST_MS,
                                     full ? "full refresh" : "fast refresh", TUYA_GPIO_LEVEL_LOW));

    PR_DEBUG("[" TAG "] %s refresh %lu ms (ctrl2=0x%02X)", full ? "full" : "fast",
             (unsigned long)(tal_system_get_millisecond() - t0), (unsigned)display_mode);

    return OPRT_OK;
}

/**
 * @brief Initialize the SSD1677 controller with the X4 Pro production values.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __controller_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    SYS_TIME_T  t0;

    PR_DEBUG("[" TAG "] init step 1/7: SWRESET + fixed 10 ms settle");
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SOFT_RESET));
    /* Pro production sequence: fixed 10 ms settle — active-high BUSY may not
     * have asserted by the first GPIO sample, so wait_busy() alone is not a
     * substitute for this delay. */
    tal_system_sleep(10);
    t0 = tal_system_get_millisecond();
    TUYA_CALL_ERR_RETURN(__wait_busy(X4PRO_EPD_BUSY_TIMEOUT_FULL_MS, "SWRESET", TUYA_GPIO_LEVEL_LOW));
    PR_DEBUG("[" TAG "] init step 1/7: BUSY idle %lu ms after SWRESET",
             (unsigned long)(tal_system_get_millisecond() - t0));

    PR_DEBUG("[" TAG "] init step 2/7: temp sensor = internal (0x80)");
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_TEMP_SENSOR_CONTROL));
    TUYA_CALL_ERR_RETURN(__send_data(TEMP_SENSOR_INTERNAL));

    PR_DEBUG("[" TAG "] init step 3/7: booster soft-start AE C7 C3 C0 80");
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_BOOSTER_SOFT_START));
    TUYA_CALL_ERR_RETURN(__send_data(0xAE));
    TUYA_CALL_ERR_RETURN(__send_data(0xC7));
    TUYA_CALL_ERR_RETURN(__send_data(0xC3));
    TUYA_CALL_ERR_RETURN(__send_data(0xC0));
    TUYA_CALL_ERR_RETURN(__send_data(0x80));

    PR_DEBUG("[" TAG "] init step 4/7: driver output control (%u-1 gates, GD/SM)",
             (unsigned)X4PRO_EPD_HEIGHT);
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DRIVER_OUTPUT_CONTROL));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((X4PRO_EPD_HEIGHT - 1U) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((X4PRO_EPD_HEIGHT - 1U) >> 8)));
    TUYA_CALL_ERR_RETURN(__send_data(0x02));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_BORDER_WAVEFORM));
    TUYA_CALL_ERR_RETURN(__send_data(BORDER_WAVEFORM_INIT));

    PR_DEBUG("[" TAG "] init step 5/7: RAM window %ux%u (entry X-inc/Y-dec)",
             (unsigned)X4PRO_EPD_WIDTH, (unsigned)X4PRO_EPD_HEIGHT);
    TUYA_CALL_ERR_RETURN(__set_ram_area(0, 0, X4PRO_EPD_WIDTH, X4PRO_EPD_HEIGHT));

    PR_DEBUG("[" TAG "] init step 6/7: auto-write BW plane (0xF7) + wait BUSY");
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_AUTO_WRITE_BW_RAM));
    TUYA_CALL_ERR_RETURN(__send_data(0xF7));
    TUYA_CALL_ERR_RETURN(__wait_busy(X4PRO_EPD_BUSY_TIMEOUT_FULL_MS, "auto-write BW", TUYA_GPIO_LEVEL_LOW));

    PR_DEBUG("[" TAG "] init step 7/7: auto-write RED plane (0xF7) + wait BUSY");
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_AUTO_WRITE_RED_RAM));
    TUYA_CALL_ERR_RETURN(__send_data(0xF7));
    TUYA_CALL_ERR_RETURN(__wait_busy(X4PRO_EPD_BUSY_TIMEOUT_FULL_MS, "auto-write RED", TUYA_GPIO_LEVEL_LOW));

    s_screen_on = FALSE;
    return OPRT_OK;
}

/**
 * @brief Power down analog/clock the stock X4 Pro way (border parked at init).
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __power_off(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("[" TAG "] power-off: border parked + ctrl2=0x03 + 200 ms settle");
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_BORDER_WAVEFORM));
    TUYA_CALL_ERR_RETURN(__send_data(BORDER_WAVEFORM_INIT));
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DISPLAY_UPDATE_CTRL2));
    TUYA_CALL_ERR_RETURN(__send_data(POWER_OFF_SEQUENCE));
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_MASTER_ACTIVATION));
    /* Production power-off time; wait out any remaining BUSY afterwards. */
    tal_system_sleep(200);
    TUYA_CALL_ERR_RETURN(__wait_busy(X4PRO_EPD_BUSY_TIMEOUT_FULL_MS, "power off", TUYA_GPIO_LEVEL_LOW));

    return OPRT_OK;
}

/* ── UC8179 / UC8279 backend (FreeInk OEM sequences) ─────────────────────────
 * KW-mode differential model: DTM1 (0x10) = OLD plane, DTM2 (0x13) = NEW
 * plane; full refresh seeds OLD white and runs the OTP GC waveform, fast
 * refresh enters PTIN and runs the DU waveform against the OLD plane (synced
 * to the just-displayed frame after every successful refresh). BUSY is
 * idle-HIGH on both parts. Orientation: rows reversed on upload + SHL in PSR
 * (the vendor convention FreeInk validated on hardware). */

typedef struct {
    uint8_t  psr0;        /* PSR byte0 at init (0x3F UC8179 / 0x37 UC8279) */
    uint8_t  psr1;        /* 0x0A / 0x4D                                   */
    uint16_t gate_offset; /* white gate rows before the visible window     */
    BOOL_T   has_btst;    /* UC8179 writes BTST + power-save; UC8279 OTP   */
    BOOL_T   has_cdi;     /* UC8179 asserts CDI per refresh; UC8279 none   */
    BOOL_T   has_pll;     /* UC8279 programs the PLL at init               */
} UC_BACKEND_CFG_T;

static const UC_BACKEND_CFG_T s_uc8179_cfg = {0x3F, 0x0A, 0U, TRUE, TRUE, FALSE};
static const UC_BACKEND_CFG_T s_uc8279_cfg = {0x37, 0x4D, UC8279_GATE_OFFSET, FALSE, FALSE, TRUE};

#define UC_TSSET_FULL     0x1E /* GC full waveform                          */
#define UC_TSSET_FAST     0x5A /* DU frame-rate lever (per RE)              */
#define UC_CDI_ACTIVE     0x29 /* UC8179 CDI during refresh                 */
#define UC_CDI_IDLE       0xA9 /* UC8179 CDI restored after                 */
#define UC_CDI_INTERVAL   0x07 /* UC8179 CDI byte1, constant                */
#define UC_PON_TIMEOUT_MS 2000U

/**
 * @brief Select the active UC backend config.
 */
static const UC_BACKEND_CFG_T *__uc_cfg(void)
{
    return (s_controller == EPD_CTRL_UC8279) ? &s_uc8279_cfg : &s_uc8179_cfg;
}

/**
 * @brief Stream one framebuffer into a UC RAM plane, mirror-Y via row
 *        reversal, white-padded to the addressed 600-gate scan.
 * @param[in] ram_cmd UC_CMD_DTM1 or UC_CMD_DTM2.
 * @param[in] fb framebuffer pointer (800x480, 100 bytes/row).
 * @param[in] invert bitwise-invert each row (charge-scrub paths).
 * @param[in] cfg active backend config.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __uc_stream_plane(uint8_t ram_cmd, const uint8_t *fb, BOOL_T invert, const UC_BACKEND_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t     row[X4PRO_EPD_WIDTH_BYTES];
    uint16_t    y;

    TUYA_CALL_ERR_RETURN(__send_cmd(ram_cmd));

    (void)memset(row, 0xFF, sizeof(row));
    for (y = 0; y < cfg->gate_offset; y++) {
        TUYA_CALL_ERR_RETURN(__send_data_buf(row, sizeof(row)));
    }
    for (y = X4PRO_EPD_HEIGHT; y-- > 0U;) {
        const uint8_t *src = fb + (uint32_t)y * X4PRO_EPD_WIDTH_BYTES;
        if (invert) {
            uint16_t i;
            for (i = 0; i < X4PRO_EPD_WIDTH_BYTES; i++) {
                row[i] = (uint8_t)~src[i];
            }
            TUYA_CALL_ERR_RETURN(__send_data_buf(row, sizeof(row)));
        } else {
            TUYA_CALL_ERR_RETURN(__send_data_buf(src, X4PRO_EPD_WIDTH_BYTES));
        }
    }
    (void)memset(row, 0xFF, sizeof(row));
    for (y = (uint16_t)(cfg->gate_offset + X4PRO_EPD_HEIGHT); y < UC_TRES_HEIGHT; y++) {
        TUYA_CALL_ERR_RETURN(__send_data_buf(row, sizeof(row)));
    }

    return OPRT_OK;
}

/**
 * @brief Fill one UC RAM plane with white across the whole 600-gate scan.
 * @param[in] ram_cmd UC_CMD_DTM1 or UC_CMD_DTM2.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __uc_plane_white_fill(uint8_t ram_cmd)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t     row[X4PRO_EPD_WIDTH_BYTES];
    uint16_t    y;

    (void)memset(row, 0xFF, sizeof(row));
    TUYA_CALL_ERR_RETURN(__send_cmd(ram_cmd));
    for (y = 0; y < UC_TRES_HEIGHT; y++) {
        TUYA_CALL_ERR_RETURN(__send_data_buf(row, sizeof(row)));
    }

    return OPRT_OK;
}

/**
 * @brief Vendor runtime init for both UC parts: PSR, TRES (800x600), GSST,
 *        PFS (+PLL on UC8279, +BTST/E3 on UC8179). No plane fill here.
 * @param[in] cfg active backend config.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __uc_controller_init(const UC_BACKEND_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_PANEL_SETTING));
    TUYA_CALL_ERR_RETURN(__send_data(cfg->psr0));
    TUYA_CALL_ERR_RETURN(__send_data(cfg->psr1));

    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_TRES));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((X4PRO_EPD_WIDTH >> 8) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(X4PRO_EPD_WIDTH & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((UC_TRES_HEIGHT >> 8) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(UC_TRES_HEIGHT & 0xFFU)));

    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_GSST));
    TUYA_CALL_ERR_RETURN(__send_data(0x00));
    TUYA_CALL_ERR_RETURN(__send_data(0x00));
    TUYA_CALL_ERR_RETURN(__send_data(0x00));
    TUYA_CALL_ERR_RETURN(__send_data(0x00));

    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_PFS));
    TUYA_CALL_ERR_RETURN(__send_data(0x20));

    if (cfg->has_pll) {
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_PLL));
        TUYA_CALL_ERR_RETURN(__send_data(0x0E));
    }

    if (cfg->has_btst) {
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_BTST));
        TUYA_CALL_ERR_RETURN(__send_data(0x25));
        TUYA_CALL_ERR_RETURN(__send_data(0x25));
        TUYA_CALL_ERR_RETURN(__send_data(0x3C));
        TUYA_CALL_ERR_RETURN(__send_data(0x25));
    }

    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_GATE_SCAN));
    TUYA_CALL_ERR_RETURN(__send_data(0x02));

    if (cfg->has_btst) { /* UC8179 power-save: VCOM 2 lines, source 2*660 ns */
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_POWER_SAVE));
        TUYA_CALL_ERR_RETURN(__send_data(0x22));
    }

    s_screen_on = FALSE;
    return OPRT_OK;
}

/**
 * @brief PON once; rails then stay up between paints (no POF per refresh).
 * @param[in] what sequence name for the BUSY diagnostic.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __uc_power_on(const char *what)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_screen_on) {
        return OPRT_OK;
    }
    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_POWER_ON));
    TUYA_CALL_ERR_RETURN(__wait_busy(UC_PON_TIMEOUT_MS, what, TUYA_GPIO_LEVEL_HIGH));
    s_screen_on = TRUE;
    return OPRT_OK;
}

/**
 * @brief Run one UC8179/UC8279 refresh (exact OEM order).
 * @param[in] full true for the absolute GC flash, false for the DU partial.
 * @param[in] fb framebuffer to display.
 * @param[in] cfg active backend config.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __uc_refresh(BOOL_T full, const uint8_t *fb, const UC_BACKEND_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    SYS_TIME_T  t0 = tal_system_get_millisecond();

    /* NEW plane = new frame. */
    TUYA_CALL_ERR_RETURN(__uc_stream_plane(UC_CMD_DTM2, fb, FALSE, cfg));
    if (full) {
        /* Absolute GC-from-white: seed the OLD plane white across the whole
         * 600-gate scan. A fast refresh leaves OLD on the previous frame
         * (synced below after every successful refresh). */
        TUYA_CALL_ERR_RETURN(__uc_plane_white_fill(UC_CMD_DTM1));
    }

    if (cfg->has_cdi) {
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_CDI));
        TUYA_CALL_ERR_RETURN(__send_data(UC_CDI_ACTIVE));
        TUYA_CALL_ERR_RETURN(__send_data(UC_CDI_INTERVAL));
    }
    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_CCSET));
    TUYA_CALL_ERR_RETURN(__send_data(0x02));
    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_TSSET));
    TUYA_CALL_ERR_RETURN(__send_data(full ? UC_TSSET_FULL : UC_TSSET_FAST));
    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_PANEL_SETTING));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(cfg->psr0 & 0xDFU))); /* REG cleared -> OTP */
    TUYA_CALL_ERR_RETURN(__send_data(cfg->psr1));
    if (!full) {
        /* Fast-only PFS/gate-scan re-assert: without them the OTP waveform
         * runs at the full frame count (same duration + garbled). */
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_PFS));
        TUYA_CALL_ERR_RETURN(__send_data(0x20));
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_GATE_SCAN));
        TUYA_CALL_ERR_RETURN(__send_data(0x02));
    }

    TUYA_CALL_ERR_RETURN(__uc_power_on("UC PON"));

    if (!full) {
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_PTIN));
    }
    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_DISPLAY_REFRESH));
    TUYA_CALL_ERR_RETURN(__wait_busy(full ? X4PRO_EPD_BUSY_TIMEOUT_FULL_MS : X4PRO_EPD_BUSY_TIMEOUT_FAST_MS,
                                     full ? "UC full DRF" : "UC fast DRF", TUYA_GPIO_LEVEL_HIGH));
    if (!full) {
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_PTOUT));
    }

    if (cfg->has_cdi) { /* restore the idle CDI (border), as the OEM does */
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_CDI));
        TUYA_CALL_ERR_RETURN(__send_data(UC_CDI_IDLE));
        TUYA_CALL_ERR_RETURN(__send_data(UC_CDI_INTERVAL));
    }

    /* Sync the OLD plane with the just-displayed frame so the next partial
     * diffs against it (KW ghosting management). */
    TUYA_CALL_ERR_RETURN(__uc_stream_plane(UC_CMD_DTM1, fb, FALSE, cfg));

    PR_DEBUG("[" TAG "] %s refresh %lu ms (ctrl=%s)", full ? "full" : "fast",
             (unsigned long)(tal_system_get_millisecond() - t0),
             (s_controller == EPD_CTRL_UC8279) ? "UC8279" : "UC8179");

    return OPRT_OK;
}

/**
 * @brief UC power-down + deep sleep (check code 0xA5, RAM discarded).
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __uc_sleep(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_screen_on) {
        TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_POWER_OFF));
        TUYA_CALL_ERR_RETURN(__wait_busy(X4PRO_EPD_BUSY_TIMEOUT_FULL_MS, "UC POF", TUYA_GPIO_LEVEL_HIGH));
        s_screen_on = FALSE;
    }
    TUYA_CALL_ERR_RETURN(__send_cmd(UC_CMD_DEEP_SLEEP));
    TUYA_CALL_ERR_RETURN(__send_data(0xA5));

    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_epd_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_epd_inited) {
        return OPRT_OK;
    }

    (void)memset(s_prev_frame, 0xFF, sizeof(s_prev_frame));
    s_prev_valid = FALSE;
    s_screen_on  = FALSE;

    PR_NOTICE("[" TAG "] init: SPI port%d MODE0 %u kHz | SCLK=%d MOSI=%d CS=%d DC=%d RST=%d BUSY=%d",
              (int)X4PRO_EPD_SPI_PORT, (unsigned)(X4PRO_EPD_SPI_FREQ_HZ / 1000U), (int)X4PRO_EPD_PIN_SCLK,
              (int)X4PRO_EPD_PIN_MOSI, (int)X4PRO_EPD_PIN_CS, (int)X4PRO_EPD_PIN_DC, (int)X4PRO_EPD_PIN_RST,
              (int)X4PRO_EPD_PIN_BUSY);

    /* Identify the silicon BEFORE claiming the SPI bus: newer X4 Pro batches
     * carry a UC8179/UC8279 whose register map is nothing like the SSD1677. */
    s_controller = __probe_controller();

    TUYA_CALL_ERR_RETURN(__bus_init());

    if (s_controller != EPD_CTRL_SSD1677) {
        PR_DEBUG("[" TAG "] init: UC81xx HW reset (RST LOW 50 ms) + OEM register init");
        /* The vendor identification path holds RST_N low for 50 ms; the UC
         * init is less forgiving than normal operation about reset quality. */
        TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
        tal_system_sleep(20);
        TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_LOW));
        tal_system_sleep(50);
        TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4PRO_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
        tal_system_sleep(30);
        TUYA_CALL_ERR_RETURN(__uc_controller_init(__uc_cfg()));
    } else {
        PR_DEBUG("[" TAG "] init: HW reset pulse (HIGH 20 ms / LOW 2 ms / HIGH 20 ms)");
        TUYA_CALL_ERR_RETURN(__reset());
        TUYA_CALL_ERR_RETURN(__controller_init());
    }

    s_epd_inited = TRUE;
    PR_NOTICE("[" TAG "] panel ready (ctrl=%s), first paint will be a full refresh",
              (s_controller == EPD_CTRL_UC8279) ? "UC8279" : (s_controller == EPD_CTRL_UC8179) ? "UC8179" : "SSD1677");
    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_epd_clear(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t     white_line[X4PRO_EPD_WIDTH_BYTES];
    uint16_t    y;

    if (!s_epd_inited) {
        return OPRT_COM_ERROR;
    }

    if (s_controller != EPD_CTRL_SSD1677) {
        /* White through the absolute GC waveform; the plane sync at the end
         * of the refresh re-seeds the differential baseline to white too. */
        (void)memset(s_prev_frame, 0xFF, sizeof(s_prev_frame));
        TUYA_CALL_ERR_RETURN(__uc_refresh(TRUE, s_prev_frame, __uc_cfg()));
        s_prev_valid = TRUE;
        return OPRT_OK;
    }

    (void)memset(white_line, 0xFF, sizeof(white_line));
    TUYA_CALL_ERR_RETURN(__set_ram_area(0, 0, X4PRO_EPD_WIDTH, X4PRO_EPD_HEIGHT));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_WRITE_RAM_BW));
    for (y = 0; y < X4PRO_EPD_HEIGHT; y++) {
        TUYA_CALL_ERR_RETURN(__send_data_buf(white_line, sizeof(white_line)));
    }

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_WRITE_RAM_RED));
    for (y = 0; y < X4PRO_EPD_HEIGHT; y++) {
        TUYA_CALL_ERR_RETURN(__send_data_buf(white_line, sizeof(white_line)));
    }

    TUYA_CALL_ERR_RETURN(__refresh(true));
    (void)memset(s_prev_frame, 0xFF, sizeof(s_prev_frame));
    s_prev_valid = TRUE;

    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_epd_display(uint8_t *image)
{
    OPERATE_RET rt           = OPRT_OK;
    bool        full_refresh = false;

    if (!s_epd_inited || NULL == image) {
        return OPRT_COM_ERROR;
    }

    /* The first paint after init must be absolute: a differential DU update
     * only drives pixels that differ from the OLD/RED baseline and cannot
     * clear whatever is physically on the panel at boot. */
    full_refresh = (s_prev_valid == FALSE);

    if (s_controller != EPD_CTRL_SSD1677) {
        TUYA_CALL_ERR_RETURN(__uc_refresh(full_refresh, image, __uc_cfg()));
        (void)memcpy(s_prev_frame, image, sizeof(s_prev_frame));
        s_prev_valid = TRUE;
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(__set_ram_area(0, 0, X4PRO_EPD_WIDTH, X4PRO_EPD_HEIGHT));
    TUYA_CALL_ERR_RETURN(__write_ram_buffer(CMD_WRITE_RAM_BW, image));
    TUYA_CALL_ERR_RETURN(__write_ram_buffer(CMD_WRITE_RAM_RED, full_refresh ? image : s_prev_frame));
    TUYA_CALL_ERR_RETURN(__refresh(full_refresh));

    (void)memcpy(s_prev_frame, image, sizeof(s_prev_frame));
    s_prev_valid = TRUE;

    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_epd_display_full_refresh(uint8_t *image)
{
    OPERATE_RET rt = OPRT_OK;

    if (!s_epd_inited || NULL == image) {
        return OPRT_COM_ERROR;
    }

    if (s_controller != EPD_CTRL_SSD1677) {
        TUYA_CALL_ERR_RETURN(__uc_refresh(TRUE, image, __uc_cfg()));
        (void)memcpy(s_prev_frame, image, sizeof(s_prev_frame));
        s_prev_valid = TRUE;
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(__set_ram_area(0, 0, X4PRO_EPD_WIDTH, X4PRO_EPD_HEIGHT));
    TUYA_CALL_ERR_RETURN(__write_ram_buffer(CMD_WRITE_RAM_BW, image));
    TUYA_CALL_ERR_RETURN(__write_ram_buffer(CMD_WRITE_RAM_RED, image));
    TUYA_CALL_ERR_RETURN(__refresh(true));

    (void)memcpy(s_prev_frame, image, sizeof(s_prev_frame));
    s_prev_valid = TRUE;

    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_epd_sleep(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (!s_epd_inited) {
        return OPRT_COM_ERROR;
    }

    if (s_controller != EPD_CTRL_SSD1677) {
        /* Deep sleep discards controller RAM — the next init re-seeds the
         * differential baseline with a full refresh anyway. */
        PR_NOTICE("[" TAG "] entering UC deep sleep (DSLP 0xA5, RAM discarded)");
        TUYA_CALL_ERR_RETURN(__uc_sleep());
        s_epd_inited = FALSE;
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(__power_off());
    s_screen_on = FALSE;

    /* Deep sleep mode 2 (0x03): controller RAM is discarded — the next init
     * must run a full refresh to re-seed the differential baseline. */
    PR_NOTICE("[" TAG "] entering deep sleep mode 2 (RAM discarded)");
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DEEP_SLEEP));
    TUYA_CALL_ERR_RETURN(__send_data(0x03));
    s_epd_inited = FALSE;

    return OPRT_OK;
}
