#include "simulation/stats.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {

std::string routing_action_name(RoutingAction action) {
    switch (action) {
    case RoutingAction::DELIVER_LOCAL:
        return "DELIVER_LOCAL";
    case RoutingAction::FORWARD_ONE:
        return "FORWARD_ONE";
    case RoutingAction::FORWARD_FLOOD:
        return "FORWARD_FLOOD";
    case RoutingAction::ROUTE_FAIL:
        return "ROUTE_FAIL";
    }
    return "ROUTE_FAIL";
}

std::vector<ContactId> route_contact_ids(const Route& route) {
    std::vector<ContactId> ids;
    ids.reserve(route.legs.size());
    for (const RouteLeg& leg : route.legs) {
        if (leg.contact != nullptr) {
            ids.push_back(leg.contact->contact_id);
        }
    }
    return ids;
}

std::string join_node_path(const std::vector<NodeId>& path) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < path.size(); ++index) {
        if (index > 0) {
            stream << "->";
        }
        stream << path[index];
    }
    return stream.str();
}

void ensure_parent_directory(const std::filesystem::path& filepath) {
    const std::filesystem::path parent = filepath.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

}  // namespace

SimStats::SimStats(TimePoint start_time,
                   TimePoint end_time,
                   std::uint32_t failure_seed,
                   std::string scenario_name) {
    set_metadata(start_time, end_time, failure_seed, std::move(scenario_name));
}

void SimStats::set_metadata(TimePoint start_time,
                            TimePoint end_time,
                            std::uint32_t failure_seed,
                            std::string scenario_name) {
    metadata_.start_time = start_time;
    metadata_.end_time = end_time;
    metadata_.failure_seed = failure_seed;
    metadata_.scenario_name = std::move(scenario_name);
}

void SimStats::bind_contact_plan(const ContactPlan& plan) {
    metadata_.contact_plan_version = std::max(metadata_.contact_plan_version, plan.version());
    for (const Contact& contact : plan.contacts()) {
        ContactStatsRecord& record = ensure_contact_record(contact);
        record.plan_version = contact.plan_version;
        record.sending_node = contact.sending_node;
        record.receiving_node = contact.receiving_node;
        record.start_time = contact.start_time;
        record.end_time = contact.end_time;
        record.data_rate = contact.data_rate;
        record.capacity = contact.volume();
    }
}

void SimStats::note_bundle_created(const Bundle& bundle) {
    ++summary_.bundles_created;
    ensure_bundle_record(bundle);
    refresh_summary();
}

void SimStats::note_bundle_forwarded(const Bundle& bundle) {
    ++summary_.bundles_forwarded;
    ensure_bundle_record(bundle);
    refresh_summary();
}

void SimStats::note_routing_trace(const Bundle& bundle,
                                  NodeId at_node,
                                  TimePoint time,
                                  std::size_t route_attempt,
                                  const RoutingTrace& trace) {
    ++summary_.routing_invocations;
    if (route_attempt > 1) {
        ++summary_.reroute_invocations;
    }

    BundleStatsRecord& bundle_record = ensure_bundle_record(bundle);
    bundle_record.route_attempts = std::max(bundle_record.route_attempts, route_attempt);

    RoutingStatsRecord record;
    record.bundle_id = bundle.id;
    record.at_node = at_node;
    record.time = time;
    record.route_attempt = route_attempt;
    record.action = trace.decision.action;
    record.failure_reason = trace.decision.failure_reason;
    record.reroute_reason = trace.reroute_reason;
    record.reroute_count = trace.reroute_count;
    record.reroute_excluded_neighbors = trace.reroute_excluded_neighbors;
    record.redundancy_considered = trace.redundancy_considered;
    if (trace.reroute_reason == "reactive_anti_loop") {
        ++summary_.reactive_anti_loop_trigger_count;
    }
    if (trace.decision.primary.has_value()) {
        record.primary_contact_ids = route_contact_ids(trace.decision.primary->route);
    }
    for (const CandidateRoute& flood_route : trace.decision.flood_routes) {
        record.flood_route_contact_ids.push_back(route_contact_ids(flood_route.route));
    }
    for (std::size_t index = 0; index < trace.attempts.size(); ++index) {
        const RoutingAttemptTrace& attempt_trace = trace.attempts[index];
        RoutingAttemptStats attempt;
        attempt.attempt_index = index + 1;
        attempt.phase1_route_count = attempt_trace.phase1_routes.size();
        attempt.phase2_candidate_count = attempt_trace.phase2_candidates.size();
        attempt.need_recompute = attempt_trace.need_recompute;
        for (const CandidateRoute& candidate : attempt_trace.phase2_candidates) {
            if (candidate.valid) {
                ++attempt.phase2_valid_candidate_count;
            } else if (!candidate.reject_reason.empty()) {
                attempt.reject_reasons.push_back(candidate.reject_reason);
            }
        }
        record.attempts.push_back(std::move(attempt));
    }
    routing_records_.push_back(std::move(record));
    refresh_summary();
}

