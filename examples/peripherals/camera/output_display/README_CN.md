# 相机输出显示数据流向文档

## 概述

本文档详细介绍了 `example_camera.c` 中从相机采集到屏幕显示的完整数据流向，包括各阶段的数据格式、处理方法和转换过程。

## 数据流向图

```
相机采集 → YUV422格式 → 格式转换 → 显示缓冲区 → 旋转处理 → 屏幕刷新
   ↓           ↓            ↓            ↓           ↓          ↓
硬件设备    YUYV编码    RGB565/Binary   帧缓冲     可选变换    LCD显示
```

---

## 1. 相机数据采集阶段

### 1.1 数据源
- **硬件设备**: 通过 `tdl_camera_find_dev(CAMERA_NAME)` 获取相机句柄
- **采集配置**: `TDL_CAMERA_CFG_T` 结构体

### 1.2 相机配置参数
```c
TDL_CAMERA_CFG_T cfg = {
    .fps = EXAMPLE_CAMERA_FPS,          // 帧率
    .width = EXAMPLE_CAMERA_WIDTH,      // 图像宽度
    .height = EXAMPLE_CAMERA_HEIGHT,    // 图像高度
    .out_fmt = TDL_CAMERA_FMT_YUV422,   // 输出格式：YUV422
    .get_frame_cb = callback_function    // 帧回调函数
};
```

### 1.3 输出数据格式：YUV422

#### 1.3.1 YUV色彩空间基础

**YUV色彩空间简介**:
- **Y (Luminance)**: 亮度分量，表示图像的灰度信息
  - 范围: 0-255 (8位)
  - 0 = 黑色，255 = 白色
  - 单独使用Y分量即可得到灰度图像
  
- **U (Cb - Blue-difference)**: 蓝色色度分量
  - 范围: 0-255 (实际表示 -128 到 +127)
  - 表示蓝色与亮度的差值
  - 128 = 无色度偏移
  
- **V (Cr - Red-difference)**: 红色色度分量
  - 范围: 0-255 (实际表示 -128 到 +127)
  - 表示红色与亮度的差值
  - 128 = 无色度偏移

**为什么使用YUV**:
1. **符合人眼特性**: 人眼对亮度变化比色彩变化更敏感
2. **压缩效率高**: 可以对色度分量进行降采样而不影响感知质量
3. **兼容黑白系统**: Y分量可直接用于黑白显示
4. **传输带宽优化**: 减少色度信息传输，节省带宽

#### 1.3.2 YUV422采样格式

**采样比例说明**:
- **YUV422**: 色度水平采样减半，垂直不变
- **采样比例**: Y:U:V = 4:2:2
- **含义**: 每2个像素共享一对U/V色度值，但各有独立的Y亮度值

**采样示意图**:
```
原始像素:  P0    P1    P2    P3    P4    P5
Y 采样:    Y0    Y1    Y2    Y3    Y4    Y5  (每像素一个)
U 采样:    U0          U2          U4        (每2像素一个)
V 采样:    V0          V2          V4        (每2像素一个)

实际存储: [Y0 U0 Y1 V0] [Y2 U2 Y3 V2] [Y4 U4 Y5 V4]
```

**数据压缩效果**:
- RGB888: 每像素3字节 (R+G+B)
- YUV422: 每像素2字节 (平均)
- 压缩比: 33% 数据量减少

#### 1.3.3 YUV422编码格式

YUV422有多种打包方式，本项目使用的是 **UYVY** 格式。

##### 格式对比表

| 格式 | 字节序 | 4字节组 | 像素映射 |
|------|--------|---------|----------|
| **UYVY** | U Y V Y | `[U0][Y0][V0][Y1]` | 像素0=(Y0,U0,V0), 像素1=(Y1,U0,V0) |
| YUYV | Y U Y V | `[Y0][U0][Y1][V0]` | 像素0=(Y0,U0,V0), 像素1=(Y1,U0,V0) |
| YVYU | Y V Y U | `[Y0][V0][Y1][U0]` | 像素0=(Y0,U0,V0), 像素1=(Y1,U0,V0) |
| VYUY | V Y U Y | `[V0][Y0][U0][Y1]` | 像素0=(Y0,U0,V0), 像素1=(Y1,U0,V0) |

