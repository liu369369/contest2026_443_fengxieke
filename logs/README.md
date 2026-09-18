# logs/ — AI Coding 日志目录

存放你在开发中与 AI 工具的对话日志，和作品代码一并提交。

> 本目录现在是**示例**，请替换成你自己导出的真实日志（删掉示例的 `your-github-login/` 目录）。

## 目录结构

```text
logs/
└── <github_login>/              # 你的 GitHub 用户名，一人一目录
    ├── manifest.json            # 会话清单
    └── <date>/                  # 日期 YYYY-MM-DD
        └── <tool>__<sid>.jsonl  # 一个会话一个文件（工具名与 session id 用 __ 连接）
```

- `<tool>`：`claude-code` / `opencode` / `codex` / `kiro`
- 每个 `.jsonl` 每行一个事件，由组委会提供的日志归集工具导出，**只提交 JSONL 本身**。

导出与提交的完整步骤、字段定义见[《AI Coding 日志归集与提交手册》](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_coding_log_guide.md)。

## 导出说明（2026-09-18 补充）

本目录的 Codex 会话日志由大赛官方 contest-log-collector 的 schema 与脱敏规则统一导出。
2026-09-18 当晚，官方收集器对 Codex App 的新会话格式解析结果为空（会话数与事件数不一致），
因此改用同一个 schema、同一套 redact.json 脱敏规则，从本机原始会话记录（rollout JSONL）重新导出，
并已通过官方校验脚本 	ools/validate-log.py（✅ ALL OK，58 个文件 / 4564 条事件）。
重导只针对格式合规性，未新增、未伪造任何会话内容；如需原始 rollout 文件可另行提供。
