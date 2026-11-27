# Desk Buddy 8 MiB QSPI PSRAM Allocation

> 硬件接口、驱动设计、TKL 堆实现、风险门禁和验收标准见
> [`PSRAM_PORTING.md`](PSRAM_PORTING.md)。

## 物理器件与 OSPI 映射

- 器件：`APS6404L-3SQR-SN`，8 MiB，3.3 V，SPI/QPI SDR。
- 控制器：OSPI0，经 OSPIM Port0 连接。
- OSPI0 memory-mapped 窗口：`0x9000_0000 - 0x907F_FFFF`。
- 第一版运行时 PSRAM 堆：`0x9040_0000 - 0x907F_FFFF`，共 4 MiB。
- `0x9000_0000 - 0x903F_FFFF` 的 4 MiB 保留给后续预加载数据。
- DMA 可访问性、memory-mapped sub-word 写能力和 D-Cache 策略属于
  实板验收门禁，未验证前不得把 PSRAM 当作通用 DMA 缓冲。


## 片上 SRAM 组成

GD32H779IW 数据手册标称总 SRAM 为 1152 KiB。当前 Desk Buddy 工程的生成链接脚本只声明了
AXISRAM 768 KiB，其余区域尚未纳入链接和内存分配。

| SRAM 区域 | 可规划容量 | 访问特性 | 当前工程 | 建议用途 |
|---|---:|---|---|---|
| AXISRAM | 768 KiB | 通用、DMA 可访问 | 已使用 | FreeRTOS 主堆、DMA、音频/网络/显示缓冲 |
| DTCM | 256 KiB | CPU 高速访问，优先给栈和热数据 | 未使用 | 任务栈、LVGL 热对象、TLS/codec 热状态 |
| ITCM | 128 KiB | 指令紧耦合，适合热点代码 | 未使用 | 中断、DMA 回调、音频/显示热点函数 |
| SRAM0 | 16 KiB | AHB/DMA | 未使用 | UART/SPI/QSPI/DVP 临时 DMA 缓冲 |
| SRAM1 | 16 KiB | AHB/DMA | 未使用 | 第二组 DMA 缓冲或显示/音频 staging |
| Backup SRAM | 4 KiB | 备份域，可掉电保持 | 未使用 | 复位原因、运行标志、少量持久配置 |

> 注：芯片内部存在共享 SRAM 区域，不能简单把各最大值重复相加；实际可用总量以芯片手册标称的
> 1152 KiB 为准。当前 AXISRAM 以外的区域必须先扩展 GD32H77X 的链接脚本和 TKL 内存分区才能使用。

| 区域 | 子区域 | 大小 | 用途与访问约束 |
|---|---|---:|---|
| **预加载数据 4096 KiB** | 图片预加载区 | 2048 KiB | 启动时加载图片、表情、图标；只读或顺序访问 |
|  | 音频/提示音预加载区 | 1024 KiB | 本地 TTS 提示音、音效、语音包 |
|  | 字体/模型预加载区 | 768 KiB | 字体位图、轻量模型、配置数据 |
|  | 预加载暂存/对齐区 | 256 KiB | 加载临时缓冲和 4 KiB 对齐余量 |
| **Camera/Video 576 KiB** | Raw frame slot × 3 | 480 KiB | 每帧 160 KiB，满足 320×240 YUV422 153.6 KiB；采集、处理、排队各 1 帧 |
|  | Camera 临时缓冲 | 64 KiB | 拼接、缩放、异常帧丢弃 |
|  | 元数据/碎片余量 | 32 KiB | 帧描述、链表节点、对齐损耗 |
| **AI/Audio 384 KiB** | AI 输入 ring | 32 KiB | PCM/Opus 上传缓存，顺序读写 |
|  | AI 输出 ring | 32 KiB | 云端下行数据缓存 |
|  | TTS/Player ring | 64 KiB | TTS 播放缓冲，抵抗网络抖动 |
|  | PCM 采集/传输暂存 | 64 KiB | CI3422 PCM 分帧、双缓冲和格式转换 |
|  | 播放/编码临时缓冲 | 64 KiB | 本地 Opus、重采样和播放峰值缓冲 |
|  | 音频公共/对齐余量 | 128 KiB | 公共 PSRAM 内存池，按需分配 |
| **Image/JPEG/UI 1536 KiB** | JPEG 编解码工作区 | 256 KiB | 图片解码输出和编码暂存，分块访问 |
|  | 图片下载 staging | 512 KiB | 下行图片缓冲，避免直接写文件系统 |
|  | 缩放/旋转缓冲 | 256 KiB | 图片后处理，按行或块处理 |
|  | UI 图片运行时缓存 | 384 KiB | 已解码图片、表情、背景缓存 |
|  | UI 热资源缓存 | 128 KiB | 常用小图和页面切换缓存 |
| **公共运行时池 1600 KiB** | 全部动态分配 | 1600 KiB | JPEG/压缩帧不设专用池；与峰值临时缓冲、防碎片余量和诊断空间统一使用公共 PSRAM 内存池 |
| **PSRAM 总计** |  | **8192 KiB** | 预加载固定 4 MiB，运行时固定池和公共池合计 4 MiB |

## 网络与 Tuya Cloud

VW553 负责 WiFi/BLE 协议栈和射频，H77 只承担 AT transport、TLS、MQTT/HTTP 和 Tuya Cloud 业务。以下内存属于 H77。

