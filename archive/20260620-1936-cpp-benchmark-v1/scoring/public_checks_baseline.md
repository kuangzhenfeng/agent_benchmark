# 公开检查基线结果（主 agent 干净重跑，g++ 9.4.0，C++17，无并发）

> 脚本内各测试自带 `timeout 5s`。干净重跑两轮，下表为稳定判定（两轮一致为准；偶发项注明）。

| slug | Q1 subscription-hub | Q2 coalescing-cache | Q3 routing-config |
|------|---------------------|---------------------|-------------------|
| opencode-coding-glm | ✅ pass | ✅ pass | ✅ pass |
| opencode-coding-qwen-preview | ✅ pass | ✅ pass | ✅ pass |
| opencode-coding-deepseek | ✅ pass | ✅ pass | ✅ pass（轮1偶发 ASAN hang，轮2通过） |
| opencode-coding-minimax | ⏰ callable_lifetime 死锁(hang) | ⚠️ 偶发 ASAN fail134（数据竞争） | ✅ pass |
| opencode-coding-qwen | ⏰ callable_lifetime 死锁(hang) | ✅ pass | ⏰ observer_copy 死锁(hang) |
| opencode-coding-kimi | ⏰ callable_lifetime 死锁(hang) | ⏰ clock_reentrancy 死锁(hang) | ⏰ observer_copy 死锁(hang) |

## 失败性质

- `⏰ hang(124)` = 公开检查在内部 `timeout 5s` 后被杀，典型为**死锁**（mutex 重入 / 持锁调用用户 callable）。
- `❌ fail(134)` = ASAN 检测到数据竞争/信号后被杀（SIGABRT 134），典型为**数据竞争 / use-after-free**。
- `✅ pass` = 全部测试退出码 0。

> 公开检查通过不等于满分；公开检查失败是该题正确性的强负证据。此基线仅作客观事实，最终评分由 scorer subagent 基于 rubric 盲评。
