#include "simulation/engine.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {

constexpr double kEpsilon = 1e-9;

bool expired_at(const Bundle& bundle,
                TimePoint now) {
    return now + kEpsilon >= bundle.expiration_time();
}

std::vector<NodeId> sorted_nodes(const std::set<NodeId>& nodes) {
    return std::vector<NodeId>(nodes.begin(), nodes.end());
}

std::string join_node_ids(const std::vector<NodeId>& nodes) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        stream << nodes[index];
    }
    return stream.str();
}

EventRecord make_event_record(EventType type,
                              TimePoint time,
                              const std::string& message,
                              const std::shared_ptr<Bundle>& bundle = nullptr,
                              NodeId node_id = -1,
                              ContactId contact_id = 0,
                              PlanVersion plan_version = 0) {
    EventRecord record;
    record.type = type;
    record.time = time;
    record.bundle_id = bundle == nullptr ? 0 : bundle->id;
    record.has_bundle = bundle != nullptr;
    record.node_id = node_id;
    record.contact_id = contact_id;
    record.plan_version = plan_version;
    record.message = message;
    return record;
}

}  // namespace

SimEngine::SimEngine(EngineConfig config)
        : config_(std::move(config)),
          stats_(config_.start_time, config_.end_time, config_.failure_seed),
          failure_injector_(config_.failure_seed, config_.failure_rules) {}

SimEngine::SimEngine(std::shared_ptr<ContactPlan> contact_plan,
                     std::vector<TrafficPattern> traffic_patterns,
                     EngineConfig config)
    : config_(std::move(config)),
      contact_plan_(std::move(contact_plan)),
            traffic_generator_(std::move(traffic_patterns)),
            stats_(config_.start_time, config_.end_time, config_.failure_seed),
            failure_injector_(config_.failure_seed, config_.failure_rules) {
    if (contact_plan_ != nullptr) {
        runtime_.bind_contact_plan(*contact_plan_);
                stats_.bind_contact_plan(*contact_plan_);
    }
}

void SimEngine::set_contact_plan(std::shared_ptr<ContactPlan> contact_plan) {
    contact_plan_ = std::move(contact_plan);
    if (contact_plan_ != nullptr) {
        runtime_.bind_contact_plan(*contact_plan_);
        stats_.bind_contact_plan(*contact_plan_);
    }
}

void SimEngine::set_traffic_patterns(std::vector<TrafficPattern> traffic_patterns) {
    traffic_generator_.set_patterns(std::move(traffic_patterns));
}

void SimEngine::set_multicast_groups(std::vector<MulticastGroup> groups) {
    std::string failure_reason;
    if (!multicast_registry_.replace_groups(groups, &failure_reason)) {
        throw std::invalid_argument("invalid multicast groups: " + failure_reason);
    }
}

void SimEngine::set_multicast_registry(const MulticastGroupRegistry& registry) {
    multicast_registry_ = registry;
}

void SimEngine::set_scenario_name(std::string scenario_name) {
    stats_.set_metadata(config_.start_time,
                        config_.end_time,
                        config_.failure_seed,
                        std::move(scenario_name));
}

Node& SimEngine::ensure_node(NodeId node_id) {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        auto inserted = nodes_.emplace(node_id, std::make_unique<Node>(node_id));
        it = inserted.first;
        if (contact_plan_ != nullptr) {
            it->second->contact_plan = contact_plan_.get();
        }
        runtime_.add_node(*it->second);
    }
    return *it->second;
}

void SimEngine::initialize() {
    if (initialized_) {
        return;
    }

    if (contact_plan_ != nullptr) {
        runtime_.bind_contact_plan(*contact_plan_);
        for (const Contact& contact : contact_plan_->contacts()) {
            ensure_node(contact.sending_node);
            ensure_node(contact.receiving_node);
        }
    }

    for (const TrafficPattern& pattern : traffic_generator_.patterns()) {
        ensure_node(pattern.source_node);
        if (pattern.destination_node >= 0) {
            ensure_node(pattern.destination_node);
        }
    }

    register_handlers();
    schedule_contact_events(config_.start_time);
    schedule_plan_updates();
    schedule_traffic();
    initialized_ = true;
}

void SimEngine::run() {
    if (!initialized_) {
        initialize();
    }
    scheduler_.run(config_.end_time);
}

std::shared_ptr<Bundle> SimEngine::find_bundle(BundleId bundle_id) const {
    auto it = bundles_.find(bundle_id);
    return it == bundles_.end() ? nullptr : it->second;
}

BundleLifecycleState SimEngine::bundle_state(BundleId bundle_id) const {
    auto it = bundle_states_.find(bundle_id);
    return it == bundle_states_.end() ? BundleLifecycleState::UNKNOWN : it->second;
}

