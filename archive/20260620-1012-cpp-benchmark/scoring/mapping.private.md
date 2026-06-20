# Mapping (Private) — 20260620-1012-cpp-benchmark

> ⚠️ 私有文件。只给主 agent 使用，**绝不**传给 scorer subagent，也不要复制到 blind-package。

匿名 ID 与参评对象的映射。顺序经过随机化，不暗示排名或身份分组。

| 匿名 ID | 参评对象（原样） | slug |
|---------|------------------|------|
| P01 | claude code + glm-5.2 | claude-code-glm-5-2 |
| P02 | codex + gpt-5.5 | codex-gpt-5-5 |
| P03 | claude code + qwen3.7max | claude-code-qwen3-7-max |
| P04 | opencode + glm-5.2 | opencode-glm-5-2 |
| P05 | opencode + qwen3.7max | opencode-qwen3-7-max |

## 盲评包构建方式

- 每个 `blind-package/Pxx/q01|q02|q03/` 含：公共 `QUESTION.md` + 该参评对象的 `include/`、`src/`、`tests/`、`run_public_checks.sh`、`ANSWER.md`。
- 路径不含原 slug，文件名仅保留技术含义。
- 已对身份信息扫描（agent 名、模型名、绝对路径中的 slug），结果见 `redaction-log.md`。

## 解盲时机

收到 scorer subagent 的匿名报告后，主 agent 才读取本文件做解盲汇总。主 agent 不重新打分。
