# 火眼应用

当前应用版本 `2.0.0`，面向 2026-10 新载板的24V低压监测原型。
旧ACS712演示板只作为兼容后端保留。当前范围见
[`docs/current/scope-and-roadmap.md`](../../docs/current/scope-and-roadmap.md)。

## 当前数据流

```text
INA228 / NTC
    ↓
滑动平均 + 一阶低通
    ↓
阈值 + 正斜率趋势 + 连续防抖
    ↓
NORMAL → WARNING → ALARM
    ↓
OLED / 蜂鸣器 / 报警灯 / 负载使能 / HTTP 上报
```

## 传感器与执行器

| 功能 | 当前方案 | 接口 |
|---|---|---|
| 电流 | INA228 + 2mΩ 分流 | I2C1，`0x40` |
| 母线电压 | INA228 VBUS | I2C1 |
| 温度 | NTC 10K B3950 | PA6，ADC0_IN6 |
| 负载 | 常开继电器 | PB0，高电平接通 |
| 蜂鸣器 | 板载有源蜂鸣器 | PB1，高电平导通 |
| 报警灯 | LED 模块 | PD9，高电平触发 |
| OLED | SSD1306 | I2C1，PB10/PB11 |
| 网络 | W5500 | SPI1 |

当前 GD32 诊断版中，INA228 和 OLED 的应用访问走 PB10/PB11 软件 I2C。
原因是 NuttX GD32 硬件 I2C 的多字节接收状态机会读错数据；硬件 I2C
设备仍注册为 `/dev/i2c1`，仅供 `i2c` 工具调试。

INA228/OLED 初始化前重复执行硬件 I2C reset 会干扰实时监测启动，该重复
reset 已移除。硬件 I2C 接收状态机本身仍未修复。

## 负载安全语义

应用层只表达“接通负载/断开负载”：

- NORMAL：接通负载。
- WARNING：接通负载，声光预警。
- ALARM/SHUTDOWN：断开负载，声光报警。
- 上电和复位：板级默认释放继电器，负载断开。

板级接口 `gd32_fireeye_set_relay(true)` 表示线圈励磁，不代表“断电”。
旧 NC 板兼容代码在传感器模块内部转换，业务层不得直接使用该接口。

新载板的目标接法是5脚COM→2脚NO→负载，1/4脚为线圈。当前新板已用一根
绝缘线短接3/5脚，把 `24V_SW` 引入真正的COM5脚，基础带载实测通过。

EasyEDA实时工程确认24V主功率路径符合60mil设计；连接INA228的10mil
`24V_SW`仅用于VBUS采样。F1为2A保险丝、D1为SS54，禁止5A/8A实流测试。

`CONFIG_FIREYEYE_RELAY_WIRED_NC` 不能修复该错误。NC 接法在掉电时闭合负载，
不具备失效安全，不能作为最终配置。

## 配置

Kconfig 新增电流后端选择：

```text
CONFIG_FIREYEYE_CURRENT_SENSOR_INA228=y
# CONFIG_FIREYEYE_CURRENT_SENSOR_ACS712 is not set
```

采样率、电流/温度阈值和连续防抖次数由 Kconfig 配置，并真正传入应用。

## 自检命令

```text
nsh> fireeye current
nsh> fireeye temp
nsh> fireeye oled
nsh> fireeye test
nsh> fireeye level
nsh> fireeye -v
```

- `current`：打印 INA228 ID、原始码、电流和母线电压。
- `temp`：打印 NTC 的 PA6 原始码、电压和换算温度。
- `oled`：初始化 OLED 并执行全屏与文字显示自检。
- `test`：声光与继电器联合自检。
- `level`：逐脚输出 LOW/HIGH，用于极性排查。
- `-v`：打开周期数据日志。

`fireeye current`、`temp`、`oled` 和 `test` 执行完会返回 NSH。无参数 `fireeye`
和 `fireeye -v` 会进入无限监测循环；当前构建未启用 `CONFIG_TTY_SIGINT`，Ctrl+C
也不能退出，需要复位开发板。`fireeye current` 初始化时会释放继电器，不能用于
标准 NO 接法下的带载电流测试；带载观察应运行 `fireeye -v`。

## OLED 页面

实时页面以128x64整帧缓冲一次刷新：

- 顶栏：`FIRE EYE` 与 `NORMAL/WARNING/ALARM` 状态；
- 第二行：`LOAD ON/OFF`；
- 主区域：双倍字高显示三位小数电流；
- 下方分区显示母线电压、NTC 温度和默认跳闸阈值；
- 启动时显示 `SAFETY MONITOR / STARTING` 页面；
- `fireeye oled` 显示全屏点亮、清屏和 `OLED OK` 测试页。

## 当前限制

- GD32 上 INA228 ID/配置回读和 OLED 自检已通过。
- NTC 常温、超温告警、降温恢复以及启动阶段假趋势修复已上板验证。
- 当前网络构建启用 W5500、静态 IPv4 和 HTTP 上报。
- W5500 直连电脑时 Ping 和 HTTP JSON 上报初测通过；长时间稳定性待验收。
- NuttX GD32 硬件 I2C 多字节接收缺陷尚未在公共驱动中正式修复。
- 温度精度尚未用标准温度计标定；NTC 曾读到 `4095` 并触发 ALARM，当前已恢复
  约 24℃，仍需观察接插件和长期稳定性。
- 蜂鸣器和报警灯已通过`fireeye test`；Q1、线圈1/4脚和基础带载通路正常。
- INA228基础带载实测约0.08A；量程、方向和告警/掉电断载仍未验收。
- 固件默认5A/8A阈值与当前2A保险丝不匹配，不能在当前板上作为实流验收值。
- 板端 Flash 存证在当前演示构建中未启用。
- 漏电、断网补传、RTC/NTP、TinyML 和看门狗未实现。

完整当前状态见 `docs/current/`。
