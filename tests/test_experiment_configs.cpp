#include <gtest/gtest.h>

#include "io/parser.hpp"

#include <filesystem>
#include <string>

namespace {

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::filesystem::path exp1_dir() {
    return repo_root() / "configs" / "experiments" / "exp1_unicast_correctness";
}

std::filesystem::path exp2_dir() {
    return repo_root() / "configs" / "experiments" / "exp2_unicast_scale";
}

std::filesystem::path exp3_dir() {
    return repo_root() / "configs" / "experiments" / "exp3_multicast_plan";
}

std::filesystem::path exp4_dir() {
    return repo_root() / "configs" / "experiments" / "exp4_multicast_repair";
}

std::filesystem::path exp6_dir() {
    return repo_root() / "configs" / "experiments" / "exp6_ablation";
}

std::filesystem::path exp5_dir() {
    return repo_root() / "configs" / "experiments" / "exp5_redundancy";
}

const ExperimentDimension* find_dimension(const ExperimentMatrixConfig& matrix,
                                          const std::string& name) {
    for (const ExperimentDimension& dimension : matrix.dimensions) {
        if (dimension.name == name) {
            return &dimension;
        }
    }
    return nullptr;
}

}  // namespace

TEST(ExperimentConfigTest, Exp1BaselineScenarioIsExecutable) {
    const std::filesystem::path baseline_path = exp1_dir() / "baseline.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(baseline_path);

    EXPECT_EQ(scenario.scenario_name, "exp1_unicast_correctness_baseline");
    EXPECT_EQ(scenario.source_file.lexically_normal(), baseline_path.lexically_normal());
    EXPECT_EQ(scenario.output_directory,
              (repo_root() / "results" / "exp1_unicast_correctness" / "baseline").lexically_normal());
    ASSERT_NE(scenario.contact_plan, nullptr);
    EXPECT_EQ(scenario.contact_plan->contacts().size(), 6u);
    EXPECT_EQ(scenario.contact_plan->ranges().size(), 6u);
    EXPECT_GT(scenario.contact_plan->version(), 0u);

    ASSERT_EQ(scenario.traffic_patterns.size(), 2u);
    EXPECT_EQ(scenario.traffic_patterns[0].mode, TrafficGenerationMode::SINGLE);
    EXPECT_EQ(scenario.traffic_patterns[0].source_node, 1);
    EXPECT_EQ(scenario.traffic_patterns[0].destination_node, 5);
    EXPECT_EQ(scenario.traffic_patterns[1].mode, TrafficGenerationMode::PERIODIC);
    EXPECT_EQ(scenario.traffic_patterns[1].bundle_count, 4u);

    EXPECT_EQ(scenario.engine.phase1_config.k_paths, 4);
    EXPECT_DOUBLE_EQ(scenario.engine.owlt_margin, 0.5);
    EXPECT_EQ(scenario.engine.recompute_budget, 3);
    EXPECT_TRUE(scenario.engine.phase1_config.one_route_per_neighbor);
    EXPECT_TRUE(scenario.engine.phase2_config.queue_delay_enhancement);
    EXPECT_TRUE(scenario.engine.phase2_config.anti_loop_proactive);
}

TEST(ExperimentConfigTest, Exp1MatrixDimensionsMatchStageGoal) {
    const std::filesystem::path matrix_path = exp1_dir() / "matrix.json";

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);

    EXPECT_EQ(matrix.experiment_name, "exp1_unicast_correctness");
    ASSERT_EQ(matrix.scenario_files.size(), 1u);
    EXPECT_EQ(matrix.scenario_files.front().lexically_normal(),
              (exp1_dir() / "baseline.json").lexically_normal());
    ASSERT_EQ(matrix.dimensions.size(), 3u);

    const ExperimentDimension* payload_size = find_dimension(matrix, "traffic[0].payload_size");
    ASSERT_NE(payload_size, nullptr);
    ASSERT_EQ(payload_size->values.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(payload_size->values[0]));
    EXPECT_EQ(std::get<std::int64_t>(payload_size->values[0]), 64);

    const ExperimentDimension* bundle_count = find_dimension(matrix, "traffic[1].bundle_count");
    ASSERT_NE(bundle_count, nullptr);
    ASSERT_EQ(bundle_count->values.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(bundle_count->values[2]));
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[2]), 64);

    const ExperimentDimension* k_paths = find_dimension(matrix, "simulation.phase1.k_paths");
    ASSERT_NE(k_paths, nullptr);
    ASSERT_EQ(k_paths->values.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(k_paths->values[1]));
    EXPECT_EQ(std::get<std::int64_t>(k_paths->values[1]), 4);
}

