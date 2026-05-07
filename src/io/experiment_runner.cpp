#include "io/experiment_runner.hpp"

#include "algorithms/multicast.hpp"

#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {

using json = nlohmann::json;

struct PathToken {
    std::string key;
    bool has_index = false;
    bool wildcard_index = false;
    std::size_t index = 0;
};

std::filesystem::path normalize_path(const std::filesystem::path& filepath) {
    if (filepath.empty()) {
        return {};
    }
    if (filepath.is_absolute()) {
        return filepath.lexically_normal();
    }
    return std::filesystem::absolute(filepath).lexically_normal();
}

json experiment_value_to_json(const ExperimentValue& value) {
    return std::visit([](const auto& current) { return json(current); }, value);
}

std::string sanitize_component(std::string value) {
    for (char& ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!std::isalnum(uch) && ch != '_' && ch != '-') {
            ch = '_';
        }
    }
    return value;
}

std::string format_run_id(std::size_t run_index) {
    std::ostringstream stream;
    stream << "matrix_" << std::setw(4) << std::setfill('0') << run_index;
    return stream.str();
}

std::string current_timestamp_utc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t current_time = std::chrono::system_clock::to_time_t(now);
    std::tm utc_time{};
#ifdef _WIN32
    gmtime_s(&utc_time, &current_time);
#else
    gmtime_r(&current_time, &utc_time);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::vector<PathToken> parse_path_tokens(const std::string& path) {
    std::vector<PathToken> tokens;
    std::size_t cursor = 0;
    while (cursor < path.size()) {
        const std::size_t key_end = path.find_first_of(".[", cursor);
        if (key_end == cursor) {
            throw std::runtime_error("invalid experiment path: " + path);
        }

        PathToken token;
        token.key = path.substr(cursor, key_end - cursor);
        cursor = key_end;

        if (cursor != std::string::npos && cursor < path.size() && path[cursor] == '[') {
            const std::size_t close = path.find(']', cursor);
            if (close == std::string::npos) {
                throw std::runtime_error("unterminated path index: " + path);
            }
            token.has_index = true;
            const std::string index_text = path.substr(cursor + 1, close - cursor - 1);
            if (index_text == "*") {
                token.wildcard_index = true;
            } else {
                token.index = static_cast<std::size_t>(std::stoull(index_text));
            }
            cursor = close + 1;
        }

        if (cursor != std::string::npos && cursor < path.size() && path[cursor] == '.') {
            ++cursor;
        }

        tokens.push_back(std::move(token));
    }

    if (tokens.empty()) {
        throw std::runtime_error("empty experiment path");
    }
    return tokens;
}

json* ensure_member(json* cursor,
                    const PathToken& token,
                    const std::string& full_path,
                    bool allow_missing_last,
                    bool is_last) {
    if (!cursor->is_object()) {
        throw std::runtime_error("expected object while applying path: " + full_path);
    }

    auto member = cursor->find(token.key);
    if (member == cursor->end()) {
        if (allow_missing_last && is_last) {
            member = cursor->insert(cursor->end(), {token.key, json{}});
        } else {
            throw std::runtime_error("missing key '" + token.key + "' while applying path: " + full_path);
        }
    }

    return &(*member);
}

void assign_json_path_recursive(json* cursor,
                                const std::vector<PathToken>& tokens,
                                std::size_t token_index,
                                const ExperimentValue& value,
                                const std::string& full_path) {
    const PathToken& token = tokens[token_index];
    const bool is_last = token_index + 1 == tokens.size();
    json* next = ensure_member(cursor, token, full_path, true, is_last);

    if (!token.has_index) {
        if (is_last) {
            *next = experiment_value_to_json(value);
            return;
        }
        assign_json_path_recursive(next, tokens, token_index + 1, value, full_path);
        return;
    }

    if (!next->is_array()) {
        throw std::runtime_error("expected array at '" + token.key + "' while applying path: " + full_path);
    }

    if (token.wildcard_index) {
        if (next->empty()) {
            throw std::runtime_error("array is empty while applying path: " + full_path);
        }
        for (json& item : *next) {
            if (is_last) {
                item = experiment_value_to_json(value);
            } else {
                assign_json_path_recursive(&item, tokens, token_index + 1, value, full_path);
            }
        }
        return;
    }

    if (token.index >= next->size()) {
        throw std::runtime_error("array index out of range while applying path: " + full_path);
    }

    json* indexed = &next->at(token.index);
    if (is_last) {
        *indexed = experiment_value_to_json(value);
        return;
    }

    assign_json_path_recursive(indexed, tokens, token_index + 1, value, full_path);
}

void assign_json_path(json& document,
                      const std::vector<PathToken>& tokens,
                      const ExperimentValue& value,
                      const std::string& full_path) {
    assign_json_path_recursive(&document, tokens, 0, value, full_path);
}

