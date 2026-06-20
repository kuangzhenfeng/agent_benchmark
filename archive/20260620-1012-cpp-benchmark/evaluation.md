# Agent Benchmark Evaluation — 20260620-1012-cpp-benchmark

## 总结

本轮 5 个 AI Coding Agent×模型组合在 `cpp17-advanced-v1`（C++17，3 道高级工程题）上评测。头部三名（opencode+glm-5.2、codex+gpt-5.5、claude code+glm-5.2）总分差 ≤ 8，统计上未拉开显著差距；但**模型维度上 GLM-5.2 明显领先 Qwen3.7max**——两个 GLM 作答（均分 278）均显著高于两个 Qwen 作答（均分 241），平均差 37 分。GPT-5.5 单作答（280）与 GLM-5.2 头部接近。Agent 框架与模型存在交互效应，本轮无法分离。置信度受限于样本量（每模型仅 2 个作答）与全程缺 TSan。

## 盲评流程

- 匿名 ID：P01–P05（映射顺序随机化，不暗示身份/分组）
- 评分员：两个独立 scorer subagent（A、B），互不可见
- scorer 是否看到 Agent/模型身份：**否**（仅读 blind-package + scorer-reference）
- 主 agent 在解盲前对两位评分员的**关键分歧项**独立编译探针复核（复核记录见下文「分歧裁决」），复核基于匿名 ID，复核完成后再解盲
- 未完全匿名的信息：代码风格、自写测试命名（`extra_boundary_checks`/`edge_semantics_checks`/`extra_checks`）客观存在差异，属无法在不篡改技术内容前提下消除的残余风险（见 `scoring/redaction-log.md`）

### 分歧裁决（主 agent 复核，匿名 ID 阶段）

| 分歧项 | scorer A | scorer B | 主 agent 复核 | 采纳 |
|--------|----------|----------|---------------|------|
| P01 Q1 取消语义（cancelled 是否置位） | 96（判正确） | 82（判缺陷） | 源码确认 unsubscribe 从不置 cancelled（src:41-58）；探针实测回调内自取消 `delivered=3` 收到 a/b/c | **B**（82） |
| P02 Q1 析构重入崩溃 | 92（coredump 扣 5） | 93（无重大） | 探针实测析构期重入 subscribe → mutex owner 断言 core（exit=134），ASan 实锤 heap-use-after-free | **A**（92） |
| P05 Q2 跨实例递归检测 | 84（未明确判跨实例） | 74（误返 recursive_load） | 干净探针：a.loader 内对另一实例 b 同 key 调用，仅 P05 误返 recursive_load，其余 4 个正确 | **B**（74） |
| P03 Q2 完成时刻 Clock 异常逃逸 | 82（异常逃逸+永久阻塞） | 90（双锁偏离但同步正确） | 源码确认 `state_->now()`（src:122）无 try/catch，owner 异常逃逸且 flight 永不 done | **A**（82） |
| 其余一致项 | — | — | 两位一致，取合理值 | 取均/整数 |

## 总分

| 排名 | 参评对象 | Q1/100 | Q2/100 | Q3/100 | 总分/300 | 平均/100 | 结论 |
|------|----------|--------|--------|--------|----------|----------|------|
| 1 | opencode + glm-5.2 | 94 | 94 | 94 | **282** | 94.0 | 头部，无明显短板 |
| 2 | codex + gpt-5.5 | 92 | 94 | 94 | **280** | 93.3 | 头部，Q1 析构重入路径有 UAF 风险 |
| 3 | claude code + glm-5.2 | 82 | 96 | 96 | **274** | 91.3 | Q2/Q3 最佳，Q1 取消语义有伪修复 |
| 4 | claude code + qwen3.7max | 87 | 82 | 90 | **259** | 86.3 | 中游，Q2 Clock 异常逃逸 + 无自写测试 |
| 5 | opencode + qwen3.7max | 59 | 74 | 90 | **223** | 74.3 | Q1 触发硬性封顶，Q2 跨实例递归误判 |

> 单题/总分均采用解盲裁决后的分数（见上「分歧裁决」）。原始两位 scorer 报告原样归档于 `scoring/scorer-report-A.md`、`scoring/scorer-report-B.md`。

## 分题评分

