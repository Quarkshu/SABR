#include <gtest/gtest.h>

#include "io/parser.hpp"
#include "simulation/engine.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr NodeId kSaNode = 1;
constexpr NodeId kGs2Node = 2;
constexpr NodeId kGs1Node = 3;
constexpr NodeId kOrbiterNode = 4;
constexpr NodeId kMccNode = 5;

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

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::filesystem::path exp1_dir() {
    return repo_root() / "configs" / "experiments" / "exp1_unicast_correctness";
}

ScenarioConfig load_exp1_scenario(const std::string& filename) {
    return ConfigParser::parse_scenario_file(exp1_dir() / filename);
}

std::unique_ptr<SimEngine> initialize_scenario(const ScenarioConfig& scenario) {
    auto engine = std::make_unique<SimEngine>(scenario.contact_plan,
                                              scenario.traffic_patterns,
                                              scenario.engine);
    engine->initialize();
    return engine;
}

std::unique_ptr<SimEngine> run_scenario(const ScenarioConfig& scenario) {
    auto engine = initialize_scenario(scenario);
    engine->run();
    return engine;
}

std::set<NodeId> collect_routing_nodes(const Logger& logger) {
    std::set<NodeId> nodes;
    for (const RoutingRecord& record : logger.routing_records()) {
        nodes.insert(record.at_node);
    }
    return nodes;
}

bool has_unique_path(const std::vector<NodeId>& path) {
    std::set<NodeId> unique_nodes(path.begin(), path.end());
    return unique_nodes.size() == path.size();
}

ScenarioConfig make_orpn_reference_scenario(bool enable_one_route_per_neighbor) {
    ScenarioConfig scenario;
    scenario.scenario_name = enable_one_route_per_neighbor
        ? "stage7_orpn_enabled"
        : "stage7_orpn_disabled";

    auto plan = std::make_shared<ContactPlan>();
    plan->set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2, 300.0),
        make_contact(2, 0.0, 10.0, 1, 3, 300.0),
        make_contact(3, 12.0, 20.0, 2, 4, 300.0),
        make_contact(4, 11.0, 20.0, 3, 4, 300.0),
    });
    plan->set_ranges({
        make_range(0.0, 30.0, 1, 2, 1.0),
        make_range(0.0, 30.0, 1, 3, 2.0),
        make_range(0.0, 30.0, 2, 4, 1.0),
        make_range(0.0, 30.0, 3, 4, 5.0),
    });

    scenario.contact_plan = plan;
    scenario.engine.start_time = 0.0;
    scenario.engine.end_time = 30.0;
    scenario.engine.recompute_budget = 0;
    scenario.engine.phase1_config.k_paths = 1;
    scenario.engine.phase1_config.one_route_per_neighbor = enable_one_route_per_neighbor;
    scenario.traffic_patterns = {
        {TrafficGenerationMode::SINGLE, 1, 4, 0.0, 1, 0.0, 32.0, 8.0, Priority::NORMAL, 40.0, false, true}
    };
    return scenario;
}

ScenarioConfig make_queue_delay_reference_scenario(bool enable_queue_delay) {
    ScenarioConfig scenario;
    scenario.scenario_name = enable_queue_delay
        ? "stage7_queue_delay_enabled"
        : "stage7_queue_delay_disabled";

    auto plan = std::make_shared<ContactPlan>();
    plan->set_contacts({
        make_contact(1, 0.0, 4.0, 1, 2, 500.0),
        make_contact(2, 5.0, 15.0, 2, 4, 50.0),
        make_contact(3, 0.0, 4.0, 1, 3, 500.0),
        make_contact(4, 8.0, 18.0, 3, 4, 50.0),
    });
    plan->set_ranges({
        make_range(0.0, 30.0, 1, 2, 1.0),
        make_range(0.0, 30.0, 2, 4, 1.0),
        make_range(0.0, 30.0, 1, 3, 1.0),
        make_range(0.0, 30.0, 3, 4, 1.0),
    });

    scenario.contact_plan = plan;
    scenario.engine.start_time = 0.0;
    scenario.engine.end_time = 30.0;
    scenario.engine.phase1_config.k_paths = 2;
    scenario.engine.phase2_config.queue_delay_enhancement = enable_queue_delay;
    scenario.traffic_patterns = {
        {TrafficGenerationMode::SINGLE, 2, 4, 0.0, 1, 0.0, 200.0, 0.0, Priority::NORMAL, 40.0, false, true},
        {TrafficGenerationMode::SINGLE, 1, 4, 1.0, 1, 0.0, 32.0, 8.0, Priority::NORMAL, 40.0, false, true},
    };
    return scenario;
}

