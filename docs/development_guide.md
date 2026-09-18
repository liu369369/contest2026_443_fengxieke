# 火眼·开发协同指南

> 目的：让新队友 clone 后能快速上手——知道环境怎么搭、代码往哪写、硬件怎么接、怎么提交。作品提交截止：**2026-09-20**。

## 1. 项目一句话

基于 openvela + GD32F470V-START（官方已适配开发板），做电动自行车充电电气安全监测终端：采集电流/温度，超限声光报警 + 联网上报 + 板端本地存证（片上 Flash，断电可回放）；漏电监测为后续扩展项。

## 2. 仓库与分支

- 主仓：`open-vela/contest2026_443_fengxieke`，开发分支 `dev-ai-contest-2026`
- 流程：**fork 主仓 → 在你自己的 fork 开发 → PR 回主仓 → 自行 review 合入**
- 作品代码只放本仓 `app/` 下；公共仓（nuttx/packages/vendor 等）改动另走独立 PR 给组委会
- 首次 PR 需在官网签 CLA（`openvela.com/#/community/cla`，账号用提交作者邮箱一致的那个）

## 3. 环境搭建（WSL Ubuntu）

```bash
# 安装 WSL Ubuntu 22.04 后执行
git clone https://github.com/open-vela/contest2026_443_fengxieke.git
cd contest2026_443_fengxieke
repo init -u https://github.com/open-vela/contest2026_443_fengxieke \
  -b dev-ai-contest-2026 -m contest2026_443_fengxieke.xml
repo sync -c -j8

# 构建入口（在 openvela 工作区根目录，即本仓上一级）
cd ..
./build.sh <gd32f470v-start-board-config> [-j8]
```

> [待确认] GD32F470V-START 的 board config 路径以官方《AI 硬件赛道教程导航》为准；用 `find . -iname "*gd32*"` 或 `ls` 定位后填入。

## 4. 硬件与接线

| 功能 | 器件 | 接口 | 状态 |
|---|---|---|---|
| 电流采集 | ACS712 / 霍尔电流模块 | 模拟输出 → ADC | 必需 |
| 温度采集 | NTC 热敏电阻（ADC）或 DS18B20（1-Wire）/ MLX90614（I2C） | 对应接口 | 必需 |
| 声报警 | 蜂鸣器 | GPIO | 必需 |
| 光报警 | 警示灯/LED | PWM | 必需 |
| 本地显示 | 0.96" SPI OLED | SPI | 必需 |
| 联网上报 | W5500 SPI 以太网模块（或板载 RMII PHY） | SPI / RMII | [待确认] 板载 PHY |
| 漏电监测 | 漏电互感器 | 模拟 → ADC | 扩展 |
| 充电桩联动 | CAN 收发器 / RS485 | CAN / UART | 扩展 |

> 接线务必先看板卡原理图/引脚映射；传感器供电、共地、ADC 分压要一次接对。

## 5. 采集与算法原型（可先在 PC 端写）

建议的最小闭环结构：

1. 采样：电流/温度多通道（DMA 或轮询），采样率 ~10Hz 以上；
2. 滤波：滑动平均（窗口 5~20）+ 一阶低通；
3. 判定：单阈值 + 趋势（斜率/方差）双条件，**超限要连续 N 个采样点才触发**（防抖）；
4. 状态机：`正常 → 预警 → 报警 → 联动断电/上报`，可人工复位；
5. （可选）异常检测：PC 端采集历史数据训练轻量模型（异常检测/分类），导出为 C 或 TFLite Micro 部署。

> PC 端可用 Python 脚本先验证算法（读 CSV/串口数据），确认无误再移进 `app/` 的 C 实现。

## 6. 目录约定（本仓）

```text
app/hello_app/      # 应用骨架（将替换为火眼应用，即 contest2026_443_hello_app）
docs/               # 文档（本指南、提交清单）
logs/liu369369/     # AI Coding 日志（必须提交）
README.md           # 作品说明
```

新增应用子目录时，在 `contest2026_443_fengxieke.xml` 补一条 `<linkfile>` 映射到 `packages/demos/`。

## 7. 开发排期（W1–W4）

| 周 | 目标 | 交付 |
|---|---|---|
| W1 | 环境搭建 + 拉源码 + 编译 GD32 配置；PC 端采集算法原型 | 可编译环境、算法脚本 |
| W2 | 板级打通：ADC 采样 + SPI OLED 显示；充电模拟实验台 | 板子上数据可读 |
| W3 | 状态机/告警 + 声光报警 + 联网上报 | 端到端告警演示 |
| W4 | 板端本地存证（片上 Flash 回放）+ 上位机报警留存 + 打磨 + 演示视频 + 文档 + AI 日志 + PR | 完整作品 |

## 8. AI Coding 日志

- 导出到 `logs/liu369369/<YYYY-MM-DD>/codex__<会话id>.jsonl`，并更新 `logs/liu369369/manifest.json`；
- 目录示例可参考模板（已删除占位，用时会重建）；
- **必须提交**，不要被 .gitignore 忽略。

## 9. 提交流程（含 CLA）

```bash
git add -A
git commit -m "feat: <说明>"
git push origin dev-ai-contest-2026        # 推到你的 fork
# 浏览器：到主仓点 Compare 创建 PR（base=dev-ai-contest-2026, head=你的fork）
# 若 CLA 未通过：官网签 CLA 后，在 PR 评论 /check-cla
# 通过后点 Merge（Rebase and merge）
```

## 10. 分工建议

- 硬件/接线：1 人（传感器、电源、接线、原理图）
- 系统/驱动：1 人（openvela 构建、ADC/SPI/以太网）
- 算法/状态机：1 人（滤波、阈值、防抖、异常检测）
- 文档/演示/日志：1 人（视频、README、AI 日志、传播）

---

*contest2026_443_fengxieke · 火眼项目组*
