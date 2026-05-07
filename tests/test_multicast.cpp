#include <gtest/gtest.h>

#include "algorithms/multicast.hpp"
#include "io/parser.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace {

Contact make_contact(ContactId id,
                     TimePoint start_time,
                     TimePoint end_time,
                     NodeId from,
                     NodeId to,
                     Volume data_rate = 10.0) {
    return Contact{id, start_time, end_time, from, to, data_rate, 0, {}};
}

RangeInterval make_range(TimePoint start_time,
                         TimePoint end_time,
                         NodeId node_a,
                         NodeId node_b,
                         TimePoint distance_light_seconds) {
    return RangeInterval{start_time, end_time, node_a, node_b, distance_light_seconds};
}

Bundle make_multicast_bundle(NodeId source_node,
                             Volume payload_size = 128.0,
                             Volume header_size = 32.0) {
    Bundle bundle;
    bundle.id = 1001;
    bundle.source_node = source_node;
    bundle.destination_eid = -1;
    bundle.creation_time = 0.0;
    bundle.ttl = 120.0;
    bundle.payload_size = payload_size;
    bundle.header_size = header_size;
    bundle.priority = Priority::NORMAL;
    bundle.is_multicast = true;
    bundle.allow_fragmentation = true;
    return bundle;
}

ContactPlan make_shared_prefix_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 20.0, 1, 2, 500.0),
        make_contact(2, 21.0, 40.0, 2, 4, 500.0),
        make_contact(3, 21.0, 40.0, 2, 5, 500.0),
        make_contact(4, 0.0, 20.0, 1, 3, 500.0),
        make_contact(5, 21.0, 40.0, 3, 6, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
        make_range(0.0, 100.0, 2, 5, 1.0),
        make_range(0.0, 100.0, 1, 3, 1.0),
        make_range(0.0, 100.0, 3, 6, 1.0),
    });
    return plan;
}

ContactPlan make_subset_repair_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 10.0, 1, 2, 500.0),
        make_contact(2, 11.0, 20.0, 2, 4, 500.0),
        make_contact(3, 11.0, 20.0, 2, 5, 500.0),
        make_contact(4, 21.0, 30.0, 2, 6, 500.0),
        make_contact(5, 31.0, 40.0, 6, 5, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
        make_range(0.0, 100.0, 2, 5, 1.0),
        make_range(0.0, 100.0, 2, 6, 1.0),
        make_range(0.0, 100.0, 6, 5, 1.0),
    });
    return plan;
}

ContactPlan make_trunk_backup_plan() {
    ContactPlan plan;
    plan.set_contacts({
        make_contact(1, 0.0, 5.0, 1, 2, 500.0),
        make_contact(2, 6.0, 10.0, 2, 4, 500.0),
        make_contact(3, 6.0, 10.0, 2, 5, 500.0),
        make_contact(4, 0.0, 5.0, 1, 3, 500.0),
        make_contact(5, 7.0, 12.0, 3, 4, 500.0),
        make_contact(6, 7.0, 12.0, 3, 5, 500.0),
    });
    plan.set_ranges({
        make_range(0.0, 100.0, 1, 2, 1.0),
        make_range(0.0, 100.0, 2, 4, 1.0),
        make_range(0.0, 100.0, 2, 5, 1.0),
        make_range(0.0, 100.0, 1, 3, 1.0),
        make_range(0.0, 100.0, 3, 4, 1.0),
        make_range(0.0, 100.0, 3, 5, 1.0),
    });
    return plan;
}

std::map<ContactId, std::array<Volume, 3>> mtv_snapshot(
    const ContactPlan& plan,
    std::initializer_list<ContactId> contact_ids) {
    std::map<ContactId, std::array<Volume, 3>> snapshot;
    for (const ContactId contact_id : contact_ids) {
        const Contact* contact = plan.get_contact(contact_id);
        EXPECT_NE(contact, nullptr);
        snapshot.emplace(contact_id, contact->mtv);
    }
    return snapshot;
}

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::filesystem::path exp3_dir() {
    return repo_root() / "configs" / "experiments" / "exp3_multicast_plan";
}

std::filesystem::path exp4_dir() {
    return repo_root() / "configs" / "experiments" / "exp4_multicast_repair";
}

std::vector<MulticastGroup> as_registry_groups(const ScenarioConfig& scenario) {
    std::vector<MulticastGroup> groups;
    groups.reserve(scenario.multicast_groups.size());
    for (const MulticastGroupConfig& config : scenario.multicast_groups) {
        groups.push_back({
            config.group_id,
            config.source_node,
            std::set<NodeId>(config.member_nodes.begin(), config.member_nodes.end())});
    }
    return groups;
}

