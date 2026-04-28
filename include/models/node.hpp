#pragma once

#include "models/contact_plan.hpp"
#include "models/route.hpp"
#include "models/bundle.hpp"
#include <unordered_map>
#include <map>
#include <set>
#include <deque>
#include <utility>
#include <vector>

struct RouteCacheEntry {
    PlanVersion plan_version = 0;
    std::vector<Route> routes;
};

// DS-NOD-01 ~ DS-NOD-05: DTN 节点
class Node {
public:
    // DS-NOD-01: 唯一节点编号
    int node_number;

    // DS-NOD-04: 引用全局 ContactPlan
    ContactPlan* contact_plan = nullptr;

    // DS-NOD-02 / DS-CP-04: 路由表 destination_node -> {plan_version, RouteList}
    std::unordered_map<int, RouteCacheEntry> routing_table;

    // DS-NOD-03: 发送队列 (neighbor_node, priority) -> queue<Bundle>
    std::map<std::pair<int, int>, std::deque<Bundle>> send_queues;

    // DS-NOD-05: 排除邻居列表
    std::set<int> excluded_neighbors;

    explicit Node(int number = 0) : node_number(number) {}

    // 路由表管理
    PlanVersion current_plan_version() const noexcept;
    void invalidate_routes();
    void invalidate_stale_routes();
    std::vector<Route>& get_route_list(int destination);
    const std::vector<Route>* find_route_list(int destination) const;
};
