# SABR

SABR is a C++17 space DTN CGR/SABR simulation workspace with experiment orchestration, result export, and plotting support.

## Build

Primary validated path in this workspace:

```powershell
cmake --build build --target sabr sabr_tests
ctest --test-dir build --output-on-failure
```

`CMake Tools` remains unreliable in this workspace. The stable path is the existing `build` directory with terminal-based `cmake --build` and `ctest`.

## Quick Start

Run one scenario:

```powershell
.\build\sabr.exe run-scenario --config .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\quick_start\exp1
```

Run one matrix experiment with a smoke limit:

```powershell
.\build\sabr.exe run-experiment --matrix .\configs\experiments\exp2_unicast_scale\matrix.json --output .\results\quick_start_matrix --limit 3
```

Run the six smoke experiments through the Python helper:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\run_experiments.py --mode smoke --sabr .\build\sabr.exe --output-root .\results\script_smoke
```

Run the Stage 9 NF-PF-01 benchmark:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\benchmark_nf_pf_01.py --output-root .\results\stage9\performance
```

Run the full Stage 9 suite with representative experiments, exports, plots, and release bundle packaging:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\stage9_suite.py --output-root .\results\stage9\final
```

## CLI

Show help:

```powershell
.\build\sabr.exe --help
```

Export flat and pivot summaries from a results directory:

```powershell
.\build\sabr.exe export --input .\results\script_smoke --output .\results\export_smoke --format both --metric summary::delivery_rate --row-key scenario_name
```

Generate plots:

```powershell
.\build\sabr.exe plot --kind topology --input .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\plot_smoke\topology.svg
.\build\sabr.exe plot --kind timeline --input .\results\script_smoke\exp1_unicast_correctness --output .\results\plot_smoke\timeline.svg
.\build\sabr.exe plot --kind utilization --input .\results\script_smoke\exp1_unicast_correctness --output .\results\plot_smoke\utilization.svg
.\build\sabr.exe plot --kind comparison --input .\results\export_smoke\aggregated_summary.csv --output .\results\plot_smoke\comparison.svg --metric summary::delivery_rate --x-key scenario_name --title Smoke Delivery Comparison
```

## Result Layout

Single scenario output:

```text
results/<run_name>/
  stats.json
  bundle_summary.csv
  contact_utilization.csv
  run_manifest.json
```

Matrix output:

```text
results/<run_name>/<experiment>/<scenario>/matrix_0001/
  stats.json
  bundle_summary.csv
  contact_utilization.csv
  run_manifest.json
```

Aggregated export output:

```text
results/<export_name>/
  aggregated_summary.json
  aggregated_summary.csv
  aggregated_pivot.csv
```

Plot output naming convention:

```text
topology.svg
timeline.svg
utilization.svg
comparison.svg
```

Stage 9 validation output:

```text
results/stage9/<run_name>/
  performance/
    generated_benchmark_scenario.json
    performance_report.json
    run/
  representative/
  exports/
  plots/
  release_bundle/
  stage9_execution_plan.json
  stage9_execution_report.json
```

## Sample Inputs And Outputs

Sample inputs:

- [configs/experiments/exp1_unicast_correctness/baseline.json](configs/experiments/exp1_unicast_correctness/baseline.json)
- [configs/experiments/exp2_unicast_scale/matrix.json](configs/experiments/exp2_unicast_scale/matrix.json)
- [configs/experiments/exp5_redundancy/multi_backup.json](configs/experiments/exp5_redundancy/multi_backup.json)

Sample outputs generated in this workspace:

- [results/script_smoke/exp1_unicast_correctness/run_manifest.json](results/script_smoke/exp1_unicast_correctness/run_manifest.json)
- [results/export_smoke/aggregated_summary.csv](results/export_smoke/aggregated_summary.csv)
- [results/export_smoke/aggregated_pivot.csv](results/export_smoke/aggregated_pivot.csv)
- [results/plot_smoke/topology.svg](results/plot_smoke/topology.svg)
- [results/plot_smoke/timeline.svg](results/plot_smoke/timeline.svg)
- [results/plot_smoke/utilization.svg](results/plot_smoke/utilization.svg)
- [results/plot_smoke/comparison.svg](results/plot_smoke/comparison.svg)

## Documentation

- [docs/experiments.md](docs/experiments.md) — experiment catalog, commands, output conventions, and chart workflow
- [docs/experiments_zh.md](docs/experiments_zh.md) — 中文实验手册，覆盖环境、命令、实验一到实验六、阶段 9 验收与交付流程
- [docs/metrics.md](docs/metrics.md) — summary field and CSV column reference
- [docs/reproducibility.md](docs/reproducibility.md) — Stage 9 benchmark, representative suite, and standard release bundle workflow