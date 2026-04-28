#pragma once

#include "simulation/event.hpp"

#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

class DiscreteEventScheduler {
public:
    using EventHandler = std::function<void(const Event&)>;

    void schedule(const Event& event);
    void register_handler(EventType type, EventHandler handler);

    bool step();
    void run(TimePoint end_time);

    TimePoint current_time() const noexcept { return current_time_; }
    bool has_events() const noexcept { return !queue_.empty(); }
    std::size_t pending_count() const noexcept { return queue_.size(); }

private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> queue_;
    std::unordered_map<EventType, std::vector<EventHandler>, EventTypeHash> handlers_;
    TimePoint current_time_ = 0.0;
    EventId next_event_id_ = 1;
    std::uint64_t seq_counter_ = 0;
};