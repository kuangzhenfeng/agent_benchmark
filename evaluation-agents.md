# Harness 与模型登记表

参评对象由 **harness + 模型** 组成：harness 管理工具调用、上下文和执行流程；模型是实际推理后端。两者独立登记，供应商与 CLI 别名在组合表维护，避免把模型名称绑定到某个工具。

## Harness

| Harness ID | 名称 | 启动命令模板 |
|---|---|---|
| claude code | Claude Code | `claude -p <prompt> --model <model>` |
| codex | Codex CLI | `codex exec <prompt> -m <model> -s workspace-write` |
| opencode | OpenCode | `opencode run <prompt> -m <model>` |
| omp | oh-my-pi | `omp -p <prompt> --model <model>` |

`<model>` 使用下方组合表的 CLI 模型选择器，不直接从模型显示名称推断。omp 模板依据 [oh-my-pi 官方文档](https://github.com/can1357/oh-my-pi)；已登记组合沿用首轮配置；每轮启动前仍核对本机版本、供应商映射及权限。

## 模型

| 模型 ID / 显示名称 | CLI 模型选择器 |
|---|---|
| glm-5.2 | coding-glm |
| qwen3.8-max | coding-qwen-preview |
| gpt-5.5 | gpt-5.5 |
| deepseek-v4-flash | coding-deepseek |

模型登记不表示每个 harness 都已配置该模型，也不表示已验证本轮可用性。

## 已登记组合

此表是 `create_run.py` 默认 ALL（以及显式 `--all`）的读取范围：每行生成一个 `Harness ID + 模型 ID` 参评对象，不对两张登记表做笛卡尔积。当前登记 3 个 omp 组合。使用 `--agent 'codex + gpt-5.5'` 时，也可选择 Harness 与模型表中分别登记、但未加入 ALL 的组合。

| Harness ID | 模型 ID | CLI 模型选择器 |
|---|---|---|
| omp | glm-5.2 | `coding-glm` |
| omp | qwen3.8-max | `coding-qwen-preview` |
| omp | deepseek-v4-flash | `coding-deepseek` |

## 选择与命名

用户已经指定对象时沿用该选择；未指定时默认使用 ALL，不再询问。ALL 仅指“已登记组合”表中的全部对象，不扩展为 Harness 与模型的笛卡尔积。维护登记表或题库时不启动评测。

`--agent 'codex + gpt-5.5'` 可选择单个对象，`--agent` 可重复。Harness 或模型未登记时，先更新本文件；分发脚本只生成副本和启动命令，不自动执行。

分发脚本把显示名称转换为小写英文数字与短横线组成的 slug，拒绝空值和重名。显示名称保留在 participants.md，生成的作答 README 不带对象身份，并且各对象完全一致。同一组合需要不同配置时使用不同显示名称，在本轮记录准确映射。

启动前在本机核对 CLI 版本、模型别名和供应商映射，并记录到本轮 participants.md；不要未经记录替换模型。额外角色模型、回退模型、插件、推理设置和权限差异也须记录，以便区分 harness 与模型带来的影响。

## 统一启动提示词

每个对象都以自己目录内 `QUESTION.md` 的完整原文作为提示词，不添加实现建议、验收细节或模型专属提示。`create_run.py` 会在轮次目录生成可核对的 `launch.md`，例如：

```bash
cd benchmark/<run-id>/agents/omp-glm-5-2
omp -p "$(cat QUESTION.md)" --model coding-glm
```

每个对象使用独立工作目录，结果固定写入自己的 `showcase/index.html`。分发脚本不自动启动 Agent；只有用户明确要求开始评测时才执行 `launch.md` 中的命令。

## 结果汇总

全部对象完成后运行：

```bash
python3 .claude/skills/agent-benchmark/scripts/build_showcase.py --run <run-id>
```

脚本读取 `run.json` 中的参评顺序，将每个 `agents/<slug>/showcase/index.html` 内嵌到同一个 `showcase/index.html`，同时在轮次目录保存 `showcase.html`。各结果在 `sandbox="allow-scripts"` 的 iframe 中运行，互不修改；缺失结果会明确显示为“等待结果”。该流程只做并列展示，不评分、不排名。