### Q1 SubscriptionHub（组合 Bugfix，唯一含「硬性封顶条件」的题）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|-----------|------------|---------|-----------|---------|----------|----------|
| opencode + glm-5.2 | 42 | 19 | 15 | 9 | 9 | **94** | shared_ptr+atomic alive，锁外回调，逐事件查 alive，取消语义探针 delivered=1 正确；析构锁外；手写双指针分区正确 |
| codex + gpt-5.5 | 40 | 18 | 15 | 9 | 10 | **92** | shared_ptr<Callback>+map，drain 锁外回调，subscription_cutoff 冻结新订阅；**hub 析构期 callable 析构重入 subscribe → map UAF/coredump（探针实测复现）** |
| claude code + qwen3.7max | 40 | 17 | 15 | 8 | 7 | **87** | shared_ptr<Callback>，逐事件 any_of 重查；O(n²) 但功能对；**全程无自写测试**；锁内 make_shared 移动（措辞违规、探针不死锁） |
| claude code + glm-5.2 | 36 | 18 | 15 | 9 | 8 | **82** | 整体结构好（shared_ptr+atomic cancelled）；**unsubscribe 从不置 cancelled=true → 回调内自取消仍投递后续事件（探针 delivered=3），ANSWER 自述与实现不符（伪修复），自写测试还 assert 错误行为** |
| opencode + qwen3.7max | 20 | 12 | 8 | 7 | 6 | **59** | **Subscription 按值持有 Callback，drain 锁内拷贝整个 vector 即拷贝每个 std::function；探针实测 callable 拷贝重入确定性死锁（EXIT=124）→ 触发 Q1 硬性封顶（≤60）；取较低者 59** |

**Q1 分水岭**：是否用 `shared_ptr` 托管 callable、drain 锁内只动控制块。4/5 做到，唯独 opencode+qwen3.7max 按值持有并锁内拷贝，触发硬性封顶。

### Q2 CoalescingCache（Implementation）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|-----------|------------|---------|-----------|---------|----------|----------|
| claude code + glm-5.2 | 43 | 19 | 15 | 9 | 10 | **96** | 单 mutex 覆盖 ready+in-flight+done/result（严格单一同步关系）；owner 一次临界区原子发布；flight->owner 检测递归；invalidate 不取消 loader；16 线程合并 loader=1；自写 11 例边界 + ASan |
| codex + gpt-5.5 | 43 | 19 | 15 | 9 | 8 | **94** | 单 mutex；thread_local (state,key) 按实例检测递归（跨实例嵌套正确 ok）；ActiveCallGuard RAII 清理；完成 Clock 异常合理降级 failed |
| opencode + glm-5.2 | 42 | 19 | 15 | 9 | 9 | **94** | 单 mutex+单 cv，严格无第二把 mutex（注释明示）；thread_local vector<Flight*> 按 flight 检测递归；32 线程压测 loader=1 |
| claude code + qwen3.7max | 37 | 17 | 15 | 8 | 5 | **82** | 双 mutex（state+flight）；合并/失效/TTL 功能探针通过；**完成时刻 state_->now()（src:122）无 try/catch，Clock 异常逃逸 get_or_load 且 flight 永不 done → 等待者永久阻塞（探针实测 EXCEPTION ESCAPED）；全程无自写测试** |
| opencode + qwen3.7max | 30 | 15 | 13 | 8 | 8 | **74** | 基本状态机在；**递归检测用全局 thread_local unordered_set<string>（只按 key 不按实例），探针实测：a.loader 内对另一实例 b 同 key 调用误返 recursive_load（参考解答明示的伪修复模式）；ready 过期不清理致 stale 滞留；should_cache 漏判 capacity>0；双 mutex 三段式** |

**Q2 分水岭**：(1) 递归检测是否按 cache 实例（flight owner / (state,key) / per-flight 指针 = 正确；全局 key 集合 = 错误）；(2) 完成时刻 Clock 异常是否隔离；(3) 是否严格单一同步点。

### Q3 RoutingConfig（Refactor/Design）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|-----------|------------|---------|-----------|---------|----------|----------|
| claude code + glm-5.2 | 43 | 19 | 15 | 9 | 10 | **96** | shared_ptr<const Config> 原子发布；valid 失败不变；版本+1；current() 用 thread_local map<this> per-instance pin（跨实例不互覆盖，ASan 通过）；observer 锁内拷指针锁外通知；subscribe/unsubscribe callable 移动销毁锁外；自写 11 例 + clang 交叉 + ASan |
| codex + gpt-5.5 | 43 | 18 | 15 | 9 | 9 | **94** | 等价设计；find_target 经 snapshot() 一致快照最长前缀；锁外构造 observer |
| opencode + glm-5.2 | 43 | 19 | 15 | 9 | 8 | **94** | 抽取 notify_observers 私有方法，结构清晰；subscribe/unsubscribe 锁外；ASan 通过（自述本机 ASan 损坏改用 clang 交叉，诚实说明） |
| claude code + qwen3.7max | 41 | 17 | 13 | 8 | 7 | **90** | 主体正确，跨实例 pin 通过；**subscribe 锁内 make_shared<Observer>(move) 移动 std::function（字面违反"移动锁外"，探针不死锁故未硬性封顶）；全程无自写测试** |
| opencode + qwen3.7max | 41 | 18 | 13 | 8 | 8 | **90** | 主体正确；**subscribe 锁内移动 observer std::function（同上）；current() 持锁写 TLS（过度持锁）；无自写测试** |

**Q3 分水岭**：全员主体正确（跨实例 current() pin 均通过 ASan），主要差异在 subscribe 是否锁外构造 observer、以及自写测试投入。

## 关键差异

**按维度归纳（基于源码与验证证据）：**

