/**
 * @file tdd_audio_ty_uart.c
 * @brief Generic TDD audio driver for voice chips implementing the Tuya
 *        offline voice UART protocol.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_TDD_AUDIO_TY_UART) && (ENABLE_TDD_AUDIO_TY_UART == 1)

#include "tdd_audio_ty_uart.h"
#include <stdio.h>

#define TY_UART_VOICE_HEAD_0                 0x55U
#define TY_UART_VOICE_HEAD_1                 0xAAU
#define TY_UART_VOICE_CMD                    0x92U
#define TY_UART_VOICE_FRAME_OVERHEAD         7U
#define TY_UART_VOICE_DATA_MAX               2048U
#define TY_UART_VOICE_FRAME_MAX              \
    (TY_UART_VOICE_FRAME_OVERHEAD + TY_UART_VOICE_DATA_MAX)
#define TY_UART_VOICE_RX_TIMEOUT_MS          100U
#define TY_UART_SPK_FLOW_TIMEOUT_MS          6000U

#define TY_UART_VOICE_VER_MODULE             0x00U
#define TY_UART_VOICE_VER_CHIP               0x03U

#define TY_UART_VOICE_SUBCMD_VERSION         0x01U
#define TY_UART_VOICE_SUBCMD_MIC_CFG         0x02U
#define TY_UART_VOICE_SUBCMD_SPK_CFG         0x03U
#define TY_UART_VOICE_SUBCMD_MIC_DATA        0x05U
#define TY_UART_VOICE_SUBCMD_SPK_PLAY        0x06U
#define TY_UART_VOICE_SUBCMD_VOICE_CFG       0x07U
#define TY_UART_VOICE_SUBCMD_WAKEUP          0x08U
#define TY_UART_VOICE_SUBCMD_SLEEP           0x09U
#define TY_UART_VOICE_SUBCMD_EXT_WAKEUP      0x1AU

#define TY_UART_VOICE_MIC_STATUS_START       0U
#define TY_UART_VOICE_MIC_STATUS_DATA        1U
#define TY_UART_VOICE_MIC_STATUS_STOP        2U

#define TY_UART_VOICE_SPK_STAGE_START        0U
#define TY_UART_VOICE_SPK_STAGE_DATA         1U
#define TY_UART_VOICE_SPK_STAGE_STOP         2U

#define TY_UART_VOICE_CFG_MIC_SWITCH         0x01U
#define TY_UART_VOICE_CFG_SPK_SWITCH         0x02U
#define TY_UART_VOICE_CFG_WAKE_SWITCH        0x03U
#define TY_UART_VOICE_CFG_VAD                0x04U


typedef struct {
    uint8_t version;
    uint8_t cmd;
    uint16_t data_len;
    uint8_t data[TY_UART_VOICE_DATA_MAX];
} TY_UART_VOICE_FRAME_T;

typedef void (*TY_UART_VOICE_FRAME_CB)(const TY_UART_VOICE_FRAME_T *frame,
                                      void *user_data);

typedef struct {
    uint8_t state;
    uint8_t checksum_rx;
    uint8_t checksum_calc;
    uint16_t data_len;
    uint16_t data_pos;
    uint32_t last_byte_ms;
    TY_UART_VOICE_FRAME_T frame;
} TY_UART_VOICE_PARSER_T;

static void __voice_parser_init(TY_UART_VOICE_PARSER_T *parser);
static bool __voice_parser_feed(TY_UART_VOICE_PARSER_T *parser, uint8_t byte,
                               uint32_t now_ms, TY_UART_VOICE_FRAME_CB callback,
                               void *user_data);
static size_t __voice_frame_build(uint8_t version, const uint8_t *data,
                                 uint16_t data_len, uint8_t *out,
                                 size_t out_size);

typedef enum {
    TY_UART_VOICE_RX_HEAD_0 = 0,
    TY_UART_VOICE_RX_HEAD_1,
    TY_UART_VOICE_RX_VERSION,
    TY_UART_VOICE_RX_CMD,
    TY_UART_VOICE_RX_LEN_HIGH,
    TY_UART_VOICE_RX_LEN_LOW,
    TY_UART_VOICE_RX_DATA,
    TY_UART_VOICE_RX_CHECKSUM,
} TY_UART_VOICE_RX_STATE_E;

static uint8_t __checksum_add(uint8_t checksum, uint8_t byte)
{
    return (uint8_t)(checksum + byte);
}

static void __parser_reset(TY_UART_VOICE_PARSER_T *parser)
{
    memset(parser, 0, sizeof(*parser));
    parser->state = TY_UART_VOICE_RX_HEAD_0;
}

static void __parser_restart(TY_UART_VOICE_PARSER_T *parser, uint8_t byte,
                             uint32_t now_ms)
{
    __parser_reset(parser);
    parser->last_byte_ms = now_ms;
    if (byte == TY_UART_VOICE_HEAD_0) {
        parser->checksum_calc = byte;
        parser->state = TY_UART_VOICE_RX_HEAD_1;
    }
}

static void __voice_parser_init(TY_UART_VOICE_PARSER_T *parser)
{
    if (parser != NULL) {
        __parser_reset(parser);
    }
}

static bool __voice_parser_feed(TY_UART_VOICE_PARSER_T *parser, uint8_t byte,
                               uint32_t now_ms, TY_UART_VOICE_FRAME_CB callback,
                               void *user_data)
{
    bool complete = false;

    if (parser == NULL) {
        return false;
    }

    if (parser->state != TY_UART_VOICE_RX_HEAD_0 &&
        (uint32_t)(now_ms - parser->last_byte_ms) > TY_UART_VOICE_RX_TIMEOUT_MS) {
        __parser_restart(parser, byte, now_ms);
        return false;
    }
    parser->last_byte_ms = now_ms;

    switch ((TY_UART_VOICE_RX_STATE_E)parser->state) {
    case TY_UART_VOICE_RX_HEAD_0:
        if (byte == TY_UART_VOICE_HEAD_0) {
            parser->checksum_calc = byte;
            parser->state = TY_UART_VOICE_RX_HEAD_1;
        }
        break;

    case TY_UART_VOICE_RX_HEAD_1:
        if (byte == TY_UART_VOICE_HEAD_1) {
            parser->checksum_calc = __checksum_add(parser->checksum_calc, byte);
            parser->state = TY_UART_VOICE_RX_VERSION;
        } else if (byte != TY_UART_VOICE_HEAD_0) {
            __parser_reset(parser);
        } else {
            parser->checksum_calc = TY_UART_VOICE_HEAD_0;
        }
        break;

    case TY_UART_VOICE_RX_VERSION:
        parser->frame.version = byte;
        parser->checksum_calc = __checksum_add(parser->checksum_calc, byte);
        parser->state = TY_UART_VOICE_RX_CMD;
        break;

    case TY_UART_VOICE_RX_CMD:
        parser->frame.cmd = byte;
        parser->checksum_calc = __checksum_add(parser->checksum_calc, byte);
        parser->state = TY_UART_VOICE_RX_LEN_HIGH;
        break;

    case TY_UART_VOICE_RX_LEN_HIGH:
        parser->data_len = (uint16_t)((uint16_t)byte << 8);
        parser->checksum_calc = __checksum_add(parser->checksum_calc, byte);
        parser->state = TY_UART_VOICE_RX_LEN_LOW;
        break;

    case TY_UART_VOICE_RX_LEN_LOW:
        parser->data_len |= byte;
        parser->checksum_calc = __checksum_add(parser->checksum_calc, byte);
        if (parser->data_len > TY_UART_VOICE_DATA_MAX) {
            __parser_restart(parser, byte, now_ms);
            break;
        }
        parser->frame.data_len = parser->data_len;
        parser->data_pos = 0;
        parser->state = parser->data_len == 0U ?
                        TY_UART_VOICE_RX_CHECKSUM : TY_UART_VOICE_RX_DATA;
        break;

    case TY_UART_VOICE_RX_DATA:
        parser->frame.data[parser->data_pos++] = byte;
        parser->checksum_calc = __checksum_add(parser->checksum_calc, byte);
        if (parser->data_pos >= parser->data_len) {
            parser->state = TY_UART_VOICE_RX_CHECKSUM;
        }
        break;

    case TY_UART_VOICE_RX_CHECKSUM:
        parser->checksum_rx = byte;
        if (parser->checksum_rx == parser->checksum_calc) {
            if (callback != NULL) {
                callback(&parser->frame, user_data);
            }
            complete = true;
            __parser_reset(parser);
        } else {
            __parser_restart(parser, byte, now_ms);
        }
        break;

    default:
        __parser_restart(parser, byte, now_ms);
        break;
    }

    return complete;
}

static size_t __voice_frame_build(uint8_t version, const uint8_t *data,
                                 uint16_t data_len, uint8_t *out,
                                 size_t out_size)
{
    size_t offset = 0;
    size_t frame_size;
    uint8_t checksum = 0;

    if (out == NULL || data_len > TY_UART_VOICE_DATA_MAX ||
        (data_len > 0U && data == NULL)) {
        return 0U;
    }

    frame_size = TY_UART_VOICE_FRAME_OVERHEAD + data_len;
    if (out_size < frame_size) {
        return 0U;
    }

    out[offset++] = TY_UART_VOICE_HEAD_0;
    out[offset++] = TY_UART_VOICE_HEAD_1;
    out[offset++] = version;
    out[offset++] = TY_UART_VOICE_CMD;
    out[offset++] = (uint8_t)(data_len >> 8);
    out[offset++] = (uint8_t)(data_len & 0xFFU);
    if (data_len > 0U) {
        memcpy(out + offset, data, data_len);
        offset += data_len;
    }

    for (size_t i = 0; i < offset; ++i) {
        checksum = __checksum_add(checksum, out[i]);
    }
    out[offset++] = checksum;

    return offset;
}


#include "tal_api.h"
#include "tal_memory.h"
#include "tkl_gpio.h"
#include "uart_hooks.h"

#define TY_UART_DEFAULT_BAUDRATE             921600U
#define TY_UART_DEFAULT_BOOT_DELAY_MS        300U
#define TY_UART_DEFAULT_CAPTURE_RATE         16000U
#define TY_UART_DEFAULT_PLAYBACK_RATE        16000U
#define TY_UART_DEFAULT_SAMPLE_CHANNELS      1U
#define TY_UART_DEFAULT_SAMPLE_BITS          16U
#define TY_UART_DEFAULT_FRAME_MS             20U
#define TY_UART_DEFAULT_VOLUME               50U
#define TY_UART_DEFAULT_MIC_GAIN_DB          26U
#define TY_UART_DEFAULT_CAPTURE_PACKET_MAX   640U
#define TY_UART_DEFAULT_PLAYBACK_PACKET_MAX  1152U
#define TY_UART_RX_BUFFER_SIZE               8192U
#define TY_UART_RX_POLL_MS                   2U
#define TY_UART_THREAD_EXIT_WAIT_MS          1000U
#define TY_UART_LINK_RETRY_MS                100U
#define TY_UART_LINK_RETRY_COUNT             50U
#define TY_UART_SPK_SEND_YIELD_MS           10U
#define TY_UART_SPK_PAYLOAD_MAX              TY_UART_VOICE_DATA_MAX
#define TY_UART_MIC_EVENT_DATA_MAX           640U
#define TY_UART_MIC_EVENT_QUEUE_DEPTH        12U

typedef struct {
    TDL_AUDIO_STATUS_E status;
    uint16_t len;
    uint8_t data[TY_UART_MIC_EVENT_DATA_MAX];
} TY_UART_MIC_EVENT_T;

typedef struct {
    TDL_AUDIO_EVENT_E event;
    uint32_t param;
    uint8_t score;
} TY_UART_VOICE_EVENT_T;

typedef struct {
    TDD_AUDIO_TY_UART_CFG_T cfg;
    TDL_AUDIO_MIC_CB mic_cb;
    TDL_AUDIO_EVENT_CB event_cb;
    void *event_user_data;
    MUTEX_HANDLE tx_lock;
    MUTEX_HANDLE lifecycle_lock;
    MUTEX_HANDLE play_lock;
    THREAD_HANDLE rx_thread;
    THREAD_HANDLE mic_thread;
    SEM_HANDLE rx_exit_sem;
    SEM_HANDLE mic_exit_sem;
    SEM_HANDLE link_sem;
    QUEUE_HANDLE mic_queue;
    QUEUE_HANDLE voice_event_queue;
    volatile bool running;
    bool started;
    bool spk_active;
    uint8_t volume;
    uint32_t spk_sequence;
    uint32_t spk_ack_count;
    uint32_t last_spk_ack_sequence;
    uint8_t last_spk_ack_stage;
    uint32_t spk_flow_wait_count;
    bool spk_flow_waiting;
    uint32_t mic_dropped;
    uint32_t mic_cb_frames;
    uint32_t mic_cb_bytes;
    uint8_t tx_frame[TY_UART_VOICE_FRAME_MAX];
    uint8_t play_buf[TY_UART_SPK_PAYLOAD_MAX];
    TY_UART_VOICE_PARSER_T parser;
} TY_UART_CTX_T;

static TY_UART_CTX_T *s_ty_uart_ctx;

static void __put_u32_be(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static uint32_t __get_u32_be(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}


static OPERATE_RET __tdd_audio_ty_uart_play_stop(void);

static void __rx_thread_exit(void)
{
    if (s_ty_uart_ctx != NULL && s_ty_uart_ctx->rx_exit_sem != NULL) {
        tal_semaphore_post(s_ty_uart_ctx->rx_exit_sem);
    }
}

static void __mic_thread_exit(void)
{
    if (s_ty_uart_ctx != NULL && s_ty_uart_ctx->mic_exit_sem != NULL) {
        tal_semaphore_post(s_ty_uart_ctx->mic_exit_sem);
    }
}

static OPERATE_RET __send_frame(TY_UART_CTX_T *ctx, uint8_t version,
                                const uint8_t *data, uint16_t data_len)
{
    size_t encoded_len;
    int written;

    if (ctx == NULL || data_len > TY_UART_VOICE_DATA_MAX ||
        (data_len > 0U && data == NULL)) {
        return OPRT_INVALID_PARM;
    }
    if (!ctx->running) {
        return OPRT_RESOURCE_NOT_READY;
    }

    tal_mutex_lock(ctx->tx_lock);
    encoded_len = __voice_frame_build(version, data, data_len,
                                            ctx->tx_frame,
                                            sizeof(ctx->tx_frame));
    if (encoded_len == 0U) {
        tal_mutex_unlock(ctx->tx_lock);
        return OPRT_INVALID_PARM;
    }
    written = tal_uart_write(ctx->cfg.uart_port, ctx->tx_frame,
                             (uint32_t)encoded_len);
    tal_mutex_unlock(ctx->tx_lock);

    if (written != (int)encoded_len) {
        PR_ERR("Tuya UART audio UART write failed: %d/%u", written, (unsigned)encoded_len);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static OPERATE_RET __send_version_query(TY_UART_CTX_T *ctx)
{
    const uint8_t data[] = {TY_UART_VOICE_SUBCMD_VERSION};
    return __send_frame(ctx, TY_UART_VOICE_VER_MODULE, data, sizeof(data));
}

static OPERATE_RET __send_mic_config(TY_UART_CTX_T *ctx)
{
    uint8_t data[13] = {0};

    data[0] = TY_UART_VOICE_SUBCMD_MIC_CFG;
    __put_u32_be(&data[1], ctx->cfg.capture.sample_rate);
    data[5] = (uint8_t)ctx->cfg.capture.sample_bits;
    data[6] = (uint8_t)ctx->cfg.capture.sample_ch_num;
    data[7] = ctx->cfg.mic_gain_db;
    data[8] = ctx->cfg.capture.format;
    __put_u32_be(&data[9], ctx->cfg.capture.max_payload_size);

    return __send_frame(ctx, TY_UART_VOICE_VER_MODULE, data, sizeof(data));
}

static OPERATE_RET __send_spk_config(TY_UART_CTX_T *ctx)
{
    uint8_t data[12] = {0};

    data[0] = TY_UART_VOICE_SUBCMD_SPK_CFG;
    __put_u32_be(&data[1], ctx->cfg.playback.sample_rate);
    data[5] = (uint8_t)ctx->cfg.playback.sample_bits;
    data[6] = ctx->volume;
    data[7] = ctx->cfg.playback.format;
    __put_u32_be(&data[8], ctx->cfg.playback.max_payload_size);

    return __send_frame(ctx, TY_UART_VOICE_VER_MODULE, data, sizeof(data));
}

static OPERATE_RET __send_voice_cfg(TY_UART_CTX_T *ctx, uint8_t type,
                                    uint8_t value)
{
    const uint8_t data[] = {TY_UART_VOICE_SUBCMD_VOICE_CFG, type, value};
    return __send_frame(ctx, TY_UART_VOICE_VER_MODULE, data, sizeof(data));
}

static void __enqueue_voice_event(TY_UART_CTX_T *ctx,
                                  TDL_AUDIO_EVENT_E event,
                                  uint32_t param, uint8_t score)
{
    TY_UART_VOICE_EVENT_T msg = {
        .event = event,
        .param = param,
        .score = score,
    };

    if (ctx == NULL || ctx->voice_event_queue == NULL) {
        return;
    }
    (void)tal_queue_post(ctx->voice_event_queue, &msg, 0U);
}

static void __enqueue_mic_data(TY_UART_CTX_T *ctx,
                               const TY_UART_VOICE_FRAME_T *frame)
{
    TY_UART_MIC_EVENT_T event = {0};

    if (ctx == NULL || ctx->mic_queue == NULL || ctx->mic_cb == NULL ||
        frame == NULL || frame->data_len < 2U) {
        return;
    }

    switch (frame->data[1]) {
    case TY_UART_VOICE_MIC_STATUS_START:
        event.status = TDL_AUDIO_STATUS_VAD_START;
        break;
    case TY_UART_VOICE_MIC_STATUS_DATA:
        event.status = TDL_AUDIO_STATUS_RECEIVING;
        event.len = (uint16_t)(frame->data_len - 2U);
        break;
    case TY_UART_VOICE_MIC_STATUS_STOP:
        event.status = TDL_AUDIO_STATUS_VAD_END;
        break;
    default:
        return;
    }

    if (event.len > sizeof(event.data)) {
        ctx->mic_dropped++;
        return;
    }
    if (event.len > 0U) {
        memcpy(event.data, &frame->data[2], event.len);
    }

    if (tal_queue_post(ctx->mic_queue, &event, 0U) != OPRT_OK) {
        ctx->mic_dropped++;
    }
}

static OPERATE_RET __send_ack(TY_UART_CTX_T *ctx, uint8_t subcmd)
{
    const uint8_t data[] = {subcmd, 0x00U};
    return __send_frame(ctx, TY_UART_VOICE_VER_MODULE, data, sizeof(data));
}

static void __on_spk_ack(TY_UART_CTX_T *ctx, const TY_UART_VOICE_FRAME_T *frame)
{
    uint32_t sequence;
    uint8_t stage;

    if (frame->data_len < 6U) {
        return;
    }

    stage = frame->data[1];
    sequence = __get_u32_be(&frame->data[2]);
    ctx->spk_ack_count++;
    ctx->last_spk_ack_stage = stage;
    ctx->last_spk_ack_sequence = sequence;

    /* ACKs are observational and do not gate SPK transmission. */
}

