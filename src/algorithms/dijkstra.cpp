#include "algorithms/dijkstra.hpp"

#include <algorithm>
#include <limits>
#include <queue>

namespace {

constexpr TimePoint kInfinity = std::numeric_limits<TimePoint>::infinity();

using QueueEntry = std::pair<TimePoint, std::size_t>;

}  // namespace

std::size_t GraphEdgeKeyHash::operator()(const GraphEdgeKey& edge) const noexcept {
    const std::size_t from_hash = std::hash<std::size_t>{}(edge.from);
    const std::size_t to_hash = std::hash<std::size_t>{}(edge.to);
    return from_hash ^ (to_hash << 1);
}

bool DijkstraConstraints::is_vertex_disabled(std::size_t vertex) const noexcept {
    return disabled_vertices.find(vertex) != disabled_vertices.end();
}

bool DijkstraConstraints::is_edge_disabled(std::size_t from, std::size_t to) const noexcept {
    return disabled_edges.find({from, to}) != disabled_edges.end();
}

bool DijkstraResult::reached(std::size_t index) const {
    return index < arrival_times.size() && arrival_times[index] < kInfinity;
}

DijkstraResult DijkstraRouter::compute(const ContactGraph& graph,
                                       std::size_t source_index,
                                       const ContactPlan& plan,
                                       TimePoint now,
                                       TimePoint owlt_margin,
                                       const DijkstraConstraints& constraints) const {
    DijkstraResult result;
    result.arrival_times.assign(graph.vertices().size(), kInfinity);
    result.predecessors.assign(graph.vertices().size(), std::nullopt);

    if (source_index >= graph.vertices().size() || constraints.is_vertex_disabled(source_index)) {
        return result;
    }

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> frontier;
    result.arrival_times[source_index] = now;
    frontier.push({now, source_index});

    while (!frontier.empty()) {
        const auto [current_arrival, current_index] = frontier.top();
        frontier.pop();

        if (current_arrival > result.arrival_times[current_index]) {
            continue;
        }

        if (current_index == graph.terminal_index()) {
            break;
        }

        const GraphVertex& current_vertex = graph.vertex(current_index);
        for (const GraphEdge& edge : graph.outgoing_edges(current_index)) {
            if (constraints.is_edge_disabled(edge.from, edge.to) ||
                constraints.is_vertex_disabled(edge.to)) {
                continue;
            }

            const GraphVertex& next_vertex = graph.vertex(edge.to);
            TimePoint candidate_arrival = current_arrival;

            if (next_vertex.type == GraphVertex::Type::CONTACT) {
                const Contact& next_contact = *next_vertex.contact;

                if (current_vertex.type == GraphVertex::Type::ROOT &&
                    constraints.forced_first_hop_neighbor.has_value() &&
                    next_contact.receiving_node != *constraints.forced_first_hop_neighbor) {
                    continue;
                }

                const bool is_local_contact = current_vertex.type == GraphVertex::Type::ROOT;
                const TimePoint transmission_time = earliest_transmission_time(
                    next_contact,
                    current_arrival,
                    now,
                    is_local_contact);

                if (transmission_time > next_contact.end_time) {
                    continue;
                }

                const TimePoint owlt = plan.get_owlt(
                    next_contact.sending_node,
                    next_contact.receiving_node,
                    transmission_time);
                candidate_arrival = earliest_arrival_time(transmission_time, owlt, owlt_margin);
            }

            if (candidate_arrival < result.arrival_times[edge.to]) {
                result.arrival_times[edge.to] = candidate_arrival;
                result.predecessors[edge.to] = current_index;
                frontier.push({candidate_arrival, edge.to});
            }
        }
    }

    return result;
}

std::optional<Route> DijkstraRouter::extract_route(const ContactGraph& graph,
                                                   const DijkstraResult& result,
                                                   std::size_t destination_index) const {
    if (!result.reached(destination_index)) {
        return std::nullopt;
    }

    std::vector<std::size_t> vertex_path;
    std::optional<std::size_t> current_index = destination_index;
    while (current_index.has_value()) {
        vertex_path.push_back(*current_index);
        current_index = result.predecessors[*current_index];
    }
    std::reverse(vertex_path.begin(), vertex_path.end());

    if (vertex_path.empty() || vertex_path.front() != graph.root_index()) {
        return std::nullopt;
    }

    Route route;
    for (std::size_t vertex_index : vertex_path) {
        const GraphVertex& vertex = graph.vertex(vertex_index);
        if (vertex.type != GraphVertex::Type::CONTACT) {
            continue;
        }

        const std::optional<std::size_t> predecessor_index = result.predecessors[vertex_index];
        if (!predecessor_index.has_value()) {
            return std::nullopt;
        }

        const GraphVertex& predecessor_vertex = graph.vertex(*predecessor_index);
        RouteLeg leg;
        leg.contact = const_cast<Contact*>(vertex.contact);
        leg.earliest_transmission_time = earliest_transmission_time(
            *vertex.contact,
            result.arrival_times[*predecessor_index],
            result.arrival_times[graph.root_index()],
            predecessor_vertex.type == GraphVertex::Type::ROOT);
        leg.earliest_arrival_time = result.arrival_times[vertex_index];
        route.legs.push_back(leg);
    }

    if (route.legs.empty()) {
        return std::nullopt;
    }

    route.best_case_delivery_time = result.arrival_times[destination_index];
    route.termination_time = route.compute_termination_time();
    route.entry_node = route.compute_entry_node();

    if (!route.is_valid()) {
        return std::nullopt;
    }

    return route;
}

TimePoint DijkstraRouter::earliest_transmission_time(const Contact& contact,
                                                     TimePoint arrival_at_sending_node,
                                                     TimePoint current_time,
                                                     bool is_local_contact) const {
    return std::max(contact.start_time, is_local_contact ? current_time : arrival_at_sending_node);
}

TimePoint DijkstraRouter::earliest_arrival_time(TimePoint transmission_time,
                                                TimePoint owlt,
                                                TimePoint owlt_margin) const {
    return transmission_time + owlt + owlt_margin;
}