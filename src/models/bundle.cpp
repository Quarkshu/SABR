#include "models/bundle.hpp"

#include <sstream>

// DS-BDL-02: expiration_time = creation_time + ttl
TimePoint Bundle::expiration_time() const {
    return creation_time + ttl;
}

// DS-BDL-03: EVC = h + p + max(0.03*(h+p), 100)
Volume Bundle::evc() const {
    Volume total = header_size + payload_size + multicast_plan_size();
    return total + std::max(0.03 * total, 100.0);
}

Volume Bundle::multicast_plan_size() const {
    if (!is_multicast) {
        return 0.0;
    }
    return static_cast<Volume>(multicast_plan.serialize().size());
}

bool Bundle::has_visited(NodeId node_id) const {
    return std::find(visited_nodes.begin(), visited_nodes.end(), node_id) != visited_nodes.end();
}

void Bundle::mark_visited(NodeId node_id) {
    visited_nodes.push_back(node_id);
}

bool Bundle::mark_destination_delivered(NodeId node_id) {
    auto pending_it = pending_destinations.find(node_id);
    bool changed = pending_it != pending_destinations.end() || !delivered_destinations.count(node_id);
    if (pending_it != pending_destinations.end()) {
        pending_destinations.erase(pending_it);
    }
    delivered_destinations.insert(node_id);
    return changed;
}

Bundle Bundle::make_replica(BundleId new_bundle_id, BundleId new_replica_id) const {
    Bundle replica = *this;
    replica.id = new_bundle_id;
    replica.parent_bundle_id = id;
    replica.origin_bundle_id = origin_bundle_id == 0 ? id : origin_bundle_id;
    replica.replica_id = new_bundle_id == id ? replica_id : new_replica_id;
    replica.delivered_time = -1;
    return replica;
}

BundleId Bundle::canonical_origin_id() const noexcept {
    return origin_bundle_id == 0 ? id : origin_bundle_id;
}

bool Bundle::participates_in_redundancy() const noexcept {
    return redundancy_applied || replica_role != ReplicaRole::PRIMARY;
}

std::string Bundle::redundancy_scope_key() const {
    if (!is_multicast) {
        return "u:" + std::to_string(destination_eid);
    }

    std::ostringstream stream;
    stream << "m";
    for (NodeId destination : pending_destinations) {
        stream << ':' << destination;
    }
    return stream.str();
}

// FR-MT-04: 判断分叉节点（有多个下一跳）
bool MulticastTree::is_branching_node(NodeId node_id) const {
    auto it = nodes.find(node_id);
    if (it == nodes.end()) return false;
    return it->second.next_hops.size() > 1;
}

const TreeNode* MulticastTree::get_node(NodeId node_id) const {
    auto it = nodes.find(node_id);
    if (it == nodes.end()) return nullptr;
    return &it->second;
}

// 裁剪子树：返回仅包含指定目的节点子集的子树
MulticastTree MulticastTree::prune(NodeId subtree_root, const std::set<NodeId>& destinations) const {
    MulticastTree result;
    result.root = subtree_root;

    // BFS/DFS 从 subtree_root 向下遍历，只保留通往 destinations 中节点的路径
    std::vector<NodeId> stack = {subtree_root};
    while (!stack.empty()) {
        NodeId current = stack.back();
        stack.pop_back();

        auto it = nodes.find(current);
        if (it == nodes.end()) continue;

        TreeNode pruned_node;
        pruned_node.node_id = current;

        for (const auto& [next_hop, branch] : it->second.next_hops) {
            std::set<NodeId> filtered;
            for (NodeId d : branch.destinations) {
                if (destinations.count(d)) {
                    filtered.insert(d);
                }
            }
            if (!filtered.empty()) {
                pruned_node.next_hops[next_hop] = {branch.contact_id, filtered};
                stack.push_back(next_hop);
            }
        }

        if (!pruned_node.next_hops.empty() || destinations.count(current)) {
            result.nodes[current] = pruned_node;
        }
    }

    return result;
}

// 简单序列化：格式 "root;node_id:next_hop@contact_id=d1,d2:next_hop@contact_id=d3;..."
std::string MulticastTree::serialize() const {
    std::string s = std::to_string(root);
    for (const auto& [nid, tnode] : nodes) {
        s += ";" + std::to_string(nid);
        for (const auto& [nh, branch] : tnode.next_hops) {
            s += ":" + std::to_string(nh) + "@" + std::to_string(branch.contact_id) + "=";
            bool first = true;
            for (NodeId d : branch.destinations) {
                if (!first) s += ",";
                s += std::to_string(d);
                first = false;
            }
        }
    }
    return s;
}

// 反序列化
MulticastTree MulticastTree::deserialize(const std::string& data) {
    MulticastTree tree;
    if (data.empty()) return tree;

    // 按 ';' 分割
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < data.size()) {
        size_t end = data.find(';', start);
        if (end == std::string::npos) end = data.size();
        parts.push_back(data.substr(start, end - start));
        start = end + 1;
    }

    if (parts.empty()) return tree;
    tree.root = std::stoi(parts[0]);

    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string& part = parts[i];
        // 格式: node_id:next_hop@contact_id=d1,d2:next_hop@contact_id=d3
        size_t colon_pos = part.find(':');
        if (colon_pos == std::string::npos) {
            // 叶节点, 无下一跳
            NodeId nid = std::stoi(part);
            TreeNode tn;
            tn.node_id = nid;
            tree.nodes[nid] = tn;
            continue;
        }

        NodeId nid = std::stoi(part.substr(0, colon_pos));
        TreeNode tn;
        tn.node_id = nid;

        // 解析多个 next_hop@contact_id=d1,d2 段
        size_t seg_start = colon_pos + 1;
        while (seg_start < part.size()) {
            size_t seg_end = part.find(':', seg_start);
            if (seg_end == std::string::npos) seg_end = part.size();
            std::string seg = part.substr(seg_start, seg_end - seg_start);

            size_t at_pos = seg.find('@');
            size_t eq_pos = seg.find('=');
            if (at_pos != std::string::npos && eq_pos != std::string::npos && at_pos < eq_pos) {
                NodeId nh = std::stoi(seg.substr(0, at_pos));
                ContactId contact_id = static_cast<ContactId>(std::stoull(seg.substr(at_pos + 1, eq_pos - at_pos - 1)));
                std::set<NodeId> dests;
                size_t d_start = eq_pos + 1;
                while (d_start < seg.size()) {
                    size_t d_end = seg.find(',', d_start);
                    if (d_end == std::string::npos) d_end = seg.size();
                    dests.insert(std::stoi(seg.substr(d_start, d_end - d_start)));
                    d_start = d_end + 1;
                }
                tn.next_hops[nh] = {contact_id, dests};
            }

            seg_start = seg_end + 1;
        }

        tree.nodes[nid] = tn;
    }

    return tree;
}
