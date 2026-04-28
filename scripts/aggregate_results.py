from __future__ import annotations

import argparse
from pathlib import Path

from sabr_results import build_pivot_rows, collect_flat_rows, write_csv, write_json

def main() -> int:
    parser = argparse.ArgumentParser(description="Aggregate SABR run manifests and stats into flat JSON and CSV tables.")
    parser.add_argument("--input", type=Path, required=True, help="Root directory containing run_manifest.json files.")
    parser.add_argument("--output", type=Path, help="Directory to write aggregated output files.")
    parser.add_argument("--format", choices=["flat", "pivot", "both"], default="flat")
    parser.add_argument("--metric", default="summary::delivery_rate")
    parser.add_argument("--row-key", default="scenario_name")
    parser.add_argument("--column-key", default="")
    args = parser.parse_args()

    input_root = args.input.resolve()
    output_root = (args.output or input_root).resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    rows = collect_flat_rows(input_root)

    outputs: list[Path] = []
    if args.format in {"flat", "both"}:
        json_path = output_root / "aggregated_summary.json"
        csv_path = output_root / "aggregated_summary.csv"
        write_json(rows, json_path)
        write_csv(rows, csv_path)
        outputs.extend([json_path, csv_path])

    if args.format in {"pivot", "both"}:
        pivot_rows = build_pivot_rows(rows, args.row_key, args.column_key, args.metric)
        pivot_path = output_root / "aggregated_pivot.csv"
        write_csv(pivot_rows, pivot_path)
        outputs.append(pivot_path)

    print(f"aggregated {len(rows)} runs")
    for output in outputs:
        print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())