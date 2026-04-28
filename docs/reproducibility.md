# Reproducibility And Delivery

Stage 9 adds two reproducibility entry points on top of the existing experiment and plotting workflow:

- `scripts/benchmark_nf_pf_01.py` for the NF-PF-01 performance benchmark
- `scripts/stage9_suite.py` for representative experiment execution, export, plotting, and standard release bundle packaging

## NF-PF-01 Benchmark

Run the benchmark:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\benchmark_nf_pf_01.py --output-root .\results\stage9\performance
```

What the benchmark does:

- generates a deterministic 20-node / 200-contact / 1000-bundle scenario
- runs `sabr run-scenario` against that generated scenario
- records runtime and result metadata in `performance_report.json`

Output layout:

```text
results/stage9/performance/
  generated_benchmark_scenario.json
  performance_report.json
  run/
    stats.json
    bundle_summary.csv
    contact_utilization.csv
    run_manifest.json
```

## Representative Stage 9 Suite

Run the full suite:

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\stage9_suite.py --output-root .\results\stage9\final
```

Useful options:

- `--experiment <name>` to restrict execution to selected experiment families
- `--matrix-limit <n>` to control the representative matrix subset size
- `--skip-benchmark` to skip the NF-PF-01 step when only the experiment delivery chain is needed
- `--skip-bundle` to skip release bundle generation
- `--dry-run` to only write `stage9_execution_plan.json`

Representative catalog used by default:

- `exp1_unicast_correctness`: `baseline.json`
- `exp2_unicast_scale`: `matrix.json` with a representative limit
- `exp3_multicast_plan`: `tree_plan.json` plus `split_unicast_control.json`
- `exp4_multicast_repair`: `baseline.json`
- `exp5_redundancy`: `primary_only.json` plus `single_backup.json`
- `exp6_ablation`: `matrix.json` with a representative limit

Output layout:

```text
results/stage9/final/
  performance/
  representative/
  exports/
  plots/
  release_bundle/
  stage9_execution_plan.json
  stage9_execution_report.json
```

## Standard Release Bundle

The standard Stage 9 release bundle contains:

- `README.md`
- `docs/`
- `configs/experiments/`
- `scripts/` entries needed for benchmark, export, plotting, and representative execution
- `samples/performance/`
- `samples/representative/`
- `samples/exports/`
- `samples/plots/`
- `samples/stage9_execution_plan.json`
- `samples/stage9_execution_report.json`

The bundle manifest is written to:

```text
results/stage9/final/release_bundle/bundle_manifest.json
```

## Recommended Final Verification

Before shipping or archiving a Stage 9 bundle, rerun the following commands in order:

```powershell
cmake --build build --target sabr sabr_tests
ctest --test-dir build --output-on-failure
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\stage9_suite.py --output-root .\results\stage9\final
```

This sequence verifies the C++ baseline, the Python Stage 9 checks, the NF-PF-01 benchmark, the representative experiment set, the SVG outputs, and the release bundle in one pass.