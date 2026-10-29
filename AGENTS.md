# AGENTS.md - 火眼项目协作规则

## 当前基线与强制边界

- 当前项目以 2026-10 新载板为准。
- PCB 已完成并焊接；GD32 新固件已烧录，NSH、INA228 基础读、OLED 自检、
  NTC 本地告警闭环和输出空载自检通过。
- GD32 NuttX 硬件 I2C 的多字节接收状态机已确认存在故障。当前应用通过
  PB10/PB11 软件 I2C 访问 INA228/OLED，正式驱动修复仍待完成。
- 当前诊断固件关闭 W5500、网络初始化和联网上报；整机联调未完成。
- 当前电流传感器是 INA228，I2C1 地址 `0x40`，2mΩ 分流。
- 继电器目标接法是5脚COM→2脚NO→负载，1/4脚为线圈。当前新板已短接
  3/5脚，把 `24V_SW` 引入COM。
- 当前蜂鸣器按高电平有效，PB1；报警灯 PD9 高电平触发。
- 新 W5500 的 SPI/MACRAW 初始化通过，但 PHY 无 Link；疑模块
  RJ45/PHY/网络变压器链路故障，当前冻结。
- 旧 ACS712 + PA4、COM+NC 和“继电器 true 等于断电”只属于历史演示板。
- Q1、线圈1/4脚和基础带载通路已经验证；INA228量程/方向以及告警、掉电断载
  仍未验收。
- INA228量程不等于整板载流能力：24V主功率线已实时复核为60mil，连接
  INA228的10mil支路仅用于VBUS采样；F1为2A保险丝，禁止直接实流测试5A/8A。

当前事实入口：

- `docs/current/README.md`
- `docs/current/hardware-baseline.md`
- `docs/current/firmware-status.md`
- `docs/current/bringup.md`
- `docs/current/repo-and-logs.md`
- `docs/current/i2c-validation-20261025.md`
- `docs/current/board-validation-20261028.md`

## 项目

- 主控：GD32F470V-START，板载 GD32F407VKT6。
- 系统：openvela/NuttX。
- 作品：电动自行车充电电气安全监测终端。
- 赛道：AI 硬件产品创新 + 新硬件平台适配。
- 决赛：2026-11-07 至 2026-11-08，8 分钟展示加 2 分钟问答。
- 官方材料提交截止：2026-11-04；默认分支中已经合入的代码和日志才计入评审。

## 代码规则

- 应用入口固定在 `app/fireeye/`，不要移动，避免破坏 manifest。
- 业务层使用“接通负载/断开负载”，不要直接解释继电器电平。
- 新功能必须保留旧硬件兼容路径时，通过 Kconfig 明确选择。
- 不能用文档掩盖未验证的硬件行为。
- 修改固件后至少完成一次 WSL 编译。
- 公共仓源码注释使用英文。
- 不得提交密钥、令牌、运行日志、`__pycache__` 或本地事件 JSONL。

## 构建

```bash
cd ~/openvela
make -C out_i2c_irq nuttx -j8
arm-none-eabi-objcopy -O ihex out_i2c_irq/nuttx out_i2c_irq/nuttx.hex
arm-none-eabi-objcopy -O binary out_i2c_irq/nuttx out_i2c_irq/nuttx.bin
```

当前已验证构建目录：`/home/liu/openvela/out_i2c_irq`

## 上板顺序

1. 静态短路和电源检查。
2. 12V 空载上电，检查继电器上电动作和声光默认状态。
3. `i2c bus`、`i2c dev -b1 -f100000 0x3c 0x40` 和 `fireeye current`。
4. `fireeye test`、`fireeye level`。
5. NORMAL/WARNING/ALARM 和负载通断。
6. 新 W5500 芯片、link、路由和 HTTP。

详细步骤见 `docs/current/bringup.md`。

## 仓库

- 唯一正式仓：`open-vela/contest2026_443_fengxieke`。
- 默认分支：`dev-ai-contest-2026`。
- 使用 fork → PR。
- 公共仓 nuttx/vendor 改动单独提 PR。
- 不从未知旧路径提交。

## AI 日志

- 日志目录：`logs/<github_login>/<date>/<tool>__<session>.jsonl`。
- 使用官方采集器生成，不手工修改内容。
- 提交前运行官方 `validate-log.py`。
- 采集器已恢复；后续开发日志和提交步骤见 `docs/current/repo-and-logs.md`。

## 文档同步

硬件行为、固件后端、引脚、网络地址或验收状态改变时，必须同步：

- `README.md`
- `docs/current/*`
- `app/fireeye/README.md`
- 相关板级补丁