static void __on_frame(const TY_UART_VOICE_FRAME_T *frame, void *user_data)
{
    TY_UART_CTX_T *ctx = (TY_UART_CTX_T *)user_data;
    uint8_t subcmd;

    if (ctx == NULL || frame == NULL || frame->cmd != TY_UART_VOICE_CMD ||
        frame->data_len == 0U) {
        return;
    }

    subcmd = frame->data[0];
    switch (subcmd) {
    case TY_UART_VOICE_SUBCMD_VERSION:
        tal_semaphore_post(ctx->link_sem);
        break;

    case TY_UART_VOICE_SUBCMD_MIC_DATA:
        __enqueue_mic_data(ctx, frame);
        break;

    case TY_UART_VOICE_SUBCMD_SPK_PLAY:
        __on_spk_ack(ctx, frame);
        break;

    case TY_UART_VOICE_SUBCMD_WAKEUP:
        if (frame->data_len >= 5U) {
            uint32_t wake_id = __get_u32_be(&frame->data[1]);
            __enqueue_voice_event(ctx, TDL_AUDIO_EVENT_WAKEUP, wake_id, 0U);
            (void)__send_ack(ctx, TY_UART_VOICE_SUBCMD_WAKEUP);
        }
        break;

    case TY_UART_VOICE_SUBCMD_SLEEP:
        if (frame->data_len >= 2U) {
            __enqueue_voice_event(ctx, TDL_AUDIO_EVENT_SLEEP,
                                  frame->data[1], 0U);
            (void)__send_ack(ctx, TY_UART_VOICE_SUBCMD_SLEEP);
        }
        break;

    case TY_UART_VOICE_SUBCMD_EXT_WAKEUP:
        if (frame->data_len >= 9U && (frame->data[1] & 0x01U) != 0U) {
            uint32_t semantic_id = __get_u32_be(&frame->data[5]);
            __enqueue_voice_event(ctx, TDL_AUDIO_EVENT_ASR_RESULT,
                                  semantic_id, 0U);
            (void)__send_ack(ctx, TY_UART_VOICE_SUBCMD_EXT_WAKEUP);
        }
        break;

    default:
        break;
    }
}