void apply_multicast_group_size(json& document,
                                const ExperimentValue& value,
                                const std::string& full_path) {
    if (!std::holds_alternative<std::int64_t>(value)) {
        throw std::runtime_error("multicast.group_size requires integer value");
    }

    const std::int64_t requested_raw = std::get<std::int64_t>(value);
    if (requested_raw <= 0) {
        throw std::runtime_error("multicast.group_size must be positive");
    }
    const std::size_t requested = static_cast<std::size_t>(requested_raw);

    if (document.contains("multicast_groups")) {
        json& groups = document.at("multicast_groups");
        if (!groups.is_array() || groups.empty()) {
            throw std::runtime_error("multicast_groups must be a non-empty array for path: " + full_path);
        }
        json& members = groups.at(0).at("member_nodes");
        if (!members.is_array() || requested > members.size()) {
            throw std::runtime_error("multicast.group_size exceeds member_nodes size");
        }
        members.erase(members.begin() + static_cast<json::difference_type>(requested), members.end());
    }

    if (document.contains("traffic")) {
        json& traffic = document.at("traffic");
        if (!traffic.is_array() || requested > traffic.size()) {
            if (!document.contains("multicast_groups")) {
                throw std::runtime_error("multicast.group_size exceeds traffic size");
            }
            return;
        }

        bool has_only_unicast_entries = true;
        for (const json& item : traffic) {
            if (item.contains("multicast_group_id")) {
                has_only_unicast_entries = false;
                break;
            }
        }
        if (has_only_unicast_entries) {
            traffic.erase(traffic.begin() + static_cast<json::difference_type>(requested), traffic.end());
        }
    }
}

std::vector<std::string> first_segment_aliases(const std::string& first_segment) {
    if (first_segment == "failure_injection") {
        return {"failure_injection", "failures"};
    }
    if (first_segment == "failures") {
        return {"failures", "failure_injection"};
    }
    return {first_segment};
}

void apply_coordinate_override(json& document,
                               const ExperimentCoordinate& coordinate) {
    if (coordinate.name == "multicast.group_size") {
        apply_multicast_group_size(document, coordinate.value, coordinate.name);
        return;
    }

    const std::vector<PathToken> original_tokens = parse_path_tokens(coordinate.name);
    std::runtime_error last_error("failed to apply coordinate: " + coordinate.name);
    bool applied = false;

    for (const std::string& alias : first_segment_aliases(original_tokens.front().key)) {
        std::vector<PathToken> tokens = original_tokens;
        tokens.front().key = alias;
        try {
            assign_json_path(document, tokens, coordinate.value, coordinate.name);
            applied = true;
            break;
        } catch (const std::runtime_error& error) {
            last_error = error;
        }
    }

    if (!applied) {
        throw last_error;
    }
}

std::vector<MulticastGroup> to_registry_groups(const ScenarioConfig& scenario) {
    std::vector<MulticastGroup> groups;
    groups.reserve(scenario.multicast_groups.size());
    for (const MulticastGroupConfig& config : scenario.multicast_groups) {
        groups.push_back({
            config.group_id,
            config.source_node,
            std::set<NodeId>(config.member_nodes.begin(), config.member_nodes.end())});
    }
    return groups;
}

json build_manifest_json(const ScenarioRunArtifacts& artifacts,
                         const std::string& started_at,
                         const std::string& completed_at) {
    json coordinates = json::array();
    for (const ExperimentCoordinate& coordinate : artifacts.manifest.coordinates) {
        coordinates.push_back({
            {"name", coordinate.name},
            {"value", experiment_value_to_json(coordinate.value)},
        });
    }

    return {
        {"run_id", artifacts.manifest.run_id},
        {"experiment_name", artifacts.manifest.experiment_name},
        {"scenario_name", artifacts.manifest.scenario_name},
        {"scenario_file", artifacts.manifest.scenario_file.string()},
        {"output_directory", artifacts.manifest.output_directory.string()},
        {"started_at", started_at},
        {"completed_at", completed_at},
        {"success", artifacts.success},
        {"error_message", artifacts.error_message},
        {"matrix_coordinates", std::move(coordinates)},
        {"generated_files", {
            {"manifest", artifacts.manifest_path.string()},
            {"stats", artifacts.stats_path.string()},
            {"bundle_summary", artifacts.bundle_summary_path.string()},
            {"contact_utilization", artifacts.contact_utilization_path.string()},
        }},
    };
}

void write_manifest_file(const ScenarioRunArtifacts& artifacts,
                         const std::string& started_at,
                         const std::string& completed_at) {
    std::filesystem::create_directories(artifacts.manifest.output_directory);
    std::ofstream stream(artifacts.manifest_path);
    stream << build_manifest_json(artifacts, started_at, completed_at).dump(2);
}

std::filesystem::path default_output_directory(const ScenarioConfig& scenario) {
    if (!scenario.output_directory.empty()) {
        return scenario.output_directory;
    }
    return normalize_path(std::filesystem::current_path() / "results" / sanitize_component(scenario.scenario_name));
}

}  // namespace

