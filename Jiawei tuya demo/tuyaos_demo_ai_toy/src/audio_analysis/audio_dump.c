/**
* Copyright (C) by Tuya Inc                                                  
* All rights reserved                                                        
*
* @file audio_dump.c
* @brief audio dump mic/ref/aec
* @version 1.0
* @author linch
* @date 2025-06-02
*
*/

#include "tuya_device_cfg.h"
#include "tuya_cloud_types.h"
#include "audio_dump.h"
#include "tkl_memory.h"
#include "tal_uart.h"
#include "tkl_ipc.h"
#include "aud_intf_types.h"

#define AUDIO_DUMP_MIC          0
#define AUDIO_DUMP_REF          1
#define AUDIO_DUMP_AEC          2
#define AUDIO_DUMP_ASR          3
#define AUDIO_DUMP_MAX          4

#define AUDIO_DUMP_BUF          1024*1024

typedef struct {
    uint8_t  *data;
    uint32_t  datalen;
} audio_dump_t;

typedef struct {
    const char *name;
    aud_intf_voc_aec_para_t para;
} aa_alg_para_map_t;

static audio_dump_t audio_dump[AUDIO_DUMP_MAX];
static int audio_dump_flag = false;
static aa_alg_para_map_t audio_alg_para_map[] = {
    {"aec_init_flags", TKL_AUDIO_ALG_AEC_INIT_FLAG},     // recommended value range: 0/1, 0: disable, 1: enable
    {"aec_ec_depth", TKL_AUDIO_ALG_AEC_EC_DEPTH},        // recommended value range: 1~50, the greater the echo, the greater the value setting
    {"aec_mic_delay", TKL_AUDIO_ALG_AEC_MIC_DELAY},      // set delay points of ref data according to dump data
    {"aec_ref_scale", TKL_AUDIO_ALG_AEC_REF_SCALE},      // value range:0,1,2, the greater the signal amplitude, the greater the setting
    {"aec_voice_vol", TKL_AUDIO_ALG_AEC_VOICE_VOL},      // the voice volume level
    {"aec_TxRxThr", TKL_AUDIO_ALG_AEC_TXRX_THR},         // the max amplitude of rx audio data
    {"aec_TxRxFlr", TKL_AUDIO_ALG_AEC_TXRX_FLR},         // the min amplitude of rx audio data
    {"aec_ns_level", TKL_AUDIO_ALG_AEC_NS_LEVEL},        // recommended value range: 1~8, the lower the noise, the lower the level
    {"aec_ns_para", TKL_AUDIO_ALG_AEC_NS_PARA},          // value range:0,1,2, the lower the noise, the lower the level, the default value is recommended
    {"aec_drc", TKL_AUDIO_ALG_AEC_DRC},                  // recommended value range:0x10~0x1f, the greater the value, the greater the volume
    {"vad_SPthr", TKL_AUDIO_ALG_VAD_SPTHR},              // vad SPthr parameter array list[14]
};

void audio_dump_init(void);

void audio_dump_write(int type, uint8_t *data, uint16_t datalen)
{
#if ENABLE_AUDIO_ANALYSIS
    static int init = 0;
    if (!init) {
        audio_dump_init();
        init = 1;
        return;
    }

    if (!audio_dump_flag) {
        return;
    }

    if (type >= AUDIO_DUMP_MAX) {
        return;
    }

    if (audio_dump[type].datalen + datalen > AUDIO_DUMP_BUF) {
        return;
    }

    memcpy(audio_dump[type].data + audio_dump[type].datalen, data, datalen);
    audio_dump[type].datalen += datalen;
#endif // ENABLE_AUDIO_ANALYSIS
}

void audio_dump_enable(void)
{
    audio_dump_flag = true;
}

void audio_dump_disable(void)
{
    audio_dump_flag = false;
}

void audio_dump_reset(void)
{
    audio_dump[0].datalen = 0;
    audio_dump[1].datalen = 0;
    audio_dump[2].datalen = 0;
}

