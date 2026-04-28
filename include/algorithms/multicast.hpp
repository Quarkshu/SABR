#pragma once

#include "algorithms/cgr.hpp"

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

struct MulticastGroup {
    std::string group_id;
    NodeId source_node = -1;
    std::set<NodeId> member_nodes;
};

class MulticastGroupRegistry {
public:
    bool add_group(const MulticastGroup& group, std::string* failure_reason = nullptr);
    bool update_group(const MulticastGroup& group, std::string* failure_reason = nullptr);
    bool remove_group(const std::string& group_id);
    const MulticastGroup* find_group(const std::string& group_id) const;
    bool replace_groups(const std::vector<MulticastGroup>& groups,
                        std::string* failure_reason = nullptr);

    std::size_t size() const noexcept { return groups_.size(); }
    bool empty() const noexcept { return groups_.empty(); }

private:
    static bool validate_group(const MulticastGroup& group, std::string* failure_reason);

    std::map<std::string, MulticastGroup> groups_;
};

struct MulticastPlanRequest {
    Node* source_node = nullptr;
    const Bundle* bundle = nullptr;
    std::set<NodeId> destinations;
    TimePoint current_time = 0.0;
    Phase1::Config phase1_config{};
    Phase2::Config phase2_config{};
    Phase2::ValidationContext validation_context{};
    int recompute_budget = 1;
};

struct MulticastPlanResult {
    MulticastTree tree;
    std::set<NodeId> reachable_destinations;
    std::set<NodeId> unreachable_destinations;
    std::map<NodeId, std::string> failure_reasons;
    std::string failure_reason;

    bool ok() const noexcept { return failure_reason.empty(); }
    bool has_partial_failures() const noexcept { return !unreachable_destinations.empty(); }
};

struct MulticastDispatch {
    Bundle bundle;
    ContactId contact_id = 0;
    NodeId next_hop = -1;
    std::set<NodeId> destinations;
};

struct MulticastExecutionResult {
    bool deliver_local = false;
    std::set<NodeId> delivered_here;
    std::vector<MulticastDispatch> dispatches;
    std::string failure_reason;

    bool ok() const noexcept { return failure_reason.empty(); }
};

class MulticastPlanner {
public:
    MulticastPlanResult build_plan(const MulticastPlanRequest& request) const;
    MulticastPlanResult build_plan_for_group(const MulticastPlanRequest& request,
                                             const MulticastGroupRegistry& registry,
                                             const std::string& group_id) const;
    static MulticastExecutionResult plan_forwarding(const Bundle& bundle,
                                                    NodeId at_node,
                                                    BundleId* next_bundle_id);

private:
    static bool validate_request(const MulticastPlanRequest& request, std::string* failure_reason);
    static std::string merge_route(NodeId source_node,
                                   NodeId destination,
                                   const Route& route,
                                   MulticastTree* tree);

    CGRRouter router_;
};