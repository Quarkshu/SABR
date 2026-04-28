# SABR 仿真系统 — 开发日志

---

## 阶段 0：项目脚手架搭建

**日期**：2026-04-12  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 |
|---|---|---|
| 0.1 | 创建标准目录结构 | ✅ |
| 0.2 | 编写根 CMakeLists.txt | ✅ |
| 0.3 | 集成 Google Test（FetchContent） | ✅ |
| 0.4 | 编写最小 main.cpp 和空测试 | ✅ |
| 0.5 | 配置 .gitignore | ✅ |

### 目录结构

```
sabr/
├── include/
│   ├── models/
│   ├── algorithms/
│   ├── simulation/
│   └── io/
├── src/
│   ├── models/
│   ├── algorithms/
│   ├── simulation/
│   ├── io/
│   └── main.cpp
├── tests/
│   ├── fixtures/
│   └── test_stub.cpp
├── configs/
├── scripts/
├── CMakeLists.txt
└── .gitignore
```

### 构建系统设计

- **CMake ≥ 3.16**，C++17 标准（`CMAKE_CXX_STANDARD 17`，强制 `REQUIRED`，禁用扩展）
- **sabr_core**：核心库（模型 + 算法 + 仿真引擎），零外部依赖。阶段 0 为 `INTERFACE` 库占位，阶段 1 起切换为 `STATIC` 库
- **sabr_io**：IO 库，依赖 sabr_core 和 nlohmann/json。阶段 0 为 `INTERFACE` 库占位
- **sabr**：可执行文件入口，链接 sabr_core + sabr_io
- **sabr_tests**：测试可执行文件，通过 FetchContent 拉取 Google Test v1.14.0
- 使用 `gtest_discover_tests()` 自动注册测试用例到 CTest

### 环境信息

| 项 | 值 |
|---|---|
| 操作系统 | Windows |
| 编译器 | GCC 15.2.0 (MinGW-w64) |
| CMake 生成器 | MinGW Makefiles |
| CMake | `D:\Program Files\CMake\bin\cmake.exe` |
| C++ 编译器 | `D:\Program Files\mingw64\bin\c++.exe` |

> **注意**：当前环境需指定 `-G "MinGW Makefiles"` 生成器，否则 CMake 默认选择 NMake 会因无 MSVC 编译器而失败。

### 验收结果

```
cmake -B build -G "MinGW Makefiles"   # 配置成功
cmake --build build                    # 编译通过，产出 sabr.exe + sabr_tests.exe
ctest --test-dir build                 # 1/1 测试通过 (StubTest.BuildChainWorks)
.\build\sabr.exe                       # 输出 "sabr v1.0.0"
```

### 产出文件

| 文件 | 说明 |
|---|---|
| `CMakeLists.txt` | 根构建脚本 |
| `src/main.cpp` | 最小入口，输出版本号 |
| `tests/test_stub.cpp` | 空测试，验证构建链路 |
| `.gitignore` | Git 忽略规则（build/、IDE 文件、编译产物等） |

---

## 阶段 1：数据模型层

**日期**：2026-04-12  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 1.1 | 实现 `Contact` 结构体：字段定义、`volume()` 计算、MTV 初始化与 `consume_mtv()`、`is_terminated()` | ✅ | DS-CON-01~05 |
| 1.2 | 实现 `RangeInterval` 结构体：字段定义、`involves()` 双向查询、`get_owlt()` | ✅ | DS-RNG-01~02 |
| 1.3 | 实现 `ContactPlan` 类：Contact/Range 容器、查询接口（`get_contacts_from/to/between`、`get_owlt`）、`add_contact`、`remove_contact`、`purge_terminated`、修改标志管理 | ✅ | DS-CP-01~03 |
| 1.4 | 实现 `Bundle` 结构体：全部字段、`expiration_time()`、`evc()` 计算、Priority 枚举 | ✅ | DS-BDL-01~07 |
| 1.5 | 实现 `Route` 结构体：hops 序列、`compute_termination_time()`、`compute_entry_node()`、`is_valid()` 校验 | ✅ | DS-RTE-01~06 |
| 1.6 | 实现 `Node` 类：node_number、routing_table、send_queues、excluded_neighbors、`invalidate_routes()`、`get_route_list()` | ✅ | DS-NOD-01~05 |
| 1.7 | 实现 `MulticastTree`/`TreeNode` 结构体：节点/下一跳映射、`is_branching_node()`、`prune()`、`serialize()`/`deserialize()` | ✅ | FR-MT-04 |
| 1.8 | 编写 `test_models.cpp`：42 个测试用例，覆盖全部数据模型 | ✅ | VT-UT-02 |

### 构建系统变更

- `sabr_core` 从 `INTERFACE` 库切换为 `STATIC` 库，添加 5 个源文件
- `sabr_tests` 添加 `tests/test_models.cpp`

### 设计决策

- **Contact**：`mtv` 使用 `std::array<double, 3>` 固定大小数组（3 个优先级），避免动态分配。`consume_mtv(priority, evc)` 对指定优先级及所有更低优先级扣减
- **RangeInterval**：与 `Contact` 合并在同一头文件 `contact.hpp` 中，因二者紧密关联
- **ContactPlan**：`get_owlt()` 无匹配 RangeInterval 时返回 0.0（默认无延迟）；`purge_terminated()` 和 `remove_contact()` 仅在实际删除时触发 `mark_modified()`
- **Bundle**：`MulticastTree` 和 `TreeNode` 与 Bundle 放在同一头文件，因 Bundle 直接包含 MulticastTree 成员
- **Route**：`is_valid()` 校验连续性约束（c 和 d），首尾节点约束（a 和 b）由调用方保证
- **MulticastTree**：序列化采用简单文本格式 `root;node_id:next_hop=d1,d2;...`，满足封装到 Bundle 的需求

### 验收结果

```
cmake --build build                    # 编译通过
ctest --test-dir build                 # 42/42 测试通过
```

```
100% tests passed, 0 tests failed out of 42
Total Test time (real) =   0.32 sec
```

重点验证通过项：
- `Contact::volume()` = `(end - start) * rate`
- `Bundle::evc()` = `h + p + max(0.03*(h+p), 100)`（大/小 Bundle 两分支 + 边界）
- `Contact::consume_mtv()` 按优先级分层扣减（Bulk 仅扣 [0]，Normal 扣 [0,1]，Expedited 扣 [0,1,2]）
- `Route::is_valid()` 合法/非法序列判断（空路由、断链、时间违反）
- `ContactPlan` 修改标志触发与重置
- `MulticastTree` 序列化/反序列化往返一致、裁剪子树正确

### 产出文件

| 文件 | 说明 |
|---|---|
| `include/models/contact.hpp` | Contact + RangeInterval 头文件 |
| `include/models/contact_plan.hpp` | ContactPlan 头文件 |
| `include/models/bundle.hpp` | Priority 枚举 + Bundle + MulticastTree/TreeNode 头文件 |
| `include/models/route.hpp` | Route 头文件 |
| `include/models/node.hpp` | Node 头文件 |
| `src/models/contact.cpp` | Contact + RangeInterval 实现 |
| `src/models/contact_plan.cpp` | ContactPlan 实现 |
| `src/models/bundle.cpp` | Bundle + MulticastTree 实现 |
| `src/models/route.cpp` | Route 实现 |
| `src/models/node.cpp` | Node 实现 |
| `tests/test_models.cpp` | 数据模型层单元测试（42 个用例） |

---

## 阶段 1 补充：模型层契约对齐

**日期**：2026-04-13  
**状态**：✅ 已完成

### 背景

根据 `需求说明与架构设计文档.md` 的更新内容，对已完成的数据模型层进行了基线重构，使后续阶段直接建立在新的计划版本、无副作用规划和多播局部修复语义上，避免阶段 2 以后出现结构性返工。

### 完成任务

| 编号 | 任务 | 状态 |
|---|---|---|
| 1A.1 | `Contact` 增加 `contact_id`、`plan_version`，保留 MTV 分层容量语义 | ✅ |
| 1A.2 | `ContactPlan` 从 `modified` 标志切换为单调递增 `plan_version` | ✅ |
| 1A.3 | `Node` 路由缓存增加版本绑定能力 | ✅ |
| 1A.4 | `Bundle` 增加多播计划、副本血缘、待交付/已交付目的集合等字段 | ✅ |
| 1A.5 | `Route` 从 `hops` 切换为 `RouteLeg` 逐段结构 | ✅ |
| 1A.6 | `MulticastTree` 的分支记录增加 `contact_id` | ✅ |
| 1A.7 | 重写模型层测试以覆盖新增契约 | ✅ |
| 1A.8 | 全面重排 `development_plan.md`，同步后续阶段目标与验收项 | ✅ |

### 代码变更摘要

- **Contact / ContactPlan**
	- `Contact` 现在显式记录 `contact_id` 与 `plan_version`
	- `ContactPlan` 的 Contact 和 Range 容器改为受控访问
	- `set_contacts()`、`set_ranges()`、`add_contact()`、`add_range()`、`remove_contact()`、`purge_terminated()` 均统一驱动 `plan_version` 递增
	- 计划版本变化后，所有存量 Contact 会重新绑定到当前版本

- **Node**
	- `routing_table` 改为 `destination -> {plan_version, routes}`
	- 新增 `current_plan_version()`、`invalidate_stale_routes()`、`find_route_list()`
	- `get_route_list()` 在版本不匹配时自动清空旧缓存并重建

- **Bundle / MulticastTree**
	- `Bundle` 增加 `pending_destinations`、`delivered_destinations`、`multicast_plan`、`encoded_plan_version`、`origin_bundle_id`、`parent_bundle_id`、`replica_id`
	- `Bundle::evc()` 在多播场景下计入传播计划编码开销
	- 新增 `mark_visited()`、`has_visited()`、`mark_destination_delivered()`、`make_replica()`
	- `MulticastTree` 的每条分支现在保存 `contact_id` 和目的节点子集

- **Route**
	- 由 `std::vector<Contact*> hops` 升级为 `std::vector<RouteLeg> legs`
	- `RouteLeg` 预留 `earliest_transmission_time`、`earliest_arrival_time`、`evl`
	- `Route::is_valid()`、`compute_termination_time()`、`compute_entry_node()` 全部基于逐段结构重写

### 测试更新

`tests/test_models.cpp` 从 42 个测试扩展为 52 个测试，新增覆盖：

- Contact 的 `plan_version` 绑定
- ContactPlan 的版本递增与 Contact 对齐
- Bundle 的多播计划开销计入 EVC
- Bundle 的 visited_nodes、目的节点交付状态与副本血缘
- RouteLeg 的空指针合法性检查与 hop_count
- Node 的版本绑定缓存与陈旧缓存失效
- MulticastTree 分支中的 `contact_id` 序列化 / 反序列化

### 验收结果

```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **52/52 测试通过**

### 本次未纳入范围

- 未实现 `ContactGraph`、`Dijkstra`、`YenKSP`、`Phase1/2/3`、`CGR`
- 未实现仿真引擎和 IO 层功能
- 未实现多播执行、局部修复和增强功能

这些内容已在更新后的 `development_plan.md` 中重新排期。

---

## 阶段 2A：接触图构造与动态 Dijkstra

**日期**：2026-04-20  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 2A.1 | 新增 `ContactGraph`、`GraphVertex`、`GraphEdge`、`ContactGraphBuilder` | ✅ | FR-CG-01 ~ FR-CG-03 |
| 2A.2 | 实现动态 Dijkstra 搜索器 `DijkstraRouter` 和 `DijkstraResult` | ✅ | FR-P1-01 ~ FR-P1-04 |
| 2A.3 | 在最短路径回溯阶段回填 `RouteLeg.earliest_transmission_time` / `earliest_arrival_time` | ✅ | DS-RTE-07 |
| 2A.4 | 新增阶段 2A 单元测试并接入构建系统 | ✅ | VT-UT-03, VT-SC-01 |

### 新增文件

| 文件 | 说明 |
|---|---|
| `include/algorithms/contact_graph.hpp` | 接触图顶点、边和构图器接口 |
| `src/algorithms/contact_graph.cpp` | 接触图构造实现 |
| `include/algorithms/dijkstra.hpp` | 动态 Dijkstra 结果类型与搜索接口 |
| `src/algorithms/dijkstra.cpp` | 动态松弛、前驱记录和 Route 回溯实现 |
| `tests/test_dijkstra.cpp` | 阶段 2A 的 ContactGraph / Dijkstra 单元测试 |

### 构建系统变更

- 新增 `sabr_algorithms` 静态库，承载 `contact_graph.cpp` 和 `dijkstra.cpp`
- `sabr` 和 `sabr_tests` 均链接 `sabr_algorithms`
- `sabr_tests` 新增 `tests/test_dijkstra.cpp`

### 设计决策

- **ContactGraph 仅表达可连接关系**
	- 图的边不存静态最短路径代价
	- 邻接关系只表示“当前 Contact 之后可能继续使用的 Contact”
	- 最早传输时间和最早到达时间全部在 Dijkstra 松弛阶段动态计算

- **虚拟 root / terminal 顶点**
	- `root_contact_id = max(ContactId)`，`terminal_contact_id = max(ContactId) - 1`
	- root 顶点表示本地节点自身，terminal 顶点表示目的节点自身
	- root 到首跳 Contact、末跳 Contact 到 terminal 的边代替特殊起点/终点分支逻辑

- **图的拓扑顺序约束**
	- 真实 Contact 顶点按 `start_time`、`contact_id` 排序后入图
	- 仅从较早序的 Contact 向较晚序的 Contact 建边，保持图结构无环
	- 该实现优先满足阶段 2A 的 DAG 与动态松弛验证要求

- **动态 Dijkstra**
	- `dist[v]` 表示“到达顶点 v 的最早时间”
	- 对本地首跳 Contact：`ETT = max(contact.start_time, current_time)`
	- 对非首跳 Contact：`ETT = max(contact.start_time, predecessor_arrival_time)`
	- 若 `ETT > contact.end_time`，则该 Contact 在当前路径上不可用，松弛时直接跳过
	- `EAT = ETT + OWLT + owlt_margin`

- **Route 回填策略**
	- 回溯时跳过 root 和 terminal，仅将真实 Contact 组装为 `RouteLeg`
	- 使用搜索结果中的前驱顶点到达时间回填 `earliest_transmission_time`
	- 使用搜索结果中的顶点最早到达时间回填 `earliest_arrival_time`
	- `best_case_delivery_time`、`termination_time`、`entry_node` 在回溯后统一写入 `Route`

### 测试覆盖

`tests/test_dijkstra.cpp` 新增 12 个测试用例，覆盖：

- ContactGraph 的 root / terminal 创建
- root 到首跳、末跳到 terminal 的边生成
- 线性链路上的顺序建边
- 已终止 Contact 的过滤
- 图上所有边满足拓扑顺序（无环结构约束）
- 线性拓扑最短路径搜索
- 本地首跳的 `current_time` 约束
- 非首跳 Contact 使用前驱到达时间计算 `ETT`
- `owlt_margin` 对最终最早到达时间的影响
- `ETT > end_time` 时跳过 Contact
- 双路径场景下选择更小 `EAT` 的路径
- 目的节点不可达时返回空路由

### 验收结果

```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **64/64 测试通过**
- 其中阶段 2A 新增测试 **12/12 通过**

