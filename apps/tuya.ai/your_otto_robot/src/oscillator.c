//--------------------------------------------------------------
//-- Oscillator.c
//-- Generate sinusoidal oscillations in the servos
//--------------------------------------------------------------
//-- Original work (c) Juan Gonzalez-Gomez (Obijuan), Dec 2011
//-- GPL license
//-- Ported to Tuya AI development board by [txp666], 2025
//--------------------------------------------------------------

#include "oscillator.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// 声明全局oscillator结构体
Oscillator_t g_oscillators[MAX_OSCILLATORS];
static uint8_t g_oscillator_count = 0;

static unsigned long millis()
{
    return tal_system_get_millisecond();
}

// 创建新的oscillator并返回索引
int oscillator_create(int trim)
{
    if (g_oscillator_count >= MAX_OSCILLATORS) {
        PR_ERR("超出最大Oscillator数量");
        return -1;
    }

    int idx = g_oscillator_count++;
    Oscillator_t *osc = &g_oscillators[idx];

    osc->trim = trim;
    osc->diff_limit = 0;
    osc->is_attached = false;

    osc->sampling_period = 30;
    osc->period = 2000;
    osc->number_samples = osc->period / osc->sampling_period;
    osc->inc = 2 * M_PI / osc->number_samples;

    osc->amplitude = 45;
    osc->phase = 0;
    osc->phase0 = 0;
    osc->offset = 0;
    osc->stop = false;
    osc->rev = false;

    osc->pos = 90;
    osc->previous_millis = 0;

    return idx;
}

// 释放oscillator
void oscillator_destroy(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;

    oscillator_detach(idx);
}

uint32_t oscillator_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) /
               (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) +
           SERVO_MIN_PULSEWIDTH_US;
}

bool oscillator_next_sample(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return false;

    Oscillator_t *osc = &g_oscillators[idx];
    osc->current_millis = millis();

    if (osc->current_millis - osc->previous_millis > osc->sampling_period) {
        osc->previous_millis = osc->current_millis;
        return true;
    }

    return false;
}

void oscillator_attach(int idx, int pin, bool rev)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;

    Oscillator_t *osc = &g_oscillators[idx];

    if (osc->is_attached) {
        oscillator_detach(idx);
    }

    osc->pin = pin;
    osc->rev = rev;
    osc->pwm_channel = (TUYA_PWM_NUM_E)pin; // 假设PWM通道与引脚对应

    // 配置PWM参数
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty = 0,
        .frequency = 50, // 50Hz for servos
        .polarity = TUYA_PWM_NEGATIVE,
    };

    tkl_pwm_init(osc->pwm_channel, &pwm_cfg);

    // 初始位置
    osc->previous_servo_command_millis = millis();
    osc->is_attached = true;
}

void oscillator_detach(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;

    Oscillator_t *osc = &g_oscillators[idx];

    if (!osc->is_attached)
        return;

    tkl_pwm_stop(osc->pwm_channel);
    osc->is_attached = false;
}

void oscillator_set_t(int idx, unsigned int T)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;

    Oscillator_t *osc = &g_oscillators[idx];
    osc->period = T;

    osc->number_samples = osc->period / osc->sampling_period;
    osc->inc = 2 * M_PI / osc->number_samples;
}

void oscillator_set_a(int idx, unsigned int amplitude)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].amplitude = amplitude;
}

void oscillator_set_o(int idx, int offset)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].offset = offset;
}

void oscillator_set_ph(int idx, double ph)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].phase0 = ph;
}

void oscillator_set_trim(int idx, int trim)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].trim = trim;
}

void oscillator_set_limiter(int idx, int diff_limit)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].diff_limit = diff_limit;
}

void oscillator_disable_limiter(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].diff_limit = 0;
}

int oscillator_get_trim(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return 0;
    return g_oscillators[idx].trim;
}

void oscillator_set_position(int idx, int position)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    oscillator_write(idx, position);
}

void oscillator_stop(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].stop = true;
}

void oscillator_play(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].stop = false;
}

void oscillator_reset(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;
    g_oscillators[idx].phase = 0;
}

int oscillator_get_position(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return 90;
    return g_oscillators[idx].pos;
}

void oscillator_refresh(int idx)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;

    Oscillator_t *osc = &g_oscillators[idx];

    if (oscillator_next_sample(idx)) {
        if (!osc->stop) {
            // 根据sin函数计算当前位置
            double sinVal = sin(osc->phase + osc->phase0);
            int pos = (int)round(osc->amplitude * sinVal + osc->offset);
            if (osc->rev)
                pos = -pos;
            oscillator_write(idx, pos + 90);
            // PR_NOTICE("------------->[idx:%d]pos=%d",idx,pos);
        }

        osc->phase = osc->phase + osc->inc;
    }
}

void oscillator_write(int idx, int position)
{
    if (idx < 0 || idx >= g_oscillator_count)
        return;

    Oscillator_t *osc = &g_oscillators[idx];

    if (!osc->is_attached)
        return;

    unsigned long currentMillis = millis();
    if (osc->diff_limit > 0) {
        int limit = MAX(1, (((int)(currentMillis - osc->previous_servo_command_millis)) * osc->diff_limit) / 1000);
        if (abs(position - osc->pos) > limit) {
            osc->pos += position < osc->pos ? -limit : limit;
        } else {
            osc->pos = position;
        }
    } else {
        osc->pos = position;
    }
    osc->previous_servo_command_millis = currentMillis;

    int angle = osc->pos + osc->trim;
    // 限制角度范围
    angle = MIN(MAX(angle, 0), 180);

    // 计算占空比：0-10000范围，0表示0%，10000表示100%
    // 标准舵机：0.5ms~2.5ms脉冲对应0°~180°，周期20ms
    // 占空比计算：(0.5 + 角度/180 * 2.0) / 20 * 10000
    uint32_t duty = (uint32_t)((0.5 + angle / 180.0 * 2.0) * 10000 / 20);

    tkl_pwm_duty_set(osc->pwm_channel, duty);
    tkl_pwm_start(osc->pwm_channel);
}