std::unique_ptr<SimEngine> run_multicast_scenario(const ScenarioConfig& scenario) {
    auto engine = std::make_unique<SimEngine>(scenario.contact_plan,
                                              scenario.traffic_patterns,
                                              scenario.engine);
    engine->set_multicast_groups(as_registry_groups(scenario));
    engine->initialize();
    engine->run();
    return engine;
}

const ExperimentDimension* find_dimension(const ExperimentMatrixConfig& matrix,
                                          const std::string& name) {
    for (const ExperimentDimension& dimension : matrix.dimensions) {
        if (dimension.name == name) {
            return &dimension;
        }
    }
    return nullptr;
}

const ContactStatsRecord* find_contact_record(const std::vector<ContactStatsRecord>& records,
                                              ContactId contact_id) {
    for (const ContactStatsRecord& record : records) {
        if (record.contact_id == contact_id) {
            return &record;
        }
    }
    return nullptr;
}

}  // namespace

TEST(MulticastGroupRegistryTest, AddUpdateRemoveAndReplaceGroups) {
    MulticastGroupRegistry registry;
    std::string failure_reason;

    EXPECT_TRUE(registry.empty());
    EXPECT_TRUE(registry.add_group({"science_team", 1, {4, 5, 6}}, &failure_reason));
    EXPECT_TRUE(failure_reason.empty());
    EXPECT_EQ(registry.size(), 1u);

    const MulticastGroup* group = registry.find_group("science_team");
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(group->source_node, 1);
    EXPECT_EQ(group->member_nodes, (std::set<NodeId>{4, 5, 6}));

    EXPECT_FALSE(registry.add_group({"science_team", 1, {7}}, &failure_reason));
    EXPECT_EQ(failure_reason, "group_exists");

    EXPECT_FALSE(registry.add_group({"invalid_group", 1, {1, 7}}, &failure_reason));
    EXPECT_EQ(failure_reason, "source_is_member");

    EXPECT_TRUE(registry.update_group({"science_team", 1, {4, 6}}, &failure_reason));
    EXPECT_TRUE(failure_reason.empty());
    group = registry.find_group("science_team");
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(group->member_nodes, (std::set<NodeId>{4, 6}));

    EXPECT_TRUE(registry.replace_groups({
        {"ops_team", 2, {7, 8}},
        {"relay_team", 3, {9}},
    }, &failure_reason));
    EXPECT_TRUE(failure_reason.empty());
    EXPECT_EQ(registry.size(), 2u);
    EXPECT_EQ(registry.find_group("science_team"), nullptr);
    EXPECT_NE(registry.find_group("ops_team"), nullptr);

    EXPECT_TRUE(registry.remove_group("ops_team"));
    EXPECT_FALSE(registry.remove_group("missing_group"));
}

TEST(MulticastPlannerTest, BuildPlanMergesSharedPrefixWithoutConsumingMtv) {
    ContactPlan plan = make_shared_prefix_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_multicast_bundle(1);
    MulticastPlanRequest request;
    request.source_node = &node;
    request.bundle = &bundle;
    request.destinations = {4, 5, 6};
    request.current_time = 0.0;
    request.phase1_config.k_paths = 4;

    const std::map<ContactId, std::array<Volume, 3>> before = mtv_snapshot(plan, {1, 2, 3, 4, 5});

    MulticastPlanner planner;
    const MulticastPlanResult result = planner.build_plan(request);

    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(result.failure_reason.empty());
    EXPECT_EQ(result.reachable_destinations, (std::set<NodeId>{4, 5, 6}));
    EXPECT_TRUE(result.unreachable_destinations.empty());
    EXPECT_EQ(result.tree.root, 1);

    const TreeNode* root = result.tree.get_node(1);
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(root->next_hops.size(), 2u);
    EXPECT_EQ(root->next_hops.at(2).contact_id, 1u);
    EXPECT_EQ(root->next_hops.at(2).destinations, (std::set<NodeId>{4, 5}));
    EXPECT_EQ(root->next_hops.at(3).contact_id, 4u);
    EXPECT_EQ(root->next_hops.at(3).destinations, (std::set<NodeId>{6}));

    const TreeNode* node_two = result.tree.get_node(2);
    ASSERT_NE(node_two, nullptr);
    ASSERT_EQ(node_two->next_hops.size(), 2u);
    EXPECT_EQ(node_two->next_hops.at(4).contact_id, 2u);
    EXPECT_EQ(node_two->next_hops.at(4).destinations, (std::set<NodeId>{4}));
    EXPECT_EQ(node_two->next_hops.at(5).contact_id, 3u);
    EXPECT_EQ(node_two->next_hops.at(5).destinations, (std::set<NodeId>{5}));

    const TreeNode* node_three = result.tree.get_node(3);
    ASSERT_NE(node_three, nullptr);
    ASSERT_EQ(node_three->next_hops.size(), 1u);
    EXPECT_EQ(node_three->next_hops.at(6).contact_id, 5u);
    EXPECT_EQ(node_three->next_hops.at(6).destinations, (std::set<NodeId>{6}));

    EXPECT_NE(result.tree.get_node(4), nullptr);
    EXPECT_NE(result.tree.get_node(5), nullptr);
    EXPECT_NE(result.tree.get_node(6), nullptr);
    EXPECT_EQ(mtv_snapshot(plan, {1, 2, 3, 4, 5}), before);
}