TEST(ExperimentConfigTest, Exp2BaselineScenarioIsExecutable) {
    const std::filesystem::path baseline_path = exp2_dir() / "baseline.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(baseline_path);

    EXPECT_EQ(scenario.scenario_name, "exp2_unicast_scale_baseline");
    EXPECT_EQ(scenario.source_file.lexically_normal(), baseline_path.lexically_normal());
    EXPECT_EQ(scenario.output_directory,
              (repo_root() / "results" / "exp2_unicast_scale" / "baseline").lexically_normal());
    ASSERT_NE(scenario.contact_plan, nullptr);
    EXPECT_FALSE(scenario.contact_plan->contacts().empty());
    EXPECT_FALSE(scenario.contact_plan->ranges().empty());

    ASSERT_EQ(scenario.traffic_patterns.size(), 1u);
    EXPECT_EQ(scenario.traffic_patterns[0].mode, TrafficGenerationMode::PERIODIC);
    EXPECT_EQ(scenario.traffic_patterns[0].source_node, 1);
    EXPECT_EQ(scenario.traffic_patterns[0].destination_node, 8);
    EXPECT_EQ(scenario.traffic_patterns[0].bundle_count, 20u);
    EXPECT_DOUBLE_EQ(scenario.traffic_patterns[0].period, 0.09);

    EXPECT_EQ(scenario.engine.phase1_config.k_paths, 3);
    EXPECT_FALSE(scenario.engine.phase1_config.one_route_per_neighbor);
    EXPECT_DOUBLE_EQ(scenario.engine.owlt_margin, 0.6);
    EXPECT_EQ(scenario.engine.recompute_budget, 6);
    EXPECT_EQ(scenario.engine.failure_seed, 202u);
}

TEST(ExperimentConfigTest, Exp2MatrixDimensionsMatchScaleExperiment) {
    const std::filesystem::path matrix_path = exp2_dir() / "matrix.json";

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);

    EXPECT_EQ(matrix.experiment_name, "exp2_unicast_scale");
    ASSERT_EQ(matrix.scenario_files.size(), 1u);
    EXPECT_EQ(matrix.scenario_files.front().lexically_normal(),
              (exp2_dir() / "baseline.json").lexically_normal());
    ASSERT_EQ(matrix.dimensions.size(), 4u);

    const ExperimentDimension* bundle_count = find_dimension(matrix, "traffic[0].bundle_count");
    ASSERT_NE(bundle_count, nullptr);
    ASSERT_EQ(bundle_count->values.size(), 3u);
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[0]), 20);
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[2]), 500);

    const ExperimentDimension* payload_size = find_dimension(matrix, "traffic[0].payload_size");
    ASSERT_NE(payload_size, nullptr);
    ASSERT_EQ(payload_size->values.size(), 3u);
    EXPECT_EQ(std::get<std::int64_t>(payload_size->values[1]), 1024);
    EXPECT_EQ(std::get<std::int64_t>(payload_size->values[2]), 2048);

    const ExperimentDimension* k_paths = find_dimension(matrix, "simulation.phase1.k_paths");
    ASSERT_NE(k_paths, nullptr);
    ASSERT_EQ(k_paths->values.size(), 3u);
    EXPECT_EQ(std::get<std::int64_t>(k_paths->values[0]), 1);
    EXPECT_EQ(std::get<std::int64_t>(k_paths->values[2]), 3);

    const ExperimentDimension* owlt_margin = find_dimension(matrix, "simulation.owlt_margin");
    ASSERT_NE(owlt_margin, nullptr);
    ASSERT_EQ(owlt_margin->values.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<double>(owlt_margin->values[0]));
    EXPECT_DOUBLE_EQ(std::get<double>(owlt_margin->values[0]), 0.0);
    EXPECT_DOUBLE_EQ(std::get<double>(owlt_margin->values[1]), 1.0);
    EXPECT_DOUBLE_EQ(std::get<double>(owlt_margin->values[2]), 2.0);
}

