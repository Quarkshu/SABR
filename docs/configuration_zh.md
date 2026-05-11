# SABR 配置文件使用手册

本文档说明 SABR 实验配置文件的结构、字段含义、默认值、枚举取值和常见写法，适用于：

- 场景配置文件，例如 `baseline.json`、`tree_plan.json`
- 参数矩阵文件，例如 `matrix.json`
- 接触计划文件，例如 `contact_plan.ion`、`replacement_plan.json`

如果你只想快速写出一个可运行的配置，建议先看第 1 节和第 4 节；如果你要做批量实验，重点看第 8 节。

## 1. 配置类型总览

SABR 当前支持三类输入配置：

| 类型 | 用途 | 典型文件 |
|---|---|---|
| 场景配置 | 定义单次仿真的输入 | `configs/experiments/.../baseline.json` |
| 参数矩阵 | 对一个或多个场景做批量参数展开 | `configs/experiments/.../matrix.json` |
| 接触计划 | 定义 contacts 和可选 ranges | `contact_plan.ion`、`replacement_plan.json` |

几个重要规则：

1. 场景配置里必须二选一提供 `contact_plan` 或 `contact_plan_file`，不能同时缺失，也不能同时出现。
2. 所有相对路径都相对于“当前 JSON 文件所在目录”解析。
3. 枚举字符串大小写不敏感，例如 `batch`、`BATCH`、`Batch` 都可被识别。
4. `run-scenario` 和 `run-experiment` 的 `--output` 可以覆盖配置里的 `output_directory`。

## 2. 场景配置顶层结构

一个完整的场景配置通常长这样：

```json
{
  "scenario_name": "exp_example",
  "output_directory": "../../../results/example",
  "contact_plan_file": "contact_plan.ion",
  "enhancements": {
    "one_route_per_neighbor": true,
    "queue_delay": true,
    "anti_loop_proactive": true
  },
  "simulation": {
    "start_time": 0.0,
    "end_time": 70.0,
    "owlt_margin": 0.5,
    "recompute_budget": 4,
    "failure_seed": 303,
    "phase1": {
      "k_paths": 6
    }
  },
  "traffic": [
    {
      "mode": "BATCH",
      "source_node": 1,
      "destination_node": 4,
      "start_time": 2.0,
      "bundle_count": 4,
      "payload_size": 256.0,
      "header_size": 32.0,
      "priority": "NORMAL",
      "ttl": 80.0
    }
  ],
  "failures": [
    {
      "mode": "CONTACT_SET",
      "trigger_time": 10.0,
      "contact_ids": [5],
      "label": "remove_midpath"
    }
  ],
  "redundancy": {
    "mode": "SINGLE_BACKUP",
    "max_extra_copies": 1
  }
}
```

顶层字段说明如下：

| 字段 | 是否必填 | 默认值 | 说明 |
|---|---|---|---|
| `scenario_name` / `name` | 否 | 当前文件名去扩展名 | 场景名称，写入结果和 manifest |
| `output_directory` | 否 | 见下文 | 场景默认输出目录 |
| `contact_plan_file` | 二选一必填 | 无 | 外部接触计划文件 |
| `contact_plan` | 二选一必填 | 无 | 内联接触计划对象 |
| `enhancements` | 否 | 全部为 `false` | 开关型增强项 |
| `simulation` / `engine` | 否 | 内置默认值 | 仿真时间窗、Phase1/2 参数等 |
| `traffic` / `traffic_patterns` | 否 | 空数组 | 业务流生成规则 |
| `multicast_groups` | 否 | 空数组 | 多播组定义 |
| `failures` / `failure_injection` | 否 | 空数组 | 失效注入规则 |
| `redundancy` | 否 | 见第 7 节 | 冗余副本策略 |
| `experiment_matrix` | 否 | 空 | 可内嵌矩阵定义 |

关于 `output_directory`：

- 如果场景 JSON 里写了它，会先按该值解析。
- 如果命令行传入 `--output`，命令行优先。
- 如果两者都没有，默认输出为当前工作目录下的 `results/<scenario_name>`。

## 3. 接触计划配置

### 3.1 使用外部文件

最常见写法是：

```json
{
  "contact_plan_file": "contact_plan.ion"
}
```

支持的文件格式：

- ION 文本格式，例如 `.ion`
- JSON 格式，例如 `.json`

### 3.2 内联 `contact_plan`

如果你不想单独放文件，也可以直接内联：

