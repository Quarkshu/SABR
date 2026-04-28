#include <gtest/gtest.h>

#include "simulation/engine.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

Contact make_contact(ContactId id,
                     TimePoint start_time,
                     TimePoint end_time,
                     NodeId from,
                     NodeId to,
                     Volume data_rate) {
    return Contact{id, start_time, end_time, from, to, data_rate, 0, {}};
}

RangeInterval make_range(TimePoint start_time,
                         TimePoint end_time,
                         NodeId node_a,
                         NodeId node_b,
                         TimePoint distance_light_seconds) {
    return RangeInterval{start_time, end_time, node_a, node_b, distance_light_seconds};
}

ContactPlan make_three_node_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 20.0, 1, 2, 200.0),
        make_contact(2, 2.0, 30.0, 2, 3, 200.0),
        make_contact(3, 20.0, 40.0, 1, 3, 200.0),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 3, 1.0),
        make_range(0.0, 100.0, 1, 3, 1.0),
    });
    return plan;
}

ContactPlan make_failure_reroute_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 5.0, 15.0, 1, 2, 500.0),
        make_contact(2, 16.0, 30.0, 2, 3, 500.0),
        make_contact(3, 20.0, 40.0, 1, 3, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 50.0, 1, 2, 1.0),
        make_range(0.0, 50.0, 2, 3, 1.0),
        make_range(0.0, 50.0, 1, 3, 1.0),
    });
    return plan;
}

ContactPlan make_reactive_anti_loop_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 2, 1, 500.0),
        make_contact(2, 5.0, 20.0, 1, 4, 500.0),
        make_contact(3, 0.0, 10.0, 2, 3, 500.0),
        make_contact(4, 12.0, 25.0, 3, 4, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 40.0, 2, 1, 1.0),
        make_range(0.0, 40.0, 1, 4, 1.0),
        make_range(0.0, 40.0, 2, 3, 1.0),
        make_range(0.0, 40.0, 3, 4, 1.0),
    });
    return plan;
}

ContactPlan make_redundant_delivery_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2, 500.0),
        make_contact(2, 11.0, 20.0, 2, 4, 500.0),
        make_contact(3, 0.0, 10.0, 1, 3, 500.0),
        make_contact(4, 21.0, 32.0, 3, 4, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 50.0, 1, 2, 1.0),
        make_range(0.0, 50.0, 2, 4, 1.0),
        make_range(0.0, 50.0, 1, 3, 1.0),
        make_range(0.0, 50.0, 3, 4, 1.0),
    });
    return plan;
}

TrafficPattern make_reactive_anti_loop_pattern() {
    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 2;
    pattern.destination_node = 4;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 60.0;
    pattern.header_size = 0.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 100.0;
    pattern.is_critical = false;
    pattern.allow_fragmentation = true;
    return pattern;
}

TrafficPattern make_single_bundle_pattern() {
    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 1;
    pattern.destination_node = 3;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 60.0;
    pattern.header_size = 0.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 100.0;
    pattern.is_critical = false;
    pattern.allow_fragmentation = true;
    return pattern;
}

TrafficPattern make_redundant_bundle_pattern() {
    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 1;
    pattern.destination_node = 4;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 60.0;
    pattern.header_size = 0.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 100.0;
    pattern.is_critical = false;
    pattern.allow_fragmentation = true;
    return pattern;
}

class StatsExportTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        temp_dir_ = std::filesystem::temp_directory_path() /
            ("sabr_stats_test_" + std::to_string(static_cast<long long>(now)));
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(temp_dir_, error);
    }

    std::string read_text(const std::filesystem::path& filepath) {
        std::ifstream stream(filepath);
        std::ostringstream contents;
        contents << stream.rdbuf();
        return contents.str();
    }

    std::filesystem::path temp_dir_;
};

}  // namespace

