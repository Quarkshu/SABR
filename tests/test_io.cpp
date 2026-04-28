#include <gtest/gtest.h>

#include "io/parser.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        temp_dir_ = std::filesystem::temp_directory_path() /
            ("sabr_io_test_" + std::to_string(static_cast<long long>(now)));
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(temp_dir_, error);
    }

    std::filesystem::path write_file(const std::filesystem::path& relative_path,
                                     const std::string& content) {
        const std::filesystem::path full_path = temp_dir_ / relative_path;
        std::filesystem::create_directories(full_path.parent_path());

        std::ofstream stream(full_path);
        stream << content;
        stream.close();
        return full_path;
    }

protected:
    std::filesystem::path temp_dir_;
};

TEST_F(ParserTest, ParsesIonContactPlanAndAssignsSequentialContactIds) {
    const std::filesystem::path plan_path = write_file("plan.ion", R"ION(
# comment
a contact +0 +10 1 2 100
a contact +12 +22 2 3 200
a range +0 +30 1 2 1.5
a range +0 +30 2 3 2.0
)ION");

    const std::shared_ptr<ContactPlan> plan = ConfigParser::parse_contact_plan_file(plan_path);
    ASSERT_NE(plan, nullptr);
    ASSERT_EQ(plan->contacts().size(), 2u);
    EXPECT_EQ(plan->contacts()[0].contact_id, 1u);
    EXPECT_EQ(plan->contacts()[1].contact_id, 2u);
    EXPECT_EQ(plan->contacts()[0].plan_version, plan->version());
    EXPECT_EQ(plan->contacts()[1].plan_version, plan->version());
    EXPECT_DOUBLE_EQ(plan->contacts()[0].mtv[0], plan->contacts()[0].volume());
    EXPECT_DOUBLE_EQ(plan->contacts()[0].mtv[1], plan->contacts()[0].volume());
    EXPECT_DOUBLE_EQ(plan->contacts()[0].mtv[2], plan->contacts()[0].volume());
    EXPECT_DOUBLE_EQ(plan->get_owlt(1, 2, 5.0), 1.5);
    EXPECT_DOUBLE_EQ(plan->get_owlt(2, 3, 15.0), 2.0);
}

TEST_F(ParserTest, ParsesJsonContactPlanWithFieldAliasesAndAutoIds) {
    const std::filesystem::path plan_path = write_file("plan.json", R"JSON(
{
  "contacts": [
    {
      "start_time": 0.0,
      "end_time": 10.0,
      "from_node": 1,
      "to_node": 2,
      "data_rate": 120.0
    },
    {
      "start": 12.0,
      "end": 20.0,
      "sending_node": 2,
      "receiving_node": 3,
      "rate": 180.0
    }
  ],
  "ranges": [
    {
      "start": 0.0,
      "end": 30.0,
      "node_a": 1,
      "node_b": 2,
      "distance_light_seconds": 1.25
    }
  ]
}
)JSON");

    const std::shared_ptr<ContactPlan> plan = ConfigParser::parse_contact_plan_file(plan_path);
    ASSERT_NE(plan, nullptr);
    ASSERT_EQ(plan->contacts().size(), 2u);
    EXPECT_EQ(plan->contacts()[0].contact_id, 1u);
    EXPECT_EQ(plan->contacts()[1].contact_id, 2u);
    EXPECT_EQ(plan->contacts()[1].sending_node, 2);
    EXPECT_EQ(plan->contacts()[1].receiving_node, 3);
    EXPECT_DOUBLE_EQ(plan->contacts()[1].data_rate, 180.0);
    EXPECT_DOUBLE_EQ(plan->get_owlt(2, 1, 5.0), 1.25);
}

