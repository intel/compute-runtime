/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/api/internal/l0_graph.h"
#include "level_zero/core/test/unit_tests/experimental/test_graph.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_graph.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include <array>

namespace L0 {
namespace ult {

struct ExecutableGraphNodesUpdateFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        module = std::make_unique<Mock<Module>>(device, nullptr);
        kernel.setModule(module.get());
        executableGraph.src = &sourceGraph;
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    void setNumKernelArguments(size_t numKernelArguments) {
        kernel.descriptor.payloadMappings.explicitArgs.resize(numKernelArguments);
    }

    void setValueKernelArgument() {
        auto &explicitArgs = this->kernel.descriptor.payloadMappings.explicitArgs;
        explicitArgs.resize(1);
        explicitArgs[0] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
        explicitArgs[0].getTraits().argByValSize = sizeof(uint32_t);
    }

    GraphNode *createInstantiatedNode(ze_mutable_graph_node_exp_flags_t flags, uint64_t commandId, L0::CommandList *commandList) {
        auto &createdGraphNode = sourceGraph.recordedApiCommands.externalStorage.graphNode;
        createdGraphNode = std::make_unique<GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, flags);
        createdGraphNode->setKernel(&kernel);
        sourceGraph.storeCreatedGraphNode();
        executableGraph.getNodeCommands()->push_back({commandId, commandList});
        return sourceGraph.graphNodes->back().get();
    }

    ze_result_t updateNode(GraphNode &node, const void *pUpdateDesc) {
        ze_graph_node_handle_t nodeHandle = node.toHandle();
        return executableGraph.updateNodes(1u, &nodeHandle, &pUpdateDesc);
    }

    template <typename FamilyType>
    auto recordLastExecution(bool implicitSyncMutation) -> UltCommandStreamReceiver<FamilyType> & {
        auto &ultCsr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
        ultCsr.callBaseWaitForCompletionWithTimeout = false;
        this->executableGraph.implicitSyncMutation = implicitSyncMutation;
        this->executableGraph.lastExecutionCsr = &ultCsr;
        this->executableGraph.lastExecutionTaskCount = this->lastExecutionTaskCount;
        return ultCsr;
    }

    std::unique_ptr<Mock<Module>> module;
    Mock<KernelImp> kernel;
    ContextStubMock context;
    MockGraph sourceGraph{&context, true, true};
    MockExecutableGraph executableGraph;
    const size_t kernelArgumentSize = sizeof(uint32_t);
    const uint32_t kernelArgumentValue = 1u;
    const void *kernelArgumentValues = &kernelArgumentValue;
    const TaskCountType lastExecutionTaskCount = 5u;
};

