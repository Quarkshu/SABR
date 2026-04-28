# SABR 中文实验手册

本手册面向当前工作区中的 SABR 仿真系统实验执行、结果导出、图表生成、性能验证与最终交付流程，目标是让使用者在不修改代码的前提下，仅通过配置文件、命令行和脚本完成实验一到实验六，以及阶段 9 的正式验收流程。

当前仓库已经具备以下基线能力：

- 单场景执行：`run-scenario`
- 参数矩阵执行：`run-experiment`
- 结果导出：`export`
- 图表生成：`plot`
- 批量 smoke / full 运行：`scripts/run_experiments.py`
- NF-PF-01 性能基准：`scripts/benchmark_nf_pf_01.py`
- 阶段 9 正式执行套件：`scripts/stage9_suite.py`

---

## 1. 手册目标

本手册覆盖以下内容：

- 实验环境准备与构建方法
- CLI 命令体系与结果目录约定
- 实验一到实验六的目的、输入文件、推荐命令与结果解读
- 批量运行、聚合导出与 SVG 图表工作流
- 阶段 9 的性能测试、正式代表性实验执行和标准复现包流程
- 常见问题与排错建议

如果你只需要最短路径入口，建议按以下顺序阅读：

1. 第 2 节 环境与构建
2. 第 3 节 目录与命令约定
3. 第 4 节 快速开始
4. 第 5 节 实验目录与执行说明
5. 第 8 节 阶段 9 正式验收流程

---

## 2. 环境与构建

### 2.1 当前验证环境

当前仓库的主验证环境为：

- 操作系统：Windows
- 编译链：MinGW / GCC
- 构建系统：CMake
- Python：工作区 `.venv`

当前工作区内，`CMake Tools` 并不是稳定主路径。权威构建和测试路径是终端中的 `cmake --build` 与 `ctest`。

### 2.2 构建命令

推荐执行：

```powershell
cmake -S . -B build
cmake --build build --target sabr sabr_tests
ctest --test-dir build --output-on-failure
```

说明：

- `sabr` 是主程序
- `sabr_tests` 是 C++ 与阶段 9 Python 验证混合回归入口
- 当前最终基线为 189/189 测试通过

### 2.3 Python 环境

推荐使用当前工作区虚拟环境：

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe
```

Python 脚本默认只依赖标准库和仓库内已有脚本，不需要额外安装绘图库。

---

## 3. 目录与命令约定

### 3.1 关键目录

| 目录 | 作用 |
|---|---|
| `configs/experiments/` | 六类实验的场景文件、矩阵文件和 contact plan 输入 |
| `scripts/` | 批量运行、导出、绘图、性能基准与阶段 9 套件脚本 |
| `results/` | 所有运行结果、导出结果、图表与交付样例目录 |
| `docs/` | 实验说明、指标说明、复现与交付说明 |
| `build/` | CMake 构建产物与主程序可执行文件 |

### 3.2 CLI 主命令

SABR 主程序支持以下命令：

- `run-scenario`：执行单个场景配置
- `run-experiment`：执行矩阵实验
- `export`：将结果目录导出为 flat / pivot 汇总
- `plot`：生成 topology / timeline / utilization / comparison 四类 SVG 图
- `help` / `--help`：查看帮助
- `--version`：查看版本

查看帮助：

```powershell
.\build\sabr.exe --help
```

### 3.3 结果目录约定

单场景输出：

```text
results/<run_name>/
  stats.json
  bundle_summary.csv
  contact_utilization.csv
  run_manifest.json
```

矩阵输出：

```text
results/<run_root>/<experiment>/<scenario>/matrix_0001/
  stats.json
  bundle_summary.csv
  contact_utilization.csv
  run_manifest.json
```

聚合导出输出：

```text
results/<export_name>/
  aggregated_summary.json
  aggregated_summary.csv
  aggregated_pivot.csv
