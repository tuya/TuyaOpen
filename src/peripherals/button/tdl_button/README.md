# Button Function Component
## Basic Features

1. Provides button management mechanism, supporting short press, long press, and continuous press.

2. Supports two button detection mechanisms: polling and interrupt.

## Resource Dependency

| Resource            | Size  | Description |
| ------------------- | ----- | ----------- |
| Registering a button | ~2.3k |             |

## Interface List Description

- Button management interfaces. For detailed usage, refer to `tdl_button_manage.h`.

| Interface                                                        | Description         |
| ---------------------------------------------------------------- | ------------------- |
| OPERATE_RET tdl_button_create(char *name, TDL_BUTTON_CFG_T *button_cfg, TDL_BUTTON_HANDLE *handle); | Create a button instance |
| OPERATE_RET tdl_button_delete(TDL_BUTTON_HANDLE handle);         | Delete a button     |
| void tdl_button_event_register(TDL_BUTTON_HANDLE handle, TDL_BUTTON_TOUCH_EVENT_E event, TDL_BUTTON_EVENT_CB cb); | Register button event |
| OPERATE_RET tdl_button_deep_sleep_ctrl(uint8_t enable);          | Enable button       |

## Button Event Description

Below is a detailed description of button events:

| Button Event                  | Description                                                                 |
| ----------------------------- | --------------------------------------------------------------------------- |
| `TDL_BUTTON_PRESS_DOWN`       | Triggered when the button is pressed down.                                  |
| `TDL_BUTTON_PRESS_UP`         | Triggered when the button is released.                                      |
| `TDL_BUTTON_PRESS_SINGLE_CLICK` | Triggered when the button is quickly pressed and released once.             |
| `TDL_BUTTON_PRESS_DOUBLE_CLICK` | Triggered when the button is quickly pressed and released twice.            |
| `TDL_BUTTON_PRESS_REPEAT`     | Triggered when the button is pressed multiple times consecutively.          |
| `TDL_BUTTON_LONG_PRESS_START` | Triggered when the button is held down for longer than the configured long press time. |
| `TDL_BUTTON_LONG_PRESS_HOLD`  | Triggered periodically when the button is held down beyond the configured hold time. |
| `TDL_BUTTON_RECOVER_PRESS_UP` | Triggered when the button remains active after power-on and then recovers.  |

## Usage Instructions

```C
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#define BUTTON_DEV    "my_device"

TDL_BUTTON_HANDLE  button_handle;

/* Button callback function. Registered events will notify through this callback. */
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

    /* Register the button */
    op_ret = tdd_gpio_button_register(BUTTON_DEV, &gpio_cfg);
    if (OPRT_OK != op_ret) {
        PR_ERR("tdd_gpio_button_register err:%d", op_ret);
        return;
    } 

    /* Create the button and start checking */
    op_ret = tdl_button_create(BUTTON_DEV, &button_cfg, &button_handle);
    if(OPRT_OK != op_ret) {
        PR_ERR("tdl_button_create err:%d", op_ret);
        return;
    }

    /* Register a short press event. Only registered events will notify through the callback. */
    tdl_button_event_register(button_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, tdl_button_cb);
}
```