TEST_F(ParserTest, ParsesInlineScenarioConfigIntoRuntimeTypes) {
    const std::filesystem::path scenario_path = write_file("scenario.json", R"JSON(
{
  "name": "inline_scenario",
  "output_directory": "results/exp1_inline",
  "contact_plan": {
    "contacts": [
      {"contact_id": 10, "start_time": 0.0, "end_time": 10.0, "from_node": 1, "to_node": 2, "data_rate": 100.0},
      {"start_time": 12.0, "end_time": 22.0, "from_node": 2, "to_node": 3, "data_rate": 100.0}
    ],
    "ranges": [
      {"start_time": 0.0, "end_time": 30.0, "node_a": 1, "node_b": 2, "distance_light_seconds": 1.0},
      {"start_time": 0.0, "end_time": 30.0, "node_a": 2, "node_b": 3, "distance_light_seconds": 1.0}
    ]
  },
  "enhancements": {
    "one_route_per_neighbor": true,
    "queue_delay": true,
    "anti_loop_reactive": true,
    "anti_loop_proactive": true
  },
  "simulation": {
    "start_time": 0.0,
    "end_time": 40.0,
    "owlt_margin": 0.5,
    "recompute_budget": 3,
    "failure_seed": 12345,
    "phase1": {
      "k_paths": 4
    }
  },
  "multicast_groups": [
    {
      "group_id": "ops_group",
      "source_node": 1,
      "member_nodes": [3, 4]
    }
  ],
  "traffic": [
    {
      "mode": "SINGLE",
      "source_node": 1,
      "destination_node": 3,
      "start_time": 1.0,
      "payload_size": 64.0,
      "header_size": 8.0,
      "priority": "NORMAL",
      "ttl": 60.0
    },
    {
      "mode": "BATCH",
      "source_node": 1,
      "multicast_group_id": "ops_group",
      "is_multicast": true,
      "start_time": 2.0,
      "bundle_count": 2,
      "payload_size": 128.0,
      "priority": "EXPEDITED",
      "ttl": 80.0,
      "is_critical": true,
      "allow_fragmentation": false
    }
  ],
  "failures": [
    {
      "mode": "CONTACT_SET",
      "trigger_time": 5.0,
      "contact_ids": [10],
      "label": "cut_first_hop"
    },
    {
      "mode": "PLAN_REPLACEMENT",
      "trigger_time": 8.0,
      "replacement_plan": {
        "contacts": [
          {"start_time": 0.0, "end_time": 20.0, "from_node": 1, "to_node": 3, "data_rate": 90.0}
        ],
        "ranges": [
          {"start_time": 0.0, "end_time": 30.0, "node_a": 1, "node_b": 3, "distance_light_seconds": 2.0}
        ]
      }
    }
  ],
  "redundancy": {
    "mode": "SINGLE_BACKUP",
    "max_extra_copies": 2,
    "risk_threshold": 0.7,
    "min_diversity_score": 0.6,
    "min_capacity_reserve_ratio": 0.2,
    "min_ttl_slack": 5.0,
    "enable_for_unicast": true,
    "enable_for_multicast_trunk": false
  },
  "experiment_matrix": {
    "experiment_name": "exp_inline",
    "dimensions": [
      {"name": "contact_failure_probability", "values": [0.0, 0.1, 0.2]},
      {"name": "redundancy_mode", "values": ["NONE", "SINGLE_BACKUP"]}
    ]
  }
}
)JSON");

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(scenario_path);
    ASSERT_NE(scenario.contact_plan, nullptr);
    EXPECT_EQ(scenario.scenario_name, "inline_scenario");
    EXPECT_EQ(scenario.output_directory, (temp_dir_ / "results/exp1_inline").lexically_normal());
    EXPECT_DOUBLE_EQ(scenario.engine.end_time, 40.0);
    EXPECT_DOUBLE_EQ(scenario.engine.owlt_margin, 0.5);
    EXPECT_EQ(scenario.engine.recompute_budget, 3);
    EXPECT_EQ(scenario.engine.failure_seed, 12345u);
    EXPECT_EQ(scenario.engine.phase1_config.k_paths, 4);
    EXPECT_TRUE(scenario.engine.phase1_config.one_route_per_neighbor);
    EXPECT_TRUE(scenario.engine.phase2_config.queue_delay_enhancement);
    EXPECT_TRUE(scenario.engine.phase2_config.anti_loop_reactive);
    EXPECT_TRUE(scenario.engine.phase2_config.anti_loop_proactive);
    EXPECT_EQ(scenario.engine.redundancy.mode, RedundancyMode::SINGLE_BACKUP);
    EXPECT_EQ(scenario.engine.redundancy.max_extra_copies, 2u);
    EXPECT_DOUBLE_EQ(scenario.engine.redundancy.risk_threshold, 0.7);
    EXPECT_FALSE(scenario.engine.redundancy.enable_for_multicast_trunk);
    ASSERT_EQ(scenario.traffic_patterns.size(), 2u);
    EXPECT_FALSE(scenario.traffic_patterns[0].is_multicast);
    EXPECT_TRUE(scenario.traffic_patterns[1].is_multicast);
    EXPECT_EQ(scenario.traffic_patterns[1].multicast_group_id, "ops_group");
    ASSERT_EQ(scenario.engine.failure_rules.size(), 2u);
    EXPECT_EQ(scenario.engine.failure_rules[0].mode, FailureRuleMode::CONTACT_SET);
    EXPECT_EQ(scenario.engine.failure_rules[1].mode, FailureRuleMode::PLAN_REPLACEMENT);
    ASSERT_NE(scenario.engine.failure_rules[1].replacement_plan, nullptr);
    ASSERT_EQ(scenario.multicast_groups.size(), 1u);
    EXPECT_EQ(scenario.multicast_groups[0].group_id, "ops_group");
    EXPECT_EQ(scenario.redundancy.mode, RedundancyMode::SINGLE_BACKUP);
    EXPECT_EQ(scenario.redundancy.max_extra_copies, 2u);
    ASSERT_EQ(scenario.experiment_matrix.dimensions.size(), 2u);
    ASSERT_EQ(scenario.experiment_matrix.dimensions[0].values.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<double>(scenario.experiment_matrix.dimensions[0].values[1]));
    EXPECT_DOUBLE_EQ(std::get<double>(scenario.experiment_matrix.dimensions[0].values[1]), 0.1);
    EXPECT_TRUE(std::holds_alternative<std::string>(scenario.experiment_matrix.dimensions[1].values[1]));
    EXPECT_EQ(std::get<std::string>(scenario.experiment_matrix.dimensions[1].values[1]), "SINGLE_BACKUP");
}