TEST(ExperimentConfigTest, Exp3TreePlanScenarioIsExecutable) {
    const std::filesystem::path scenario_path = exp3_dir() / "tree_plan.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(scenario_path);

    EXPECT_EQ(scenario.scenario_name, "exp3_multicast_plan_tree");
    ASSERT_EQ(scenario.multicast_groups.size(), 1u);
    EXPECT_EQ(scenario.multicast_groups[0].group_id, "science_team");
    EXPECT_EQ(scenario.multicast_groups[0].member_nodes.size(), 3u);
    ASSERT_EQ(scenario.traffic_patterns.size(), 1u);
    EXPECT_TRUE(scenario.traffic_patterns[0].is_multicast);
    EXPECT_EQ(scenario.traffic_patterns[0].multicast_group_id, "science_team");
}

TEST(ExperimentConfigTest, Exp3MatrixDimensionsMatchMulticastPlanExperiment) {
    const std::filesystem::path matrix_path = exp3_dir() / "matrix.json";

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);

    EXPECT_EQ(matrix.experiment_name, "exp3_multicast_plan");
    ASSERT_EQ(matrix.scenario_files.size(), 2u);
    ASSERT_EQ(matrix.dimensions.size(), 3u);

    const ExperimentDimension* bundle_count = find_dimension(matrix, "traffic[*].bundle_count");
    ASSERT_NE(bundle_count, nullptr);
    ASSERT_EQ(bundle_count->values.size(), 3u);
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[0]), 1);

    const ExperimentDimension* group_size = find_dimension(matrix, "multicast.group_size");
    ASSERT_NE(group_size, nullptr);
    ASSERT_EQ(group_size->values.size(), 2u);
    EXPECT_EQ(std::get<std::int64_t>(group_size->values[0]), 2);
    EXPECT_EQ(std::get<std::int64_t>(group_size->values[1]), 3);
}

TEST(ExperimentConfigTest, Exp4BaselineScenarioIncludesPlanReplacement) {
    const std::filesystem::path scenario_path = exp4_dir() / "baseline.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(scenario_path);

    EXPECT_EQ(scenario.scenario_name, "exp4_multicast_repair_baseline");
    ASSERT_EQ(scenario.multicast_groups.size(), 1u);
    EXPECT_EQ(scenario.multicast_groups[0].group_id, "ops_broadcast");
    ASSERT_EQ(scenario.engine.failure_rules.size(), 1u);
    EXPECT_EQ(scenario.engine.failure_rules[0].mode, FailureRuleMode::PLAN_REPLACEMENT);
    EXPECT_NE(scenario.engine.failure_rules[0].replacement_plan, nullptr);
}

TEST(ExperimentConfigTest, Exp4MatrixDimensionsMatchMulticastRepairExperiment) {
    const std::filesystem::path matrix_path = exp4_dir() / "matrix.json";

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);

    EXPECT_EQ(matrix.experiment_name, "exp4_multicast_repair");
    ASSERT_EQ(matrix.scenario_files.size(), 1u);
    ASSERT_EQ(matrix.dimensions.size(), 3u);

    const ExperimentDimension* failure_time = find_dimension(matrix, "failure_injection[0].trigger_time");
    ASSERT_NE(failure_time, nullptr);
    ASSERT_EQ(failure_time->values.size(), 3u);
    EXPECT_DOUBLE_EQ(std::get<double>(failure_time->values[0]), 4.0);

    const ExperimentDimension* bundle_count = find_dimension(matrix, "traffic[0].bundle_count");
    ASSERT_NE(bundle_count, nullptr);
    ASSERT_EQ(bundle_count->values.size(), 3u);
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[2]), 8);
}

TEST(ExperimentConfigTest, Exp6BaselineScenarioIncludesReactiveAntiLoop) {
    const std::filesystem::path baseline_path = exp6_dir() / "baseline.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(baseline_path);

    EXPECT_EQ(scenario.scenario_name, "exp6_ablation_full_stack");
    EXPECT_TRUE(scenario.engine.phase1_config.one_route_per_neighbor);
    EXPECT_TRUE(scenario.engine.phase2_config.queue_delay_enhancement);
    EXPECT_TRUE(scenario.engine.phase2_config.anti_loop_reactive);
    EXPECT_TRUE(scenario.engine.phase2_config.anti_loop_proactive);
}

