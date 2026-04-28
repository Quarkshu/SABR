#pragma once

#include "models/node.hpp"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class BundleProtocolAgent {
public:
    explicit BundleProtocolAgent(Node& node);

    void bind_node(Node& node);

    NodeId node_id() const noexcept;
    void on_bundle_created(const std::shared_ptr<Bundle>& bundle);
    void on_bundle_arrived(const std::shared_ptr<Bundle>& bundle);
    bool note_redundant_arrival(const Bundle& bundle);
    void enqueue_for_contact(const std::shared_ptr<Bundle>& bundle,
                             ContactId contact_id,
                             Priority priority);
    void deliver_local(const std::shared_ptr<Bundle>& bundle);
    bool has_queued_for_contact(ContactId contact_id) const noexcept;
    std::shared_ptr<Bundle> dequeue_next_for_contact(ContactId contact_id);
    std::vector<ContactId> queued_contact_ids() const;
    std::vector<std::shared_ptr<Bundle>> drain_contact_queue(ContactId contact_id);
    std::vector<std::shared_ptr<Bundle>> drain_all_contact_queues();

    std::size_t pending_bundle_count() const noexcept;
    std::size_t delivered_bundle_count() const noexcept;
    std::size_t queued_for_contact(ContactId contact_id) const noexcept;
    Volume queued_volume_for_contact(ContactId contact_id) const noexcept;

    const std::vector<std::shared_ptr<Bundle>>& pending_bundles() const noexcept {
        return pending_bundles_;
    }

    const std::vector<std::shared_ptr<Bundle>>& delivered_bundles() const noexcept {
        return delivered_bundles_;
    }

private:
    void remove_bundle_from_send_queues(BundleId bundle_id);

    Node* node_ = nullptr;
    std::vector<std::shared_ptr<Bundle>> pending_bundles_;
    std::vector<std::shared_ptr<Bundle>> delivered_bundles_;
    std::map<std::pair<ContactId, int>, std::deque<std::shared_ptr<Bundle>>> outbound_queues_;
    std::unordered_set<std::string> redundancy_arrival_registry_;
};

struct NodeRuntime {
    Node* node = nullptr;
    std::shared_ptr<BundleProtocolAgent> bpa;

    bool has_contact_plan() const noexcept;
};

class RuntimeContext {
public:
    using LogHook = std::function<void(const std::string&)>;
    using StatHook = std::function<void(const std::string&, double)>;

    explicit RuntimeContext(ContactPlan* contact_plan = nullptr);

    void bind_contact_plan(ContactPlan& plan);
    ContactPlan* contact_plan() noexcept { return contact_plan_; }
    const ContactPlan* contact_plan() const noexcept { return contact_plan_; }

    NodeRuntime& add_node(Node& node);
    bool has_node(NodeId node_id) const;
    NodeRuntime* find_node(NodeId node_id);
    const NodeRuntime* find_node(NodeId node_id) const;
    std::vector<NodeId> node_ids() const;

    void set_log_hook(LogHook hook);
    void set_stat_hook(StatHook hook);
    void emit_log(const std::string& message) const;
    void emit_stat(const std::string& key, double value) const;

private:
    ContactPlan* contact_plan_ = nullptr;
    std::unordered_map<NodeId, NodeRuntime> nodes_;
    LogHook log_hook_;
    StatHook stat_hook_;
};