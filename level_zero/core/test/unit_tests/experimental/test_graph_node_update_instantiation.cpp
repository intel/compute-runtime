/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/internal/l0_graph.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/core/source/mutable_cmdlist/variable_dispatch.h"
#include "level_zero/core/test/unit_tests/experimental/test_graph.h"
#include "level_zero/core/test/unit_tests/mocks/mock_graph.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_cmdlist.h"
#include "level_zero/driver_experimental/zex_graph.h"

#include <array>
#include <cstring>
#include <vector>

namespace L0 {
namespace ult {

struct GraphNodeUpdateInstantiationFixtureInit : public MutableCommandListFixture<false, -1> {
    static constexpr uint32_t capturedNodesCount = 2U;
    static constexpr size_t argumentsAppendIndex = 0U;
    static constexpr size_t parametersAppendIndex = 1U;
    static constexpr uint32_t bufferArgumentIndex = 0U;
    static constexpr uint32_t valueArgumentIndex = 1U;
    static constexpr uint32_t kernelArgumentsCount = 2U;
    static constexpr std::array<size_t, kernelArgumentsCount> kernelArgumentSizes = {sizeof(void *), sizeof(uint32_t)};
    static constexpr size_t bufferSize = 4096U;

    using NodeHandles = std::array<ze_graph_node_handle_t, capturedNodesCount>;

    void setUp(ze_mutable_graph_node_exp_flags_t supportedMutableGraphNodeFlags) {
        MutableCommandListFixture<false, -1>::setUp();
        this->nextFreeCrossThreadDataOffset = this->crossThreadOffset;
        NEO::debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(supportedMutableGraphNodeFlags));
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(this->context->toHandle(), this->device->toHandle(), &cmdQueueDesc, &this->immCmdListHandle));
    }