TEST(ExperimentConfigTest, Exp5BaselineScenarioIncludesRuntimeRedundancy) {
    const std::filesystem::path baseline_path = exp5_dir() / "single_backup.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(baseline_path);

    EXPECT_EQ(scenario.scenario_name, "exp5_redundancy_single_backup");
    EXPECT_EQ(scenario.engine.redundancy.mode, RedundancyMode::SINGLE_BACKUP);
    EXPECT_EQ(scenario.engine.redundancy.max_extra_copies, 1u);
    EXPECT_DOUBLE_EQ(scenario.engine.redundancy.risk_threshold, 0.55);
    EXPECT_TRUE(scenario.engine.redundancy.enable_for_unicast);
    EXPECT_FALSE(scenario.engine.redundancy.enable_for_multicast_trunk);
}

TEST(ExperimentConfigTest, Exp5AdditionalBaselineScenariosCoverExpandedModes) {
    const ScenarioConfig multi_backup = ConfigParser::parse_scenario_file(exp5_dir() / "multi_backup.json");
    const ScenarioConfig trunk_only = ConfigParser::parse_scenario_file(exp5_dir() / "trunk_only.json");

    EXPECT_EQ(multi_backup.engine.redundancy.mode, RedundancyMode::MULTI_BACKUP);
    EXPECT_EQ(multi_backup.engine.redundancy.max_extra_copies, 2u);
    EXPECT_TRUE(multi_backup.engine.redundancy.enable_for_unicast);

    EXPECT_EQ(trunk_only.engine.redundancy.mode, RedundancyMode::TRUNK_ONLY);
    EXPECT_FALSE(trunk_only.engine.redundancy.enable_for_unicast);
    EXPECT_TRUE(trunk_only.engine.redundancy.enable_for_multicast_trunk);
}

TEST(ExperimentConfigTest, Exp5MatrixDimensionsMatchRedundancyExperiment) {
    const std::filesystem::path matrix_path = exp5_dir() / "matrix.json";

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);

    EXPECT_EQ(matrix.experiment_name, "exp5_redundancy");
    ASSERT_EQ(matrix.scenario_files.size(), 2u);
    ASSERT_EQ(matrix.dimensions.size(), 3u);

    const ExperimentDimension* failure_probability = find_dimension(matrix, "failures[0].probability");
    ASSERT_NE(failure_probability, nullptr);
    ASSERT_EQ(failure_probability->values.size(), 4u);
    EXPECT_DOUBLE_EQ(std::get<double>(failure_probability->values[0]), 0.0);
    EXPECT_DOUBLE_EQ(std::get<double>(failure_probability->values[3]), 0.3);

    const ExperimentDimension* redundancy_mode = find_dimension(matrix, "redundancy.mode");
    ASSERT_NE(redundancy_mode, nullptr);
    ASSERT_EQ(redundancy_mode->values.size(), 3u);
    EXPECT_EQ(std::get<std::string>(redundancy_mode->values[0]), "NONE");
    EXPECT_EQ(std::get<std::string>(redundancy_mode->values[2]), "MULTI_BACKUP");

    const ExperimentDimension* extra_copies = find_dimension(matrix, "redundancy.max_extra_copies");
    ASSERT_NE(extra_copies, nullptr);
    ASSERT_EQ(extra_copies->values.size(), 2u);
    EXPECT_EQ(std::get<std::int64_t>(extra_copies->values[0]), 1);
    EXPECT_EQ(std::get<std::int64_t>(extra_copies->values[1]), 2);
}

TEST(ExperimentConfigTest, Exp6MatrixIncludesReactiveAntiLoopDimension) {
    const std::filesystem::path matrix_path = exp6_dir() / "matrix.json";

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);

    EXPECT_EQ(matrix.experiment_name, "exp6_ablation");
    ASSERT_EQ(matrix.dimensions.size(), 6u);

    const ExperimentDimension* reactive = find_dimension(matrix, "enhancements.anti_loop_reactive");
    ASSERT_NE(reactive, nullptr);
    ASSERT_EQ(reactive->values.size(), 2u);
    EXPECT_TRUE(std::holds_alternative<bool>(reactive->values[0]));
    EXPECT_TRUE(std::holds_alternative<bool>(reactive->values[1]));
}