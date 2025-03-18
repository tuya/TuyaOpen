/**
 * @file tdd_button_gpio.c
 * @brief tdd_button_gpio, irq
 * @version 1.0
 * @date 2022-03-20
 * @copyright Copyright (c) tuya.inc 2022
 * GPIO button adaptation
 */

#include "string.h"

// TDL
#include "tdl_button_manage.h"
#include "tdd_button_gpio.h"

#include "tal_memory.h"
#include "tal_log.h"

#include "tkl_gpio.h"

// 添加按键并存入数据
static OPERATE_RET __add_new_button(char *name, BUTTON_GPIO_CFG_T *data, DEVICE_BUTTON_HANDLE *handle)
{
    BUTTON_GPIO_CFG_T *p_gpio_local_cfg = NULL;

    if (NULL == data) {
        return OPRT_INVALID_PARM;
    }
    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    // 申请存储空间
    p_gpio_local_cfg = (BUTTON_GPIO_CFG_T *)tal_malloc(sizeof(BUTTON_GPIO_CFG_T));
    if (NULL == p_gpio_local_cfg) {
        PR_ERR("tdd gpio malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(p_gpio_local_cfg, 0, sizeof(BUTTON_GPIO_CFG_T));
    memcpy(p_gpio_local_cfg, data, sizeof(BUTTON_GPIO_CFG_T));

    *handle = (DEVICE_BUTTON_HANDLE *)p_gpio_local_cfg;

    return OPRT_OK;
}

// 按键硬件初始化
static OPERATE_RET __tdd_create_gpio_button(TDL_BUTTON_OPRT_INFO *dev)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    BUTTON_GPIO_CFG_T *p_gpio_local = NULL;

    if (NULL == dev) {
        PR_ERR("tdd dev err");
        return OPRT_INVALID_PARM;
    }

    if (NULL == dev->dev_handle) {
        PR_ERR("tdd dev handle err");
        return OPRT_INVALID_PARM;
    }

    p_gpio_local = (BUTTON_GPIO_CFG_T *)(dev->dev_handle);

    // GPIO硬件初始化
    if (p_gpio_local->mode == BUTTON_TIMER_SCAN_MODE) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg;
        gpio_cfg.direct = TUYA_GPIO_INPUT;
        gpio_cfg.level = p_gpio_local->level;
        gpio_cfg.mode = p_gpio_local->pin_type.gpio_pull;

        ret = tkl_gpio_init(p_gpio_local->pin, &gpio_cfg);
        if (OPRT_OK != ret) {
            PR_ERR("gpio select err");
            return ret;
        }
    } else if (p_gpio_local->mode == BUTTON_IRQ_MODE) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {0};
        gpio_cfg.direct = TUYA_GPIO_INPUT;
        gpio_cfg.level = p_gpio_local->level;
        gpio_cfg.mode = (p_gpio_local->level == TUYA_GPIO_LEVEL_HIGH) ? TUYA_GPIO_PULLUP : TUYA_GPIO_PULLDOWN;
        ret = tkl_gpio_init(p_gpio_local->pin, &gpio_cfg);
        if (OPRT_OK != ret) {
            PR_ERR("irq gpio init err");
            return ret;
        }

        TUYA_GPIO_IRQ_T gpio_irq_cfg;
        gpio_irq_cfg.mode = p_gpio_local->pin_type.irq_edge;
        gpio_irq_cfg.cb = dev->irq_cb;
        gpio_irq_cfg.arg = p_gpio_local;
        ret = tkl_gpio_irq_init(p_gpio_local->pin, &gpio_irq_cfg);
        if (OPRT_OK != ret) {
            PR_ERR("gpio irq init err=%d", ret);
            return OPRT_COM_ERROR;
        }

        ret = tkl_gpio_irq_enable(p_gpio_local->pin);
        if (OPRT_OK != ret) {
            PR_ERR("gpio irq enable err=%d", ret);
            return OPRT_COM_ERROR;
        }
    }

    return OPRT_OK;
}

// 删除一个按键
static OPERATE_RET __tdd_delete_gpio_button(TDL_BUTTON_OPRT_INFO *dev)
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
static OPERATE_RET __tdd_read_gpio_value(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value)
{
    OPERATE_RET ret = -1;
    TUYA_GPIO_LEVEL_E result;
    BUTTON_GPIO_CFG_T *p_local_cfg = NULL;

    if (dev == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == dev->dev_handle) {
        PR_ERR("handle not get");
        return OPRT_INVALID_PARM;
    }

    p_local_cfg = (BUTTON_GPIO_CFG_T *)(dev->dev_handle);

    ret = tkl_gpio_read(p_local_cfg->pin, &result);
    if (OPRT_OK == ret) {
        // PR_NOTICE("pin=%d,result=%d",p_local_cfg->pin, result);
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

OPERATE_RET tdd_gpio_button_register(char *name, BUTTON_GPIO_CFG_T *gpio_cfg)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_BUTTON_CTRL_INFO ctrl_info;
    DEVICE_BUTTON_HANDLE handle = NULL;
    TDL_BUTTON_DEVICE_INFO_T device_info;

    memset(&ctrl_info, 0, sizeof(TDL_BUTTON_CTRL_INFO));

    ctrl_info.button_create = __tdd_create_gpio_button;
    ctrl_info.button_delete = __tdd_delete_gpio_button;
    ctrl_info.read_value = __tdd_read_gpio_value;

    // 添加新按键并载入数据,生成句柄
    ret = __add_new_button(name, gpio_cfg, &handle);
    if (OPRT_OK != ret) {
        PR_ERR("gpio add err");
        return ret;
    }

    if (NULL != handle) {
        device_info.dev_handle = handle;
        device_info.mode = gpio_cfg->mode;
    }

    ret = tdl_button_register(name, &ctrl_info, &device_info);
    if (OPRT_OK != ret) {
        PR_ERR("tdl button resgest err");
        return ret;
    }

    PR_DEBUG("tdd_gpio_button_register succ");
    return ret;
}

// 更新按键配置的有效电平
OPERATE_RET tdd_gpio_button_update_level(DEVICE_BUTTON_HANDLE handle, TUYA_GPIO_LEVEL_E level)
{
    BUTTON_GPIO_CFG_T *p_gpio_cfg = NULL;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    p_gpio_cfg = (BUTTON_GPIO_CFG_T *)handle;

    p_gpio_cfg->level = level;

    return OPRT_OK;
}
