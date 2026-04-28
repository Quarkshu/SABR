#pragma once

#include "algorithms/cgr.hpp"
#include "json.hpp"
#include "simulation/event.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct BundleStatsRecord {
    BundleId bundle_id = 0;
    NodeId source_node = 0;
    NodeId destination_node = -1;
    std::string final_state = "ACTIVE";
    TimePoint creation_time = 0.0;
    TimePoint expiration_time = 0.0;
    TimePoint delivered_time = -1.0;
    TimePoint end_to_end_delay = -1.0;
    std::size_t hop_count = 0;
    std::size_t route_attempts = 0;
    bool is_critical = false;
    bool is_multicast = false;
    std::vector<NodeId> route_path;
};

struct ContactStatsRecord {
    ContactId contact_id = 0;
    PlanVersion plan_version = 0;
    NodeId sending_node = -1;
    NodeId receiving_node = -1;
    TimePoint start_time = 0.0;
    TimePoint end_time = 0.0;
    Volume data_rate = 0.0;
    Volume capacity = 0.0;
    Volume committed_volume = 0.0;
    std::size_t commit_count = 0;
    std::size_t tx_started = 0;
    std::size_t tx_completed = 0;
    double utilization_ratio = 0.0;
};

struct RoutingAttemptStats {
    std::size_t attempt_index = 0;
    std::size_t phase1_route_count = 0;
    std::size_t phase2_candidate_count = 0;
    std::size_t phase2_valid_candidate_count = 0;
    bool need_recompute = false;
    std::vector<std::string> reject_reasons;
};

struct RoutingStatsRecord {
    BundleId bundle_id = 0;
    NodeId at_node = -1;
    TimePoint time = 0.0;
    std::size_t route_attempt = 0;
    RoutingAction action = RoutingAction::ROUTE_FAIL;
    std::string failure_reason;
    std::string reroute_reason;
    std::size_t reroute_count = 0;
    std::vector<NodeId> reroute_excluded_neighbors;
    std::vector<ContactId> primary_contact_ids;
    std::vector<std::vector<ContactId>> flood_route_contact_ids;
    std::vector<RoutingAttemptStats> attempts;
    bool local_repair_triggered = false;
    bool redundancy_considered = false;
};

struct PlanUpdateStatsRecord {
    TimePoint time = 0.0;
    std::vector<ContactId> removed_contact_ids;
    bool has_replacement_plan = false;
    std::string label;
    std::size_t drained_bundle_count = 0;
};

struct StatsSummary {
    std::size_t bundles_created = 0;
    std::size_t bundles_forwarded = 0;
    std::size_t bundles_delivered = 0;
    std::size_t bundles_expired = 0;
    std::size_t route_failures = 0;
    std::size_t tx_started = 0;
    std::size_t tx_completed = 0;
    std::size_t contact_start_events = 0;
    std::size_t contact_end_events = 0;
    std::size_t routing_invocations = 0;
    std::size_t reroute_invocations = 0;
    std::size_t plan_update_events = 0;
    std::size_t queued_bundle_replans = 0;
    std::size_t stale_contact_events_ignored = 0;
    std::size_t reactive_anti_loop_trigger_count = 0;
    std::size_t local_repair_count = 0;
    std::size_t redundancy_trigger_count = 0;
    std::size_t redundant_first_hit_count = 0;
    std::size_t duplicate_replica_discard_count = 0;
    double redundancy_benefit_cost_ratio = 0.0;
    double delivery_rate = 0.0;
    double average_delivery_latency = 0.0;
    double max_delivery_latency = 0.0;
    double average_hop_count = 0.0;
};

class SimStats {
public:
    SimStats(TimePoint start_time = 0.0,
             TimePoint end_time = 0.0,
             std::uint32_t failure_seed = 0,
             std::string scenario_name = {});

    void set_metadata(TimePoint start_time,
                      TimePoint end_time,
                      std::uint32_t failure_seed,
                      std::string scenario_name = {});
    void bind_contact_plan(const ContactPlan& plan);

    void note_bundle_created(const Bundle& bundle);
    void note_bundle_forwarded(const Bundle& bundle);
    void note_routing_trace(const Bundle& bundle,
                            NodeId at_node,
                            TimePoint time,
                            std::size_t route_attempt,
                            const RoutingTrace& trace);
    void note_commit(const Bundle& bundle,
                     const Route& route,
                     TimePoint time,
                     std::size_t committed_leg_count = 1);
    void note_tx_start(const Bundle& bundle,
                       ContactId contact_id,
                       TimePoint time);
    void note_tx_end(const Bundle& bundle,
                     ContactId contact_id,
                     TimePoint time);
    void note_bundle_delivered(const Bundle& bundle,
                               NodeId destination,
                               TimePoint time,
                               std::size_t route_attempts);
    void note_bundle_expired(const Bundle& bundle,
                             TimePoint time,
                             std::size_t route_attempts);
    void note_route_failed(const Bundle& bundle,
                           NodeId at_node,
                           TimePoint time,
                           std::size_t route_attempts);
    void note_contact_start(ContactId contact_id);
    void note_contact_end(ContactId contact_id);
    void note_plan_update(const PlanUpdateData& update,
                          TimePoint time,
                          std::size_t drained_bundle_count);
    void note_queued_bundle_replan();
    void note_local_repair();
    void note_redundancy_trigger(std::size_t extra_copy_count);
    void note_redundant_first_hit();
    void note_stale_contact_event();
    void note_duplicate_dropped(const Bundle& bundle,
                                NodeId at_node,
                                TimePoint time);

    const StatsSummary& summary() const noexcept { return summary_; }
    std::vector<BundleStatsRecord> bundle_records() const;
    std::vector<ContactStatsRecord> contact_records() const;
    const std::vector<RoutingStatsRecord>& routing_records() const noexcept { return routing_records_; }
    const std::vector<PlanUpdateStatsRecord>& plan_updates() const noexcept { return plan_updates_; }

    nlohmann::json to_json() const;
    void write_json(const std::filesystem::path& filepath) const;
    void write_bundle_summary_csv(const std::filesystem::path& filepath) const;
    void write_contact_utilization_csv(const std::filesystem::path& filepath) const;

private:
    struct Metadata {
        TimePoint start_time = 0.0;
        TimePoint end_time = 0.0;
        std::uint32_t failure_seed = 0;
        std::string scenario_name;
        PlanVersion contact_plan_version = 0;
    };

    BundleStatsRecord& ensure_bundle_record(const Bundle& bundle);
    ContactStatsRecord& ensure_contact_record(ContactId contact_id);
    ContactStatsRecord& ensure_contact_record(const Contact& contact);
    void refresh_summary();

private:
    Metadata metadata_;
    StatsSummary summary_{};
    std::map<BundleId, BundleStatsRecord> bundles_;
    std::map<ContactId, ContactStatsRecord> contacts_;
    std::vector<RoutingStatsRecord> routing_records_;
    std::vector<PlanUpdateStatsRecord> plan_updates_;
    std::size_t redundancy_extra_copy_count_ = 0;
};