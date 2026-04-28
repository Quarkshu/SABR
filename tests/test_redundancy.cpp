#include <gtest/gtest.h>

#include "algorithms/cgr.hpp"
#include "algorithms/redundancy.hpp"
#include "models/contact_plan.hpp"

#include <initializer_list>
#include <vector>

namespace {

Contact make_contact(ContactId id,
                     TimePoint start_time,
                     TimePoint end_time,
                     NodeId from,
                     NodeId to,
                     Volume data_rate = 500.0) {
    return Contact{id, start_time, end_time, from, to, data_rate, 0, {}};
}

RangeInterval make_range(TimePoint start_time,
                         TimePoint end_time,
                         NodeId node_a,
                         NodeId node_b,
                         TimePoint distance_light_seconds) {
    return RangeInterval{start_time, end_time, node_a, node_b, distance_light_seconds};
}

ContactPlan make_redundancy_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 12.0, 1, 2),
        make_contact(2, 13.0, 24.0, 2, 5),
        make_contact(3, 0.0, 12.0, 1, 3),
        make_contact(4, 14.0, 30.0, 3, 5),
        make_contact(5, 0.0, 12.0, 1, 2),
        make_contact(6, 13.0, 24.0, 2, 4),
        make_contact(7, 25.0, 38.0, 4, 5),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 5, 1.0),
        make_range(0.0, 100.0, 1, 3, 1.0),
        make_range(0.0, 100.0, 3, 5, 1.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
        make_range(0.0, 100.0, 4, 5, 1.0),
    });
    return plan;
}

Bundle make_bundle() {
    Bundle bundle;
    bundle.id = 100;
    bundle.source_node = 1;
    bundle.destination_eid = 5;
    bundle.creation_time = 0.0;
    bundle.ttl = 60.0;
    bundle.payload_size = 64.0;
    bundle.header_size = 16.0;
    bundle.priority = Priority::NORMAL;
    bundle.allow_fragmentation = true;
    return bundle;
}

Route make_route(ContactPlan& plan,
                 std::initializer_list<ContactId> contact_ids,
                 TimePoint best_case_delivery_time) {
    Route route;
    for (ContactId contact_id : contact_ids) {
        RouteLeg leg;
        leg.contact = plan.get_contact(contact_id);
        route.legs.push_back(leg);
    }
    route.entry_node = route.compute_entry_node();
    route.best_case_delivery_time = best_case_delivery_time;
    route.termination_time = route.compute_termination_time();
    route.pbat = best_case_delivery_time;
    return route;
}

CandidateRoute make_candidate(Route route) {
    CandidateRoute candidate;
    candidate.route = std::move(route);
    candidate.valid = true;
    return candidate;
}

std::vector<ContactId> contact_ids(const Route& route) {
    std::vector<ContactId> ids;
    for (const RouteLeg& leg : route.legs) {
        ids.push_back(leg.contact->contact_id);
    }
    return ids;
}

}  // namespace

TEST(RouteDiversityAnalyzerTest, PrefersDisjointEntryNeighbors) {
    ContactPlan plan = make_redundancy_plan();
    RouteDiversityAnalyzer analyzer;

    const Route primary = make_route(plan, {1, 2}, 20.0);
    const Route alternate = make_route(plan, {3, 4}, 24.0);
    const Route shared_entry = make_route(plan, {5, 6, 7}, 28.0);

    EXPECT_GT(analyzer.score(primary, alternate), analyzer.score(primary, shared_entry));
    EXPECT_GT(analyzer.score(primary, alternate), 0.9);
}

TEST(RedundancyManagerTest, SelectsSingleBackupWhenPrimaryRiskExceedsThreshold) {
    ContactPlan plan = make_redundancy_plan();
    Bundle bundle = make_bundle();

    const CandidateRoute primary = make_candidate(make_route(plan, {1, 2}, 20.0));
    const CandidateRoute first_backup = make_candidate(make_route(plan, {3, 4}, 24.0));
    const CandidateRoute shared_entry = make_candidate(make_route(plan, {5, 6, 7}, 28.0));

    RedundancyConfig config;
    config.mode = RedundancyMode::SINGLE_BACKUP;
    config.max_extra_copies = 1;
    config.risk_threshold = 0.55;
    config.min_diversity_score = 0.5;
    config.min_capacity_reserve_ratio = 0.1;
    config.min_ttl_slack = 4.0;

    RedundancyManager manager;
    const std::vector<CandidateRoute> backups = manager.select_backups(
        bundle,
        {primary, first_backup, shared_entry},
        primary,
        0.0,
        config);

    ASSERT_EQ(backups.size(), 1u);
    EXPECT_EQ(contact_ids(backups.front().route), (std::vector<ContactId>{3, 4}));
}

TEST(RedundancyManagerTest, SkipsAlreadyExpandedFamilies) {
    ContactPlan plan = make_redundancy_plan();
    Bundle bundle = make_bundle();
    bundle.redundancy_applied = true;

    const CandidateRoute primary = make_candidate(make_route(plan, {1, 2}, 20.0));
    const CandidateRoute first_backup = make_candidate(make_route(plan, {3, 4}, 24.0));

    RedundancyConfig config;
    config.mode = RedundancyMode::MULTI_BACKUP;
    config.max_extra_copies = 2;
    config.risk_threshold = 0.0;

    RedundancyManager manager;
    EXPECT_TRUE(manager.select_backups(bundle, {primary, first_backup}, primary, 0.0, config).empty());
}

TEST(RedundancyManagerTest, RouterTraceIncludesBackupRouteWhenAlternativesExist) {
    ContactPlan plan = make_redundancy_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_bundle();
    RoutingContext context(node, bundle, 0.0);
    context.phase1_config.k_paths = 4;
    context.redundancy_config.mode = RedundancyMode::SINGLE_BACKUP;
    context.redundancy_config.max_extra_copies = 1;
    context.redundancy_config.risk_threshold = 0.0;
    context.redundancy_config.min_diversity_score = 0.5;
    context.redundancy_config.min_capacity_reserve_ratio = 0.0;
    context.redundancy_config.min_ttl_slack = 0.0;

    CGRRouter router;
    const RoutingTrace trace = router.route_unicast_with_trace(context);

    ASSERT_TRUE(trace.decision.primary.has_value());
    EXPECT_TRUE(trace.redundancy_considered);
    ASSERT_EQ(trace.backup_routes.size(), 1u);
    EXPECT_EQ(contact_ids(trace.backup_routes.front().route), (std::vector<ContactId>{3, 4}));
}