std::vector<BundleId> SimEngine::delivered_bundle_ids() const {
    std::vector<BundleId> ids;
    for (const auto& entry : bundle_states_) {
        if (entry.second == BundleLifecycleState::DELIVERED) {
            ids.push_back(entry.first);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void SimEngine::register_handlers() {
    scheduler_.register_handler(EventType::BUNDLE_CREATED,
        [this](const Event& event) { on_bundle_created(event); });
    scheduler_.register_handler(EventType::BUNDLE_ARRIVED,
        [this](const Event& event) { on_bundle_arrived(event); });
    scheduler_.register_handler(EventType::BUNDLE_TX_START,
        [this](const Event& event) { on_bundle_tx_start(event); });
    scheduler_.register_handler(EventType::BUNDLE_TX_END,
        [this](const Event& event) { on_bundle_tx_end(event); });
    scheduler_.register_handler(EventType::BUNDLE_DELIVERED,
        [this](const Event& event) { on_bundle_delivered(event); });
    scheduler_.register_handler(EventType::BUNDLE_EXPIRED,
        [this](const Event& event) { on_bundle_expired(event); });
    scheduler_.register_handler(EventType::DUPLICATE_DROPPED,
        [this](const Event& event) { on_duplicate_dropped(event); });
    scheduler_.register_handler(EventType::CONTACT_START,
        [this](const Event& event) { on_contact_start(event); });
    scheduler_.register_handler(EventType::CONTACT_END,
        [this](const Event& event) { on_contact_end(event); });
    scheduler_.register_handler(EventType::ROUTE_FAILED,
        [this](const Event& event) { on_route_failed(event); });
    scheduler_.register_handler(EventType::PLAN_UPDATED,
        [this](const Event& event) { on_plan_updated(event); });
}

void SimEngine::schedule_contact_events(TimePoint from_time) {
    if (contact_plan_ == nullptr) {
        return;
    }

    for (const Contact& contact : contact_plan_->contacts()) {
        if (contact.end_time <= from_time || contact.start_time > config_.end_time) {
            continue;
        }

        const TimePoint start_time = contact.start_time <= from_time
            ? from_time
            : contact.start_time;
        scheduler_.schedule({
            start_time,
            0,
            0,
            EventType::CONTACT_START,
            ContactStateData{contact.contact_id, contact.plan_version}});

        if (contact.end_time > from_time && contact.end_time <= config_.end_time) {
            scheduler_.schedule({
                contact.end_time,
                0,
                0,
                EventType::CONTACT_END,
                ContactStateData{contact.contact_id, contact.plan_version}});
        }
    }
}

void SimEngine::schedule_plan_updates() {
    failure_injector_.set_seed(config_.failure_seed);
    failure_injector_.set_rules(config_.failure_rules);

    ContactPlan empty_plan;
    const ContactPlan& base_plan = contact_plan_ == nullptr ? empty_plan : *contact_plan_;
    const std::vector<PlanUpdateData> updates = failure_injector_.materialize(
        base_plan,
        config_.start_time,
        config_.end_time);

    for (const PlanUpdateData& update : updates) {
        scheduler_.schedule({update.trigger_time, 0, 0, EventType::PLAN_UPDATED, update});
    }
}

void SimEngine::schedule_traffic() {
    std::vector<ScheduledBundle> scheduled = traffic_generator_.materialize(
        config_.start_time,
        config_.end_time,
        next_bundle_id_);

    for (const ScheduledBundle& request : scheduled) {
        if (request.bundle == nullptr) {
            continue;
        }

        bundles_[request.bundle->id] = request.bundle;
        bundle_states_[request.bundle->id] = BundleLifecycleState::ACTIVE;
        scheduler_.schedule({
            request.timestamp,
            0,
            0,
            EventType::BUNDLE_CREATED,
            BundleCreatedData{request.bundle, request.source_node, request.multicast_group_id}});
        scheduler_.schedule({
            request.bundle->expiration_time(),
            0,
            0,
            EventType::BUNDLE_EXPIRED,
            BundleExpiredData{request.bundle}});
    }
}

void SimEngine::process_bundle(const std::shared_ptr<Bundle>& bundle,
                               NodeId at_node,
                               TimePoint now) {
    if (bundle == nullptr || is_terminal(bundle)) {
        return;
    }

    if (expired_at(*bundle, now)) {
        scheduler_.schedule({now, 0, 0, EventType::BUNDLE_EXPIRED, BundleExpiredData{bundle}});
        return;
    }

    if (bundle->route_path.empty() || bundle->route_path.back() != at_node) {
        bundle->route_path.push_back(at_node);
    }
    if (!bundle->has_visited(at_node)) {
        bundle->mark_visited(at_node);
    }

    if (bundle->is_multicast) {
        process_multicast_bundle(bundle, at_node, now);
        return;
    }

    if (at_node == bundle->destination_eid) {
        scheduler_.schedule({now, 0, 0, EventType::BUNDLE_DELIVERED, BundleDeliveredData{bundle, at_node}});
        return;
    }

    Node& node = ensure_node(at_node);
    const std::size_t route_attempt = note_route_attempt(*bundle);

    const RoutingTrace trace = route_unicast_with_reactive_guard(node, *bundle, at_node, now);

    if (!trace.reroute_reason.empty()) {
        std::ostringstream stream;
        stream << "REACTIVE_ANTI_LOOP bundle=" << bundle->id
               << " node=" << at_node
               << " reroutes=" << trace.reroute_count;
        if (!trace.reroute_excluded_neighbors.empty()) {
            stream << " excluded=" << join_node_ids(trace.reroute_excluded_neighbors);
        }
        log_event(stream.str());
    }

    logger_.record_routing(*bundle, at_node, now, route_attempt, trace);
    stats_.note_routing_trace(*bundle, at_node, now, route_attempt, trace);

    const RoutingDecision& decision = trace.decision;
    switch (decision.action) {
    case RoutingAction::DELIVER_LOCAL:
        scheduler_.schedule({now, 0, 0, EventType::BUNDLE_DELIVERED, BundleDeliveredData{bundle, at_node}});
        break;
    case RoutingAction::FORWARD_ONE:
        if (decision.primary.has_value()) {
            forward_unicast_with_redundancy(bundle, *decision.primary, trace.backup_routes, now);
        } else {
            scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, at_node}});
        }
        break;
    case RoutingAction::FORWARD_FLOOD:
        if (decision.flood_routes.empty()) {
            scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, at_node}});
            break;
        }

        for (std::size_t index = 0; index < decision.flood_routes.size(); ++index) {
            std::shared_ptr<Bundle> forwarding_bundle = bundle;
            if (index > 0) {
                Bundle replica = bundle->make_replica(next_bundle_id_, next_bundle_id_);
                ++next_bundle_id_;
                forwarding_bundle = std::make_shared<Bundle>(std::move(replica));
                bundles_[forwarding_bundle->id] = forwarding_bundle;
                bundle_states_[forwarding_bundle->id] = BundleLifecycleState::ACTIVE;
                scheduler_.schedule({
                    forwarding_bundle->expiration_time(),
                    0,
                    0,
                    EventType::BUNDLE_EXPIRED,
                    BundleExpiredData{forwarding_bundle}});
            }
            forward_along_route(forwarding_bundle, decision.flood_routes[index].route, now);
        }
        break;
    case RoutingAction::ROUTE_FAIL:
        scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, at_node}});
        break;
    }
}

RoutingContext SimEngine::make_routing_context(Node& node,
                                               const Bundle& bundle,
                                               TimePoint now) const {
    RoutingContext context(node, bundle, now);
    context.phase1_config = config_.phase1_config;
    context.phase2_config = config_.phase2_config;
    context.phase2_config.owlt_margin = config_.owlt_margin;
    context.validation_context = build_validation_context();
    context.redundancy_config = config_.redundancy;
    context.recompute_budget = config_.recompute_budget;
    return context;
}

Phase2::ValidationContext SimEngine::build_validation_context() const {
    Phase2::ValidationContext context;
    for (NodeId node_id : runtime_.node_ids()) {
        const NodeRuntime* node_runtime = runtime_.find_node(node_id);
        if (node_runtime == nullptr || node_runtime->bpa == nullptr) {
            continue;
        }

        for (ContactId contact_id : node_runtime->bpa->queued_contact_ids()) {
            context.allocated_bytes[contact_id] += node_runtime->bpa->queued_volume_for_contact(contact_id);
        }
    }
    return context;
}

RoutingTrace SimEngine::route_unicast_with_reactive_guard(Node& node,
                                                          const Bundle& bundle,
                                                          NodeId at_node,
                                                          TimePoint now) {
    RoutingTrace trace;
    const std::set<NodeId> original_excluded = node.excluded_neighbors;
    std::set<NodeId> reactive_exclusions;
    std::set<NodeId> outbound_neighbors;
    if (node.contact_plan != nullptr) {
        for (const Contact* contact : node.contact_plan->get_contacts_from(at_node)) {
            if (contact != nullptr) {
                outbound_neighbors.insert(contact->receiving_node);
            }
        }
    }

    const int max_guard_attempts = std::max(1, static_cast<int>(outbound_neighbors.size()));
    std::size_t reroute_count = 0;

    for (int attempt = 0; attempt <= max_guard_attempts; ++attempt) {
        trace = cgr_router_.route_unicast_with_trace(make_routing_context(node, bundle, now));
        if (!config_.phase2_config.anti_loop_reactive) {
            break;
        }

        const std::set<NodeId> offending_neighbors = collect_reactive_loop_neighbors(trace.decision, bundle, at_node);
        if (offending_neighbors.empty()) {
            break;
        }

        const std::size_t before = reactive_exclusions.size();
        reactive_exclusions.insert(offending_neighbors.begin(), offending_neighbors.end());
        if (reactive_exclusions.size() == before || attempt == max_guard_attempts) {
            trace.decision = RoutingDecision{};
            trace.decision.action = RoutingAction::ROUTE_FAIL;
            trace.decision.failure_reason = "reactive_anti_loop_exhausted";
            break;
        }

        ++reroute_count;
        node.excluded_neighbors = original_excluded;
        node.excluded_neighbors.insert(reactive_exclusions.begin(), reactive_exclusions.end());
    }

    node.excluded_neighbors = original_excluded;
    if (!reactive_exclusions.empty()) {
        trace.reroute_reason = "reactive_anti_loop";
        trace.reroute_count = reroute_count;
        trace.reroute_excluded_neighbors = sorted_nodes(reactive_exclusions);
    }

    return trace;
}