    void tearDown() {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(this->immCmdListHandle));
        MutableCommandListFixture<false, -1>::tearDown();
    }

    auto setupKernelArguments() -> void {
        ASSERT_EQ(this->crossThreadOffset, this->nextFreeCrossThreadDataOffset);
        this->resizeKernelArg(kernelArgumentsCount);
        this->prepareKernelArg(bufferArgumentIndex, L0::MCL::VariableType::buffer, kernel1Bit);
        this->prepareKernelArg(valueArgumentIndex, L0::MCL::VariableType::value, kernel1Bit);
        this->nextFreeCrossThreadDataOffset += static_cast<CrossThreadDataOffset>(kernelArgumentsCount * this->nextArgOffset);

        this->capturedBuffer = this->allocateUsm(bufferSize);
        ASSERT_NE(nullptr, this->capturedBuffer);
        this->capturedArguments = {&this->capturedBuffer, &this->capturedValue};
    }

    auto setupDispatchTraitsCrossThreadDataOffsets() -> void {
        constexpr uint32_t channelsCount = 3U;
        constexpr CrossThreadDataOffset channelsSize = channelsCount * sizeof(uint32_t);
        auto &dispatchTraits = this->mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
        this->fillOffsets(dispatchTraits.globalWorkSize, this->nextFreeCrossThreadDataOffset, channelsCount);
        this->nextFreeCrossThreadDataOffset += channelsSize;
        this->fillOffsets(dispatchTraits.numWorkGroups, this->nextFreeCrossThreadDataOffset, channelsCount);
        this->nextFreeCrossThreadDataOffset += channelsSize;
        this->fillOffsets(dispatchTraits.enqueuedLocalWorkSize, this->nextFreeCrossThreadDataOffset, channelsCount);
        this->nextFreeCrossThreadDataOffset += channelsSize;
        this->fillOffsets(dispatchTraits.globalWorkOffset, this->nextFreeCrossThreadDataOffset, channelsCount);
        this->nextFreeCrossThreadDataOffset += channelsSize;
    }

    auto captureLaunchKernelNodes(L0::Graph &srcGraph, ze_mutable_graph_node_exp_flags_t nodeFlags, void **kernelArguments) -> NodeHandles {
        NodeHandles nodeHandles = {nullptr, nullptr};
        ze_graph_node_exp_desc_t argumentsNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
        argumentsNodeDesc.flags = nodeFlags;
        argumentsNodeDesc.phNodeHandle = &nodeHandles[argumentsAppendIndex];
        ze_graph_node_exp_desc_t parametersNodeDesc = argumentsNodeDesc;
        parametersNodeDesc.phNodeHandle = &nodeHandles[parametersAppendIndex];

        auto srcGraphHandle = srcGraph.toHandle();
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(this->immCmdListHandle, srcGraphHandle, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(this->immCmdListHandle, this->kernelHandle, this->testGroupCount, this->testGroupSize, kernelArguments, &argumentsNodeDesc, nullptr, 0U, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(this->immCmdListHandle, this->kernelHandle, &this->testGroupCount, &parametersNodeDesc, nullptr, 0U, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(this->immCmdListHandle, &srcGraphHandle, nullptr));
        return nodeHandles;
    }

    auto getNodeCommand(MockExecutableGraph &execGraph, ze_graph_node_handle_t nodeHandle) -> const ExecutableGraphNodeCommand & {
        return (*execGraph.getNodeCommands())[GraphNode::fromHandle(nodeHandle)->getIndex()];
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    auto getSharedMutableCommandList(MockExecutableGraph &execGraph, const NodeHandles &nodeHandles) -> MutableCommandListCoreFamily<gfxCoreFamily> * {
        auto argumentsCommandList = this->getNodeCommand(execGraph, nodeHandles[argumentsAppendIndex]).commandList;
        auto parametersCommandList = this->getNodeCommand(execGraph, nodeHandles[parametersAppendIndex]).commandList;
        EXPECT_NE(nullptr, argumentsCommandList);
        EXPECT_EQ(argumentsCommandList, parametersCommandList);
        if ((nullptr == argumentsCommandList) || (argumentsCommandList != parametersCommandList)) {
            return nullptr;
        }
        EXPECT_NE(nullptr, argumentsCommandList->asMutable());
        if (nullptr == argumentsCommandList->asMutable()) {
            return nullptr;
        }
        auto mutableCommandList = static_cast<MutableCommandListCoreFamily<gfxCoreFamily> *>(argumentsCommandList);
        EXPECT_EQ(capturedNodesCount, mutableCommandList->dispatches.size());
        EXPECT_EQ(capturedNodesCount, mutableCommandList->mutableWalkerCmds.size());
        if ((capturedNodesCount != mutableCommandList->dispatches.size()) || (capturedNodesCount != mutableCommandList->mutableWalkerCmds.size())) {
            return nullptr;
        }
        return mutableCommandList;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    auto getKernelVariables(MutableCommandListCoreFamily<gfxCoreFamily> &mutableCommandList, uint64_t commandId) -> L0::MCL::KernelVariableDescriptor * {
        EXPECT_NE(0u, commandId);
        EXPECT_LE(commandId, mutableCommandList.kernelMutations.size());
        if ((0u == commandId) || (commandId > mutableCommandList.kernelMutations.size())) {
            return nullptr;
        }
        return mutableCommandList.getVariableDescriptorContainer(mutableCommandList.kernelMutations[commandId - 1]);
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    auto expectDispatchOfVariableUsesWalker(MutableCommandListCoreFamily<gfxCoreFamily> &mutableCommandList, const L0::MCL::Variable &variable, size_t appendIndex) -> void {
        const auto &kernelDispatch = *mutableCommandList.dispatches[appendIndex];
        EXPECT_EQ(std::vector<L0::MCL::VariableDispatch *>{kernelDispatch.varDispatch.get()}, variable.getDispatches());
        EXPECT_EQ(mutableCommandList.mutableWalkerCmds[appendIndex]->getWalkerCmdPointer(), kernelDispatch.walkerCmd);
    }

    auto getCrossThreadDataValues(const L0::MCL::KernelDispatch &kernelDispatch, CrossThreadDataOffset offset) -> const uint32_t * {
        return reinterpret_cast<const uint32_t *>(ptrOffset(kernelDispatch.varDispatch->getIndirectData()->getCrossThreadDataBaseAddress(), offset));
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    auto expectGlobalWorkSizeOfMutableCommand(MutableCommandListCoreFamily<gfxCoreFamily> &mutableCommandList, size_t appendIndex, const ze_group_count_t &expectedGroupCount, const ze_group_size_t &expectedGroupSize) -> void {
        const auto &kernelDispatch = *mutableCommandList.dispatches[appendIndex];
        ASSERT_EQ(0u, kernelDispatch.varDispatch->getIndirectData()->getInlineDataSize());
        const auto globalWorkSize = this->getCrossThreadDataValues(kernelDispatch, kernelDispatch.varDispatch->getIndirectDataOffsets().globalWorkSize[0]);
        EXPECT_EQ(expectedGroupCount.groupCountX * expectedGroupSize.groupSizeX, globalWorkSize[0]);
        EXPECT_EQ(expectedGroupCount.groupCountY * expectedGroupSize.groupSizeY, globalWorkSize[1]);
        EXPECT_EQ(expectedGroupCount.groupCountZ * expectedGroupSize.groupSizeZ, globalWorkSize[2]);
    }

    template <typename FamilyType>
    auto expectGroupCountOfMutableCommand(MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> &mutableCommandList, uint64_t commandId, size_t appendIndex, const ze_group_count_t &expectedGroupCount) -> void {
        using WalkerType = typename FamilyType::PorWalkerType;

        const auto kernelVariables = this->getKernelVariables(mutableCommandList, commandId);
        ASSERT_NE(nullptr, kernelVariables);
        const auto groupCountVariable = kernelVariables->groupCount;
        ASSERT_NE(nullptr, groupCountVariable);
        EXPECT_EQ(expectedGroupCount.groupCountX, groupCountVariable->getDesc().kernelDispatch.groupCount[0]);
        EXPECT_EQ(expectedGroupCount.groupCountY, groupCountVariable->getDesc().kernelDispatch.groupCount[1]);
        EXPECT_EQ(expectedGroupCount.groupCountZ, groupCountVariable->getDesc().kernelDispatch.groupCount[2]);
        EXPECT_EQ(groupCountVariable, mutableCommandList.dispatches[appendIndex]->varDispatch->getGroupCountVar());
        this->expectDispatchOfVariableUsesWalker(mutableCommandList, *groupCountVariable, appendIndex);

        const auto &kernelDispatch = *mutableCommandList.dispatches[appendIndex];
        ASSERT_EQ(0u, kernelDispatch.varDispatch->getIndirectData()->getInlineDataSize());
        const auto numWorkGroups = this->getCrossThreadDataValues(kernelDispatch, kernelDispatch.varDispatch->getIndirectDataOffsets().numWorkGroups[0]);
        EXPECT_EQ(expectedGroupCount.groupCountX, numWorkGroups[0]);
        EXPECT_EQ(expectedGroupCount.groupCountY, numWorkGroups[1]);
        EXPECT_EQ(expectedGroupCount.groupCountZ, numWorkGroups[2]);

        const auto walker = reinterpret_cast<WalkerType *>(kernelDispatch.walkerCmd);
        EXPECT_EQ(expectedGroupCount.groupCountX, walker->getThreadGroupIdXDimension());
        EXPECT_EQ(expectedGroupCount.groupCountY, walker->getThreadGroupIdYDimension());
        EXPECT_EQ(expectedGroupCount.groupCountZ, walker->getThreadGroupIdZDimension());
    }

    template <typename FamilyType>
    auto expectGroupSizeOfMutableCommand(MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> &mutableCommandList, uint64_t commandId, size_t appendIndex, const ze_group_size_t &expectedGroupSize) -> void {
        using WalkerType = typename FamilyType::PorWalkerType;

        const auto kernelVariables = this->getKernelVariables(mutableCommandList, commandId);
        ASSERT_NE(nullptr, kernelVariables);
        const auto groupSizeVariable = kernelVariables->groupSize;
        ASSERT_NE(nullptr, groupSizeVariable);
        EXPECT_EQ(expectedGroupSize.groupSizeX, groupSizeVariable->getDesc().kernelDispatch.groupSize[0]);
        EXPECT_EQ(expectedGroupSize.groupSizeY, groupSizeVariable->getDesc().kernelDispatch.groupSize[1]);
        EXPECT_EQ(expectedGroupSize.groupSizeZ, groupSizeVariable->getDesc().kernelDispatch.groupSize[2]);
        EXPECT_EQ(groupSizeVariable, mutableCommandList.dispatches[appendIndex]->varDispatch->getGroupSizeVar());
        this->expectDispatchOfVariableUsesWalker(mutableCommandList, *groupSizeVariable, appendIndex);

        const auto &kernelDispatch = *mutableCommandList.dispatches[appendIndex];
        ASSERT_EQ(0u, kernelDispatch.varDispatch->getIndirectData()->getInlineDataSize());
        const auto enqueuedLocalWorkSize = this->getCrossThreadDataValues(kernelDispatch, kernelDispatch.varDispatch->getIndirectDataOffsets().enqLocalWorkSize[0]);
        EXPECT_EQ(expectedGroupSize.groupSizeX, enqueuedLocalWorkSize[0]);
        EXPECT_EQ(expectedGroupSize.groupSizeY, enqueuedLocalWorkSize[1]);
        EXPECT_EQ(expectedGroupSize.groupSizeZ, enqueuedLocalWorkSize[2]);

        const auto walker = reinterpret_cast<WalkerType *>(kernelDispatch.walkerCmd);
        EXPECT_EQ(expectedGroupSize.groupSizeX - 1, walker->getLocalXMaximum());
        EXPECT_EQ(expectedGroupSize.groupSizeY - 1, walker->getLocalYMaximum());
        EXPECT_EQ(expectedGroupSize.groupSizeZ - 1, walker->getLocalZMaximum());
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    auto expectGlobalOffsetOfMutableCommand(MutableCommandListCoreFamily<gfxCoreFamily> &mutableCommandList, uint64_t commandId, size_t appendIndex, const ze_graph_node_update_global_offset_exp_desc_t &expectedGlobalOffset) -> void {
        const auto kernelVariables = this->getKernelVariables(mutableCommandList, commandId);
        ASSERT_NE(nullptr, kernelVariables);
        const auto globalOffsetVariable = kernelVariables->globalOffset;
        ASSERT_NE(nullptr, globalOffsetVariable);
        EXPECT_EQ(expectedGlobalOffset.offsetX, globalOffsetVariable->getDesc().kernelDispatch.globalOffset[0]);
        EXPECT_EQ(expectedGlobalOffset.offsetY, globalOffsetVariable->getDesc().kernelDispatch.globalOffset[1]);
        EXPECT_EQ(expectedGlobalOffset.offsetZ, globalOffsetVariable->getDesc().kernelDispatch.globalOffset[2]);
        EXPECT_EQ(globalOffsetVariable, mutableCommandList.dispatches[appendIndex]->varDispatch->getGlobalOffsetVar());
        this->expectDispatchOfVariableUsesWalker(mutableCommandList, *globalOffsetVariable, appendIndex);

        const auto &kernelDispatch = *mutableCommandList.dispatches[appendIndex];
        ASSERT_EQ(0u, kernelDispatch.varDispatch->getIndirectData()->getInlineDataSize());
        const auto globalWorkOffset = this->getCrossThreadDataValues(kernelDispatch, kernelDispatch.varDispatch->getIndirectDataOffsets().globalWorkOffset[0]);
        EXPECT_EQ(expectedGlobalOffset.offsetX, globalWorkOffset[0]);
        EXPECT_EQ(expectedGlobalOffset.offsetY, globalWorkOffset[1]);
        EXPECT_EQ(expectedGlobalOffset.offsetZ, globalWorkOffset[2]);
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    auto expectKernelArgumentsOfMutableCommand(MutableCommandListCoreFamily<gfxCoreFamily> &mutableCommandList, uint64_t commandId, size_t appendIndex, void *expectedBuffer, uint32_t expectedValue) -> void {
        const auto kernelVariables = this->getKernelVariables(mutableCommandList, commandId);
        ASSERT_NE(nullptr, kernelVariables);
        const auto &kernelArgumentVariables = kernelVariables->kernelArguments;
        ASSERT_EQ(kernelArgumentsCount, kernelArgumentVariables.size());
        const auto bufferVariable = kernelArgumentVariables[bufferArgumentIndex].kernelArgumentVariable;
        ASSERT_NE(nullptr, bufferVariable);
        EXPECT_EQ(L0::MCL::VariableType::buffer, bufferVariable->getDesc().type);
        ASSERT_EQ(1u, bufferVariable->getBufferUsages().statelessWithoutOffset.size());
        const auto valueVariable = kernelArgumentVariables[valueArgumentIndex].kernelArgumentVariable;
        ASSERT_NE(nullptr, valueVariable);
        EXPECT_EQ(L0::MCL::VariableType::value, valueVariable->getDesc().type);
        ASSERT_EQ(1u, valueVariable->getValueUsages().statelessWithoutOffset.size());

        const auto &kernelDispatch = *mutableCommandList.dispatches[appendIndex];
        ASSERT_FALSE(kernelDispatch.kernelData->passInlineData);
        const auto kernelIndirectDataBegin = reinterpret_cast<uintptr_t>(kernelDispatch.indirectObjectHeap.begin());
        const auto kernelIndirectDataEnd = reinterpret_cast<uintptr_t>(kernelDispatch.indirectObjectHeap.end());

        const auto bufferPatchAddress = static_cast<uintptr_t>(bufferVariable->getBufferUsages().statelessWithoutOffset[0]);
        uint64_t patchedBufferAddress = 0U;
        ASSERT_LE(kernelIndirectDataBegin, bufferPatchAddress);
        ASSERT_LE(bufferPatchAddress + sizeof(patchedBufferAddress), kernelIndirectDataEnd);
        memcpy(&patchedBufferAddress, reinterpret_cast<const void *>(bufferPatchAddress), sizeof(patchedBufferAddress));
        EXPECT_EQ(reinterpret_cast<uint64_t>(expectedBuffer), patchedBufferAddress);

        const auto valuePatchAddress = static_cast<uintptr_t>(valueVariable->getValueUsages().statelessWithoutOffset[0]);
        uint32_t patchedValue = 0U;
        ASSERT_LE(kernelIndirectDataBegin, valuePatchAddress);
        ASSERT_LE(valuePatchAddress + sizeof(patchedValue), kernelIndirectDataEnd);
        memcpy(&patchedValue, reinterpret_cast<const void *>(valuePatchAddress), sizeof(patchedValue));
        EXPECT_EQ(expectedValue, patchedValue);
    }

    const ze_group_size_t testGroupSize = {1U, 1U, 1U};
    void *capturedBuffer = nullptr;
    uint32_t capturedValue = 1U;
    std::array<void *, kernelArgumentsCount> capturedArguments = {nullptr, nullptr};
    ze_command_list_handle_t immCmdListHandle = nullptr;
    CrossThreadDataOffset nextFreeCrossThreadDataOffset = 0U;
    GraphsCleanupGuard graphCleanup;
};

template <ze_mutable_graph_node_exp_flags_t supportedMutableGraphNodeFlags>
struct GraphNodeUpdateInstantiationFixture : public GraphNodeUpdateInstantiationFixtureInit {
    void setUp() {
        GraphNodeUpdateInstantiationFixtureInit::setUp(supportedMutableGraphNodeFlags);
    }
};

using GraphNodeGroupCountUpdateInstantiationTest = Test<GraphNodeUpdateInstantiationFixture<ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeGroupCountUpdateInstantiationTest, givenGroupCountOfGraphNodesUpdatedWhenInstantiatingExecutableGraphThenMutableCommandsUseUpdatedGroupCount) {
    this->setupDispatchTraitsCrossThreadDataOffsets();

    L0::Graph srcGraph(this->context, true, true);
    auto nodeHandles = this->captureLaunchKernelNodes(srcGraph, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, nullptr);
    ASSERT_NE(nullptr, nodeHandles[argumentsAppendIndex]);
    ASSERT_NE(nullptr, nodeHandles[parametersAppendIndex]);

    const ze_group_count_t launchKernelUpdateGroupCount = {2U, 3U, 4U};
    const ze_group_count_t groupCountUpdateGroupCount = {5U, 6U, 7U};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupCount = launchKernelUpdateGroupCount;
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.groupCount = groupCountUpdateGroupCount;
    std::array<const void *, capturedNodesCount> updateDescs = {&launchKernelDesc, &groupCountDesc};
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(srcGraph.toHandle(), capturedNodesCount, nodeHandles.data(), updateDescs.data(), nullptr));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph));
    const auto mutableCommandList = this->getSharedMutableCommandList<FamilyType::gfxCoreFamily>(execGraph, nodeHandles);
    ASSERT_NE(nullptr, mutableCommandList);
    const auto argumentsCommandId = this->getNodeCommand(execGraph, nodeHandles[argumentsAppendIndex]).commandId;
    const auto parametersCommandId = this->getNodeCommand(execGraph, nodeHandles[parametersAppendIndex]).commandId;

    this->expectGroupCountOfMutableCommand<FamilyType>(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, launchKernelUpdateGroupCount);
    this->expectGlobalWorkSizeOfMutableCommand(*mutableCommandList, argumentsAppendIndex, launchKernelUpdateGroupCount, this->testGroupSize);
    this->expectGroupCountOfMutableCommand<FamilyType>(*mutableCommandList, parametersCommandId, parametersAppendIndex, groupCountUpdateGroupCount);
    this->expectGlobalWorkSizeOfMutableCommand(*mutableCommandList, parametersAppendIndex, groupCountUpdateGroupCount, this->testGroupSize);
}

using GraphNodeGroupSizeUpdateInstantiationTest = Test<GraphNodeUpdateInstantiationFixture<ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeGroupSizeUpdateInstantiationTest, givenGroupSizeOfGraphNodesUpdatedWhenInstantiatingExecutableGraphThenMutableCommandsUseUpdatedGroupSize) {
    this->setupDispatchTraitsCrossThreadDataOffsets();

    L0::Graph srcGraph(this->context, true, true);
    auto nodeHandles = this->captureLaunchKernelNodes(srcGraph, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE, nullptr);
    ASSERT_NE(nullptr, nodeHandles[argumentsAppendIndex]);
    ASSERT_NE(nullptr, nodeHandles[parametersAppendIndex]);

    const ze_group_size_t launchKernelUpdateGroupSize = {4U, 2U, 3U};
    const ze_group_size_t groupSizeUpdateGroupSize = {2U, 2U, 2U};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupSize = launchKernelUpdateGroupSize;
    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    groupSizeDesc.groupSize = groupSizeUpdateGroupSize;
    std::array<const void *, capturedNodesCount> updateDescs = {&launchKernelDesc, &groupSizeDesc};
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(srcGraph.toHandle(), capturedNodesCount, nodeHandles.data(), updateDescs.data(), nullptr));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph));
    const auto mutableCommandList = this->getSharedMutableCommandList<FamilyType::gfxCoreFamily>(execGraph, nodeHandles);
    ASSERT_NE(nullptr, mutableCommandList);
    const auto argumentsCommandId = this->getNodeCommand(execGraph, nodeHandles[argumentsAppendIndex]).commandId;
    const auto parametersCommandId = this->getNodeCommand(execGraph, nodeHandles[parametersAppendIndex]).commandId;

    this->expectGroupSizeOfMutableCommand<FamilyType>(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, launchKernelUpdateGroupSize);
    this->expectGlobalWorkSizeOfMutableCommand(*mutableCommandList, argumentsAppendIndex, this->testGroupCount, launchKernelUpdateGroupSize);
    this->expectGroupSizeOfMutableCommand<FamilyType>(*mutableCommandList, parametersCommandId, parametersAppendIndex, groupSizeUpdateGroupSize);
    this->expectGlobalWorkSizeOfMutableCommand(*mutableCommandList, parametersAppendIndex, this->testGroupCount, groupSizeUpdateGroupSize);
}

