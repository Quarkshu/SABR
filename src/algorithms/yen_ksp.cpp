#include "algorithms/yen_ksp.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace {

struct PathState {
    std::vector<std::size_t> vertex_path;
    Route route;
};

std::vector<std::size_t> reconstruct_vertex_path(const DijkstraResult& result,
                                                 std::size_t destination_index) {
    if (!result.reached(destination_index)) {
        return {};
    }

    std::vector<std::size_t> vertex_path;
    std::optional<std::size_t> current_index = destination_index;
    while (current_index.has_value()) {
        vertex_path.push_back(*current_index);
        current_index = result.predecessors[*current_index];
    }

    std::reverse(vertex_path.begin(), vertex_path.end());
    return vertex_path;
}

std::vector<ContactId> route_signature(const Route& route) {
    if (route.local_delivery) {
        return {
            std::numeric_limits<ContactId>::max(),
            static_cast<ContactId>(route.local_delivery_node)
        };
    }

    std::vector<ContactId> signature;
    signature.reserve(route.legs.size());
    for (const RouteLeg& leg : route.legs) {
        if (leg.contact != nullptr) {
            signature.push_back(leg.contact->contact_id);
        }
    }
    return signature;
}

bool route_less(const Route& lhs, const Route& rhs) {
    if (lhs.best_case_delivery_time != rhs.best_case_delivery_time) {
        return lhs.best_case_delivery_time < rhs.best_case_delivery_time;
    }
    if (lhs.hop_count() != rhs.hop_count()) {
        return lhs.hop_count() < rhs.hop_count();
    }
    if (lhs.termination_time != rhs.termination_time) {
        return lhs.termination_time < rhs.termination_time;
    }
    if (lhs.entry_node != rhs.entry_node) {
        return lhs.entry_node < rhs.entry_node;
    }
    return route_signature(lhs) < route_signature(rhs);
}

bool path_state_less(const PathState& lhs, const PathState& rhs) {
    return route_less(lhs.route, rhs.route);
}

TimePoint arrival_time_at_position(const PathState& path,
                                   std::size_t position,
                                   TimePoint current_time) {
    if (position == 0) {
        return current_time;
    }
    if (position + 1 == path.vertex_path.size()) {
        return path.route.best_case_delivery_time;
    }
    return path.route.legs.at(position - 1).earliest_arrival_time;
}

std::optional<Route> build_route_from_vertex_path(const ContactGraph& graph,
                                                  const ContactPlan& plan,
                                                  const std::vector<std::size_t>& vertex_path,
                                                  TimePoint current_time,
                                                  TimePoint owlt_margin) {
    if (vertex_path.size() < 3 || vertex_path.front() != graph.root_index() ||
        vertex_path.back() != graph.terminal_index()) {
        return std::nullopt;
    }

    Route route;
    TimePoint previous_arrival = current_time;

    for (std::size_t path_index = 1; path_index + 1 < vertex_path.size(); ++path_index) {
        const GraphVertex& vertex = graph.vertex(vertex_path[path_index]);
        const GraphVertex& predecessor = graph.vertex(vertex_path[path_index - 1]);
        if (vertex.type != GraphVertex::Type::CONTACT || vertex.contact == nullptr) {
            return std::nullopt;
        }

        const bool is_local_contact = predecessor.type == GraphVertex::Type::ROOT;
        const TimePoint transmission_time = std::max(
            vertex.contact->start_time,
            is_local_contact ? current_time : previous_arrival);
        if (transmission_time > vertex.contact->end_time) {
            return std::nullopt;
        }

        const TimePoint owlt = plan.get_owlt(
            vertex.contact->sending_node,
            vertex.contact->receiving_node,
            transmission_time);
        previous_arrival = transmission_time + owlt + owlt_margin;
        route.legs.push_back({
            const_cast<Contact*>(vertex.contact),
            transmission_time,
            previous_arrival,
            0.0,
        });
    }

    if (route.legs.empty()) {
        return std::nullopt;
    }

    route.best_case_delivery_time = previous_arrival;
    route.termination_time = route.compute_termination_time();
    route.entry_node = route.compute_entry_node();
    if (!route.is_valid()) {
        return std::nullopt;
    }

    return route;
}

