#include <gtest/gtest.h>

#include "models/contact.hpp"
#include "models/contact_plan.hpp"
#include "models/bundle.hpp"
#include "models/route.hpp"
#include "models/node.hpp"

// ============================================================
// Contact 测试
// ============================================================

class ContactTest : public ::testing::Test {
protected:
    Contact c;

    void SetUp() override {
        c = {1, 0.0, 100.0, 1, 2, 10.0, 0, {}};
        c.initialize_mtv();
    }
};

TEST_F(ContactTest, VolumeCalculation) {
    // DS-CON-02: volume = (end - start) * rate
    EXPECT_DOUBLE_EQ(c.volume(), 1000.0);
}

TEST_F(ContactTest, VolumeZeroDuration) {
    Contact c0 = {2, 50.0, 50.0, 1, 2, 10.0, 0, {}};
    EXPECT_DOUBLE_EQ(c0.volume(), 0.0);
}

TEST_F(ContactTest, MTVInitialization) {
    // DS-CON-03: MTV 初始值 = volume()
    EXPECT_DOUBLE_EQ(c.mtv[0], 1000.0);
    EXPECT_DOUBLE_EQ(c.mtv[1], 1000.0);
    EXPECT_DOUBLE_EQ(c.mtv[2], 1000.0);
}

TEST_F(ContactTest, ConsumeMTVExpedited) {
    // 消耗 Expedited (2)：降低所有三个级别
    c.consume_mtv(2, 200.0);
    EXPECT_DOUBLE_EQ(c.mtv[0], 800.0);
    EXPECT_DOUBLE_EQ(c.mtv[1], 800.0);
    EXPECT_DOUBLE_EQ(c.mtv[2], 800.0);
}

TEST_F(ContactTest, ConsumeMTVBulk) {
    // 消耗 Bulk (0)：仅降低 Bulk 级别
    c.consume_mtv(0, 300.0);
    EXPECT_DOUBLE_EQ(c.mtv[0], 700.0);
    EXPECT_DOUBLE_EQ(c.mtv[1], 1000.0);
    EXPECT_DOUBLE_EQ(c.mtv[2], 1000.0);
}

TEST_F(ContactTest, ConsumeMTVNormal) {
    // 消耗 Normal (1)：降低 Bulk 和 Normal
    c.consume_mtv(1, 150.0);
    EXPECT_DOUBLE_EQ(c.mtv[0], 850.0);
    EXPECT_DOUBLE_EQ(c.mtv[1], 850.0);
    EXPECT_DOUBLE_EQ(c.mtv[2], 1000.0);
}

TEST_F(ContactTest, ConsumeMTVMultipleTimes) {
    c.consume_mtv(2, 100.0);
    c.consume_mtv(0, 200.0);
    EXPECT_DOUBLE_EQ(c.mtv[0], 700.0);  // 1000 - 100 - 200
    EXPECT_DOUBLE_EQ(c.mtv[1], 900.0);  // 1000 - 100
    EXPECT_DOUBLE_EQ(c.mtv[2], 900.0);  // 1000 - 100
}

TEST_F(ContactTest, IsTerminated) {
    // DS-CON-05: end_time <= current_time 为已终止
    EXPECT_FALSE(c.is_terminated(0.0));
    EXPECT_FALSE(c.is_terminated(99.9));
    EXPECT_TRUE(c.is_terminated(100.0));
    EXPECT_TRUE(c.is_terminated(200.0));
}

TEST_F(ContactTest, BindPlanVersion) {
    c.bind_plan_version(7);
    EXPECT_EQ(c.plan_version, 7u);
}

// ============================================================
// RangeInterval 测试
// ============================================================

TEST(RangeIntervalTest, InvolvesSymmetric) {
    // DS-RNG-02: 双向性
    RangeInterval r = {0.0, 1000.0, 1, 2, 0.5};
    EXPECT_TRUE(r.involves(1, 2));
    EXPECT_TRUE(r.involves(2, 1));
    EXPECT_FALSE(r.involves(1, 3));
    EXPECT_FALSE(r.involves(3, 2));
}

