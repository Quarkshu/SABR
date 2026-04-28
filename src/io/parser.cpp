#include "io/parser.hpp"

#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {

using json = nlohmann::json;

std::string lowercase_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

std::string trim_copy(std::string value) {
    const auto is_space = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    auto begin = std::find_if_not(value.begin(), value.end(), is_space);
    auto end = std::find_if_not(value.rbegin(), value.rend(), is_space).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::filesystem::path normalize_path(const std::filesystem::path& filepath) {
    if (filepath.is_absolute()) {
        return filepath.lexically_normal();
    }
    return std::filesystem::absolute(filepath).lexically_normal();
}

std::filesystem::path resolve_path(const std::filesystem::path& base_dir,
                                   const std::string& raw_path) {
    const std::filesystem::path path(raw_path);
    if (path.is_absolute()) {
        return path.lexically_normal();
    }
    return (base_dir / path).lexically_normal();
}

std::string read_text_file(const std::filesystem::path& filepath) {
    std::ifstream stream(filepath);
    if (!stream.is_open()) {
        throw std::runtime_error("failed to open file: " + filepath.string());
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

json read_json_file(const std::filesystem::path& filepath) {
    std::ifstream stream(filepath);
    if (!stream.is_open()) {
        throw std::runtime_error("failed to open json file: " + filepath.string());
    }

    json document;
    stream >> document;
    return document;
}

const json* find_first_member(const json& object,
                              std::initializer_list<const char*> keys) {
    if (!object.is_object()) {
        return nullptr;
    }

    for (const char* key : keys) {
        auto it = object.find(key);
        if (it != object.end()) {
            return &(*it);
        }
    }
    return nullptr;
}

const json& require_member(const json& object,
                           std::initializer_list<const char*> keys,
                           const std::string& context) {
    const json* value = find_first_member(object, keys);
    if (value == nullptr) {
        throw std::runtime_error("missing required field in " + context);
    }
    return *value;
}

template <typename T>
T require_number(const json& object,
                 std::initializer_list<const char*> keys,
                 const std::string& context) {
    const json& value = require_member(object, keys, context);
    if (!value.is_number()) {
        throw std::runtime_error("expected number in " + context);
    }
    return value.get<T>();
}

template <typename T>
T optional_number(const json& object,
                  std::initializer_list<const char*> keys,
                  T fallback) {
    const json* value = find_first_member(object, keys);
    if (value == nullptr) {
        return fallback;
    }
    if (!value->is_number()) {
        throw std::runtime_error("expected numeric optional field");
    }
    return value->get<T>();
}

bool optional_bool(const json& object,
                   std::initializer_list<const char*> keys,
                   bool fallback) {
    const json* value = find_first_member(object, keys);
    if (value == nullptr) {
        return fallback;
    }
    if (!value->is_boolean()) {
        throw std::runtime_error("expected boolean optional field");
    }
    return value->get<bool>();
}

std::string optional_string(const json& object,
                            std::initializer_list<const char*> keys,
                            std::string fallback = {}) {
    const json* value = find_first_member(object, keys);
    if (value == nullptr) {
        return fallback;
    }
    if (!value->is_string()) {
        throw std::runtime_error("expected string optional field");
    }
    return value->get<std::string>();
}

std::string require_string(const json& object,
                           std::initializer_list<const char*> keys,
                           const std::string& context) {
    const json& value = require_member(object, keys, context);
    if (!value.is_string()) {
        throw std::runtime_error("expected string in " + context);
    }
    return value.get<std::string>();
}

TimePoint parse_prefixed_time_token(const std::string& token,
                                    const std::string& context) {
    std::string normalized = token;
    if (!normalized.empty() && normalized.front() == '+') {
        normalized.erase(normalized.begin());
    }

    try {
        return std::stod(normalized);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid time token '" + token + "' in " + context);
    }
}

Priority parse_priority(const json& value) {
    if (value.is_number_integer()) {
        const int raw = value.get<int>();
        switch (raw) {
        case 0:
            return Priority::BULK;
        case 1:
            return Priority::NORMAL;
        case 2:
            return Priority::EXPEDITED;
        default:
            break;
        }
    }

    if (!value.is_string()) {
        throw std::runtime_error("priority must be integer or string");
    }

    const std::string normalized = lowercase_copy(value.get<std::string>());
    if (normalized == "bulk") {
        return Priority::BULK;
    }
    if (normalized == "normal") {
        return Priority::NORMAL;
    }
    if (normalized == "expedited") {
        return Priority::EXPEDITED;
    }

    throw std::runtime_error("unknown priority: " + value.get<std::string>());
}

TrafficGenerationMode parse_traffic_mode(const json& value) {
    if (!value.is_string()) {
        throw std::runtime_error("traffic mode must be string");
    }

    const std::string normalized = lowercase_copy(value.get<std::string>());
    if (normalized == "single") {
        return TrafficGenerationMode::SINGLE;
    }
    if (normalized == "batch") {
        return TrafficGenerationMode::BATCH;
    }
    if (normalized == "periodic") {
        return TrafficGenerationMode::PERIODIC;
    }

    throw std::runtime_error("unknown traffic mode: " + value.get<std::string>());
}

FailureRuleMode parse_failure_mode(const json& value) {
    if (!value.is_string()) {
        throw std::runtime_error("failure rule mode must be string");
    }

    const std::string normalized = lowercase_copy(value.get<std::string>());
    if (normalized == "contact_set") {
        return FailureRuleMode::CONTACT_SET;
    }
    if (normalized == "probabilistic_contacts") {
        return FailureRuleMode::PROBABILISTIC_CONTACTS;
    }
    if (normalized == "plan_replacement") {
        return FailureRuleMode::PLAN_REPLACEMENT;
    }

    throw std::runtime_error("unknown failure rule mode: " + value.get<std::string>());
}

RedundancyMode parse_redundancy_mode(const json& value) {
    if (!value.is_string()) {
        throw std::runtime_error("redundancy mode must be string");
    }

    const std::string normalized = lowercase_copy(value.get<std::string>());
    if (normalized == "none") {
        return RedundancyMode::NONE;
    }
    if (normalized == "single_backup") {
        return RedundancyMode::SINGLE_BACKUP;
    }
    if (normalized == "multi_backup") {
        return RedundancyMode::MULTI_BACKUP;
    }
    if (normalized == "trunk_only") {
        return RedundancyMode::TRUNK_ONLY;
    }

    throw std::runtime_error("unknown redundancy mode: " + value.get<std::string>());
}

Contact build_json_contact(const json& object,
                           ContactId& next_contact_id,
                           std::unordered_set<ContactId>& used_ids,
                           const std::string& context) {
    ContactId contact_id = 0;
    if (const json* value = find_first_member(object, {"contact_id", "id"})) {
        if (!value->is_number_unsigned() && !value->is_number_integer()) {
            throw std::runtime_error("contact_id must be numeric in " + context);
        }
        contact_id = value->get<ContactId>();
    } else {
        while (used_ids.count(next_contact_id) > 0) {
            ++next_contact_id;
        }
        contact_id = next_contact_id++;
    }

    if (!used_ids.insert(contact_id).second) {
        throw std::runtime_error("duplicate contact_id in " + context);
    }

    const TimePoint start_time = require_number<TimePoint>(object, {"start_time", "start"}, context);
    const TimePoint end_time = require_number<TimePoint>(object, {"end_time", "end"}, context);
    const NodeId sending_node = require_number<NodeId>(object, {"from_node", "sending_node"}, context);
    const NodeId receiving_node = require_number<NodeId>(object, {"to_node", "receiving_node"}, context);
    const Volume data_rate = require_number<Volume>(object, {"data_rate", "rate"}, context);

    if (end_time < start_time) {
        throw std::runtime_error("contact end_time precedes start_time in " + context);
    }

    return Contact{contact_id, start_time, end_time, sending_node, receiving_node, data_rate, 0, {}};
}

RangeInterval build_json_range(const json& object,
                               const std::string& context) {
    const TimePoint start_time = require_number<TimePoint>(object, {"start_time", "start"}, context);
    const TimePoint end_time = require_number<TimePoint>(object, {"end_time", "end"}, context);
    const NodeId node_a = require_number<NodeId>(object, {"node_a"}, context);
    const NodeId node_b = require_number<NodeId>(object, {"node_b"}, context);
    const TimePoint distance = require_number<TimePoint>(object, {"distance_light_seconds", "owlt", "distance"}, context);

    if (end_time < start_time) {
        throw std::runtime_error("range end_time precedes start_time in " + context);
    }

    return RangeInterval{start_time, end_time, node_a, node_b, distance};
}

std::shared_ptr<ContactPlan> build_contact_plan(ContactPlan::ContactList contacts,
                                                ContactPlan::RangeList ranges,
                                                const std::string& context) {
    if (contacts.empty()) {
        throw std::runtime_error("contact plan has no contacts in " + context);
    }

    auto contact_plan = std::make_shared<ContactPlan>();
    contact_plan->set_contacts(std::move(contacts));
    contact_plan->set_ranges(std::move(ranges));
    return contact_plan;
}

std::shared_ptr<ContactPlan> parse_contact_plan_json_value(const json& document,
                                                           const std::string& context) {
    const json* root = &document;
    if (const json* nested = find_first_member(document, {"contact_plan"})) {
        root = nested;
    }

    if (!root->is_object()) {
        throw std::runtime_error("contact plan json root must be object in " + context);
    }

    const json& contacts_json = require_member(*root, {"contacts"}, context);
    if (!contacts_json.is_array()) {
        throw std::runtime_error("contacts must be array in " + context);
    }

    ContactPlan::ContactList contacts;
    contacts.reserve(contacts_json.size());
    std::unordered_set<ContactId> used_ids;
    ContactId next_contact_id = 1;

    for (std::size_t index = 0; index < contacts_json.size(); ++index) {
        const json& item = contacts_json.at(index);
        if (!item.is_object()) {
            throw std::runtime_error("contact entry must be object in " + context);
        }
        contacts.push_back(build_json_contact(item, next_contact_id, used_ids,
            context + " contact[" + std::to_string(index) + "]"));
    }

    ContactPlan::RangeList ranges;
    if (const json* ranges_json = find_first_member(*root, {"ranges"})) {
        if (!ranges_json->is_array()) {
            throw std::runtime_error("ranges must be array in " + context);
        }
        ranges.reserve(ranges_json->size());
        for (std::size_t index = 0; index < ranges_json->size(); ++index) {
            const json& item = ranges_json->at(index);
            if (!item.is_object()) {
                throw std::runtime_error("range entry must be object in " + context);
            }
            ranges.push_back(build_json_range(item,
                context + " range[" + std::to_string(index) + "]"));
        }
    }

    return build_contact_plan(std::move(contacts), std::move(ranges), context);
}

EnhancementConfig parse_enhancement_config(const json& root) {
    EnhancementConfig config;
    const json* section = find_first_member(root, {"enhancements"});
    if (section == nullptr) {
        return config;
    }
    if (!section->is_object()) {
        throw std::runtime_error("enhancements must be object");
    }

    config.one_route_per_neighbor = optional_bool(*section, {"one_route_per_neighbor"}, config.one_route_per_neighbor);
    config.queue_delay = optional_bool(*section, {"queue_delay"}, config.queue_delay);
    config.anti_loop_reactive = optional_bool(*section, {"anti_loop_reactive"}, config.anti_loop_reactive);
    config.anti_loop_proactive = optional_bool(*section, {"anti_loop_proactive"}, config.anti_loop_proactive);
    return config;
}

RedundancyConfig parse_redundancy_config(const json& root) {
    RedundancyConfig config;
    const json* section = find_first_member(root, {"redundancy"});
    if (section == nullptr) {
        return config;
    }
    if (!section->is_object()) {
        throw std::runtime_error("redundancy must be object");
    }

    if (const json* mode = find_first_member(*section, {"mode"})) {
        config.mode = parse_redundancy_mode(*mode);
    }
    config.max_extra_copies = optional_number<std::size_t>(*section, {"max_extra_copies"}, config.max_extra_copies);
    config.risk_threshold = optional_number<double>(*section, {"risk_threshold"}, config.risk_threshold);
    config.min_diversity_score = optional_number<double>(*section, {"min_diversity_score"}, config.min_diversity_score);
    config.min_capacity_reserve_ratio = optional_number<double>(*section, {"min_capacity_reserve_ratio"}, config.min_capacity_reserve_ratio);
    config.min_ttl_slack = optional_number<double>(*section, {"min_ttl_slack"}, config.min_ttl_slack);
    config.enable_for_unicast = optional_bool(*section, {"enable_for_unicast"}, config.enable_for_unicast);
    config.enable_for_multicast_trunk = optional_bool(*section, {"enable_for_multicast_trunk"}, config.enable_for_multicast_trunk);
    return config;
}

std::vector<MulticastGroupConfig> parse_multicast_groups(const json& root) {
    std::vector<MulticastGroupConfig> groups;
    const json* section = find_first_member(root, {"multicast_groups"});
    if (section == nullptr) {
        return groups;
    }
    if (!section->is_array()) {
        throw std::runtime_error("multicast_groups must be array");
    }

    std::unordered_set<std::string> seen_ids;
    groups.reserve(section->size());
    for (std::size_t index = 0; index < section->size(); ++index) {
        const json& item = section->at(index);
        if (!item.is_object()) {
            throw std::runtime_error("multicast group entry must be object");
        }

        MulticastGroupConfig group;
        group.group_id = require_string(item, {"group_id"}, "multicast_groups[" + std::to_string(index) + "]");
        if (!seen_ids.insert(group.group_id).second) {
            throw std::runtime_error("duplicate multicast group id: " + group.group_id);
        }
        group.source_node = optional_number<NodeId>(item, {"source_node"}, -1);

        const json& members = require_member(item, {"member_nodes"}, "multicast_groups[" + std::to_string(index) + "]");
        if (!members.is_array()) {
            throw std::runtime_error("member_nodes must be array");
        }
        for (const json& member : members) {
            if (!member.is_number_integer()) {
                throw std::runtime_error("member_nodes entries must be integers");
            }
            group.member_nodes.push_back(member.get<NodeId>());
        }

        groups.push_back(std::move(group));
    }

    return groups;
}

void apply_engine_section(const json& root,
                          const EnhancementConfig& enhancements,
                          const RedundancyConfig& redundancy,
                          EngineConfig& engine) {
    engine.phase1_config.one_route_per_neighbor = enhancements.one_route_per_neighbor;
    engine.redundancy = redundancy;
    engine.phase2_config.queue_delay_enhancement = enhancements.queue_delay;
    engine.phase2_config.anti_loop_reactive = enhancements.anti_loop_reactive;
    engine.phase2_config.anti_loop_proactive = enhancements.anti_loop_proactive;

    const json* section = find_first_member(root, {"simulation", "engine"});
    if (section == nullptr) {
        section = &root;
    }
    if (!section->is_object()) {
        throw std::runtime_error("simulation section must be object");
    }

    engine.start_time = optional_number<TimePoint>(*section, {"start_time"}, engine.start_time);
    engine.end_time = optional_number<TimePoint>(*section, {"end_time"}, engine.end_time);
    engine.owlt_margin = optional_number<TimePoint>(*section, {"owlt_margin"}, engine.owlt_margin);
    engine.recompute_budget = optional_number<int>(*section, {"recompute_budget"}, engine.recompute_budget);
    engine.failure_seed = optional_number<std::uint32_t>(*section, {"failure_seed", "random_seed"}, engine.failure_seed);
    engine.phase1_config.owlt_margin = engine.owlt_margin;
    engine.phase2_config.owlt_margin = engine.owlt_margin;

    if (const json* phase1 = find_first_member(*section, {"phase1", "phase1_config"})) {
        if (!phase1->is_object()) {
            throw std::runtime_error("phase1 section must be object");
        }
        engine.phase1_config.k_paths = optional_number<int>(*phase1, {"k_paths"}, engine.phase1_config.k_paths);
        engine.phase1_config.owlt_margin = optional_number<TimePoint>(*phase1, {"owlt_margin"}, engine.phase1_config.owlt_margin);
        engine.phase1_config.one_route_per_neighbor = optional_bool(*phase1, {"one_route_per_neighbor"}, engine.phase1_config.one_route_per_neighbor);
    }

    if (const json* phase2 = find_first_member(*section, {"phase2", "phase2_config"})) {
        if (!phase2->is_object()) {
            throw std::runtime_error("phase2 section must be object");
        }
        engine.phase2_config.owlt_margin = optional_number<TimePoint>(*phase2, {"owlt_margin"}, engine.phase2_config.owlt_margin);
        engine.phase2_config.queue_delay_enhancement = optional_bool(*phase2, {"queue_delay_enhancement", "queue_delay"}, engine.phase2_config.queue_delay_enhancement);
        engine.phase2_config.anti_loop_reactive = optional_bool(*phase2, {"anti_loop_reactive"}, engine.phase2_config.anti_loop_reactive);
        engine.phase2_config.anti_loop_proactive = optional_bool(*phase2, {"anti_loop_proactive"}, engine.phase2_config.anti_loop_proactive);
    }
}

TrafficPattern parse_traffic_pattern(const json& item,
                                    const std::unordered_set<std::string>& known_groups,
                                    const std::string& context) {
    if (!item.is_object()) {
        throw std::runtime_error("traffic entry must be object in " + context);
    }

    TrafficPattern pattern;
    if (const json* mode = find_first_member(item, {"mode"})) {
        pattern.mode = parse_traffic_mode(*mode);
    }
    pattern.source_node = require_number<NodeId>(item, {"source_node", "source"}, context);
    pattern.start_time = optional_number<TimePoint>(item, {"start_time"}, pattern.start_time);
    pattern.bundle_count = optional_number<std::size_t>(item, {"bundle_count"}, pattern.bundle_count);
    pattern.period = optional_number<TimePoint>(item, {"period"}, pattern.period);
    pattern.payload_size = optional_number<Volume>(item, {"payload_size"}, pattern.payload_size);
    pattern.header_size = optional_number<Volume>(item, {"header_size"}, pattern.header_size);
    if (const json* priority = find_first_member(item, {"priority"})) {
        pattern.priority = parse_priority(*priority);
    }
    pattern.ttl = optional_number<TimePoint>(item, {"ttl"}, pattern.ttl);
    pattern.is_critical = optional_bool(item, {"is_critical"}, pattern.is_critical);
    pattern.allow_fragmentation = optional_bool(item, {"allow_fragmentation"}, pattern.allow_fragmentation);
    pattern.is_multicast = optional_bool(item, {"is_multicast"}, pattern.is_multicast);
    pattern.multicast_group_id = optional_string(item, {"multicast_group_id", "group_id", "destination_group"});

    if (!pattern.multicast_group_id.empty()) {
        pattern.is_multicast = true;
    }

    if (pattern.is_multicast) {
        if (pattern.multicast_group_id.empty()) {
            throw std::runtime_error("missing multicast_group_id in " + context);
        }
        if (known_groups.count(pattern.multicast_group_id) == 0) {
            throw std::runtime_error("unknown multicast group in " + context + ": " + pattern.multicast_group_id);
        }
        pattern.destination_node = optional_number<NodeId>(item, {"destination_node", "destination", "destination_eid"}, -1);
        return pattern;
    }

    if (const json* destination = find_first_member(item, {"destination_node", "destination", "destination_eid"})) {
        if (!destination->is_number_integer()) {
            throw std::runtime_error("destination must be integer in " + context);
        }
        pattern.destination_node = destination->get<NodeId>();
    } else {
        throw std::runtime_error("missing destination_node in " + context);
    }

    return pattern;
}

std::shared_ptr<ContactPlan> parse_replacement_plan(const json& item,
                                                    const std::filesystem::path& base_dir,
                                                    const std::string& context) {
    if (const json* replacement_file = find_first_member(item, {"replacement_plan_file"})) {
        if (!replacement_file->is_string()) {
            throw std::runtime_error("replacement_plan_file must be string in " + context);
        }
        return ConfigParser::parse_contact_plan_file(resolve_path(base_dir, replacement_file->get<std::string>()));
    }

    if (const json* replacement_plan = find_first_member(item, {"replacement_plan"})) {
        return parse_contact_plan_json_value(*replacement_plan, context + " replacement_plan");
    }

    return nullptr;
}

std::vector<FailureRule> parse_failure_rules(const json& root,
                                             const std::filesystem::path& base_dir) {
    const json* section = find_first_member(root, {"failures", "failure_injection"});
    if (section == nullptr) {
        return {};
    }
    if (!section->is_array()) {
        throw std::runtime_error("failures must be array");
    }

    std::vector<FailureRule> rules;
    rules.reserve(section->size());
    for (std::size_t index = 0; index < section->size(); ++index) {
        const json& item = section->at(index);
        if (!item.is_object()) {
            throw std::runtime_error("failure rule entry must be object");
        }

        FailureRule rule;
        rule.mode = parse_failure_mode(require_member(item, {"mode"}, "failure rule"));
        rule.trigger_time = require_number<TimePoint>(item, {"trigger_time"}, "failure rule");
        rule.label = optional_string(item, {"label"});

        if (const json* ids = find_first_member(item, {"contact_ids"})) {
            if (!ids->is_array()) {
                throw std::runtime_error("contact_ids must be array in failure rule");
            }
            for (const json& id : *ids) {
                if (!id.is_number_integer() && !id.is_number_unsigned()) {
                    throw std::runtime_error("contact_ids entries must be numeric");
                }
                rule.contact_ids.push_back(id.get<ContactId>());
            }
        }

        switch (rule.mode) {
        case FailureRuleMode::CONTACT_SET:
            if (rule.contact_ids.empty()) {
                throw std::runtime_error("contact_set failure rule requires contact_ids");
            }
            break;
        case FailureRuleMode::PROBABILISTIC_CONTACTS:
            rule.probability = optional_number<double>(item, {"probability"}, -1.0);
            if (rule.probability < 0.0 || rule.probability > 1.0) {
                throw std::runtime_error("probability must be within [0, 1]");
            }
            break;
        case FailureRuleMode::PLAN_REPLACEMENT:
            rule.replacement_plan = parse_replacement_plan(item, base_dir, "failure rule");
            if (rule.replacement_plan == nullptr) {
                throw std::runtime_error("plan_replacement failure rule requires replacement plan");
            }
            break;
        }

        rules.push_back(std::move(rule));
    }

    return rules;
}

ExperimentValue parse_experiment_value(const json& value) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer() || value.is_number_unsigned()) {
        return static_cast<std::int64_t>(value.get<std::int64_t>());
    }
    if (value.is_number_float()) {
        return value.get<double>();
    }
    if (value.is_string()) {
        return value.get<std::string>();
    }

    throw std::runtime_error("unsupported experiment matrix value type");
}

ExperimentMatrixConfig parse_experiment_matrix_json(const json& root,
                                                    const std::filesystem::path& base_dir) {
    const json* matrix = find_first_member(root, {"experiment_matrix"});
    const json* source = matrix == nullptr ? &root : matrix;
    if (!source->is_object()) {
        throw std::runtime_error("experiment matrix root must be object");
    }

    ExperimentMatrixConfig config;
    config.experiment_name = optional_string(*source, {"experiment_name", "name"});

    if (const json* scenarios = find_first_member(*source, {"scenario_files", "scenarios"})) {
        if (!scenarios->is_array()) {
            throw std::runtime_error("scenario_files must be array");
        }
        for (const json& item : *scenarios) {
            if (!item.is_string()) {
                throw std::runtime_error("scenario_files entries must be strings");
            }
            config.scenario_files.push_back(resolve_path(base_dir, item.get<std::string>()));
        }
    }

    if (const json* dimensions = find_first_member(*source, {"dimensions"})) {
        if (!dimensions->is_array()) {
            throw std::runtime_error("dimensions must be array");
        }

        for (std::size_t index = 0; index < dimensions->size(); ++index) {
            const json& item = dimensions->at(index);
            if (!item.is_object()) {
                throw std::runtime_error("dimension entry must be object");
            }

            ExperimentDimension dimension;
            dimension.name = require_string(item, {"name"}, "dimension[" + std::to_string(index) + "]");
            const json& values = require_member(item, {"values"}, "dimension[" + std::to_string(index) + "]");
            if (!values.is_array()) {
                throw std::runtime_error("dimension values must be array");
            }
            for (const json& value : values) {
                dimension.values.push_back(parse_experiment_value(value));
            }
            config.dimensions.push_back(std::move(dimension));
        }
    }

    return config;
}

}  // namespace