TEST(MulticastPlannerTest, BuildPlanForGroupUsesRegistryAndTracksUnreachableDestinations) {
    ContactPlan plan = make_shared_prefix_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_multicast_bundle(1);
    MulticastPlanRequest request;
    request.source_node = &node;
    request.bundle = &bundle;
    request.current_time = 0.0;
    request.phase1_config.k_paths = 4;

    MulticastGroupRegistry registry;
    std::string failure_reason;
    ASSERT_TRUE(registry.add_group({"science_team", 1, {4, 7}}, &failure_reason));
    ASSERT_TRUE(failure_reason.empty());

    MulticastPlanner planner;
    const MulticastPlanResult result = planner.build_plan_for_group(request, registry, "science_team");

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.reachable_destinations, (std::set<NodeId>{4}));
    EXPECT_EQ(result.unreachable_destinations, (std::set<NodeId>{7}));
    ASSERT_EQ(result.failure_reasons.size(), 1u);
    EXPECT_EQ(result.failure_reasons.at(7), "cgr_not_applicable");

    const TreeNode* root = result.tree.get_node(1);
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(root->next_hops.size(), 1u);
    EXPECT_EQ(root->next_hops.at(2).contact_id, 1u);
    EXPECT_EQ(root->next_hops.at(2).destinations, (std::set<NodeId>{4}));
}

TEST(MulticastPlannerTest, PlanForwardingPrunesSubtreesAndAssignsReplicaLineage) {
    ContactPlan plan = make_shared_prefix_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_multicast_bundle(1);
    MulticastPlanRequest request;
    request.source_node = &node;
    request.bundle = &bundle;
    request.destinations = {4, 5, 6};
    request.current_time = 0.0;
    request.phase1_config.k_paths = 4;

    MulticastPlanner planner;
    const MulticastPlanResult plan_result = planner.build_plan(request);
    ASSERT_TRUE(plan_result.ok());

    bundle.pending_destinations = request.destinations;
    bundle.multicast_plan = plan_result.tree;
    bundle.encoded_plan_version = plan.version();

    BundleId next_bundle_id = 2000;
    const MulticastExecutionResult execution = MulticastPlanner::plan_forwarding(bundle, 1, &next_bundle_id);

    ASSERT_TRUE(execution.ok());
    EXPECT_FALSE(execution.deliver_local);
    EXPECT_TRUE(execution.delivered_here.empty());
    ASSERT_EQ(execution.dispatches.size(), 2u);
    EXPECT_EQ(next_bundle_id, 2001u);

    const MulticastDispatch& first = execution.dispatches[0];
    EXPECT_EQ(first.contact_id, 1u);
    EXPECT_EQ(first.next_hop, 2);
    EXPECT_EQ(first.destinations, (std::set<NodeId>{4, 5}));
    EXPECT_EQ(first.bundle.id, bundle.id);
    EXPECT_EQ(first.bundle.origin_bundle_id, bundle.id);
    EXPECT_EQ(first.bundle.parent_bundle_id, 0u);
    EXPECT_EQ(first.bundle.replica_id, bundle.id);
    EXPECT_EQ(first.bundle.pending_destinations, (std::set<NodeId>{4, 5}));
    EXPECT_EQ(first.bundle.multicast_plan.root, 1);
    ASSERT_NE(first.bundle.multicast_plan.get_node(1), nullptr);
    ASSERT_NE(first.bundle.multicast_plan.get_node(2), nullptr);
    EXPECT_EQ(first.bundle.multicast_plan.get_node(1)->next_hops.size(), 1u);
    EXPECT_EQ(first.bundle.multicast_plan.get_node(1)->next_hops.at(2).destinations, (std::set<NodeId>{4, 5}));

    const MulticastDispatch& second = execution.dispatches[1];
    EXPECT_EQ(second.contact_id, 4u);
    EXPECT_EQ(second.next_hop, 3);
    EXPECT_EQ(second.destinations, (std::set<NodeId>{6}));
    EXPECT_EQ(second.bundle.id, 2000u);
    EXPECT_EQ(second.bundle.origin_bundle_id, bundle.id);
    EXPECT_EQ(second.bundle.parent_bundle_id, bundle.id);
    EXPECT_EQ(second.bundle.replica_id, 2000u);
    EXPECT_EQ(second.bundle.pending_destinations, (std::set<NodeId>{6}));
    EXPECT_EQ(second.bundle.multicast_plan.root, 1);
    ASSERT_NE(second.bundle.multicast_plan.get_node(1), nullptr);
    ASSERT_NE(second.bundle.multicast_plan.get_node(3), nullptr);
    EXPECT_EQ(second.bundle.multicast_plan.get_node(1)->next_hops.size(), 1u);
    EXPECT_EQ(second.bundle.multicast_plan.get_node(1)->next_hops.at(3).destinations, (std::set<NodeId>{6}));
}

