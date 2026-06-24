#include "notifier.hpp"

#include <iostream>

void ConsoleNotifier::on_event(const std::string& event, const TaskResult& result) {
    std::cout << "[event] " << event << " id=" << result.id.value
              << " ok=" << (result.ok ? "yes" : "no")
              << " code=" << result.code << "\n";
}

CallbackNotifier::CallbackNotifier(Callback cb) : cb_(std::move(cb)) {}

void CallbackNotifier::on_event(const std::string& event, const TaskResult& result) {
    if (cb_) {
        cb_(event, result);
    }
}

void CompositeNotifier::add(std::shared_ptr<INotifier> child) {
    children_.push_back(std::move(child));
}

void CompositeNotifier::on_event(const std::string& event, const TaskResult& result) {
    for (auto& child : children_) {
        child->on_event(event, result);
    }
}