void SimStats::note_commit(const Bundle& bundle,
                           const Route& route,
                           TimePoint,
                           std::size_t committed_leg_count) {
    const std::size_t leg_count = std::min(committed_leg_count, route.legs.size());
    for (std::size_t index = 0; index < leg_count; ++index) {
        const RouteLeg& leg = route.legs[index];
        if (leg.contact == nullptr) {
            continue;
        }
        ContactStatsRecord& record = ensure_contact_record(*leg.contact);
        record.committed_volume += bundle.evc();
        ++record.commit_count;
    }
    refresh_summary();
}

void SimStats::note_tx_start(const Bundle& bundle,
                             ContactId contact_id,
                             TimePoint) {
    ++summary_.tx_started;
    ensure_bundle_record(bundle);
    ++ensure_contact_record(contact_id).tx_started;
    refresh_summary();
}

void SimStats::note_tx_end(const Bundle& bundle,
                           ContactId contact_id,
                           TimePoint) {
    ++summary_.tx_completed;
    ensure_bundle_record(bundle);
    ++ensure_contact_record(contact_id).tx_completed;
    refresh_summary();
}

void SimStats::note_bundle_delivered(const Bundle& bundle,
                                     NodeId,
                                     TimePoint time,
                                     std::size_t route_attempts) {
    ++summary_.bundles_delivered;
    BundleStatsRecord& record = ensure_bundle_record(bundle);
    record.final_state = "DELIVERED";
    record.delivered_time = time;
    record.end_to_end_delay = time - bundle.creation_time;
    record.route_attempts = std::max(record.route_attempts, route_attempts);
    record.route_path = bundle.route_path;
    record.hop_count = bundle.route_path.empty() ? 0u : bundle.route_path.size() - 1u;
    refresh_summary();
}

void SimStats::note_bundle_expired(const Bundle& bundle,
                                   TimePoint,
                                   std::size_t route_attempts) {
    ++summary_.bundles_expired;
    BundleStatsRecord& record = ensure_bundle_record(bundle);
    record.final_state = "EXPIRED";
    record.route_attempts = std::max(record.route_attempts, route_attempts);
    record.route_path = bundle.route_path;
    record.hop_count = bundle.route_path.empty() ? 0u : bundle.route_path.size() - 1u;
    refresh_summary();
}

void SimStats::note_route_failed(const Bundle& bundle,
                                 NodeId,
                                 TimePoint,
                                 std::size_t route_attempts) {
    ++summary_.route_failures;
    BundleStatsRecord& record = ensure_bundle_record(bundle);
    record.final_state = "ROUTE_FAILED";
    record.route_attempts = std::max(record.route_attempts, route_attempts);
    record.route_path = bundle.route_path;
    record.hop_count = bundle.route_path.empty() ? 0u : bundle.route_path.size() - 1u;
    refresh_summary();
}

void SimStats::note_contact_start(ContactId) {
    ++summary_.contact_start_events;
    refresh_summary();
}

