# Agent Benchmark 评测报告

## 总结

本轮 **benchmark-v3**（产物型架构图绘制，1 题）评测 6 个 Agent+模型组合。**codex 框架的两版 GPT 明显领先**：codex + gpt-5.4（80.0）与 codex + gpt-5.5（79.5）居前两位，且命名规范度最高；第二梯队 claude code（glm-5.2 / qwen3.7max，均 74.0）与 opencode + glm-5.2（73.5）几乎打平；opencode + qwen3.7max 末位（64.5）。结论置信度较高：双 scorer 最大分差仅 3 分（阈值 8），排名稳定。注意本轮是单题产物型任务，**不应外推**到其他题型或编码能力整体。

## 盲评流程

- 匿名 ID：P01–P06（人工分配，与能力无关）
- 评分员：2 个独立 scorer subagent（A、B）
- scorer 是否看到 Agent/模型身份：**否**
- 最大总分差：3 分（阈值 8）→ 无需匿名复核，取平均裁决
- 正确性维度客观主轴：`grade_svg.py` 产出的 `machine-grade.json`（5 视角 `data-*` 标注与私有 ground truth 的集合覆盖率）
- 未完全匿名信息：产物型 SVG 视觉/行文风格可能被推断身份，列为残余风险（见 redaction-log.md）

## 总分（裁决 = 双 scorer 平均）

| 排名 | 参评对象 | 正确/45 | SVG/20 | 约束/15 | 维护/10 | 验证/10 | 总分/100 | 结论 |
|------|----------|---------|--------|---------|---------|---------|----------|------|
| 1 | codex + gpt-5.4 | 31.2 | 17.0 | 15 | 8.0 | 9.0 | **80.0** | component 命名最规范，正确性最高 |
| 2 | codex + gpt-5.5 | 30.6 | 17.0 | 15 | 8.0 | 9.0 | **79.5** | dataflow 含完整重试环，视角均衡 |
| 3 | claude code + glm-5.2 | 24.8 | 16.5 | 15 | 9.0 | 8.5 | **74.0** | 陷阱处理最细致，生成脚本可复现 |
| 3 | claude code + qwen3.7-max | 28.2 | 15.0 | 15 | 8.5 | 7.5 | **74.0** | 唯一命中接口依赖陷阱，类图最规范 |
| 5 | opencode + glm-5.2 | 26.0 | 15.5 | 15 | 9.0 | 8.0 | **73.5** | 类图边种齐全，ANSWER 表格化清晰 |
| 6 | opencode + qwen3.7-max | 21.0 | 12.5 | 15 | 8.0 | 8.0 | **64.5** | layered 最全，组件图用 "-module" 粒度失分 |

匿名映射（解盲）：P01=claude code+glm-5.2、P02=codex+gpt-5.5、P03=opencode+qwen3.7-max、P04=claude code+qwen3.7-max、P05=codex+gpt-5.4、P06=opencode+glm-5.2。

## 机器分（正确性客观依据，coverage_score/45）

| 匿名 | 参评对象 | coverage/45 | layered | component | dataflow | class | deployment | interface_deps |
|------|----------|-------------|---------|-----------|----------|-------|------------|----------------|
| P05 | codex + gpt-5.4 | **30.39** | 0.653 | **0.821** | 0.603 | 0.670 | 0.298 | 否 |
| P02 | codex + gpt-5.5 | 30.24 | 0.663 | 0.763 | **0.649** | 0.657 | 0.308 | 否 |
| P04 | claude code + qwen3.7-max | 27.42 | 0.768 | 0.588 | 0.464 | **0.712** | 0.029 | **是** |
| P06 | opencode + glm-5.2 | 25.08 | 0.654 | 0.605 | 0.286 | 0.678 | 0.236 | 否 |
| P01 | claude code + glm-5.2 | 23.68 | 0.671 | 0.684 | 0.114 | 0.626 | 0.208 | 否 |
| P03 | opencode + qwen3.7-max | 19.94 | **0.795** | 0.167 | 0.253 | 0.521 | 0.253 | 否 |

## 分题评分（q01 batch-engine-architecture）

