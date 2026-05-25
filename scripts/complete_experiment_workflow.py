from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

from sabr_results import write_json


EXPERIMENT_METADATA: dict[str, dict[str, str]] = {
    "exp1_unicast_correctness": {
        "script_name": "run_exp1_complete.py",
        "output_prefix": "exp1_complete",
        "display_name": "Exp1 unicast correctness",
    },
    "exp2_unicast_scale": {
        "script_name": "run_exp2_complete.py",
        "output_prefix": "exp2_complete",
        "display_name": "Exp2 unicast scale",
    },
    "exp3_multicast_plan": {
        "script_name": "run_exp3_complete.py",
        "output_prefix": "exp3_complete",
        "display_name": "Exp3 multicast plan",
    },
    "exp4_multicast_repair": {
        "script_name": "run_exp4_complete.py",
        "output_prefix": "exp4_complete",
        "display_name": "Exp4 multicast repair",
    },
    "exp5_redundancy": {
        "script_name": "run_exp5_complete.py",
        "output_prefix": "exp5_complete",
        "display_name": "Exp5 redundancy",
    },
    "exp6_ablation": {
        "script_name": "run_exp6_complete.py",
        "output_prefix": "exp6_complete",
        "display_name": "Exp6 ablation",
    },
}


def default_workspace_root() -> Path:
    return Path(__file__).resolve().parent.parent


def default_executable(workspace_root: Path) -> Path:
    candidate = workspace_root / "build" / "sabr.exe"
    if candidate.exists():
        return candidate
    return workspace_root / "build" / "sabr"


def build_default_output_root(workspace_root: Path, experiment_name: str) -> Path:
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    prefix = EXPERIMENT_METADATA[experiment_name]["output_prefix"]
    return (workspace_root / "results" / f"{prefix}_{timestamp}").resolve()


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


def scenario_step(executable: Path,
                  name: str,
                  config_path: Path,
                  output_path: Path) -> dict[str, object]:
    return {
        "name": name,
        "kind": "run-scenario",
        "input_path": str(config_path),
        "output_path": str(output_path),
        "command": [
            str(executable),
            "run-scenario",
            "--config",
            str(config_path),
            "--output",
            str(output_path),
        ],
    }


def matrix_step(executable: Path,
                name: str,
                matrix_path: Path,
                output_path: Path) -> dict[str, object]:
    return {
        "name": name,
        "kind": "run-experiment",
        "input_path": str(matrix_path),
        "output_path": str(output_path),
        "command": [
            str(executable),
            "run-experiment",
            "--matrix",
            str(matrix_path),
            "--output",
            str(output_path),
        ],
    }


def export_step(executable: Path,
                name: str,
                input_path: Path,
                output_path: Path,
                metric: str,
                row_key: str,
                column_key: str = "") -> dict[str, object]:
    command = [
        str(executable),
        "export",
        "--input",
        str(input_path),
        "--output",
        str(output_path),
        "--format",
        "both",
        "--metric",
        metric,
        "--row-key",
        row_key,
    ]
    if column_key:
        command.extend(["--column-key", column_key])
    return {
        "name": name,
        "kind": "export",
        "input_path": str(input_path),
        "output_path": str(output_path),
        "command": command,
    }


def plot_step(executable: Path,
              name: str,
              kind: str,
              input_path: Path,
              output_path: Path,
              metric: str = "",
              x_key: str = "",
              series_key: str = "",
              title: str = "") -> dict[str, object]:
    command = [
        str(executable),
        "plot",
        "--kind",
        kind,
        "--input",
        str(input_path),
        "--output",
        str(output_path),
    ]
    if metric:
        command.extend(["--metric", metric])
    if x_key:
        command.extend(["--x-key", x_key])
    if series_key:
        command.extend(["--series-key", series_key])
    if title:
        command.extend(["--title", title])
    return {
        "name": name,
        "kind": f"plot:{kind}",
        "input_path": str(input_path),
        "output_path": str(output_path),
        "command": command,
    }


def build_complete_experiment_catalog(workspace_root: Path) -> dict[str, dict[str, object]]:
    experiments_root = workspace_root / "configs" / "experiments"
    return {
        "exp1_unicast_correctness": {
            "metadata": EXPERIMENT_METADATA["exp1_unicast_correctness"],
            "experiment_dir": experiments_root / "exp1_unicast_correctness",
        },
        "exp2_unicast_scale": {
            "metadata": EXPERIMENT_METADATA["exp2_unicast_scale"],
            "experiment_dir": experiments_root / "exp2_unicast_scale",
        },
        "exp3_multicast_plan": {
            "metadata": EXPERIMENT_METADATA["exp3_multicast_plan"],
            "experiment_dir": experiments_root / "exp3_multicast_plan",
        },
        "exp4_multicast_repair": {
            "metadata": EXPERIMENT_METADATA["exp4_multicast_repair"],
            "experiment_dir": experiments_root / "exp4_multicast_repair",
        },
        "exp5_redundancy": {
            "metadata": EXPERIMENT_METADATA["exp5_redundancy"],
            "experiment_dir": experiments_root / "exp5_redundancy",
        },
        "exp6_ablation": {
            "metadata": EXPERIMENT_METADATA["exp6_ablation"],
            "experiment_dir": experiments_root / "exp6_ablation",
        },
    }