```json
{
  "contact_plan": {
    "contacts": [
      {
        "start_time": 0.0,
        "end_time": 10.0,
        "from_node": 1,
        "to_node": 2,
        "data_rate": 100.0
      }
    ],
    "ranges": [
      {
        "start_time": 0.0,
        "end_time": 20.0,
        "node_a": 1,
        "node_b": 2,
        "distance_light_seconds": 1.0
      }
    ]
  }
}
```

### 3.3 JSON 接触计划字段

`contacts` 是必填数组，`ranges` 是可选数组。

`contacts` 中每个元素支持：

| 字段 | 是否必填 | 别名 | 说明 |
|---|---|---|---|
| `contact_id` | 否 | `id` | 接触编号；不写时自动分配 |
| `start_time` | 是 | `start` | 接触开始时间 |
| `end_time` | 是 | `end` | 接触结束时间 |
| `from_node` | 是 | `sending_node` | 发送节点 |
| `to_node` | 是 | `receiving_node` | 接收节点 |
| `data_rate` | 是 | `rate` | 数据率 |

`ranges` 中每个元素支持：

| 字段 | 是否必填 | 别名 | 说明 |
|---|---|---|---|
| `start_time` | 是 | `start` | 生效开始时间 |
| `end_time` | 是 | `end` | 生效结束时间 |
| `node_a` | 是 | 无 | 节点 A |
| `node_b` | 是 | 无 | 节点 B |
| `distance_light_seconds` | 是 | `owlt`、`distance` | 光行时 |

约束：

1. `end_time` 不能早于 `start_time`。
2. `contacts` 不能为空。
3. 若 `contact_id` 重复，解析会失败。

### 3.4 ION 接触计划格式

示例：

```text
a contact +0 +12 1 2 100
a range +0 +20 1 2 1.0
```

其中：

- `a contact <start> <end> <from> <to> <rate>` 表示一条接触
- `a range <start> <end> <node_a> <node_b> <owlt>` 表示一条距离区间

时间 token 可以写成 `+12` 这种带前缀形式。

## 4. `simulation` 和 `enhancements`

### 4.1 `enhancements`

`enhancements` 是一组简洁的布尔开关，默认都为 `false`：

| 字段 | 默认值 | 作用 |
|---|---|---|
| `one_route_per_neighbor` | `false` | 映射到 Phase1 的首跳邻居去重开关 |
| `queue_delay` | `false` | 映射到 Phase2 的队列时延增强 |
| `anti_loop_reactive` | `false` | 映射到 Phase2 的反应式防环 |
| `anti_loop_proactive` | `false` | 映射到 Phase2 的预防式防环 |

注意：

1. `enhancements` 会先写入引擎配置。
2. 如果你同时在 `simulation.phase1` 或 `simulation.phase2` 中写了对应字段，子配置会覆盖 `enhancements` 的值。

### 4.2 `simulation` 顶层字段

`simulation` 也可以写成 `engine`。默认值来自引擎配置：

| 字段 | 默认值 | 别名 | 说明 |
|---|---|---|---|
| `start_time` | `0.0` | 无 | 仿真开始时间 |
| `end_time` | `3600.0` | 无 | 仿真结束时间 |
| `owlt_margin` | `0.0` | 无 | OWLT 裕量 |
| `recompute_budget` | `1` | 无 | 允许的重算预算 |
| `failure_seed` | `0` | `random_seed` | 失效注入随机种子 |
| `phase1` | 空对象 | `phase1_config` | Phase1 参数 |
| `phase2` | 空对象 | `phase2_config` | Phase2 参数 |

### 4.3 `simulation.phase1`

| 字段 | 默认值 | 说明 |
|---|---|---|
| `k_paths` | `3` | Phase1 计算的候选路径数 |
| `owlt_margin` | 继承 `simulation.owlt_margin` | Phase1 专用 OWLT 裕量 |
| `one_route_per_neighbor` | `false` | 是否限制为每个首跳邻居仅保留一条路由 |

### 4.4 `simulation.phase2`

| 字段 | 默认值 | 别名 | 说明 |
|---|---|---|---|
| `owlt_margin` | 继承 `simulation.owlt_margin` | 无 | Phase2 专用 OWLT 裕量 |
| `queue_delay_enhancement` | `false` | `queue_delay` | 启用队列时延增强 |
| `anti_loop_reactive` | `false` | 无 | 启用反应式防环 |
| `anti_loop_proactive` | `false` | 无 | 启用预防式防环 |

一个常见写法如下：

