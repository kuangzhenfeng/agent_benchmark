#!/usr/bin/env python3
# coding: utf-8
"""
gen_svgs.py —— 生成 q01 的 5 张带 data-* 标注的 SVG 架构图。

坐标由本脚本计算（端点自动落到节点矩形边缘），可复现：
    python3 gen_svgs.py
仅写 diagrams/*.svg，不触碰引擎源码 / QUESTION.md / svg_check.py。
节点/边的语义标注依据 include/ 与 src/ 的真实架构（见 ANSWER.md）。
"""

import html
import os

VIEW_W, VIEW_H = 1200, 800

# 默认节点半宽 / 半高（矩形尺寸 = 2*hw x 2*hh）
HW, HH = 86, 27

# 关系 -> (颜色, 是否虚线)
KIND_STYLE = {
    "depends":    ("#52606d", False),
    "callback":   ("#d97706", True),
    "flow":       ("#1d4ed8", False),
    "implements": ("#7c3aed", True),
    "inherits":   ("#7c3aed", True),
    "composes":   ("#047857", False),
    "aggregates": ("#059669", True),
    "uses":       ("#64748b", True),
    "contains":   ("#475569", False),
    "connects":   ("#1d4ed8", False),
    "runs-on":    ("#b45309", True),
}

NODE_STYLE = {
    "app":        ("#eaf2fb", "#2f6db5"),
    "core":       ("#e3f3ea", "#1f8a4c"),
    "exec":       ("#fdf3e1", "#c2761b"),
    "contract":   ("#efeaf7", "#7c3aed"),
    "infra":      ("#f1f2f4", "#475569"),
    "dead":       ("#fbeaea", "#c0392b"),
    "impl":       ("#ffffff", "#7c3aed"),
    "instance":   ("#e3f3ea", "#1f8a4c"),
    "thread":     ("#eaf2fb", "#2f6db5"),
    "data":       ("#eef6ff", "#1d4ed8"),
}


def esc(s):
    return html.escape(str(s), quote=True)