static void __mic_dispatch_thread(void *arg)
{
    TY_UART_CTX_T *ctx = (TY_UART_CTX_T *)arg;
    TY_UART_MIC_EVENT_T event;

    if (ctx == NULL) {
        return;
    }

    while (ctx->running) {
        TY_UART_VOICE_EVENT_T voice_event;

        if (tal_queue_fetch(ctx->voice_event_queue, &voice_event, 0U) ==
            OPRT_OK) {
            if (ctx->event_cb != NULL) {
                ctx->event_cb(voice_event.event, voice_event.param,
                              voice_event.score, ctx->event_user_data);
            }
            continue;
        }

        if (tal_queue_fetch(ctx->mic_queue, &event, 20U) != OPRT_OK) {
            continue;
        }
        if (ctx->mic_cb != NULL) {
            ctx->mic_cb_frames++;
            ctx->mic_cb_bytes += event.len;
            if (ctx->mic_cb_frames == 1U) {
                PR_NOTICE("Tuya UART audio first MIC callback len=%u status=%u",
                          (unsigned)event.len, (unsigned)event.status);
            }
            ctx->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, event.status,
                        event.len > 0U ? event.data : NULL, event.len);
        }
    }
}

static void __rx_thread(void *arg)
{
    TY_UART_CTX_T *ctx = (TY_UART_CTX_T *)arg;
    uint8_t rx_buf[128];

    if (ctx == NULL) {
        return;
    }

    while (ctx->running) {
        uint32_t now_ms = (uint32_t)tal_system_get_millisecond();
        int read_len = tal_uart_read(ctx->cfg.uart_port, rx_buf,
                                     sizeof(rx_buf));
        if (read_len > 0) {
            for (int i = 0; i < read_len; ++i) {
                (void)__voice_parser_feed(&ctx->parser, rx_buf[i], now_ms,
                                          __on_frame, ctx);
            }
        } else if (read_len < 0) {
            tal_system_sleep(10U);
        } else {
            tal_system_sleep(TY_UART_RX_POLL_MS);
        }
    }
}

