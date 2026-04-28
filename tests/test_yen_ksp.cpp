#include <gtest/gtest.h>

#include "algorithms/contact_graph.hpp"
#include "algorithms/yen_ksp.hpp"

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
    std::vector<ContactId> result;
    for (const RouteLeg& leg : route.legs) {
        result.push_back(leg.contact->contact_id);
    }
    return result;
}

ContactPlan make_linear_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2),
        make_contact(2, 12.0, 25.0, 2, 4),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
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

ContactPlan make_shared_prefix_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2),
        make_contact(2, 11.0, 20.0, 2, 3),
        make_contact(3, 11.0, 20.0, 2, 4),
        make_contact(4, 21.0, 30.0, 3, 5),
        make_contact(5, 15.0, 25.0, 4, 5),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 3, 1.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
        make_range(0.0, 100.0, 3, 5, 1.0),
        make_range(0.0, 100.0, 4, 5, 1.0),
    });
    return plan;
}

}  // namespace

class YenKSPTest : public ::testing::Test {
protected:
    ContactGraphBuilder builder;
    YenKSP yen;
};

TEST_F(YenKSPTest, ReturnsSinglePathForLinearTopology) {
    ContactPlan plan = make_linear_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    const std::vector<Route> routes = yen.compute(graph, plan, 0.0, 0.0, 3);

    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(signature(routes[0]), (std::vector<ContactId>{1, 2}));
}

TEST_F(YenKSPTest, ReturnsMultiplePathsInArrivalOrder) {
    ContactPlan plan = make_diamond_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    const std::vector<Route> routes = yen.compute(graph, plan, 0.0, 0.0, 2);

    ASSERT_EQ(routes.size(), 2u);
    EXPECT_EQ(signature(routes[0]), (std::vector<ContactId>{1, 3}));
    EXPECT_EQ(signature(routes[1]), (std::vector<ContactId>{2, 4}));
    EXPECT_LT(routes[0].best_case_delivery_time, routes[1].best_case_delivery_time);
}

TEST_F(YenKSPTest, TruncatesWhenKExceedsAvailablePaths) {
    ContactPlan plan = make_diamond_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    const std::vector<Route> routes = yen.compute(graph, plan, 0.0, 0.0, 5);

    ASSERT_EQ(routes.size(), 2u);
}

TEST_F(YenKSPTest, ReturnsEmptyWhenDestinationUnreachable) {
    ContactPlan plan;
    plan.set_contacts({make_contact(1, 0.0, 10.0, 1, 2)});
    plan.set_ranges({make_range(0.0, 100.0, 1, 2, 1.0)});
    const ContactGraph graph = builder.build(plan, 1, 5, 0.0);

    const std::vector<Route> routes = yen.compute(graph, plan, 0.0, 0.0, 3);

    EXPECT_TRUE(routes.empty());
}

TEST_F(YenKSPTest, HandlesSharedPrefixSpurPaths) {
    ContactPlan plan = make_shared_prefix_plan();
    const ContactGraph graph = builder.build(plan, 1, 5, 0.0);

    const std::vector<Route> routes = yen.compute(graph, plan, 0.0, 0.0, 2);

    ASSERT_EQ(routes.size(), 2u);
    EXPECT_EQ(signature(routes[0]), (std::vector<ContactId>{1, 3, 5}));
    EXPECT_EQ(signature(routes[1]), (std::vector<ContactId>{1, 2, 4}));
}