```

阶段 9 正式收口输出：

```text
results/stage9/<run_name>/
  performance/
  representative/
  exports/
  plots/
  release_bundle/
  stage9_execution_plan.json
  stage9_execution_report.json
```

---

## 4. 快速开始

### 4.1 执行单个场景

```powershell
.\build\sabr.exe run-scenario --config .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\quick_start\exp1
```

适用场景：

- 验证单个配置文件是否可执行
- 快速检查 `stats.json`、`run_manifest.json` 是否正常生成
- 调试某一组固定输入

### 4.2 执行一个矩阵实验

```powershell
.\build\sabr.exe run-experiment --matrix .\configs\experiments\exp2_unicast_scale\matrix.json --output .\results\quick_start_matrix --limit 3
```

适用场景：

- 快速扫描参数变化对结果的影响
- 只取前若干个矩阵组合做 smoke 或轻量验证

### 4.3 跑六个实验的 smoke

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\run_experiments.py --mode smoke --sabr .\build\sabr.exe --output-root .\results\script_smoke
```

这条命令适合：

- 首次确认实验一到实验六都能走通
- 生成一组最小样例输出
- 为导出和绘图脚本准备输入目录

---

## 5. 实验目录与执行说明

本仓库当前包含六类实验，每类实验都位于 `configs/experiments/` 下的独立目录中。

### 5.1 总览表

| 实验 | 目录 | 目标 | 推荐入口 |
|---|---|---|---|
| Exp1 | `configs/experiments/exp1_unicast_correctness` | 五节点单播正确性参考场景 | 单场景 |
| Exp2 | `configs/experiments/exp2_unicast_scale` | 单播规模和参数扫描 | 矩阵 |
| Exp3 | `configs/experiments/exp3_multicast_plan` | 多播传播计划与 split-unicast 对照 | 单场景或矩阵 |
| Exp4 | `configs/experiments/exp4_multicast_repair` | 多播局部修复与 plan replacement | 单场景或矩阵 |
| Exp5 | `configs/experiments/exp5_redundancy` | 冗余模式和失效背景对照 | 单场景或矩阵 |
| Exp6 | `configs/experiments/exp6_ablation` | enhancement 与 redundancy 消融 | 单场景或矩阵 |

### 5.2 Exp1：单播正确性实验

**目标**

- 验证参考五节点拓扑上的基本单播行为
- 验证首跳分流、基础交付率和路径选择行为

**主要输入**

- `baseline.json`
- `stage5_reference_baseline.json`
- `stage5_downlink_split.json`
- `matrix.json`

**推荐命令**

最小运行：

```powershell
.\build\sabr.exe run-scenario --config .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\exp1_smoke
```

**重点观察项**

- `summary::delivery_rate`
- `summary::average_delivery_latency`
- `summary::average_hop_count`
- `bundles` 中每个 bundle 的 `final_state` 和 `route_path`

**适合生成的图**

- timeline：查看 bundle 何时创建、何时送达
- topology：查看 contact plan 拓扑结构

### 5.3 Exp2：单播规模实验

**目标**

- 评估流量规模、payload 大小、`k_paths` 和 `owlt_margin` 对单播性能的影响
- 为后续性能趋势分析和参数扫描提供矩阵数据

**主要输入**

- `baseline.json`
- `matrix.json`

`matrix.json` 当前包含以下维度：

- `traffic[0].bundle_count`
- `traffic[0].payload_size`
- `simulation.phase1.k_paths`
- `simulation.owlt_margin`

**推荐命令**

轻量扫描：

```powershell
.\build\sabr.exe run-experiment --matrix .\configs\experiments\exp2_unicast_scale\matrix.json --output .\results\exp2_smoke --limit 9
```

**重点观察项**

- `summary::delivery_rate`
- `summary::average_delivery_latency`
- `summary::route_failures`
- matrix 坐标字段，如 `coord::simulation.phase1.k_paths`

**适合生成的图**

