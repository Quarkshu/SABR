#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "models/contact.hpp"

// DS-BDL-06: 优先级枚举 — Bulk=0, Normal=1, Expedited=2
enum class Priority { BULK = 0, NORMAL = 1, EXPEDITED = 2 };

enum class ReplicaRole {
    PRIMARY,
    REDUNDANT_BACKUP,
    MULTICAST_TRUNK_BACKUP,
};

using BundleId = std::uint64_t;

struct BranchPlan {
    ContactId contact_id = 0;
    std::set<NodeId> destinations;
};

// FR-MT-04: 传播树节点
struct TreeNode {
    NodeId node_id = 0;
    // 下一跳 -> 该下一跳负责的目的节点子集
    std::map<NodeId, BranchPlan> next_hops;
};

// FR-MT-04, FR-MF-01 ~ FR-MF-02: 多播传播树
struct MulticastTree {
    NodeId root = -1;
    std::map<NodeId, TreeNode> nodes;

    bool is_branching_node(NodeId node_id) const;
    const TreeNode* get_node(NodeId node_id) const;
    MulticastTree prune(NodeId subtree_root, const std::set<NodeId>& destinations) const;
    std::string serialize() const;
    static MulticastTree deserialize(const std::string& data);
};

// DS-BDL-01 ~ DS-BDL-09: Bundle 数据包
struct Bundle {
    BundleId        id = 0;
    NodeId          source_node = 0;
    NodeId          destination_eid = -1;
    TimePoint       creation_time = 0.0;
    TimePoint       ttl = 0.0;
    Volume          payload_size = 0.0;
    Volume          header_size = 0.0;
    Priority        priority = Priority::BULK;
    bool            is_critical = false;
    bool            is_multicast = false;
    bool            allow_fragmentation = false;

    // 逐跳追踪
    NodeId           previous_node = -1;
    std::vector<NodeId> visited_nodes;

    // 多播扩展
    std::set<NodeId> pending_destinations;
    std::set<NodeId> delivered_destinations;
    MulticastTree multicast_plan;
    PlanVersion encoded_plan_version = 0;
    BundleId origin_bundle_id = 0;
    BundleId parent_bundle_id = 0;
    BundleId replica_id = 0;
    ReplicaRole replica_role = ReplicaRole::PRIMARY;
    bool redundancy_applied = false;

    // DS-BDL-02: expiration_time = creation_time + ttl
    TimePoint expiration_time() const;

    // DS-BDL-03: EVC = h + p + max(0.03*(h+p), 100)
    Volume evc() const;

    Volume multicast_plan_size() const;
    bool has_visited(NodeId node_id) const;
    void mark_visited(NodeId node_id);
    bool mark_destination_delivered(NodeId node_id);
    Bundle make_replica(BundleId new_bundle_id, BundleId new_replica_id) const;
    BundleId canonical_origin_id() const noexcept;
    bool participates_in_redundancy() const noexcept;
    std::string redundancy_scope_key() const;

    // 统计追踪
    std::vector<NodeId> route_path;
    TimePoint delivered_time = -1;
};
