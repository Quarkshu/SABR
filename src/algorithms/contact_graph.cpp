#include "algorithms/contact_graph.hpp"

#include <algorithm>
#include <utility>

void ContactGraph::initialize_virtual_contacts(NodeId source,
                                               NodeId destination,
                                               TimePoint now,
                                               PlanVersion version) {
    root_contact_.contact_id = root_contact_id;
    root_contact_.start_time = now;
    root_contact_.end_time = std::numeric_limits<TimePoint>::max();
    root_contact_.sending_node = source;
    root_contact_.receiving_node = source;
    root_contact_.data_rate = 0.0;
    root_contact_.plan_version = version;
    root_contact_.mtv.fill(0.0);

    terminal_contact_.contact_id = terminal_contact_id;
    terminal_contact_.start_time = 0.0;
    terminal_contact_.end_time = std::numeric_limits<TimePoint>::max();
    terminal_contact_.sending_node = destination;
    terminal_contact_.receiving_node = destination;
    terminal_contact_.data_rate = 0.0;
    terminal_contact_.plan_version = version;
    terminal_contact_.mtv.fill(0.0);
}

void ContactGraph::add_vertex(const GraphVertex& vertex) {
    vertices_.push_back(vertex);
    adjacency_.emplace_back();
    if (vertex.contact != nullptr) {
        vertex_by_contact_id_[vertex.contact->contact_id] = vertices_.size() - 1;
    }
}

void ContactGraph::add_edge(const GraphEdge& edge) {
    if (edge.from >= adjacency_.size() || edge.to >= vertices_.size()) {
        return;
    }
    adjacency_[edge.from].push_back(edge);
}

std::optional<std::size_t> ContactGraph::find_vertex(ContactId contact_id) const {
    auto it = vertex_by_contact_id_.find(contact_id);
    if (it == vertex_by_contact_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

ContactGraph ContactGraphBuilder::build(const ContactPlan& plan,
                                        NodeId source,
                                        NodeId destination,
                                        TimePoint now) const {
    ContactGraph graph;
    graph.initialize_virtual_contacts(source, destination, now, plan.version());

    graph.add_vertex({GraphVertex::Type::ROOT, &graph.root_contact()});
    graph.set_root_index(0);

    std::vector<const Contact*> active_contacts;
    active_contacts.reserve(plan.contacts().size());
    for (const auto& contact : plan.contacts()) {
        if (!contact.is_terminated(now)) {
            active_contacts.push_back(&contact);
        }
    }

    std::sort(active_contacts.begin(), active_contacts.end(), [](const Contact* lhs, const Contact* rhs) {
        if (lhs->start_time == rhs->start_time) {
            return lhs->contact_id < rhs->contact_id;
        }
        return lhs->start_time < rhs->start_time;
    });

    std::vector<std::size_t> contact_vertex_indices;
    contact_vertex_indices.reserve(active_contacts.size());
    for (const Contact* contact : active_contacts) {
        graph.add_vertex({GraphVertex::Type::CONTACT, contact});
        contact_vertex_indices.push_back(graph.vertices().size() - 1);
    }

    graph.add_vertex({GraphVertex::Type::TERMINAL, &graph.terminal_contact()});
    graph.set_terminal_index(graph.vertices().size() - 1);

    for (std::size_t index = 0; index < active_contacts.size(); ++index) {
        const Contact& contact = *active_contacts[index];
        const std::size_t vertex_index = contact_vertex_indices[index];

        if (contact.sending_node == source) {
            graph.add_edge({graph.root_index(), vertex_index});
        }

        if (contact.receiving_node == destination) {
            graph.add_edge({vertex_index, graph.terminal_index()});
        }
    }

    for (std::size_t from_index = 0; from_index < active_contacts.size(); ++from_index) {
        for (std::size_t to_index = from_index + 1; to_index < active_contacts.size(); ++to_index) {
            if (can_connect(*active_contacts[from_index], *active_contacts[to_index])) {
                graph.add_edge({contact_vertex_indices[from_index], contact_vertex_indices[to_index]});
            }
        }
    }

    return graph;
}

bool ContactGraphBuilder::can_connect(const Contact& from, const Contact& to) const noexcept {
    return from.contact_id != to.contact_id &&
           from.receiving_node == to.sending_node &&
           to.end_time >= from.start_time;
}