### 阶段结论

阶段 2A 已达到当前开发计划中的交付目标：

- 已具备独立的接触图构造能力
- 已具备基于动态松弛的最早到达搜索能力
- 已能将搜索结果回填为后续阶段可直接复用的 `Route` / `RouteLeg`

后续可继续进入阶段 2B：Phase 1 与 Yen KSP。

---

## 阶段 2B：Phase 1 与 Yen KSP

**日期**：2026-04-20  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 2B.1 | 新增 `YenKSP`，基于接触图与动态 Dijkstra 计算 K 条候选路由 | ✅ | FR-P1-06 |
| 2B.2 | 新增 `Phase1`，实现版本绑定缓存复用与首次求路 | ✅ | FR-P1-05, FR-P1-07 |
| 2B.3 | 新增 `Phase1::recompute_more`，支持缓存失效后重算与额外路径扩容 | ✅ | FR-P1-08, DS-CP-04 |
| 2B.4 | 实现 One-Route-Per-Neighbor 增强 | ✅ | FR-ENH-01 |
| 2B.5 | 为“本地节点即目的节点”实现零跳本地交付路由 | ✅ | 阶段 2B 范围补充 |
| 2B.6 | 为 YenKSP / Phase1 / 约束版 Dijkstra / 零跳路由补充单元测试 | ✅ | VT-UT-03 |

### 新增文件

| 文件 | 说明 |
|---|---|
| `include/algorithms/yen_ksp.hpp` | Yen K 最短路径搜索接口 |
| `src/algorithms/yen_ksp.cpp` | 标准 Yen 算法、spur 搜索与候选池管理实现 |
| `include/algorithms/phase1.hpp` | Phase 1 公共接口与配置 |
| `src/algorithms/phase1.cpp` | 缓存命中、重算、邻居补齐与零跳本地交付实现 |
| `tests/test_yen_ksp.cpp` | YenKSP 单元测试 |
| `tests/test_phase1.cpp` | Phase1 单元测试 |

### 变更文件

| 文件 | 说明 |
|---|---|
| `include/models/route.hpp` / `src/models/route.cpp` | 为零跳本地交付补充 `local_delivery` 表示和工厂函数 |
| `include/algorithms/dijkstra.hpp` / `src/algorithms/dijkstra.cpp` | 为 Yen spur 搜索和邻居补齐新增约束版遍历能力 |
| `tests/test_models.cpp` | 新增零跳本地交付路由测试 |
| `tests/test_dijkstra.cpp` | 新增禁用边、禁用顶点、限制首跳邻居测试 |
| `CMakeLists.txt` | 将 `yen_ksp.cpp`、`phase1.cpp` 与新增测试接入构建 |

### 设计决策

- **YenKSP 复用同一份 ContactGraph 与 DijkstraRouter**
	- 不复制新的图模型，也不对 ContactGraph 做结构性裁剪
	- spur 搜索通过约束版 Dijkstra 表达：禁用边、禁用顶点、限制 root 首跳邻居
	- 这样 YenKSP 与 FR-ENH-01 共享同一套图搜索实现，避免两套路径生成逻辑分叉

- **约束版 Dijkstra 作为 2B 的共用底座**
	- `DijkstraConstraints` 支持 `disabled_vertices`、`disabled_edges`、`forced_first_hop_neighbor`
	- 原有 2A 路径搜索接口保持兼容，默认无约束时行为不变
	- 该能力既服务 Yen spur 搜索，也服务 One-Route-Per-Neighbor 的首跳限定

- **Phase1 保持无副作用规划**
	- 只读 `ContactPlan` 和 `Node.routing_table`
	- 只写入当前 `plan_version` 对应的路由缓存，不修改 Contact 的 MTV
	- `recompute_more` 通过提高目标路径数重新调用 YenKSP，而不是保留跨调用的候选池状态

- **One-Route-Per-Neighbor 作为标准 Yen 之后的补齐步骤**
	- 先计算标准 K 条最短路径
	- 再基于 root 的直达邻居集合检查当前 `entry_node` 覆盖情况
	- 对未覆盖邻居执行“限制首跳邻居”的约束搜索，并与标准 Yen 结果去重合并

- **零跳本地交付显式建模**
	- `Route` 新增 `local_delivery` 与 `local_delivery_node`
	- 用 `Route::make_local_delivery()` 生成空 `legs` 的合法路由
	- `compute_termination_time()`、`compute_entry_node()`、`is_valid()` 已支持该表示

### 测试覆盖

本轮新增 16 个测试用例，总测试数从 64 增长到 80：

- `tests/test_models.cpp`：3 个零跳本地交付路由测试
- `tests/test_dijkstra.cpp`：3 个约束版 Dijkstra 测试
- `tests/test_yen_ksp.cpp`：5 个 YenKSP 测试
- `tests/test_phase1.cpp`：5 个 Phase1 测试

覆盖场景包括：

- YenKSP 在线性、菱形、共享前缀和不可达拓扑上的 K 路搜索
- Phase1 的缓存命中、版本失效后重算、额外路径扩容
- One-Route-Per-Neighbor 的邻居覆盖补齐
- 本地节点即目的节点时的零跳本地交付
- 约束版 Dijkstra 的禁用边、禁用顶点、限制首跳邻居

### 验收结果

```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **80/80 测试通过**

### 阶段结论

阶段 2B 已达到当前开发计划中的交付目标：

- 已具备标准 Yen K 最短路径搜索能力
- 已具备版本绑定的 Phase1 缓存复用与额外重算入口
- 已具备 One-Route-Per-Neighbor 增强能力
- 已具备“本地即目的”的零跳本地交付表达

后续可进入阶段 2C：Phase 2 路由验证。

---

## 阶段 2C：Phase 2 路由验证

**日期**：2026-04-20  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 2C.1 | 实现排除节点构造：合并 `previous_node` 与 `excluded_neighbors` | ✅ | FR-P2-02 |
| 2C.2 | 实现完整 ETO 计算，保留 prior volume / relief / backlog lien 中间变量 | ✅ | FR-P2-03 |
| 2C.3 | 实现 PBAT 逐段计算，并回填 RouteLeg 的 first / last byte 时序 | ✅ | FR-P2-04 |
| 2C.4 | 实现 EVL / RVL 计算，逐段写入 `RouteLeg.evl` | ✅ | FR-P2-05, DS-RTE-08 |
| 2C.5 | 实现 7 条排除规则，并按独立函数组织 | ✅ | FR-P2-06 |
| 2C.6 | 实现 `Phase2::validate` 与 `CandidateRoute` 结果模型 | ✅ | FR-P2-01, FR-P2-07, FR-P2-08 |
| 2C.7 | 实现 Queue-Delay 与主动式 Anti-Loop 增强 | ✅ | FR-ENH-02, FR-ENH-04 |
| 2C.8 | 为 CandidateRoute / Phase2 / Queue-Delay / Anti-Loop 新增测试并纳入全量回归 | ✅ | VT-UT-02, VT-UT-04 |

### 新增文件

| 文件 | 说明 |
|---|---|
| `include/algorithms/phase2.hpp` | Phase2、ValidationContext、配置与结果接口 |
| `src/algorithms/phase2.cpp` | ETO / PBAT / RVL / 排除规则 / need_recompute 实现 |
| `tests/test_phase2.cpp` | Phase2 单元测试 |

### 变更文件

| 文件 | 说明 |
|---|---|
| `include/models/route.hpp` | RouteLeg 新增 last-byte 时序字段；新增 `Route.eto` 与 `CandidateRoute` |
| `tests/test_models.cpp` | 新增 RouteLeg / CandidateRoute 的模型契约测试 |
| `CMakeLists.txt` | 将 `phase2.cpp` 和 `test_phase2.cpp` 接入构建 |

### 设计决策

- **Phase2 结果采用 CandidateRoute，而不是继续直接返回 Route**
	- `Route` 继续作为 Phase1 产出的纯路径结构
	- `CandidateRoute` 负责承载 `valid` 和 `reject_reason`
	- 路由指标仍保留在 `Route` 上：`eto`、`pbat`、`rvl`、`possibly_looping`

- **ValidationContext 作为 2C 的最小上下文补口**
	- 当前仓库尚未实现完整 `NodeState / RoutingContext` 体系
	- 为满足 FR-P2-03 的完整 ETO 公式和 FR-ENH-02 的 Queue-Delay，本轮引入轻量 `ValidationContext`
	- 其职责仅是提供 `applicable_prior_contact_volume`、`applicable_backlog_relief`、`allocated_bytes` 和 `rerouted_due_to_guard`
	- 上述字段默认可为空或零值，便于在当前阶段保持最小侵入，并为后续仿真引擎真实填充预留接口

- **RouteLeg 时序扩展策略**
	- 保留现有 `earliest_transmission_time` / `earliest_arrival_time`，在 Phase2 中明确作为 first-byte 语义
	- 新增 `last_byte_transmission_time` / `last_byte_arrival_time`
	- 这样 PBAT 的逐段 first/last byte 计算可以完整回填到模型层，而无需再造临时结构

- **排除规则保持函数级拆分**
	- `best_case_expired`
	- `excluded_entry`
	- `loop_to_local`
	- `eto_too_late`
	- `pbat_expired`
	- `rvl_depleted`
	- `fragmentation_required`
	- 该组织方式便于 VT-UT-04 对每条规则独立命中

- **Critical Bundle 的 need_recompute 判定**
	- 普通 Bundle：仅在 `candidate_routes` 为空时置 `need_recompute = true`
	- Critical Bundle：额外比较“输入 Route 列表可覆盖的 `entry_node` 集合”和“valid 候选实际覆盖的 `entry_node` 集合”
	- 若存在缺失邻居，则同样触发 `need_recompute = true`

- **主动式 Anti-Loop 仅打标，不直接排除**
	- 若路由上某个远端 Contact 端节点出现在 `bundle.visited_nodes` 中，则标记 `possibly_looping = true`
	- Phase2 不因该标记直接判 invalid
	- 后续由 Phase3 在存在非闭环候选时优先选择非闭环路由

### 测试覆盖

本轮新增 14 个测试用例，总测试数从 80 增长到 94：

- `tests/test_phase2.cpp`：12 个 Phase2 测试
- `tests/test_models.cpp`：2 个 CandidateRoute / RouteLeg 模型测试

覆盖场景包括：

- zero-hop local delivery 在 Phase2 中直接通过
- 完整 ETO 公式中的 backlog / prior volume / relief / backlog lien
- PBAT 的 first / last byte 时序回填
- Queue-Delay 对后续 Contact 的延迟影响
- 7 条排除规则的独立命中
- RVL 与不可分片约束
- 主动式 Anti-Loop 的 potential loop 标记
- Critical Bundle 缺失邻居覆盖时触发 `need_recompute`

### 验收结果

```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **94/94 测试通过**

### 阶段结论

阶段 2C 已达到当前开发计划中的交付目标：

- 已具备完整的路由验证与候选筛选能力
- 已具备 CandidateRoute 结果模型
- 已具备 Queue-Delay 与主动式 Anti-Loop 增强能力
- 已具备 need_recompute 触发条件，为后续 Phase3 / CGR 编排提供稳定输入

后续可进入阶段 2D：Phase 3、提交接口与 CGR 编排。

---

## 阶段 2D：Phase 3、提交接口与 CGR 编排

**日期**：2026-04-20  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 2D.1 | 实现 `Phase3Selector` 的四级比较器：PBAT → hop_count → termination_time → entry_node | ✅ | FR-P3-02 |
| 2D.2 | 实现标准 Bundle 单一路由选择与 Critical Bundle 的受控泛洪选择 | ✅ | FR-P3-01, FR-P3-03, FR-P3-04 |
| 2D.3 | 新增 `RoutingDecision` 结果模型，显式区分 `DELIVER_LOCAL` / `FORWARD_ONE` / `FORWARD_FLOOD` / `ROUTE_FAIL` | ✅ | FR-P3-01, FR-P3-03, FR-P3-04 |
| 2D.4 | 实现 `CGRRouter::route_unicast`，串接计划修剪、适用性检查、Phase1、Phase2、Phase3 和补算循环 | ✅ | FR-CPC-01, FR-RP-01, FR-RP-02, FR-RR-01 |
| 2D.5 | 实现显式 `commit_route()`，确保只有最终提交阶段才扣减 MTV | ✅ | FR-P3-05 |
| 2D.6 | 新增阶段 2D 单元测试并接入构建系统 | ✅ | VT-UT-07 |

### 新增文件

| 文件 | 说明 |
|---|---|
| `include/algorithms/phase3.hpp` | `RoutingAction`、`RoutingDecision` 和 `Phase3Selector` 接口 |
| `src/algorithms/phase3.cpp` | 四级比较器、非闭环优先和 Critical 逐邻居选择实现 |
| `include/algorithms/cgr.hpp` | `RoutingContext` 与 `CGRRouter` 接口 |
| `src/algorithms/cgr.cpp` | `route_unicast()`、`plan_unicast()`、`evaluate_unicast()`、`commit_route()` 实现 |
| `tests/test_cgr.cpp` | 阶段 2D 单元测试 |

