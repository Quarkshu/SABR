# SABR 实验报告复用模板

本模板用于统一 Exp1 到 Exp6 的正式实验报告结构，适用于单播、多播、冗余、局部修复与对照类实验。当前 Exp1 到 Exp5 的正式报告已经按这一骨架收敛，后续 Exp6 可直接复用。

使用原则：

1. 报告必须面向独立归档和直接引用，默认读者不会同时打开代码或旧报告。
2. 每个结论都应优先落在 execution_report、aggregated_summary、aggregated_pivot、stats.json、bundle_summary.csv 或配置文件上，避免空泛表述。
3. 如果当前正式基线经历过修正，必须在“版本说明”中明确旧结果为什么不能继续作为正式基线。
4. 如果实验口径不能直接使用 summary::delivery_rate，必须先定义主指标再分析，例如接收者完成率、冗余收益成本比等。

## 1. 模板正文

将下面内容复制到目标结果目录中的正式报告文件，再按实验实际内容填写。

```md
# ExpX <实验名称>实验报告

> 本报告对应当前正式结果目录 results/<expX_complete_timestamp>，面向独立归档和直接引用。它将 ExpX 的实验目标、设计依据、执行完整性、结果分析和正式结论整理为一份完整报告。

## 1. 实验目标与版本说明

### 1.1 实验目标

ExpX 的核心目标是：

1. <目标 1>
2. <目标 2>
3. <目标 3>

### 1.2 当前正式基线

ExpX 当前正式基线为 results/<expX_complete_timestamp>。

如存在旧版本结果，需要明确写出：

1. <旧目录 A>：<不能继续作为正式基线的原因>
2. <旧目录 B>：<保留价值或修正背景>
3. <当前目录>：<为什么成为当前正式基线>

如果没有旧版本沿革，则直接说明当前目录由标准 complete workflow 生成，并包含 baseline / scenarios、matrix、execution_report、exports、plots 等关键产物。

## 2. 实验设计与分析依据

### 2.1 主要输入

ExpX 使用以下核心输入：

- <baseline 或 primary scenario 配置>
- <control / alternate scenario 配置>
- matrix.json
- <contact_plan.ion / replacement_plan.ion / 其他关键输入>

### 2.2 场景或拓扑语义

这里写清：

1. 拓扑结构、关键链路、失效背景或对照关系。
2. baseline / scenario / control 的语义差异。
3. 哪些机制是本实验真正要验证的，例如冗余、副本首达、共享 trunk 复用、局部修复、反环路等。

如果实验里有一个特别重要的主场景，也可在本节直接列出核心流量参数，例如：

- source_node = <值>
- destination_node = <值>
- start_time = <值>
- period = <值>
- bundle_count = <值>
- payload_size = <值>
- ttl = <值>

### 2.3 矩阵定义

ExpX matrix 共 <N> 个组合，扫描以下维度：

- <维度 1>
- <维度 2>
- <维度 3>
- <维度 4，可选>

### 2.4 重点观察指标

本实验最关键的判断指标是：

- <metric 1>
- <metric 2>
- <metric 3>

分别用于说明：

1. <指标与实验问题的对应关系>
2. <为什么不能只看 delivery_rate>
3. <为什么这些指标足够支持正式结论>

## 3. 执行完整性

### 3.1 当前结果链条

根据 execution_report.json 或现有结果目录，可确认：

- 正式结果目录：results/<expX_complete_timestamp>
- baseline 或 scenarios：<成功数>/<总数>
- matrix：<成功数>/<总数>
- export：<成功数>/<总数>
- plot：<成功数>/<总数>
- failed_runs：<数值>

结果目录中包含：

- <baseline / scenarios>
- matrix
- exports/<...>
- plots/<...>

因此，当前结果链路<完整 / 基本完整但缺少 execution_report，需要依赖归档内容判定>。

### 3.2 收敛性或一致性状态（可选）

如果实验需要讨论 ACTIVE、矩阵公平性、聚合口径或 determinism，则在本节补充：

1. 是否所有 bundle 都已进入终态。
2. 不同 control / tree / redundancy scenario 是否承受对齐的 offered load。
3. failure_seed 或其他维度为何相同或不同是合理的。

### 3.3 历史异常说明（可选）

如果当前正式基线来自一次修正后重跑，需要简要记录：

1. 旧结果的问题是什么。
2. 问题属于实现缺陷、配置语义错误还是聚合口径错误。
3. 当前正式基线为何不再受该问题影响。

## 4. 单场景结果分析

如果实验是多单场景对比，可用“单场景结果分析”；如果实验只有 baseline，也可改成“baseline 结果分析”。

### 4.1 汇总表

| 场景或指标 | <指标 1> | <指标 2> | <指标 3> |
|---|---:|---:|---:|
| <baseline / scenario A> | <值> | <值> | <值> |
| <scenario B> | <值> | <值> | <值> |

### 4.2 关键场景 A

这里写：

1. 最能代表实验设计目标的场景结果。
2. 路径、交付、失败或 repair 行为如何支持结论。
3. 与仓库测试基线、配置语义或拓扑直觉是否一致。

### 4.3 关键场景 B / control（可选）

如果实验存在 control、alternate scenario 或修复前后对比，则继续写：

1. 它与主场景相比好在哪里或差在哪里。
2. 差异是由 offered load、路径共享、副本数量、故障时刻等哪个因素造成的。

### 4.4 单场景小结

用 2 到 4 句收束：

1. 单场景已经证明了什么。
2. 单场景还不能证明什么，需要矩阵继续补充什么。

## 5. 矩阵结果分析

### 5.1 矩阵总体特征

先用一段话说明：

1. 当前矩阵主要用于观察什么趋势。
2. 哪些维度是主导维度，哪些只是次级维度。
3. 是否需要按 scenario_file、control group、receiver completion 等二级口径分层分析。

### 5.2 按维度 A 聚合

| 维度 A | <主指标> | <辅助指标 1> | <辅助指标 2> |
|---|---:|---:|---:|
| <值 1> | <值> | <值> | <值> |
| <值 2> | <值> | <值> | <值> |

解释：

1. <趋势 1>
2. <趋势 2>
3. <为什么这与拓扑 / 配置 / 机制一致>

### 5.3 按维度 B 聚合

| 维度 B | <主指标> | <辅助指标 1> | <辅助指标 2> |
|---|---:|---:|---:|
| <值 1> | <值> | <值> | <值> |
| <值 2> | <值> | <值> | <值> |

解释：

1. <趋势 1>
2. <趋势 2>
3. <是否存在平台期、截断、非线性收益或边界退化>

### 5.4 按维度 C 聚合或代表性坐标（可选）

如果还有第三个核心维度，继续按聚合表展开；如果更适合举代表性坐标，则列出若干 run：

| 组合 | <主指标> | <辅助指标 1> | <辅助指标 2> |
|---|---:|---:|---:|
| <组合 1> | <值> | <值> | <值> |
| <组合 2> | <值> | <值> | <值> |

这里重点写“哪些组合最能说明结论”，不要把 run 清单堆成流水账。

## 6. 合理性判断

综合 execution_report、aggregated_summary、aggregated_pivot、bundle_summary、关键路径证据和配置语义，可以给出如下判断：

1. <结果是否完整可信>
2. <单场景与矩阵是否相互印证>
3. <异常是否能被时间窗、负载、拓扑、修复机制或聚合口径直接解释>
4. <当前结果为什么可以或不可以作为正式基线>

结论：当前 ExpX 结果<整体合理 / 需修正后再作为正式基线>。

## 7. 正式结论

基于 results/<expX_complete_timestamp>，可以给出以下正式结论：

1. <结论 1>
2. <结论 2>
3. <结论 3>
4. <如果有必要，给出版本沿革或限制条件>

## 8. 建议引用文件

- 正式报告：results/<expX_complete_timestamp>/<report_file>.md
- 执行回执：results/<expX_complete_timestamp>/execution_report.json
- 单场景或 baseline 汇总：results/<expX_complete_timestamp>/exports/<...>/aggregated_summary.csv
- 矩阵汇总：results/<expX_complete_timestamp>/exports/<...>/aggregated_summary.csv
- 矩阵透视：results/<expX_complete_timestamp>/exports/<...>/aggregated_pivot.csv
- 场景图：results/<expX_complete_timestamp>/plots/<scenario_or_baseline_plot>.svg
- 矩阵图：results/<expX_complete_timestamp>/plots/<matrix_plot>.svg
```

