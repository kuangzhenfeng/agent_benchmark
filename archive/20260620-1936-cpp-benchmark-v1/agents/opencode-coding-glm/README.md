# Agent Benchmark 作答说明

你只允许在当前目录内答题。

## 禁止

- 读取其他 `agents/<slug>` 目录
- 读取或参考 `submissions/` 中其他参评对象结果
- 修改 benchmark 公共题源 `questions/`
- 联网查资料

## 要求

- 每题在对应 `qXX-*` 目录内修改或新增文件（仅改允许的源码与 `ANSWER.md`）
- 填写每题 `ANSWER.md`：说明你的修改、验证方式、已知风险
- 记录你执行过的验证方式（如编译命令、测试输出）；如未验证，说明原因
- 完成后停止，不要替其他 Agent 评分

## 本轮信息

- 预设：`benchmark-v1，共 3 题（Q1 subscription-hub / Q2 coalescing-cache / Q3 routing-config）`
- 语言：C++17；各题用 `run_public_checks.sh` 验证
- 完成后停止，等待评分通知