bool SimEngine::route_revisits_visited_node(const Route& route,
                                            const Bundle& bundle,
                                            NodeId at_node) const {
    if (route.local_delivery) {
        return false;
    }

    for (const RouteLeg& leg : route.legs) {
        if (leg.contact == nullptr) {
            continue;
        }
        if (leg.contact->sending_node != at_node && bundle.has_visited(leg.contact->sending_node)) {
            return true;
        }
        if (leg.contact->receiving_node != at_node && bundle.has_visited(leg.contact->receiving_node)) {
            return true;
        }
    }

    return false;
}

std::set<NodeId> SimEngine::collect_reactive_loop_neighbors(const RoutingDecision& decision,
                                                            const Bundle& bundle,
                                                            NodeId at_node) const {
    std::set<NodeId> offending_neighbors;
    auto collect_from_route = [&](const Route& route) {
        if (route.entry_node >= 0 && route_revisits_visited_node(route, bundle, at_node)) {
            offending_neighbors.insert(route.entry_node);
        }
    };

    switch (decision.action) {
    case RoutingAction::FORWARD_ONE:
        if (decision.primary.has_value()) {
            collect_from_route(decision.primary->route);
        }
        break;
    case RoutingAction::FORWARD_FLOOD:
        for (const CandidateRoute& candidate : decision.flood_routes) {
            collect_from_route(candidate.route);
        }
        break;
    case RoutingAction::DELIVER_LOCAL:
    case RoutingAction::ROUTE_FAIL:
        break;
    }

    return offending_neighbors;
}

bool SimEngine::initialize_multicast_bundle(const std::shared_ptr<Bundle>& bundle,
                                           NodeId source_node,
                                           const std::string& multicast_group_id,
                                           TimePoint now) {
    if (bundle == nullptr || !bundle->is_multicast) {
        return true;
    }
    if (!bundle->pending_destinations.empty() || !bundle->multicast_plan.nodes.empty()) {
        return true;
    }
    if (multicast_group_id.empty()) {
        return false;
    }

    Node& source = ensure_node(source_node);
    MulticastPlanRequest request;
    request.source_node = &source;
    request.bundle = bundle.get();
    request.current_time = now;
    request.phase1_config = config_.phase1_config;
    request.phase2_config = config_.phase2_config;
    request.phase2_config.owlt_margin = config_.owlt_margin;
    request.recompute_budget = config_.recompute_budget;

    const MulticastPlanResult result = multicast_planner_.build_plan_for_group(
        request,
        multicast_registry_,
        multicast_group_id);
    if (!result.ok() || result.reachable_destinations.empty()) {
        return false;
    }

    bundle->pending_destinations = result.reachable_destinations;
    bundle->multicast_plan = result.tree;
    bundle->encoded_plan_version = contact_plan_ == nullptr ? 0 : contact_plan_->version();
    if (bundle->origin_bundle_id == 0) {
        bundle->origin_bundle_id = bundle->id;
    }
    if (bundle->replica_id == 0) {
        bundle->replica_id = bundle->id;
    }

    return true;
}

void SimEngine::process_multicast_bundle(const std::shared_ptr<Bundle>& bundle,
                                         NodeId at_node,
                                         TimePoint now) {
    if (bundle == nullptr) {
        return;
    }

    BundleId next_bundle_id = next_bundle_id_;
    const MulticastExecutionResult execution = MulticastPlanner::plan_forwarding(*bundle, at_node, &next_bundle_id);
    if (!execution.ok()) {
        if (execution.deliver_local && execution.dispatches.empty()) {
            scheduler_.schedule({now, 0, 0, EventType::BUNDLE_DELIVERED, BundleDeliveredData{bundle, at_node}});
            return;
        }
        scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, at_node}});
        return;
    }

    std::vector<MulticastDispatch> ready_dispatches;
    ready_dispatches.reserve(execution.dispatches.size());
    std::set<NodeId> affected_destinations;
    for (const MulticastDispatch& dispatch : execution.dispatches) {
        if (is_multicast_contact_usable(dispatch.contact_id, bundle->encoded_plan_version, now)) {
            ready_dispatches.push_back(dispatch);
            continue;
        }

        affected_destinations.insert(dispatch.destinations.begin(), dispatch.destinations.end());
    }

    if (!affected_destinations.empty()) {
        std::vector<MulticastDispatch> repaired_dispatches;
        if (plan_multicast_repair_dispatches(
                bundle,
                at_node,
                affected_destinations,
                now,
                ready_dispatches.empty(),
                &next_bundle_id,
                &repaired_dispatches)) {
            std::ostringstream repair_stream;
            repair_stream << "LOCAL_REPAIR bundle=" << bundle->id
                          << " node=" << at_node
                          << " affected=" << affected_destinations.size()
                          << " time=" << now;
            log_event(repair_stream.str());
            stats_.note_local_repair();
            ready_dispatches.insert(
                ready_dispatches.end(),
                repaired_dispatches.begin(),
                repaired_dispatches.end());
        } else if (ready_dispatches.empty()) {
            scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, at_node}});
            return;
        }
    }

    if (execution.deliver_local && !ready_dispatches.empty()) {
        if (bundle->mark_destination_delivered(at_node)) {
            Node& node = ensure_node(at_node);
            NodeRuntime* runtime = runtime_.find_node(node.node_number);
            if (runtime != nullptr && runtime->bpa != nullptr) {
                runtime->bpa->deliver_local(bundle);
            }

            std::ostringstream stream;
            stream << "BUNDLE_DELIVERED_LOCAL bundle=" << bundle->id
                   << " node=" << at_node
                   << " time=" << now;
            log_event(stream.str());
            logger_.record_event(make_event_record(
                EventType::BUNDLE_DELIVERED,
                now,
                stream.str(),
                bundle,
                at_node));
        }
    }

    if (execution.deliver_local && ready_dispatches.empty()) {
        scheduler_.schedule({now, 0, 0, EventType::BUNDLE_DELIVERED, BundleDeliveredData{bundle, at_node}});
        return;
    }

    if (ready_dispatches.empty()) {
        scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, at_node}});
        return;
    }

    if (!bundle->redundancy_applied &&
        config_.redundancy.mode != RedundancyMode::NONE &&
        config_.redundancy.enable_for_multicast_trunk) {
        const std::size_t max_trunk_backups = config_.redundancy.mode == RedundancyMode::MULTI_BACKUP
            ? config_.redundancy.max_extra_copies
            : std::min<std::size_t>(1u, config_.redundancy.max_extra_copies);
        std::vector<MulticastDispatch> sorted_dispatches = ready_dispatches;
        std::sort(sorted_dispatches.begin(), sorted_dispatches.end(),
            [](const MulticastDispatch& lhs, const MulticastDispatch& rhs) {
                return lhs.destinations.size() > rhs.destinations.size();
            });

        std::vector<MulticastDispatch> backup_dispatches;
        for (const MulticastDispatch& dispatch : sorted_dispatches) {
            if (backup_dispatches.size() >= max_trunk_backups) {
                break;
            }
            if (!should_apply_multicast_trunk_redundancy(*bundle, at_node, dispatch)) {
                continue;
            }

            MulticastDispatch backup_dispatch;
            if (plan_multicast_backup_dispatch(bundle, at_node, dispatch, now, &next_bundle_id, &backup_dispatch)) {
                backup_dispatches.push_back(std::move(backup_dispatch));
            }
        }

        if (!backup_dispatches.empty()) {
            if (bundle->origin_bundle_id == 0) {
                bundle->origin_bundle_id = bundle->id;
            }
            if (bundle->replica_id == 0) {
                bundle->replica_id = bundle->id;
            }
            bundle->redundancy_applied = true;
            bundle->replica_role = ReplicaRole::PRIMARY;
            note_redundancy_family(*bundle, backup_dispatches.size());

            for (MulticastDispatch& dispatch : ready_dispatches) {
                dispatch.bundle.origin_bundle_id = bundle->canonical_origin_id();
                if (dispatch.bundle.replica_id == 0) {
                    dispatch.bundle.replica_id = dispatch.bundle.id;
                }
                dispatch.bundle.redundancy_applied = true;
            }

            std::ostringstream redundancy_stream;
            redundancy_stream << "REDUNDANCY_TRIGGER bundle=" << bundle->id
                              << " node=" << at_node
                              << " trunk_backups=" << backup_dispatches.size();
            log_event(redundancy_stream.str());

            ready_dispatches.insert(
                ready_dispatches.end(),
                backup_dispatches.begin(),
                backup_dispatches.end());
        }
    }

    next_bundle_id_ = next_bundle_id;
    for (const MulticastDispatch& dispatch : ready_dispatches) {
        std::shared_ptr<Bundle> forwarding_bundle = bundle;
        if (dispatch.bundle.id == bundle->id) {
            *forwarding_bundle = dispatch.bundle;
        } else {
            forwarding_bundle = std::make_shared<Bundle>(dispatch.bundle);
            bundles_[forwarding_bundle->id] = forwarding_bundle;
            bundle_states_[forwarding_bundle->id] = BundleLifecycleState::ACTIVE;
            scheduler_.schedule({
                forwarding_bundle->expiration_time(),
                0,
                0,
                EventType::BUNDLE_EXPIRED,
                BundleExpiredData{forwarding_bundle}});
        }

        forward_to_contact(forwarding_bundle, dispatch.contact_id, now);
    }
}

