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

### 0. 非阻塞UART模式 (Non-blocking UART - 最关键的设计决策)

**问题**: 在多线程共享UART资源的场景下，如果使用阻塞模式，会导致严重的死锁问题。

**阻塞模式的问题**:
```c
// 阻塞模式配置
TAL_UART_CFG_T cfg = {
    .open_mode = O_BLOCK  // ❌ 危险！
};

// 阻塞模式下的tal_uart_read行为
int tal_uart_read(UART_NUM port, uint8_t *data, uint32_t len) {
    if (buffer_empty) {
        uart_info->wait_rx_flag = TRUE;
        // ❌ 线程会永久阻塞在这个信号量上
        tal_semaphore_wait(uart_info->rx_block_sem, SEM_WAIT_FOREVER);
    }
    return tuya_ring_buff_read(rx_ring, data, len);
}

// 致命场景:
// 1. Worker线程调用tal_uart_read，没有数据，阻塞在rx_block_sem上
// 2. Printer线程调用tal_uart_deinit，释放了uart_info及其信号量
// 3. Printer线程调用tal_uart_init，创建新的uart_info和新的信号量
// 4. Worker线程仍然阻塞在已被释放的旧信号量上 → 永久死锁！
```

**非阻塞模式的解决方案**:
```c
// 非阻塞模式配置 ✅
TAL_UART_CFG_T cfg = {
    .open_mode = 0  // 非阻塞模式
};

// 非阻塞模式下的tal_uart_read行为
int tal_uart_read(UART_NUM port, uint8_t *data, uint32_t len) {
    // ✅ 立即返回，不等待信号量
    return tuya_ring_buff_read(rx_ring, data, len);  // 返回0或实际读取字节数
}

// Worker线程的非阻塞轮询
while (running) {
    int read_len = tal_uart_read(USR_UART_NUM, buffer, size);
    if (read_len > 0) {
        process_data();
    } else {
        // ✅ 没有数据就主动休眠，不会被动阻塞
        tal_system_sleep(100);
    }
}
```

**为什么非阻塞模式是必须的**:
1. ✅ **避免死锁**: Worker线程不会阻塞在已被释放的信号量上
2. ✅ **响应模式切换**: 可以及时检测到`sg_mode_switch_request`标志
3. ✅ **容错性强**: UART重新初始化不会导致线程失效
4. ✅ **可控休眠**: 通过`tal_system_sleep`主动控制轮询频率

**性能影响**:
- ⚠️ **CPU使用略增**: 轮询模式会增加少量CPU使用(可通过调整sleep时间优化)
- ✅ **响应性良好**: 100ms轮询间隔对RFID/AI Log场景完全足够
- ✅ **可配置**: 不同模式可以使用不同的休眠时间(RFID: 100ms, AI Log: 50ms)

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

**问题**: 原设计中三个线程独立管理UART，切换时需要复杂的线程创建/销毁逻辑。

**解决方案**: 使用单一 `uart_worker_thread` 统一处理UART读取：
- 所有UART读取操作在一个线程中完成
- 根据当前模式分发数据到不同的处理函数
- 模式切换只需更改状态变量和波特率，无需重启线程
- **使用非阻塞模式**避免线程在UART重新初始化时死锁

```c
// 工作线程核心逻辑
while (running) {
    // 1. 检查模式切换请求
    lock_mutex();
    need_switch = mode_switch_request;
    new_mode = target_mode;
    unlock_mutex();
    
    if (need_switch) {
        // 2. 重新初始化UART(非阻塞模式)
        reinit_uart_with_new_baudrate();
        
        // 3. 更新状态
        lock_mutex();
        current_mode = new_mode;
        mode_switch_request = FALSE;
        unlock_mutex();
    }
    
    // 4. 非阻塞读取数据
    read_len = uart_read(buffer);  // 立即返回，不等待信号量
    
    // 5. 根据模式处理
    if (read_len > 0) {
        switch (current_mode) {
            case UART_MODE_RFID_SCAN:
                process_rfid_data();
                sleep(100ms);
                break;
            case UART_MODE_AI_LOG:
                process_ai_log_data();
                sleep(50ms);
                break;
        }
    } else {
        sleep(100ms);  // 无数据时等待
    }
}
```

**关键优化**：
- ✅ **非阻塞UART**: `open_mode = 0`，避免阻塞在信号量上
- ✅ **最小锁范围**: 只在读取/修改共享状态时持锁
- ✅ **自适应休眠**: 根据当前模式调整sleep时间(RFID: 100ms, AI Log: 50ms)

### 2. 回调注册机制 (Callback Registration)

**问题**: 数据从UART到UI的传递路径复杂。

**解决方案**: UART层提供回调注册，由UART层内部使用：

