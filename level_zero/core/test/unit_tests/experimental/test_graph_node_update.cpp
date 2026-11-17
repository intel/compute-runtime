/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/internal/l0_graph.h"
#include "level_zero/core/test/unit_tests/experimental/test_graph.h"
#include "level_zero/core/test/unit_tests/mocks/mock_graph.h"
#include "level_zero/driver_experimental/zex_graph.h"
#include "level_zero/ze_api.h"

#include "gtest/gtest.h"

#include <array>
#include <memory>

namespace L0 {

namespace ult {

TEST(GraphNodesUpdateTest, GivenInvalidParametersWhenUpdatingNodesThroughApiThenInvalidArgumentIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.stopCapturing();

    ze_graph_node_handle_t hNode = nullptr;
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    const void *updateDescs[] = {&groupCountDesc};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(nullptr, 1, &hNode, updateDescs, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 0, &hNode, updateDescs, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, nullptr, updateDescs, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, nullptr, nullptr));
}

TEST(GraphNodesUpdateTest, GivenNotMutableGraphWhenUpdatingNodesThenInvalidArgumentIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();

    ze_graph_node_handle_t hNode = nullptr;
    const void *updateDesc = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, graph.updateNodes(1, &hNode, &updateDesc));
}

TEST(GraphNodesUpdateTest, GivenMutableGraphStillRecordingWhenUpdatingNodesThenNotReadyIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};

    ze_graph_node_handle_t hNode = nullptr;
    const void *updateDesc = nullptr;
    EXPECT_EQ(ZE_RESULT_NOT_READY, graph.updateNodes(1, &hNode, &updateDesc));
}

TEST(GraphNodesUpdateTest, GivenNodeNotOwnedByGraphWhenUpdatingNodesThenInvalidArgumentIsReturnedAndNodeIsNotUpdated) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.stopCapturing();

    L0::GraphNode foreignNode{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT};
    ze_group_count_t capturedGroupCount = {1, 1, 1};
    foreignNode.setGroupCounts(&capturedGroupCount);

    ze_graph_node_handle_t hNode = foreignNode.toHandle();
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.groupCount = {2, 3, 4};
    const void *updateDesc = &groupCountDesc;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    EXPECT_EQ(1u, capturedGroupCount.groupCountX);
}

TEST(GraphNodesUpdateTest, GivenNullUpdateDescriptorWhenUpdatingNodesThenInvalidArgumentIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT));
    graph.stopCapturing();

    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    const void *updateDesc = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
}

TEST(GraphNodesUpdateTest, GivenChainedGroupCountAndGroupSizeDescriptorsWhenUpdatingNodeThenGroupCountAndGroupSizeAreUpdated) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE));
    graph.stopCapturing();

    auto &node = *(*graph.graphNodes)[0];
    ze_group_count_t capturedGroupCount = {1, 1, 1};
    ze_group_size_t capturedGroupSize = {1, 1, 1};
    node.setGroupCounts(&capturedGroupCount);
    node.setGroupSizes(&capturedGroupSize);

    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    groupSizeDesc.groupSize = {8, 4, 2};
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.pNext = &groupSizeDesc;
    groupCountDesc.groupCount = {2, 3, 4};

    ze_graph_node_handle_t hNode = node.toHandle();
    const void *updateDesc = &groupCountDesc;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));

    auto currentGroupCount = node.getCurrentGroupCount();
    EXPECT_EQ(2u, currentGroupCount.groupCountX);
    EXPECT_EQ(3u, currentGroupCount.groupCountY);
    EXPECT_EQ(4u, currentGroupCount.groupCountZ);
    auto currentGroupSize = node.getCurrentGroupSize();
    EXPECT_EQ(8u, currentGroupSize.groupSizeX);
    EXPECT_EQ(4u, currentGroupSize.groupSizeY);
    EXPECT_EQ(2u, currentGroupSize.groupSizeZ);
}

TEST(GraphNodesUpdateTest, GivenNodeWithZeroFlagsWhenUpdatingGroupCountThenGroupCountIsUpdated) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U));
    graph.stopCapturing();

    auto &node = *(*graph.graphNodes)[0];
    ze_group_count_t capturedGroupCount = {1, 1, 1};
    node.setGroupCounts(&capturedGroupCount);

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.groupCount = {2, 3, 4};
    ze_graph_node_handle_t hNode = node.toHandle();
    const void *updateDesc = &groupCountDesc;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    EXPECT_EQ(2u, capturedGroupCount.groupCountX);
    EXPECT_EQ(3u, capturedGroupCount.groupCountY);
    EXPECT_EQ(4u, capturedGroupCount.groupCountZ);
}