bool SimEngine::is_multicast_contact_usable(ContactId contact_id,
                                            PlanVersion encoded_plan_version,
                                            TimePoint now) const {
    if (contact_plan_ == nullptr) {
        return false;
    }

    const Contact* contact = contact_plan_->get_contact(contact_id);
    if (contact == nullptr || contact->is_terminated(now)) {
        return false;
    }

    return encoded_plan_version == 0 || contact->plan_version == encoded_plan_version;
}

bool SimEngine::plan_multicast_repair_dispatches(const std::shared_ptr<Bundle>& bundle,
                                                 NodeId at_node,
                                                 const std::set<NodeId>& affected_destinations,
                                                 TimePoint now,
                                                 bool reuse_original_bundle,
                                                 BundleId* next_bundle_id,
                                                 std::vector<MulticastDispatch>* repaired_dispatches) {
    if (bundle == nullptr || contact_plan_ == nullptr || repaired_dispatches == nullptr) {
        return false;
    }

    Node& current_node = ensure_node(at_node);
    Bundle repair_bundle = *bundle;
    repair_bundle.pending_destinations = affected_destinations;
    repair_bundle.destination_eid = -1;

    MulticastPlanRequest request;
    request.source_node = &current_node;
    request.bundle = &repair_bundle;
    request.destinations = affected_destinations;
    request.current_time = now;
    request.phase1_config = config_.phase1_config;
    request.phase2_config = config_.phase2_config;
    request.phase2_config.owlt_margin = config_.owlt_margin;
    request.recompute_budget = config_.recompute_budget;

    const MulticastPlanResult repair_plan = multicast_planner_.build_plan(request);
    if (!repair_plan.ok() || repair_plan.reachable_destinations.empty()) {
        return false;
    }

    repair_bundle.pending_destinations = repair_plan.reachable_destinations;
    repair_bundle.multicast_plan = repair_plan.tree;
    repair_bundle.encoded_plan_version = contact_plan_->version();
    repair_bundle.origin_bundle_id = bundle->origin_bundle_id == 0 ? bundle->id : bundle->origin_bundle_id;

    if (reuse_original_bundle) {
        repair_bundle.id = bundle->id;
        repair_bundle.parent_bundle_id = bundle->parent_bundle_id;
        repair_bundle.replica_id = bundle->replica_id == 0 ? bundle->id : bundle->replica_id;
    } else {
        if (next_bundle_id == nullptr || *next_bundle_id == 0) {
            return false;
        }
        repair_bundle.id = *next_bundle_id;
        repair_bundle.parent_bundle_id = bundle->id;
        repair_bundle.replica_id = *next_bundle_id;
        ++(*next_bundle_id);
    }

    const MulticastExecutionResult repair_execution = MulticastPlanner::plan_forwarding(
        repair_bundle,
        at_node,
        next_bundle_id);
    if (!repair_execution.ok() || repair_execution.dispatches.empty()) {
        return false;
    }

    repaired_dispatches->insert(
        repaired_dispatches->end(),
        repair_execution.dispatches.begin(),
        repair_execution.dispatches.end());
    return true;
}

void SimEngine::forward_along_route(const std::shared_ptr<Bundle>& bundle,
                                    const Route& route,
                                    TimePoint now) {
    if (bundle == nullptr) {
        return;
    }
    if (route.local_delivery) {
        scheduler_.schedule({now, 0, 0, EventType::BUNDLE_DELIVERED, BundleDeliveredData{bundle, route.local_delivery_node}});
        return;
    }
    if (route.legs.empty() || route.legs.front().contact == nullptr) {
        scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, bundle->previous_node}});
        return;
    }

    Contact* contact = route.legs.front().contact;
    Node& node = ensure_node(contact->sending_node);
    NodeRuntime* runtime = runtime_.find_node(node.node_number);
    if (runtime == nullptr || runtime->bpa == nullptr) {
        scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, node.node_number}});
        return;
    }

    runtime->bpa->enqueue_for_contact(bundle, contact->contact_id, bundle->priority);
    commit_first_contact(*bundle, route);
    ++metrics_.bundles_forwarded;
    stats_.note_bundle_forwarded(*bundle);
    logger_.record_commit(*bundle, route, now, 1);
    stats_.note_commit(*bundle, route, now, 1);

    std::ostringstream stream;
    stream << "FORWARD_ENQUEUED bundle=" << bundle->id
           << " contact=" << contact->contact_id
           << " time=" << now;
    log_event(stream.str());

    schedule_contact_dispatch(contact->contact_id, now);
}