TEST_F(ParserTest, ResolvesRelativeContactPlanAndReplacementPlanFiles) {
    const std::filesystem::path contact_plan_path = write_file("plans/base.ion", R"ION(
a contact +0 +12 1 2 100
a range +0 +20 1 2 1.0
)ION");
    const std::filesystem::path replacement_plan_path = write_file("plans/replacement.json", R"JSON(
{
  "contacts": [
    {"start_time": 0.0, "end_time": 18.0, "from_node": 1, "to_node": 3, "data_rate": 80.0}
  ],
  "ranges": [
    {"start_time": 0.0, "end_time": 20.0, "node_a": 1, "node_b": 3, "distance_light_seconds": 2.5}
  ]
}
)JSON");
    const std::filesystem::path scenario_path = write_file("scenarios/relative.json", R"JSON(
{
  "scenario_name": "relative_paths",
  "contact_plan_file": "../plans/base.ion",
  "simulation": {
    "end_time": 50.0,
    "failure_seed": 7
  },
  "traffic": [
    {
      "source_node": 1,
      "destination_node": 2,
      "start_time": 1.0,
      "payload_size": 32.0,
      "ttl": 30.0
    }
  ],
  "failure_injection": [
    {
      "mode": "PLAN_REPLACEMENT",
      "trigger_time": 6.0,
      "replacement_plan_file": "../plans/replacement.json",
      "label": "swap"
    }
  ]
}
)JSON");

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(scenario_path);
    ASSERT_NE(scenario.contact_plan, nullptr);
    ASSERT_EQ(scenario.contact_plan->contacts().size(), 1u);
    ASSERT_EQ(scenario.engine.failure_rules.size(), 1u);
    ASSERT_NE(scenario.engine.failure_rules[0].replacement_plan, nullptr);
    EXPECT_EQ(scenario.engine.failure_rules[0].replacement_plan->contacts().size(), 1u);
    EXPECT_EQ(scenario.contact_plan->contacts()[0].receiving_node, 2);
    EXPECT_EQ(scenario.engine.failure_rules[0].replacement_plan->contacts()[0].receiving_node, 3);
    EXPECT_EQ(contact_plan_path.filename().string(), "base.ion");
    EXPECT_EQ(replacement_plan_path.filename().string(), "replacement.json");
}

