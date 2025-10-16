/**
 * @file tdd_camera_ov2640_init_seq.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_OV2640_INIT_SEQ_H__
#define __TDD_CAMERA_OV2640_INIT_SEQ_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define R_BYPASS   0x05
#define QS         0x44
#define CTRLI      0x50
#define HSIZE      0x51
#define VSIZE      0x52
#define XOFFL      0x53
#define YOFFL      0x54
#define VHYX       0x55
#define DPRP       0x56
#define TEST       0x57
#define ZMOW       0x5A
#define ZMOH       0x5B
#define ZMHH       0x5C
#define BPADDR     0x7C
#define BPDATA     0x7D
#define CTRL2      0x86
#define CTRL3      0x87
#define SIZEL      0x8C
#define HSIZE8     0xC0
#define VSIZE8     0xC1
#define CTRL0      0xC2
#define CTRL1      0xC3
#define R_DVP_SP   0xD3
#define IMAGE_MODE 0xDA
#define RESET      0xE0
#define MS_SP      0xF0
#define SS_ID      0xF7
#define SS_CTRL    0xF7
#define MC_BIST    0xF9
#define MC_AL      0xFA
#define MC_AH      0xFB
#define MC_D       0xFC
#define P_CMD      0xFD
#define P_STATUS   0xFE
#define BANK_SEL   0xFF

#define CTRLI_LP_DP 0x80
#define CTRLI_ROUND 0x40

#define CTRL0_AEC_EN   0x80
#define CTRL0_AEC_SEL  0x40
#define CTRL0_STAT_SEL 0x20
#define CTRL0_VFIRST   0x10
#define CTRL0_YUV422   0x08
#define CTRL0_YUV_EN   0x04
#define CTRL0_RGB_EN   0x02
#define CTRL0_RAW_EN   0x01

#define CTRL2_DCW_EN    0x20
#define CTRL2_SDE_EN    0x10
#define CTRL2_UV_ADJ_EN 0x08
#define CTRL2_UV_AVG_EN 0x04
#define CTRL2_CMX_EN    0x01

#define CTRL3_BPC_EN 0x80
#define CTRL3_WPC_EN 0x40

#define R_DVP_SP_AUTO_MODE 0x80

#define R_BYPASS_DSP_EN    0x00
#define R_BYPASS_DSP_BYPAS 0x01

#define IMAGE_MODE_Y8_DVP_EN   0x40
#define IMAGE_MODE_JPEG_EN     0x10
#define IMAGE_MODE_YUV422      0x00
#define IMAGE_MODE_RAW10       0x04
#define IMAGE_MODE_RGB565      0x08
#define IMAGE_MODE_HREF_VSYNC  0x02
#define IMAGE_MODE_LBYTE_FIRST 0x01

#define RESET_MICROC 0x40
#define RESET_SCCB   0x20
#define RESET_JPEG   0x10
#define RESET_DVP    0x04
#define RESET_IPU    0x02
#define RESET_CIF    0x01

#define MC_BIST_RESET           0x80
#define MC_BIST_BOOT_ROM_SEL    0x40
#define MC_BIST_12KB_SEL        0x20
#define MC_BIST_12KB_MASK       0x30
#define MC_BIST_512KB_SEL       0x08
#define MC_BIST_512KB_MASK      0x0C
#define MC_BIST_BUSY_BIT_R      0x02
#define MC_BIST_MC_RES_ONE_SH_W 0x02
#define MC_BIST_LAUNCH          0x01

typedef enum { BANK_DSP, BANK_SENSOR, BANK_MAX } ov2640_bank_t;

/* Sensor register bank FF=0x01*/
#define GAIN       0x00
#define COM1       0x03
#define REG04      0x04
#define REG08      0x08
#define COM2       0x09
#define REG_PID    0x0A
#define REG_VER    0x0B
#define COM3       0x0C
#define COM4       0x0D
#define AEC        0x10
#define CLKRC      0x11
#define COM7       0x12
#define COM8       0x13
#define COM9       0x14 /* AGC gain ceiling */
#define COM10      0x15
#define HSTART     0x17
#define HSTOP      0x18
#define VSTART     0x19
#define VSTOP      0x1A
#define REG_MIDH   0x1C
#define REG_MIDL   0x1D
#define AEW        0x24
#define AEB        0x25
#define VV         0x26
#define REG2A      0x2A
#define FRARL      0x2B
#define ADDVSL     0x2D
#define ADDVSH     0x2E
#define YAVG       0x2F
#define HSDY       0x30
#define HEDY       0x31
#define REG32      0x32
#define ARCOM2     0x34
#define REG45      0x45
#define FLL        0x46
#define FLH        0x47
#define COM19      0x48
#define ZOOMS      0x49
#define COM22      0x4B
#define COM25      0x4E
#define BD50       0x4F
#define BD60       0x50
#define REG5D      0x5D
#define REG5E      0x5E
#define REG5F      0x5F
#define REG60      0x60
#define HISTO_LOW  0x61
#define HISTO_HIGH 0x62