- **模型维度（本轮最显著差异）**：GLM-5.2 两个作答（claude code 274、opencode 282，均分 278）均高于 Qwen3.7max 两个作答（claude code 259、opencode 223，均分 241），平均差 37 分。差距主要来自 Qwen3.7max 侧两处硬伤：opencode+qwen 的 Q1 锁内拷贝（硬性封顶）与 claude+qwen 的 Q2 Clock 异常逃逸。GPT-5.5 单作答（codex 280）与 GLM-5.2 头部接近。

- **Agent 框架维度（存在交互效应，本轮无法定论）**：
  - GLM-5.2 下：opencode（282）≈ claude code（274），差 8，几乎打平
  - Qwen3.7max 下：claude code（259）≫ opencode（223），差 36
  - 即"哪个 Agent 框架更好"取决于模型——对 GLM 几乎无差异，对 Qwen 则 claude code 明显更稳。这是「Agent 框架 × 模型」交互效应，5 个样本不足以分离。

- **Q1 硬性封顶**：唯一触发的是 opencode+qwen（drain 锁内拷贝 std::function，callable 拷贝重入确定性死锁）。其余 4 个均用 shared_ptr 托管 callable、锁内只动控制块。

- **验证意识（自写测试投入）**：claude code+glm-5.2（每题 8–11 例 + ASan + clang 交叉，验证最充分）、opencode+glm-5.2（8–12 例 + ASan）、codex+gpt-5.5（edge 用例 + clang）证据意识强；两个 Qwen 作答全程无自写边界/并发测试，其隐藏缺陷（Q2 Clock 异常逃逸、Q1 锁内拷贝）均由 scorer 探针证伪，而非自身发现。

- **"伪修复"模式**：本轮出现两例参考解答明示的伪修复——claude code+glm-5.2 的 `cancelled` 标志声明了却从不置位（ANSWER 自称已实现取消语义，实际无效），opencode+qwen 的全局 key 集合递归检测（单实例正确，跨实例失效）。

## 风险与限制

- **样本量**：5 个参评对象。每模型仅 2 个作答（GLM-5.2、Qwen3.7max 各 2，GPT-5.5 单独 1）。模型/框架结论应视为本轮观察，不可外推到「GLM-5.2 一定优于 Qwen3.7max」或「某 Agent 框架更好」的普适结论——尤其 Agent 框架交互效应需更多样本确认。

- **TSan 全程未运行**：环境 g++ 9.4.0 缺 `libtsan_preinit.o`。所有"无数据竞争"结论基于锁覆盖分析 + shared_ptr 原子引用计数 + ASan 内存安全通过，**非 TSan 实证**。对双 mutex 设计（claude+qwen Q2、opencode+qwen Q2）尤其属未验证风险。

- **未运行或无法确认的测试**：两位 scorer 均实测编译并运行了全部 `run_public_checks.sh`（EXIT=0）与多组自写探针；主 agent 独立复核了 4 个关键分歧项的探针。但 ASan 在 WSL2 下对 q03 脚本偶发 DEADLYSIGNAL 级联（脚本字节相同重跑 PASS），判定为环境抖动，未计为代码缺陷。

- **单轮单题型**：仅 1 套预设（Bugfix/Implementation/Refactor 各 1）、单次作答。结论限定于「本轮题型 + 本轮样本」，不应外推到所有 C++ 任务或所有任务类型。

- **工具权限差异未逐一核验**：各 Agent 实际是否调用编译器、是否真实执行测试，以各自 ANSWER.md 自述 + scorer 复现为准；本轮未独立审计各 Agent 的运行环境权限。

## 下一轮建议

- **扩大样本以分离模型/框架交互**：若要确认「GLM-5.2 > Qwen3.7max」或「某 Agent 框架更优」，建议每模型挂 2 个以上不同 Agent、每组合跑 ≥2 套不同预设（增加 Bugfix/并发/系统设计覆盖）。
- **补 TSan 环境**：配置可运行 ThreadSanitizer 的工具链，对 Q2/Q3 的并发/双锁设计做实证验证，减少「未验证风险」。
- **聚焦 Q1 硬性封顶区分度**：本轮 Q1 硬性封顶成功拉开了 opencode+qwen 与其余 4 个；可保留该类「callable 生命周期锁边界」题作为强区分项。
- **跟踪"伪修复"模式**：claude+glm 的 cancelled 死字段、opencode+qwen 的全局 key 递归检测，提示下一轮可增加针对「字段声明了却未接线」「单实例正确跨实例失效」的探针式检查。

## 附：评分依据来源

- 公共题面：`questions/qXX-*/QUESTION.md`
- 私有参考解答（验收矩阵）：`scoring/scorer-reference/cpp17-advanced-v1.md`（SHA-256 完整性见 `scoring/reference-integrity.md`）
- 两位 scorer 匿名报告：`scoring/scorer-report-A.md`、`scoring/scorer-report-B.md`（原样归档，未修改）
- 匿名映射：`scoring/mapping.private.md`
- 脱敏记录：`scoring/redaction-log.md`
