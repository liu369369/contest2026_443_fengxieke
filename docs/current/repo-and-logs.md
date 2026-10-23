# 仓库与 AI 日志计划

## 一、仓库

- 唯一正式仓：`open-vela/contest2026_443_fengxieke`
- 默认分支：`dev-ai-contest-2026`
- 不从旧 `D:\codex` 路径提交。
- 不新建第二个决赛仓。

当前整理策略：

1. 已关闭历史分叉的 PR #6。
2. 已从官方 `dev-ai-contest-2026` 新建 `feat/new-carrier-baseline`。
3. 已拆分提交构建产物清理、硬件原型、文档基线和固件改动。
4. PR #7 已通过 CLA 检查并以 rebase 方式合入，官方主线为 `dd53b7a`。
5. 后续上板证据使用新的短分支和独立 PR。
6. 公共仓 nuttx/vendor 改动继续走独立 PR。

## 二、AI 日志

官方要求：

- 日志位于 `logs/<github_login>/<date>/<tool>__<session>.jsonl`。
- 只能由官方采集器生成，不手工修改内容。
- 提交前运行 `validate-log.py`。
- 最终上传仍由开发者执行 `git add logs/`、commit 和 push。

当前状态：

- 仓库已有 58 个日志，最后日期为 2026-09-18。
- 官方采集器已恢复到 WSL `/home/liu/openvela/.claude`。
- `verify-setup.sh` 通过 10 项、失败 0 项。
- WSL 正式仓日志已恢复为 58 个，官方校验通过（4564 条事件）。
- 旧 WSL 日志冲突副本已移到 `/home/liu/openvela/logs_backup_pre_20261023/`。
- `D:\openvela` 仍不在含 `.repo/` 的官方采集工作区内，因此后续需要计入日志的
  Codex 开发应在 WSL openvela 工作区中启动。

后续步骤：

1. 后续开发固定从真实 openvela 工作区启动 Codex。
2. 用官方 Codex backfill 工具补录可确认属于工作区的历史会话。
3. 每个关键阶段运行官方校验，不修改 JSONL 内容。
4. 将新增日志同步回正式仓后再提交。

## 三、决赛材料

GitHub 官方文档确认：

- 决赛：2026-11-07 至 2026-11-08。
- 8 分钟展示加 2 分钟问答。
- 真机、外壳、身份证件。
- 技术和产品方案并重。
- PPT 模板和最终提交安排以决赛群通知为准。

视频不纳入当前工作范围。
