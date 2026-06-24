#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

// 任务状态机：覆盖一次提交从入队到终态的完整生命周期
enum class TaskStatus : std::uint8_t {
    Pending = 0,
    Queued,
    Running,
    Retrying,
    Succeeded,
    Failed,
    Cancelled
};

// 任务优先级，数值越大优先级越高
enum class Priority : std::uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// 强类型任务标识，避免与普通整数混淆
struct TaskId {
    std::uint64_t value{0};
};

struct TaskIdHash {
    std::size_t operator()(TaskId id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

struct TaskIdEq {
    bool operator()(TaskId a, TaskId b) const noexcept {
        return a.value == b.value;
    }
};

// 任务执行结果：由 Worker 产出，回传给调度器结算
struct TaskResult {
    TaskId id;
    bool ok{false};
    int code{0};
    std::uint16_t attempt{0};   // 本次执行是第几次尝试（从 1 起）
    std::string detail;
};

// 任务实体
struct Task {
    TaskId id;
    std::string name;
    Priority priority{Priority::Normal};
    std::uint16_t max_attempts{3};
    std::function<int()> work;        // 真正的业务负载，返回 0 视为成功
    std::uint16_t attempt{0};         // 已尝试次数
    TaskStatus status{TaskStatus::Pending};
    std::uint64_t created_tick{0};
    std::uint64_t last_run_tick{0};
};

// 便于日志/展示的状态名
const char* task_status_name(TaskStatus status);
