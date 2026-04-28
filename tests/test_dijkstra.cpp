#include <gtest/gtest.h>

#include "algorithms/contact_graph.hpp"
#include "algorithms/dijkstra.hpp"

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

bool has_edge(const ContactGraph& graph, ContactId from, ContactId to) {
    const auto from_index = graph.find_vertex(from);
    const auto to_index = graph.find_vertex(to);
    if (!from_index.has_value() || !to_index.has_value()) {
        return false;
    }

    for (const GraphEdge& edge : graph.outgoing_edges(*from_index)) {
        if (edge.to == *to_index) {
            return true;
        }
    }
    return false;
}

ContactPlan make_linear_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2),
        make_contact(2, 12.0, 25.0, 2, 3),
        make_contact(3, 30.0, 40.0, 3, 4),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 3, 1.5),
        make_range(0.0, 100.0, 3, 4, 2.0),
    });
    return plan;
}

ContactPlan make_two_hop_overlap_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2),
        make_contact(2, 2.0, 20.0, 2, 3),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 3, 1.0),
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

class ContactGraphBuilderTest : public ::testing::Test {
protected:
    ContactGraphBuilder builder;
};

TEST_F(ContactGraphBuilderTest, CreatesRootAndTerminalVertices) {
    ContactPlan plan = make_linear_plan();

    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    ASSERT_EQ(graph.vertices().size(), 5u);
    EXPECT_EQ(graph.vertex(graph.root_index()).type, GraphVertex::Type::ROOT);
    EXPECT_EQ(graph.vertex(graph.root_index()).contact->contact_id, ContactGraph::root_contact_id);
    EXPECT_EQ(graph.vertex(graph.terminal_index()).type, GraphVertex::Type::TERMINAL);
    EXPECT_EQ(graph.vertex(graph.terminal_index()).contact->contact_id, ContactGraph::terminal_contact_id);
}

TEST_F(ContactGraphBuilderTest, AddsEdgesFromRootAndToTerminal) {
    ContactPlan plan = make_linear_plan();

    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    EXPECT_TRUE(has_edge(graph, ContactGraph::root_contact_id, 1));
    EXPECT_TRUE(has_edge(graph, 3, ContactGraph::terminal_contact_id));
}

TEST_F(ContactGraphBuilderTest, ConnectsSequentialContacts) {
    ContactPlan plan = make_linear_plan();

    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    EXPECT_TRUE(has_edge(graph, 1, 2));
    EXPECT_TRUE(has_edge(graph, 2, 3));
    EXPECT_FALSE(has_edge(graph, 1, 3));
}

TEST_F(ContactGraphBuilderTest, FiltersTerminatedContacts) {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 5.0, 1, 2),
        make_contact(2, 10.0, 20.0, 2, 3),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 3, 1.0),
    });

    const ContactGraph graph = builder.build(plan, 1, 3, 6.0);

    EXPECT_FALSE(graph.find_vertex(1).has_value());
    EXPECT_TRUE(graph.find_vertex(2).has_value());
    EXPECT_EQ(graph.vertices().size(), 3u);
}

TEST_F(ContactGraphBuilderTest, MaintainsTopologicalOrderInAdjacency) {
    ContactPlan plan = make_diamond_plan();

    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    for (std::size_t from_index = 0; from_index < graph.adjacency().size(); ++from_index) {
        for (const GraphEdge& edge : graph.outgoing_edges(from_index)) {
            EXPECT_LT(edge.from, edge.to);
        }
    }
}

class DijkstraRouterTest : public ::testing::Test {
protected:
    ContactGraphBuilder builder;
    DijkstraRouter router;
};

TEST_F(DijkstraRouterTest, FindsLinearShortestPath) {
    ContactPlan plan = make_linear_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 0.0, 0.0);
    const std::optional<Route> route = router.extract_route(graph, result, graph.terminal_index());

    ASSERT_TRUE(route.has_value());
    ASSERT_EQ(route->legs.size(), 3u);
    EXPECT_EQ(route->legs[0].contact->contact_id, 1u);
    EXPECT_EQ(route->legs[1].contact->contact_id, 2u);
    EXPECT_EQ(route->legs[2].contact->contact_id, 3u);
    EXPECT_DOUBLE_EQ(route->best_case_delivery_time, 32.0);
    EXPECT_DOUBLE_EQ(route->termination_time, 10.0);
    EXPECT_EQ(route->entry_node, 2);
}

TEST_F(DijkstraRouterTest, UsesCurrentTimeForLocalFirstContact) {
    ContactPlan plan = make_linear_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 5.0);

    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 5.0, 0.0);
    const std::optional<Route> route = router.extract_route(graph, result, graph.terminal_index());

    ASSERT_TRUE(route.has_value());
    EXPECT_DOUBLE_EQ(route->legs[0].earliest_transmission_time, 5.0);
    EXPECT_DOUBLE_EQ(route->legs[0].earliest_arrival_time, 6.0);
}