class Canvas:
    def __init__(self, style, vw=VIEW_W, vh=VIEW_H, title=""):
        self.style = style
        self.vw, self.vh = vw, vh
        self.title = title
        self.bg = []      # 背景层 / 层条 / 标签
        self.nodes = {}   # id -> dict(cx,cy,hw,hh,kind,title,sub,style)
        self.edges = []   # dict(from,to,kind,label)
        self.legends = []

    def band(self, x, y, w, h, fill, stroke, label, label_color="#33475b", label_size=13, rx=10):
        self.bg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" opacity="0.6"/>')
        if label:
            self.bg.append(f'<text x="{x+16}" y="{y+22}" font-size="{label_size}" font-weight="700" fill="{label_color}">{esc(label)}</text>')

    def note(self, x, y, text, size=11, color="#5a6b7b", anchor="start", weight="400"):
        self.bg.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" text-anchor="{anchor}" font-weight="{weight}">{esc(text)}</text>')

    def add_node(self, nid, cx, cy, title, sub="", kind="class", style="infra",
                 hw=HW, hh=HH, title_size=15, sub_size=10.5):
        self.nodes[nid] = dict(cx=cx, cy=cy, hw=hw, hh=hh, kind=kind, title=title,
                               sub=sub, style=style, ts=title_size, ss=sub_size)

    def add_edge(self, a, b, kind, label=""):
        self.edges.append(dict(a=a, b=b, kind=kind, label=label))

    def legend(self, x, y, items):
        self.legends.append((x, y, items))

    def _edge_endpoints(self, a, b):
        na, nb = self.nodes[a], self.nodes[b]
        return _rect_edge(na, nb), _rect_edge(nb, na)

    def render(self):
        out = []
        out.append(f'<?xml version="1.0" encoding="UTF-8"?>')
        out.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.vw} {self.vh}" '
                   f'font-family="Helvetica, Arial, sans-serif" data-style="{self.style}">')
        out.append('<defs>')
        for kind, (color, dash) in KIND_STYLE.items():
            out.append(f'<marker id="mk-{kind}" viewBox="0 0 10 10" refX="9" refY="5" '
                       f'markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
                       f'<path d="M0,0 L10,5 L0,10 z" fill="{color}"/></marker>')
        out.append('</defs>')

        out.append(f'<rect x="0" y="0" width="{self.vw}" height="{self.vh}" fill="#fbfcfd"/>')
        out.append(f'<text x="{self.vw/2}" y="38" font-size="22" font-weight="700" '
                   f'text-anchor="middle" fill="#1f2d3d">{esc(self.title)}</text>')

        out.extend(self.bg)

        # 边（先画，后被节点矩形遮盖穿越段）
        out.append('<g data-role="edges">')
        for e in self.edges:
            color, dash = KIND_STYLE.get(e["kind"], ("#52606d", False))
            (x1, y1), (x2, y2) = self._edge_endpoints(e["a"], e["b"])
            attr = ' stroke-dasharray="6 4"' if dash else ''
            out.append(f'<line data-from="{esc(e["a"])}" data-to="{esc(e["b"])}" '
                       f'data-kind="{esc(e["kind"])}" x1="{x1:.1f}" y1="{y1:.1f}" '
                       f'x2="{x2:.1f}" y2="{y2:.1f}" stroke="{color}" stroke-width="1.7" '
                       f'fill="none" marker-end="url(#mk-{e["kind"]})"{attr}/>')
            if e["label"]:
                mx, my = (x1 + x2) / 2, (y1 + y2) / 2
                out.append(f'<text x="{mx:.1f}" y="{my-4:.1f}" font-size="10.5" fill="{color}" '
                           f'text-anchor="middle" font-weight="600">{esc(e["label"])}</text>')
                out.append(f'<rect x="{mx-2}" y="{my-15}" width="1" height="1" fill="none"/>')
        out.append('</g>')

        # 节点
        out.append('<g data-role="nodes">')
        for nid, n in self.nodes.items():
            fill, stroke = NODE_STYLE.get(n["style"], ("#fff", "#475569"))
            x = n["cx"] - n["hw"]
            y = n["cy"] - n["hh"]
            out.append(f'<g data-id="{esc(nid)}" data-kind="{esc(n["kind"])}">')
            out.append(f'<rect x="{x}" y="{y}" width="{2*n["hw"]}" height="{2*n["hh"]}" '
                       f'rx="9" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
            if n["sub"]:
                out.append(f'<text x="{n["cx"]}" y="{n["cy"]-3}" text-anchor="middle" '
                           f'font-size="{n["ts"]}" font-weight="700" fill="#1f2d3d">{esc(n["title"])}</text>')
                out.append(f'<text x="{n["cx"]}" y="{n["cy"]+13}" text-anchor="middle" '
                           f'font-size="{n["ss"]}" fill="#5a6b7b">{esc(n["sub"])}</text>')
            else:
                out.append(f'<text x="{n["cx"]}" y="{n["cy"]+5}" text-anchor="middle" '
                           f'font-size="{n["ts"]}" font-weight="700" fill="#1f2d3d">{esc(n["title"])}</text>')
            out.append('</g>')
        out.append('</g>')

        # 图例
        for (lx, ly, items) in self.legends:
            out.append(f'<rect x="{lx}" y="{ly}" width="300" height="{18+22*len(items)}" '
                       f'rx="8" fill="#ffffff" stroke="#d7dde5"/>')
            out.append(f'<text x="{lx+12}" y="{ly+20}" font-size="12" font-weight="700" fill="#33475b">图例</text>')
            for i, (k, desc) in enumerate(items):
                color, dash = KIND_STYLE.get(k, ("#52606d", False))
                da = ' stroke-dasharray="6 4"' if dash else ''
                yy = ly + 40 + i * 22
                out.append(f'<line x1="{lx+12}" y1="{yy}" x2="{lx+44}" y2="{yy}" '
                           f'stroke="{color}" stroke-width="2"{da} marker-end="url(#mk-{k})"/>')
                out.append(f'<text x="{lx+52}" y="{yy+4}" font-size="11.5" fill="#33475b">{esc(desc)}</text>')

        out.append('</svg>')
        return "\n".join(out)


def _rect_edge(src, dst):
    """从 src 节点矩形边缘，朝 dst 中心方向出来的点。"""
    cx, cy = src["cx"], src["cy"]
    hw, hh = src["hw"], src["hh"]
    dx = dst["cx"] - cx
    dy = dst["cy"] - cy
    if dx == 0 and dy == 0:
        return (cx, cy)
    sx = hw / abs(dx) if dx != 0 else float("inf")
    sy = hh / abs(dy) if dy != 0 else float("inf")
    t = min(sx, sy)
    return (cx + dx * t, cy + dy * t)


# ---------------------------------------------------------------------------
# 图1：分层架构图
# ---------------------------------------------------------------------------
def build_layered():
    c = Canvas("layered", title="TaskForge 分层架构图（按真实 #include / 类型依赖方向分层）")
    # 层背景条
    c.band(40, 76, 880, 100, "#eaf2fb", "#bcd5ee", "L4 应用入口 / 组合根（main：装配所有具体实现）")
    c.band(40, 196, 880, 100, "#e3f3ea", "#a7dcc0", "L3 编排核心（Orchestration）")
    c.band(40, 316, 880, 100, "#fdf3e1", "#eccfa3", "L2 执行单元（Execution）")
    c.band(40, 436, 880, 100, "#efeaf7", "#cdbce6", "L1 领域契约与汇合（Contracts / Concurrency）")
    c.band(40, 556, 880, 100, "#f1f2f4", "#cfd4dc", "L0 基础设施 / 值类型（Foundation）")
    c.band(940, 316, 240, 100, "#fbeaea", "#e6b8b8", "死代码 / 历史遗留（编译进产物，无人调用）", label_color="#b03a3a")

    # 节点
    c.add_node("main", 600, 126, "main", "src/main.cpp · 组合根", "app", style="app")
    c.add_node("Scheduler", 600, 246, "Scheduler", "scheduler.hpp:19 · 编排核心", "core", style="core")
    c.add_node("WorkerPool", 300, 366, "WorkerPool", "worker.hpp:37", "exec", style="exec")
    c.add_node("Worker", 500, 366, "Worker", "worker.hpp:15", "exec", style="exec")
    c.add_node("TaskQueue", 700, 366, "TaskQueue", "task_queue.hpp:13 · 优先级队列(在用)", "exec", style="exec")
    c.add_node("ITaskStore", 180, 486, "ITaskStore", "task_store.hpp:11 · 抽象接口", "contract", style="contract")
    c.add_node("INotifier", 360, 486, "INotifier", "notifier.hpp:11 · 抽象接口", "contract", style="contract")
    c.add_node("IRetryPolicy", 540, 486, "IRetryPolicy", "retry_policy.hpp:9 · 抽象接口", "contract", style="contract")
    c.add_node("IClock", 720, 486, "IClock", "clock.hpp:7 · 抽象接口", "contract", style="contract")
    c.add_node("Task", 180, 606, "Task", "task.hpp:54 · 实体/值类型", "data", style="infra")
    c.add_node("MetricsCollector", 380, 606, "MetricsCollector", "metrics.hpp:8 · 统计(叶子,非核心)", "infra", style="infra")
    c.add_node("EngineConfig", 560, 606, "EngineConfig", "config.hpp:8 · 运行配置", "infra", style="infra")
    c.add_node("TaskQueueLegacy", 1060, 366, "TaskQueueLegacy", "task_queue.hpp:42 · 注释自称“通用队列”", "dead", style="dead", title_size=12.5, sub_size=9.5)

    # 依赖边（真实 #include / 类型成员依赖，自上而下）
    for a, b in [
        ("main", "Scheduler"),
        ("main", "MetricsCollector"),
        ("main", "INotifier"),
        ("main", "IClock"),
        ("main", "EngineConfig"),
        ("Scheduler", "WorkerPool"),
        ("Scheduler", "TaskQueue"),
        ("Scheduler", "ITaskStore"),
        ("Scheduler", "INotifier"),
        ("Scheduler", "IRetryPolicy"),
        ("Scheduler", "IClock"),
        ("Scheduler", "MetricsCollector"),
        ("Scheduler", "EngineConfig"),
        ("WorkerPool", "Worker"),
        ("Worker", "TaskQueue"),
        ("Worker", "Task"),
        ("TaskQueue", "Task"),
        ("ITaskStore", "Task"),
        ("INotifier", "Task"),
    ]:
        c.add_edge(a, b, "depends")

    c.note(600, 705, "分层依据真实依赖方向：main 仅装配具体实现；Scheduler 只持有抽象接口指针；TaskQueueLegacy 全仓库无人调用 → 不进入主流依赖，归入死代码侧栏。",
           size=11.5, anchor="middle")
    c.legend(950, 600, [("depends", "编译期 / 类型依赖（上层 → 下层）")])
    return c


# ---------------------------------------------------------------------------
# 图2：组件依赖图
# ---------------------------------------------------------------------------
def build_component():
    c = Canvas("component", title="TaskForge 组件依赖图（编译期依赖 depends · 运行时回调 callback）")
    c.add_node("main", 160, 400, "main", "src/main.cpp", "app", style="app")
    c.add_node("Scheduler", 470, 400, "Scheduler", "scheduler.hpp", "core", style="core")
    c.add_node("EngineConfig", 470, 150, "EngineConfig", "config.hpp", "infra", style="infra", hw=90, hh=24)
    c.add_node("WorkerPool", 760, 250, "WorkerPool", "worker.hpp:37", "exec", style="exec")
    c.add_node("Worker", 760, 400, "Worker", "worker.hpp:15", "exec", style="exec")
    c.add_node("TaskQueue", 760, 555, "TaskQueue", "task_queue.hpp:13(在用)", "exec", style="exec")
    c.add_node("ITaskStore", 1050, 170, "ITaskStore", "task_store.hpp:11", "contract", style="contract")
    c.add_node("INotifier", 1050, 305, "INotifier", "notifier.hpp:11", "contract", style="contract")
    c.add_node("IRetryPolicy", 1050, 440, "IRetryPolicy", "retry_policy.hpp:9", "contract", style="contract")
    c.add_node("IClock", 1050, 575, "IClock", "clock.hpp:7", "contract", style="contract")
    c.add_node("MetricsCollector", 300, 640, "MetricsCollector", "metrics.hpp:8", "infra", style="infra")
    c.add_node("Task", 160, 640, "Task", "task.hpp:54", "data", style="infra")

    # 编译期依赖
    for a, b in [
        ("main", "Scheduler"),
        ("main", "MetricsCollector"),
        ("main", "EngineConfig"),
        ("Scheduler", "WorkerPool"),
        ("Scheduler", "TaskQueue"),
        ("Scheduler", "ITaskStore"),
        ("Scheduler", "INotifier"),
        ("Scheduler", "IRetryPolicy"),
        ("Scheduler", "IClock"),
        ("Scheduler", "MetricsCollector"),
        ("Scheduler", "EngineConfig"),
        ("Scheduler", "Task"),
        ("WorkerPool", "Worker"),
        ("Worker", "TaskQueue"),
        ("Worker", "Task"),
        ("TaskQueue", "Task"),
        ("ITaskStore", "Task"),
        ("INotifier", "Task"),
    ]:
        c.add_edge(a, b, "depends")

    # 运行时回调（反向，非编译期循环依赖）
    c.add_edge("WorkerPool", "Scheduler", "callback", "completion_handler 注入(scheduler.cpp:48)")
    c.add_edge("Worker", "Scheduler", "callback", "on_done_ 回调(worker.cpp:69)")

    c.note(600, 705,
           "关键区分：Worker→Scheduler 是经 set_completion_handler 运行时注入的回调（worker.hpp 不 include scheduler.hpp），"
           "不是编译期循环依赖。回调边用橙色虚线，编译期依赖用灰实线。",
           size=11.5, anchor="middle")
    c.legend(40, 110, [
        ("depends", "编译期依赖（#include / 类型）"),
        ("callback", "仅运行时回调（注入的 handler，非类型依赖）"),
    ])
    return c


# ---------------------------------------------------------------------------
# 图3：数据流图
# ---------------------------------------------------------------------------
def build_dataflow():
    c = Canvas("dataflow", vw=1400, vh=820,
               title="TaskForge 数据流图（一次任务：提交→入队→执行→结算→重试反馈环→终态通知）")

    c.add_node("submit", 110, 410, "Client", "submit() / submitBatch()", "app", style="app", hw=70, hh=30)
    c.add_node("Scheduler.submit", 320, 410, "Scheduler::submit", "scheduler.cpp:22", "core", style="core", hw=95, hh=28)
    c.add_node("store.save", 320, 210, "ITaskStore::save", "save(task)", "contract", style="contract", hw=95, hh=28)
    c.add_node("queue.push", 560, 410, "TaskQueue::push", "优先级队列(task_queue.cpp:3)", "exec", style="exec", hw=95, hh=28)
    c.add_node("worker.exec", 820, 410, "Worker::loop", "blocking_pop + 执行 work()", "exec", style="exec", hw=88, hh=30)
    c.add_node("TaskResult", 1050, 250, "TaskResult", "{id,ok,attempt,code}", "data", style="data", hw=78, hh=30)
    c.add_node("handle_completion", 1050, 410, "handle_completion", "scheduler.cpp:82 结算", "core", style="core", hw=100, hh=30)
    c.add_node("store.retry", 820, 610, "ITaskStore", "load+save(Retrying)", "contract", style="contract", hw=80, hh=28)
    c.add_node("notifier.event", 1280, 410, "INotifier::on_event", "task.succeeded / failed", "contract", style="contract", hw=95, hh=28)

    # 数据流
    c.add_edge("submit", "Scheduler.submit", "flow", "Task")
    c.add_edge("Scheduler.submit", "store.save", "flow", "save(task)")
    c.add_edge("Scheduler.submit", "queue.push", "flow", "push(task)")
    c.add_edge("queue.push", "worker.exec", "flow", "blocking_pop")
    c.add_edge("worker.exec", "TaskResult", "flow", "产出 result")
    c.add_edge("worker.exec", "handle_completion", "flow", "回调 result(运行时)")
    c.add_edge("handle_completion", "store.retry", "flow", "load + save(attempt++)")
    c.add_edge("store.retry", "queue.push", "flow", "re-push(重试反馈环)")  # 回环
    c.add_edge("handle_completion", "notifier.event", "flow", "终态事件")
    c.add_edge("handle_completion", "store.save", "flow", "update_status / remove(成功)")

    c.note(700, 760,
           "重试反馈环：handle_completion 在仍有重试机会时，将 attempt 同步回 store 并重新 push 进队列，形成 worker←→queue 的回环（scheduler.cpp:101-109）。"
           "注意 worker 本地累加的 attempt 必须写回 store，否则永不递增（scheduler.cpp:105）。",
           size=11.5, anchor="middle")
    c.legend(40, 700, [("flow", "有向数据流（标注流转内容）")])
    return c


# ---------------------------------------------------------------------------
# 图4：类与模块关系图
# ---------------------------------------------------------------------------
def build_class():
    c = Canvas("class", vw=1420, vh=860,
               title="TaskForge 类关系图（implements / composes / aggregates / uses；依赖画到抽象接口）")

    # 接口列
    c.add_node("ITaskStore", 200, 170, "ITaskStore", "«interface»", "contract", style="contract")
    c.add_node("INotifier", 200, 360, "INotifier", "«interface»", "contract", style="contract")
    c.add_node("IRetryPolicy", 200, 540, "IRetryPolicy", "«interface»", "contract", style="contract")
    c.add_node("IClock", 200, 700, "IClock", "«interface»", "contract", style="contract")

    # 实现列
    c.add_node("InMemoryTaskStore", 540, 120, "InMemoryTaskStore", "main 实际装配", "impl", style="impl", hw=110, hh=24, title_size=13)
    c.add_node("PersistentTaskStore", 540, 215, "PersistentTaskStore", "编译进产物·未实例化", "dead", style="dead", hw=110, hh=24, title_size=12.5, sub_size=9.5)
    c.add_node("ConsoleNotifier", 540, 310, "ConsoleNotifier", "由 main 装入 Composite", "impl", style="impl", hw=100, hh=24, title_size=13)
    c.add_node("CallbackNotifier", 540, 385, "CallbackNotifier", "由 main 装入 Composite", "impl", style="impl", hw=100, hh=24, title_size=13)
    c.add_node("CompositeNotifier", 540, 460, "CompositeNotifier", "main 装配的根通知器", "impl", style="impl", hw=110, hh=24, title_size=13)
    c.add_node("FixedBackoffPolicy", 540, 515, "FixedBackoffPolicy", "编译进产物·未实例化", "dead", style="dead", hw=110, hh=24, title_size=12.5, sub_size=9.5)
    c.add_node("ExponentialBackoffPolicy", 540, 590, "ExponentialBackoffPolicy", "main 实际装配", "impl", style="impl", hw=130, hh=24, title_size=13)
    c.add_node("SystemClock", 540, 700, "SystemClock", "main 实际装配", "impl", style="impl", hw=90, hh=24, title_size=13)

    # 编排 / 聚合者
    c.add_node("main", 900, 90, "main", "组合根：装配具体实现", "app", style="app", hw=90, hh=26)
    c.add_node("Scheduler", 900, 270, "Scheduler", "只持接口指针", "core", style="core")
    c.add_node("WorkerPool", 900, 450, "WorkerPool", "拥有 N 个 Worker", "exec", style="exec")
    c.add_node("Worker", 900, 610, "Worker", "持有回调 handler", "exec", style="exec")

    # 值 / 工具
    c.add_node("TaskQueue", 1190, 360, "TaskQueue", "值成员(在用)", "exec", style="exec")
    c.add_node("MetricsCollector", 1190, 450, "MetricsCollector", "shared_ptr", "infra", style="infra")
    c.add_node("Task", 1190, 540, "Task", "值/实体", "data", style="infra")
    c.add_node("TaskResult", 1190, 610, "TaskResult", "Worker 产出", "data", style="data")

    # implements
    for a, b in [
        ("InMemoryTaskStore", "ITaskStore"),
        ("PersistentTaskStore", "ITaskStore"),
        ("ConsoleNotifier", "INotifier"),
        ("CallbackNotifier", "INotifier"),
        ("CompositeNotifier", "INotifier"),
        ("FixedBackoffPolicy", "IRetryPolicy"),
        ("ExponentialBackoffPolicy", "IRetryPolicy"),
        ("SystemClock", "IClock"),
    ]:
        c.add_edge(a, b, "implements")

    # 组合模式：CompositeNotifier 聚合子通知器
    c.add_edge("CompositeNotifier", "INotifier", "aggregates", "children_(组合模式)")

    # Scheduler 聚合抽象接口（不依赖具体类）
    c.add_edge("Scheduler", "ITaskStore", "aggregates", "shared_ptr<ITaskStore>")
    c.add_edge("Scheduler", "IRetryPolicy", "aggregates", "unique_ptr<IRetryPolicy>")
    c.add_edge("Scheduler", "INotifier", "aggregates", "shared_ptr<INotifier>")
    c.add_edge("Scheduler", "IClock", "uses", "IClock&")
    c.add_edge("Scheduler", "MetricsCollector", "aggregates", "shared_ptr")
    # Scheduler 值成员组合
    c.add_edge("Scheduler", "TaskQueue", "composes", "值成员 queue_")
    c.add_edge("Scheduler", "WorkerPool", "composes", "值成员 pool_")
    # WorkerPool / Worker
    c.add_edge("WorkerPool", "Worker", "composes", "unique_ptr[]")
    c.add_edge("Worker", "TaskQueue", "aggregates", "TaskQueue&")
    c.add_edge("Worker", "TaskResult", "uses", "产出并回调")
    # 组合根装配
    c.add_edge("main", "InMemoryTaskStore", "composes", "make_shared")
    c.add_edge("main", "ExponentialBackoffPolicy", "composes", "make_unique")
    c.add_edge("main", "CompositeNotifier", "composes", "make_shared")
    c.add_edge("main", "SystemClock", "composes", "栈对象")
    c.add_edge("main", "MetricsCollector", "composes", "make_shared")

    c.note(710, 805,
           "陷阱处理：Scheduler 对存储/重试/通知的聚合边一律画到抽象接口（ITaskStore/IRetryPolicy/INotifier），不画到具体类；"
           "具体实现由组合根 main 装配。PersistentTaskStore / FixedBackoffPolicy 虽实现接口但运行时未被实例化（红色，死代码标注）。",
           size=11.5, anchor="middle")
    c.legend(40, 90, [
        ("implements", "实现接口（虚线）"),
        ("composes", "值组合 / owns"),
        ("aggregates", "聚合（指针/引用，虚线）"),
        ("uses", "使用（虚线）"),
    ])
    return c


# ---------------------------------------------------------------------------
# 图5：部署 / 运行时拓扑图
# ---------------------------------------------------------------------------
def build_deployment():
    c = Canvas("deployment", vw=1200, vh=880,
               title="TaskForge 部署 / 运行时拓扑图（进程 · 线程 · 实例 · 共享汇合点）")

    # 进程大框
    c.band(40, 70, 820, 720, "#f4f8fc", "#bcd5ee", "")
    c.note(60, 96, "Process：TaskForge（./run_public_checks.sh 编译出的单一可执行）", size=13, weight="700", color="#2f6db5")

    # 线程
    c.add_node("main-thread", 180, 150, "main thread", "运行 Scheduler::run()", "thread", style="thread", hw=80, hh=26, title_size=13)
    c.add_node("stopper-thread", 470, 150, "stopper thread", "800ms 后调 stop()", "thread", style="thread", hw=85, hh=26, title_size=13)
    c.add_node("worker-threads", 720, 150, "4 × worker threads", "config.worker_count=4", "thread", style="thread", hw=100, hh=26, title_size=13)

    # Scheduler 实例
    c.add_node("Scheduler", 180, 250, "Scheduler", "唯一实例(栈对象)", "core", style="core")

    # 共享汇合点
    c.add_node("TaskQueue", 180, 410, "TaskQueue", "共享汇合点(优先级队列)", "exec", style="exec", hw=95, hh=30)

    # WorkerPool + 4 worker
    c.add_node("WorkerPool", 460, 410, "WorkerPool", "拥有 4 个 Worker", "exec", style="exec", hw=90, hh=28)
    c.add_node("Worker1", 430, 540, "Worker#1", "线程", "instance", style="instance", hw=60, hh=22, title_size=12)
    c.add_node("Worker2", 510, 540, "Worker#2", "线程", "instance", style="instance", hw=60, hh=22, title_size=12)
    c.add_node("Worker3", 430, 600, "Worker#3", "线程", "instance", style="instance", hw=60, hh=22, title_size=12)
    c.add_node("Worker4", 510, 600, "Worker#4", "线程", "instance", style="instance", hw=60, hh=22, title_size=12)

    # 其它实例（main 装配的具体实现）
    c.add_node("InMemoryTaskStore", 720, 320, "InMemoryTaskStore", "store_ 实际指向", "instance", style="instance", hw=120, hh=26, title_size=12.5)
    c.add_node("ExponentialBackoffPolicy", 720, 390, "ExponentialBackoffPolicy", "retry_ 实际指向", "instance", style="instance", hw=140, hh=26, title_size=12)
    c.add_node("CompositeNotifier", 720, 460, "CompositeNotifier", "notifier_ 实际指向", "instance", style="instance", hw=110, hh=26, title_size=12.5)
    c.add_node("ConsoleNotifier", 660, 560, "ConsoleNotifier", "子通知器", "instance", style="instance", hw=95, hh=22, title_size=11.5)
    c.add_node("CallbackNotifier", 800, 560, "CallbackNotifier", "子通知器", "instance", style="instance", hw=100, hh=22, title_size=11.5)
    c.add_node("MetricsCollector", 720, 630, "MetricsCollector", "metrics_ 指向", "instance", style="instance", hw=110, hh=24, title_size=12)
    c.add_node("SystemClock", 720, 690, "SystemClock", "clock_(栈对象)", "instance", style="instance", hw=90, hh=24, title_size=12)

    # 归属 contains
    c.add_edge("Scheduler", "TaskQueue", "contains")
    c.add_edge("Scheduler", "WorkerPool", "contains")
    c.add_edge("Scheduler", "InMemoryTaskStore", "contains")
    c.add_edge("Scheduler", "ExponentialBackoffPolicy", "contains")
    c.add_edge("Scheduler", "CompositeNotifier", "contains")
    c.add_edge("Scheduler", "MetricsCollector", "contains")
    c.add_edge("Scheduler", "SystemClock", "contains")
    c.add_edge("WorkerPool", "Worker1", "contains")
    c.add_edge("WorkerPool", "Worker2", "contains")
    c.add_edge("WorkerPool", "Worker3", "contains")
    c.add_edge("WorkerPool", "Worker4", "contains")
    c.add_edge("CompositeNotifier", "ConsoleNotifier", "contains")
    c.add_edge("CompositeNotifier", "CallbackNotifier", "contains")

    # runs-on
    c.add_edge("Scheduler", "main-thread", "runs-on", "run()")
    c.add_edge("Worker1", "worker-threads", "runs-on")
    c.add_edge("Worker2", "worker-threads", "runs-on")

    # connects（共享拉取）
    c.add_edge("Worker1", "TaskQueue", "connects", "共享 blocking_pop")
    c.add_edge("Worker4", "TaskQueue", "connects", "共享 blocking_pop")

    # callback
    c.add_edge("WorkerPool", "Scheduler", "callback", "注入 completion_handler")

    # 编译进产物但运行时未实例化（侧栏）
    c.band(900, 70, 270, 300, "#fbeaea", "#e6b8b8", "编译进产物，运行时未实例化 / 无人调用", label_color="#b03a3a")
    c.add_node("PersistentTaskStore", 1035, 150, "PersistentTaskStore", "无人 new", "dead", style="dead", hw=115, hh=24, title_size=12, sub_size=9.5)
    c.add_node("FixedBackoffPolicy", 1035, 215, "FixedBackoffPolicy", "无人 new", "dead", style="dead", hw=115, hh=24, title_size=12, sub_size=9.5)
    c.add_node("ConsoleLogger", 1035, 280, "ConsoleLogger / ILogger", "无人 new", "dead", style="dead", hw=120, hh=24, title_size=12, sub_size=9.5)
    c.add_node("TaskQueueLegacy", 1035, 345, "TaskQueueLegacy", "全仓库无人调用", "dead", style="dead", hw=115, hh=24, title_size=12, sub_size=9.5)

    c.note(430, 815,
           "运行时真实拓扑：1 个 Scheduler 栈实例 + 1 个共享 TaskQueue + 4 个 worker 线程（worker_count=4，ConfigLoader::default_for(\"default\")）。"
           "右侧红框为编译进产物但 main 从未实例化或无人调用的类型。",
           size=11.5, anchor="middle")
    c.legend(900, 560, [
        ("contains", "归属 / owns（实线）"),
        ("runs-on", "运行于某线程（虚线）"),
        ("connects", "实例间连接（共享拉取）"),
        ("callback", "运行时注入的回调"),
    ])
    return c


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "diagrams")
    os.makedirs(out_dir, exist_ok=True)

    builders = [
        ("01_layered_architecture.svg", build_layered),
        ("02_component_dependency.svg", build_component),
        ("03_dataflow.svg", build_dataflow),
        ("04_class_relationship.svg", build_class),
        ("05_deployment_topology.svg", build_deployment),
    ]
    for fname, fn in builders:
        svg = fn().render()
        path = os.path.join(out_dir, fname)
        with open(path, "w", encoding="utf-8") as f:
            f.write(svg)
        print(f"[OK] wrote {fname}")


if __name__ == "__main__":
    main()
