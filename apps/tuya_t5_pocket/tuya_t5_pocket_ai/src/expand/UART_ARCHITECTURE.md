# UART 多模式架构设计文档

## 设计目标

解决三个线程共享UART资源的互斥问题,并实现UI层与UART管理层的解耦。

## 架构概述

```
┌─────────────────────────────────────────────────────────┐
│                    UI Layer (显示层)                      │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │ RFID Screen │  │ AI Log Screen│  │ Printer UI   │   │
│  │             │  │  (注册回调)   │  │              │   │
│  └─────────────┘  └──────┬───────┘  └──────────────┘   │
│                          │                               │
│                          │ Lifecycle Callback            │
│                          │ (init/deinit通知)             │
└──────────────────────────┼───────────────────────────────┘
                           ▼
┌─────────────────────────────────────────────────────────┐
│              UART Expansion Layer (管理层)                │
│  ┌──────────────────────────────────────────────────┐  │
│  │    __ai_log_screen_lifecycle_handler()           │  │
│  │    - 监听UI生命周期                               │  │
│  │    - 自动注册UART回调                             │  │
│  │    - 自动切换UART模式                             │  │
│  └──────────────────────────────────────────────────┘  │
│                                                          │
│  ┌──────────────────────────────────────────────────┐  │
│  │         Mode State Machine (状态机)               │  │
│  │  ┌─────────┐    ┌──────────┐    ┌────────┐      │  │
│  │  │ RFID    │───▶│ AI Log   │───▶│ Printer│      │  │
│  │  │ 115200  │◀───│ 460800   │◀───│ 9600   │      │  │
│  │  └─────────┘    └──────────┘    └────────┘      │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                         │
                         │ UART Read/Write
                         ▼
┌─────────────────────────────────────────────────────────┐
│              Hardware Layer (硬件层)                      │
│  ┌──────────────────────────────────────────────────┐  │
│  │         UART Worker Thread (统一工作线程)          │  │
│  │   - 波特率动态切换                                  │  │
│  │   - 模式感知数据处理                                │  │
│  │   - 回调通知                                        │  │
│  └──────────────────────────────────────────────────┘  │
│                                                          │
│  ┌──────────────────────────────────────────────────┐  │
│  │      Printer Thread (独立后台线程)                  │  │
│  │   - Ring Buffer处理                                │  │
│  │   - UTF8 to GBK转换                                │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## 核心设计模式

### 1. 生命周期回调模式 (Lifecycle Callback Pattern)

**设计理念**: UI层不应该了解UART的细节,只需要提供生命周期通知接口。

**实现方式**:

```c
// ai_log_screen.h - UI层接口
typedef void (*ai_log_screen_lifecycle_cb_t)(BOOL_T is_init);
void ai_log_screen_register_lifecycle_cb(ai_log_screen_lifecycle_cb_t callback);

// ai_log_screen.c - UI层实现
void ai_log_screen_init(void) {
    // UI初始化逻辑
    mount_sd_card();
    create_ui_widgets();
    
    // 通知外部模块 (不关心谁在监听)
    if (sg_lifecycle_callback) {
        sg_lifecycle_callback(TRUE);  // 告诉外界:我初始化了
    }
}

void ai_log_screen_deinit(void) {
    // UI清理逻辑
    destroy_ui_widgets();
    
    // 通知外部模块
    if (sg_lifecycle_callback) {
        sg_lifecycle_callback(FALSE);  // 告诉外界:我要销毁了
    }
}
```

```c
// uart_expand.c - UART层处理
static void __ai_log_screen_lifecycle_handler(BOOL_T is_init)
{
    if (is_init) {
        // UI初始化时,自动配置UART
        uart_expand_register_callback(UART_MODE_AI_LOG, __ai_log_uart_data_callback);
        uart_expand_switch_mode(UART_MODE_AI_LOG);
    } else {
        // UI销毁时,自动恢复UART
        uart_expand_register_callback(UART_MODE_AI_LOG, NULL);
        uart_expand_switch_mode(UART_MODE_RFID_SCAN);
    }
}

