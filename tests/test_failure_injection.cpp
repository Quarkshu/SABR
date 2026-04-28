#include <gtest/gtest.h>

#include "simulation/engine.hpp"
#include "simulation/failure_injector.hpp"

#include <memory>
#include <sstream>
#include <vector>

namespace {

Contact make_contact(ContactId id,
                     TimePoint start_time,
                     TimePoint end_time,
                     NodeId from,
                     NodeId to,
                     Volume data_rate) {
    return Contact{id, start_time, end_time, from, to, data_rate, 0, {}};
}

RangeInterval make_range(TimePoint start_time,
                         TimePoint end_time,
                         NodeId node_a,
                         NodeId node_b,
                         TimePoint distance_light_seconds) {
    return RangeInterval{start_time, end_time, node_a, node_b, distance_light_seconds};
}

ContactPlan make_many_contact_plan() {
    ContactPlan plan;
    ContactPlan::ContactList contacts;
    contacts.reserve(20);
    for (ContactId contact_id = 1; contact_id <= 20; ++contact_id) {
        contacts.push_back(make_contact(contact_id, 0.0, 50.0, 1, 2, 100.0));
    }
    plan.set_contacts(std::move(contacts));
    plan.set_ranges({make_range(0.0, 50.0, 1, 2, 1.0)});
    return plan;
}

ContactPlan make_failure_reroute_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 5.0, 15.0, 1, 2, 500.0),
        make_contact(2, 16.0, 30.0, 2, 3, 500.0),
        make_contact(3, 20.0, 40.0, 1, 3, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 50.0, 1, 2, 1.0),
        make_range(0.0, 50.0, 2, 3, 1.0),
        make_range(0.0, 50.0, 1, 3, 1.0),
    });
    return plan;
}

ContactPlan make_replacement_base_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 5.0, 15.0, 1, 2, 500.0),
        make_contact(2, 16.0, 30.0, 2, 3, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 50.0, 1, 2, 1.0),
        make_range(0.0, 50.0, 2, 3, 1.0),
    });
    return plan;
}

ContactPlan make_replacement_target_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(10, 12.0, 35.0, 1, 3, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 50.0, 1, 3, 1.0),
    });
    return plan;
}

ContactPlan make_first_hop_commit_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2, 500.0),
        make_contact(2, 20.0, 30.0, 2, 3, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 50.0, 1, 2, 1.0),
        make_range(0.0, 50.0, 2, 3, 1.0),
    });
    return plan;
}

TrafficPattern make_single_bundle_pattern() {
    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 1;
    pattern.destination_node = 3;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 60.0;
    pattern.header_size = 0.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 100.0;
    pattern.is_critical = false;
    pattern.allow_fragmentation = true;
    return pattern;
}

std::string join_event_log(const std::vector<std::string>& event_log) {
    std::ostringstream stream;
    for (const std::string& message : event_log) {
        stream << message << '\n';
    }
    return stream.str();
}

}  // namespace

TEST(FailureInjectorTest, MaterializesDeterministicallyForFixedSeed) {
    ContactPlan plan = make_many_contact_plan();
    auto replacement_plan = std::make_shared<ContactPlan>(make_replacement_target_plan());

    FailureRule explicit_rule;
    explicit_rule.mode = FailureRuleMode::CONTACT_SET;
    explicit_rule.trigger_time = 2.0;
    explicit_rule.contact_ids = {3, 1, 3};
    explicit_rule.label = "explicit";

    FailureRule probabilistic_rule;
    probabilistic_rule.mode = FailureRuleMode::PROBABILISTIC_CONTACTS;
    probabilistic_rule.trigger_time = 4.0;
    probabilistic_rule.contact_ids = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                      11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    probabilistic_rule.probability = 0.5;
    probabilistic_rule.label = "prob";

    FailureRule replacement_rule;
    replacement_rule.mode = FailureRuleMode::PLAN_REPLACEMENT;
    replacement_rule.trigger_time = 6.0;
    replacement_rule.replacement_plan = replacement_plan;
    replacement_rule.label = "replace";

    FailureInjector injector_a(1234, {explicit_rule, probabilistic_rule, replacement_rule});
    FailureInjector injector_b(1234, {explicit_rule, probabilistic_rule, replacement_rule});
    FailureInjector injector_c(5678, {explicit_rule, probabilistic_rule, replacement_rule});

    const std::vector<PlanUpdateData> updates_a = injector_a.materialize(plan, 0.0, 10.0);
    const std::vector<PlanUpdateData> updates_b = injector_b.materialize(plan, 0.0, 10.0);
    const std::vector<PlanUpdateData> updates_c = injector_c.materialize(plan, 0.0, 10.0);

    ASSERT_EQ(updates_a.size(), 3u);
    ASSERT_EQ(updates_b.size(), 3u);
    ASSERT_EQ(updates_c.size(), 3u);

    EXPECT_EQ(updates_a[0].removed_contact_ids, (std::vector<ContactId>{1, 3}));
    EXPECT_EQ(updates_a[0].removed_contact_ids, updates_b[0].removed_contact_ids);
    EXPECT_EQ(updates_a[1].removed_contact_ids, updates_b[1].removed_contact_ids);
    EXPECT_NE(updates_a[1].removed_contact_ids, updates_c[1].removed_contact_ids);
    EXPECT_EQ(updates_a[2].replacement_plan, replacement_plan);
}

