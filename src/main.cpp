#include "io/cli.hpp"
#include "io/experiment_runner.hpp"
#include "io/script_bridge.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

int run_scenario_command(const CLICommand& command) {
    const ExpandedExperimentRun run = ExperimentRunner::build_run(
        command.config_path,
        {},
        {},
        0,
        command.output_path);
    const ScenarioRunArtifacts artifacts = ExperimentRunner::run_scenario(run);

    if (!artifacts.success) {
        std::cerr << "run-scenario failed: " << artifacts.error_message << std::endl;
        std::cerr << "manifest: " << artifacts.manifest_path << std::endl;
        return 1;
    }

    std::cout << "scenario: " << artifacts.manifest.scenario_name << std::endl;
    std::cout << "output: " << artifacts.manifest.output_directory << std::endl;
    std::cout << "stats: " << artifacts.stats_path << std::endl;
    std::cout << "manifest: " << artifacts.manifest_path << std::endl;
    return 0;
}

int run_experiment_command(const CLICommand& command) {
    const std::vector<ScenarioRunArtifacts> artifacts = ExperimentRunner::run_experiment(
        command.matrix_path,
        command.output_path,
        command.limit);

    std::size_t failed_runs = 0;
    for (const ScenarioRunArtifacts& artifact : artifacts) {
        if (!artifact.success) {
            ++failed_runs;
            std::cerr << "run-experiment failed: " << artifact.manifest.scenario_name
                      << " -> " << artifact.error_message << std::endl;
        }
    }

    std::cout << "runs: " << artifacts.size() << std::endl;
    std::cout << "failed_runs: " << failed_runs << std::endl;
    return failed_runs == 0 ? 0 : 1;
}

std::vector<std::string> build_export_arguments(const CLICommand& command) {
    std::vector<std::string> arguments = {
        "--input",
        command.input_path.string(),
        "--format",
        command.format.empty() ? std::string("flat") : command.format,
    };
    if (!command.output_path.empty()) {
        arguments.push_back("--output");
        arguments.push_back(command.output_path.string());
    }
    if (!command.metric.empty()) {
        arguments.push_back("--metric");
        arguments.push_back(command.metric);
    }
    if (!command.row_key.empty()) {
        arguments.push_back("--row-key");
        arguments.push_back(command.row_key);
    }
    if (!command.column_key.empty()) {
        arguments.push_back("--column-key");
        arguments.push_back(command.column_key);
    }
    return arguments;
}

int export_command(const CLICommand& command) {
    const std::filesystem::path workspace_root = find_workspace_root(std::filesystem::current_path());
    return run_python_script(workspace_root,
                             "aggregate_results.py",
                             build_export_arguments(command));
}

std::string plot_script_name(const std::string& kind) {
    if (kind == "topology") {
        return "plot_topology.py";
    }
    if (kind == "timeline") {
        return "plot_timeline.py";
    }
    if (kind == "utilization") {
        return "plot_utilization.py";
    }
    if (kind == "comparison") {
        return "plot_comparison.py";
    }
    throw CLIParseError("unsupported plot kind: " + kind);
}

std::vector<std::string> build_plot_arguments(const CLICommand& command) {
    std::vector<std::string> arguments = {
        "--input",
        command.input_path.string(),
    };
    if (!command.output_path.empty()) {
        arguments.push_back("--output");
        arguments.push_back(command.output_path.string());
    }
    if (!command.metric.empty()) {
        arguments.push_back("--metric");
        arguments.push_back(command.metric);
    }
    if (!command.title.empty()) {
        arguments.push_back("--title");
        arguments.push_back(command.title);
    }
    if (!command.x_key.empty()) {
        arguments.push_back("--x-key");
        arguments.push_back(command.x_key);
    }
    if (!command.series_key.empty()) {
        arguments.push_back("--series-key");
        arguments.push_back(command.series_key);
    }
    return arguments;
}

int plot_command(const CLICommand& command) {
    const std::filesystem::path workspace_root = find_workspace_root(std::filesystem::current_path());
    return run_python_script(workspace_root,
                             plot_script_name(command.plot_kind),
                             build_plot_arguments(command));
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        const CLICommand command = parse_cli_command(args);

        if (command.kind == CLICommandKind::VERSION) {
            std::cout << "sabr v1.0.0" << std::endl;
            return 0;
        }
        if (command.kind == CLICommandKind::HELP) {
            std::cout << build_usage_text();
            return 0;
        }
        if (command.kind == CLICommandKind::RUN_SCENARIO) {
            return run_scenario_command(command);
        }
        if (command.kind == CLICommandKind::RUN_EXPERIMENT) {
            return run_experiment_command(command);
        }
        if (command.kind == CLICommandKind::EXPORT) {
            return export_command(command);
        }
        if (command.kind == CLICommandKind::PLOT) {
            return plot_command(command);
        }

        std::cerr << build_usage_text();
        return 1;
    } catch (const CLIParseError& error) {
        std::cerr << error.what() << std::endl;
        std::cerr << build_usage_text();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