##### UYVY格式详解 (本项目使用)

**字节排列**:
```
字节索引:  0    1    2    3    4    5    6    7    8    9   10   11
内容:     [U0] [Y0] [V0] [Y1] [U1] [Y2] [V1] [Y3] [U2] [Y4] [V2] [Y5]
          └─────── 像素0-1 ──────┘ └─────── 像素2-3 ──────┘ └──────...
```

**像素还原**:
```c
// 像素0 (偶数位置)
像素0.Y = data[1]
像素0.U = data[0]
像素0.V = data[2]

// 像素1 (奇数位置) - 共享U/V
像素1.Y = data[3]
像素1.U = data[0]  // 与像素0共享
像素1.V = data[2]  // 与像素0共享
```

**提取Y分量 (用于二值化)**:
```c
// 方法1: 按像素索引提取
for (int x = 0; x < width; x++) {
    int yuv_index = row_offset + x * 2 + 1;  // Y在奇数位置: 1,3,5,7...
    uint8_t Y = yuv422_data[yuv_index];
}

// 方法2: 按字节序列提取
for (int i = 0; i < data_size; i += 4) {
    uint8_t Y0 = data[i + 1];  // 第1个像素的Y
    uint8_t Y1 = data[i + 3];  // 第2个像素的Y
}
```

#### 1.3.4 数据存储布局

**单行像素存储** (宽度=8像素):
```
像素编号:    0      1      2      3      4      5      6      7
字节偏移:   [0][1] [2][3] [4][5] [6][7] [8][9] [10][11] [12][13] [14][15]
UYVY格式:   U0 Y0  V0 Y1  U2 Y2  V2 Y3  U4 Y4  V4 Y5    U6 Y6    V6 Y7
颜色信息:   └─ 像素0-1 ─┘ └─ 像素2-3 ─┘ └─ 像素4-5 ─┘  └─ 像素6-7 ─┘
```

**多行图像存储** (宽度=W, 高度=H):
```
总字节数 = W × H × 2

行0: [U00][Y00][V00][Y01][U02][Y02][V02][Y03]... (W×2 字节)
行1: [U10][Y10][V10][Y11][U12][Y12][V12][Y13]... (W×2 字节)
行2: [U20][Y20][V20][Y21][U22][Y22][V22][Y23]... (W×2 字节)
...
行H-1: ...

访问公式:
byte_index = (y × width + x) × 2 + component_offset
其中 component_offset:
  - U (偶数像素): 0
  - Y (偶数像素): 1
  - V (偶数像素): 2
  - Y (奇数像素): 3
```

#### 1.3.5 实际数据示例

**示例1: 4×2像素的YUV422图像**

```
像素矩阵 (颜色值):
    像素(0,0)     像素(1,0)     像素(2,0)     像素(3,0)
    R=255,G=0,B=0 R=0,G=255,B=0 R=0,G=0,B=255 R=255,G=255,B=255
    (红色)        (绿色)        (蓝色)        (白色)

    像素(0,1)     像素(1,1)     像素(2,1)     像素(3,1)
    R=0,G=0,B=0   R=128,G=128,B=128 R=255,G=0,B=0 R=0,G=255,B=0
    (黑色)        (灰色)        (红色)        (绿色)

转换为YUV (近似值):
行0: [U0=84][Y0=76][V0=255][Y1=150] [U2=255][Y2=29][V2=107][Y3=255]
     └──── 红-绿像素对 ────────┘    └──── 蓝-白像素对 ────────┘

行1: [U4=128][Y4=0][V4=128][Y5=128] [U6=84][Y6=76][V6=255][Y7=150]
     └──── 黑-灰像素对 ────────┘    └──── 红-绿像素对 ────────┘

总字节数: 4像素/行 × 2行 × 2字节/像素 = 16字节
```

**示例2: 提取亮度制作灰度图**

