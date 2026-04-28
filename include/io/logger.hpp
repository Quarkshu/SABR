#pragma once

#include "algorithms/cgr.hpp"
#include "simulation/event.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct RouteLegLog {
    ContactId contact_id = 0;
    NodeId sending_node = -1;
    NodeId receiving_node = -1;
    TimePoint earliest_transmission_time = 0.0;
    TimePoint earliest_arrival_time = 0.0;
    TimePoint last_byte_transmission_time = 0.0;
    TimePoint last_byte_arrival_time = 0.0;
    Volume evl = 0.0;
};

struct RouteLog {
    bool local_delivery = false;
    NodeId local_delivery_node = -1;
    TimePoint best_case_delivery_time = 0.0;
    TimePoint termination_time = 0.0;
    NodeId entry_node = -1;
    TimePoint eto = 0.0;
    TimePoint pbat = 0.0;
    Volume rvl = 0.0;
    bool possibly_looping = false;
    std::vector<RouteLegLog> legs;
};

struct CandidateRouteLog {
    bool valid = false;
    std::string reject_reason;
    RouteLog route;
};

struct RoutingAttemptLog {
    std::size_t attempt_index = 0;
    bool need_recompute = false;
    std::vector<RouteLog> phase1_routes;
    std::vector<CandidateRouteLog> phase2_candidates;
};

struct EventRecord {
    EventType type = EventType::BUNDLE_CREATED;
    TimePoint time = 0.0;
    BundleId bundle_id = 0;
    bool has_bundle = false;
    NodeId node_id = -1;
    ContactId contact_id = 0;
    PlanVersion plan_version = 0;
    std::string message;
};

struct RoutingRecord {
    BundleId bundle_id = 0;
    NodeId at_node = -1;
    TimePoint time = 0.0;
    std::size_t route_attempt = 0;
    RoutingAction action = RoutingAction::ROUTE_FAIL;
    std::string failure_reason;
    std::string reroute_reason;
    std::size_t reroute_count = 0;
    std::vector<NodeId> reroute_excluded_neighbors;
    std::optional<RouteLog> primary;
    std::vector<RouteLog> flood_routes;
    std::vector<RoutingAttemptLog> attempts;
    bool local_repair_triggered = false;
    bool redundancy_considered = false;
};

struct CommitRecord {
    BundleId bundle_id = 0;
    TimePoint time = 0.0;
    Priority priority = Priority::BULK;
    Volume evc = 0.0;
    std::vector<ContactId> selected_contact_ids;
    std::vector<ContactId> committed_contact_ids;
};

struct PlanUpdateRecord {
    TimePoint time = 0.0;
    std::vector<ContactId> removed_contact_ids;
    bool has_replacement_plan = false;
    std::string label;
    std::size_t drained_bundle_count = 0;
};

class Logger {
public:
    void record_event(const EventRecord& record);
    void record_routing(const Bundle& bundle,
                        NodeId at_node,
                        TimePoint time,
                        std::size_t route_attempt,
                        const RoutingTrace& trace);
    void record_commit(const Bundle& bundle,
                       const Route& route,
                       TimePoint time,
                       std::size_t committed_leg_count = 1);
    void record_plan_update(const PlanUpdateData& update,
                            TimePoint time,
                            std::size_t drained_bundle_count);

    const std::vector<EventRecord>& events() const noexcept { return events_; }
    const std::vector<RoutingRecord>& routing_records() const noexcept { return routing_records_; }
    const std::vector<CommitRecord>& commit_records() const noexcept { return commit_records_; }
    const std::vector<PlanUpdateRecord>& plan_updates() const noexcept { return plan_updates_; }

    void clear();

private:
    std::vector<EventRecord> events_;
    std::vector<RoutingRecord> routing_records_;
    std::vector<CommitRecord> commit_records_;
    std::vector<PlanUpdateRecord> plan_updates_;
};