using GraphNodeGlobalOffsetUpdateInstantiationTest = Test<GraphNodeUpdateInstantiationFixture<ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeGlobalOffsetUpdateInstantiationTest, givenGlobalOffsetOfGraphNodesUpdatedWhenInstantiatingExecutableGraphThenMutableCommandsUseUpdatedGlobalOffset) {
    this->setupDispatchTraitsCrossThreadDataOffsets();

    L0::Graph srcGraph(this->context, true, true);
    auto nodeHandles = this->captureLaunchKernelNodes(srcGraph, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET, nullptr);
    ASSERT_NE(nullptr, nodeHandles[argumentsAppendIndex]);
    ASSERT_NE(nullptr, nodeHandles[parametersAppendIndex]);

    const ze_graph_node_update_global_offset_exp_desc_t argumentsGlobalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC, nullptr, 1U, 2U, 3U};
    const ze_graph_node_update_global_offset_exp_desc_t parametersGlobalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC, nullptr, 4U, 5U, 6U};
    std::array<const void *, capturedNodesCount> updateDescs = {&argumentsGlobalOffsetDesc, &parametersGlobalOffsetDesc};
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(srcGraph.toHandle(), capturedNodesCount, nodeHandles.data(), updateDescs.data(), nullptr));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph));
    const auto mutableCommandList = this->getSharedMutableCommandList<FamilyType::gfxCoreFamily>(execGraph, nodeHandles);
    ASSERT_NE(nullptr, mutableCommandList);
    const auto argumentsCommandId = this->getNodeCommand(execGraph, nodeHandles[argumentsAppendIndex]).commandId;
    const auto parametersCommandId = this->getNodeCommand(execGraph, nodeHandles[parametersAppendIndex]).commandId;

    this->expectGlobalOffsetOfMutableCommand(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsGlobalOffsetDesc);
    this->expectGlobalOffsetOfMutableCommand(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersGlobalOffsetDesc);
}

