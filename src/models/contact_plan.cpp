#include "models/contact_plan.hpp"

std::vector<Contact*> ContactPlan::get_contacts_from(NodeId node) {
    std::vector<Contact*> result;
    for (auto& c : contacts_) {
        if (c.sending_node == node) {
            result.push_back(&c);
        }
    }
    return result;
}

std::vector<const Contact*> ContactPlan::get_contacts_from(NodeId node) const {
    std::vector<const Contact*> result;
    for (const auto& c : contacts_) {
        if (c.sending_node == node) {
            result.push_back(&c);
        }
    }
    return result;
}

std::vector<Contact*> ContactPlan::get_contacts_to(NodeId node) {
    std::vector<Contact*> result;
    for (auto& c : contacts_) {
        if (c.receiving_node == node) {
            result.push_back(&c);
        }
    }
    return result;
}

std::vector<const Contact*> ContactPlan::get_contacts_to(NodeId node) const {
    std::vector<const Contact*> result;
    for (const auto& c : contacts_) {
        if (c.receiving_node == node) {
            result.push_back(&c);
        }
    }
    return result;
}

std::vector<Contact*> ContactPlan::get_contacts_between(NodeId from, NodeId to) {
    std::vector<Contact*> result;
    for (auto& c : contacts_) {
        if (c.sending_node == from && c.receiving_node == to) {
            result.push_back(&c);
        }
    }
    return result;
}

std::vector<const Contact*> ContactPlan::get_contacts_between(NodeId from, NodeId to) const {
    std::vector<const Contact*> result;
    for (const auto& c : contacts_) {
        if (c.sending_node == from && c.receiving_node == to) {
            result.push_back(&c);
        }
    }
    return result;
}

Contact* ContactPlan::get_contact(ContactId contact_id) {
    for (auto& c : contacts_) {
        if (c.contact_id == contact_id) {
            return &c;
        }
    }
    return nullptr;
}

const Contact* ContactPlan::get_contact(ContactId contact_id) const {
    for (const auto& c : contacts_) {
        if (c.contact_id == contact_id) {
            return &c;
        }
    }
    return nullptr;
}

// 在给定时间点查找适用的 RangeInterval，返回 OWLT
// 若无匹配则返回 0.0（默认无延迟）
TimePoint ContactPlan::get_owlt(NodeId from, NodeId to, TimePoint time) const {
    for (const auto& r : ranges_) {
        if (r.involves(from, to) && r.start_time <= time && time < r.end_time) {
            return r.distance_light_seconds;
        }
    }
    return 0.0;
}

void ContactPlan::set_contacts(ContactList contacts) {
    contacts_ = std::move(contacts);
    for (auto& contact : contacts_) {
        contact.initialize_mtv();
    }
    bump_version();
}

void ContactPlan::set_ranges(RangeList ranges) {
    ranges_ = std::move(ranges);
    bump_version();
}

// DS-CP-02/03: 添加 Contact 并递增版本
void ContactPlan::add_contact(Contact contact) {
    contact.initialize_mtv();
    contacts_.push_back(std::move(contact));
    bump_version();
}

void ContactPlan::add_range(const RangeInterval& range) {
    ranges_.push_back(range);
    bump_version();
}

// DS-CP-02/03: 按 id 移除 Contact 并递增版本
void ContactPlan::remove_contact(ContactId contact_id) {
    auto it = std::remove_if(contacts_.begin(), contacts_.end(),
        [contact_id](const Contact& c) { return c.contact_id == contact_id; });
    if (it != contacts_.end()) {
        contacts_.erase(it, contacts_.end());
        bump_version();
    }
}

// 移除所有已终止的 Contact
void ContactPlan::purge_terminated(TimePoint current_time) {
    auto it = std::remove_if(contacts_.begin(), contacts_.end(),
        [current_time](const Contact& c) { return c.is_terminated(current_time); });
    if (it != contacts_.end()) {
        contacts_.erase(it, contacts_.end());
        bump_version();
    }
}

void ContactPlan::bump_version() {
    ++version_;
    rebind_contacts();
}

void ContactPlan::rebind_contacts() {
    for (auto& contact : contacts_) {
        contact.bind_plan_version(version_);
    }
}
