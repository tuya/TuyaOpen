# 触摸驱动例程

## 概述

本例程将介绍如何使用 `touch` 外设的实现原理、配置方法和开发技巧。触摸驱动基于电容式触摸感应技术，通过检测人体接触时电容值的变化来识别触摸事件。

* 触摸传感器简介

  电容式触摸检测的基本原理是利用人体作为导体，当手指接近或接触触摸电极时，会改变电极与地之间的电容值。驱动通过持续监测这种电容变化，结合复杂的信号处理算法，准确识别触摸、释放和长按等不同的用户操作。

## 功能特性

### 核心功能

触摸驱动提供了完整的触摸检测方案，支持多达16个独立的触摸通道。驱动内置了三种主要的触摸事件检测：

**触摸按下事件**：当检测到有效触摸时立即触发，响应时间通常在几十毫秒内。这是最基本也是最重要的触摸事件，用于响应用户的基本交互操作。系统通过`TUYA_TOUCH_EVENT_PRESSED`事件类型来通知应用层。

**触摸释放事件**：当手指离开触摸区域时触发，确保每次触摸操作都有完整的开始和结束。释放检测采用了防抖动算法，避免因手指轻微移动造成的误判。通过`TUYA_TOUCH_EVENT_RELEASED`事件类型传递给应用。

**长按事件**：当触摸持续超过预设时间（默认2秒）时触发，常用于实现特殊功能或进入设置模式。长按时间可以根据应用需求进行调整。系统使用`TUYA_TOUCH_EVENT_LONG_PRESS`事件类型
来标识长按操作。（**触摸时间超过10s后会清除触摸状态，需要释放后再次触发**）

