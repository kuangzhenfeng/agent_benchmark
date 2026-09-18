#!/usr/bin/env python3
"""按 evaluation-agents.md 生成隔离的多 Agent 作答目录。"""

from __future__ import annotations

import argparse
import json
import re
import shlex
import shutil
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
QUESTION_DIR = ROOT / ".claude/skills/agent-benchmark/presets/otter-apple-peeler"


@dataclass(frozen=True)
class Participant:
    harness: str
    model: str
    selector: str

    @property
    def name(self) -> str:
        return f"{self.harness} + {self.model}"

    @property
    def slug(self) -> str:
        value = re.sub(r"[^a-z0-9]+", "-", self.name.lower()).strip("-")
        if not value:
            raise ValueError(f"无法为参评对象生成目录名：{self.name!r}")
        return value


def registered_participants(path: Path) -> list[Participant]:
    """只解析“已登记组合”表，不对 harness 与模型做笛卡尔积。"""
    text = path.read_text(encoding="utf-8")
    match = re.search(r"## 已登记组合\n(?P<body>.*?)(?=\n## |\Z)", text, re.S)
    if not match:
        raise ValueError(f"{path} 缺少“已登记组合”章节")

    rows: list[Participant] = []
    for line in match.group("body").splitlines():
        if not line.startswith("|") or "---" in line or "Harness ID" in line:
            continue
        cells = [cell.strip().strip("`") for cell in line.strip().strip("|").split("|")]
        if len(cells) >= 3 and all(cells[:3]):
            rows.append(Participant(cells[0], cells[1], cells[2]))
    if not rows:
        raise ValueError(f"{path} 的“已登记组合”表为空")
    return rows