void SimEngine::forward_unicast_with_redundancy(const std::shared_ptr<Bundle>& bundle,
                                                const CandidateRoute& primary,
                                                const std::vector<CandidateRoute>& backup_routes,
                                                TimePoint now) {
    if (bundle == nullptr) {
        return;
    }

    if (bundle->origin_bundle_id == 0) {
        bundle->origin_bundle_id = bundle->id;
    }
    if (bundle->replica_id == 0) {
        bundle->replica_id = bundle->id;
    }

    if (backup_routes.empty()) {
        forward_along_route(bundle, primary.route, now);
        return;
    }

    bundle->redundancy_applied = true;
    bundle->replica_role = ReplicaRole::PRIMARY;
    note_redundancy_family(*bundle, backup_routes.size());

        const NodeId redundancy_node = bundle->route_path.empty() ? bundle->source_node : bundle->route_path.back();
    std::ostringstream stream;
    stream << "REDUNDANCY_TRIGGER bundle=" << bundle->id
            << " node=" << redundancy_node
           << " backups=" << backup_routes.size();
    log_event(stream.str());

    forward_along_route(bundle, primary.route, now);
    for (const CandidateRoute& backup_route : backup_routes) {
        Bundle replica = bundle->make_replica(next_bundle_id_, next_bundle_id_);
        replica.redundancy_applied = true;
        replica.replica_role = ReplicaRole::REDUNDANT_BACKUP;
        std::shared_ptr<Bundle> forwarding_bundle = std::make_shared<Bundle>(std::move(replica));
        bundles_[forwarding_bundle->id] = forwarding_bundle;
        bundle_states_[forwarding_bundle->id] = BundleLifecycleState::ACTIVE;
        scheduler_.schedule({
            forwarding_bundle->expiration_time(),
            0,
            0,
            EventType::BUNDLE_EXPIRED,
            BundleExpiredData{forwarding_bundle}});
        ++next_bundle_id_;
        forward_along_route(forwarding_bundle, backup_route.route, now);
    }
}

void SimEngine::forward_to_contact(const std::shared_ptr<Bundle>& bundle,
                                   ContactId contact_id,
                                   TimePoint now) {
    if (bundle == nullptr || contact_plan_ == nullptr) {
        return;
    }

    Contact* contact = contact_plan_->get_contact(contact_id);
    if (contact == nullptr) {
        scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, bundle->previous_node}});
        return;
    }

    Node& node = ensure_node(contact->sending_node);
    NodeRuntime* runtime = runtime_.find_node(node.node_number);
    if (runtime == nullptr || runtime->bpa == nullptr) {
        scheduler_.schedule({now, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, node.node_number}});
        return;
    }

    runtime->bpa->enqueue_for_contact(bundle, contact_id, bundle->priority);

    Route route;
    route.legs.push_back({contact});
    ++metrics_.bundles_forwarded;
    stats_.note_bundle_forwarded(*bundle);
    logger_.record_commit(*bundle, route, now, 1);
    stats_.note_commit(*bundle, route, now, 1);

    std::ostringstream stream;
    stream << "FORWARD_ENQUEUED bundle=" << bundle->id
           << " contact=" << contact_id
           << " time=" << now;
    log_event(stream.str());

    schedule_contact_dispatch(contact_id, now);
}

bool SimEngine::should_apply_multicast_trunk_redundancy(const Bundle& bundle,
                                                        NodeId,
                                                        const MulticastDispatch& dispatch) const {
    if (!bundle.is_multicast || bundle.redundancy_applied) {
        return false;
    }
    if (config_.redundancy.mode == RedundancyMode::NONE || !config_.redundancy.enable_for_multicast_trunk) {
        return false;
    }
    return dispatch.destinations.size() > 1u;
}

bool SimEngine::plan_multicast_backup_dispatch(const std::shared_ptr<Bundle>& bundle,
                                               NodeId at_node,
                                               const MulticastDispatch& primary_dispatch,
                                               TimePoint now,
                                               BundleId* next_bundle_id,
                                               MulticastDispatch* backup_dispatch) {
    if (bundle == nullptr || backup_dispatch == nullptr || contact_plan_ == nullptr ||
        next_bundle_id == nullptr || *next_bundle_id == 0) {
        return false;
    }

    Node& current_node = ensure_node(at_node);
    const std::set<NodeId> original_excluded = current_node.excluded_neighbors;
    current_node.excluded_neighbors.insert(primary_dispatch.next_hop);

    Bundle backup_bundle = primary_dispatch.bundle;
    backup_bundle.pending_destinations = primary_dispatch.destinations;
    backup_bundle.destination_eid = -1;

    MulticastPlanRequest request;
    request.source_node = &current_node;
    request.bundle = &backup_bundle;
    request.destinations = primary_dispatch.destinations;
    request.current_time = now;
    request.phase1_config = config_.phase1_config;
    request.phase2_config = config_.phase2_config;
    request.phase2_config.owlt_margin = config_.owlt_margin;
    request.validation_context = build_validation_context();
    request.recompute_budget = config_.recompute_budget;

    const MulticastPlanResult repair_plan = multicast_planner_.build_plan(request);
    current_node.excluded_neighbors = original_excluded;
    if (!repair_plan.ok() || repair_plan.reachable_destinations != primary_dispatch.destinations) {
        return false;
    }

    backup_bundle.id = *next_bundle_id;
    backup_bundle.parent_bundle_id = bundle->id;
    backup_bundle.origin_bundle_id = bundle->canonical_origin_id();
    backup_bundle.replica_id = *next_bundle_id;
    backup_bundle.replica_role = ReplicaRole::MULTICAST_TRUNK_BACKUP;
    backup_bundle.redundancy_applied = true;
    backup_bundle.multicast_plan = repair_plan.tree;
    backup_bundle.encoded_plan_version = contact_plan_->version();
    ++(*next_bundle_id);

    const MulticastExecutionResult execution = MulticastPlanner::plan_forwarding(
        backup_bundle,
        at_node,
        next_bundle_id);
    if (!execution.ok() || execution.dispatches.size() != 1u) {
        return false;
    }
    if (execution.dispatches.front().destinations != primary_dispatch.destinations ||
        execution.dispatches.front().next_hop == primary_dispatch.next_hop) {
        return false;
    }

    *backup_dispatch = execution.dispatches.front();
    return true;
}

void SimEngine::schedule_contact_dispatch(ContactId contact_id,
                                          TimePoint now) {
    if (contact_plan_ == nullptr || contact_busy_[contact_id] || dispatch_scheduled_[contact_id]) {
        return;
    }

    const Contact* contact = contact_plan_->get_contact(contact_id);
    if (contact == nullptr) {
        return;
    }
    if (now > contact->end_time + kEpsilon) {
        return;
    }

    NodeRuntime* sender_runtime = runtime_.find_node(contact->sending_node);
    if (sender_runtime == nullptr || sender_runtime->bpa == nullptr ||
        !sender_runtime->bpa->has_queued_for_contact(contact_id)) {
        return;
    }

    dispatch_scheduled_[contact_id] = true;
    scheduler_.schedule({
        std::max(now, contact->start_time),
        0,
        0,
        EventType::BUNDLE_TX_START,
        BundleTxStartData{nullptr, contact_id, contact->plan_version}});
}

void SimEngine::commit_first_contact(const Bundle& bundle,
                                     const Route& route) {
    if (route.local_delivery || route.legs.empty() || route.legs.front().contact == nullptr) {
        return;
    }

    Route first_leg_route;
    first_leg_route.legs.push_back(route.legs.front());
    cgr_router_.commit_route(bundle, first_leg_route);
}

std::size_t SimEngine::note_route_attempt(const Bundle& bundle) {
    ++metrics_.routing_invocations;
    std::size_t& attempts = metrics_.route_attempts[bundle.id];
    ++attempts;
    if (attempts > 1) {
        ++metrics_.reroute_invocations;
    }
    return attempts;
}

