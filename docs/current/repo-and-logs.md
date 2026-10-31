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
4. PR #7、#8、#9 已合入官方主线，当前主线为 `15c8e4a`。
5. 当前工作树包含 INA228/OLED 软件 I2C、W5500 网络恢复、网络超时修复、
   上板验证文档和 AI 日志增量，需通过短分支独立 PR 合入。
6. 后续上板证据继续使用新的短分支和独立 PR。
7. 公共仓 nuttx/vendor 改动继续走独立 PR。

**2026-10-31提交状态**：网络直连、Ping、HTTP 上报和网页显示已初测通过；
网络超时修复已编译，长时间稳定性仍待上板验收。按官方规则，未合入默认分支的
改动不计入评审。

## 二、AI 日志

官方要求：

- 日志位于 `logs/<github_login>/<date>/<tool>__<session>.jsonl`。
- 只能由官方采集器生成，不手工修改内容。
- 提交前运行 `validate-log.py`。
- 最终上传仍由开发者执行 `git add logs/`、commit 和 push。

当前状态：

- 仓库现有76个日志，最后补录目录为2026-10-31。
- 官方采集器已恢复到 WSL `/home/liu/openvela/.claude`。
- `verify-setup.sh` 通过 10 项、失败 0 项。
- 2026-10-29新增7个Codex会话，覆盖10月9日至当时开发会话。
- 2026-10-31通过官方 Codex 解析和 redact 规则增量采集当前会话。
- 官方`validate-log.py`校验通过：76个文件、10510个事件、`ALL OK`。
- 脱敏规则扫描未发现GitHub/API/Bearer密钥残留。
- 旧 WSL 日志冲突副本已移到 `/home/liu/openvela/logs_backup_pre_20261023/`。
- `D:\openvela` 与 WSL `.repo/` 工作区分离；本次已通过官方解析入口补采，
  后续 Codex 开发仍建议从 WSL openvela 工作区启动，避免依赖手工补采。

后续步骤：

1. 后续开发固定从真实 openvela 工作区启动 Codex。
2. 每个关键阶段用官方工具增量采集并运行校验。
3. 当前Windows Codex会话仍在进行；本轮最后一次快照之后的助手回复，
   会在下一次调用官方采集器时作为增量追加。
4. 提交前再运行一次`validate-log.py`，不修改JSONL内容。

## 三、决赛材料

GitHub 官方文档确认：

- 决赛：2026-11-07 至 2026-11-08。
- 8 分钟展示加 2 分钟问答。
- 真机、外壳、身份证件。
- 技术和产品方案并重。
- 官方材料提交截止：2026-11-04。
- 答辩PPT必传，建议使用决赛群公告中的官方模板。
- 作品仓库地址必填；评委以截止时专属仓中已经合入的代码为准。
- 演示视频和作品实物照片建议上传。
- 初赛后主要更新可选填。
- 队伍名称和答辩时段必须与官方安排一致。
- 表单可重复提交，以最后一次为准。

提交入口：
<https://mi.feishu.cn/share/base/form/shrcnx9rdrDGHECQyNN5yxhNxJe>
