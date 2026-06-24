#!/usr/bin/env python3
"""生成 5 张 TaskForge 架构图 SVG"""

import os
import math

DIAG_DIR = os.path.join(os.path.dirname(__file__), "diagrams")
os.makedirs(DIAG_DIR, exist_ok=True)

SVG_HEADER = '<?xml version="1.0" encoding="UTF-8"?>\n'


def rect(x, y, w, h, fill, stroke="#333", rx=6, sw=1.5):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" rx="{rx}"/>'


def text(x, y, content, size=14, anchor="middle", fill="#222", weight="normal"):
    return f'<text x="{x}" y="{y}" font-family="Segoe UI, Arial, sans-serif" font-size="{size}" text-anchor="{anchor}" fill="{fill}" font-weight="{weight}">{content}</text>'


def node_box(data_id, data_kind, x, y, w, h, fill, label, sublabel=None):
    parts = [f'<g data-id="{data_id}" data-kind="{data_kind}">']
    parts.append(rect(x, y, w, h, fill))
    if sublabel:
        parts.append(text(x + w/2, y + h/2 - 4, label, size=12, weight="bold"))
        parts.append(text(x + w/2, y + h/2 + 12, sublabel, size=9, fill="#555"))
    else:
        parts.append(text(x + w/2, y + h/2 + 5, label, size=12, weight="bold"))
    parts.append('</g>')
    return '\n'.join(parts)


def edge_arrow(data_from, data_to, data_kind, x1, y1, x2, y2, stroke="#555", label=None, dashed=False):
    dash = ' stroke-dasharray="6,3"' if dashed else ''
    parts = []
    parts.append(f'<line data-from="{data_from}" data-to="{data_to}" data-kind="{data_kind}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{stroke}" stroke-width="1.5"{dash}/>')
    # arrowhead
    dx = x2 - x1
    dy = y2 - y1
    length = math.sqrt(dx*dx + dy*dy)
    if length > 0:
        ux, uy = dx/length, dy/length
        ax, ay = x2 - ux*8, y2 - uy*8
        px, py = -uy*4, ux*4
        parts.append(f'<polygon points="{ax+px},{ay+py} {x2},{y2} {ax-px},{ay-py}" fill="{stroke}"/>')
    if label:
        mx, my = (x1+x2)/2, (y1+y2)/2 - 6
        parts.append(f'<text x="{mx}" y="{my}" font-family="Arial" font-size="9" text-anchor="middle" fill="#666">{label}</text>')
    return '\n'.join(parts)


def layer_bg(x, y, w, h, fill, label):
    parts = [rect(x, y, w, h, fill, stroke="#bbb", rx=10, sw=1)]
    parts.append(text(x + 16, y + 22, label, size=13, anchor="start", fill="#444", weight="bold"))
    return '\n'.join(parts)


