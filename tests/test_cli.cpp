#include <gtest/gtest.h>

#include "io/cli.hpp"

TEST(CliParseTest, EmptyArgsMapToVersion) {
    const CLICommand command = parse_cli_command({});

    EXPECT_EQ(command.kind, CLICommandKind::VERSION);
}

TEST(CliParseTest, HelpCommandProducesExpandedUsageText) {
    const CLICommand command = parse_cli_command({"help"});
    const std::string usage = build_usage_text();

    EXPECT_EQ(command.kind, CLICommandKind::HELP);
    EXPECT_NE(usage.find("run-scenario"), std::string::npos);
    EXPECT_NE(usage.find("run-experiment"), std::string::npos);
    EXPECT_NE(usage.find("export"), std::string::npos);
    EXPECT_NE(usage.find("plot"), std::string::npos);
}

TEST(CliParseTest, ParsesRunScenarioArguments) {
    const CLICommand command = parse_cli_command(
        {"run-scenario", "--config", "configs/exp1.json", "--output", "results/out"});

    EXPECT_EQ(command.kind, CLICommandKind::RUN_SCENARIO);
    EXPECT_EQ(command.config_path, std::filesystem::path("configs/exp1.json"));
    EXPECT_EQ(command.output_path, std::filesystem::path("results/out"));
}

TEST(CliParseTest, ParsesRunExperimentArguments) {
    const CLICommand command = parse_cli_command(
        {"run-experiment", "--matrix", "configs/matrix.json", "--output", "results/out", "--limit", "3"});

    EXPECT_EQ(command.kind, CLICommandKind::RUN_EXPERIMENT);
    EXPECT_EQ(command.matrix_path, std::filesystem::path("configs/matrix.json"));
    EXPECT_EQ(command.output_path, std::filesystem::path("results/out"));
    EXPECT_EQ(command.limit, 3u);
}

TEST(CliParseTest, ParsesExportArgumentsWithDefaultFormat) {
    const CLICommand command = parse_cli_command(
        {"export", "--input", "results/script_smoke"});

    EXPECT_EQ(command.kind, CLICommandKind::EXPORT);
    EXPECT_EQ(command.input_path, std::filesystem::path("results/script_smoke"));
    EXPECT_EQ(command.format, "flat");
}

TEST(CliParseTest, ParsesExportArgumentsWithPivotOptions) {
    const CLICommand command = parse_cli_command(
        {"export", "--input", "results/cli_matrix_smoke", "--format", "pivot", "--metric", "summary::delivery_rate", "--row-key", "coord::traffic[0].payload_size", "--column-key", "coord::simulation.phase1.k_paths"});

    EXPECT_EQ(command.kind, CLICommandKind::EXPORT);
    EXPECT_EQ(command.format, "pivot");
    EXPECT_EQ(command.metric, "summary::delivery_rate");
    EXPECT_EQ(command.row_key, "coord::traffic[0].payload_size");
    EXPECT_EQ(command.column_key, "coord::simulation.phase1.k_paths");
}

TEST(CliParseTest, ParsesPlotArguments) {
    const CLICommand command = parse_cli_command(
        {"plot", "--kind", "timeline", "--input", "results/script_smoke/exp1_unicast_correctness", "--output", "timeline.svg"});

    EXPECT_EQ(command.kind, CLICommandKind::PLOT);
    EXPECT_EQ(command.plot_kind, "timeline");
    EXPECT_EQ(command.input_path, std::filesystem::path("results/script_smoke/exp1_unicast_correctness"));
    EXPECT_EQ(command.output_path, std::filesystem::path("timeline.svg"));
}

TEST(CliParseTest, ParsesPlotArgumentsWithMetricAndTitle) {
    const CLICommand command = parse_cli_command(
        {"plot", "--kind", "comparison", "--input", "results/script_smoke/aggregated/aggregated_summary.csv", "--metric", "summary::delivery_rate", "--x-key", "scenario_name", "--title", "Smoke Comparison"});

    EXPECT_EQ(command.kind, CLICommandKind::PLOT);
    EXPECT_EQ(command.metric, "summary::delivery_rate");
    EXPECT_EQ(command.x_key, "scenario_name");
    EXPECT_EQ(command.title, "Smoke Comparison");
}

TEST(CliParseTest, ParsesMultiTokenPlotTitle) {
    const CLICommand command = parse_cli_command(
        {"plot", "--kind", "comparison", "--input", "results/script_smoke/aggregated/aggregated_summary.csv", "--title", "Smoke", "Delivery", "Comparison"});

    EXPECT_EQ(command.title, "Smoke Delivery Comparison");
}

TEST(CliParseTest, RejectsUnknownCommand) {
    EXPECT_THROW(parse_cli_command({"unknown-command"}), CLIParseError);
}

TEST(CliParseTest, RejectsMissingRequiredOption) {
    EXPECT_THROW(parse_cli_command({"run-scenario"}), CLIParseError);
    EXPECT_THROW(parse_cli_command({"plot", "--input", "results"}), CLIParseError);
}