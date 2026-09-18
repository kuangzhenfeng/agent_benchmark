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

此表是 `create_run.py --all` 的读取范围：每行生成一个 `Harness ID + 模型 ID` 参评对象，不对两张登记表做笛卡尔积。当前登记 3 个 omp 组合，其他 harness 可按已确认配置另行添加。

| Harness ID | 模型 ID | CLI 模型选择器 |
|---|---|---|
| omp | glm-5.2 | `coding-glm` |
| omp | qwen3.8-max | `coding-qwen-preview` |
| omp | deepseek-v4-flash | `coding-deepseek` |

## 选择与命名

用户已经指定对象时沿用该选择；未指定时按 harness + 模型组合询问多选，并提供 ALL 建议选项。ALL 仅指已登记组合，不自动启动。维护登记表或题库时不询问参评对象。

`--agent 'codex + gpt-5.5'` 仍可选择单个对象；也保留自定义组合名称入口，自定义对象须在本轮 participants.md 补全 harness、模型和实际命令。分发脚本只生成副本，不解析或执行命令模板。

分发脚本把显示名称转换为小写英文数字与短横线组成的 slug，拒绝空值和重名。显示名称保留在 participants.md，生成的作答 README 不带对象身份，并且各对象完全一致。同一组合需要不同配置时使用不同显示名称，在本轮记录准确映射。

启动前在本机核对 CLI 版本、模型别名和供应商映射，并记录到本轮 participants.md；不要未经记录替换模型。额外角色模型、回退模型、插件、推理设置和权限差异也须记录，以便区分 harness 与模型带来的影响。

## 统一启动提示词

将 `<prompt>` 作为一个完整命令参数传入，内容为：

```text
请按当前题目 QUESTION.md 和组织者提供的统一规则完成本题作答，填写 ANSWER.md；完成后停止。
```

详细权限、时间盒和提交规则来自分发后的 README.md，其源模板为 [agent-readme.md](.claude/skills/agent-benchmark/templates/agent-readme.md)。所有对象使用同一提示词，不追加针对某个模型的根因提示。

当前题库命令在对应对象当前单题的独立工作区执行，每题全新会话，不共享其他等级源码和上下文。工作目录本身不限制读取上级文件；实际环境须只提供该对象的题目副本，并把工具、网络、系统和编译器差异记入 participants.md。分发脚本只复制文件；自动执行必须属于用户已要求的范围。详细操作见 [分发与运行](.claude/skills/agent-benchmark/references/running.md)。
