#include "simulation/runtime_context.hpp"

#include <algorithm>

namespace {

std::pair<ContactId, int> queue_key(ContactId contact_id, Priority priority) {
    return {contact_id, static_cast<int>(priority)};
}

std::pair<NodeId, int> node_queue_key(const Contact* contact, Priority priority) {
    if (contact == nullptr) {
        return {-1, static_cast<int>(priority)};
    }
    return {contact->receiving_node, static_cast<int>(priority)};
}

void erase_first_matching_bundle(std::deque<Bundle>& queue,
                                 BundleId bundle_id) {
    auto it = std::find_if(queue.begin(), queue.end(),
        [bundle_id](const Bundle& queued_bundle) {
            return queued_bundle.id == bundle_id;
        });
    if (it != queue.end()) {
        queue.erase(it);
    }
}

}  // namespace

BundleProtocolAgent::BundleProtocolAgent(Node& node)
    : node_(&node) {}

void BundleProtocolAgent::bind_node(Node& node) {
    node_ = &node;
}

NodeId BundleProtocolAgent::node_id() const noexcept {
    return node_ == nullptr ? -1 : node_->node_number;
}

void BundleProtocolAgent::on_bundle_created(const std::shared_ptr<Bundle>& bundle) {
    if (bundle != nullptr) {
        pending_bundles_.push_back(bundle);
    }
}

void BundleProtocolAgent::on_bundle_arrived(const std::shared_ptr<Bundle>& bundle) {
    if (bundle != nullptr) {
        pending_bundles_.push_back(bundle);
    }
}

bool BundleProtocolAgent::note_redundant_arrival(const Bundle& bundle) {
    if (!bundle.participates_in_redundancy()) {
        return true;
    }

    const std::string key = std::to_string(bundle.canonical_origin_id()) + "|" + bundle.redundancy_scope_key();
    return redundancy_arrival_registry_.insert(key).second;
}

void BundleProtocolAgent::enqueue_for_contact(const std::shared_ptr<Bundle>& bundle,
                                              ContactId contact_id,
                                              Priority priority) {
    if (bundle != nullptr) {
        outbound_queues_[queue_key(contact_id, priority)].push_back(bundle);

        if (node_ != nullptr && node_->contact_plan != nullptr) {
            const Contact* contact = node_->contact_plan->get_contact(contact_id);
            if (contact != nullptr) {
                node_->send_queues[node_queue_key(contact, priority)].push_back(*bundle);
            }
        }
    }
}

void BundleProtocolAgent::deliver_local(const std::shared_ptr<Bundle>& bundle) {
    if (bundle != nullptr) {
        delivered_bundles_.push_back(bundle);
    }
}

void BundleProtocolAgent::remove_bundle_from_send_queues(BundleId bundle_id) {
    if (node_ == nullptr) {
        return;
    }

    for (auto& entry : node_->send_queues) {
        erase_first_matching_bundle(entry.second, bundle_id);
    }
}

