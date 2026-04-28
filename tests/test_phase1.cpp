#include <gtest/gtest.h>

#include "algorithms/phase1.hpp"

#include <set>
#include <vector>

namespace {

Contact make_contact(ContactId id,
                     TimePoint start_time,
                     TimePoint end_time,
                     NodeId from,
                     NodeId to,
                     Volume data_rate = 1.0) {
    return Contact{id, start_time, end_time, from, to, data_rate, 0, {}};
}

RangeInterval make_range(TimePoint start_time,
                         TimePoint end_time,
                         NodeId node_a,
                         NodeId node_b,
                         TimePoint distance_light_seconds) {
    return RangeInterval{start_time, end_time, node_a, node_b, distance_light_seconds};
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

ContactPlan make_cache_update_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2),
        make_contact(2, 12.0, 20.0, 2, 4),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
    });
    return plan;
}

}  // namespace

class Phase1Test : public ::testing::Test {
protected:
    Phase1::Config config;
};

TEST_F(Phase1Test, ReusesCachedRoutesWhenPlanVersionMatches) {
    ContactPlan plan = make_diamond_plan();
    Node node(1);
    node.contact_plan = &plan;

    config.k_paths = 1;
    const std::vector<Route>& first = Phase1::compute(node, 4, 0.0, config);
    const TimePoint first_delivery = first.front().best_case_delivery_time;
    const std::vector<Route>& second = Phase1::compute(node, 4, 5.0, config);

    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(&first, &second);
    EXPECT_DOUBLE_EQ(second.front().best_case_delivery_time, first_delivery);
    EXPECT_EQ(node.routing_table.at(4).plan_version, plan.version());
}

TEST_F(Phase1Test, RecomputesAfterPlanVersionChanges) {
    ContactPlan plan = make_cache_update_plan();
    Node node(1);
    node.contact_plan = &plan;

    config.k_paths = 1;
    const std::vector<Route>& first = Phase1::compute(node, 4, 0.0, config);
    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(signature(first.front()), (std::vector<ContactId>{1, 2}));
    const TimePoint first_delivery_time = first.front().best_case_delivery_time;

    plan.add_contact(make_contact(3, 0.0, 3.0, 1, 4));
    plan.add_range(make_range(0.0, 100.0, 1, 4, 0.5));

    const std::vector<Route>& second = Phase1::compute(node, 4, 0.0, config);

    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(signature(second.front()), (std::vector<ContactId>{3}));
    EXPECT_LT(second.front().best_case_delivery_time, first_delivery_time);
    EXPECT_EQ(node.routing_table.at(4).plan_version, plan.version());
}

TEST_F(Phase1Test, RecomputeMoreExpandsRouteList) {
    ContactPlan plan = make_diamond_plan();
    Node node(1);
    node.contact_plan = &plan;

    config.k_paths = 1;
    const std::vector<Route>& first = Phase1::compute(node, 4, 0.0, config);
    ASSERT_EQ(first.size(), 1u);

    const std::vector<Route>& second = Phase1::recompute_more(node, 4, 0.0, config, 1);

    ASSERT_EQ(second.size(), 2u);
    EXPECT_EQ(signature(second[0]), (std::vector<ContactId>{1, 3}));
    EXPECT_EQ(signature(second[1]), (std::vector<ContactId>{2, 4}));
}

TEST_F(Phase1Test, OneRoutePerNeighborAddsMissingNeighborCoverage) {
    ContactPlan plan = make_diamond_plan();
    Node node(1);
    node.contact_plan = &plan;

    config.k_paths = 1;
    config.one_route_per_neighbor = true;
    const std::vector<Route>& routes = Phase1::compute(node, 4, 0.0, config);

    ASSERT_EQ(routes.size(), 2u);
    std::set<NodeId> entry_nodes;
    for (const Route& route : routes) {
        entry_nodes.insert(route.entry_node);
    }
    EXPECT_EQ(entry_nodes, (std::set<NodeId>{2, 3}));
}

TEST_F(Phase1Test, ReturnsLocalDeliveryRouteForSameNodeDestination) {
    ContactPlan plan = make_diamond_plan();
    Node node(4);
    node.contact_plan = &plan;

    const std::vector<Route>& routes = Phase1::compute(node, 4, 8.0, config);

    ASSERT_EQ(routes.size(), 1u);
    EXPECT_TRUE(routes.front().local_delivery);
    EXPECT_TRUE(routes.front().legs.empty());
    EXPECT_EQ(routes.front().entry_node, 4);
    EXPECT_DOUBLE_EQ(routes.front().best_case_delivery_time, 8.0);
    EXPECT_DOUBLE_EQ(routes.front().termination_time, 8.0);
}