bool SimEngine::is_terminal(const std::shared_ptr<Bundle>& bundle) const {
    if (bundle == nullptr) {
        return true;
    }
    auto it = bundle_states_.find(bundle->id);
    return it != bundle_states_.end() && it->second != BundleLifecycleState::ACTIVE;
}

void SimEngine::log_event(const std::string& message) {
    event_log_.push_back(message);
    runtime_.emit_log(message);
}

bool SimEngine::is_current_contact(ContactId contact_id,
                                   PlanVersion plan_version) const {
    if (contact_plan_ == nullptr) {
        return false;
    }

    const Contact* contact = contact_plan_->get_contact(contact_id);
    return contact != nullptr && contact->plan_version == plan_version;
}

void SimEngine::mark_plan_rebound() {
    if (contact_plan_ == nullptr) {
        return;
    }

    runtime_.bind_contact_plan(*contact_plan_);
    for (const Contact& contact : contact_plan_->contacts()) {
        ensure_node(contact.sending_node);
        ensure_node(contact.receiving_node);
    }

    for (auto& entry : nodes_) {
        entry.second->contact_plan = contact_plan_.get();
        entry.second->invalidate_routes();
    }
}

void SimEngine::reschedule_queued_contacts(TimePoint now) {
    std::set<ContactId> queued_contact_ids;
    for (NodeId node_id : runtime_.node_ids()) {
        NodeRuntime* node_runtime = runtime_.find_node(node_id);
        if (node_runtime == nullptr || node_runtime->bpa == nullptr) {
            continue;
        }

        const std::vector<ContactId> node_contacts = node_runtime->bpa->queued_contact_ids();
        queued_contact_ids.insert(node_contacts.begin(), node_contacts.end());
    }

    for (ContactId contact_id : queued_contact_ids) {
        dispatch_scheduled_.erase(contact_id);
        schedule_contact_dispatch(contact_id, now);
    }
}

std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>> SimEngine::drain_contact_queues(
    const std::vector<ContactId>& contact_ids) {
    std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>> drained;
    std::set<std::pair<NodeId, BundleId>> seen;
    std::vector<ContactId> ids = contact_ids;
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    for (NodeId node_id : runtime_.node_ids()) {
        NodeRuntime* node_runtime = runtime_.find_node(node_id);
        if (node_runtime == nullptr || node_runtime->bpa == nullptr) {
            continue;
        }

        for (ContactId contact_id : ids) {
            std::vector<std::shared_ptr<Bundle>> queued = node_runtime->bpa->drain_contact_queue(contact_id);
            for (const std::shared_ptr<Bundle>& bundle : queued) {
                if (bundle == nullptr) {
                    continue;
                }

                if (seen.insert({node_id, bundle->id}).second) {
                    drained.emplace_back(node_id, bundle);
                }
            }
        }
    }

    return drained;
}

std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>> SimEngine::drain_all_queues() {
    std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>> drained;
    std::set<std::pair<NodeId, BundleId>> seen;

    for (NodeId node_id : runtime_.node_ids()) {
        NodeRuntime* node_runtime = runtime_.find_node(node_id);
        if (node_runtime == nullptr || node_runtime->bpa == nullptr) {
            continue;
        }

        std::vector<std::shared_ptr<Bundle>> queued = node_runtime->bpa->drain_all_contact_queues();
        for (const std::shared_ptr<Bundle>& bundle : queued) {
            if (bundle == nullptr) {
                continue;
            }

            if (seen.insert({node_id, bundle->id}).second) {
                drained.emplace_back(node_id, bundle);
            }
        }
    }

    return drained;
}

void SimEngine::reroute_drained_bundles(
    const std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>>& drained,
    TimePoint now) {
    for (const auto& entry : drained) {
        if (entry.second == nullptr || is_terminal(entry.second)) {
            continue;
        }

        ++metrics_.queued_bundle_replans;
        stats_.note_queued_bundle_replan();
        process_bundle(entry.second, entry.first, now);
    }
}

void SimEngine::note_redundancy_family(const Bundle& bundle,
                                       std::size_t extra_copy_count) {
    if (extra_copy_count == 0u) {
        return;
    }

    RedundancyFamilyState& state = redundancy_families_[bundle.canonical_origin_id()];
    state.extra_copy_count += extra_copy_count;
    stats_.note_redundancy_trigger(extra_copy_count);
}

void SimEngine::on_bundle_created(const Event& event) {
    const auto& data = std::get<BundleCreatedData>(event.payload);
    if (data.bundle == nullptr) {
        return;
    }

    bundle_states_[data.bundle->id] = BundleLifecycleState::ACTIVE;
    ++metrics_.bundles_created;
    stats_.note_bundle_created(*data.bundle);

    Node& node = ensure_node(data.source_node);
    NodeRuntime* runtime = runtime_.find_node(node.node_number);
    if (runtime != nullptr && runtime->bpa != nullptr) {
        runtime->bpa->on_bundle_created(data.bundle);
    }

    std::ostringstream stream;
    stream << "BUNDLE_CREATED bundle=" << data.bundle->id
           << " node=" << data.source_node
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::BUNDLE_CREATED,
        event.timestamp,
        stream.str(),
        data.bundle,
        data.source_node));

    if (data.bundle->is_multicast &&
        !initialize_multicast_bundle(data.bundle, data.source_node, data.multicast_group_id, event.timestamp)) {
        scheduler_.schedule({event.timestamp, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{data.bundle, data.source_node}});
        return;
    }

    process_bundle(data.bundle, data.source_node, event.timestamp);
}

void SimEngine::on_bundle_arrived(const Event& event) {
    const auto& data = std::get<BundleArrivedData>(event.payload);
    if (data.bundle == nullptr || is_terminal(data.bundle)) {
        return;
    }

    if (expired_at(*data.bundle, event.timestamp)) {
        scheduler_.schedule({event.timestamp, 0, 0, EventType::BUNDLE_EXPIRED, BundleExpiredData{data.bundle}});
        return;
    }

    Node& node = ensure_node(data.arrived_at);
    NodeRuntime* runtime = runtime_.find_node(node.node_number);
    if (runtime != nullptr && runtime->bpa != nullptr &&
        !runtime->bpa->note_redundant_arrival(*data.bundle)) {
        scheduler_.schedule({
            event.timestamp,
            0,
            0,
            EventType::DUPLICATE_DROPPED,
            DuplicateDroppedData{data.bundle, data.arrived_at}});
        return;
    }
    if (runtime != nullptr && runtime->bpa != nullptr) {
        runtime->bpa->on_bundle_arrived(data.bundle);
    }

    std::ostringstream stream;
    stream << "BUNDLE_ARRIVED bundle=" << data.bundle->id
           << " node=" << data.arrived_at
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::BUNDLE_ARRIVED,
        event.timestamp,
        stream.str(),
        data.bundle,
        data.arrived_at));

    process_bundle(data.bundle, data.arrived_at, event.timestamp);
}