```c
// 原始YUV422数据 (UYVY格式)
uint8_t yuv_data[] = {
    84, 76, 255, 150,  // 行0: 红色(Y=76)和绿色(Y=150)
    255, 29, 107, 255, // 行0: 蓝色(Y=29)和白色(Y=255)
    128, 0, 128, 128,  // 行1: 黑色(Y=0)和灰色(Y=128)
    84, 76, 255, 150   // 行1: 红色(Y=76)和绿色(Y=150)
};

// 提取的Y分量 (灰度值)
uint8_t gray_data[] = {
    76, 150, 29, 255,  // 行0: 暗、亮、很暗、很亮
    0, 128, 76, 150    // 行1: 黑、中灰、暗、亮
};

// 可视化灰度图 (0=黑，255=白)
行0: ▓▓ ░░ ██ ··  (红、绿、蓝、白)
行1: ██ ▒▒ ▓▓ ░░  (黑、灰、红、绿)
```

#### 1.3.6 YUV到RGB转换公式

**标准转换公式** (ITU-R BT.601):
```
R = Y + 1.402 × (V - 128)
G = Y - 0.344136 × (U - 128) - 0.714136 × (V - 128)
B = Y + 1.772 × (U - 128)
```

**整数优化公式** (避免浮点运算):
```c
R = Y + ((V - 128) × 359 + 128) >> 8
G = Y - ((U - 128) × 88 + 128) >> 8 - ((V - 128) × 183 + 128) >> 8
B = Y + ((U - 128) × 454 + 128) >> 8
```

**范围限制**:
```c
#define CLAMP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
R = CLAMP(R);
G = CLAMP(G);
B = CLAMP(B);
```

#### 1.3.7 内存占用计算

**常见分辨率的YUV422数据量**:

| 分辨率 | 像素数 | YUV422大小 | RGB888大小 | 节省比例 |
|--------|--------|-----------|-----------|---------|
| 160×120 | 19,200 | 38,400 字节 (37.5 KB) | 57,600 字节 (56.3 KB) | 33% |
| 320×240 | 76,800 | 153,600 字节 (150 KB) | 230,400 字节 (225 KB) | 33% |
| 640×480 | 307,200 | 614,400 字节 (600 KB) | 921,600 字节 (900 KB) | 33% |
| 1280×720 | 921,600 | 1,843,200 字节 (1.76 MB) | 2,764,800 字节 (2.64 MB) | 33% |
| 1920×1080 | 2,073,600 | 4,147,200 字节 (3.96 MB) | 6,220,800 字节 (5.93 MB) | 33% |

**计算公式**:
```c
yuv422_size = width × height × 2;
rgb888_size = width × height × 3;
savings = 1 - (yuv422_size / rgb888_size) = 33.33%
```

#### 1.3.8 数据结构定义

```c
/**
 * @brief 相机帧数据结构
 */
typedef struct {
    uint8_t *data;              // YUV422原始数据指针 (UYVY格式)
    int width;                  // 图像宽度（像素）
    int height;                 // 图像高度（像素）
    uint32_t data_size;         // 数据大小 = width × height × 2
    uint64_t timestamp;         // 时间戳（微秒）
    uint32_t frame_id;          // 帧序号
} TDL_CAMERA_FRAME_T;
```

**访问宏定义**:
```c
// 计算YUV422缓冲区大小
#define YUV422_BUFFER_SIZE(w, h) ((w) * (h) * 2)

// 获取指定像素的Y分量 (UYVY格式)
#define GET_Y_UYVY(data, x, y, width) \
    ((data)[((y) * (width) + (x)) * 2 + 1])

// 获取指定像素的U分量 (共享，偶数像素)
#define GET_U_UYVY(data, x, y, width) \
    ((data)[(((y) * (width) + ((x) & ~1)) * 2)])

// 获取指定像素的V分量 (共享，偶数像素)
#define GET_V_UYVY(data, x, y, width) \
    ((data)[(((y) * (width) + ((x) & ~1)) * 2 + 2)])
```

#### 1.3.9 格式识别方法

