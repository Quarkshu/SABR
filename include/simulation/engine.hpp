#pragma once

#include "algorithms/redundancy.hpp"
#include "algorithms/multicast.hpp"
#include "io/logger.hpp"
#include "simulation/failure_injector.hpp"
#include "simulation/runtime_context.hpp"
#include "simulation/scheduler.hpp"
#include "simulation/stats.hpp"
#include "simulation/traffic.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class BundleLifecycleState {
    UNKNOWN,
    ACTIVE,
    DELIVERED,
    EXPIRED,
    DUPLICATE_DROPPED,
    ROUTE_FAILED,
};

struct EngineConfig {
    TimePoint start_time = 0.0;
    TimePoint end_time = 3600.0;
    TimePoint owlt_margin = 0.0;
    RedundancyConfig redundancy{};
    Phase1::Config phase1_config{};
    Phase2::Config phase2_config{};
    int recompute_budget = 1;
    std::uint32_t failure_seed = 0;
    std::vector<FailureRule> failure_rules;
};

struct EngineMetrics {
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
    std::unordered_map<BundleId, std::size_t> route_attempts;
};

class SimEngine {
public:
    explicit SimEngine(EngineConfig config = {});
    SimEngine(std::shared_ptr<ContactPlan> contact_plan,
              std::vector<TrafficPattern> traffic_patterns,
              EngineConfig config = {});

    void set_contact_plan(std::shared_ptr<ContactPlan> contact_plan);
    void set_traffic_patterns(std::vector<TrafficPattern> traffic_patterns);
    void set_multicast_groups(std::vector<MulticastGroup> groups);
    void set_multicast_registry(const MulticastGroupRegistry& registry);
    void set_scenario_name(std::string scenario_name);

    Node& ensure_node(NodeId node_id);

    void initialize();
    void run();

    RuntimeContext& runtime() noexcept { return runtime_; }
    const RuntimeContext& runtime() const noexcept { return runtime_; }
    const EngineMetrics& metrics() const noexcept { return metrics_; }
    const Logger& logger() const noexcept { return logger_; }
    const SimStats& stats() const noexcept { return stats_; }
    const std::vector<std::string>& event_log() const noexcept { return event_log_; }
    const DiscreteEventScheduler& scheduler() const noexcept { return scheduler_; }
    const MulticastGroupRegistry& multicast_registry() const noexcept { return multicast_registry_; }

