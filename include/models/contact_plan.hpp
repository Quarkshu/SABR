#pragma once

#include "models/contact.hpp"
#include <algorithm>
#include <vector>

// DS-CP-01 ~ DS-CP-04: 接触计划
class ContactPlan {
public:
    using ContactList = std::vector<Contact>;
    using RangeList = std::vector<RangeInterval>;

    PlanVersion version() const noexcept { return version_; }

    const ContactList& contacts() const noexcept { return contacts_; }
    const RangeList& ranges() const noexcept { return ranges_; }

    // 查询接口
    std::vector<Contact*> get_contacts_from(NodeId node);
    std::vector<const Contact*> get_contacts_from(NodeId node) const;
    std::vector<Contact*> get_contacts_to(NodeId node);
    std::vector<const Contact*> get_contacts_to(NodeId node) const;
    std::vector<Contact*> get_contacts_between(NodeId from, NodeId to);
    std::vector<const Contact*> get_contacts_between(NodeId from, NodeId to) const;
    Contact* get_contact(ContactId contact_id);
    const Contact* get_contact(ContactId contact_id) const;
    TimePoint get_owlt(NodeId from, NodeId to, TimePoint time) const;

    // DS-CP-01 / DS-CP-03: 批量替换接触计划内容
    void set_contacts(ContactList contacts);
    void set_ranges(RangeList ranges);

    // DS-CP-02 / DS-CP-03: 动态修改
    void add_contact(Contact contact);
    void add_range(const RangeInterval& range);
    void remove_contact(ContactId contact_id);

    // 移除已终止的 Contact
    void purge_terminated(TimePoint current_time);

private:
    void bump_version();
    void rebind_contacts();

    PlanVersion version_ = 0;
    ContactList contacts_;
    RangeList ranges_;
};