TEST(RangeIntervalTest, GetOWLT) {
    RangeInterval r = {0.0, 1000.0, 1, 2, 1.3};
    EXPECT_DOUBLE_EQ(r.get_owlt(1, 2), 1.3);
    EXPECT_DOUBLE_EQ(r.get_owlt(2, 1), 1.3);
}

// ============================================================
// ContactPlan 测试
// ============================================================

class ContactPlanTest : public ::testing::Test {
protected:
    ContactPlan cp;

    void SetUp() override {
        cp.set_contacts({
            {1, 0.0, 100.0, 1, 2, 10.0, 0, {}},
            {2, 50.0, 150.0, 2, 3, 20.0, 0, {}},
            {3, 10.0, 80.0, 1, 3, 5.0, 0, {}}
        });
        cp.set_ranges({
            {0.0, 1000.0, 1, 2, 0.5},
            {0.0, 1000.0, 2, 3, 1.0}
        });
    }
};

TEST_F(ContactPlanTest, GetContactsFrom) {
    auto result = cp.get_contacts_from(1);
    EXPECT_EQ(result.size(), 2u);  // c1 和 c3
}

TEST_F(ContactPlanTest, GetContactsTo) {
    auto result = cp.get_contacts_to(3);
    EXPECT_EQ(result.size(), 2u);  // c2 和 c3
}

TEST_F(ContactPlanTest, GetContactsBetween) {
    auto result = cp.get_contacts_between(1, 2);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0]->contact_id, 1u);
}

TEST_F(ContactPlanTest, GetOWLTFound) {
    EXPECT_DOUBLE_EQ(cp.get_owlt(1, 2, 0.0), 0.5);
    EXPECT_DOUBLE_EQ(cp.get_owlt(2, 1, 500.0), 0.5);  // 双向
    EXPECT_DOUBLE_EQ(cp.get_owlt(2, 3, 100.0), 1.0);
}

TEST_F(ContactPlanTest, GetOWLTNotFound) {
    // 无匹配的节点对
    EXPECT_DOUBLE_EQ(cp.get_owlt(1, 3, 0.0), 0.0);
    // 时间超出范围
    EXPECT_DOUBLE_EQ(cp.get_owlt(1, 2, 1000.0), 0.0);
}

TEST_F(ContactPlanTest, AddContactSetsModified) {
    PlanVersion initial_version = cp.version();
    Contact c4 = {4, 200.0, 300.0, 3, 1, 15.0, 0, {}};
    cp.add_contact(c4);
    EXPECT_EQ(cp.version(), initial_version + 1);
    EXPECT_EQ(cp.contacts().size(), 4u);
    ASSERT_NE(cp.get_contact(4), nullptr);
    EXPECT_EQ(cp.get_contact(4)->plan_version, cp.version());
}

TEST_F(ContactPlanTest, RemoveContactSetsModified) {
    PlanVersion initial_version = cp.version();
    cp.remove_contact(2);
    EXPECT_EQ(cp.version(), initial_version + 1);
    EXPECT_EQ(cp.contacts().size(), 2u);
    EXPECT_EQ(cp.get_contact(2), nullptr);
}

TEST_F(ContactPlanTest, RemoveContactNotFound) {
    PlanVersion initial_version = cp.version();
    cp.remove_contact(999);
    EXPECT_EQ(cp.version(), initial_version);
    EXPECT_EQ(cp.contacts().size(), 3u);
}

TEST_F(ContactPlanTest, PurgeTerminated) {
    // 在 t=80, c3(end=80) 应被清除
    PlanVersion initial_version = cp.version();
    cp.purge_terminated(80.0);
    EXPECT_EQ(cp.version(), initial_version + 1);
    EXPECT_EQ(cp.contacts().size(), 2u);
    for (const auto& c : cp.contacts()) {
        EXPECT_NE(c.contact_id, 3u);
        EXPECT_EQ(c.plan_version, cp.version());
    }
}

