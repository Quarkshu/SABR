#pragma once

#include "models/node.hpp"

#include <set>
#include <unordered_map>
#include <vector>

class Phase2 {
public:
    struct Config {
        TimePoint owlt_margin = 0.0;
        bool queue_delay_enhancement = false;
        bool anti_loop_reactive = false;
        bool anti_loop_proactive = false;
    };

    struct ValidationContext {
        bool rerouted_due_to_guard = false;
        std::unordered_map<ContactId, Volume> applicable_prior_contact_volume;
        std::unordered_map<ContactId, Volume> applicable_backlog_relief;
        std::unordered_map<ContactId, Volume> allocated_bytes;
    };

    struct Result {
        std::vector<CandidateRoute> evaluated_routes;
        std::vector<CandidateRoute> candidate_routes;
        bool need_recompute = false;
    };

    static Result validate(const Node& node,
                           const Bundle& bundle,
                           const std::vector<Route>& route_list,
                           TimePoint current_time,
                           const Config& config,
                           const ValidationContext& context);

    static Result validate(const Node& node,
                           const Bundle& bundle,
                           const std::vector<Route>& route_list,
                           TimePoint current_time,
                           const Config& config);

    static CandidateRoute validate_one(const Node& node,
                                       const Bundle& bundle,
                                       const Route& route,
                                       TimePoint current_time,
                                       const std::set<NodeId>& excluded_nodes,
                                       const Config& config,
                                       const ValidationContext& context);

private:
    static TimePoint compute_eto(const Contact& first_contact,
                                 const Node& node,
                                 const Bundle& bundle,
                                 TimePoint current_time,
                                 const ValidationContext& context);

    static TimePoint compute_pbat(Route& route,
                                  const Bundle& bundle,
                                  const ContactPlan& plan,
                                  TimePoint current_time,
                                  const Config& config,
                                  const ValidationContext& context);

    static Volume compute_rvl(Route& route,
                              const Bundle& bundle);

    static std::set<NodeId> build_excluded_nodes(const Node& node,
                                                 const Bundle& bundle,
                                                 const ValidationContext& context);

    static bool reject_by_best_case_expired(const Route& route,
                                            const Bundle& bundle);
    static bool reject_by_excluded_entry(const Route& route,
                                         const std::set<NodeId>& excluded_nodes);
    static bool reject_by_loop_to_local(const Route& route,
                                        NodeId local_node,
                                        NodeId destination);
    static bool reject_by_eto(const Route& route,
                              TimePoint eto);
    static bool reject_by_pbat(TimePoint pbat,
                               const Bundle& bundle);
    static bool reject_by_rvl_depleted(Volume rvl);
    static bool reject_by_fragmentation(Volume rvl,
                                        const Bundle& bundle);
    static bool detect_potential_loop(const Route& route,
                                      const Bundle& bundle,
                                      NodeId local_node);
};