#define REG04_DEFAULT   0x28
#define REG04_HFLIP_IMG 0x80
#define REG04_VFLIP_IMG 0x40
#define REG04_VREF_EN   0x10
#define REG04_HREF_EN   0x08
#define REG04_SET(x)    (REG04_DEFAULT | x)

#define COM2_STDBY        0x10
#define COM2_OUT_DRIVE_1x 0x00
#define COM2_OUT_DRIVE_2x 0x01
#define COM2_OUT_DRIVE_3x 0x02
#define COM2_OUT_DRIVE_4x 0x03

#define COM3_DEFAULT     0x38
#define COM3_BAND_50Hz   0x04
#define COM3_BAND_60Hz   0x00
#define COM3_BAND_AUTO   0x02
#define COM3_BAND_SET(x) (COM3_DEFAULT | x)

#define COM7_SRST      0x80
#define COM7_RES_UXGA  0x00 /* UXGA */
#define COM7_RES_SVGA  0x40 /* SVGA */
#define COM7_RES_CIF   0x20 /* CIF  */
#define COM7_ZOOM_EN   0x04 /* Enable Zoom */
#define COM7_COLOR_BAR 0x02 /* Enable Color Bar Test */

#define COM8_DEFAULT 0xC0
#define COM8_BNDF_EN 0x20 /* Enable Banding filter */
#define COM8_AGC_EN  0x04 /* AGC Auto/Manual control selection */
#define COM8_AEC_EN  0x01 /* Auto/Manual Exposure control */
#define COM8_SET(x)  (COM8_DEFAULT | x)

#define COM9_DEFAULT       0x08
#define COM9_AGC_GAIN_2x   0x00 /* AGC:    2x */
#define COM9_AGC_GAIN_4x   0x01 /* AGC:    4x */
#define COM9_AGC_GAIN_8x   0x02 /* AGC:    8x */
#define COM9_AGC_GAIN_16x  0x03 /* AGC:   16x */
#define COM9_AGC_GAIN_32x  0x04 /* AGC:   32x */
#define COM9_AGC_GAIN_64x  0x05 /* AGC:   64x */
#define COM9_AGC_GAIN_128x 0x06 /* AGC:  128x */
#define COM9_AGC_SET(x)    (COM9_DEFAULT | (x << 5))

#define COM10_HREF_EN   0x80 /* HSYNC changes to HREF */
#define COM10_HSYNC_EN  0x40 /* HREF changes to HSYNC */
#define COM10_PCLK_FREE 0x20 /* PCLK output option: free running PCLK */
#define COM10_PCLK_EDGE 0x10 /* Data is updated at the rising edge of PCLK */
#define COM10_HREF_NEG  0x08 /* HREF negative */
#define COM10_VSYNC_NEG 0x02 /* VSYNC negative */
#define COM10_HSYNC_NEG 0x01 /* HSYNC negative */

#define CTRL1_AWB 0x08 /* Enable AWB */

#define VV_AGC_TH_SET(h, l) ((h << 4) | (l & 0x0F))

#define REG32_UXGA 0x36
#define REG32_SVGA 0x09
#define REG32_CIF  0x89

#define CLKRC_2X      0x80
#define CLKRC_2X_UXGA (0x01 | CLKRC_2X)
#define CLKRC_2X_SVGA CLKRC_2X
#define CLKRC_2X_CIF  CLKRC_2X


/***********************************************************
***********************const define***********************
***********************************************************/