TEST(MulticastPlannerTest, PlanForwardingMarksLocalDeliveryAndCarriesDeliveredSetIntoChildren) {
    ContactPlan plan = make_shared_prefix_plan();
    Node node(1);
    node.contact_plan = &plan;

    Bundle bundle = make_multicast_bundle(1);
    MulticastPlanRequest request;
    request.source_node = &node;
    request.bundle = &bundle;
    request.destinations = {4, 5, 6};
    request.current_time = 0.0;
    request.phase1_config.k_paths = 4;

    MulticastPlanner planner;
    const MulticastPlanResult plan_result = planner.build_plan(request);
    ASSERT_TRUE(plan_result.ok());

    bundle.pending_destinations = {1, 4, 5, 6};
    bundle.multicast_plan = plan_result.tree;
    bundle.encoded_plan_version = plan.version();

    BundleId next_bundle_id = 3000;
    const MulticastExecutionResult execution = MulticastPlanner::plan_forwarding(bundle, 1, &next_bundle_id);

    ASSERT_TRUE(execution.ok());
    EXPECT_TRUE(execution.deliver_local);
    EXPECT_EQ(execution.delivered_here, (std::set<NodeId>{1}));
    ASSERT_EQ(execution.dispatches.size(), 2u);
    EXPECT_EQ(execution.dispatches[0].bundle.delivered_destinations, (std::set<NodeId>{1}));
    EXPECT_EQ(execution.dispatches[1].bundle.delivered_destinations, (std::set<NodeId>{1}));
    EXPECT_EQ(execution.dispatches[0].bundle.pending_destinations, (std::set<NodeId>{4, 5}));
    EXPECT_EQ(execution.dispatches[1].bundle.pending_destinations, (std::set<NodeId>{6}));
}

TEST(MulticastExecutionTest, EngineExecutesSharedPrefixTreeEndToEnd) {
    auto contact_plan = std::make_shared<ContactPlan>(make_shared_prefix_plan());

    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 1;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 96.0;
    pattern.header_size = 16.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 120.0;
    pattern.is_multicast = true;
    pattern.multicast_group_id = "science_team";

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 60.0;
    config.owlt_margin = 0.0;
    config.recompute_budget = 4;
    config.phase1_config.k_paths = 4;

    SimEngine engine(contact_plan, {pattern}, config);
    engine.set_multicast_groups({{"science_team", 1, {4, 5, 6}}});
    engine.initialize();
    engine.run();

    EXPECT_EQ(engine.metrics().bundles_created, 1u);
    EXPECT_EQ(engine.metrics().bundles_forwarded, 5u);
    EXPECT_EQ(engine.metrics().bundles_delivered, 3u);
    EXPECT_EQ(engine.metrics().route_failures, 0u);
    EXPECT_EQ(engine.delivered_bundle_ids(), (std::vector<BundleId>{1, 2, 3}));

    const std::shared_ptr<Bundle> bundle_one = engine.find_bundle(1);
    const std::shared_ptr<Bundle> bundle_two = engine.find_bundle(2);
    const std::shared_ptr<Bundle> bundle_three = engine.find_bundle(3);
    ASSERT_NE(bundle_one, nullptr);
    ASSERT_NE(bundle_two, nullptr);
    ASSERT_NE(bundle_three, nullptr);

    EXPECT_EQ(bundle_one->route_path, (std::vector<NodeId>{1, 2, 4}));
    EXPECT_EQ(bundle_two->route_path, (std::vector<NodeId>{1, 3, 6}));
    EXPECT_EQ(bundle_three->route_path, (std::vector<NodeId>{1, 2, 5}));

    EXPECT_EQ(bundle_one->origin_bundle_id, 1u);
    EXPECT_EQ(bundle_one->parent_bundle_id, 0u);
    EXPECT_EQ(bundle_one->replica_id, 1u);

    EXPECT_EQ(bundle_two->origin_bundle_id, 1u);
    EXPECT_EQ(bundle_two->parent_bundle_id, 1u);
    EXPECT_EQ(bundle_two->replica_id, 2u);

    EXPECT_EQ(bundle_three->origin_bundle_id, 1u);
    EXPECT_EQ(bundle_three->parent_bundle_id, 1u);
    EXPECT_EQ(bundle_three->replica_id, 3u);
}