std::shared_ptr<ContactPlan> IONParser::parse_file(const std::filesystem::path& filepath) const {
    std::ifstream stream(filepath);
    if (!stream.is_open()) {
        throw std::runtime_error("failed to open ion contact plan file: " + filepath.string());
    }

    ContactPlan::ContactList contacts;
    ContactPlan::RangeList ranges;
    ContactId next_contact_id = 1;
    std::string line;
    std::size_t line_no = 0;

    while (std::getline(stream, line)) {
        ++line_no;

        const std::size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line.erase(comment_pos);
        }

        line = trim_copy(line);
        if (line.empty() || line.rfind("//", 0) == 0) {
            continue;
        }

        std::istringstream tokens(line);
        std::string action;
        std::string type;
        if (!(tokens >> action >> type)) {
            throw std::runtime_error("invalid ION line at " + filepath.string() + ":" + std::to_string(line_no));
        }

        if (action != "a") {
            throw std::runtime_error("unsupported ION action at " + filepath.string() + ":" + std::to_string(line_no));
        }

        if (type == "contact") {
            std::string start_token;
            std::string end_token;
            NodeId sending_node = 0;
            NodeId receiving_node = 0;
            Volume data_rate = 0.0;
            if (!(tokens >> start_token >> end_token >> sending_node >> receiving_node >> data_rate)) {
                throw std::runtime_error("invalid contact definition at " + filepath.string() + ":" + std::to_string(line_no));
            }

            contacts.push_back(Contact{
                next_contact_id++,
                parse_prefixed_time_token(start_token, "ION contact"),
                parse_prefixed_time_token(end_token, "ION contact"),
                sending_node,
                receiving_node,
                data_rate,
                0,
                {}});
            continue;
        }

        if (type == "range") {
            std::string start_token;
            std::string end_token;
            NodeId node_a = 0;
            NodeId node_b = 0;
            TimePoint distance = 0.0;
            if (!(tokens >> start_token >> end_token >> node_a >> node_b >> distance)) {
                throw std::runtime_error("invalid range definition at " + filepath.string() + ":" + std::to_string(line_no));
            }

            ranges.push_back(RangeInterval{
                parse_prefixed_time_token(start_token, "ION range"),
                parse_prefixed_time_token(end_token, "ION range"),
                node_a,
                node_b,
                distance});
            continue;
        }

        throw std::runtime_error("unsupported ION record type at " + filepath.string() + ":" + std::to_string(line_no));
    }

    return build_contact_plan(std::move(contacts), std::move(ranges), filepath.string());
}

