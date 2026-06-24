#!/usr/bin/env python3
import os

DIAG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "diagrams")
os.makedirs(DIAG_DIR, exist_ok=True)

def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def svg_header(style, title, W=1200, H=800):
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}"
     data-style="{style}">
  <defs>
    <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="2" dy="2" stdDeviation="2" flood-opacity="0.2"/>
    </filter>
    <marker id="arr-depends" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#555"/>
    </marker>
    <marker id="arr-callback" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#e91e63"/>
    </marker>
    <marker id="arr-flow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#0d47a1"/>
    </marker>
    <marker id="arr-retry" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#e65100"/>
    </marker>
    <marker id="arr-impl" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#0d47a1"/>
    </marker>
    <marker id="arr-uses" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#888"/>
    </marker>
    <marker id="arr-aggr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#e65100"/>
    </marker>
    <marker id="arr-comp" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#2e7d32"/>
    </marker>
    <marker id="arr-conn" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#444"/>
    </marker>
    <marker id="arr-cont" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">
      <path d="M0,0 L10,5 L0,10 z" fill="#1565c0"/>
    </marker>
  </defs>
  <rect x="0" y="0" width="{W}" height="{H}" fill="#f8f9fa"/>
  <text x="{W//2}" y="38" font-family="sans-serif" font-size="21" font-weight="bold" text-anchor="middle" fill="#1a1a2e">{esc(title)}</text>
