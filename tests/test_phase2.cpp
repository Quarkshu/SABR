#include <gtest/gtest.h>

#include "algorithms/phase2.hpp"

#include <initializer_list>
#include <vector>

namespace {

Contact make_contact(ContactId id,
                     TimePoint start_time,
                     TimePoint end_time,
                     NodeId from,
                     NodeId to,
                     Volume data_rate) {
    return Contact{id, start_time, end_time, from, to, data_rate, 0, {}};
}

RangeInterval make_range(TimePoint start_time,
                         TimePoint end_time,
                         NodeId node_a,
                         NodeId node_b,
                         TimePoint distance_light_seconds) {
    return RangeInterval{start_time, end_time, node_a, node_b, distance_light_seconds};
}

Bundle make_bundle(TimePoint ttl = 100.0,
                   Priority priority = Priority::BULK,
                   bool allow_fragmentation = true,
                   bool is_critical = false,
                   Volume payload_size = 0.0,
                   Volume header_size = 0.0) {
    Bundle bundle;
    bundle.source_node = 1;
    bundle.destination_eid = 3;
    bundle.creation_time = 0.0;
    bundle.ttl = ttl;
    bundle.priority = priority;
    bundle.allow_fragmentation = allow_fragmentation;
    bundle.is_critical = is_critical;
    bundle.payload_size = payload_size;
    bundle.header_size = header_size;
    return bundle;
}

Route make_route(ContactPlan& plan,
                 std::initializer_list<ContactId> contact_ids,
                 TimePoint best_case_delivery_time) {
    Route route;
    for (const ContactId contact_id : contact_ids) {
        route.legs.push_back({plan.get_contact(contact_id)});
    }
    route.best_case_delivery_time = best_case_delivery_time;
    route.termination_time = route.compute_termination_time();
    route.entry_node = route.compute_entry_node();
    return route;
}

ContactPlan make_single_hop_plan(TimePoint end_time = 100.0,
                                 Volume data_rate = 10.0) {
    ContactPlan plan;
    plan.set_contacts({make_contact(1, 0.0, end_time, 1, 2, data_rate)});
    plan.set_ranges({make_range(0.0, 1000.0, 1, 2, 1.0)});
    return plan;
}

ContactPlan make_two_hop_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 100.0, 1, 2, 10.0),
        make_contact(2, 0.0, 100.0, 2, 3, 5.0),
    });
    plan.set_ranges({
        make_range(0.0, 1000.0, 1, 2, 1.0),
        make_range(0.0, 1000.0, 2, 3, 2.0),
    });
    return plan;
}

ContactPlan make_loop_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 100.0, 1, 2, 10.0),
        make_contact(2, 0.0, 100.0, 2, 1, 10.0),
        make_contact(3, 0.0, 100.0, 1, 3, 10.0),
    });
    plan.set_ranges({
        make_range(0.0, 1000.0, 1, 2, 1.0),
        make_range(0.0, 1000.0, 2, 1, 1.0),
        make_range(0.0, 1000.0, 1, 3, 1.0),
    });
    return plan;
}

ContactPlan make_dual_neighbor_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 100.0, 1, 2, 10.0),
        make_contact(2, 0.0, 100.0, 1, 3, 10.0),
        make_contact(3, 0.0, 100.0, 2, 4, 10.0),
        make_contact(4, 0.0, 100.0, 3, 4, 10.0),
    });
    plan.set_ranges({
        make_range(0.0, 1000.0, 1, 2, 1.0),
        make_range(0.0, 1000.0, 1, 3, 1.0),
        make_range(0.0, 1000.0, 2, 4, 1.0),
        make_range(0.0, 1000.0, 3, 4, 1.0),
    });
    return plan;
}

}  // namespace

class Phase2Test : public ::testing::Test {
protected:
    Phase2::Config config;
    Phase2::ValidationContext context;
};

TEST_F(Phase2Test, LocalDeliveryRouteValidatesImmediately) {
    ContactPlan plan = make_single_hop_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle();
    bundle.destination_eid = 1;
    const Route route = Route::make_local_delivery(1, 8.0);

    const CandidateRoute candidate = Phase2::validate_one(
        node,
        bundle,
        route,
        8.0,
        {},
        config,
        context);

    EXPECT_TRUE(candidate.valid);
    EXPECT_TRUE(candidate.reject_reason.empty());
    EXPECT_DOUBLE_EQ(candidate.route.eto, 8.0);
    EXPECT_DOUBLE_EQ(candidate.route.pbat, 8.0);
}

