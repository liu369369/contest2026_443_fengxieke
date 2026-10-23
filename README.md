# 火眼——电动自行车充电电气安全监测终端

队伍：风歇客（刘艳秋、李晓、王阳、龙潇）

2026 首届 openvela AI 硬件开发者大赛决赛项目。系统基于 openvela/NuttX 和
GD32F407VK，监测充电回路电流、温度与母线电压，在本地完成分级告警和负载断电，
并通过以太网把数据送到上位机做事件留痕与 AI 研判。

## 当前状态

> 2026-10-23：新载板 PCB 已完成并焊接；固件已迁移到 INA228 并完成编译，
> 但尚未烧录和上板验证。新 W5500 尚未测试。

| 项目 | 状态 |
|---|---|
| 新载板 PCB | 已完成焊接（用户确认） |
| INA228 固件 | 已实现并编译 |
| 继电器安全语义 | 已改为负载接通/断开，待实测 |
| 蜂鸣器 | 上电默认关闭，待验证报警 |
| 新 W5500 | 已购买，未测试 |
| 3D 打印外壳 | 已有原型，待装配验收 |
| 固件上板 | 未开始 |
| 决赛 PPT | 待按决赛群模板制作 |

当前完整说明从 [docs/current/README.md](docs/current/README.md) 开始。

## 当前硬件

- 主控：GD32F470V-START，板载 GD32F407VKT6，168MHz。
- 被测回路：24V，经保护、INA228 的 2mΩ 分流电阻和常开继电器后接负载。
- 终端供电：独立 12V 电池，经 DC-DC 转 5V、AMS1117 转 3.3V。
- 电流：INA228，I2C1 地址 `0x40`。
- 继电器：`COM → NO → LOAD`，PB0 高电平励磁并接通负载。
- 蜂鸣器：PB1 高电平导通。
- 报警灯：PD9 高电平触发。
- OLED：SSD1306，I2C1。
- 调试串口：USART0，PB6/PB7，115200-8-N-1。

详细引脚和待验证项见
[hardware-baseline.md](docs/current/hardware-baseline.md)。

## 软件结构

- `app/fireeye/src/fireeye_ina228.c`：INA228 驱动。
- `app/fireeye/src/fireeye_sensors.c`：传感器、负载和报警输出。
- `app/fireeye/src/fireeye_filter.c`：滑动平均与低通滤波。
- `app/fireeye/src/fireeye_threshold.c`：阈值、趋势与连续防抖。
- `app/fireeye/src/fireeye_fsm.c`：状态机。
- `app/fireeye/src/fireeye_net.c`：W5500 HTTP 上报。
- `tools/fireeye_monitor.py`：上位机、事件留存和 MiMo 研判。

## 构建

```bash
repo init -u https://github.com/open-vela/contest2026_443_fengxieke \
  -b dev-ai-contest-2026 -m contest2026_443_fengxieke.xml
repo sync -c -j8
source build/envsetup.sh
./build.sh vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/nsh --cmake -j8
```

当前 WSL 增量构建：

```bash
cd ~/openvela
ninja -C out3 nuttx nuttx.hex nuttx.bin -j8
```

公共仓改动必须能通过仓库内 `patches/` 复现。

## 上板自检

```text
nsh> i2c bus 1
nsh> fireeye current
nsh> fireeye test
nsh> fireeye -v
```

完整顺序见 [bringup.md](docs/current/bringup.md)。

## 历史边界

ACS712 + PA4、COM+NC、“继电器 true 表示断电”等内容属于 2026-09 旧演示板，
仅作历史参考。相关文档保留但已标记为历史资料，不得指导新载板。

## 已知限制

- 新载板尚未完成上电、传感器、继电器和网络验收。
- 板端 Flash 存证曾在旧板上未跑通，当前演示固件未启用。
- 漏电监测、断网补传、RTC/NTP、CAN/RS485、TinyML 和看门狗未实现。
- 外壳仍是原型，不是冻结的制造版本。
- 新 W5500 的最终通信状态未知。

## AI Coding 与提交

历史日志保存在 `logs/liu369369/`。当前 AI 采集器需要恢复，具体计划见
[repo-and-logs.md](docs/current/repo-and-logs.md)。

决赛答辩为 2026-11-07 至 2026-11-08，8 分钟展示加 2 分钟问答。最终材料以决赛群通知为准。
