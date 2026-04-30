from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

from sabr_results import write_json


def default_workspace_root() -> Path:
    return Path(__file__).resolve().parent.parent


def default_executable(workspace_root: Path) -> Path:
    candidate = workspace_root / "build" / "sabr.exe"
    if candidate.exists():
        return candidate
    return workspace_root / "build" / "sabr"


def build_experiment_catalog(workspace_root: Path, matrix_limit: int) -> dict[str, dict[str, object]]:
    experiments_root = workspace_root / "configs" / "experiments"
    return {
        "exp1_unicast_correctness": {
            "runs": [
                {
                    "name": "baseline",
                    "mode": "scenario",
                    "path": experiments_root / "exp1_unicast_correctness" / "baseline.json",
                }
            ],
            "export": {
                "metric": "summary::delivery_rate",
                "row_key": "scenario_name",
                "column_key": "",
            },
            "plots": [
                {
                    "kind": "timeline",
                    "input": "run:baseline",
                    "output_name": "exp1_timeline.svg",
                    "title": "Exp1 Baseline Timeline",
                }
            ],
        },
        "exp2_unicast_scale": {
            "runs": [
                {
                    "name": "matrix",
                    "mode": "experiment",
                    "path": experiments_root / "exp2_unicast_scale" / "matrix.json",
                    "limit": matrix_limit,
                }
            ],
            "export": {
                "metric": "summary::delivery_rate",
                "row_key": "coord::simulation.phase1.k_paths",
                "column_key": "coord::simulation.owlt_margin",
            },
            "plots": [
                {
                    "kind": "comparison",
                    "input": "export_summary",
                    "output_name": "exp2_comparison.svg",
                    "metric": "summary::delivery_rate",
                    "x_key": "coord::simulation.phase1.k_paths",
                    "series_key": "coord::simulation.owlt_margin",
                    "title": "Exp2 Delivery Comparison",
                }
            ],
        },
        "exp3_multicast_plan": {
            "runs": [
                {
                    "name": "tree_plan",
                    "mode": "scenario",
                    "path": experiments_root / "exp3_multicast_plan" / "tree_plan.json",
                },
                {
                    "name": "split_unicast_control",
                    "mode": "scenario",
                    "path": experiments_root / "exp3_multicast_plan" / "split_unicast_control.json",
                },
            ],
            "export": {
                "metric": "summary::delivery_rate",
                "row_key": "scenario_name",
                "column_key": "",
            },
            "plots": [
                {
                    "kind": "topology",
                    "input": "config:tree_plan",
                    "output_name": "exp3_topology.svg",
                    "title": "Exp3 Multicast Topology",
                },
                {
                    "kind": "comparison",
                    "input": "export_summary",
                    "output_name": "exp3_comparison.svg",
                    "metric": "summary::delivery_rate",
                    "x_key": "scenario_name",
                    "series_key": "",
                    "title": "Exp3 Plan Comparison",
                }
            ],
        },
        "exp4_multicast_repair": {
            "runs": [
                {
                    "name": "baseline",
                    "mode": "scenario",
                    "path": experiments_root / "exp4_multicast_repair" / "baseline.json",
                }
            ],
            "export": {
                "metric": "summary::delivery_rate",
                "row_key": "scenario_name",
                "column_key": "",
            },
            "plots": [
                {
                    "kind": "timeline",
                    "input": "run:baseline",
                    "output_name": "exp4_timeline.svg",
                    "title": "Exp4 Repair Timeline",
                }
            ],
        },
        "exp5_redundancy": {
            "runs": [
                {
                    "name": "primary_only",
                    "mode": "scenario",
                    "path": experiments_root / "exp5_redundancy" / "primary_only.json",
                },
                {
                    "name": "single_backup",
                    "mode": "scenario",
                    "path": experiments_root / "exp5_redundancy" / "single_backup.json",
                },
            ],
            "export": {
                "metric": "summary::delivery_rate",
                "row_key": "scenario_name",
                "column_key": "",
            },
            "plots": [
                {
                    "kind": "comparison",
                    "input": "export_summary",
                    "output_name": "exp5_comparison.svg",
                    "metric": "summary::delivery_rate",
                    "x_key": "scenario_name",
                    "series_key": "",
                    "title": "Exp5 Redundancy Comparison",
                }
            ],
        },
        "exp6_ablation": {
            "runs": [
                {
                    "name": "matrix",
                    "mode": "experiment",
                    "path": experiments_root / "exp6_ablation" / "matrix.json",
                    "limit": matrix_limit,
                }
            ],
            "export": {
                "metric": "summary::delivery_rate",
                "row_key": "coord::simulation.phase1.k_paths",
                "column_key": "coord::redundancy.mode",
            },
            "plots": [
                {
                    "kind": "comparison",
                    "input": "export_summary",
                    "output_name": "exp6_comparison.svg",
                    "metric": "summary::delivery_rate",
                    "x_key": "coord::simulation.phase1.k_paths",
                    "series_key": "coord::redundancy.mode",
                    "title": "Exp6 Ablation Comparison",
                }
            ],
        },
    }


