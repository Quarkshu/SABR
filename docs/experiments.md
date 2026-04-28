# Experiments

For a comprehensive Chinese manual covering environment setup, experiment execution, export, plotting, Stage 9 validation, and delivery workflow, see [experiments_zh.md](experiments_zh.md).

This workspace contains six experiment families under [configs/experiments](../configs/experiments).

## Experiment Catalog

| Experiment | Directory | Goal | Recommended smoke command |
|---|---|---|---|
| Exp1 | [configs/experiments/exp1_unicast_correctness](../configs/experiments/exp1_unicast_correctness) | Reference unicast correctness on the five-node topology | `.\build\sabr.exe run-scenario --config .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\exp1_smoke` |
| Exp2 | [configs/experiments/exp2_unicast_scale](../configs/experiments/exp2_unicast_scale) | Unicast scale and parameter sweep | `.\build\sabr.exe run-experiment --matrix .\configs\experiments\exp2_unicast_scale\matrix.json --output .\results\exp2_smoke --limit 3` |
| Exp3 | [configs/experiments/exp3_multicast_plan](../configs/experiments/exp3_multicast_plan) | Multicast planning and split-unicast control comparison | `.\build\sabr.exe run-scenario --config .\configs\experiments\exp3_multicast_plan\tree_plan.json --output .\results\exp3_smoke` |
| Exp4 | [configs/experiments/exp4_multicast_repair](../configs/experiments/exp4_multicast_repair) | Multicast local repair under plan replacement | `.\build\sabr.exe run-scenario --config .\configs\experiments\exp4_multicast_repair\baseline.json --output .\results\exp4_smoke` |
| Exp5 | [configs/experiments/exp5_redundancy](../configs/experiments/exp5_redundancy) | Redundancy modes and failure background comparison | `.\build\sabr.exe run-scenario --config .\configs\experiments\exp5_redundancy\single_backup.json --output .\results\exp5_smoke` |
| Exp6 | [configs/experiments/exp6_ablation](../configs/experiments/exp6_ablation) | Enhancement and redundancy ablation study | `.\build\sabr.exe run-scenario --config .\configs\experiments\exp6_ablation\baseline.json --output .\results\exp6_smoke` |

## Batch Workflow

Run the built-in smoke sweep:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\run_experiments.py --mode smoke --sabr .\build\sabr.exe --output-root .\results\script_smoke
```

Run the full matrix sweep for selected experiments:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\run_experiments.py --mode full --sabr .\build\sabr.exe --experiment exp2_unicast_scale --experiment exp5_redundancy --output-root .\results\full_runs --limit 9
```

## Stage 9 Formal Workflow

Run the NF-PF-01 benchmark in the validated Windows + MinGW environment:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\benchmark_nf_pf_01.py --output-root .\results\stage9\performance
```

Run the full Stage 9 suite:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\stage9_suite.py --output-root .\results\stage9\final
```

The built-in representative set currently covers:

- `exp1_unicast_correctness`: `baseline.json`
- `exp2_unicast_scale`: `matrix.json` with `--limit 9`
- `exp3_multicast_plan`: `tree_plan.json` and `split_unicast_control.json`
- `exp4_multicast_repair`: `baseline.json`
- `exp5_redundancy`: `primary_only.json` and `single_backup.json`
- `exp6_ablation`: `matrix.json` with `--limit 9`

The suite writes one consolidated output root containing:

- `performance/` for the NF-PF-01 scenario and performance report
- `representative/` for per-experiment run directories
- `exports/` for per-experiment and global aggregated summaries
- `plots/` for representative SVG outputs
- `release_bundle/` for the standard reproducibility package
- `stage9_execution_plan.json` and `stage9_execution_report.json` for auditability

## Export Workflow

Create flat and pivot summaries from any result root:

```powershell
.\build\sabr.exe export --input .\results\script_smoke --output .\results\export_smoke --format both --metric summary::delivery_rate --row-key scenario_name
```

Useful export keys:

- `summary::delivery_rate`
- `summary::average_delivery_latency`
- `summary::average_hop_count`
- `summary::route_failures`
- `summary::local_repair_count`
- `summary::reactive_anti_loop_trigger_count`
- `summary::redundancy_trigger_count`
- `coord::traffic[0].payload_size`
- `coord::simulation.phase1.k_paths`

If the input root contains matrix results with coordinates, a pivot export can produce matrix-oriented CSV files. For example:

```powershell
.\build\sabr.exe export --input .\results\cli_matrix_smoke --output .\results\export_matrix --format pivot --metric summary::delivery_rate --row-key coord::traffic[0].payload_size --column-key coord::simulation.phase1.k_paths
```

## Plot Workflow

Topology plot from a scenario or contact plan:

```powershell
.\build\sabr.exe plot --kind topology --input .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\plot_smoke\topology.svg
```

Timeline plot from a run directory or stats.json:

```powershell
.\build\sabr.exe plot --kind timeline --input .\results\script_smoke\exp1_unicast_correctness --output .\results\plot_smoke\timeline.svg
```

Utilization plot from a run directory or contact_utilization.csv:

```powershell
.\build\sabr.exe plot --kind utilization --input .\results\script_smoke\exp1_unicast_correctness --output .\results\plot_smoke\utilization.svg
```

Comparison plot from aggregated results:

```powershell
.\build\sabr.exe plot --kind comparison --input .\results\export_smoke\aggregated_summary.csv --output .\results\plot_smoke\comparison.svg --metric summary::delivery_rate --x-key scenario_name --title Smoke Delivery Comparison
```

## Output Conventions

Per-run files:

- `stats.json`
- `bundle_summary.csv`
- `contact_utilization.csv`
- `run_manifest.json`

Per-export files:

- `aggregated_summary.json`
- `aggregated_summary.csv`
- `aggregated_pivot.csv`

Recommended chart filenames:

- `topology.svg`
- `timeline.svg`
- `utilization.svg`
- `comparison.svg`

## Existing Workspace Examples

Generated examples currently available:

- [results/script_smoke](../results/script_smoke)
- [results/export_smoke](../results/export_smoke)
- [results/plot_smoke](../results/plot_smoke)

Use those directories as templates when preparing reproducible experiment bundles or reports.

For the standard Stage 9 reproducibility bundle structure and delivery notes, see [docs/reproducibility.md](reproducibility.md).