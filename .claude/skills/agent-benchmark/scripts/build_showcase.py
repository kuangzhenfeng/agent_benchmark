#!/usr/bin/env python3
"""把一个轮次内的多 Agent HTML 结果合并为单个展示网页。"""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


def preview_frame(name: str, source: str | None, state: str) -> str:
    if source is None:
        body = '<div class="missing">尚未找到 <code>showcase/index.html</code></div>'
    else:
        escaped = html.escape(source, quote=True)
        body = (
            f'<iframe title="{html.escape(name, quote=True)} 的动画结果" '
            f'sandbox="allow-scripts" srcdoc="{escaped}"></iframe>'
        )
    return f"""
      <article class="result">
        <header><h2>{html.escape(name)}</h2><span>{state}</span></header>
        {body}
      </article>"""


def build(run_dir: Path) -> tuple[str, int, int]:
    metadata = json.loads((run_dir / "run.json").read_text(encoding="utf-8"))
    frames: list[str] = []
    completed = 0
    participants = metadata.get("participants", [])
    for item in participants:
        result = run_dir / item["result"]
        if result.is_file():
            source = result.read_text(encoding="utf-8")
            completed += 1
            state = "已完成"
        else:
            source = None
            state = "等待结果"
        frames.append(preview_frame(item["name"], source, state))

    total = len(participants)
    document = f"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>多 Agent SVG 动画结果</title>
  <style>
    :root {{ color-scheme: dark; --bg: #0d0d10; --surface: #18181e; --line: #34343d; --ink: #f5f5f7; --muted: #aaaab6; --accent: #a78bfa; }}
    * {{ box-sizing: border-box; }}
    body {{ margin: 0; background: var(--bg); color: var(--ink); font-family: ui-sans-serif, system-ui, sans-serif; }}
    .top {{ padding: clamp(28px, 5vw, 72px); border-bottom: 1px solid var(--line); }}
    h1 {{ max-width: 15ch; margin: 0 0 14px; font-size: clamp(36px, 6vw, 76px); line-height: .98; letter-spacing: -.04em; }}
    .summary {{ margin: 0; color: var(--muted); font-size: 16px; }}
    .results {{ display: grid; gap: 28px; padding: clamp(20px, 4vw, 56px); }}
    .result {{ min-width: 0; }}
    .result header {{ display: flex; align-items: center; justify-content: space-between; gap: 16px; margin-bottom: 10px; }}
    h2 {{ margin: 0; font-size: 18px; }}
    .result header span {{ color: var(--accent); font-size: 13px; }}
    iframe, .missing {{ display: block; width: 100%; min-height: min(76vh, 760px); border: 1px solid var(--line); border-radius: 12px; background: #fff; }}
    .missing {{ display: grid; place-items: center; color: var(--muted); background: var(--surface); }}
    code {{ color: var(--ink); }}
    footer {{ padding: 20px clamp(20px, 4vw, 56px); color: var(--muted); border-top: 1px solid var(--line); font-size: 12px; }}
    @media (min-width: 1500px) {{ .results {{ grid-template-columns: repeat(2, minmax(0, 1fr)); }} iframe, .missing {{ min-height: 600px; }} }}
  </style>
</head>
<body>
  <header class="top">
    <h1>同一道题，{total} 种回答。</h1>
    <p class="summary">{html.escape(metadata['run_id'])} · 已完成 {completed}/{total} · 仅展示，不评分</p>
  </header>
  <main class="results">{''.join(frames)}
  </main>
  <footer>各结果在禁用同源权限的沙箱中运行；页面由 build_showcase.py 生成。</footer>
</body>
</html>
"""
    return document, completed, total


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", required=True, help="benchmark 下的轮次 ID，或轮次目录路径")
    parser.add_argument("--output", type=Path, default=ROOT / "showcase/index.html", help="汇总展示页路径")
    args = parser.parse_args()

    candidate = Path(args.run)
    run_dir = candidate.resolve() if candidate.is_dir() else ROOT / "benchmark" / args.run
    if not (run_dir / "run.json").is_file():
        parser.error(f"找不到轮次元数据：{run_dir / 'run.json'}")

    document, completed, total = build(run_dir)
    outputs = [args.output.resolve(), run_dir / "showcase.html"]
    for output in outputs:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(document, encoding="utf-8")
    print(f"已汇总 {completed}/{total} 个结果：{outputs[0]}")
    print(f"轮次副本：{outputs[1]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
