#include "models/contact.hpp"

// DS-CON-02: volume = (end_time - start_time) * data_rate
Volume Contact::volume() const {
    return (end_time - start_time) * data_rate;
}

// DS-CON-05: Contact 终止判定
bool Contact::is_terminated(TimePoint current_time) const {
    return end_time <= current_time;
}

// DS-CON-03: 消耗 MTV——对指定优先级及所有更低优先级减去 evc
void Contact::consume_mtv(int priority, Volume evc) {
    for (int p = priority; p >= 0; --p) {
        mtv[p] -= evc;
    }
}

// DS-CON-03: 初始化 MTV 为 volume()
void Contact::initialize_mtv() {
    Volume vol = volume();
    mtv.fill(vol);
}

void Contact::bind_plan_version(PlanVersion version) noexcept {
    plan_version = version;
}

// DS-RNG-02: 双向性——查询 (n1, n2) 或 (n2, n1) 均匹配
bool RangeInterval::involves(NodeId n1, NodeId n2) const {
    return (node_a == n1 && node_b == n2) ||
           (node_a == n2 && node_b == n1);
}

// DS-RNG-02: 返回单向光延迟（双向相同）
TimePoint RangeInterval::get_owlt(NodeId from, NodeId to) const {
    (void)from;
    (void)to;
    return distance_light_seconds;
}
