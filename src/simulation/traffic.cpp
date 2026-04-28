#include "simulation/traffic.hpp"

#include <algorithm>
#include <utility>

TrafficGenerator::TrafficGenerator(std::vector<TrafficPattern> patterns)
    : patterns_(std::move(patterns)) {}

void TrafficGenerator::set_patterns(std::vector<TrafficPattern> patterns) {
    patterns_ = std::move(patterns);
}

std::vector<ScheduledBundle> TrafficGenerator::materialize(TimePoint window_start,
                                                           TimePoint window_end,
                                                           BundleId& next_bundle_id) const {
    std::vector<ScheduledBundle> scheduled;

    for (const TrafficPattern& pattern : patterns_) {
        auto schedule_one = [&](TimePoint timestamp) {
            if (timestamp < window_start || timestamp > window_end) {
                return;
            }
            ScheduledBundle request;
            request.timestamp = timestamp;
            request.source_node = pattern.source_node;
            request.bundle = make_bundle(pattern, next_bundle_id++, timestamp);
            request.multicast_group_id = pattern.multicast_group_id;
            scheduled.push_back(std::move(request));
        };

        switch (pattern.mode) {
        case TrafficGenerationMode::SINGLE:
            schedule_one(pattern.start_time);
            break;
        case TrafficGenerationMode::BATCH:
            for (std::size_t index = 0; index < pattern.bundle_count; ++index) {
                schedule_one(pattern.start_time);
            }
            break;
        case TrafficGenerationMode::PERIODIC:
            if (pattern.period <= 0.0) {
                schedule_one(pattern.start_time);
                break;
            }
            for (std::size_t index = 0; index < pattern.bundle_count; ++index) {
                schedule_one(pattern.start_time + static_cast<TimePoint>(index) * pattern.period);
            }
            break;
        }
    }

    std::sort(scheduled.begin(), scheduled.end(),
        [](const ScheduledBundle& lhs, const ScheduledBundle& rhs) {
            if (lhs.timestamp != rhs.timestamp) {
                return lhs.timestamp < rhs.timestamp;
            }
            if (lhs.source_node != rhs.source_node) {
                return lhs.source_node < rhs.source_node;
            }
            if (lhs.bundle == nullptr || rhs.bundle == nullptr) {
                return lhs.bundle != nullptr;
            }
            return lhs.bundle->id < rhs.bundle->id;
        });

    return scheduled;
}

std::shared_ptr<Bundle> TrafficGenerator::make_bundle(const TrafficPattern& pattern,
                                                      BundleId bundle_id,
                                                      TimePoint creation_time) const {
    auto bundle = std::make_shared<Bundle>();
    bundle->id = bundle_id;
    bundle->source_node = pattern.source_node;
    bundle->destination_eid = pattern.destination_node;
    bundle->creation_time = creation_time;
    bundle->ttl = pattern.ttl;
    bundle->payload_size = pattern.payload_size;
    bundle->header_size = pattern.header_size;
    bundle->priority = pattern.priority;
    bundle->is_critical = pattern.is_critical;
    bundle->is_multicast = pattern.is_multicast;
    bundle->allow_fragmentation = pattern.allow_fragmentation;
    return bundle;
}