**如何判断是UYVY还是YUYV**:
```c
// 方法1: 检查统计特性
// Y分量范围通常分布在16-235，U/V在16-240
// 统计奇数位置和偶数位置的值分布

// 方法2: 查看已知颜色
// 纯白色(255,255,255): Y≈235, U≈128, V≈128
// 纯黑色(0,0,0): Y≈16, U≈128, V≈128
// 纯红色(255,0,0): Y≈82, U≈90, V≈240

// 方法3: 代码测试
void detect_yuv_format(uint8_t *data, int width, int height) {
    int y_sum_odd = 0, y_sum_even = 0;
    
    for (int i = 0; i < width * height * 2; i++) {
        if (i % 2 == 0) {
            y_sum_even += data[i];
        } else {
            y_sum_odd += data[i];
        }
    }
    
    int avg_odd = y_sum_odd / (width * height);
    int avg_even = y_sum_even / (width * height);
    
    // Y分量平均值通常在100-150之间
    // U/V分量平均值通常在120-135之间（接近128）
    if (abs(avg_odd - 128) < abs(avg_even - 128)) {
        printf("Format: UYVY (Y at odd positions)\n");
    } else {
        printf("Format: YUYV (Y at even positions)\n");
    }
}
```

---

## 2. 数据转换阶段

根据显示设备的像素格式，相机数据会经过不同的转换路径。

### 2.1 转换路径A：YUV422 → RGB565（彩色显示）

#### 2.1.1 适用条件
```c
if (sg_display_info.fmt == TUYA_PIXEL_FMT_RGB565)
```

#### 2.1.2 转换方法
**使用DMA2D硬件加速**（如果支持）:

```c
// 输入帧配置
TKL_DMA2D_FRAME_INFO_T sg_in_frame = {
    .type = TUYA_FRAME_FMT_YUV422,
    .width = frame->width,
    .height = frame->height,
    .pbuf = frame->data
};

// 输出帧配置
TKL_DMA2D_FRAME_INFO_T sg_out_frame = {
    .type = TUYA_FRAME_FMT_RGB565,
    .width = display_width,
    .height = display_height,
    .pbuf = display_buffer
};

// 硬件转换
tkl_dma2d_convert(&sg_in_frame, &sg_out_frame);
```

#### 2.1.3 RGB565格式说明

**位分布结构**:
```
16位RGB565: RRRRR GGGGGG BBBBB
位编号:     15-11  10-5   4-0
```

**颜色范围**:
- **R (Red)**: 5位 (0-31) → 实际显示 0-255 (左移3位)
- **G (Green)**: 6位 (0-63) → 实际显示 0-255 (左移2位)
- **B (Blue)**: 5位 (0-31) → 实际显示 0-255 (左移3位)

**为什么G是6位**？
- 人眼对绿色最敏感，分配更多位数可提升视觉质量
- 5+6+5=16位，正好占用2字节

**RGB888到RGB565的转换**:
```c
// RGB888: R(8bit) G(8bit) B(8bit)
uint8_t r8 = 255, g8 = 128, b8 = 64;

// 转换为RGB565
uint16_t rgb565 = ((r8 >> 3) << 11) |  // R: 取高5位
                  ((g8 >> 2) << 5)  |  // G: 取高6位
                  ((b8 >> 3) << 0);    // B: 取高5位

// 结果: 0xFC84
// 二进制: 1111 1100 1000 0100
//         RRRRR GGGGGG BBBBB
```

**颜色示例**:
```
纯红色:   RGB(255,0,0)   → 0xF800 → 0b11111_000000_00000
纯绿色:   RGB(0,255,0)   → 0x07E0 → 0b00000_111111_00000
纯蓝色:   RGB(0,0,255)   → 0x001F → 0b00000_000000_11111
纯白色:   RGB(255,255,255) → 0xFFFF → 0b11111_111111_11111
纯黑色:   RGB(0,0,0)     → 0x0000 → 0b00000_000000_00000
```

#### 2.1.4 字节交换 - 深入解析

##### 为什么需要RGB565字节交换？

**核心原因：CPU字节序与显示控制器字节序不匹配**

##### 问题背景

**1. 字节序的概念**

在计算机中，多字节数据的存储有两种方式：

- **小端序 (Little-Endian)**: 低字节存储在低地址
- **大端序 (Big-Endian)**: 高字节存储在高地址

**示例：RGB565值 0xF800 (纯红色) 在内存中的存储**

```
小端序 (Little-Endian):
地址 0x1000: [0x00]  低字节先存储
地址 0x1001: [0xF8]  高字节后存储
从CPU读取: 0xF800 ✓

大端序 (Big-Endian):
地址 0x1000: [0xF8]  高字节先存储
地址 0x1001: [0x00]  低字节后存储
从CPU读取: 0xF800 ✓
```

**2. 为什么会出现字节序不匹配**