'''

def svg_footer():
    return '</svg>\n'

def layer_bg(y, h, color, label):
    return (f'  <rect x="40" y="{y}" width="1120" height="{h}" rx="8" fill="{color}" opacity="0.18"/>'
            f'  <text x="60" y="{y+20}" font-family="sans-serif" font-size="13" fill="#333" font-style="italic">{esc(label)}</text>')

def node(data_id, kind, x, y, w, h, text, fill):
    safe = esc(text)
    lines = safe.split("\n")
    text_els = ""
    cy = y + h // 2 - (len(lines) - 1) * 7
    for line in lines:
        text_els += f'    <text x="{x+w//2}" y="{cy}" font-family="monospace" font-size="12" text-anchor="middle" fill="#fff">{line}</text>\n'
        cy += 14
    return (f'  <g data-id="{data_id}" data-kind="{kind}">\n'
            f'    <rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="{fill}" filter="url(#shadow)"/>\n'
            f'{text_els}'
            f'  </g>\n')

def edge_line(x1, y1, x2, y2, from_id, to_id, kind, marker="arr-depends", color="#555", dash=""):
    da = f' stroke-dasharray="{dash}"' if dash else ''
    return (f'  <line data-from="{from_id}" data-to="{to_id}" data-kind="{kind}"\n'
            f'        x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}"\n'
            f'        stroke="{color}" stroke-width="2" marker-end="url(#{marker})"{da}/>\n')

# ====================================================================
# 1. Layered Architecture
# ====================================================================
s = svg_header("layered", "TaskForge 分层架构图 (Layered Architecture)")
# 层背景
s += layer_bg(65,  75, "#e3f2fd", "Layer A — Application Entry  (main.cpp)")
s += layer_bg(150, 105, "#e8f5e9", "Layer B — Orchestration  (Scheduler, WorkerPool, Worker)")
s += layer_bg(265, 145, "#fff8e1", "Layer C — Domain Contracts  (接口 + 领域实体)")
s += layer_bg(420, 260, "#fce4ec", "Layer D — Infrastructure  (具体实现 + 支撑组件)")
s += layer_bg(690, 95, "#f3e5f5", "Dead Code  (编译进产物但未被核心路径使用)")

s += '  <g data-role="nodes">\n'
# Layer A
s += node("main", "component", 500, 82, 200, 40, "main()  Composition Root", "#1565c0")
# Layer B
s += node("Scheduler",            "class", 120, 175, 260, 60, "Scheduler\n(编排核心)", "#2e7d32")
s += node("WorkerPool",           "class", 420, 175, 220, 60, "WorkerPool\n(线程池)", "#388e3c")
s += node("Worker",               "class", 680, 175, 180, 60, "Worker [N=4]\n(std::thread)", "#43a047")
# Layer C
s += node("ITaskStore",           "interface", 80,  285, 175, 50, "ITaskStore  <<if>>", "#f9a825")
s += node("IRetryPolicy",         "interface", 280, 285, 175, 50, "IRetryPolicy  <<if>>", "#f9a825")
s += node("INotifier",            "interface", 480, 285, 175, 50, "INotifier  <<if>>", "#f9a825")
s += node("IClock",               "interface", 680, 285, 155, 50, "IClock  <<if>>", "#f9a825")
s += node("Task",                 "class",    860, 285, 210, 50, "Task / TaskResult\n(Domain Entity)", "#ff8f00")
s += node("MetricsCollector",     "class",    1090, 285, 90, 50, "Metrics\nCollector", "#5e35b1")
# Layer D — 具体实现
s += node("InMemoryTaskStore",    "class", 80,  445, 205, 50, "InMemoryTaskStore\n(main 装配)", "#c62828")
s += node("ExponentialBackoffPolicy","class",308, 445, 215, 50, "ExponentialBackoff\nPolicy (main 装配)", "#c62828")
s += node("FixedBackoffPolicy",   "class", 546, 445, 195, 50, "FixedBackoffPolicy", "#d32f2f")
s += node("ConsoleNotifier",      "class", 80,  505, 195, 50, "ConsoleNotifier", "#e65100")
s += node("CallbackNotifier",     "class", 300, 505, 195, 50, "CallbackNotifier", "#e65100")
s += node("CompositeNotifier",    "class", 520, 505, 200, 50, "CompositeNotifier\n(main 装配)", "#e65100")
s += node("SystemClock",          "class", 750, 505, 165, 50, "SystemClock\n(main 装配)", "#b71c1c")
s += node("TaskQueue",            "class", 750, 445, 195, 50, "TaskQueue\n(priority queue)", "#c62828")
s += node("EngineConfig",         "class", 960, 445, 175, 50, "EngineConfig\nConfigLoader", "#00695c")
# Dead code row
s += node("TaskQueueLegacy",      "class", 100, 710, 210, 50, "TaskQueueLegacy\n全仓库无人调用", "#9e9e9e")
s += node("ILogger",              "interface",340,710,210,50,"ILogger / ConsoleLogger\nScheduler 未使用","#9e9e9e")
s += node("PersistentTaskStore",  "class",    580,710,225,50,"PersistentTaskStore\n(main 未装配)", "#757575")
s += '  </g>\n'

s += '  <g data-role="edges">\n'
# A→B
s += edge_line(600, 122, 250, 175, "main", "Scheduler", "depends", "arr-depends", "#1565c0")
# B→C (Scheduler 依赖各接口)
s += edge_line(200, 235, 170, 285, "Scheduler", "ITaskStore",  "depends", "arr-depends", "#2e7d32")
s += edge_line(235, 235, 368, 285, "Scheduler", "IRetryPolicy","depends", "arr-depends", "#2e7d32")
s += edge_line(280, 235, 568, 285, "Scheduler", "INotifier",   "depends", "arr-depends", "#2e7d32")
s += edge_line(300, 235, 758, 285, "Scheduler", "IClock",      "depends", "arr-depends", "#2e7d32")
s += edge_line(350, 225, 1135,285, "Scheduler","MetricsCollector","depends","arr-depends","#2e7d32")
# B→B
s += edge_line(380, 205, 420, 205, "Scheduler","WorkerPool",   "depends", "arr-depends", "#2e7d32")
s += edge_line(640, 205, 680, 205, "WorkerPool","Worker",      "depends", "arr-depends", "#388e3c")
# B→D (Scheduler/Worker → TaskQueue)
s += edge_line(380, 220, 810, 445, "Scheduler","TaskQueue",    "depends", "arr-depends", "#2e7d32")
s += edge_line(770, 235, 848, 445, "Worker",   "TaskQueue",    "depends", "arr-depends", "#43a047")
# C→D (接口被实现)
s += edge_line(170, 335, 183, 445, "ITaskStore",     "InMemoryTaskStore",         "depends","arr-impl","#0d47a1")
s += edge_line(368, 335, 415, 445, "IRetryPolicy",   "ExponentialBackoffPolicy",  "depends","arr-impl","#0d47a1")
s += edge_line(568, 335, 415, 495, "INotifier",      "CallbackNotifier",          "depends","arr-impl","#0d47a1")
s += edge_line(568, 335, 620, 505, "INotifier",      "CompositeNotifier",         "depends","arr-impl","#0d47a1")
s += edge_line(758, 335, 832, 505, "IClock",          "SystemClock",               "depends","arr-impl","#0d47a1")
# B→C (Worker uses Task)
s += edge_line(770, 235, 965, 285, "Worker", "Task",            "depends", "arr-uses", "#888888", "4,3")
s += '</g>\n'

s += """
  <rect x="50" y="775" width="1100" height="20" rx="4" fill="none"/>
  <text x="600" y="792" font-family="sans-serif" font-size="11" fill="#757575" text-anchor="middle" font-style="italic">依赖方向从上到下；Scheduler 依赖抽象接口，具体实现由 main() 组合根注入</text>
"""
s += svg_footer()

with open(os.path.join(DIAG_DIR, "01_layered_architecture.svg"), "w") as f:
    f.write(s)
print("[OK] 01_layered_architecture.svg")

# ====================================================================
# 2. Component Dependency
# ====================================================================
s = svg_header("component", "TaskForge 组件依赖图 (Compile-time vs Runtime)")
s += '  <g data-role="nodes">\n'
s += node("main",             "module", 470, 80,  240, 55, "main.cpp\n(Composition Root)", "#1565c0")
s += node("Scheduler",        "module", 80,  180, 300, 60, "scheduler.hpp / .cpp", "#2e7d32")
s += node("WorkerPool",       "module", 420, 180, 250, 60, "worker.hpp / .cpp\n(WorkerPool + Worker)", "#388e3c")
s += node("Notifier-module",  "module", 710, 180, 230, 60, "notifier.hpp / .cpp\n(Console+Callback+Composite)", "#e65100")
s += node("TaskStore-module", "module", 80,  295, 210, 55, "task_store.hpp/.cpp\n(ITaskStore + impls)", "#f57f17")
s += node("TaskQueue-module", "module", 320, 295, 220, 55, "task_queue.hpp/.cpp\n(TaskQueue + Legacy)", "#f9a825")
s += node("RetryPolicy-module","module",570, 295, 210, 55, "retry_policy.hpp/.cpp\n(Fixed + Exponential)", "#7b1fa2")
s += node("Metrics-module",   "module", 810, 295, 170, 55, "metrics.hpp/.cpp\nMetricsCollector", "#5e35b1")
s += node("Task-module",      "module", 80,  410, 155, 50, "task.hpp / .cpp", "#ff8f00")
s += node("Clock-module",     "module", 265, 410, 155, 50, "clock.hpp", "#00838f")
s += node("Config-module",    "module", 450, 410, 155, 50, "config.hpp / .cpp", "#00695c")
s += node("Logger-module",    "module", 640, 410, 155, 50, "logger.hpp", "#9e9e9e")
# Dead code
s += node("TaskQueueLegacy-note","module",320,490,220,50,"TaskQueueLegacy\n(全仓库无人调用,死代码)","#bdbdbd")
s += node("ILogger-note",     "module", 640, 490, 155, 50, "ILogger\n(未注入 Scheduler)", "#bdbdbd")
s += '  </g>\n'

s += '  <g data-role="edges">\n'
# main→modules
s += edge_line(590, 135, 230, 180, "main", "Scheduler",       "depends","arr-depends","#1565c0")
s += edge_line(590, 135, 545, 180, "main", "WorkerPool",      "depends","arr-depends","#1565c0")
s += edge_line(590, 135, 825, 180, "main", "Notifier-module", "depends","arr-depends","#1565c0")
# Scheduler 编译期依赖
s += edge_line(180, 240, 185, 295, "Scheduler", "TaskStore-module",  "depends","arr-depends","#2e7d32")
s += edge_line(230, 240, 430, 295, "Scheduler", "TaskQueue-module",  "depends","arr-depends","#2e7d32")
s += edge_line(280, 240, 675, 295, "Scheduler", "RetryPolicy-module","depends","arr-depends","#2e7d32")
s += edge_line(340, 240, 895, 295, "Scheduler", "Metrics-module",    "depends","arr-depends","#2e7d32")
s += edge_line(200, 240, 157, 410, "Scheduler", "Task-module",       "depends","arr-depends","#2e7d32")
s += edge_line(300, 240, 342, 410, "Scheduler", "Clock-module",      "depends","arr-depends","#2e7d32")
s += edge_line(370, 240, 527, 410, "Scheduler", "Config-module",     "depends","arr-depends","#2e7d32")
# Scheduler → Worker 编译期（scheduler.hpp #include worker.hpp）
s += edge_line(380, 210, 420, 210, "Scheduler","WorkerPool","depends","arr-depends","#2e7d32")
# Worker 编译期依赖
s += edge_line(545, 240, 430, 295, "WorkerPool","TaskQueue-module","depends","arr-depends","#388e3c")
s += edge_line(500, 240, 175, 410, "WorkerPool","Task-module",     "depends","arr-depends","#388e3c")
# 运行时回调（NOT 编译期）
s += edge_line(545, 200, 380, 200, "WorkerPool", "Scheduler", "callback","arr-callback","#e91e63","6,3")
# notifier→task
s += edge_line(825, 240, 175, 410, "Notifier-module","Task-module","depends","arr-depends","#e65100")
# store→task
s += edge_line(185, 350, 175, 410, "TaskStore-module","Task-module","depends","arr-depends","#f57f17")
# queue→task
s += edge_line(430, 350, 175, 410, "TaskQueue-module","Task-module","depends","arr-depends","#f9a825")
s += '</g>\n'

s += """
  <rect x="50" y="540" width="60" height="20" fill="#2e7d32" rx="3"/>
  <text x="120" y="555" font-family="sans-serif" font-size="12" fill="#333">实线 = 编译期 #include 依赖</text>
  <rect x="50" y="565" width="60" height="20" fill="#e91e63" rx="3"/>
  <line x1="50" y1="575" x2="110" y2="575" stroke="#e91e63" stroke-width="2" stroke-dasharray="6,3"/>
  <text x="120" y="580" font-family="sans-serif" font-size="12" fill="#333">虚线 = 运行时回调 (std::function, NOT 编译期依赖)</text>
"""
s += svg_footer()

with open(os.path.join(DIAG_DIR, "02_component_dependency.svg"), "w") as f:
    f.write(s)
print("[OK] 02_component_dependency.svg")

# ====================================================================
# 3. Data Flow
# ====================================================================
s = svg_header("dataflow", "TaskForge 数据流图 (Task 完整生命周期)")
s += '  <g data-role="nodes">\n'
s += node("Caller",         "actor",     60,  100, 165, 55, "Caller\n(提交 Task)", "#1565c0")
s += node("Scheduler",      "class",     60,  200, 165, 60, "Scheduler::submit\n+submitBatch()", "#2e7d32")
s += node("ITaskStore-save","interface", 60,  305, 165, 55, "ITaskStore::save\n(持久化状态)", "#f57f17")
s += node("TaskQueue-push", "class",     260, 200, 175, 60, "TaskQueue::push\n(priority pq)", "#c62828")
s += node("Metrics-submit", "class",     260, 305, 175, 55, "MetricsCollector\nrecord_submit()", "#5e35b1")
s += node("Worker",         "class",     470, 200, 175, 60, "Worker::loop\nblocking_pop → work()", "#388e3c")
s += node("TaskResult",     "class",     470, 310, 180, 55, "TaskResult\n{id, ok, attempt, code}", "#ff8f00")
s += node("Scheduler-cb",   "class",     690, 200, 230, 60, "Scheduler::handle_\ncompletion (runtime cb)", "#2e7d32")
s += node("INotifier-ok",   "interface", 690, 100, 210, 55, "INotifier::on_event\n(task.succeeded)", "#e65100")
s += node("Store-update",   "interface", 940, 100, 200, 55, "Store::update_status\n→ Succeeded → remove()", "#f57f17")
s += node("IRetryPolicy",   "interface", 690, 310, 225, 60, "IRetryPolicy::\nnext_delay_ms(attempt)", "#7b1fa2")
s += node("Retry-requeue",  "class",     470, 410, 215, 60, "Store::load\n+ TaskQueue::push\n(重试入队)", "#c62828")
s += node("Metrics-retry",  "class",     320, 410, 140, 55, "MetricsCollector\nrecord_retry()", "#5e35b1")
s += node("INotifier-fail", "interface", 700, 420, 220, 60, "INotifier::on_event\n(task.failed 终态)", "#b71c1c")
s += node("Metrics-fail",   "class",     940, 320, 140, 55, "MetricsCollector\nrecord_failure()", "#5e35b1")
s += node("INotifier-fail2","interface", 940, 420, 140, 55, "MetricsCollector\nrecord_success()", "#5e35b1")
s += '  </g>\n'

s += '  <g data-role="edges">\n'
s += edge_line(142, 155, 142, 200, "Caller",    "Scheduler",      "flow","arr-flow","#0d47a1")
s += edge_line(142, 260, 142, 305, "Scheduler", "ITaskStore-save","flow","arr-flow","#0d47a1")
s += edge_line(225, 230, 260, 230, "Scheduler", "TaskQueue-push", "flow","arr-flow","#0d47a1")
s += edge_line(165, 260, 260, 332, "Scheduler", "Metrics-submit", "flow","arr-flow","#0d47a1")
s += edge_line(435, 230, 470, 230, "TaskQueue-push","Worker",    "flow","arr-flow","#0d47a1")
s += "  <text x='440' y='270' font-family='monospace' font-size='10' fill='#0d47a1'>Task (dequeued)</text>\n"
s += edge_line(557, 260, 560, 310, "Worker",    "TaskResult",     "flow","arr-flow","#0d47a1")
s += "  <text x='565' y='290' font-family='monospace' font-size='10' fill='#0d47a1'>TaskResult</text>\n"
s += edge_line(650, 230, 690, 230, "TaskResult","Scheduler-cb",   "flow","arr-flow","#0d47a1")
s += "  <text x='655' y='270' font-family='monospace' font-size='9' fill='#0d47a1'>runtime callback</text>\n"
# success branch
s += edge_line(805, 200, 795, 155, "Scheduler-cb","INotifier-ok", "flow","arr-flow","#2e7d32")
s += edge_line(920, 200, 1040,155, "Scheduler-cb","Store-update", "flow","arr-flow","#2e7d32")
s += edge_line(920, 195, 940, 320, "Scheduler-cb","Metrics-retry","flow","arr-flow","#2e7d32","4,3")
# retry / fail branch
s += edge_line(802, 260, 802, 310, "Scheduler-cb","IRetryPolicy", "flow","arr-retry","#e65100")
# retry requeue feedback loop
s += edge_line(690, 340, 577, 410, "IRetryPolicy","Retry-requeue","flow","arr-retry","#e65100")
s += edge_line(470, 410, 345, 255, "Retry-requeue","TaskQueue-push","flow","arr-retry","#e65100")
s += "  <text x='375' y='350' font-family='monospace' font-size='10' fill='#e65100'>retry loop</text>\n"
# retry metrics
s += edge_line(470, 440, 320, 437, "Retry-requeue","Metrics-retry","flow","arr-flow","#5e35b1")
# final fail
s += edge_line(815, 370, 815, 420, "IRetryPolicy", "INotifier-fail","flow","arr-retry","#b71c1c")
s += edge_line(815, 370, 1010,420, "IRetryPolicy", "Metrics-fail", "flow","arr-retry","#b71c1c")
s += '</g>\n'

s += """
  <rect x="50" y="500" width="16" height="16" fill="#0d47a1" rx="3"/>
  <text x="75" y="515" font-family="sans-serif" font-size="11">正常流转 (submit → execute → succeed → notify)</text>
  <rect x="280" y="500" width="16" height="16" fill="#e65100" rx="3"/>
  <text x="305" y="515" font-family="sans-serif" font-size="11">重试路径 (fail → retry policy → requeue → retry loop)</text>
  <rect x="590" y="500" width="16" height="16" fill="#b71c1c" rx="3"/>
  <text x="615" y="515" font-family="sans-serif" font-size="11">终态失败 (fail → cap exceeded → task.failed)</text>
"""
s += svg_footer()

with open(os.path.join(DIAG_DIR, "03_dataflow.svg"), "w") as f:
    f.write(s)
print("[OK] 03_dataflow.svg")

# ====================================================================
# 4. Class Relationship
# ====================================================================
s = svg_header("class", "TaskForge 类与接口关系图 (Class / Interface)")
s += '  <g data-role="nodes">\n'
# 接口行
s += node("ITaskStore",      "interface", 70,  80,  225, 55, "ITaskStore  <<interface>>", "#f9a825")
s += node("IRetryPolicy",    "interface", 340, 80,  230, 55, "IRetryPolicy  <<interface>>", "#f9a825")
s += node("INotifier",       "interface", 615, 80,  225, 55, "INotifier  <<interface>>", "#f9a825")
s += node("IClock",          "interface", 885, 80,  225, 55, "IClock  <<interface>>", "#f9a825")
# 实现行
s += node("InMemoryTaskStore","class",    70,  195, 225, 55, "InMemoryTaskStore\n(装配)", "#c62828")
s += node("PersistentTS",    "class",    70,  270, 225, 55, "PersistentTaskStore\n(编译进, 未装配)", "#9e9e9e")
s += node("ExpoBackoff",     "class",    340, 195, 230, 55, "ExponentialBackoffPolicy\n(装配)", "#c62828")
s += node("FixedBackoff",    "class",    340, 270, 225, 55, "FixedBackoffPolicy", "#d32f2f")
s += node("ConsoleNotifier", "class",    615, 195, 225, 50, "ConsoleNotifier", "#e65100")
s += node("CallbackNotifier","class",    615, 265, 225, 50, "CallbackNotifier", "#e65100")
s += node("CompositeNotif",  "class",    615, 335, 225, 55, "CompositeNotifier\n(组合模式, 装配)", "#e65100")
s += node("SystemClock",     "class",    885, 195, 225, 50, "SystemClock  (装配)", "#b71c1c")
# 核心类
s += node("Scheduler",       "class",    70,  420, 280, 65, "Scheduler\n聚合: ITaskStore*, IRetryPolicy*,\nINotifier*, IClock and, MetricsCollector*", "#2e7d32")
s += node("WorkerPool",      "class",    395, 420, 210, 65, "WorkerPool final\n合成: vector Worker[]", "#388e3c")
s += node("Worker",          "class",    640, 420, 200, 65, "Worker\n引用: TaskQueue and\nstd::function callback", "#43a047")
s += node("TaskQueue",       "class",    885, 420, 245, 65, "TaskQueue\n内含: priority_queue\nmutex+condvar", "#c62828")
# 领域实体
s += node("Task",            "class",    70,  530, 160, 55, "Task  struct\n(核心领域实体)", "#ff8f00")
s += node("TaskResult",      "class",    240, 530, 160, 55, "TaskResult  struct", "#ff8f00")
s += node("TaskStatus",      "class",    410, 530, 145, 55, "TaskStatus\nenum class", "#ff8f00")
s += node("Priority",        "class",    565, 530, 115, 55, "Priority\nenum class", "#ff8f00")
s += node("EngineConfig",    "class",    695, 530, 145, 55, "EngineConfig\nstruct", "#00695c")
s += node("MetricsCollector","class",    860, 530, 260, 55, "MetricsCollector\n(concrete, 非接口)", "#5e35b1")
s += '  </g>\n'

s += '  <g data-role="edges">\n'
# implements
s += edge_line(183, 195, 183, 135, "InMemoryTaskStore","ITaskStore",   "implements","arr-impl","#0d47a1")
s += edge_line(183, 270, 183, 135, "PersistentTS",    "ITaskStore",   "implements","arr-impl","#0d47a1")
s += edge_line(455, 195, 455, 135, "ExpoBackoff",     "IRetryPolicy", "implements","arr-impl","#0d47a1")
s += edge_line(455, 270, 455, 135, "FixedBackoff",    "IRetryPolicy", "implements","arr-impl","#0d47a1")
s += edge_line(727, 195, 727, 135, "ConsoleNotifier",  "INotifier",   "implements","arr-impl","#0d47a1")
s += edge_line(727, 265, 727, 135, "CallbackNotifier", "INotifier",   "implements","arr-impl","#0d47a1")
s += edge_line(727, 335, 727, 135, "CompositeNotif",   "INotifier",   "implements","arr-impl","#0d47a1")
s += edge_line(998, 195, 998, 135, "SystemClock",      "IClock",      "implements","arr-impl","#0d47a1")
# Scheduler aggregates (interface pointers)
s += edge_line(210, 420, 183, 135, "Scheduler", "ITaskStore",      "aggregates","arr-aggr","#e65100")
s += edge_line(250, 420, 455, 135, "Scheduler", "IRetryPolicy",    "aggregates","arr-aggr","#e65100")
s += edge_line(290, 420, 727, 135, "Scheduler", "INotifier",       "aggregates","arr-aggr","#e65100")
s += edge_line(330, 420, 998, 135, "Scheduler", "IClock",          "aggregates","arr-aggr","#e65100")
s += edge_line(350, 450, 990, 530, "Scheduler","MetricsCollector", "aggregates","arr-aggr","#e65100")
# Scheduler aggregates WorkerPool
s += edge_line(350, 452, 395, 452, "Scheduler","WorkerPool",       "aggregates","arr-aggr","#e65100")
# WorkerPool composes Workers
s += edge_line(605, 452, 640, 452, "WorkerPool","Worker",         "composes",  "arr-comp","#2e7d32")
# Worker composes/ref TaskQueue
s += edge_line(840, 452, 885, 452, "Worker",     "TaskQueue",      "composes",  "arr-comp","#2e7d32")
# Scheduler uses Task
s += edge_line(180, 485, 150, 530, "Scheduler",  "Task",          "uses",      "arr-uses","#888888")
# Worker uses Task + TaskResult
s += edge_line(740, 485, 150, 530, "Worker",     "Task",          "uses",      "arr-uses","#888888","4,3")
# CompositeNotifier composes children (aggregates)
s += edge_line(727, 335, 727, 265, "CompositeNotif","CallbackNotifier","composes","arr-comp","#2e7d32")
s += edge_line(740, 335, 727, 250, "CompositeNotif","ConsoleNotifier", "composes","arr-comp","#2e7d32")
s += '</g>\n'

s += """
  <rect x="50" y="610" width="16" height="16" fill="#0d47a1" rx="3"/>
  <text x="75" y="625" font-family="sans-serif" font-size="11">implements / inherits</text>
  <rect x="230" y="610" width="16" height="16" fill="#e65100" rx="3"/>
  <text x="255" y="625" font-family="sans-serif" font-size="11">aggregates (interface pointer)</text>
  <rect x="470" y="610" width="16" height="16" fill="#2e7d32" rx="3"/>
  <text x="495" y="625" font-family="sans-serif" font-size="11">composes (ownership)</text>
  <rect x="670" y="610" width="16" height="16" fill="#888" rx="3"/>
  <text x="695" y="625" font-family="sans-serif" font-size="11">uses (reference/copy)</text>
"""
s += svg_footer()

with open(os.path.join(DIAG_DIR, "04_class_relationship.svg"), "w") as f:
    f.write(s)
print("[OK] 04_class_relationship.svg")

# ====================================================================
# 5. Deployment / Runtime Topology
# ====================================================================
W, H = 1200, 850
s = svg_header("deployment", "TaskForge 部署与运行时拓扑", W, H)

# Process box
s += f'''  <rect x="40" y="70" width="1120" height="760" rx="12" fill="#e3f2fd" stroke="#1565c0" stroke-width="2"/>
  <text x="70" y="97" font-family="sans-serif" font-size="16" font-weight="bold" fill="#1565c0">{esc("Single Process (单进程, main())")}</text>
