#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

using ContactId = std::uint64_t;
using PlanVersion = std::uint64_t;
using NodeId = int;
using TimePoint = double;
using Volume = double;

// DS-CON-01 ~ DS-CON-07: Contact 数据结构
// 对应 [REF-1] §2.3.1
struct Contact {
    ContactId   contact_id = 0;
    TimePoint   start_time = 0.0;
    TimePoint   end_time = 0.0;
    NodeId      sending_node = 0;
    NodeId      receiving_node = 0;
    Volume      data_rate = 0.0;
    PlanVersion plan_version = 0;

    // MTV: 按优先级分层的剩余容量
    // 索引 0=Bulk, 1=Normal, 2=Expedited
    std::array<Volume, 3> mtv{};

    // DS-CON-02: volume = (end_time - start_time) * data_rate
    Volume volume() const;

    // DS-CON-05: end_time <= current_time 时为已终止
    bool is_terminated(TimePoint current_time) const;

    // DS-CON-03: 消耗 MTV：对指定优先级及所有更低优先级减去 evc
    void consume_mtv(int priority, Volume evc);

    // DS-CON-03: 初始化 MTV 为 volume()
    void initialize_mtv();

    // DS-CON-07: 记录 Contact 所属的接触计划版本
    void bind_plan_version(PlanVersion version) noexcept;
};

// DS-RNG-01 ~ DS-RNG-02: 距离区间
// 对应 [REF-1] §2.3.1
struct RangeInterval {
    TimePoint   start_time = 0.0;
    TimePoint   end_time = 0.0;
    NodeId      node_a = 0;
    NodeId      node_b = 0;
    TimePoint   distance_light_seconds = 0.0;

    // DS-RNG-02: 双向性——查询时不区分 a/b 顺序
    bool involves(NodeId n1, NodeId n2) const;
    TimePoint get_owlt(NodeId from, NodeId to) const;
};