```c
// UART层内部注册数据回调
static void __ai_log_uart_data_callback(UART_MODE_E mode, const uint8_t *data, size_t len)
{
    if (mode != UART_MODE_AI_LOG || !data || len == 0) {
        return;
    }
    
    // 处理UART数据
    app_display_send_msg(POCKET_DISP_TP_AI_LOG, data, len);
    
    // 非流式状态时上传
    if (!app_get_text_stream_status()) {
        ai_text_agent_upload(data, len);
    }
}

// 当AI log screen初始化时，自动注册此回调
uart_expand_register_callback(UART_MODE_AI_LOG, __ai_log_uart_data_callback);
```

**数据处理流程**:
1. Worker线程读取UART数据
2. 根据模式调用对应的处理函数
3. AI Log模式: 通过KMP搜索查找"ty E"标记
4. 找到标记后调用注册的回调函数
5. 回调函数发送消息到显示层并上传到云端

### 3. 互斥锁保护 (Mutex Protection)

**问题**: 多个线程可能同时访问UART资源，导致竞态条件。

**解决方案**: 使用互斥锁保护关键状态和UART操作：

```c
// 模式切换时的锁保护
OPERATE_RET uart_expand_switch_mode(UART_MODE_E mode)
{
    tal_mutex_lock(sg_mode_mutex);
    if (sg_current_mode == mode) {
        tal_mutex_unlock(sg_mode_mutex);
        return OPRT_OK;  // 已经是目标模式
    }
    
    sg_target_mode = mode;
    sg_mode_switch_request = TRUE;
    tal_mutex_unlock(sg_mode_mutex);
    
    // 等待worker线程完成切换(轮询mode_switch_request标志)
    for (uint32_t timeout = 0; sg_mode_switch_request && timeout < 200; timeout += 10) {
        tal_system_sleep(10);
    }
    
    return sg_mode_switch_request ? OPRT_COM_ERROR : OPRT_OK;
}
```

**锁的使用原则**:
- ✅ **最小锁范围**: 只在访问共享变量时持锁
- ✅ **避免死锁**: 打印机线程仅在波特率切换时持锁，打印过程不持锁
- ✅ **快速释放**: Worker线程读取状态后立即释放锁
- ✅ **轮询等待**: 模式切换使用轮询而非阻塞等待，避免信号量问题

### 4. 打印机独立线程 (Independent Printer Thread)

**设计理由**:
- 打印机功能需要持续运行，不受模式切换影响
- 使用Ring Buffer解耦数据生产者和消费者
- UTF8到GBK的转换是耗时操作，独立线程避免阻塞主流程
- 打印机使用9600低波特率，需要特殊的UART切换机制

```
┌──────────────┐     Ring Buffer     ┌──────────────────┐
│ Data Producer│────────────────────▶│ Printer Thread   │
│  (任意模块)   │    uart_print_write │  (独立运行)       │
└──────────────┘                     └──────────────────┘
                                              │
                                              ▼
                                     ┌────────────────┐
                                     │ 1. 检测数据    │
                                     │ 2. 切换波特率  │
                                     │ 3. UTF8→GBK   │
                                     │ 4. 逐字符打印  │
                                     │ 5. 恢复波特率  │
                                     │ 6. 唤醒worker  │
                                     └────────────────┘
```

**打印线程核心流程**:
```c
while (running) {
    // 1. 检查Ring Buffer
    if (buffer_empty) {
        if (was_printing) {
            // 打印结束，恢复UART
            lock_mutex();
            reinit_uart(original_baudrate);
            trigger_mode_switch_request();  // 唤醒worker线程
            unlock_mutex();
            was_printing = FALSE;
        }
        sleep(100ms);
        continue;
    }
    
    // 2. 开始打印会话
    if (!is_printing) {
        lock_mutex();
        save_current_mode();
        reinit_uart(9600);
        unlock_mutex();
        is_printing = TRUE;
    }
    
    // 3. 逐字符处理(不持锁)
    read_utf8_char_from_buffer();
    convert_utf8_to_gbk();
    print_to_device();
    sleep(5ms);
}
```

**关键点**:
- ✅ **即时打印**: 检测到数据立即开始打印，无批处理延迟
- ✅ **细粒度锁**: 仅在UART重新初始化时持锁
- ✅ **Worker同步**: 打印完成后通过mode_switch_request强制worker重新初始化UART
- ✅ **非阻塞设计**: 配合worker线程的非阻塞模式，避免死锁

## API设计

### UI层接口 (纯粹的UI)
```c
// ai_log_screen.h - UI只提供生命周期通知
typedef void (*ai_log_screen_lifecycle_cb_t)(BOOL_T is_init);
void ai_log_screen_register_lifecycle_cb(ai_log_screen_lifecycle_cb_t callback);
void ai_log_screen_update_log(const char *log_text, size_t length);
void ai_log_screen_append_log(const char *log_text, size_t length);
void ai_log_screen_clear_log(void);
```