def build_execution_plan(workspace_root: Path,
                         executable: Path,
                         experiment_name: str,
                         output_root: Path) -> dict[str, object]:
    catalog = build_complete_experiment_catalog(workspace_root)
    if experiment_name not in catalog:
        raise ValueError(f"unsupported experiment: {experiment_name}")

    experiment_dir = Path(catalog[experiment_name]["experiment_dir"])
    metadata = catalog[experiment_name]["metadata"]
    steps: list[dict[str, object]] = []

    if experiment_name == "exp1_unicast_correctness":
        scenarios_root = output_root / "scenarios"
        matrix_root = output_root / "matrix"
        exports_root = output_root / "exports"
        plots_root = output_root / "plots"
        steps = [
            scenario_step(executable, "baseline", experiment_dir / "baseline.json", scenarios_root / "baseline"),
            scenario_step(executable, "reference_baseline", experiment_dir / "stage5_reference_baseline.json", scenarios_root / "reference_baseline"),
            scenario_step(executable, "downlink_split", experiment_dir / "stage5_downlink_split.json", scenarios_root / "downlink_split"),
            matrix_step(executable, "matrix", experiment_dir / "matrix.json", matrix_root),
            export_step(executable, "export_scenarios", scenarios_root, exports_root / "scenarios", "summary::delivery_rate", "scenario_name"),
            export_step(executable, "export_matrix", matrix_root, exports_root / "matrix", "summary::delivery_rate", "coord::traffic[0].payload_size", "coord::simulation.phase1.k_paths"),
            plot_step(executable, "topology", "topology", experiment_dir / "baseline.json", plots_root / "exp1_topology.svg", title="Exp1 Topology"),
            plot_step(executable, "baseline_timeline", "timeline", scenarios_root / "baseline", plots_root / "exp1_baseline_timeline.svg", title="Exp1 Baseline Timeline"),
            plot_step(executable, "baseline_utilization", "utilization", scenarios_root / "baseline", plots_root / "exp1_baseline_utilization.svg", title="Exp1 Baseline Utilization"),
            plot_step(executable, "scenarios_comparison", "comparison", exports_root / "scenarios" / "aggregated_summary.csv", plots_root / "exp1_scenarios_comparison.svg", metric="summary::delivery_rate", x_key="scenario_name", title="Exp1 Scenario Delivery Comparison"),
            plot_step(executable, "matrix_comparison", "comparison", exports_root / "matrix" / "aggregated_summary.csv", plots_root / "exp1_matrix_comparison.svg", metric="summary::delivery_rate", x_key="coord::traffic[0].payload_size", series_key="coord::simulation.phase1.k_paths", title="Exp1 Matrix Delivery Comparison"),
        ]
    elif experiment_name == "exp2_unicast_scale":
        baseline_root = output_root / "baseline"
        matrix_root = output_root / "matrix"
        exports_root = output_root / "exports"
        plots_root = output_root / "plots"
        steps = [
            scenario_step(executable, "baseline", experiment_dir / "baseline.json", baseline_root / "baseline"),
            matrix_step(executable, "matrix", experiment_dir / "matrix.json", matrix_root),
            export_step(executable, "export_baseline", baseline_root, exports_root / "baseline", "summary::delivery_rate", "scenario_name"),
            export_step(executable, "export_matrix", matrix_root, exports_root / "matrix", "summary::delivery_rate", "coord::traffic[0].bundle_count", "coord::simulation.phase1.k_paths"),
            plot_step(executable, "baseline_utilization", "utilization", baseline_root / "baseline", plots_root / "exp2_baseline_utilization.svg", title="Exp2 Baseline Utilization"),
            plot_step(executable, "baseline_timeline", "timeline", baseline_root / "baseline", plots_root / "exp2_baseline_timeline.svg", title="Exp2 Baseline Timeline"),
            plot_step(executable, "matrix_comparison", "comparison", exports_root / "matrix" / "aggregated_summary.csv", plots_root / "exp2_matrix_comparison.svg", metric="summary::delivery_rate", x_key="coord::traffic[0].bundle_count", series_key="coord::simulation.phase1.k_paths", title="Exp2 Matrix Delivery Comparison"),
        ]
    elif experiment_name == "exp3_multicast_plan":
        scenarios_root = output_root / "scenarios"
        matrix_root = output_root / "matrix"
        exports_root = output_root / "exports"
        plots_root = output_root / "plots"
        steps = [
            scenario_step(executable, "tree_plan", experiment_dir / "tree_plan.json", scenarios_root / "tree_plan"),
            scenario_step(executable, "split_unicast_control", experiment_dir / "split_unicast_control.json", scenarios_root / "split_unicast_control"),
            matrix_step(executable, "matrix", experiment_dir / "matrix.json", matrix_root),
            export_step(executable, "export_scenarios", scenarios_root, exports_root / "scenarios", "summary::delivery_rate", "scenario_name"),
            export_step(executable, "export_matrix", matrix_root, exports_root / "matrix", "summary::delivery_rate", "coord::traffic[*].bundle_count", "coord::multicast.group_size"),
            plot_step(executable, "topology", "topology", experiment_dir / "tree_plan.json", plots_root / "exp3_topology.svg", title="Exp3 Multicast Topology"),
            plot_step(executable, "scenarios_comparison", "comparison", exports_root / "scenarios" / "aggregated_summary.csv", plots_root / "exp3_scenarios_comparison.svg", metric="summary::delivery_rate", x_key="scenario_name", title="Exp3 Scenario Delivery Comparison"),
            plot_step(executable, "matrix_comparison", "comparison", exports_root / "matrix" / "aggregated_summary.csv", plots_root / "exp3_matrix_comparison.svg", metric="summary::delivery_rate", x_key="coord::traffic[*].bundle_count", series_key="scenario_name", title="Exp3 Matrix Delivery Comparison"),
        ]
    elif experiment_name == "exp4_multicast_repair":
        baseline_root = output_root / "baseline"
        matrix_root = output_root / "matrix"
        exports_root = output_root / "exports"
        plots_root = output_root / "plots"
        steps = [
            scenario_step(executable, "baseline", experiment_dir / "baseline.json", baseline_root / "baseline"),
            matrix_step(executable, "matrix", experiment_dir / "matrix.json", matrix_root),
            export_step(executable, "export_baseline", baseline_root, exports_root / "baseline", "derived::receiver_completion", "scenario_name"),
            export_step(executable, "export_matrix", matrix_root, exports_root / "matrix", "derived::receiver_completion", "coord::failure_injection[0].trigger_time", "coord::traffic[0].bundle_count"),
            plot_step(executable, "baseline_timeline", "timeline", baseline_root / "baseline", plots_root / "exp4_baseline_timeline.svg", title="Exp4 Baseline Timeline"),
            plot_step(executable, "matrix_comparison", "comparison", exports_root / "matrix" / "aggregated_summary.csv", plots_root / "exp4_matrix_comparison.svg", metric="derived::receiver_completion", x_key="coord::failure_injection[0].trigger_time", series_key="coord::traffic[0].bundle_count", title="Exp4 Matrix Receiver Completion Comparison"),
        ]
    elif experiment_name == "exp5_redundancy":
        scenarios_root = output_root / "scenarios"
        matrix_root = output_root / "matrix"
        exports_root = output_root / "exports"
        plots_root = output_root / "plots"
        matrix_group_root = matrix_root / "exp5_redundancy"
        steps = [
            scenario_step(executable, "primary_only", experiment_dir / "primary_only.json", scenarios_root / "primary_only"),
            scenario_step(executable, "single_backup", experiment_dir / "single_backup.json", scenarios_root / "single_backup"),
            scenario_step(executable, "multi_backup", experiment_dir / "multi_backup.json", scenarios_root / "multi_backup"),
            scenario_step(executable, "trunk_only", experiment_dir / "trunk_only.json", scenarios_root / "trunk_only"),
            matrix_step(executable, "matrix", experiment_dir / "matrix.json", matrix_root),
            export_step(executable, "export_scenarios", scenarios_root, exports_root / "scenarios", "summary::delivery_rate", "scenario_name"),
            export_step(executable, "export_matrix", matrix_root, exports_root / "matrix", "summary::delivery_rate", "coord::failures[0].probability", "coord::redundancy.mode"),
            export_step(executable, "export_matrix_primary_only", matrix_group_root / "exp5_redundancy_primary_only", exports_root / "matrix_primary_only", "summary::delivery_rate", "coord::failures[0].probability", "coord::redundancy.mode"),
            export_step(executable, "export_matrix_single_backup", matrix_group_root / "exp5_redundancy_single_backup", exports_root / "matrix_single_backup", "summary::delivery_rate", "coord::failures[0].probability", "coord::redundancy.mode"),
            plot_step(executable, "scenarios_comparison", "comparison", exports_root / "scenarios" / "aggregated_summary.csv", plots_root / "exp5_scenarios_comparison.svg", metric="summary::delivery_rate", x_key="scenario_name", title="Exp5 Scenario Delivery Comparison"),
            plot_step(executable, "matrix_comparison", "comparison", exports_root / "matrix" / "aggregated_summary.csv", plots_root / "exp5_matrix_comparison.svg", metric="summary::delivery_rate", x_key="coord::failures[0].probability", series_key="coord::redundancy.mode", title="Exp5 Matrix Delivery Comparison"),
            plot_step(executable, "matrix_primary_only_comparison", "comparison", exports_root / "matrix_primary_only" / "aggregated_summary.csv", plots_root / "exp5_matrix_primary_only_comparison.svg", metric="summary::delivery_rate", x_key="coord::failures[0].probability", series_key="coord::redundancy.mode", title="Exp5 Primary-Only Matrix Delivery Comparison"),
            plot_step(executable, "matrix_single_backup_comparison", "comparison", exports_root / "matrix_single_backup" / "aggregated_summary.csv", plots_root / "exp5_matrix_single_backup_comparison.svg", metric="summary::delivery_rate", x_key="coord::failures[0].probability", series_key="coord::redundancy.mode", title="Exp5 Single-Backup Matrix Delivery Comparison"),
        ]
    else:
        baseline_root = output_root / "baseline"
        matrix_root = output_root / "matrix"
        exports_root = output_root / "exports"
        plots_root = output_root / "plots"
        steps = [
            scenario_step(executable, "baseline", experiment_dir / "baseline.json", baseline_root / "baseline"),
            matrix_step(executable, "matrix", experiment_dir / "matrix.json", matrix_root),
            export_step(executable, "export_baseline", baseline_root, exports_root / "baseline", "summary::delivery_rate", "scenario_name"),
            export_step(executable, "export_matrix", matrix_root, exports_root / "matrix", "summary::delivery_rate", "coord::simulation.phase1.k_paths", "coord::redundancy.mode"),
            plot_step(executable, "baseline_timeline", "timeline", baseline_root / "baseline", plots_root / "exp6_baseline_timeline.svg", title="Exp6 Baseline Timeline"),
            plot_step(executable, "matrix_comparison", "comparison", exports_root / "matrix" / "aggregated_summary.csv", plots_root / "exp6_matrix_comparison.svg", metric="summary::delivery_rate", x_key="coord::simulation.phase1.k_paths", series_key="coord::redundancy.mode", title="Exp6 Matrix Delivery Comparison"),
        ]

    return {
        "experiment_name": experiment_name,
        "display_name": metadata["display_name"],
        "script_name": metadata["script_name"],
        "workspace_root": str(workspace_root),
        "sabr_executable": str(executable),
        "output_root": str(output_root),
        "steps": steps,
    }