### 变更文件

| 文件 | 说明 |
|---|---|
| `CMakeLists.txt` | 将 `phase3.cpp`、`cgr.cpp` 和 `test_cgr.cpp` 接入构建 |

### 设计决策

- **文件命名与公开类型分离**
	- 文件名保持 `phase3.hpp/.cpp`、`cgr.hpp/.cpp`，与开发计划中的阶段命名一致
	- 公开类型采用 `Phase3Selector`、`CGRRouter`、`RoutingDecision`
	- 这样既保持阶段结构稳定，也对齐更新后的架构文档

- **route_unicast 与 commit_route 明确分离**
	- `route_unicast()` 只负责规划、验证和选择，返回 `RoutingDecision`
	- `commit_route()` 才执行 `Contact::consume_mtv()`
	- 该边界直接落实了“Phase 1/2/3 无容量副作用，提交阶段才扣减 MTV”的 2D 核心约束

- **Phase3 的 non-looping 优先策略**
	- `select_best()`：若存在非闭环候选，则仅在非闭环候选中做四级比较
	- `select_flood_set()`：按 `entry_node` 分组后，在每个邻居组内优先非闭环候选，而不是全局过滤所有闭环路径
	- 这样既保留主动式 Anti-Loop 的偏好，又不会破坏 FR-P3-04 的“每个有候选的邻居各一份副本”语义

- **CGRRouter 继续复用 Node 本地缓存，不引入全局 route cache**
	- `plan_unicast()` 直接复用 `Phase1::compute()`
	- `route_unicast()` 在进入 Phase1 之前执行 `ContactPlan::purge_terminated()` 与 `Node::invalidate_stale_routes()`
	- 若接触计划中不存在任何指向目的节点的 Contact，则直接返回 `ROUTE_FAIL`，`failure_reason = "cgr_not_applicable"`

- **补算循环最小化实现**
	- 当 `Phase2::Result.need_recompute` 为真且 `recompute_budget > 0` 时，`CGRRouter` 调用 `Phase1::recompute_more()` 追加候选路径
	- 当前实现每轮补算追加 1 条路径，保持行为简单、可验证，也避免无限重算

### 测试覆盖

本轮新增 7 个测试用例，总测试数从 94 增长到 101：

- `Phase3SelectorTest.SelectBestAppliesFourLevelComparator`
- `Phase3SelectorTest.SelectBestPrefersNonLoopingCandidate`
- `Phase3SelectorTest.SelectFloodSetChoosesBestRoutePerNeighbor`
- `CGRRouterTest.RouteUnicastReturnsRouteFailWhenCgrIsNotApplicable`
- `CGRRouterTest.RouteUnicastRecomputesMoreWhenInitialCandidatesAreEmpty`
- `CGRRouterTest.RouteUnicastReturnsFloodDecisionForCriticalBundle`
- `CGRRouterTest.PlanningDoesNotConsumeMtvUntilCommit`

覆盖场景包括：

- PBAT、hop_count、termination_time、entry_node 四级 tie-break
- 主动式 Anti-Loop 标记进入 Phase3 后的 non-looping 优先
- Critical Bundle 逐邻居选一条最佳候选
- FR-CPC-01 的“不适用即失败”路径
- Phase2 触发补算后由 CGRRouter 调 `recompute_more()` 收敛到有效候选
- `route_unicast()` 在提交前不修改 MTV，`commit_route()` 对 NORMAL Bundle 正确扣减 NORMAL 与 BULK 层而不影响 EXPEDITED 层

### 验收结果