# =============================================================================
# 1. 分层架构图 (layered)
# =============================================================================
def gen_layered():
    W, H = 1200, 800
    parts = [SVG_HEADER]
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" data-style="layered">')
    parts.append(text(W/2, 35, "TaskForge 分层架构图", size=22, weight="bold"))

    # Layer backgrounds
    parts.append(layer_bg(40, 55, W-80, 90, "#e8f4fd", "应用入口层 (Application Entry)"))
    parts.append(layer_bg(40, 165, W-80, 100, "#fef3e2", "编排层 (Orchestration)"))
    parts.append(layer_bg(40, 285, W-80, 140, "#e8f8e8", "领域服务层 (Domain Services)"))
    parts.append(layer_bg(40, 445, W-80, 180, "#f3e8ff", "基础设施层 (Infrastructure)"))
    parts.append(layer_bg(40, 650, W-80, 90, "#f0f0f0", "死代码 / 历史遗留 (Dead Code)"))

    # All nodes in one container
    parts.append('<g data-role="nodes">')
    parts.append(node_box("main", "module", 500, 72, 200, 50, "#b3d9f2", "main (组合根)"))
    parts.append(node_box("Scheduler", "class", 500, 185, 200, 55, "#ffd699", "Scheduler", "编排核心"))
    parts.append(node_box("WorkerPool", "class", 80, 310, 170, 50, "#a8d8a8", "WorkerPool"))
    parts.append(node_box("Worker", "class", 270, 310, 140, 50, "#a8d8a8", "Worker"))
    parts.append(node_box("TaskQueue", "class", 430, 310, 170, 50, "#a8d8a8", "TaskQueue", "优先级队列"))
    parts.append(node_box("MetricsCollector", "class", 620, 310, 180, 50, "#a8d8a8", "MetricsCollector"))
    parts.append(node_box("IRetryPolicy", "interface", 820, 310, 180, 50, "#c8e8c8", "IRetryPolicy"))
    parts.append(node_box("Task", "class", 1020, 310, 100, 50, "#a8d8a8", "Task"))
    parts.append(node_box("ITaskStore", "interface", 80, 475, 170, 50, "#d4b8f0", "ITaskStore"))
    parts.append(node_box("InMemoryTaskStore", "class", 80, 545, 190, 50, "#c8a8e8", "InMemoryTaskStore", "默认装配"))
    parts.append(node_box("PersistentTaskStore", "class", 290, 545, 200, 50, "#ddd", "PersistentTaskStore", "编译但未装配"))
    parts.append(node_box("IClock", "interface", 510, 475, 130, 50, "#d4b8f0", "IClock"))
    parts.append(node_box("SystemClock", "class", 510, 545, 150, 50, "#c8a8e8", "SystemClock"))
    parts.append(node_box("INotifier", "interface", 680, 475, 150, 50, "#d4b8f0", "INotifier"))
    parts.append(node_box("ConsoleNotifier", "class", 680, 545, 170, 50, "#c8a8e8", "ConsoleNotifier"))
    parts.append(node_box("CallbackNotifier", "class", 870, 545, 170, 50, "#c8a8e8", "CallbackNotifier"))
    parts.append(node_box("CompositeNotifier", "class", 870, 475, 180, 50, "#c8a8e8", "CompositeNotifier"))
    parts.append(node_box("EngineConfig", "class", 1060, 475, 80, 50, "#d4b8f0", "EngineConfig"))
    parts.append(node_box("TaskQueueLegacy", "class", 80, 670, 200, 50, "#e0e0e0", "TaskQueueLegacy", "无人调用"))
    parts.append(node_box("ILogger", "interface", 310, 670, 130, 50, "#e0e0e0", "ILogger", "未使用"))
    parts.append(node_box("ConsoleLogger", "class", 460, 670, 160, 50, "#e0e0e0", "ConsoleLogger", "未使用"))
    parts.append(node_box("run_legacy", "method", 640, 670, 160, 50, "#e0e0e0", "run_legacy()", "兼容入口"))
    parts.append('</g>')

    parts.append('<g data-role="edges">')
    parts.append(edge_arrow("main", "Scheduler", "depends", 600, 122, 600, 185))
    parts.append(edge_arrow("Scheduler", "WorkerPool", "depends", 520, 240, 165, 310))
    parts.append(edge_arrow("Scheduler", "TaskQueue", "depends", 580, 240, 515, 310))
    parts.append(edge_arrow("Scheduler", "MetricsCollector", "depends", 620, 240, 710, 310))
    parts.append(edge_arrow("Scheduler", "IRetryPolicy", "depends", 650, 240, 910, 310))
    parts.append(edge_arrow("Scheduler", "ITaskStore", "depends", 520, 240, 165, 475))
    parts.append(edge_arrow("Scheduler", "IClock", "depends", 590, 240, 575, 475))
    parts.append(edge_arrow("Scheduler", "INotifier", "depends", 630, 240, 755, 475))
    parts.append(edge_arrow("Scheduler", "EngineConfig", "depends", 660, 240, 1100, 475))
    parts.append(edge_arrow("WorkerPool", "Worker", "depends", 200, 335, 270, 335))
    parts.append(edge_arrow("Worker", "TaskQueue", "depends", 380, 335, 430, 335))
    parts.append(edge_arrow("InMemoryTaskStore", "ITaskStore", "depends", 175, 545, 165, 525))
    parts.append(edge_arrow("SystemClock", "IClock", "depends", 585, 545, 575, 525))
    parts.append(edge_arrow("CompositeNotifier", "INotifier", "depends", 960, 475, 755, 525))
    parts.append('</g>')

    parts.append('</svg>')
    return '\n'.join(parts)