static OPERATE_RET __init_spk_flow(TY_UART_CTX_T *ctx)
{
    TUYA_GPIO_BASE_CFG_T flow_cfg = {
        .mode = TUYA_GPIO_PULLDOWN,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_LOW,
    };

    if (ctx->cfg.spk_flow_pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_OK;
    }

    return tkl_gpio_init(ctx->cfg.spk_flow_pin, &flow_cfg);
}

static OPERATE_RET __wait_spk_flow_ready(TY_UART_CTX_T *ctx)
{
    OPERATE_RET rt;
    uint32_t start_ms;
    uint32_t waited_ms;

    if (ctx->cfg.spk_flow_pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_OK;
    }

    start_ms = (uint32_t)tal_system_get_millisecond();
    for (;;) {
        TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_LOW;
        TUYA_CALL_ERR_RETURN(tkl_gpio_read(ctx->cfg.spk_flow_pin, &level));
        if (level == ctx->cfg.spk_flow_active_level) {
            if (ctx->spk_flow_waiting) {
                ctx->spk_flow_waiting = false;
                waited_ms = (uint32_t)(tal_system_get_millisecond() - start_ms);
                PR_DEBUG("Tuya UART audio SPK flow resumed after %lums total=%lu",
                         (unsigned long)waited_ms,
                         (unsigned long)ctx->spk_flow_wait_count);
            }
            return OPRT_OK;
        }
        if (!ctx->spk_flow_waiting) {
            ctx->spk_flow_waiting = true;
            ctx->spk_flow_wait_count++;
            PR_DEBUG("Tuya UART audio SPK flow wait start seq=%u total=%lu",
                     (unsigned)ctx->spk_sequence,
                     (unsigned long)ctx->spk_flow_wait_count);
        }
        waited_ms = (uint32_t)(tal_system_get_millisecond() - start_ms);
        if (waited_ms >= TY_UART_SPK_FLOW_TIMEOUT_MS) {
            ctx->spk_flow_waiting = false;
            PR_ERR("Tuya UART audio SPK flow blocked for %lums",
                   (unsigned long)waited_ms);
            return OPRT_TIMEOUT;
        }
        tal_system_sleep(1U);
    }
}

