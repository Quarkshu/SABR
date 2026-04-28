#pragma once

#include "io/parser.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct ExperimentCoordinate {
    std::string name;
    ExperimentValue value;
};

struct ExperimentRunManifest {
    std::string run_id;
    std::string experiment_name;
    std::string scenario_name;
    std::filesystem::path scenario_file;
    std::filesystem::path output_directory;
    std::vector<ExperimentCoordinate> coordinates;
};

struct ExpandedExperimentRun {
    ScenarioConfig scenario;
    ExperimentRunManifest manifest;
};

struct ScenarioRunArtifacts {
    ExperimentRunManifest manifest;
    std::filesystem::path manifest_path;
    std::filesystem::path stats_path;
    std::filesystem::path bundle_summary_path;
    std::filesystem::path contact_utilization_path;
    bool success = false;
    std::string error_message;
};

class ExperimentRunner {
public:
    static ExpandedExperimentRun build_run(const std::filesystem::path& scenario_file,
                                           const std::vector<ExperimentCoordinate>& coordinates = {},
                                           const std::string& experiment_name = {},
                                           std::size_t run_index = 0,
                                           const std::filesystem::path& output_root_override = {});

    static std::vector<ExpandedExperimentRun> expand_matrix_file(
        const std::filesystem::path& matrix_file,
        const std::filesystem::path& output_root_override = {});

    static ScenarioRunArtifacts run_scenario(const ExpandedExperimentRun& run);

    static std::vector<ScenarioRunArtifacts> run_experiment(
        const std::filesystem::path& matrix_file,
        const std::filesystem::path& output_root_override = {},
        std::size_t max_runs = 0);
};