void SimStats::note_contact_end(ContactId) {
    ++summary_.contact_end_events;
    refresh_summary();
}

void SimStats::note_plan_update(const PlanUpdateData& update,
                                TimePoint time,
                                std::size_t drained_bundle_count) {
    ++summary_.plan_update_events;
    plan_updates_.push_back({time,
                             update.removed_contact_ids,
                             update.replacement_plan != nullptr,
                             update.label,
                             drained_bundle_count});
    refresh_summary();
}

void SimStats::note_queued_bundle_replan() {
    ++summary_.queued_bundle_replans;
    refresh_summary();
}

void SimStats::note_local_repair() {
    ++summary_.local_repair_count;
    refresh_summary();
}

void SimStats::note_redundancy_trigger(std::size_t extra_copy_count) {
    ++summary_.redundancy_trigger_count;
    redundancy_extra_copy_count_ += extra_copy_count;
    refresh_summary();
}

void SimStats::note_redundant_first_hit() {
    ++summary_.redundant_first_hit_count;
    refresh_summary();
}

void SimStats::note_stale_contact_event() {
    ++summary_.stale_contact_events_ignored;
    refresh_summary();
}

void SimStats::note_duplicate_dropped(const Bundle& bundle,
                                      NodeId,
                                      TimePoint) {
    ++summary_.duplicate_replica_discard_count;
    BundleStatsRecord& record = ensure_bundle_record(bundle);
    record.final_state = "DUPLICATE_DROPPED";
    refresh_summary();
}

std::vector<BundleStatsRecord> SimStats::bundle_records() const {
    std::vector<BundleStatsRecord> result;
    result.reserve(bundles_.size());
    for (const auto& entry : bundles_) {
        result.push_back(entry.second);
    }
    return result;
}

std::vector<ContactStatsRecord> SimStats::contact_records() const {
    std::vector<ContactStatsRecord> result;
    result.reserve(contacts_.size());
    for (const auto& entry : contacts_) {
        ContactStatsRecord record = entry.second;
        record.utilization_ratio = record.capacity <= 0.0 ? 0.0
                                                          : record.committed_volume / record.capacity;
        result.push_back(std::move(record));
    }
    return result;
}