static OPERATE_RET __wait_for_link(TY_UART_CTX_T *ctx)
{
    OPERATE_RET rt;

    for (uint32_t i = 0; i < TY_UART_LINK_RETRY_COUNT; ++i) {
        TUYA_CALL_ERR_RETURN(__send_version_query(ctx));
        rt = tal_semaphore_wait(ctx->link_sem, TY_UART_LINK_RETRY_MS);
        if (rt == OPRT_OK) {
            return OPRT_OK;
        }
    }

    return OPRT_TIMEOUT;
}

static void __cleanup_start(TY_UART_CTX_T *ctx)
{
    ctx->running = false;
    if (ctx->rx_exit_sem != NULL) {
        (void)tal_semaphore_wait(ctx->rx_exit_sem, TY_UART_THREAD_EXIT_WAIT_MS);
    }
    if (ctx->mic_exit_sem != NULL) {
        (void)tal_semaphore_wait(ctx->mic_exit_sem, TY_UART_THREAD_EXIT_WAIT_MS);
    }
    if (ctx->cfg.uart_port < TUYA_UART_NUM_MAX) {
        (void)tal_uart_deinit(ctx->cfg.uart_port);
    }
    if (ctx->rx_exit_sem != NULL) {
        tal_semaphore_release(ctx->rx_exit_sem);
        ctx->rx_exit_sem = NULL;
    }
    if (ctx->mic_exit_sem != NULL) {
        tal_semaphore_release(ctx->mic_exit_sem);
        ctx->mic_exit_sem = NULL;
    }
    if (ctx->link_sem != NULL) {
        tal_semaphore_release(ctx->link_sem);
        ctx->link_sem = NULL;
    }
    if (ctx->mic_queue != NULL) {
        tal_queue_free(ctx->mic_queue);
        ctx->mic_queue = NULL;
    }
    if (ctx->voice_event_queue != NULL) {
        tal_queue_free(ctx->voice_event_queue);
        ctx->voice_event_queue = NULL;
    }
    ctx->rx_thread = NULL;
    ctx->mic_thread = NULL;
    ctx->started = false;
}