TEST(MulticastExecutionTest, SharedPrefixCapacityIsCommittedOnlyOncePerActualCopy) {
    auto contact_plan = std::make_shared<ContactPlan>(make_shared_prefix_plan());

    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 1;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 96.0;
    pattern.header_size = 16.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 120.0;
    pattern.is_multicast = true;
    pattern.multicast_group_id = "science_team";

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 60.0;
    config.recompute_budget = 4;
    config.phase1_config.k_paths = 4;

    SimEngine engine(contact_plan, {pattern}, config);
    engine.set_multicast_groups({{"science_team", 1, {4, 5, 6}}});
    engine.initialize();
    engine.run();

    const std::vector<ContactStatsRecord> contacts = engine.stats().contact_records();
    const ContactStatsRecord* contact_one = find_contact_record(contacts, 1);
    const ContactStatsRecord* contact_two = find_contact_record(contacts, 2);
    const ContactStatsRecord* contact_three = find_contact_record(contacts, 3);
    const ContactStatsRecord* contact_four = find_contact_record(contacts, 4);
    const ContactStatsRecord* contact_five = find_contact_record(contacts, 5);
    ASSERT_NE(contact_one, nullptr);
    ASSERT_NE(contact_two, nullptr);
    ASSERT_NE(contact_three, nullptr);
    ASSERT_NE(contact_four, nullptr);
    ASSERT_NE(contact_five, nullptr);

    EXPECT_EQ(contact_one->commit_count, 1u);
    EXPECT_EQ(contact_two->commit_count, 1u);
    EXPECT_EQ(contact_three->commit_count, 1u);
    EXPECT_EQ(contact_four->commit_count, 1u);
    EXPECT_EQ(contact_five->commit_count, 1u);
}

TEST(MulticastExecutionTest, IntermediateDestinationIsDeliveredAndStillForwardsChildren) {
    auto contact_plan = std::make_shared<ContactPlan>(make_subset_repair_plan());

    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 1;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 64.0;
    pattern.header_size = 16.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 120.0;
    pattern.is_multicast = true;
    pattern.multicast_group_id = "relay_group";

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 60.0;
    config.phase1_config.k_paths = 4;

    SimEngine engine(contact_plan, {pattern}, config);
    engine.set_multicast_groups({{"relay_group", 1, {2, 4, 5}}});
    engine.initialize();
    engine.run();

    const NodeRuntime* relay_runtime = engine.runtime().find_node(2);
    ASSERT_NE(relay_runtime, nullptr);
    ASSERT_NE(relay_runtime->bpa, nullptr);
    EXPECT_EQ(relay_runtime->bpa->delivered_bundle_count(), 1u);
    EXPECT_EQ(engine.delivered_bundle_ids(), (std::vector<BundleId>{1, 2}));

    const std::shared_ptr<Bundle> branch_four = engine.find_bundle(1);
    const std::shared_ptr<Bundle> branch_five = engine.find_bundle(2);
    ASSERT_NE(branch_four, nullptr);
    ASSERT_NE(branch_five, nullptr);
    EXPECT_EQ(branch_four->delivered_destinations, (std::set<NodeId>{2, 4}));
    EXPECT_EQ(branch_five->delivered_destinations, (std::set<NodeId>{2, 5}));
}

