#include "algorithms/multicast.hpp"

namespace {

void set_failure_reason(std::string* failure_reason,
                        std::string reason) {
    if (failure_reason != nullptr) {
        *failure_reason = std::move(reason);
    }
}

}  // namespace

bool MulticastGroupRegistry::validate_group(const MulticastGroup& group,
                                            std::string* failure_reason) {
    if (group.group_id.empty()) {
        set_failure_reason(failure_reason, "empty_group_id");
        return false;
    }

    if (group.source_node <= 0) {
        set_failure_reason(failure_reason, "invalid_source_node");
        return false;
    }

    if (group.member_nodes.empty()) {
        set_failure_reason(failure_reason, "empty_member_nodes");
        return false;
    }

    if (group.member_nodes.count(group.source_node) != 0u) {
        set_failure_reason(failure_reason, "source_is_member");
        return false;
    }

    for (const NodeId member : group.member_nodes) {
        if (member <= 0) {
            set_failure_reason(failure_reason, "invalid_member_node");
            return false;
        }
    }

    set_failure_reason(failure_reason, "");
    return true;
}

bool MulticastGroupRegistry::add_group(const MulticastGroup& group,
                                       std::string* failure_reason) {
    if (!validate_group(group, failure_reason)) {
        return false;
    }

    auto [it, inserted] = groups_.emplace(group.group_id, group);
    if (!inserted) {
        set_failure_reason(failure_reason, "group_exists");
        return false;
    }

    set_failure_reason(failure_reason, "");
    return true;
}

bool MulticastGroupRegistry::update_group(const MulticastGroup& group,
                                          std::string* failure_reason) {
    if (!validate_group(group, failure_reason)) {
        return false;
    }

    auto it = groups_.find(group.group_id);
    if (it == groups_.end()) {
        set_failure_reason(failure_reason, "group_not_found");
        return false;
    }

    it->second = group;
    set_failure_reason(failure_reason, "");
    return true;
}

bool MulticastGroupRegistry::remove_group(const std::string& group_id) {
    return groups_.erase(group_id) != 0u;
}

const MulticastGroup* MulticastGroupRegistry::find_group(const std::string& group_id) const {
    auto it = groups_.find(group_id);
    if (it == groups_.end()) {
        return nullptr;
    }

    return &it->second;
}

bool MulticastGroupRegistry::replace_groups(const std::vector<MulticastGroup>& groups,
                                            std::string* failure_reason) {
    std::map<std::string, MulticastGroup> replacement;
    for (const MulticastGroup& group : groups) {
        if (!validate_group(group, failure_reason)) {
            return false;
        }

        auto [it, inserted] = replacement.emplace(group.group_id, group);
        if (!inserted) {
            set_failure_reason(failure_reason, "duplicate_group_id");
            return false;
        }
    }

    groups_ = std::move(replacement);
    set_failure_reason(failure_reason, "");
    return true;
}

bool MulticastPlanner::validate_request(const MulticastPlanRequest& request,
                                        std::string* failure_reason) {
    if (request.source_node == nullptr) {
        set_failure_reason(failure_reason, "missing_source_node");
        return false;
    }

    if (request.bundle == nullptr) {
        set_failure_reason(failure_reason, "missing_bundle");
        return false;
    }

    if (request.destinations.empty()) {
        set_failure_reason(failure_reason, "empty_destinations");
        return false;
    }

    for (const NodeId destination : request.destinations) {
        if (destination <= 0) {
            set_failure_reason(failure_reason, "invalid_destination");
            return false;
        }

        if (destination == request.source_node->node_number) {
            set_failure_reason(failure_reason, "source_in_destinations");
            return false;
        }
    }

    set_failure_reason(failure_reason, "");
    return true;
}

MulticastExecutionResult MulticastPlanner::plan_forwarding(const Bundle& bundle,
                                                           NodeId at_node,
                                                           BundleId* next_bundle_id) {
    MulticastExecutionResult result;
    if (!bundle.is_multicast) {
        result.failure_reason = "bundle_not_multicast";
        return result;
    }

    Bundle base_bundle = bundle;
    if (base_bundle.origin_bundle_id == 0) {
        base_bundle.origin_bundle_id = base_bundle.id;
    }
    if (base_bundle.replica_id == 0) {
        base_bundle.replica_id = base_bundle.id;
    }

    if (base_bundle.pending_destinations.count(at_node) != 0u) {
        result.deliver_local = true;
        result.delivered_here.insert(at_node);
    }

    const TreeNode* current_node = base_bundle.multicast_plan.get_node(at_node);
    if (current_node == nullptr) {
        if (result.deliver_local) {
            return result;
        }

        result.failure_reason = "missing_tree_node";
        return result;
    }

    bool reuse_original = true;
    for (const auto& [next_hop, branch] : current_node->next_hops) {
        if (branch.contact_id == 0) {
            result.failure_reason = "missing_branch_contact";
            result.dispatches.clear();
            return result;
        }
        if (branch.destinations.empty()) {
            result.failure_reason = "empty_branch_destinations";
            result.dispatches.clear();
            return result;
        }

        MulticastDispatch dispatch;
        if (reuse_original) {
            dispatch.bundle = base_bundle;
            reuse_original = false;
        } else {
            if (next_bundle_id == nullptr || *next_bundle_id == 0) {
                result.failure_reason = "missing_next_bundle_id";
                result.dispatches.clear();
                return result;
            }
            dispatch.bundle = base_bundle.make_replica(*next_bundle_id, *next_bundle_id);
            ++(*next_bundle_id);
        }

        dispatch.contact_id = branch.contact_id;
        dispatch.next_hop = next_hop;
        dispatch.destinations = branch.destinations;

        dispatch.bundle.delivered_destinations.insert(result.delivered_here.begin(), result.delivered_here.end());
        dispatch.bundle.pending_destinations = branch.destinations;
        dispatch.bundle.multicast_plan = base_bundle.multicast_plan.prune(at_node, branch.destinations);
        dispatch.bundle.encoded_plan_version = base_bundle.encoded_plan_version;

        result.dispatches.push_back(std::move(dispatch));
    }

    return result;
}

