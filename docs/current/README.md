# 火眼当前基线（2026-10-29）

本文是当前项目的入口。历史演示板资料继续保留，但不再作为接线、固件和验收依据。

当前交付范围、已验证能力、限制条件和下一代48V/5A目标统一见
[scope-and-roadmap.md](scope-and-roadmap.md)。

## 一、当前阶段

- 新载板 PCB 已完成并焊接（用户确认，2026-10-23）。
- 固件已从旧 ACS712 路径迁移到 INA228，并完成 GD32 烧录和基础上板验证。
- 独立 STM32F103 硬件 I2C2 测试已验证 OLED、INA228 和共享总线正常
  （2026-10-25）。
- GD32 实测确认 NuttX 硬件 I2C 的轮询和多字节接收路径存在故障；
  当前应用改用 PB10/PB11 软件 I2C，INA228 和 OLED 基础通信已通过。
- 当前诊断固件继续关闭 W5500、网络初始化和联网上报。
- 继电器业务语义已改为“负载接通/断开”，板级上电默认释放。
- 蜂鸣器当前按高电平有效处理，上电默认关闭。
- 新 W5500 已确认 SPI 访问和 MACRAW 初始化正常，但 PHY Link 未建立。
  同一网线接电脑与树莓派时正常，接 W5500 与树莓派时两侧网口灯均不亮；
  当前优先怀疑模块 RJ45/PHY/网络变压器链路故障，尚待替换模块确认。
- 3D 打印外壳已有可打印原型，仍需与实物完成装配验收。
- GD32 基础启动、INA228、OLED、NTC 和输出自检已上板验证；
  INA228 带载标定和整机硬件联调仍未完成。

## 二、当前硬件

以 [hardware-baseline.md](hardware-baseline.md) 为唯一接线依据：

| 功能 | 当前方案 | 状态 |
|---|---|---|
| 电流 | INA228，I2C1，地址 `0x40`，2mΩ 分流 | GD32 ID/配置回读通过；带载电流、零点和方向待标定 |
| 温度 | NTC 10K B3950 接 PA6 | 常温、超温 ALARM 和降温恢复已验证 |
| 继电器 | COM→NO→负载，PB0 高电平励磁 | PCB 实物接法已确认；输出自检通过，带载通断待复测 |
| 蜂鸣器 | 板载有源蜂鸣器，PB1 高电平导通 | `fireeye test` 和告警状态已验证 |
| 报警灯 | PD9 高电平触发 | `fireeye test` 和告警状态已验证 |
| 显示 | SSD1306 OLED，I2C1，地址 `0x3C` | 实时监测已验证；新版仪表盘已编译，待烧录确认 |
| 网络 | 新 W5500，SPI1 | SPI/MACRAW 通过；PHY 无 Link，疑模块硬件故障，当前冻结 |

功率路径：EasyEDA实时复核确认`24V_IN`和`LOAD_P`全部60mil，`24V_SW`
主功率线为60mil；10mil `24V_SW`仅用于INA228 VBUS采样。当前主要限制是
2A自恢复保险丝和SS54，仍未改板前禁止直接实流测试5A/8A。

## 三、启动流程

1. 先阅读 [firmware-status.md](firmware-status.md)。
2. I2C 器件排障先阅读
   [i2c-validation-20261025.md](i2c-validation-20261025.md)。
3. NTC、继电器和 W5500 的最新实机结论见
   [board-validation-20261028.md](board-validation-20261028.md)。
4. 烧录前按 [bringup.md](bringup.md) 检查电源和负载。
5. 构建、烧录和上板命令见 [firmware-status.md](firmware-status.md)。
6. 仓库提交和 AI 日志规则见 [repo-and-logs.md](repo-and-logs.md)。
7. 11月4日前必须完成代码和日志合入，材料清单见
   [submit_checklist.md](../submit_checklist.md)。

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
