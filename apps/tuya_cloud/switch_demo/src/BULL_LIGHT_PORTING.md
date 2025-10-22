# 公牛灯控制模块移植说明

## 概述
本模块提供了完整的公牛酷毙灯控制功能，通过继电器模拟触摸按键来控制灯具的各种功能。

## 文件结构
```
TuyaOpen/apps/tuya_cloud/switch_demo/src/
├── bull_light_control.h      # 核心头文件
├── bull_light_control.c      # 核心实现文件
├── bull_light_config.h       # 配置文件
└── BULL_LIGHT_PORTING.md     # 移植说明文档
```

## 移植步骤

### 1. 复制文件
将以下文件复制到目标项目：
- `bull_light_control.h`
- `bull_light_control.c`
- `bull_light_config.h`

### 2. 修改硬件配置
编辑 `bull_light_config.h` 文件，根据实际硬件修改以下配置：

#### 2.1 GPIO引脚配置
```c
// 根据实际硬件连接修改GPIO引脚
#define BULL_RELAY_POWER_PIN       GPIO_NUM_2   // 电源开关继电器
#define BULL_RELAY_BRIGHTNESS_PIN  GPIO_NUM_3   // 亮度调节继电器
#define BULL_RELAY_COLOR_PIN       GPIO_NUM_4   // 色温调节继电器
```

#### 2.2 继电器触发方式
```c
// 根据继电器模块特性修改触发电平
#define BULL_RELAY_TRIGGER_LEVEL   TKL_GPIO_LEVEL_LOW   // 低电平触发
#define BULL_RELAY_RELEASE_LEVEL   TKL_GPIO_LEVEL_HIGH  // 高电平释放
```

### 3. 修改时间参数
根据实际测试结果调整时间参数：
```c
#define BULL_TOUCH_SHORT_PRESS_MS  100  // 短按持续时间
#define BULL_TOUCH_LONG_PRESS_MS   2000 // 长按持续时间
#define BULL_TOUCH_RELEASE_TIME_MS 50   // 按键释放后等待时间
#define BULL_TOUCH_INTERVAL_MS     200  // 连续按键间隔时间
```

### 4. 集成到主程序

#### 4.1 包含头文件
```c
#include "bull_light_control.h"
```

#### 4.2 初始化模块
```c
// 在系统初始化时调用
bull_light_control_init();
```

#### 4.3 处理DP命令
```c
// 在DP命令处理函数中调用
bull_light_handle_dp_command(dpobj);
```

### 5. 功能测试

#### 5.1 基本功能测试
- 开关控制
- 亮度调节
- 色温调节

#### 5.2 高级功能测试
- 短按调节
- 长按连续调节
- 状态同步

## 注意事项

### 1. 硬件连接
- 确保继电器模块正确连接到指定的GPIO引脚
- 检查继电器模块的电源和地线连接
- 确认继电器模块的触发电平

### 2. 时间参数
- 不同型号的公牛灯可能需要不同的时间参数
- 建议通过实际测试确定最佳参数
- 时间参数过短可能导致按键无效
- 时间参数过长可能影响用户体验

### 3. 状态同步
- 模块内部维护灯光状态，可能与实际灯具状态不同步
- 建议定期同步状态或通过其他方式获取实际状态

### 4. 错误处理
- 模块包含基本的错误处理机制
- 建议在调用模块函数时检查返回值
- 可以根据需要添加更详细的错误处理

## 扩展功能

### 1. 环境光反馈
可以添加环境光传感器，实现自动亮度调节功能。

### 2. 定时控制
可以添加定时开关功能，实现定时控制。

### 3. 场景模式
可以添加场景模式功能，实现一键切换不同的灯光效果。

### 4. 状态上报
可以添加状态上报功能，实时同步灯具状态到云端。

## 技术支持
如有问题，请参考代码注释或联系技术支持。