ExpandedExperimentRun ExperimentRunner::build_run(const std::filesystem::path& scenario_file,
                                                  const std::vector<ExperimentCoordinate>& coordinates,
                                                  const std::string& experiment_name,
                                                  std::size_t run_index,
                                                  const std::filesystem::path& output_root_override) {
    const std::filesystem::path normalized_scenario = normalize_path(scenario_file);
    json scenario_document = ConfigParser::load_json_file(normalized_scenario);
    for (const ExperimentCoordinate& coordinate : coordinates) {
        apply_coordinate_override(scenario_document, coordinate);
    }

    ScenarioConfig scenario = ConfigParser::parse_scenario_document(scenario_document, normalized_scenario);
    const std::string base_scenario_name = scenario.scenario_name;

    std::filesystem::path output_directory;
    if (output_root_override.empty()) {
        output_directory = default_output_directory(scenario);
    } else if (coordinates.empty() && run_index == 0) {
        output_directory = normalize_path(output_root_override);
    } else {
        output_directory = normalize_path(output_root_override);
        if (!experiment_name.empty()) {
            output_directory /= sanitize_component(experiment_name);
        }
        output_directory /= sanitize_component(base_scenario_name);
    }

    std::string run_id = "scenario";
    if (!coordinates.empty() || run_index > 0) {
        run_id = format_run_id(run_index == 0 ? 1 : run_index);
        output_directory /= run_id;
        scenario.scenario_name = base_scenario_name + "__" + run_id;
    }

    scenario.output_directory = output_directory.lexically_normal();

    ExpandedExperimentRun run;
    run.scenario = std::move(scenario);
    run.manifest.run_id = std::move(run_id);
    run.manifest.experiment_name = experiment_name;
    run.manifest.scenario_name = run.scenario.scenario_name;
    run.manifest.scenario_file = normalized_scenario;
    run.manifest.output_directory = run.scenario.output_directory;
    run.manifest.coordinates = coordinates;
    return run;
}

std::vector<ExpandedExperimentRun> ExperimentRunner::expand_matrix_file(
    const std::filesystem::path& matrix_file,
    const std::filesystem::path& output_root_override) {
    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_file);
    if (matrix.scenario_files.empty()) {
        throw std::runtime_error("experiment matrix has no scenario_files: " + normalize_path(matrix_file).string());
    }

    std::vector<ExpandedExperimentRun> runs;
    std::size_t next_run_index = 1;

    for (const std::filesystem::path& scenario_file : matrix.scenario_files) {
        std::vector<ExperimentCoordinate> coordinates;
        const auto expand_dimensions = [&](const auto& self, std::size_t dimension_index) -> void {
            if (dimension_index >= matrix.dimensions.size()) {
                runs.push_back(build_run(scenario_file,
                                         coordinates,
                                         matrix.experiment_name,
                                         next_run_index++,
                                         output_root_override));
                return;
            }

            const ExperimentDimension& dimension = matrix.dimensions[dimension_index];
            for (const ExperimentValue& value : dimension.values) {
                coordinates.push_back({dimension.name, value});
                self(self, dimension_index + 1);
                coordinates.pop_back();
            }
        };

        expand_dimensions(expand_dimensions, 0);
    }

    return runs;
}

ScenarioRunArtifacts ExperimentRunner::run_scenario(const ExpandedExperimentRun& run) {
    ScenarioRunArtifacts artifacts;
    artifacts.manifest = run.manifest;
    artifacts.stats_path = run.manifest.output_directory / "stats.json";
    artifacts.bundle_summary_path = run.manifest.output_directory / "bundle_summary.csv";
    artifacts.contact_utilization_path = run.manifest.output_directory / "contact_utilization.csv";
    artifacts.manifest_path = run.manifest.output_directory / "run_manifest.json";

    const std::string started_at = current_timestamp_utc();
    try {
        std::filesystem::create_directories(run.manifest.output_directory);

        SimEngine engine(run.scenario.contact_plan,
                         run.scenario.traffic_patterns,
                         run.scenario.engine);
        engine.set_scenario_name(run.scenario.scenario_name);
        if (!run.scenario.multicast_groups.empty()) {
            engine.set_multicast_groups(to_registry_groups(run.scenario));
        }

        engine.initialize();
        engine.run();

        engine.stats().write_json(artifacts.stats_path);
        engine.stats().write_bundle_summary_csv(artifacts.bundle_summary_path);
        engine.stats().write_contact_utilization_csv(artifacts.contact_utilization_path);
        artifacts.success = true;
    } catch (const std::exception& error) {
        artifacts.error_message = error.what();
        artifacts.success = false;
    }

    write_manifest_file(artifacts, started_at, current_timestamp_utc());
    return artifacts;
}

std::vector<ScenarioRunArtifacts> ExperimentRunner::run_experiment(
    const std::filesystem::path& matrix_file,
    const std::filesystem::path& output_root_override,
    std::size_t max_runs) {
    std::vector<ExpandedExperimentRun> runs = expand_matrix_file(matrix_file, output_root_override);
    if (max_runs > 0 && runs.size() > max_runs) {
        runs.resize(max_runs);
    }

    std::vector<ScenarioRunArtifacts> artifacts;
    artifacts.reserve(runs.size());
    for (const ExpandedExperimentRun& run : runs) {
        artifacts.push_back(run_scenario(run));
    }
    return artifacts;
}