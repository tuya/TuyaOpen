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


Oscillator_t g_oscillators[MAX_OSCILLATORS];
static uint8_t g_oscillator_count = 0;

static unsigned long millis()
{
    return tal_system_get_millisecond();
}


int oscillator_create(int trim)
{
    if (g_oscillator_count >= MAX_OSCILLATORS) {
        PR_ERR("Oscillator count exceeded");
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

// Release oscillator
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
    osc->pwm_channel = (TUYA_PWM_NUM_E)pin; 

    // 打印初始化信息
    PR_DEBUG("oscillator_attach: idx=%d, pin=%d, pwm_channel=%d, rev=%s", 
              idx, pin, osc->pwm_channel, rev ? "true" : "false");
   
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty = 0,
        .frequency = 50, // 50Hz for servos
        .polarity = TUYA_PWM_NEGATIVE,
    };

    tkl_pwm_init(osc->pwm_channel, &pwm_cfg);

  
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

            double sinVal = sin(osc->phase + osc->phase0);
            int pos = (int)round(osc->amplitude * sinVal + osc->offset);
            if (osc->rev)
                pos = -pos;
            oscillator_write(idx, pos + 90);
            // PR_DEBUG("------------->[idx:%d]pos=%d",idx,pos);
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

    angle = MIN(MAX(angle, 0), 180);

    // 计算占空比
    uint32_t duty = (uint32_t)((0.5 + angle / 180.0 * 2.0) * 10000 / 20);
    
    // 计算占空比百分比 (duty/10000 * 100)
    //float duty_percent = (float)duty / 100.0f;
    
    // 合并的占空比打印信息 - 显示百分比
   // PR_DEBUG("oscillator_write: idx=%d, pin=%d, pwm_channel=%d, pos=%d->angle=%d(trim:%d), duty=%d, duty_percent=%.1f%%", 
     //         idx, osc->pin, osc->pwm_channel, position, angle, osc->trim, duty, duty_percent);

    tkl_pwm_duty_set(osc->pwm_channel, duty);
    tkl_pwm_start(osc->pwm_channel);
}