```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **101/101 测试通过**

### 阶段结论

阶段 2D 已达到当前开发计划中的交付目标：

- 已具备 `RoutingDecision` 驱动的 Phase3 / CGR 单播决策链路
- 已具备显式 `commit_route()`，不再把容量副作用混入规划阶段
- 已具备基础补算编排与 `ROUTE_FAIL` 失败路径
- 已为阶段 3A/3B 的事件系统和逐跳重新路由提供稳定接口基线

后续可进入阶段 3A：事件系统。

---

## 阶段 3A：事件系统

**日期**：2026-04-21  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 3A.1 | 实现 `EventType`、`EventPayload`、`Event` 和 `EventId` 基础事件模型 | ✅ | FR-SIM-01, FR-SIM-02 |
| 3A.2 | 实现 `DiscreteEventScheduler`，保证时间优先与同时间戳稳定顺序 | ✅ | FR-SIM-01, FR-SIM-03 |
| 3A.3 | 实现 `RuntimeContext`、`NodeRuntime` 和最小 `BundleProtocolAgent` 运行时壳层 | ✅ | NF-EX-02, FR-BPA-01 |
| 3A.4 | 新增阶段 3A 单元测试并接入构建系统 | ✅ | FR-SIM-01 |

### 新增文件

| 文件 | 说明 |
|---|---|
| `include/simulation/event.hpp` | 事件类型、事件载荷和 `Event` 数据结构接口 |
| `src/simulation/event.cpp` | `event_type_name()` 实现 |
| `include/simulation/scheduler.hpp` | `DiscreteEventScheduler` 接口 |
| `src/simulation/scheduler.cpp` | 事件入队、稳定排序和分发实现 |
| `include/simulation/runtime_context.hpp` | `RuntimeContext`、`NodeRuntime` 和 `BundleProtocolAgent` 接口 |
| `src/simulation/runtime_context.cpp` | 最小运行时装配与 BPA 队列壳层实现 |
| `tests/test_simulation.cpp` | 阶段 3A 单元测试 |

### 变更文件

| 文件 | 说明 |
|---|---|
| `CMakeLists.txt` | 新增 `sabr_simulation` 静态库，并接入 `test_simulation.cpp` |

### 设计决策

- **事件载荷采用强类型 `std::variant`，不引入 `std::any`**
	- 事件模型直接用 `BundleCreatedData`、`ContactStateData` 等结构体表达业务载荷
	- 这样可以让调度器和后续引擎代码在编译期获得类型检查，避免字符串键或弱类型分发

- **调度顺序由 `timestamp + seq_no` 双键决定**
	- `DiscreteEventScheduler::schedule()` 为每个事件分配单调递增的 `seq_no`
	- 相同 `timestamp` 下按入队顺序执行，满足实验复现对确定性的要求
	- 若在当前逻辑时间之后补入“过去时间”的事件，调度器会把它钳到 `current_time()`，避免时间倒退

- **3A 只做最小 BPA 运行时壳层，不提前混入 3B 路由逻辑**
	- `BundleProtocolAgent` 当前只跟踪待处理 Bundle、本地交付结果和按 Contact/优先级分组的出队缓存
	- `RuntimeContext` 统一装配 `ContactPlan`、节点运行时对象，以及 Logger / Stats 钩子
	- 这样 3B 可以直接在这层之上接入 `SimEngine`、TrafficGenerator 和真实逐跳事件处理，而不用返工事件基础设施

### 测试覆盖

本轮新增 7 个测试用例，总测试数从 101 增长到 108：

- `EventTest.EventTypeNameMatchesEnumValue`
- `DiscreteEventSchedulerTest.ProcessesEventsInTimestampOrder`
- `DiscreteEventSchedulerTest.PreservesInsertionOrderForSameTimestamp`
- `DiscreteEventSchedulerTest.RunStopsBeforeFutureEvents`
- `RuntimeContextTest.BindsContactPlanAndCreatesNodeRuntime`
- `RuntimeContextTest.EmitsLogAndStatHooks`
- `BundleProtocolAgentTest.TracksPendingQueuedAndDeliveredBundles`

覆盖场景包括：

- 事件类型名映射与基础事件模型可用性
- 按时间顺序执行事件
- 同时间戳事件按注册顺序稳定执行
- `run(end_time)` 只消费截止时间之前的事件
- `RuntimeContext` 对 `ContactPlan` 和节点运行时对象的装配行为
- Logger / Stats 钩子的最小联通性
- `BundleProtocolAgent` 的待处理、待发送和本地交付跟踪语义

### 验收结果

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **108/108 测试通过**

### 阶段结论

阶段 3A 已达到当前开发计划中的交付目标：

- 已具备可扩展的强类型事件模型和确定性离散事件调度能力
- 已具备 `RuntimeContext` / `NodeRuntime` / `BundleProtocolAgent` 的最小运行时装配壳层
- 已为阶段 3B 的仿真引擎、流量生成、节点生命周期处理和事件回调链路提供稳定基线

后续可进入阶段 3B：仿真引擎与流量生成。

---

## 阶段 3B：仿真引擎与流量生成

**日期**：2026-04-21  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 3B.1 | 实现 `TrafficGenerator`，支持 `SINGLE` / `BATCH` / `PERIODIC` 三种流量模式 | ✅ | FR-TG-01 ~ FR-TG-03 |
| 3B.2 | 实现 `SimEngine` 初始化、节点装配与 Contact start/end 事件注册 | ✅ | FR-SIM-02, FR-SIM-04 |
| 3B.3 | 实现 `BUNDLE_CREATED` / `BUNDLE_ARRIVED` / `BUNDLE_TX_START` / `BUNDLE_TX_END` / `BUNDLE_DELIVERED` / `BUNDLE_EXPIRED` 处理流程，并接入 `route_unicast()` 与 `commit_route()` | ✅ | FR-SIM-02, FR-BPA-01 ~ FR-BPA-03, FR-RR-01 |
| 3B.4 | 实现按优先级发送队列出队、传输延迟、传播延迟与逐跳重计算计数 | ✅ | FR-BPA-02, FR-BPA-03, FR-RR-01 |
| 3B.5 | 新增 3 节点 / 3 Contact / 5 Bundle 小场景单播仿真测试 | ✅ | FR-BPA-01 |

### 新增文件

| 文件 | 说明 |
|---|---|
| `include/simulation/traffic.hpp` | `TrafficGenerationMode`、`TrafficPattern`、`ScheduledBundle` 与 `TrafficGenerator` 接口 |
| `src/simulation/traffic.cpp` | 单次、批量、周期流量实例化实现 |
| `include/simulation/engine.hpp` | `EngineConfig`、`EngineMetrics`、`SimEngine` 接口 |
| `src/simulation/engine.cpp` | Contact/Bundle 事件流、队列调度与单播仿真引擎实现 |

### 变更文件

| 文件 | 说明 |
|---|---|
| `include/simulation/runtime_context.hpp` | 为 `BundleProtocolAgent` 增加按 Contact 查询和优先级出队接口 |
| `src/simulation/runtime_context.cpp` | 让 BPA 队列与 `Node.send_queues` 同步，支撑 Phase2 的 ETO backlog 计算 |
| `tests/test_simulation.cpp` | 增加 3B 的流量生成与端到端单播仿真测试 |
| `CMakeLists.txt` | 将 `traffic.cpp`、`engine.cpp` 接入 `sabr_simulation` |

### 设计决策

- **SimEngine 采用“事件驱动 + 当前节点重规划”最小闭环**
	- `BUNDLE_CREATED` 和 `BUNDLE_ARRIVED` 都统一进入 `process_bundle()`
	- 每到一个节点就重新构造 `RoutingContext`，调用 `CGRRouter::route_unicast()` 做当前节点视角的无副作用规划
	- 只有在确定进入发送队列时，才调用 `commit_route()` 扣减容量，保持 2D 确立的规划/提交边界

- **发送队列优先级和 Phase2 backlog 共享同一份运行时事实**
	- BPA 的内部 `outbound_queues_` 保存 `shared_ptr<Bundle>`，供引擎按 Contact 和优先级出队
	- 同时把等待发送的 Bundle 镜像写入 `Node.send_queues`
	- 这样 `Phase2::compute_eto()` 可以直接复用现有 backlog 语义，而不需要在 3B 重新发明一套并行队列模型

- **同时间戳优先级竞争通过“延迟到 TX_START 再选包”解决**
	- Contact 上第一次发现有待发 Bundle 时，只注册一个占位的 `BUNDLE_TX_START`
	- 真正到 `TX_START` 事件处理时，才从 BPA 队列里选择当前最高优先级 Bundle
	- 这样同一逻辑时刻新到达或新创建的高优先级 Bundle 仍可在首个发送机会前插队，满足 FR-BPA-02

### 测试覆盖

本轮新增 3 个测试用例，总测试数从 108 增长到 111：

- `TrafficGeneratorTest.MaterializesSingleBatchAndPeriodicPatterns`
- `SimEngineTest.ProcessesContactBoundaryEvents`
- `SimEngineTest.DeliversFiveBundlesAcrossThreeNodeScenario`

覆盖场景包括：

- 单次、批量、周期三种流量模式的 Bundle 实例化与时间排序
- Contact start/end 事件的初始化注册与调度执行
- 3 节点 / 3 Contact / 5 Bundle 小场景中的两跳单播转发
- 按优先级发送队列的抢占语义：后创建的 EXPEDITED Bundle 早于先创建的 BULK Bundle 交付
- 逐跳重规划计数：两跳交付场景下每个 Bundle 触发两次路由决策

### 验收结果

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **111/111 测试通过**

### 阶段结论

阶段 3B 已达到当前开发计划中的交付目标：

- 已具备最小可运行的单播仿真引擎与流量生成器
- 已把 `route_unicast()`、`commit_route()`、发送队列、传输延迟和传播延迟接入真实事件流
- 已建立可用于 3C 故障注入和 4B 统计接线的运行时骨架

后续可进入阶段 3C：故障注入与动态计划更新。

---

## 阶段 3C：故障注入与动态计划更新

**日期**：2026-04-21  
**状态**：✅ 已完成

### 完成任务

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 3C.1 | 新增 `FailureInjector`，支持显式 Contact 集失效、按概率抽样失效和整份 ContactPlan 替换 | ✅ | 实验方案 4.3(5), 6.4, 6.5 |
| 3C.2 | 为事件模型新增 `PLAN_UPDATED` 事件，并在引擎中接入运行时计划更新处理 | ✅ | FR-RP-01, DS-CP-04 |
| 3C.3 | 实现 BPA 队列按 Contact / 全量排空、受影响 Bundle 重路由与 stale 事件忽略 | ✅ | FR-RR-01, FR-RP-01 |
| 3C.4 | 将运行时容量提交收窄为“仅提交实际发送的首跳 Contact” | ✅ | FR-P3-05, FR-RR-01 |
| 3C.5 | 新增阶段 3C 测试并接入构建系统 | ✅ | FR-RR-01, FR-RP-01 |

### 新增文件

| 文件 | 说明 |
|---|---|
| `include/simulation/failure_injector.hpp` | `FailureRule`、`FailureRuleMode` 与 `FailureInjector` 接口 |
| `src/simulation/failure_injector.cpp` | 故障规则确定性物化与固定随机种子重放实现 |
| `tests/test_failure_injection.cpp` | 阶段 3C 测试：固定 seed、显式失效、整计划替换、首跳提交回归 |

### 变更文件

| 文件 | 说明 |
|---|---|
| `include/simulation/event.hpp` | 新增 `EventType::PLAN_UPDATED`、计划更新载荷，以及 Contact/Tx 事件的 `plan_version` 字段 |
| `src/simulation/event.cpp` | 补充 `PLAN_UPDATED` 的事件名映射 |
| `include/simulation/runtime_context.hpp` | 为 `BundleProtocolAgent` 增加按 Contact 枚举与批量排空接口 |
| `src/simulation/runtime_context.cpp` | 实现 BPA 队列排空，并确保 `Node.send_queues` 同步清理 |
| `include/simulation/engine.hpp` | 扩展 `EngineConfig`、`EngineMetrics` 和计划更新相关引擎接口 |
| `src/simulation/engine.cpp` | 实现运行时计划更新、排队 Bundle 重路由、stale 事件处理和首跳提交 |
| `CMakeLists.txt` | 将 `failure_injector.cpp` 与 `test_failure_injection.cpp` 接入构建 |

### 设计决策

- **FailureInjector 只做“规则物化”，不直接改运行时状态**
	- 失效规则先在初始化阶段被确定性展开为 `PlanUpdateData`
	- 引擎只消费统一的 `PLAN_UPDATED` 事件，便于固定 seed 重放和后续 4A 配置接入

- **运行时计划变化统一走“排空队列 -> 更新计划 -> 重绑节点 -> 重路由”链路**
	- 显式移除 Contact 时，仅排空受影响 Contact 的 BPA 队列
	- 整份 ContactPlan 替换时，先排空全部 BPA 队列，再补注册新计划的未来 Contact 边界事件
	- 旧计划遗留事件不做删除，而是在处理阶段按 `contact_id + plan_version` 判定为 stale 并忽略

- **已开始传输不中断，但需要单独保存 in-flight 上下文**
	- `BUNDLE_TX_START` 成功后，记录发送节点、接收节点、`plan_version` 与 `owlt`
	- 即使 Contact 在 `TX_END` 前被移除，已开始的传输仍按启动时的上下文完成交付

- **3C 顺手修正了 3B 的提交粒度问题**
	- 引擎不再在源节点预扣整条路径的下游容量
	- 当前实现只对“本次实际发送的首跳 Contact”调用 `commit_route()`
	- 这样逐跳重算和计划更新后的重路由都不会产生下游 Contact 的虚假预扣

- **整计划替换必须显式清空节点路由缓存，不能只依赖版本数碰巧变化**
	- 新旧 `ContactPlan` 可能拥有相同的本地 `version()` 数值
	- 因此 3C 在重绑计划时会主动 `invalidate_routes()`，并在必要时推进 replacement plan 的版本，避免复用旧缓存

### 测试覆盖

本轮新增 4 个测试用例，总测试数从 111 增长到 115：

- `FailureInjectorTest.MaterializesDeterministicallyForFixedSeed`
- `SimEngineFailureInjectionTest.ExplicitContactRemovalReroutesQueuedBundle`
- `SimEngineFailureInjectionTest.PlanReplacementReroutesAndIgnoresStaleEvents`
- `SimEngineFailureInjectionTest.CommitsOnlyFirstHopCapacityBeforeNextHopRouting`

覆盖场景包括：

- 相同随机种子下的概率型失效规则物化结果可稳定重放，不同种子可产生不同失效集合
- 首跳 Contact 在 `TX_START` 前被移除时，排队 Bundle 会从 BPA 队列排空并在当前节点重新规划替代路径
- 整份 ContactPlan 替换后，新计划的 Contact 事件生效，旧计划遗留事件被安全忽略
- 运行时只提交首跳容量，不会在下一跳重新规划前预扣未实际发送的下游 Contact

### 验收结果

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **115/115 测试通过**

### 阶段结论

阶段 3C 已达到当前开发计划中的交付目标：

- 已具备可复现的故障注入、Contact 失效与整计划替换能力
- 已具备 `plan_version` 传播、节点缓存失效和排队 Bundle 重路由闭环
- 已把“已开始传输不中断、未开始发送可重路由”的运行时边界落到真实代码和测试中
- 已为后续 4A 配置接入和 4B 统计扩展提供稳定的故障事件骨架

后续可进入阶段 4A：解析器与实验配置。

## 阶段 4A：解析器与实验配置

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 4A.1 | 实现 ION / JSON Contact Plan 解析，并在导入时自动分配缺失的 `contact_id` | ✅ | IO-IN-01 ~ IO-IN-03 |
| 4A.2 | 实现 JSON 场景配置解析，覆盖仿真参数、流量、多播组、故障注入、冗余模式和实验矩阵 | ✅ | IO-IN-01, IO-IN-04, IO-IN-05 |
| 4A.3 | 保证导入 ContactPlan 时正确初始化 MTV、`plan_version` 和固定随机种子配置 | ✅ | DS-CON-03, DS-CP-03 |
| 4A.4 | 为实验一到实验六补齐标准配置模板和参数矩阵 | ✅ | 实验方案 4.2, 4.3, 6.1 ~ 6.6 |
| 4A.5 | 新增解析正确性、配置校验、模板回归和错误处理测试 | ✅ | VT-UT-01 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `CMakeLists.txt` | 将 `sabr_io` 从占位库改为真实静态库，并把 `tests/test_io.cpp` 接入测试目标 |
| `include/io/parser.hpp` | 新增 parser 公共接口与 4A 配置模型：`ScenarioConfig`、`ExperimentMatrixConfig`、`RedundancyConfig`、`MulticastGroupConfig` 等 |
| `src/io/parser.cpp` | 新增 ION / JSON Contact Plan 解析、JSON 场景解析、实验矩阵解析、路径解析和配置校验逻辑 |
| `include/simulation/traffic.hpp` | 为 `TrafficPattern` 增加 `is_multicast` 与 `multicast_group_id` |
| `src/simulation/traffic.cpp` | 生成 Bundle 时透传 `is_multicast` 配置 |
| `tests/test_io.cpp` | 新增 4A 单测与仓库模板解析回归测试 |
| `configs/experiments/**` | 新增实验一到实验六模板、场景文件、参数矩阵和 Contact Plan 样例 |

### 实现说明

- **解析器接口与配置模型一次性落地**
	- 新增 `ContactPlanParser`、`IONParser`、`JSONParser`、`ConfigParser`、`ParserFactory`
	- `ScenarioConfig` 直接复用现有运行时结构：`EngineConfig`、`TrafficPattern`、`FailureRule`、`ContactPlan`

- **离线构建环境直接复用仓库内 JSON 单头文件**
	- 初版尝试用 `FetchContent` 拉取 `nlohmann_json`，但当前工作区处于离线配置模式，生成阶段无法下载依赖
	- 最终改为直接复用仓库自带的 `include/json.hpp`，避免 4A 引入新的网络依赖

- **Contact Plan 导入保持模型层语义一致**
	- 解析完成后统一通过 `ContactPlan::set_contacts()` / `set_ranges()` 构造计划
	- 这样 `contact_id` 自动补齐、MTV 初始化、`plan_version` 绑定与 Range 重建逻辑都沿用现有模型层契约

- **场景配置支持相对路径与强校验**
	- `contact_plan_file`、`replacement_plan_file`、`output_directory`、`scenario_files` 都按配置文件所在目录解析相对路径
	- 场景文件强制要求二选一：内联 `contact_plan` 或 `contact_plan_file`
	- 多播流量要求 `multicast_group_id` 必须存在且能在 `multicast_groups` 中解析到

- **故障与实验配置已提前为后续阶段接线**
	- 已支持 `CONTACT_SET`、`PROBABILISTIC_CONTACTS`、`PLAN_REPLACEMENT` 三类失败规则解析
	- 已支持 `NONE` / `SINGLE_BACKUP` / `MULTI_BACKUP` / `TRUNK_ONLY` 冗余模式配置解析
	- 已为实验一到实验六分别提供标准场景模板与参数矩阵，后续 8A 只需实现“矩阵应用 + 批量运行”即可复用

### 测试覆盖

本轮新增 9 个测试用例，总测试数从 115 增长到 124：

- `ParserTest.ParsesIonContactPlanAndAssignsSequentialContactIds`
- `ParserTest.ParsesJsonContactPlanWithFieldAliasesAndAutoIds`
- `ParserTest.ParsesInlineScenarioConfigIntoRuntimeTypes`
- `ParserTest.ResolvesRelativeContactPlanAndReplacementPlanFiles`
- `ParserTest.ParsesStandaloneExperimentMatrixFile`
- `ParserTest.RejectsUnknownMulticastGroupInTrafficConfig`
- `ParserTest.RejectsInvalidIonDefinition`
- `ParserTest.RejectsMissingReplacementPlanInPlanReplacementRule`
- `ParserTemplateTest.ParsesRepositoryExperimentTemplates`

覆盖场景包括：

- ION Contact / Range 导入与自动 `contact_id` 分配
- JSON Contact Plan 字段别名兼容
- 场景配置到运行时对象的完整映射
- `contact_plan_file` 与 `replacement_plan_file` 的相对路径解析
- 实验矩阵值类型解析
- 未知多播组、非法 ION 行、缺失 replacement plan 等错误路径
- 仓库内 6 组实验模板和 6 个矩阵文件的持续可解析性

### 验收结果

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：

- 配置成功
- 编译通过
- **124/124 测试通过**

### 阶段结论

阶段 4A 已达到当前开发计划中的交付目标：

- 已具备 ION / JSON Contact Plan 与 JSON 场景 / 矩阵配置解析能力
- 已把多播、故障注入和冗余相关配置在 IO 层全部接线完成
- 已为实验一到实验六补齐标准模板，并用自动化测试持续校验模板有效性
- 已消除 4A 对外部网络依赖的构建风险，当前命令行构建路径可稳定复现

后续可进入阶段 4B：日志、统计与结果导出。

---

## 阶段 4B：日志、统计与结果导出

**日期**：2026-04-24  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 4B.1 | 为 CGR 增加可观察 trace 结果，保留 Phase 1 / 2 / 3 过程数据与拒绝原因 | ✅ | IO-OUT-01 |
| 4B.2 | 实现 `Logger`，记录事件、路由决策、commit 与计划更新 | ✅ | IO-OUT-01 |
| 4B.3 | 实现 `SimStats`，聚合 Bundle / Contact / Routing / PlanUpdate 统计 | ✅ | IO-OUT-02, IO-OUT-03 |
| 4B.4 | 实现 JSON / CSV 导出，并定义机器可读结果结构 | ✅ | IO-OUT-04 |
| 4B.5 | 将 Logger / Stats 接入 `SimEngine`，并补充统计与 trace 回归测试 | ✅ | IO-OUT-01 ~ IO-OUT-04 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `include/io/logger.hpp` | 新增结构化日志记录模型：事件、路由、commit、计划更新 |
| `src/io/logger.cpp` | 新增路由 trace / commit / 计划更新序列化与记录逻辑 |
| `include/simulation/stats.hpp` | 新增 `SimStats` 统计记录模型与导出接口 |
| `src/simulation/stats.cpp` | 新增 Bundle / Contact / Routing 聚合、JSON / CSV 导出实现 |
| `include/algorithms/cgr.hpp` | 新增 `RoutingAttemptTrace`、`RoutingTrace` 与 `route_unicast_with_trace()` 接口 |
| `src/algorithms/cgr.cpp` | 将 CGR 单播编排扩展为可返回完整 trace，并保留失败原因 |
| `include/algorithms/phase2.hpp` | 为 Phase2 结果新增 `evaluated_routes`，保留被拒绝候选 |
| `src/algorithms/phase2.cpp` | 在验证阶段同时输出全部候选与有效候选，供日志 / 统计消费 |
| `include/simulation/engine.hpp` | 为引擎新增 `logger()` / `stats()` 访问器与内部持有对象 |
| `src/simulation/engine.cpp` | 把事件、路由、commit、计划更新、stale 事件与重路由计数接入 Logger / Stats |
| `tests/test_stats.cpp` | 新增 4B 统计导出与计划更新指标测试 |
| `tests/test_cgr.cpp` | 新增 route trace 回归测试 |
| `CMakeLists.txt` | 将 logger / stats 源文件与 `test_stats.cpp` 接入构建 |

### 实现说明

- **4B 先补“数据源”，再接日志与统计**
	- 仅靠原有 `RoutingDecision` 无法还原 Phase1 / Phase2 / Phase3 过程，因此先在 `CGRRouter` 上新增 `route_unicast_with_trace()`
	- 同时让 `Phase2::Result` 保留 `evaluated_routes`，避免无效候选在验证后被直接丢弃，保证 reject reason 可记录

- **Logger 与 SimStats 分层明确**
	- `Logger` 负责保留结构化原始记录，便于后续调试、导出和实验追溯
	- `SimStats` 负责把运行过程聚合成 Bundle、Contact、Routing 和 PlanUpdate 维度的统计摘要
	- 两者都由 `SimEngine` 驱动，但各自保持单一职责，避免把统计逻辑塞进算法层

- **引擎运行时事件已完成 4B 接线**
	- Bundle created / arrived / forwarded / delivered / expired
	- Contact start / end
	- 路由尝试、首跳 commit、计划更新、排队 Bundle 重路由
	- stale Contact 事件忽略与重复副本丢弃计数

- **导出格式以实验后处理为目标设计**
	- `write_json()` 输出 metadata、summary、bundles、contacts、routing、plan_updates
	- `write_bundle_summary_csv()` 输出 Bundle 级摘要表
	- `write_contact_utilization_csv()` 输出 Contact 利用率表

- **与后续阶段的边界保持清晰**
	- 4B 已提供局部修复和计划更新可观测性
	- 冗余相关字段目前仅作为统计骨架预留，完整冗余行为与收益计算仍在后续 7B 实现

### 测试覆盖

本轮新增 4 个测试用例，总测试数从 124 增长到 128：

- `SimStatsTest.CapturesBundleContactAndRoutingSummaries`
- `SimStatsTest.CapturesPlanUpdateAndStaleEventMetrics`
- `StatsExportTest.ExportsJsonAndCsvFiles`
- `CGRRouterTest.RouteUnicastWithTraceCapturesRecomputeAttempts`

覆盖场景包括：

- 三节点单播场景下的 Bundle 交付、跳数、逐跳发送、Contact 利用率与 routing invocation 聚合
- JSON / CSV 导出文件生成与字段结构校验
- Contact 删除导致的排队 Bundle 重路由、计划更新记录与 stale 事件忽略
- CGR trace 中对补算轮次、Phase1 路由集、Phase2 候选集与 reject reason 的完整保留

### 验收结果

```bash
cmake --build build --target sabr_tests
ctest --test-dir build --output-on-failure -R "SimStatsTest|StatsExportTest|SimEngineTest|SimEngineFailureInjectionTest|CGRRouterTest\.RouteUnicastWithTraceCapturesRecomputeAttempts"
ctest --test-dir build --output-on-failure
```

结果：

- 编译通过
- 聚焦回归通过
- **128/128 测试通过**

### 阶段结论

阶段 4B 已达到当前开发计划中的交付目标：

- 已具备结构化事件 / 路由 / commit / 计划更新日志
- 已具备 Bundle / Contact / Routing / PlanUpdate 聚合统计
- 已具备 JSON / CSV 结果导出能力
- 已把 3C 的计划更新与局部修复路径纳入可观测范围
- 已为后续 7B 冗余统计保留扩展位，但未提前实现尚未落地的冗余行为

后续可进入阶段 5：单播集成与标准符合性。

---

## 阶段 5：单播集成与标准符合性

**日期**：2026-04-25  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 5.1 | 复用 `exp1_unicast_correctness` 五节点拓扑作为 REF-2 等价参考基线，并新增阶段 5 专用 reference / downlink 场景配置 | ✅ | VT-IT-01 |
| 5.2 | 新增五节点 reference 场景端到端单播集成测试 | ✅ | VT-IT-01 |
| 5.3 | 新增 SA → MCC 下行分流测试，验证前 10 个 Bundle 经 GS2、后 10 个经 GS1，并保留逐跳重算证据 | ✅ | VT-IT-02, FR-RR-01 |
| 5.4 | 新增 PICS-style 覆盖映射测试，串接阶段 2A ~ 2D 与阶段 5 的标准符合性证据 | ✅ | VT-SC-01, VT-SC-02 |
| 5.5 | 新增实验一 / 实验二 smoke case，验证 exp1 / exp2 模板最小可执行 | ✅ | 实验方案 6.1, 6.2 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `configs/experiments/exp1_unicast_correctness/stage5_reference_baseline.json` | 新增阶段 5 五节点 reference 场景，关闭增强开关，供 VT-IT-01 使用 |
| `configs/experiments/exp1_unicast_correctness/stage5_downlink_split.json` | 新增阶段 5 下行分流场景，使用双 batch 流量复现前 10 / 后 10 首跳分流 |
| `tests/test_reference_scenario.cpp` | 新增五节点 reference 场景测试、下行分流测试与 PICS-style 覆盖映射 |
| `tests/test_experiment_configs.cpp` | 新增 exp1 / exp2 baseline 与 matrix 的 smoke case |
| `CMakeLists.txt` | 将阶段 5 新增测试文件接入 `sabr_tests` |

### 实现说明

- **阶段 5 复用现有 exp1 五节点拓扑，不再复制新的 REF-2 fixture**
	- 现有 `configs/experiments/exp1_unicast_correctness/contact_plan.ion` 已具备 SA、GS2、GS1、Orbiter、MCC 五节点拓扑
	- 阶段 5 直接在该目录下补充 `stage5_reference_baseline.json` 与 `stage5_downlink_split.json`
	- 这样可以把工作重点放在集成回归和标准追踪，而不是重复造场景数据

- **VT-IT-01 聚焦 reference 场景的端到端闭环**
	- 使用五节点 reference baseline 场景驱动 `ConfigParser -> SimEngine -> Logger -> SimStats` 全链路
	- 验证 Bundle 全部交付、无 route failure、逐跳重算计数、路由日志节点覆盖，以及 Bundle 级统计摘要

- **VT-IT-02 聚焦“首跳分流 + 逐跳重算”，而不是把它混成另一个端到端成功率测试**
	- 通过双 batch 流量场景复现“前 10 个 Bundle 经 GS2、后 10 个经 GS1”的源节点首跳选择
	- 当前场景的结果为：前 10 个 Bundle 走 `1 -> 2 -> 5` 并交付，后 10 个 Bundle 走 `1 -> 3` 并在 TTL 后过期
	- 该场景仍满足阶段 5 关心的核心事实：无增强条件下的 10/10 首跳分流、每个 Bundle 两次 routing attempt、以及日志 / 统计层对节点 1、2、3 的完整记录

- **VT-SC-01 / VT-SC-02 采用 PICS-style 覆盖映射落地**
	- `tests/test_reference_scenario.cpp` 不重复实现阶段 2A ~ 2D 的局部算法测试
	- 取而代之的是把 `test_dijkstra.cpp`、`test_phase1.cpp`、`test_phase2.cpp`、`test_cgr.cpp` 与阶段 5 新增测试串成一张标准覆盖表
	- 这样能把标准条款与仓库中的自动化证据稳定绑定，避免出现重复而脆弱的测试实现

- **实验一 / 实验二 smoke case 直接复用现有模板**
	- `tests/test_experiment_configs.cpp` 读取 exp1 / exp2 的 `baseline.json` 与 `matrix.json`
	- 验证场景解析、矩阵维度、关键 traffic / simulation 参数和相对路径解析均可驱动后续实验编排

### 测试覆盖

本轮新增 8 个测试用例，总测试数从 128 增长到 136：

- `ReferenceScenarioTest.LoadsExp1BaselineAsRef2Equivalent`
- `ReferenceScenarioTest.VT_IT_01_EndToEndUnicastOnFiveNodeScenario`
- `ReferenceScenarioTest.VT_IT_02_DownlinkFirstTenViaGs2LastTenViaGs1`
- `ReferenceScenarioTest.VT_SC_01_And_VT_SC_02_PicsCoverageMapIsSatisfied`
- `ExperimentConfigTest.Exp1BaselineScenarioIsExecutable`
- `ExperimentConfigTest.Exp1MatrixDimensionsMatchStageGoal`
- `ExperimentConfigTest.Exp2BaselineScenarioIsExecutable`
- `ExperimentConfigTest.Exp2MatrixDimensionsMatchScaleExperiment`

覆盖场景包括：

- 五节点 reference baseline 的场景加载、角色映射与端到端单播闭环
- SA → MCC 下行链路在无增强条件下的前 10 / 后 10 首跳分流
- 每个 Bundle 的两次 routing attempt 与日志 / 统计节点覆盖
- REF-1 / PICS 必选项与阶段 2A ~ 2D 既有测试的覆盖映射
- exp1 / exp2 baseline 与 matrix 的最小可执行 smoke case

### 验收结果

```bash
cmake --build build --target sabr_tests
ctest --test-dir build --output-on-failure -R "ReferenceScenarioTest|ExperimentConfigTest|ParserTemplateTest|SimEngineTest|CGRRouterTest"
ctest --test-dir build --output-on-failure
```

结果：

- 编译通过
- 聚焦回归通过
- **136/136 测试通过**

### 阶段结论

阶段 5 已达到当前开发计划中的交付目标：

- 已具备五节点 reference 场景端到端单播集成回归
- 已具备 SA → MCC 下行首跳分流与逐跳重算证据
- 已具备 PICS-style 标准覆盖映射
- 已把 exp1 / exp2 模板提升为最小可执行 smoke case

后续可进入阶段 6A：多播传播计划构造。

## 阶段 6A：多播传播计划构造

**日期**：2026-04-26  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 6A.1 | 实现 `MulticastGroupRegistry`，提供 group 的增删改查与批量替换能力 | ✅ | FR-MC-01, FR-MC-02 |
| 6A.2 | 为 CGR 补入 `select_best_unicast()`，提供无 flood、无 commit 的最佳单播复用接口 | ✅ | FR-MT-01 |
| 6A.3 | 实现 `MulticastPlanner`，将多个目的节点的最佳单播路由合并为共享前缀传播树 | ✅ | FR-MT-02, FR-MT-03, FR-MF-02 |
| 6A.4 | 新增构树单元测试，验证共享前缀合并结构正确且构树阶段不修改 MTV | ✅ | VT-UT-06, VT-UT-07 |
| 6A.5 | 将实验三 `tree_plan` / `split_unicast_control` / `matrix` 纳入自动化 smoke case | ✅ | 实验方案 6.3 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `include/algorithms/cgr.hpp` | 新增 `BestRouteResult` 与 `select_best_unicast()`，为多播构树暴露最佳单播复用接口 |
| `src/algorithms/cgr.cpp` | 抽出共享的候选收集逻辑，复用到 trace 路由与最佳单播 helper |
| `include/algorithms/multicast.hpp` | 新增 `MulticastGroup`、`MulticastGroupRegistry`、`MulticastPlanRequest`、`MulticastPlanResult`、`MulticastPlanner` |
| `src/algorithms/multicast.cpp` | 实现 group 校验、批量替换、group lookup、共享前缀 route merge 与 group-based planning |
| `tests/test_cgr.cpp` | 新增最佳单播 helper 的无副作用与 critical bundle 单播语义测试 |
| `tests/test_multicast.cpp` | 新增 group 管理、共享前缀构树、MTV 不变性与 exp3 配置 smoke case |
| `CMakeLists.txt` | 将 `src/algorithms/multicast.cpp` 与 `tests/test_multicast.cpp` 接入构建系统 |

### 实现说明

- **先补稳定的单播复用接口，再做多播构树**
	- 原有 `route_unicast()` 在 critical bundle 上会进入 flood 语义，不适合作为多播构树的基础接口
	- 本轮先在 `CGRRouter` 中新增 `select_best_unicast()`，内部直接复用 `plan_unicast()`、`evaluate_unicast()` 和 `Phase3Selector::select_best()`
	- 这样多播构树可以稳定拿到“最佳单播 CandidateRoute”，同时继续保持“不提交 MTV”的语义边界

- **`MulticastPlanner` 只负责传播计划生成，不接入执行层**
	- `MulticastPlanner::build_plan()` 逐个目的节点调用 `select_best_unicast()`
	- 然后按 `contact_id + next_hop` 把多条最佳单播路径合并到 `MulticastTree`
	- 共享前缀只扩展目的节点子集，不重复创建分支；若同一 `next_hop` 出现不同 `contact_id`，则显式返回 `conflicting_branch_contact`
	- 本阶段不生成副本、不消费 `SimEngine`、不提交容量，保持 6A 与 6B 的职责边界清晰

- **group 管理与 exp3 基线同步落地**
	- `MulticastGroupRegistry` 支持 add / update / remove / find / replace_groups 五类操作
	- group 校验覆盖：空 group_id、非法 source、空 member 列表、source 出现在成员中、非法 member 节点
	- 现有 `configs/experiments/exp3_multicast_plan` 目录被直接纳入自动化测试，不再新建一套实验三模板

### 测试覆盖

本轮新增 8 个测试用例，总测试数从 136 增长到 144：

- `CGRRouterTest.SelectBestUnicastReturnsPrimaryCandidateWithoutConsumingMtv`
- `CGRRouterTest.SelectBestUnicastKeepsSingleRouteSemanticsForCriticalBundle`
- `MulticastGroupRegistryTest.AddUpdateRemoveAndReplaceGroups`
- `MulticastPlannerTest.BuildPlanMergesSharedPrefixWithoutConsumingMtv`
- `MulticastPlannerTest.BuildPlanForGroupUsesRegistryAndTracksUnreachableDestinations`
- `MulticastExperimentConfigTest.Exp3TreePlanScenarioIsExecutable`
- `MulticastExperimentConfigTest.Exp3SplitUnicastControlScenarioIsExecutable`
- `MulticastExperimentConfigTest.Exp3MatrixDimensionsMatchPlanExperiment`

覆盖场景包括：

- 最佳单播 helper 在 normal / critical bundle 上都保持单播选择，不触发 flood
- group registry 的增删改查、批量替换和非法输入拒绝
- 三个目的节点共享前缀的传播树合并结果
- 构树前后 Contact MTV 完全一致
- group-based planning 的部分不可达目的节点追踪
- exp3 的 `tree_plan` / `split_unicast_control` / `matrix` 配置持续可解析

### 验收结果

```bash
cmake --build build --target sabr_tests
ctest --test-dir build --output-on-failure -R "(CGRRouterTest|Phase3SelectorTest)"
ctest --test-dir build --output-on-failure -R "(MulticastGroupRegistryTest|MulticastPlannerTest|MulticastExperimentConfigTest|MulticastTreeTest)"
ctest --test-dir build --output-on-failure
```

结果：

- 编译通过
- CGR 聚焦回归通过
- multicast 聚焦回归通过
- **144/144 测试通过**

### 阶段结论

阶段 6A 已达到当前开发计划中的交付目标：

- 已具备独立的 `MulticastGroup` 管理能力
- 已具备可复用的最佳单播无副作用规划接口
- 已具备把多个单播最佳路由合并为共享前缀传播树的能力
- 已用自动化测试证明构树阶段不会修改 MTV
- 已把实验三 tree / split control 配置提升为最小可执行 smoke case

后续可进入阶段 6B：多播执行与局部修复。

## 阶段 6B：多播执行与局部修复

**日期**：2026-04-26  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 6B.1 | 实现源节点按传播计划第一层分支初始化多播副本，并让排队副本携带“当前节点为根”的裁剪子树 | ✅ | FR-MF-01, FR-MF-03, FR-MF-04 |
| 6B.2 | 实现中间节点读取局部子树并继续分叉转发；补齐“当前节点既本地交付又继续转发”的非终态语义 | ✅ | FR-MF-05 |
| 6B.3 | 实现 contact 可用性检查与受影响目的子集的局部 repair | ✅ | FR-MR-01, FR-MR-02 |
| 6B.4 | 锁定共享前缀只按实际副本数 commit 一次容量 | ✅ | FR-MF-07, VT-UT-08 |
| 6B.5 | 把 lineage、局部 repair 计数和 exp3 / exp4 场景测试接入自动化 | ✅ | DS-BDL-05, VT-IT-06, VT-IT-07, VT-IT-08, 实验方案 6.3, 6.4 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `include/simulation/traffic.hpp` | 为 `ScheduledBundle` 增加 `multicast_group_id`，把多播组上下文带入运行时 |
| `src/simulation/traffic.cpp` | 在物化流量时透传 `multicast_group_id` 到创建事件链路 |
| `include/simulation/event.hpp` | 为 `BundleCreatedData` 增加 `multicast_group_id`，使源节点创建时可解析 group |
| `include/simulation/engine.hpp` | 新增 multicast registry setter、多播初始化 / 执行 / repair / contact 校验接口 |
| `src/simulation/engine.cpp` | 实现 source / intermediate 执行、非终态本地交付、局部 repair、计划更新后健康队列重调度 |
| `include/simulation/stats.hpp` | 新增 `note_local_repair()` 接口，接通 repair 计数 |
| `src/simulation/stats.cpp` | 落地 `local_repair_count` 统计更新 |
| `src/algorithms/multicast.cpp` | 新增 `plan_forwarding()`，负责当前节点的分叉复制、子树裁剪与 lineage 保留 |
| `tests/test_multicast.cpp` | 新增 6B 的执行、容量、repair、lineage 与 exp3 / exp4 场景测试 |
| `configs/experiments/exp4_multicast_repair/*` | 将实验四模板校准为可执行的局部 repair 基线 |

### 实现说明

- **多播组上下文正式接入 runtime**
	- `SimEngine` 新增 `set_multicast_groups()` / `set_multicast_registry()`，不再让 `multicast_groups` 只停留在 parser 层
	- `TrafficGenerator` 和 `BundleCreatedData` 现在会把 `multicast_group_id` 传到源节点创建路径
	- 源节点在 `BUNDLE_CREATED` 时即可用当前 contact plan + group registry 生成完整传播树，并填充 `pending_destinations`、`multicast_plan`、`encoded_plan_version`

- **分叉执行直接消费传播计划，不回退到单播 route_unicast**
	- `MulticastPlanner::plan_forwarding()` 现在负责把“当前节点”展开成若干 `contact_id + next_hop + pruned subtree` dispatch
	- 每个 dispatch 都保留“当前节点为根”的裁剪子树，这样队列排空后仍能在当前节点做 repair，而不会丢失上下文
	- `SimEngine` 对 multicast bundle 走独立执行路径：按 dispatch 直接 enqueue 到 contact，并只在真实发送的副本上 commit 首跳容量

- **局部 repair 与 plan_version 切换闭环已落地**
	- 若 dispatch 指向的 `contact_id` 已失效、已终止或 plan_version 与 `encoded_plan_version` 不一致，则 engine 只对该 dispatch 负责的目的子集重新构树
	- 当计划更新导致未受影响的排队 contact 也拥有旧 `plan_version` 的 TX_START 事件时，engine 会在 `on_plan_updated()` 末尾统一重调度健康队列，避免它们被 stale 事件卡死
	- `SimStats` 已接通 `local_repair_count`，event log 中会记录 `LOCAL_REPAIR bundle=... node=...`

- **非终态本地交付语义已进入运行时**
	- 若当前节点既是目的节点又仍有后续子分支，engine 现在会在当前节点执行本地交付并继续 forward children
	- 该语义不会把当前副本提前终结，而是通过 `delivered_destinations` 传播到后续子副本

### 测试覆盖

本轮新增 10 个测试用例，总测试数从 144 增长到 154：

- `MulticastPlannerTest.PlanForwardingPrunesSubtreesAndAssignsReplicaLineage`
- `MulticastPlannerTest.PlanForwardingMarksLocalDeliveryAndCarriesDeliveredSetIntoChildren`
- `MulticastExecutionTest.EngineExecutesSharedPrefixTreeEndToEnd`
- `MulticastExecutionTest.SharedPrefixCapacityIsCommittedOnlyOncePerActualCopy`
- `MulticastExecutionTest.IntermediateDestinationIsDeliveredAndStillForwardsChildren`
- `MulticastRepairTest.EngineRepairsAffectedSubsetAfterContactRemoval`
- `MulticastScenarioTest.Exp3TreePlanScenarioRunsEndToEnd`
- `MulticastScenarioTest.Exp4RepairScenarioExecutesLocalRepairPath`
- `MulticastExperimentConfigTest.Exp4RepairScenarioIsExecutable`
- `MulticastExperimentConfigTest.Exp4MatrixDimensionsMatchRepairExperiment`

覆盖场景包括：

- 当前节点分叉复制、子树裁剪与副本 lineage 保持
- source / intermediate 节点的多播执行闭环
- 共享前缀 Contact 仅 commit 一次容量
- 当前节点为目的节点时的非终态本地交付
- Contact 失效后仅对受影响目的子集做局部 repair
- exp3 tree_plan 端到端执行和 exp4 repair 场景执行
- exp4 baseline 与 matrix 模板持续可解析

### 验收结果

```bash
cmake --build build --target sabr_tests
ctest --test-dir build --output-on-failure -R "Multicast|SimEngineFailureInjectionTest|ExperimentConfigTest|ParserTemplateTest|CGRRouterTest"
ctest --test-dir build --output-on-failure
```

结果：

- 编译通过
- 6B 聚焦回归通过
- **154/154 测试通过**

### 阶段结论

阶段 6B 已达到当前开发计划中的交付目标：

- 已具备 source / intermediate 多播执行能力
- 已具备共享前缀单次容量提交语义
- 已具备基于受影响目的子集的局部 repair 闭环
- 已具备多播副本 lineage 追踪与非终态本地交付语义
- 已把实验三和实验四提升为可执行并受自动化保护的场景基线

后续可进入阶段 7A：增强功能收口。

## 阶段 7A：增强功能收口

**日期**：2026-04-26  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 7A.1 | 补齐 `anti_loop_reactive` 配置链，把 enhancement 开关稳定映射进 `Phase2::Config` | ✅ | FR-ENH-01, FR-ENH-03 |
| 7A.2 | 在仿真运行时接通 Queue-Delay 所需的 `allocated_bytes` 上下文 | ✅ | FR-ENH-02 |
| 7A.3 | 在 `SimEngine` 单播执行路径落地 reactive Anti-Loop 排除 / 重选，并把 reroute 原因写入日志与统计 | ✅ | FR-ENH-03 |
| 7A.4 | 用集成测试校准 proactive Anti-Loop 与 Phase3 non-looping preference 的组合行为 | ✅ | FR-ENH-04 |
| 7A.5 | 补齐 VT-IT-03 / VT-IT-04 / VT-IT-05，并把 `exp6_ablation` 扩展到 reactive Anti-Loop 维度 | ✅ | FR-ENH-05, VT-IT-03, VT-IT-04, VT-IT-05 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `include/algorithms/phase2.hpp` | 为 `Phase2::Config` 新增 `anti_loop_reactive` 运行时字段 |
| `src/io/parser.cpp` | 将 `enhancements.anti_loop_reactive` 和 `simulation.phase2.anti_loop_reactive` 映射到运行时配置 |
| `include/algorithms/cgr.hpp` | 为 `RoutingTrace` 增加 reroute reason / count / excluded neighbors 承载字段 |
| `include/io/logger.hpp` | 为 `RoutingRecord` 增加 reactive reroute 可观测字段 |
| `include/simulation/stats.hpp` | 为 `RoutingStatsRecord` 和 `StatsSummary` 增加 reactive Anti-Loop 统计字段 |
| `src/simulation/runtime_context.cpp` | 新增按 `contact_id` 聚合排队字节数的 BPA 接口 |
| `src/simulation/engine.cpp` | 在单播运行时接通 `allocated_bytes`，新增 reactive Anti-Loop 提交前检查与重选回路 |
| `src/io/logger.cpp` | 将 reroute 原因、重试次数和排除邻居写入 routing records |
| `src/simulation/stats.cpp` | 记录 reactive Anti-Loop 触发计数并导出到 JSON |
| `tests/test_reference_scenario.cpp` | 新增 VT-IT-03 / VT-IT-04 / VT-IT-05 |
| `tests/test_stats.cpp` | 新增 reactive Anti-Loop 统计链路测试 |
| `tests/test_experiment_configs.cpp` | 新增 `exp6_ablation` baseline / matrix 的 reactive 开关验证 |
| `configs/experiments/exp6_ablation/matrix.json` | 将 `anti_loop_reactive` 纳入组合矩阵 |

### 实现说明

- **配置链已补齐到运行时**
	- `anti_loop_reactive` 不再停留在 `EnhancementConfig`；parser 现在会把该开关传到 `engine.phase2_config`
	- Queue-Delay、proactive Anti-Loop、ORPN 与 reactive Anti-Loop 现已统一走场景配置到 engine config 的链路

- **Queue-Delay 已真正进入仿真执行面**
	- 本轮实现前，`Phase2::compute_pbat()` 虽有 queue-delay 分支，但 engine 从未填充 `ValidationContext.allocated_bytes`
	- 现在 `BundleProtocolAgent` 可按 `contact_id` 统计排队 volume，`SimEngine` 会在每次路由前构造全局 `allocated_bytes`，使 Queue-Delay 能在集成场景里改变路径选择，而不是只存在于 Phase2 单测

- **reactive Anti-Loop 已作为提交前最后一道保护落地**
	- `SimEngine` 现在会在 `route_unicast_with_trace()` 之后、真正 enqueue/commit 之前检查选中路径是否重入 `visited_nodes`
	- 若命中，则按 entry neighbor 将该邻居临时加入 `excluded_neighbors` 后重选；若无可替代路径，则返回 `reactive_anti_loop_exhausted`
	- reroute reason、reroute count、排除邻居集合会进入 `RoutingTrace`、`Logger`、`SimStats` 和运行时 event log

- **proactive 与 Phase3 的组合行为已被锁定**
	- 保持 `Phase2` 只负责标记 `possibly_looping`，`Phase3` 继续保持“优先非闭环、必要时降级”的现有策略
	- 集成测试已证明：在 proactive 已足够把闭环候选降级时，reactive 不会被多余触发；在 proactive 关闭时，reactive 会接管最后一道保护

- **7A 的 CLI 范围仍保持最小边界**
	- 本轮没有提前实现 8B 的 `run-scenario` / `run-experiment` 子命令体系
	- 7A 只完成 enhancement 配置链的运行时接线与组合验证，完整 CLI 收口仍留给阶段 8B

### 测试覆盖

本轮新增 6 个测试用例，总测试数从 154 增长到 160：

- `ReferenceScenarioTest.VT_IT_03_OneRoutePerNeighborCanBeEnabledIndependently`
- `ReferenceScenarioTest.VT_IT_04_QueueDelayCanBeEnabledIndependently`
- `ReferenceScenarioTest.VT_IT_05_AntiLoopCombinationPrefersNonLoopingBeforeReactiveReroute`
- `SimStatsTest.ReactiveAntiLoopReroutesAndReportsReason`
- `ExperimentConfigTest.Exp6BaselineScenarioIncludesReactiveAntiLoop`
- `ExperimentConfigTest.Exp6MatrixIncludesReactiveAntiLoopDimension`

覆盖场景包括：

- ORPN 在 `recompute_more` 关闭时仍能通过首轮邻居覆盖维持可达性
- Queue-Delay 在存在后台排队 volume 时能改变集成场景的路径选择
- reactive Anti-Loop 的 reroute 原因、次数与排除邻居可被日志和统计面观测
- proactive Anti-Loop 与 Phase3 non-looping preference 的组合不会误触发 reactive reroute
- `exp6_ablation` baseline 和 matrix 现在都可解析 `anti_loop_reactive`

### 验收结果

```bash
cmake --build build --target sabr_tests
.\build\sabr_tests.exe --gtest_filter="ParserTest.ParsesInlineScenarioConfigIntoRuntimeTypes:SimStatsTest.ReactiveAntiLoopReroutesAndReportsReason:ReferenceScenarioTest.VT_IT_03_OneRoutePerNeighborCanBeEnabledIndependently:ReferenceScenarioTest.VT_IT_04_QueueDelayCanBeEnabledIndependently:ReferenceScenarioTest.VT_IT_05_AntiLoopCombinationPrefersNonLoopingBeforeReactiveReroute:ExperimentConfigTest.Exp6BaselineScenarioIncludesReactiveAntiLoop:ExperimentConfigTest.Exp6MatrixIncludesReactiveAntiLoopDimension"
ctest --test-dir build --output-on-failure
```

结果：

- 编译通过
- 7A 聚焦回归通过
- **160/160 测试通过**

### 阶段结论

阶段 7A 已达到当前开发计划中的交付目标：

- 各增强开关已可独立启停，并进入 parser → engine → logger/stats → 集成测试链路
- Queue-Delay 已从“算法层开关”提升为“仿真层可观测行为”
- reactive Anti-Loop 已作为运行时保护与 observability 能力进入基线
- VT-IT-03、VT-IT-04、VT-IT-05 与 `exp6_ablation` 组合验证已纳入自动化

后续可进入阶段 7B：冗余传输与去重。

## 阶段 7B：冗余传输与去重

**日期**：2026-04-26  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 7B.1 | 新增独立 `redundancy` 模块，落实 risk / diversity / capacity / TTL gate 的最小 v1 语义 | ✅ | 实验方案 6.5 |
| 7B.2 | 将 `RedundancyConfig` 从 parser 接入 `EngineConfig` / `RoutingContext`，并让 `CGRRouter` 输出 `backup_routes` 与 `redundancy_considered` | ✅ | 实验方案 6.5, 6.6 |
| 7B.3 | 扩展 `Bundle` 冗余元数据，补齐 `ReplicaRole`、`redundancy_applied`、canonical origin 与 scope helper | ✅ | 实验方案 6.5 |
| 7B.4 | 在 `SimEngine` / `BundleProtocolAgent` 中实现单播备份副本物化、node-local first-arrival wins 去重与 `DUPLICATE_DROPPED` 终态 | ✅ | 实验方案 6.5 |
| 7B.5 | 为多播 shared-prefix trunk 提供最小 `TRUNK_ONLY` backup dispatch，并在目的节点收敛重复副本 | ✅ | 实验方案 6.5, 6.6 |
| 7B.6 | 补齐 redundancy / exp5 / stats / multicast trunk 的自动化测试与全量回归 | ✅ | 实验方案 6.5, 6.6 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `include/algorithms/redundancy.hpp` | 新增 `RedundancyConfig`、`RouteDiversityAnalyzer`、`DeliveryRiskEstimator`、`RedundancyManager` 接口 |
| `src/algorithms/redundancy.cpp` | 实现冗余风险估计、多样性评分和备份路径筛选 |
| `include/algorithms/cgr.hpp` | 为 `RoutingContext` / `RoutingTrace` 增加 redundancy 配置与 backup route 承载面 |
| `src/algorithms/cgr.cpp` | 在主路径选择后插入 redundancy planning，输出 `backup_routes` |
| `include/models/bundle.hpp` | 新增 `ReplicaRole`、`redundancy_applied`、canonical origin / scope helper |
| `src/models/bundle.cpp` | 实现冗余 family helper，并保持 `make_replica()` 复用现有 lineage 语义 |
| `include/simulation/runtime_context.hpp` | 为 BPA 增加 per-node redundancy arrival registry |
| `src/simulation/runtime_context.cpp` | 实现 node-local first-arrival wins 去重登记 |
| `include/simulation/engine.hpp` | 扩展 duplicate lifecycle、冗余 family 状态与 trunk backup helper 声明 |
| `src/simulation/engine.cpp` | 接入单播 backup 物化、duplicate drop、redundancy family 统计与最小 trunk-only dispatch |
| `src/io/logger.cpp` | 将 `redundancy_considered` 接入 routing records |
| `include/simulation/stats.hpp` | 为冗余触发、首达收益、duplicate drop 增加统计接口 |
| `src/simulation/stats.cpp` | 接通 `redundancy_trigger_count`、`redundant_first_hit_count`、`duplicate_replica_discard_count` 与收益比 |
| `tests/test_redundancy.cpp` | 新增 redundancy unit tests 与 router trace backup 测试 |
| `tests/test_stats.cpp` | 新增单播 redundancy 首达 / duplicate drop / summary 测试 |
| `tests/test_multicast.cpp` | 新增最小 `TRUNK_ONLY` trunk redundancy 集成测试 |
| `tests/test_experiment_configs.cpp` | 新增 `exp5_redundancy` baseline / matrix 配置覆盖 |

### 实现说明

- **先固定 redundancy 语义，再接入 runtime**
	- 本轮先把 `RedundancyConfig` 与算法对象从 parser 头文件中抽离成独立模块，避免 `EngineConfig` 与 parser 类型产生循环依赖
	- `RedundancyManager` 采用启发式 v1：primary risk 由 hop_count、TTL slack 与容量 reserve 组合估计；backup 选择则受 diversity / capacity / TTL 三类硬门槛约束

- **CGR 保持主路径语义不变，backup 只在主路径之后补选**
	- `Phase3Selector` 仍只负责 primary route 决策，不改变既有 comparator 和 critical flood 语义
	- `CGRRouter::route_unicast_with_trace()` 现在会在 primary route 选定后，从剩余 valid candidates 中补选 `backup_routes`
	- `redundancy_considered` 与 `backup_routes` 被保留在 `RoutingTrace` 中，供 engine / logger / stats 直接复用

- **单播副本执行采用“只展开一次 + 节点级首达去重”**
	- `Bundle` 新增 `redundancy_applied`，保证同一 bundle family 不会在后续 hop 上继续指数级展开
	- `SimEngine` 在 `FORWARD_ONE` 分支实际物化 backup 副本，复用现有 `make_replica()`、enqueue、tx、arrival 事件链路，不引入新事件类型
	- `BundleProtocolAgent` 在节点接受点用 canonical origin + scope key 做 node-local 判重；后到副本直接走既有 `DUPLICATE_DROPPED` 事件

- **最小 `TRUNK_ONLY` 落在 shared-prefix trunk，而不是 branch-level redundancy**
	- 仅当当前 multicast dispatch 仍承载多个目的节点时，engine 才尝试规划单条替代 trunk dispatch
	- backup trunk 通过临时排除原 trunk 首跳重新构树，要求替代计划在当前节点仍保持单 dispatch，避免把 7B 扩成 branch-level redundancy 子系统
	- primary dispatch 与 trunk backup 共享同一套 node-local duplicate 语义，因此重复副本会在最终目的节点收敛，而不会重复 fan-out

- **冗余 observability 已接通到 logger / stats / experiment smoke**
	- `Logger` 现可记录 `redundancy_considered`
	- `SimStats` 已接通 `redundancy_trigger_count`、`redundant_first_hit_count`、`duplicate_replica_discard_count` 与 `redundancy_benefit_cost_ratio`
	- `exp5_redundancy` baseline / matrix 已纳入自动化配置验证，runtime 现可直接读取冗余配置

### 测试覆盖

本轮新增 9 个测试用例，总测试数从 160 增长到 169：

- `RouteDiversityAnalyzerTest.PrefersDisjointEntryNeighbors`
- `RedundancyManagerTest.SelectsSingleBackupWhenPrimaryRiskExceedsThreshold`
- `RedundancyManagerTest.SkipsAlreadyExpandedFamilies`
- `RedundancyManagerTest.RouterTraceIncludesBackupRouteWhenAlternativesExist`
- `BundleTest.RedundancyHelpersReflectReplicaFamily`
- `SimStatsTest.RedundantBackupDeliversOnceAndDropsDuplicateReplica`
- `MulticastExecutionTest.TrunkOnlyRedundancyDropsLaterBackupBranches`
- `ExperimentConfigTest.Exp5BaselineScenarioIncludesRuntimeRedundancy`
- `ExperimentConfigTest.Exp5MatrixDimensionsMatchRedundancyExperiment`

覆盖场景包括：

- redundancy risk / diversity / capacity / TTL 选择语义
- CGR trace 层 backup route 产出
- 单播 `SINGLE_BACKUP` 的副本物化、首达交付、duplicate drop 与收益统计
- 多播 shared-prefix trunk 的最小 `TRUNK_ONLY` backup dispatch 与重复副本收敛
- `exp5_redundancy` baseline / matrix 的 runtime 配置可执行性

### 验收结果

```bash
cmake --build build --target sabr_tests
.\build\sabr_tests.exe --gtest_filter="RouteDiversityAnalyzerTest.*:RedundancyManagerTest.*:BundleTest.RedundancyHelpersReflectReplicaFamily:ParserTest.ParsesInlineScenarioConfigIntoRuntimeTypes:ExperimentConfigTest.Exp5*:SimStatsTest.RedundantBackupDeliversOnceAndDropsDuplicateReplica:MulticastExecutionTest.TrunkOnlyRedundancyDropsLaterBackupBranches"
ctest --test-dir build --output-on-failure
```

结果：

- 编译通过
- 7B 聚焦回归通过
- **169/169 测试通过**

### 阶段结论

阶段 7B 已达到当前开发计划中的交付目标：

- 单播 `NONE` / `SINGLE_BACKUP` / `MULTI_BACKUP` 的 runtime redundancy 闭环已具备
- node-local first-arrival wins、`DUPLICATE_DROPPED` 与冗余收益统计已进入基线
- 多播已具备最小 `TRUNK_ONLY` shared-prefix trunk redundancy 能力
- `exp5_redundancy` 已接入自动化配置覆盖，完整回归基线提升到 169/169

后续可进入阶段 8A：实验编排与批量运行。

## 阶段 8A：实验编排与批量运行

**日期**：2026-04-27  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 8A.1 | 在 IO 层新增 experiment runner，支持 JSON 文档级 matrix 覆写、笛卡尔积展开、run manifest 和稳定输出目录 | ✅ | 实验方案 4.2, 4.3 |
| 8A.2 | 将 `src/main.cpp` 从版本存根升级为最小 CLI，支持 `run-scenario` 和 `run-experiment` | ✅ | 实验方案 4.2 |
| 8A.3 | 新增 `scripts/run_experiments.py` 与 `scripts/aggregate_results.py`，打通批量 smoke / full 运行与聚合输出 | ✅ | 实验方案输出要求 |
| 8A.4 | 为 exp3 / exp4 补齐自动化配置覆盖，为 exp5 增加 `multi_backup` / `trunk_only` 场景文件 | ✅ | 实验方案 6.3 ~ 6.6 |
| 8A.5 | 新增 runner 聚焦测试、扩展实验配置测试，并完成六个实验的脚本 smoke 闭环 | ✅ | 100% 可执行性 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `include/io/parser.hpp` | 公开 JSON 文档读取与 `parse_scenario_document()`，供 experiment runner 复用既有 parser 语义 |
| `include/io/experiment_runner.hpp` | 新增 experiment runner 公共接口、run manifest 与执行产物模型 |
| `src/io/parser.cpp` | 将场景解析核心抽成文档级入口，保持 file-based parser 与 override runner 共享同一条解析链 |
| `src/io/experiment_runner.cpp` | 实现 matrix 覆写、别名路径处理、`multicast.group_size` 合成维度、输出目录与 manifest 落盘 |
| `include/simulation/engine.hpp` | 为 runner 增加 `set_scenario_name()` 声明 |
| `src/simulation/engine.cpp` | 通过 `SimStats::set_metadata()` 写入 scenario metadata，保证批量运行结果可识别 |
| `src/main.cpp` | 实现 `run-scenario` / `run-experiment` 最小 CLI |
| `scripts/run_experiments.py` | 顺序执行 smoke / full 批量运行，枚举 exp1 ~ exp6 并汇总失败状态 |
| `scripts/aggregate_results.py` | 递归消费 `run_manifest.json` 与 `stats.json`，输出扁平 aggregated JSON / CSV |
| `tests/test_experiment_runner.cpp` | 新增 matrix 展开、合成维度覆写、单场景执行与 manifest 落盘测试 |
| `tests/test_experiment_configs.cpp` | 新增 exp3 / exp4 配置覆盖，补充 exp5 `multi_backup` / `trunk_only` 场景校验 |
| `configs/experiments/exp5_redundancy/multi_backup.json` | 新增 `MULTI_BACKUP` 基线场景 |
| `configs/experiments/exp5_redundancy/trunk_only.json` | 新增 `TRUNK_ONLY` 基线场景 |

### 实现说明

- **matrix 覆写落在 JSON 文档层，而不是强类型结构反射层**
	- `ExperimentRunner` 先读取 scenario JSON 文档，再按 matrix 维度路径就地覆写，最后调用 `ConfigParser::parse_scenario_document()` 进入既有解析链
	- 这样避免了为 `ScenarioConfig` 维护第二套覆写语义，也保证 `contact_plan_file`、`failure_injection` / `failures`、enhancements、redundancy 等既有 parser 行为不分叉

- **runner 对矩阵路径做了最小兼容，而不是强推重写实验配置**
	- 常规路径如 `traffic[0].payload_size`、`simulation.phase1.k_paths` 直接映射到原始 JSON 节点
	- 别名路径 `failure_injection[0].trigger_time` / `failures[0].probability` 支持首段别名兼容
	- `multicast.group_size` 作为 exp3 的合成维度进入 runner：多播场景收缩 `member_nodes`，split-unicast control 场景收缩 `traffic` 条目数

- **最小 CLI 只服务自动化，不抢 8B 的交付范围**
	- `run-scenario` 负责单场景执行、写出 `stats.json` / CSV / `run_manifest.json`
	- `run-experiment` 负责单个 matrix 文件的顺序 sweep，可选 `--limit` 做快速 smoke
	- 更完整的帮助、plot/export 子命令和交付文档继续保留到 8B

- **批量脚本与聚合脚本已经形成最小闭环**
	- `run_experiments.py` 会枚举 `configs/experiments/exp1` 到 `exp6`，在 smoke 模式下选择轻量场景执行，在 full 模式下调用 `run-experiment`
	- `aggregate_results.py` 递归扫描结果目录中的 `run_manifest.json`，结合 `stats.json` 的 `metadata` / `summary` 输出扁平 JSON / CSV 汇总表

### 测试覆盖

本轮新增 8 个测试用例，总测试数从 169 增长到 177：

- `ExperimentRunnerTest.Exp2MatrixExpansionAppliesOverridesAndAssignsRunDirectories`
- `ExperimentRunnerTest.Exp3SyntheticGroupSizeAdjustsScenarioDocumentBeforeParsing`
- `ExperimentRunnerTest.RunScenarioWritesArtifactsAndManifest`
- `ExperimentConfigTest.Exp3TreePlanScenarioIsExecutable`
- `ExperimentConfigTest.Exp3MatrixDimensionsMatchMulticastPlanExperiment`
- `ExperimentConfigTest.Exp4BaselineScenarioIncludesPlanReplacement`
- `ExperimentConfigTest.Exp4MatrixDimensionsMatchMulticastRepairExperiment`
- `ExperimentConfigTest.Exp5AdditionalBaselineScenariosCoverExpandedModes`

覆盖场景包括：

- exp2 matrix 的笛卡尔积展开、参数覆写和输出目录生成
- exp3 `multicast.group_size` 合成维度的文档级覆写
- 单场景执行后的 `stats.json` / CSV / `run_manifest.json` 落盘
- exp3 / exp4 / exp5 的配置可执行性与关键字段语义

### 验收结果

```bash
cmake --build build --target sabr sabr_tests
ctest --test-dir build -R "ExperimentRunnerTest|ExperimentConfigTest" --output-on-failure
.\build\sabr.exe run-scenario --config .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\cli_smoke\exp1
.\build\sabr.exe run-experiment --matrix .\configs\experiments\exp2_unicast_scale\matrix.json --output .\results\cli_matrix_smoke --limit 1
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\run_experiments.py --mode smoke --sabr .\build\sabr.exe --output-root .\results\script_smoke
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\aggregate_results.py --input .\results\script_smoke --output .\results\script_smoke\aggregated
ctest --test-dir build --output-on-failure
```

结果：

- 编译通过
- runner / experiment config 聚焦测试通过
- `run-scenario` 与 `run-experiment` CLI 实跑通过
- 六个实验的脚本 smoke 全部通过，聚合脚本成功生成 JSON / CSV
- **177/177 测试通过**

### 阶段结论

阶段 8A 已达到当前开发计划中的交付目标：

- matrix 覆写、批量运行、结果 manifest 与聚合脚本已接入基线
- `run-scenario` / `run-experiment` 最小自动化入口已可供脚本和 CI 直接调用
- exp1 ~ exp6 已具备自动化 smoke 闭环，六个实验都能从配置与命令直接运行
- 全量回归基线提升到 177/177

后续可进入阶段 8B：可视化、CLI 与实验交付。

## 阶段 8B：可视化、CLI 与实验交付

**日期**：2026-04-27  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 8B.1 | 新增共享 Python 结果工具与四类 SVG 绘图脚本，覆盖 topology / timeline / utilization / comparison | ✅ | IO-OUT-05 |
| 8B.2 | 将最小 CLI 升级为完整命令分发层，支持 `run-scenario` / `run-experiment` / `export` / `plot` / `help` / `--version` | ✅ | 可用性 |
| 8B.3 | 补齐 README、实验运行手册和指标说明，固定构建、执行、结果目录与图表工作流 | ✅ | 100% 可执行性 |
| 8B.4 | 生成最小样例导出与图表产物，作为交付模板与文档示例 | ✅ | 交付 |
| 8B.5 | 新增 CLI 解析测试并完成 export / plot 命令级 smoke，收口全量回归 | ✅ | 自动化验证 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `include/io/cli.hpp` | 新增 CLI 命令模型、命令类型与解析错误类型 |
| `src/io/cli.cpp` | 实现命令解析、帮助文本、`export` / `plot` 参数模型 |
| `include/io/script_bridge.hpp` | 声明 Python 解释器发现、脚本定位与脚本执行桥 |
| `src/io/script_bridge.cpp` | 实现工作区根目录发现、`.venv` Python 发现与 Windows 进程调用桥 |
| `src/main.cpp` | 将 CLI 切到命令分发层，并接通 `export` / `plot` 子命令 |
| `scripts/sabr_results.py` | 抽出 manifest / stats 读取、扁平行生成、pivot 构造、contact plan 加载等共享能力 |
| `scripts/aggregate_results.py` | 扩展为支持 `flat` / `pivot` / `both` 三种导出模式 |
| `scripts/plot_topology.py` | 从 scenario 或 contact plan 生成拓扑 SVG |
| `scripts/plot_timeline.py` | 从 run 目录或 `stats.json` 生成 bundle 生命周期时间轴 SVG |
| `scripts/plot_utilization.py` | 从 `contact_utilization.csv` 或 run 目录生成利用率 SVG |
| `scripts/plot_comparison.py` | 从聚合结果生成实验或矩阵对比 SVG |
| `tests/test_cli.cpp` | 新增 CLI 帮助、命令解析、export/plot 参数与多词 title 测试 |
| `README.md` | 新增根级使用说明、CLI 快速入口、样例输入输出与结果目录约定 |
| `docs/experiments.md` | 新增实验目录、推荐命令、export / plot 工作流与样例目录说明 |
| `docs/metrics.md` | 新增 `stats.json`、summary 字段和 CSV 契约说明 |

### 实现说明

- **CLI 进入稳定交付形态，而不是继续在 `main.cpp` 堆分支**
	- 8A 的最小命令面已经够用，但帮助、错误信息和脚本桥都过于粗糙，因此本轮先把解析逻辑抽到 `cli` 模块
	- `main.cpp` 现在只做命令分发；命令参数、帮助文本和错误边界则由 `CLICommand` / `CLIParseError` 统一承载

- **导出和绘图继续保留在 Python 层，由 C++ 做 wrapper**
	- 本轮没有把 plotting 或 pivot 逻辑回灌到 C++，而是新增 `script_bridge`，让 CLI 直接调用 Python 脚本
	- Windows 下直接使用进程执行桥，而不是依赖脆弱的 shell 拼接；这样 `export` / `plot` 能稳定处理工作区内现有脚本和路径

- **聚合、pivot 与绘图共享一套结果读取语义**
	- `sabr_results.py` 统一处理 `run_manifest.json`、`stats.json`、CSV、matrix 坐标和 contact plan 输入，避免 `aggregate_results.py` 与各个 plot 脚本各自维护一套字段解释
	- `aggregate_results.py` 在扁平汇总之外新增 `aggregated_pivot.csv`，为矩阵结果对比和后续报告生成提供稳定输入

- **图表交付采用零第三方依赖的 SVG 路线**
	- 四个 plot 脚本全部直接写 SVG，不依赖 `matplotlib`，因此与当前仓库轻依赖风格一致
	- 拓扑图从 contact plan 生成、时间轴从 `stats.json` bundle 记录生成、利用率图从 `contact_utilization.csv` 生成、comparison 图从聚合结果生成，输入边界都与 8A 结果契约直接对齐

- **交付文档不再依赖开发计划和 devlog 充当用户说明**
	- 根目录 `README.md` 现已覆盖构建、CLI、结果目录和样例输入输出
	- `docs/experiments.md` 与 `docs/metrics.md` 则承接实验目录、工作流和字段说明，后续 9 阶段可直接在此基础上扩实验复现包

### 测试覆盖

本轮新增 11 个测试用例，总测试数从 177 增长到 188：

- `CliParseTest.EmptyArgsMapToVersion`
- `CliParseTest.HelpCommandProducesExpandedUsageText`
- `CliParseTest.ParsesRunScenarioArguments`
- `CliParseTest.ParsesRunExperimentArguments`
- `CliParseTest.ParsesExportArgumentsWithDefaultFormat`
- `CliParseTest.ParsesExportArgumentsWithPivotOptions`
- `CliParseTest.ParsesPlotArguments`
- `CliParseTest.ParsesPlotArgumentsWithMetricAndTitle`
- `CliParseTest.ParsesMultiTokenPlotTitle`
- `CliParseTest.RejectsUnknownCommand`
- `CliParseTest.RejectsMissingRequiredOption`

另外，本轮还完成了命令级 smoke 验证：

- `sabr --help`
- `sabr export --input .\results\script_smoke --output .\results\export_smoke --format both ...`
- `sabr plot --kind topology ...`
- `sabr plot --kind timeline ...`
- `sabr plot --kind utilization ...`
- `sabr plot --kind comparison ...`

产物已实际写入：

- `results/export_smoke/aggregated_summary.json`
- `results/export_smoke/aggregated_summary.csv`
- `results/export_smoke/aggregated_pivot.csv`
- `results/plot_smoke/topology.svg`
- `results/plot_smoke/timeline.svg`
- `results/plot_smoke/utilization.svg`
- `results/plot_smoke/comparison.svg`

### 验收结果

```bash
cmake --build build --target sabr sabr_tests
ctest --test-dir build -R "CliParseTest" --output-on-failure
.\build\sabr.exe --help
.\build\sabr.exe export --input .\results\script_smoke --output .\results\export_smoke --format both --metric summary::delivery_rate --row-key scenario_name
.\build\sabr.exe plot --kind topology --input .\configs\experiments\exp1_unicast_correctness\baseline.json --output .\results\plot_smoke\topology.svg
.\build\sabr.exe plot --kind timeline --input .\results\script_smoke\exp1_unicast_correctness --output .\results\plot_smoke\timeline.svg
.\build\sabr.exe plot --kind utilization --input .\results\script_smoke\exp1_unicast_correctness --output .\results\plot_smoke\utilization.svg
.\build\sabr.exe plot --kind comparison --input .\results\export_smoke\aggregated_summary.csv --output .\results\plot_smoke\comparison.svg --metric summary::delivery_rate --x-key scenario_name --title Smoke Delivery Comparison
ctest --test-dir build --output-on-failure
```

结果：

- 编译通过
- CLI 聚焦测试通过
- `export` / `plot` 四类命令 smoke 通过
- README 与 docs 文档已落盘且无编辑器诊断错误
- **188/188 测试通过**

### 阶段结论

阶段 8B 已达到当前开发计划中的交付目标：

- CLI 已从最小自动化入口升级为完整交付命令面
- 结果导出、pivot 与 SVG 图表链已进入基线
- README、实验手册和指标说明已补齐，用户不再需要依赖开发日志理解结果目录和指标语义
- 工作区内已生成最小样例导出与图表产物，可直接作为实验汇报模板
- 全量回归基线提升到 188/188

后续可进入阶段 9：系统测试与最终交付。

---

## 阶段 9：系统测试与最终交付

**日期**：2026-04-27  
**状态**：✅ 已完成

### 完成项

| 编号 | 任务 | 状态 | 覆盖需求 |
|---|---|---|---|
| 9.1 | 重跑全量回归并把 Stage 9 Python 验证接入 CTest | ✅ | 全部测试需求 |
| 9.2 | 新增 NF-PF-01 性能基准，生成 20 节点 / 200 Contact / 1000 Bundle 场景并输出性能报告 | ✅ | NF-PF-01 |
| 9.3 | 执行 exp1 到 exp6 的代表性正式场景，生成原始结果、聚合导出和 SVG 图表 | ✅ | 实验方案 6.1 ~ 6.6 |
| 9.4 | 对性能未达标时才进入 profiling/优化的路径做收口；本轮因 NF-PF-01 通过而未触发优化分支 | ✅ | NF-PF-02 |
| 9.5 | 补齐阶段 9 的 README、实验工作流、性能报告与复现说明文档 | ✅ | NF-MT-02 |
| 9.6 | 生成标准 Stage 9 复现包，包含源码文档、脚本、配置和代表性结果样例 | ✅ | 最终交付 |

### 主要变更文件

| 文件 | 作用 |
|---|---|
| `scripts/benchmark_nf_pf_01.py` | 生成并执行 NF-PF-01 大规模场景，写出 `performance_report.json` |
| `scripts/stage9_suite.py` | 串联性能基准、代表性实验执行、export、plot 与标准复现包打包 |
| `tests/test_stage9_scripts.py` | 验证 NF-PF-01 场景形状、Stage 9 执行计划和复现包来源集合 |
| `CMakeLists.txt` | 接入 Python 解释器发现与 `sabr_python_stage9_tests` |
| `README.md` | 新增 Stage 9 benchmark / suite 命令和结果布局说明 |
| `docs/experiments.md` | 新增 Stage 9 正式执行流程与代表性实验目录 |
| `docs/metrics.md` | 新增 `performance_report.json` 契约说明 |
| `docs/reproducibility.md` | 新增 Stage 9 复现与交付说明文档 |

### 实现说明

- **NF-PF-01 基准改为可重复生成，而不是手工维护超大静态场景**
	- `benchmark_nf_pf_01.py` 直接生成内嵌 JSON contact plan 场景，保证 20 节点 / 200 Contact / 1000 Bundle 的结构约束始终可检查
	- 基准报告同时记录阈值、运行命令、wall clock、stats 摘要和环境信息，避免只留下一个裸时间数字

- **Stage 9 不再依赖零散命令，而是收口为一条正式执行链**
	- `stage9_suite.py` 将 NF-PF-01、代表性实验执行、聚合导出、SVG 绘图和标准复现包打包串成一个入口
	- 默认代表性集合覆盖 exp1 到 exp6，共执行 24 个 run，并生成 per-experiment export / plot 以及全局 overview 图

- **交付面验证进入自动化基线**
	- 新增 `tests/test_stage9_scripts.py`，并通过 `sabr_python_stage9_tests` 接入 CTest
	- 这样阶段 9 的关键入口不再只是一次性脚本，而是纳入正式回归面

- **标准复现包按 Stage 9 交付范围固定**
	- 复现包包含 `README.md`、`docs/`、`configs/experiments/`、Stage 9 所需脚本，以及 `samples/` 下的 benchmark、representative、exports、plots 和执行计划/报告
	- `bundle_manifest.json` 记录复制清单，便于审计交付内容

### 关键结果与产物

- 全量回归：**189/189 测试通过**
- NF-PF-01：20 节点 / 200 Contact / 1000 Bundle，**2.886 秒** 完成，低于 `< 60 秒` 阈值
- 代表性实验正式执行：**24 个 run**，全部成功
- 全局导出：`results/stage9/final/exports/all/aggregated_summary.csv`
- 全局图表：`results/stage9/final/plots/stage9_overview.svg`
- 复现包：`results/stage9/final/release_bundle/`

### 验收结果

```bash
cmake -S . -B build
cmake --build build --target sabr sabr_tests
ctest --test-dir build --output-on-failure
d:\code_workbench_D\SABR\.venv\Scripts\python.exe .\scripts\stage9_suite.py --output-root .\results\stage9\final
```

结果：

- 配置成功，Python 解释器已通过 CMake 发现并接入 CTest
- 编译通过
- **189/189 测试通过**，包含 `sabr_python_stage9_tests`
- Stage 9 suite 成功完成 benchmark、代表性实验、export、plot 和 release bundle
- `results/stage9/final/release_bundle/bundle_manifest.json` 无 skipped 条目

### 阶段结论

阶段 9 已完成当前开发计划中的最终交付目标：

- NF-PF-01 已在当前 Windows + MinGW 环境下实测通过，无需进入 profiling / 优化分支
- exp1 到 exp6 已拥有一条统一的正式执行、导出、绘图和记录流程
- 标准复现包已生成，交付内容具备审计清单
- 用户文档、实验工作流和性能报告说明已与当前代码基线对齐
- 项目整体基线提升到 **189/189**，并具备可直接复现的 Stage 9 最终交付目录
