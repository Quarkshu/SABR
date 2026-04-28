#include <gtest/gtest.h>

#include "algorithms/cgr.hpp"

#include <array>
#include <initializer_list>
#include <set>
#include <vector>

namespace {

Contact make_contact(ContactId id,
                     TimePoint start_time,
                     TimePoint end_time,
                     NodeId from,
                     NodeId to,
                     Volume data_rate = 10.0) {
    return Contact{id, start_time, end_time, from, to, data_rate, 0, {}};
}

RangeInterval make_range(TimePoint start_time,
                         TimePoint end_time,
                         NodeId node_a,
                         NodeId node_b,
                         TimePoint distance_light_seconds) {
    return RangeInterval{start_time, end_time, node_a, node_b, distance_light_seconds};
}

Bundle make_bundle(NodeId destination,
                   Priority priority = Priority::BULK,
                   bool is_critical = false,
                   Volume payload_size = 0.0,
                   Volume header_size = 0.0) {
    Bundle bundle;
    bundle.source_node = 1;
    bundle.destination_eid = destination;
    bundle.creation_time = 0.0;
    bundle.ttl = 200.0;
    bundle.priority = priority;
    bundle.allow_fragmentation = true;
    bundle.is_critical = is_critical;
    bundle.payload_size = payload_size;
    bundle.header_size = header_size;
    return bundle;
}

Route make_route(ContactPlan& plan,
                 std::initializer_list<ContactId> contact_ids,
                 TimePoint best_case_delivery_time,
                 TimePoint pbat = 0.0,
                 bool possibly_looping = false) {
    Route route;
    for (const ContactId contact_id : contact_ids) {
        route.legs.push_back({plan.get_contact(contact_id)});
    }
    route.best_case_delivery_time = best_case_delivery_time;
    route.termination_time = route.compute_termination_time();
    route.entry_node = route.compute_entry_node();
    route.pbat = pbat;
    route.possibly_looping = possibly_looping;
    return route;
}

CandidateRoute make_candidate(Route route,
                              bool valid = true) {
    CandidateRoute candidate;
    candidate.route = std::move(route);
    candidate.valid = valid;
    return candidate;
}

std::vector<ContactId> signature(const Route& route) {
    if (route.local_delivery) {
        return {};
    }

    std::vector<ContactId> result;
    for (const RouteLeg& leg : route.legs) {
        result.push_back(leg.contact->contact_id);
    }
    return result;
}

std::array<Volume, 3> mtv_snapshot(const Contact& contact) {
    return contact.mtv;
}

ContactPlan make_phase3_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 40.0, 1, 2),
        make_contact(2, 0.0, 40.0, 1, 3),
        make_contact(3, 0.0, 50.0, 2, 4),
        make_contact(4, 0.0, 60.0, 3, 4),
        make_contact(5, 0.0, 50.0, 1, 4),
        make_contact(6, 0.0, 50.0, 1, 5),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 1, 3, 1.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
        make_range(0.0, 100.0, 3, 4, 1.0),
        make_range(0.0, 100.0, 1, 4, 1.0),
        make_range(0.0, 100.0, 1, 5, 1.0),
    });
    return plan;
}

ContactPlan make_diamond_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2),
        make_contact(2, 0.0, 10.0, 1, 3),
        make_contact(3, 12.0, 20.0, 2, 4),
        make_contact(4, 11.0, 20.0, 3, 4),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 1, 3, 2.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
        make_range(0.0, 100.0, 3, 4, 5.0),
    });
    return plan;
}

}  // namespace

TEST(Phase3SelectorTest, SelectBestAppliesFourLevelComparator) {
    ContactPlan plan = make_phase3_plan();
    Phase3Selector selector;

    {
        const CandidateRoute lower_pbat = make_candidate(make_route(plan, {1, 3}, 5.0, 20.0));
        const CandidateRoute higher_pbat = make_candidate(make_route(plan, {5}, 5.0, 21.0));
        const std::optional<CandidateRoute> selected = selector.select_best({higher_pbat, lower_pbat});
        ASSERT_TRUE(selected.has_value());
        EXPECT_EQ(signature(selected->route), (std::vector<ContactId>{1, 3}));
    }

    {
        const CandidateRoute fewer_hops = make_candidate(make_route(plan, {5}, 5.0, 20.0));
        const CandidateRoute more_hops = make_candidate(make_route(plan, {1, 3}, 5.0, 20.0));
        const std::optional<CandidateRoute> selected = selector.select_best({more_hops, fewer_hops});
        ASSERT_TRUE(selected.has_value());
        EXPECT_EQ(signature(selected->route), (std::vector<ContactId>{5}));
    }

    {
        const CandidateRoute later_termination = make_candidate(make_route(plan, {5}, 5.0, 20.0));
        const CandidateRoute earlier_termination = make_candidate(make_route(plan, {1}, 5.0, 20.0));
        const std::optional<CandidateRoute> selected = selector.select_best({earlier_termination, later_termination});
        ASSERT_TRUE(selected.has_value());
        EXPECT_EQ(signature(selected->route), (std::vector<ContactId>{5}));
    }

    {
        const CandidateRoute smaller_entry = make_candidate(make_route(plan, {1}, 5.0, 20.0));
        const CandidateRoute larger_entry = make_candidate(make_route(plan, {2}, 5.0, 20.0));
        const std::optional<CandidateRoute> selected = selector.select_best({larger_entry, smaller_entry});
        ASSERT_TRUE(selected.has_value());
        EXPECT_EQ(signature(selected->route), (std::vector<ContactId>{1}));
    }
}

