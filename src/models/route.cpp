#include "models/route.hpp"
#include <algorithm>
#include <limits>

Route Route::make_local_delivery(NodeId local_node, TimePoint current_time) {
    Route route;
    route.local_delivery = true;
    route.local_delivery_node = local_node;
    route.best_case_delivery_time = current_time;
    route.termination_time = current_time;
    route.entry_node = local_node;
    return route;
}

// DS-RTE-03: termination_time = min of all hops end_time
TimePoint Route::compute_termination_time() const {
    if (local_delivery) {
        return best_case_delivery_time;
    }
    if (legs.empty()) {
        return 0.0;
    }
    double min_end = std::numeric_limits<double>::max();
    for (const auto& leg : legs) {
        if (leg.contact == nullptr) {
            return 0.0;
        }
        min_end = std::min(min_end, leg.contact->end_time);
    }
    return min_end;
}

// DS-RTE-04: entry_node = hops[0]->receiving_node
NodeId Route::compute_entry_node() const {
    if (local_delivery) {
        return local_delivery_node;
    }
    if (legs.empty() || legs.front().contact == nullptr) return -1;
    return legs.front().contact->receiving_node;
}

// DS-RTE-01: 结构约束校验
// (a) 首个 Contact 的发送节点为本地节点（由调用方保证）
// (b) 末个 Contact 的接收节点为目的节点（由调用方保证）
// (c) 第 i 个 Contact 的 receiving_node == 第 i+1 个 Contact 的 sending_node
// (d) 第 i+1 个 Contact 的 end_time >= 第 i 个 Contact 的 start_time
bool Route::is_valid() const {
    if (local_delivery) {
        return legs.empty() && local_delivery_node >= 0;
    }

    if (legs.empty()) return false;

    for (const auto& leg : legs) {
        if (leg.contact == nullptr) {
            return false;
        }
    }

    for (size_t i = 0; i + 1 < legs.size(); ++i) {
        // 约束 (c): 连续性
        if (legs[i].contact->receiving_node != legs[i + 1].contact->sending_node) {
            return false;
        }
        // 约束 (d): 时间顺序
        if (legs[i + 1].contact->end_time < legs[i].contact->start_time) {
            return false;
        }
    }

    return true;
}