TEST_F(ParserTest, ParsesStandaloneExperimentMatrixFile) {
    const std::filesystem::path matrix_path = write_file("matrix.json", R"JSON(
{
  "experiment_name": "exp_matrix",
  "scenario_files": ["configs/base.json", "configs/variant.json"],
  "dimensions": [
    {"name": "bundle_count", "values": [50, 200, 1000]},
    {"name": "redundancy_enabled", "values": [true, false]}
  ]
}
)JSON");

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);
    EXPECT_EQ(matrix.experiment_name, "exp_matrix");
    ASSERT_EQ(matrix.scenario_files.size(), 2u);
    EXPECT_EQ(matrix.scenario_files[0], (temp_dir_ / "configs/base.json").lexically_normal());
    ASSERT_EQ(matrix.dimensions.size(), 2u);
    EXPECT_EQ(matrix.dimensions[0].name, "bundle_count");
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(matrix.dimensions[0].values[0]));
    EXPECT_EQ(std::get<std::int64_t>(matrix.dimensions[0].values[2]), 1000);
    EXPECT_TRUE(std::holds_alternative<bool>(matrix.dimensions[1].values[1]));
    EXPECT_FALSE(std::get<bool>(matrix.dimensions[1].values[1]));
}

TEST_F(ParserTest, RejectsUnknownMulticastGroupInTrafficConfig) {
    const std::filesystem::path scenario_path = write_file("invalid_group.json", R"JSON(
{
  "contact_plan": {
    "contacts": [
      {"start_time": 0.0, "end_time": 12.0, "from_node": 1, "to_node": 2, "data_rate": 100.0}
    ],
    "ranges": []
  },
  "traffic": [
    {
      "source_node": 1,
      "multicast_group_id": "missing_group",
      "is_multicast": true,
      "start_time": 0.0,
      "payload_size": 10.0,
      "ttl": 20.0
    }
  ]
}
)JSON");

    EXPECT_THROW(ConfigParser::parse_scenario_file(scenario_path), std::runtime_error);
}

TEST_F(ParserTest, RejectsInvalidIonDefinition) {
    const std::filesystem::path plan_path = write_file("invalid.ion", R"ION(
a contact +0 +10 1 2
)ION");

    EXPECT_THROW(ConfigParser::parse_contact_plan_file(plan_path), std::runtime_error);
}

TEST_F(ParserTest, RejectsMissingReplacementPlanInPlanReplacementRule) {
    const std::filesystem::path scenario_path = write_file("invalid_failure.json", R"JSON(
{
  "contact_plan": {
    "contacts": [
      {"start_time": 0.0, "end_time": 12.0, "from_node": 1, "to_node": 2, "data_rate": 100.0}
    ],
    "ranges": []
  },
  "traffic": [
    {
      "source_node": 1,
      "destination_node": 2,
      "start_time": 0.0,
      "payload_size": 10.0,
      "ttl": 20.0
    }
  ],
  "failures": [
    {
      "mode": "PLAN_REPLACEMENT",
      "trigger_time": 5.0
    }
  ]
}
)JSON");

    EXPECT_THROW(ConfigParser::parse_scenario_file(scenario_path), std::runtime_error);
}

TEST(ParserTemplateTest, ParsesRepositoryExperimentTemplates) {
  const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
  const std::filesystem::path experiments_dir = repo_root / "configs" / "experiments";

  ASSERT_TRUE(std::filesystem::exists(experiments_dir));

  std::size_t scenario_count = 0;
  std::size_t matrix_count = 0;

  for (const auto& entry : std::filesystem::recursive_directory_iterator(experiments_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".json") {
      continue;
    }

    if (entry.path().filename() == "matrix.json") {
      const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(entry.path());
      ++matrix_count;

      EXPECT_FALSE(matrix.experiment_name.empty());
      ASSERT_FALSE(matrix.scenario_files.empty());
      for (const std::filesystem::path& scenario_path : matrix.scenario_files) {
        EXPECT_TRUE(std::filesystem::exists(scenario_path));
        if (std::filesystem::exists(scenario_path)) {
          const ScenarioConfig scenario = ConfigParser::parse_scenario_file(scenario_path);
          EXPECT_NE(scenario.contact_plan, nullptr);
          EXPECT_FALSE(scenario.scenario_name.empty());
        }
      }
      continue;
    }

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(entry.path());
    ++scenario_count;
    EXPECT_NE(scenario.contact_plan, nullptr);
    EXPECT_FALSE(scenario.scenario_name.empty());
  }

  EXPECT_GE(matrix_count, 6u);
  EXPECT_GE(scenario_count, 8u);
}

}  // namespace