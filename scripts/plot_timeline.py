from __future__ import annotations

import argparse
from pathlib import Path

from sabr_results import load_stats, resolve_stats_path


COLORS = {
    "DELIVERED": "#2a9d8f",
    "ROUTE_FAILED": "#c1121f",
    "EXPIRED": "#f77f00",
    "DUPLICATE_DROPPED": "#6c757d",
}


def default_output_path(input_path: Path) -> Path:
    stats_path = resolve_stats_path(input_path)
    return stats_path.with_name(stats_path.stem + "_timeline.svg")


def main() -> int:
    parser = argparse.ArgumentParser(description="Render a bundle lifecycle timeline SVG from stats.json or a run directory.")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--title", default="")
    args = parser.parse_args()

    stats = load_stats(args.input)
    bundles = sorted(stats.get("bundles", []), key=lambda item: int(item.get("bundle_id", 0)))
    if not bundles:
        raise SystemExit("no bundle records found for timeline plot")

    min_time = min(float(bundle.get("creation_time", 0.0)) for bundle in bundles)
    max_time = max(float(bundle.get("delivered_time", bundle.get("expiration_time", 0.0)))
                   if float(bundle.get("delivered_time", -1.0)) >= 0.0
                   else float(bundle.get("expiration_time", 0.0))
                   for bundle in bundles)
    width = 1280
    height = 140 + len(bundles) * 54
    left = 180
    right = 70
    top = 90
    track_width = width - left - right
    time_span = max(max_time - min_time, 1.0)

    def to_x(time_value: float) -> float:
        return left + (time_value - min_time) / time_span * track_width

    output_path = (args.output or default_output_path(args.input)).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    title = args.title or stats.get("metadata", {}).get("scenario_name", "Bundle Timeline")

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        f'<rect width="{width}" height="{height}" fill="#fcfbf7"/>',
        f'<text x="{width / 2}" y="44" text-anchor="middle" font-family="Segoe UI, sans-serif" font-size="28" fill="#13293d">{title}</text>',
        f'<line x1="{left}" y1="{top - 24}" x2="{width - right}" y2="{top - 24}" stroke="#adb5bd" stroke-width="2"/>',
    ]

    for tick in range(6):
        tick_time = min_time + time_span * tick / 5
        x = to_x(tick_time)
        lines.append(f'<line x1="{x:.1f}" y1="{top - 34}" x2="{x:.1f}" y2="{height - 30}" stroke="#e5e5e5" stroke-width="1"/>')
        lines.append(f'<text x="{x:.1f}" y="{top - 42}" text-anchor="middle" font-family="Consolas, monospace" font-size="12" fill="#495057">{tick_time:.1f}</text>')

    for index, bundle in enumerate(bundles):
        y = top + index * 54
        creation = float(bundle.get("creation_time", 0.0))
        delivered = float(bundle.get("delivered_time", -1.0))
        expiration = float(bundle.get("expiration_time", creation))
        end_time = delivered if delivered >= 0.0 else expiration
        state = str(bundle.get("final_state", "ACTIVE"))
        color = COLORS.get(state, "#457b9d")
        route_path = "->".join(str(node) for node in bundle.get("route_path", []))

        lines.append(f'<text x="24" y="{y + 4:.1f}" font-family="Segoe UI, sans-serif" font-size="14" fill="#13293d">bundle {bundle.get("bundle_id")}</text>')
        lines.append(f'<text x="24" y="{y + 22:.1f}" font-family="Consolas, monospace" font-size="11" fill="#495057">{route_path}</text>')
        lines.append(f'<line x1="{to_x(creation):.1f}" y1="{y:.1f}" x2="{to_x(end_time):.1f}" y2="{y:.1f}" stroke="{color}" stroke-width="8" stroke-linecap="round"/>')
        lines.append(f'<circle cx="{to_x(creation):.1f}" cy="{y:.1f}" r="5" fill="#1d3557"/>')
        lines.append(f'<circle cx="{to_x(end_time):.1f}" cy="{y:.1f}" r="6" fill="{color}"/>')
        lines.append(f'<text x="{width - right + 8}" y="{y + 4:.1f}" font-family="Segoe UI, sans-serif" font-size="13" fill="#212529">{state}</text>')

    lines.append("</svg>")
    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())