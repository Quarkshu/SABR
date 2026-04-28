#include "simulation/event.hpp"

const char* event_type_name(EventType type) noexcept {
    switch (type) {
    case EventType::BUNDLE_CREATED:
        return "BUNDLE_CREATED";
    case EventType::BUNDLE_ARRIVED:
        return "BUNDLE_ARRIVED";
    case EventType::BUNDLE_TX_START:
        return "BUNDLE_TX_START";
    case EventType::BUNDLE_TX_END:
        return "BUNDLE_TX_END";
    case EventType::BUNDLE_DELIVERED:
        return "BUNDLE_DELIVERED";
    case EventType::BUNDLE_EXPIRED:
        return "BUNDLE_EXPIRED";
    case EventType::DUPLICATE_DROPPED:
        return "DUPLICATE_DROPPED";
    case EventType::CONTACT_START:
        return "CONTACT_START";
    case EventType::CONTACT_END:
        return "CONTACT_END";
    case EventType::ROUTE_FAILED:
        return "ROUTE_FAILED";
    case EventType::PLAN_UPDATED:
        return "PLAN_UPDATED";
    }
    return "UNKNOWN";
}