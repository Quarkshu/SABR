#include "models/node.hpp"

PlanVersion Node::current_plan_version() const noexcept {
    return contact_plan == nullptr ? 0 : contact_plan->version();
}

// DS-NOD-02/DS-CP-03: 当 contact_plan 被修改时清空路由表
void Node::invalidate_routes() {
    routing_table.clear();
}

void Node::invalidate_stale_routes() {
    const PlanVersion current_version = current_plan_version();
    for (auto it = routing_table.begin(); it != routing_table.end(); ) {
        if (it->second.plan_version != current_version) {
            it = routing_table.erase(it);
        } else {
            ++it;
        }
    }
}

// 获取指定目的节点的路由列表（不存在则创建空列表）
std::vector<Route>& Node::get_route_list(int destination) {
    RouteCacheEntry& entry = routing_table[destination];
    const PlanVersion current_version = current_plan_version();
    if (entry.plan_version != current_version) {
        entry.plan_version = current_version;
        entry.routes.clear();
    }
    return entry.routes;
}

const std::vector<Route>* Node::find_route_list(int destination) const {
    auto it = routing_table.find(destination);
    if (it == routing_table.end()) {
        return nullptr;
    }
    if (it->second.plan_version != current_plan_version()) {
        return nullptr;
    }
    return &it->second.routes;
}
