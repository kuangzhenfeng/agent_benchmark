# Q1：range.tc v6 长上下文校验器

`range.tc` 是一套运载火箭箭载测控协议，从 v1 演进到了 v6。本目录的 `corpus/` 是这套协议**完整的版本演进资料**（约 0.8M token）。请**仅修改 `src/range_validator.cpp`**，实现**最新版 v6** 的报文校验规则表，使其严格匹配 48 个"实现绑定"结构的 v6 真值。

## 为什么这题要读很多

v6 的正确值**并不全在 `corpus/protocol/v6.proto` 里**。这套协议的真实演进方式决定了：很多字段的最终值散落在 changelog、勘误表、字段词典甚至历史代码里，而且部分 `v6.proto` 注释、历史代码、历史测试给出的值**已经被后续文档推翻**。

要拿到正确值，你必须：
1. 读 `corpus/adr/ADR-009.md` 里**只出现一次**的文档取舍优先级规则；
2. 按该优先级，在 `v6.proto` / `changelog/` / `errata/` / `dict/` / `legacy-code/` 之间对每个字段交叉核对；
3. 识别并**抵抗**这些幻觉陷阱：
   - `v6.proto` 注释里写的就是对的（多数是，但部分字段被勘误表推翻）；
   - 历史代码 `validator_v2.cpp` / `validator_v4.cpp` 看起来权威，但含已知缺陷；
   - 字段词典 `field_dictionary_v4.md` 停留在 v4，部分字段标注"已废弃"实际被 changelog 重新启用；
   - 历史测试 `golden_v3.cpp` 的预期值是 v3 时代的，已不适用于 v6；
   - 单位在 v3 漂移（ms → 0.1ms）；枚举在 v5 重编号（飞行阶段编号不同）；
   - 近似变体名称（单/复数、`_view`/`_patch`/`_legacy`/`_v2`）值不同，易混用。

## 公开输入

- `corpus/`：完整演进语料（v1–v6 proto、5 份 changelog、12 份勘误、ADR、v4 词典、历史代码与测试、索引）。
- `include/range_validator.hpp`：不可修改的公开 API。
- `src/range_validator.cpp`：起始实现，规则表填的是**未读全语料的错误直觉值**，必须校正。

## 适配范围

48 个实现绑定结构（8 子系统族 × 6 变体），见 `corpus/index/structure_index.md`。每个结构 10 个字段维度：路由码、小版本、标识策略、回放域、是否需时间戳、是否需序号、模式位（6 bit 逐位）、飞行阶段编号、载荷分支、合法区间。

派生函数语义与公开 API 一致：`accepts` / `replayable` / `flag_active` / `phase_rank` / `branch_of` / `within_band`，且必须由规则表驱动。

## 约束与验收

- 只使用 C++17 标准库，不得引入 protobuf 等外部依赖；不得联网。
- 不得修改公开头文件、测试、`corpus/` 任何文件。
- 公开检查只抽样 8 个结构与 `flag_active`；评分会检查**全部 48 项**真值，并核对陷阱字段是否被正确取舍。
- **硬性封顶条件（该题总分不超过 60）**：仅据 `v6.proto` 或历史代码/词典的单一来源填写真值而未按 ADR-009 优先级交叉核对，导致被推翻字段错误；或修改了公开 API、测试或 corpus。

## 验证

```bash
./run_public_checks.sh
```

公开检查通过的，只代表抽样正确；评分会逐字段核对全部 480 个真值点。
