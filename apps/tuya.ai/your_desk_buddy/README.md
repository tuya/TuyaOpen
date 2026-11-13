# Your Desk Buddy

`your_desk_buddy` 是 GD32H779I_DESK_BUDDY 的本地外设自测应用。当前不连接云端 AI，
首页只负责选择外设，点击任一设备会进入独立的测试详情页，再通过“开始”运行测试。

测试任务在独立线程中执行，触摸回调只负责投递请求，避免相机采集、SD 挂载等阻塞操作卡住
LVGL 界面。前后摄像头共享 DVP 总线，因此测试请求通过单线程队列串行执行。

Desk Buddy 从 AXISRAM 的预留空隙中划出约 304 KiB 作为板级摄像头帧内存池，并通过
`tdl_camera_set_frame_mem_ops()` 接入 TDL。前后两路摄像头共用两个 320×240 YUV422
帧缓冲：优先按独立帧依次分配，独立区不足时自动回退到两个共享帧循环分配。因此同一时刻
只能打开一路摄像头；自测队列已经保证这一约束。帧内存不再占用通用堆。

LED 目前由 `ENABLE_LED` 条件保护，默认关闭。使能后还需要板级注册可用的 LED TDD 设备。

## 测试与关闭

- 前后摄像头：采集通过后自动调用 `tdl_camera_dev_close()` 关闭。
- SD 卡：读写校验完成后删除测试文件并调用 `tkl_fs_unmount()` 卸载。
- 氛围灯：两路灯依次点亮，测试结束后切灭并调用 `tdl_led_close()` 关闭。
- 加速度计：读取成功后自动调用 `tdl_motion_close()` 关闭。
- 编码器：测试通过后保持打开，以便详情页持续显示位置和按键状态；点击“停止并关闭”或返回首页时关闭。

详情页的“停止并关闭”会取消正在运行的测试，并等待对应 worker 完成资源清理。无论测试是通过、失败还是被停止，返回首页前都会关闭本次测试打开的外设。

## 编译

```bash
cd apps/tuya.ai/your_desk_buddy
tos.py config choice -c GD32H779I_DESK_BUDDY.config
tos.py build
tos.py flash
```
