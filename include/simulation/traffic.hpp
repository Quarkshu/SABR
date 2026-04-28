#pragma once

#include "models/bundle.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class TrafficGenerationMode {
    SINGLE,
    BATCH,
    PERIODIC,
};

struct TrafficPattern {
    TrafficGenerationMode mode = TrafficGenerationMode::SINGLE;
    NodeId source_node = 0;
    NodeId destination_node = -1;
    TimePoint start_time = 0.0;
    std::size_t bundle_count = 1;
    TimePoint period = 0.0;
    Volume payload_size = 0.0;
    Volume header_size = 0.0;
    Priority priority = Priority::BULK;
    TimePoint ttl = 0.0;
    bool is_critical = false;
    bool allow_fragmentation = true;
    bool is_multicast = false;
    std::string multicast_group_id;
};

struct ScheduledBundle {
    TimePoint timestamp = 0.0;
    NodeId source_node = 0;
    std::shared_ptr<Bundle> bundle;
    std::string multicast_group_id;
};

class TrafficGenerator {
public:
    explicit TrafficGenerator(std::vector<TrafficPattern> patterns = {});

    void set_patterns(std::vector<TrafficPattern> patterns);
    const std::vector<TrafficPattern>& patterns() const noexcept { return patterns_; }

    std::vector<ScheduledBundle> materialize(TimePoint window_start,
                                             TimePoint window_end,
                                             BundleId& next_bundle_id) const;

private:
    std::shared_ptr<Bundle> make_bundle(const TrafficPattern& pattern,
                                        BundleId bundle_id,
                                        TimePoint creation_time) const;

    std::vector<TrafficPattern> patterns_;
};