// 在uart_expand_init()中注册
ai_log_screen_register_lifecycle_cb(__ai_log_screen_lifecycle_handler);
```

**优势**:
- ✅ UI层完全不依赖UART头文件
- ✅ UI层不需要知道UART模式、波特率等细节
- ✅ UI层只关心自己的显示逻辑
- ✅ UART层自动响应UI生命周期

### 2. 统一工作线程 (Unified Worker Thread)

**问题**: 原设计中三个线程独立管理UART,切换时需要复杂的线程创建/销毁逻辑。

**解决方案**: 使用单一 `uart_worker_thread` 统一处理UART读取:
- 所有UART读取操作在一个线程中完成
- 根据当前模式分发数据到不同的处理函数
- 模式切换只需更改状态变量和波特率,无需重启线程

```c
// 工作线程伪代码
while (running) {
    // 检查模式切换请求
    if (mode_switch_request) {
        reinit_uart_with_new_baudrate();
        current_mode = target_mode;
    }
    
    // 读取数据
    read_len = uart_read(buffer);
    
    // 根据模式处理
    switch (current_mode) {
        case UART_MODE_RFID_SCAN:
            process_rfid_data();
            break;
        case UART_MODE_AI_LOG:
            process_ai_log_data();
            break;
    }
}
```

### 2. 回调注册机制 (Callback Registration)

**问题**: 数据从UART到UI的传递路径复杂。

**解决方案**: UART层提供回调注册,但由UART层自己使用:

```c
// UART层内部注册数据回调
static void __ai_log_uart_data_callback(UART_MODE_E mode, const uint8_t *data, size_t len)
{
    // 处理UART数据
    app_display_send_msg(POCKET_DISP_TP_AI_LOG, data, len);
    ai_text_agent_upload(data, len);
}

// 当AI log screen初始化时,自动注册此回调
uart_expand_register_callback(UART_MODE_AI_LOG, __ai_log_uart_data_callback);
```

### 3. 互斥锁保护 (Mutex Protection)

**问题**: 多个模块可能同时请求模式切换,导致竞态条件。

**解决方案**: 使用互斥锁保护模式切换:

```c
OPERATE_RET uart_expand_switch_mode(UART_MODE_E mode)
{
    tal_mutex_lock(sg_mode_mutex);
    
    sg_target_mode = mode;
    sg_mode_switch_request = TRUE;
    
    tal_mutex_unlock(sg_mode_mutex);
    
    // 等待切换完成
    wait_for_mode_switch_complete();
}
```

### 4. 打印机独立线程 (Independent Printer Thread)

**设计理由**:
- 打印机功能需要持续运行,不受模式切换影响
- 使用Ring Buffer解耦数据生产者和消费者
- UTF8到GBK的转换是耗时操作,独立线程避免阻塞主流程

```
┌──────────────┐     Ring Buffer     ┌──────────────────┐
│ Data Producer│────────────────────▶│ Printer Thread   │
│  (任意模式)   │    uart_print_write │  (独立运行)       │
└──────────────┘                     └──────────────────┘
```

## API设计

### UI层接口 (纯粹的UI)
```c
// ai_log_screen.h
void ai_log_screen_register_lifecycle_cb(ai_log_screen_lifecycle_cb_t callback);
void ai_log_screen_update_log(const char *log_text, size_t length);
void ai_log_screen_append_log(const char *log_text, size_t length);
void ai_log_screen_clear_log(void);
```

### UART层接口 (不被UI直接调用)
```c
// uart_expand.h
OPERATE_RET uart_expand_switch_mode(UART_MODE_E mode);
UART_MODE_E uart_expand_get_mode(void);
OPERATE_RET uart_expand_register_callback(UART_MODE_E mode, uart_data_callback_t callback);
uint32_t uart_print_write(const uint8_t *data, size_t len);
```

## 完整调用流程

### 场景: 用户进入AI日志界面

```
1. 用户操作 → screen_manager 切换到 ai_log_screen

2. ai_log_screen_init() 被调用
   └─ 挂载SD卡
   └─ 创建UI控件
   └─ 调用 sg_lifecycle_callback(TRUE)

3. __ai_log_screen_lifecycle_handler(TRUE) 被触发
   └─ uart_expand_register_callback(UART_MODE_AI_LOG, __ai_log_uart_data_callback)
   └─ uart_expand_switch_mode(UART_MODE_AI_LOG)

4. UART worker线程自动切换
   └─ 重新初始化UART(460800波特率)
   └─ 开始接收AI日志数据

5. 数据到达时
   └─ __uart_worker_thread 读取数据
   └─ __process_ai_log_data 处理
   └─ __ai_log_uart_data_callback 被调用
   └─ app_display_send_msg(POCKET_DISP_TP_AI_LOG)
   └─ UI显示更新
```

### 场景: 用户退出AI日志界面

```
1. 用户按ESC → screen_manager 返回上一界面

2. ai_log_screen_deinit() 被调用
   └─ 清理UI控件
   └─ 调用 sg_lifecycle_callback(FALSE)

3. __ai_log_screen_lifecycle_handler(FALSE) 被触发
   └─ uart_expand_register_callback(UART_MODE_AI_LOG, NULL)
   └─ uart_expand_switch_mode(UART_MODE_RFID_SCAN)

4. UART worker线程自动恢复
   └─ 重新初始化UART(115200波特率)
   └─ 恢复RFID扫描