using ExecutableGraphNodesUpdateTest = Test<ExecutableGraphNodesUpdateFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodesOfExecutableGraphWhenUpdatingThenDescriptorsArePassedToMutableCommandListWithNodeCommandIdAndFlagsAndCommandListIsRecordedOnce) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto defaultFlagsNode = createInstantiatedNode(0u, 1u, &commandList);
    auto groupSizeNode = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE, 2u, &commandList);

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    std::array<ze_graph_node_handle_t, 2> nodes = {defaultFlagsNode->toHandle(), groupSizeNode->toHandle()};
    std::array<const void *, 2> updateDescs = {&groupCountDesc, &groupSizeDesc};

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.updateNodes(2u, nodes.data(), updateDescs.data()));

    const auto &updates = commandList.mutableCommandList.recordedGraphNodeUpdates;
    ASSERT_EQ(2u, updates.size());
    EXPECT_EQ(1u, updates[0].commandId);
    EXPECT_EQ(0u, updates[0].flags);
    EXPECT_EQ(&groupCountDesc, updates[0].pUpdateDesc);
    EXPECT_EQ(2u, updates[1].commandId);
    EXPECT_EQ(static_cast<ze_mutable_graph_node_exp_flags_t>(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE), updates[1].flags);
    EXPECT_EQ(&groupSizeDesc, updates[1].pUpdateDesc);
    EXPECT_EQ(std::vector<L0::CommandList *>{&commandList}, executableGraph.updatedNodeCommandLists);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeWithKernelArgumentAndGlobalOffsetFlagsWhenUpdatingWithAllowedDescriptorChainThenChainIsPassedToMutableCommandList) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setNumKernelArguments(1u);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, 1u, &commandList);

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = &kernelArgumentSize;
    launchKernelDesc.pArgValues = &kernelArgumentValues;
    ze_graph_node_update_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC};
    globalOffsetDesc.pNext = &launchKernelDesc;
    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    kernelArgumentDesc.argIndex = 0u;
    kernelArgumentDesc.pNext = &globalOffsetDesc;

    EXPECT_EQ(ZE_RESULT_SUCCESS, updateNode(*node, &kernelArgumentDesc));

    const auto &updates = commandList.mutableCommandList.recordedGraphNodeUpdates;
    ASSERT_EQ(1u, updates.size());
    EXPECT_EQ(&kernelArgumentDesc, updates[0].pUpdateDesc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeNotInstantiatedInExecutableGraphWhenUpdatingThenInvalidArgumentIsReturned) {
    GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT};
    node.setKernel(&kernel);
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(node, &groupCountDesc));
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeOfOtherGraphWithIndexOfInstantiatedNodeWhenUpdatingThenInvalidArgumentIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto instantiatedNode = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);
    GraphNode otherGraphNode{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT};
    otherGraphNode.setKernel(&kernel);
    otherGraphNode.setIndex(instantiatedNode->getIndex());
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(otherGraphNode, &groupCountDesc));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenSecondNodeWithDescriptorNotAllowedByItsFlagsWhenUpdatingThenInvalidArgumentIsReturnedAndNoNodeIsUpdated) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto groupCountNode = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);
    auto secondGroupCountNode = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 2u, &commandList);

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    std::array<ze_graph_node_handle_t, 2> nodes = {groupCountNode->toHandle(), secondGroupCountNode->toHandle()};
    std::array<const void *, 2> updateDescs = {&groupCountDesc, &groupSizeDesc};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, executableGraph.updateNodes(2u, nodes.data(), updateDescs.data()));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenDescriptorsNotAllowedByNodeFlagsWhenUpdatingThenInvalidArgumentIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setNumKernelArguments(1u);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS, 1u, &commandList);

    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    ze_graph_node_update_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC};
    const std::array<const void *, 4> notAllowedDescs = {&kernelArgumentDesc, &groupCountDesc, &groupSizeDesc, &globalOffsetDesc};

    for (const void *updateDesc : notAllowedDescs) {
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(*node, updateDesc));
    }
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenKernelArgumentDescriptorWithIndexOutOfRangeWhenUpdatingThenInvalidArgumentIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setNumKernelArguments(1u);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, 1u, &commandList);

    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    kernelArgumentDesc.argIndex = 1u;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(*node, &kernelArgumentDesc));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenArgumentValueSmallerThanKernelArgumentRequiresWhenUpdatingWithKernelArgumentOrLaunchKernelDescriptorThenInvalidArgumentIsReturnedAndNothingIsPassedToMutableCommandList) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setNumKernelArguments(1u);
    auto &valueArgument = kernel.descriptor.payloadMappings.explicitArgs[0];
    valueArgument = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
    valueArgument.as<NEO::ArgDescValue>().elements.push_back({.size = sizeof(uint32_t), .sourceOffset = sizeof(uint32_t)});
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, 1u, &commandList);

    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    kernelArgumentDesc.argIndex = 0u;
    kernelArgumentDesc.argSize = kernelArgumentSize;
    kernelArgumentDesc.pArgValue = &kernelArgumentValue;
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = &kernelArgumentSize;
    launchKernelDesc.pArgValues = &kernelArgumentValues;
    const std::array<const void *, 2> tooSmallValueDescs = {&kernelArgumentDesc, &launchKernelDesc};

    for (const void *updateDesc : tooSmallValueDescs) {
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(*node, updateDesc));
    }
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeWithKernelInstructionFlagWhenUpdatingWithLaunchKernelDescriptorThenUnsupportedFeatureIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_INSTRUCTION, 1u, &commandList);

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, updateNode(*node, &launchKernelDesc));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeWithKernelArgumentFlagAndKernelWithArgumentsWhenUpdatingWithLaunchKernelDescriptorWithoutArgumentArraysThenInvalidArgumentIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setNumKernelArguments(1u);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, 1u, &commandList);

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelWithoutSizesDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelWithoutSizesDesc.pArgValues = &kernelArgumentValues;
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelWithoutValuesDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelWithoutValuesDesc.pArgSizes = &kernelArgumentSize;
    const std::array<const void *, 2> invalidDescs = {&launchKernelWithoutSizesDesc, &launchKernelWithoutValuesDesc};

    for (const void *updateDesc : invalidDescs) {
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(*node, updateDesc));
    }
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeWithKernelArgumentFlagAndKernelWithoutArgumentsWhenUpdatingWithLaunchKernelDescriptorWithoutArgumentArraysThenDescriptorIsPassedToMutableCommandList) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, 1u, &commandList);

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};

    EXPECT_EQ(ZE_RESULT_SUCCESS, updateNode(*node, &launchKernelDesc));
    EXPECT_EQ(1u, commandList.mutableCommandList.recordedGraphNodeUpdates.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeWithoutKernelArgumentFlagAndKernelWithArgumentsWhenUpdatingWithLaunchKernelDescriptorWithoutArgumentArraysThenDescriptorIsPassedToMutableCommandList) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setNumKernelArguments(1u);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};

    EXPECT_EQ(ZE_RESULT_SUCCESS, updateNode(*node, &launchKernelDesc));
    EXPECT_EQ(1u, commandList.mutableCommandList.recordedGraphNodeUpdates.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenDescriptorChainWithSecondDescriptorNotAllowedByNodeFlagsWhenUpdatingThenInvalidArgumentIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);

    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.pNext = &groupSizeDesc;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(*node, &groupCountDesc));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenUnsupportedUpdateDescriptorWhenUpdatingThenUnsupportedFeatureIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(0u, 1u, &commandList);

    ze_graph_node_update_signal_event_exp_desc_t signalEventDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_SIGNAL_EVENT_EXP_DESC};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, updateNode(*node, &signalEventDesc));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNullUpdateDescriptorWhenUpdatingThenInvalidArgumentIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(0u, 1u, &commandList);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(*node, nullptr));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNullNodeHandleWhenUpdatingThenInvalidArgumentIsReturned) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    std::array<ze_graph_node_handle_t, 2> nodes = {node->toHandle(), nullptr};
    std::array<const void *, 2> updateDescs = {&groupCountDesc, &groupCountDesc};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, executableGraph.updateNodes(2u, nodes.data(), updateDescs.data()));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenMutableCommandListUpdateFailureWhenUpdatingThenErrorIsReturnedAndCommandListIsRecordedForCommit) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    commandList.mutableCommandList.updateMutableCommandForGraphNodeResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, updateNode(*node, &groupCountDesc));
    EXPECT_EQ(std::vector<L0::CommandList *>{&commandList}, executableGraph.updatedNodeCommandLists);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenValidArgumentsWhenUpdatingExecutableGraphNodesThroughApiThenNodeIsUpdated) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    ze_graph_node_handle_t nodeHandle = node->toHandle();
    const void *updateDesc = &groupCountDesc;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeExecutableGraphNodesUpdateExp(executableGraph.toHandle(), 1u, &nodeHandle, &updateDesc, nullptr));
    const auto &updates = commandList.mutableCommandList.recordedGraphNodeUpdates;
    ASSERT_EQ(1u, updates.size());
    EXPECT_EQ(&groupCountDesc, updates[0].pUpdateDesc);
    EXPECT_EQ(std::vector<L0::CommandList *>{&commandList}, executableGraph.updatedNodeCommandLists);
}