嵌入式系统中的硬件组件可能使用不同的字节序：

| 组件 | 常见字节序 | 示例 |
|------|-----------|------|
| ARM Cortex-M CPU | 小端序 | ESP32, STM32 |
| DMA2D硬件加速器 | 通常小端序 | 输出RGB565小端格式 |
| LCD控制器 (SPI接口) | **可能是大端序** | 如ST7789, ILI9341 |
| LCD控制器 (并口) | 通常小端序 | RGB接口 |

**3. 具体问题示例**

假设要显示纯红色像素 RGB(255, 0, 0)：

```c
// Step 1: DMA2D转换后的RGB565值
uint16_t pixel = 0xF800;  // 二进制: 11111_000000_00000

// Step 2: CPU内存存储 (小端序)
内存布局:
[地址 0]: 0x00  (低字节)
[地址 1]: 0xF8  (高字节)

// Step 3A: LCD期望大端序 - 错误情况
如果直接发送到大端序LCD:
LCD接收顺序: 0x00, 0xF8
LCD解析为: 0x00F8 = 0b00000_000111_11000
         = R=0, G=1, B=24 → 显示为深蓝色 ❌

// Step 3B: 字节交换后 - 正确情况
交换后内存:
[地址 0]: 0xF8  (高字节)
[地址 1]: 0x00  (低字节)

LCD接收顺序: 0xF8, 0x00
LCD解析为: 0xF800 = 0b11111_000000_00000
         = R=31, G=0, B=0 → 显示为红色 ✓
```

##### 字节交换的实现

**代码实现**:
```c
/**
 * @brief 交换RGB565数据的高低字节
 * @param buffer RGB565数据缓冲区 (uint16_t数组)
 * @param pixel_count 像素数量
 */
void tdl_disp_dev_rgb565_swap(uint16_t *buffer, uint32_t pixel_count)
{
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint16_t pixel = buffer[i];
        // 方法1: 位运算
        buffer[i] = (pixel >> 8) | (pixel << 8);
        
        // 方法2: 字节指针
        // uint8_t *bytes = (uint8_t *)&buffer[i];
        // uint8_t temp = bytes[0];
        // bytes[0] = bytes[1];
        // bytes[1] = temp;
    }
}
```

**交换前后对比**:
```
原始值: 0xF800 (纯红色)
二进制: 11111000 00000000
字节:   [0xF8]   [0x00]

交换后: 0x00F8
二进制: 00000000 11111000
字节:   [0x00]   [0xF8]

内存中的实际变化:
交换前 (小端序): [0x00] [0xF8]
交换后 (大端序): [0xF8] [0x00]
```

##### 如何判断是否需要字节交换

**方法1: 查看硬件数据手册**
```
LCD控制器规格:
- SPI接口的ST7789: 通常需要字节交换
- RGB接口的LCD: 通常不需要字节交换
- 8080并口: 根据具体型号确定
```

**方法2: 实际测试**
```c
// 测试代码：显示纯红色
uint16_t test_pixel = 0xF800;  // 应该是红色

// 如果显示为红色 → 不需要交换
// 如果显示为蓝色/其他颜色 → 需要交换
```

**方法3: 观察显示效果**
| 期望颜色 | 正常显示 | 字节序错误显示 |
|---------|---------|---------------|
| 红色 (0xF800) | 红色 | 深蓝绿色 |
| 绿色 (0x07E0) | 绿色 | 黄绿色 |
| 蓝色 (0x001F) | 蓝色 | 橙红色 |
| 白色 (0xFFFF) | 白色 | 白色 (不变) |
| 黑色 (0x0000) | 黑色 | 黑色 (不变) |

**典型症状**: 颜色整体偏差，特别是纯色显示错误

##### 配置示例

**显示设备信息结构**:
```c
typedef struct {
    TUYA_PIXEL_FMT_E fmt;        // 像素格式
    TUYA_DISPLAY_ROTATION_E rotation;  // 旋转角度
    bool is_swap;                // ← 是否需要字节交换
    // ... 其他字段
} TDL_DISP_DEV_INFO_T;
```