bool BundleProtocolAgent::has_queued_for_contact(ContactId contact_id) const noexcept {
    for (int priority = static_cast<int>(Priority::EXPEDITED);
         priority >= static_cast<int>(Priority::BULK);
         --priority) {
        auto it = outbound_queues_.find({contact_id, priority});
        if (it != outbound_queues_.end() && !it->second.empty()) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<Bundle> BundleProtocolAgent::dequeue_next_for_contact(ContactId contact_id) {
    for (int priority = static_cast<int>(Priority::EXPEDITED);
         priority >= static_cast<int>(Priority::BULK);
         --priority) {
        const std::pair<ContactId, int> key{contact_id, priority};
        auto queue_it = outbound_queues_.find(key);
        if (queue_it == outbound_queues_.end() || queue_it->second.empty()) {
            continue;
        }

        std::shared_ptr<Bundle> bundle = queue_it->second.front();
        queue_it->second.pop_front();
        if (queue_it->second.empty()) {
            outbound_queues_.erase(queue_it);
        }

        if (bundle != nullptr) {
            remove_bundle_from_send_queues(bundle->id);
        }

        return bundle;
    }

    return nullptr;
}

std::vector<ContactId> BundleProtocolAgent::queued_contact_ids() const {
    std::vector<ContactId> ids;
    ids.reserve(outbound_queues_.size());

    for (const auto& entry : outbound_queues_) {
        if (!entry.second.empty()) {
            ids.push_back(entry.first.first);
        }
    }

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

std::vector<std::shared_ptr<Bundle>> BundleProtocolAgent::drain_contact_queue(ContactId contact_id) {
    std::vector<std::shared_ptr<Bundle>> drained;
    while (has_queued_for_contact(contact_id)) {
        std::shared_ptr<Bundle> bundle = dequeue_next_for_contact(contact_id);
        if (bundle != nullptr) {
            drained.push_back(bundle);
        }
    }
    return drained;
}

std::vector<std::shared_ptr<Bundle>> BundleProtocolAgent::drain_all_contact_queues() {
    std::vector<std::shared_ptr<Bundle>> drained;
    const std::vector<ContactId> contact_ids = queued_contact_ids();
    for (ContactId contact_id : contact_ids) {
        std::vector<std::shared_ptr<Bundle>> contact_bundles = drain_contact_queue(contact_id);
        drained.insert(drained.end(), contact_bundles.begin(), contact_bundles.end());
    }
    return drained;
}

std::size_t BundleProtocolAgent::pending_bundle_count() const noexcept {
    return pending_bundles_.size();
}

std::size_t BundleProtocolAgent::delivered_bundle_count() const noexcept {
    return delivered_bundles_.size();
}

std::size_t BundleProtocolAgent::queued_for_contact(ContactId contact_id) const noexcept {
    std::size_t count = 0;
    for (const auto& entry : outbound_queues_) {
        if (entry.first.first == contact_id) {
            count += entry.second.size();
        }
    }
    return count;
}

Volume BundleProtocolAgent::queued_volume_for_contact(ContactId contact_id) const noexcept {
    Volume queued_volume = 0.0;
    for (const auto& entry : outbound_queues_) {
        if (entry.first.first != contact_id) {
            continue;
        }
        for (const std::shared_ptr<Bundle>& bundle : entry.second) {
            if (bundle != nullptr) {
                queued_volume += bundle->evc();
            }
        }
    }
    return queued_volume;
}

bool NodeRuntime::has_contact_plan() const noexcept {
    return node != nullptr && node->contact_plan != nullptr;
}

RuntimeContext::RuntimeContext(ContactPlan* contact_plan)
    : contact_plan_(contact_plan) {}

void RuntimeContext::bind_contact_plan(ContactPlan& plan) {
    contact_plan_ = &plan;
    for (auto& entry : nodes_) {
        if (entry.second.node != nullptr) {
            entry.second.node->contact_plan = contact_plan_;
        }
    }
}

NodeRuntime& RuntimeContext::add_node(Node& node) {
    NodeRuntime& runtime = nodes_[node.node_number];
    runtime.node = &node;
    if (runtime.bpa == nullptr) {
        runtime.bpa = std::make_shared<BundleProtocolAgent>(node);
    } else {
        runtime.bpa->bind_node(node);
    }
    if (contact_plan_ != nullptr) {
        node.contact_plan = contact_plan_;
    }
    return runtime;
}

bool RuntimeContext::has_node(NodeId node_id) const {
    return nodes_.find(node_id) != nodes_.end();
}

NodeRuntime* RuntimeContext::find_node(NodeId node_id) {
    auto it = nodes_.find(node_id);
    return it == nodes_.end() ? nullptr : &it->second;
}

const NodeRuntime* RuntimeContext::find_node(NodeId node_id) const {
    auto it = nodes_.find(node_id);
    return it == nodes_.end() ? nullptr : &it->second;
}

std::vector<NodeId> RuntimeContext::node_ids() const {
    std::vector<NodeId> ids;
    ids.reserve(nodes_.size());
    for (const auto& entry : nodes_) {
        ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void RuntimeContext::set_log_hook(LogHook hook) {
    log_hook_ = std::move(hook);
}

void RuntimeContext::set_stat_hook(StatHook hook) {
    stat_hook_ = std::move(hook);
}

void RuntimeContext::emit_log(const std::string& message) const {
    if (log_hook_) {
        log_hook_(message);
    }
}

void RuntimeContext::emit_stat(const std::string& key, double value) const {
    if (stat_hook_) {
        stat_hook_(key, value);
    }
}