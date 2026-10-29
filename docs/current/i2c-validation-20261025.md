# I2C 器件与共享总线验证记录（2026-10-25）

本文记录使用独立 STM32F103C8T6 测试主机对新载板 I2C 器件的验证结果。
该结果用于排除 OLED、INA228 和共享总线问题，不等同于 GD32 整机固件验证。

## 一、测试目的

GD32 应用无法稳定识别 INA228 时，需要区分以下可能原因：

1. INA228 芯片、供电、焊接或地址异常。
2. OLED 与 INA228 不能在同一条总线上共存。
3. STM32/GD32 的 I2C 引脚、驱动或初始化时序异常。

为避免继续依赖待排查的 GD32 I2C 驱动，本轮改用已知可独立烧录和串口观测的
STM32F103C8T6 作为 I2C 主机。

## 二、测试环境

| 项目 | 配置 |
|---|---|
| 测试主机 | STM32F103C8T6 |
| I2C 外设 | 硬件 I2C2 |
| SCL | PB10 |
| SDA | PB11 |
| 速率 | 100 kHz |
| 串口 | USART1，PA9/PA10，115200-8-N-1 |
| OLED | SSD1306，地址 `0x3C` |
| 电流芯片 | INA228，地址 `0x40` |
| 烧录校验 | ST-Link，Verification OK |

本地诊断工程：

```text
C:\Users\刘艳秋\Desktop\火眼_INA228_STM32F103_Keil
```

测试固件 MD5：

```text
779cfd4caa827cd2176ea88757879588
```

该工程是本轮排查工具，尚未纳入正式参赛仓。

## 三、接线原则

- STM32 PB10 接总线 SCL。
- STM32 PB11 接总线 SDA。
- OLED 和 INA228 共用同一条 SCL/SDA 总线。
- STM32 与载板只共地，不互接两个 3.3V 电源。
- 载板必须正常供电，再由外部上拉维持空闲高电平。
- 若 GD32 也连接在同一条总线上，测试期间必须让 GD32 保持在复位或高阻状态，
  避免两个 MCU 同时驱动总线。

## 四、实测结果

OLED 和 INA228 同时连接、载板正常供电后，串口输出为：

```text
Input pullup levels SCL=1 SDA=1
Recovered levels SCL=1 SDA=1
I2C2 scan:
  found 0x3C
  found 0x40
Scan devices=02
OLED 0x3C: ACK
INA228 0x40: ACK

MANUFACTURER_ID: err=00 value=0x5449
DEVICE_ID: err=00 value=0x2281
```

## 五、排查中确认的问题

1. 初次改为硬件 I2C 后，PB10/PB11 被误判为总线短路。实际原因是固件删除了
   原软件 I2C 初始化代码时，也漏掉了 GPIOB 时钟使能，导致引脚配置未生效。
2. 载板未供电时，SCL 和 SDA 会被外部电路拉低。测试程序在总线非空闲时拒绝
   启动硬件 I2C 是正确保护行为。
3. 只接 OLED 时可扫描到 `0x3C`；只接 INA228 时可扫描到 `0x40`；两者同时连接
   时可扫描到两个地址，说明地址和共享总线均正常。

## 六、结论

- OLED 的 `0x3C` 地址和 I2C 通信正常。
- INA228 的供电、焊接、`0x40` 地址和通信正常。
- INA228 返回的厂商 ID `0x5449` 和器件 ID `0x2281` 符合预期。
- OLED 与 INA228 可以在同一硬件 I2C 总线上共存。
- GD32 此前无法识别 INA228，问题不在 INA228 芯片本体。

## 七、GD32 上板结果

在同一载板和同一组 SCL/SDA 上切换为 GD32 主机后，依次做了三层验证：

1. 应用启动时把 PB10/PB11 临时配置为开漏 GPIO，用软件位翻转探测地址。
2. 绕过 NuttX 驱动，直接操作 I2C1 寄存器探测地址。
3. 通过 NuttX `/dev/i2c1` 中断驱动扫描、写 OLED 和读取 INA228。

实测启动日志：

```text
FireEye I2C bitbang idle: SCL=1 SDA=1
FireEye I2C bitbang 0x3c: ACK
FireEye I2C bitbang 0x40: ACK
FireEye I2C hw 0x3c: ACK STAT0=0x0082
FireEye I2C hw 0x40: ACK STAT0=0x0082
```

结论：

- 负载、上拉、PB10/PB11、AF4、GD32 I2C1 外设均正常。
- NuttX 的 GD32 I2C 轮询路径无法完成地址访问。
- 中断路径可扫描到 `0x3C` 和 `0x40`，OLED 写入自检通过。
- 中断路径读取 INA228 16 位寄存器时返回错误值，并导致下一次事务 AERR；
  问题位于 NuttX GD32 I2C 接收状态机。

当前固件的 INA228/OLED 应用访问改为 PB10/PB11 软件 I2C，硬件 I2C 驱动
仅保留给调试工具。修复后实测：

```text
INA228: mfg=0x5449 id=0x2281 config=0x0010 adc=0xb142
raw20=2 current=0.000A bus=0.01V
OLED test OK
```

2026-10-27 至 2026-10-28 的后续测试还确认：INA228/OLED 初始化前的重复硬件
I2C reset 会干扰实时监测启动，该重复 reset 已移除。应用仍走软件 I2C，因此这
不等于 NuttX 硬件 I2C 接收状态机已经修复。

## 八、验证边界

本记录不能证明以下项目已经完成：

- NuttX GD32 硬件 I2C 多字节接收路径已修复；当前只是应用层绕过。
- INA228 带载后的电流方向、零点、母线电压和 20 位原始码正确。
- 继电器带实际负载的通断、掉电断开和触点压降已经验收。
- 新 W5500 的 PHY Link、路由和 HTTP 上报正常。

NTC常温、超温ALARM、降温恢复以及蜂鸣器/报警灯动作已经通过。继电器线圈
1/4脚正常，3/5脚短接返修后基础带载通路也已通过。2026-10-28
的 W5500 探测显示 SPI/MACRAW 正常但 PHY 无 Link，详见
[board-validation-20261028.md](board-validation-20261028.md)。

下一步先接稳 NTC 并完成 INA228 带载标定，再把 NuttX GD32 I2C 修复或明确的
应用层兼容方案同步到 `patches/`。W5500 在获得替换模块前保持冻结。