TEST_F(ContactPlanTest, PurgeTerminatedNone) {
    PlanVersion initial_version = cp.version();
    cp.purge_terminated(0.0);
    EXPECT_EQ(cp.version(), initial_version);
    EXPECT_EQ(cp.contacts().size(), 3u);
}

TEST_F(ContactPlanTest, SetRangesBumpsVersionAndKeepsContactsAligned) {
    PlanVersion previous_version = cp.version();
    cp.set_ranges({{0.0, 2000.0, 1, 4, 2.5}});

    EXPECT_EQ(cp.version(), previous_version + 1);
    ASSERT_NE(cp.get_contact(1), nullptr);
    EXPECT_EQ(cp.get_contact(1)->plan_version, cp.version());
}

// ============================================================
// Bundle 测试
// ============================================================

TEST(BundleTest, ExpirationTime) {
    // DS-BDL-02: expiration_time = creation_time + ttl
    Bundle b;
    b.creation_time = 100.0;
    b.ttl = 3600.0;
    EXPECT_DOUBLE_EQ(b.expiration_time(), 3700.0);
}

TEST(BundleTest, EVCLargeBundle) {
    // DS-BDL-03: EVC = h + p + max(0.03*(h+p), 100)
    // 大 Bundle: 0.03*(h+p) > 100
    Bundle b;
    b.header_size = 500.0;
    b.payload_size = 10000.0;
    double total = 10500.0;
    double overhead = 0.03 * total;  // 315.0
    EXPECT_DOUBLE_EQ(b.evc(), total + overhead);  // 10815.0
}

TEST(BundleTest, EVCSmallBundle) {
    // 小 Bundle: 0.03*(h+p) < 100，使用 100
    Bundle b;
    b.header_size = 50.0;
    b.payload_size = 200.0;
    double total = 250.0;
    EXPECT_DOUBLE_EQ(b.evc(), total + 100.0);  // 350.0
}

TEST(BundleTest, EVCBoundary) {
    // 边界情况: 0.03*(h+p) = 100 -> h+p = 3333.33...
    Bundle b;
    b.header_size = 0.0;
    b.payload_size = 10000.0 / 3.0;  // 3333.33...
    double total = b.header_size + b.payload_size;
    double overhead = std::max(0.03 * total, 100.0);
    EXPECT_DOUBLE_EQ(b.evc(), total + overhead);
}

TEST(BundleTest, MulticastPlanOverheadIncludedInEVC) {
    Bundle b;
    b.is_multicast = true;
    b.header_size = 50.0;
    b.payload_size = 200.0;

    TreeNode root;
    root.node_id = 1;
    root.next_hops[2] = {42, {4, 5}};
    b.multicast_plan.root = 1;
    b.multicast_plan.nodes[1] = root;

    double total = 250.0 + b.multicast_plan_size();
    EXPECT_DOUBLE_EQ(b.evc(), total + 100.0);
}

TEST(BundleTest, VisitedNodesTracking) {
    Bundle b;
    b.mark_visited(3);
    b.mark_visited(5);

    EXPECT_TRUE(b.has_visited(3));
    EXPECT_FALSE(b.has_visited(4));
    EXPECT_EQ(b.visited_nodes, std::vector<int>({3, 5}));
}

TEST(BundleTest, DeliveredDestinationsTracking) {
    Bundle b;
    b.is_multicast = true;
    b.pending_destinations = {4, 5, 6};

    EXPECT_TRUE(b.mark_destination_delivered(5));
    EXPECT_FALSE(b.pending_destinations.count(5));
    EXPECT_TRUE(b.delivered_destinations.count(5));
}

TEST(BundleTest, ReplicaKeepsOriginLineage) {
    Bundle b;
    b.id = 10;
    auto replica = b.make_replica(11, 1);

    EXPECT_EQ(replica.id, 11u);
    EXPECT_EQ(replica.parent_bundle_id, 10u);
    EXPECT_EQ(replica.origin_bundle_id, 10u);
    EXPECT_EQ(replica.replica_id, 1u);
}