TEST(Phase3SelectorTest, SelectBestPrefersNonLoopingCandidate) {
    ContactPlan plan = make_phase3_plan();
    Phase3Selector selector;

    const CandidateRoute looping = make_candidate(make_route(plan, {5}, 5.0, 10.0, true));
    const CandidateRoute non_looping = make_candidate(make_route(plan, {1, 3}, 5.0, 20.0, false));

    const std::optional<CandidateRoute> selected = selector.select_best({looping, non_looping});

    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(signature(selected->route), (std::vector<ContactId>{1, 3}));
    EXPECT_FALSE(selected->route.possibly_looping);
}

TEST(Phase3SelectorTest, SelectFloodSetChoosesBestRoutePerNeighbor) {
    ContactPlan plan = make_phase3_plan();
    Phase3Selector selector;

    const CandidateRoute neighbor_two_looping = make_candidate(make_route(plan, {1}, 5.0, 10.0, true));
    const CandidateRoute neighbor_two_non_looping = make_candidate(make_route(plan, {1, 3}, 5.0, 15.0, false));
    const CandidateRoute neighbor_three_only = make_candidate(make_route(plan, {2}, 5.0, 8.0, true));

    const std::vector<CandidateRoute> selected = selector.select_flood_set(
        {neighbor_two_looping, neighbor_two_non_looping, neighbor_three_only});

    ASSERT_EQ(selected.size(), 2u);
    EXPECT_EQ(signature(selected[0].route), (std::vector<ContactId>{1, 3}));
    EXPECT_EQ(signature(selected[1].route), (std::vector<ContactId>{2}));
}

TEST(CGRRouterTest, RouteUnicastReturnsRouteFailWhenCgrIsNotApplicable) {
    ContactPlan plan = make_diamond_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(5);
    RoutingContext context(node, bundle, 0.0);

    CGRRouter router;
    const RoutingDecision decision = router.route_unicast(context);

    EXPECT_EQ(decision.action, RoutingAction::ROUTE_FAIL);
    EXPECT_EQ(decision.failure_reason, "cgr_not_applicable");
    EXPECT_FALSE(decision.primary.has_value());
}

TEST(CGRRouterTest, RouteUnicastRecomputesMoreWhenInitialCandidatesAreEmpty) {
    ContactPlan plan = make_diamond_plan();
    Node node(1);
    node.contact_plan = &plan;
    node.excluded_neighbors.insert(2);

    Bundle bundle = make_bundle(4);
    RoutingContext context(node, bundle, 0.0);
    context.phase1_config.k_paths = 1;
    context.recompute_budget = 1;

    CGRRouter router;
    const RoutingDecision decision = router.route_unicast(context);

    ASSERT_EQ(decision.action, RoutingAction::FORWARD_ONE);
    ASSERT_TRUE(decision.primary.has_value());
    EXPECT_EQ(signature(decision.primary->route), (std::vector<ContactId>{2, 4}));
}