# =============================================================================
# 2. 组件依赖图 (component)
# =============================================================================
def gen_component():
    W, H = 1200, 800
    parts = [SVG_HEADER]
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" data-style="component">')
    parts.append(text(W/2, 35, "TaskForge 组件依赖图", size=22, weight="bold"))
    parts.append(text(W/2, 55, "实线 = 编译期依赖 (#include) | 虚线 = 运行时回调 (callback)", size=12, fill="#666"))

    parts.append('<g data-role="nodes">')
    parts.append(node_box("main", "module", 500, 80, 200, 55, "#ffcc80", "main.cpp", "组合根"))
    parts.append(node_box("scheduler", "module", 500, 200, 200, 55, "#ffd699", "scheduler.hpp/.cpp"))
    parts.append(node_box("worker", "module", 180, 320, 180, 55, "#a8d8a8", "worker.hpp/.cpp"))
    parts.append(node_box("task_queue", "module", 400, 320, 200, 55, "#a8d8a8", "task_queue.hpp/.cpp"))
    parts.append(node_box("task_store", "module", 80, 460, 200, 55, "#c8a8e8", "task_store.hpp/.cpp"))
    parts.append(node_box("retry_policy", "module", 310, 460, 200, 55, "#a8d8a8", "retry_policy.hpp/.cpp"))
    parts.append(node_box("metrics", "module", 540, 460, 180, 55, "#a8d8a8", "metrics.hpp/.cpp"))
    parts.append(node_box("clock", "module", 750, 460, 150, 55, "#c8a8e8", "clock.hpp/.cpp"))
    parts.append(node_box("config", "module", 930, 460, 150, 55, "#c8a8e8", "config.hpp/.cpp"))
    parts.append(node_box("notifier", "module", 750, 320, 200, 55, "#c8a8e8", "notifier.hpp/.cpp"))
    parts.append(node_box("logger", "module", 980, 320, 150, 55, "#e0e0e0", "logger.hpp", "未使用"))
    parts.append(node_box("task", "module", 540, 600, 150, 55, "#a8d8a8", "task.hpp/.cpp"))
    parts.append(node_box("TaskQueueLegacy", "module", 340, 600, 170, 55, "#e0e0e0", "TaskQueueLegacy", "死代码"))
    parts.append('</g>')

    parts.append('<g data-role="edges">')
    parts.append(edge_arrow("main", "scheduler", "depends", 600, 135, 600, 200))
    parts.append(edge_arrow("main", "notifier", "depends", 680, 107, 850, 320))
    parts.append(edge_arrow("scheduler", "task_queue", "depends", 550, 255, 500, 320))
    parts.append(edge_arrow("scheduler", "worker", "depends", 530, 255, 270, 320))
    parts.append(edge_arrow("scheduler", "retry_policy", "depends", 570, 255, 410, 460))
    parts.append(edge_arrow("scheduler", "metrics", "depends", 620, 255, 630, 460))
    parts.append(edge_arrow("scheduler", "clock", "depends", 660, 255, 825, 460))
    parts.append(edge_arrow("scheduler", "config", "depends", 690, 255, 1005, 460))
    parts.append(edge_arrow("worker", "task_queue", "depends", 330, 347, 400, 347))
    parts.append(edge_arrow("task_queue", "task", "depends", 500, 375, 615, 600))
    parts.append(edge_arrow("task_store", "task", "depends", 180, 515, 570, 627))
    parts.append(edge_arrow("notifier", "task", "depends", 850, 375, 660, 600))
    parts.append(edge_arrow("scheduler", "task_store", "depends", 530, 255, 180, 460))
    # Runtime callback only (worker -> scheduler via completion_handler)
    parts.append(edge_arrow("worker", "scheduler", "callback", 270, 320, 560, 255, stroke="#e06060", label="completion_handler (runtime)", dashed=True))
    parts.append('</g>')

    parts.append('</svg>')
    return '\n'.join(parts)


