/**
 * @file tdd_joystick_gpio.c
 * @brief tdd_joystick_gpio, irq
 * @version 1.0
 * @date 2022-03-20
 * @copyright Copyright (c) tuya.inc 2022
 * GPIO joystick adaptation
 */

#include "string.h"

// TDL
#include "tdl_joystick_manage.h"
#include "tdd_joystick.h"

#include "tal_memory.h"
#include "tal_log.h"

#include "tkl_gpio.h"
#include "tkl_adc.h"

// 按键硬件初始化
static OPERATE_RET __tdd_create_gpio_joystick(TDL_JOYSTICK_OPRT_INFO *dev)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    JOYSTICK_GPIO_CFG_T *p_gpio_local = NULL;

    if (NULL == dev) {
        PR_ERR("tdd dev err");
        return OPRT_INVALID_PARM;
    }

    if (NULL == dev->dev_handle) {
        PR_ERR("tdd dev handle err");
        return OPRT_INVALID_PARM;
    }

    p_gpio_local = (JOYSTICK_GPIO_CFG_T *)(dev->dev_handle);

    // GPIO硬件初始化
    if (p_gpio_local->mode == JOYSTICK_TIMER_SCAN_MODE) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg;
        gpio_cfg.direct = TUYA_GPIO_INPUT;
        gpio_cfg.level = p_gpio_local->level;
        gpio_cfg.mode = p_gpio_local->pin_type.gpio_pull;

        ret = tkl_gpio_init(p_gpio_local->btn_pin, &gpio_cfg);
        if (OPRT_OK != ret) {
            PR_ERR("gpio select err");
            return ret;
        }
    } else if (p_gpio_local->mode == JOYSTICK_IRQ_MODE) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {0};
        gpio_cfg.direct = TUYA_GPIO_INPUT;
        gpio_cfg.level = p_gpio_local->level;
        gpio_cfg.mode = (p_gpio_local->level == TUYA_GPIO_LEVEL_HIGH) ? TUYA_GPIO_PULLUP : TUYA_GPIO_PULLDOWN;
        ret = tkl_gpio_init(p_gpio_local->btn_pin, &gpio_cfg);
        if (OPRT_OK != ret) {
            PR_ERR("irq gpio init err");
            return ret;
        }

        TUYA_GPIO_IRQ_T gpio_irq_cfg;
        gpio_irq_cfg.mode = p_gpio_local->pin_type.irq_edge;
        gpio_irq_cfg.cb = dev->irq_cb;
        gpio_irq_cfg.arg = p_gpio_local;
        ret = tkl_gpio_irq_init(p_gpio_local->btn_pin, &gpio_irq_cfg);
        if (OPRT_OK != ret) {
            PR_ERR("gpio irq init err=%d", ret);
            return OPRT_COM_ERROR;
        }

        ret = tkl_gpio_irq_enable(p_gpio_local->btn_pin);
        if (OPRT_OK != ret) {
            PR_ERR("gpio irq enable err=%d", ret);
            return OPRT_COM_ERROR;
        }
    }

    // ADC硬件初始化
    ret = tkl_adc_init(p_gpio_local->adc_num, &p_gpio_local->adc_cfg);
    if (OPRT_OK != ret) {
        PR_ERR("adc init err");
        return ret;
    }

    return OPRT_OK;
}

// 删除一个按键
static OPERATE_RET __tdd_delete_gpio_joystick(TDL_JOYSTICK_OPRT_INFO *dev)
{
    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == dev->dev_handle) {
        return OPRT_INVALID_PARM;
    }

    tal_free(dev->dev_handle);

    return OPRT_OK;
}

// 获取参数:已经做完高低有效电平处理,与有效电平一致返回1,不同返回0
static OPERATE_RET __tdd_read_gpio_joystick_value(TDL_JOYSTICK_OPRT_INFO *dev, uint8_t *value)
{
    OPERATE_RET ret = -1;
    TUYA_GPIO_LEVEL_E result;
    JOYSTICK_GPIO_CFG_T *p_local_cfg = NULL;

    if (dev == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == dev->dev_handle) {
        PR_ERR("handle not get");
        return OPRT_INVALID_PARM;
    }

    p_local_cfg = (JOYSTICK_GPIO_CFG_T *)(dev->dev_handle);

    ret = tkl_gpio_read(p_local_cfg->btn_pin, &result);
    if (OPRT_OK == ret) {
        // PR_NOTICE("btn_pin=%d,result=%d",p_local_cfg->btn_pin, result);
        // 处理有效电平逻辑
        if (p_local_cfg->level == result) {
            result = 1;
        } else {
            result = 0;
        }

        *value = result;
        return OPRT_OK;
    }
    return ret;
}

// 添加按键并存入数据
static OPERATE_RET __add_new_joystick(char *name, JOYSTICK_GPIO_CFG_T *data, TDL_JOYSTICK_DEV_HANDLE *handle)
{
    JOYSTICK_GPIO_CFG_T *p_gpio_local_cfg = NULL;

    if (NULL == data) {
        return OPRT_INVALID_PARM;
    }
    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    // 申请存储空间
    p_gpio_local_cfg = (JOYSTICK_GPIO_CFG_T *)tal_malloc(sizeof(JOYSTICK_GPIO_CFG_T));
    if (NULL == p_gpio_local_cfg) {
        PR_ERR("tdd gpio malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(p_gpio_local_cfg, 0, sizeof(JOYSTICK_GPIO_CFG_T));
    memcpy(p_gpio_local_cfg, data, sizeof(JOYSTICK_GPIO_CFG_T));

    *handle = (TDL_JOYSTICK_DEV_HANDLE *)p_gpio_local_cfg;

    return OPRT_OK;
}

OPERATE_RET tdd_joystick_register(char *name, JOYSTICK_GPIO_CFG_T *gpio_cfg)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_JOYSTICK_CTRL_INFO ctrl_info;
    TDL_JOYSTICK_DEV_HANDLE handle = NULL;
    TDL_JOYSTICK_DEVICE_INFO_T device_info;

    memset(&ctrl_info, 0, sizeof(TDL_JOYSTICK_CTRL_INFO));

    ctrl_info.joystick_create = __tdd_create_gpio_joystick;
    ctrl_info.joystick_delete = __tdd_delete_gpio_joystick;
    ctrl_info.read_value = __tdd_read_gpio_joystick_value;

    // 添加新按键并载入数据,生成句柄
    ret = __add_new_joystick(name, gpio_cfg, &handle);
    if (OPRT_OK != ret) {
        PR_ERR("gpio add err");
        return ret;
    }

    if (NULL != handle) {
        device_info.dev_handle = handle;
        device_info.mode = gpio_cfg->mode;
    }

    ret = tdl_joystick_register(name, &ctrl_info, &device_info);
    if (OPRT_OK != ret) {
        PR_ERR("tdl joystick resgest err");
        return ret;
    }

    PR_DEBUG("tdd_joystick_register succ");
    return ret;
}

// 更新按键配置的有效电平
OPERATE_RET tdd_joystick_update_level(TDL_JOYSTICK_DEV_HANDLE handle, TUYA_GPIO_LEVEL_E level)  
{
    JOYSTICK_GPIO_CFG_T *p_gpio_cfg = NULL;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    p_gpio_cfg = (JOYSTICK_GPIO_CFG_T *)handle;

    p_gpio_cfg->level = level;

    return OPRT_OK;
}
