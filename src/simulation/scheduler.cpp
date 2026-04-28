#include "simulation/scheduler.hpp"

#include <algorithm>

void DiscreteEventScheduler::schedule(const Event& event) {
    Event scheduled = event;
    if (scheduled.timestamp < current_time_) {
        scheduled.timestamp = current_time_;
    }

    if (scheduled.id == 0) {
        scheduled.id = next_event_id_++;
    } else {
        next_event_id_ = std::max(next_event_id_, scheduled.id + 1);
    }
    scheduled.seq_no = seq_counter_++;
    queue_.push(std::move(scheduled));
}

void DiscreteEventScheduler::register_handler(EventType type, EventHandler handler) {
    handlers_[type].push_back(std::move(handler));
}

bool DiscreteEventScheduler::step() {
    if (queue_.empty()) {
        return false;
    }

    const Event event = queue_.top();
    queue_.pop();
    current_time_ = event.timestamp;

    auto it = handlers_.find(event.type);
    if (it != handlers_.end()) {
        for (const EventHandler& handler : it->second) {
            handler(event);
        }
    }

    return true;
}

void DiscreteEventScheduler::run(TimePoint end_time) {
    while (!queue_.empty() && queue_.top().timestamp <= end_time) {
        step();
    }
}