def build_release_sources(workspace_root: Path, output_root: Path) -> list[dict[str, str]]:
    scripts = [
        "aggregate_results.py",
        "benchmark_nf_pf_01.py",
        "complete_experiment_workflow.py",
        "plot_comparison.py",
        "plot_timeline.py",
        "plot_topology.py",
        "plot_utilization.py",
        "run_exp1_complete.py",
        "run_exp2_complete.py",
        "run_exp3_complete.py",
        "run_exp4_complete.py",
        "run_exp5_complete.py",
        "run_exp6_complete.py",
        "run_experiments.py",
        "sabr_results.py",
        "stage9_suite.py",
    ]

    sources = [
        {"source": str(workspace_root / "README.md"), "destination": "README.md"},
        {"source": str(workspace_root / "docs"), "destination": "docs"},
        {"source": str(workspace_root / "configs" / "experiments"), "destination": "configs/experiments"},
        {"source": str(output_root / "performance"), "destination": "samples/performance"},
        {"source": str(output_root / "representative"), "destination": "samples/representative"},
        {"source": str(output_root / "exports"), "destination": "samples/exports"},
        {"source": str(output_root / "plots"), "destination": "samples/plots"},
        {"source": str(output_root / "stage9_execution_plan.json"), "destination": "samples/stage9_execution_plan.json"},
        {"source": str(output_root / "stage9_execution_report.json"), "destination": "samples/stage9_execution_report.json"},
    ]
    for script_name in scripts:
        sources.append({
            "source": str(workspace_root / "scripts" / script_name),
            "destination": f"scripts/{script_name}",
        })
    return sources