TEST(SimStatsTest, CapturesBundleContactAndRoutingSummaries) {
    auto contact_plan = std::make_shared<ContactPlan>(make_three_node_plan());

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 50.0;
    config.owlt_margin = 0.5;

    std::vector<TrafficPattern> traffic_patterns = {
        {TrafficGenerationMode::BATCH, 1, 3, 0.0, 4, 0.0, 0.0, 0.0, Priority::BULK, 60.0, false, true},
        {TrafficGenerationMode::SINGLE, 1, 3, 0.0, 1, 0.0, 0.0, 0.0, Priority::EXPEDITED, 60.0, false, true},
    };

    SimEngine engine(contact_plan, traffic_patterns, config);
    engine.initialize();
    engine.run();

    const StatsSummary& summary = engine.stats().summary();
    EXPECT_EQ(summary.bundles_created, 5u);
    EXPECT_EQ(summary.bundles_delivered, 5u);
    EXPECT_EQ(summary.tx_started, 10u);
    EXPECT_EQ(summary.tx_completed, 10u);
    EXPECT_EQ(summary.routing_invocations, 10u);
    EXPECT_EQ(summary.reroute_invocations, 5u);
    EXPECT_DOUBLE_EQ(summary.delivery_rate, 1.0);
    EXPECT_GT(summary.average_delivery_latency, 0.0);
    EXPECT_DOUBLE_EQ(summary.average_hop_count, 2.0);

    const std::vector<BundleStatsRecord> bundles = engine.stats().bundle_records();
    ASSERT_EQ(bundles.size(), 5u);
    EXPECT_EQ(bundles.front().final_state, "DELIVERED");
    EXPECT_EQ(bundles.front().hop_count, 2u);
    EXPECT_EQ(bundles.front().route_attempts, 2u);
    EXPECT_EQ(bundles.front().route_path, (std::vector<NodeId>{1, 2, 3}));

    const std::vector<ContactStatsRecord> contacts = engine.stats().contact_records();
    ASSERT_EQ(contacts.size(), 3u);
    EXPECT_DOUBLE_EQ(contacts[0].committed_volume, 500.0);
    EXPECT_EQ(contacts[0].commit_count, 5u);
    EXPECT_EQ(contacts[0].tx_started, 5u);
    EXPECT_EQ(contacts[0].tx_completed, 5u);
    EXPECT_DOUBLE_EQ(contacts[0].utilization_ratio, 0.125);
    EXPECT_DOUBLE_EQ(contacts[1].committed_volume, 500.0);
    EXPECT_EQ(contacts[1].commit_count, 5u);
    EXPECT_EQ(contacts[2].committed_volume, 0.0);

    const Logger& logger = engine.logger();
    ASSERT_EQ(logger.routing_records().size(), 10u);
    EXPECT_EQ(logger.routing_records().front().action, RoutingAction::FORWARD_ONE);
    ASSERT_TRUE(logger.routing_records().front().primary.has_value());
    ASSERT_FALSE(logger.routing_records().front().attempts.empty());
    EXPECT_FALSE(logger.routing_records().front().attempts.front().phase1_routes.empty());
    EXPECT_FALSE(logger.routing_records().front().attempts.front().phase2_candidates.empty());
    ASSERT_EQ(logger.commit_records().size(), 10u);
    EXPECT_EQ(logger.commit_records().front().selected_contact_ids, (std::vector<ContactId>{1, 2}));
    EXPECT_EQ(logger.commit_records().front().committed_contact_ids, (std::vector<ContactId>{1}));
}

TEST_F(StatsExportTest, ExportsJsonAndCsvFiles) {
    auto contact_plan = std::make_shared<ContactPlan>(make_three_node_plan());

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 20.0;
    config.owlt_margin = 0.5;

    SimEngine engine(contact_plan, {make_single_bundle_pattern()}, config);
    engine.initialize();
    engine.run();

    const std::filesystem::path json_path = temp_dir_ / "stats.json";
    const std::filesystem::path bundle_csv = temp_dir_ / "bundle_summary.csv";
    const std::filesystem::path contact_csv = temp_dir_ / "contact_utilization.csv";

    engine.stats().write_json(json_path);
    engine.stats().write_bundle_summary_csv(bundle_csv);
    engine.stats().write_contact_utilization_csv(contact_csv);

    ASSERT_TRUE(std::filesystem::exists(json_path));
    ASSERT_TRUE(std::filesystem::exists(bundle_csv));
    ASSERT_TRUE(std::filesystem::exists(contact_csv));

    const nlohmann::json document = engine.stats().to_json();
    EXPECT_TRUE(document.contains("metadata"));
    EXPECT_TRUE(document.contains("summary"));
    EXPECT_TRUE(document.contains("bundles"));
    EXPECT_TRUE(document.contains("contacts"));
    EXPECT_TRUE(document.contains("routing"));

    const std::string bundle_csv_text = read_text(bundle_csv);
    EXPECT_NE(bundle_csv_text.find("bundle_id,source_node,destination_node"), std::string::npos);
    EXPECT_NE(bundle_csv_text.find("1,1,3,DELIVERED"), std::string::npos);

    const std::string contact_csv_text = read_text(contact_csv);
    EXPECT_NE(contact_csv_text.find("contact_id,plan_version,sending_node"), std::string::npos);
    EXPECT_NE(contact_csv_text.find("1,"), std::string::npos);
}