- comparison：横轴使用 `coord::simulation.phase1.k_paths`
- pivot CSV：按 `owlt_margin` 或 `payload_size` 做列展开

### 5.4 Exp3：多播传播计划实验

**目标**

- 对比原生多播传播计划与 split-unicast control
- 观察共享前缀、多播树构造和目的节点覆盖行为

**主要输入**

- `tree_plan.json`
- `split_unicast_control.json`
- `matrix.json`

**推荐命令**

```powershell
.\build\sabr.exe run-scenario --config .\configs\experiments\exp3_multicast_plan\tree_plan.json --output .\results\exp3_tree
.\build\sabr.exe run-scenario --config .\configs\experiments\exp3_multicast_plan\split_unicast_control.json --output .\results\exp3_split
```

**重点观察项**

- `summary::delivery_rate`
- `summary::average_hop_count`
- bundle 记录里的多播相关状态
- 多播组规模变化对输出的影响

**适合生成的图**

- topology：查看 contact 拓扑
- comparison：对比 `tree_plan` 与 `split_unicast_control`

### 5.5 Exp4：多播局部修复实验

**目标**

- 验证 plan replacement 触发后的局部修复能力
- 观察局部修复是否影响最终交付率和延迟

**主要输入**

- `baseline.json`
- `matrix.json`

当前关注维度通常包括：

- `failure_injection[0].trigger_time`
- `traffic[0].bundle_count`

**推荐命令**

```powershell
.\build\sabr.exe run-scenario --config .\configs\experiments\exp4_multicast_repair\baseline.json --output .\results\exp4_smoke
```

**重点观察项**

- `summary::local_repair_count`
- `summary::plan_update_events`
- `summary::queued_bundle_replans`
- 是否出现 `stale_contact_events_ignored`

**适合生成的图**

- timeline：观察 bundle 生命周期变化
- comparison：对比不同故障触发时间或不同负载

### 5.6 Exp5：冗余传输实验

**目标**

- 比较 `NONE`、`SINGLE_BACKUP`、`MULTI_BACKUP`、`TRUNK_ONLY` 等模式
- 观察冗余收益、额外复制成本与失败背景之间的关系

**主要输入**

- `primary_only.json`
- `single_backup.json`
- `multi_backup.json`
- `trunk_only.json`
- `matrix.json`

**推荐命令**

```powershell
.\build\sabr.exe run-scenario --config .\configs\experiments\exp5_redundancy\single_backup.json --output .\results\exp5_smoke
```

**重点观察项**

- `summary::redundancy_trigger_count`
- `summary::redundant_first_hit_count`
- `summary::duplicate_replica_discard_count`
- `summary::redundancy_benefit_cost_ratio`
- `summary::delivery_rate`

**适合生成的图**

- comparison：对比 `primary_only`、`single_backup`、`multi_backup`

### 5.7 Exp6：消融实验

**目标**

- 对 enhancement 与 redundancy 进行组合消融
- 评估 `one_route_per_neighbor`、`queue_delay`、`anti_loop_reactive`、`anti_loop_proactive` 和 `redundancy.mode` 的组合效果

**主要输入**

- `baseline.json`
- `matrix.json`

当前矩阵维度包括：

- `enhancements.one_route_per_neighbor`
- `enhancements.queue_delay`
- `enhancements.anti_loop_reactive`
- `enhancements.anti_loop_proactive`
- `redundancy.mode`
- `simulation.phase1.k_paths`

**推荐命令**

```powershell
.\build\sabr.exe run-experiment --matrix .\configs\experiments\exp6_ablation\matrix.json --output .\results\exp6_smoke --limit 9
```

**重点观察项**

- `summary::delivery_rate`
- `summary::average_delivery_latency`
- `summary::reactive_anti_loop_trigger_count`
- `summary::redundancy_trigger_count`

**适合生成的图**

- comparison：以 `coord::simulation.phase1.k_paths` 为横轴，以 `coord::redundancy.mode` 为系列

