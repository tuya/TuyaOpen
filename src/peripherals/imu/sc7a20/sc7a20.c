/**
 * @file sc7a20.c
 * @brief SC7A20 3-axis accelerometer TDD driver
 * @details Register-compatible with ST LIS3DH. Configurable range/ODR, mg
 *          conversion, data-ready, FIFO batch read.
 * @version 0.3
 * @date 2025-07-14
 */

#include "sc7a20.h"
#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* CTRL_REG1 bits */
#define SC7A20_CTRL1_LPEN            0x08   /* low-power mode enable */
#define SC7A20_CTRL1_AXES            0x07   /* X/Y/Z enable */

/* CTRL_REG2 bits */
#define SC7A20_CTRL2_FDS             0x04   /* filtered data selection (HPF -> data) */

/* CTRL_REG4 bits */
#define SC7A20_CTRL4_HR              0x08   /* high-resolution (12-bit) */

/* CTRL_REG5 bits */
#define SC7A20_CTRL5_FIFO_EN         0x40

/***********************************************************
***********************static functions*********************
***********************************************************/
static OPERATE_RET __sc7a20_write_reg(TUYA_I2C_NUM_E port, uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return tkl_i2c_master_send(port, addr, buf, 2, TRUE);
}

static OPERATE_RET __sc7a20_read_reg(TUYA_I2C_NUM_E port, uint8_t addr, uint8_t reg, uint8_t *val)
{
    OPERATE_RET rt;
    rt = tkl_i2c_master_send(port, addr, &reg, 1, FALSE);
    if (rt != OPRT_OK) {
        return rt;
    }
    return tkl_i2c_master_receive(port, addr, val, 1, TRUE);
}

static OPERATE_RET __sc7a20_read_multi(TUYA_I2C_NUM_E port, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    OPERATE_RET rt;
    uint8_t reg_auto = reg | SC7A20_MULTI_READ;
    rt = tkl_i2c_master_send(port, addr, &reg_auto, 1, FALSE);
    if (rt != OPRT_OK) {
        return rt;
    }
    return tkl_i2c_master_receive(port, addr, buf, len, TRUE);
}

/* read-modify-write: set bits <mask> of <reg> */
static OPERATE_RET __sc7a20_reg_set_mask(TUYA_I2C_NUM_E port, uint8_t addr, uint8_t reg, uint8_t mask)
{
    uint8_t v = 0;
    OPERATE_RET rt = __sc7a20_read_reg(port, addr, reg, &v);
    if (rt != OPRT_OK) {
        return rt;
    }
    return __sc7a20_write_reg(port, addr, reg, (uint8_t)(v | mask));
}

/***********************************************************
***********************public functions*********************
***********************************************************/
OPERATE_RET tdd_sc7a20_init(const TDD_SC7A20_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    /* I2C bus init */
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role       = TUYA_IIC_MODE_MASTER,
        .speed      = TUYA_IIC_BUS_SPEED_400K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };
    TUYA_CALL_ERR_RETURN(tkl_i2c_init(cfg->i2c_port, &i2c_cfg));

    /* Verify WHO_AM_I */
    uint8_t who = 0;
    rt = __sc7a20_read_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_WHO_AM_I, &who);
    if (rt != OPRT_OK || who != SC7A20_WHO_AM_I_VAL) {
        PR_ERR("SC7A20 not found! WHO_AM_I=0x%02X (expect 0x%02X), rt=%d", who, SC7A20_WHO_AM_I_VAL, rt);
        return OPRT_COM_ERROR;
    }

    /* CTRL_REG1: ODR | LPen | X/Y/Z enable */
    uint8_t reg1 = (uint8_t)cfg->odr | SC7A20_CTRL1_AXES;
    if (cfg->low_power) {
        reg1 |= SC7A20_CTRL1_LPEN;
    }
    TUYA_CALL_ERR_RETURN(__sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_CTRL_REG1, reg1));

    /* CTRL_REG4: FS(range<<4) | HR(12-bit, unless low-power) */
    uint8_t reg4 = (uint8_t)(cfg->range << 4);
    if (!cfg->low_power) {
        reg4 |= SC7A20_CTRL4_HR;
    }
    TUYA_CALL_ERR_RETURN(__sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_CTRL_REG4, reg4));

    /* CTRL_REG2: apply HPF to data output if requested */
    if (cfg->hpf_data) {
        TUYA_CALL_ERR_RETURN(__sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_CTRL_REG2, SC7A20_CTRL2_FDS));
    }

    PR_NOTICE("SC7A20 init: odr=0x%02x range=%d lp=%d hpf=%d", cfg->odr, cfg->range, cfg->low_power, cfg->hpf_data);
    return OPRT_OK;
}

OPERATE_RET tdd_sc7a20_read_accel(const TDD_SC7A20_CFG_T *cfg, SC7A20_ACCEL_DATA_T *raw)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t buf[6] = {0};

    if (NULL == cfg || NULL == raw) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(__sc7a20_read_multi(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_OUT_X_L, buf, 6));

    raw->x = (int16_t)((buf[1] << 8) | buf[0]);
    raw->y = (int16_t)((buf[3] << 8) | buf[2]);
    raw->z = (int16_t)((buf[5] << 8) | buf[4]);

    return OPRT_OK;
}

