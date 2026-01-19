/**
 * @file tdd_disp_ssd1306.c
 * @brief Implementation file for the SSD1306 OLED display driver using I2C interface.
 *
 * This file contains the implementation of functions required to initialize and control 
 * an SSD1306-based OLED display via the I2C communication protocol. It includes routines 
 * for display initialization, frame buffer flushing, and low-level I2C operations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_I2C) && (ENABLE_I2C==1)
#include "tal_log.h"
#include "tal_memory.h"

#include "tkl_i2c.h"
#include "tkl_gpio.h"
#include "tkl_pinmux.h"
#include "tdd_disp_ssd1306.h"
#include "tdl_display_driver.h"
#include "tdl_display_manage.h"
/***********************************************************
************************macro define************************
***********************************************************/
const uint8_t cSSD1306_INIT_SEQ[] = {
    0xAE,   //--turn off oled panel
    0x00,   //---set low column address
    0x10,   //---set high column address
    0x40,   //--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
    0x81,   //--set contrast control register
    0xCF,   // Set SEG Output Current Brightness
    0xA1,   //--Set SEG/Column Mapping     0xa0 left-right reversed, 0xa1 normal
    0xC8,   //Set COM/Row Scan Direction   0xc0 upside down, 0xc8 normal
    0xA6,   //--set normal display
    0xD3,   //-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
    0x00,   //-not offset
    0xD5,   //--set display clock divide ratio/oscillator frequency
    0x80,   //--set divide ratio, Set Clock as 100 Frames/Sec
    0xD9,   //--set pre-charge period
    0xF1,   //Set Pre-Charge as 15 Clocks & Discharge as 1 Clock  
    0xDB,   //--set vcomh
    0x40,   //Set VCOM Deselect Level
    0x8D,   //--set Charge Pump enable/disable
    0x14,   //--set(0x10) disable
    0xA4,   // Disable Entire Display On (0xa4/0xa5)
};

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_DISP_DEV_INFO_T      disp_info;
    DISP_SSD1306_INIT_CFG_T  init_cfg;
    TUYA_I2C_NUM_E           port;       // I2C port number
    uint8_t                  slave_addr; // I2C slave address
    TDL_DISP_FRAME_BUFF_T   *convert_fb; // Frame buffer for conversion
}DISP_SSD1306_DEV_T;


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
// pixels：
// P0 P0 ...
// P1 P1 ...
// P2 P2 ...
// P3 P3 ...
// P4 P4 ...
// P5 P5 ...
// P6 P6 ...
// P7 P7 ...
// P0 P0 ...
// ...
static void __tdd_ssd1306_convert(uint32_t width, uint32_t height, uint8_t *in_buf, uint8_t *out_buf)
{
    uint32_t i=0, j=0, m=0, width_bytes = 0;
    uint8_t b = 0, mix = 0;

    if(NULL == in_buf || NULL == out_buf) {
        return;
    }

    width_bytes = width / 8;

    for(i=0; i<height; i+=8) { 
        for(j=0; j<width; j++) {
            for(m=0; m<8; m++) {
                if(i + m >= height) {
                    continue;
                }
                b = in_buf[(i+m)*width_bytes + j/8];
                b = (b >> (j%8)) & 0x01;
                mix |= (b << m);
            }
            out_buf[(i/8)*width + j] = mix;
            mix = 0;
        }
    }
}

static OPERATE_RET __disp_i2c_init(TUYA_I2C_NUM_E port)
{
    OPERATE_RET rt = OPRT_OK;

    /*i2c init*/
    TUYA_IIC_BASE_CFG_T i2c_cfg;
    
    /*i2c init*/
    i2c_cfg.role       = TUYA_IIC_MODE_MASTER;
    i2c_cfg.speed      = TUYA_IIC_BUS_SPEED_400K;
    i2c_cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    TUYA_CALL_ERR_RETURN(tkl_i2c_init(port, &i2c_cfg));

    return rt;
}

static OPERATE_RET __disp_i2c_write_one_byte(TUYA_I2C_NUM_E port, uint8_t slave_addr, uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {0};

    buf[0] = reg;
    buf[1] = data;

    return tkl_i2c_master_send(port, slave_addr, (void *)buf, sizeof(buf), true);
}

static OPERATE_RET __disp_i2c_write_data(TUYA_I2C_NUM_E port, uint8_t slave_addr, uint8_t reg, uint8_t *p_data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *p_buf = NULL;
    uint32_t buf_len = len + 1;

    if(NULL == p_data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    p_buf = (uint8_t *)tal_malloc(buf_len);
    if(NULL == p_buf) {
        return OPRT_MALLOC_FAILED;
    }

    p_buf[0] = reg;
    memcpy(p_buf+1, p_data, len);

    rt = tkl_i2c_master_send(port, slave_addr, (void *)p_buf, buf_len, true);

    tal_free(p_buf);

    return rt;
}

static void __disp_i2c_ssd1306_set_pos(TUYA_I2C_NUM_E port, uint8_t slave_addr, uint8_t x, uint8_t y)
{
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, 0xB0+y);
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, ((x&0xF0)>>4)|0x10);
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, (x&0x0F));
}

static void __disp_i2c_ssd1306_multiplex_ratio(TUYA_I2C_NUM_E port, uint8_t slave_addr, uint8_t height)
{
    if(height < 2 || height > 64) {
        PR_ERR("Invalid height for SSD1306: %d", height);
        return;
    }

    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, 0xA8);
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, height-1);
}

static void __disp_i2c_ssd1306_com_pin_cfg(TUYA_I2C_NUM_E port, uint8_t slave_addr, uint8_t cfg)
{
    PR_DEBUG("SSD1306_COM_PIN_CFG: %x", cfg);
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, 0xDA);
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, cfg);
}

