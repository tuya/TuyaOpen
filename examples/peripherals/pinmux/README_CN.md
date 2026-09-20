# TKL Pinmux

这是通用 TKL pinmux 示例，不绑定任何芯片、开发板、GPIO 编号或外设地址。引脚和 I2C 控制器由 Kconfig 配置，因此可用于所有实现 `tkl_pinmux` 的平台。

示例只调用 pinmux 接口，不初始化 I2C，也不会访问板载设备或驱动 GPIO。请先根据当前开发板原理图选择空闲且有效的 SCL/SDA 引脚；pinmux 必须在对应外设初始化前调用。

```bash
cd examples/peripherals/pinmux
tos.py config set EXAMPLE_PINMUX_I2C_PORT=0 EXAMPLE_PINMUX_I2C_SCL_PIN=<SCL_GPIO> EXAMPLE_PINMUX_I2C_SDA_PIN=<SDA_GPIO>
tos.py build
```

先按项目的常规流程选择目标平台和开发板；也可以通过 `tos.py config menu` 在 `Pinmux example` 菜单中设置相同参数。不同平台对外设和引脚的支持范围不同，日志含义如下：

```text
supported                         # 当前平台接受该路由
not supported by this platform    # 当前平台没有该外设或不支持该路由
```
