# 作答说明 — Q3 RoutingConfig

## 修改摘要

starter 的核心问题：`current_` 是被就地修改的 `Config`（`current_ = std::move(candidate)`）→ reload 后旧引用悬空；`current()` 返回 `current_` 引用且全程持锁 → 与 `reload` 的写竞争 + 跨实例共用一个 TLS 槽（题目测试所指）；observer 与 observer callable 的拷贝/销毁全在锁内 → 回调重入死锁；reload 在锁内直接调用 observer → observer 重入死锁；异常会中断其余 observer。重构后：

### 不可变快照 + 原子发布（第 1、2 条）

- 内部不再存可变 `Config`，改为 `std::shared_ptr<const Config> published_`。每次 `reload`：**先完整校验**（`valid`：空 prefix / 空 target / 重复 prefix 任一即返回 `false`，快照、版本、observer 全不变）→ 校验通过才 `candidate.version = published_->version + 1`（严格加一）→ 构造新的 `make_shared<const Config>` → **在锁内指针交换** `published_`（原子发布）。reader 通过 `snapshot()` 取 `shared_ptr` 副本，永不被就地修改、永不悬空。

### `current()` 的引用保活（第 3 条）

- `published_` 是不可变对象，但 `const Config& current()` 必须返回一个**引用稳定**的对象——不能每次返回临时。设计为：**thread_local 的 `unordered_map<const RoutingConfig*, shared_ptr<const Config>>`**，按实例身份（`this`）分槽。
  - `current()` 取当前 `published_` 的 `shared_ptr` 副本，存入 `tls_pins[this]`，返回其引用。
  - 同线程对**同一实例**再次调 `current()` 会替换该槽（符合「下次调用前引用有效」）。
  - 对**另一实例**调 `current()` 写的是另一槽，不动本实例 pin（满足「跨实例 current() 不影响前一实例引用」）。
  - `reload()` 只交换 `published_`，旧 `shared_ptr` 仍被 TLS pin 持有 → 旧引用持续有效、内容仍是旧快照（不可变）。ASan 下 `cross_instance_current_checks` 不再 UAF。
  - 这是「通过正常原子发布路径获取的不可变快照」的 per-thread/per-instance 引用管理，**不是**线程局部全局共享可变路由表——不同线程、不同实例互不干扰，路由真相只在 `published_`。

### Observer 锁外通知 + 快照时刻（第 4 条）

- `reload` 在锁内完成校验、版本自增、`published_` 交换，并**在锁内复制 observer 列表的 `shared_ptr<Observer>` 引用**（仅复制控制块指针，绝不拷贝用户 callable）到局部 `to_notify`，然后**释放锁**。
- 通知阶段（`(*observer)(new_snapshot)`）完全在锁外：observer 可重入 `subscribe`/`unsubscribe`/`reload`/`snapshot`/`current` 而不死锁。被通知的集合是「reload 开始时已订阅」者，回调中新建的订阅不在该集合（不收本次）。
- 每个收到的是**刚发布的不可变快照** `new_snapshot`。Observer callable 的拷贝/移动/销毁全部不在锁内：`subscribe` 在持锁**前**把用户 Observer 移入 `shared_ptr`；`unsubscribe` 在锁内只移出控制块指针，锁外 `reset()` 触发析构（含析构重入）。

### 异常隔离（第 5 条）

- 每个 observer 单独 `try/catch(...)`；抛异常者 `++observer_errors_`（锁内自增计数），**不**中断其余 observer，也**不**回滚已发布配置（发布在通知前完成，符合第 5 条「不能阻止回滚已发布配置」——这里指异常不能让 reload 失败/回滚，已发布配置保持）。`observer_error_count()` 累加每次抛出。

### `find_target`（第 6 条）

- 锁内取**一个** `published_` 副本，在该一致快照上做最长前缀匹配（`path.compare(0, prefix.size(), prefix) == 0` 且选最长 prefix）；无匹配返回空串。`compare` 在 prefix 长于 path 时返回非零，避免越界误匹配。

### Observer 注册/注销的特殊成员（第 4 条后半）