TEST(SimStatsTest, CapturesPlanUpdateAndStaleEventMetrics) {
    auto contact_plan = std::make_shared<ContactPlan>(make_failure_reroute_plan());
    FailureRule rule;
    rule.mode = FailureRuleMode::CONTACT_SET;
    rule.trigger_time = 1.0;
    rule.contact_ids = {1};
    rule.label = "remove_first_hop";

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 50.0;
    config.failure_rules = {rule};

    SimEngine engine(contact_plan, {make_single_bundle_pattern()}, config);
    engine.initialize();
    engine.run();

    const StatsSummary& summary = engine.stats().summary();
    EXPECT_EQ(summary.plan_update_events, 1u);
    EXPECT_EQ(summary.queued_bundle_replans, 1u);
    EXPECT_GT(summary.stale_contact_events_ignored, 0u);
    ASSERT_EQ(engine.stats().plan_updates().size(), 1u);
    EXPECT_EQ(engine.stats().plan_updates().front().label, "remove_first_hop");
    EXPECT_EQ(engine.stats().plan_updates().front().drained_bundle_count, 1u);
    ASSERT_EQ(engine.logger().plan_updates().size(), 1u);
    EXPECT_EQ(engine.logger().plan_updates().front().removed_contact_ids, (std::vector<ContactId>{1}));
}

TEST(SimStatsTest, ReactiveAntiLoopReroutesAndReportsReason) {
    auto contact_plan = std::make_shared<ContactPlan>(make_reactive_anti_loop_plan());

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 40.0;
    config.phase2_config.anti_loop_reactive = true;

    SimEngine engine(contact_plan, {make_reactive_anti_loop_pattern()}, config);
    engine.initialize();

    const std::shared_ptr<Bundle> bundle = engine.find_bundle(1);
    ASSERT_NE(bundle, nullptr);
    bundle->mark_visited(1);

    engine.run();

    ASSERT_EQ(engine.bundle_state(1), BundleLifecycleState::DELIVERED);
    EXPECT_EQ(bundle->route_path, (std::vector<NodeId>{2, 3, 4}));

    const StatsSummary& summary = engine.stats().summary();
    EXPECT_EQ(summary.reactive_anti_loop_trigger_count, 1u);

    ASSERT_EQ(engine.logger().routing_records().size(), 2u);
    const RoutingRecord& first_record = engine.logger().routing_records().front();
    EXPECT_EQ(first_record.at_node, 2);
    EXPECT_EQ(first_record.action, RoutingAction::FORWARD_ONE);
    EXPECT_EQ(first_record.reroute_reason, "reactive_anti_loop");
    EXPECT_EQ(first_record.reroute_count, 1u);
    EXPECT_EQ(first_record.reroute_excluded_neighbors, (std::vector<NodeId>{1}));
    ASSERT_TRUE(first_record.primary.has_value());
    ASSERT_EQ(first_record.primary->legs.size(), 2u);
    EXPECT_EQ(first_record.primary->legs[0].contact_id, 3u);
    EXPECT_EQ(first_record.primary->legs[1].contact_id, 4u);

    ASSERT_EQ(engine.stats().routing_records().size(), 2u);
    const RoutingStatsRecord& first_stats_record = engine.stats().routing_records().front();
    EXPECT_EQ(first_stats_record.reroute_reason, "reactive_anti_loop");
    EXPECT_EQ(first_stats_record.reroute_count, 1u);
    EXPECT_EQ(first_stats_record.reroute_excluded_neighbors, (std::vector<NodeId>{1}));

    const auto reactive_event = std::find_if(
        engine.event_log().begin(),
        engine.event_log().end(),
        [](const std::string& message) {
            return message.find("REACTIVE_ANTI_LOOP bundle=1 node=2") != std::string::npos;
        });
    EXPECT_NE(reactive_event, engine.event_log().end());
}