TEST_F(DijkstraRouterTest, UsesPredecessorArrivalForNonLocalContact) {
    ContactPlan plan = make_two_hop_overlap_plan();
    const ContactGraph graph = builder.build(plan, 1, 3, 5.0);

    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 5.0, 0.0);
    const std::optional<Route> route = router.extract_route(graph, result, graph.terminal_index());

    ASSERT_TRUE(route.has_value());
    ASSERT_EQ(route->legs.size(), 2u);
    EXPECT_DOUBLE_EQ(route->legs[0].earliest_transmission_time, 5.0);
    EXPECT_DOUBLE_EQ(route->legs[0].earliest_arrival_time, 6.0);
    EXPECT_DOUBLE_EQ(route->legs[1].earliest_transmission_time, 6.0);
    EXPECT_DOUBLE_EQ(route->legs[1].earliest_arrival_time, 7.0);
}

TEST_F(DijkstraRouterTest, AppliesOwltMarginPerHop) {
    ContactPlan plan = make_linear_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    const DijkstraResult without_margin = router.compute(graph, graph.root_index(), plan, 0.0, 0.0);
    const DijkstraResult with_margin = router.compute(graph, graph.root_index(), plan, 0.0, 2.0);

    ASSERT_TRUE(without_margin.reached(graph.terminal_index()));
    ASSERT_TRUE(with_margin.reached(graph.terminal_index()));
    EXPECT_DOUBLE_EQ(without_margin.arrival_times[graph.terminal_index()], 32.0);
    EXPECT_DOUBLE_EQ(with_margin.arrival_times[graph.terminal_index()], 34.0);
}

TEST_F(DijkstraRouterTest, SkipsContactWhenTransmissionMissesEndTime) {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2),
        make_contact(2, 2.0, 5.0, 2, 3),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 3, 1.0),
    });

    const ContactGraph graph = builder.build(plan, 1, 3, 5.0);
    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 5.0, 0.0);

    EXPECT_FALSE(result.reached(graph.terminal_index()));
    EXPECT_FALSE(router.extract_route(graph, result, graph.terminal_index()).has_value());
}

TEST_F(DijkstraRouterTest, ChoosesPathWithLowestArrivalTime) {
    ContactPlan plan = make_diamond_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 0.0, 0.0);
    const std::optional<Route> route = router.extract_route(graph, result, graph.terminal_index());

    ASSERT_TRUE(route.has_value());
    ASSERT_EQ(route->legs.size(), 2u);
    EXPECT_EQ(route->legs[0].contact->contact_id, 1u);
    EXPECT_EQ(route->legs[1].contact->contact_id, 3u);
    EXPECT_DOUBLE_EQ(route->best_case_delivery_time, 13.0);
}

TEST_F(DijkstraRouterTest, SupportsDisabledEdgeConstraints) {
    ContactPlan plan = make_diamond_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    DijkstraConstraints constraints;
    constraints.disabled_edges.insert({graph.root_index(), *graph.find_vertex(1)});

    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 0.0, 0.0, constraints);
    const std::optional<Route> route = router.extract_route(graph, result, graph.terminal_index());

    ASSERT_TRUE(route.has_value());
    ASSERT_EQ(route->legs.size(), 2u);
    EXPECT_EQ(route->legs[0].contact->contact_id, 2u);
    EXPECT_EQ(route->legs[1].contact->contact_id, 4u);
}

TEST_F(DijkstraRouterTest, SupportsDisabledVertexConstraints) {
    ContactPlan plan = make_diamond_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    DijkstraConstraints constraints;
    constraints.disabled_vertices.insert(*graph.find_vertex(1));

    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 0.0, 0.0, constraints);
    const std::optional<Route> route = router.extract_route(graph, result, graph.terminal_index());

    ASSERT_TRUE(route.has_value());
    ASSERT_EQ(route->legs.size(), 2u);
    EXPECT_EQ(route->legs[0].contact->contact_id, 2u);
    EXPECT_EQ(route->legs[1].contact->contact_id, 4u);
}

TEST_F(DijkstraRouterTest, SupportsForcedFirstHopNeighbor) {
    ContactPlan plan = make_diamond_plan();
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    DijkstraConstraints constraints;
    constraints.forced_first_hop_neighbor = 3;

    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 0.0, 0.0, constraints);
    const std::optional<Route> route = router.extract_route(graph, result, graph.terminal_index());

    ASSERT_TRUE(route.has_value());
    ASSERT_EQ(route->legs.size(), 2u);
    EXPECT_EQ(route->legs[0].contact->contact_id, 2u);
    EXPECT_EQ(route->legs[1].contact->contact_id, 4u);
    EXPECT_EQ(route->entry_node, 3);
}

TEST_F(DijkstraRouterTest, ReturnsNoRouteWhenDestinationUnreachable) {
    ContactPlan plan;
    plan.set_contacts({make_contact(1, 0.0, 10.0, 1, 2)});
    plan.set_ranges({make_range(0.0, 100.0, 1, 2, 1.0)});
    const ContactGraph graph = builder.build(plan, 1, 4, 0.0);

    const DijkstraResult result = router.compute(graph, graph.root_index(), plan, 0.0, 0.0);

    EXPECT_FALSE(result.reached(graph.terminal_index()));
    EXPECT_FALSE(router.extract_route(graph, result, graph.terminal_index()).has_value());
}