TEST(SimEngineFailureInjectionTest, ExplicitContactRemovalReroutesQueuedBundle) {
    auto contact_plan = std::make_shared<ContactPlan>(make_failure_reroute_plan());
    FailureRule rule;
    rule.mode = FailureRuleMode::CONTACT_SET;
    rule.trigger_time = 1.0;
    rule.contact_ids = {1};
    rule.label = "remove_first_hop";

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 50.0;
    config.failure_rules = {rule};

    SimEngine engine(contact_plan, {make_single_bundle_pattern()}, config);
    engine.initialize();
    engine.run();

    ASSERT_EQ(engine.bundle_state(1), BundleLifecycleState::DELIVERED)
        << join_event_log(engine.event_log());
    ASSERT_NE(engine.find_bundle(1), nullptr);
    EXPECT_EQ(engine.find_bundle(1)->route_path, (std::vector<NodeId>{1, 3}));

    EXPECT_EQ(engine.metrics().plan_update_events, 1u);
    EXPECT_EQ(engine.metrics().queued_bundle_replans, 1u);
    ASSERT_EQ(engine.metrics().route_attempts.count(1), 1u);
    EXPECT_EQ(engine.metrics().route_attempts.at(1), 2u);
    EXPECT_EQ(engine.delivered_bundle_ids(), (std::vector<BundleId>{1}));
    EXPECT_EQ(contact_plan->get_contact(1), nullptr);
    EXPECT_GT(engine.metrics().stale_contact_events_ignored, 0u);
}

TEST(SimEngineFailureInjectionTest, PlanReplacementReroutesAndIgnoresStaleEvents) {
    auto contact_plan = std::make_shared<ContactPlan>(make_replacement_base_plan());
    auto replacement_plan = std::make_shared<ContactPlan>(make_replacement_target_plan());

    FailureRule rule;
    rule.mode = FailureRuleMode::PLAN_REPLACEMENT;
    rule.trigger_time = 1.0;
    rule.replacement_plan = replacement_plan;
    rule.label = "swap_plan";

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 50.0;
    config.failure_rules = {rule};

    SimEngine engine(contact_plan, {make_single_bundle_pattern()}, config);
    engine.initialize();
    engine.run();

    ASSERT_EQ(engine.bundle_state(1), BundleLifecycleState::DELIVERED);
    ASSERT_NE(engine.runtime().contact_plan(), nullptr);
    EXPECT_EQ(engine.runtime().contact_plan(), replacement_plan.get());
    ASSERT_EQ(engine.metrics().route_attempts.count(1), 1u);
    EXPECT_EQ(engine.metrics().route_attempts.at(1), 2u);
    EXPECT_EQ(engine.metrics().plan_update_events, 1u);
    EXPECT_EQ(engine.metrics().queued_bundle_replans, 1u);
    EXPECT_GT(engine.metrics().stale_contact_events_ignored, 0u);
    EXPECT_EQ(engine.find_bundle(1)->route_path, (std::vector<NodeId>{1, 3}));
}

TEST(SimEngineFailureInjectionTest, CommitsOnlyFirstHopCapacityBeforeNextHopRouting) {
    auto contact_plan = std::make_shared<ContactPlan>(make_first_hop_commit_plan());

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 0.5;

    SimEngine engine(contact_plan, {make_single_bundle_pattern()}, config);
    engine.initialize();
    engine.run();

    const std::shared_ptr<Bundle> bundle = engine.find_bundle(1);
    ASSERT_NE(bundle, nullptr);

    const Contact* first_contact = contact_plan->get_contact(1);
    const Contact* second_contact = contact_plan->get_contact(2);
    ASSERT_NE(first_contact, nullptr);
    ASSERT_NE(second_contact, nullptr);

    EXPECT_DOUBLE_EQ(first_contact->mtv[0], first_contact->volume() - bundle->evc());
    EXPECT_DOUBLE_EQ(first_contact->mtv[1], first_contact->volume() - bundle->evc());
    EXPECT_DOUBLE_EQ(first_contact->mtv[2], first_contact->volume());
    EXPECT_DOUBLE_EQ(second_contact->mtv[0], second_contact->volume());
    EXPECT_DOUBLE_EQ(second_contact->mtv[1], second_contact->volume());
    EXPECT_DOUBLE_EQ(second_contact->mtv[2], second_contact->volume());
    EXPECT_EQ(engine.metrics().bundles_forwarded, 1u);
    EXPECT_EQ(engine.bundle_state(1), BundleLifecycleState::ACTIVE);
}