void SimEngine::on_bundle_tx_start(const Event& event) {
    const auto& data = std::get<BundleTxStartData>(event.payload);
    dispatch_scheduled_[data.contact_id] = false;

    if (contact_plan_ == nullptr) {
        return;
    }
    Contact* contact = contact_plan_->get_contact(data.contact_id);
    if (contact == nullptr || contact->plan_version != data.plan_version) {
        ++metrics_.stale_contact_events_ignored;
        stats_.note_stale_contact_event();

        std::ostringstream stream;
        stream << "BUNDLE_TX_START_STALE contact=" << data.contact_id
               << " time=" << event.timestamp;
        log_event(stream.str());
        logger_.record_event(make_event_record(
            EventType::BUNDLE_TX_START,
            event.timestamp,
            stream.str(),
            nullptr,
            -1,
            data.contact_id,
            data.plan_version));
        return;
    }

    NodeRuntime* sender_runtime = runtime_.find_node(contact->sending_node);
    if (sender_runtime == nullptr || sender_runtime->bpa == nullptr) {
        return;
    }

    std::shared_ptr<Bundle> bundle = data.bundle;
    if (bundle == nullptr) {
        bundle = sender_runtime->bpa->dequeue_next_for_contact(data.contact_id);
    }
    if (bundle == nullptr) {
        return;
    }

    if (is_terminal(bundle)) {
        schedule_contact_dispatch(data.contact_id, event.timestamp);
        return;
    }

    if (expired_at(*bundle, event.timestamp)) {
        scheduler_.schedule({event.timestamp, 0, 0, EventType::BUNDLE_EXPIRED, BundleExpiredData{bundle}});
        schedule_contact_dispatch(data.contact_id, event.timestamp);
        return;
    }

    if (contact->data_rate <= 0.0) {
        scheduler_.schedule({event.timestamp, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, contact->sending_node}});
        schedule_contact_dispatch(data.contact_id, event.timestamp);
        return;
    }

    const TimePoint tx_end_time = event.timestamp + bundle->evc() / contact->data_rate;
    if (tx_end_time > contact->end_time + kEpsilon) {
        scheduler_.schedule({event.timestamp, 0, 0, EventType::ROUTE_FAILED, RouteFailedData{bundle, contact->sending_node}});
        schedule_contact_dispatch(data.contact_id, event.timestamp);
        return;
    }

    contact_busy_[data.contact_id] = true;
    const TimePoint owlt = contact_plan_->get_owlt(contact->sending_node,
                                                   contact->receiving_node,
                                                   tx_end_time);
    in_flight_transmissions_[bundle->id] = {
        data.contact_id,
        contact->plan_version,
        contact->sending_node,
        contact->receiving_node,
        owlt};
    ++metrics_.tx_started;
    stats_.note_tx_start(*bundle, data.contact_id, event.timestamp);

    std::ostringstream stream;
    stream << "BUNDLE_TX_START bundle=" << bundle->id
           << " contact=" << data.contact_id
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::BUNDLE_TX_START,
        event.timestamp,
        stream.str(),
        bundle,
        contact->sending_node,
        data.contact_id,
        contact->plan_version));

    scheduler_.schedule({
        tx_end_time,
        0,
        0,
        EventType::BUNDLE_TX_END,
        BundleTxEndData{bundle, data.contact_id, contact->plan_version}});
}

void SimEngine::on_bundle_tx_end(const Event& event) {
    const auto& data = std::get<BundleTxEndData>(event.payload);
    contact_busy_[data.contact_id] = false;

    auto transmission_it = data.bundle == nullptr
        ? in_flight_transmissions_.end()
        : in_flight_transmissions_.find(data.bundle->id);
    const InFlightTransmission* transmission = nullptr;
    if (transmission_it != in_flight_transmissions_.end() &&
        transmission_it->second.contact_id == data.contact_id &&
        transmission_it->second.plan_version == data.plan_version) {
        transmission = &transmission_it->second;
    }

    if (data.bundle == nullptr || is_terminal(data.bundle)) {
        if (transmission_it != in_flight_transmissions_.end()) {
            in_flight_transmissions_.erase(transmission_it);
        }
        schedule_contact_dispatch(data.contact_id, event.timestamp);
        return;
    }

    Contact* contact = contact_plan_ == nullptr ? nullptr : contact_plan_->get_contact(data.contact_id);
    if (contact != nullptr && contact->plan_version != data.plan_version) {
        contact = nullptr;
    }
    if (contact == nullptr && transmission == nullptr) {
        ++metrics_.stale_contact_events_ignored;
        stats_.note_stale_contact_event();

        std::ostringstream stream;
        stream << "BUNDLE_TX_END_STALE contact=" << data.contact_id
               << " time=" << event.timestamp;
        log_event(stream.str());
        logger_.record_event(make_event_record(
            EventType::BUNDLE_TX_END,
            event.timestamp,
            stream.str(),
            data.bundle,
            -1,
            data.contact_id,
            data.plan_version));

        if (transmission_it != in_flight_transmissions_.end()) {
            in_flight_transmissions_.erase(transmission_it);
        }
        return;
    }

    NodeId sending_node = contact == nullptr ? transmission->sending_node : contact->sending_node;
    NodeId receiving_node = contact == nullptr ? transmission->receiving_node : contact->receiving_node;
    TimePoint owlt = contact == nullptr ? transmission->owlt
                                        : contact_plan_->get_owlt(contact->sending_node,
                                                                  contact->receiving_node,
                                                                  event.timestamp);
    if (transmission != nullptr) {
        sending_node = transmission->sending_node;
        receiving_node = transmission->receiving_node;
        owlt = transmission->owlt;
    }

    ++metrics_.tx_completed;
    stats_.note_tx_end(*data.bundle, data.contact_id, event.timestamp);

    std::ostringstream stream;
    stream << "BUNDLE_TX_END bundle=" << data.bundle->id
           << "contact=" << data.contact_id
           << "time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::BUNDLE_TX_END,
        event.timestamp,
        stream.str(),
        data.bundle,
        receiving_node,
        data.contact_id,
        data.plan_version));

    if (expired_at(*data.bundle, event.timestamp)) {
        scheduler_.schedule({event.timestamp, 0, 0, EventType::BUNDLE_EXPIRED, BundleExpiredData{data.bundle}});
        if (transmission_it != in_flight_transmissions_.end()) {
            in_flight_transmissions_.erase(transmission_it);
        }
        schedule_contact_dispatch(data.contact_id, event.timestamp);
        return;
    }

    data.bundle->previous_node = sending_node;
    scheduler_.schedule({
        event.timestamp + owlt + config_.owlt_margin,
        0,
        0,
        EventType::BUNDLE_ARRIVED,
        BundleArrivedData{data.bundle, receiving_node}});

    if (transmission_it != in_flight_transmissions_.end()) {
        in_flight_transmissions_.erase(transmission_it);
    }

    schedule_contact_dispatch(data.contact_id, event.timestamp);
}

void SimEngine::on_bundle_delivered(const Event& event) {
    const auto& data = std::get<BundleDeliveredData>(event.payload);
    if (data.bundle == nullptr || is_terminal(data.bundle)) {
        return;
    }

    bundle_states_[data.bundle->id] = BundleLifecycleState::DELIVERED;
    in_flight_transmissions_.erase(data.bundle->id);
    data.bundle->delivered_time = event.timestamp;
    data.bundle->mark_destination_delivered(data.destination);
    ++metrics_.bundles_delivered;
    const std::size_t delivered_attempts = metrics_.route_attempts.count(data.bundle->id) == 0
        ? 0u
        : metrics_.route_attempts.at(data.bundle->id);
    stats_.note_bundle_delivered(*data.bundle, data.destination, event.timestamp, delivered_attempts);

    auto redundancy_it = redundancy_families_.find(data.bundle->canonical_origin_id());
    if (redundancy_it != redundancy_families_.end() && !redundancy_it->second.first_hit_recorded) {
        redundancy_it->second.first_hit_recorded = true;
        stats_.note_redundant_first_hit();
    }

    Node& node = ensure_node(data.destination);
    NodeRuntime* runtime = runtime_.find_node(node.node_number);
    if (runtime != nullptr && runtime->bpa != nullptr) {
        runtime->bpa->deliver_local(data.bundle);
    }

    std::ostringstream stream;
    stream << "BUNDLE_DELIVERED bundle=" << data.bundle->id
           << " node=" << data.destination
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::BUNDLE_DELIVERED,
        event.timestamp,
        stream.str(),
        data.bundle,
        data.destination));
}

