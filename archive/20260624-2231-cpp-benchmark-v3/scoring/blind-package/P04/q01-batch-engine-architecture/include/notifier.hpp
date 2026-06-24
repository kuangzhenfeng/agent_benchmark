#pragma once

#include "task.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// 通知器抽象：任务终态事件经此对外广播
class INotifier {
public:
    virtual ~INotifier() = default;
    virtual void on_event(const std::string& event, const TaskResult& result) = 0;
};

// 控制台通知：把事件打印到 stdout
class ConsoleNotifier final : public INotifier {
public:
    void on_event(const std::string& event, const TaskResult& result) override;
};

// 回调通知：把事件转交给用户提供的可调用对象
class CallbackNotifier final : public INotifier {
public:
    using Callback = std::function<void(const std::string&, const TaskResult&)>;
    explicit CallbackNotifier(Callback cb);
    void on_event(const std::string& event, const TaskResult& result) override;

private:
    Callback cb_;
};

// 组合通知：按顺序向所有子通知器广播（组合模式）
class CompositeNotifier final : public INotifier {
public:
    void add(std::shared_ptr<INotifier> child);
    void on_event(const std::string& event, const TaskResult& result) override;

private:
    std::vector<std::shared_ptr<INotifier>> children_;
};