## 2. 填写说明

### 2.1 一级章节固定，二级章节按实验裁剪

一级章节统一固定为：

1. 实验目标与版本说明
2. 实验设计与分析依据
3. 执行完整性
4. 单场景结果分析 或 baseline 结果分析
5. 矩阵结果分析
6. 合理性判断
7. 正式结论
8. 建议引用文件

允许裁剪的部分：

- 3.2 和 3.3 可按需要省略
- 4.x 可按 baseline / scenarios / control 的具体结构改名
- 5.x 的维度数量不固定，但建议至少保留“总体特征 + 两个核心维度”

### 2.2 常见实验类型如何套模板

单播正确性类，例如 Exp1：

- 第 4 节重点写 reference baseline、窗口约束和路由失败的可解释性
- 第 5 节重点写 payload、bundle_count、k_paths 的影响与窗口截断

单播规模类，例如 Exp2：

- 第 4 节重点写健康 baseline
- 第 5 节重点写吞吐平台、payload 退化、k_paths 收益平台、owlt_margin 的次级作用

多播对照类，例如 Exp3：

- 第 2.4 节先定义接收者完成率
- 第 4 节写 tree 与 control 的单场景差异
- 第 5 节写 offered load 对齐后再做矩阵比较

局部修复类，例如 Exp4：

