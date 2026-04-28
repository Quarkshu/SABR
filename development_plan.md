# 空间DTN网络 CGR/SABR 仿真系统 — 项目开发计划

| 项目名称 | sabr |
|---|---|
| 版本 | v1.2 |
| 日期 | 2026-04-27 |
| 依据 | requirements_specification.md v1.0, system_design.md v1.0, 需求说明与架构设计文档.md v1.0, 实验方案v0.1.txt |
| 当前进度 | 阶段 0 完成，阶段 1 完成并已按新需求完成模型层契约对齐，阶段 2A 完成，阶段 2B 完成，阶段 2C 完成，阶段 2D 完成，阶段 3A 完成，阶段 3B 完成，阶段 3C 完成，阶段 4A 完成，阶段 4B 完成，阶段 5 完成，阶段 6A 完成，阶段 6B 完成，阶段 7A 完成，阶段 7B 完成，阶段 8A 完成，阶段 8B 完成，阶段 9 完成；当前基线已补齐 NF-PF-01 性能基准、Stage 9 代表性实验执行套件、标准复现包与交付文档，全量回归 189/189 通过 |

---

## 目录

1. [开发策略](#1-开发策略)
2. [阶段划分与里程碑](#2-阶段划分与里程碑)
3. [详细任务分解](#3-详细任务分解)
4. [阶段依赖与并行关系](#4-阶段依赖与并行关系)
5. [验收标准](#5-验收标准)
6. [风险与应对](#6-风险与应对)

---

## 1. 开发策略

### 1.1 总体原则

- 自底向上，但先固定契约：先锁定数据模型与缓存语义，再实现算法、仿真与 IO
- 测试驱动：每个阶段都必须伴随对应的单元测试或集成测试，不接受只写实现不写验证
- 单播优先，但提前为多播留接口：多播在阶段 6 才进入功能实现，但阶段 1 和阶段 2 的接口必须提前支持传播计划、版本校验与副本血缘信息
- 规划与提交分离：Phase 1、Phase 2、Phase 3 均为无副作用规划流程，容量扣减只能在 commitRoute 或等价提交接口发生
- 实验可执行性作为硬约束：剩余阶段不仅要“实现功能”，还必须让实验一到实验六都能通过配置、命令和脚本直接运行
- 冗余传输独立于 Critical Bundle 泛洪：Critical Bundle 继续沿用标准受控泛洪；风险感知备份路径选择作为单独模块实现与验证
- 文档与代码同步：development_plan.md 和 devlog.md 必须和当前代码基线一致，避免“代码已变、计划未变”

### 1.2 当前架构基线

当前代码库已完成以下基础能力：

- Contact 支持 contact_id、plan_version 和分层 MTV
- ContactPlan 使用单调递增的 plan_version 作为计划变更依据
- Node 路由缓存已与 plan_version 绑定
- Bundle 已支持 visited_nodes、多播计划、副本血缘和目的集合状态
- Route 已切换到逐段 RouteLeg 结构，包含逐段时序、EVL 占位字段，并支持零跳本地交付表示
- Dijkstra 已支持禁用边、禁用顶点和首跳邻居约束，可复用于 YenKSP 与 One-Route-Per-Neighbor
- Phase1 与 YenKSP 已实现，包含版本缓存复用、额外重算入口和 One-Route-Per-Neighbor
- Phase2 已实现，包含 CandidateRoute 结果模型、完整 ETO / PBAT / RVL 计算、7 条排除规则、Queue-Delay 与主动式 Anti-Loop
- Phase3、RoutingDecision 与 CGRRouter 已实现，具备单播选择、Critical 受控泛洪、补算循环和显式 commitRoute
- 仿真层已实现 TrafficGenerator、SimEngine、Contact 边界事件注册、Bundle 生命周期单播事件流，以及基于优先级发送队列的逐跳转发基线
- 当前代码库已具备 3B 以前的单元与小规模集成基线；后续缺口集中在故障注入、IO、多播执行、冗余传输、统计导出和实验编排

后续所有阶段必须以这组接口为基线推进，不再回退到旧版 modified 标志、旧版 Route.hops 或“Phase 3 内直接扣减 MTV”的设计。

### 1.3 关键技术约束

- C++17 实现，核心算法层只依赖标准库
- 路由缓存失效必须通过 plan_version 判断，不再依赖布尔 modified 标志
- Dijkstra 的最短路径代价采用动态松弛，不使用静态边权近似
- 多播树构造必须调用无副作用单播规划接口
- 多播计划编码必须能标识 contact_id，以支持中间节点校验与局部修复
- 故障注入、动态 ContactPlan 更新和实验随机性必须支持固定随机种子，以保证实验可复现
- 冗余传输必须提供独立模式开关、去重规则和统计项，不能用 Critical Bundle 泛洪替代
- 实验运行结果必须同时支持机器可读导出（JSON / CSV）和面向报告的图表生成

### 1.4 实验对齐目标

本版计划的完成标准不再只是“功能实现完毕”，而是满足以下交付条件：

- 实验一到实验六都具备可直接运行的配置文件、命令行入口和结果输出目录规范
- 单播、多播、局部修复、冗余传输四类行为都能在日志和统计层被追踪与量化
- 所有关键扰动变量都可配置：网络规模、流量负载、TTL、Contact 容量、Contact 失效、动态计划更新、机制开关和冗余模式
- 无需手工改代码即可复现实验方案中的对比组，仅通过配置与脚本完成切换

---

## 2. 阶段划分与里程碑

| 阶段 | 名称 | 交付物 | 里程碑标志 | 状态 |
|---|---|---|---|---|
| 0 | 项目脚手架 | CMake、目录结构、GTest 集成 | cmake 配置和空测试通过 | 已完成 |
| 1 | 数据模型层与契约对齐 | 版本化数据模型、版本绑定缓存、逐段 Route、模型测试 | 模型层全部测试通过 | 已完成 |
| 2A | 接触图与动态 Dijkstra | ContactGraph、动态松弛 Dijkstra、标准示例测试 | REF-1 示例拓扑最短路径正确 | 已完成 |
| 2B | Phase 1 与 Yen KSP | Phase1、YenKSP、缓存复用测试 | K 条路由结果正确且缓存受版本控制 | 已完成 |
| 2C | Phase 2 路由验证 | ETO、PBAT、EVL、RVL、7 条排除规则 | 全部排除规则独立测试通过 | 已完成 |
| 2D | Phase 3、提交接口与 CGR 编排 | Phase3、commitRoute、CGR Router | 选择逻辑正确且提交前无容量副作用 | 已完成 |
| 3A | 事件系统 | Event、EventScheduler、运行时上下文 | 事件严格按时间顺序调度 | 已完成 |
| 3B | 仿真引擎与流量生成 | SimEngine、TrafficGenerator、BPA 生命周期处理 | 小规模单播仿真可运行 | 已完成 |
| 3C | 故障注入与动态计划更新 | FailureInjector、动态 ContactPlan 更新、可复现扰动机制 | Contact 失效和计划变化可被稳定重放 | 已完成 |
| 4A | 解析器与实验配置 | ION/JSON 解析、场景配置、实验参数矩阵 | 实验方案所需输入文件可正确解析 | 已完成 |
| 4B | 日志、统计与结果导出 | Logger、SimStats、JSON/CSV 导出 | 路由 trace、计划更新、Bundle/Contact 统计和 JSON/CSV 导出可用 | 已完成 |
| 5 | 单播集成与标准符合性 | 五节点 reference 场景测试、PICS 对照、exp1/exp2 smoke case | 五节点 reference 场景、下行首跳分流、PICS-style 覆盖映射和 smoke case 可用 | 已完成 |
| 6A | 多播传播计划构造 | MulticastGroup、无副作用单播规划接口、传播计划合并 | 多播构树正确且不修改 MTV | 已完成 |
| 6B | 多播执行与局部修复 | Multicast 执行展开、局部重路由、共享前缀容量控制 | VT-IT-06、VT-IT-07、VT-IT-08 和 VT-UT-08 通过 | 已完成 |
| 7A | 增强功能收口 | One-Route-Per-Neighbor、Queue-Delay、Anti-Loop、统一开关 | 各增强独立启停且组合行为可验证 | 已完成 |
| 7B | 冗余传输与去重 | RedundancyManager、备份路径选择、首达去重、冗余统计 | 实验五和实验六中的冗余组可直接运行 | 已完成 |
| 8A | 实验编排与批量运行 | 实验配置模板、批量运行器、聚合脚本 | 实验一到实验六均有可执行配置与命令 | 已完成 |
| 8B | 可视化、CLI 与实验交付 | 绘图脚本、CLI、实验运行手册 | 可生成图表并按文档复现实验 | 已完成 |
| 9 | 系统测试与最终交付 | 回归测试、性能测试、实验复现包 | 全量测试通过，实验方案 100% 可执行 | 已完成 |

---

## 3. 详细任务分解

### 阶段 0：项目脚手架搭建

已完成。保持现状，不再扩展阶段 0 内容。

### 阶段 1：数据模型层与契约对齐

已完成，当前基线应视为后续开发的正式起点。

| 编号 | 任务 | 产出文件 | 状态 |
|---|---|---|---|
| 1.1 | Contact 增加 contact_id、plan_version，保留 MTV 分层容量语义 | contact.hpp/.cpp | 已完成 |
| 1.2 | ContactPlan 改为 plan_version 驱动，所有修改路径统一递增版本 | contact_plan.hpp/.cpp | 已完成 |
| 1.3 | Node 路由缓存与 plan_version 绑定 | node.hpp/.cpp | 已完成 |
| 1.4 | Bundle 增加多播计划、副本血缘、pending/delivered destinations | bundle.hpp/.cpp | 已完成 |
| 1.5 | Route 切换为 RouteLeg 逐段结构 | route.hpp/.cpp | 已完成 |
| 1.6 | 更新模型层测试覆盖新增语义 | tests/test_models.cpp | 已完成 |

### 阶段 2A：接触图构造与动态 Dijkstra

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 2A.1 | 实现 ContactGraph，仅表达 Contact 间可连接关系，不预先固化静态边权 | contact_graph.hpp/.cpp | FR-CG-01 ~ FR-CG-03 |
| 2A.2 | 实现动态 Dijkstra：松弛时根据前驱到达时刻计算当前 Contact 的 earliest_transmission_time 和 earliest_arrival_time | dijkstra.hpp/.cpp | FR-P1-01 ~ FR-P1-04 |
| 2A.3 | 在 RouteLeg 中回填逐段 earliest_transmission_time / earliest_arrival_time | dijkstra.cpp | DS-RTE-07 |
| 2A.4 | 用 REF-1 图 3-1 至图 3-4 编写单元测试，验证接触图与最短路径正确性 | tests/test_dijkstra.cpp | VT-UT-03, VT-SC-01 |

### 阶段 2B：Phase 1 与 Yen KSP

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 2B.1 | 实现 YenKSP，返回按 EAT 排序的多条候选路由 | yen_ksp.hpp/.cpp | FR-P1-06 |
| 2B.2 | 实现 Phase1，写入 Node 的版本绑定路由缓存 | phase1.hpp/.cpp | FR-P1-05, FR-P1-07 |
| 2B.3 | 当缓存版本失效或 Phase 2 请求更多路由时，重新进入 Phase 1 | phase1.cpp | FR-P1-08, DS-CP-04 |
| 2B.4 | 编写缓存复用与版本失效测试 | tests/test_phase1.cpp | VT-UT-03 |

### 阶段 2C：Phase 2 路由验证

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 2C.1 | 实现排除节点构造：previous_node 与 excluded_neighbors 合并 | phase2.hpp/.cpp | FR-P2-02 |
| 2C.2 | 实现完整 ETO 计算 | phase2.hpp/.cpp | FR-P2-03 |
| 2C.3 | 实现完整 PBAT 计算，并逐段回填 RouteLeg 时序信息 | phase2.hpp/.cpp | FR-P2-04 |
| 2C.4 | 实现 MTV、EVL、RVL 计算，逐段写入 RouteLeg.evl | phase2.hpp/.cpp | FR-P2-05, DS-RTE-08 |
| 2C.5 | 实现 7 条排除规则，按独立函数组织 | phase2.hpp/.cpp | FR-P2-06 |
| 2C.6 | 实现 Phase2::validate，输出 CandidateRoute 列表但不修改容量 | phase2.hpp/.cpp | FR-P2-01, FR-P2-07, FR-P2-08 |
| 2C.7 | 编写 ETO/PBAT/RVL 和 7 条排除规则测试 | tests/test_phase2.cpp | VT-UT-02, VT-UT-04 |

### 阶段 2D：Phase 3、提交接口与 CGR 编排

已完成。当前基线已具备 `Phase3Selector`、`CGRRouter`、`RoutingDecision` 和显式 `commit_route()` 接口；后续阶段应继续建立在“route_unicast 无容量副作用、commit_route 单独提交”的边界之上。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 2D.1 | 实现四级比较器：PBAT → hop_count → termination_time → entry_node | phase3.hpp/.cpp | FR-P3-02 |
| 2D.2 | 实现标准 Bundle 单一路由选择和 Critical Bundle 的受控泛洪选择 | phase3.hpp/.cpp | FR-P3-01, FR-P3-03, FR-P3-04 |
| 2D.3 | 实现 commitRoute：只在最终发送决策确定后扣减容量 | cgr.hpp/.cpp | FR-P3-05 |
| 2D.4 | 实现 CGR 编排器：修剪、检查、Phase 1、Phase 2、Phase 3、提交 | cgr.hpp/.cpp | FR-CPC-01, FR-RP-01, FR-RP-02, FR-RR-01 |
| 2D.5 | 编写“规划与提交分离”测试，验证 Phase 1/2/3 本身不修改 MTV | tests/test_cgr.cpp | VT-UT-07 |

### 阶段 3A：事件系统

已完成。当前基线已具备基础事件模型、确定性调度器，以及面向后续 3B 引擎接入的最小运行时装配能力。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 3A.1 | 实现 EventType、Event、EventId 等基础事件数据结构 | simulation/event.hpp/.cpp | FR-SIM-01, FR-SIM-02 |
| 3A.2 | 实现 EventScheduler，支持稳定的时间优先与同时间戳确定性顺序 | simulation/scheduler.hpp/.cpp | FR-SIM-01, FR-SIM-03 |
| 3A.3 | 设计 RuntimeContext / NodeRuntime / BPA 最小运行时对象，统一持有 ContactPlan、Nodes、Logger、Stats 钩子 | simulation/runtime_context.hpp/.cpp | NF-EX-02, FR-BPA-01 |
| 3A.4 | 编写事件调度、事件稳定排序和最小运行时装配测试 | tests/test_simulation.cpp | FR-SIM-01 |

### 阶段 3B：仿真引擎与流量生成

已完成。当前基线已具备最小单播仿真闭环：TrafficGenerator 可生成事件流，SimEngine 可调度 Contact 与 Bundle 生命周期事件，并已通过 3 节点 / 3 Contact / 5 Bundle 场景回归。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 3B.1 | 实现 TrafficGenerator，支持单次、批量、周期三种生成模式 | simulation/traffic.hpp/.cpp | FR-TG-01 ~ FR-TG-03 |
| 3B.2 | 实现 SimEngine 初始化与 Contact start/end 事件注册 | simulation/engine.hpp/.cpp | FR-SIM-02, FR-SIM-04 |
| 3B.3 | 实现 Bundle created / arrived / forwarded / delivered / expired 处理流程，并接入 `route_unicast()` 与 `commitRoute()` | simulation/engine.hpp/.cpp | FR-SIM-02, FR-BPA-01 ~ FR-BPA-03, FR-RR-01 |
| 3B.4 | 实现节点发送队列、传输延迟、传播延迟与逐跳重计算计数 | simulation/engine.cpp | FR-BPA-02, FR-BPA-03, FR-RR-01 |
| 3B.5 | 编写 3 节点 / 3 Contact / 5 Bundle 小场景单播仿真测试 | tests/test_simulation.cpp | FR-BPA-01 |

### 阶段 3C：故障注入与动态计划更新

已完成。当前基线已具备确定性 `FailureInjector`、统一 `PLAN_UPDATED` 事件、运行时 Contact 移除 / 整计划替换、`plan_version` 传播、排队 Bundle 重路由，以及固定随机种子重放能力；同时引擎运行时提交粒度已收窄到“仅提交本次实际发送的首跳 Contact”，避免逐跳重算与失效重路由场景下的下游容量预扣失真。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 3C.1 | 新增 FailureInjector，支持按时间、按概率和按指定 Contact 集注入失效 | simulation/failure_injector.hpp/.cpp | 实验方案 4.3(5) |
| 3C.2 | 实现运行时 ContactPlan 动态更新与 `plan_version` 传播，保证路由缓存正确失效 | simulation/engine.cpp, models/contact_plan.cpp | FR-RP-01, DS-CP-04 |
| 3C.3 | 支持关键干线 Contact 人为移除、动态计划替换和固定随机种子重放 | simulation/failure_injector.cpp | 实验方案 6.4, 6.5 |
| 3C.4 | 编写故障注入与动态计划更新测试，验证逐跳重算和局部修复前置条件 | tests/test_failure_injection.cpp | FR-RR-01, FR-RP-01 |

### 阶段 4A：解析器与实验配置

已完成。当前基线已具备 ION/JSON Contact Plan 解析、JSON 场景与实验矩阵解析、相对路径配置解析、实验一到实验六模板文件，以及模板级解析回归测试。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 4A.1 | 解析 ION 格式 Contact Plan，并分配 `contact_id` | io/parser.hpp/.cpp | IO-IN-01 ~ IO-IN-03 |
| 4A.2 | 解析 JSON 场景配置：仿真参数、流量、多播组、故障注入、冗余模式和实验矩阵 | io/parser.hpp/.cpp | IO-IN-01, IO-IN-04, IO-IN-05 |
| 4A.3 | 保证导入 ContactPlan 时正确初始化 MTV、`plan_version` 和实验随机种子 | io/parser.cpp | DS-CON-03, DS-CP-03 |
| 4A.4 | 为实验一到实验六定义标准配置模板与参数矩阵 | configs/experiments/*.json | 实验方案 4.2, 4.3, 6.1 ~ 6.6 |
| 4A.5 | 编写解析正确性、配置校验和错误处理测试 | tests/test_io.cpp | VT-UT-01 |

### 阶段 4B：日志、统计与结果导出

已完成。当前基线已具备结构化事件 / 路由 / commit / 计划更新日志、Bundle / Contact / Routing 聚合统计，以及 JSON / CSV 结果导出；同时为后续 7B 冗余实验预留了相关统计字段，但完整冗余行为仍以后续阶段实现为准。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 4B.1 | 实现 Logger，输出 Phase 1 / 2 / 3 trace、commit 和计划更新日志，并保留局部修复 / 冗余决策标记位 | io/logger.hpp/.cpp | IO-OUT-01 |
| 4B.2 | 实现 SimStats，记录 Bundle 交付、失败、跳数、端到端时延与 Contact 利用率 | simulation/stats.hpp/.cpp | IO-OUT-02, IO-OUT-03 |
| 4B.3 | 扩展统计骨架：逐跳重算次数、局部修复次数、计划更新影响、重复副本丢弃计数，并为冗余触发 / 首达收益指标预留字段 | simulation/stats.hpp/.cpp | 实验方案 6.1 ~ 6.6 |
| 4B.4 | 实现 JSON / CSV 导出与实验结果聚合格式 | simulation/stats.hpp/.cpp | IO-OUT-04 |
| 4B.5 | 将 Logger / Stats 接入 SimEngine，并编写统计正确性测试 | simulation/engine.cpp, tests/test_stats.cpp | IO-OUT-01 ~ IO-OUT-04 |

### 阶段 5：单播集成与标准符合性

已完成。当前基线复用 `exp1_unicast_correctness` 的五节点拓扑作为 REF-2 等价参考场景，在不新增独立 fixture 目录的前提下补齐了五节点端到端回归、下行首跳分流验证、PICS-style 覆盖映射，以及实验一 / 实验二的最小可执行 smoke case。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 5.1 | 复用 `exp1_unicast_correctness/contact_plan.ion` 作为五节点参考拓扑，并新增阶段 5 专用 reference / downlink 场景配置 | configs/experiments/exp1_unicast_correctness/contact_plan.ion, stage5_reference_baseline.json, stage5_downlink_split.json | VT-IT-01 |
| 5.2 | 实现端到端单播集成测试框架 | tests/test_reference_scenario.cpp | VT-IT-01 |
| 5.3 | 下行链路测试 SA → MCC，验证逐跳重算、路由日志与统计行为 | tests/test_reference_scenario.cpp | VT-IT-02, FR-RR-01 |
| 5.4 | 对照 REF-1 标准示例进行符合性检查 | tests/test_reference_scenario.cpp | VT-SC-01, VT-SC-02 |
| 5.5 | 为实验一和实验二建立最小可执行 smoke case，并直接复用现有 exp1 / exp2 模板 | tests/test_experiment_configs.cpp, configs/experiments/exp1_unicast_correctness/*.json, configs/experiments/exp2_unicast_scale/*.json | 实验方案 6.1, 6.2 |

### 阶段 6A：多播传播计划构造

已完成。当前基线在不修改 `SimEngine` 多播执行路径的前提下，补齐了 `MulticastGroup` registry、无 flood / 无 commit 的最佳单播复用接口、共享前缀传播树构造，以及实验三 `tree_plan` / `split_unicast_control` 配置的 smoke case；构树阶段已通过单元测试证明不会修改 Contact MTV。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 6A.1 | 实现 MulticastGroup 管理 | algorithms/multicast.hpp/.cpp | FR-MC-01, FR-MC-02 |
| 6A.2 | 提供无副作用单播规划接口，供多播构树复用 | algorithms/cgr.hpp/.cpp | FR-MT-01 |
| 6A.3 | 合并单播最优路由为传播计划，分支记录 `contact_id`、下一跳和目的子集 | algorithms/multicast.hpp/.cpp | FR-MT-02, FR-MT-03, FR-MF-02 |
| 6A.4 | 编写构树测试，并验证构树阶段不会修改 MTV | tests/test_multicast.cpp | VT-UT-06, VT-UT-07 |
| 6A.5 | 加入“多播拆为多个独立单播”对照基线，用于实验三资源消耗比较 | tests/test_multicast.cpp, configs/experiments/exp3_multicast_plan/tree_plan.json, configs/experiments/exp3_multicast_plan/split_unicast_control.json, configs/experiments/exp3_multicast_plan/matrix.json | 实验方案 6.3 |

### 阶段 6B：多播执行与局部修复

已完成。当前基线已把 `MulticastPlanner` 产出的传播计划真正接入 `SimEngine`：源节点可按第一层分支初始化多播副本，中间节点可按局部子树继续分叉转发，计划更新后会对失效分支负责的目的子集执行局部 repair，并在 plan_version 变化后重新调度未受影响但仍排队的健康分支；同时已用自动化测试锁定共享前缀只按实际副本数提交一次容量，以及“当前节点既本地交付又继续转发”的非终态多播语义。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 6B.1 | 实现源节点初始多播复制与子树裁剪 | algorithms/multicast.hpp/.cpp, simulation/engine.cpp | FR-MF-01, FR-MF-03, FR-MF-04 |
| 6B.2 | 实现中间节点按传播计划执行分叉或直接转发 | algorithms/multicast.hpp/.cpp, simulation/engine.cpp | FR-MF-05 |
| 6B.3 | 校验计划中的 `contact_id` 是否仍可用，失效时仅对受影响目的节点子集局部重路由 | algorithms/multicast.hpp/.cpp, simulation/engine.cpp | FR-MR-01, FR-MR-02 |
| 6B.4 | 实现共享前缀容量只按实际副本数扣减一次的提交逻辑 | algorithms/multicast.cpp, simulation/engine.cpp, simulation/stats.cpp | FR-MF-07 |
| 6B.5 | 实现副本血缘追踪、局部修复日志与子树更新编码 | algorithms/multicast.cpp, simulation/engine.cpp, simulation/stats.cpp | DS-BDL-05 |
| 6B.6 | 编写多播端到端、局部修复和副本追踪测试 | tests/test_multicast.cpp | VT-UT-08, VT-IT-06, VT-IT-07, VT-IT-08 |
| 6B.7 | 为实验三和实验四建立可执行场景与对比模板 | configs/experiments/exp3_multicast_plan/*.json, configs/experiments/exp4_multicast_repair/*.json | 实验方案 6.3, 6.4 |

### 阶段 7A：增强功能收口

已完成。当前基线已把 One-Route-Per-Neighbor、Queue-Delay、proactive Anti-Loop 和 reactive Anti-Loop 的配置链统一接到仿真运行时：`anti_loop_reactive` 已从 parser 映射进 `Phase2::Config`，`SimEngine` 会在单播提交前执行 reactive Anti-Loop 排除与重选，并将 reroute 原因、排除邻居和触发次数写入 routing trace / logger / stats；同时，`ValidationContext.allocated_bytes` 已在仿真运行时接通，Queue-Delay 不再只停留在 Phase2 单测。集成层现已补齐 VT-IT-03 / VT-IT-04 / VT-IT-05，`exp6_ablation` 也已加入 `anti_loop_reactive` 维度；完整命令行子命令体系仍保持在阶段 8B 收口。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 7A.1 | 将 One-Route-Per-Neighbor 接入仿真配置与 CLI | algorithms/phase1.cpp, io/parser.hpp/.cpp | FR-ENH-01 |
| 7A.2 | 将 Queue-Delay 接入仿真配置与 CLI | algorithms/phase2.cpp, io/parser.hpp/.cpp | FR-ENH-02 |
| 7A.3 | 实现反应式 Anti-Loop，并把 reroute 原因接入运行时日志与统计 | algorithms/cgr.cpp, models/bundle.hpp, simulation/engine.cpp | FR-ENH-03 |
| 7A.4 | 校准主动式 Anti-Loop 与 Phase3 的 non-looping 偏好在仿真中的组合行为 | algorithms/phase2.cpp, algorithms/phase3.cpp | FR-ENH-04 |
| 7A.5 | 实现增强功能总开关与组合测试 | algorithms/cgr.hpp, tests/test_reference_scenario.cpp | FR-ENH-05, VT-IT-03, VT-IT-04, VT-IT-05 |

### 阶段 7B：冗余传输与去重

已完成。当前基线已新增独立 `redundancy` 模块，并把 `RedundancyConfig` 从 parser 接入 `EngineConfig` 与 `RoutingContext`；`CGRRouter` 现在会在主路径选定后生成 `backup_routes` 与 `redundancy_considered`，`SimEngine` 会对单播 `SINGLE_BACKUP` / `MULTI_BACKUP` 物化备份副本，并在 `BundleProtocolAgent` 节点接受点执行 node-local first-arrival wins 去重，复用既有 `DUPLICATE_DROPPED` 事件链路写入 logger / stats。多播侧已补齐最小 `TRUNK_ONLY` shared-prefix trunk backup：当共享 trunk 存在可替代首跳时，engine 会创建单条 trunk backup dispatch，并在后续分支目的节点收敛重复副本。实验配置层已把 `exp5_redundancy` 接入 runtime 覆盖，完整回归基线提升到 169/169。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 7B.1 | 新增 `redundancy.hpp/.cpp`，实现 `RedundancyConfig`、`RouteDiversityAnalyzer`、`DeliveryRiskEstimator`、`RedundancyManager` | algorithms/redundancy.hpp/.cpp | 创新目标、实验方案 6.5 |
| 7B.2 | 在 `CGRRouter` 中插入“主路径选择 → 冗余规划 → commit”链路，支持 `NONE` / `SINGLE_BACKUP` / `MULTI_BACKUP` / `TRUNK_ONLY` 模式 | algorithms/cgr.hpp/.cpp, algorithms/redundancy.cpp | 实验方案 6.5, 6.6 |
| 7B.3 | 扩展 Bundle 冗余副本元数据，区分主副本、备份副本和干线冗余副本 | models/bundle.hpp/.cpp | 实验方案 6.5 |
| 7B.4 | 在 SimEngine / BPA 中实现首达交付、重复副本去重与丢弃统计 | simulation/engine.cpp, simulation/stats.hpp/.cpp | 实验方案 6.5 |
| 7B.5 | 实现多播干线冗余与高价值分支冗余的最小可用策略，默认支持 `TRUNK_ONLY` | algorithms/multicast.cpp, algorithms/redundancy.cpp | 实验方案 6.5, 6.6 |
| 7B.6 | 编写冗余测试，覆盖风险阈值、差异度、容量保护、TTL 裕量和副本数量上限 | tests/test_redundancy.cpp | 实验方案 6.5, 6.6 |

### 阶段 8A：实验编排与批量运行

已完成。当前基线已具备文档级 matrix 覆写、实验运行 manifest、`run-scenario` / `run-experiment` 最小 CLI、Python 批量 smoke / full 运行脚本、结果聚合 JSON / CSV，以及 exp1 ~ exp6 的自动化 smoke 执行链路。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 8A.1 | 为实验一到实验六定义标准配置模板、对照组和参数扫描矩阵 | configs/experiments/**/*.json | 实验方案 6.1 ~ 6.6 |
| 8A.2 | 实现批量实验运行器，支持单场景执行、矩阵 sweep、固定随机种子和结果目录规范 | scripts/run_experiments.py, src/main.cpp | 实验方案 4.2, 4.3 |
| 8A.3 | 实现结果聚合脚本，输出汇总表、每组统计 JSON / CSV 和关键指标摘要 | scripts/aggregate_results.py | 实验方案输出要求 |
| 8A.4 | 为实验一到实验六分别提供最小可执行 smoke case，并纳入轻量自动校验 | tests/test_experiment_configs.cpp | 100% 可执行性 |

### 阶段 8B：可视化、CLI 与实验交付

已完成。当前基线已具备 `export` / `plot` 子命令、共享 Python 结果工具、SVG 拓扑/时间轴/利用率/对比绘图脚本、README、实验手册与指标说明；`results/export_smoke` 和 `results/plot_smoke` 也已在工作区内产出最小样例，可直接作为交付模板与文档示例。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 8B.1 | 完善 `plot_topology.py`、`plot_timeline.py`、`plot_utilization.py`，并新增实验对比绘图脚本 | scripts/*.py | IO-OUT-05 |
| 8B.2 | 完善 `main.cpp`、命令行参数和帮助信息，支持 `run-scenario` / `run-experiment` / `export` / `plot` 子命令 | src/main.cpp | 可用性 |
| 8B.3 | 编写实验运行手册、结果目录约定和指标解释说明 | README.md, docs/experiments.md | 100% 可执行性 |
| 8B.4 | 提供样例输入、样例输出和图表模板，确保实验结果可直接复现与汇报 | configs/, docs/, scripts/ | 交付 |

### 阶段 9：系统测试与最终交付

已完成。当前基线已新增 `scripts/benchmark_nf_pf_01.py` 和 `scripts/stage9_suite.py`，并在当前 Windows + MinGW 环境下完成正式收口：`ctest --test-dir build --output-on-failure` 达到 **189/189**，NF-PF-01 场景以 20 节点 / 200 Contact / 1000 Bundle 形态稳定生成并在 **2.886 秒** 内执行完成，Stage 9 代表性套件共完成 **24 个 run** 的正式执行、聚合导出、SVG 绘图与标准复现包打包，最终产物已写入 `results/stage9/final`。

| 编号 | 任务 | 产出文件 | 覆盖需求 |
|---|---|---|---|
| 9.1 | 全量回归测试 | 测试报告 | 全部测试需求 |
| 9.2 | 性能测试：20 节点 / 200 Contact / 1000 Bundle | 性能报告 | NF-PF-01 |
| 9.3 | 执行实验一到实验六的代表性场景，确认全部可从配置与命令行直接运行 | 实验执行记录 | 实验方案 6.1 ~ 6.6 |
| 9.4 | 若不达标则进行 profiling 和优化 | 各模块源文件 | NF-PF-02 |
| 9.5 | 审查算法注释、统计项和实验文档是否与需求、设计和实验方案完全对齐 | 各模块源文件, docs/ | NF-MT-02 |
| 9.6 | 完善 README、构建与运行说明，打包实验复现包 | README.md, release package | 最终交付 |

---

## 4. 阶段依赖与并行关系

### 4.1 关键路径

新的关键路径为：

0 → 1 → 2A → 2B → 2C → 2D → 3A → 3B → 3C → 4B → 5 → 6A → 6B → 7A → 7B → 8A → 8B → 9

### 4.2 可并行阶段

- 3A 可在阶段 1 完成后启动，不必等待算法层完成
- 4A 只依赖数据模型，可在阶段 1 完成后并行推进，但必须在 8A 前收口实验配置格式
- 4B 可在 3A 事件结构稳定后与 3B 并行推进
- 实验样例数据和参考场景可在 4A 后提前准备，但只有在 8A 的运行器完成后才视为“可执行”
- 8B 的绘图脚本可在 4B 统计格式稳定后并行推进，但命令行与手册必须等 8A 输出目录规范定稿

### 4.3 关键依赖说明

- 2D 的 `commitRoute` 完成前，不允许启动 6A 的无副作用多播构树
- 3C 必须依赖 3B，因为 Contact 失效、动态计划更新和逐跳重算都要求真实事件流
- 4A 必须覆盖故障注入和冗余配置，否则实验四、实验五、实验六无法仅通过配置运行
- 6B 必须依赖 3C，因为局部修复实验要求在运行时注入 Contact 失效并观察局部传播计划更新
- 7A 的增强功能只能在 5 和 6B 的基线行为稳定后进入，以免把算法错误与增强错误混在一起
- 7B 必须依赖 2D、4B 和 6B；冗余传输既依赖主路径选择，也依赖去重统计和多播干线语义
- 8A 必须依赖 4A、4B、5、6B、7B；在这些阶段完成前，实验方案只能“理论可做”，不能判定为 100% 可执行
- 9.3 是最终实验可执行性闸门：只有实验一到实验六都能按文档和命令直接运行，计划才算真正完成

---

## 5. 验收标准

### 5.1 阶段通过标准

| 阶段 | 通过标准 |
|---|---|
| 1 | 模型层测试通过，覆盖版本化、缓存绑定、多播副本元数据和 RouteLeg |
| 2A | REF-1 示例拓扑下最短路径与标准一致 |
| 2B | K 条路由结果正确，且 `plan_version` 变化后缓存自动失效 |
| 2C | 7 条排除规则均有正反例测试，Phase 2 自身不修改 MTV |
| 2D | Phase 3 选择结果正确，`commitRoute` 前后容量状态符合预期 |
| 3A | 事件调度在同时间戳下仍保持确定性顺序 |
| 3B | 小规模单播仿真完成，逐跳重计算正确触发 |
| 3C | Contact 失效、关键干线移除和动态计划更新都能被固定随机种子稳定重放 |
| 4A | 实验方案所需配置文件可正确解析，`contact_id`、`plan_version`、机制开关和实验参数初始化正确 |
| 4B | 路由日志、局部修复日志、冗余日志与统计 JSON / CSV 可导出 |
| 5 | VT-IT-01、VT-IT-02、VT-SC-01 通过 |
| 6A | 多播构树正确，且不产生容量副作用 |
| 6B | VT-IT-06、VT-IT-07、VT-IT-08 和 VT-UT-08 通过 |
| 7A | 增强功能可独立启停，VT-IT-03、VT-IT-04、VT-IT-05 通过 |
| 7B | 冗余模式 `NONE`、`SINGLE_BACKUP` 和 `TRUNK_ONLY` 可运行，且至少一个高风险场景中交付可靠性提升同时受容量阈值约束 |
| 8A | 实验一到实验六均具备可执行配置、批量运行命令和最小 smoke case |
| 8B | 绘图脚本、CLI 和实验运行手册完整，第三方无需改代码即可复现实验 |
| 9 | 全量测试通过，性能达标，实验方案六个实验全部可执行 |

### 5.2 实验方案 100% 可执行判定标准

- 实验一能够输出单播路由日志、交付统计、ETO / PBAT / RVL 明细和逐跳重算次数
- 实验二能够按节点数、Contact 数和流量负载做矩阵 sweep，并输出运行时间、交付率、时延和 Contact 利用率
- 实验三能够对比“独立单播”和“多播传播计划”，并输出共享前缀节省率与全组完成率
- 实验四能够注入 Contact 失效并输出局部修复次数、受影响目的节点子集、修复成功率和交付保持率
- 实验五能够对比无冗余、单备份冗余和多播干线冗余，并输出冗余触发率、首达命中率、重复副本丢弃率、冗余开销比和收益成本比
- 实验六能够通过机制开关运行全部 6 组消融方案，并输出统一格式的统计表与对比图
- 上述六个实验均应具备：配置文件、运行命令、输出目录、结果说明和最小 smoke case

### 5.3 最终交付标准

- 所有“必须”级需求实现完毕
- 所有“必须”级验证项通过，包括 VT-UT-07、VT-UT-08、VT-IT-07、VT-IT-08，以及新增的冗余与实验执行测试
- 单播、多播、局部修复、增强功能和冗余传输的行为都可通过日志与统计结果追踪
- 实验一到实验六均可在 Windows 上通过 CMake 构建后的程序和脚本直接运行，并具备跨平台迁移条件

---

## 6. 风险与应对

| 风险 | 影响 | 可能性 | 应对措施 |
|---|---|---|---|
| 动态 Dijkstra 理解偏差导致 Phase 1 错误 | 高 | 中 | 先用 REF-1 图例做最小验证，再进入 Phase 1 缓存逻辑 |
| 规划与提交分离没有执行到底，Phase 2 或运行时提前污染容量 | 高 | 中 | 为 Phase 2、Phase 3、SimEngine 提交路径和 `commitRoute` 分别写独立测试 |
| `plan_version` 失效机制遗漏某条修改路径 | 高 | 中 | ContactPlan 的所有修改入口集中走版本更新，并在解析器、故障注入、局部修复场景重复验证 |
| 多播共享前缀重复扣减容量 | 高 | 中 | 在提交层按 ContactId 和实际副本数去重，并增加 VT-UT-08 与实验三对照测试 |
| 多播局部修复后传播计划不一致 | 高 | 中 | 每次局部修复都重新编码受影响子树，并记录修复前后版本号和目的节点子集 |
| 冗余副本数量失控导致容量被耗尽 | 高 | 中 | RedundancyManager 强制执行 `max_extra_copies`、容量保护阈值和 TTL 裕量约束 |
| 首达去重逻辑错误导致统计偏差 | 高 | 中 | 为单播和多播分别设计去重测试，日志和统计同时校验 `origin_bundle_id` 语义 |
| 实验参数矩阵过大导致运行时间不可控 | 中 | 高 | 预设代表性场景与完整 sweep 两级方案，批量运行器支持按标签、按阶段和按随机种子分批执行 |
| 统计项与实验方案指标定义不一致 | 中 | 中 | 在 4B 明确统计字典和字段含义，并在 8B 手册中固定结果解释说明 |
| 文档再次滞后于代码 | 中 | 中 | 每完成一个阶段同步更新 devlog.md，阶段切换前审查 development_plan.md 与实验手册 |