using GraphNodeKernelArgumentUpdateInstantiationTest = Test<GraphNodeUpdateInstantiationFixture<ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeKernelArgumentUpdateInstantiationTest, givenKernelArgumentsOfGraphNodesUpdatedWhenInstantiatingExecutableGraphThenMutableCommandsUseUpdatedKernelArguments) {
    this->setupKernelArguments();
    this->setupDispatchTraitsCrossThreadDataOffsets();

    L0::Graph srcGraph(this->context, true, true);
    auto nodeHandles = this->captureLaunchKernelNodes(srcGraph, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, this->capturedArguments.data());
    ASSERT_NE(nullptr, nodeHandles[argumentsAppendIndex]);
    ASSERT_NE(nullptr, nodeHandles[parametersAppendIndex]);

    void *const launchKernelUpdateBuffer = this->allocateUsm(bufferSize);
    ASSERT_NE(nullptr, launchKernelUpdateBuffer);
    const uint32_t launchKernelUpdateValue = 2U;
    std::array<const void *, kernelArgumentsCount> launchKernelArgumentValues = {&launchKernelUpdateBuffer, &launchKernelUpdateValue};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = kernelArgumentSizes.data();
    launchKernelDesc.pArgValues = launchKernelArgumentValues.data();

    void *const kernelArgumentUpdateBuffer = this->allocateUsm(bufferSize);
    ASSERT_NE(nullptr, kernelArgumentUpdateBuffer);
    const uint32_t kernelArgumentUpdateValue = 3U;
    ze_graph_node_update_kernel_argument_exp_desc_t valueArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    valueArgumentDesc.argIndex = valueArgumentIndex;
    valueArgumentDesc.argSize = kernelArgumentSizes[valueArgumentIndex];
    valueArgumentDesc.pArgValue = &kernelArgumentUpdateValue;
    ze_graph_node_update_kernel_argument_exp_desc_t bufferArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    bufferArgumentDesc.pNext = &valueArgumentDesc;
    bufferArgumentDesc.argIndex = bufferArgumentIndex;
    bufferArgumentDesc.argSize = kernelArgumentSizes[bufferArgumentIndex];
    bufferArgumentDesc.pArgValue = &kernelArgumentUpdateBuffer;

    std::array<const void *, capturedNodesCount> updateDescs = {&launchKernelDesc, &bufferArgumentDesc};
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(srcGraph.toHandle(), capturedNodesCount, nodeHandles.data(), updateDescs.data(), nullptr));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph));
    const auto mutableCommandList = this->getSharedMutableCommandList<FamilyType::gfxCoreFamily>(execGraph, nodeHandles);
    ASSERT_NE(nullptr, mutableCommandList);
    const auto argumentsCommandId = this->getNodeCommand(execGraph, nodeHandles[argumentsAppendIndex]).commandId;
    const auto parametersCommandId = this->getNodeCommand(execGraph, nodeHandles[parametersAppendIndex]).commandId;

    this->expectKernelArgumentsOfMutableCommand(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, launchKernelUpdateBuffer, launchKernelUpdateValue);
    this->expectKernelArgumentsOfMutableCommand(*mutableCommandList, parametersCommandId, parametersAppendIndex, kernelArgumentUpdateBuffer, kernelArgumentUpdateValue);
}