### UART层接口 (内部使用，不被UI直接调用)
```c
// uart_expand.h - UART管理接口
OPERATE_RET uart_expand_init(void);
OPERATE_RET uart_expand_switch_mode(UART_MODE_E mode);
UART_MODE_E uart_expand_get_mode(void);
OPERATE_RET uart_expand_register_callback(UART_MODE_E mode, uart_data_callback_t callback);

// uart_expand.h - 打印机接口(任意模块可调用)
uint32_t uart_print_write(const uint8_t *data, size_t len);
```

### 数据回调类型
```c
// 数据回调函数签名
typedef void (*uart_data_callback_t)(UART_MODE_E mode, const uint8_t *data, size_t len);
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

4. uart_expand_switch_mode 执行
   └─ 设置 sg_mode_switch_request = TRUE
   └─ 轮询等待 sg_mode_switch_request 变为 FALSE (最多200ms)

5. UART worker线程检测到模式切换请求
   └─ 读取 sg_mode_switch_request 和 sg_target_mode
   └─ 调用 __uart_reinit_with_baudrate(460800)
        ├─ tal_uart_deinit (释放旧UART资源)
        ├─ tal_uart_init (open_mode=0, 非阻塞模式)
        └─ sg_current_baudrate = 460800
   └─ 设置 sg_mode_switch_request = FALSE
   └─ 开始以460800波特率接收AI日志数据

6. 数据到达时 (worker线程非阻塞轮询)
   └─ read_len = tal_uart_read() (立即返回，不阻塞)
   └─ 如果 read_len > 0:
        ├─ __process_ai_log_data 处理
        ├─ KMP搜索 "ty E" 标记
        ├─ __ai_log_uart_data_callback 被调用
        ├─ app_display_send_msg(POCKET_DISP_TP_AI_LOG)
        └─ ai_text_agent_upload (如果非流式状态)
   └─ 如果 read_len == 0: 休眠50ms后继续轮询
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
   └─ __uart_reinit_with_baudrate(115200)
   └─ sg_mode_switch_request = FALSE
   └─ 恢复RFID扫描 (100ms轮询间隔)
```

### 场景: 打印文本 (并发执行)

```
并发场景: Worker线程正在RFID模式，同时需要打印文本

1. 应用调用 uart_print_write(utf8_data, len)
   └─ 数据写入 Ring Buffer

2. Printer线程检测到 Ring Buffer有数据
   └─ 上锁 tal_mutex_lock(sg_mode_mutex)
   └─ 保存当前模式 saved_mode = sg_current_mode (RFID_SCAN)
   └─ __uart_reinit_with_baudrate(9600)
   └─ 解锁 tal_mutex_unlock(sg_mode_mutex)
   └─ 开始逐字符打印 (不持锁)

3. Worker线程继续运行 (非阻塞)
   └─ tal_uart_read 返回0 (因为UART被printer占用)
   └─ 休眠100ms后继续轮询
   └─ ✅ 不会死锁，因为使用非阻塞模式

4. Printer线程完成打印
   └─ 上锁 tal_mutex_lock(sg_mode_mutex)
   └─ __uart_reinit_with_baudrate(115200)
   └─ 设置 sg_mode_switch_request = TRUE
   └─ 解锁 tal_mutex_unlock(sg_mode_mutex)

5. Worker线程检测到 sg_mode_switch_request
   └─ 重新初始化UART (获取新的UART句柄)
   └─ ✅ RFID扫描恢复正常
```

## 模式特性

| 模式 | 波特率 | 用途 | 数据处理 | 备注 |
|-----|--------|------|---------|------|
| UART_MODE_RFID_SCAN | 115200 | RFID卡片扫描 | 长度检查(>28字节) + CRC校验 | 默认模式 |
| UART_MODE_AI_LOG | 460800 | AI日志接收 | KMP搜索("ty E"标记) + 回调通知 | 按需切换 |
| Printer (非模式) | 9600 | 打印机输出 | Ring Buffer + UTF8→GBK | **独立线程,即时打印** |

### 打印机特殊处理机制

打印机**不是**一个可切换的UART模式，而是采用**即时打印机制**：

```
数据流: 应用 → uart_print_write() → Ring Buffer → 打印机线程

打印流程:
1. 打印机线程持续监控Ring Buffer
2. 当检测到数据可用时:
   a. 保存当前UART模式
   b. 上锁并切换到9600波特率
   c. 解锁，开始逐字符打印(UTF8→GBK转换)
3. 当Ring Buffer清空时:
   a. 如果非流式状态，添加换行符
   b. 上锁并恢复原UART模式波特率
   c. 触发mode_switch_request唤醒worker线程
   d. 解锁
4. 继续监控Ring Buffer等待下次打印
```