TEST(MulticastRepairTest, EngineRepairsAffectedSubsetAfterContactRemoval) {
    auto contact_plan = std::make_shared<ContactPlan>(make_subset_repair_plan());

    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 1;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 64.0;
    pattern.header_size = 16.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 120.0;
    pattern.is_multicast = true;
    pattern.multicast_group_id = "repair_group";

    FailureRule removal;
    removal.mode = FailureRuleMode::CONTACT_SET;
    removal.trigger_time = 5.0;
    removal.contact_ids = {3};
    removal.label = "remove_branch_to_5";

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 60.0;
    config.failure_rules = {removal};
    config.phase1_config.k_paths = 4;

    SimEngine engine(contact_plan, {pattern}, config);
    engine.set_multicast_groups({{"repair_group", 1, {4, 5}}});
    engine.initialize();
    engine.run();

    EXPECT_EQ(engine.metrics().plan_update_events, 1u);
    EXPECT_EQ(engine.metrics().queued_bundle_replans, 1u);
    EXPECT_EQ(engine.metrics().route_failures, 0u);
    EXPECT_EQ(engine.stats().summary().local_repair_count, 1u);
    EXPECT_EQ(engine.delivered_bundle_ids(), (std::vector<BundleId>{1, 2}));

    const std::shared_ptr<Bundle> branch_four = engine.find_bundle(1);
    const std::shared_ptr<Bundle> repaired_branch = engine.find_bundle(2);
    ASSERT_NE(branch_four, nullptr);
    ASSERT_NE(repaired_branch, nullptr);

    EXPECT_EQ(branch_four->route_path, (std::vector<NodeId>{1, 2, 4}));
    EXPECT_EQ(repaired_branch->route_path, (std::vector<NodeId>{1, 2, 6, 5}));
    EXPECT_EQ(repaired_branch->origin_bundle_id, 1u);
    EXPECT_EQ(repaired_branch->parent_bundle_id, 1u);
    EXPECT_EQ(repaired_branch->replica_id, 2u);

    bool saw_local_repair = false;
    for (const std::string& message : engine.event_log()) {
        if (message.find("LOCAL_REPAIR bundle=2 node=2") != std::string::npos) {
            saw_local_repair = true;
            break;
        }
    }
    EXPECT_TRUE(saw_local_repair);
}

TEST(MulticastExecutionTest, TrunkOnlyRedundancyDropsLaterBackupBranches) {
    auto contact_plan = std::make_shared<ContactPlan>(make_trunk_backup_plan());

    TrafficPattern pattern;
    pattern.mode = TrafficGenerationMode::SINGLE;
    pattern.source_node = 1;
    pattern.start_time = 0.0;
    pattern.bundle_count = 1;
    pattern.payload_size = 64.0;
    pattern.header_size = 16.0;
    pattern.priority = Priority::NORMAL;
    pattern.ttl = 120.0;
    pattern.is_multicast = true;
    pattern.multicast_group_id = "trunk_group";

    EngineConfig config;
    config.start_time = 0.0;
    config.end_time = 40.0;
    config.phase1_config.k_paths = 4;
    config.redundancy.mode = RedundancyMode::TRUNK_ONLY;
    config.redundancy.max_extra_copies = 1;
    config.redundancy.enable_for_multicast_trunk = true;

    SimEngine engine(contact_plan, {pattern}, config);
    engine.set_multicast_groups({{"trunk_group", 1, {4, 5}}});
    engine.initialize();
    engine.run();

    EXPECT_EQ(engine.metrics().bundles_delivered, 2u);
    EXPECT_EQ(engine.stats().summary().redundancy_trigger_count, 1u);
    EXPECT_EQ(engine.stats().summary().duplicate_replica_discard_count, 2u);
    EXPECT_EQ(engine.delivered_bundle_ids(), (std::vector<BundleId>{1, 3}));

    bool saw_redundancy_trigger = false;
    for (const std::string& message : engine.event_log()) {
        if (message.find("REDUNDANCY_TRIGGER bundle=1 node=1 trunk_backups=1") != std::string::npos) {
            saw_redundancy_trigger = true;
            break;
        }
    }
    EXPECT_TRUE(saw_redundancy_trigger);
}

TEST(MulticastScenarioTest, Exp3TreePlanScenarioRunsEndToEnd) {
    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(exp3_dir() / "tree_plan.json");

    std::unique_ptr<SimEngine> engine = run_multicast_scenario(scenario);

    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->metrics().bundles_created, 4u);
    EXPECT_EQ(engine->metrics().bundles_forwarded, 20u);
    EXPECT_EQ(engine->metrics().route_failures, 0u);
    EXPECT_EQ(engine->metrics().bundles_delivered, 12u);
    EXPECT_EQ(engine->stats().summary().local_repair_count, 0u);
    EXPECT_EQ(engine->delivered_bundle_ids().size(), 12u);
}