TEST_F(ExecutableGraphNodesUpdateTest, givenUpdatedNodeCommandListsWhenCommittingThenEachCommandListIsClosedOnceAndListIsCleared) {
    Mock<CommandList> firstCommandList;
    Mock<CommandList> secondCommandList;
    executableGraph.updatedNodeCommandLists = {&firstCommandList, &secondCommandList};

    executableGraph.commitUpdatedNodeCommandLists();

    EXPECT_EQ(1u, firstCommandList.closeCalled);
    EXPECT_EQ(1u, secondCommandList.closeCalled);
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
}

TEST_F(ExecutableGraphNodesUpdateTest, givenNullArgumentsOrZeroNodesWhenUpdatingExecutableGraphNodesThenInvalidArgumentIsReturned) {
    ze_graph_node_handle_t nodeHandle = nullptr;
    const void *updateDesc = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphNodesUpdateExp(nullptr, 1u, &nodeHandle, &updateDesc, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphNodesUpdateExp(executableGraph.toHandle(), 0u, &nodeHandle, &updateDesc, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphNodesUpdateExp(executableGraph.toHandle(), 1u, nullptr, &updateDesc, nullptr));

    ze_graph_node_handle_t instantiatedNodeHandle = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, nullptr)->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphNodesUpdateExp(executableGraph.toHandle(), 1u, &instantiatedNodeHandle, nullptr, nullptr));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodesOfSourceGraphWhenUpdatingExecutableGraphFromSourceGraphThenCurrentNodeValuesAllowedByNodeFlagsAreAppliedWithNodeCommandIdAndCommandListIsRecordedOnce) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setValueKernelArgument();
    ze_group_count_t groupCount = {2u, 3u, 4u};
    ze_group_size_t groupSize = {8u, 4u, 2u};
    const uint64_t groupCountAndSizeCommandId = 7u;
    const uint64_t globalOffsetCommandId = 3u;
    auto groupCountAndSizeNode = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE, groupCountAndSizeCommandId, &commandList);
    groupCountAndSizeNode->setGroupCounts(&groupCount);
    groupCountAndSizeNode->setGroupSizes(&groupSize);
    createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, globalOffsetCommandId, &commandList);
    this->kernel.privateState.globalOffsets[0] = 5u;
    this->kernel.privateState.globalOffsets[1] = 6u;
    this->kernel.privateState.globalOffsets[2] = 7u;

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.update(sourceGraph));

    const auto &mutableCommandList = commandList.mutableCommandList;
    EXPECT_TRUE(mutableCommandList.recordedKernelArgumentMutations.empty());
    ASSERT_EQ(1u, mutableCommandList.recordedGroupCountMutations.size());
    const auto &groupCountMutation = mutableCommandList.recordedGroupCountMutations[0];
    EXPECT_EQ(groupCountAndSizeCommandId, groupCountMutation.commandId);
    EXPECT_EQ(groupCount.groupCountX, groupCountMutation.groupCount.groupCountX);
    EXPECT_EQ(groupCount.groupCountY, groupCountMutation.groupCount.groupCountY);
    EXPECT_EQ(groupCount.groupCountZ, groupCountMutation.groupCount.groupCountZ);
    ASSERT_EQ(1u, mutableCommandList.recordedGroupSizeMutations.size());
    const auto &groupSizeMutation = mutableCommandList.recordedGroupSizeMutations[0];
    EXPECT_EQ(groupCountAndSizeCommandId, groupSizeMutation.commandId);
    EXPECT_EQ(groupSize.groupSizeX, groupSizeMutation.groupSize[0]);
    EXPECT_EQ(groupSize.groupSizeY, groupSizeMutation.groupSize[1]);
    EXPECT_EQ(groupSize.groupSizeZ, groupSizeMutation.groupSize[2]);
    ASSERT_EQ(1u, mutableCommandList.recordedGlobalOffsetMutations.size());
    const auto &globalOffsetMutation = mutableCommandList.recordedGlobalOffsetMutations[0];
    EXPECT_EQ(globalOffsetCommandId, globalOffsetMutation.commandId);
    EXPECT_EQ(5u, globalOffsetMutation.globalOffset[0]);
    EXPECT_EQ(6u, globalOffsetMutation.globalOffset[1]);
    EXPECT_EQ(7u, globalOffsetMutation.globalOffset[2]);
    EXPECT_EQ(std::vector<L0::CommandList *>{&commandList}, executableGraph.updatedNodeCommandLists);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeWithKernelArgumentFlagAndArgumentsOfEachTypeWhenUpdatingExecutableGraphFromSourceGraphThenOnlyMutableArgumentsAreAppliedWithCurrentValues) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto &explicitArgs = this->kernel.descriptor.payloadMappings.explicitArgs;
    explicitArgs.resize(5);
    explicitArgs[0] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
    explicitArgs[0].getTraits().argByValSize = sizeof(uint32_t);
    explicitArgs[1] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTPointer};
    explicitArgs[1].getTraits().argByValSize = sizeof(void *);
    explicitArgs[1].getTraits().addressQualifier = NEO::KernelArgMetadata::AddrGlobal;
    explicitArgs[2] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTPointer};
    explicitArgs[2].getTraits().argByValSize = sizeof(void *);
    explicitArgs[2].getTraits().addressQualifier = NEO::KernelArgMetadata::AddrLocal;
    explicitArgs[3] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTImage};
    explicitArgs[3].getTraits().argByValSize = sizeof(void *);
    explicitArgs[4] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTSampler};
    explicitArgs[4].getTraits().argByValSize = sizeof(void *);

    uint32_t capturedValue = 1u;
    void *capturedBuffer = reinterpret_cast<void *>(0x1000);
    size_t capturedSlmSize = 64u;
    void *capturedImage = reinterpret_cast<void *>(0x2000);
    void *capturedSampler = reinterpret_cast<void *>(0x3000);
    std::array<void *, 5> capturedArguments = {&capturedValue, &capturedBuffer, &capturedSlmSize, &capturedImage, &capturedSampler};
    L0::ClosureExternalStorage storage;
    const auto argumentsId = storage.registerKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), capturedArguments.data());

    const uint64_t commandId = 5u;
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, commandId, &commandList);
    node->setExternalStorage(&storage);
    node->setArgumentsId(argumentsId);

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.update(sourceGraph));

    const auto &mutableCommandList = commandList.mutableCommandList;
    const auto &kernelArgumentMutations = mutableCommandList.recordedKernelArgumentMutations;
    ASSERT_EQ(3u, kernelArgumentMutations.size());
    const auto &valueMutation = kernelArgumentMutations[0];
    EXPECT_EQ(commandId, valueMutation.commandId);
    EXPECT_EQ(0u, valueMutation.argIndex);
    EXPECT_EQ(sizeof(uint32_t), valueMutation.argSize);
    ASSERT_NE(nullptr, valueMutation.argValue);
    EXPECT_EQ(capturedValue, *static_cast<const uint32_t *>(valueMutation.argValue));
    const auto &bufferMutation = kernelArgumentMutations[1];
    EXPECT_EQ(commandId, bufferMutation.commandId);
    EXPECT_EQ(1u, bufferMutation.argIndex);
    EXPECT_EQ(sizeof(void *), bufferMutation.argSize);
    ASSERT_NE(nullptr, bufferMutation.argValue);
    EXPECT_EQ(capturedBuffer, *static_cast<void *const *>(bufferMutation.argValue));
    const auto &slmMutation = kernelArgumentMutations[2];
    EXPECT_EQ(commandId, slmMutation.commandId);
    EXPECT_EQ(2u, slmMutation.argIndex);
    EXPECT_EQ(capturedSlmSize, slmMutation.argSize);
    EXPECT_EQ(nullptr, slmMutation.argValue);
    EXPECT_TRUE(mutableCommandList.recordedGroupCountMutations.empty());
    EXPECT_TRUE(mutableCommandList.recordedGroupSizeMutations.empty());
    EXPECT_TRUE(mutableCommandList.recordedGlobalOffsetMutations.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenGraphOtherThanSourceGraphWhenUpdatingExecutableGraphThenUnsupportedFeatureIsReturnedAndNothingIsApplied) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    ze_group_count_t groupCount = {2u, 3u, 4u};
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);
    node->setGroupCounts(&groupCount);
    MockGraph otherGraph{&context, true, true};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, executableGraph.update(otherGraph));
    EXPECT_TRUE(commandList.mutableCommandList.recordedGroupCountMutations.empty());
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
}

