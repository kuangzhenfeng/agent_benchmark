# 作答说明

## 修改摘要

### 设计

用 `std::shared_ptr<const Config>` 替代裸 `Config` 作为发布单元，实现原子替换。

```
published_  — shared_ptr<const Config>，由 mutex_ 保护
pinned_     — map<thread::id, shared_ptr<const Config>>，为 current() 的返回引用保活
observers_  — map<ObserverId, shared_ptr<Observer>>（shared_ptr 避免持锁复制 Observer）
```

### 发布点

`reload(candidate)`：
1. 校验（无前缀冲突 / 空 prefix / 空 target），失败直接返回 false。
2. 持锁：复制 `observers_`（只复制 `shared_ptr<Observer>`，不复制 Observer 本身）；版本号自增；构造新 `shared_ptr<const Config>` 原子替换 `published_`。
3. 解锁。
4. 遍历 `observers_copy`，锁外调用每个 observer，异常各自捕获并计数。
5. 若有异常计数，持锁累加到 `observer_errors_`。

### current() 的引用保活机制

`current()` 签名无法改为返回 shared_ptr，但必须保证返回的引用在多次 reload 后仍有效，且跨实例互不影响。

采用**实例级线程绑定 pin**：调用 `current()` 时，将当前 `published_`（shared_ptr）保存到 `pinned_[此线程 ID]`，返回 `*pinned_[tid]`。此后该线程再 reload 也不会影响已 pin 的 shared_ptr 的引用计数；直到同一线程再次调用同一个实例的 `current()` 时才替换 pin。不同实例的 `pinned_` 独立，互不干扰。

### 观察者快照时刻与异常隔离

- 观察者在 `reload` 解锁后（版本已发布）收到通知，参数是 `published_` 的 shared_ptr——此时 snapshot 已经是对外可见的当前版本。
- 异常在锁外捕获，单个 observer 抛异常不影响其他 observer 的通知；不影响已发布的配置（不可回滚，因为配置已发布）。
- 每次成功 `reload` 后，所有在调用开始时已订阅的 observer 至多通知一次。通知中新 subscribe 的不收当前通知（observer list 来自 reload 时的快照）。通知中 unsubscribe 的仅影响后续 reload。
- Observer 复制/析构：`subscribe` 将 observer 以 `make_shared` 嵌入 shared_ptr（持锁写入 map），observer 的复制发生在调用 `subscribe` 传参时（锁外）；`unsubscribe` 将 shared_ptr 移出 map 后解锁，在锁外析构。

### 生命周期策略说明

- `pinned_` map 随线程增加而增长，无主动清理。在合理的网关工作线程数量下可接受。生产环境可注册线程退出回调清理。
- `published_` 的旧版本因被其他线程的 `pinned_` 或 `snapshot()`、`find_target` 返回的 shared_ptr 持有而存活，无悬空风险。

## 验证

```bash
cd q03-routing-config && ./run_public_checks.sh
```

输出：
```
Running public_checks.cpp
Running cross_instance_current_checks.cpp with AddressSanitizer
Running observer_copy_reentrancy_checks.cpp
```

三项均静默退出，通过。包括 AddressSanitizer 检测跨实例 use-after-free。

## 剩余风险

- `pinned_` map 在线程长时间存活时不释放旧版本内存。线程退出后条目变为垃圾，但不影响正确性（引用计数确保对象存活）。
- `find_target` 通过 `snapshot()` 获取一致快照，`snapshot()` 持锁时间极短。