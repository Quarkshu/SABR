#pragma once

#include "algorithms/phase1.hpp"
#include "algorithms/phase2.hpp"
#include "algorithms/phase3.hpp"
#include "algorithms/redundancy.hpp"

struct RoutingContext {
    Node& node;
    const Bundle& bundle;
    TimePoint current_time;
    Phase1::Config phase1_config{};
    Phase2::Config phase2_config{};
    Phase2::ValidationContext validation_context{};
    RedundancyConfig redundancy_config{};
    int recompute_budget = 1;

    RoutingContext(Node& local_node,
                   const Bundle& local_bundle,
                   TimePoint time_point)
        : node(local_node), bundle(local_bundle), current_time(time_point) {}
};

struct RoutingAttemptTrace {
    std::vector<Route> phase1_routes;
    std::vector<CandidateRoute> phase2_candidates;
    bool need_recompute = false;
};

struct RoutingTrace {
    std::vector<RoutingAttemptTrace> attempts;
    RoutingDecision decision;
    std::string reroute_reason;
    std::size_t reroute_count = 0;
    std::vector<NodeId> reroute_excluded_neighbors;
    bool redundancy_considered = false;
    std::vector<CandidateRoute> backup_routes;
};

struct BestRouteResult {
    std::optional<CandidateRoute> candidate;
    std::string failure_reason;
};

class CGRRouter {
public:
    const std::vector<Route>& plan_unicast(const RoutingContext& context) const;
    Phase2::Result evaluate_unicast(const RoutingContext& context,
                                    const std::vector<Route>& routes) const;
    BestRouteResult select_best_unicast(const RoutingContext& context) const;
    RoutingTrace route_unicast_with_trace(const RoutingContext& context) const;
    RoutingDecision route_unicast(const RoutingContext& context) const;
    void commit_route(const Bundle& bundle,
                      const Route& route) const;

private:
    std::optional<Phase2::Result> collect_unicast_candidates(
        const RoutingContext& context,
        std::vector<RoutingAttemptTrace>* attempts,
        std::string* failure_reason) const;
    Phase3Selector phase3_;
    RedundancyManager redundancy_manager_;
};