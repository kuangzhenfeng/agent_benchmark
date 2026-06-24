# 脱敏日志（v1）

记录匿名包创建时的脱敏操作。只写脱敏位置与类型，不写参评身份。

| 位置 | 类型 | 说明 |
|------|------|------|
| 目录名 `agents/<slug>/` → `blind-package/P0X/` | 身份脱敏 | 用 P01..P06 替代原 slug 目录名 |
| 脱敏扫描 | 残留检查 | grep 所有 Pxx 内的 `opencode-coding-*` / `coding-deepseek` / `agents/opencode` 等身份串，无残留 |
| 评分参考 `scorer-reference/benchmark-v1.md` | 来源隔离 | 仅从 skill 原始文件复制，不含任何参评提交或身份 |

## 残余风险

- 代码风格本身可能隐含身份，但本轮 6 个对象均为 opencode + 同类 coding 模型，本体一致，风格差异主要来自模型，不构成可识别身份信号。
- 日志（`_logs/`）未进入盲评包，scorer 看不到。
