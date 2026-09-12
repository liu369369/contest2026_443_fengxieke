# AGENTS.md — 火眼·电动自行车充电电气安全监测（openvela 大赛）

> 本文件供所有在此仓工作的 AI 智能体（Codex 等）自动读取。开始工作前先读 `docs/development_guide.md`。

## 项目

- 基于 openvela（NuttX）+ GD32F470V-START 开发板（板名，实际板载芯片为 **GD32F407VKT6**：
  代码区 512KB + 数据区 2560KB，SRAM 192KB，主频上限 168MHz；openvela 对 F407 的芯片层支持不完整，需自行补齐分发）
- 采集充电回路电流/温度/漏电 → 端侧判定 → 声光报警 → 联网上报 + 本地存证
- 赛道：AI 硬件产品创新 ｜ 提交截止：2026-09-20

## 仓库结构

```text
app/                      # 作品代码（唯一开发位置，manifest 映射到 packages/demos/）
docs/                     # 文档（development_guide.md, submit_checklist.md）
logs/liu369369/           # AI Coding 日志（必须提交）
README.md                 # 作品说明
contest2026_443_fengxieke.xml  # manifest；新增应用子目录需补 <linkfile>
```

## 环境与构建

WSL Ubuntu 22.04 内：

```bash
repo init -u https://github.com/open-vela/contest2026_443_fengxieke \
  -b dev-ai-contest-2026 -m contest2026_443_fengxieke.xml
repo sync -c -j8
```

openvela 工作区根目录（本仓上一级）用：

```bash
./build.sh <gd32f470v-start-board-config> [-j8]
```

> board config 路径以官方《AI 硬件赛道教程导航》为准（【待确认】）。
> 实测可用路径：`vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/nsh`（详见 docs/adaptation_F407.md）。

## 串口控制台（已核实）

- USART0 = **PB6(TX) / PB7(RX)**，115200-8-N-1；板上 GD-Link 无虚拟串口，必须外接 3.3V USB-TTL
  （JP6 第 16/13 脚，GND 在第 1 脚）。
- **不要同时启用 I2C0**：PB6/PB7 与 I2C0 复用；火眼 OLED 走 I2C1（PB10/PB11）。

## 硬件

| 功能 | 器件 | 接口 |
|---|---|---|
| 电流 | ACS712 / 霍尔 | ADC |
| 温度 | NTC（ADC）或 DS18B20 / MLX90614 | ADC / 1-Wire / I2C |
| 声报警 | 蜂鸣器 | GPIO |
| 光报警 | 警示灯 | PWM |
| 显示 | 0.96" SPI OLED | SPI |
| 联网 | W5500 SPI 以太网（或板载 RMII PHY） | SPI / RMII |
| 漏电 | 漏电互感器 | ADC（扩展） |
| 联动 | CAN / RS485 收发器 | CAN / UART（扩展） |

详见 `docs/development_guide.md`，接线上电前先核对板卡原理图。

## 算法要点

- 采样 → 滑动平均 + 一阶低通 → 阈值 + 趋势（斜率/方差）→ 连续 N 点防抖
- 状态机：正常 → 预警 → 报警 → 联动断电/上报，支持复位
- 异常检测可选：PC 训练轻量模型 → 导出 C / TFLite Micro
- PC 端先用 Python 验证算法（读 CSV/串口），再移进 `app/` 的 C 实现

## 提交流程

1. 只在 fork 开发，目标分支 `dev-ai-contest-2026`；
2. commit 信息用 `feat:` / `fix:` / `docs:` / `chore:` 前缀；
3. push 到 fork → 主仓创建 PR → 自行合入（Rebase and merge）；
4. 首次 PR 前签 CLA（openvela.com/#/community/cla，账号 `liu369369`，提交作者邮箱须与签署 CLA 的邮箱一致）；PR 评论 `/check-cla` 复检；
5. 公共仓（nuttx/packages/vendor）改动另开 PR 给组委会。

## AI Coding 日志

- 导出到 `logs/liu369369/<YYYY-MM-DD>/codex__<会话id>.jsonl`，并更新 `logs/liu369369/manifest.json`
- 必须提交，不要被 .gitignore 忽略

## 规则

- 改后必验：任何改动都要有能编译/能跑的最小验证
- fail-fast：关键路径断言/报错退出，禁止静默 `continue`
- 文档同步：行为变化同步更新 README 与 docs/development_guide.md
- 不破坏历史：保留旧产物与结论，废弃内容在文档标注