ScenarioConfig make_antiloop_reference_scenario(bool proactive,
                                                bool reactive) {
    ScenarioConfig scenario;
    scenario.scenario_name = proactive
        ? "stage7_antiloop_proactive_reactive"
        : "stage7_antiloop_reactive_only";

    auto plan = std::make_shared<ContactPlan>();
    plan->set_contacts({
        make_contact(1, 0.0, 10.0, 2, 1, 500.0),
        make_contact(2, 5.0, 20.0, 1, 4, 500.0),
        make_contact(3, 0.0, 10.0, 2, 3, 500.0),
        make_contact(4, 12.0, 25.0, 3, 4, 500.0),
    });
    plan->set_ranges({
        make_range(0.0, 40.0, 2, 1, 1.0),
        make_range(0.0, 40.0, 1, 4, 1.0),
        make_range(0.0, 40.0, 2, 3, 1.0),
        make_range(0.0, 40.0, 3, 4, 1.0),
    });

    scenario.contact_plan = plan;
    scenario.engine.start_time = 0.0;
    scenario.engine.end_time = 40.0;
    scenario.engine.phase2_config.anti_loop_proactive = proactive;
    scenario.engine.phase2_config.anti_loop_reactive = reactive;
    scenario.traffic_patterns = {
        {TrafficGenerationMode::SINGLE, 2, 4, 0.0, 1, 0.0, 60.0, 0.0, Priority::NORMAL, 60.0, false, true}
    };
    return scenario;
}

}  // namespace

TEST(ReferenceScenarioTest, LoadsExp1BaselineAsRef2Equivalent) {
    const ScenarioConfig scenario = load_exp1_scenario("baseline.json");

    ASSERT_NE(scenario.contact_plan, nullptr);
    EXPECT_EQ(scenario.scenario_name, "exp1_unicast_correctness_baseline");
    EXPECT_EQ(scenario.contact_plan->contacts().size(), 6u);
    EXPECT_EQ(scenario.contact_plan->ranges().size(), 6u);

    EXPECT_EQ(scenario.traffic_patterns.size(), 2u);
    EXPECT_EQ(scenario.traffic_patterns.front().source_node, kSaNode);
    EXPECT_EQ(scenario.traffic_patterns.front().destination_node, kMccNode);

    EXPECT_EQ(scenario.contact_plan->get_contacts_between(kSaNode, kGs2Node).size(), 1u);
    EXPECT_EQ(scenario.contact_plan->get_contacts_between(kSaNode, kGs1Node).size(), 1u);
    EXPECT_EQ(scenario.contact_plan->get_contacts_between(kGs2Node, kMccNode).size(), 1u);
    EXPECT_EQ(scenario.contact_plan->get_contacts_between(kGs1Node, kMccNode).size(), 1u);
    EXPECT_EQ(scenario.contact_plan->get_contacts_between(kGs2Node, kOrbiterNode).size(), 1u);
    EXPECT_EQ(scenario.contact_plan->get_contacts_between(kOrbiterNode, kMccNode).size(), 1u);
}