static OPERATE_RET __start_locked(TY_UART_CTX_T *ctx)
{
    OPERATE_RET rt;
    TAL_UART_CFG_T uart_cfg = {0};
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096U,
        .priority = THREAD_PRIO_1,
        .thrdname = "ty_uart_audio",
        .psram_mode = 0,
    };

    if (ctx->started) {
        return OPRT_OK;
    }

    uart_cfg.rx_buffer_size = TY_UART_RX_BUFFER_SIZE;
    uart_cfg.open_mode = 0U;
    uart_cfg.base_cfg.baudrate = ctx->cfg.baudrate;
    uart_cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    uart_cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    uart_cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    uart_cfg.base_cfg.flowctrl = TUYA_UART_FLOWCTRL_NONE;

    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&ctx->rx_exit_sem, 0U, 1U), __ERR);
    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&ctx->mic_exit_sem, 0U, 1U), __ERR);
    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&ctx->link_sem, 0U, 1U), __ERR);
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&ctx->mic_queue,
                                              sizeof(TY_UART_MIC_EVENT_T),
                                              TY_UART_MIC_EVENT_QUEUE_DEPTH),
                       __ERR);
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&ctx->voice_event_queue,
                                              sizeof(TY_UART_VOICE_EVENT_T),
                                              8U), __ERR);
    TUYA_CALL_ERR_GOTO(tal_uart_init(ctx->cfg.uart_port, &uart_cfg), __ERR);

    __voice_parser_init(&ctx->parser);
    ctx->running = true;
    TUYA_CALL_ERR_GOTO(tal_thread_create_and_start(&ctx->rx_thread, NULL,
                                                    __rx_thread_exit,
                                                    __rx_thread, ctx,
                                                    &thread_cfg), __ERR);
    thread_cfg.priority = THREAD_PRIO_0;
    thread_cfg.thrdname = "ty_uart_mic";
    TUYA_CALL_ERR_GOTO(tal_thread_create_and_start(&ctx->mic_thread, NULL,
                                                    __mic_thread_exit,
                                                    __mic_dispatch_thread, ctx,
                                                    &thread_cfg), __ERR);

    ctx->started = true;
    return OPRT_OK;

__ERR:
    __cleanup_start(ctx);
    return rt;
}

static OPERATE_RET __require_started(TY_UART_CTX_T *ctx)
{
    BOOL_T started;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->lifecycle_lock);
    started = ctx->started;
    tal_mutex_unlock(ctx->lifecycle_lock);
    return started ? OPRT_OK : OPRT_RESOURCE_NOT_READY;
}

static OPERATE_RET __stop_locked(TY_UART_CTX_T *ctx)
{
    if (!ctx->started) {
        return OPRT_OK;
    }

    __cleanup_start(ctx);
    ctx->spk_active = false;
    ctx->mic_cb = NULL;
    ctx->volume = ctx->cfg.default_volume;
    ctx->spk_sequence = 0U;
    ctx->spk_flow_wait_count = 0U;
    ctx->spk_flow_waiting = false;
    __voice_parser_init(&ctx->parser);
    return OPRT_OK;
}

static OPERATE_RET __send_spk_stage(TY_UART_CTX_T *ctx, uint8_t stage,
                                    const uint8_t *data, uint16_t len)
{
    OPERATE_RET rt;
    uint32_t sequence = ctx->spk_sequence;
    uint16_t payload_len = (uint16_t)(6U + len);

    if (payload_len > sizeof(ctx->play_buf)) {
        return OPRT_INVALID_PARM;
    }

    if (stage == TY_UART_VOICE_SPK_STAGE_DATA) {
        rt = __wait_spk_flow_ready(ctx);
        if (rt != OPRT_OK) {
            PR_ERR("Tuya UART audio SPK flow timeout stage=%u seq=%u",
                   (unsigned)stage, (unsigned)sequence);
            return rt;
        }
    }

    ctx->play_buf[0] = TY_UART_VOICE_SUBCMD_SPK_PLAY;
    ctx->play_buf[1] = stage;
    __put_u32_be(&ctx->play_buf[2], sequence);
    if (len > 0U) {
        memcpy(&ctx->play_buf[6], data, len);
    }

    rt = __send_frame(ctx, TY_UART_VOICE_VER_MODULE, ctx->play_buf, payload_len);
    if (rt != OPRT_OK) {
        return rt;
    }

    ctx->spk_sequence++;
    if (stage == TY_UART_VOICE_SPK_STAGE_START) {
        PR_NOTICE("Tuya UART audio SPK START sent seq=%u", (unsigned)sequence);
    } else if (stage == TY_UART_VOICE_SPK_STAGE_STOP) {
        PR_NOTICE("Tuya UART audio SPK STOP sent seq=%u", (unsigned)sequence);
    }
    tal_system_sleep(TY_UART_SPK_SEND_YIELD_MS);
    return OPRT_OK;
}

static OPERATE_RET __tdd_audio_ty_uart_open(TDD_AUDIO_HANDLE_T handle,
                                          TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    TY_UART_CTX_T *ctx = (TY_UART_CTX_T *)handle;

    if (ctx == NULL || ctx != s_ty_uart_ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->lifecycle_lock);
    if (ctx->started) {
        ctx->mic_cb = mic_cb;
        tal_mutex_unlock(ctx->lifecycle_lock);
        return OPRT_OK;
    }

    ctx->mic_cb = mic_cb;
    TUYA_CALL_ERR_GOTO(__start_locked(ctx), __ERR);
    TUYA_CALL_ERR_GOTO(__init_spk_flow(ctx), __ERR);
    tal_system_sleep(ctx->cfg.boot_delay_ms);
    TUYA_CALL_ERR_GOTO(__wait_for_link(ctx), __ERR);
    TUYA_CALL_ERR_GOTO(__send_mic_config(ctx), __ERR);
    tal_system_sleep(20U);
    TUYA_CALL_ERR_GOTO(__send_spk_config(ctx), __ERR);
    tal_system_sleep(20U);

    TUYA_CALL_ERR_GOTO(__send_voice_cfg(ctx, TY_UART_VOICE_CFG_SPK_SWITCH, 1U), __ERR);
    TUYA_CALL_ERR_GOTO(__send_voice_cfg(ctx, TY_UART_VOICE_CFG_WAKE_SWITCH, 0U), __ERR);
    TUYA_CALL_ERR_GOTO(__send_voice_cfg(ctx, TY_UART_VOICE_CFG_VAD, 0U), __ERR);
    TUYA_CALL_ERR_GOTO(__send_voice_cfg(ctx, TY_UART_VOICE_CFG_MIC_SWITCH,
                                        mic_cb != NULL ? 1U : 0U), __ERR);

    PR_NOTICE("Tuya UART audio offline voice link ready on UART%d (%u baud)",
              (int)ctx->cfg.uart_port, (unsigned)ctx->cfg.baudrate);
    tal_mutex_unlock(ctx->lifecycle_lock);
    return OPRT_OK;

__ERR:
    ctx->mic_cb = NULL;
    __stop_locked(ctx);
    tal_mutex_unlock(ctx->lifecycle_lock);
    return rt;
}