TEST_F(Phase2Test, ComputesFullEtoFormulaFromQueueAndContext) {
    ContactPlan plan = make_single_hop_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle queued_bulk = make_bundle(100.0, Priority::BULK, true, false);
    Bundle queued_normal = make_bundle(100.0, Priority::NORMAL, true, false);
    node.send_queues[{2, static_cast<int>(Priority::BULK)}].push_back(queued_bulk);
    node.send_queues[{2, static_cast<int>(Priority::NORMAL)}].push_back(queued_normal);

    Bundle bundle = make_bundle(200.0, Priority::BULK, true, false);
    bundle.destination_eid = 2;

    context.applicable_prior_contact_volume[1] = 50.0;
    context.applicable_backlog_relief[1] = 20.0;

    Route route = make_route(plan, {1}, 10.0);
    const CandidateRoute candidate = Phase2::validate_one(
        node,
        bundle,
        route,
        5.0,
        {},
        config,
        context);

    ASSERT_TRUE(candidate.valid);
    EXPECT_DOUBLE_EQ(candidate.route.eto, 22.0);
    EXPECT_DOUBLE_EQ(candidate.route.legs[0].earliest_transmission_time, 22.0);
    EXPECT_DOUBLE_EQ(candidate.route.legs[0].last_byte_transmission_time, 32.0);
    EXPECT_DOUBLE_EQ(candidate.route.legs[0].earliest_arrival_time, 23.0);
    EXPECT_DOUBLE_EQ(candidate.route.legs[0].last_byte_arrival_time, 33.0);
    EXPECT_DOUBLE_EQ(candidate.route.pbat, 33.0);
}

TEST_F(Phase2Test, QueueDelayEnhancementDelaysSecondHopPbat) {
    ContactPlan plan = make_two_hop_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(200.0, Priority::BULK, true, false);
    bundle.destination_eid = 3;
    Route route = make_route(plan, {1, 2}, 10.0);

    context.allocated_bytes[2] = 25.0;

    const CandidateRoute without_queue_delay = Phase2::validate_one(
        node,
        bundle,
        route,
        0.0,
        {},
        config,
        context);

    config.queue_delay_enhancement = true;
    const CandidateRoute with_queue_delay = Phase2::validate_one(
        node,
        bundle,
        route,
        0.0,
        {},
        config,
        context);

    ASSERT_TRUE(without_queue_delay.valid);
    ASSERT_TRUE(with_queue_delay.valid);
    EXPECT_DOUBLE_EQ(without_queue_delay.route.pbat, 33.0);
    EXPECT_DOUBLE_EQ(with_queue_delay.route.pbat, 38.0);
    EXPECT_DOUBLE_EQ(with_queue_delay.route.legs[1].earliest_transmission_time, 16.0);
    EXPECT_DOUBLE_EQ(with_queue_delay.route.legs[1].last_byte_transmission_time, 36.0);
    EXPECT_DOUBLE_EQ(with_queue_delay.route.legs[1].last_byte_arrival_time, 38.0);
}

TEST_F(Phase2Test, RejectsBestCaseExpiredRule) {
    ContactPlan plan = make_single_hop_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(10.0);
    bundle.destination_eid = 2;
    Route route = make_route(plan, {1}, 15.0);

    const CandidateRoute candidate = Phase2::validate_one(node, bundle, route, 0.0, {}, config, context);

    EXPECT_FALSE(candidate.valid);
    EXPECT_EQ(candidate.reject_reason, "best_case_expired");
}

TEST_F(Phase2Test, RejectsExcludedEntryRule) {
    ContactPlan plan = make_single_hop_plan();
    Node node(1);
    node.contact_plan = &plan;
    node.excluded_neighbors.insert(2);

    Bundle bundle = make_bundle(100.0);
    bundle.destination_eid = 2;
    Route route = make_route(plan, {1}, 5.0);

    const Phase2::Result result = Phase2::validate(node, bundle, {route}, 0.0, config, context);

    EXPECT_TRUE(result.candidate_routes.empty());
    EXPECT_TRUE(result.need_recompute);
}