![tkl_touch_event_process_zh](https://images.tuyacn.com/fe-static/docs/img/1723970a-7d89-4a51-98fa-3cd1154db5c0.png)

## 驱动流程架构

### 信号处理流程

触摸检测的信号处理过程是一个复杂的多级处理流程，每一级都有其特定的作用：

![tkl_touch_single_process_zh](https://images.tuyacn.com/fe-static/docs/img/eef1dfeb-edb9-4e3c-b0c2-4c0182a07d2e.png)

**原始信号采集**：驱动首先通过硬件电路获取每个触摸通道的原始电容值。这个过程需要精确的时序控制和模拟电路设计，以确保测量的准确性和重复性。采集频率通常在几十到几百赫兹之间，以平衡响应速度和功耗。

**预处理滤波**：原始信号通常包含各种噪声和干扰，需要通过初级滤波来去除高频噪声和随机干扰。这一步骤对后续处理的准确性至关重要。预处理阶段会去除明显的异常值和脉冲干扰。

**自适应基线跟踪**：基线是指无触摸时的电容值，它会随着环境条件的变化而缓慢变化。驱动使用智能算法持续跟踪和更新基线，确保触摸检测的准确性不受环境变化影响。基线更新速度会根据信号的稳定性自动调整。

**触摸信号检测**：通过比较当前信号与基线的差值，结合预设的阈值参数，判断是否发生了有效的触摸事件。这个过程使用了滞回比较器的概念，避免信号在阈值附近波动时产生的误判。

**事件状态管理**：将检测到的信号变化转换为具体的触摸事件，包括按下、释放和长按的识别。状态机确保事件的逻辑正确性和时序准确性。

### 滤波算法详解

驱动采用了多种滤波算法的组合，以应对不同类型的干扰和噪声：

![tkl_touch_filter_process_zh.png](https://images.tuyacn.com/fe-static/docs/img/e8af3eaf-a530-4041-a427-3301ea79989f.png)

**IIR数字滤波器**：无限脉冲响应滤波器用于平滑信号变化，去除高频噪声。驱动根据信号的变化速度自动调整滤波器参数，在快速响应和稳定性之间找到最佳平衡。当检测到快速变化时，滤波器会减少延迟以提高响应速度。

**中值滤波器**：有效去除脉冲型干扰和随机噪声。通过对一定时间窗口内的信号进行排序，选择中值作为输出，能够很好地保持信号的边沿特性。这种滤波方法对于去除偶发的强干扰特别有效。

**方波滤波算法**：这是专门为触摸信号设计的滤波算法，能够快速响应触摸的开始和结束，同时在稳定状态下保持良好的抗噪声性能。算法会根据信号的变化特征自动选择合适的滤波强度。

**自适应滤波**：根据信号的统计特性和环境条件，动态调整滤波参数。在信号稳定时使用较强的滤波以提高抗噪声能力，在信号变化时减少滤波以提高响应速度。

## 关键配置参数详解

```c
typedef struct {
    TUYA_TOUCH_SENSITIVITY_LEVEL_E sensitivity_level;  // Sensitivity level
    TUYA_TOUCH_DETECT_THRESHOLD_E  detect_threshold;   // Detection threshold
    TUYA_TOUCH_DETECT_RANGE_E      detect_range;       // Detection range
    TUYA_TOUCH_THRESHOLD_T         threshold;          // threshold parameters
} TUYA_TOUCH_CONFIG_T;
```

前三个参数用于原厂初始化，一般按照例程中内容保持默认即可。

  - `sensitivity_level`: 触摸灵敏度级别，用于控制触摸的敏感度。
  - `detect_threshold`: 触摸检测阈值，用于判断触摸是否发生。
  - `detect_range`: 触摸检测范围，用于指定触摸检测的响应范围。

```c
typedef struct {
    float touch_static_noise_threshold;
    float touch_filter_update_threshold;
    float touch_detect_threshold;
    float touch_variance_threshold;
} TUYA_TOUCH_THRESHOLD_T;
```

`threshold` 参数用于自定义触摸参数，需根据实际应用需求进行设置。

 - `touch_static_noise_threshold` 静态噪声阈值，指传感器数值的波动范围，一般设置为略大于静止状态下的传感器数值 **最大值**
 - `touch_filter_update_threshold` 滤波参数更新阈值，指滤波参数的更新范围，一般设置为静止状态下的传感器数值最大值
 - `touch_detect_threshold` 触摸检测阈值，指触摸检测的阈值，一般设置为略小于`touch_filter_update_threshold` 的值
 - `touch_variance_threshold` 触摸原始数据的方差值，一般按照例程中保持默认

:tipping_hand_man: 开发者可以根据 `tkl_touch_get_filter_update_threshold` 函数与 `tkl_touch_get_single_median_filter_value` 函数获取返回参数做差估算得到 `touch_filter_update_threshold` ，根据上述提示调整其他参数。
:tipping_hand_woman: 在调试阈值过程中，可以开启 `tkl_touch.c` 中的 `DEBUG_ENABLE` 宏，配合微软商店的 **串口调试助手** 查看波形辅助调试。

## 运行结果

```c
ap0:W(306):baseline[1]: 7.521484
ap0:W(307):Touch init success, channel_mask=0x2
ap0:W(308):[01-01 00:00:00 ty D][example_touch.c:101] touch channel [1] cap value: 0.000000
ap0:W(1808):[01-01 00:00:01 ty D][example_touch.c:101] touch channel [1] cap value: 7.658960
ap0:W(2070):[01-01 00:00:02 ty N][example_touch.c:44] *** TOUCH EVENT PRESSD DOWN *** Channel 1
ap0:W(2308):[01-01 00:00:02 ty D][example_touch.c:101] touch channel [1] cap value: 8.372679
ap0:W(4808):[01-01 00:00:04 ty D][example_touch.c:101] touch channel [1] cap value: 8.856133
ap0:W(5043):[01-01 00:00:05 ty N][example_touch.c:52] *** TOUCH EVENT LONG PRESSED *** Channel 1
ap0:W(5308):[01-01 00:00:05 ty D][example_touch.c:101] touch channel [1] cap value: 8.928961
ap0:W(5808):[01-01 00:00:05 ty D][example_touch.c:101] touch channel [1] cap value: 8.825094
[01-01 00:00:06 ty N][example_touch.c:48] *** TOUCH EVENT RELEASED UP *** Channel 1
```

