/**
 * @file atk_dnesp32s3.c
 * @brief ATK-DNESP32S3 module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#include "tuya_cloud_types.h"

#include "app_board_api.h"

#include "board_config.h"
#include "oled_display.h"

#include "tal_log.h"
#include "tdd_audio_codec_bus.h"
#include "tdd_audio_es8388_codec.h"
#include "tdd_xl9555_io.h"

static TDD_AUDIO_I2C_HANDLE i2c_bus_handle = NULL;
static TDD_AUDIO_I2S_TX_HANDLE i2s_tx_handle = NULL;
static TDD_AUDIO_I2S_RX_HANDLE i2s_rx_handle = NULL;

int app_audio_driver_init(const char *name)
{
    TDD_AUDIO_CODEC_BUS_CFG_T bus_cfg = {
        .i2c_id = I2C_NUM,
        .i2c_sda_io = I2C_SDA_IO,
        .i2c_scl_io = I2C_SCL_IO,
        .i2s_id = I2S_NUM,
        .i2s_mck_io = I2S_MCK_IO,
        .i2s_bck_io = I2S_BCK_IO,
        .i2s_ws_io = I2S_WS_IO,
        .i2s_do_io = I2S_DO_IO,
        .i2s_di_io = I2S_DI_IO,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .sample_rate = I2S_OUTPUT_SAMPLE_RATE,
    };
    
    tdd_audio_codec_bus_i2c_new(bus_cfg, &i2c_bus_handle);
    tdd_audio_codec_bus_i2s_new(bus_cfg, &i2s_tx_handle, &i2s_rx_handle);
    
    /* P10, P11, P12, P13, and P14 are inputs, other pins are outputs --> 0001 1111 0000 0000 Note: 0 is output, 1 is input */
    tdd_xl9555_io_init(i2c_bus_handle, 0xF003);
    /* Turn off buzzer */
    tdd_xl9555_io_set(BEEP_IO, 1);
    /* Turn on Speaker */
    tdd_xl9555_io_set(SPK_EN_IO, 0);

    TDD_AUDIO_ES8388_CODEC_T codec = {
        .i2c_id = I2C_NUM,
        .i2c_handle = i2c_bus_handle,
        .i2s_id = I2S_NUM,
        .i2s_tx_handle = i2s_tx_handle,
        .i2s_rx_handle = i2s_rx_handle,
        .mic_sample_rate = I2S_INPUT_SAMPLE_RATE,
        .spk_sample_rate = I2S_OUTPUT_SAMPLE_RATE,
        .es8388_addr = AUDIO_CODEC_ES8388_ADDR,
        .pa_pin = -1, /* Speaker power is controled by XL9555 */
        .defaule_volume = 80,
    };
    return tdd_audio_es8388_codec_register(name, codec);
}

OPERATE_RET app_display_init(void)
{
    return OPRT_OK;
}

OPERATE_RET app_display_send_msg(TY_DISPLAY_TYPE_E tp, uint8_t *data, int len)
{
    return OPRT_OK;
}