TEST_F(Phase2Test, RejectsLoopToLocalRule) {
    ContactPlan plan = make_loop_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(100.0);
    bundle.destination_eid = 3;
    Route route = make_route(plan, {1, 2, 3}, 5.0);

    const CandidateRoute candidate = Phase2::validate_one(node, bundle, route, 0.0, {}, config, context);

    EXPECT_FALSE(candidate.valid);
    EXPECT_EQ(candidate.reject_reason, "loop_to_local");
}

TEST_F(Phase2Test, RejectsEtoTooLateRule) {
    ContactPlan plan = make_single_hop_plan(10.0, 10.0);
    Node node(1);
    node.contact_plan = &plan;
    node.send_queues[{2, static_cast<int>(Priority::BULK)}].push_back(make_bundle(100.0));

    Bundle bundle = make_bundle(100.0);
    bundle.destination_eid = 2;
    Route route = make_route(plan, {1}, 5.0);

    const CandidateRoute candidate = Phase2::validate_one(node, bundle, route, 5.0, {}, config, context);

    EXPECT_FALSE(candidate.valid);
    EXPECT_EQ(candidate.reject_reason, "eto_too_late");
}

TEST_F(Phase2Test, RejectsPbatExpiredRule) {
    ContactPlan plan = make_two_hop_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(20.0);
    bundle.destination_eid = 3;
    Route route = make_route(plan, {1, 2}, 10.0);

    const CandidateRoute candidate = Phase2::validate_one(node, bundle, route, 0.0, {}, config, context);

    EXPECT_FALSE(candidate.valid);
    EXPECT_EQ(candidate.reject_reason, "pbat_expired");
}

TEST_F(Phase2Test, RejectsDepletedRvlRule) {
    ContactPlan plan = make_single_hop_plan();
    Node node(1);
    node.contact_plan = &plan;
    plan.get_contact(1)->mtv[static_cast<int>(Priority::BULK)] = 0.0;

    Bundle bundle = make_bundle(100.0, Priority::BULK, true, false);
    bundle.destination_eid = 2;
    Route route = make_route(plan, {1}, 5.0);

    const CandidateRoute candidate = Phase2::validate_one(node, bundle, route, 0.0, {}, config, context);

    EXPECT_FALSE(candidate.valid);
    EXPECT_EQ(candidate.reject_reason, "rvl_depleted");
}

TEST_F(Phase2Test, RejectsFragmentationRuleWhenDisabled) {
    ContactPlan plan = make_single_hop_plan();
    Node node(1);
    node.contact_plan = &plan;
    plan.get_contact(1)->mtv[static_cast<int>(Priority::BULK)] = 50.0;

    Bundle bundle = make_bundle(100.0, Priority::BULK, false, false);
    bundle.destination_eid = 2;
    Route route = make_route(plan, {1}, 5.0);

    const CandidateRoute candidate = Phase2::validate_one(node, bundle, route, 0.0, {}, config, context);

    EXPECT_FALSE(candidate.valid);
    EXPECT_EQ(candidate.reject_reason, "fragmentation_required");
}

TEST_F(Phase2Test, MarksPotentialLoopWhenVisitedNodeAppearsOnRoute) {
    ContactPlan plan = make_two_hop_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle(100.0);
    bundle.destination_eid = 3;
    bundle.visited_nodes = {2};
    Route route = make_route(plan, {1, 2}, 10.0);

    config.anti_loop_proactive = true;
    const CandidateRoute candidate = Phase2::validate_one(node, bundle, route, 0.0, {}, config, context);

    ASSERT_TRUE(candidate.valid);
    EXPECT_TRUE(candidate.route.possibly_looping);
}

TEST_F(Phase2Test, CriticalBundleRequestsRecomputeWhenNeighborCoverageMissing) {
    ContactPlan plan = make_dual_neighbor_plan();
    Node node(1);
    node.contact_plan = &plan;
    node.excluded_neighbors.insert(3);

    Bundle bundle = make_bundle(100.0, Priority::BULK, true, true);
    bundle.destination_eid = 4;
    const Route route_a = make_route(plan, {1, 3}, 5.0);
    const Route route_b = make_route(plan, {2, 4}, 5.0);

    const Phase2::Result result = Phase2::validate(node, bundle, {route_a, route_b}, 0.0, config, context);

    ASSERT_EQ(result.candidate_routes.size(), 1u);
    EXPECT_EQ(result.candidate_routes.front().route.entry_node, 2);
    EXPECT_TRUE(result.need_recompute);
}