#include "task.hpp"

const char* task_status_name(TaskStatus status) {
    switch (status) {
        case TaskStatus::Pending:   return "pending";
        case TaskStatus::Queued:    return "queued";
        case TaskStatus::Running:   return "running";
        case TaskStatus::Retrying:  return "retrying";
        case TaskStatus::Succeeded: return "succeeded";
        case TaskStatus::Failed:    return "failed";
        case TaskStatus::Cancelled: return "cancelled";
    }
    return "unknown";
}
