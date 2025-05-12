# DAC 驱动

## 简要说明

DAC（Digital to analog converter）即数字模拟转换器，它可以将数字信号转换为模拟信号。它的功能与 ADC 相反。

在常见的数字信号系统中，大部分传感器信号被转化成电压信号，而 ADC 把电压模拟信号转换成易于计算机存储、处理的数字编码，由计算机处理完成后，再由 DAC 输出电压模拟信号，该电压模拟信号常常用来驱动某些执行器件，使人类易于感知。如音频信号的采集及还原就是这样一个过程。

## API 描述

### tkl_dac_init

```c
OPERATE_RET tkl_dac_init(TUYA_DAC_NUM_E port_num);
```

- 功能描述:
  - 通过端口号初始化对应的 dac 实例，返回初始化结果。
  
- 参数:
  - port_num: 端口号
  
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_dac_deinit

```c
OPERATE_RET tkl_dac_deinit(TUYA_DAC_NUM_E port_num);
```

- 功能描述:
  - dac 实例反初始化。该接口会停止 dac。
- 参数:
  - port_num: 端口号。
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_dac_controller_config

```c
OPERATE_RET tkl_dac_controller_config(TUYA_DAC_NUM_E port_num, TUYA_DAC_CMD_E cmd, void * argu);
```

- 功能描述:

  - 配置相应的dac设备。

- 参数:

  - port_num: 端口号。

  - cmd: 命令字

    **TUYA_DAC_CMD_E**：

    | 名字                  | 含义              |
    | --------------------- | ----------------- |
    | TUYA_DAC_WRITE_FIFO   | 设置 DAC 的 fifo 数据 |
    | TUYA_DAC_SET_BASE_CFG | 设置 DAC 的基本配置 |
    
  - argu：参数 可为下面的两个结构体

  ```c
  typedef struct{	
  	uint8_t *data;       //data
  	uint32_t len;        // data length
  }TUYA_DAC_DATA_T;
  ```

  ```c
  typedef struct {
      TUYA_AD_DA_CH_LIST_U  ch_list;       // dac channel list
      uint8_t  ch_nums;       // dac channel number
      uint8_t  width;         // output width
      uint32_t freq;          // convert freq
  } TUYA_DAC_BASE_CFG_T;
  ```

- 返回值:

  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_dac_base_cfg_get

```c
OPERATE_RET  tkl_dac_base_cfg_get (TUYA_DAC_NUM_E port_num,TUYA_DAC_BASE_CFG_T *cfg);
```

- 功能描述:
  
  - 获取相应 DAC 的基本配置
- 参数:
  
  - `port_num`: 端口号。
  
  - `cfg`：dac基本配置结构体
  
    ```c
    typedef struct {
        TUYA_AD_DA_CH_LIST_U  ch_list;       // dac channel list
        uint8_t  ch_nums;       // dac channel number
        uint8_t  width;         // output width
        uint32_t freq;          // convert freq
    } TUYA_DAC_BASE_CFG_T;
    ```
- 返回值:
  
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_dac_start

```c
OPERATE_RET tkl_dac_start(TUYA_DAC_NUM_E port_num);
```

- 功能描述:
  - 启动相应的DAC 开始转换
- 参数:
  - `port_num`: 端口号
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_dac_stop

```c
OPERATE_RET tkl_dac_stop(TUYA_DAC_NUM_E port_num);
```

- 功能描述:
  - 停止相应的DAC 
- 参数:
  - `port_num`: 端口号。
- 返回值:
  - 错误码，OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

### tkl_dac_fifo_reset

```c
OPERATE_RET  tkl_dac_fifo_reset (TUYA_DAC_NUM_E port_num);
```

- 功能描述:相应的 DAC 的 FIFO 重置
- 参数:
  - `port_num`: 端口号。
- 返回值:
  - 错误码，OPRT_OK 成功，其他请参考文件 `tuya_error_code.h` 定义部分。

## 示例

### 1.dac示例一

```c
/*初始化第0个dac外设*/
tkl_dac_init(0);

/*配置第0个DAC的基本配置*/
TUYA_DAC_BASE_CFG_T dac_base_cfg;

dac_base_cfg.freq = 8000;  /*DAC的转换速度为1秒8000次*/
dac_base_cfg.width = 16;   /*DAC的精度为16*/
dac_base_cfg.ch_nums = 1;  /*dac为单通道输出  端口号为2（chs_cfg = 2）*/
dac_base_cfg.ch_list.bits.ch_2 = 1;

tkl_dac_controller_config(0,TUYA_DAC_SET_BASE_CFG, &dac_base_cfg);

/*写DAC的FIFO*/
TUYA_DAC_DATA_T data_fifo;
uint8_t dac_data[1024];

data_fifo.len = sizeof(dac_data);
data_fifo.data = dac_data;
tkl_dac_controller_config(0,TUYA_DAC_WRITE_FIFO, &data_fifo);

/*启动第0个dac开始转换*/
tkl_dac_start(0);

/*持续的往DAC的FIFO 写数据 让DAC转换*/
while(1)
{
    data_fifo.len = sizeof(dac_data);
	data_fifo.data = dac_data;
	tkl_dac_controller_config(0,TUYA_DAC_WRITE_FIFO, &data_fifo);
}

/*停止第0个dac开始转换*/
tkl_dac_stop(0);

/*deinit 第0个DAC*/
tkl_dac_deinit(0);
```