def execute_execution_plan(plan: dict[str, object]) -> dict[str, object]:
    report = {
        "experiment_name": plan["experiment_name"],
        "plan_path": str(Path(plan["output_root"]) / "execution_plan.json"),
        "steps": [],
        "success": True,
    }
    for step in plan["steps"]:
        result = run_command(step["command"])
        result["name"] = step["name"]
        result["kind"] = step["kind"]
        result["input_path"] = step.get("input_path", "")
        result["output_path"] = step.get("output_path", "")
        report["steps"].append(result)
        if not result["success"]:
            report["success"] = False
            break
    return report


def build_argument_parser(experiment_name: str) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=f"Run the complete workflow for {EXPERIMENT_METADATA[experiment_name]['display_name']}."
    )
    parser.add_argument("--workspace-root", type=Path, default=default_workspace_root())
    parser.add_argument("--sabr", type=Path, help="Path to sabr executable.")
    parser.add_argument("--output-root", type=Path, help="Output directory for this complete experiment workflow.")
    parser.add_argument("--dry-run", action="store_true", help="Only write execution_plan.json without running commands.")
    return parser


def main_for_experiment(experiment_name: str) -> int:
    parser = build_argument_parser(experiment_name)
    args = parser.parse_args()

    workspace_root = args.workspace_root.resolve()
    executable = (args.sabr or default_executable(workspace_root)).resolve()
    output_root = args.output_root.resolve() if args.output_root else build_default_output_root(workspace_root, experiment_name)
    output_root.mkdir(parents=True, exist_ok=True)

    if not args.dry_run and not executable.exists():
        print(f"sabr executable not found: {executable}", file=sys.stderr)
        return 1

    plan = build_execution_plan(workspace_root, executable, experiment_name, output_root)
    plan_path = output_root / "execution_plan.json"
    write_json(plan, plan_path)

    if args.dry_run:
        print(f"plan: {plan_path}")
        return 0

    report = execute_execution_plan(plan)
    report_path = output_root / "execution_report.json"
    write_json(report, report_path)

    print(f"output: {output_root}")
    print(f"report: {report_path}")
    return 0 if report["success"] else 1