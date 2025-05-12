# RTC 驱动

## 简要说明

​	实时时钟（RTC）是专用于维持一秒时基的计时器。此外 RTC 通常用于在软件或硬件中跟踪时钟时间和日历日期。RTC 的许多功能是非常专业的，是维持高精度和非常可靠的操作所必需的。

​	一般的 SOC 内部自带 RTC 硬件单元，可以直接操作寄存器去设置读取 RTC 时间。部分的 SOC 也可以也可以通过 IIC、SPI 接口外扩 RTC 的外设。

## API 描述

### tkl_rtc_init

```c
OPERATE_RET tkl_rtc_init(void);
```

- 功能描述:
  - 初始化 RTC，返回初始化结果。
  
- 参数:
  - void 
  
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_rtc_deinit

```c
OPERATE_RET tkl_rtc_deinit(void);
```

- 功能描述:
  - RTC 反初始化，停止 RTC
- 参数:
  - void
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_rtc_time_set

```c
OPERATE_RET tkl_rtc_time_set(TIME_T time_sec);
```

- 功能描述:

  - 设置 RTC 的时间

- 参数:

  - time_sec: 一个 UTC 时间。

  
```c
  typedef unsigned int TIME_T;
```
  
- 返回值:

  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_rtc_time_get

```c
OPERATE_RET tkl_rtc_time_get(TIME_T *time_sec);
```

- 功能描述:
  
  - 获取 RTC 的时间
- 参数:
  
  - time_sec：UTC 时间
  
    ```c
    typedef unsigned int TIME_T;
    ```
- 返回值:
  
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

## 示例

### RTC 示例

```c
/*初始化RTC*/
tkl_rtc_init(void);

/*设置RTC时间*/
TIME_T time_sec_set = 0x1000000;
tkl_rtc_time_set(&time_sec_set);

/*获取RTC时间*/
TIME_T time_sec_get
tkl_rtc_time_get(&time_sec_get);

/*反初始化RTC*/
tkl_rtc_deinit(void);

```