void SimEngine::on_bundle_expired(const Event& event) {
    const auto& data = std::get<BundleExpiredData>(event.payload);
    if (data.bundle == nullptr || is_terminal(data.bundle)) {
        return;
    }

    bundle_states_[data.bundle->id] = BundleLifecycleState::EXPIRED;
    in_flight_transmissions_.erase(data.bundle->id);
    ++metrics_.bundles_expired;
    const std::size_t expired_attempts = metrics_.route_attempts.count(data.bundle->id) == 0
        ? 0u
        : metrics_.route_attempts.at(data.bundle->id);
    stats_.note_bundle_expired(*data.bundle, event.timestamp, expired_attempts);

    std::ostringstream stream;
    stream << "BUNDLE_EXPIRED bundle=" << data.bundle->id
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::BUNDLE_EXPIRED,
        event.timestamp,
        stream.str(),
        data.bundle));
}

void SimEngine::on_duplicate_dropped(const Event& event) {
    const auto& data = std::get<DuplicateDroppedData>(event.payload);
    if (data.bundle == nullptr || is_terminal(data.bundle)) {
        return;
    }

    bundle_states_[data.bundle->id] = BundleLifecycleState::DUPLICATE_DROPPED;
    in_flight_transmissions_.erase(data.bundle->id);

    std::ostringstream stream;
    stream << "DUPLICATE_DROPPED bundle=" << data.bundle->id
           << " node=" << data.at_node
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::DUPLICATE_DROPPED,
        event.timestamp,
        stream.str(),
        data.bundle,
        data.at_node));
    stats_.note_duplicate_dropped(*data.bundle, data.at_node, event.timestamp);
}

void SimEngine::on_contact_start(const Event& event) {
    const auto& data = std::get<ContactStateData>(event.payload);
    if (!is_current_contact(data.contact_id, data.plan_version)) {
        ++metrics_.stale_contact_events_ignored;
        stats_.note_stale_contact_event();

        std::ostringstream stale_stream;
        stale_stream << "CONTACT_START_STALE contact=" << data.contact_id
                     << " time=" << event.timestamp;
        log_event(stale_stream.str());
        logger_.record_event(make_event_record(
            EventType::CONTACT_START,
            event.timestamp,
            stale_stream.str(),
            nullptr,
            -1,
            data.contact_id,
            data.plan_version));
        return;
    }

    ++metrics_.contact_start_events;
    stats_.note_contact_start(data.contact_id);

    std::ostringstream stream;
    stream << "CONTACT_START contact=" << data.contact_id
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::CONTACT_START,
        event.timestamp,
        stream.str(),
        nullptr,
        -1,
        data.contact_id,
        data.plan_version));
}

void SimEngine::on_contact_end(const Event& event) {
    const auto& data = std::get<ContactStateData>(event.payload);
    if (!is_current_contact(data.contact_id, data.plan_version)) {
        ++metrics_.stale_contact_events_ignored;
        stats_.note_stale_contact_event();

        std::ostringstream stale_stream;
        stale_stream << "CONTACT_END_STALE contact=" << data.contact_id
                     << " time=" << event.timestamp;
        log_event(stale_stream.str());
        logger_.record_event(make_event_record(
            EventType::CONTACT_END,
            event.timestamp,
            stale_stream.str(),
            nullptr,
            -1,
            data.contact_id,
            data.plan_version));
        return;
    }

    ++metrics_.contact_end_events;
    stats_.note_contact_end(data.contact_id);

    std::ostringstream stream;
    stream << "CONTACT_END contact=" << data.contact_id
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::CONTACT_END,
        event.timestamp,
        stream.str(),
        nullptr,
        -1,
        data.contact_id,
        data.plan_version));
}

void SimEngine::on_route_failed(const Event& event) {
    const auto& data = std::get<RouteFailedData>(event.payload);
    if (data.bundle == nullptr || is_terminal(data.bundle)) {
        return;
    }

    bundle_states_[data.bundle->id] = BundleLifecycleState::ROUTE_FAILED;
    in_flight_transmissions_.erase(data.bundle->id);
    ++metrics_.route_failures;
    const std::size_t failed_attempts = metrics_.route_attempts.count(data.bundle->id) == 0
        ? 0u
        : metrics_.route_attempts.at(data.bundle->id);
    stats_.note_route_failed(*data.bundle, data.at_node, event.timestamp, failed_attempts);

    std::ostringstream stream;
    stream << "ROUTE_FAILED bundle=" << data.bundle->id
           << " node=" << data.at_node
           << " time=" << event.timestamp;
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::ROUTE_FAILED,
        event.timestamp,
        stream.str(),
        data.bundle,
        data.at_node));
}

void SimEngine::on_plan_updated(const Event& event) {
    const auto& data = std::get<PlanUpdateData>(event.payload);
    ++metrics_.plan_update_events;

    std::ostringstream stream;
    stream << "PLAN_UPDATED time=" << event.timestamp
           << " removed=" << data.removed_contact_ids.size()
           << " replacement=" << (data.replacement_plan != nullptr ? 1 : 0);
    if (!data.label.empty()) {
        stream << " label=" << data.label;
    }
    log_event(stream.str());
    logger_.record_event(make_event_record(
        EventType::PLAN_UPDATED,
        event.timestamp,
        stream.str()));

    std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>> drained;
    if (data.replacement_plan != nullptr) {
        drained = drain_all_queues();
        dispatch_scheduled_.clear();

        if (contact_plan_ != nullptr) {
            const PlanVersion current_version = contact_plan_->version();
            while (data.replacement_plan->version() <= current_version) {
                data.replacement_plan->set_ranges(data.replacement_plan->ranges());
            }
        }

        set_contact_plan(data.replacement_plan);
        mark_plan_rebound();
        schedule_contact_events(event.timestamp);
    } else if (!data.removed_contact_ids.empty() && contact_plan_ != nullptr) {
        drained = drain_contact_queues(data.removed_contact_ids);

        std::vector<ContactId> removed_ids = data.removed_contact_ids;
        std::sort(removed_ids.begin(), removed_ids.end());
        removed_ids.erase(std::unique(removed_ids.begin(), removed_ids.end()), removed_ids.end());

        for (ContactId contact_id : removed_ids) {
            dispatch_scheduled_.erase(contact_id);
            contact_plan_->remove_contact(contact_id);
        }

        mark_plan_rebound();
    }

    logger_.record_plan_update(data, event.timestamp, drained.size());
    stats_.note_plan_update(data, event.timestamp, drained.size());

    reroute_drained_bundles(drained, event.timestamp);
    reschedule_queued_contacts(event.timestamp);
}