#include "algorithms/phase2.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr TimePoint kInfinity = std::numeric_limits<TimePoint>::infinity();
constexpr Volume kInfiniteVolume = std::numeric_limits<Volume>::infinity();
constexpr double kEpsilon = 1e-9;

int priority_index(Priority priority) {
    return static_cast<int>(priority);
}

Volume lookup_volume(const std::unordered_map<ContactId, Volume>& values,
                     ContactId contact_id) {
    auto it = values.find(contact_id);
    return it == values.end() ? 0.0 : it->second;
}

NodeId route_entry_node(const Route& route) {
    if (route.local_delivery) {
        return route.local_delivery_node;
    }
    if (route.entry_node >= 0) {
        return route.entry_node;
    }
    return route.compute_entry_node();
}

bool greater_than(TimePoint lhs, TimePoint rhs) {
    return lhs > rhs + kEpsilon;
}

std::set<NodeId> reachable_neighbors(const std::vector<Route>& routes) {
    std::set<NodeId> neighbors;
    for (const Route& route : routes) {
        if (!route.local_delivery) {
            const NodeId entry_node = route_entry_node(route);
            if (entry_node >= 0) {
                neighbors.insert(entry_node);
            }
        }
    }
    return neighbors;
}

std::set<NodeId> covered_neighbors(const std::vector<CandidateRoute>& candidates) {
    std::set<NodeId> neighbors;
    for (const CandidateRoute& candidate : candidates) {
        if (candidate.valid && !candidate.route.local_delivery) {
            const NodeId entry_node = route_entry_node(candidate.route);
            if (entry_node >= 0) {
                neighbors.insert(entry_node);
            }
        }
    }
    return neighbors;
}

}  // namespace

Phase2::Result Phase2::validate(const Node& node,
                                const Bundle& bundle,
                                const std::vector<Route>& route_list,
                                TimePoint current_time,
                                const Config& config,
                                const ValidationContext& context) {
    Result result;
    const std::set<NodeId> excluded_nodes = build_excluded_nodes(node, bundle, context);

    for (const Route& route : route_list) {
        CandidateRoute candidate = validate_one(
            node,
            bundle,
            route,
            current_time,
            excluded_nodes,
            config,
            context);
        result.evaluated_routes.push_back(candidate);
        if (candidate.valid) {
            result.candidate_routes.push_back(std::move(candidate));
        }
    }

    if (result.candidate_routes.empty()) {
        result.need_recompute = true;
    }

    if (bundle.is_critical) {
        const std::set<NodeId> expected_neighbors = reachable_neighbors(route_list);
        const std::set<NodeId> actual_neighbors = covered_neighbors(result.candidate_routes);
        if (!std::includes(actual_neighbors.begin(), actual_neighbors.end(),
                           expected_neighbors.begin(), expected_neighbors.end())) {
            result.need_recompute = true;
        }
    }

    return result;
}

Phase2::Result Phase2::validate(const Node& node,
                                const Bundle& bundle,
                                const std::vector<Route>& route_list,
                                TimePoint current_time,
                                const Config& config) {
    return validate(node, bundle, route_list, current_time, config, ValidationContext{});
}