---

## 6. 批量运行、导出与绘图

### 6.1 批量运行脚本

脚本：`scripts/run_experiments.py`

两种主要模式：

- `--mode smoke`
- `--mode full`

Smoke 运行：

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\run_experiments.py --mode smoke --sabr .\build\sabr.exe --output-root .\results\script_smoke
```

Full 运行示例：

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\run_experiments.py --mode full --sabr .\build\sabr.exe --experiment exp2_unicast_scale --experiment exp5_redundancy --output-root .\results\full_runs --limit 9
```

### 6.2 结果导出

将结果目录转换为 flat / pivot 汇总：

```powershell
.\build\sabr.exe export --input .\results\script_smoke --output .\results\export_smoke --format both --metric summary::delivery_rate --row-key scenario_name
```

常用参数：

- `--format flat|pivot|both`
- `--metric`
- `--row-key`
- `--column-key`

常用指标字段：

- `summary::delivery_rate`
- `summary::average_delivery_latency`
- `summary::average_hop_count`
- `summary::route_failures`
- `summary::local_repair_count`
- `summary::reactive_anti_loop_trigger_count`
- `summary::redundancy_trigger_count`

常用矩阵坐标字段：

- `coord::traffic[0].payload_size`
- `coord::simulation.phase1.k_paths`
- `coord::simulation.owlt_margin`
- `coord::redundancy.mode`

### 6.3 图表生成

#### 拓扑图

```powershell
.\build\sabr.exe plot --kind topology --input .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\plot_smoke\topology.svg
```

#### 时间轴图

```powershell
.\build\sabr.exe plot --kind timeline --input .\results\script_smoke\exp1_unicast_correctness --output .\results\plot_smoke\timeline.svg
```

#### 利用率图

```powershell
.\build\sabr.exe plot --kind utilization --input .\results\script_smoke\exp1_unicast_correctness --output .\results\plot_smoke\utilization.svg
```

#### 对比图

```powershell
.\build\sabr.exe plot --kind comparison --input .\results\export_smoke\aggregated_summary.csv --output .\results\plot_smoke\comparison.svg --metric summary::delivery_rate --x-key scenario_name --title Smoke Delivery Comparison
```

---

## 7. 结果文件解读

### 7.1 `run_manifest.json`

作用：

- 记录本次运行的场景、输出目录、是否成功
- 记录矩阵坐标
- 记录生成文件位置

适合用于：

- 自动化聚合
- 结果目录追踪
- 复现实验时确认输入来源

### 7.2 `stats.json`

作用：

- 提供机器可读的核心统计契约

主要顶层部分：

- `metadata`
- `summary`
- `bundles`
- `contacts`
- `routing`
- `plan_updates`

### 7.3 `bundle_summary.csv`

适合查看：

- 每个 bundle 的最终状态
- 端到端时延
- hop 数
- route path

### 7.4 `contact_utilization.csv`

适合查看：

- 每个 contact 的容量利用率
- 发送完成次数
- 提交流量与容量关系

### 7.5 `aggregated_summary.csv`

适合查看：

- 每个 run 一行的汇总结果
- 将 manifest 字段、matrix 坐标、metadata、summary 统一扁平化后做比较

### 7.6 `aggregated_pivot.csv`

适合查看：

- 参数扫描结果矩阵化展开
- 做外部表格分析或报告绘图输入

指标详解请配合阅读 [metrics.md](metrics.md)。

---

## 8. 阶段 9 正式验收流程

阶段 9 的目标不是补新功能，而是完成性能验收、正式实验记录和交付收口。

### 8.1 NF-PF-01 性能测试

执行命令：

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\benchmark_nf_pf_01.py --output-root .\results\stage9\performance
```

该脚本会：

- 生成 20 节点 / 200 Contact / 1000 Bundle 的确定性基准场景
- 调用 `sabr run-scenario`
- 输出 `performance_report.json`

NF-PF-01 当前门槛：

- `< 60 秒`

### 8.2 正式代表性实验执行

执行命令：

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\stage9_suite.py --output-root .\results\stage9\final
```