nlohmann::json SimStats::to_json() const {
    nlohmann::json root;
    root["metadata"] = {
        {"scenario_name", metadata_.scenario_name},
        {"start_time", metadata_.start_time},
        {"end_time", metadata_.end_time},
        {"failure_seed", metadata_.failure_seed},
        {"contact_plan_version", metadata_.contact_plan_version},
    };

    root["summary"] = {
        {"bundles_created", summary_.bundles_created},
        {"bundles_forwarded", summary_.bundles_forwarded},
        {"bundles_delivered", summary_.bundles_delivered},
        {"bundles_expired", summary_.bundles_expired},
        {"route_failures", summary_.route_failures},
        {"tx_started", summary_.tx_started},
        {"tx_completed", summary_.tx_completed},
        {"contact_start_events", summary_.contact_start_events},
        {"contact_end_events", summary_.contact_end_events},
        {"routing_invocations", summary_.routing_invocations},
        {"reroute_invocations", summary_.reroute_invocations},
        {"plan_update_events", summary_.plan_update_events},
        {"queued_bundle_replans", summary_.queued_bundle_replans},
        {"stale_contact_events_ignored", summary_.stale_contact_events_ignored},
        {"reactive_anti_loop_trigger_count", summary_.reactive_anti_loop_trigger_count},
        {"local_repair_count", summary_.local_repair_count},
        {"redundancy_trigger_count", summary_.redundancy_trigger_count},
        {"redundant_first_hit_count", summary_.redundant_first_hit_count},
        {"duplicate_replica_discard_count", summary_.duplicate_replica_discard_count},
        {"redundancy_benefit_cost_ratio", summary_.redundancy_benefit_cost_ratio},
        {"delivery_rate", summary_.delivery_rate},
        {"average_delivery_latency", summary_.average_delivery_latency},
        {"max_delivery_latency", summary_.max_delivery_latency},
        {"average_hop_count", summary_.average_hop_count},
    };

    root["bundles"] = nlohmann::json::array();
    for (const BundleStatsRecord& record : bundle_records()) {
        root["bundles"].push_back({
            {"bundle_id", record.bundle_id},
            {"source_node", record.source_node},
            {"destination_node", record.destination_node},
            {"final_state", record.final_state},
            {"creation_time", record.creation_time},
            {"expiration_time", record.expiration_time},
            {"delivered_time", record.delivered_time},
            {"end_to_end_delay", record.end_to_end_delay},
            {"hop_count", record.hop_count},
            {"route_attempts", record.route_attempts},
            {"is_critical", record.is_critical},
            {"is_multicast", record.is_multicast},
            {"route_path", record.route_path},
        });
    }

    root["contacts"] = nlohmann::json::array();
    for (const ContactStatsRecord& record : contact_records()) {
        root["contacts"].push_back({
            {"contact_id", record.contact_id},
            {"plan_version", record.plan_version},
            {"sending_node", record.sending_node},
            {"receiving_node", record.receiving_node},
            {"start_time", record.start_time},
            {"end_time", record.end_time},
            {"data_rate", record.data_rate},
            {"capacity", record.capacity},
            {"committed_volume", record.committed_volume},
            {"commit_count", record.commit_count},
            {"tx_started", record.tx_started},
            {"tx_completed", record.tx_completed},
            {"utilization_ratio", record.utilization_ratio},
        });
    }

    root["routing"] = nlohmann::json::array();
    for (const RoutingStatsRecord& record : routing_records_) {
        nlohmann::json attempts = nlohmann::json::array();
        for (const RoutingAttemptStats& attempt : record.attempts) {
            attempts.push_back({
                {"attempt_index", attempt.attempt_index},
                {"phase1_route_count", attempt.phase1_route_count},
                {"phase2_candidate_count", attempt.phase2_candidate_count},
                {"phase2_valid_candidate_count", attempt.phase2_valid_candidate_count},
                {"need_recompute", attempt.need_recompute},
                {"reject_reasons", attempt.reject_reasons},
            });
        }

        root["routing"].push_back({
            {"bundle_id", record.bundle_id},
            {"at_node", record.at_node},
            {"time", record.time},
            {"route_attempt", record.route_attempt},
            {"action", routing_action_name(record.action)},
            {"failure_reason", record.failure_reason},
            {"reroute_reason", record.reroute_reason},
            {"reroute_count", record.reroute_count},
            {"reroute_excluded_neighbors", record.reroute_excluded_neighbors},
            {"primary_contact_ids", record.primary_contact_ids},
            {"flood_route_contact_ids", record.flood_route_contact_ids},
            {"attempts", attempts},
            {"local_repair_triggered", record.local_repair_triggered},
            {"redundancy_considered", record.redundancy_considered},
        });
    }

    root["plan_updates"] = nlohmann::json::array();
    for (const PlanUpdateStatsRecord& record : plan_updates_) {
        root["plan_updates"].push_back({
            {"time", record.time},
            {"removed_contact_ids", record.removed_contact_ids},
            {"has_replacement_plan", record.has_replacement_plan},
            {"label", record.label},
            {"drained_bundle_count", record.drained_bundle_count},
        });
    }

    return root;
}

void SimStats::write_json(const std::filesystem::path& filepath) const {
    ensure_parent_directory(filepath);
    std::ofstream stream(filepath);
    stream << to_json().dump(2);
}