CandidateRoute Phase2::validate_one(const Node& node,
                                    const Bundle& bundle,
                                    const Route& route,
                                    TimePoint current_time,
                                    const std::set<NodeId>& excluded_nodes,
                                    const Config& config,
                                    const ValidationContext& context) {
    CandidateRoute candidate;
    candidate.route = route;
    candidate.route.entry_node = route_entry_node(candidate.route);
    candidate.route.termination_time = candidate.route.compute_termination_time();
    candidate.route.eto = 0.0;
    candidate.route.pbat = 0.0;
    candidate.route.rvl = 0.0;
    candidate.route.possibly_looping = false;

    if (!candidate.route.is_valid()) {
        candidate.reject_reason = "invalid_route";
        return candidate;
    }

    if (candidate.route.local_delivery) {
        candidate.route.eto = current_time;
        candidate.route.pbat = current_time;
        candidate.route.rvl = kInfiniteVolume;
        candidate.valid = true;
        return candidate;
    }

    if (node.contact_plan == nullptr) {
        candidate.reject_reason = "missing_contact_plan";
        return candidate;
    }

    if (reject_by_best_case_expired(candidate.route, bundle)) {
        candidate.reject_reason = "best_case_expired";
        return candidate;
    }

    if (reject_by_excluded_entry(candidate.route, excluded_nodes)) {
        candidate.reject_reason = "excluded_entry";
        return candidate;
    }

    if (reject_by_loop_to_local(candidate.route, node.node_number, bundle.destination_eid)) {
        candidate.reject_reason = "loop_to_local";
        return candidate;
    }

    candidate.route.eto = compute_eto(
        *candidate.route.legs.front().contact,
        node,
        bundle,
        current_time,
        context);
    if (reject_by_eto(candidate.route, candidate.route.eto)) {
        candidate.reject_reason = "eto_too_late";
        return candidate;
    }

    candidate.route.pbat = compute_pbat(
        candidate.route,
        bundle,
        *node.contact_plan,
        current_time,
        config,
        context);
    if (!std::isfinite(candidate.route.pbat)) {
        candidate.reject_reason = "pbat_infeasible";
        return candidate;
    }
    if (reject_by_pbat(candidate.route.pbat, bundle)) {
        candidate.reject_reason = "pbat_expired";
        return candidate;
    }

    candidate.route.rvl = compute_rvl(candidate.route, bundle);
    if (reject_by_rvl_depleted(candidate.route.rvl)) {
        candidate.reject_reason = "rvl_depleted";
        return candidate;
    }
    if (reject_by_fragmentation(candidate.route.rvl, bundle)) {
        candidate.reject_reason = "fragmentation_required";
        return candidate;
    }

    if (config.anti_loop_proactive) {
        candidate.route.possibly_looping = detect_potential_loop(candidate.route, bundle, node.node_number);
    }

    candidate.valid = true;
    return candidate;
}

TimePoint Phase2::compute_eto(const Contact& first_contact,
                              const Node& node,
                              const Bundle& bundle,
                              TimePoint current_time,
                              const ValidationContext& context) {
    // REF-1 §3.2.6.2 adjusted start time
    const TimePoint adjusted_start = std::max(first_contact.start_time, current_time);

    // REF-1 §3.2.6.2 applicable backlog
    Volume applicable_backlog = 0.0;
    const int current_priority = priority_index(bundle.priority);
    for (const auto& [queue_key, queue] : node.send_queues) {
        const NodeId neighbor = queue_key.first;
        const int queued_priority = queue_key.second;
        if (neighbor != first_contact.receiving_node || queued_priority < current_priority) {
            continue;
        }

        for (const Bundle& queued_bundle : queue) {
            applicable_backlog += queued_bundle.evc();
        }
    }

    // REF-1 §3.2.6.2 applicable prior contact volume / backlog relief
    const Volume applicable_prior_contact_volume = lookup_volume(
        context.applicable_prior_contact_volume,
        first_contact.contact_id);
    const Volume applicable_backlog_relief = lookup_volume(
        context.applicable_backlog_relief,
        first_contact.contact_id);

    // REF-1 §3.2.6.2 residual backlog / backlog lien / ETO
    const Volume residual_backlog = std::max(
        0.0,
        applicable_backlog - applicable_prior_contact_volume + applicable_backlog_relief);
    if (first_contact.data_rate <= 0.0) {
        return kInfinity;
    }

    const TimePoint backlog_lien = residual_backlog / first_contact.data_rate;
    return adjusted_start + backlog_lien;
}