```json
{
  "enhancements": {
    "one_route_per_neighbor": true,
    "queue_delay": true
  },
  "simulation": {
    "start_time": 0.0,
    "end_time": 80.0,
    "owlt_margin": 0.5,
    "recompute_budget": 4,
    "phase1": {
      "k_paths": 6
    },
    "phase2": {
      "anti_loop_proactive": true
    }
  }
}
```

## 5. `traffic` 参数说明

`traffic` 也可以写成 `traffic_patterns`。它是一个数组，每个元素描述一类业务流。

### 5.1 模式语义

`mode` 支持三种取值：

| 取值 | 语义 |
|---|---|
| `SINGLE` | 在 `start_time` 生成 1 个 bundle |
| `BATCH` | 在 `start_time` 同时生成 `bundle_count` 个 bundle |
| `PERIODIC` | 从 `start_time` 开始，每隔 `period` 生成一个，共 `bundle_count` 个 |

补充规则：

1. `mode` 缺省时默认为 `SINGLE`。
2. `PERIODIC` 如果 `period <= 0`，会退化成只在 `start_time` 生成 1 个 bundle。
3. `BATCH` 和 `PERIODIC` 才会真正使用 `bundle_count`。

### 5.2 字段表

| 字段 | 是否必填 | 默认值 | 别名 | 说明 |
|---|---|---|---|---|
| `mode` | 否 | `SINGLE` | 无 | 生成模式 |
| `source_node` | 是 | 无 | `source` | 源节点 |
| `destination_node` | 单播必填 | 无 | `destination`、`destination_eid` | 单播目的节点 |
| `start_time` | 否 | `0.0` | 无 | 首次生成时间 |
| `bundle_count` | 否 | `1` | 无 | bundle 数量 |
| `period` | 否 | `0.0` | 无 | 周期，仅 `PERIODIC` 有意义 |
| `payload_size` | 否 | `0.0` | 无 | 载荷大小 |
| `header_size` | 否 | `0.0` | 无 | 头部大小 |
| `priority` | 否 | `BULK` | 无 | 优先级 |
| `ttl` | 否 | `0.0` | 无 | 生存时间 |
| `is_critical` | 否 | `false` | 无 | 是否关键业务 |
| `allow_fragmentation` | 否 | `true` | 无 | 是否允许分片 |
| `is_multicast` | 否 | `false` | 无 | 是否多播 |
| `multicast_group_id` | 多播必填 | 空字符串 | `group_id`、`destination_group` | 多播组 ID |

`priority` 支持：

- 字符串：`BULK`、`NORMAL`、`EXPEDITED`
- 整数：`0`、`1`、`2`

### 5.3 单播示例

```json
{
  "traffic": [
    {
      "mode": "PERIODIC",
      "source_node": 1,
      "destination_node": 6,
      "start_time": 1.0,
      "period": 5.0,
      "bundle_count": 6,
      "payload_size": 256.0,
      "header_size": 24.0,
      "priority": "NORMAL",
      "ttl": 80.0
    }
  ]
}
```

### 5.4 多播示例

如果条目里出现 `multicast_group_id`，解析器会把它当作多播业务，即使你没显式写 `is_multicast: true`。

```json
{
  "multicast_groups": [
    {
      "group_id": "ops_broadcast",
      "source_node": 1,
      "member_nodes": [4, 5]
    }
  ],
  "traffic": [
    {
      "mode": "SINGLE",
      "source_node": 1,
      "multicast_group_id": "ops_broadcast",
      "start_time": 2.0,
      "bundle_count": 1,
      "payload_size": 64.0,
      "header_size": 16.0,
      "priority": "NORMAL",
      "ttl": 80.0,
      "allow_fragmentation": false
    }
  ]
}
```

多播约束：

1. 多播条目必须能找到对应的 `multicast_group_id`。
2. 如果 `is_multicast` 为 `true` 但没写组 ID，解析会失败。
3. 单播条目若不写 `destination_node`，解析会失败。

## 6. `multicast_groups` 和 `failures`

### 6.1 `multicast_groups`

字段如下：

| 字段 | 是否必填 | 默认值 | 说明 |
|---|---|---|---|
| `group_id` | 是 | 无 | 多播组唯一标识 |
| `source_node` | 强烈建议必填 | `-1` | 源节点；运行时要求为正整数 |
| `member_nodes` | 是 | 无 | 成员节点数组 |

运行时约束：

1. `group_id` 不能重复。
2. `source_node` 必须大于 0。
3. `member_nodes` 不能为空。
4. `member_nodes` 中不能包含 `source_node`。
5. 成员节点必须全部为正整数。

### 6.2 `failures` 和 `failure_injection`