def build_execution_plan(workspace_root: Path,
                         sabr_executable: Path,
                         output_root: Path,
                         matrix_limit: int,
                         selected_experiments: set[str],
                         include_benchmark: bool,
                         include_bundle: bool) -> dict[str, object]:
    catalog = build_experiment_catalog(workspace_root, matrix_limit)
    chosen_names = [name for name in catalog.keys() if not selected_experiments or name in selected_experiments]

    experiments: list[dict[str, object]] = []
    for experiment_name in chosen_names:
        definition = catalog[experiment_name]
        experiment_root = output_root / "representative" / experiment_name
        runs_root = experiment_root / "runs"
        export_root = output_root / "exports" / experiment_name
        plot_root = output_root / "plots"

        run_entries: list[dict[str, object]] = []
        run_lookup: dict[str, Path] = {}
        config_lookup: dict[str, Path] = {}
        for run in definition["runs"]:
            run_name = str(run["name"])
            run_path = Path(run["path"])
            if run["mode"] == "scenario":
                output_directory = runs_root / run_name
                command = [
                    str(sabr_executable),
                    "run-scenario",
                    "--config",
                    str(run_path),
                    "--output",
                    str(output_directory),
                ]
            else:
                output_directory = runs_root
                command = [
                    str(sabr_executable),
                    "run-experiment",
                    "--matrix",
                    str(run_path),
                    "--output",
                    str(output_directory),
                    "--limit",
                    str(run.get("limit", matrix_limit)),
                ]
            run_lookup[run_name] = output_directory
            config_lookup[run_name] = run_path
            run_entries.append({
                "name": run_name,
                "mode": run["mode"],
                "input_path": str(run_path),
                "output_directory": str(output_directory),
                "command": command,
            })

        export_command = [
            str(sabr_executable),
            "export",
            "--input",
            str(runs_root),
            "--output",
            str(export_root),
            "--format",
            "both",
            "--metric",
            str(definition["export"]["metric"]),
            "--row-key",
            str(definition["export"]["row_key"]),
        ]
        if definition["export"].get("column_key"):
            export_command.extend(["--column-key", str(definition["export"]["column_key"])])

        plot_entries: list[dict[str, object]] = []
        for plot in definition["plots"]:
            if plot["input"] == "export_summary":
                plot_input = export_root / "aggregated_summary.csv"
            elif str(plot["input"]).startswith("run:"):
                plot_input = run_lookup[str(plot["input"]).split(":", 1)[1]]
            elif str(plot["input"]).startswith("config:"):
                plot_input = config_lookup[str(plot["input"]).split(":", 1)[1]]
            else:
                raise ValueError(f"unsupported plot input descriptor: {plot['input']}")

            command = [
                str(sabr_executable),
                "plot",
                "--kind",
                str(plot["kind"]),
                "--input",
                str(plot_input),
                "--output",
                str(plot_root / str(plot["output_name"])),
            ]
            if plot.get("metric"):
                command.extend(["--metric", str(plot["metric"])])
            if plot.get("x_key"):
                command.extend(["--x-key", str(plot["x_key"])])
            if plot.get("series_key"):
                command.extend(["--series-key", str(plot["series_key"])])
            if plot.get("title"):
                command.extend(["--title", str(plot["title"])])

            plot_entries.append({
                "kind": plot["kind"],
                "input_path": str(plot_input),
                "output_path": str(plot_root / str(plot["output_name"])),
                "command": command,
            })

        experiments.append({
            "experiment_name": experiment_name,
            "runs": run_entries,
            "export": {
                "input_path": str(runs_root),
                "output_directory": str(export_root),
                "command": export_command,
            },
            "plots": plot_entries,
        })

    benchmark = None
    if include_benchmark:
        benchmark = {
            "command": [
                sys.executable,
                str(workspace_root / "scripts" / "benchmark_nf_pf_01.py"),
                "--workspace-root",
                str(workspace_root),
                "--sabr",
                str(sabr_executable),
                "--output-root",
                str(output_root / "performance"),
            ],
            "output_directory": str(output_root / "performance"),
        }

    global_export = {
        "command": [
            str(sabr_executable),
            "export",
            "--input",
            str(output_root / "representative"),
            "--output",
            str(output_root / "exports" / "all"),
            "--format",
            "both",
            "--metric",
            "summary::delivery_rate",
            "--row-key",
            "scenario_name",
        ],
        "output_directory": str(output_root / "exports" / "all"),
    }
    global_plot = {
        "command": [
            str(sabr_executable),
            "plot",
            "--kind",
            "comparison",
            "--input",
            str(output_root / "exports" / "all" / "aggregated_summary.csv"),
            "--output",
            str(output_root / "plots" / "stage9_overview.svg"),
            "--metric",
            "summary::delivery_rate",
            "--x-key",
            "scenario_name",
            "--title",
            "Stage9 Representative Delivery",
        ],
        "output_path": str(output_root / "plots" / "stage9_overview.svg"),
    }

    bundle = None
    if include_bundle:
        bundle = {
            "bundle_root": str(output_root / "release_bundle"),
            "sources": build_release_sources(workspace_root, output_root),
        }

    return {
        "workspace_root": str(workspace_root),
        "sabr_executable": str(sabr_executable),
        "output_root": str(output_root),
        "benchmark": benchmark,
        "experiments": experiments,
        "global_export": global_export,
        "global_plot": global_plot,
        "bundle": bundle,
    }


def run_command(command: list[str]) -> dict[str, object]:
    started = time.perf_counter()
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    elapsed = time.perf_counter() - started
    return {
        "command": command,
        "return_code": result.returncode,
        "stdout": result.stdout.strip(),
        "stderr": result.stderr.strip(),
        "wall_clock_seconds": elapsed,
        "success": result.returncode == 0,
    }


