#pragma once

#include "models/contact_plan.hpp"
#include "models/bundle.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

using EventId = std::uint64_t;

enum class EventType {
    BUNDLE_CREATED,
    BUNDLE_ARRIVED,
    BUNDLE_TX_START,
    BUNDLE_TX_END,
    BUNDLE_DELIVERED,
    BUNDLE_EXPIRED,
    DUPLICATE_DROPPED,
    CONTACT_START,
    CONTACT_END,
    ROUTE_FAILED,
    PLAN_UPDATED,
};

const char* event_type_name(EventType type) noexcept;

struct BundleCreatedData {
    std::shared_ptr<Bundle> bundle;
    NodeId source_node = 0;
    std::string multicast_group_id;
};

struct BundleArrivedData {
    std::shared_ptr<Bundle> bundle;
    NodeId arrived_at = 0;
};

struct BundleTxStartData {
    std::shared_ptr<Bundle> bundle;
    ContactId contact_id = 0;
    PlanVersion plan_version = 0;
};

struct BundleTxEndData {
    std::shared_ptr<Bundle> bundle;
    ContactId contact_id = 0;
    PlanVersion plan_version = 0;
};

struct BundleDeliveredData {
    std::shared_ptr<Bundle> bundle;
    NodeId destination = 0;
};

struct BundleExpiredData {
    std::shared_ptr<Bundle> bundle;
};

struct DuplicateDroppedData {
    std::shared_ptr<Bundle> bundle;
    NodeId at_node = 0;
};

struct ContactStateData {
    ContactId contact_id = 0;
    PlanVersion plan_version = 0;
};

struct RouteFailedData {
    std::shared_ptr<Bundle> bundle;
    NodeId at_node = 0;
};

struct PlanUpdateData {
    TimePoint trigger_time = 0.0;
    std::vector<ContactId> removed_contact_ids;
    std::shared_ptr<ContactPlan> replacement_plan;
    std::string label;
};

using EventPayload = std::variant<
    BundleCreatedData,
    BundleArrivedData,
    BundleTxStartData,
    BundleTxEndData,
    BundleDeliveredData,
    BundleExpiredData,
    DuplicateDroppedData,
    ContactStateData,
    RouteFailedData,
    PlanUpdateData>;

struct Event {
    TimePoint timestamp = 0.0;
    EventId id = 0;
    std::uint64_t seq_no = 0;
    EventType type = EventType::BUNDLE_CREATED;
    EventPayload payload{};

    bool operator>(const Event& other) const noexcept {
        if (timestamp != other.timestamp) {
            return timestamp > other.timestamp;
        }
        return seq_no > other.seq_no;
    }
};

struct EventTypeHash {
    std::size_t operator()(EventType type) const noexcept {
        return static_cast<std::size_t>(type);
    }
};