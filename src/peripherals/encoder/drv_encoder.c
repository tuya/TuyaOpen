/**
 * @file drv_encoder.c
 * @brief Implementation of encoder driver for various types of encoders. This file
 *        offers initialization functions, data acquisition, and control operations
 *        for different kinds of encoders used in the system, enabling accurate
 *        position and speed feedback.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "drv_encoder.h"

static int32_t encode_angle = 0;

static SEM_HANDLE example_sem_hdl = NULL;
static THREAD_HANDLE wait_thrd_hdl = NULL;
static MUTEX_HANDLE mutex_hdl = NULL;

static void __gpio_irq_callback(void *args)
{
    uint8_t pin_id = (uint8_t)args;

    tal_semaphore_post(example_sem_hdl);
}

/**
 * @brief Wait for semaphore and process encoder input task.
 *
 * This function waits for a semaphore in an infinite loop, reads the encoder's input signals,
 * and updates the encoding angle based on the state of the input signals.
 * When the input signal is low, the encoding angle is increased; when the input signal is high,
 * the encoding angle is decreased. The function also checks the thread state to decide whether
 * to exit the loop.
 *
 * @param args The passed-in parameter, currently unused.
 */
static void __sema_wait_task(void *args)
{
    uint32_t cnt = 0;
    TUYA_GPIO_LEVEL_E a_level = 0;
    TUYA_GPIO_LEVEL_E b_level = 0;

    while (1) {
        tal_semaphore_wait(example_sem_hdl, SEM_WAIT_FOREVER);

        tkl_gpio_read(DECODER_INPUT_A, &a_level);
        tkl_gpio_read(DECODER_INPUT_B, &b_level);

        if (a_level == TUYA_GPIO_LEVEL_LOW && b_level == TUYA_GPIO_LEVEL_LOW) {
            tal_system_sleep(3);
            tkl_gpio_read(DECODER_INPUT_A, &a_level);
            tkl_gpio_read(DECODER_INPUT_B, &b_level);

            if (a_level == TUYA_GPIO_LEVEL_LOW && b_level == TUYA_GPIO_LEVEL_LOW) {
                tal_mutex_lock(mutex_hdl);
                encode_angle += 1;
                tal_mutex_unlock(mutex_hdl);
            }
        } else if (a_level == TUYA_GPIO_LEVEL_LOW && b_level == TUYA_GPIO_LEVEL_HIGH) {
            tal_system_sleep(3);
            tkl_gpio_read(DECODER_INPUT_A, &a_level);
            tkl_gpio_read(DECODER_INPUT_B, &b_level);

            if (a_level == TUYA_GPIO_LEVEL_LOW && b_level == TUYA_GPIO_LEVEL_HIGH) {
                tal_mutex_lock(mutex_hdl);
                encode_angle -= 1;
                tal_mutex_unlock(mutex_hdl);
            }
        }

        cnt = 0;
        while (1) {
            tkl_gpio_read(DECODER_INPUT_A, &a_level);
            tkl_gpio_read(DECODER_INPUT_B, &b_level);

            if (a_level == TUYA_GPIO_LEVEL_HIGH && b_level == TUYA_GPIO_LEVEL_HIGH) {
                break;
            }

            tal_system_sleep(10);
            cnt++;
            if (cnt > 100) {
                PR_ERR("encoder wait timeout");
                break;
            }
        }

        if (THREAD_STATE_STOP == tal_thread_get_state(wait_thrd_hdl)) {
            break;
        }
    }

    wait_thrd_hdl = NULL;
    PR_DEBUG("thread __sema_wait_task will delete");

    return;
}

/**
 * @brief Get the angle value of the encoder.
 *
 * This function locks the mutex before getting the angle value to ensure thread safety.
 * After reading the angle value, it unlocks the mutex. The returned value is the current
 * angle of the encoder.
 *
 * @return The current angle value of the encoder, in degrees.
 */
int32_t encoder_get_angle(void)
{
    int32_t value;

    tal_mutex_lock(mutex_hdl);
    value = encode_angle;
    tal_mutex_unlock(mutex_hdl);

    return value;
}

/**
 * @brief Check if the encoder button is pressed.
 *
 * This function reads the voltage level of the encoder input pin. If a low voltage level
 * is still detected after a short delay, it returns 1 to indicate that the button is pressed;
 * otherwise, it returns 0 to indicate that it is not pressed.
 *
 * @return uint8_t Returns 1 if the button is pressed, returns 0 if not pressed.
 */
uint8_t encoder_get_pressed(void)
{
    TUYA_GPIO_LEVEL_E read_level = TUYA_GPIO_LEVEL_LOW;

    tkl_gpio_read(DECODER_INPUT_P, &read_level);
    if (read_level == TUYA_GPIO_LEVEL_LOW) {
        tal_system_sleep(5);
        tkl_gpio_read(DECODER_INPUT_P, &read_level);
        if (read_level == TUYA_GPIO_LEVEL_LOW) {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Initialize the encoder module.
 *
 * This function is responsible for creating semaphores and mutexes, initializing GPIO input pins,
 * and setting up the encoder input interrupt configuration. If the waiting thread has not been
 * created yet, it will start the thread to handle semaphore waiting.
 *
 * @return None
 */
void tkl_encoder_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&example_sem_hdl, 0, 1));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&mutex_hdl));

    THREAD_CFG_T thread_cfg = {
        .thrdname = "sem_wait",
        .stackDepth = 2048,
        .priority = THREAD_PRIO_2,
    };

    if (NULL == wait_thrd_hdl) {
        TUYA_CALL_ERR_RETURN(
            tal_thread_create_and_start(&wait_thrd_hdl, NULL, NULL, __sema_wait_task, NULL, &thread_cfg));
    }

    /*GPIO input init*/
    TUYA_GPIO_BASE_CFG_T in_pin_cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
    };

    TUYA_CALL_ERR_LOG(tkl_gpio_init(DECODER_INPUT_A, &in_pin_cfg));

    TUYA_CALL_ERR_LOG(tkl_gpio_init(DECODER_INPUT_B, &in_pin_cfg));

    TUYA_CALL_ERR_LOG(tkl_gpio_init(DECODER_INPUT_P, &in_pin_cfg));

    /*Encoder input init*/
    TUYA_GPIO_IRQ_T irq_cfg = {
        .cb = __gpio_irq_callback,
        .arg = NULL,
        .mode = TUYA_GPIO_IRQ_FALL,
    };
    TUYA_CALL_ERR_LOG(tkl_gpio_irq_init(DECODER_INPUT_A, &irq_cfg));

    /*irq enable*/
    TUYA_CALL_ERR_LOG(tkl_gpio_irq_enable(DECODER_INPUT_A));
}