见上表总分与机器分。所有提交约束遵循均为满分 15（5 风格齐全、XML 合法、未改源码、无联网、无硬性封顶触发），区分度集中在正确性（架构事实覆盖率）与 SVG 产物质量（命名规范度）。

### 关键事实核验（两 scorer 共识）

- **6 份 `include/`+`src/`+`svg_check.py` 逐字节一致**，均未篡改引擎源码/题面/检查脚本。
- **6 份 `svg_check.py` 全部通过**，节点/边计数与 ANSWER 声称逐一吻合，无编造。
- **6 份陷阱处理整体正确**：伪循环依赖（Worker→Scheduler 标 callback，对应 worker.hpp 不 include scheduler.hpp）、死代码（TaskQueueLegacy/run_legacy/PersistentTaskStore/FixedBackoffPolicy 归侧栏）、分层误导（MetricsCollector 归 infra 叶子，不因注释自称"核心"归编排层）。

## 关键差异

- **命名规范度是主要拉分点**：codex+GPT 两版（P05/P02）的 SVG 节点 `data-id` 贴合源码英文类标识符（`Scheduler`/`ITaskStore`/`WorkerPool`），在 component/dataflow 视角获机器最高覆盖率（0.821/0.649）。P03 用 `*-module` 粒度、P04 用 `*_inst` 后缀、P01/P06 用方法步骤名/小写模块名，在对应视角被机器惩罚，即便架构理解本身正确。
- **claude code+qwen3.7-max（P04）是唯一命中"接口依赖"陷阱的提交**：class 图正确把 Scheduler 依赖画到 ITaskStore/IRetryPolicy/INotifier 抽象接口而非具体类（机器 interface_deps 标志唯一为真，+3 trap bonus），class 视角覆盖率最高（0.712）；但 deployment 视角因 `*_inst` 命名几乎归零（0.029），拖累总分。
- **codex 框架 gpt-5.4 vs gpt-5.5 几乎打平**（80.0 vs 79.5），与 v1/v2 历史轮次一致：单题单轮样本下两 GPT 版本能力接近，无法判出版本优劣。
- **框架维度**：codex（79.75 均分）> claude code（74.0）≈ opencode（glm 73.5 / qwen 64.5）；但 opencode+qwen3.7max 末位主要因命名粒度问题，非架构理解缺失（其 layered 视角覆盖率 0.795 全场最高）。

## 风险与限制

- **样本量**：N=6，单题（产物型），单次作答。不应外推到改代码类任务、多语言或整体编码能力。
- **正确性高度依赖机器标注核对**：`grade_svg.py` 只核对 `data-*` 标注的架构事实，**不评价视觉质量**；命名不规范的提交会被机器惩罚，scorer A 对"画对了但词汇不匹配"做了有限上调、scorer B 严格不调，裁决取平均以兼顾——这引入了主观性，已如实记录。
- **未运行真实渲染**：SVG 仅做 XML 合法性 + 标注约定检查（svg_check.py），未在浏览器实际渲染验证视觉效果；视觉质量由 scorer 主观给分（20 分维度）。
- **残余身份风险**：产物型任务的 SVG 视觉风格、ANSWER 行文习惯可能被推断出 Agent/模型，无法完全脱敏，未伪装成完全双盲。
- **时间盒非硬性**：6 份均在推荐 120–150 分钟内（实际 ~15 分钟并行完成），但非强制同时间盒。

## 下一轮建议

- 本轮产物型任务下 codex+GPT 两版稳定居首。建议补一轮 **v1（并发/所有权工程实践）或 v2（长上下文/协议适配）**，验证 codex+GPT 的领先是否在改代码类任务上同样成立、claude code 与 opencode 能否反超。
- 若要继续压测 v3，可考虑：①增加"命名规范度"为单独公开验收项，减少机器评分对非标准命名的惩罚；②要求提交 SVG 在浏览器渲染截图作为验证证据，强化验证维度。

---

> 本报告为单轮结论。匿名评分报告详见 `scoring/scorer-report.md`，机器分详见 `scoring/machine-grade.json`，脱敏记录见 `scoring/redaction-log.md`，映射见 `scoring/mapping.private.md`。
