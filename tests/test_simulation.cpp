#include <gtest/gtest.h>

#include "simulation/engine.hpp"
#include "simulation/event.hpp"
#include "simulation/runtime_context.hpp"
#include "simulation/scheduler.hpp"
#include "simulation/traffic.hpp"

#include <memory>
#include <string>
#include <utility>
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

ContactPlan make_three_node_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 20.0, 1, 2, 200.0),
        make_contact(2, 2.0, 30.0, 2, 3, 200.0),
        make_contact(3, 20.0, 40.0, 1, 3, 200.0),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 3, 1.0),
        make_range(0.0, 100.0, 1, 3, 1.0),
    });
    return plan;
}

}  // namespace

TEST(EventTest, EventTypeNameMatchesEnumValue) {
    EXPECT_STREQ(event_type_name(EventType::BUNDLE_CREATED), "BUNDLE_CREATED");
    EXPECT_STREQ(event_type_name(EventType::CONTACT_END), "CONTACT_END");
}

TEST(DiscreteEventSchedulerTest, ProcessesEventsInTimestampOrder) {
    DiscreteEventScheduler scheduler;
    std::vector<ContactId> processed;

    scheduler.register_handler(
        EventType::CONTACT_START,
        [&processed](const Event& event) {
            processed.push_back(std::get<ContactStateData>(event.payload).contact_id);
        });

    scheduler.schedule({3.0, 0, 0, EventType::CONTACT_START, ContactStateData{3}});
    scheduler.schedule({1.0, 0, 0, EventType::CONTACT_START, ContactStateData{1}});
    scheduler.schedule({2.0, 0, 0, EventType::CONTACT_START, ContactStateData{2}});

    scheduler.run(5.0);

    EXPECT_EQ(processed, (std::vector<ContactId>{1, 2, 3}));
    EXPECT_DOUBLE_EQ(scheduler.current_time(), 3.0);
}

TEST(DiscreteEventSchedulerTest, PreservesInsertionOrderForSameTimestamp) {
    DiscreteEventScheduler scheduler;
    std::vector<ContactId> processed;

    scheduler.register_handler(
        EventType::CONTACT_END,
        [&processed](const Event& event) {
            processed.push_back(std::get<ContactStateData>(event.payload).contact_id);
        });

    scheduler.schedule({5.0, 0, 0, EventType::CONTACT_END, ContactStateData{11}});
    scheduler.schedule({5.0, 0, 0, EventType::CONTACT_END, ContactStateData{12}});
    scheduler.schedule({5.0, 0, 0, EventType::CONTACT_END, ContactStateData{13}});

    scheduler.run(5.0);

    EXPECT_EQ(processed, (std::vector<ContactId>{11, 12, 13}));
}

TEST(DiscreteEventSchedulerTest, RunStopsBeforeFutureEvents) {
    DiscreteEventScheduler scheduler;
    std::vector<ContactId> processed;

    scheduler.register_handler(
        EventType::CONTACT_START,
        [&processed](const Event& event) {
            processed.push_back(std::get<ContactStateData>(event.payload).contact_id);
        });

    scheduler.schedule({1.0, 0, 0, EventType::CONTACT_START, ContactStateData{21}});
    scheduler.schedule({4.0, 0, 0, EventType::CONTACT_START, ContactStateData{22}});

    scheduler.run(2.0);

    EXPECT_EQ(processed, (std::vector<ContactId>{21}));
    EXPECT_TRUE(scheduler.has_events());
    EXPECT_EQ(scheduler.pending_count(), 1u);
    EXPECT_DOUBLE_EQ(scheduler.current_time(), 1.0);
}

TEST(RuntimeContextTest, BindsContactPlanAndCreatesNodeRuntime) {
    ContactPlan plan;
    Node node_a(1);
    Node node_b(2);

    RuntimeContext context;
    context.add_node(node_a);
    context.bind_contact_plan(plan);
    NodeRuntime& runtime_b = context.add_node(node_b);

    ASSERT_TRUE(context.has_node(1));
    ASSERT_TRUE(context.has_node(2));
    EXPECT_EQ(node_a.contact_plan, &plan);
    EXPECT_EQ(node_b.contact_plan, &plan);
    ASSERT_NE(runtime_b.bpa, nullptr);
    EXPECT_TRUE(runtime_b.has_contact_plan());
    EXPECT_EQ(context.node_ids(), (std::vector<NodeId>{1, 2}));
}

TEST(RuntimeContextTest, EmitsLogAndStatHooks) {
    RuntimeContext context;
    std::vector<std::string> logs;
    std::vector<std::pair<std::string, double>> stats;

    context.set_log_hook([&logs](const std::string& message) {
        logs.push_back(message);
    });
    context.set_stat_hook([&stats](const std::string& key, double value) {
        stats.emplace_back(key, value);
    });

    context.emit_log("bundle-arrived");
    context.emit_stat("delivered", 2.0);

    ASSERT_EQ(logs.size(), 1u);
    EXPECT_EQ(logs.front(), "bundle-arrived");
    ASSERT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats.front().first, "delivered");
    EXPECT_DOUBLE_EQ(stats.front().second, 2.0);
}

