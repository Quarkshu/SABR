#include "io/cli.hpp"

#include <sstream>

namespace {

std::string find_option_value(const std::vector<std::string>& args,
                              const std::string& option) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == option) {
            return args[index + 1];
        }
    }
    return {};
}

std::string find_option_value_joined(const std::vector<std::string>& args,
                                     const std::string& option) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] != option) {
            continue;
        }

        std::string value = args[index + 1];
        for (std::size_t cursor = index + 2; cursor < args.size(); ++cursor) {
            if (args[cursor].rfind("--", 0) == 0) {
                break;
            }
            value += ' ';
            value += args[cursor];
        }
        return value;
    }
    return {};
}

std::string require_option_value(const std::vector<std::string>& args,
                                 const std::string& option,
                                 const std::string& command_name) {
    const std::string value = find_option_value(args, option);
    if (value.empty()) {
        throw CLIParseError("missing required option '" + option + "' for command '" + command_name + "'");
    }
    return value;
}

std::size_t parse_limit_option(const std::vector<std::string>& args,
                               const std::string& command_name) {
    const std::string raw_limit = find_option_value(args, "--limit");
    if (raw_limit.empty()) {
        return 0;
    }

    try {
        return static_cast<std::size_t>(std::stoull(raw_limit));
    } catch (const std::exception&) {
        throw CLIParseError("invalid numeric value for '--limit' in command '" + command_name + "': " + raw_limit);
    }
}

CLICommand parse_run_scenario(const std::vector<std::string>& args) {
    CLICommand command;
    command.kind = CLICommandKind::RUN_SCENARIO;
    command.config_path = require_option_value(args, "--config", "run-scenario");
    const std::string output = find_option_value(args, "--output");
    if (!output.empty()) {
        command.output_path = output;
    }
    return command;
}

CLICommand parse_run_experiment(const std::vector<std::string>& args) {
    CLICommand command;
    command.kind = CLICommandKind::RUN_EXPERIMENT;
    command.matrix_path = require_option_value(args, "--matrix", "run-experiment");
    command.limit = parse_limit_option(args, "run-experiment");
    const std::string output = find_option_value(args, "--output");
    if (!output.empty()) {
        command.output_path = output;
    }
    return command;
}

CLICommand parse_export(const std::vector<std::string>& args) {
    CLICommand command;
    command.kind = CLICommandKind::EXPORT;
    command.input_path = require_option_value(args, "--input", "export");
    command.format = find_option_value(args, "--format");
    if (command.format.empty()) {
        command.format = "flat";
    }
    command.metric = find_option_value(args, "--metric");
    command.row_key = find_option_value(args, "--row-key");
    command.column_key = find_option_value(args, "--column-key");
    const std::string output = find_option_value(args, "--output");
    if (!output.empty()) {
        command.output_path = output;
    }
    return command;
}

CLICommand parse_plot(const std::vector<std::string>& args) {
    CLICommand command;
    command.kind = CLICommandKind::PLOT;
    command.input_path = require_option_value(args, "--input", "plot");
    command.plot_kind = require_option_value(args, "--kind", "plot");
    command.metric = find_option_value(args, "--metric");
    command.title = find_option_value_joined(args, "--title");
    command.x_key = find_option_value(args, "--x-key");
    command.series_key = find_option_value(args, "--series-key");
    const std::string output = find_option_value(args, "--output");
    if (!output.empty()) {
        command.output_path = output;
    }
    return command;
}

}  // namespace

std::string build_usage_text() {
    std::ostringstream stream;
    stream << "sabr v1.0.0\n"
           << "Usage:\n"
           << "  sabr run-scenario --config <scenario.json> [--output <dir>]\n"
           << "  sabr run-experiment --matrix <matrix.json> [--output <dir>] [--limit <count>]\n"
            << "  sabr export --input <results_dir> [--output <dir>] [--format flat|pivot|both] [--metric <name>] [--row-key <field>] [--column-key <field>]\n"
            << "  sabr plot --kind <topology|timeline|utilization|comparison> --input <path> [--output <file>] [--metric <name>] [--title <text>] [--x-key <field>] [--series-key <field>]\n"
           << "  sabr help\n"
           << "  sabr --version\n"
           << "\n"
           << "Commands:\n"
           << "  run-scenario    Execute a single scenario config and write result artifacts.\n"
           << "  run-experiment  Expand and execute an experiment matrix sequentially.\n"
            << "  export          Generate aggregated flat summaries and optional pivot CSV exports.\n"
            << "  plot            Produce SVG plots from contact plans, run outputs, or aggregated results.\n"
            << "\n"
            << "Plot kinds:\n"
            << "  topology        Visualize the contact topology from a scenario or contact plan.\n"
            << "  timeline        Plot bundle lifecycle timelines from stats.json or a run directory.\n"
            << "  utilization     Plot contact utilization bars from a run directory or CSV.\n"
            << "  comparison      Plot experiment or matrix comparisons from aggregated results.\n";
    return stream.str();
}

CLICommand parse_cli_command(const std::vector<std::string>& args) {
    if (args.empty()) {
        return CLICommand{CLICommandKind::VERSION};
    }
    if (args[0] == "--help" || args[0] == "help") {
        return CLICommand{CLICommandKind::HELP};
    }
    if (args[0] == "--version") {
        return CLICommand{CLICommandKind::VERSION};
    }
    if (args[0] == "run-scenario") {
        return parse_run_scenario(args);
    }
    if (args[0] == "run-experiment") {
        return parse_run_experiment(args);
    }
    if (args[0] == "export") {
        return parse_export(args);
    }
    if (args[0] == "plot") {
        return parse_plot(args);
    }

    throw CLIParseError("unknown command: " + args[0]);
}