# =============================================================================
# 3. 数据流图 (dataflow)
# =============================================================================
def gen_dataflow():
    W, H = 1200, 800
    parts = [SVG_HEADER]
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" data-style="dataflow">')
    parts.append(text(W/2, 35, "TaskForge 任务数据流图", size=22, weight="bold"))
    parts.append(text(W/2, 55, "一次任务从提交、入队、执行、产出结果、失败重试、到终态通知的完整流转", size=11, fill="#666"))

    parts.append('<g data-role="nodes">')
    parts.append(node_box("Client", "actor", 50, 130, 140, 55, "#b3d9f2", "Client / main", "submit()"))
    parts.append(node_box("Scheduler", "class", 250, 130, 160, 55, "#ffd699", "Scheduler", "submit/submitBatch"))
    parts.append(node_box("ITaskStore", "interface", 250, 280, 160, 55, "#d4b8f0", "ITaskStore", "save/load/update"))
    parts.append(node_box("TaskQueue", "class", 480, 130, 160, 55, "#a8d8a8", "TaskQueue", "push/blocking_pop"))
    parts.append(node_box("Worker", "class", 700, 130, 140, 55, "#a8d8a8", "Worker (xN)", "loop()"))
    parts.append(node_box("task_work", "function", 920, 130, 160, 55, "#ffcc80", "task.work()", "业务负载"))
    parts.append(node_box("TaskResult", "struct", 920, 280, 160, 55, "#ffcc80", "TaskResult", "ok/code/attempt"))
    parts.append(node_box("handle_completion", "method", 700, 280, 200, 55, "#ffd699", "handle_completion", "Scheduler 回调"))
    parts.append(node_box("IRetryPolicy", "interface", 530, 400, 170, 55, "#d4b8f0", "IRetryPolicy", "next_delay_ms"))
    parts.append(node_box("MetricsCollector", "class", 920, 400, 180, 55, "#a8d8a8", "MetricsCollector", "record_*"))
    parts.append(node_box("INotifier", "interface", 700, 460, 160, 55, "#d4b8f0", "INotifier", "on_event"))
    parts.append(node_box("Succeeded", "state", 50, 460, 120, 45, "#90ee90", "Succeeded"))
    parts.append(node_box("Failed", "state", 50, 530, 120, 45, "#ff9999", "Failed"))
    parts.append(node_box("Retrying", "state", 350, 530, 120, 45, "#ffcc80", "Retrying"))
    parts.append('</g>')

    parts.append('<g data-role="edges">')
    parts.append(edge_arrow("Client", "Scheduler", "flow", 190, 157, 250, 157, label="Task"))
    parts.append(edge_arrow("Scheduler", "ITaskStore", "flow", 330, 185, 330, 280, label="save(task)"))
    parts.append(edge_arrow("Scheduler", "TaskQueue", "flow", 410, 157, 480, 157, label="push(task)"))
    parts.append(edge_arrow("TaskQueue", "Worker", "flow", 640, 157, 700, 157, label="blocking_pop"))
    parts.append(edge_arrow("Worker", "task_work", "flow", 840, 157, 920, 157, label="execute"))
    parts.append(edge_arrow("task_work", "TaskResult", "flow", 1000, 185, 1000, 280, label="result"))
    parts.append(edge_arrow("Worker", "handle_completion", "flow", 770, 185, 790, 280, label="callback(TaskResult)"))
    parts.append(edge_arrow("handle_completion", "ITaskStore", "flow", 700, 307, 410, 307, label="update_status"))
    parts.append(edge_arrow("handle_completion", "MetricsCollector", "flow", 900, 307, 1010, 400, label="record_*"))
    parts.append(edge_arrow("handle_completion", "IRetryPolicy", "flow", 730, 335, 615, 400, label="next_delay_ms"))
    parts.append(edge_arrow("handle_completion", "INotifier", "flow", 800, 335, 780, 460, label="on_event"))
    parts.append(edge_arrow("handle_completion", "Succeeded", "flow", 700, 310, 110, 482, label="ok=true"))
    parts.append(edge_arrow("handle_completion", "Failed", "flow", 700, 320, 110, 552, label="exhausted"))
    parts.append(edge_arrow("handle_completion", "Retrying", "flow", 730, 335, 410, 552, label="can retry"))
    parts.append(edge_arrow("Retrying", "TaskQueue", "flow", 410, 535, 530, 185, label="re-push(task)"))
    parts.append('</g>')

    parts.append('</svg>')
    return '\n'.join(parts)