    std::shared_ptr<Bundle> find_bundle(BundleId bundle_id) const;
    BundleLifecycleState bundle_state(BundleId bundle_id) const;
    std::vector<BundleId> delivered_bundle_ids() const;

private:
    void register_handlers();
    void schedule_contact_events(TimePoint from_time);
    void schedule_plan_updates();
    void schedule_traffic();
    void process_bundle(const std::shared_ptr<Bundle>& bundle,
                        NodeId at_node,
                        TimePoint now);
    RoutingContext make_routing_context(Node& node,
                                        const Bundle& bundle,
                                        TimePoint now) const;
    Phase2::ValidationContext build_validation_context() const;
    RoutingTrace route_unicast_with_reactive_guard(Node& node,
                                                   const Bundle& bundle,
                                                   NodeId at_node,
                                                   TimePoint now);
    bool route_revisits_visited_node(const Route& route,
                                     const Bundle& bundle,
                                     NodeId at_node) const;
    std::set<NodeId> collect_reactive_loop_neighbors(const RoutingDecision& decision,
                                                     const Bundle& bundle,
                                                     NodeId at_node) const;
    bool initialize_multicast_bundle(const std::shared_ptr<Bundle>& bundle,
                                     NodeId source_node,
                                     const std::string& multicast_group_id,
                                     TimePoint now);
    void process_multicast_bundle(const std::shared_ptr<Bundle>& bundle,
                                  NodeId at_node,
                                  TimePoint now);
    bool is_multicast_contact_usable(ContactId contact_id,
                                     PlanVersion encoded_plan_version,
                                     TimePoint now) const;
    bool plan_multicast_repair_dispatches(const std::shared_ptr<Bundle>& bundle,
                                          NodeId at_node,
                                          const std::set<NodeId>& affected_destinations,
                                          TimePoint now,
                                          bool reuse_original_bundle,
                                          BundleId* next_bundle_id,
                                          std::vector<MulticastDispatch>* repaired_dispatches);
    void forward_along_route(const std::shared_ptr<Bundle>& bundle,
                             const Route& route,
                             TimePoint now);
    void forward_unicast_with_redundancy(const std::shared_ptr<Bundle>& bundle,
                                         const CandidateRoute& primary,
                                         const std::vector<CandidateRoute>& backup_routes,
                                         TimePoint now);
    void forward_to_contact(const std::shared_ptr<Bundle>& bundle,
                            ContactId contact_id,
                            TimePoint now);
    bool should_apply_multicast_trunk_redundancy(const Bundle& bundle,
                                                 NodeId at_node,
                                                 const MulticastDispatch& dispatch) const;
    bool plan_multicast_backup_dispatch(const std::shared_ptr<Bundle>& bundle,
                                        NodeId at_node,
                                        const MulticastDispatch& primary_dispatch,
                                        TimePoint now,
                                        BundleId* next_bundle_id,
                                        MulticastDispatch* backup_dispatch);
    void schedule_contact_dispatch(ContactId contact_id,
                                   TimePoint now);
    void commit_first_contact(const Bundle& bundle,
                              const Route& route);
    std::size_t note_route_attempt(const Bundle& bundle);
    bool is_terminal(const std::shared_ptr<Bundle>& bundle) const;
    void log_event(const std::string& message);
    bool is_current_contact(ContactId contact_id,
                            PlanVersion plan_version) const;
    void mark_plan_rebound();
    void reschedule_queued_contacts(TimePoint now);
    std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>> drain_contact_queues(
        const std::vector<ContactId>& contact_ids);
    std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>> drain_all_queues();
    void reroute_drained_bundles(
        const std::vector<std::pair<NodeId, std::shared_ptr<Bundle>>>& drained,
        TimePoint now);
    void note_redundancy_family(const Bundle& bundle,
                                std::size_t extra_copy_count);

    void on_bundle_created(const Event& event);
    void on_bundle_arrived(const Event& event);
    void on_bundle_tx_start(const Event& event);
    void on_bundle_tx_end(const Event& event);
    void on_bundle_delivered(const Event& event);
    void on_bundle_expired(const Event& event);
    void on_duplicate_dropped(const Event& event);
    void on_contact_start(const Event& event);
    void on_contact_end(const Event& event);
    void on_route_failed(const Event& event);
    void on_plan_updated(const Event& event);

private:
    struct InFlightTransmission {
        ContactId contact_id = 0;
        PlanVersion plan_version = 0;
        NodeId sending_node = 0;
        NodeId receiving_node = 0;
        TimePoint owlt = 0.0;
    };

    struct RedundancyFamilyState {
        std::size_t extra_copy_count = 0;
        bool first_hit_recorded = false;
    };

    EngineConfig config_;
    DiscreteEventScheduler scheduler_;
    RuntimeContext runtime_;
    std::shared_ptr<ContactPlan> contact_plan_;
    TrafficGenerator traffic_generator_;
    FailureInjector failure_injector_;
    CGRRouter cgr_router_;
    MulticastPlanner multicast_planner_;
    MulticastGroupRegistry multicast_registry_;
    std::unordered_map<NodeId, std::unique_ptr<Node>> nodes_;
    std::unordered_map<BundleId, std::shared_ptr<Bundle>> bundles_;
    std::unordered_map<BundleId, BundleLifecycleState> bundle_states_;
    std::unordered_map<BundleId, InFlightTransmission> in_flight_transmissions_;
    std::unordered_map<BundleId, RedundancyFamilyState> redundancy_families_;
    std::unordered_map<ContactId, bool> contact_busy_;
    std::unordered_map<ContactId, bool> dispatch_scheduled_;
    BundleId next_bundle_id_ = 1;
    EngineMetrics metrics_{};
    Logger logger_{};
    SimStats stats_{};
    std::vector<std::string> event_log_;
    bool initialized_ = false;
};

using SimulationEngine = SimEngine;