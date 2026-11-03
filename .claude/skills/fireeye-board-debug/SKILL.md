---
name: fireeye-board-debug
description: "火眼（电动自行车充电电气安全监测终端）当前载板的构建、烧录、串口自检、INA228/NTC采集、继电器失效安全和W5500网络排查流程。Trigger: 火眼固件, GD32F407, openvela/NuttX, INA228, W5500, 软件I2C, 继电器断载, fireeye test."
---

# 火眼板级构建与故障排查

## 当前硬件基线

- 主控：GD32F470V-START，板载 GD32F407VKT6。
- 系统：openvela/NuttX，应用入口 `app/fireeye/`。
- 电流与母线电压：INA228，I2C1 地址 `0x40`，2mΩ 分流。
- 温度：NTC 10K B3950，接 PA6。
- 继电器：负载接 COM→NO；PB0 高电平使线圈励磁并接通负载，掉电自然断开。
- 蜂鸣器：PB1 高电平导通。
- 报警灯：PD9 高电平触发。
- 显示：SSD1306 OLED，I2C1 地址 `0x3C`。
- 网络：W5500 走 SPI1，静态地址 `192.168.1.200`，通过 HTTP 上报。
- 串口：USART0，PB6/PB7，115200-8-N-1。

旧 ACS712、COM-NC 和“继电器 true 表示断电”属于历史演示板，不再作为当前操作依据。

## 构建与烧录

当前 WSL 增量构建：

```bash
cd ~/openvela
make -C out_i2c_irq nuttx -j8
arm-none-eabi-objcopy -O ihex out_i2c_irq/nuttx out_i2c_irq/nuttx.hex
arm-none-eabi-objcopy -O binary out_i2c_irq/nuttx out_i2c_irq/nuttx.bin
```

- 配置文件：`vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/nsh`。
- 使用 GD-Link Utility 烧录 HEX。
- 板载 LED 心跳表示 openvela 已启动。

## 串口自检

```text
nsh> i2c bus
nsh> i2c dev -b1 -f100000 0x3c 0x40
nsh> fireeye -v
nsh> fireeye current
nsh> fireeye temp
nsh> fireeye oled
nsh> fireeye test
```

`fireeye test` 会让继电器、蜂鸣器和报警灯动作，执行前应断开功率负载。

## 当前已验证

- INA228 的 ID、配置回读、基础带载电流和独立 VBUS 测量通过。
- OLED 自检、实时状态显示和本地界面通过。
- NTC 常温、超温 ALARM、降温恢复和启动假趋势修复通过。
- 1.2A WARNING、1.5A ALARM、声光提示和继电器断载通过。
- W5500 直连电脑的 Link、Ping、HTTP、网页和 AI 功能通过。
- 断电、复位或应用未启动时，常开继电器保持负载断开。

## 重要故障与修复

### GD32 硬件 I2C 多字节读取异常

板级 NuttX 硬件 I2C 路径在 INA228/OLED 多字节接收时存在状态机问题。
当前应用使用 PB10/PB11 软件 I2C 完成兼容访问。不要在没有复测的情况下
重新切回硬件 I2C。

### 网络启动 HardFault

单核配置同时启用 `CONFIG_UP=y` 和 `CONFIG_SPINLOCK=y` 会触发
`CFSR=0x01000000` 非对齐故障。单核构建不要启用 spinlock。

### HTTP 上报停滞

不要使用缺少应用层超时的阻塞式 `connect/send/recv`。保持非阻塞 socket，
通过 `poll()` 设置 3 秒硬超时，异常后关闭 socket，避免网络锁长期占用。

## 安全边界

- 当前是 24V 低压监测原型，不是 48V/60V/72V 通用产品。
- F1 是 2A 自恢复保险丝，禁止进行 5A/8A 实流测试。
- INA228 软件增益和零点标定仍需复核，不宣称计量级精度。
- 网络和主机 AI 不参与本地断保。

## 常见文件

- `app/fireeye/src/fireeye_ina228.c`：INA228 驱动。
- `app/fireeye/src/fireeye_sensors.c`：NTC、负载和报警输出。
- `app/fireeye/src/fireeye_filter.c`：滑动平均与低通滤波。
- `app/fireeye/src/fireeye_threshold.c`：阈值、趋势和连续防抖。
- `app/fireeye/src/fireeye_fsm.c`：安全状态机。
- `app/fireeye/src/fireeye_net.c`：W5500 HTTP 上报。
- `tools/fireeye_monitor.py`：网页、事件留存和 MiMo 辅助研判。