TEST(BundleTest, RedundancyHelpersReflectReplicaFamily) {
    Bundle b;
    b.id = 10;
    b.destination_eid = 7;

    EXPECT_EQ(b.canonical_origin_id(), 10u);
    EXPECT_FALSE(b.participates_in_redundancy());
    EXPECT_EQ(b.redundancy_scope_key(), "u:7");

    auto replica = b.make_replica(11, 2);
    replica.redundancy_applied = true;
    replica.replica_role = ReplicaRole::REDUNDANT_BACKUP;

    EXPECT_EQ(replica.canonical_origin_id(), 10u);
    EXPECT_TRUE(replica.participates_in_redundancy());
    EXPECT_EQ(replica.redundancy_scope_key(), "u:7");
}

// ============================================================
// Route 测试
// ============================================================

class RouteTest : public ::testing::Test {
protected:
    Contact c1, c2, c3;

    void SetUp() override {
        c1 = {1, 0.0, 100.0, 1, 2, 10.0, 0, {}};
        c2 = {2, 50.0, 120.0, 2, 3, 20.0, 0, {}};
        c3 = {3, 80.0, 200.0, 3, 4, 15.0, 0, {}};
    }
};

TEST_F(RouteTest, IsValidCorrectSequence) {
    // DS-RTE-01: 合法路由
    Route r;
    r.legs = {{&c1, 0.0, 1.0, 100.0}, {&c2, 2.0, 3.0, 90.0}, {&c3, 4.0, 5.0, 80.0}};
    EXPECT_TRUE(r.is_valid());
}

TEST_F(RouteTest, IsValidEmptyRoute) {
    Route r;
    EXPECT_FALSE(r.is_valid());
}

TEST_F(RouteTest, LocalDeliveryRouteIsValid) {
    Route r = Route::make_local_delivery(1, 12.5);

    EXPECT_TRUE(r.is_valid());
    EXPECT_TRUE(r.local_delivery);
    EXPECT_TRUE(r.legs.empty());
    EXPECT_EQ(r.local_delivery_node, 1);
    EXPECT_DOUBLE_EQ(r.compute_termination_time(), 12.5);
    EXPECT_EQ(r.compute_entry_node(), 1);
}

TEST_F(RouteTest, LocalDeliveryRouteHasZeroHopCount) {
    Route r = Route::make_local_delivery(7, 4.0);

    EXPECT_EQ(r.hop_count(), 0u);
    EXPECT_EQ(r.entry_node, 7);
    EXPECT_DOUBLE_EQ(r.best_case_delivery_time, 4.0);
    EXPECT_DOUBLE_EQ(r.termination_time, 4.0);
}

TEST_F(RouteTest, LocalDeliveryDoesNotAllowContactLegs) {
    Route r = Route::make_local_delivery(1, 1.0);
    r.legs.push_back({&c1, 0.0, 1.0, 0.0});

    EXPECT_FALSE(r.is_valid());
}

TEST_F(RouteTest, IsValidBrokenChain) {
    // 连续性违反: c1.receiving_node (2) != c3.sending_node (3)
    Route r;
    r.legs = {{&c1, 0.0, 1.0, 100.0}, {&c3, 2.0, 3.0, 90.0}};
    EXPECT_FALSE(r.is_valid());
}

TEST_F(RouteTest, IsValidTimeViolation) {
    // 时间顺序违反
    Contact cy = {5, 200.0, 300.0, 1, 2, 10.0, 0, {}};
    Contact cz = {6, 0.0, 50.0, 2, 3, 10.0, 0, {}};
    Route r;
    r.legs = {{&cy, 0.0, 0.0, 0.0}, {&cz, 0.0, 0.0, 0.0}};
    EXPECT_FALSE(r.is_valid());
}

