# range.tc 协议演进语料

本目录是 range.tc 测控协议从 v1 到 v6 的完整演进资料。任务是实现 v6 的报文校验器，正确值需要结合本目录下多份文档交叉核对。

## 阅读顺序建议

1. `index/structure_index.md`：结构清单与目录导航。
2. `adr/ADR-009.md`：文档取舍优先级（重要）。
3. `protocol/v6.proto`：最新物模型（注释不全，需配合 changelog/勘误）。
4. `changelog/`、`errata/`、`dict/`：补充与订正。
5. `legacy-code/`、`legacy-tests/`：历史实现与测试（已知过时，谨慎参考）。

> 提示：不要只读 v6.proto。许多字段的真值在 changelog 或勘误表中，且部分 proto 注释与历史代码的值已被推翻。