void SimStats::write_bundle_summary_csv(const std::filesystem::path& filepath) const {
    ensure_parent_directory(filepath);
    std::ofstream stream(filepath);
    stream << "bundle_id,source_node,destination_node,final_state,creation_time,expiration_time,delivered_time,end_to_end_delay,hop_count,route_attempts,is_critical,is_multicast,route_path\n";
    for (const BundleStatsRecord& record : bundle_records()) {
        stream << record.bundle_id << ','
               << record.source_node << ','
               << record.destination_node << ','
               << record.final_state << ','
               << record.creation_time << ','
               << record.expiration_time << ','
               << record.delivered_time << ','
               << record.end_to_end_delay << ','
               << record.hop_count << ','
               << record.route_attempts << ','
               << (record.is_critical ? 1 : 0) << ','
               << (record.is_multicast ? 1 : 0) << ','
               << '"' << join_node_path(record.route_path) << '"' << '\n';
    }
}

void SimStats::write_contact_utilization_csv(const std::filesystem::path& filepath) const {
    ensure_parent_directory(filepath);
    std::ofstream stream(filepath);
    stream << "contact_id,plan_version,sending_node,receiving_node,start_time,end_time,data_rate,capacity,committed_volume,commit_count,tx_started,tx_completed,utilization_ratio\n";
    for (const ContactStatsRecord& record : contact_records()) {
        stream << record.contact_id << ','
               << record.plan_version << ','
               << record.sending_node << ','
               << record.receiving_node << ','
               << record.start_time << ','
               << record.end_time << ','
               << record.data_rate << ','
               << record.capacity << ','
               << record.committed_volume << ','
               << record.commit_count << ','
               << record.tx_started << ','
               << record.tx_completed << ','
               << record.utilization_ratio << '\n';
    }
}

BundleStatsRecord& SimStats::ensure_bundle_record(const Bundle& bundle) {
    auto [it, inserted] = bundles_.emplace(bundle.id, BundleStatsRecord{});
    BundleStatsRecord& record = it->second;
    if (inserted) {
        record.bundle_id = bundle.id;
        record.source_node = bundle.source_node;
        record.destination_node = bundle.destination_eid;
        record.creation_time = bundle.creation_time;
        record.expiration_time = bundle.expiration_time();
        record.is_critical = bundle.is_critical;
        record.is_multicast = bundle.is_multicast;
    }
    return record;
}

ContactStatsRecord& SimStats::ensure_contact_record(ContactId contact_id) {
    auto [it, inserted] = contacts_.emplace(contact_id, ContactStatsRecord{});
    if (inserted) {
        it->second.contact_id = contact_id;
    }
    return it->second;
}

ContactStatsRecord& SimStats::ensure_contact_record(const Contact& contact) {
    ContactStatsRecord& record = ensure_contact_record(contact.contact_id);
    record.plan_version = contact.plan_version;
    record.sending_node = contact.sending_node;
    record.receiving_node = contact.receiving_node;
    record.start_time = contact.start_time;
    record.end_time = contact.end_time;
    record.data_rate = contact.data_rate;
    record.capacity = contact.volume();
    return record;
}

void SimStats::refresh_summary() {
    summary_.delivery_rate = summary_.bundles_created == 0
        ? 0.0
        : static_cast<double>(summary_.bundles_delivered) /
            static_cast<double>(summary_.bundles_created);

    double total_latency = 0.0;
    double max_latency = 0.0;
    double total_hops = 0.0;
    std::size_t delivered_count = 0;
    for (const auto& entry : bundles_) {
        const BundleStatsRecord& record = entry.second;
        if (record.final_state != "DELIVERED") {
            continue;
        }
        ++delivered_count;
        total_latency += record.end_to_end_delay;
        max_latency = std::max(max_latency, record.end_to_end_delay);
        total_hops += static_cast<double>(record.hop_count);
    }

    summary_.average_delivery_latency = delivered_count == 0
        ? 0.0
        : total_latency / static_cast<double>(delivered_count);
    summary_.max_delivery_latency = max_latency;
    summary_.average_hop_count = delivered_count == 0
        ? 0.0
        : total_hops / static_cast<double>(delivered_count);
    summary_.redundancy_benefit_cost_ratio = redundancy_extra_copy_count_ == 0
        ? 0.0
        : static_cast<double>(summary_.redundant_first_hit_count) /
            static_cast<double>(redundancy_extra_copy_count_);
}