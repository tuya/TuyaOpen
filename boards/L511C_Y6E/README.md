# L511C_Y6E 命名与配置

标准映射如下：

- SDK 平台仓库分支：`l511c-y6e`
- TuyaOpen 平台名：`L511C_Y6E`
- 顶层平台选择符：`BOARD_CHOICE_L511C_Y6E`
- 板型选择符：`BOARD_CHOICE_L511C_Y6E_BOARD`
- 派生板身份：`BOARD_CHOICE="L511C_Y6E"`

板型选择符带 `_BOARD` 后缀，用来和顶层平台选择符区分。派生板身份仍为 `L511C_Y6E`，对应 SDK 校验使用的板名及 BSP 目录 `boards/L511C_Y6E/L511C_Y6E/`。

现有工程可用以下命令切换到该预设：

```sh
tos.py config choice -d -c L511C_Y6E
```

此命令会替换当前应用配置；如有应用自定义配置项，请切换后合并回去。需要直接设置选择项时，可用 `tos.py config set` 同时设置顶层和板型选择符；不要手写派生身份符号。
