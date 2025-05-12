# WATCHDOG 驱动

## 简要说明

看门狗是一个定时器电路，负责在程序跑飞或硬件停滞的情况下对处理器进行复位操作，使程序能从头执行。

工作原理：看门狗是一个定时器电路，这个电路有一个输入和一个输出，输入就是喂狗操作（至于什么是喂狗，见下文），所谓喂狗，就是通过外部输入重装载看门狗计数器的值，输出接到另一个电路的复位端。当看门狗的计数器由初始值递减至0时，输出一个信号到另一个电路的复位端，程序执行复位操作。

使用方法：为监测程序是否跑飞，在程序中隔一段时间执行一次喂狗操作，即在一个完整的程序段中，间隔性的放入多个喂狗操作，如果程序在某个点出错开始跑飞，那下一次的喂狗操作就得不到执行，这样的话，当看门狗的计数器由初始值递减至0时，程序执行复位操作。要注意的是，要注意喂狗的时间间隔，不能太晚，太晚的话，超过了计数器的溢出时间，程序就执行复位操作了，就相当于错误的判断成程序跑飞了。


## API 描述

### tkl_watchdog_init

```c
uint32_t tkl_watchdog_init(TUYA_WDOG_BASE_CFG_T *cfg);
```

- 功能描述:
  
  - 根据配置初始化看门狗，返回初始化结果。
  
- 参数:
  
  - cfg：看门狗的基本配置
  
    ```c
    typedef struct {
        uint32_t interval_ms; //看门狗时间
    } TUYA_WDOG_BASE_CFG_T;
    ```
  
- 返回值:
  
  - OPRT_OK 成功，如果大于 0，则为实际的看门狗时间。

### tkl_watchdog_deinit

```c
OPERATE_RET tkl_watchdog_deinit(void);
```

- 功能描述:
  - watchdog反初始化，停止watchdog
- 参数:
  - void
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_watchdog_refresh

```c
OPERATE_RET tkl_watchdog_refresh(void);
```

- 功能描述:

  - 喂看门狗

- 参数:

  - void。

- 返回值:

  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

## 示例

### WATCHDOG 示例

```c
/*初始化看门狗*/
TUYA_WDOG_BASE_CFG_T cfg;
uint32_t  actual_interval_ms = 0;
cfg.interval_ms = 100;
actual_interval_ms = tkl_watchdog_init(&cfg);

if (actual_interval_m) {
	/*最终的实际的看门狗时间为 actual_interval_ms*/    
} else {
  	/*最终的实际的看门狗时间为 cfg.interval_ms*/   
}

/*喂狗*/
tkl_watchdog_refresh();

/*反初始化看门狗*/
tkl_watchdog_deinit();

```