```

## 模式特性

| 模式 | 波特率 | 用途 | 数据处理 | 备注 |
|-----|--------|------|---------|------|
| UART_MODE_RFID_SCAN | 115200 | RFID卡片扫描 | CRC校验 + 回调通知 | 主UART模式 |
| UART_MODE_AI_LOG | 460800 | AI日志接收 | KMP搜索 + 上传云端 | 主UART模式 |
| Printer (非模式) | 9600 | 打印机输出 | Ring Buffer + UTF8→GBK | **独立线程,临时切换** |

### 打印机特殊处理机制

打印机**不是**一个可切换的UART模式,而是采用特殊的批量打印机制:

```
数据流: 应用 → uart_print_write() → Ring Buffer → 打印机线程

打印流程:
1. 打印机线程从Ring Buffer读取UTF8数据
2. 批量累积数据到批处理缓冲区(256字节)
3. 当批处理缓冲区满 或 Ring Buffer空时:
   a. 保存当前UART模式
   b. 临时切换到9600波特率
   c. 批量发送数据到打印机
   d. 立即恢复原UART模式
4. 继续处理下一批数据
```

**设计原因**:
- ✅ 减少波特率切换次数(批量打印)
- ✅ 最小化对RFID/AI Log的干扰
- ✅ 避免单字节打印导致频繁切换
- ✅ 保证打印机9600波特率需求

## 优势总结

✅ **UI层纯粹**: UI只关注显示,不依赖UART/硬件头文件  
✅ **自动化管理**: UART模式切换完全自动化,UI无需手动控制  
✅ **线程管理简化**: 从3个独立线程减少到2个(worker + printer)  
✅ **线程安全**: 互斥锁保护关键状态  
✅ **扩展性强**: 新增模式只需添加枚举和处理函数  
✅ **资源高效**: 避免频繁创建/销毁线程  
✅ **分层清晰**: UI层、管理层、硬件层职责明确

## 层次职责划分

| 层次 | 职责 | 不应该做的事 |
|-----|------|------------|
| UI层 | 显示内容、响应用户操作、提供生命周期通知 | ❌ 直接操作UART<br>❌ 了解波特率<br>❌ 包含硬件头文件 |
| 管理层(UART) | 监听UI生命周期、管理UART模式、分发数据 | ❌ 直接操作UI控件<br>❌ 了解UI布局 |
| 硬件层 | UART读写、线程调度、硬件配置 | ❌ 了解业务逻辑<br>❌ 直接更新UI |

## 代码示例对比

### ❌ 旧代码 (耦合严重)

```c
// ai_log_screen.c
#include "uart_expand.h"  // UI依赖UART头文件 ❌
#include "app_display.h"
#include "ai_audio.h"

void ai_log_screen_init(void) {
    // UI直接控制UART线程 ❌
    rfid_log_scan_start();
}

void ai_log_screen_deinit(void) {
    // UI直接控制UART线程 ❌
    rfid_log_scan_stop();
}
```

### ✅ 新代码 (解耦清晰)

```c
// ai_log_screen.c
// 只包含必要的UI相关头文件 ✅
#include "ai_log_screen.h"
#include "tkl_fs.h"  // 只为SD卡操作

void ai_log_screen_init(void) {
    mount_sd_card();
    create_ui_widgets();
    
    // 只通知生命周期,不关心谁在监听 ✅
    if (sg_lifecycle_callback) {
        sg_lifecycle_callback(TRUE);
    }
}

void ai_log_screen_deinit(void) {
    cleanup_ui_widgets();
    
    // 只通知生命周期,不关心谁在监听 ✅
    if (sg_lifecycle_callback) {
        sg_lifecycle_callback(FALSE);
    }
}
```

```c
// uart_expand.c
// UART层自动响应UI生命周期 ✅
static void __ai_log_screen_lifecycle_handler(BOOL_T is_init)
{
    if (is_init) {
        // 自动配置UART
        uart_expand_register_callback(UART_MODE_AI_LOG, __ai_log_uart_data_callback);
        uart_expand_switch_mode(UART_MODE_AI_LOG);
    } else {
        // 自动恢复UART
        uart_expand_register_callback(UART_MODE_AI_LOG, NULL);
        uart_expand_switch_mode(UART_MODE_RFID_SCAN);
    }
}

// 在初始化时注册监听 ✅
uart_expand_init() {
    // ...
    ai_log_screen_register_lifecycle_cb(__ai_log_screen_lifecycle_handler);
}
```

## 注意事项

⚠️ **模式切换时机**: 避免在UART正在传输数据时切换模式  
⚠️ **回调执行时间**: 回调函数应尽快返回,避免阻塞UART线程  
⚠️ **打印机Buffer**: Ring Buffer满时会停止写入,需确保打印机线程正常运行  

## 未来优化方向

- [ ] 支持模式优先级队列
- [ ] 添加模式切换事件通知
- [ ] 统计各模式使用时长
- [ ] 支持多UART端口管理
