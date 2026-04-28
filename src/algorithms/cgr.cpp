#include "algorithms/cgr.hpp"

#include <algorithm>
#include <utility>

namespace {

RoutingDecision route_fail(std::string reason) {
    RoutingDecision decision;
    decision.failure_reason = std::move(reason);
    return decision;
}

bool has_destination_contact(const ContactPlan& plan,
                             NodeId destination) {
    return !plan.get_contacts_to(destination).empty();
}

RoutingTrace route_fail_trace(std::string reason) {
    RoutingTrace trace;
    trace.decision.failure_reason = std::move(reason);
    return trace;
}

}  // namespace

const std::vector<Route>& CGRRouter::plan_unicast(const RoutingContext& context) const {
    return Phase1::compute(
        context.node,
        context.bundle.destination_eid,
        context.current_time,
        context.phase1_config);
}

Phase2::Result CGRRouter::evaluate_unicast(const RoutingContext& context,
                                           const std::vector<Route>& routes) const {
    return Phase2::validate(
        context.node,
        context.bundle,
        routes,
        context.current_time,
        context.phase2_config,
        context.validation_context);
}

std::optional<Phase2::Result> CGRRouter::collect_unicast_candidates(
    const RoutingContext& context,
    std::vector<RoutingAttemptTrace>* attempts,
    std::string* failure_reason) const {
    if (context.node.contact_plan != nullptr) {
        context.node.contact_plan->purge_terminated(context.current_time);
        context.node.invalidate_stale_routes();
    } else if (context.bundle.destination_eid != context.node.node_number) {
        if (failure_reason != nullptr) {
            *failure_reason = "missing_contact_plan";
        }
        return std::nullopt;
    }

    if (context.bundle.destination_eid != context.node.node_number &&
        context.node.contact_plan != nullptr &&
        !has_destination_contact(*context.node.contact_plan, context.bundle.destination_eid)) {
        if (failure_reason != nullptr) {
            *failure_reason = "cgr_not_applicable";
        }
        return std::nullopt;
    }

    const std::vector<Route>* routes = &plan_unicast(context);
    Phase2::Result validation = evaluate_unicast(context, *routes);
    if (attempts != nullptr) {
        attempts->push_back({*routes, validation.evaluated_routes, validation.need_recompute});
    }

    int remaining_recomputes = std::max(0, context.recompute_budget);
    while (validation.need_recompute && remaining_recomputes > 0) {
        routes = &Phase1::recompute_more(
            context.node,
            context.bundle.destination_eid,
            context.current_time,
            context.phase1_config,
            1);
        validation = evaluate_unicast(context, *routes);
        if (attempts != nullptr) {
            attempts->push_back({*routes, validation.evaluated_routes, validation.need_recompute});
        }
        --remaining_recomputes;
    }

    if (validation.candidate_routes.empty()) {
        if (failure_reason != nullptr) {
            *failure_reason = "no_candidate_routes";
        }
        return std::nullopt;
    }

    return validation;
}

BestRouteResult CGRRouter::select_best_unicast(const RoutingContext& context) const {
    BestRouteResult result;
    std::optional<Phase2::Result> validation = collect_unicast_candidates(context, nullptr, &result.failure_reason);
    if (!validation.has_value()) {
        return result;
    }

    result.candidate = phase3_.select_best(validation->candidate_routes);
    if (!result.candidate.has_value()) {
        result.failure_reason = "phase3_no_selection";
    }

    return result;
}

RoutingTrace CGRRouter::route_unicast_with_trace(const RoutingContext& context) const {
    RoutingTrace trace;
    std::string failure_reason;
    std::optional<Phase2::Result> validation = collect_unicast_candidates(context, &trace.attempts, &failure_reason);
    if (!validation.has_value()) {
        trace.decision = route_fail(std::move(failure_reason));
        return trace;
    }

    if (context.bundle.destination_eid == context.node.node_number) {
        std::optional<CandidateRoute> selected = phase3_.select_best(validation->candidate_routes);
        if (!selected.has_value()) {
            trace.decision = route_fail("phase3_no_selection");
            return trace;
        }

        trace.decision.action = RoutingAction::DELIVER_LOCAL;
        trace.decision.primary = std::move(*selected);
        return trace;
    }

    if (context.bundle.is_critical) {
        std::vector<CandidateRoute> selected_routes = phase3_.select_flood_set(validation->candidate_routes);
        if (selected_routes.empty()) {
            trace.decision = route_fail("phase3_no_selection");
            return trace;
        }

        trace.decision.action = RoutingAction::FORWARD_FLOOD;
        trace.decision.flood_routes = std::move(selected_routes);
        return trace;
    }

    std::optional<CandidateRoute> selected = phase3_.select_best(validation->candidate_routes);
    if (!selected.has_value()) {
        trace.decision = route_fail("phase3_no_selection");
        return trace;
    }

    trace.redundancy_considered = redundancy_manager_.should_consider(
        context.bundle,
        false,
        context.redundancy_config);
    if (trace.redundancy_considered && !selected->route.local_delivery) {
        trace.backup_routes = redundancy_manager_.select_backups(
            context.bundle,
            validation->candidate_routes,
            *selected,
            context.current_time,
            context.redundancy_config);
    }

    trace.decision.action = selected->route.local_delivery ? RoutingAction::DELIVER_LOCAL
                                                           : RoutingAction::FORWARD_ONE;
    trace.decision.primary = std::move(*selected);
    return trace;
}

RoutingDecision CGRRouter::route_unicast(const RoutingContext& context) const {
    return route_unicast_with_trace(context).decision;
}

void CGRRouter::commit_route(const Bundle& bundle,
                             const Route& route) const {
    if (route.local_delivery) {
        return;
    }

    const int priority = static_cast<int>(bundle.priority);
    const Volume evc = bundle.evc();
    for (const RouteLeg& leg : route.legs) {
        if (leg.contact != nullptr) {
            leg.contact->consume_mtv(priority, evc);
        }
    }
}