# =============================================================================
# 4. 类关系图 (class)
# =============================================================================
def gen_class():
    W, H = 1200, 800
    parts = [SVG_HEADER]
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" data-style="class">')
    parts.append(text(W/2, 35, "TaskForge 类与模块关系图", size=22, weight="bold"))

    parts.append('<g data-role="nodes">')
    # Interfaces
    parts.append(node_box("ITaskStore", "interface", 60, 100, 170, 50, "#d4b8f0", "ITaskStore"))
    parts.append(node_box("IRetryPolicy", "interface", 280, 100, 180, 50, "#d4b8f0", "IRetryPolicy"))
    parts.append(node_box("INotifier", "interface", 510, 100, 160, 50, "#d4b8f0", "INotifier"))
    parts.append(node_box("IClock", "interface", 720, 100, 130, 50, "#d4b8f0", "IClock"))
    parts.append(node_box("ILogger", "interface", 900, 100, 130, 50, "#e0e0e0", "ILogger (未用)"))
    # Implementations
    parts.append(node_box("InMemoryTaskStore", "class", 40, 220, 200, 45, "#c8a8e8", "InMemoryTaskStore"))
    parts.append(node_box("PersistentTaskStore", "class", 40, 285, 200, 45, "#ddd", "PersistentTaskStore"))
    parts.append(node_box("FixedBackoffPolicy", "class", 270, 220, 200, 45, "#a8d8a8", "FixedBackoffPolicy"))
    parts.append(node_box("ExponentialBackoffPolicy", "class", 270, 285, 220, 45, "#a8d8a8", "ExponentialBackoffPolicy"))
    parts.append(node_box("ConsoleNotifier", "class", 510, 220, 170, 45, "#a8d8a8", "ConsoleNotifier"))
    parts.append(node_box("CallbackNotifier", "class", 510, 285, 170, 45, "#a8d8a8", "CallbackNotifier"))
    parts.append(node_box("CompositeNotifier", "class", 510, 350, 180, 45, "#a8d8a8", "CompositeNotifier"))
    parts.append(node_box("SystemClock", "class", 720, 220, 150, 45, "#a8d8a8", "SystemClock"))
    parts.append(node_box("ConsoleLogger", "class", 900, 220, 150, 45, "#e0e0e0", "ConsoleLogger"))
    # Core classes
    parts.append(node_box("Scheduler", "class", 350, 460, 200, 55, "#ffd699", "Scheduler", "编排核心"))
    parts.append(node_box("WorkerPool", "class", 600, 460, 160, 50, "#a8d8a8", "WorkerPool"))
    parts.append(node_box("Worker", "class", 600, 540, 140, 45, "#a8d8a8", "Worker"))
    parts.append(node_box("TaskQueue", "class", 820, 460, 160, 50, "#a8d8a8", "TaskQueue"))
    parts.append(node_box("MetricsCollector", "class", 820, 540, 180, 45, "#a8d8a8", "MetricsCollector"))
    parts.append(node_box("EngineConfig", "class", 1000, 460, 140, 50, "#c8a8e8", "EngineConfig"))
    parts.append(node_box("Task", "class", 1000, 540, 100, 45, "#a8d8a8", "Task"))
    parts.append(node_box("TaskResult", "struct", 1000, 610, 120, 40, "#ffcc80", "TaskResult"))
    parts.append('</g>')

    parts.append('<g data-role="edges">')
    # implements
    parts.append(edge_arrow("InMemoryTaskStore", "ITaskStore", "implements", 140, 220, 145, 150))
    parts.append(edge_arrow("PersistentTaskStore", "ITaskStore", "implements", 140, 285, 145, 150))
    parts.append(edge_arrow("FixedBackoffPolicy", "IRetryPolicy", "implements", 370, 220, 370, 150))
    parts.append(edge_arrow("ExponentialBackoffPolicy", "IRetryPolicy", "implements", 380, 285, 370, 150))
    parts.append(edge_arrow("ConsoleNotifier", "INotifier", "implements", 595, 220, 590, 150))
    parts.append(edge_arrow("CallbackNotifier", "INotifier", "implements", 595, 285, 590, 150))
    parts.append(edge_arrow("CompositeNotifier", "INotifier", "implements", 600, 350, 590, 150))
    parts.append(edge_arrow("SystemClock", "IClock", "implements", 795, 220, 785, 150))
    parts.append(edge_arrow("ConsoleLogger", "ILogger", "implements", 975, 220, 965, 150))
    # composes
    parts.append(edge_arrow("Scheduler", "TaskQueue", "composes", 550, 487, 820, 485))
    parts.append(edge_arrow("Scheduler", "WorkerPool", "composes", 550, 480, 600, 480))
    parts.append(edge_arrow("WorkerPool", "Worker", "composes", 680, 510, 670, 540))
    # aggregates
    parts.append(edge_arrow("CompositeNotifier", "INotifier", "aggregates", 600, 375, 590, 150, stroke="#228B22"))
    # uses (Scheduler depends on interfaces)
    parts.append(edge_arrow("Scheduler", "ITaskStore", "uses", 400, 460, 145, 150, stroke="#4169E1"))
    parts.append(edge_arrow("Scheduler", "IRetryPolicy", "uses", 420, 460, 370, 150, stroke="#4169E1"))
    parts.append(edge_arrow("Scheduler", "INotifier", "uses", 470, 460, 590, 150, stroke="#4169E1"))
    parts.append(edge_arrow("Scheduler", "IClock", "uses", 500, 460, 785, 150, stroke="#4169E1"))
    parts.append(edge_arrow("Scheduler", "MetricsCollector", "uses", 530, 487, 910, 562))
    parts.append(edge_arrow("Scheduler", "EngineConfig", "uses", 550, 480, 1070, 485))
    # Worker uses TaskQueue
    parts.append(edge_arrow("Worker", "TaskQueue", "uses", 670, 562, 900, 485, stroke="#4169E1"))
    # depends
    parts.append(edge_arrow("TaskQueue", "Task", "depends", 940, 485, 1050, 540))
    parts.append(edge_arrow("Worker", "TaskResult", "depends", 720, 562, 1060, 630))
    parts.append('</g>')

    parts.append('</svg>')
    return '\n'.join(parts)


