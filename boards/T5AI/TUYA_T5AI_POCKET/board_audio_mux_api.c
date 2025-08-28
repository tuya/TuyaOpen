/**
 * @file board_audio_mux_api.c
 * @author Tuya Inc.
 * @brief Implementation of audio multiplexer control API for TUYA_T5AI_POCKET board.
 *
 * This module provides APIs to control the audio multiplexer that routes signals
 * to the MIC2 input of the audio codec. The mux can switch between microphone
 * input and speaker loopback for audio processing.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"
#include "board_audio_mux_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET board_audio_mux_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_NUM_E sel_gpio = BOARD_AUDIO_MUX_SEL_PIN;
    TUYA_GPIO_LEVEL_E sel_level = BOARD_AUDIO_MUX_SEL_LOOPBACK_LV;

    // Initialize GPIO pin for audio mux control using base config struct
    TUYA_GPIO_BASE_CFG_T cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = sel_level,
    };
    rt = tkl_gpio_init(sel_gpio, &cfg);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Set default route to microphone input
    rt = tkl_gpio_write(sel_gpio, sel_level);
    if (OPRT_OK != rt) {
        return rt;
    }

    return rt;
}

OPERATE_RET board_audio_mux_set_mic_route(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_NUM_E sel_gpio = BOARD_AUDIO_MUX_SEL_PIN;
    TUYA_GPIO_LEVEL_E sel_level = BOARD_AUDIO_MUX_SEL_MIC_LV;

    rt = tkl_gpio_write(sel_gpio, sel_level);
    return rt;
}

OPERATE_RET board_audio_mux_set_loopback_route(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_NUM_E sel_gpio = BOARD_AUDIO_MUX_SEL_PIN;
    TUYA_GPIO_LEVEL_E sel_level = BOARD_AUDIO_MUX_SEL_LOOPBACK_LV;

    rt = tkl_gpio_write(sel_gpio, sel_level);
    return rt;
}

OPERATE_RET board_audio_mux_set_route(BOARD_AUDIO_MUX_ROUTE_E route)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_NUM_E sel_gpio = BOARD_AUDIO_MUX_SEL_PIN;
    TUYA_GPIO_LEVEL_E sel_level;

    switch (route) {
        case BOARD_AUDIO_MUX_ROUTE_MIC:
            sel_level = BOARD_AUDIO_MUX_SEL_MIC_LV;
            break;
        case BOARD_AUDIO_MUX_ROUTE_LOOPBACK:
            sel_level = BOARD_AUDIO_MUX_SEL_LOOPBACK_LV;
            break;
        default:
            return OPRT_INVALID_PARM;
    }

    rt = tkl_gpio_write(sel_gpio, sel_level);
    return rt;
}

OPERATE_RET board_audio_mux_get_route(BOARD_AUDIO_MUX_ROUTE_E *route)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_NUM_E sel_gpio = BOARD_AUDIO_MUX_SEL_PIN;
    TUYA_GPIO_LEVEL_E sel_level;

    if (NULL == route) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_gpio_read(sel_gpio, &sel_level);
    if (OPRT_OK != rt) {
        return rt;
    }

    if (sel_level == BOARD_AUDIO_MUX_SEL_MIC_LV) {
        *route = BOARD_AUDIO_MUX_ROUTE_MIC;
    } else if (sel_level == BOARD_AUDIO_MUX_SEL_LOOPBACK_LV) {
        *route = BOARD_AUDIO_MUX_ROUTE_LOOPBACK;
    } else {
        // Invalid state, default to microphone route
        *route = BOARD_AUDIO_MUX_ROUTE_MIC;
    }

    return rt;
}

OPERATE_RET board_audio_mux_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_NUM_E sel_gpio = BOARD_AUDIO_MUX_SEL_PIN;

    rt = tkl_gpio_deinit(sel_gpio);
    return rt;
}