TEST(SimStatsTest, RedundantBackupDeliversOnceAndDropsDuplicateReplica) {
    auto contact_plan = std::make_shared<ContactPlan>(make_redundant_delivery_plan());

    Node router_node(1);
    router_node.contact_plan = contact_plan.get();
    Bundle router_bundle;
    router_bundle.source_node = 1;
    router_bundle.destination_eid = 4;
    router_bundle.creation_time = 0.0;
    router_bundle.ttl = 100.0;
    router_bundle.payload_size = 60.0;
    router_bundle.priority = Priority::NORMAL;
    router_bundle.allow_fragmentation = true;

    RoutingContext routing_context(router_node, router_bundle, 0.0);
    routing_context.phase1_config.k_paths = 4;
    routing_context.redundancy_config.mode = RedundancyMode::SINGLE_BACKUP;
    routing_context.redundancy_config.max_extra_copies = 1;
    routing_context.redundancy_config.risk_threshold = 0.0;
    routing_context.redundancy_config.min_diversity_score = 0.5;
    routing_context.redundancy_config.min_capacity_reserve_ratio = 0.0;
    routing_context.redundancy_config.min_ttl_slack = 0.0;

    CGRRouter router;
    const RoutingTrace routing_trace = router.route_unicast_with_trace(routing_context);
    ASSERT_EQ(routing_trace.backup_routes.size(), 1u);

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 50.0;
    config.phase1_config.k_paths = 4;
    config.redundancy.mode = RedundancyMode::SINGLE_BACKUP;
    config.redundancy.max_extra_copies = 1;
    config.redundancy.risk_threshold = 0.0;
    config.redundancy.min_diversity_score = 0.5;
    config.redundancy.min_capacity_reserve_ratio = 0.0;
    config.redundancy.min_ttl_slack = 0.0;

    SimEngine engine(contact_plan, {make_redundant_bundle_pattern()}, config);
    engine.initialize();
    engine.run();

    ASSERT_FALSE(engine.stats().routing_records().empty());
    ASSERT_FALSE(engine.stats().routing_records().front().attempts.empty());
    EXPECT_GE(engine.stats().routing_records().front().attempts.front().phase2_valid_candidate_count, 2u);

    EXPECT_EQ(engine.bundle_state(1), BundleLifecycleState::DELIVERED);
    EXPECT_EQ(engine.bundle_state(2), BundleLifecycleState::DUPLICATE_DROPPED);
    EXPECT_EQ(engine.delivered_bundle_ids(), (std::vector<BundleId>{1}));

    const StatsSummary& summary = engine.stats().summary();
    EXPECT_EQ(summary.redundancy_trigger_count, 1u);
    EXPECT_EQ(summary.redundant_first_hit_count, 1u);
    EXPECT_EQ(summary.duplicate_replica_discard_count, 1u);
    EXPECT_DOUBLE_EQ(summary.redundancy_benefit_cost_ratio, 1.0);

    bool saw_duplicate = false;
    for (const std::string& message : engine.event_log()) {
        if (message.find("DUPLICATE_DROPPED bundle=2 node=4") != std::string::npos) {
            saw_duplicate = true;
            break;
        }
    }
    EXPECT_TRUE(saw_duplicate);

    bool saw_redundancy_considered = false;
    for (const RoutingRecord& record : engine.logger().routing_records()) {
        if (record.at_node == 1 && record.redundancy_considered) {
            saw_redundancy_considered = true;
            break;
        }
    }
    EXPECT_TRUE(saw_redundancy_considered);
}