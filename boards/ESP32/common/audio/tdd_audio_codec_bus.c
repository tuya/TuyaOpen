#include "esp_check.h"

#include "driver/i2c_master.h"
#include "driver/i2s_std.h"

#include "tdd_audio_codec_bus.h"

OPERATE_RET tdd_audio_codec_bus_i2c_new(TDD_AUDIO_CODEC_BUS_CFG_T cfg, TDD_AUDIO_I2C_HANDLE *handle)
{
    i2c_master_bus_handle_t i2c_bus_handle = NULL;

    // Initialize I2C bus
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = cfg.i2c_id,
        .sda_io_num = cfg.i2c_sda_io,
        .scl_io_num = cfg.i2c_scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_handle));

    *handle = (TDD_AUDIO_I2C_HANDLE)i2c_bus_handle;

    return OPRT_OK;
}

OPERATE_RET tdd_audio_codec_bus_i2s_new(TDD_AUDIO_CODEC_BUS_CFG_T cfg, TDD_AUDIO_I2S_TX_HANDLE *tx_handle, TDD_AUDIO_I2S_RX_HANDLE *rx_handle)
{
    i2s_chan_handle_t i2s_tx_handle = NULL;
    i2s_chan_handle_t i2s_rx_handle = NULL;

    // Initialize I2S channels
    i2s_chan_config_t chan_cfg = {
        .id = cfg.i2s_id,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = cfg.dma_desc_num,
        .dma_frame_num = cfg.dma_frame_num,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &i2s_tx_handle, &i2s_rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = cfg.sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = cfg.i2s_mck_io,
            .bclk = cfg.i2s_bck_io,
            .ws = cfg.i2s_ws_io,
            .dout = cfg.i2s_do_io,
            .din = cfg.i2s_di_io,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_rx_handle, &std_cfg));

    *tx_handle = (TDD_AUDIO_I2S_TX_HANDLE)i2s_tx_handle;
    *rx_handle = (TDD_AUDIO_I2S_RX_HANDLE)i2s_rx_handle;

    return OPRT_OK;
}