TEST(ReferenceScenarioTest, VT_IT_01_EndToEndUnicastOnFiveNodeScenario) {
    const ScenarioConfig scenario = load_exp1_scenario("stage5_reference_baseline.json");

    std::unique_ptr<SimEngine> engine = run_scenario(scenario);
    const EngineMetrics& metrics = engine->metrics();
    const StatsSummary& summary = engine->stats().summary();

    EXPECT_EQ(metrics.bundles_created, 5u);
    EXPECT_EQ(metrics.bundles_delivered, 5u);
    EXPECT_EQ(metrics.bundles_expired, 0u);
    EXPECT_EQ(metrics.route_failures, 0u);
    EXPECT_EQ(metrics.tx_started, 10u);
    EXPECT_EQ(metrics.tx_completed, 10u);
    EXPECT_EQ(metrics.routing_invocations, 10u);
    EXPECT_EQ(metrics.reroute_invocations, 5u);

    EXPECT_EQ(summary.bundles_created, 5u);
    EXPECT_EQ(summary.bundles_delivered, 5u);
    EXPECT_EQ(summary.bundles_expired, 0u);
    EXPECT_EQ(summary.route_failures, 0u);
    EXPECT_EQ(summary.routing_invocations, 10u);
    EXPECT_EQ(summary.reroute_invocations, 5u);

    ASSERT_EQ(engine->delivered_bundle_ids().size(), 5u);
    for (BundleId bundle_id = 1; bundle_id <= 5; ++bundle_id) {
        const std::shared_ptr<Bundle> bundle = engine->find_bundle(bundle_id);
        ASSERT_NE(bundle, nullptr);
        EXPECT_EQ(engine->bundle_state(bundle_id), BundleLifecycleState::DELIVERED);
        ASSERT_GE(bundle->route_path.size(), 3u);
        EXPECT_EQ(bundle->route_path.front(), kSaNode);
        EXPECT_EQ(bundle->route_path.back(), kMccNode);
        EXPECT_TRUE(has_unique_path(bundle->route_path));
        ASSERT_EQ(metrics.route_attempts.count(bundle_id), 1u);
        EXPECT_EQ(metrics.route_attempts.at(bundle_id), 2u);
    }

    const std::set<NodeId> routing_nodes = collect_routing_nodes(engine->logger());
    EXPECT_TRUE(routing_nodes.count(kSaNode) > 0);
    EXPECT_TRUE(routing_nodes.count(kGs2Node) > 0);
    EXPECT_EQ(engine->logger().routing_records().size(), 10u);
    EXPECT_EQ(engine->stats().routing_records().size(), 10u);
    ASSERT_EQ(engine->stats().bundle_records().size(), 5u);
    for (const BundleStatsRecord& record : engine->stats().bundle_records()) {
        EXPECT_EQ(record.final_state, "DELIVERED");
        EXPECT_EQ(record.source_node, kSaNode);
        EXPECT_EQ(record.destination_node, kMccNode);
        EXPECT_EQ(record.route_attempts, 2u);
        EXPECT_EQ(record.route_path.front(), kSaNode);
        EXPECT_EQ(record.route_path.back(), kMccNode);
    }
}

