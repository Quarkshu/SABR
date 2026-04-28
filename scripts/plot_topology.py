from __future__ import annotations

import argparse
import math
from pathlib import Path

from sabr_results import load_contact_plan, resolve_contact_plan_path


PALETTE = [
    "#005f73",
    "#0a9396",
    "#94d2bd",
    "#ee9b00",
    "#ca6702",
    "#bb3e03",
]


def default_output_path(input_path: Path) -> Path:
    base_path = resolve_contact_plan_path(input_path)
    return base_path.with_name(base_path.stem + "_topology.svg")


def main() -> int:
    parser = argparse.ArgumentParser(description="Render a contact topology SVG from a scenario or contact plan.")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--title", default="")
    args = parser.parse_args()

    contacts, _ = load_contact_plan(args.input)
    nodes = sorted({int(contact["sending_node"]) for contact in contacts} | {int(contact["receiving_node"]) for contact in contacts})
    if not nodes:
        raise SystemExit("no contacts found for topology plot")

    width = 960
    height = 720
    center_x = width / 2
    center_y = height / 2
    radius = min(width, height) * 0.32
    node_positions: dict[int, tuple[float, float]] = {}
    for index, node_id in enumerate(nodes):
        angle = (2 * math.pi * index / len(nodes)) - math.pi / 2
        node_positions[node_id] = (
            center_x + radius * math.cos(angle),
            center_y + radius * math.sin(angle),
        )

    output_path = (args.output or default_output_path(args.input)).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    title = args.title or resolve_contact_plan_path(args.input).stem.replace("_", " ").title()

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        "<defs><marker id=\"arrow\" viewBox=\"0 0 10 10\" refX=\"8\" refY=\"5\" markerWidth=\"6\" markerHeight=\"6\" orient=\"auto-start-reverse\"><path d=\"M 0 0 L 10 5 L 0 10 z\" fill=\"#34495e\"/></marker></defs>",
        f'<rect width="{width}" height="{height}" fill="#fffdf8"/>',
        f'<text x="{width / 2}" y="42" text-anchor="middle" font-family="Segoe UI, sans-serif" font-size="26" fill="#13293d">{title}</text>',
    ]

    for index, contact in enumerate(contacts):
        source = int(contact["sending_node"])
        target = int(contact["receiving_node"])
        x1, y1 = node_positions[source]
        x2, y2 = node_positions[target]
        dx = x2 - x1
        dy = y2 - y1
        length = math.hypot(dx, dy) or 1.0
        offset_x = -dy / length * (14 if index % 2 == 0 else -14)
        offset_y = dx / length * (14 if index % 2 == 0 else -14)
        color = PALETTE[index % len(PALETTE)]
        label_x = (x1 + x2) / 2 + offset_x
        label_y = (y1 + y2) / 2 + offset_y
        lines.append(
            f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{color}" stroke-width="3" marker-end="url(#arrow)" opacity="0.9"/>'
        )
        label = f"c{contact['contact_id']}: {contact['start_time']}-{contact['end_time']} @ {contact['data_rate']}"
        lines.append(
            f'<text x="{label_x:.1f}" y="{label_y:.1f}" text-anchor="middle" font-family="Consolas, monospace" font-size="12" fill="#3d405b">{label}</text>'
        )

    for node_id, (x, y) in node_positions.items():
        lines.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="30" fill="#ffffff" stroke="#13293d" stroke-width="3"/>')
        lines.append(f'<text x="{x:.1f}" y="{y + 7:.1f}" text-anchor="middle" font-family="Segoe UI, sans-serif" font-size="22" fill="#13293d">{node_id}</text>')

    lines.append("</svg>")
    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())