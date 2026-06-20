# Q2 作答说明

## 修改范围

仅修改了 `src/protocol_adapter.cpp` 中的规则表 `kRules`，将系统 A（range.tc）的迁移前值替换为系统 B（orbit.tc）的最终真值。

## 数据来源

根据 `系统B/` 目录中的 proto 注释中的"适配契约元数据"逐结构独立读取，48 个实现绑定结构散布在 348 个结构中，通过查找"是否实现绑定: 是"标注定位。

关键注意事项：
- 系统 B 阶段编号：IDLE=0, STANDBY=1, BOOST=2, ASCENT=3, RESERVED=4, COAST=5, INSERTION=6
- 系统 A 与系统 B 的阶段编号不同（系统 A: coast=4, insertion=5；系统 B: coast=5, insertion=6, reserved=4）
- 不根据系统 A 的值、结构名称、子系统族或相邻位置推断系统 B 值
- 不对标识、回放令牌或样本值增加 trim、哈希或单位换算

## 验证方式

运行公开检查：
```
g++ -std=c++17 -Iinclude src/protocol_adapter.cpp tests/public_checks.cpp -o /tmp/q2_check && /tmp/q2_check
```
输出：PASSED

## 已知风险

- 48 个结构的字段值均从系统 B proto 注释逐字段提取，但评分会验证全部真值点