TEST_F(RouteTest, ComputeTerminationTime) {
    // DS-RTE-03: min of all end_times
    Route r;
    r.legs = {{&c1, 0.0, 1.0, 100.0}, {&c2, 2.0, 3.0, 90.0}, {&c3, 4.0, 5.0, 80.0}};
    EXPECT_DOUBLE_EQ(r.compute_termination_time(), 100.0);  // min(100, 120, 200)
}

TEST_F(RouteTest, ComputeEntryNode) {
    // DS-RTE-04: hops[0]->receiving_node
    Route r;
    r.legs = {{&c1, 0.0, 1.0, 100.0}, {&c2, 2.0, 3.0, 90.0}, {&c3, 4.0, 5.0, 80.0}};
    EXPECT_EQ(r.compute_entry_node(), 2);
}

TEST_F(RouteTest, ComputeEntryNodeEmpty) {
    Route r;
    EXPECT_EQ(r.compute_entry_node(), -1);
}

TEST_F(RouteTest, SingleHopRoute) {
    Route r;
    r.legs = {{&c1, 0.0, 1.0, 100.0}};
    EXPECT_TRUE(r.is_valid());
    EXPECT_DOUBLE_EQ(r.compute_termination_time(), 100.0);
    EXPECT_EQ(r.compute_entry_node(), 2);
}

TEST_F(RouteTest, NullContactLegIsInvalid) {
    Route r;
    r.legs = {{nullptr, 0.0, 0.0, 0.0}};
    EXPECT_FALSE(r.is_valid());
}

TEST_F(RouteTest, HopCountUsesLegCount) {
    Route r;
    r.legs = {{&c1, 0.0, 1.0, 100.0}, {&c2, 2.0, 3.0, 90.0}};
    EXPECT_EQ(r.hop_count(), 2u);
}

TEST(RouteLegTest, LastByteTimesDefaultToZero) {
    RouteLeg leg;

    EXPECT_DOUBLE_EQ(leg.last_byte_transmission_time, 0.0);
    EXPECT_DOUBLE_EQ(leg.last_byte_arrival_time, 0.0);
}

TEST(CandidateRouteTest, DefaultsToInvalidWithoutRejectReason) {
    CandidateRoute candidate;

    EXPECT_FALSE(candidate.valid);
    EXPECT_TRUE(candidate.reject_reason.empty());
}

// ============================================================
// Node 测试
// ============================================================

TEST(NodeTest, Construction) {
    Node n(42);
    EXPECT_EQ(n.node_number, 42);
    EXPECT_TRUE(n.routing_table.empty());
    EXPECT_TRUE(n.send_queues.empty());
    EXPECT_TRUE(n.excluded_neighbors.empty());
    EXPECT_EQ(n.contact_plan, nullptr);
}

TEST(NodeTest, GetRouteListCreatesEmptyList) {
    Node n(1);
    auto& routes = n.get_route_list(5);
    EXPECT_TRUE(routes.empty());
    // 确认已创建条目
    EXPECT_EQ(n.routing_table.count(5), 1u);
    EXPECT_EQ(n.routing_table.at(5).plan_version, 0u);
}

TEST(NodeTest, InvalidateRoutes) {
    Node n(1);
    n.get_route_list(1);
    n.get_route_list(2);
    EXPECT_EQ(n.routing_table.size(), 2u);
    n.invalidate_routes();
    EXPECT_TRUE(n.routing_table.empty());
}

TEST(NodeTest, RouteCacheTracksPlanVersion) {
    ContactPlan cp;
    cp.set_contacts({{1, 0.0, 10.0, 1, 2, 1.0, 0, {}}});

    Node n(1);
    n.contact_plan = &cp;
    auto& routes = n.get_route_list(5);

    EXPECT_TRUE(routes.empty());
    EXPECT_EQ(n.routing_table.at(5).plan_version, cp.version());
}

TEST(NodeTest, FindRouteListRejectsStaleVersion) {
    ContactPlan cp;
    cp.set_contacts({{1, 0.0, 10.0, 1, 2, 1.0, 0, {}}});

    Node n(1);
    n.contact_plan = &cp;
    n.get_route_list(8);

    cp.add_contact({2, 10.0, 20.0, 2, 3, 1.0, 0, {}});
    EXPECT_EQ(n.find_route_list(8), nullptr);

    n.invalidate_stale_routes();
    EXPECT_TRUE(n.routing_table.empty());
}