| 项目 | 推荐位置 | 预算 | 说明 |
|---|---|---:|---|
| VW553 AT session/parser pool | AXISRAM | 约 53 KiB | 32 个 event、12 个 data slot、31 个 line slot，实测池约 54184 B |
| AT RX stream | AXISRAM | 12 KiB | SPI/UART 接收流缓冲，DMA 可访问 |
| SPI frame/transaction staging | SRAM0/SRAM1 | 4 KiB | 256 B 帧、序号、重试和 DMA staging |
| TLS/MbedTLS 工作区 | DTCM | 40～56 KiB | `SSL_MAX_CONTENT_LEN=4096`，包含双向内容缓冲、握手和证书解析 |
| MQTT 网络缓冲 | DTCM/AXISRAM | 4 KiB | `CORE_MQTT_BUFFER_SIZE=2048` 加运行上下文 |
| HTTP/JSON/云端 payload | DTCM | 12～16 KiB | HTTP header、DP、激活、天气和 OTA 元数据 |
| Cloud/MQTT/Netmgr 任务与控制块 | DTCM | 24～32 KiB | workqueue、消息队列、MQTT bind、health monitor 等 |
| 网络峰值/碎片余量 | DTCM/AXISRAM | 16～24 KiB | TLS 握手和重连峰值 |
| **网络/Cloud 合计** |  | **约 160～192 KiB SRAM** | 不计 VW553 自身 RAM |

## AI 基座与音频

CI3422 的 RAM 很小，不承担 Opus 编解码；H77 保留 Opus encoder/decoder、AI client、协议分片和网络上传。

| 项目 | 推荐位置 | 预算 | 说明 |
|---|---|---:|---|
| AI client 任务栈 | DTCM | 16 KiB | `AI_MAX_FRAGMENT_LENGTH=8192` 时协议接收缓冲位于调用栈，8 KiB 栈偏紧 |
| AI input 任务栈 | DTCM | 64 KiB | 本地 Opus encoder 默认高栈需求 |
| AI output 任务栈 | DTCM | 4 KiB | 云端下行流处理 |
| AI biz/chat mode/monitor 任务栈 | DTCM | 8～12 KiB | monitor 可在量产配置关闭 |
| Opus encoder/decoder 状态 | DTCM | 48～80 KiB | CPU 高频随机访问，不放 PSRAM |
| AI input ring | PSRAM | 32 KiB | 压缩帧顺序读写 |
| AI input buffer | PSRAM | 8 KiB | 协议上传拼接 |
| AI output ring | PSRAM | 32 KiB | 云端下行缓存 |
| AI output buffer | PSRAM | 8 KiB | 解包和 TTS 转发 |
| AI write socket buffer | PSRAM | 8 KiB | 网络写缓冲，按平台配置启用 |
| AI session/attribute/event | PSRAM | 16 KiB | 2 个 session、5 个 biz 的上下文和属性 |
| **AI 基座 PSRAM 小计** |  | **约 96～128 KiB** | 音频 ring 和 PCM 缓冲已计入 AI/Audio 384 KiB 池 |

## Screen/LVGL 相关内存

| 项目 | 放置位置 | 大小 | 说明 |
|---|---|---:|---|
| LVGL partial draw buffer × 2 | AXISRAM | 约 52 KiB | 360×360 RGB565，`LV_DRAW_BUF_PARTS=10`；同时作为 QSPI/DMA 数据源 |
| LVGL 旋转缓冲 | AXISRAM | 约 26 KiB | 仅当屏幕旋转角度不为 0 时存在；需要 DMA 可见 |
| LVGL 对象/样式/文本布局堆 | DTCM | 64～96 KiB | UI 高频随机访问；不要跟随全局 `LV_MEM_CUSTOM_ALLOC` 放到 PSRAM |
| QSPI 显示驱动描述符/DMA 状态 | AXISRAM | < 8 KiB | 不保存整屏图像 |
| 摄像头 RGB565 预览缓冲 | PSRAM | 150 KiB | 320×240 RGB565；由 Camera/UI 预览使用 |
| UI 图片解码/运行时缓存 | PSRAM | 384 KiB | 已在 Image/JPEG/UI 公共池中 |
| 整屏 RGB565 framebuffer | 不使用 | 253 KiB/块 | 360×360×2；必须使用 partial flush，禁止走整屏双缓冲 |

## 片上 SRAM 目标分配

| 区域 | 推荐分配 | 约束 |
|---|---|---|
| AXISRAM 768 KiB | FreeRTOS 主堆、DVP/SPI/QSPI DMA、LVGL draw buffer、音频/网络大缓冲 | 保持现有链接区，作为通用和 DMA 主内存 |
| DTCM 256 KiB | 任务栈、TLS/codec 状态、LVGL 对象堆、CPU 高频数据 | 不将普通 DMA 目标放入 DTCM，除非确认对应 DMA 可访问 |
| ITCM 128 KiB | 中断、DMA 回调、音频/显示热点代码 | 以函数级 section 放置，不当作通用堆 |
| SRAM0 16 KiB | 串口/SPI/QSPI 小 DMA staging | 与 AXISRAM DMA 缓冲分开，降低争用 |
| SRAM1 16 KiB | DVP/音频第二组 staging | 供高优先级 DMA 路径使用 |
| Backup SRAM 4 KiB | 复位原因、运行状态、少量持久配置 | 不作为普通 heap 或大缓冲 |