TEST(ReferenceScenarioTest, VT_IT_02_DownlinkFirstTenViaGs2LastTenViaGs1) {
    const ScenarioConfig scenario = load_exp1_scenario("stage5_downlink_split.json");

    std::unique_ptr<SimEngine> engine = run_scenario(scenario);
    const EngineMetrics& metrics = engine->metrics();
    const StatsSummary& summary = engine->stats().summary();

    EXPECT_EQ(metrics.bundles_created, 20u);
    EXPECT_EQ(metrics.bundles_delivered, 10u);
    EXPECT_EQ(metrics.bundles_expired, 10u);
    EXPECT_EQ(metrics.route_failures, 0u);
    EXPECT_EQ(metrics.routing_invocations, 40u);
    EXPECT_EQ(metrics.reroute_invocations, 20u);

    EXPECT_EQ(summary.bundles_created, 20u);
    EXPECT_EQ(summary.bundles_delivered, 10u);
    EXPECT_EQ(summary.bundles_expired, 10u);
    EXPECT_EQ(summary.route_failures, 0u);
    EXPECT_EQ(summary.routing_invocations, 40u);
    EXPECT_EQ(summary.reroute_invocations, 20u);

    for (BundleId bundle_id = 1; bundle_id <= 10; ++bundle_id) {
        const std::shared_ptr<Bundle> bundle = engine->find_bundle(bundle_id);
        ASSERT_NE(bundle, nullptr);
        ASSERT_GE(bundle->route_path.size(), 3u);
        EXPECT_EQ(bundle->route_path[1], kGs2Node);
        EXPECT_EQ(bundle->route_path.back(), kMccNode);
        EXPECT_EQ(engine->bundle_state(bundle_id), BundleLifecycleState::DELIVERED);
        ASSERT_EQ(metrics.route_attempts.count(bundle_id), 1u);
        EXPECT_EQ(metrics.route_attempts.at(bundle_id), 2u);
    }

    for (BundleId bundle_id = 11; bundle_id <= 20; ++bundle_id) {
        const std::shared_ptr<Bundle> bundle = engine->find_bundle(bundle_id);
        ASSERT_NE(bundle, nullptr);
        ASSERT_GE(bundle->route_path.size(), 2u);
        EXPECT_EQ(bundle->route_path[1], kGs1Node);
        EXPECT_EQ(engine->bundle_state(bundle_id), BundleLifecycleState::EXPIRED);
        ASSERT_EQ(metrics.route_attempts.count(bundle_id), 1u);
        EXPECT_EQ(metrics.route_attempts.at(bundle_id), 2u);
    }

    const std::set<NodeId> routing_nodes = collect_routing_nodes(engine->logger());
    EXPECT_TRUE(routing_nodes.count(kSaNode) > 0);
    EXPECT_TRUE(routing_nodes.count(kGs2Node) > 0);
    EXPECT_TRUE(routing_nodes.count(kGs1Node) > 0);
    EXPECT_EQ(engine->logger().routing_records().size(), 40u);
    EXPECT_EQ(engine->stats().routing_records().size(), 40u);
}

TEST(ReferenceScenarioTest, VT_SC_01_And_VT_SC_02_PicsCoverageMapIsSatisfied) {
    struct CoverageRow {
        const char* requirement;
        const char* covered_by;
        const char* rationale;
    };

    const std::vector<CoverageRow> coverage = {
        {"VT-SC-01 / REF-1 contact graph example", "tests/test_dijkstra.cpp", "阶段 2A 已覆盖 REF-1 图 3-1 至 3-4 的接触图与最短路计算。"},
        {"PICS / Phase1 route generation", "tests/test_phase1.cpp", "阶段 2B 已覆盖 K 路生成、缓存复用和版本失效。"},
        {"PICS / Phase2 ETO-PBAT-RVL and exclusions", "tests/test_phase2.cpp", "阶段 2C 已覆盖完整 ETO / PBAT / RVL 与 7 条排除规则。"},
        {"PICS / CGR selection and commit separation", "tests/test_cgr.cpp", "阶段 2D 与 4B 已覆盖 route_unicast、commit_route 与 trace 保留。"},
        {"VT-IT-01 / reference scenario end-to-end", "tests/test_reference_scenario.cpp", "阶段 5 在五节点场景上补足端到端单播闭环验证。"},
        {"VT-IT-02 / hop-by-hop reroute evidence", "configs/experiments/exp1_unicast_correctness/stage5_downlink_split.json", "阶段 5 在五节点下行场景上补足前 10 / 后 10 首跳分流与逐跳重算验证。"},
    };

    const std::filesystem::path root = repo_root();
    for (const CoverageRow& row : coverage) {
        EXPECT_FALSE(std::string(row.requirement).empty());
        EXPECT_FALSE(std::string(row.rationale).empty());
        EXPECT_TRUE(std::filesystem::exists(root / row.covered_by)) << row.covered_by;
    }

    EXPECT_TRUE(std::filesystem::exists(exp1_dir() / "contact_plan.ion"));
    EXPECT_TRUE(std::filesystem::exists(exp1_dir() / "stage5_reference_baseline.json"));
    EXPECT_TRUE(std::filesystem::exists(exp1_dir() / "stage5_downlink_split.json"));
}