TEST(GraphNodesUpdateTest, GivenDescriptorForFlagNotSelectedOnNodeWhenUpdatingNodeThenInvalidArgumentIsReturnedAndNodeIsNotUpdated) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT));
    graph.stopCapturing();

    auto &groupSizeNode = *(*graph.graphNodes)[0];
    auto &groupCountNode = *(*graph.graphNodes)[1];
    ze_group_count_t capturedGroupCount = {1, 1, 1};
    ze_group_size_t capturedGroupSize = {1, 1, 1};
    groupSizeNode.setGroupCounts(&capturedGroupCount);
    groupCountNode.setGroupSizes(&capturedGroupSize);

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.groupCount = {2, 3, 4};
    ze_graph_node_handle_t hNode = groupSizeNode.toHandle();
    const void *updateDesc = &groupCountDesc;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    EXPECT_EQ(1u, capturedGroupCount.groupCountX);

    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    groupSizeDesc.groupSize = {8, 4, 2};
    hNode = groupCountNode.toHandle();
    updateDesc = &groupSizeDesc;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    EXPECT_EQ(1u, capturedGroupSize.groupSizeX);
}

TEST(GraphNodesUpdateTest, GivenUnsupportedUpdateDescriptorWhenUpdatingNodeThenUnsupportedFeatureIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U));
    graph.stopCapturing();

    ze_graph_node_update_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC};
    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    const void *updateDesc = &globalOffsetDesc;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
}

TEST(GraphNodesUpdateTest, GivenLaunchKernelDescriptorWhenUpdatingNodesThenOnlyGroupCountAndGroupSizeSelectedByNodeFlagsAreUpdated) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE));
    graph.stopCapturing();

    auto &groupCountNode = *(*graph.graphNodes)[0];
    auto &groupSizeNode = *(*graph.graphNodes)[1];
    std::array<ze_group_count_t, 2> capturedGroupCounts = {{{1, 1, 1}, {1, 1, 1}}};
    std::array<ze_group_size_t, 2> capturedGroupSizes = {{{1, 1, 1}, {1, 1, 1}}};
    groupCountNode.setGroupCounts(&capturedGroupCounts[0]);
    groupCountNode.setGroupSizes(&capturedGroupSizes[0]);
    groupSizeNode.setGroupCounts(&capturedGroupCounts[1]);
    groupSizeNode.setGroupSizes(&capturedGroupSizes[1]);

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupCount = {2, 3, 4};
    launchKernelDesc.groupSize = {8, 4, 2};
    std::array<ze_graph_node_handle_t, 2> hNodes = {groupCountNode.toHandle(), groupSizeNode.toHandle()};
    std::array<const void *, 2> updateDescs = {&launchKernelDesc, &launchKernelDesc};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 2, hNodes.data(), updateDescs.data(), nullptr));

    EXPECT_EQ(2u, capturedGroupCounts[0].groupCountX);
    EXPECT_EQ(3u, capturedGroupCounts[0].groupCountY);
    EXPECT_EQ(4u, capturedGroupCounts[0].groupCountZ);
    EXPECT_EQ(1u, capturedGroupSizes[0].groupSizeX);

    EXPECT_EQ(1u, capturedGroupCounts[1].groupCountX);
    EXPECT_EQ(8u, capturedGroupSizes[1].groupSizeX);
    EXPECT_EQ(4u, capturedGroupSizes[1].groupSizeY);
    EXPECT_EQ(2u, capturedGroupSizes[1].groupSizeZ);
}

TEST(GraphNodesUpdateTest, GivenLaunchKernelDescriptorAndNodeWithKernelInstructionFlagWhenUpdatingNodeThenUnsupportedFeatureIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_INSTRUCTION));
    graph.stopCapturing();

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    const void *updateDesc = &launchKernelDesc;
    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
}

TEST(GraphNodesUpdateTest, GivenInvalidDescriptorForSecondNodeWhenUpdatingNodesThenErrorIsReturnedAndFirstNodeIsNotUpdated) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT));
    graph.stopCapturing();

    ze_group_count_t capturedGroupCount = {1, 1, 1};
    (*graph.graphNodes)[0]->setGroupCounts(&capturedGroupCount);

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.groupCount = {2, 3, 4};
    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    std::array<ze_graph_node_handle_t, 2> hNodes = {(*graph.graphNodes)[0]->toHandle(), (*graph.graphNodes)[1]->toHandle()};
    std::array<const void *, 2> updateDescs = {&groupCountDesc, &groupSizeDesc};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 2, hNodes.data(), updateDescs.data(), nullptr));
    EXPECT_EQ(1u, capturedGroupCount.groupCountX);
    EXPECT_EQ(1u, capturedGroupCount.groupCountY);
    EXPECT_EQ(1u, capturedGroupCount.groupCountZ);
}

} // namespace ult
} // namespace L0
