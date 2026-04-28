from __future__ import annotations

import argparse
import platform
import subprocess
import sys
import time
from pathlib import Path

from sabr_results import load_json, write_json


NODE_COUNT = 20
CONTACT_COUNT = 200
TOTAL_BUNDLES = 1000
DEFAULT_MAX_RUNTIME_SECONDS = 60.0


def default_workspace_root() -> Path:
    return Path(__file__).resolve().parent.parent


def default_executable(workspace_root: Path) -> Path:
    candidate = workspace_root / "build" / "sabr.exe"
    if candidate.exists():
        return candidate
    return workspace_root / "build" / "sabr"


def build_contact_plan() -> dict[str, object]:
    contacts: list[dict[str, object]] = []
    ranges: list[dict[str, object]] = []
    range_pairs: set[tuple[int, int]] = set()
    shifts = [1, 2, 3, 4, 5, 6, 7, 8, 9, 19]

    contact_id = 1
    for window_index, shift in enumerate(shifts):
        start_time = float(window_index * 18)
        end_time = start_time + 16.0
        for source_node in range(1, NODE_COUNT + 1):
            destination_node = ((source_node - 1 + shift) % NODE_COUNT) + 1
            contacts.append({
                "contact_id": contact_id,
                "start_time": start_time,
                "end_time": end_time,
                "from_node": source_node,
                "to_node": destination_node,
                "data_rate": 120000.0,
            })
            contact_id += 1

            pair = tuple(sorted((source_node, destination_node)))
            if pair not in range_pairs:
                range_pairs.add(pair)
                ranges.append({
                    "start_time": 0.0,
                    "end_time": 220.0,
                    "node_a": pair[0],
                    "node_b": pair[1],
                    "distance_light_seconds": round(0.4 + (shift * 0.03), 3),
                })

    return {
        "contacts": contacts,
        "ranges": ranges,
    }


def build_traffic_patterns() -> list[dict[str, object]]:
    flows = [
        (1, 20, 200, 1.0),
        (2, 17, 200, 2.0),
        (5, 11, 200, 3.0),
        (9, 3, 200, 4.0),
        (14, 7, 200, 5.0),
    ]
    traffic_patterns: list[dict[str, object]] = []
    for source_node, destination_node, bundle_count, start_time in flows:
        traffic_patterns.append({
            "mode": "PERIODIC",
            "source_node": source_node,
            "destination_node": destination_node,
            "start_time": start_time,
            "period": 0.1,
            "bundle_count": bundle_count,
            "payload_size": 512.0,
            "header_size": 64.0,
            "priority": "NORMAL",
            "ttl": 220.0,
        })
    return traffic_patterns


def build_scenario_document() -> dict[str, object]:
    return {
        "scenario_name": "nf_pf_01_large_scale",
        "output_directory": "./run",
        "contact_plan": build_contact_plan(),
        "enhancements": {
            "one_route_per_neighbor": True,
            "queue_delay": True,
            "anti_loop_reactive": True,
            "anti_loop_proactive": True,
        },
        "simulation": {
            "start_time": 0.0,
            "end_time": 220.0,
            "owlt_margin": 0.6,
            "recompute_budget": 8,
            "failure_seed": 909,
            "phase1": {
                "k_paths": 8,
            },
        },
        "traffic": build_traffic_patterns(),
    }


def summarize_scenario(document: dict[str, object]) -> dict[str, object]:
    contact_plan = document["contact_plan"]
    contacts = contact_plan["contacts"]
    nodes = set()
    for contact in contacts:
        nodes.add(int(contact["from_node"]))
        nodes.add(int(contact["to_node"]))

    bundle_count = sum(int(pattern["bundle_count"]) for pattern in document["traffic"])
    return {
        "node_count": len(nodes),
        "contact_count": len(contacts),
        "range_count": len(contact_plan.get("ranges", [])),
        "bundle_count": bundle_count,
    }


def run_command(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False)


def write_report(report: dict[str, object], output_root: Path) -> Path:
    report_path = output_root / "performance_report.json"
    write_json(report, report_path)
    return report_path


