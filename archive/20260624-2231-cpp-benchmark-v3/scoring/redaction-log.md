# 脱敏记录（只记位置与类型，不写参评身份）

匿名盲评包 `scoring/blind-package/P01`–`P06` 由各参评对象提交经以下脱敏处理生成。身份信息（Agent 名、模型名、原目录 slug、对话来源）一律未出现在盲评包中。

## 脱敏措施

| 项 | 处理 |
|----|------|
| 目录 slug | 各 `agents/<slug>/` 重命名为匿名 `P01`–`P06`，slug 含模型/Agent 信息，已抹除 |
| 生成脚本文件名 | `gen_svgs.py` / `generate_svgs.py` 保留原文件名（不含身份词），作为产物来源证据 |
| ANSWER.md / SVG 内容 | 扫描关键词 `claude code`/`codex`/`opencode`/`gpt-5`/`glm`/`qwen`/`coding-qwen`/`stevenk`/`xag`/`参评`——**均未命中**（仅 `svg_check.py` 第 4 行的题面通用词"参评对象可见"出现，是公共题源自带，六份相同，不构成身份信号） |
| 路径/对话来源 | 盲评包路径均为 `P0x/q01-batch-engine-architecture/...`，无原 slug、无 `agents/`、无 `claude/codex/opencode` 会话痕迹 |

## 残余风险

- **代码/制图风格可能暴露身份**：产物型任务的 SVG 视觉风格（布局、配色、注释习惯）与 ANSWER.md 行文风格本身可能被推断出 Agent/模型，这无法完全脱敏，列为残余风险，不伪装成完全双盲。
- 6 个提交均用脚本生成 SVG（codex 系另两个手写 XML），生成脚本命名风格略有差异，但不直接含身份词。