这两个顶层字段是同义别名，二者任选其一即可。

每条失效规则支持：

| 字段 | 是否必填 | 默认值 | 说明 |
|---|---|---|---|
| `mode` | 是 | 无 | 失效模式 |
| `trigger_time` | 是 | 无 | 触发时间 |
| `contact_ids` | 视模式而定 | 空数组 | 受影响接触集合 |
| `probability` | 概率模式必填 | 无 | 删除候选接触的概率，范围 `[0, 1]` |
| `replacement_plan_file` | 替换模式二选一必填 | 无 | 外部替换计划 |
| `replacement_plan` | 替换模式二选一必填 | 无 | 内联替换计划 |
| `label` | 否 | 空字符串 | 规则标签 |

`mode` 支持：

| 取值 | 语义 |
|---|---|
| `CONTACT_SET` | 到达 `trigger_time` 时直接移除 `contact_ids` 指定的接触 |
| `PROBABILISTIC_CONTACTS` | 在候选接触中按 `probability` 做伯努利抽样并移除 |
| `PLAN_REPLACEMENT` | 在 `trigger_time` 时替换为新接触计划 |

补充规则：

1. `CONTACT_SET` 必须提供非空 `contact_ids`。
2. `PROBABILISTIC_CONTACTS` 中，若未提供 `contact_ids`，系统会对所有“在 `trigger_time` 之后仍然活跃”的接触进行抽样。
3. `PLAN_REPLACEMENT` 必须提供 `replacement_plan_file` 或 `replacement_plan`。
4. `replacement_plan_file` 也是相对当前场景 JSON 解析。
5. 如果规则触发时间不在仿真窗口内，该规则不会生效。

示例：

```json
{
  "failures": [
    {
      "mode": "PROBABILISTIC_CONTACTS",
      "trigger_time": 0.0,
      "probability": 0.15,
      "contact_ids": [3, 4, 5, 6],
      "label": "background_failures"
    },
    {
      "mode": "PLAN_REPLACEMENT",
      "trigger_time": 5.0,
      "replacement_plan_file": "replacement_plan.ion",
      "label": "repair_after_backbone_loss"
    }
  ]
}
```

## 7. `redundancy` 参数说明

`redundancy` 全部字段都是可选的。默认配置如下：

| 字段 | 默认值 | 说明 |
|---|---|---|
| `mode` | `NONE` | 冗余模式 |
| `max_extra_copies` | `1` | 最多额外副本数 |
| `risk_threshold` | `0.65` | 风险阈值 |
| `min_diversity_score` | `0.50` | 最小路径多样性分数 |
| `min_capacity_reserve_ratio` | `0.15` | 最小容量保留比例 |
| `min_ttl_slack` | `0.0` | 最小 TTL 余量 |
| `enable_for_unicast` | `true` | 是否允许用于单播 |
| `enable_for_multicast_trunk` | `true` | 是否允许用于多播 trunk |

`mode` 支持：

| 取值 | 说明 |
|---|---|
| `NONE` | 不生成额外副本 |
| `SINGLE_BACKUP` | 最多选 1 条备用路径 |
| `MULTI_BACKUP` | 最多选 `max_extra_copies` 条备用路径 |
| `TRUNK_ONLY` | 仅考虑多播 trunk 场景 |

示例：

```json
{
  "redundancy": {
    "mode": "MULTI_BACKUP",
    "max_extra_copies": 2,
    "risk_threshold": 0.55,
    "min_diversity_score": 0.5,
    "min_capacity_reserve_ratio": 0.2,
    "min_ttl_slack": 6.0,
    "enable_for_unicast": true,
    "enable_for_multicast_trunk": false
  }
}
```

建议：

1. 若你只做单播冗余，优先从 `SINGLE_BACKUP` 开始。
2. 若想控制副本爆炸，重点调 `max_extra_copies` 和 `risk_threshold`。
3. 做多播主干冗余时，再考虑 `TRUNK_ONLY` 和 `enable_for_multicast_trunk`。

## 8. 参数矩阵文件 `matrix.json`

矩阵文件通常独立存在，例如：

```json
{
  "experiment_name": "exp2_unicast_scale",
  "scenario_files": ["baseline.json"],
  "dimensions": [
    {
      "name": "traffic[0].bundle_count",
      "values": [20, 100, 500]
    },
    {
      "name": "simulation.phase1.k_paths",
      "values": [4, 8, 16]
    }
  ]
}
```

### 8.1 顶层字段

