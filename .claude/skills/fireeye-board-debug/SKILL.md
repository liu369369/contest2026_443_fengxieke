---
name: fireeye-board-debug
description: "火眼（电动自行车充电电气安全监测终端）在 GD32F407VK 开发板上的构建、烧录与上板自检流程。当需要重新编译固件、确认继电器/蜂鸣器/报警灯极性、排查电流或温度采样链路、或复现板端存证与 flash 编程现象时使用。Trigger: 火眼构建、fireeye 固件、GD32F407 烧录、继电器极性、fireeye adc、fireeye flash、存证排查."
---

# 火眼 · 板级构建与自检

## 一、构建（WSL）

```bash
cd ~/openvela
ninja -C out3 nuttx nuttx.hex nuttx.bin -j8
```

配置目录：`vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/nsh`。
改动 defconfig 后必须重新生成 `.config`（拷 defconfig 到 out3/.config 后运行 olddefconfig，
PYTHONPATH 指向 `prebuilts/tools/python/dist-packages/kconfiglib`），否则改动不会生效。
产物：`out3/nuttx.hex`（GD-Link Utility 烧录）、`out3/nuttx.bin`。

## 二、烧录与串口

- GD-Link Utility 烧 `nuttx.hex`；
- 串口 USART0 = PB6(TX)/PB7(RX)，115200-8-N-1，需外接 3.3V USB-TTL（JP6 第 16/13 脚）；
- 上电后板载 LED（PC6）1Hz 心跳表示系统已启动。

## 三、上板自检命令

| 命令 | 作用 |
|---|---|
| `fireeye test` | 蜂鸣器 3 短声 → 继电器吸合 2 秒 → 预警音型 → 报警灯亮灭各 1 秒 ×3 |
| `fireeye level` | 三个输出脚依次 LOW 2 秒 / HIGH 2 秒，用于确认模块极性 |
| `fireeye adc` | 打印继电器吸合/释放两种状态下电流通道的原始 ADC 码值（负载不接时差值即线圈耦合偏移） |
| `fireeye flash` | 打印 FMC 容量、寄存器与四个候选地址快照，并做字节/字编程测试 |
| `fireeye -v` | 运行监测并每 10 秒打印一行数据 |

## 四、已知硬件事实（实测，写材料时以此为准）

- 芯片 GD32F407VK，3MB Flash（`fireeye flash` 读出 `FMC_SIZE=3072KB`）、SRAM 192KB、168MHz；
- 继电器模块**高电平触发**，负载接 COM + NC；上电时板级 bring-up 默认吸合（负载断电），
  运行后进入 NORMAL 才供电；报警时吸合断电；
- 蜂鸣器低电平触发（PB1），报警灯高电平触发（PD9），按键 PA0；
- 电流 ACS712-30A + 10k/10k 分压进 PA4，1 码约 0.024 A；负载为 5V 风扇（铭牌 0.15 A）；
- 存证：0x08080000 可编程、同扇区同页的 0x08080018 写不进 ⇒ 板端存证在本板不可用，
  演示固件已关闭（详见 README「已知限制」）。

## 五、常见问题

| 现象 | 处理 |
|---|---|
| 风扇一直不转 | 检查继电器是否接 COM + NC；用 `fireeye level` 确认真机上的模块极性 |
| 读数不为 0（0.1~0.2 A） | 正常：分辨率 1 码约 0.024 A + 线圈耦合偏移，按量级讲即可 |
| `fireeye -e` 提示已关闭 | 演示固件关闭了板端存证，事件留痕看上位机 `tools/data/fireeye_events.jsonl` |
