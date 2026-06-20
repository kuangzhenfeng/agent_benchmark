# cpp17-advanced-v2 私有评分参考

仅在全部参评对象停止作答后复制给匿名评分员。不得进入公共题源、参评目录或作答前交付物。

本文件是公开业务协议的语义校准与验收矩阵，不是唯一源码模板。只要行为等价、资源安全且遵守题面，必须接受不同的数据结构与并发设计。

## Q1：结算账本协议缺陷修复

- 订单键为 merchant_id 与 order_id；成功回放键为 merchant_id 与 message_id；长期文本必须拥有副本。
- apply 顺序为：merchant/message 可查性、成功回放、新命令 order 校验、首条或已有订单、精确版本、撤销终态、命令金额规则。
- 被拒绝的新命令不保存回放；成功命令才同时提交订单状态与回放记录。撤销订单的旧版本先返回 revision_gap，正确下一版本才返回 already_voided。
- AuditRecord 在解锁前构造为拥有文本的值；AuditSink 的包装、替换、调用和可能的最后析构均在锁外执行，审计异常被捕获。

| 组合场景 | 正确结果 | 常见错误 |
|----------|----------|----------|
| 相同消息标识，不同商户 | 两个独立成功命令 | 把消息标识当全局键 |
| 成功消息以篡改负载重发 | duplicate 且复用首次回执 | 比较负载或返回当前订单 |
| 失败后用相同消息标识重试 | 后续可以成功 | 记忆失败结果 |
| 临时字符串后查询 | 原订单仍完整 | 保存 string_view |
| 审计查询账本或抛异常 | 不死锁，不回滚提交 | 持锁回调或异常逃逸 |

## Q2：权益分配协议实现

- 内部至少维护活动剩余预算、活动客户累计、成功单条回放和批次回放；键必须拥有文本。
- 单条先查成功回放，再验证客户、金额、活动和半开时间窗口。双额度不足时 customer_limit 优先。
- 单条失败不回放。非空 batch_id 的批次在任何行校验前查历史批次；成功和拒绝批次都回放。
- 新批次在临时账本按输入顺序模拟。历史成功 claim 为 batch_conflict；仅无历史成功时才检查当前批次重复 claim。
- 成功批次一次性提交预算、客户累计、所有成功 claim 和批次回放；审计在锁外按输入顺序执行。

| 组合场景 | 正确结果 | 常见错误 |
|----------|----------|----------|
| ends_at 时领取 | outside_window | 把结束点作为闭区间 |
| 客户与活动均不足 | customer_limit | 固定选择活动余额不足 |
| 窗口外单条重试 | 可成功 | 缓存所有失败 |
| 批次后一行失败 | 完全回滚且保存拒绝批次 | 前序行已经提交 |
| 重放拒绝批次并篡改行 | duplicate 与历史回复 | 重新评估行内容 |

## Q3：协议一变更向协议二适配

- 只能修改 src/protocol_adaptation.cpp。公开头文件、测试、协议工件与非绑定结构均不可修改。
- 协议一只提供迁移动机与定位；下表是协议二二十四张实现绑定卡片的最终真值。
- accepts 与 replayable 必须继续由规则表驱动；不得以名称、位置或同族关系生成默认规则。

| # | 结构代号 | 线标记 | 修订号 | 标识策略 | 回放域 | 需要源时间 | 需要序号 |
|---|----------|--------|--------|----------|--------|------------|----------|
| 1 | settlement_record | 601 | 2 | 保留空值 | 账户内回放 | true | true |
| 2 | settlement_records | 618 | 4 | 必填 | 无回放 | false | true |
| 3 | settlement_record_view | 635 | 6 | 空值生成 | 批次内回放 | false | false |
| 4 | settlement_record_patch | 652 | 3 | 保留空值 | 商户内回放 | true | false |
| 5 | settlement_receipt | 669 | 5 | 必填 | 账户内回放 | false | false |
| 6 | settlement_receipts | 686 | 2 | 空值生成 | 无回放 | false | true |
| 7 | settlement_receipt_view | 703 | 4 | 保留空值 | 批次内回放 | true | true |
| 8 | settlement_receipt_patch | 720 | 6 | 必填 | 商户内回放 | true | false |
| 9 | settlement_reservation | 737 | 3 | 空值生成 | 账户内回放 | false | false |
| 10 | settlement_reservations | 754 | 5 | 保留空值 | 无回放 | true | true |
| 11 | settlement_reservation_view | 771 | 2 | 必填 | 批次内回放 | false | false |
| 12 | settlement_reservation_patch | 788 | 4 | 空值生成 | 商户内回放 | false | false |
| 13 | settlement_reversal | 805 | 6 | 保留空值 | 账户内回放 | true | true |
| 14 | settlement_reversals | 822 | 3 | 必填 | 无回放 | false | true |
| 15 | settlement_reversal_view | 839 | 5 | 空值生成 | 批次内回放 | true | false |
| 16 | settlement_reversal_patch | 856 | 2 | 保留空值 | 商户内回放 | true | false |
| 17 | settlement_adjustment | 873 | 4 | 必填 | 账户内回放 | false | false |
| 18 | settlement_adjustments | 607 | 6 | 空值生成 | 无回放 | false | true |
| 19 | settlement_adjustment_view | 624 | 3 | 保留空值 | 批次内回放 | true | true |
| 20 | settlement_adjustment_patch | 641 | 5 | 必填 | 商户内回放 | false | false |
| 21 | settlement_attachment | 658 | 2 | 空值生成 | 账户内回放 | false | false |
| 22 | settlement_attachments | 675 | 4 | 保留空值 | 无回放 | true | true |
| 23 | settlement_attachment_view | 692 | 6 | 必填 | 批次内回放 | false | false |
| 24 | settlement_attachment_patch | 709 | 3 | 空值生成 | 商户内回放 | false | false |

| 审查点 | 正确判断 |
|--------|----------|
| 仅公开检查样本通过 | 仍须检查全部二十四项 |
| 把协议一数值复制到协议二 | 违反目标协议 |
| 为非绑定上下游卡片新增规则 | 违反范围约束 |
| 空值生成与回放域混为一谈 | 违反独立字段语义 |

## 通用评分使用规则

- 先以 QUESTION.md 与结构化业务协议为准；本文件只帮助识别看似合理但违约的实现。
- 无法从源码和验证记录确认的并发性质应标注为未验证风险，不得臆测通过。
- 不得以“现实系统通常会有”的额外策略扣分，例如负载哈希、时钟单调校验或全局去重。
- 本参考不得出现在公开题源、参评目录、作答前交付或面向参评者的最终报告中。
