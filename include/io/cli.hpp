#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

enum class CLICommandKind {
    VERSION,
    HELP,
    RUN_SCENARIO,
    RUN_EXPERIMENT,
    EXPORT,
    PLOT,
};

class CLIParseError : public std::runtime_error {
public:
    explicit CLIParseError(const std::string& message)
        : std::runtime_error(message) {}
};

struct CLICommand {
    CLICommandKind kind = CLICommandKind::HELP;
    std::filesystem::path config_path;
    std::filesystem::path matrix_path;
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::string format;
    std::string plot_kind;
    std::string metric;
    std::string row_key;
    std::string column_key;
    std::string title;
    std::string x_key;
    std::string series_key;
    std::size_t limit = 0;
};

std::string build_usage_text();
CLICommand parse_cli_command(const std::vector<std::string>& args);