默认覆盖：

- Exp1：`baseline.json`
- Exp2：`matrix.json --limit 9`
- Exp3：`tree_plan.json` + `split_unicast_control.json`
- Exp4：`baseline.json`
- Exp5：`primary_only.json` + `single_backup.json`
- Exp6：`matrix.json --limit 9`

该套件会自动完成：

- NF-PF-01 基准
- 代表性 run 执行
- per-experiment export
- per-experiment plot
- global export
- global overview plot
- 标准复现包打包

### 8.3 关键输出

- `performance/performance_report.json`
- `stage9_execution_plan.json`
- `stage9_execution_report.json`
- `plots/stage9_overview.svg`
- `release_bundle/bundle_manifest.json`

### 8.4 当前最终基线

当前仓库已验证：

- 全量回归：189/189
- NF-PF-01：通过
- 阶段 9 正式套件：通过
- 标准复现包：成功生成

复现与交付细节请配合阅读 [reproducibility.md](reproducibility.md)。

---

## 9. 标准复现包说明

标准复现包位于：

```text
results/stage9/final/release_bundle/
```

当前标准集包含：

- `README.md`
- `docs/`
- `configs/experiments/`
- `scripts/` 中阶段 9 所需脚本
- `samples/performance/`
- `samples/representative/`
- `samples/exports/`
- `samples/plots/`
- `samples/stage9_execution_plan.json`
- `samples/stage9_execution_report.json`

审计清单位于：

```text
results/stage9/final/release_bundle/bundle_manifest.json
```

---

## 10. 常见问题与排错

### 10.1 `CMake Tools` 无法正确配置怎么办

使用终端主路径：

```powershell
cmake -S . -B build
cmake --build build --target sabr sabr_tests
ctest --test-dir build --output-on-failure
```

### 10.2 找不到 Python 脚本解释器怎么办

优先使用：

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe
```

### 10.3 `plot comparison` 图为空或只有单列怎么办

先检查：

- `aggregated_summary.csv` 是否确实包含多条 run
- `--x-key` 是否存在于 CSV 列中
- `--series-key` 是否存在于 CSV 列中
- 使用的 `--metric` 是否是数值字段

### 10.4 `aggregated_pivot.csv` 列为空怎么办

通常说明：

- 输入目录不是矩阵输出
- `--row-key` 或 `--column-key` 选择错误
- 对应 run 没有 `coord::` 字段

### 10.5 阶段 9 复现包里缺文件怎么办

重新执行：

```powershell
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\stage9_suite.py --output-root .\results\stage9\final
```

然后检查：

- `stage9_execution_report.json`
- `release_bundle/bundle_manifest.json`

---

## 11. 推荐执行顺序

如果你是首次接手该仓库，推荐按以下顺序执行：

1. 构建与回归：`cmake --build build --target sabr sabr_tests`，随后 `ctest --test-dir build --output-on-failure`
2. 跑 smoke：`scripts/run_experiments.py --mode smoke`
3. 试一次 export 与 plot，确认结果链闭合
4. 读取 `stats.json`、`aggregated_summary.csv` 和 `comparison.svg`，确认你理解结果结构
5. 根据目标选择单实验、矩阵实验或 Stage 9 正式套件
6. 交付前执行 `scripts/stage9_suite.py --output-root .\results\stage9\final`

---

## 12. 相关文档

- 英文实验说明： [experiments.md](experiments.md)
- 指标说明： [metrics.md](metrics.md)
- 复现与交付说明： [reproducibility.md](reproducibility.md)

如果需要对某一类实验单独扩写，可以在本手册基础上继续拆成：

- 单播实验手册
- 多播实验手册
- 冗余实验手册
- 阶段 9 验收手册