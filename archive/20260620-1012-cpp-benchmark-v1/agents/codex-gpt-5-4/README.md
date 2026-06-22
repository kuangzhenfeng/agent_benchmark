# Agent Benchmark Instructions

你只允许在**当前目录**（`agents/<slug>/`）内答题。

## 禁止

- 读取其他 `agents/<slug>` 目录
- 读取或参考 `submissions/` 中其他参评对象的结果
- 修改 benchmark 公共题源 `questions/`
- 读取 `scoring/`、`scoring-reference/` 或任何标注为「私有参考解答」的材料

## 要求

- 每题在对应的 `qXX-*` 目录内修改或新增文件
- 填写每题的 `ANSWER.md`（修改说明、验证方式、风险）
- 记录你执行过的验证方式；如未验证，说明原因
- 每题有时间盒：Q1 `subscription-hub` 50 分钟，Q2 `coalescing-cache` 70 分钟，Q3 `routing-config` 60 分钟
- 使用 C++17（编译器 `g++ 9.4.0`，`-std=c++17`），可用 `clang++ 10.0.0` 交叉验证
- 完成后停止，不要替其他 Agent 评分

## 题目清单

| 题号 | 目录 | 类型 | 时间盒 |
|------|------|------|--------|
| Q1 | `q01-subscription-hub/` | 组合 Bugfix | 50 分钟 |
| Q2 | `q02-coalescing-cache/` | Implementation | 70 分钟 |
| Q3 | `q03-routing-config/` | Refactor/Design | 60 分钟 |

每题目录含：`QUESTION.md`（题面与验收标准）、starter code（`include/`、`src/`）、`tests/`（公开检查）、`run_public_checks.sh`（公开验证脚本）、`ANSWER.md`（作答模板）。

## 公开检查的初始状态（非预评分，仅说明覆盖范围）

- Q1：基础路径可通过；完整公开套件含 callable 生命周期/重入/批次边界检查，starter 状态预期失败。
- Q2：starter 为未实现骨架，完整公开套件预期失败。
- Q3：基础路径可通过；完整公开套件含跨实例 `current()` 生命周期、observer 复制重入检查，starter 状态预期失败。

> Q1 的 callable 生命周期锁边界是该预设中唯一标记为「硬性封顶条件」的语义。

请先通读三道 `QUESTION.md`，再开始作答。

## 本目录作答状态（2026-06-22）

- Q1 `subscription-hub`：已完成；修复自有存储、批次快照、回调锁边界、异常隔离和 `uint32_t` 过期语义，补充 `tests/reentrancy_batch_checks.cpp`。
- Q2 `coalescing-cache`：已完成；实现 in-flight 合并、同 key 递归保护、TTL/LRU、`invalidate` 禁止回写，补充 `tests/recursive_invalidate_checks.cpp`。
- Q3 `routing-config`：已完成；重构为不可变快照发布、按实例 TLS `current()` 保活、观察者锁外通知，补充 `tests/reload_observer_lifecycle_checks.cpp`。
- 验证：三题均执行了各自 `./run_public_checks.sh`，并手工编译运行新增附加测试。
