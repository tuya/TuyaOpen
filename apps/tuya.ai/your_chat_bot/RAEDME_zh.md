[English](./README.md) | 简体中文

# your_chat_bot
 [your_chat_bot](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_chat_bot) 是基于 tuya.ai 开源的大模型智能聊天机器人。通过麦克风采集语音，语音识别，实现对话、互动、调侃，还能通过屏幕看到实时聊天内容。


## 支持功能

1. 智能对话
2. 按键唤醒, 回合制对话
3. 支持 LCD 显示实时聊天内容、支持 APP 端实时查看聊天内容
4. 蓝牙配网快捷连接路由器
5. APP 端实时切换 AI 智能体角色

![](../../../docs/images/apps/your_chat_bot.png)

## 依赖硬件能力
1. 音频采集
2. 音频播放

## 已支持硬件
|  型号  | 说明 | 重置方式 |
| --- | --- | --- | 
| TUYA T5AI_Board 开发板 | [https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj](https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | 重启(按 RST 按钮) 3 次重置 |
| TUYA T5AI_EVB 开发板 | [https://oshwhub.com/flyingcys/t5ai_evb](https://oshwhub.com/flyingcys/t5ai_evb) | 重启(按 RST 按钮) 3 次重置 |


## 编译
1. 运行 `tos config_choice` 命令， 选择当前运行的开发板。
2. 如需修改配置，请先运行 `tso menuconfig` 命令修改配置。
3. 运行 `tso build` 命令，编译工程。
  