TEST(NodeTest, ExcludedNeighbors) {
    Node n(1);
    n.excluded_neighbors.insert(3);
    n.excluded_neighbors.insert(5);
    EXPECT_EQ(n.excluded_neighbors.size(), 2u);
    EXPECT_TRUE(n.excluded_neighbors.count(3));
    EXPECT_FALSE(n.excluded_neighbors.count(4));
}

// ============================================================
// MulticastTree 测试
// ============================================================

TEST(MulticastTreeTest, BranchingNode) {
    MulticastTree tree;
    tree.root = 1;
    TreeNode tn;
    tn.node_id = 1;
    tn.next_hops[2] = {42, {4, 5}};
    tn.next_hops[3] = {55, {6}};
    tree.nodes[1] = tn;

    EXPECT_TRUE(tree.is_branching_node(1));   // 两个下一跳
    EXPECT_FALSE(tree.is_branching_node(2));  // 不存在
}

TEST(MulticastTreeTest, GetNode) {
    MulticastTree tree;
    tree.root = 1;
    TreeNode tn;
    tn.node_id = 1;
    tree.nodes[1] = tn;

    EXPECT_NE(tree.get_node(1), nullptr);
    EXPECT_EQ(tree.get_node(99), nullptr);
}

TEST(MulticastTreeTest, Prune) {
    // 树: 1 -> {2: {4,5}, 3: {6}}
    // 裁剪只保留目的节点 {4, 6}
    MulticastTree tree;
    tree.root = 1;

    TreeNode tn1;
    tn1.node_id = 1;
    tn1.next_hops[2] = {42, {4, 5}};
    tn1.next_hops[3] = {55, {6}};
    tree.nodes[1] = tn1;

    TreeNode tn2;
    tn2.node_id = 2;
    tree.nodes[2] = tn2;

    TreeNode tn3;
    tn3.node_id = 3;
    tree.nodes[3] = tn3;

    auto pruned = tree.prune(1, {4, 6});
    EXPECT_EQ(pruned.root, 1);
    // 节点1 应保留，下一跳 2 只含 {4}，下一跳 3 包含 {6}
    auto* pn = pruned.get_node(1);
    ASSERT_NE(pn, nullptr);
    EXPECT_EQ(pn->next_hops.size(), 2u);
    EXPECT_EQ(pn->next_hops.at(2).contact_id, 42u);
    EXPECT_EQ(pn->next_hops.at(2).destinations, std::set<int>({4}));
    EXPECT_EQ(pn->next_hops.at(3).contact_id, 55u);
    EXPECT_EQ(pn->next_hops.at(3).destinations, std::set<int>({6}));
}

TEST(MulticastTreeTest, SerializeDeserialize) {
    MulticastTree tree;
    tree.root = 1;

    TreeNode tn;
    tn.node_id = 1;
    tn.next_hops[2] = {42, {4, 5}};
    tn.next_hops[3] = {55, {6}};
    tree.nodes[1] = tn;

    std::string s = tree.serialize();
    auto restored = MulticastTree::deserialize(s);

    EXPECT_EQ(restored.root, 1);
    auto* rn = restored.get_node(1);
    ASSERT_NE(rn, nullptr);
    EXPECT_EQ(rn->next_hops.at(2).contact_id, 42u);
    EXPECT_EQ(rn->next_hops.at(2).destinations, std::set<int>({4, 5}));
    EXPECT_EQ(rn->next_hops.at(3).contact_id, 55u);
    EXPECT_EQ(rn->next_hops.at(3).destinations, std::set<int>({6}));
}

TEST(MulticastTreeTest, DeserializeEmpty) {
    auto tree = MulticastTree::deserialize("");
    EXPECT_EQ(tree.root, -1);
    EXPECT_TRUE(tree.nodes.empty());
}