TimePoint Phase2::compute_pbat(Route& route,
                               const Bundle& bundle,
                               const ContactPlan& plan,
                               TimePoint current_time,
                               const Config& config,
                               const ValidationContext& context) {
    if (route.local_delivery) {
        return current_time;
    }

    const Volume bundle_evc = bundle.evc();
    for (std::size_t leg_index = 0; leg_index < route.legs.size(); ++leg_index) {
        RouteLeg& leg = route.legs[leg_index];
        if (leg.contact == nullptr || leg.contact->data_rate <= 0.0) {
            return kInfinity;
        }

        TimePoint first_byte_tx_time = 0.0;
        if (leg_index == 0) {
            first_byte_tx_time = route.eto;
        } else {
            const RouteLeg& previous_leg = route.legs[leg_index - 1];
            first_byte_tx_time = std::max(leg.contact->start_time, previous_leg.last_byte_arrival_time);

            if (config.queue_delay_enhancement) {
                const Volume allocated_bytes = lookup_volume(context.allocated_bytes, leg.contact->contact_id);
                first_byte_tx_time += allocated_bytes / leg.contact->data_rate;
            }
        }

        if (greater_than(first_byte_tx_time, leg.contact->end_time)) {
            return kInfinity;
        }

        const TimePoint last_byte_tx_time = first_byte_tx_time + bundle_evc / leg.contact->data_rate;
        const TimePoint owlt = plan.get_owlt(
            leg.contact->sending_node,
            leg.contact->receiving_node,
            first_byte_tx_time);
        const TimePoint first_byte_arrival_time = first_byte_tx_time + owlt + config.owlt_margin;
        const TimePoint last_byte_arrival_time = last_byte_tx_time + owlt + config.owlt_margin;

        leg.earliest_transmission_time = first_byte_tx_time;
        leg.earliest_arrival_time = first_byte_arrival_time;
        leg.last_byte_transmission_time = last_byte_tx_time;
        leg.last_byte_arrival_time = last_byte_arrival_time;
    }

    return route.legs.back().last_byte_arrival_time;
}

Volume Phase2::compute_rvl(Route& route,
                           const Bundle& bundle) {
    if (route.local_delivery) {
        return kInfiniteVolume;
    }

    const int bundle_priority = priority_index(bundle.priority);
    Volume rvl = kInfiniteVolume;
    for (RouteLeg& leg : route.legs) {
        if (leg.contact == nullptr) {
            return 0.0;
        }

        leg.evl = leg.contact->mtv[bundle_priority];
        rvl = std::min(rvl, leg.evl);
    }

    return std::isfinite(rvl) ? rvl : 0.0;
}

std::set<NodeId> Phase2::build_excluded_nodes(const Node& node,
                                              const Bundle& bundle,
                                              const ValidationContext& context) {
    std::set<NodeId> excluded_nodes(node.excluded_neighbors.begin(), node.excluded_neighbors.end());
    if (!context.rerouted_due_to_guard && bundle.previous_node >= 0) {
        excluded_nodes.insert(bundle.previous_node);
    }
    return excluded_nodes;
}

bool Phase2::reject_by_best_case_expired(const Route& route,
                                         const Bundle& bundle) {
    return greater_than(route.best_case_delivery_time, bundle.expiration_time());
}

bool Phase2::reject_by_excluded_entry(const Route& route,
                                      const std::set<NodeId>& excluded_nodes) {
    return excluded_nodes.find(route.entry_node) != excluded_nodes.end();
}

bool Phase2::reject_by_loop_to_local(const Route& route,
                                     NodeId local_node,
                                     NodeId destination) {
    if (destination == local_node) {
        return false;
    }

    for (const RouteLeg& leg : route.legs) {
        if (leg.contact != nullptr && leg.contact->receiving_node == local_node) {
            return true;
        }
    }
    return false;
}

bool Phase2::reject_by_eto(const Route& route,
                           TimePoint eto) {
    return route.legs.empty() || route.legs.front().contact == nullptr ||
           greater_than(eto, route.legs.front().contact->end_time);
}

bool Phase2::reject_by_pbat(TimePoint pbat,
                            const Bundle& bundle) {
    return greater_than(pbat, bundle.expiration_time());
}

bool Phase2::reject_by_rvl_depleted(Volume rvl) {
    return rvl <= kEpsilon;
}

bool Phase2::reject_by_fragmentation(Volume rvl,
                                     const Bundle& bundle) {
    return !bundle.allow_fragmentation && rvl + kEpsilon < bundle.evc();
}

bool Phase2::detect_potential_loop(const Route& route,
                                   const Bundle& bundle,
                                   NodeId local_node) {
    for (const RouteLeg& leg : route.legs) {
        if (leg.contact == nullptr) {
            continue;
        }

        if (leg.contact->sending_node != local_node && bundle.has_visited(leg.contact->sending_node)) {
            return true;
        }
        if (leg.contact->receiving_node != local_node && bundle.has_visited(leg.contact->receiving_node)) {
            return true;
        }
    }

    return false;
}