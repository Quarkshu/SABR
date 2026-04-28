#include <gtest/gtest.h>

#include "io/experiment_runner.hpp"

#include <filesystem>

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

std::filesystem::path temp_output_root() {
    return std::filesystem::temp_directory_path() / "sabr_experiment_runner_tests";
}

void clear_temp_output_root() {
    std::error_code error;
    std::filesystem::remove_all(temp_output_root(), error);
}

}  // namespace

TEST(ExperimentRunnerTest, Exp2MatrixExpansionAppliesOverridesAndAssignsRunDirectories) {
    const std::filesystem::path matrix_path = exp2_dir() / "matrix.json";

    const std::vector<ExpandedExperimentRun> runs = ExperimentRunner::expand_matrix_file(matrix_path);

    ASSERT_EQ(runs.size(), 81u);
    EXPECT_EQ(runs.front().manifest.run_id, "matrix_0001");
    EXPECT_EQ(runs.front().manifest.experiment_name, "exp2_unicast_scale");
    EXPECT_EQ(runs.front().manifest.coordinates.size(), 4u);
    EXPECT_EQ(runs.front().scenario.traffic_patterns[0].bundle_count, 20u);
    EXPECT_DOUBLE_EQ(runs.front().scenario.traffic_patterns[0].payload_size, 256.0);
    EXPECT_EQ(runs.front().scenario.engine.phase1_config.k_paths, 4);
    EXPECT_DOUBLE_EQ(runs.front().scenario.engine.owlt_margin, 0.3);
    EXPECT_EQ(runs.front().scenario.output_directory,
              (repo_root() / "results" / "exp2_unicast_scale" / "baseline" / "matrix_0001").lexically_normal());

    EXPECT_EQ(runs.back().manifest.run_id, "matrix_0081");
    EXPECT_EQ(runs.back().scenario.traffic_patterns[0].bundle_count, 500u);
    EXPECT_DOUBLE_EQ(runs.back().scenario.traffic_patterns[0].payload_size, 4096.0);
    EXPECT_EQ(runs.back().scenario.engine.phase1_config.k_paths, 16);
    EXPECT_DOUBLE_EQ(runs.back().scenario.engine.owlt_margin, 1.0);
}

TEST(ExperimentRunnerTest, Exp3SyntheticGroupSizeAdjustsScenarioDocumentBeforeParsing) {
    const std::filesystem::path scenario_path = exp3_dir() / "tree_plan.json";

    const ExpandedExperimentRun run = ExperimentRunner::build_run(
        scenario_path,
        {{"multicast.group_size", static_cast<std::int64_t>(2)}},
        "exp3_multicast_plan",
        1);

    ASSERT_EQ(run.scenario.multicast_groups.size(), 1u);
    EXPECT_EQ(run.scenario.multicast_groups[0].member_nodes.size(), 2u);
    EXPECT_EQ(run.scenario.scenario_name, "exp3_multicast_plan_tree__matrix_0001");
}

TEST(ExperimentRunnerTest, RunScenarioWritesArtifactsAndManifest) {
    clear_temp_output_root();
    const std::filesystem::path scenario_path = exp1_dir() / "baseline.json";
    const ExpandedExperimentRun run = ExperimentRunner::build_run(
        scenario_path,
        {},
        {},
        0,
        temp_output_root() / "exp1_single");

    const ScenarioRunArtifacts artifacts = ExperimentRunner::run_scenario(run);

    ASSERT_TRUE(artifacts.success) << artifacts.error_message;
    EXPECT_TRUE(std::filesystem::exists(artifacts.stats_path));
    EXPECT_TRUE(std::filesystem::exists(artifacts.bundle_summary_path));
    EXPECT_TRUE(std::filesystem::exists(artifacts.contact_utilization_path));
    EXPECT_TRUE(std::filesystem::exists(artifacts.manifest_path));

    const nlohmann::json manifest = ConfigParser::load_json_file(artifacts.manifest_path);
    EXPECT_TRUE(manifest.at("success").get<bool>());
    EXPECT_EQ(manifest.at("run_id").get<std::string>(), "scenario");
    EXPECT_EQ(manifest.at("scenario_name").get<std::string>(), "exp1_unicast_correctness_baseline");

    const nlohmann::json stats = ConfigParser::load_json_file(artifacts.stats_path);
    EXPECT_EQ(stats.at("metadata").at("scenario_name").get<std::string>(),
              "exp1_unicast_correctness_baseline");
    EXPECT_TRUE(stats.at("summary").contains("delivery_rate"));

    clear_temp_output_root();
}