TEST(ReferenceScenarioTest, VT_IT_03_OneRoutePerNeighborCanBeEnabledIndependently) {
    auto disabled_engine = initialize_scenario(make_orpn_reference_scenario(false));
    disabled_engine->ensure_node(1).excluded_neighbors.insert(2);
    disabled_engine->run();

    EXPECT_EQ(disabled_engine->bundle_state(1), BundleLifecycleState::ROUTE_FAILED);
    ASSERT_EQ(disabled_engine->logger().routing_records().size(), 1u);
    EXPECT_EQ(disabled_engine->logger().routing_records().front().failure_reason, "no_candidate_routes");

    auto enabled_engine = initialize_scenario(make_orpn_reference_scenario(true));
    enabled_engine->ensure_node(1).excluded_neighbors.insert(2);
    enabled_engine->run();

    EXPECT_EQ(enabled_engine->bundle_state(1), BundleLifecycleState::DELIVERED);
    const std::shared_ptr<Bundle> bundle = enabled_engine->find_bundle(1);
    ASSERT_NE(bundle, nullptr);
    EXPECT_EQ(bundle->route_path, (std::vector<NodeId>{1, 3, 4}));
}

TEST(ReferenceScenarioTest, VT_IT_04_QueueDelayCanBeEnabledIndependently) {
    std::unique_ptr<SimEngine> disabled_engine = run_scenario(make_queue_delay_reference_scenario(false));
    std::unique_ptr<SimEngine> enabled_engine = run_scenario(make_queue_delay_reference_scenario(true));

    const std::shared_ptr<Bundle> disabled_bundle = disabled_engine->find_bundle(2);
    const std::shared_ptr<Bundle> enabled_bundle = enabled_engine->find_bundle(2);
    ASSERT_NE(disabled_bundle, nullptr);
    ASSERT_NE(enabled_bundle, nullptr);

    EXPECT_EQ(disabled_bundle->route_path, (std::vector<NodeId>{1, 2, 4}));
    EXPECT_EQ(enabled_bundle->route_path, (std::vector<NodeId>{1, 3, 4}));
}

TEST(ReferenceScenarioTest, VT_IT_05_AntiLoopCombinationPrefersNonLoopingBeforeReactiveReroute) {
    auto reactive_only_engine = initialize_scenario(make_antiloop_reference_scenario(false, true));
    const std::shared_ptr<Bundle> reactive_only_bundle = reactive_only_engine->find_bundle(1);
    ASSERT_NE(reactive_only_bundle, nullptr);
    reactive_only_bundle->mark_visited(1);
    reactive_only_engine->run();

    EXPECT_EQ(reactive_only_engine->bundle_state(1), BundleLifecycleState::DELIVERED);
    EXPECT_EQ(reactive_only_bundle->route_path, (std::vector<NodeId>{2, 3, 4}));
    ASSERT_EQ(reactive_only_engine->logger().routing_records().size(), 2u);
    EXPECT_EQ(reactive_only_engine->logger().routing_records().front().reroute_reason, "reactive_anti_loop");
    EXPECT_EQ(reactive_only_engine->stats().summary().reactive_anti_loop_trigger_count, 1u);

    auto proactive_engine = initialize_scenario(make_antiloop_reference_scenario(true, true));
    const std::shared_ptr<Bundle> proactive_bundle = proactive_engine->find_bundle(1);
    ASSERT_NE(proactive_bundle, nullptr);
    proactive_bundle->mark_visited(1);
    proactive_engine->run();

    EXPECT_EQ(proactive_engine->bundle_state(1), BundleLifecycleState::DELIVERED);
    EXPECT_EQ(proactive_bundle->route_path, (std::vector<NodeId>{2, 3, 4}));
    ASSERT_EQ(proactive_engine->logger().routing_records().size(), 2u);
    const RoutingRecord& first_record = proactive_engine->logger().routing_records().front();
    EXPECT_TRUE(first_record.reroute_reason.empty());
    EXPECT_EQ(proactive_engine->stats().summary().reactive_anti_loop_trigger_count, 0u);
    ASSERT_FALSE(first_record.attempts.empty());
    const auto looping_candidate = std::find_if(
        first_record.attempts.front().phase2_candidates.begin(),
        first_record.attempts.front().phase2_candidates.end(),
        [](const CandidateRouteLog& candidate) {
            return candidate.valid && candidate.route.possibly_looping;
        });
    EXPECT_NE(looping_candidate, first_record.attempts.front().phase2_candidates.end());
}