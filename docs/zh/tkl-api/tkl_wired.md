# WIRED

`tkl_wired.c` 文件是一个有线驱动，该文件提供了获取有线链接状态、IP 地址、MAC 地址及注册状态改变回调函数的功能。本文件是由涂鸦操作系统（TuyaOS）自动生成的，其中留有一定范围供开发者在标记的区域内自定义和实现相关逻辑。

## API 说明

### tkl_wired_get_status

```c
OPERATE_RET tkl_wired_get_status(TKL_WIRED_STAT_E *status);
```

#### 功能

获取有线连接的链接状态。

#### 参数

- `status`: 用于存储链接状态的指针。

#### 返回值

- `OPRT_OK`: 成功。
- 其他错误码: 请参见 `tuya_error_code.h`。


### tkl_wired_set_status_cb

```c
OPERATE_RET tkl_wired_set_status_cb(TKL_WIRED_STATUS_CHANGE_CB cb);
```

#### 功能

注册网络状态变化的回调函数。

#### 参数

- `cb`: 状态变化时的回调函数。

#### 返回值

- `OPRT_OK`: 成功。
- 其他错误码: 请参见 `tuya_error_code.h`。

### tkl_wired_get_ip

```c
OPERATE_RET tkl_wired_get_ip(NW_IP_S *ip);
```

#### 功能

获取有线链接的IP地址。

#### 参数

- `ip`: 用于存储 IP 地址的指针。

#### 返回值

- `OPRT_OK`: 成功。
- 其他错误码: 请参见 `tuya_error_code.h`。

### tkl_wired_get_mac

```c
OPERATE_RET tkl_wired_get_mac(NW_MAC_S *mac);
```

#### 功能

获取有线链接的 MAC 地址。

#### 参数

- `mac`: 用于存储 MAC 地址的指针。

#### 返回值

- `OPRT_OK`: 成功。
- 其他错误码: 请参见 `tuya_error_code.h`。

### 注意事项

默认 MAC 地址是硬编码的。

### tkl_wired_set_mac

```c
OPERATE_RET tkl_wired_set_mac(const NW_MAC_S *mac);
```

#### 功能

设置有线链接的 MAC 地址。

#### 参数

- `mac`: 包含要设置的 MAC 地址的指针。

#### 返回值

- `OPRT_OK`: 成功。
- 其他错误码: 请参见 `tuya_error_code.h`。

### 注意事项

在当前的代码风格下，`tkl_wired_set_mac` 函数只返回 `OPRT_OK`，无实际设置 MAC 地址的操作。
