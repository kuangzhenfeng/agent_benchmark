#include "task_store.hpp"

#include <algorithm>
#include <fstream>

// ---------------------------------------------------------------------------
// InMemoryTaskStore
// ---------------------------------------------------------------------------

void InMemoryTaskStore::save(const Task& task) {
    std::lock_guard<std::mutex> lock(m_);
    for (auto& existing : tasks_) {
        if (existing.id.value == task.id.value) {
            existing = task;
            return;
        }
    }
    tasks_.push_back(task);
}

bool InMemoryTaskStore::load(TaskId id, Task& out) const {
    std::lock_guard<std::mutex> lock(m_);
    for (const auto& task : tasks_) {
        if (task.id.value == id.value) {
            out = task;
            return true;
        }
    }
    return false;
}

void InMemoryTaskStore::update_status(TaskId id, TaskStatus status) {
    std::lock_guard<std::mutex> lock(m_);
    for (auto& task : tasks_) {
        if (task.id.value == id.value) {
            task.status = status;
            return;
        }
    }
}

std::vector<Task> InMemoryTaskStore::pending() const {
    std::lock_guard<std::mutex> lock(m_);
    std::vector<Task> result;
    for (const auto& task : tasks_) {
        if (task.status == TaskStatus::Pending || task.status == TaskStatus::Queued) {
            result.push_back(task);
        }
    }
    return result;
}

void InMemoryTaskStore::remove(TaskId id) {
    std::lock_guard<std::mutex> lock(m_);
    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                [id](const Task& task) { return task.id.value == id.value; }),
                 tasks_.end());
}

// ---------------------------------------------------------------------------
// PersistentTaskStore（追加写日志式持久化）
// ---------------------------------------------------------------------------

PersistentTaskStore::PersistentTaskStore(std::string path) : path_(std::move(path)) {
    // 启动时尝试 flush 一个标记行，证明文件可写
    std::ofstream out(path_, std::ios::app);
    if (out) {
        out << "[store] opened " << path_ << "\n";
    }
}

void PersistentTaskStore::save(const Task& task) {
    std::lock_guard<std::mutex> lock(m_);
    for (auto& existing : tasks_) {
        if (existing.id.value == task.id.value) {
            existing = task;
            return;
        }
    }
    tasks_.push_back(task);
    std::ofstream out(path_, std::ios::app);
    if (out) {
        out << "[store] save id=" << task.id.value << " name=" << task.name << "\n";
    }
}

bool PersistentTaskStore::load(TaskId id, Task& out) const {
    std::lock_guard<std::mutex> lock(m_);
    for (const auto& task : tasks_) {
        if (task.id.value == id.value) {
            out = task;
            return true;
        }
    }
    return false;
}

void PersistentTaskStore::update_status(TaskId id, TaskStatus status) {
    std::lock_guard<std::mutex> lock(m_);
    for (auto& task : tasks_) {
        if (task.id.value == id.value) {
            task.status = status;
            std::ofstream out(path_, std::ios::app);
            if (out) {
                out << "[store] update id=" << task.id.value
                    << " status=" << task_status_name(status) << "\n";
            }
            return;
        }
    }
}

std::vector<Task> PersistentTaskStore::pending() const {
    std::lock_guard<std::mutex> lock(m_);
    std::vector<Task> result;
    for (const auto& task : tasks_) {
        if (task.status == TaskStatus::Pending || task.status == TaskStatus::Queued) {
            result.push_back(task);
        }
    }
    return result;
}

void PersistentTaskStore::remove(TaskId id) {
    std::lock_guard<std::mutex> lock(m_);
    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                [id](const Task& task) { return task.id.value == id.value; }),
                 tasks_.end());
}