TEST(CGRRouterTest, RouteUnicastWithTraceCapturesRecomputeAttempts) {
    ContactPlan plan = make_diamond_plan();
    Node node(1);
    node.contact_plan = &plan;
    node.excluded_neighbors.insert(2);

    Bundle bundle = make_bundle(4);
    RoutingContext context(node, bundle, 0.0);
    context.phase1_config.k_paths = 1;
    context.recompute_budget = 1;

    CGRRouter router;
    const RoutingTrace trace = router.route_unicast_with_trace(context);

    ASSERT_EQ(trace.decision.action, RoutingAction::FORWARD_ONE);
    ASSERT_TRUE(trace.decision.primary.has_value());
    EXPECT_EQ(signature(trace.decision.primary->route), (std::vector<ContactId>{2, 4}));
    ASSERT_EQ(trace.attempts.size(), 2u);
    EXPECT_TRUE(trace.attempts[0].need_recompute);
    EXPECT_FALSE(trace.attempts[1].need_recompute);
    ASSERT_EQ(trace.attempts[0].phase1_routes.size(), 1u);
    EXPECT_EQ(signature(trace.attempts[0].phase1_routes.front()), (std::vector<ContactId>{1, 3}));
    ASSERT_EQ(trace.attempts[0].phase2_candidates.size(), 1u);
    EXPECT_FALSE(trace.attempts[0].phase2_candidates.front().valid);
    EXPECT_EQ(trace.attempts[0].phase2_candidates.front().reject_reason, "excluded_entry");
    ASSERT_EQ(trace.attempts[1].phase1_routes.size(), 2u);
    ASSERT_EQ(trace.attempts[1].phase2_candidates.size(), 2u);
}

TEST(CGRRouterTest, RouteUnicastReturnsFloodDecisionForCriticalBundle) {
    ContactPlan plan = make_diamond_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(4, Priority::BULK, true);
    RoutingContext context(node, bundle, 0.0);
    context.phase1_config.k_paths = 2;

    CGRRouter router;
    const RoutingDecision decision = router.route_unicast(context);

    ASSERT_EQ(decision.action, RoutingAction::FORWARD_FLOOD);
    ASSERT_EQ(decision.flood_routes.size(), 2u);
    std::set<NodeId> entry_nodes;
    for (const CandidateRoute& candidate : decision.flood_routes) {
        entry_nodes.insert(candidate.route.entry_node);
    }
    EXPECT_EQ(entry_nodes, (std::set<NodeId>{2, 3}));
}

TEST(CGRRouterTest, SelectBestUnicastReturnsPrimaryCandidateWithoutConsumingMtv) {
    ContactPlan plan = make_phase3_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(4, Priority::NORMAL, false, 200.0);
    RoutingContext context(node, bundle, 0.0);
    context.phase1_config.k_paths = 2;

    Contact* first_hop = plan.get_contact(5);
    ASSERT_NE(first_hop, nullptr);
    const std::array<Volume, 3> before = mtv_snapshot(*first_hop);

    CGRRouter router;
    const BestRouteResult result = router.select_best_unicast(context);

    EXPECT_TRUE(result.failure_reason.empty());
    ASSERT_TRUE(result.candidate.has_value());
    EXPECT_EQ(signature(result.candidate->route), (std::vector<ContactId>{5}));
    EXPECT_EQ(mtv_snapshot(*first_hop), before);
}

TEST(CGRRouterTest, SelectBestUnicastKeepsSingleRouteSemanticsForCriticalBundle) {
    ContactPlan plan = make_phase3_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(4, Priority::BULK, true);
    RoutingContext context(node, bundle, 0.0);
    context.phase1_config.k_paths = 2;

    CGRRouter router;
    const BestRouteResult result = router.select_best_unicast(context);

    EXPECT_TRUE(result.failure_reason.empty());
    ASSERT_TRUE(result.candidate.has_value());
    EXPECT_EQ(signature(result.candidate->route), (std::vector<ContactId>{5}));
}

TEST(CGRRouterTest, PlanningDoesNotConsumeMtvUntilCommit) {
    ContactPlan plan = make_phase3_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(4, Priority::NORMAL, false, 200.0);
    RoutingContext context(node, bundle, 0.0);
    context.phase1_config.k_paths = 2;

    Contact* first_hop = plan.get_contact(5);
    ASSERT_NE(first_hop, nullptr);
    const std::array<Volume, 3> before = mtv_snapshot(*first_hop);

    CGRRouter router;
    const RoutingDecision decision = router.route_unicast(context);

    ASSERT_EQ(decision.action, RoutingAction::FORWARD_ONE);
    ASSERT_TRUE(decision.primary.has_value());
    EXPECT_EQ(mtv_snapshot(*first_hop), before);

    router.commit_route(bundle, decision.primary->route);

    EXPECT_EQ(first_hop->mtv[static_cast<int>(Priority::BULK)], before[0] - 300.0);
    EXPECT_EQ(first_hop->mtv[static_cast<int>(Priority::NORMAL)], before[1] - 300.0);
    EXPECT_EQ(first_hop->mtv[static_cast<int>(Priority::EXPEDITED)], before[2]);
}