OPERATE_RET tdd_sc7a20_read_mg(const TDD_SC7A20_CFG_T *cfg, SC7A20_ACCEL_DATA_T *mg)
{
    OPERATE_RET rt = OPRT_OK;
    SC7A20_ACCEL_DATA_T raw;
    int32_t fs;

    if (NULL == cfg || NULL == mg) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(tdd_sc7a20_read_accel(cfg, &raw));

    /* 12-bit HR left-justified to 16-bit: mg = raw * fullscale_g * 1000 / 32768 */
    fs = 2 << cfg->range;   /* 2/4/8/16 g */
    mg->x = (int16_t)((int32_t)raw.x * fs * 1000 / 32768);
    mg->y = (int16_t)((int32_t)raw.y * fs * 1000 / 32768);
    mg->z = (int16_t)((int32_t)raw.z * fs * 1000 / 32768);

    return OPRT_OK;
}

bool tdd_sc7a20_data_ready(const TDD_SC7A20_CFG_T *cfg)
{
    uint8_t st = 0;

    if (NULL == cfg) {
        return false;
    }

    if (OPRT_OK != __sc7a20_read_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_STATUS, &st)) {
        return false;
    }
    return (st & SC7A20_STATUS_ZYXDA) != 0;
}

OPERATE_RET tdd_sc7a20_fifo_set_mode(const TDD_SC7A20_CFG_T *cfg, SC7A20_FIFO_MODE_E mode, uint8_t watermark)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }
    if (watermark > 31) {
        watermark = 31;
    }
    /* CTRL_REG5: enable FIFO */
    TUYA_CALL_ERR_RETURN(__sc7a20_reg_set_mask(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_CTRL_REG5, SC7A20_CTRL5_FIFO_EN));
    /* FIFO_CTRL_REG: FM[7:6] | FTH[5:0] */
    TUYA_CALL_ERR_RETURN(__sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_FIFO_CTRL_REG,
                                             (uint8_t)mode | (uint8_t)(watermark & 0x1F)));
    return OPRT_OK;
}

OPERATE_RET tdd_sc7a20_fifo_read(const TDD_SC7A20_CFG_T *cfg, SC7A20_ACCEL_DATA_T *buf, uint8_t *count)
{
    uint8_t level, n, tmp[6];

    if (NULL == cfg || NULL == buf || NULL == count) {
        return OPRT_INVALID_PARM;
    }

    level = tdd_sc7a20_fifo_level(cfg);
    if (level == 0) {
        *count = 0;
        return OPRT_OK;
    }
    n = (level < *count) ? level : *count;

    for (uint8_t i = 0; i < n; i++) {
        if (OPRT_OK != __sc7a20_read_multi(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_OUT_X_L, tmp, 6)) {
            *count = i;
            return OPRT_COM_ERROR;
        }
        buf[i].x = (int16_t)((tmp[1] << 8) | tmp[0]);
        buf[i].y = (int16_t)((tmp[3] << 8) | tmp[2]);
        buf[i].z = (int16_t)((tmp[5] << 8) | tmp[4]);
    }
    *count = n;
    return OPRT_OK;
}

uint8_t tdd_sc7a20_fifo_level(const TDD_SC7A20_CFG_T *cfg)
{
    uint8_t src = 0;

    if (NULL == cfg) {
        return 0;
    }
    if (OPRT_OK != __sc7a20_read_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_FIFO_SRC_REG, &src)) {
        return 0;
    }
    return src & 0x1F;   /* FSS[4:0] */
}

OPERATE_RET tdd_sc7a20_deinit(const TDD_SC7A20_CFG_T *cfg)
{
    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }
    /* power down: CTRL_REG1 = 0 */
    return __sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_CTRL_REG1, 0x00);
}

OPERATE_RET tdd_sc7a20_int_config(const TDD_SC7A20_CFG_T *cfg, const TDD_SC7A20_INT_CFG_T *int_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t     int1_cfg;

    if (NULL == cfg || NULL == int_cfg) {
        return OPRT_INVALID_PARM;
    }

    /* INT1_CFG: OR(AOI=0) high events for activity, AND(AOI=1) low events for free-fall */
    int1_cfg = (int_cfg->event == SC7A20_INT_FREEFALL) ? 0x95 : 0x2A;

    /* CTRL_REG5: latch INT1 until INT1_SRC is read */
    TUYA_CALL_ERR_RETURN(__sc7a20_reg_set_mask(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_CTRL_REG5, SC7A20_CTRL5_LIR_INT1));
    /* event config / threshold / duration */
    TUYA_CALL_ERR_RETURN(__sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_INT1_CFG, int1_cfg));
    TUYA_CALL_ERR_RETURN(__sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_INT1_THS, int_cfg->threshold & 0x7F));
    TUYA_CALL_ERR_RETURN(__sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_INT1_DURATION, int_cfg->duration & 0x7F));
    /* CTRL_REG3: route AOI1 to INT1 pin (active-high by default) */
    TUYA_CALL_ERR_RETURN(__sc7a20_write_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_CTRL_REG3, SC7A20_CTRL3_I1_IA1));

    return OPRT_OK;
}

OPERATE_RET tdd_sc7a20_int_read_src(const TDD_SC7A20_CFG_T *cfg, uint8_t *src)
{
    if (NULL == cfg || NULL == src) {
        return OPRT_INVALID_PARM;
    }
    /* Reading INT1_SRC clears the latched interrupt on the INT1 pin */
    return __sc7a20_read_reg(cfg->i2c_port, cfg->i2c_addr, SC7A20_REG_INT1_SRC, src);
}
