# CLAUDE.md - 火眼项目

> 规范以 `AGENTS.md` 为准，当前事实以 `docs/current/` 为准。

## 本项目

- openvela（NuttX）+ GD32F470V-START（GD32F407VK）
- 当前载板使用 INA228 + 2mΩ、独立 12V 供电和常开继电器
- 采集电流/温度/母线电压 → 端侧判定 → 声光报警 → 负载断电 → 联网上报
- 赛道：AI 硬件产品创新 ｜ 截止 2026-09-20

## 开发要点

- 作品代码放 `app/`（manifest 映射到 packages/demos/）；新增子目录需补 `<linkfile>`
- 环境：WSL Ubuntu，`repo init -u https://github.com/open-vela/contest2026_443_fengxieke -b dev-ai-contest-2026 -m contest2026_443_fengxieke.xml && repo sync -c -j8`
- 构建：openvela 工作区根目录 `./build.sh <gd32f470v-start-board-config> [-j8]`
- 硬件：见 `docs/current/hardware-baseline.md`
- 算法：采样 → 滤波 → 阈值+趋势 → 连续 N 点防抖 → 状态机；可选 TinyML

## 提交与日志

- 只在 fork 开发，目标分支 `dev-ai-contest-2026`；commit 前缀 `feat:`/`fix:`/`docs:`/`chore:`
- push fork → 主仓 PR → 自行合入；首次 PR 前签 CLA（账号 `liu369369`）
- AI 日志导出到 `logs/liu369369/<YYYY-MM-DD>/codex__<会话id>.jsonl` 并更新 manifest.json，必须提交

## 规则

- 改后必验；fail-fast；文档同步；不破坏历史