def copy_release_sources(bundle_root: Path, sources: list[dict[str, str]]) -> dict[str, object]:
    copied: list[str] = []
    skipped: list[str] = []
    for item in sources:
        source = Path(item["source"])
        destination = bundle_root / item["destination"]
        if not source.exists():
            skipped.append(str(source))
            continue
        if source.is_dir():
            shutil.copytree(source, destination, dirs_exist_ok=True, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        copied.append(item["destination"])
    manifest = {
        "bundle_root": str(bundle_root),
        "copied": copied,
        "skipped": skipped,
    }
    write_json(manifest, bundle_root / "bundle_manifest.json")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the Stage 9 representative execution suite, performance benchmark, and standard release bundle packaging."
    )
    parser.add_argument("--workspace-root", type=Path, default=default_workspace_root())
    parser.add_argument("--sabr", type=Path, help="Path to sabr executable.")
    parser.add_argument("--output-root", type=Path, help="Stage 9 output root.")
    parser.add_argument("--experiment", action="append", default=[])
    parser.add_argument("--matrix-limit", type=int, default=9)
    parser.add_argument("--skip-benchmark", action="store_true")
    parser.add_argument("--skip-bundle", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    workspace_root = args.workspace_root.resolve()
    sabr_executable = (args.sabr or default_executable(workspace_root)).resolve()
    output_root = (args.output_root or (workspace_root / "results" / "stage9")).resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    if not args.dry_run and not sabr_executable.exists():
        print(f"sabr executable not found: {sabr_executable}", file=sys.stderr)
        return 1

    plan = build_execution_plan(
        workspace_root,
        sabr_executable,
        output_root,
        args.matrix_limit,
        set(args.experiment),
        not args.skip_benchmark,
        not args.skip_bundle,
    )
    plan_path = output_root / "stage9_execution_plan.json"
    write_json(plan, plan_path)

    if args.dry_run:
        print(f"plan: {plan_path}")
        return 0

    report_path = output_root / "stage9_execution_report.json"
    report: dict[str, object] = {
        "plan_path": str(plan_path),
        "benchmark": None,
        "experiments": [],
        "global_export": None,
        "global_plot": None,
        "bundle": None,
        "success": True,
    }

    if plan["benchmark"] is not None:
        benchmark_result = run_command(plan["benchmark"]["command"])
        report["benchmark"] = benchmark_result
        report["success"] = report["success"] and benchmark_result["success"]

    for experiment in plan["experiments"]:
        experiment_result = {
            "experiment_name": experiment["experiment_name"],
            "runs": [],
            "export": None,
            "plots": [],
            "success": True,
        }
        for run in experiment["runs"]:
            run_result = run_command(run["command"])
            run_result["name"] = run["name"]
            experiment_result["runs"].append(run_result)
            experiment_result["success"] = experiment_result["success"] and run_result["success"]

        if experiment_result["success"]:
            export_result = run_command(experiment["export"]["command"])
            experiment_result["export"] = export_result
            experiment_result["success"] = experiment_result["success"] and export_result["success"]

        if experiment_result["success"]:
            for plot in experiment["plots"]:
                plot_result = run_command(plot["command"])
                plot_result["kind"] = plot["kind"]
                experiment_result["plots"].append(plot_result)
                experiment_result["success"] = experiment_result["success"] and plot_result["success"]

        report["experiments"].append(experiment_result)
        report["success"] = report["success"] and experiment_result["success"]

    if report["success"]:
        global_export_result = run_command(plan["global_export"]["command"])
        report["global_export"] = global_export_result
        report["success"] = report["success"] and global_export_result["success"]

    if report["success"]:
        global_plot_result = run_command(plan["global_plot"]["command"])
        report["global_plot"] = global_plot_result
        report["success"] = report["success"] and global_plot_result["success"]

    write_json(report, report_path)

    if report["success"] and plan["bundle"] is not None:
        bundle_manifest = copy_release_sources(Path(plan["bundle"]["bundle_root"]), plan["bundle"]["sources"])
        report["bundle"] = bundle_manifest

    write_json(report, report_path)
    print(f"report: {report_path}")
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())