# =============================================================================
# 5. 部署/运行时拓扑图 (deployment)
# =============================================================================
def gen_deployment():
    W, H = 1200, 800
    parts = [SVG_HEADER]
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" data-style="deployment">')
    parts.append(text(W/2, 35, "TaskForge 部署 / 运行时拓扑图", size=22, weight="bold"))
    parts.append(text(W/2, 55, "进程内多线程模型：1 主线程 + N Worker 线程 + 1 停止线程", size=11, fill="#666"))

    # Process boundary
    parts.append(rect(40, 70, W-80, H-100, "#f8f8ff", stroke="#888", rx=16))
    parts.append(text(60, 95, "TaskForge Process (进程内)", size=15, anchor="start", weight="bold", fill="#444"))

    # Regions (background only, not nodes)
    parts.append(rect(60, 110, 350, 240, "#e8f4fd", stroke="#6699cc", rx=10))
    parts.append(text(80, 132, "Main Thread (主线程)", size=13, anchor="start", weight="bold"))
    parts.append(rect(440, 110, 480, 240, "#e8f8e8", stroke="#669966", rx=10))
    parts.append(text(460, 132, "WorkerPool (4 Worker Threads)", size=13, anchor="start", weight="bold"))
    parts.append(rect(60, 380, W-120, 130, "#fff8e8", stroke="#cc9966", rx=10))
    parts.append(text(80, 402, "Shared Resources (共享资源)", size=13, anchor="start", weight="bold"))
    parts.append(rect(60, 540, 350, 90, "#fce8e8", stroke="#cc6666", rx=10))
    parts.append(text(80, 562, "Stopper Thread (Demo)", size=13, anchor="start", weight="bold"))
    parts.append(rect(440, 540, 660, 180, "#f5f5f5", stroke="#aaa", rx=10))
    parts.append(text(460, 562, "编译进产物但运行时未实例化", size=12, anchor="start", weight="bold", fill="#888"))

    parts.append('<g data-role="nodes">')
    # Main thread instances
    parts.append(node_box("main_loop", "thread", 80, 145, 310, 45, "#b3d9f2", "scheduler.run() 循环"))
    parts.append(node_box("Scheduler_inst", "instance", 80, 205, 200, 42, "#ffd699", "Scheduler (x1)"))
    parts.append(node_box("MetricsCollector_inst", "instance", 80, 265, 230, 42, "#a8d8a8", "MetricsCollector (x1)"))
    parts.append(node_box("Store_inst", "instance", 315, 265, 85, 42, "#c8a8e8", "ITaskStore"))

    # Worker instances
    for i in range(4):
        x = 460 + i * 110
        parts.append(node_box(f"Worker_{i}", "instance", x, 145, 100, 42, "#a8d8a8", f"Worker-{i}"))
        parts.append(node_box(f"WorkerThread_{i}", "thread", x, 205, 100, 42, "#90c890", f"Thread-{i}"))
    parts.append(node_box("WorkerPool_inst", "instance", 460, 265, 200, 42, "#a8d8a8", "WorkerPool (x1)"))

    # Shared resources
    parts.append(node_box("TaskQueue_inst", "instance", 80, 415, 250, 50, "#a8d8a8", "TaskQueue (x1)", "mutex + condvar"))
    parts.append(node_box("IRetryPolicy_inst", "instance", 360, 415, 250, 50, "#c8a8e8", "IRetryPolicy (x1)", "ExponentialBackoff"))
    parts.append(node_box("INotifier_inst", "instance", 640, 415, 280, 50, "#c8a8e8", "INotifier (x1)", "CompositeNotifier"))
    parts.append(node_box("IClock_inst", "instance", 950, 415, 150, 50, "#c8a8e8", "IClock (x1)", "SystemClock"))

    # Stopper
    parts.append(node_box("StopperThread", "thread", 80, 575, 200, 42, "#ff9999", "std::thread (x1)", "800ms 后 stop()"))

    # Not instantiated
    parts.append(node_box("PersistentTaskStore_inst", "unused", 460, 580, 190, 42, "#e0e0e0", "PersistentTaskStore"))
    parts.append(node_box("TaskQueueLegacy_inst", "unused", 670, 580, 180, 42, "#e0e0e0", "TaskQueueLegacy"))
    parts.append(node_box("FixedBackoffPolicy_inst", "unused", 870, 580, 190, 42, "#e0e0e0", "FixedBackoffPolicy"))
    parts.append(node_box("ILogger_inst", "unused", 460, 640, 130, 42, "#e0e0e0", "ILogger"))
    parts.append(node_box("ConsoleLogger_inst", "unused", 610, 640, 150, 42, "#e0e0e0", "ConsoleLogger"))
    parts.append('</g>')

    parts.append('<g data-role="edges">')
    parts.append(edge_arrow("main_loop", "Scheduler_inst", "contains", 235, 190, 180, 205))
    parts.append(edge_arrow("Scheduler_inst", "WorkerPool_inst", "connects", 280, 226, 560, 286, stroke="#6699cc"))
    for i in range(4):
        x = 510 + i * 110
        parts.append(edge_arrow(f"Worker_{i}", "TaskQueue_inst", "connects", x, 187, 205, 415, stroke="#669966"))
    parts.append(edge_arrow("Scheduler_inst", "TaskQueue_inst", "connects", 180, 247, 205, 415, stroke="#6699cc"))
    parts.append(edge_arrow("Scheduler_inst", "Store_inst", "contains", 250, 226, 357, 286))
    parts.append(edge_arrow("Scheduler_inst", "MetricsCollector_inst", "contains", 200, 247, 195, 265))
    parts.append(edge_arrow("Scheduler_inst", "IRetryPolicy_inst", "connects", 230, 247, 485, 440, stroke="#cc9966"))
    parts.append(edge_arrow("Scheduler_inst", "INotifier_inst", "connects", 250, 247, 780, 440, stroke="#cc9966"))
    parts.append(edge_arrow("Scheduler_inst", "IClock_inst", "connects", 270, 247, 1025, 440, stroke="#cc9966"))
    parts.append(edge_arrow("StopperThread", "Scheduler_inst", "callback", 180, 575, 180, 247, stroke="#cc6666", label="stop()", dashed=True))
    parts.append(edge_arrow("Worker_0", "Scheduler_inst", "callback", 510, 187, 250, 226, stroke="#e06060", label="completion", dashed=True))
    for i in range(4):
        x = 510 + i * 110
        parts.append(edge_arrow(f"Worker_{i}", f"WorkerThread_{i}", "runs-on", x, 187, x, 205, stroke="#336633"))
    parts.append('</g>')

    parts.append('</svg>')
    return '\n'.join(parts)


# =============================================================================
# Generate all
# =============================================================================
generators = {
    "01_layered_architecture.svg": gen_layered,
    "02_component_dependency.svg": gen_component,
    "03_dataflow.svg": gen_dataflow,
    "04_class_relationship.svg": gen_class,
    "05_deployment_topology.svg": gen_deployment,
}

for fname, gen_fn in generators.items():
    path = os.path.join(DIAG_DIR, fname)
    svg_content = gen_fn()
    with open(path, 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print(f"Generated: {fname}")

print("\nAll 5 SVGs generated successfully.")
