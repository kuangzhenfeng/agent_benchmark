#pragma once

#include "task.hpp"

#include <mutex>
#include <string>
#include <vector>

// 状态存储抽象：所有持久化/状态查询的契约都通过本接口，
// 上层只依赖 ITaskStore，不依赖任何具体存储实现
class ITaskStore {
public:
    virtual ~ITaskStore() = default;
    virtual void save(const Task& task) = 0;
    virtual bool load(TaskId id, Task& out) const = 0;
    virtual void update_status(TaskId id, TaskStatus status) = 0;
    virtual std::vector<Task> pending() const = 0;
    virtual void remove(TaskId id) = 0;
};

// 内存存储：默认实现，组合根实际装配
class InMemoryTaskStore final : public ITaskStore {
public:
    void save(const Task& task) override;
    bool load(TaskId id, Task& out) const override;
    void update_status(TaskId id, TaskStatus status) override;
    std::vector<Task> pending() const override;
    void remove(TaskId id) override;

private:
    std::vector<Task> tasks_;
    mutable std::mutex m_;
};

// 持久化存储：追加写日志式实现。已编译进产物，但组合根（main）默认装配 InMemoryTaskStore
class PersistentTaskStore final : public ITaskStore {
public:
    explicit PersistentTaskStore(std::string path);
    void save(const Task& task) override;
    bool load(TaskId id, Task& out) const override;
    void update_status(TaskId id, TaskStatus status) override;
    std::vector<Task> pending() const override;
    void remove(TaskId id) override;

private:
    std::string path_;
    std::vector<Task> tasks_;
    mutable std::mutex m_;
};