def registered_harnesses(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8")
    match = re.search(r"## Harness\n(?P<body>.*?)(?=\n## |\Z)", text, re.S)
    if not match:
        raise ValueError(f"{path} 缺少 Harness 章节")
    values = set()
    for line in match.group("body").splitlines():
        if line.startswith("|") and "---" not in line and "Harness ID" not in line:
            cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
            if cells and cells[0]:
                values.add(cells[0])
    return values


def registered_models(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    match = re.search(r"## 模型\n(?P<body>.*?)(?=\n## |\Z)", text, re.S)
    if not match:
        raise ValueError(f"{path} 缺少模型章节")
    values = {}
    for line in match.group("body").splitlines():
        if line.startswith("|") and "---" not in line and "模型 ID" not in line:
            cells = [cell.strip().strip("`") for cell in line.strip().strip("|").split("|")]
            if len(cells) >= 2 and cells[0] and cells[1]:
                values[cells[0]] = cells[1]
    return values


def resolve_participants(
    all_registered: list[Participant],
    harnesses: set[str],
    models: dict[str, str],
    requested: list[str],
    use_all: bool,
) -> list[Participant]:
    if use_all and requested:
        raise ValueError("--all 与 --agent 不能同时使用")

    selected: list[Participant] = []
    if use_all or not requested:
        selected = list(all_registered)
    else:
        for name in requested:
            parts = [part.strip() for part in name.split("+")]
            if len(parts) != 2 or not all(parts):
                raise ValueError(f"参评对象格式应为 'harness + model'：{name!r}")
            harness, model = parts
            harness_key = next((item for item in harnesses if item.lower() == harness.lower()), None)
            model_key = next((item for item in models if item.lower() == model.lower()), None)
            if harness_key is None or model_key is None:
                raise ValueError(f"未登记参评对象：{name!r}；请先更新 evaluation-agents.md")
            selected.append(Participant(harness_key, model_key, models[model_key]))

    slugs = [item.slug for item in selected]
    if len(slugs) != len(set(slugs)):
        raise ValueError("参评对象生成了重复目录名")
    return selected


def render_participants(items: list[Participant], run_id: str) -> str:
    lines = [
        "# 参评对象",
        "",
        f"轮次：`{run_id}`",
        "",
        "| 参评对象 | Harness | 模型 | CLI 选择器 | 作答目录 |",
        "|---|---|---|---|---|",
    ]
    for item in items:
        lines.append(
            f"| {item.name} | {item.harness} | {item.model} | `{item.selector}` | `agents/{item.slug}/` |"
        )
    lines.extend(["", "本轮仅汇总展示结果，不进行评分或排名。", ""])
    return "\n".join(lines)


def render_run_readme(run_id: str, items: list[Participant]) -> str:
    return f"""# 多 Agent SVG 动画评测

轮次：`{run_id}`  
参评对象：{len(items)} 个

每个对象在自己的 `agents/<slug>/` 目录内作答，收到完全一致的 `QUESTION.md`。完成后运行：

```bash
python3 .claude/skills/agent-benchmark/scripts/build_showcase.py --run {run_id}
```

该命令会把所有对象的 `showcase/index.html` 安全嵌入仓库根目录 `showcase/index.html`，并保存一份轮次内的 `showcase.html`。不生成评分、排名或匿名评分材料。
"""


COMMANDS = {
    "claude code": 'claude -p "$(cat QUESTION.md)" --model {model}',
    "codex": 'codex exec "$(cat QUESTION.md)" -m {model} -s workspace-write',
    "opencode": 'opencode run "$(cat QUESTION.md)" -m {model}',
    "omp": 'omp -p "$(cat QUESTION.md)" --model {model}',
}


def render_launch(run_dir: Path, items: list[Participant]) -> str:
    lines = [
        "# 启动命令",
        "",
        "以下命令均把 `QUESTION.md` 原文作为完整提示词，每个对象使用独立工作目录。执行前请核对本机 CLI、模型映射与权限。",
        "",
    ]
    for item in items:
        template = COMMANDS.get(item.harness.lower())
        if template is None:
            raise ValueError(f"缺少 harness 启动模板：{item.harness}")
        agent_dir = run_dir / "agents" / item.slug
        command = template.format(model=shlex.quote(item.selector))
        lines.extend([f"## {item.name}", "", "```bash", f"cd {shlex.quote(str(agent_dir))}", command, "```", ""])
    return "\n".join(lines)


def create_run(run_dir: Path, run_id: str, items: list[Participant]) -> None:
    if run_dir.exists():
        raise FileExistsError(f"拒绝覆盖已有轮次：{run_dir}")

    question_target = run_dir / "questions/otter-apple-peeler"
    question_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(QUESTION_DIR, question_target)

    agent_readme = """# 作答说明

请在当前目录完成 `QUESTION.md` 中的题目，并把结果保存到 `showcase/index.html`。

不要读取或修改其他参评对象的目录。完成后停止，不进行评分。
"""

    records = []
    for item in items:
        agent_dir = run_dir / "agents" / item.slug
        (agent_dir / "showcase").mkdir(parents=True, exist_ok=True)
        shutil.copy2(question_target / "QUESTION.md", agent_dir / "QUESTION.md")
        (agent_dir / "README.md").write_text(agent_readme, encoding="utf-8")
        records.append(
            {
                "name": item.name,
                "harness": item.harness,
                "model": item.model,
                "selector": item.selector,
                "slug": item.slug,
                "result": f"agents/{item.slug}/showcase/index.html",
            }
        )

    (run_dir / "README.md").write_text(render_run_readme(run_id, items), encoding="utf-8")
    (run_dir / "participants.md").write_text(render_participants(items, run_id), encoding="utf-8")
    (run_dir / "launch.md").write_text(render_launch(run_dir, items), encoding="utf-8")
    (run_dir / "run.json").write_text(
        json.dumps({"run_id": run_id, "participants": records}, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--all", action="store_true", help="显式选择全部已登记组合；不传选择参数时同样默认为 ALL")
    group.add_argument("--agent", action="append", default=[], help="覆盖默认 ALL，选择一个已登记的 'harness + model'，可重复")
    parser.add_argument("--run-id", help="轮次 ID；默认使用当前时间")
    parser.add_argument("--run-dir", type=Path, help="显式输出目录，主要用于流程验证")
    args = parser.parse_args()

    run_id = args.run_id or datetime.now().strftime("%Y%m%d-%H%M-svg-animation")
    run_dir = args.run_dir.resolve() if args.run_dir else ROOT / "benchmark" / run_id

    try:
        registry = ROOT / "evaluation-agents.md"
        registered = registered_participants(registry)
        selected = resolve_participants(
            registered,
            registered_harnesses(registry),
            registered_models(registry),
            args.agent,
            args.all,
        )
        create_run(run_dir, run_id, selected)
    except (ValueError, FileExistsError) as exc:
        parser.error(str(exc))

    print(f"已生成：{run_dir}")
    for item in selected:
        print(f"- {item.name}: {run_dir / 'agents' / item.slug}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
