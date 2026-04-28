from __future__ import annotations

import argparse
from pathlib import Path

from sabr_results import collect_flat_rows, parse_numeric, write_csv


PALETTE = ["#005f73", "#ee9b00", "#ae2012", "#0a9396", "#6d597a", "#ca6702"]


def default_output_path(input_path: Path) -> Path:
    resolved = input_path.resolve()
    if resolved.is_dir():
        return resolved / "comparison.svg"
    return resolved.with_name(resolved.stem + "_comparison.svg")


def main() -> int:
    parser = argparse.ArgumentParser(description="Render a comparison SVG from aggregated results or manifest directories.")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--metric", default="summary::delivery_rate")
    parser.add_argument("--x-key", default="scenario_name")
    parser.add_argument("--series-key", default="")
    parser.add_argument("--title", default="")
    args = parser.parse_args()

    rows = collect_flat_rows(args.input)
    if not rows:
        raise SystemExit("no aggregated rows found for comparison plot")

    x_values = sorted({str(row.get(args.x_key, "")) for row in rows})
    series_values = [""]
    if args.series_key:
        series_values = sorted({str(row.get(args.series_key, "")) for row in rows})

    width = max(1080, 220 + len(x_values) * max(120, len(series_values) * 70))
    height = 680
    left = 120
    bottom = 110
    plot_height = height - 180

    series_metrics: dict[tuple[str, str], float] = {}
    max_metric = 0.0
    for x_value in x_values:
        for series_value in series_values:
            matched = [row for row in rows if str(row.get(args.x_key, "")) == x_value and (not args.series_key or str(row.get(args.series_key, "")) == series_value)]
            numeric_values = [parsed for row in matched if (parsed := parse_numeric(row.get(args.metric))) is not None]
            metric_value = 0.0 if not numeric_values else sum(numeric_values) / len(numeric_values)
            series_metrics[(x_value, series_value)] = metric_value
            max_metric = max(max_metric, metric_value)
    max_metric = max(max_metric, 1.0)

    output_path = (args.output or default_output_path(args.input)).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    title = args.title or f"Comparison: {args.metric}"

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        f'<rect width="{width}" height="{height}" fill="#fffef8"/>',
        f'<text x="{width / 2}" y="44" text-anchor="middle" font-family="Segoe UI, sans-serif" font-size="28" fill="#13293d">{title}</text>',
        f'<line x1="{left}" y1="80" x2="{left}" y2="{height - bottom}" stroke="#495057" stroke-width="2"/>',
        f'<line x1="{left}" y1="{height - bottom}" x2="{width - 40}" y2="{height - bottom}" stroke="#495057" stroke-width="2"/>',
    ]

    for tick in range(6):
        tick_value = max_metric * tick / 5
        y = height - bottom - plot_height * tick / 5
        lines.append(f'<line x1="{left}" y1="{y:.1f}" x2="{width - 40}" y2="{y:.1f}" stroke="#e9ecef" stroke-width="1"/>')
        lines.append(f'<text x="{left - 12}" y="{y + 4:.1f}" text-anchor="end" font-family="Consolas, monospace" font-size="12" fill="#495057">{tick_value:.2f}</text>')

    group_width = (width - left - 80) / max(len(x_values), 1)
    bar_group_width = group_width * 0.72
    bar_width = bar_group_width / max(len(series_values), 1)

    for group_index, x_value in enumerate(x_values):
        group_x = left + group_width * group_index + (group_width - bar_group_width) / 2
        for series_index, series_value in enumerate(series_values):
            metric_value = series_metrics[(x_value, series_value)]
            bar_height = metric_value / max_metric * plot_height
            x = group_x + series_index * bar_width
            y = height - bottom - bar_height
            color = PALETTE[series_index % len(PALETTE)]
            lines.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_width - 10:.1f}" height="{bar_height:.1f}" fill="{color}" rx="6"/>')
            lines.append(f'<text x="{x + (bar_width - 10) / 2:.1f}" y="{y - 10:.1f}" text-anchor="middle" font-family="Consolas, monospace" font-size="12" fill="#495057">{metric_value:.2f}</text>')

        lines.append(f'<text x="{group_x + bar_group_width / 2:.1f}" y="{height - bottom + 26}" text-anchor="middle" font-family="Segoe UI, sans-serif" font-size="12" fill="#13293d">{x_value}</text>')

    if args.series_key:
        legend_x = width - 280
        legend_y = 74
        for series_index, series_value in enumerate(series_values):
            color = PALETTE[series_index % len(PALETTE)]
            y = legend_y + series_index * 22
            lines.append(f'<rect x="{legend_x}" y="{y - 11}" width="14" height="14" fill="{color}" rx="3"/>')
            lines.append(f'<text x="{legend_x + 22}" y="{y}" font-family="Segoe UI, sans-serif" font-size="12" fill="#13293d">{series_value}</text>')

    lines.append("</svg>")
    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())