# cpp17-advanced-v2

这是面向长上下文与协议适配的 C++17 基准预设。协议为全新虚构的航空航天测控物模型（proto3，仿物模型工程组织但与 tsl 企业协议无内容重叠）：运载火箭箭载测控协议 `range.tc` 及其目标系统 `orbit.tc`。

| 题目 | 类型 | 推荐时间盒 | 协议工件 | 语料体量 | 评测重点 |
|------|------|------------|----------|----------|----------|
| q01-long-context-protocol | 长上下文实现 | 300 分钟 | corpus/（v1–v6 物模型 + changelog + 勘误 + ADR + v4 词典 + 历史代码/测试） | ~0.8M token | 纵向历史长程记忆、跨距离交叉引用、文档优先级取舍、幻觉抵抗 |
| q02-protocol-adaptation | 协议适配 | 300 分钟 | 系统A/系统B（两套 proto 物模型各 ~348 结构）+ 变更映射.json | 各 ~5,700 行 | 横向结构逐字段迁移、近似命名辨别、enum 重编号、bit flag、范围控制 |

两题共用同一协议家族，但维度正交：**Q1 考纵向版本演进的历史长程记忆，Q2 考横向相似系统的迁移精度**。

## 公开性与边界

- 所有业务规则都在题面、`corpus/` 或 `系统A/系统B/变更映射.json` 中公开；评分不得使用未公开的私人条件。
- Q1 的 ~0.8M 语料由确定性生成器（`_generator/`，固定种子）产出，可完全复现；生成器同时输出私有 ground truth，保证语料与评分参考自洽（全部 480 个真值点均可从 corpus 推导，已自动审计 100%）。Agent 通常会对超长语料自动压缩上下文，这正是本题要检验的长程记忆衰减点，不作为限制单列。
- Q1 的文档取舍优先级（勘误表 > changelog > v6.proto 注释 > 词典 > 历史代码）只在 `corpus/adr/ADR-009.md` 出现一次，是跨距离依赖的核心，属公开合同。
- 本预设不保证任何模型或人类必然失败或通过。能力结论只能来自同题、隔离作答与匿名盲评。

## 重新生成题源

题源由生成器产出。若需复现或微调（如调整语料体量、绑定结构数、陷阱分布），运行：

```bash
cd _generator
python3 model.py            # 共享模型自检
python3 gen_q02_proto.py .. # 生成 Q02 系统A/系统B + 变更映射 + truth
python3 gen_q02_cpp.py ..   # 生成 Q02 C++ 头/starter/公开检查
python3 gen_q01_main.py ..  # 生成 Q01 corpus + C++ 头/starter/公开检查 + truth
python3 verify_q02.py ..    # 验证 Q02 正确解通过公开检查
```

修改 `gen_q01_main.py` 的 `structures_per_version` 可调整 Q1 语料体量（默认 ~0.8M token）。

## 使用规则

- 用户明确选择 cpp17-advanced-v2 时，完整复制两个题目目录到本轮 questions，再复制到每个 agents 目录。
- corpus、QUESTION.md、ANSWER.md、起始代码与 tests 目录都是公开题源；所有参评对象必须拿到字节等价副本。
- 生成与作答阶段绝不能复制私有评分参考（`_private/*_truth.json`、`scoring-reference/cpp17-advanced-v2.md`）到 questions、agents 或 submissions。
- 全部参评对象停止作答后，才把对应私有评分参考复制到本轮 scorer-reference。