**关键设计点**:
- ✅ **非阻塞UART模式**: 使用`open_mode = 0`避免worker线程阻塞在旧的信号量上
- ✅ **即时字符打印**: 逐个UTF8字符读取、转换、打印，无批处理
- ✅ **最小锁粒度**: 仅在波特率切换时持有mutex，打印过程不持锁
- ✅ **Worker线程同步**: 打印完成后通过mode_switch_request强制worker线程重新初始化UART
- ✅ **流式支持**: 根据`app_get_text_stream_status()`决定是否添加换行符

**为何采用即时打印而非批量打印**:
- ❌ 批量打印会导致用户感知延迟
- ✅ 即时打印提供更好的实时反馈
- ✅ 9600波特率足够慢，逐字符打印不会产生明显性能问题
- ✅ 简化了缓冲区管理逻辑

## 优势总结

✅ **UI层纯粹**: UI只关注显示，不依赖UART/硬件头文件  
✅ **自动化管理**: UART模式切换完全自动化，UI无需手动控制  
✅ **线程管理简化**: 从3个独立线程减少到2个(worker + printer)  
✅ **线程安全**: 互斥锁保护关键状态，最小锁粒度避免性能损失  
✅ **避免死锁**: 非阻塞UART模式防止线程在UART重新初始化时阻塞  
✅ **即时打印**: 检测到数据立即开始打印，提供良好的用户体验  
✅ **扩展性强**: 新增模式只需添加枚举和处理函数  
✅ **资源高效**: 避免频繁创建/销毁线程  
✅ **分层清晰**: UI层、管理层、硬件层职责明确  
✅ **容错性**: Worker线程持续运行，UART错误自动恢复

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

⚠️ **非阻塞模式关键**: UART必须使用非阻塞模式(`open_mode = 0`)，否则worker线程会在UART重新初始化后永久阻塞在旧的信号量上  
⚠️ **模式切换同步**: 打印机线程完成打印后必须通过`sg_mode_switch_request`通知worker线程重新初始化UART  
⚠️ **回调执行时间**: 回调函数应尽快返回，避免阻塞UART线程  
⚠️ **打印机Buffer**: Ring Buffer满时会停止写入(OVERFLOW_STOP_TYPE)，需确保打印机线程正常运行  
⚠️ **锁的粒度**: 只在访问共享状态时持锁，打印过程不应持锁  
⚠️ **UTF8处理**: 需等待完整的多字节UTF8字符到达后再转换，避免乱码

## 已知问题与解决方案

### 问题1: Worker线程在UART重新初始化后停止响应
**原因**: 使用阻塞模式(`O_BLOCK`)时，worker线程阻塞在`tal_uart_read`的信号量等待中。当printer线程deinit/reinit UART后，旧的信号量被释放，worker线程永久阻塞。

**解决方案**: 
```c
// 使用非阻塞模式
TAL_UART_CFG_T cfg = {
    .open_mode = 0  // 非阻塞，tal_uart_read立即返回
};
```

### 问题2: 打印后RFID扫描失败
**原因**: Printer线程reinit UART后，worker线程的UART句柄失效，但worker线程未感知。

**解决方案**:
```c
// Printer线程完成打印后
tal_mutex_lock(sg_mode_mutex);
__uart_reinit_with_baudrate(original_baudrate);
sg_target_mode = saved_mode;
sg_mode_switch_request = TRUE;  // 强制worker重新初始化UART
tal_mutex_unlock(sg_mode_mutex);
```  

## 未来优化方向

- [ ] 支持模式优先级队列(当前是简单的请求-响应模式)
- [ ] 添加模式切换事件通知(让其他模块感知模式变化)
- [ ] 统计各模式使用时长(用于性能分析)
- [ ] 支持多UART端口管理(当前硬编码TUYA_UART_NUM_2)
- [ ] 优化打印机批量处理(考虑小批量累积以减少切换次数)
- [ ] 添加UART错误恢复机制(当前只有简单的错误日志)
- [ ] 实现数据流控制(Ring Buffer接近满时的背压处理)

## 技术债务

1. **硬编码配置**: 波特率、休眠时间等配置硬编码在宏定义中，应考虑配置文件或运行时配置
2. **错误处理**: 当前错误处理比较简单，建议增加重试机制和故障转移
3. **性能监控**: 缺少性能指标收集(如数据吞吐量、模式切换延迟等)
4. **测试覆盖**: 缺少边界条件测试(如频繁切换、大量数据、异常断电等)
