# tdl_ir_device

该组件依赖以下组件：

+ tdd_ir_driver

组件功能：

+ 红外设备抽象

+ NEC等红外协议解码

+ 红外多设备管理

该组件能够同时兼容照明红外和网关红外的需求，照明红外和网关红外的区别是，

在实际业务中照明红外为了能够响应及时需要在接收红外码的同时进行解码；网关红外只是将接收到的红外码进行编码后上传到云端，在一段红外码接收完成后再处理。所以在使用红外组件时，需要接收到红外码值后立马进行处理，在初始化之后还需要注册一个回调函数用于及时通知应用。

## 接口介绍

初始流程为：硬件注册->查找设备->打开设备->操作设备（红外发送，介绍）

### 1. 硬件注册

选择驱动方式，支持下列三种驱动方式：

+ 单硬件定时器驱动

+ 双硬件定时器驱动

+ PWM 捕获驱动

捕获方式驱动不一定所有硬件都支持，其中除单硬件定时器驱动方式外，其他两种驱动方式都支持自发自收，这里选择的定时器必须是微妙级(us)的定时器。发送引脚应支持 38KHz 的 PWM 输出。

硬件注册示例代码如下：

```c
char dev_name[] = {"IR_DEVICE"};

/* 8720cf IR hardware config */
IR_DRV_CFG_T ir_drv_cfg = {
    .send_pin = GPIO_NUM_2, //PWM0
    .recv_pin = GPIO_NUM_3,
    .send_timer = TIMER_NUM_1,
    .recv_timer = TIMER_NUM_0,
    .send_duty = 50
};

op_ret = tdd_ir_driver_register(dev_name, IR_DRV_DUAL_TIMER, ir_drv_cfg);
if (OPRT_OK != op_ret) {
    PR_ERR("ir register error, ret=%d", op_ret);
    goto __EXIT;
}
```

### 2. 设备查找

根据设备名称，获取设备操作句柄。示例代码如下：

```c
IR_HANDLE_T ir_handle;

op_ret = tdl_ir_dev_find(dev_name, &ir_handle);
if (OPRT_OK != op_ret) {
    PR_NOTICE("tdl_ir_dev_find error:%d", op_ret);
    goto __EXIT;
}
```

### 3. 打开设备

这里可以通过配置，通过不同的协议方式开启设备，目前支持下列两种方式：

+ 时间码类型
+ NEC 协议类型

配置红外设备模式，支持以下三种模式配置：

+ 仅发送
+ 仅接收
+ 发送接收同时支持

时间码类型（timecode），是将红外码值按照发送时的高低电平时间进行发送和接收的。示例代码如下：

```c
ir_cfg.ir_mode = IR_MODE_SEND_RECV;  // 红外设备模式
ir_cfg.recv_queue_num = 3;    // 最多接收缓存红外数据条数
ir_cfg.recv_buf_size = 1024; // 单条红外码值最大支持空间大小
ir_cfg.recv_timeout = 1000; // 接收超时时间

ir_cfg.prot_opt = IR_PROT_TIMECODE;  // 时间码协议类型

op_ret = tdl_ir_dev_open(ir_handle, &ir_cfg);
if (OPRT_OK != op_ret) {
    PR_ERR("open failed, %d", op_ret);
    goto __EXIT;
}
```

NEC 协议初始化示例代码：

```c
ir_cfg.ir_mode = IR_MODE_SEND_RECV;
ir_cfg.recv_queue_num = 3;
ir_cfg.recv_buf_size = 1024;
ir_cfg.recv_timeout = 1000;

ir_cfg.prot_opt = IR_PROT_NEC; // NEC 协议类型

ir_cfg.prot_cfg.nec_cfg.is_nec_msb = 0, // 最高有效位使能
ir_cfg.prot_cfg.nec_cfg.leader_err_percent = 70; // 引导码最大误差率
ir_cfg.prot_cfg.nec_cfg.logic0_err_percent = 45; // 逻辑 0 最大误差率
ir_cfg.prot_cfg.nec_cfg.logic1_err_percent = 70; // 逻辑 1 最大误差率
ir_cfg.prot_cfg.nec_cfg.repeat_err_percent = 50; // 重复码最大误差率

ir_cfg.prot_cfg.nec_cfg.is_nec_msb = 0,
ir_cfg.prot_cfg.nec_cfg.lead_err = 70; // 引导码最大误差率
ir_cfg.prot_cfg.nec_cfg.logics_err = 50; // 逻辑 0/1 高电平最大误差率
ir_cfg.prot_cfg.nec_cfg.logic0_err = 45; // 逻辑 0 低电平最大误差率
ir_cfg.prot_cfg.nec_cfg.logic1_err = 70; // 逻辑 1 低电平最大误差率
ir_cfg.prot_cfg.nec_cfg.repeat_err = 50; // 重复码最大误差率

op_ret = tdl_ir_dev_open(ir_handle, &ir_cfg);
if (OPRT_OK != op_ret) {
    PR_ERR("open failed, %d", op_ret);
    goto __EXIT;
}
```

### 4. 红外发送

**这里需要注意的是，红外发送采用的是阻塞的方式**，因为发送时使用数据是发送传入的数据的空间，所以必须等红外码值发送完成后才会执行后面的程序。