TEST(MulticastScenarioTest, Exp4RepairScenarioExecutesLocalRepairPath) {
    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(exp4_dir() / "baseline.json");

    std::unique_ptr<SimEngine> engine = run_multicast_scenario(scenario);

    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->metrics().bundles_created, 1u);
    EXPECT_EQ(engine->metrics().plan_update_events, 1u);
    EXPECT_EQ(engine->metrics().queued_bundle_replans, 2u);
    EXPECT_EQ(engine->metrics().route_failures, 0u);
    EXPECT_EQ(engine->metrics().bundles_delivered, 2u);
    EXPECT_EQ(engine->stats().summary().local_repair_count, 2u);
    EXPECT_EQ(engine->delivered_bundle_ids(), (std::vector<BundleId>{1, 2}));
}

TEST(MulticastExperimentConfigTest, Exp3TreePlanScenarioIsExecutable) {
    const std::filesystem::path scenario_path = exp3_dir() / "tree_plan.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(scenario_path);

    EXPECT_EQ(scenario.scenario_name, "exp3_multicast_plan_tree");
    EXPECT_EQ(scenario.source_file.lexically_normal(), scenario_path.lexically_normal());
    EXPECT_EQ(scenario.output_directory,
              (repo_root() / "results" / "exp3_multicast_plan" / "tree_plan").lexically_normal());
    ASSERT_NE(scenario.contact_plan, nullptr);
    EXPECT_FALSE(scenario.contact_plan->contacts().empty());
    EXPECT_FALSE(scenario.contact_plan->ranges().empty());
    ASSERT_EQ(scenario.multicast_groups.size(), 1u);
    EXPECT_EQ(scenario.multicast_groups[0].group_id, "science_team");
    EXPECT_EQ(scenario.multicast_groups[0].source_node, 1);
    EXPECT_EQ(std::set<NodeId>(scenario.multicast_groups[0].member_nodes.begin(),
                               scenario.multicast_groups[0].member_nodes.end()),
              (std::set<NodeId>{4, 5, 6}));

    ASSERT_EQ(scenario.traffic_patterns.size(), 1u);
    EXPECT_TRUE(scenario.traffic_patterns[0].is_multicast);
    EXPECT_EQ(scenario.traffic_patterns[0].multicast_group_id, "science_team");
    EXPECT_EQ(scenario.engine.phase1_config.k_paths, 6);
    EXPECT_EQ(scenario.engine.recompute_budget, 4);
    EXPECT_TRUE(scenario.engine.phase1_config.one_route_per_neighbor);
    EXPECT_TRUE(scenario.engine.phase2_config.queue_delay_enhancement);
    EXPECT_TRUE(scenario.engine.phase2_config.anti_loop_proactive);
}

TEST(MulticastExperimentConfigTest, Exp3SplitUnicastControlScenarioIsExecutable) {
    const std::filesystem::path scenario_path = exp3_dir() / "split_unicast_control.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(scenario_path);

    EXPECT_EQ(scenario.scenario_name, "exp3_multicast_plan_split_unicast_control");
    EXPECT_EQ(scenario.source_file.lexically_normal(), scenario_path.lexically_normal());
    EXPECT_EQ(scenario.output_directory,
              (repo_root() / "results" / "exp3_multicast_plan" / "split_unicast_control").lexically_normal());
    ASSERT_NE(scenario.contact_plan, nullptr);
    ASSERT_EQ(scenario.traffic_patterns.size(), 3u);
    EXPECT_FALSE(scenario.traffic_patterns[0].is_multicast);
    EXPECT_FALSE(scenario.traffic_patterns[1].is_multicast);
    EXPECT_FALSE(scenario.traffic_patterns[2].is_multicast);
    EXPECT_EQ(scenario.traffic_patterns[0].destination_node, 4);
    EXPECT_EQ(scenario.traffic_patterns[1].destination_node, 5);
    EXPECT_EQ(scenario.traffic_patterns[2].destination_node, 6);
}

