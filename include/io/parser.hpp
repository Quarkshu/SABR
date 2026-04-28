#pragma once

#include "algorithms/redundancy.hpp"
#include "json.hpp"
#include "simulation/engine.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

struct EnhancementConfig {
    bool one_route_per_neighbor = false;
    bool queue_delay = false;
    bool anti_loop_reactive = false;
    bool anti_loop_proactive = false;
};

struct MulticastGroupConfig {
    std::string group_id;
    NodeId source_node = -1;
    std::vector<NodeId> member_nodes;
};

using ExperimentValue = std::variant<bool, std::int64_t, double, std::string>;

struct ExperimentDimension {
    std::string name;
    std::vector<ExperimentValue> values;
};

struct ExperimentMatrixConfig {
    std::string experiment_name;
    std::vector<std::filesystem::path> scenario_files;
    std::vector<ExperimentDimension> dimensions;
};

struct ScenarioConfig {
    std::string scenario_name;
    std::filesystem::path source_file;
    std::filesystem::path output_directory;
    std::shared_ptr<ContactPlan> contact_plan;
    EngineConfig engine;
    std::vector<TrafficPattern> traffic_patterns;
    std::vector<MulticastGroupConfig> multicast_groups;
    EnhancementConfig enhancements;
    RedundancyConfig redundancy;
    ExperimentMatrixConfig experiment_matrix;
};

class ContactPlanParser {
public:
    virtual ~ContactPlanParser() = default;

    virtual std::shared_ptr<ContactPlan> parse_file(const std::filesystem::path& filepath) const = 0;
};

class IONParser : public ContactPlanParser {
public:
    std::shared_ptr<ContactPlan> parse_file(const std::filesystem::path& filepath) const override;
};

class JSONParser : public ContactPlanParser {
public:
    std::shared_ptr<ContactPlan> parse_file(const std::filesystem::path& filepath) const override;
};

class ConfigParser {
public:
    static nlohmann::json load_json_file(const std::filesystem::path& filepath);
    static std::shared_ptr<ContactPlan> parse_contact_plan_file(const std::filesystem::path& filepath);
    static ScenarioConfig parse_scenario_document(const nlohmann::json& document,
                                                  const std::filesystem::path& source_path);
    static ScenarioConfig parse_scenario_file(const std::filesystem::path& filepath);
    static ExperimentMatrixConfig parse_matrix_file(const std::filesystem::path& filepath);
};

class ParserFactory {
public:
    static std::unique_ptr<ContactPlanParser> create(const std::filesystem::path& filepath);
};