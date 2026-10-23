# 当前固件状态

## 一、版本

- 应用版本：`FireEye 2.0.0`
- 基线日期：2026-10-23
- 构建结果：通过
- HEX：905494 字节，MD5 `1d886a5caae8ad985df0faffbbb666ec`
- BIN：321904 字节，MD5 `5313335b387ffbe00327a8e021d215d7`
- 构建环境：WSL `/home/liu/openvela/out3`
- 状态：已编译，尚未烧录和上板验证

## 二、本版改动

- 新增 INA228 驱动，默认从 I2C1 地址 `0x40` 读取电流和母线电压。
- 保留 ACS712 后端作为旧演示板兼容选项，通过 Kconfig 选择。
- 新增 `fireeye current` 电流链路自检命令。
- 继电器改为明确的负载接通/断开语义。
- 板级上电默认改为继电器释放、负载断开。
- 蜂鸣器统一为高电平有效。
- 修复滑动平均窗口参数未生效。
- 修复“连续 N 点”防抖实际会衰减计数的问题。
- 趋势判定只使用正斜率，不再把下降趋势当成快速上升。
- Kconfig 中的采样率和阈值现在真正传入应用。
- HTTP JSON 增加 `bus_v` 字段。

## 三、构建

```bash
cd ~/openvela
ninja -C out3 nuttx nuttx.hex nuttx.bin -j8
```

正式复现应使用仓库内 `patches/` 的两个补丁，而不是只依赖本机 WSL 工作区。

## 四、上板自检

```text
nsh> fireeye current
nsh> fireeye test
nsh> fireeye -v
```

`fireeye current` 应打印 INA228 厂商 ID、设备 ID、20 位原始码、电流和母线电压。

## 五、仍未验证

- 新载板电源与地。
- INA228 实际读数、方向和零点。
- 常开继电器在正常、报警、复位和掉电时的动作。
- 蜂鸣器和报警灯极性。
- OLED、NTC。
- 新 W5500 的芯片版本、MAC、link、路由和 HTTP 上报。
