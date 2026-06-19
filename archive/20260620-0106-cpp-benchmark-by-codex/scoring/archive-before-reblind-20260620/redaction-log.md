# 脱敏记录

记录盲评包创建过程中对身份信息的处理。只记录脱敏的**位置和类型**，不记录参评身份。

## 处理方式

- **未复制含身份的 README.md**：每个 `agents/<slug>/README.md` 含参评对象名称与 slug，在创建 `blind-package/Pxx` 时一律**不复制**该文件。盲评包内每个 `Pxx` 只有题面、源码、测试、`ANSWER.md` 和 `run_public_checks.sh`，无任何 per-agent 指令文件。
- **路径无 slug**：盲评包目录为 `P01..P05`，其下为 `q01..q03`，不含原 `agents/<slug>` 路径片段。
- **`run_public_checks.sh` 相对路径**：脚本使用 `$(dirname "${BASH_SOURCE[0]}")` 取自身目录，不硬编码 `agents/` 绝对路径。
- **参考解答隔离**：`scorer-reference/cpp17-advanced-v1.md` 不含参评身份，且未放入 `blind-package/Pxx`。

## 残留扫描结果

对盲评包内全部 `.cpp/.hpp/.md/.sh` 执行关键词扫描（claude/qwen/glm/opencode/codex/gpt/anthropic 等及 slug 片段、通义/智谱等中文厂商名）：

- ✅ 无身份关键词残留
- ✅ 无 slug 路径残留

## 残余风险

- **代码风格本身可能隐含身份**（如命名习惯、注释语气、特定 idiom 偏好）。这类信号无法在不破坏技术完整性的前提下清除，列为残余风险；若 scorer 报告中因此产生倾向，会在最终报告中标注，不伪装为完全双盲。
- **作答时间起点不均**：qwen3.7max 两个对象先开始、用时可能更长；glm-5.2 / codex-gpt-5.5 后加入。这是分发时序造成的，不影响匿名性，但影响时间公平性，已记入 `evaluation.md` 风险章节。
