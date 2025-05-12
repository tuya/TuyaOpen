# I2S 驱动

## 简要说明

I2S 全称 Inter-IC Sound, Integrated Interchip Sound，或简写 IIS，是飞利浦在 1986 年定义（1996 年修订）的数字音频传输标准，用于数字音频数据在系统内部器件之间传输，例如编解码器 CODEC、DSP、数字输入/输出接口、ADC、DAC 和数字滤波器等。

I2S 是比较简单的数字接口协议，没有地址或设备选择机制。在 I2S 总线上，只能同时存在一个主设备和发送设备。主设备可以是发送设备，也可以是接收设备，或是协调发送设备和接收设备的其它控制设备。

在 I2S 系统中，提供时钟（SCK 和 WS）的设备为主设备，下面为常见的 I2S 系统框图。

![image-20220411195655196](https://images.tuyacn.com/fe-static/docs/img/fa0ba41c-d0ba-4e01-976b-23e505f61b41.png)

![image-20220411200756538](https://images.tuyacn.com/fe-static/docs/img/a7a1f523-49bc-43bd-86b3-c228101867ea.png)

![image-20220411200931639](https://images.tuyacn.com/fe-static/docs/img/900ca665-a6bb-4cc5-9ce1-f07536e00596.png)

其中：

`SCK`: 对应数字音频的每一位数据，SCK 都有 1 个脉冲。SCK 的频率= 2 × 采样频率 × 采样位数。

`WS`: 用于切换左右声道的数据。WS的频率 ＝ 采样频率，WS为 “1” 表示正在传输的是左声道的数据，WS 为 “0” 表示正在传输的是右声道的数据。

`SD`：串行数据，用二进制补码表示的音频数据。 

## API 说明

### tkl_i2s_init

```c
OPERATE_RET tkl_i2s_init(TUYA_I2S_NUM_E i2s_num, const TUYA_I2S_BASE_CFG_T *i2s_config);
```

- 功能描述:

  - 通过设备号和基础配置初始化对应的 I2S 实例，返回初始化结果。

- 参数:

  - `i2s_num`: 端口号。

  - `i2s_config`: 基础配置

    ```c
    typedef struct {
        TUYA_I2S_MODE_E             mode;                   /*!< I2S work mode */
        uint32_t                    sample_rate;            /*!< I2S sample rate */
        uint32_t                    mclk;                   /*!< I2S mclk */
        TUYA_I2S_BITS_PER_SAMP_E    bits_per_sample;        /*!< I2S sample bits in one channel */
        TUYA_I2S_CHANNEL_FMT_E      channel_format;         /*!< I2S channel format.*/
        TUYA_I2S_COMM_FORMAT_E      communication_format;   /*!< I2S communication format */
        uint32_t                    i2s_dma_flags;          /*!< I2S dma format , 1 use dma */  
    } TUYA_I2S_BASE_CFG_T;
    ```

    #### TUYA_I2S_MODE_E：

| 名字                 | 定义     | 备注 |
|:---------------- ----|:---------------------|:------|
| TUYA_I2S_MODE_MASTER | 主机模式 |      |
| TUYA_I2S_MODE_SLAVE  | 从机模式 |      |
| TUYA_I2S_MODE_TX     | 发送模式 |      |
| TUYA_I2S_MODE_RX     | 接收模式 |      |

    用户可使用四种模式组合，分别是:
    - TUYA_I2S_MODE_MASTER|TUYA_I2S_MODE_TX
    - TUYA_I2S_MODE_MASTER|TUYA_I2S_MODE_RX
    - TUYA_I2S_MODE_SLAVE|TUYA_I2S_MODE_TX
    - TUYA_I2S_MODE_SLAVE|TUYA_I2S_MODE_RX

    `sample_rate`: 采样频率。

    `mclk`: 主时钟，一般是采样频率的 256 或者 384 倍。

    #### TUYA_I2S_BITS_PER_SAMP_E：

| 名字                           | 定义       | 备注 |
|:-------------------------------|:---------------------|:------|
| TUYA_I2S_BITS_PER_SAMPLE_8BIT  | 数据位宽8  |      |
| TUYA_I2S_BITS_PER_SAMPLE_16BIT | 数据位宽16 |      |
| TUYA_I2S_BITS_PER_SAMPLE_24BIT | 数据位宽24 |      |
| TUYA_I2S_BITS_PER_SAMPLE_32BIT | 数据位宽32 |      |

#### TUYA_I2S_CHANNEL_FMT_E：

| 名字                            | 定义                     | 备注       |
|:--------------------------------|:---------------------|:------|
| TUYA_I2S_CHANNEL_FMT_RIGHT_LEFT | 左右通道分开             |            |
| TUYA_I2S_CHANNEL_FMT_ALL_RIGHT  | 加载右声道数据到两个通道 |            |
| TUYA_I2S_CHANNEL_FMT_ALL_LEFT   | 加载左声道数据到两个通道 |            |
| TUYA_I2S_CHANNEL_FMT_ONLY_RIGHT | 仅加载右声道数据         | 单声道模式 |
| TUYA_I2S_CHANNEL_FMT_ONLY_LEFT  | 仅加载左声道数据         | 单声道模式 |

#### TUYA_I2S_COMM_FORMAT_E:

| 名字                            | 定义                                                         | 备注 |
|:--------------------------------|:---------------------|:------|
| I2S_COMM_FORMAT_STAND_I2S       | Philips标准，数据在第二个BCK传输                             |      |
| I2S_COMM_FORMAT_STAND_MSB       | MSB(左对齐)标准，数据在第一个BCK传输                         |      |
| I2S_COMM_FORMAT_STAND_PCM_SHORT | PCM短标准，也称为DSP模式。 同步信号(WS)的周期为1个BCK周期 。 |      |
| I2S_COMM_FORMAT_STAND_PCM_LONG  | PCM长标准，同步信号(WS)的周期为channel_bit个BCK周期          |      |

- 返回值:
  - NULL: 初始化失败。
  - 其它: 实例句柄。

### tkl_i2s_send

```c
OPERATE_RET tkl_i2s_send(TUYA_I2S_NUM_E i2s_num, void *buff, uint32_t len);
```

- 功能描述:

  - I2S 传送数据到发送器。

- 参数:

  - `i2s_num`: 端口号。

  - `buff`: 指向将要发送的数据。

  - `len`: 发送数据的长度。


- 返回值:
  - OPRT_OK 成功，其他请参考文件tuya_error_code.h，OS_ADAPTER_I2S定义部分。

### tkl_i2s_send_stop

```c
OPERATE_RET tkl_i2s_send_stop(TUYA_I2S_NUM_E i2s_num);
```

- 功能描述:

  - I2S 停止传送数据到发送器。
- 参数:

  - `i2s_num`: 端口号。

- 返回值:
  - OPRT_OK 成功，其他请参考文件tuya_error_code.h，OS_ADAPTER_I2S定义部分。

### tkl_i2s_recv

```c
int tkl_i2s_recv(TUYA_I2S_NUM_E i2s_num, void *buff, uint32_t len);
```

- 功能描述:

  - I2S 异步接收数据。

- 参数:

  - `i2s_num`: 端口号。

  - `buff`: 指向将要接收数据的缓存。

  - `len`: 接收数据的长度。

- 返回值:
  - OPRT_OK 成功，其他请参考文件tuya_error_code.h，OS_ADAPTER_I2S定义部分。

### tkl_i2s_recv_stop

```c
OPERATE_RET tkl_i2s_recv_stop(TUYA_I2S_NUM_E i2s_num);
```

- 功能描述:

  - I2S 停止数据的接收。
- 参数:

  - `i2s_num`: 端口号。

- 返回值:
  - OPRT_OK 成功，其他请参考文件tuya_error_code.h，OS_ADAPTER_I2S定义部分。

### tkl_i2s_deinit

```c
OPERATE_RET tkl_i2s_deinit(TUYA_I2S_NUM_E i2s_num);
```

- 功能描述:

  - I2S 去初始化。
- 参数:

  - `i2s_num`: 端口号。

- 返回值:
  - OPRT_OK 成功，其他请参考文件tuya_error_code.h，OS_ADAPTER_I2S定义部分。