- 第 2.4 节先定义接收者完成率
- 第 4 节写 baseline repair 成功闭环
- 第 5 节写 trigger_time、bundle_count 与 repair 开销

冗余类，例如 Exp5：

- 第 3.2 节补收敛性状态
- 第 5 节优先按 scenario_file 分层，再比较 redundancy mode 和 failure probability

Exp6 反环路或路由策略类：

- 第 2 节先把对照机制、失败背景和主指标定义清楚
- 第 4 节确认 baseline / control 的可解释差异
- 第 5 节按最能体现策略收益的两个或三个维度展开，不必机械复刻 Exp5 的所有小节

### 2.3 数据来源对照表

| 报告内容 | 优先数据来源 |
|---|---|
| 实验目标与输入 | configs/experiments/<exp>/ 下的 baseline、scenario、matrix、contact plan |
| 正式基线与执行完整性 | execution_report.json、execution_plan.json、结果目录结构 |
| 单场景指标 | exports/scenarios 或 exports/baseline 的 aggregated_summary.csv；必要时回看 stats.json / bundle_summary.csv |
| 矩阵聚合 | exports/matrix/aggregated_summary.csv、aggregated_pivot.csv |
| 图表引用 | plots/ 下的 comparison / timeline / utilization / topology SVG |
| 历史异常说明 | 修正前结果目录、相关代码修复、回归测试或 rerun 结论 |

### 2.4 落地规则

1. 标题使用“ExpX + 实验名称 + 实验报告”的格式。
2. 导语首段必须写明当前正式结果目录。
3. 第 6 节必须显式回答“结果是否合理，能否作为正式基线”。
4. 第 8 节必须给出可直接引用的文件清单，优先列 report、execution_report、aggregated_summary、aggregated_pivot、matrix comparison 图。
5. 如果某项产物不存在，例如 Exp1 缺 execution_report.json，必须如实写出，不要假装目录齐全。

## 3. 推荐文件命名

为避免后续 Exp6 再出现命名分裂，建议新报告优先统一为：

- exp1_experiment_report.md
- exp2_experiment_report.md
- exp3_experiment_report.md
- exp4_experiment_report.md
- exp5_experiment_report.md
- exp6_experiment_report.md

如果历史归档已经使用了其他文件名，可以先保留原文件名，但正文结构应继续使用本模板。