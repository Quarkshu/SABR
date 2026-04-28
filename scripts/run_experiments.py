from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def default_workspace_root() -> Path:
    return Path(__file__).resolve().parent.parent


def default_executable(workspace_root: Path) -> Path:
    candidate = workspace_root / "build" / "sabr.exe"
    if candidate.exists():
        return candidate
    return workspace_root / "build" / "sabr"


def discover_experiment_dirs(experiments_root: Path, selected: set[str]) -> list[Path]:
    experiment_dirs = [path for path in sorted(experiments_root.iterdir()) if path.is_dir()]
    if not selected:
        return experiment_dirs
    return [path for path in experiment_dirs if path.name in selected]


def choose_smoke_scenario(experiment_dir: Path) -> Path:
    preferred = [
        "smoke.json",
        "baseline.json",
        "tree_plan.json",
        "single_backup.json",
        "primary_only.json",
        "split_unicast_control.json",
    ]
    for name in preferred:
        candidate = experiment_dir / name
        if candidate.exists():
            return candidate

    candidates = sorted(
        path for path in experiment_dir.glob("*.json") if path.name != "matrix.json"
    )
    if not candidates:
        raise FileNotFoundError(f"no smoke scenario found in {experiment_dir}")
    return candidates[0]


def run_command(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run SABR experiments in smoke or full mode.")
    parser.add_argument("--workspace-root", type=Path, default=default_workspace_root())
    parser.add_argument("--sabr", type=Path, help="Path to sabr executable.")
    parser.add_argument("--mode", choices=["smoke", "full"], default="smoke")
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--experiment", action="append", default=[])
    args = parser.parse_args()

    workspace_root = args.workspace_root.resolve()
    executable = (args.sabr or default_executable(workspace_root)).resolve()
    experiments_root = workspace_root / "configs" / "experiments"
    output_root = (args.output_root or (workspace_root / "results" / "automated_runs" / args.mode)).resolve()
    experiment_dirs = discover_experiment_dirs(experiments_root, set(args.experiment))

    if not executable.exists():
        print(f"sabr executable not found: {executable}", file=sys.stderr)
        return 1

    failures: list[tuple[str, str]] = []
    for experiment_dir in experiment_dirs:
        if args.mode == "smoke":
            scenario_path = choose_smoke_scenario(experiment_dir)
            command = [
                str(executable),
                "run-scenario",
                "--config",
                str(scenario_path),
                "--output",
                str(output_root / experiment_dir.name),
            ]
        else:
            matrix_path = experiment_dir / "matrix.json"
            if not matrix_path.exists():
                failures.append((experiment_dir.name, "missing matrix.json"))
                continue
            command = [
                str(executable),
                "run-experiment",
                "--matrix",
                str(matrix_path),
                "--output",
                str(output_root),
            ]
            if args.limit > 0:
                command.extend(["--limit", str(args.limit)])

        result = run_command(command)
        if result.returncode != 0:
            message = (result.stderr or result.stdout).strip()
            failures.append((experiment_dir.name, message or "unknown failure"))
        else:
            print(f"[ok] {experiment_dir.name}")

    if failures:
        for experiment_name, message in failures:
            print(f"[failed] {experiment_name}: {message}", file=sys.stderr)
        return 1

    print(f"completed {len(experiment_dirs)} experiment targets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())