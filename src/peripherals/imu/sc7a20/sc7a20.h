/**
 * @file sc7a20.h
 * @brief SC7A20 3-axis accelerometer TDD driver
 * @details Register-compatible with ST LIS3DH. Supports configurable range/ODR,
 *          mg conversion, data-ready and FIFO batch read.
 * @version 0.3
 * @date 2025-07-14
 */

#ifndef __TDD_SC7A20_H__
#define __TDD_SC7A20_H__

#include "tuya_cloud_types.h"
#include "tkl_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/* SC7A20 I2C address (7-bit): SA0=GND -> 0x18, SA0=VCC -> 0x19 */
#define SC7A20_I2C_ADDR_SA0_LOW      0x18
#define SC7A20_I2C_ADDR_SA0_HIGH     0x19

/* Register map (LIS3DH-compatible) */
#define SC7A20_REG_WHO_AM_I          0x0F
#define SC7A20_REG_CTRL_REG1         0x20  /* ODR[7:4] | LPen[3] | Zen/Yen/Xen[2:0] */
#define SC7A20_REG_CTRL_REG2         0x21  /* HPF config */
#define SC7A20_REG_CTRL_REG3         0x22  /* INT1 route */
#define SC7A20_REG_CTRL_REG4         0x23  /* FS[5:4] | HR[3] */
#define SC7A20_REG_CTRL_REG5         0x24  /* FIFO_EN[6] */
#define SC7A20_REG_STATUS            0x27  /* ZYXDA[3] data-ready */
#define SC7A20_REG_OUT_X_L           0x28  /* auto-increment: X_L/X_H/Y_L/Y_H/Z_L/Z_H */
#define SC7A20_REG_FIFO_CTRL_REG     0x2E  /* FM[7:6] mode | FTH[5:0] watermark */
#define SC7A20_REG_FIFO_SRC_REG      0x2F  /* WTM[7] | OVRN[6] | EMPTY[5] | FSS[4:0] */

/* Interrupt 1 registers (LIS3DH-compatible) */
#define SC7A20_REG_INT1_CFG          0x30  /* AOI[7]|6D[6]|ZHIE..XLIE[5:0] */
#define SC7A20_REG_INT1_SRC          0x31  /* read-only, IA[6]; reading clears latched INT1 */
#define SC7A20_REG_INT1_THS          0x32  /* THS[6:0], 1 LSB = fullscale/128 */
#define SC7A20_REG_INT1_DURATION     0x33  /* DC[6:0], duration = (DC+1)/ODR */

#define SC7A20_WHO_AM_I_VAL          0x11
#define SC7A20_MULTI_READ            0x80  /* set MSB of reg addr for auto-increment */

/* STATUS register */
#define SC7A20_STATUS_ZYXDA          0x08  /* X/Y/Z new data available */

/* CTRL_REG3 bits */
#define SC7A20_CTRL3_I1_IA1          0x40  /* route AOI1 interrupt to INT1 pin */
/* CTRL_REG5 bits */
#define SC7A20_CTRL5_LIR_INT1        0x08  /* latch INT1 until INT1_SRC is read */
/* INT1_SRC bits */
#define SC7A20_INT1_IA               0x40  /* interrupt active */

/***********************************************************
************************enum define*************************
***********************************************************/

/* Full-scale range (CTRL_REG4 FS[5:4]); also encodes g: fullscale = 2 << range */
typedef enum {
    SC7A20_RANGE_2G  = 0,   /* FS=00 */
    SC7A20_RANGE_4G  = 1,   /* FS=01 */
    SC7A20_RANGE_8G  = 2,   /* FS=10 */
    SC7A20_RANGE_16G = 3,   /* FS=11 */
} SC7A20_RANGE_E;

/* Output data rate (CTRL_REG1 ODR[7:4]) */
typedef enum {
    SC7A20_ODR_1HZ   = 0x10,
    SC7A20_ODR_10HZ  = 0x20,
    SC7A20_ODR_25HZ  = 0x30,
    SC7A20_ODR_50HZ  = 0x40,
    SC7A20_ODR_100HZ = 0x50,
    SC7A20_ODR_200HZ = 0x60,
    SC7A20_ODR_400HZ = 0x70,
} SC7A20_ODR_E;

/* FIFO mode (FIFO_CTRL_REG FM[7:6]) */
typedef enum {
    SC7A20_FIFO_BYPASS      = 0x00,  /* FM=00, FIFO disabled */
    SC7A20_FIFO_FIFO        = 0x40,  /* FM=01, stop collecting when full */
    SC7A20_FIFO_STREAM      = 0x80,  /* FM=10, keep latest, discard oldest */
    SC7A20_FIFO_STREAM2FIFO = 0xC0,  /* FM=11, stream until trigger then FIFO */
} SC7A20_FIFO_MODE_E;