**运行时判断**:
```c
// 在显示初始化时获取
tdl_disp_dev_get_info(sg_tdl_disp_hdl, &sg_display_info);

// 每帧处理时判断
if (sg_display_info.is_swap) {
    // 只有当硬件需要时才执行交换
    tdl_disp_dev_rgb565_swap((uint16_t *)frame_buffer, pixel_count);
}
```

##### 调试技巧

**问题诊断**:
```c
// 1. 显示测试图案
void test_color_pattern(void) {
    uint16_t colors[] = {
        0xF800,  // 红色
        0x07E0,  // 绿色
        0x001F,  // 蓝色
        0xFFFF,  // 白色
        0x0000   // 黑色
    };
    
    for (int i = 0; i < 5; i++) {
        fill_screen(colors[i]);
        delay(1000);
    }
}

// 2. 打印实际传输的字节
void debug_rgb565_bytes(uint16_t pixel) {
    uint8_t *bytes = (uint8_t *)&pixel;
    printf("Pixel 0x%04X: byte[0]=0x%02X, byte[1]=0x%02X\n", 
           pixel, bytes[0], bytes[1]);
}
```

**总结**: RGB565字节交换是为了解决CPU和LCD控制器之间的字节序不匹配问题，确保颜色正确显示。是否需要交换取决于具体的硬件组合。

### 2.2 转换路径B：YUV422 → 单色二值图像（黑白显示）

#### 2.2.1 适用条件
```c
if (sg_display_info.fmt == TUYA_PIXEL_FMT_MONOCHROME)
```

#### 2.2.2 二值化方法

系统支持三种二值化方法，由 `BINARY_CONFIG_T` 配置：

##### 方法1: 固定阈值法 (BINARY_METHOD_FIXED)
```c
yuv422_to_binary(yuv422_data, width, height, binary_data, threshold);
```

**处理流程**:
1. 从YUV422数据中提取Y（亮度）分量
   ```c
   // UYVY格式：Y在奇数位置 (1, 3, 5, 7...)
   int yuv_index = row_offset + x * 2 + 1;
   uint8_t luminance = yuv422_data[yuv_index];
   ```

2. 应用阈值判断
   ```c
   if (luminance >= threshold) {
       // 白色像素：清除位（bit = 0）
       binary_data[binary_index] &= ~(1 << bit_position);
   } else {
       // 黑色像素：保持位（bit = 1）
   }
   ```

3. **位打包**:
   - 每字节存储8个像素
   - 每行字节数: `(width + 7) / 8`
   - bit=0 表示白色，bit=1 表示黑色

**优点**: 速度快，可手动调节
**缺点**: 需要针对场景调整阈值

##### 方法2: 自适应阈值法 (BINARY_METHOD_ADAPTIVE)
```c
yuv422_to_binary_adaptive(yuv422_data, width, height, binary_data);
```

**处理流程**:
1. 计算图像平均亮度
   ```c
   uint32_t luminance_sum = 0;
   for (所有像素) {
       luminance_sum += Y分量;
   }
   threshold = luminance_sum / total_pixels;
   ```

2. 使用平均亮度作为阈值进行二值化

**优点**: 自动适应光照变化
**缺点**: 对比度较低的图像效果可能不理想

##### 方法3: Otsu最佳阈值法 (BINARY_METHOD_OTSU)
```c
yuv422_to_binary_otsu(yuv422_data, width, height, binary_data);
```

**处理流程**:
1. 构建亮度直方图（0-255）
   ```c
   int histogram[256] = {0};
   for (所有像素) {
       histogram[luminance]++;
   }
   ```

2. 计算类间方差最大的阈值
   ```c
   for (t = 0; t < 256; t++) {
       计算前景和背景的权重
       计算前景和背景的平均亮度
       variance = weight_bg * weight_fg * (mean_bg - mean_fg)²
       if (variance > max_variance) {
           optimal_threshold = t;
       }
   }
   ```

3. 使用最佳阈值进行二值化

**优点**: 自动找到最佳分割点，效果最好
**缺点**: 计算量较大

#### 2.2.3 二值图像格式说明

**存储格式**:
```
每字节存储8个像素（位打包）
+--------+--------+--------+--------+
| Byte 0 | Byte 1 | Byte 2 | Byte 3 |  第一行
+--------+--------+--------+--------+
| Byte N | Byte N+1 ...              |  第二行
```

