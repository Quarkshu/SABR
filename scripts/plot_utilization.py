from __future__ import annotations

import argparse
from pathlib import Path

from sabr_results import load_contact_utilization_rows, resolve_contact_utilization_csv_path


def default_output_path(input_path: Path) -> Path:
    csv_path = resolve_contact_utilization_csv_path(input_path)
    return csv_path.with_name(csv_path.stem + "_plot.svg")


def main() -> int:
    parser = argparse.ArgumentParser(description="Render a contact utilization SVG from contact_utilization.csv or a run directory.")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--title", default="")
    args = parser.parse_args()

    rows = load_contact_utilization_rows(args.input)
    if not rows:
        raise SystemExit("no contact utilization rows found")

    width = max(960, 180 + len(rows) * 100)
    height = 620
    left = 90
    bottom = 90
    plot_height = height - 160
    bar_width = 56
    max_value = max(float(row.get("utilization_ratio", 0.0)) for row in rows)
    max_value = max(max_value, 0.1)

    output_path = (args.output or default_output_path(args.input)).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    title = args.title or "Contact Utilization"

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        f'<rect width="{width}" height="{height}" fill="#fffcf6"/>',
        f'<text x="{width / 2}" y="44" text-anchor="middle" font-family="Segoe UI, sans-serif" font-size="28" fill="#13293d">{title}</text>',
        f'<line x1="{left}" y1="80" x2="{left}" y2="{height - bottom}" stroke="#495057" stroke-width="2"/>',
        f'<line x1="{left}" y1="{height - bottom}" x2="{width - 40}" y2="{height - bottom}" stroke="#495057" stroke-width="2"/>',
    ]

    for tick in range(6):
        value = max_value * tick / 5
        y = height - bottom - plot_height * tick / 5
        lines.append(f'<line x1="{left}" y1="{y:.1f}" x2="{width - 40}" y2="{y:.1f}" stroke="#e9ecef" stroke-width="1"/>')
        lines.append(f'<text x="{left - 12}" y="{y + 4:.1f}" text-anchor="end" font-family="Consolas, monospace" font-size="12" fill="#495057">{value:.2f}</text>')

    for index, row in enumerate(rows):
        ratio = float(row.get("utilization_ratio", 0.0))
        x = left + 32 + index * 100
        bar_height = 0.0 if max_value == 0.0 else ratio / max_value * plot_height
        y = height - bottom - bar_height
        lines.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_width}" height="{bar_height:.1f}" fill="#ee9b00" rx="6"/>')
        lines.append(f'<text x="{x + bar_width / 2:.1f}" y="{height - bottom + 22}" text-anchor="middle" font-family="Segoe UI, sans-serif" font-size="12" fill="#13293d">c{row.get("contact_id")}</text>')
        lines.append(f'<text x="{x + bar_width / 2:.1f}" y="{y - 10:.1f}" text-anchor="middle" font-family="Consolas, monospace" font-size="12" fill="#495057">{ratio:.2f}</text>')

    lines.append("</svg>")
    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())