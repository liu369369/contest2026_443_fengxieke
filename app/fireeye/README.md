# 火眼应用

当前应用版本 `2.0.0`，面向 2026-10 新载板。旧 ACS712 演示板只作为兼容后端保留。

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

## 负载安全语义

应用层只表达“接通负载/断开负载”：

- NORMAL：接通负载。
- WARNING：接通负载，声光预警。
- ALARM/SHUTDOWN：断开负载，声光报警。
- 上电和复位：板级默认释放继电器，负载断开。

板级接口 `gd32_fireeye_set_relay(true)` 表示线圈励磁，不代表“断电”。
旧 NC 板兼容代码在传感器模块内部转换，业务层不得直接使用该接口。

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
nsh> fireeye test
nsh> fireeye level
nsh> fireeye -v
```

- `current`：打印 INA228 ID、原始码、电流和母线电压。
- `test`：声光与继电器联合自检。
- `level`：逐脚输出 LOW/HIGH，用于极性排查。
- `-v`：打开周期数据日志。

## 当前限制

- INA228 驱动已编译，尚未上板验证。
- 新 W5500 尚未测试。
- 板端 Flash 存证在当前演示构建中未启用。
- 漏电、断网补传、RTC/NTP、TinyML 和看门狗未实现。

完整当前状态见 `docs/current/`。