**位映射**:
```
Byte = b7 b6 b5 b4 b3 b2 b1 b0
       ↓  ↓  ↓  ↓  ↓  ↓  ↓  ↓
像素   P7 P6 P5 P4 P3 P2 P1 P0
```

**像素值**:
- bit = 0: 白色像素（亮度 >= 阈值）
- bit = 1: 黑色像素（亮度 < 阈值）

**数据量计算**:
```c
binary_buffer_size = ((width + 7) / 8) * height;  // 字节
```

**示例**: 宽度=160, 高度=120
```
每行字节数 = (160 + 7) / 8 = 20 字节
总缓冲区大小 = 20 × 120 = 2400 字节
```

---

## 3. 显示缓冲区管理

### 3.1 帧缓冲区结构
```c
typedef struct {
    uint8_t *frame;              // 缓冲区数据指针
    uint32_t len;                // 缓冲区长度（字节）
    uint16_t width;              // 图像宽度
    uint16_t height;             // 图像高度
    TUYA_PIXEL_FMT_E fmt;        // 像素格式
} TDL_DISP_FRAME_BUFF_T;
```

### 3.2 双缓冲机制

系统使用双缓冲避免画面撕裂：

```c
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb = NULL;     // 当前工作缓冲
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_1 = NULL;   // 缓冲区1
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_2 = NULL;   // 缓冲区2
```

**切换逻辑**:
```c
// 刷新当前缓冲区到屏幕
tdl_disp_dev_flush(sg_tdl_disp_hdl, target_fb);

// 切换到另一个缓冲区进行下一帧处理
sg_p_display_fb = (sg_p_display_fb == sg_p_display_fb_1) 
                  ? sg_p_display_fb_2 
                  : sg_p_display_fb_1;
```

### 3.3 缓冲区大小计算

#### RGB565格式:
```c
frame_len = width * height * 2;  // 每像素2字节
```

#### 单色二值格式:
```c
frame_len = (width + 7) / 8 * height;  // 位打包
```

#### 示例计算:
| 分辨率 | RGB565 | 单色二值 |
|--------|--------|----------|
| 160×120 | 38,400字节 | 2,400字节 |
| 320×240 | 153,600字节 | 9,600字节 |
| 640×480 | 614,400字节 | 38,400字节 |

---

## 4. 图像旋转处理

### 4.1 旋转条件
```c
if (sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
    tdl_disp_draw_rotate(rotation, src_fb, dst_fb, is_swap);
    target_fb = sg_p_display_fb_rotat;
}
```

### 4.2 支持的旋转角度
- `TUYA_DISPLAY_ROTATION_0`: 不旋转
- `TUYA_DISPLAY_ROTATION_90`: 90度
- `TUYA_DISPLAY_ROTATION_180`: 180度
- `TUYA_DISPLAY_ROTATION_270`: 270度

### 4.3 旋转缓冲区
需要额外的旋转缓冲区：
```c
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_rotat = NULL;
```

### 4.4 坐标变换

| 旋转角度 | 坐标变换 | 尺寸变换 |
|----------|----------|----------|
| 0° | (x, y) | W×H |
| 90° | (y, W-1-x) | H×W |
| 180° | (W-1-x, H-1-y) | W×H |
| 270° | (H-1-y, x) | H×W |

---

## 5. 屏幕刷新阶段

### 5.1 显示设备信息
```c
TDL_DISP_DEV_INFO_T sg_display_info = {
    .fmt = TUYA_PIXEL_FMT_*,           // 像素格式
    .rotation = TUYA_DISPLAY_ROTATION_*, // 旋转角度
    .is_swap = true/false,              // 是否需要字节交换
};
```

### 5.2 刷新流程
```c
// 1. 选择目标缓冲区（旋转后或原始）
TDL_DISP_FRAME_BUFF_T *target_fb = ...;

// 2. 刷新到屏幕
tdl_disp_dev_flush(sg_tdl_disp_hdl, target_fb);
```

---


## 6. 完整数据流示例

### 6.1 RGB565彩色显示流程

```
1. 相机采集
   输出: YUV422, 320×240, UYVY格式
   数据量: 320×240×2 = 153,600字节

2. DMA2D硬件转换（如支持）
   输入: YUV422 (153,600字节)
   输出: RGB565 (153,600字节)
   转换时间:

3. 字节交换（如需要）
   交换高低字节

4. 旋转处理（如需要）
   90°旋转: 320×240 → 240×320

5. 刷新显示
   传输到LCD:
```