using GraphNodeAllUpdatesInstantiationTest = Test<GraphNodeUpdateInstantiationFixture<allMutableGraphNodeFlagsExceptKernelInstruction>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeAllUpdatesInstantiationTest, givenGraphNodesUpdatedWithLaunchKernelAndGlobalOffsetDescriptorsWhenInstantiatingExecutableGraphThenMutableCommandsUseAllUpdatedValues) {
    this->setupKernelArguments();
    this->setupDispatchTraitsCrossThreadDataOffsets();

    L0::Graph srcGraph(this->context, true, true);
    auto nodeHandles = this->captureLaunchKernelNodes(srcGraph, allMutableGraphNodeFlagsExceptKernelInstruction, this->capturedArguments.data());
    ASSERT_NE(nullptr, nodeHandles[argumentsAppendIndex]);
    ASSERT_NE(nullptr, nodeHandles[parametersAppendIndex]);

    void *const argumentsNodeBuffer = this->allocateUsm(bufferSize);
    ASSERT_NE(nullptr, argumentsNodeBuffer);
    const uint32_t argumentsNodeValue = 2U;
    std::array<const void *, kernelArgumentsCount> argumentsNodeArgumentValues = {&argumentsNodeBuffer, &argumentsNodeValue};
    const ze_graph_node_update_global_offset_exp_desc_t argumentsNodeGlobalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC, nullptr, 1U, 2U, 3U};
    ze_graph_node_update_launch_kernel_exp_desc_t argumentsNodeLaunchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    argumentsNodeLaunchKernelDesc.pNext = &argumentsNodeGlobalOffsetDesc;
    argumentsNodeLaunchKernelDesc.pArgSizes = kernelArgumentSizes.data();
    argumentsNodeLaunchKernelDesc.pArgValues = argumentsNodeArgumentValues.data();
    argumentsNodeLaunchKernelDesc.groupCount = {2U, 3U, 4U};
    argumentsNodeLaunchKernelDesc.groupSize = {4U, 2U, 3U};

    void *const parametersNodeBuffer = this->allocateUsm(bufferSize);
    ASSERT_NE(nullptr, parametersNodeBuffer);
    const uint32_t parametersNodeValue = 3U;
    std::array<const void *, kernelArgumentsCount> parametersNodeArgumentValues = {&parametersNodeBuffer, &parametersNodeValue};
    const ze_graph_node_update_global_offset_exp_desc_t parametersNodeGlobalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC, nullptr, 4U, 5U, 6U};
    ze_graph_node_update_launch_kernel_exp_desc_t parametersNodeLaunchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    parametersNodeLaunchKernelDesc.pNext = &parametersNodeGlobalOffsetDesc;
    parametersNodeLaunchKernelDesc.pArgSizes = kernelArgumentSizes.data();
    parametersNodeLaunchKernelDesc.pArgValues = parametersNodeArgumentValues.data();
    parametersNodeLaunchKernelDesc.groupCount = {5U, 6U, 7U};
    parametersNodeLaunchKernelDesc.groupSize = {2U, 2U, 2U};

    std::array<const void *, capturedNodesCount> updateDescs = {&argumentsNodeLaunchKernelDesc, &parametersNodeLaunchKernelDesc};
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(srcGraph.toHandle(), capturedNodesCount, nodeHandles.data(), updateDescs.data(), nullptr));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph));
    const auto mutableCommandList = this->getSharedMutableCommandList<FamilyType::gfxCoreFamily>(execGraph, nodeHandles);
    ASSERT_NE(nullptr, mutableCommandList);
    const auto argumentsCommandId = this->getNodeCommand(execGraph, nodeHandles[argumentsAppendIndex]).commandId;
    const auto parametersCommandId = this->getNodeCommand(execGraph, nodeHandles[parametersAppendIndex]).commandId;

    this->expectKernelArgumentsOfMutableCommand(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsNodeBuffer, argumentsNodeValue);
    this->expectGroupCountOfMutableCommand<FamilyType>(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsNodeLaunchKernelDesc.groupCount);
    this->expectGroupSizeOfMutableCommand<FamilyType>(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsNodeLaunchKernelDesc.groupSize);
    this->expectGlobalWorkSizeOfMutableCommand(*mutableCommandList, argumentsAppendIndex, argumentsNodeLaunchKernelDesc.groupCount, argumentsNodeLaunchKernelDesc.groupSize);
    this->expectGlobalOffsetOfMutableCommand(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsNodeGlobalOffsetDesc);

    this->expectKernelArgumentsOfMutableCommand(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersNodeBuffer, parametersNodeValue);
    this->expectGroupCountOfMutableCommand<FamilyType>(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersNodeLaunchKernelDesc.groupCount);
    this->expectGroupSizeOfMutableCommand<FamilyType>(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersNodeLaunchKernelDesc.groupSize);
    this->expectGlobalWorkSizeOfMutableCommand(*mutableCommandList, parametersAppendIndex, parametersNodeLaunchKernelDesc.groupCount, parametersNodeLaunchKernelDesc.groupSize);
    this->expectGlobalOffsetOfMutableCommand(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersNodeGlobalOffsetDesc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeAllUpdatesInstantiationTest, givenGraphNodesUpdatedWithChainedFineGrainDescriptorsWhenInstantiatingExecutableGraphThenMutableCommandsUseAllUpdatedValues) {
    this->setupKernelArguments();
    this->setupDispatchTraitsCrossThreadDataOffsets();

    L0::Graph srcGraph(this->context, true, true);
    auto nodeHandles = this->captureLaunchKernelNodes(srcGraph, allMutableGraphNodeFlagsExceptKernelInstruction, this->capturedArguments.data());
    ASSERT_NE(nullptr, nodeHandles[argumentsAppendIndex]);
    ASSERT_NE(nullptr, nodeHandles[parametersAppendIndex]);

    void *const argumentsNodeBuffer = this->allocateUsm(bufferSize);
    ASSERT_NE(nullptr, argumentsNodeBuffer);
    const uint32_t argumentsNodeValue = 2U;
    const ze_graph_node_update_global_offset_exp_desc_t argumentsNodeGlobalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC, nullptr, 1U, 2U, 3U};
    ze_graph_node_update_group_size_exp_desc_t argumentsNodeGroupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    argumentsNodeGroupSizeDesc.pNext = &argumentsNodeGlobalOffsetDesc;
    argumentsNodeGroupSizeDesc.groupSize = {4U, 2U, 3U};
    ze_graph_node_update_group_count_exp_desc_t argumentsNodeGroupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    argumentsNodeGroupCountDesc.pNext = &argumentsNodeGroupSizeDesc;
    argumentsNodeGroupCountDesc.groupCount = {2U, 3U, 4U};
    ze_graph_node_update_kernel_argument_exp_desc_t argumentsNodeValueDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    argumentsNodeValueDesc.pNext = &argumentsNodeGroupCountDesc;
    argumentsNodeValueDesc.argIndex = valueArgumentIndex;
    argumentsNodeValueDesc.argSize = kernelArgumentSizes[valueArgumentIndex];
    argumentsNodeValueDesc.pArgValue = &argumentsNodeValue;
    ze_graph_node_update_kernel_argument_exp_desc_t argumentsNodeBufferDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    argumentsNodeBufferDesc.pNext = &argumentsNodeValueDesc;
    argumentsNodeBufferDesc.argIndex = bufferArgumentIndex;
    argumentsNodeBufferDesc.argSize = kernelArgumentSizes[bufferArgumentIndex];
    argumentsNodeBufferDesc.pArgValue = &argumentsNodeBuffer;

    void *const parametersNodeBuffer = this->allocateUsm(bufferSize);
    ASSERT_NE(nullptr, parametersNodeBuffer);
    const uint32_t parametersNodeValue = 3U;
    const ze_graph_node_update_global_offset_exp_desc_t parametersNodeGlobalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC, nullptr, 4U, 5U, 6U};
    ze_graph_node_update_group_size_exp_desc_t parametersNodeGroupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    parametersNodeGroupSizeDesc.pNext = &parametersNodeGlobalOffsetDesc;
    parametersNodeGroupSizeDesc.groupSize = {2U, 2U, 2U};
    ze_graph_node_update_group_count_exp_desc_t parametersNodeGroupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    parametersNodeGroupCountDesc.pNext = &parametersNodeGroupSizeDesc;
    parametersNodeGroupCountDesc.groupCount = {5U, 6U, 7U};
    ze_graph_node_update_kernel_argument_exp_desc_t parametersNodeValueDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    parametersNodeValueDesc.pNext = &parametersNodeGroupCountDesc;
    parametersNodeValueDesc.argIndex = valueArgumentIndex;
    parametersNodeValueDesc.argSize = kernelArgumentSizes[valueArgumentIndex];
    parametersNodeValueDesc.pArgValue = &parametersNodeValue;
    ze_graph_node_update_kernel_argument_exp_desc_t parametersNodeBufferDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    parametersNodeBufferDesc.pNext = &parametersNodeValueDesc;
    parametersNodeBufferDesc.argIndex = bufferArgumentIndex;
    parametersNodeBufferDesc.argSize = kernelArgumentSizes[bufferArgumentIndex];
    parametersNodeBufferDesc.pArgValue = &parametersNodeBuffer;

    std::array<const void *, capturedNodesCount> updateDescs = {&argumentsNodeBufferDesc, &parametersNodeBufferDesc};
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(srcGraph.toHandle(), capturedNodesCount, nodeHandles.data(), updateDescs.data(), nullptr));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph));
    const auto mutableCommandList = this->getSharedMutableCommandList<FamilyType::gfxCoreFamily>(execGraph, nodeHandles);
    ASSERT_NE(nullptr, mutableCommandList);
    const auto argumentsCommandId = this->getNodeCommand(execGraph, nodeHandles[argumentsAppendIndex]).commandId;
    const auto parametersCommandId = this->getNodeCommand(execGraph, nodeHandles[parametersAppendIndex]).commandId;

    this->expectKernelArgumentsOfMutableCommand(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsNodeBuffer, argumentsNodeValue);
    this->expectGroupCountOfMutableCommand<FamilyType>(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsNodeGroupCountDesc.groupCount);
    this->expectGroupSizeOfMutableCommand<FamilyType>(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsNodeGroupSizeDesc.groupSize);
    this->expectGlobalWorkSizeOfMutableCommand(*mutableCommandList, argumentsAppendIndex, argumentsNodeGroupCountDesc.groupCount, argumentsNodeGroupSizeDesc.groupSize);
    this->expectGlobalOffsetOfMutableCommand(*mutableCommandList, argumentsCommandId, argumentsAppendIndex, argumentsNodeGlobalOffsetDesc);

    this->expectKernelArgumentsOfMutableCommand(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersNodeBuffer, parametersNodeValue);
    this->expectGroupCountOfMutableCommand<FamilyType>(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersNodeGroupCountDesc.groupCount);
    this->expectGroupSizeOfMutableCommand<FamilyType>(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersNodeGroupSizeDesc.groupSize);
    this->expectGlobalWorkSizeOfMutableCommand(*mutableCommandList, parametersAppendIndex, parametersNodeGroupCountDesc.groupCount, parametersNodeGroupSizeDesc.groupSize);
    this->expectGlobalOffsetOfMutableCommand(*mutableCommandList, parametersCommandId, parametersAppendIndex, parametersNodeGlobalOffsetDesc);
}

} // namespace ult
} // namespace L0