- observer 存 `shared_ptr<Observer>`：注册时 `make_shared<Observer>(std::move(observer))`（用户 callable 的移动在锁外）；注销时锁内移出控制块指针 + `erase`，锁外 `reset()` → 最后一个所有者析构（可能重入）在锁外。`observer_copy_reentrancy_checks`（拷贝构造内重入 `snapshot()`）通过。

## 发布点 / current() 保活 / observer 快照时刻 / 异常隔离（题目要求说明）

- **发布点**：`reload` 中 `published_ = new_snapshot` 这一次指针交换（锁内原子）就是唯一发布点；`snapshot()`/`find_target()`/`current()` 都从这里读。
- **current() 引用保活**：thread_local、按实例 `this` 分槽的 `shared_ptr<const Config>`；reload 只换 `published_`，TLS 中旧引用独立存活 → 引用在该线程下次对本实例调 `current()` 前始终有效且指向旧快照；换实例不影响。
- **observer 快照时刻**：锁内复制 observer 列表时刻 = reload 开始时刻；该列表内每个 observer 至多收一次本次 `new_snapshot`；新建订阅不在列表内不收本次。
- **异常隔离**：逐 observer try/catch；抛异常者计入 `observer_errors_`，不影响其余 observer 与已发布配置。

## 验证

```bash
cd q03-routing-config
./run_public_checks.sh            # public + cross_instance_current(ASan) + observer_copy_reentrancy → EXIT 0

g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I include src/routing_config.cpp tests/extra_boundary_checks.cpp -o /tmp/q3_extra && /tmp/q3_extra
# → ALL PASS（11 用例）

g++ ... -fsanitize=address ... extra_boundary_checks.cpp && /tmp/q3_asan
# → ALL PASS，无内存错误/泄漏

# clang++ 10 交叉验证
clang++ -std=c++17 -Wall -Wextra -Wpedantic -pthread ... extra_boundary_checks.cpp && /tmp/q3_clang
# → ALL PASS
```

新增 `tests/extra_boundary_checks.cpp` 覆盖：
1. 校验失败（空 prefix / 空 target / 重复 prefix）→ `false` 且无变化；
2. 版本严格 +1 + 旧快照不可变；
3. 最长前缀匹配 / 无匹配空串 / 空路径；
4. observer 收一次新快照、注销后不再收；
5. observer 异常隔离 + `observer_error_count`；
6. observer 回调内重入（snapshot/unsubscribe）；
7. 回调内新建订阅不收本次通知；
8. observer callable **析构**重入（unsubscribe 触发锁外析构并重入）；
9. 跨实例 `current()` + reload 后旧引用仍有效（ASan 友好）；
10. `current()` 引用到下次同实例调用前稳定；
11. 并发 reload/snapshot/find_target/current 不崩溃不 UAF。

## 剩余风险

- **TSan 未跑**（本机 toolchain 缺 `libtsan_preinit.o`）。`published_`、`observers_`、`observer_errors_` 统一由 `mutex_` 保护；`published_` 读写都在锁内；TLS pin 是各线程私有，`shared_ptr` 控制块原子引用计数。理论上无竞争，ASan + clang 均通过。
- **TLS pin 生命周期**：thread_local map 随线程退出销毁；但若线程长时间存活且对大量**不同** `RoutingConfig` 实例都调过 `current()`，map 会累积条目（每个条目持有一个 `shared_ptr<const Config>`）。这是「引用保活到下次同实例调用」语义的必然代价；题目未要求清理，且每实例仅一个槽，不会无限增长于同一实例。`RoutingConfig` 析构后其 `this` 槽变成 dangling key——但条目仅在再次调 `current()` 时才被访问/覆盖，访问已析构实例属 UB（与返回 dangling 引用同类问题，超出题目约束）。如需更稳健可引入弱引用清理，但会偏离「最小、清晰」的目标。
- **`current()` 语义取舍**：题目要求「该线程调用另一实例 current() 不影响前一实例引用」。用 `this` 作 key 的 TLS map 是 C++17 下满足该约束且不引入全局静态可变路由表的标准做法；它保活的是不可变快照，不是可变路由真相。