static OPERATE_RET __tdd_audio_ty_uart_play(TDD_AUDIO_HANDLE_T handle,
                                          uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    TY_UART_CTX_T *ctx = (TY_UART_CTX_T *)handle;

    if (ctx == NULL || ctx != s_ty_uart_ctx || data == NULL || len == 0U) {
        return OPRT_INVALID_PARM;
    }
    TUYA_CALL_ERR_RETURN(__require_started(ctx));

    tal_mutex_lock(ctx->play_lock);
    if (!ctx->spk_active) {
        ctx->spk_sequence = 0U;
        rt = __send_spk_stage(ctx, TY_UART_VOICE_SPK_STAGE_START, NULL, 0U);
        if (rt != OPRT_OK) {
            goto __EXIT;
        }
        ctx->spk_active = true;
    }

    while (offset < len) {
        uint32_t chunk = len - offset;
        if (chunk > ctx->cfg.playback.max_payload_size) {
            chunk = ctx->cfg.playback.max_payload_size;
        }
        rt = __send_spk_stage(ctx, TY_UART_VOICE_SPK_STAGE_DATA,
                              data + offset, (uint16_t)chunk);
        if (rt != OPRT_OK) {
            goto __EXIT;
        }
        offset += chunk;
    }

__EXIT:
    tal_mutex_unlock(ctx->play_lock);
    return rt;
}

static OPERATE_RET __tdd_audio_ty_uart_event_register(TDD_AUDIO_HANDLE_T handle,
                                                    TDL_AUDIO_EVENT_CB cb,
                                                    void *user_data)
{
    TY_UART_CTX_T *ctx = (TY_UART_CTX_T *)handle;

    if (ctx == NULL || ctx != s_ty_uart_ctx) {
        return OPRT_INVALID_PARM;
    }
    ctx->event_cb = cb;
    ctx->event_user_data = user_data;
    return OPRT_OK;
}

static OPERATE_RET __tdd_audio_ty_uart_config(TDD_AUDIO_HANDLE_T handle,
                                            TDD_AUDIO_CMD_E cmd, void *args)
{
    OPERATE_RET rt = OPRT_OK;
    TY_UART_CTX_T *ctx = (TY_UART_CTX_T *)handle;

    if (ctx == NULL || ctx != s_ty_uart_ctx) {
        return OPRT_INVALID_PARM;
    }

    switch (cmd) {
    case TDD_AUDIO_CMD_SET_VOLUME: {
        if (args == NULL) {
            return OPRT_INVALID_PARM;
        }
        uint8_t volume = *(uint8_t *)args;
        if (volume > 100U) {
            volume = 100U;
        }
        TUYA_CALL_ERR_RETURN(__require_started(ctx));
        ctx->volume = volume;
        return __send_spk_config(ctx);
    }

    case TDD_AUDIO_CMD_PLAY_STOP:
        return __tdd_audio_ty_uart_play_stop();

    default:
        return OPRT_INVALID_PARM;
    }
}

static OPERATE_RET __tdd_audio_ty_uart_close(TDD_AUDIO_HANDLE_T handle)
{
    TY_UART_CTX_T *ctx = (TY_UART_CTX_T *)handle;
    BOOL_T is_self = FALSE;

    if (ctx == NULL || ctx != s_ty_uart_ctx) {
        return OPRT_INVALID_PARM;
    }
    if (ctx->rx_thread != NULL &&
        tal_thread_is_self(ctx->rx_thread, &is_self) == OPRT_OK && is_self) {
        return OPRT_COM_ERROR;
    }

    if (ctx->started) {
        if (ctx->spk_active) {
            (void)__tdd_audio_ty_uart_play_stop();
        }
        (void)__send_voice_cfg(ctx, TY_UART_VOICE_CFG_MIC_SWITCH, 0U);
        (void)__send_voice_cfg(ctx, TY_UART_VOICE_CFG_VAD, 0U);
        (void)__send_voice_cfg(ctx, TY_UART_VOICE_CFG_WAKE_SWITCH, 0U);
        (void)__send_voice_cfg(ctx, TY_UART_VOICE_CFG_SPK_SWITCH, 0U);
    }

    tal_mutex_lock(ctx->lifecycle_lock);
    (void)__stop_locked(ctx);
    tal_mutex_unlock(ctx->lifecycle_lock);
    return OPRT_OK;
}

static OPERATE_RET __tdd_audio_ty_uart_play_stop(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_ty_uart_ctx == NULL) {
        return OPRT_RESOURCE_NOT_READY;
    }
    TUYA_CALL_ERR_RETURN(__require_started(s_ty_uart_ctx));

    tal_mutex_lock(s_ty_uart_ctx->play_lock);
    if (s_ty_uart_ctx->spk_active) {
        rt = __send_spk_stage(s_ty_uart_ctx, TY_UART_VOICE_SPK_STAGE_STOP,
                              NULL, 0U);
        s_ty_uart_ctx->spk_active = false;
    }
    tal_mutex_unlock(s_ty_uart_ctx->play_lock);
    return rt;
}