### 6.2 单色二值显示流程

```
1. 相机采集
   输出: YUV422, 160×120, UYVY格式
   数据量: 160×120×2 = 38,400字节

2. 提取亮度并计算阈值
   方法: Otsu算法
   输入: 38,400字节 YUV422
   计算: 构建直方图 + 寻找最佳阈值
   时间:

3. 二值化转换
   输入: 38,400字节 YUV422
   输出: 2,400字节 二值图像
   压缩比: 16:1
   时间: 

4. 位打包存储
   格式: 每字节8像素
   布局: 行优先，位序LSB→MSB

5. 旋转处理（如需要）
   按位操作旋转

6. 刷新显示
   传输到LCD:
```

---

## 7. 性能优化要点

### 7.1 DMA2D硬件加速
- **优势**: YUV→RGB转换速度提升10-20倍
- **条件**: 需要硬件支持 `ENABLE_DMA2D`
- **机制**: 异步转换 + 信号量同步

### 7.2 双缓冲机制
- **目的**: 避免画面撕裂
- **代价**: 双倍内存占用
- **收益**: 流畅显示

### 7.3 内存分配策略
```c
// 使用PSRAM存储帧缓冲（外部RAM）
sg_p_display_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
```

### 7.4 二值化优化
- **固定阈值**: 最快
- **自适应**: 中等
- **Otsu算法**: 最慢但最优

---

## 8. 调试信息输出

### 8.1 关键调试点
```c
// 阈值变化
PR_NOTICE("Threshold UP: %d", threshold);

// 方法切换
PR_NOTICE("Switched to OTSU method");

// Otsu阈值
PR_DEBUG("Otsu threshold calculated: %d", threshold);

// 当前配置
PR_DEBUG("Current method: %s", method_name);
```

### 8.2 错误处理
```c
// 参数校验
if (!yuv422_data || !binary_data || width <= 0 || height <= 0) {
    return -1;
}

// 设备检查
if (NULL == sg_tdl_camera_hdl) {
    PR_ERR("camera dev %s not found", CAMERA_NAME);
    return OPRT_NOT_FOUND;
}
```

---

## 9. 总结

### 9.1 数据格式对比

| 阶段 | 格式 | 每像素字节数 | 160×120示例 |
|------|------|--------------|-------------|
| 相机输出 | YUV422 | 2 | 38,400字节 |
| RGB565显示 | RGB565 | 2 | 38,400字节 |
| 单色显示 | 1-bit | 0.125 | 2,400字节 |

### 9.2 关键技术点

1. **YUV422编码**: UYVY格式，Y在奇数位置
2. **RGB565格式**: 颜色空间5-6-5位分布
3. **二值化算法**: 三种方法适应不同场景
4. **位打包**: 单色图像8像素/字节
5. **双缓冲**: 流畅显示的关键
6. **硬件加速**: DMA2D大幅提升性能
7. **图像旋转**: 支持4种角度
8. **实时控制**: 摇杆交互调节参数

### 9.3 适用场景

- **RGB565路径**: 彩色相机预览、视频监控
- **二值化路径**: 
  - 文档扫描
  - 二维码识别
  - 低功耗显示
  - 墨水屏应用

---

## 附录A: 数据结构参考

### YUV422 像素示例
```
像素数据: [U0][Y0][V0][Y1] [U1][Y2][V1][Y3]
          ---- 像素0-1 ----  ---- 像素2-3 ----
Y0,Y1,Y2,Y3: 各像素的亮度
U0,V0: 像素0和1共享的色度
U1,V1: 像素2和3共享的色度
```

### RGB565 像素示例
```
16位值: 0xF800
二进制: 1111 1000 0000 0000
        RRRRR GGGGGG BBBBB
分量:   R=31  G=0    B=0   (纯红色)
```

### 二值图像像素示例
```
字节值: 0b10110001
位序:   b7 b6 b5 b4 b3 b2 b1 b0
像素:   黑 白 黑 黑 白 白 白 黑
索引:   P7 P6 P5 P4 P3 P2 P1 P0
```