TEST_F(ExecutableGraphNodesUpdateTest, givenSourceGraphWithoutNodesWhenUpdatingExecutableGraphFromSourceGraphThenSuccessIsReturnedAndNoCommandListIsRecorded) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.update(sourceGraph));
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenMutationFailureForEachMutableNodeFlagWhenUpdatingExecutableGraphFromSourceGraphThenErrorIsReturnedAndCommandListIsRecorded) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    commandList.mutableCommandList.applyGraphNodeMutationResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    setValueKernelArgument();
    ze_group_count_t groupCount = {2u, 3u, 4u};
    const std::array<ze_mutable_graph_node_exp_flags_t, 4> mutableNodeFlags = {ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT,
                                                                               ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET};

    for (const auto nodeFlags : mutableNodeFlags) {
        sourceGraph.graphNodes->clear();
        executableGraph.getNodeCommands()->clear();
        executableGraph.updatedNodeCommandLists.clear();
        auto node = createInstantiatedNode(nodeFlags, 1u, &commandList);
        node->setGroupCounts(&groupCount);

        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, executableGraph.update(sourceGraph));
        EXPECT_EQ(std::vector<L0::CommandList *>{&commandList}, executableGraph.updatedNodeCommandLists);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeWithDefaultFlagsWhenUpdatingExecutableGraphFromSourceGraphThenKernelArgumentGroupCountGroupSizeAndGlobalOffsetAreApplied) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setValueKernelArgument();
    ze_group_count_t groupCount = {2u, 3u, 4u};
    const uint64_t commandId = 9u;
    auto node = createInstantiatedNode(0u, commandId, &commandList);
    node->setGroupCounts(&groupCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.update(sourceGraph));

    const auto &mutableCommandList = commandList.mutableCommandList;
    ASSERT_EQ(1u, mutableCommandList.recordedKernelArgumentMutations.size());
    EXPECT_EQ(commandId, mutableCommandList.recordedKernelArgumentMutations[0].commandId);
    ASSERT_EQ(1u, mutableCommandList.recordedGroupCountMutations.size());
    EXPECT_EQ(commandId, mutableCommandList.recordedGroupCountMutations[0].commandId);
    ASSERT_EQ(1u, mutableCommandList.recordedGroupSizeMutations.size());
    EXPECT_EQ(commandId, mutableCommandList.recordedGroupSizeMutations[0].commandId);
    ASSERT_EQ(1u, mutableCommandList.recordedGlobalOffsetMutations.size());
    EXPECT_EQ(commandId, mutableCommandList.recordedGlobalOffsetMutations[0].commandId);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodeWithoutLaunchKernelFlagsWhenUpdatingExecutableGraphFromSourceGraphThenNoMutationIsAppliedAndCommandListIsRecorded) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    setValueKernelArgument();
    createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS, 1u, &commandList);

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.update(sourceGraph));

    const auto &mutableCommandList = commandList.mutableCommandList;
    EXPECT_TRUE(mutableCommandList.recordedKernelArgumentMutations.empty());
    EXPECT_TRUE(mutableCommandList.recordedGroupCountMutations.empty());
    EXPECT_TRUE(mutableCommandList.recordedGroupSizeMutations.empty());
    EXPECT_TRUE(mutableCommandList.recordedGlobalOffsetMutations.empty());
    EXPECT_EQ(std::vector<L0::CommandList *>{&commandList}, executableGraph.updatedNodeCommandLists);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenNodesOnDifferentCommandListsWhenUpdatingExecutableGraphFromSourceGraphThenEachCommandListIsRecordedOnce) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> firstCommandList(this->device);
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> secondCommandList(this->device);
    createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, 1u, &firstCommandList);
    createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, 1u, &secondCommandList);
    createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, 2u, &firstCommandList);

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.update(sourceGraph));

    const std::vector<L0::CommandList *> expectedCommandLists = {&firstCommandList, &secondCommandList};
    EXPECT_EQ(expectedCommandLists, executableGraph.updatedNodeCommandLists);
}

