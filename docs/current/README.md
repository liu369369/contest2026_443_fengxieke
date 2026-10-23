# 火眼当前基线（2026-10-23）

本文是当前项目的入口。历史演示板资料继续保留，但不再作为接线、固件和验收依据。

## 一、当前阶段

- 新载板 PCB 已完成并焊接（用户确认，2026-10-23）。
- 固件已从旧 ACS712 路径迁移到 INA228，并完成编译。
- 继电器业务语义已改为“负载接通/断开”，板级上电默认释放。
- 蜂鸣器当前按高电平有效处理，上电默认关闭。
- 新购买的 W5500 尚未测试。
- 3D 打印外壳已有可打印原型，仍需与实物完成装配验收。
- 新固件尚未烧录和上板验证，不能写成“硬件联调完成”。

## 二、当前硬件

以 [hardware-baseline.md](hardware-baseline.md) 为唯一接线依据：

| 功能 | 当前方案 | 状态 |
|---|---|---|
| 电流 | INA228，I2C1，地址 `0x40`，2mΩ 分流 | 固件已实现，待上板验证 |
| 温度 | NTC 10K B3950 接 PA6 | 沿用旧方案，待新板验证 |
| 继电器 | COM→NO→负载，PB0 高电平励磁 | 固件语义已改，待验证 |
| 蜂鸣器 | 板载有源蜂鸣器，PB1 高电平导通 | 上电静音已观察，待验证报警 |
| 报警灯 | PD9 高电平触发 | 待验证 |
| 显示 | SSD1306 OLED，I2C1 | 待验证 |
| 网络 | 新 W5500，SPI1 | 未测试 |

## 三、启动流程

1. 先阅读 [firmware-status.md](firmware-status.md)。
2. 烧录前按 [bringup.md](bringup.md) 检查电源和负载。
3. 构建、烧录和上板命令见 [firmware-status.md](firmware-status.md)。
4. 仓库提交和 AI 日志规则见 [repo-and-logs.md](repo-and-logs.md)。

## 四、新旧边界

当前有效：

- `hardware/fireeye_carrier_board_design.md`
- `hardware/design_optimizations.md`
- `hardware/enclosure/fireeye_enclosure_v4.scad`
- `app/fireeye/src/fireeye_ina228.c`

仅作历史参考：

- ACS712 + PA4 模拟采样
- COM+NC 继电器接法
- “继电器 true 表示断电”的旧语义
- 旧演示板接线表、演示脚本和固件包

历史文件不删除，Git 历史和 `docs/archive/` 保留追溯依据。