'''

# Main thread
s += '''  <rect x="70" y="115" width="1060" height="110" rx="8" fill="#bbdefb" stroke="#0d47a1" stroke-width="1.5"/>
  <text x="90" y="140" font-family="sans-serif" font-size="13" font-weight="bold" fill="#0d47a1">Main Thread — 主调度循环 (Scheduler::run: sleep 20ms loop)</text>
'''

s += '  <g data-role="nodes">\n'
s += node("Scheduler",          "instance", 100, 160, 180, 45, "Scheduler (编排)", "#2e7d32")
s += node("INotifier-rt",       "instance", 310, 160, 200, 45, "INotifier*\nCompositeNotifier", "#e65100")
s += node("ITaskStore-rt",      "instance", 540, 160, 200, 45, "ITaskStore*\nInMemoryTaskStore", "#c62828")
s += node("MetricsCollector-rt","instance", 770, 160, 200, 45, "MetricsCollector\n(shared_ptr)", "#5e35b1")

# Worker thread pool
s += '''  <rect x="70" y="240" width="1060" height="200" rx="8" fill="#c8e6c9" stroke="#2e7d32" stroke-width="1.5"/>
  <text x="90" y="265" font-family="sans-serif" font-size="13" font-weight="bold" fill="#2e7d32">Worker Thread Pool (4 threads, EngineConfig.worker_count=4)</text>