/* INT1 interrupt event (INT1_CFG encoding) */
typedef enum {
    SC7A20_INT_ACT      = 0,  /* activity/motion: any axis exceeds threshold (OR, high events) */
    SC7A20_INT_FREEFALL = 1,  /* free-fall: all axes below threshold (AND, low events) */
} SC7A20_INT_EVENT_E;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/* Acceleration data: raw counts for read_accel, milli-g for read_mg */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} SC7A20_ACCEL_DATA_T;

/* Sensor configuration */
typedef struct {
    TUYA_I2C_NUM_E   i2c_port;
    uint8_t          i2c_addr;      /* 7-bit, e.g. SC7A20_I2C_ADDR_SA0_HIGH */
    SC7A20_RANGE_E   range;         /* full-scale range */
    SC7A20_ODR_E     odr;           /* output data rate */
    bool             low_power;     /* LPen (CTRL_REG1 bit3): low-power mode */
    bool             hpf_data;      /* apply high-pass filter to data output */
} TDD_SC7A20_CFG_T;

/* INT1 interrupt configuration */
typedef struct {
    SC7A20_INT_EVENT_E event;     /* activity or free-fall */
    uint8_t            threshold; /* INT1_THS, 1 LSB = fullscale/128 */
    uint8_t            duration;  /* INT1_DURATION, 1 LSB = 1/ODR */
} TDD_SC7A20_INT_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize SC7A20: I2C bus init, WHO_AM_I check, and apply
 *        range/ODR/low-power/HPF settings.
 */
OPERATE_RET tdd_sc7a20_init(const TDD_SC7A20_CFG_T *cfg);

/**
 * @brief Read raw acceleration (12-bit left-justified counts).
 */
OPERATE_RET tdd_sc7a20_read_accel(const TDD_SC7A20_CFG_T *cfg, SC7A20_ACCEL_DATA_T *raw);

/**
 * @brief Read acceleration converted to milli-g (uses cfg->range).
 *        mg = raw * (2 << range) * 1000 / 32768
 */
OPERATE_RET tdd_sc7a20_read_mg(const TDD_SC7A20_CFG_T *cfg, SC7A20_ACCEL_DATA_T *mg);

/**
 * @brief Check STATUS.ZYXDA (new sample available).
 */
bool tdd_sc7a20_data_ready(const TDD_SC7A20_CFG_T *cfg);

/**
 * @brief Set FIFO mode and watermark (enables FIFO via CTRL_REG5).
 * @param watermark  0..31 samples.
 */
OPERATE_RET tdd_sc7a20_fifo_set_mode(const TDD_SC7A20_CFG_T *cfg, SC7A20_FIFO_MODE_E mode, uint8_t watermark);

/**
 * @brief Drain FIFO: read up to *count samples into buf, return actual count.
 */
OPERATE_RET tdd_sc7a20_fifo_read(const TDD_SC7A20_CFG_T *cfg, SC7A20_ACCEL_DATA_T *buf, uint8_t *count);

/**
 * @brief Get current FIFO fill level (FIFO_SRC_REG FSS, 0..32).
 */
uint8_t tdd_sc7a20_fifo_level(const TDD_SC7A20_CFG_T *cfg);

/**
 * @brief Power down the sensor (CTRL_REG1 = 0).
 */
OPERATE_RET tdd_sc7a20_deinit(const TDD_SC7A20_CFG_T *cfg);

/**
 * @brief Configure INT1 interrupt: route AOI1 to INT1 (CTRL_REG3), latch mode
 *        (CTRL_REG5 LIR_INT1), and set event/threshold/duration
 *        (INT1_CFG/THS/DURATION). INT1 is active-high; clear it by reading
 *        INT1_SRC via tdd_sc7a20_int_read_src().
 */
OPERATE_RET tdd_sc7a20_int_config(const TDD_SC7A20_CFG_T *cfg, const TDD_SC7A20_INT_CFG_T *int_cfg);

/**
 * @brief Read INT1_SRC (reading clears the latched INT1 pin). bit
 *        SC7A20_INT1_IA (0x40) set means an INT1 event is/was active.
 */
OPERATE_RET tdd_sc7a20_int_read_src(const TDD_SC7A20_CFG_T *cfg, uint8_t *src);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_SC7A20_H__ */