OPERATE_RET tdd_audio_ty_uart_register(char *name, const TDD_AUDIO_TY_UART_CFG_T *cfg)
{
    OPERATE_RET rt;
    TY_UART_CTX_T *ctx;
    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T info = {0};

    if (name == NULL || cfg == NULL || cfg->uart_port >= TUYA_UART_NUM_MAX) {
        return OPRT_INVALID_PARM;
    }
    if (s_ty_uart_ctx != NULL) {
        PR_ERR("Tuya UART audio driver already registered");
        return OPRT_INIT_MORE_THAN_ONCE;
    }

    ctx = (TY_UART_CTX_T *)tal_calloc(1, sizeof(TY_UART_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(&ctx->cfg, cfg, sizeof(ctx->cfg));
    if (ctx->cfg.baudrate == 0U) {
        ctx->cfg.baudrate = TY_UART_DEFAULT_BAUDRATE;
    }
    if (ctx->cfg.boot_delay_ms == 0U) {
        ctx->cfg.boot_delay_ms = TY_UART_DEFAULT_BOOT_DELAY_MS;
    }
    if (ctx->cfg.capture.format == TDL_AUDIO_FRAME_FORMAT_NONE) {
        ctx->cfg.capture.format = TDL_AUDIO_FRAME_FORMAT_PCM;
    }
    if (ctx->cfg.capture.sample_rate == 0U) {
        ctx->cfg.capture.sample_rate = TY_UART_DEFAULT_CAPTURE_RATE;
    }
    if (ctx->cfg.capture.sample_ch_num == 0U) {
        ctx->cfg.capture.sample_ch_num = TY_UART_DEFAULT_SAMPLE_CHANNELS;
    }
    if (ctx->cfg.capture.sample_bits == 0U) {
        ctx->cfg.capture.sample_bits = TY_UART_DEFAULT_SAMPLE_BITS;
    }
    if (ctx->cfg.capture.sample_tm_ms == 0U) {
        ctx->cfg.capture.sample_tm_ms = TY_UART_DEFAULT_FRAME_MS;
    }
    if (ctx->cfg.capture.max_payload_size == 0U) {
        ctx->cfg.capture.max_payload_size = TY_UART_DEFAULT_CAPTURE_PACKET_MAX;
    }
    if (ctx->cfg.capture.max_payload_size > TY_UART_MIC_EVENT_DATA_MAX) {
        ctx->cfg.capture.max_payload_size = TY_UART_MIC_EVENT_DATA_MAX;
    }

    if (ctx->cfg.playback.format == TDL_AUDIO_FRAME_FORMAT_NONE) {
        ctx->cfg.playback.format = TDL_AUDIO_FRAME_FORMAT_PCM;
    }
    if (ctx->cfg.playback.sample_rate == 0U) {
        ctx->cfg.playback.sample_rate = TY_UART_DEFAULT_PLAYBACK_RATE;
    }
    if (ctx->cfg.playback.sample_ch_num == 0U) {
        ctx->cfg.playback.sample_ch_num = TY_UART_DEFAULT_SAMPLE_CHANNELS;
    }
    if (ctx->cfg.playback.sample_bits == 0U) {
        ctx->cfg.playback.sample_bits = TY_UART_DEFAULT_SAMPLE_BITS;
    }
    if (ctx->cfg.playback.max_payload_size == 0U) {
        ctx->cfg.playback.max_payload_size = TY_UART_DEFAULT_PLAYBACK_PACKET_MAX;
    }
    if (ctx->cfg.playback.max_payload_size > TY_UART_VOICE_DATA_MAX - 6U) {
        ctx->cfg.playback.max_payload_size = TY_UART_VOICE_DATA_MAX - 6U;
    }
    if (ctx->cfg.default_volume > 100U) {
        ctx->cfg.default_volume = 100U;
    } else if (ctx->cfg.default_volume == 0U) {
        ctx->cfg.default_volume = TY_UART_DEFAULT_VOLUME;
    }
    if (ctx->cfg.mic_gain_db > 32U) {
        ctx->cfg.mic_gain_db = 32U;
    } else if (ctx->cfg.mic_gain_db == 0U) {
        ctx->cfg.mic_gain_db = TY_UART_DEFAULT_MIC_GAIN_DB;
    }
    ctx->volume = ctx->cfg.default_volume;
    __voice_parser_init(&ctx->parser);

    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ctx->tx_lock), __ERR);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ctx->lifecycle_lock), __ERR);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ctx->play_lock), __ERR);

    info.capture = ctx->cfg.capture;
    info.playback = ctx->cfg.playback;

    intfs.open = __tdd_audio_ty_uart_open;
    intfs.play = __tdd_audio_ty_uart_play;
    intfs.config = __tdd_audio_ty_uart_config;
    intfs.close = __tdd_audio_ty_uart_close;
    intfs.event_register = __tdd_audio_ty_uart_event_register;

    rt = tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)ctx, &intfs, &info);
    if (rt != OPRT_OK) {
        goto __ERR;
    }

    s_ty_uart_ctx = ctx;
    return OPRT_OK;

__ERR:
    if (ctx->tx_lock != NULL) {
        tal_mutex_release(ctx->tx_lock);
    }
    if (ctx->lifecycle_lock != NULL) {
        tal_mutex_release(ctx->lifecycle_lock);
    }
    if (ctx->play_lock != NULL) {
        tal_mutex_release(ctx->play_lock);
    }
    tal_free(ctx);
    return rt;
}

#endif /* ENABLE_TDD_AUDIO_TY_UART */