TEST(BundleProtocolAgentTest, TracksPendingQueuedAndDeliveredBundles) {
    Node node(7);
    BundleProtocolAgent agent(node);

    auto created = std::make_shared<Bundle>();
    auto arrived = std::make_shared<Bundle>();
    created->priority = Priority::NORMAL;
    arrived->priority = Priority::EXPEDITED;

    agent.on_bundle_created(created);
    agent.on_bundle_arrived(arrived);
    agent.enqueue_for_contact(created, 101, Priority::NORMAL);
    agent.enqueue_for_contact(arrived, 101, Priority::EXPEDITED);
    agent.deliver_local(created);

    EXPECT_EQ(agent.node_id(), 7);
    EXPECT_EQ(agent.pending_bundle_count(), 2u);
    EXPECT_EQ(agent.queued_for_contact(101), 2u);
    EXPECT_EQ(agent.delivered_bundle_count(), 1u);
    EXPECT_EQ(agent.pending_bundles().front(), created);
    EXPECT_EQ(agent.delivered_bundles().front(), created);
}

TEST(TrafficGeneratorTest, MaterializesSingleBatchAndPeriodicPatterns) {
    TrafficGenerator generator({
        {TrafficGenerationMode::SINGLE, 1, 3, 1.0, 1, 0.0, 10.0, 5.0, Priority::NORMAL, 20.0, false, true},
        {TrafficGenerationMode::BATCH, 1, 4, 2.0, 2, 0.0, 20.0, 5.0, Priority::BULK, 30.0, false, true},
        {TrafficGenerationMode::PERIODIC, 2, 5, 3.0, 3, 2.0, 30.0, 5.0, Priority::EXPEDITED, 40.0, true, false},
    });

    BundleId next_bundle_id = 1;
    const std::vector<ScheduledBundle> scheduled = generator.materialize(0.0, 10.0, next_bundle_id);

    ASSERT_EQ(scheduled.size(), 6u);
    EXPECT_EQ(next_bundle_id, 7u);

    EXPECT_DOUBLE_EQ(scheduled[0].timestamp, 1.0);
    EXPECT_EQ(scheduled[0].bundle->destination_eid, 3);
    EXPECT_EQ(scheduled[0].bundle->priority, Priority::NORMAL);

    EXPECT_DOUBLE_EQ(scheduled[1].timestamp, 2.0);
    EXPECT_DOUBLE_EQ(scheduled[2].timestamp, 2.0);
    EXPECT_EQ(scheduled[1].bundle->destination_eid, 4);
    EXPECT_EQ(scheduled[2].bundle->destination_eid, 4);

    EXPECT_DOUBLE_EQ(scheduled[3].timestamp, 3.0);
    EXPECT_DOUBLE_EQ(scheduled[4].timestamp, 5.0);
    EXPECT_DOUBLE_EQ(scheduled[5].timestamp, 7.0);
    EXPECT_TRUE(scheduled[3].bundle->is_critical);
    EXPECT_FALSE(scheduled[3].bundle->allow_fragmentation);
}

TEST(SimEngineTest, ProcessesContactBoundaryEvents) {
    auto contact_plan = std::make_shared<ContactPlan>(make_three_node_plan());
    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 50.0;

    SimEngine engine(contact_plan, {}, config);
    engine.initialize();
    engine.run();

    EXPECT_EQ(engine.metrics().contact_start_events, 3u);
    EXPECT_EQ(engine.metrics().contact_end_events, 3u);
}

TEST(SimEngineTest, DeliversFiveBundlesAcrossThreeNodeScenario) {
    auto contact_plan = std::make_shared<ContactPlan>(make_three_node_plan());

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 50.0;
    config.owlt_margin = 0.5;

    std::vector<TrafficPattern> traffic_patterns = {
        {TrafficGenerationMode::BATCH, 1, 3, 0.0, 4, 0.0, 0.0, 0.0, Priority::BULK, 60.0, false, true},
        {TrafficGenerationMode::SINGLE, 1, 3, 0.0, 1, 0.0, 0.0, 0.0, Priority::EXPEDITED, 60.0, false, true},
    };

    SimEngine engine(contact_plan, traffic_patterns, config);
    engine.initialize();
    engine.run();

    const EngineMetrics& metrics = engine.metrics();
    EXPECT_EQ(metrics.bundles_created, 5u);
    EXPECT_EQ(metrics.bundles_delivered, 5u);
    EXPECT_EQ(metrics.bundles_expired, 0u);
    EXPECT_EQ(metrics.route_failures, 0u);
    EXPECT_EQ(metrics.tx_started, 10u);
    EXPECT_EQ(metrics.tx_completed, 10u);
    EXPECT_EQ(metrics.routing_invocations, 10u);
    EXPECT_EQ(metrics.reroute_invocations, 5u);

    for (BundleId bundle_id = 1; bundle_id <= 5; ++bundle_id) {
        ASSERT_EQ(metrics.route_attempts.count(bundle_id), 1u);
        EXPECT_EQ(metrics.route_attempts.at(bundle_id), 2u);
        EXPECT_EQ(engine.bundle_state(bundle_id), BundleLifecycleState::DELIVERED);

        const std::shared_ptr<Bundle> bundle = engine.find_bundle(bundle_id);
        ASSERT_NE(bundle, nullptr);
        EXPECT_EQ(bundle->route_path, (std::vector<NodeId>{1, 2, 3}));
        EXPECT_GT(bundle->delivered_time, 0.0);
    }

    const std::shared_ptr<Bundle> first_bulk = engine.find_bundle(1);
    const std::shared_ptr<Bundle> expedited = engine.find_bundle(5);
    ASSERT_NE(first_bulk, nullptr);
    ASSERT_NE(expedited, nullptr);
    EXPECT_LT(expedited->delivered_time, first_bulk->delivered_time);
}