const uint8_t cOV2640_INIT_TAB[][2] = {
    {BANK_SEL, BANK_DSP},
    {0x2c, 0xff},
    {0x2e, 0xdf},
    {BANK_SEL, BANK_SENSOR},
    {0x3c, 0x32},
    {CLKRC, 0x01},
    {COM2, COM2_OUT_DRIVE_3x},
    {REG04, REG04_DEFAULT},
    {COM8, COM8_DEFAULT | COM8_BNDF_EN | COM8_AGC_EN | COM8_AEC_EN},
    {COM9, COM9_AGC_SET(COM9_AGC_GAIN_2x)},
    {0x2c, 0x0c},
    {0x33, 0x78},
    {0x3a, 0x33},
    {0x3b, 0xfB},
    {0x3e, 0x00},
    {0x43, 0x11},
    {0x16, 0x10},
    {0x39, 0x92},
    {0x35, 0xda},
    {0x22, 0x1a},
    {0x37, 0xc3},
    {0x23, 0x00},
    {ARCOM2, 0xc0},
    {0x06, 0x88},
    {0x07, 0xc0},
    {COM4, 0x87},
    {0x0e, 0x41},
    {0x4c, 0x00},
    {0x4a, 0x81},
    {0x21, 0x99},
    {AEW, 0x40},
    {AEB, 0x38},
    {VV, VV_AGC_TH_SET(8, 2)},
    {0x5c, 0x00},
    {0x63, 0x00},
    {HISTO_LOW, 0x70},
    {HISTO_HIGH, 0x80},
    {0x7c, 0x05},
    {0x20, 0x80},
    {0x28, 0x30},
    {0x6c, 0x00},
    {0x6d, 0x80},
    {0x6e, 0x00},
    {0x70, 0x02},
    {0x71, 0x94},
    {0x73, 0xc1},
    {0x3d, 0x34},
    {0x5a, 0x57},
    {BD50, 0xbb},
    {BD60, 0x9c},
    {COM7, COM7_RES_CIF},
    {HSTART, 0x11},
    {HSTOP, 0x43},
    {VSTART, 0x00},
    {VSTOP, 0x25},
    {REG32, 0x89},
    {0x37, 0xc0},
    {BD50, 0xca},
    {BD60, 0xa8},
    {0x6d, 0x00},
    {0x3d, 0x38},
    {BANK_SEL, BANK_DSP},
    {0xe5, 0x7f},
    {MC_BIST, MC_BIST_RESET | MC_BIST_BOOT_ROM_SEL},
    {0x41, 0x24},
    {RESET, RESET_JPEG | RESET_DVP},
    {0x76, 0xff},
    {0x33, 0xa0},
    {0x42, 0x20},
    {0x43, 0x18},
    {0x4c, 0x00},
    {CTRL3, 0xD5},
    {0x88, 0x3f},
    {0xd7, 0x03},
    {0xd9, 0x10},
    {R_DVP_SP, R_DVP_SP_AUTO_MODE | 0x02},
    {0xc8, 0x08},
    {0xc9, 0x80},
    {BPADDR, 0x00},
    {BPDATA, 0x00},
    {BPADDR, 0x03},
    {BPDATA, 0x48},
    {BPDATA, 0x48},
    {BPADDR, 0x08},
    {BPDATA, 0x20},
    {BPDATA, 0x10},
    {BPDATA, 0x0e},
    {0x90, 0x00},
    {0x91, 0x0e},
    {0x91, 0x1a},
    {0x91, 0x31},
    {0x91, 0x5a},
    {0x91, 0x69},
    {0x91, 0x75},
    {0x91, 0x7e},
    {0x91, 0x88},
    {0x91, 0x8f},
    {0x91, 0x96},
    {0x91, 0xa3},
    {0x91, 0xaf},
    {0x91, 0xc4},
    {0x91, 0xd7},
    {0x91, 0xe8},
    {0x91, 0x20},
    {0x92, 0x00},
    {0x93, 0x06},
    {0x93, 0xe3},
    {0x93, 0x05},
    {0x93, 0x05},
    {0x93, 0x00},
    {0x93, 0x04},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x96, 0x00},
    {0x97, 0x08},
    {0x97, 0x19},
    {0x97, 0x02},
    {0x97, 0x0c},
    {0x97, 0x24},
    {0x97, 0x30},
    {0x97, 0x28},
    {0x97, 0x26},
    {0x97, 0x02},
    {0x97, 0x98},
    {0x97, 0x80},
    {0x97, 0x00},
    {0x97, 0x00},
    {0xa4, 0x00},
    {0xa8, 0x00},
    {0xc5, 0x11},
    {0xc6, 0x51},
    {0xbf, 0x80},
    {0xc7, 0x10},
    {0xb6, 0x66},
    {0xb8, 0xA5},
    {0xb7, 0x64},
    {0xb9, 0x7C},
    {0xb3, 0xaf},
    {0xb4, 0x97},
    {0xb5, 0xFF},
    {0xb0, 0xC5},
    {0xb1, 0x94},
    {0xb2, 0x0f},
    {0xc4, 0x5c},
    {CTRL1, 0xef},
    {0x7f, 0x00},
    {0xe5, 0x1f},
    {0xe1, 0x67},
    {0xdd, 0x7f},
    {IMAGE_MODE, IMAGE_MODE_YUV422},
    {RESET, 0x00},
    {R_BYPASS, R_BYPASS_DSP_EN},
    {0, 0},

    //
    // set to uxga
    //
    {BANK_SEL, BANK_SENSOR},
    {COM7, COM7_RES_UXGA},

    // Set the sensor output window
    {COM1, 0x0F},
    {REG32, REG32_UXGA},
    {HSTART, 0x11},
    {HSTOP, 0x75},
    {VSTART, 0x01},
    {VSTOP, 0x97},

    //{CLKRC, 0x00},
    {0x3d, 0x34},
    {BD50, 0xbb},
    {BD60, 0x9c},
    {0x5a, 0x57},
    {0x6d, 0x80},
    {0x39, 0x82},
    {0x23, 0x00},
    {0x07, 0xc0},
    {0x4c, 0x00},
    {0x35, 0x88},
    {0x22, 0x0a},
    {0x37, 0x40},
    {ARCOM2, 0xa0},
    {0x06, 0x02},
    {COM4, 0xb7},
    {0x0e, 0x01},
    {0x42, 0x83},
    {BANK_SEL, BANK_DSP},
    {RESET, RESET_DVP},

    // Set the sensor resolution (UXGA, SVGA, CIF)
    {HSIZE8, 0xc8},
    {VSIZE8, 0x96},
    {SIZEL, 0x00},

    // Set the image window size >= output size
    {HSIZE, 0x90},
    {VSIZE, 0x2c},
    {XOFFL, 0x00},
    {YOFFL, 0x00},
    {VHYX, 0x88},
    {TEST, 0x00},

    {CTRL2, CTRL2_DCW_EN | 0x1d},
    {CTRLI, 0x00},
    //{R_DVP_SP, 0x06},
    {0, 0},

    //
    // yuv422
    //
    {BANK_SEL, BANK_DSP},
    {RESET, RESET_DVP},
    {IMAGE_MODE, IMAGE_MODE_YUV422},
    {0xD7, 0x01},
    {0xE1, 0x67},
    {RESET, 0x00},
    {0, 0},

    //
    // agc
    //
    {BANK_SEL, BANK_DSP},
    {CTRL3, 0xD5},
    {CTRL1, 0xED},
};

