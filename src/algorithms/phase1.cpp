#include "algorithms/phase1.hpp"

#include "algorithms/contact_graph.hpp"
#include "algorithms/dijkstra.hpp"
#include "algorithms/yen_ksp.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <vector>

namespace {

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

void append_unique_route(std::vector<Route>& routes,
                         std::set<std::vector<ContactId>>& signatures,
                         Route route) {
    const std::vector<ContactId> signature = route_signature(route);
    if (signatures.insert(signature).second) {
        routes.push_back(std::move(route));
    }
}

}  // namespace

const std::vector<Route>& Phase1::compute(Node& node,
                                          NodeId destination,
                                          TimePoint current_time,
                                          const Config& config) {
    if (node.node_number == destination) {
        auto& routes = node.get_route_list(destination);
        routes.clear();
        routes.push_back(Route::make_local_delivery(node.node_number, current_time));
        return routes;
    }

    if (const auto* cached_routes = node.find_route_list(destination);
        cached_routes != nullptr && !cached_routes->empty()) {
        return *cached_routes;
    }

    auto& routes = node.get_route_list(destination);
    routes = compute_routes(node, destination, current_time, config, std::max(1, config.k_paths));
    return routes;
}

const std::vector<Route>& Phase1::recompute_more(Node& node,
                                                 NodeId destination,
                                                 TimePoint current_time,
                                                 const Config& config,
                                                 int additional_paths) {
    if (node.node_number == destination) {
        auto& routes = node.get_route_list(destination);
        routes.clear();
        routes.push_back(Route::make_local_delivery(node.node_number, current_time));
        return routes;
    }

    auto& routes = node.get_route_list(destination);
    const int requested_paths = std::max(
        std::max(1, config.k_paths),
        static_cast<int>(routes.size()) + std::max(1, additional_paths));
    routes = compute_routes(node, destination, current_time, config, requested_paths);
    return routes;
}

std::vector<Route> Phase1::compute_routes(Node& node,
                                          NodeId destination,
                                          TimePoint current_time,
                                          const Config& config,
                                          int requested_paths) {
    std::vector<Route> routes;
    if (node.contact_plan == nullptr) {
        return routes;
    }

    ContactGraphBuilder builder;
    const ContactGraph graph = builder.build(*node.contact_plan, node.node_number, destination, current_time);

    YenKSP yen_ksp;
    routes = yen_ksp.compute(
        graph,
        *node.contact_plan,
        current_time,
        config.owlt_margin,
        std::max(1, requested_paths));

    std::set<std::vector<ContactId>> signatures;
    for (const Route& route : routes) {
        signatures.insert(route_signature(route));
    }

    if (config.one_route_per_neighbor) {
        std::set<NodeId> covered_neighbors;
        for (const Route& route : routes) {
            if (!route.local_delivery && route.entry_node >= 0) {
                covered_neighbors.insert(route.entry_node);
            }
        }

        DijkstraRouter router;
        for (const GraphEdge& edge : graph.outgoing_edges(graph.root_index())) {
            const GraphVertex& vertex = graph.vertex(edge.to);
            if (vertex.type != GraphVertex::Type::CONTACT || vertex.contact == nullptr) {
                continue;
            }

            const NodeId neighbor = vertex.contact->receiving_node;
            if (covered_neighbors.count(neighbor) != 0) {
                continue;
            }

            DijkstraConstraints constraints;
            constraints.forced_first_hop_neighbor = neighbor;
            const DijkstraResult result = router.compute(
                graph,
                graph.root_index(),
                *node.contact_plan,
                current_time,
                config.owlt_margin,
                constraints);
            std::optional<Route> route = router.extract_route(graph, result, graph.terminal_index());
            if (!route.has_value()) {
                continue;
            }

            append_unique_route(routes, signatures, std::move(*route));
            covered_neighbors.insert(neighbor);
        }
    }

    std::sort(routes.begin(), routes.end(), route_less);
    return routes;
}