std::shared_ptr<ContactPlan> JSONParser::parse_file(const std::filesystem::path& filepath) const {
    const json document = read_json_file(filepath);
    return parse_contact_plan_json_value(document, filepath.string());
}

std::shared_ptr<ContactPlan> ConfigParser::parse_contact_plan_file(const std::filesystem::path& filepath) {
    std::unique_ptr<ContactPlanParser> parser = ParserFactory::create(filepath);
    return parser->parse_file(filepath);
}

json ConfigParser::load_json_file(const std::filesystem::path& filepath) {
    return read_json_file(normalize_path(filepath));
}

ScenarioConfig ConfigParser::parse_scenario_document(const nlohmann::json& document,
                                                     const std::filesystem::path& source_path) {
    const std::filesystem::path normalized_path = normalize_path(source_path);
    const std::filesystem::path base_dir = normalized_path.parent_path();

    ScenarioConfig scenario;
    scenario.source_file = normalized_path;
    scenario.scenario_name = optional_string(document, {"scenario_name", "name"}, normalized_path.stem().string());
    if (const json* output = find_first_member(document, {"output_directory"})) {
        if (!output->is_string()) {
            throw std::runtime_error("output_directory must be string");
        }
        scenario.output_directory = resolve_path(base_dir, output->get<std::string>());
    }

    const bool has_inline_contact_plan = find_first_member(document, {"contact_plan"}) != nullptr;
    const bool has_contact_plan_file = find_first_member(document, {"contact_plan_file"}) != nullptr;
    if (has_inline_contact_plan == has_contact_plan_file) {
        throw std::runtime_error("scenario must provide exactly one of contact_plan or contact_plan_file");
    }

    if (has_inline_contact_plan) {
        scenario.contact_plan = parse_contact_plan_json_value(document.at("contact_plan"), normalized_path.string());
    } else {
        const std::string contact_plan_file = require_string(document, {"contact_plan_file"}, normalized_path.string());
        scenario.contact_plan = parse_contact_plan_file(resolve_path(base_dir, contact_plan_file));
    }

    scenario.enhancements = parse_enhancement_config(document);
    scenario.redundancy = parse_redundancy_config(document);
    scenario.multicast_groups = parse_multicast_groups(document);
    apply_engine_section(document, scenario.enhancements, scenario.redundancy, scenario.engine);
    scenario.engine.failure_rules = parse_failure_rules(document, base_dir);

    std::unordered_set<std::string> known_groups;
    for (const MulticastGroupConfig& group : scenario.multicast_groups) {
        known_groups.insert(group.group_id);
    }

    if (const json* traffic = find_first_member(document, {"traffic", "traffic_patterns"})) {
        if (!traffic->is_array()) {
            throw std::runtime_error("traffic must be array");
        }

        scenario.traffic_patterns.reserve(traffic->size());
        for (std::size_t index = 0; index < traffic->size(); ++index) {
            scenario.traffic_patterns.push_back(parse_traffic_pattern(
                traffic->at(index),
                known_groups,
                "traffic[" + std::to_string(index) + "]"));
        }
    }

    if (find_first_member(document, {"experiment_matrix"}) != nullptr) {
        scenario.experiment_matrix = parse_experiment_matrix_json(document, base_dir);
    }

    return scenario;
}

ScenarioConfig ConfigParser::parse_scenario_file(const std::filesystem::path& filepath) {
    const std::filesystem::path normalized_path = normalize_path(filepath);
    return parse_scenario_document(read_json_file(normalized_path), normalized_path);
}

ExperimentMatrixConfig ConfigParser::parse_matrix_file(const std::filesystem::path& filepath) {
    const std::filesystem::path normalized_path = normalize_path(filepath);
    const json document = read_json_file(normalized_path);
    ExperimentMatrixConfig config = parse_experiment_matrix_json(document, normalized_path.parent_path());
    if (config.experiment_name.empty()) {
        config.experiment_name = normalized_path.stem().string();
    }
    return config;
}

std::unique_ptr<ContactPlanParser> ParserFactory::create(const std::filesystem::path& filepath) {
    const std::string extension = lowercase_copy(filepath.extension().string());
    if (extension == ".json") {
        return std::make_unique<JSONParser>();
    }
    return std::make_unique<IONParser>();
}