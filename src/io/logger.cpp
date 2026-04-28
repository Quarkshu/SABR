#include "io/logger.hpp"

#include <algorithm>

namespace {

RouteLegLog build_route_leg_log(const RouteLeg& leg) {
    RouteLegLog result;
    result.earliest_transmission_time = leg.earliest_transmission_time;
    result.earliest_arrival_time = leg.earliest_arrival_time;
    result.last_byte_transmission_time = leg.last_byte_transmission_time;
    result.last_byte_arrival_time = leg.last_byte_arrival_time;
    result.evl = leg.evl;
    if (leg.contact != nullptr) {
        result.contact_id = leg.contact->contact_id;
        result.sending_node = leg.contact->sending_node;
        result.receiving_node = leg.contact->receiving_node;
    }
    return result;
}

RouteLog build_route_log(const Route& route) {
    RouteLog result;
    result.local_delivery = route.local_delivery;
    result.local_delivery_node = route.local_delivery_node;
    result.best_case_delivery_time = route.best_case_delivery_time;
    result.termination_time = route.termination_time;
    result.entry_node = route.entry_node;
    result.eto = route.eto;
    result.pbat = route.pbat;
    result.rvl = route.rvl;
    result.possibly_looping = route.possibly_looping;
    result.legs.reserve(route.legs.size());
    for (const RouteLeg& leg : route.legs) {
        result.legs.push_back(build_route_leg_log(leg));
    }
    return result;
}

CandidateRouteLog build_candidate_log(const CandidateRoute& candidate) {
    CandidateRouteLog result;
    result.valid = candidate.valid;
    result.reject_reason = candidate.reject_reason;
    result.route = build_route_log(candidate.route);
    return result;
}

std::vector<ContactId> collect_contact_ids(const Route& route,
                                           std::size_t limit) {
    std::vector<ContactId> ids;
    for (const RouteLeg& leg : route.legs) {
        if (leg.contact != nullptr) {
            ids.push_back(leg.contact->contact_id);
        }
    }
    if (limit < ids.size()) {
        ids.resize(limit);
    }
    return ids;
}

}  // namespace

void Logger::record_event(const EventRecord& record) {
    events_.push_back(record);
}

void Logger::record_routing(const Bundle& bundle,
                            NodeId at_node,
                            TimePoint time,
                            std::size_t route_attempt,
                            const RoutingTrace& trace) {
    RoutingRecord record;
    record.bundle_id = bundle.id;
    record.at_node = at_node;
    record.time = time;
    record.route_attempt = route_attempt;
    record.action = trace.decision.action;
    record.failure_reason = trace.decision.failure_reason;
    record.reroute_reason = trace.reroute_reason;
    record.reroute_count = trace.reroute_count;
    record.reroute_excluded_neighbors = trace.reroute_excluded_neighbors;
    record.redundancy_considered = trace.redundancy_considered;
    if (trace.decision.primary.has_value()) {
        record.primary = build_route_log(trace.decision.primary->route);
    }
    for (const CandidateRoute& route : trace.decision.flood_routes) {
        record.flood_routes.push_back(build_route_log(route.route));
    }
    record.attempts.reserve(trace.attempts.size());
    for (std::size_t index = 0; index < trace.attempts.size(); ++index) {
        const RoutingAttemptTrace& trace_attempt = trace.attempts[index];
        RoutingAttemptLog attempt;
        attempt.attempt_index = index + 1;
        attempt.need_recompute = trace_attempt.need_recompute;
        attempt.phase1_routes.reserve(trace_attempt.phase1_routes.size());
        for (const Route& route : trace_attempt.phase1_routes) {
            attempt.phase1_routes.push_back(build_route_log(route));
        }
        attempt.phase2_candidates.reserve(trace_attempt.phase2_candidates.size());
        for (const CandidateRoute& candidate : trace_attempt.phase2_candidates) {
            attempt.phase2_candidates.push_back(build_candidate_log(candidate));
        }
        record.attempts.push_back(std::move(attempt));
    }
    routing_records_.push_back(std::move(record));
}

void Logger::record_commit(const Bundle& bundle,
                           const Route& route,
                           TimePoint time,
                           std::size_t committed_leg_count) {
    CommitRecord record;
    record.bundle_id = bundle.id;
    record.time = time;
    record.priority = bundle.priority;
    record.evc = bundle.evc();
    record.selected_contact_ids = collect_contact_ids(route, route.legs.size());
    record.committed_contact_ids = collect_contact_ids(route, committed_leg_count);
    commit_records_.push_back(std::move(record));
}

void Logger::record_plan_update(const PlanUpdateData& update,
                                TimePoint time,
                                std::size_t drained_bundle_count) {
    PlanUpdateRecord record;
    record.time = time;
    record.removed_contact_ids = update.removed_contact_ids;
    record.has_replacement_plan = update.replacement_plan != nullptr;
    record.label = update.label;
    record.drained_bundle_count = drained_bundle_count;
    plan_updates_.push_back(std::move(record));
}

void Logger::clear() {
    events_.clear();
    routing_records_.clear();
    commit_records_.clear();
    plan_updates_.clear();
}