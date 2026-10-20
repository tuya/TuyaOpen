# L511G_Y7PM 命名与配置

标准映射如下：

- SDK 平台仓库分支：`l511g-y7pm`
- TuyaOpen 平台名：`L511G_Y7PM`
- 顶层平台选择符：`BOARD_CHOICE_L511G_Y7PM`
- 板型选择符：`BOARD_CHOICE_L511G_Y7PM_BOARD`
- 派生芯片与板型身份：`L511G_Y7PM`

板型选择符使用 `_BOARD` 后缀，以区别顶层平台选择符。派生身份仍为
`L511G_Y7PM`，对应 SDK 校验使用的板名及 BSP 目录
`boards/L511G_Y7PM/L511G_Y7PM/`。

现有工程可用以下命令切换到该预设：

```sh
tos.py config choice -d -c L511G_Y7PM
```

此命令会替换当前应用配置；如有应用自定义配置项，请切换后合并回去。需要直接设置选择项时，可用
`tos.py config set` 同时设置顶层和板型选择符；不要手写派生身份符号。
