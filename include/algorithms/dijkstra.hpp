#pragma once

#include "algorithms/contact_graph.hpp"
#include "models/route.hpp"

#include <optional>
#include <unordered_set>
#include <vector>

struct GraphEdgeKey {
    std::size_t from = 0;
    std::size_t to = 0;

    bool operator==(const GraphEdgeKey& other) const noexcept {
        return from == other.from && to == other.to;
    }
};

struct GraphEdgeKeyHash {
    std::size_t operator()(const GraphEdgeKey& edge) const noexcept;
};

struct DijkstraConstraints {
    std::unordered_set<std::size_t> disabled_vertices;
    std::unordered_set<GraphEdgeKey, GraphEdgeKeyHash> disabled_edges;
    std::optional<NodeId> forced_first_hop_neighbor;

    bool is_vertex_disabled(std::size_t vertex) const noexcept;
    bool is_edge_disabled(std::size_t from, std::size_t to) const noexcept;
};

struct DijkstraResult {
    std::vector<TimePoint> arrival_times;
    std::vector<std::optional<std::size_t>> predecessors;

    bool reached(std::size_t index) const;
};

class DijkstraRouter {
public:
    DijkstraResult compute(const ContactGraph& graph,
                           std::size_t source_index,
                           const ContactPlan& plan,
                           TimePoint now,
                           TimePoint owlt_margin,
                           const DijkstraConstraints& constraints = {}) const;

    std::optional<Route> extract_route(const ContactGraph& graph,
                                       const DijkstraResult& result,
                                       std::size_t destination_index) const;

private:
    TimePoint earliest_transmission_time(const Contact& contact,
                                         TimePoint arrival_at_sending_node,
                                         TimePoint current_time,
                                         bool is_local_contact) const;
    TimePoint earliest_arrival_time(TimePoint transmission_time,
                                    TimePoint owlt,
                                    TimePoint owlt_margin) const;
};