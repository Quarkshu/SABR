#pragma once

#include "models/contact.hpp"
#include <cstddef>
#include <string>
#include <vector>

struct Bundle;
class ContactPlan;

struct RouteLeg {
    Contact* contact = nullptr;
    TimePoint earliest_transmission_time = 0.0;
    TimePoint earliest_arrival_time = 0.0;
    TimePoint last_byte_transmission_time = 0.0;
    TimePoint last_byte_arrival_time = 0.0;
    Volume evl = 0.0;
};

// DS-RTE-01 ~ DS-RTE-08: 路由
struct Route {
    std::vector<RouteLeg> legs;

    bool local_delivery = false;
    NodeId local_delivery_node = -1;

    TimePoint best_case_delivery_time = 0.0;
    TimePoint termination_time = 0.0;
    NodeId entry_node = -1;

    // Phase 2 填充
    TimePoint eto = 0.0;
    TimePoint pbat = 0.0;
    Volume rvl = 0.0;

    // Anti-Loop 标记
    bool   possibly_looping = false;

    static Route make_local_delivery(NodeId local_node, TimePoint current_time);

    // DS-RTE-03: termination_time = min of all hops end_time
    TimePoint compute_termination_time() const;

    // DS-RTE-04: entry_node = hops[0]->receiving_node
    NodeId compute_entry_node() const;

    std::size_t hop_count() const noexcept { return legs.size(); }

    // DS-RTE-01: 结构约束校验
    bool is_valid() const;
};

struct CandidateRoute {
    Route route;
    bool valid = false;
    std::string reject_reason;
};