static void __disp_i2c_ssd1306_set_color_inverse(TUYA_I2C_NUM_E port, uint8_t slave_addr, bool is_inverse)
{
    uint8_t cmd = (is_inverse ? 0xA7 : 0xA6);

    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, cmd);
}


static void __disp_i2c_ssd1306_display_on(TUYA_I2C_NUM_E port, uint8_t slave_addr)
{
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, 0x8D);
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, 0x14);
    __disp_i2c_write_one_byte(port, slave_addr, SSD1306_CMD_REG, 0xAF);
}

static OPERATE_RET __tdd_disp_i2c_ssd1306_open(TDD_DISP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SSD1306_DEV_T *disp_spi_dev = NULL;

    disp_spi_dev = (DISP_SSD1306_DEV_T *)device;

    if(NULL == disp_spi_dev) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(__disp_i2c_init(disp_spi_dev->port));

    for(uint8_t i = 0; i < sizeof(cSSD1306_INIT_SEQ); i++) {
        __disp_i2c_write_one_byte(disp_spi_dev->port, disp_spi_dev->slave_addr, SSD1306_CMD_REG, cSSD1306_INIT_SEQ[i]);
    }

    __disp_i2c_ssd1306_multiplex_ratio(disp_spi_dev->port, disp_spi_dev->slave_addr, disp_spi_dev->disp_info.height);

    __disp_i2c_ssd1306_com_pin_cfg(disp_spi_dev->port, disp_spi_dev->slave_addr, disp_spi_dev->init_cfg.com_pin_cfg);

    __disp_i2c_ssd1306_set_color_inverse(disp_spi_dev->port, disp_spi_dev->slave_addr, disp_spi_dev->init_cfg.is_color_inverse);

    __disp_i2c_ssd1306_display_on(disp_spi_dev->port, disp_spi_dev->slave_addr);

    PR_NOTICE("[SSD1306] Initialize display device successful.");

    return rt;
}

static OPERATE_RET __tdd_disp_i2c_ssd1306_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SSD1306_DEV_T *disp_spi_dev = NULL;
    uint16_t j=0, i=0, sizey = 0;

    disp_spi_dev = (DISP_SSD1306_DEV_T *)device;

    if(NULL == disp_spi_dev || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    __tdd_ssd1306_convert(disp_spi_dev->disp_info.width, disp_spi_dev->disp_info.height,
                          frame_buff->frame, disp_spi_dev->convert_fb->frame);

    sizey = (disp_spi_dev->disp_info.height + 7) / 8;

    for(i=0; i<sizey; i++) {
        __disp_i2c_ssd1306_set_pos(disp_spi_dev->port, disp_spi_dev->slave_addr, 0, i);
        __disp_i2c_write_data(disp_spi_dev->port, disp_spi_dev->slave_addr, SSD1306_DATA_REG,\
                              disp_spi_dev->convert_fb->frame + j, disp_spi_dev->disp_info.width);
        j += disp_spi_dev->disp_info.width;
    }

    if(frame_buff && frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return rt;
}

static OPERATE_RET __tdd_disp_i2c_ssd1306_close(TDD_DISP_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief Registers an SSD1306 OLED display device over I2C with the display management system.
 *
 * This function creates and initializes a new SSD1306 OLED display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the I2C OLED device configuration structure.
 * @param init_cfg Pointer to the SSD1306-specific initialization configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_i2c_oled_ssd1306_register(char *name, DISP_I2C_OLED_DEVICE_CFG_T *dev_cfg,\
                                               DISP_SSD1306_INIT_CFG_T *init_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SSD1306_DEV_T *disp_spi_dev = NULL;
    uint32_t frame_len = 0;

    if(NULL == name || NULL == dev_cfg || NULL == init_cfg) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = (DISP_SSD1306_DEV_T *)tal_malloc(sizeof(DISP_SSD1306_DEV_T));
    if(NULL == disp_spi_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(disp_spi_dev, 0, sizeof(DISP_SSD1306_DEV_T));

    frame_len = dev_cfg->width  * ((dev_cfg->height + 7) / 8);
    disp_spi_dev->convert_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == disp_spi_dev->convert_fb) {
        return OPRT_MALLOC_FAILED;
    }

    disp_spi_dev->port        = dev_cfg->port;
    disp_spi_dev->slave_addr  = dev_cfg->addr;

    memcpy(&disp_spi_dev->init_cfg, init_cfg, sizeof(DISP_SSD1306_INIT_CFG_T));

    disp_spi_dev->disp_info.type       = TUYA_DISPLAY_I2C;
    disp_spi_dev->disp_info.fmt        = TUYA_PIXEL_FMT_MONOCHROME;
    disp_spi_dev->disp_info.width      = dev_cfg->width;
    disp_spi_dev->disp_info.height     = dev_cfg->height;
    disp_spi_dev->disp_info.rotation   = dev_cfg->rotation;
    disp_spi_dev->disp_info.is_swap    = false;
    disp_spi_dev->disp_info.has_vram   = true;

    memcpy(&disp_spi_dev->disp_info.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&disp_spi_dev->disp_info.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    TDD_DISP_INTFS_T disp_spi_intfs = {
        .open  = __tdd_disp_i2c_ssd1306_open,
        .flush = __tdd_disp_i2c_ssd1306_flush,
        .close = __tdd_disp_i2c_ssd1306_close,
    };

    TUYA_CALL_ERR_RETURN(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_spi_dev, \
                                                  &disp_spi_intfs, &disp_spi_dev->disp_info));

    PR_NOTICE("tdd_disp_i2c_ssd1306_register: %s", name);

    return rt;
}



#endif