def build_report_base(args: argparse.Namespace,
                      scenario_path: Path,
                      scenario_summary: dict[str, object]) -> dict[str, object]:
    return {
        "benchmark_name": "NF-PF-01",
        "max_runtime_seconds": args.max_runtime_seconds,
        "workspace_root": str(args.workspace_root),
        "sabr_executable": str(args.sabr),
        "scenario_path": str(scenario_path),
        "output_root": str(args.output_root),
        "environment": {
            "platform": platform.platform(),
            "python_version": sys.version.split()[0],
        },
        "scenario": scenario_summary,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate and run the Stage 9 NF-PF-01 large-scale benchmark."
    )
    parser.add_argument("--workspace-root", type=Path, default=default_workspace_root())
    parser.add_argument("--sabr", type=Path, help="Path to sabr executable.")
    parser.add_argument(
        "--output-root",
        type=Path,
        help="Output directory for generated scenario, run artifacts, and the performance report.",
    )
    parser.add_argument(
        "--max-runtime-seconds",
        type=float,
        default=DEFAULT_MAX_RUNTIME_SECONDS,
        help="NF-PF-01 threshold in seconds.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Only write the generated scenario and performance report skeleton without running sabr.",
    )
    args = parser.parse_args()

    args.workspace_root = args.workspace_root.resolve()
    args.sabr = (args.sabr or default_executable(args.workspace_root)).resolve()
    args.output_root = (args.output_root or (args.workspace_root / "results" / "stage9" / "performance")).resolve()
    args.output_root.mkdir(parents=True, exist_ok=True)

    if not args.dry_run and not args.sabr.exists():
        print(f"sabr executable not found: {args.sabr}", file=sys.stderr)
        return 1

    scenario_path = args.output_root / "generated_benchmark_scenario.json"
    scenario_document = build_scenario_document()
    scenario_summary = summarize_scenario(scenario_document)
    write_json(scenario_document, scenario_path)

    report = build_report_base(args, scenario_path, scenario_summary)
    report["threshold_satisfied_by_definition"] = (
        scenario_summary["node_count"] == NODE_COUNT
        and scenario_summary["contact_count"] == CONTACT_COUNT
        and scenario_summary["bundle_count"] == TOTAL_BUNDLES
    )

    if args.dry_run:
        report["dry_run"] = True
        report["result"] = "prepared"
        report["passed"] = None
        report_path = write_report(report, args.output_root)
        print(f"prepared benchmark scenario: {scenario_path}")
        print(f"report: {report_path}")
        return 0

    run_output = args.output_root / "run"
    command = [
        str(args.sabr),
        "run-scenario",
        "--config",
        str(scenario_path),
        "--output",
        str(run_output),
    ]

    started = time.perf_counter()
    result = run_command(command)
    elapsed = time.perf_counter() - started

    report["dry_run"] = False
    report["command"] = command
    report["wall_clock_seconds"] = elapsed
    report["stdout"] = result.stdout.strip()
    report["stderr"] = result.stderr.strip()
    report["return_code"] = result.returncode

    stats_path = run_output / "stats.json"
    manifest_path = run_output / "run_manifest.json"
    report["generated_files"] = {
        "stats": str(stats_path),
        "manifest": str(manifest_path),
    }

    if stats_path.exists():
        stats = load_json(stats_path)
        report["summary"] = stats.get("summary", {})
        report["metadata"] = stats.get("metadata", {})

    if result.returncode != 0:
        report["result"] = "execution_failed"
        report["passed"] = False
        report_path = write_report(report, args.output_root)
        print(f"benchmark execution failed, report: {report_path}", file=sys.stderr)
        return 1

    report["passed"] = elapsed < args.max_runtime_seconds
    report["result"] = "passed" if report["passed"] else "threshold_exceeded"
    report_path = write_report(report, args.output_root)

    print(f"benchmark runtime: {elapsed:.3f}s")
    print(f"report: {report_path}")
    if report["passed"]:
        return 0
    return 2


if __name__ == "__main__":
    raise SystemExit(main())