void audio_dump_with_uart(int type)
{
    bk_printf("audio_dump type[%d] len %d\r\n", type, audio_dump[type].datalen);

    tal_uart_write(TUYA_UART_NUM_0, audio_dump[type].data , audio_dump[type].datalen);

    audio_dump[type].datalen = 0;
}

void audio_dump_with_net(int type)
{
    bk_printf("audio_dump type[%d] len %d\r\n", type, audio_dump[type].datalen);

    _audio_test_event(AUDIO_TEST_EVENT_NET_DUMP_AUDIO, type, (uint32_t)&audio_dump[type]);
}

// send audio test event to cpu0
extern TKL_IPC_HANDLE __ipc_handle[2];
void _audio_test_event(uint32_t event, uint32_t type, uint32_t freq)
{
    struct ipc_msg_s send_msg = {0}; 
    send_msg.type   = TKL_IPC_TYPE_AUDIO_TEST;
    send_msg.buf32[0] = event; 
    send_msg.buf32[1] = type;
    send_msg.buf32[2] = freq;

    tkl_ipc_send(__ipc_handle[0], (UINT8_T *)&send_msg, sizeof(send_msg));
    return;
}

void audio_play_bgm(int type, int freq)
{
    _audio_test_event(AUDIO_TEST_EVENT_PLAY_BGM, type, freq);
}

void audio_set_volume(int volume)
{
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }
    _audio_test_event(AUDIO_TEST_EVENT_SET_VOLUME, volume, 0);
}

void audio_set_micgain(int micgain)
{
    if (micgain < 0) {
        micgain = 0;
    } else if (micgain > 100) {
        micgain = 100;
    }
    _audio_test_event(AUDIO_TEST_EVENT_SET_MICGAIN, micgain, 0);
}

void audio_ctrl_alg(int argc, char *argv[])
{
    if (0 == strcmp(argv[2], "set")) {
        // ! ao alg set <para> <value>
        if (argc != 5) {
            bk_printf("audio alg set cmd error\r\n");
            return;
        }
        int i;
        for (i = 0; i < sizeof(audio_alg_para_map) / sizeof(aa_alg_para_map_t); i++) {
            if (0 == strcmp(argv[3], audio_alg_para_map[i].name)) {
                break;
            }
        }
        if (i >= sizeof(audio_alg_para_map) / sizeof(aa_alg_para_map_t)) {
            bk_printf("audio alg set para %s not found\r\n", argv[3]);
            return;
        }
        uint32_t type = audio_alg_para_map[i].para;
        uint32_t value = atoi(argv[4]);
        _audio_test_event(AUDIO_TEST_EVENT_SET_ALG_PARA, type, value);
    } else if (0 == strcmp(argv[2], "get")) {
        // ! ao alg get <para>
        if (argc != 4 && argc != 5) {
            return;
        }
        int i;
        for (i = 0; i < sizeof(audio_alg_para_map) / sizeof(aa_alg_para_map_t); i++) {
            if (0 == strcmp(argv[3], audio_alg_para_map[i].name)) {
                break;
            }
        }
        if (i >= sizeof(audio_alg_para_map) / sizeof(aa_alg_para_map_t)) {
            bk_printf("audio alg get para %s not found\r\n", argv[3]);
            return;
        }
        uint32_t type = audio_alg_para_map[i].para;
        uint32_t value = 0;
        if (argc == 5) {
            value = atoi(argv[4]);
        }
        _audio_test_event(AUDIO_TEST_EVENT_GET_ALG_PARA, type, value);
    } else if (0 == strcmp(argv[2], "dump")) {
        // ! ao alg dump
        _audio_test_event(AUDIO_TEST_EVENT_DUMP_ALG_PARA, 0, 0);
    } else {
        bk_printf("audio alg cmd error\r\n");
    }
}