'''
for i in range(4):
    x = 100 + i * 250
    s += node(f"Worker-{i}", "instance", x, 290, 200, 55, f"Worker-{i}\nstd::thread  (blocking_pop 50ms)", "#388e3c")

# Shared queue
s += '''  <rect x="70" y="455" width="1060" height="100" rx="8" fill="#ffebee" stroke="#c62828" stroke-width="1.5"/>
  <text x="90" y="480" font-family="sans-serif" font-size="13" font-weight="bold" fill="#c62828">Shared TaskQueue (priority_queue + mutex + condvar — 唯一共享汇合点)</text>
'''
s += node("TaskQueue", "instance", 350, 495, 450, 50, "TaskQueue — Scheduler push() / Worker blocking_pop() (线程安全)", "#c62828")

# Support
s += '''  <rect x="70" y="570" width="1060" height="165" rx="8" fill="#ede7f6" stroke="#5e35b1" stroke-width="1.5"/>
  <text x="90" y="595" font-family="sans-serif" font-size="13" font-weight="bold" fill="#5e35b1">Support Services (shared_ptr / reference injected)</text>
'''
s += node("IRetryPolicy-rt",  "instance", 100, 620, 200, 50, "IRetryPolicy*\nExponentialBackoff", "#7b1fa2")
s += node("IClock-rt",        "instance", 330, 620, 180, 50, "IClock ref\nSystemClock", "#00838f")
s += node("EngineConfig-rt",  "instance", 540, 620, 195, 50, "EngineConfig\nworker_count=4", "#00695c")
s += node("CompositeNotif-rt","instance", 765, 620, 220, 50, "CompositeNotifier\n[Console + Callback]", "#e65100")
# Dead code note
s += node("DeadCode-note",    "instance", 100, 690, 600, 55,
          "未实例化: TaskQueueLegacy / PersistentTaskStore / ILogger+ConsoleLogger / FixedBackoffPolicy", "#bdbdbd")
s += '  </g>\n'

s += '  <g data-role="edges">\n'
# main process connects scheduler→workers
s += edge_line(190, 205, 200, 290, "Scheduler", "Worker-0", "connects","arr-conn","#2e7d32")
s += edge_line(190, 205, 450, 290, "Scheduler", "Worker-1", "connects","arr-conn","#2e7d32")
s += edge_line(190, 205, 700, 290, "Scheduler", "Worker-2", "connects","arr-conn","#2e7d32")
s += edge_line(190, 205, 850, 290, "Scheduler", "Worker-3", "connects","arr-conn","#2e7d32")
# Workers→Queue (pop)
s += edge_line(200, 345, 410, 495, "Worker-0", "TaskQueue", "connects","arr-conn","#444")
s += edge_line(450, 345, 520, 495, "Worker-1", "TaskQueue", "connects","arr-conn","#444")
s += edge_line(700, 345, 630, 495, "Worker-2", "TaskQueue", "connects","arr-conn","#444")
s += edge_line(850, 345, 730, 495, "Worker-3", "TaskQueue", "connects","arr-conn","#444")
# callback from workers back to scheduler
s += edge_line(195, 315, 190, 220, "Worker-0", "Scheduler","callback","arr-callback","#e91e63","6,3")
# scheduler→support
s += edge_line(280, 205, 200, 620, "Scheduler","IRetryPolicy-rt",  "connects","arr-conn","#7b1fa2")
s += edge_line(280, 205, 420, 620, "Scheduler","IClock-rt",       "connects","arr-conn","#00838f")
# scheduler→queue
s += edge_line(280, 245, 450, 495, "Scheduler","TaskQueue",       "connects","arr-cont","#1565c0")
s += '</g>\n'

s += f'''  <text x="110" y="775" font-family="sans-serif" font-size="11" fill="#757575" font-style="italic">{esc("运行时回调 (callback): Worker → Scheduler (via std::function, NOT 编译期)")}</text>
  <text x="110" y="793" font-family="sans-serif" font-size="11" fill="#757575" font-style="italic">{esc("所有 worker 共享同一 TaskQueue 实例; Scheduler 在主线程 sleep 循环, 本身不是 worker 线程")}</text>
'''
s += svg_footer()

with open(os.path.join(DIAG_DIR, "05_deployment_topology.svg"), "w") as f:
    f.write(s)
print("[OK] 05_deployment_topology.svg")

print("\nAll 5 SVGs generated successfully.")
