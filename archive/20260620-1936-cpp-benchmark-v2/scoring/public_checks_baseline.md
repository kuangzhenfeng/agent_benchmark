# 公开检查基线结果（主 agent 运行，g++ 9.4.0，C++17）

| slug | Q1 long-context-protocol | Q2 protocol-adaptation | src 改动 | 说明 |
|------|--------------------------|------------------------|----------|------|
| opencode-coding-deepseek | ✅ pass (0) | ✅ pass (0) | 两题已改 | 全过 |
| opencode-coding-glm | ✅ pass (0) | ✅ pass (0) | 两题已改 | 全过 |
| opencode-coding-qwen-preview | ✅ pass (0) | ✅ pass (0) | 两题已改 | 全过 |
| opencode-coding-qwen | ❌ fail(134 断言) | ✅ pass (0) | Q1 未改/Q2 已改 | Q1 留 starter 值→断言失败；Q2 已改且通过 |
| opencode-coding-minimax | ❌ fail(134 断言) | ❌ fail(134 断言) | 两题均未改 | 两题均留 starter 值→断言失败；ANSWER 为空模板 |
| opencode-coding-kimi | ❌ fail(134 断言) | ❌ fail(134 断言) | 两题均未改 | 上下文超限(EXIT=1)未完成；两题留 starter 值→断言失败；ANSWER 为空模板 |

## 失败性质

- `❌ fail(134)` = 公开检查断言失败（starter 填的是未读全语料的错误直觉值/迁移前值，预期失败）。
- minimax/qwen(Q1)/kimi **未修改 src**（与 starter 字节一致），等于没有作答核心代码 → 按硬性扣分「未提交核心代码」该题 ≤30。
- qwen 的 Q1 未改、Q2 已改且通过。
- `✅ pass` = 公开检查退出码 0（仅抽样 8 结构，通过不等于真值 100%）。

> 公开检查通过不等于满分；最终正确性由 scorer 用 ground truth 逐字段核对。此基线仅作客观事实。