TEST_F(ExecutableGraphNodesUpdateTest, givenNullHandlesWhenUpdatingExecutableGraphFromSourceGraphThenInvalidArgumentIsReturned) {
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphUpdateExp(nullptr, sourceGraph.toHandle(), nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphUpdateExp(executableGraph.toHandle(), nullptr, nullptr));
}

TEST_F(ExecutableGraphNodesUpdateTest, givenSourceGraphAndOtherGraphHandlesWhenUpdatingExecutableGraphThenSuccessIsReturnedOnlyForSourceGraph) {
    MockGraph otherGraph{&context, true, true};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeExecutableGraphUpdateExp(executableGraph.toHandle(), sourceGraph.toHandle(), nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeExecutableGraphUpdateExp(executableGraph.toHandle(), otherGraph.toHandle(), nullptr));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenImplicitSyncMutationAndExecutedGraphWhenUpdatingNodesThenLastExecutionIsWaitedForBeforeNodesAreUpdated) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);
    auto &ultCsr = recordLastExecution<FamilyType>(true);
    const auto waitCallsBefore = ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load();
    bool nodesUpdatedBeforeWait = true;
    ultCsr.onWaitForCompletionWithTimeout = [&]() { nodesUpdatedBeforeWait = !commandList.mutableCommandList.recordedGraphNodeUpdates.empty(); };

    const ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    EXPECT_EQ(ZE_RESULT_SUCCESS, updateNode(*node, &groupCountDesc));

    EXPECT_EQ(waitCallsBefore + 1u, ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(lastExecutionTaskCount, ultCsr.latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_FALSE(nodesUpdatedBeforeWait);
    EXPECT_EQ(1u, commandList.mutableCommandList.recordedGraphNodeUpdates.size());
    EXPECT_EQ(nullptr, executableGraph.lastExecutionCsr);
    ultCsr.onWaitForCompletionWithTimeout = nullptr;
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenImplicitSyncMutationAndGpuHangWhenUpdatingNodesThenDeviceLostIsReturnedAndNodesAreNotUpdated) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);
    auto &ultCsr = recordLastExecution<FamilyType>(true);
    ultCsr.returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;

    const ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, updateNode(*node, &groupCountDesc));

    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
    EXPECT_EQ(&ultCsr, executableGraph.lastExecutionCsr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenImplicitSyncMutationAndDescriptorNotAllowedForSecondNodeWhenUpdatingNodesThenLastExecutionIsNotWaitedFor) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto allowedNode = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);
    auto notAllowedNode = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE, 2u, &commandList);
    auto &ultCsr = recordLastExecution<FamilyType>(true);
    const auto waitCallsBefore = ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load();

    const ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    std::array<ze_graph_node_handle_t, 2> nodes = {allowedNode->toHandle(), notAllowedNode->toHandle()};
    std::array<const void *, 2> updateDescs = {&groupCountDesc, &groupCountDesc};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, executableGraph.updateNodes(2u, nodes.data(), updateDescs.data()));

    EXPECT_EQ(waitCallsBefore, ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_TRUE(commandList.mutableCommandList.recordedGraphNodeUpdates.empty());
    EXPECT_EQ(&ultCsr, executableGraph.lastExecutionCsr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenImplicitSyncMutationDisabledAndExecutedGraphWhenUpdatingNodesThenLastExecutionIsNotWaitedForAndNodesAreUpdated) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    auto node = createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, 1u, &commandList);
    auto &ultCsr = recordLastExecution<FamilyType>(false);
    const auto waitCallsBefore = ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load();

    const ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    EXPECT_EQ(ZE_RESULT_SUCCESS, updateNode(*node, &groupCountDesc));

    EXPECT_EQ(waitCallsBefore, ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(1u, commandList.mutableCommandList.recordedGraphNodeUpdates.size());
    EXPECT_EQ(&ultCsr, executableGraph.lastExecutionCsr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenImplicitSyncMutationAndExecutedGraphWhenUpdatingExecutableGraphFromSourceGraphThenLastExecutionIsWaitedForBeforeNodesAreUpdated) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, 1u, &commandList);
    auto &ultCsr = recordLastExecution<FamilyType>(true);
    const auto waitCallsBefore = ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load();
    bool nodesUpdatedBeforeWait = true;
    ultCsr.onWaitForCompletionWithTimeout = [&]() { nodesUpdatedBeforeWait = !commandList.mutableCommandList.recordedGlobalOffsetMutations.empty(); };

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.update(sourceGraph));

    EXPECT_EQ(waitCallsBefore + 1u, ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(lastExecutionTaskCount, ultCsr.latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_FALSE(nodesUpdatedBeforeWait);
    EXPECT_EQ(1u, commandList.mutableCommandList.recordedGlobalOffsetMutations.size());
    EXPECT_EQ(nullptr, executableGraph.lastExecutionCsr);
    ultCsr.onWaitForCompletionWithTimeout = nullptr;
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenImplicitSyncMutationAndGpuHangWhenUpdatingExecutableGraphFromSourceGraphThenDeviceLostIsReturnedAndNodesAreNotUpdated) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, 1u, &commandList);
    auto &ultCsr = recordLastExecution<FamilyType>(true);
    ultCsr.returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, executableGraph.update(sourceGraph));

    EXPECT_TRUE(commandList.mutableCommandList.recordedGlobalOffsetMutations.empty());
    EXPECT_TRUE(executableGraph.updatedNodeCommandLists.empty());
    EXPECT_EQ(&ultCsr, executableGraph.lastExecutionCsr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenImplicitSyncMutationAndOtherSourceGraphWhenUpdatingExecutableGraphThenUnsupportedFeatureIsReturnedAndLastExecutionIsNotWaitedFor) {
    MockGraph otherGraph{&context, true, true};
    auto &ultCsr = recordLastExecution<FamilyType>(true);
    const auto waitCallsBefore = ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, executableGraph.update(otherGraph));

    EXPECT_EQ(waitCallsBefore, ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(&ultCsr, executableGraph.lastExecutionCsr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ExecutableGraphNodesUpdateTest, givenImplicitSyncMutationDisabledAndExecutedGraphWhenUpdatingExecutableGraphFromSourceGraphThenLastExecutionIsNotWaitedForAndNodesAreUpdated) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> commandList(this->device);
    createInstantiatedNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, 1u, &commandList);
    auto &ultCsr = recordLastExecution<FamilyType>(false);
    const auto waitCallsBefore = ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load();

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.update(sourceGraph));

    EXPECT_EQ(waitCallsBefore, ultCsr.waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(1u, commandList.mutableCommandList.recordedGlobalOffsetMutations.size());
    EXPECT_EQ(&ultCsr, executableGraph.lastExecutionCsr);
}

} // namespace ult
} // namespace L0