//！ ao start
//！ ao stop
//！ ao reset
//！ ao dump 0
//！ ao dump 1
//！ ao dump 2
//！ ao netdump 0
//！ ao netdump 1
//！ ao netdump 2
//！ ao bg 0
//！ ao bg 1 (ao bg 1 1000)
//！ ao bg 2
//！ ao volume 50
// ! ao micgain 70(default)
// ! ao alg set <para> [<para2>] <value>
// ! ao alg get <para> [<para2>]
// ! ao alg dump
void audio_dump_exec(int argc, char *argv[])
{
    if (0 == strcmp(argv[1], "start")) {
        audio_dump_enable();
        bk_printf("audio_dump start\r\n");
    } else if (0 == strcmp(argv[1], "stop")) {
        audio_dump_disable();
        bk_printf("audio_dump stop\r\n");
    } else if (0 == strcmp(argv[1], "dump")) {
        audio_dump_with_uart(atoi(argv[2]));
        bk_printf("audio_dump, %d\r\n", atoi(argv[2]));
    } else if (0 == strcmp(argv[1], "netdump")) {
        audio_dump_with_net(atoi(argv[2]));
        bk_printf("audio_dump, %d\r\n", atoi(argv[2]));
    } else if (0 == strcmp(argv[1], "reset")) {
        audio_dump_reset();
        bk_printf("audio_dump reset\r\n");
    } else if (0 == strcmp(argv[1], "bg")) {
        int freq = 0;
        if (argc > 3) {
            freq = atoi(argv[3]);
        }
        audio_play_bgm(atoi(argv[2]), freq);
        bk_printf("audio_dump play bgm %d\r\n", atoi(argv[2]));
    } else if (0 == strcmp(argv[1], "volume")) {
        audio_set_volume(atoi(argv[2]));
        bk_printf("audio_dump set volume %d\r\n", atoi(argv[2]));
    } else if (0 == strcmp(argv[1], "micgain")) {
        audio_set_micgain(atoi(argv[2]));
        bk_printf("audio_dump set micgain %d\r\n", atoi(argv[2]));
    } else if (0 == strcmp(argv[1], "alg")) {
        audio_ctrl_alg(argc, argv);
    } else {
        bk_printf("audio_dump cmd error\r\n");
    }
}


int strsplit(char* input, int *argc, char *argv[])
{
    const char delimiter[] = " ";
    char *token = NULL;

    *argc = 0;
    // Get the first token
    token = strtok(input, delimiter);
    argv[(*argc)++] = token;
    bk_printf("token %s\r\n", token);
    // Iterate over tokens
    while (token != NULL) {
        // Get the next token
        token = strtok(NULL, delimiter);
        if (token) {
            argv[(*argc)++] = token;
        }
        if (*argc >= 10) {
            return OPRT_INDEX_OUT_OF_BOUND;
        }
    }

    return OPRT_OK;
}

void audio_dump_task(void *params)
{
    int     rt;

    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = 460800;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size = 256;
    cfg.open_mode = O_BLOCK;
    rt = tal_uart_init(TUYA_UART_NUM_0, &cfg);

    uint8_t ch;
    uint8_t buffer[255];
    uint8_t index = 0;
    int     argc;
    char   *argv[10];

    for (;;) {
        tal_uart_read(TUYA_UART_NUM_0, (UINT8_T*)&ch, 1);
        if (ch != '\r' && ch != '\n') {
            buffer[index++] = ch;
            continue;
        }
        buffer[index] = '\0';
        //! if '\r\n' is end of char, '\n' is need discard， so check index
        if (index && OPRT_OK == strsplit(buffer, &argc, argv)) {
            //! parse cmd
            if (0 == strcmp(argv[0], "ao")) {
                audio_dump_exec(argc, argv);
            }
        }
        index = 0;
    }
}

#include "tkl_thread.h"

STATIC TKL_THREAD_HANDLE audio_dump_handle = NULL;

void audio_dump_init(void)
{
    int i = 0;

    for (i = 0; i < AUDIO_DUMP_MAX; i++) {
        audio_dump[i].datalen = 0;
        audio_dump[i].data    = tkl_system_psram_malloc(AUDIO_DUMP_BUF);
    }

    tkl_thread_create_in_psram(&audio_dump_handle, "audio_dump", 1024 * 4, 4, audio_dump_task, NULL);
}