const uint8_t cOV2640_1280_720_TAB[][2] = {
    {0xff, 0x00},
    {0xe0, 0x04},
    // UXGA（1600×1200）
    {0xc0, 0xc8},
    {0xc1, 0x96},
    // HSIZE 1280/4 -> 320 -> 0x0140
    {0x51, 0x40},
    // VSIZE 720/4 -> 180 -> 0xB4
    {0x52, 0xB4},
    // 1280×720 as ROI
    // XOFF 160 -> 0xA0
    {0x53, 0xA0},
    // YOFF 240 -> 0xF0
    {0x54, 0xF0},
    // VHYX
    {0x55, 0x08},
    {0x57, 0x00},
    // OUT W H
    // 1280*720
    // 320*180 -> 0x0140*0xB4
    {0x5A, 0x40},
    {0x5B, 0xB4},
    {0x5C, 0x01},
    {0xd3, 0x82},
    {0xe0, 0x00},
};

const uint8_t cOV2640_480_480_TAB[][2] = {
    {0xff, 0x00},
    {0xe0, 0x04},
    // UXGA（1600×1200）
    {0xc0, 0xc8},
    {0xc1, 0x96},
    // HSIZE 960/4 -> 240 -> 0xF0
    {0x51, 0xF0},
    // VSIZE 960/4 -> 240 -> 0xF0
    {0x52, 0xF0},
    // 960×960 as ROI
    // XOFF 320 -> 0x0140
    {0x53, 0x40},
    // YOFF 120 -> 0x78
    {0x54, 0x78},
    // VHYX
    {0x55, 0x01},
    // OUT W H
    // 480*480 -> 120*120
    {0x5A, 0x78},
    {0x5B, 0x78},
    {0x5C, 0x00},
    {0xd3, 0x82},
    {0xe0, 0x00},
};

const uint8_t cOV2640_240_240_TAB[][2] = {
    {0xff, 0x00},
    {0xe0, 0x04},
    // UXGA（1600×1200）
    {0xc0, 0xc8},
    {0xc1, 0x96},
    // HSIZE 960/4 -> 240 -> 0xF0
    {0x51, 0xF0},
    // VSIZE 960/4 -> 240 -> 0xF0
    {0x52, 0xF0},
    // 960×960 as ROI
    // XOFF 320 -> 0x0140
    {0x53, 0x40},
    // YOFF 120 -> 0x78
    {0x54, 0x78},
    // VHYX
    {0x55, 0x01},
    // OUT W H
    // 240*240 -> 60*60
    {0x5A, 0x3C},
    {0x5B, 0x3C},
    {0x5C, 0x00},
    {0xd3, 0x82},
    {0xe0, 0x00},
};


/***********************************************************
********************function declaration********************
***********************************************************/


#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_OV2640_INIT_SEQ_H__ */
