#pragma once

#include "models/contact_plan.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

struct GraphVertex {
    enum class Type {
        ROOT,
        CONTACT,
        TERMINAL,
    };

    Type type = Type::CONTACT;
    const Contact* contact = nullptr;
};

struct GraphEdge {
    std::size_t from = 0;
    std::size_t to = 0;
};

class ContactGraph {
public:
    static constexpr ContactId root_contact_id = std::numeric_limits<ContactId>::max();
    static constexpr ContactId terminal_contact_id = std::numeric_limits<ContactId>::max() - 1;

    void initialize_virtual_contacts(NodeId source, NodeId destination, TimePoint now, PlanVersion version);
    void add_vertex(const GraphVertex& vertex);
    void add_edge(const GraphEdge& edge);

    const std::vector<GraphVertex>& vertices() const noexcept { return vertices_; }
    const std::vector<std::vector<GraphEdge>>& adjacency() const noexcept { return adjacency_; }
    const GraphVertex& vertex(std::size_t index) const { return vertices_.at(index); }
    const std::vector<GraphEdge>& outgoing_edges(std::size_t index) const { return adjacency_.at(index); }
    const Contact& root_contact() const noexcept { return root_contact_; }
    const Contact& terminal_contact() const noexcept { return terminal_contact_; }

    std::size_t root_index() const noexcept { return root_index_; }
    std::size_t terminal_index() const noexcept { return terminal_index_; }
    void set_root_index(std::size_t index) noexcept { root_index_ = index; }
    void set_terminal_index(std::size_t index) noexcept { terminal_index_ = index; }

    std::optional<std::size_t> find_vertex(ContactId contact_id) const;

private:
    std::vector<GraphVertex> vertices_;
    std::vector<std::vector<GraphEdge>> adjacency_;
    std::unordered_map<ContactId, std::size_t> vertex_by_contact_id_;
    std::size_t root_index_ = 0;
    std::size_t terminal_index_ = 0;
    Contact root_contact_{};
    Contact terminal_contact_{};
};

class ContactGraphBuilder {
public:
    ContactGraph build(const ContactPlan& plan,
                       NodeId source,
                       NodeId destination,
                       TimePoint now) const;

private:
    bool can_connect(const Contact& from, const Contact& to) const noexcept;
};