std::string MulticastPlanner::merge_route(NodeId source_node,
                                          NodeId destination,
                                          const Route& route,
                                          MulticastTree* tree) {
    if (tree == nullptr) {
        return "missing_tree";
    }

    tree->root = source_node;
    tree->nodes.try_emplace(source_node, TreeNode{source_node, {}});

    if (route.local_delivery) {
        return "";
    }

    NodeId current = source_node;
    for (const RouteLeg& leg : route.legs) {
        if (leg.contact == nullptr) {
            return "missing_contact";
        }

        const NodeId next_hop = leg.contact->receiving_node;
        auto node_it = tree->nodes.find(current);
        if (node_it != tree->nodes.end()) {
            auto branch_it = node_it->second.next_hops.find(next_hop);
            if (branch_it != node_it->second.next_hops.end() &&
                branch_it->second.contact_id != leg.contact->contact_id) {
                return "conflicting_branch_contact";
            }
        }

        current = next_hop;
    }

    current = source_node;
    for (const RouteLeg& leg : route.legs) {
        const NodeId next_hop = leg.contact->receiving_node;
        TreeNode& node = tree->nodes[current];
        node.node_id = current;

        auto [branch_it, inserted] = node.next_hops.try_emplace(
            next_hop,
            BranchPlan{leg.contact->contact_id, {}});
        if (!inserted && branch_it->second.contact_id != leg.contact->contact_id) {
            return "conflicting_branch_contact";
        }

        branch_it->second.destinations.insert(destination);
        current = next_hop;
        tree->nodes.try_emplace(current, TreeNode{current, {}});
    }

    return "";
}

MulticastPlanResult MulticastPlanner::build_plan(const MulticastPlanRequest& request) const {
    MulticastPlanResult result;
    if (request.source_node != nullptr) {
        result.tree.root = request.source_node->node_number;
    }

    if (!validate_request(request, &result.failure_reason)) {
        return result;
    }

    for (const NodeId destination : request.destinations) {
        Bundle planning_bundle = *request.bundle;
        planning_bundle.destination_eid = destination;
        planning_bundle.is_critical = false;

        RoutingContext context(*request.source_node, planning_bundle, request.current_time);
        context.phase1_config = request.phase1_config;
        context.phase2_config = request.phase2_config;
        context.validation_context = request.validation_context;
        context.recompute_budget = request.recompute_budget;

        const BestRouteResult best_route = router_.select_best_unicast(context);
        if (!best_route.candidate.has_value()) {
            result.unreachable_destinations.insert(destination);
            result.failure_reasons[destination] = best_route.failure_reason.empty()
                ? "unreachable_destination"
                : best_route.failure_reason;
            continue;
        }

        const std::string merge_failure = merge_route(
            request.source_node->node_number,
            destination,
            best_route.candidate->route,
            &result.tree);
        if (!merge_failure.empty()) {
            result.unreachable_destinations.insert(destination);
            result.failure_reasons[destination] = merge_failure;
            continue;
        }

        result.reachable_destinations.insert(destination);
    }

    return result;
}

MulticastPlanResult MulticastPlanner::build_plan_for_group(
    const MulticastPlanRequest& request,
    const MulticastGroupRegistry& registry,
    const std::string& group_id) const {
    MulticastPlanResult result;
    if (request.source_node != nullptr) {
        result.tree.root = request.source_node->node_number;
    }

    if (request.source_node == nullptr) {
        result.failure_reason = "missing_source_node";
        return result;
    }

    if (request.bundle == nullptr) {
        result.failure_reason = "missing_bundle";
        return result;
    }

    const MulticastGroup* group = registry.find_group(group_id);
    if (group == nullptr) {
        result.failure_reason = "unknown_multicast_group";
        return result;
    }

    if (group->source_node != request.source_node->node_number) {
        result.failure_reason = "group_source_mismatch";
        return result;
    }

    MulticastPlanRequest group_request = request;
    group_request.destinations = group->member_nodes;
    return build_plan(group_request);
}