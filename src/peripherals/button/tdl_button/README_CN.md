# 按键功能组件
## 基本功能

1、提供按键管理机制，支持短按、长按、连续按

2、支持轮询和中断两种按键检测机制

## 资源依赖

| 资源               | 大小   | 说明 |
| ------------------ | ------ | ---- |
| 完成一个按键的注册 | 约2.3k |      |

## 接口列表说明

- 按键管理接口，详细使用说明参考`tdl_button_manage.h`

| 接口                                                         | 说明             |
| ------------------------------------------------------------ | ---------------- |
| OPERATE_RET tdl_button_create(char *name, TDL_BUTTON_CFG_T *button_cfg, TDL_BUTTON_HANDLE *handle); | 创建一个按键实例 |
| OPERATE_RET tdl_button_delete(TDL_BUTTON_HANDLE handle);  | 删除一个按键     |
| void tdl_button_event_register(TDL_BUTTON_HANDLE handle, TDL_BUTTON_TOUCH_EVENT_E event, TDL_BUTTON_EVENT_CB cb); | 按键事件注册     |
| OPERATE_RET tdl_button_deep_sleep_ctrl(uint8_t enable);       | 使能按键         |

## 按键事件说明

以下是按键事件的详细说明：

| 按键事件                     | 说明                                                                 |
| ---------------------------- | -------------------------------------------------------------------- |
| `TDL_BUTTON_PRESS_DOWN`      | 按下触发事件，当按键被按下时触发。                                   |
| `TDL_BUTTON_PRESS_UP`        | 松开触发事件，当按键被释放时触发。                                   |
| `TDL_BUTTON_PRESS_SINGLE_CLICK` | 单击触发事件，当按键被快速按下一次并释放时触发。                     |
| `TDL_BUTTON_PRESS_DOUBLE_CLICK` | 双击触发事件，当按键被快速按下两次并释放时触发。                     |
| `TDL_BUTTON_PRESS_REPEAT`    | 多击触发事件，当按键被连续按下多次时触发。                           |
| `TDL_BUTTON_LONG_PRESS_START` | 长按开始触发事件，当按键被按住超过设定的长按时间时触发。              |
| `TDL_BUTTON_LONG_PRESS_HOLD` | 长按保持触发事件，当按键被持续按住超过设定的保持时间时周期性触发。     |
| `TDL_BUTTON_RECOVER_PRESS_UP` | 恢复触发事件，当设备上电后按键一直保持有效电平并恢复时触发。          |


## 使用说明

```C
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#define BUTTON_DEV    "my_device"

TDL_BUTTON_HANDLE  button_handle;

/* 按键回调函数，注册的事件触发后会通过该回调进行通知 */
static void tdl_button_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void* argc)
{
    static int key_count = 0;
    PR_NOTICE("key_count %d", key_count);
    key_count++;
}

void tuya_button_dev_demo(void)
{
	int op_ret = 0;
	
	BUTTON_GPIO_CFG_T gpio_cfg;
    TDL_BUTTON_CFG_T button_cfg;	
    
    gpio_cfg.pin = 9;
    gpio_cfg.level = 1;
    gpio_cfg.mode = BUTTON_TIMER_SCAN_MODE;
    
    button_cfg.long_start_valid_time = 3000;
    button_cfg.long_keep_timer = 1000;
    button_cfg.button_debounce_time = 50;
    button_cfg.button_repeat_valid_count = 3;
    button_cfg.button_repeat_valid_time = 50;

    /* 注册按钮 */
    op_ret = tdd_gpio_button_register(BUTTON_DEV, &gpio_cfg);
    if (OPRT_OK != op_ret) {
        PR_ERR("tdd_gpio_button_register err:%d", op_ret);
        return;
    } 

    /* 创建按钮，开始检查按键 */
    op_ret = tdl_button_create(BUTTON_DEV, &button_cfg, &button_handle);
    if(OPRT_OK != op_ret) {
        PR_ERR("tdl_button_create err:%d", op_ret);
        return;
    }

    /* 注册短按事件，只有注册的事件才会在回调中进行通知 */
    tdl_button_event_register(button_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, tdl_button_cb);
}
```