这里的发送是和 `tdl_ir_dev_open` 时设备的协议有关的， `ir_cfg.prot_opt = IR_PROT_TIMECODE` 时红外发送示例如下：

```c
uint32_t data[] = {560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 1690, 1690, 1690};
ir_send_data.timecode.data = data;
ir_send_data.timecode.len = strlen(data);

op_ret = tdl_ir_dev_send(ir_handle, 38000, ir_send_data, 1);
if (OPRT_OK != op_ret) {
    PR_ERR("ir send error, %d", op_ret);
}
PR_DEBUG("ir send start");
```

 `ir_cfg.prot_opt = IR_PROT_NEC` 时红外发送示例如下：

```c
ir_send_data.nec_data.addr = 0x807F;
ir_send_data.nec_data.cmd = 0x1DE2;
ir_send_data.nec_data.repeat_cnt = 1;

op_ret = tdl_ir_dev_send(ir_handle, 38000, ir_send_data, 1);
if (OPRT_OK != op_ret) {
    PR_ERR("ir send error, %d", op_ret);
}
PR_DEBUG("ir send start");
```

### 5. 红外接收

红外接收默认也是阻塞的方式，但也支持通过回调的方式进行操作。如果选择的是 `IR_PROT_TIMECODE` 协议方式，那接收的数据将直接是解码后的数据。红外接收示例如下：

```c
op_ret = tdl_ir_dev_recv(ir_handle, &ir_recv_data, 3000);
if (OPRT_OK == op_ret) {
#if USE_NEC_DEMO
    PR_DEBUG("demo addr:%04x, cmd:%04x, cnt:%d", ir_recv_data->nec_data.addr, ir_recv_data->nec_data.cmd, ir_recv_data->nec_data.repeat_cnt);
#else
    PR_DEBUG("demo addr:%p, len:%d", ir_recv_data, ir_recv_data->timecode.len);
    for (i=0; i<ir_recv_data->timecode.len; i++) {
        PR_DEBUG_RAW("%d ", ir_recv_data->timecode.data[i]);
    }
    PR_DEBUG_RAW("\r\n");
#endif
    tdl_ir_dev_recv_release(ir_handle, ir_recv_data);
}
```

### 6. 接收回调注册

在 **硬件注册->查找设备->打开设备** 之后就可以注册接收回调后，红外接收到数据会立马通过注册的回调函数通知应用，无法再使用 `tdl_ir_dev_recv()` 获取数据。

在接收到有效数据后回调函数会传出两个参数 `is_frame_finish` 和 `recv_data`，`is_frame_finish` 为 1 表示这帧数据接收完成，为 0 表示这帧数据可能还没有结束还在接收中；`recv_data` 表示接收的数据。回调函数类型如下：

```c
typedef void (*IR_APP_RECV_CB)(uint8_t is_frame_finish, IR_DATA_U *recv_data);
```

这里的数据类型也是根据您初始化的时候选择的协议格式进行配置。比如：在初始化时您配置红外协议为 NEC 编码格式的话，那么红外接收到数据后会将接收到的红外码转换成 NEC 格式之后再通过回调函数将数据传出。

使用示例如下：

```c
void app_ir_recv_cb(uint8_t is_frame_finish, IR_DATA_U *recv_data)
{
    TAL_PR_NOTICE("is_finish: %d, addr: 0x%04x, cmd: 0x%04x, repeat: %d", \
                    is_frame_finish, recv_data->nec_data.addr, recv_data->nec_data.cmd, recv_data->nec_data.repeat_cnt);
}

tdl_ir_config(ir_handle, IR_CMD_RECV_CB_REGISTER, app_ir_recv_cb);
```

日志如下：

```log
[01-01 00:00:29 ty N][app_main.c:43] is_finish: 0, addr: 0x807f, cmd: 0x0cf3, repeat: 0
[01-01 00:00:29 ty N][app_main.c:43] is_finish: 0, addr: 0x807f, cmd: 0x0cf3, repeat: 1
[01-01 00:00:29 ty N][app_main.c:43] is_finish: 0, addr: 0x807f, cmd: 0x0cf3, repeat: 2
[01-01 00:00:29 ty N][app_main.c:43] is_finish: 0, addr: 0x807f, cmd: 0x0cf3, repeat: 3
[01-01 00:00:30 ty N][app_main.c:43] is_finish: 1, addr: 0x807f, cmd: 0x0cf3, repeat: 3
[01-01 00:00:31 ty N][app_main.c:43] is_finish: 0, addr: 0x807f, cmd: 0x0cf3, repeat: 0
[01-01 00:00:31 ty N][app_main.c:43] is_finish: 0, addr: 0x807f, cmd: 0x0cf3, repeat: 1
[01-01 00:00:31 ty N][app_main.c:43] is_finish: 1, addr: 0x807f, cmd: 0x0cf3, repeat: 1
[01-01 00:00:33 ty N][app_main.c:43] is_finish: 0, addr: 0x807f, cmd: 0x0cf3, repeat: 0
[01-01 00:00:33 ty N][app_main.c:43] is_finish: 1, addr: 0x807f, cmd: 0x0cf3, repeat: 0
```

在应用上可以通过 `repeat_cnt` 去判断红外按键是按下，长按或释放等动作。