| 字段 | 是否必填 | 别名 | 说明 |
|---|---|---|---|
| `experiment_name` | 否 | `name` | 实验名称；独立矩阵文件缺省时取文件名 |
| `scenario_files` | 通常必填 | `scenarios` | 要展开的场景文件列表 |
| `dimensions` | 否 | 无 | 参数维度列表 |

每个 `dimensions` 元素支持：

| 字段 | 是否必填 | 说明 |
|---|---|---|
| `name` | 是 | 要覆盖的 JSON 路径 |
| `values` | 是 | 该维度的取值列表 |

支持的矩阵值类型：

- 布尔值
- 整数
- 浮点数
- 字符串

### 8.2 路径语法

矩阵维度的 `name` 使用 JSON 路径式语法：

| 写法 | 含义 |
|---|---|
| `simulation.owlt_margin` | 覆盖对象字段 |
| `traffic[0].bundle_count` | 覆盖数组第 0 项 |
| `traffic[*].payload_size` | 覆盖数组所有元素 |
| `failure_injection[0].trigger_time` | 覆盖第 1 条失效规则 |

还有两个特殊规则：

1. `failures` 和 `failure_injection` 互为别名，矩阵里写哪个都可以。
2. `multicast.group_size` 是一个合成维度，不直接对应 JSON 字段，而是会裁剪：
   - `multicast_groups[0].member_nodes`
   - 如果 `traffic` 里是 split-unicast 控制流，还会同步裁剪 `traffic` 数组长度

示例：

```json
{
  "experiment_name": "exp3_multicast_plan",
  "scenario_files": ["tree_plan.json", "split_unicast_control.json"],
  "dimensions": [
    {
      "name": "traffic[*].bundle_count",
      "values": [1, 2, 4]
    },
    {
      "name": "traffic[*].payload_size",
      "values": [128, 256]
    },
    {
      "name": "multicast.group_size",
      "values": [2, 3]
    }
  ]
}
```

### 8.3 运行结果命名

矩阵展开后：

1. 每个组合会生成 `matrix_0001` 这样的 `run_id`。
2. 场景名会追加为 `<scenario_name>__matrix_0001`。
3. 输出目录会落到：`<output_root>/<experiment>/<scenario>/matrix_0001/`。

## 9. 最小可运行模板

### 9.1 最小单播场景

```json
{
  "scenario_name": "minimal_unicast",
  "contact_plan_file": "contact_plan.ion",
  "simulation": {
    "end_time": 30.0
  },
  "traffic": [
    {
      "source_node": 1,
      "destination_node": 2,
      "start_time": 1.0,
      "payload_size": 64.0,
      "ttl": 30.0
    }
  ]
}
```

### 9.2 最小多播场景

```json
{
  "scenario_name": "minimal_multicast",
  "contact_plan_file": "contact_plan.ion",
  "multicast_groups": [
    {
      "group_id": "g1",
      "source_node": 1,
      "member_nodes": [4, 5]
    }
  ],
  "traffic": [
    {
      "source_node": 1,
      "multicast_group_id": "g1",
      "start_time": 2.0,
      "payload_size": 64.0,
      "ttl": 30.0
    }
  ]
}
```

## 10. 常见错误

### 10.1 同时写了 `contact_plan` 和 `contact_plan_file`

这是非法的。场景配置必须二选一。

### 10.2 多播流缺少组定义

如果 `traffic` 中写了 `multicast_group_id`，但 `multicast_groups` 里没有这个组，解析会失败。

### 10.3 `PROBABILISTIC_CONTACTS` 概率越界

`probability` 必须在 `[0, 1]` 之间。

### 10.4 矩阵路径写错

例如数组下标越界、字段名不存在、把对象字段当数组处理，都会导致矩阵展开失败。建议先从一个最小维度开始验证。

### 10.5 `ttl`、`end_time` 和 `trigger_time` 不协调

如果 `ttl` 太小、仿真窗口太短，或者失效触发时间超出窗口，结果会和预期不符。做实验时建议显式写出这三个时间参数，不要完全依赖默认值。

## 11. 建议的编写顺序

推荐按下面顺序写配置：

1. 先确认 `contact_plan_file` 或内联 `contact_plan` 可被单独解析。
2. 再写一个最小单播 `traffic`，确保场景能跑通。
3. 然后补 `simulation` 的窗口、`k_paths` 和增强项。
4. 需要失效实验时再增加 `failures`。
5. 需要冗余或多播时，再分别补 `redundancy` 和 `multicast_groups`。
6. 最后再把稳定场景抽成 `matrix.json` 做批量展开。

这样排查问题最快，因为每一步新增的变量最少。