std::optional<PathState> build_path_state(const ContactGraph& graph,
                                          const ContactPlan& plan,
                                          const DijkstraResult& result,
                                          TimePoint current_time,
                                          TimePoint owlt_margin) {
    const std::vector<std::size_t> vertex_path = reconstruct_vertex_path(result, graph.terminal_index());
    if (vertex_path.empty()) {
        return std::nullopt;
    }

    std::optional<Route> route = build_route_from_vertex_path(
        graph,
        plan,
        vertex_path,
        current_time,
        owlt_margin);
    if (!route.has_value()) {
        return std::nullopt;
    }

    return PathState{vertex_path, std::move(*route)};
}

}  // namespace

std::vector<Route> YenKSP::compute(const ContactGraph& graph,
                                   const ContactPlan& plan,
                                   TimePoint current_time,
                                   TimePoint owlt_margin,
                                   int k) const {
    std::vector<Route> routes;
    if (k <= 0) {
        return routes;
    }

    const DijkstraResult first_result = router_.compute(
        graph,
        graph.root_index(),
        plan,
        current_time,
        owlt_margin);
    std::optional<PathState> first_path = build_path_state(
        graph,
        plan,
        first_result,
        current_time,
        owlt_margin);
    if (!first_path.has_value()) {
        return routes;
    }

    std::vector<PathState> shortest_paths;
    shortest_paths.push_back(std::move(*first_path));

    std::set<std::vector<ContactId>> confirmed_signatures;
    confirmed_signatures.insert(route_signature(shortest_paths.front().route));

    std::vector<PathState> candidate_pool;
    std::set<std::vector<ContactId>> candidate_signatures;

    for (int path_number = 1; path_number < k; ++path_number) {
        const PathState& reference_path = shortest_paths[path_number - 1];
        if (reference_path.vertex_path.size() < 3) {
            break;
        }

        for (std::size_t spur_position = 0;
             spur_position + 1 < reference_path.vertex_path.size();
             ++spur_position) {
            const std::size_t spur_vertex = reference_path.vertex_path[spur_position];
            std::vector<std::size_t> root_path(
                reference_path.vertex_path.begin(),
                reference_path.vertex_path.begin() + spur_position + 1);

            DijkstraConstraints constraints;
            for (std::size_t root_index = 0; root_index < spur_position; ++root_index) {
                constraints.disabled_vertices.insert(root_path[root_index]);
            }

            for (const PathState& confirmed_path : shortest_paths) {
                if (confirmed_path.vertex_path.size() <= spur_position + 1) {
                    continue;
                }
                if (!std::equal(root_path.begin(), root_path.end(), confirmed_path.vertex_path.begin())) {
                    continue;
                }

                constraints.disabled_edges.insert({
                    confirmed_path.vertex_path[spur_position],
                    confirmed_path.vertex_path[spur_position + 1],
                });
            }

            const TimePoint spur_departure_time = arrival_time_at_position(
                reference_path,
                spur_position,
                current_time);
            const DijkstraResult spur_result = router_.compute(
                graph,
                spur_vertex,
                plan,
                spur_departure_time,
                owlt_margin,
                constraints);

            std::vector<std::size_t> spur_path = reconstruct_vertex_path(
                spur_result,
                graph.terminal_index());
            if (spur_path.empty() || spur_path.front() != spur_vertex) {
                continue;
            }

            std::vector<std::size_t> total_path = root_path;
            total_path.insert(total_path.end(), std::next(spur_path.begin()), spur_path.end());

            std::optional<Route> total_route = build_route_from_vertex_path(
                graph,
                plan,
                total_path,
                current_time,
                owlt_margin);
            if (!total_route.has_value()) {
                continue;
            }

            const std::vector<ContactId> signature = route_signature(*total_route);
            if (confirmed_signatures.count(signature) != 0 ||
                candidate_signatures.count(signature) != 0) {
                continue;
            }

            candidate_signatures.insert(signature);
            candidate_pool.push_back({std::move(total_path), std::move(*total_route)});
        }

        if (candidate_pool.empty()) {
            break;
        }

        std::sort(candidate_pool.begin(), candidate_pool.end(), path_state_less);
        PathState next_path = std::move(candidate_pool.front());
        candidate_pool.erase(candidate_pool.begin());

        const std::vector<ContactId> signature = route_signature(next_path.route);
        candidate_signatures.erase(signature);
        confirmed_signatures.insert(signature);
        shortest_paths.push_back(std::move(next_path));
    }

    routes.reserve(shortest_paths.size());
    for (PathState& path : shortest_paths) {
        routes.push_back(std::move(path.route));
    }
    return routes;
}