TEST(MulticastExperimentConfigTest, Exp3MatrixDimensionsMatchPlanExperiment) {
    const std::filesystem::path matrix_path = exp3_dir() / "matrix.json";

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);

    EXPECT_EQ(matrix.experiment_name, "exp3_multicast_plan");
    ASSERT_EQ(matrix.scenario_files.size(), 2u);
    EXPECT_EQ(matrix.scenario_files[0].lexically_normal(),
              (exp3_dir() / "tree_plan.json").lexically_normal());
    EXPECT_EQ(matrix.scenario_files[1].lexically_normal(),
              (exp3_dir() / "split_unicast_control.json").lexically_normal());
    ASSERT_EQ(matrix.dimensions.size(), 3u);

    const ExperimentDimension* bundle_count = find_dimension(matrix, "traffic[*].bundle_count");
    ASSERT_NE(bundle_count, nullptr);
    ASSERT_EQ(bundle_count->values.size(), 3u);
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[0]), 1);
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[2]), 16);

    const ExperimentDimension* group_size = find_dimension(matrix, "multicast.group_size");
    ASSERT_NE(group_size, nullptr);
    ASSERT_EQ(group_size->values.size(), 2u);
    EXPECT_EQ(std::get<std::int64_t>(group_size->values[0]), 2);
    EXPECT_EQ(std::get<std::int64_t>(group_size->values[1]), 3);

    const ExperimentDimension* payload_size = find_dimension(matrix, "traffic[*].payload_size");
    ASSERT_NE(payload_size, nullptr);
    ASSERT_EQ(payload_size->values.size(), 2u);
    EXPECT_EQ(std::get<std::int64_t>(payload_size->values[0]), 128);
    EXPECT_EQ(std::get<std::int64_t>(payload_size->values[1]), 512);
}

TEST(MulticastExperimentConfigTest, Exp4RepairScenarioIsExecutable) {
    const std::filesystem::path scenario_path = exp4_dir() / "baseline.json";

    const ScenarioConfig scenario = ConfigParser::parse_scenario_file(scenario_path);

    EXPECT_EQ(scenario.scenario_name, "exp4_multicast_repair_baseline");
    EXPECT_EQ(scenario.source_file.lexically_normal(), scenario_path.lexically_normal());
    EXPECT_EQ(scenario.output_directory,
              (repo_root() / "results" / "exp4_multicast_repair" / "baseline").lexically_normal());
    ASSERT_NE(scenario.contact_plan, nullptr);
    ASSERT_EQ(scenario.multicast_groups.size(), 1u);
    EXPECT_EQ(scenario.multicast_groups[0].group_id, "ops_broadcast");
    EXPECT_EQ(std::set<NodeId>(scenario.multicast_groups[0].member_nodes.begin(),
                               scenario.multicast_groups[0].member_nodes.end()),
              (std::set<NodeId>{4, 5}));
    ASSERT_EQ(scenario.traffic_patterns.size(), 1u);
    EXPECT_TRUE(scenario.traffic_patterns[0].is_multicast);
    EXPECT_EQ(scenario.traffic_patterns[0].multicast_group_id, "ops_broadcast");
    EXPECT_EQ(scenario.traffic_patterns[0].bundle_count, 1u);
    ASSERT_EQ(scenario.engine.failure_rules.size(), 1u);
    EXPECT_EQ(scenario.engine.failure_rules[0].mode, FailureRuleMode::PLAN_REPLACEMENT);
    EXPECT_DOUBLE_EQ(scenario.engine.failure_rules[0].trigger_time, 5.0);
}

TEST(MulticastExperimentConfigTest, Exp4MatrixDimensionsMatchRepairExperiment) {
    const std::filesystem::path matrix_path = exp4_dir() / "matrix.json";

    const ExperimentMatrixConfig matrix = ConfigParser::parse_matrix_file(matrix_path);

    EXPECT_EQ(matrix.experiment_name, "exp4_multicast_repair");
    ASSERT_EQ(matrix.scenario_files.size(), 1u);
    EXPECT_EQ(matrix.scenario_files[0].lexically_normal(),
              (exp4_dir() / "baseline.json").lexically_normal());
    ASSERT_EQ(matrix.dimensions.size(), 3u);

    const ExperimentDimension* trigger_time = find_dimension(matrix, "failure_injection[0].trigger_time");
    ASSERT_NE(trigger_time, nullptr);
    ASSERT_EQ(trigger_time->values.size(), 3u);
    EXPECT_DOUBLE_EQ(std::get<double>(trigger_time->values[0]), 4.0);
    EXPECT_DOUBLE_EQ(std::get<double>(trigger_time->values[1]), 5.0);
    EXPECT_DOUBLE_EQ(std::get<double>(trigger_time->values[2]), 8.0);

    const ExperimentDimension* bundle_count = find_dimension(matrix, "traffic[0].bundle_count");
    ASSERT_NE(bundle_count, nullptr);
    ASSERT_EQ(bundle_count->values.size(), 3u);
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[0]), 1);
    EXPECT_EQ(std::get<std::int64_t>(bundle_count->values[2]), 8);
}