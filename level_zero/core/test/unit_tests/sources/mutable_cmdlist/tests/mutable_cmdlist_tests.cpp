/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/core/ze_mutable_cmdlist_api_entrypoints.h"
#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_argument.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_pipe_control_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_semaphore_wait_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_store_data_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_store_register_mem_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_variable.h"

#include "copy_offload_mode.h"

#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace L0 {
namespace ult {

using MutableCommandListTest = Test<MutableCommandListFixture<false, -1>>;

using MutableCommandListInOrderTest = Test<MutableCommandListFixture<true, -1>>;

void setMclCmdBufferAllocationsWritableForSimulation(MutableCommandList &mutableCmdList, bool writable) {
    for (auto cmdBufferAllocation : mutableCmdList.getBase()->getCmdContainer().getCmdBufferAllocations()) {
        cmdBufferAllocation->setTbxWritable(writable, NEO::GraphicsAllocation::allBanks);
    }
}

bool areMclCmdBufferAllocationsWritableForSimulation(MutableCommandList &mutableCmdList) {
    auto &cmdBufferAllocations = mutableCmdList.getBase()->getCmdContainer().getCmdBufferAllocations();
    return std::all_of(cmdBufferAllocations.begin(), cmdBufferAllocations.end(), [](NEO::GraphicsAllocation *cmdBufferAllocation) {
        return cmdBufferAllocation->isTbxWritable(NEO::GraphicsAllocation::allBanks);
    });
}

TEST(MutableCommandListKernelArgumentTest, givenKernelArgumentDescriptorsWhenCheckingIfArgumentIsMutableThenOnlyValuesAndBufferAndSlmPointersAreMutable) {
    auto makePointerArgument = [](NEO::KernelArgMetadata::AddressSpaceQualifier addressQualifier) {
        NEO::ArgDescriptor argDescriptor{NEO::ArgDescriptor::argTPointer};
        argDescriptor.getTraits().addressQualifier = addressQualifier;
        return argDescriptor;
    };

    EXPECT_TRUE(L0::MCL::isMutableKernelArgument(NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue}));
    EXPECT_TRUE(L0::MCL::isMutableKernelArgument(makePointerArgument(NEO::KernelArgMetadata::AddrGlobal)));
    EXPECT_TRUE(L0::MCL::isMutableKernelArgument(makePointerArgument(NEO::KernelArgMetadata::AddrConstant)));
    EXPECT_TRUE(L0::MCL::isMutableKernelArgument(makePointerArgument(NEO::KernelArgMetadata::AddrUnknown)));
    EXPECT_TRUE(L0::MCL::isMutableKernelArgument(makePointerArgument(NEO::KernelArgMetadata::AddrLocal)));
    EXPECT_FALSE(L0::MCL::isMutableKernelArgument(makePointerArgument(NEO::KernelArgMetadata::AddrPrivate)));
    EXPECT_FALSE(L0::MCL::isMutableKernelArgument(NEO::ArgDescriptor{NEO::ArgDescriptor::argTImage}));
    EXPECT_FALSE(L0::MCL::isMutableKernelArgument(NEO::ArgDescriptor{NEO::ArgDescriptor::argTSampler}));
    EXPECT_FALSE(L0::MCL::isMutableKernelArgument(NEO::ArgDescriptor{NEO::ArgDescriptor::argTUnknown}));
}

TEST(MutableCommandListKernelArgumentTest, givenKernelArgumentDescriptorsWhenCheckingIfArgumentIsSlmThenOnlyLocalPointersAreSlm) {
    NEO::ArgDescriptor localPointerArgument{NEO::ArgDescriptor::argTPointer};
    localPointerArgument.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrLocal;
    NEO::ArgDescriptor globalPointerArgument{NEO::ArgDescriptor::argTPointer};
    globalPointerArgument.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrGlobal;
    NEO::ArgDescriptor valueArgument{NEO::ArgDescriptor::argTValue};
    valueArgument.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrLocal;

    EXPECT_TRUE(L0::MCL::isSlmKernelArgument(localPointerArgument));
    EXPECT_FALSE(L0::MCL::isSlmKernelArgument(globalPointerArgument));
    EXPECT_FALSE(L0::MCL::isSlmKernelArgument(valueArgument));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenClosedMutableCmdListWhenClosingAgainThenCmdBufferAllocationsAreMarkedWritableOnlyWhenUpdated) {
    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = NEO::CommandStreamReceiverType::tbx;
    setMclCmdBufferAllocationsWritableForSimulation(*mutableCommandList, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());
    EXPECT_TRUE(areMclCmdBufferAllocationsWritableForSimulation(*mutableCommandList));

    setMclCmdBufferAllocationsWritableForSimulation(*mutableCommandList, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());
    EXPECT_FALSE(areMclCmdBufferAllocationsWritableForSimulation(*mutableCommandList));

    mutableCommandList->toggleCommandListUpdated();
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());
    EXPECT_TRUE(areMclCmdBufferAllocationsWritableForSimulation(*mutableCommandList));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenAllocationWithUserManagedResidencyWhenAddingToMutableResidencyThenOnlyDriverManagedAllocationIsAdded) {
    NEO::MockGraphicsAllocation userManagedAllocation;
    userManagedAllocation.setUserManagedResidency(true);
    NEO::MockGraphicsAllocation driverManagedAllocation;

    mutableCommandList->addToResidencyContainer(&userManagedAllocation, nullptr);
    mutableCommandList->addToResidencyContainer(&driverManagedAllocation, nullptr);

    auto &addedAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations).addedAllocations;
    auto isAdded = [&addedAllocations](const NEO::GraphicsAllocation *allocation) {
        return std::any_of(addedAllocations.begin(), addedAllocations.end(),
                           [allocation](const L0::MCL::AllocationReference &ref) { return ref.allocation == allocation; });
    };
    EXPECT_FALSE(isAdded(&userManagedAllocation));
    EXPECT_TRUE(isAdded(&driverManagedAllocation));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenInvalidCoreFamilyWhenCreatingCommandListThenNoObjectCreated) {
    ze_result_t returnValue;

    VariableBackup<GFXCORE_FAMILY> coreFamilyBackup(&device->getNEODevice()->getRootDeviceEnvironmentRef().getMutableHardwareInfo()->platform.eRenderCoreFamily, GFXCORE_FAMILY{NEO::maxCoreEnumValue});
    auto mcl = MutableCommandList::create(device, this->engineGroupType, 0, returnValue, false);
    EXPECT_EQ(nullptr, mcl);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, returnValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenEstimatedNumberOfCommandsWhenCreatingCommandListThenEstimateIsPropagatedToCommandContainer) {
    ze_result_t returnValue;

    constexpr uint32_t estimatedNumberOfCommands = 8u;
    auto mcl = MutableCommandList::create(device, this->engineGroupType, 0, returnValue, false, estimatedNumberOfCommands);
    ASSERT_NE(nullptr, mcl);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(estimatedNumberOfCommands, mcl->getCmdContainer().getEstimatedNumberOfCommands());

    mcl->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenCallingIsMutableExpThenTrueIsReturned) {
    ze_bool_t isMutable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getBase()->isMutableExp(&isMutable));
    EXPECT_TRUE(static_cast<bool>(isMutable));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenGettingCommandIdThenGetCorrectFlags) {
    EXPECT_EQ(0u, mutableCommandList->nextCommandId);
    EXPECT_FALSE(mutableCommandList->nextAppendKernelMutable);

    mutableCommandIdDesc.flags = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);

    ASSERT_NE(0u, mutableCommandList->kernelMutations.size());
    ASSERT_NE(0u, mutableCommandList->eventMutations.size());

    L0::MCL::MutableFlags expectedFlags = L0::MCL::MutableFlag::all & (~L0::MCL::MutableFlag::kernelInstruction);
    EXPECT_EQ(expectedFlags, mutableCommandList->nextMutationFlags);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenGettingCommandIdForGraphNodeWithFlagsThenMatchingMutableFlagsAreSet) {
    const ze_mutable_graph_node_exp_flags_t graphNodeFlags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_SIGNAL_EVENT |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS;

    auto result = mutableCommandList->getNextCommandIdForGraphNode(graphNodeFlags, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(mutableCommandList->nextCommandId, commandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);

    const L0::MCL::MutableFlags expectedFlags = L0::MCL::MutableFlag::kernelArgument |
                                                L0::MCL::MutableFlag::groupCount |
                                                L0::MCL::MutableFlag::groupSize |
                                                L0::MCL::MutableFlag::globalOffset |
                                                L0::MCL::MutableFlag::signalEvent |
                                                L0::MCL::MutableFlag::waitEvents;
    EXPECT_EQ(expectedFlags, mutableCommandList->nextMutationFlags);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenGettingCommandIdForGraphNodeWithZeroFlagsThenAllFlagsExceptKernelInstructionAreSet) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(0u, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(mutableCommandList->nextCommandId, commandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);

    const L0::MCL::MutableFlags expectedFlags = L0::MCL::MutableFlag::all & (~L0::MCL::MutableFlag::kernelInstruction);
    EXPECT_EQ(expectedFlags, mutableCommandList->nextMutationFlags);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWhenUpdatingWithChainedKernelDispatchGraphNodeDescriptorsThenAllVariablesAreUpdated) {
    const ze_mutable_graph_node_exp_flags_t graphNodeFlags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET;
    auto result = mutableCommandList->getNextCommandIdForGraphNode(graphNodeFlags, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    uint32_t value1 = 2;
    uint32_t value2 = 4;
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::value, kernelAllMask);
    result = kernel->setArgImmediate(0, sizeof(value1), &value1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    ze_graph_node_update_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC};

    kernelArgumentDesc.argIndex = 0;
    kernelArgumentDesc.argSize = sizeof(value2);
    kernelArgumentDesc.pArgValue = &value2;
    kernelArgumentDesc.pNext = &groupCountDesc;
    groupCountDesc.groupCount = {8, 2, 2};
    groupCountDesc.pNext = &groupSizeDesc;
    groupSizeDesc.groupSize = {4, 2, 1};
    groupSizeDesc.pNext = &globalOffsetDesc;
    globalOffsetDesc.offsetX = 1;
    globalOffsetDesc.offsetY = 2;
    globalOffsetDesc.offsetZ = 3;

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, graphNodeFlags, &kernelArgumentDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto valueVariable = mutation.variables.kernelArguments[0].kernelArgumentVariable;
    auto immediatePatchFullAddress = reinterpret_cast<void *>(valueVariable->getValueUsages().statelessWithoutOffset[0]);
    uint32_t valueVariablePatchValue = 0;
    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value2, valueVariablePatchValue);

    auto &groupCount = mutation.variables.groupCount->getDesc().kernelDispatch.groupCount;
    EXPECT_EQ(8u, groupCount[0]);
    EXPECT_EQ(2u, groupCount[1]);
    EXPECT_EQ(2u, groupCount[2]);

    auto &groupSize = mutation.variables.groupSize->getDesc().kernelDispatch.groupSize;
    EXPECT_EQ(4u, groupSize[0]);
    EXPECT_EQ(2u, groupSize[1]);
    EXPECT_EQ(1u, groupSize[2]);

    auto &globalOffset = mutation.variables.globalOffset->getDesc().kernelDispatch.globalOffset;
    EXPECT_EQ(1u, globalOffset[0]);
    EXPECT_EQ(2u, globalOffset[1]);
    EXPECT_EQ(3u, globalOffset[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWhenUpdatingWithChainedEventGraphNodeDescriptorsThenEventVariablesAreUpdated) {
    auto signalEvent = createTestEvent(false, false, false, false, false);
    auto signalEventHandle = signalEvent->toHandle();
    auto waitEvent = createTestEvent(false, false, false, false, false);
    auto waitEventHandle = waitEvent->toHandle();
    auto newSignalEvent = createTestEvent(false, false, false, false, false);
    auto newWaitEvent = createTestEvent(false, false, false, false, false);
    auto newWaitEventHandle = newWaitEvent->toHandle();

    const ze_mutable_graph_node_exp_flags_t graphNodeFlags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_SIGNAL_EVENT |
                                                             ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandIdForGraphNode(graphNodeFlags, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, signalEventHandle, 1, &waitEventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_graph_node_update_signal_event_exp_desc_t signalEventDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_SIGNAL_EVENT_EXP_DESC};
    ze_graph_node_update_wait_events_exp_desc_t waitEventsDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_WAIT_EVENTS_EXP_DESC};
    signalEventDesc.hEvent = newSignalEvent->toHandle();
    signalEventDesc.pNext = &waitEventsDesc;
    waitEventsDesc.numEvents = 1;
    waitEventsDesc.phEvents = &newWaitEventHandle;

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, graphNodeFlags, &signalEventDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());
    EXPECT_EQ(newSignalEvent, signalEvents[0]->getDesc().eventValue.event);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    EXPECT_EQ(newWaitEvent, waitEvents[0]->getDesc().eventValue.event);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWhenUpdatingWithUnknownDescriptorThenInvalidArgumentIsReturned) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(0u, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_base_desc_t unknownDesc = {ZE_STRUCTURE_TYPE_FORCE_UINT32};

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, 0u, &unknownDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWithZeroFlagsWhenUpdatingWithLaunchKernelDescriptorThenArgumentsGroupSizeAndGroupCountAreUpdatedAndNonMutableArgumentIsSkipped) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(0u, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    uint32_t value1 = 2;
    uint32_t value2 = 4;
    resizeKernelArg(2);
    NEO::ArgDescriptor kernelArgImage = {NEO::ArgDescriptor::argTImage};
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = kernelArgImage;
    prepareKernelArg(1, L0::MCL::VariableType::value, kernelAllMask);
    result = kernel->setArgImmediate(1, sizeof(value1), &value1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(2u, mutation.variables.kernelArguments.size());
    EXPECT_EQ(nullptr, mutation.variables.kernelArguments[0].kernelArgumentVariable);

    const size_t argSizes[] = {sizeof(void *), sizeof(value2)};
    const void *argValues[] = {nullptr, &value2};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = argSizes;
    launchKernelDesc.pArgValues = argValues;
    launchKernelDesc.groupCount = {8, 2, 2};
    launchKernelDesc.groupSize = {4, 2, 1};

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, 0u, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto valueVariable = mutation.variables.kernelArguments[1].kernelArgumentVariable;
    auto immediatePatchFullAddress = reinterpret_cast<void *>(valueVariable->getValueUsages().statelessWithoutOffset[0]);
    uint32_t valueVariablePatchValue = 0;
    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value2, valueVariablePatchValue);

    auto &groupCount = mutation.variables.groupCount->getDesc().kernelDispatch.groupCount;
    EXPECT_EQ(8u, groupCount[0]);
    EXPECT_EQ(2u, groupCount[1]);
    EXPECT_EQ(2u, groupCount[2]);

    auto &groupSize = mutation.variables.groupSize->getDesc().kernelDispatch.groupSize;
    EXPECT_EQ(4u, groupSize[0]);
    EXPECT_EQ(2u, groupSize[1]);
    EXPECT_EQ(1u, groupSize[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWhenApplyingGraphNodeMutationsThenKernelArgumentGroupCountGroupSizeAndGlobalOffsetAreUpdated) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(0u, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    uint32_t value1 = 2;
    uint32_t value2 = 4;
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::value, kernelAllMask);
    result = kernel->setArgImmediate(0, sizeof(value1), &value1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->applyGraphNodeMutation(L0::MCL::KernelArgumentMutationDescriptor{.commandId = commandId, .argIndex = 0, .argSize = sizeof(value2), .argValue = &value2}));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->applyGraphNodeMutation(L0::MCL::GroupCountMutationDescriptor{.commandId = commandId, .groupCount = {8, 2, 2}}));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->applyGraphNodeMutation(L0::MCL::GroupSizeMutationDescriptor{.commandId = commandId, .groupSize = {4, 2, 1}}));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->applyGraphNodeMutation(L0::MCL::GlobalOffsetMutationDescriptor{.commandId = commandId, .globalOffset = {1, 2, 3}}));
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto valueVariable = mutation.variables.kernelArguments[0].kernelArgumentVariable;
    auto immediatePatchFullAddress = reinterpret_cast<void *>(valueVariable->getValueUsages().statelessWithoutOffset[0]);
    uint32_t valueVariablePatchValue = 0;
    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value2, valueVariablePatchValue);

    auto &groupCount = mutation.variables.groupCount->getDesc().kernelDispatch.groupCount;
    EXPECT_EQ(8u, groupCount[0]);
    EXPECT_EQ(2u, groupCount[1]);
    EXPECT_EQ(2u, groupCount[2]);

    auto &groupSize = mutation.variables.groupSize->getDesc().kernelDispatch.groupSize;
    EXPECT_EQ(4u, groupSize[0]);
    EXPECT_EQ(2u, groupSize[1]);
    EXPECT_EQ(1u, groupSize[2]);

    auto &globalOffset = mutation.variables.globalOffset->getDesc().kernelDispatch.globalOffset;
    EXPECT_EQ(1u, globalOffset[0]);
    EXPECT_EQ(2u, globalOffset[1]);
    EXPECT_EQ(3u, globalOffset[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWhenUpdatingWithLaunchKernelDescriptorAndSelectedFlagsThenOnlySelectedFieldsAreUpdated) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(0u, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(0u, mutation.variables.kernelArguments.size());

    auto &groupCount = mutation.variables.groupCount->getDesc().kernelDispatch.groupCount;
    auto &groupSize = mutation.variables.groupSize->getDesc().kernelDispatch.groupSize;
    const uint32_t initialGroupSize[3] = {groupSize[0], groupSize[1], groupSize[2]};

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupCount = {8, 2, 2};
    launchKernelDesc.groupSize = {initialGroupSize[0] + 1, initialGroupSize[1], initialGroupSize[2]};

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(8u, groupCount[0]);
    EXPECT_EQ(2u, groupCount[1]);
    EXPECT_EQ(2u, groupCount[2]);
    EXPECT_EQ(initialGroupSize[0], groupSize[0]);
    EXPECT_EQ(initialGroupSize[1], groupSize[1]);
    EXPECT_EQ(initialGroupSize[2], groupSize[2]);

    launchKernelDesc.groupCount = {16, 4, 4};

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(8u, groupCount[0]);
    EXPECT_EQ(2u, groupCount[1]);
    EXPECT_EQ(2u, groupCount[2]);
    EXPECT_EQ(initialGroupSize[0] + 1, groupSize[0]);
    EXPECT_EQ(initialGroupSize[1], groupSize[1]);
    EXPECT_EQ(initialGroupSize[2], groupSize[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWithKernelArgumentsWhenUpdatingWithLaunchKernelDescriptorWithoutArgumentArraysThenInvalidNullPointerIsReturned) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t value = 2;
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::value, kernelAllMask);
    result = kernel->setArgImmediate(0, sizeof(value), &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    const size_t argSizes[] = {sizeof(value)};
    const void *argValues[] = {&value};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};

    launchKernelDesc.pArgSizes = nullptr;
    launchKernelDesc.pArgValues = argValues;
    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);

    launchKernelDesc.pArgSizes = argSizes;
    launchKernelDesc.pArgValues = nullptr;
    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWhenUpdatingWithKernelInstructionFlagAndLaunchKernelDescriptorWithKernelThenUnsupportedFeatureIsReturned) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(0u, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.hKernel = kernel->toHandle();
    launchKernelDesc.groupCount = {8, 2, 2};

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_INSTRUCTION | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    launchKernelDesc.hKernel = nullptr;
    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_INSTRUCTION | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWithBufferArgumentWhenUpdatingWithLaunchKernelDescriptorWithInvalidArgumentSizeThenInvalidArgumentIsReturned) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *usm = allocateUsm(4096);
    ASSERT_NE(nullptr, usm);
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    result = kernel->setArgBuffer(0, sizeof(void *), &usm);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    const size_t argSizes[] = {sizeof(uint8_t)};
    const void *argValues[] = {&usm};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = argSizes;
    launchKernelDesc.pArgValues = argValues;

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenGraphNodeCommandWithoutGroupSizeAndGroupCountVariablesWhenUpdatingWithLaunchKernelDescriptorThenInvalidArgumentIsReturned) {
    auto result = mutableCommandList->getNextCommandIdForGraphNode(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupCount = {8, 2, 2};
    launchKernelDesc.groupSize = {4, 2, 1};

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = mutableCommandList->updateMutableCommandForGraphNode(commandId, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT, &launchKernelDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenCommandListResetThenZeroContainers) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);

    ASSERT_NE(0u, mutableCommandList->kernelMutations.size());
    ASSERT_NE(0u, mutableCommandList->eventMutations.size());

    L0::MCL::MutableFlags expectedFlags = L0::MCL::MutableFlag::kernelArgument;
    EXPECT_EQ(expectedFlags, mutableCommandList->nextMutationFlags);

    result = mutableCommandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, mutableCommandList->nextCommandId);
    EXPECT_FALSE(mutableCommandList->nextAppendKernelMutable);
    EXPECT_EQ(0u, mutableCommandList->kernelMutations.size());
    EXPECT_EQ(0u, mutableCommandList->eventMutations.size());
    EXPECT_EQ(0u, mutableCommandList->nextMutationFlags);

    ze_mutable_command_id_exp_desc_t mutableCommandIdExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    mutableCommandIdExpDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    result = L0::zeCommandListGetNextCommandIdExp(mutableCommandList->toHandle(), &mutableCommandIdExpDesc, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::MCL::MutableFlags expectedFlagsExp = L0::MCL::MutableFlag::kernelArgument;
    EXPECT_EQ(expectedFlagsExp, mutableCommandList->nextMutationFlags);

    result = mutableCommandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, mutableCommandList->nextCommandId);
    EXPECT_FALSE(mutableCommandList->nextAppendKernelMutable);
    EXPECT_EQ(0u, mutableCommandList->kernelMutations.size());
    EXPECT_EQ(0u, mutableCommandList->eventMutations.size());
    EXPECT_EQ(0u, mutableCommandList->nextMutationFlags);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenAppendingKernelAndMutatingKernelArgumentsThenCorrectVariablesCreatedAndUpdated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    uint64_t usmPatchAddressValue = 0;
    uint32_t valueVariablePatchValue = 0;
    uint32_t slmVariablePatchValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    uint32_t value1 = 2, value2 = 4;
    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);
    auto usm1Allocation = getUsmAllocation(usm1);
    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);
    auto usm2Allocation = getUsmAllocation(usm2);
    bool poolAllocations = (usm1Allocation == usm2Allocation);
    size_t slm1arg1 = 4;
    size_t slm2arg1 = 2048;
    size_t slm1arg2 = 512;
    size_t slm2arg2 = 1024;

    // set kernel arg 0,1,2,3 => buffer, value, slm, slm2
    resizeKernelArg(4);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::value, kernelAllMask);
    prepareKernelArg(2, L0::MCL::VariableType::slmBuffer, kernelAllMask);
    prepareKernelArg(3, L0::MCL::VariableType::slmBuffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgImmediate(1, sizeof(value1), &value1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgSlm(2, slm1arg1, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgSlm(3, slm1arg2, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto itUsm1 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                               whiteBoxAllocations.addedAllocations.end(),
                               [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                                   return ref.allocation == usm1Allocation;
                               });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    auto kernelDispatch = mutableCommandList->dispatches[0].get();

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(mutableCommandList->updatedCommandList);

    mutableCommandList->toggleCommandListUpdated();
    EXPECT_TRUE(mutableCommandList->updatedCommandList);

    auto &bufferVarMutDesc = mutation.variables.kernelArguments[0];
    auto &bufferInternalDesc = bufferVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &valueVarMutDesc = mutation.variables.kernelArguments[1];
    auto &valueInternalDesc = valueVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::value, valueInternalDesc.type);
    auto immediatePatchFullAddress = reinterpret_cast<void *>(valueVarMutDesc.kernelArgumentVariable->getValueUsages().statelessWithoutOffset[0]);
    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value1, valueVariablePatchValue);

    auto &slmVarMutDesc = mutation.variables.kernelArguments[2];
    auto &slmInternalDesc = slmVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::slmBuffer, slmInternalDesc.type);

    auto &slm2VarMutDesc = mutation.variables.kernelArguments[3];
    auto &slm2InternalDesc = slm2VarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::slmBuffer, slm2InternalDesc.type);
    auto slmPatchFullAddress = reinterpret_cast<void *>(slm2VarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&slmVariablePatchValue, slmPatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(static_cast<uint32_t>(slm1arg1), slmVariablePatchValue);

    EXPECT_EQ(alignUp(slm1arg1 + slm1arg2, MemoryConstants::kiloByte), static_cast<size_t>(kernelDispatch->slmTotalSizePerThreadGroup));

    ze_mutable_kernel_argument_ext_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};
    ze_mutable_kernel_argument_ext_desc_t kernelValueArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};
    ze_mutable_kernel_argument_ext_desc_t kernelSlmArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};
    ze_mutable_kernel_argument_ext_desc_t kernelSlm2Arg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usm2;
    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelValueArg.argIndex = 1;
    kernelValueArg.argSize = sizeof(value2);
    kernelValueArg.commandId = commandId;
    kernelValueArg.pArgValue = &value2;
    kernelBufferArg.pNext = &kernelValueArg;

    kernelSlmArg.argIndex = 2;
    kernelSlmArg.argSize = slm2arg1;
    kernelSlmArg.commandId = commandId;
    kernelSlmArg.pArgValue = nullptr;
    kernelValueArg.pNext = &kernelSlmArg;

    kernelSlm2Arg.argIndex = 3;
    kernelSlm2Arg.argSize = slm2arg2;
    kernelSlm2Arg.commandId = commandId;
    kernelSlm2Arg.pArgValue = nullptr;
    kernelSlmArg.pNext = &kernelSlm2Arg;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    itUsm1 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                          whiteBoxAllocations.addedAllocations.end(),
                          [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                              return ref.allocation == usm1Allocation;
                          });
    if (poolAllocations) {
        EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    } else {
        EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    }

    auto itUsm2 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                               whiteBoxAllocations.addedAllocations.end(),
                               [&usm2Allocation](const L0::MCL::AllocationReference &ref) {
                                   return ref.allocation == usm2Allocation;
                               });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm2);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value2, valueVariablePatchValue);

    memcpy(&slmVariablePatchValue, slmPatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(static_cast<uint32_t>(slm2arg1), slmVariablePatchValue);

    EXPECT_EQ(alignUp(slm2arg1 + slm2arg2, MemoryConstants::kiloByte), static_cast<size_t>(kernelDispatch->slmTotalSizePerThreadGroup));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenAppendingKernelWithImageArgumentThenNoVariableCreated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    // set kernel arg 0 => image, buffer
    resizeKernelArg(2);
    NEO::ArgDescriptor kernelArgImage = {NEO::ArgDescriptor::argTImage};
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = kernelArgImage;

    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[1].getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrConstant;

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // only buffer created
    ASSERT_EQ(2u, mutation.variables.kernelArguments.size());
    // at index 1
    EXPECT_EQ(nullptr, mutation.variables.kernelArguments[0].kernelArgumentVariable);
    EXPECT_NE(nullptr, mutation.variables.kernelArguments[1].kernelArgumentVariable);

    void *buffer = reinterpret_cast<void *>(0x12345678);

    ze_mutable_kernel_argument_ext_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};
    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &buffer;

    // cannot mutate when variable is not created
    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenAppendingKernelWithOnlySlmArgumentThenSlmVariableCreated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    // set kernel arg 0 => slm
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::slmBuffer, kernelAllMask);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(1u, mutation.variables.kernelArguments.size());
    EXPECT_EQ(0u, mutation.variables.kernelArguments[0].argIndex);
    EXPECT_EQ(L0::MCL::VariableType::slmBuffer, mutation.variables.kernelArguments[0].kernelArgumentVariable->getType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndKernelBufferNullArgumentsWhenAppendingKernelAndMutatingIntoNullOrBufferKernelArgumentsThenCorrectValuesPatched) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    uint64_t usmPatchAddressValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);
    auto usm1Allocation = getUsmAllocation(usm1);
    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);
    auto usm2Allocation = getUsmAllocation(usm2);
    bool poolAllocations = (usm1Allocation == usm2Allocation);
    void *nullSurface = nullptr;

    // set kernel arg 0,1,=> buffer1, buffer2
    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0].getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrUnknown;
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[1].getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrUnknown;

    result = kernel->setArgBuffer(0, sizeof(void *), &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgBuffer(1, sizeof(void *), nullSurface);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto itUsm1 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                               whiteBoxAllocations.addedAllocations.end(),
                               [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                                   return ref.allocation == usm1Allocation;
                               });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm1);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &buffer1VarMutDesc = mutation.variables.kernelArguments[0];
    auto &buffer1InternalDesc = buffer1VarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, buffer1InternalDesc.type);
    auto gpuVa1PatchFullAddress = reinterpret_cast<void *>(buffer1VarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVa1PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &buffer2VarMutDesc = mutation.variables.kernelArguments[1];
    auto &buffer2InternalDesc = buffer2VarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, buffer2InternalDesc.type);
    auto gpuVa2PatchFullAddress = reinterpret_cast<void *>(buffer2VarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVa2PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(nullSurface), usmPatchAddressValue);

    ze_mutable_kernel_argument_ext_desc_t kernelBuffer1Arg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};
    ze_mutable_kernel_argument_ext_desc_t kernelBuffer2Arg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};

    kernelBuffer1Arg.argIndex = 0;
    kernelBuffer1Arg.argSize = sizeof(void *);
    kernelBuffer1Arg.commandId = commandId;
    kernelBuffer1Arg.pArgValue = nullSurface;
    mutableCommandsDesc.pNext = &kernelBuffer1Arg;

    kernelBuffer2Arg.argIndex = 1;
    kernelBuffer2Arg.argSize = sizeof(void *);
    kernelBuffer2Arg.commandId = commandId;
    kernelBuffer2Arg.pArgValue = &usm2;
    kernelBuffer1Arg.pNext = &kernelBuffer2Arg;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    itUsm1 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                          whiteBoxAllocations.addedAllocations.end(),
                          [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                              return ref.allocation == usm1Allocation;
                          });
    if (poolAllocations) {
        EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    } else {
        EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    }

    auto itUsm2 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                               whiteBoxAllocations.addedAllocations.end(),
                               [&usm2Allocation](const L0::MCL::AllocationReference &ref) {
                                   return ref.allocation == usm2Allocation;
                               });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm2);

    memcpy(&usmPatchAddressValue, gpuVa1PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(nullSurface), usmPatchAddressValue);

    memcpy(&usmPatchAddressValue, gpuVa2PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndKernelBufferNullArgumentsWhenAppendingKernelAndMutatingIntoNullThenAllocIdCountFromManagerDoesNotChange) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    uint64_t usmPatchAddressValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    void *nullSurface = nullptr;

    // set kernel arg 0 => buffer1
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), nullSurface);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &buffer0VarMutDesc = mutation.variables.kernelArguments[0];
    auto &buffer0InternalDesc = buffer0VarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, buffer0InternalDesc.type);
    auto gpuVa0PatchFullAddress = reinterpret_cast<void *>(buffer0VarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVa0PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(0u, usmPatchAddressValue);
    EXPECT_EQ(undefined<uint32_t>, buffer0InternalDesc.allocIdMemoryManagerCounter);

    // update manager counter
    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);

    ze_mutable_kernel_argument_ext_desc_t kernelBuffer1Arg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};

    kernelBuffer1Arg.argIndex = 0;
    kernelBuffer1Arg.argSize = sizeof(void *);
    kernelBuffer1Arg.commandId = commandId;
    kernelBuffer1Arg.pArgValue = nullSurface;
    mutableCommandsDesc.pNext = &kernelBuffer1Arg;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVa0PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(0u, usmPatchAddressValue);
    EXPECT_EQ(undefined<uint32_t>, buffer0InternalDesc.allocIdMemoryManagerCounter);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndInlineKernelWhenAppendingKernelAndMutatingArgumentsThenDataIsPatchedIntoInlineData) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;

    uint64_t usmPatchAddressValue = 0;
    uint32_t valueVariablePatchValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);
    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);
    uint32_t value1 = 2, value2 = 4;

    // set kernel arg 0,1,=> buffer, immediate
    this->crossThreadOffset = 8;
    this->nextArgOffset = 16;
    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::value, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgImmediate(1, sizeof(value1), &value1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &bufferVarMutDesc = mutation.variables.kernelArguments[0];
    auto &bufferInternalDesc = bufferVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    ASSERT_NE(0u, bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset.size());
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &immediateVarMutDesc = mutation.variables.kernelArguments[1];
    auto &immediateInternalDesc = immediateVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::value, immediateInternalDesc.type);
    ASSERT_NE(0u, immediateVarMutDesc.kernelArgumentVariable->getValueUsages().commandBufferWithoutOffset.size());
    auto immediatePatchFullAddress = reinterpret_cast<void *>(immediateVarMutDesc.kernelArgumentVariable->getValueUsages().commandBufferWithoutOffset[0]);
    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value1, valueVariablePatchValue);

    ze_mutable_kernel_argument_ext_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};
    ze_mutable_kernel_argument_ext_desc_t kernelImmediateArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usm2;
    kernelBufferArg.pNext = &kernelImmediateArg;

    kernelImmediateArg.argIndex = 1;
    kernelImmediateArg.argSize = sizeof(uint32_t);
    kernelImmediateArg.commandId = commandId;
    kernelImmediateArg.pArgValue = &value2;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value2, valueVariablePatchValue);

    kernelBufferArg.pNext = nullptr;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndSystemSharedAllocationWhenMutatingIntoBufferThenPerformMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    void *usmSystem = nullptr, *usmAllocatedHost = nullptr;

    auto result = this->context->allocHostMem(&hostDesc, 4096, 4096, &usmAllocatedHost);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, usmAllocatedHost);

    usmSystem = reinterpret_cast<void *>(0x1000);

    // set kernel arg 0 => buffer
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;
    this->crossThreadOffset = 8;
    this->nextArgOffset = 16;
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usmSystem);

    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t usmPatchAddressValue = 0;

    auto &bufferVarMutDesc = mutation.variables.kernelArguments[0];
    auto &bufferInternalDesc = bufferVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    ASSERT_NE(0u, bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset.size());
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usmSystem), usmPatchAddressValue);
    // no graphics allocation present
    EXPECT_EQ(nullptr, bufferInternalDesc.bufferAlloc);

    ze_mutable_kernel_argument_ext_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usmAllocatedHost;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usmAllocatedHost), usmPatchAddressValue);

    // graphics allocation is present
    EXPECT_NE(nullptr, bufferInternalDesc.bufferAlloc);

    result = this->context->freeMem(usmAllocatedHost);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndHostPoolWhenMutationHostBufferOfSameAddressButDifferentIdThenPerformMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    debugManager.flags.EnableUsmAllocationPoolManager.set(0);
    debugManager.flags.EnableHostUsmAllocationPool.set(1);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;

    uint64_t usmPatchAddressValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    void *usm1 = nullptr, *usm2 = nullptr;

    result = this->context->allocHostMem(&hostDesc, 4096, 4096, &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, usm1);

    result = this->context->allocHostMem(&hostDesc, 4096, 4096, &usm2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, usm2);

    // set kernel arg 0 => buffer
    this->crossThreadOffset = 8;
    this->nextArgOffset = 16;
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usm1);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &bufferVarMutDesc = mutation.variables.kernelArguments[0];
    auto &bufferInternalDesc = bufferVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    ASSERT_NE(0u, bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset.size());
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto usm1AllocId = bufferInternalDesc.allocId;

    ze_mutable_kernel_argument_ext_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usm2;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    auto usm2AllocId = bufferInternalDesc.allocId;
    EXPECT_EQ(usm1AllocId, usm2AllocId);

    auto oldUsm2 = usm2;

    result = this->context->freeMem(usm2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->context->allocHostMem(&hostDesc, 4096, 4096, &usm2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(oldUsm2, usm2);

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    result = this->context->freeMem(usm2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = this->context->freeMem(usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenAppendingKernelWithPointerPrivateQualifierThenMutationVariableNotCreated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernel1Bit);
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0].getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrPrivate;

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(1u, mutation.variables.kernelArguments.size());
    EXPECT_EQ(nullptr, mutation.variables.kernelArguments[0].kernelArgumentVariable);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoKernelArgumentsFlagSelectedWhenAppendingKernelAndMutatingArgumentsThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_kernel_argument_ext_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = nullptr;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoKernelArgumentAtGivenIndexWhenAppendingKernelAndMutatingArgumentsThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_kernel_argument_ext_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = nullptr;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelWithoutLocalIDsIsDispatchedToMutateGroupCountThenUpdatePayloadUponMutation) {
    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    mockKernelImmData->kernelDescriptor->kernelAttributes.numLocalIdChannels = 0;
    dispatchTraits.globalWorkSize[0] = this->crossThreadOffset + 3 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[1] = this->crossThreadOffset + 4 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[2] = this->crossThreadOffset + 5 * sizeof(uint32_t);
    dispatchTraits.numWorkGroups[0] = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);
    dispatchTraits.numWorkGroups[1] = dispatchTraits.globalWorkSize[0] + 4 * sizeof(uint32_t);
    dispatchTraits.numWorkGroups[2] = dispatchTraits.globalWorkSize[0] + 5 * sizeof(uint32_t);
    dispatchTraits.workDim = dispatchTraits.numWorkGroups[0] + 3 * sizeof(uint32_t);

    ze_mutable_command_id_exp_desc_t mutableCommandIdExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    mutableCommandIdExpDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;
    auto result = L0::zeCommandListGetNextCommandIdExp(mutableCommandList->toHandle(), &mutableCommandIdExpDesc, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto groupCounts = getVariableList(commandId, L0::MCL::VariableType::groupCount, nullptr);
    ASSERT_EQ(1u, groupCounts.size());
    auto groupCountVar = groupCounts[0];
    auto varDispatchGc = groupCountVar->getDispatches()[0];

    auto crossThreadDataSize = varDispatchGc->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize, crossThreadDataSize);
    auto &offsets = varDispatchGc->getIndirectDataOffsets();

    ze_mutable_commands_exp_desc_t mutableCommandsExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    ze_mutable_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};

    mutableCommandsExpDesc.pNext = &groupCountDesc;

    ze_group_count_t mutatedGroupCount = {8, 2, 2};

    groupCountDesc.commandId = commandId;
    groupCountDesc.pGroupCount = &mutatedGroupCount;

    result = L0::zeCommandListUpdateMutableCommandsExp(mutableCommandList->toHandle(), &mutableCommandsExpDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();

    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkSize[0]));
    uint32_t *numWorkGroupsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.numWorkGroups[0]));
    uint32_t *workDimBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.workDimensions));

    EXPECT_EQ(mutatedGroupCount.groupCountX * 1u, gwsBuffer[0]);
    EXPECT_EQ(mutatedGroupCount.groupCountY * 1u, gwsBuffer[1]);
    EXPECT_EQ(mutatedGroupCount.groupCountZ * 1u, gwsBuffer[2]);

    EXPECT_EQ(mutatedGroupCount.groupCountX, numWorkGroupsBuffer[0]);
    EXPECT_EQ(mutatedGroupCount.groupCountY, numWorkGroupsBuffer[1]);
    EXPECT_EQ(mutatedGroupCount.groupCountZ, numWorkGroupsBuffer[2]);

    EXPECT_EQ(3u, *workDimBuffer);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoGroupCountFlagSelectedWhenAppendingKernelAndMutatingGroupCountThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_group_count_ext_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXT_DESC};

    mutableCommandsDesc.pNext = &groupCountDesc;

    ze_group_count_t mutatedGroupCount = {8, 2, 2};

    groupCountDesc.commandId = commandId;
    groupCountDesc.pGroupCount = &mutatedGroupCount;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenSingleDimensionImplicitArgsWhenKernelDispatchMutatesGroupCountThenOnlySingleOffsetsAreMutated) {
    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.enqueuedLocalWorkSize[0] = this->crossThreadOffset;
    dispatchTraits.enqueuedLocalWorkSize[1] = undefined<CrossThreadDataOffset>;
    dispatchTraits.enqueuedLocalWorkSize[2] = undefined<CrossThreadDataOffset>;
    dispatchTraits.globalWorkSize[0] = dispatchTraits.enqueuedLocalWorkSize[0] + 2 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[1] = undefined<CrossThreadDataOffset>;
    dispatchTraits.globalWorkSize[2] = undefined<CrossThreadDataOffset>;
    dispatchTraits.workDim = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);

    ze_mutable_command_id_exp_desc_t mutableCommandIdExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    mutableCommandIdExpDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;
    auto result = L0::zeCommandListGetNextCommandIdExp(mutableCommandList->toHandle(), &mutableCommandIdExpDesc, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto groupSizes = getVariableList(commandId, L0::MCL::VariableType::groupSize, nullptr);
    ASSERT_EQ(1u, groupSizes.size());
    auto groupSizeVar = groupSizes[0];
    auto varDispatchGs = groupSizeVar->getDispatches()[0];

    auto crossThreadDataSize = varDispatchGs->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize, crossThreadDataSize);
    auto &offsets = varDispatchGs->getIndirectDataOffsets();

    ze_mutable_commands_exp_desc_t mutableCommandsExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    ze_mutable_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};

    mutableCommandsExpDesc.pNext = &groupSizeDesc;

    uint32_t mutatedGroupSizeX = 4;
    uint32_t mutatedGroupSizeY = 9;
    uint32_t mutatedGroupSizeZ = 5;

    groupSizeDesc.commandId = commandId;
    groupSizeDesc.groupSizeX = mutatedGroupSizeX;
    groupSizeDesc.groupSizeY = mutatedGroupSizeY;
    groupSizeDesc.groupSizeZ = mutatedGroupSizeZ;

    result = L0::zeCommandListUpdateMutableCommandsExp(mutableCommandList->toHandle(), &mutableCommandsExpDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();

    uint32_t *lwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.enqLocalWorkSize[0]));
    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkSize[0]));
    uint32_t *workDimBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.workDimensions));

    EXPECT_EQ(this->testGroupCount.groupCountX * mutatedGroupSizeX, gwsBuffer[0]);
    EXPECT_EQ(mutatedGroupSizeX, lwsBuffer[0]);
    EXPECT_EQ(3u, *workDimBuffer);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelDispatchIsSelectedToMutateGroupSizeThenUpdatePayloadUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_GROUP_SIZE;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.enqueuedLocalWorkSize[0] = this->crossThreadOffset;
    dispatchTraits.enqueuedLocalWorkSize[1] = this->crossThreadOffset + sizeof(uint32_t);
    dispatchTraits.enqueuedLocalWorkSize[2] = this->crossThreadOffset + 2 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[0] = dispatchTraits.enqueuedLocalWorkSize[0] + 3 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[1] = dispatchTraits.enqueuedLocalWorkSize[1] + 3 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[2] = dispatchTraits.enqueuedLocalWorkSize[2] + 3 * sizeof(uint32_t);
    dispatchTraits.workDim = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto groupSizes = getVariableList(commandId, L0::MCL::VariableType::groupSize, nullptr);
    ASSERT_EQ(1u, groupSizes.size());
    auto groupSizeVar = groupSizes[0];
    auto varDispatchGs = groupSizeVar->getDispatches()[0];

    auto crossThreadDataSize = varDispatchGs->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize, crossThreadDataSize);
    auto &offsets = varDispatchGs->getIndirectDataOffsets();

    ze_mutable_group_size_ext_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXT_DESC};

    mutableCommandsDesc.pNext = &groupSizeDesc;

    uint32_t mutatedGroupSizeX = 4;
    uint32_t mutatedGroupSizeY = 2;
    uint32_t mutatedGroupSizeZ = 1;

    groupSizeDesc.commandId = commandId;
    groupSizeDesc.groupSizeX = mutatedGroupSizeX;
    groupSizeDesc.groupSizeY = mutatedGroupSizeY;
    groupSizeDesc.groupSizeZ = mutatedGroupSizeZ;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();

    uint32_t *lwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.enqLocalWorkSize[0]));
    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkSize[0]));
    uint32_t *workDimBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.workDimensions));

    EXPECT_EQ(this->testGroupCount.groupCountX * mutatedGroupSizeX, gwsBuffer[0]);
    EXPECT_EQ(this->testGroupCount.groupCountY * mutatedGroupSizeY, gwsBuffer[1]);
    EXPECT_EQ(this->testGroupCount.groupCountZ * mutatedGroupSizeZ, gwsBuffer[2]);

    EXPECT_EQ(mutatedGroupSizeX, lwsBuffer[0]);
    EXPECT_EQ(mutatedGroupSizeY, lwsBuffer[1]);
    EXPECT_EQ(mutatedGroupSizeZ, lwsBuffer[2]);

    EXPECT_EQ(2u, *workDimBuffer);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoGroupSizeFlagSelectedWhenAppendingKernelAndMutatingGroupSizeThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_group_size_ext_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXT_DESC};

    mutableCommandsDesc.pNext = &groupSizeDesc;

    groupSizeDesc.commandId = commandId;
    groupSizeDesc.groupSizeX = 1;
    groupSizeDesc.groupSizeY = 1;
    groupSizeDesc.groupSizeZ = 2;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelDispatchIsSelectedToMutateGlobalOffsetThenUpdatePayloadUponMutation) {
    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.globalWorkOffset[0] = this->crossThreadOffset;
    dispatchTraits.globalWorkOffset[1] = this->crossThreadOffset + sizeof(uint32_t);
    dispatchTraits.globalWorkOffset[2] = this->crossThreadOffset + 2 * sizeof(uint32_t);

    ze_mutable_command_id_exp_desc_t mutableCommandIdExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    mutableCommandIdExpDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;
    auto result = L0::zeCommandListGetNextCommandIdExp(mutableCommandList->toHandle(), &mutableCommandIdExpDesc, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto globalOffsets = getVariableList(commandId, L0::MCL::VariableType::globalOffset, nullptr);
    ASSERT_EQ(1u, globalOffsets.size());
    auto globalOffsetVar = globalOffsets[0];
    auto varDispatchGo = globalOffsetVar->getDispatches()[0];

    auto crossThreadDataSize = varDispatchGo->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize, crossThreadDataSize);
    auto &offsets = varDispatchGo->getIndirectDataOffsets();

    ze_mutable_commands_exp_desc_t mutableCommandsExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    ze_mutable_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC};

    mutableCommandsExpDesc.pNext = &globalOffsetDesc;

    uint32_t mutatedGlobalOffsetX = 2;
    uint32_t mutatedGlobalOffsetY = 3;
    uint32_t mutatedGlobalOffsetZ = 5;

    globalOffsetDesc.commandId = commandId;
    globalOffsetDesc.offsetX = mutatedGlobalOffsetX;
    globalOffsetDesc.offsetY = mutatedGlobalOffsetY;
    globalOffsetDesc.offsetZ = mutatedGlobalOffsetZ;

    result = L0::zeCommandListUpdateMutableCommandsExp(mutableCommandList->toHandle(), &mutableCommandsExpDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();

    uint32_t *globalOffsetBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkOffset[0]));

    EXPECT_EQ(mutatedGlobalOffsetX, globalOffsetBuffer[0]);
    EXPECT_EQ(mutatedGlobalOffsetY, globalOffsetBuffer[1]);
    EXPECT_EQ(mutatedGlobalOffsetZ, globalOffsetBuffer[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoGlobalOffsetFlagSelectedWhenAppendingKernelAndMutatingGlobalOffsetThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_GROUP_SIZE;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_global_offset_ext_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXT_DESC};

    mutableCommandsDesc.pNext = &globalOffsetDesc;

    globalOffsetDesc.commandId = commandId;
    globalOffsetDesc.offsetX = 1;
    globalOffsetDesc.offsetY = 2;
    globalOffsetDesc.offsetZ = 3;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndInlineKernelAppendedWhenKernelDispatchIsSelectedToMutateThenUpdateInlineUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXT_FLAG_GROUP_SIZE | ZE_MUTABLE_COMMAND_EXT_FLAG_GLOBAL_OFFSET;

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;
    this->crossThreadOffset = 8;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    fillOffsets(dispatchTraits.enqueuedLocalWorkSize, this->crossThreadOffset, 3);
    fillOffsets(dispatchTraits.globalWorkSize, dispatchTraits.enqueuedLocalWorkSize[0] + 3 * sizeof(uint32_t), 3);
    fillOffsets(dispatchTraits.numWorkGroups, dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t), 3);
    fillOffsets(dispatchTraits.globalWorkOffset, dispatchTraits.numWorkGroups[0] + 3 * sizeof(uint32_t), 3);
    dispatchTraits.workDim = dispatchTraits.globalWorkOffset[0] + 3 * sizeof(uint32_t);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto groupCounts = getVariableList(commandId, L0::MCL::VariableType::groupCount, nullptr);
    ASSERT_EQ(1u, groupCounts.size());
    auto groupCountVar = groupCounts[0];
    auto varDispatchGc = groupCountVar->getDispatches()[0];

    auto groupSizes = getVariableList(commandId, L0::MCL::VariableType::groupSize, nullptr);
    ASSERT_EQ(1u, groupSizes.size());
    auto groupSizeVar = groupSizes[0];
    auto varDispatchGs = groupSizeVar->getDispatches()[0];

    auto globalOffsets = getVariableList(commandId, L0::MCL::VariableType::globalOffset, nullptr);
    ASSERT_EQ(1u, globalOffsets.size());
    auto globalOffsetVar = globalOffsets[0];
    auto varDispatchGo = globalOffsetVar->getDispatches()[0];

    EXPECT_EQ(varDispatchGc, varDispatchGs);
    EXPECT_EQ(varDispatchGc, varDispatchGo);

    auto crossThreadDataSize = varDispatchGc->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize - mutableCommandList->inlineDataSize, crossThreadDataSize);
    auto &offsets = varDispatchGc->getIndirectDataOffsets();

    ze_mutable_group_count_ext_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXT_DESC};
    ze_mutable_group_size_ext_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXT_DESC};
    ze_mutable_global_offset_ext_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXT_DESC};

    mutableCommandsDesc.pNext = &groupCountDesc;

    ze_group_count_t mutatedGroupCount = {8, 2, 1};
    uint32_t mutatedGroupSizeX = 4;
    uint32_t mutatedGroupSizeY = 1;
    uint32_t mutatedGroupSizeZ = 4;

    uint32_t mutatedGlobalOffsetX = 2;
    uint32_t mutatedGlobalOffsetY = 3;
    uint32_t mutatedGlobalOffsetZ = 5;

    groupCountDesc.commandId = commandId;
    groupCountDesc.pGroupCount = &mutatedGroupCount;
    groupCountDesc.pNext = &groupSizeDesc;

    groupSizeDesc.commandId = commandId;
    groupSizeDesc.groupSizeX = mutatedGroupSizeX;
    groupSizeDesc.groupSizeY = mutatedGroupSizeY;
    groupSizeDesc.groupSizeZ = mutatedGroupSizeZ;
    groupSizeDesc.pNext = &globalOffsetDesc;

    globalOffsetDesc.commandId = commandId;
    globalOffsetDesc.offsetX = mutatedGlobalOffsetX;
    globalOffsetDesc.offsetY = mutatedGlobalOffsetY;
    globalOffsetDesc.offsetZ = mutatedGlobalOffsetZ;

    result = mutableCommandList->updateMutableCommands(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();
    void *inlineBase = mutableCommandList->mutableWalkerCmds[0]->getInlineDataPointer();

    void *currentBase = nullptr;
    size_t offset = 0;
    if (offsets.enqLocalWorkSize[0] < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.enqLocalWorkSize[0];
    } else {
        currentBase = payloadBase;
        offset = offsets.enqLocalWorkSize[0] - mutableCommandList->inlineDataSize;
    }
    uint32_t *lwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.globalWorkSize[0] < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.globalWorkSize[0];
    } else {
        currentBase = payloadBase;
        offset = offsets.globalWorkSize[0] - mutableCommandList->inlineDataSize;
    }
    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.numWorkGroups[0] < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.numWorkGroups[0];
    } else {
        currentBase = payloadBase;
        offset = offsets.numWorkGroups[0] - mutableCommandList->inlineDataSize;
    }
    uint32_t *numWorkGroupsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.globalWorkOffset[0] < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.globalWorkOffset[0];
    } else {
        currentBase = payloadBase;
        offset = offsets.globalWorkOffset[0] - mutableCommandList->inlineDataSize;
    }
    uint32_t *globalOffsetBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.workDimensions < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.workDimensions;
    } else {
        currentBase = payloadBase;
        offset = offsets.workDimensions - mutableCommandList->inlineDataSize;
    }
    uint32_t *workDimBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    EXPECT_EQ(mutatedGroupCount.groupCountX * mutatedGroupSizeX, gwsBuffer[0]);
    EXPECT_EQ(mutatedGroupCount.groupCountY * mutatedGroupSizeY, gwsBuffer[1]);
    EXPECT_EQ(mutatedGroupCount.groupCountZ * mutatedGroupSizeZ, gwsBuffer[2]);

    EXPECT_EQ(mutatedGroupSizeX, lwsBuffer[0]);
    EXPECT_EQ(mutatedGroupSizeY, lwsBuffer[1]);
    EXPECT_EQ(mutatedGroupSizeZ, lwsBuffer[2]);

    EXPECT_EQ(mutatedGroupCount.groupCountX, numWorkGroupsBuffer[0]);
    EXPECT_EQ(mutatedGroupCount.groupCountY, numWorkGroupsBuffer[1]);
    EXPECT_EQ(mutatedGroupCount.groupCountZ, numWorkGroupsBuffer[2]);

    EXPECT_EQ(3u, *workDimBuffer);

    EXPECT_EQ(mutatedGlobalOffsetX, globalOffsetBuffer[0]);
    EXPECT_EQ(mutatedGlobalOffsetY, globalOffsetBuffer[1]);
    EXPECT_EQ(mutatedGlobalOffsetZ, globalOffsetBuffer[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndEventsSelectedToMutateWhenAppendingWithNoSigalAndNoWaitEventThenNoMutationVariableCreated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    EXPECT_EQ(0u, signalEvents.size());

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    EXPECT_EQ(0u, waitEvents.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndSignalPlainEventSelectedWhenAppendingWithSignalEventThenMutationIsPerformed) {
    using WalkerType = typename FamilyType::PorWalkerType;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto originalEvent = createTestEvent(false, false, false, false, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, false, false, false, false);
    auto newEventAddress = mutatedEvent->getGpuAddress(this->device);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    ASSERT_EQ(1u, mutableCommandList->mutableWalkerCmds.size());
    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(originalEventAddress, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(newEventAddress, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(newEventAddress, walkerPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoSignalEventFlagSelectedWhenAppendingKernelAndMutatingSignalEventThenErrorIsReturned) {
    auto originalEvent = createTestEvent(false, false, false, false, false);
    auto mutatedEvent = createTestEvent(false, false, false, false, false);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, originalEvent->toHandle(), 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, mutatedEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListOnDcFlushPlatformAndSignalEventWithSignalScopeSelectedWhenAppendingWithSignalEventThenMutationIsPerformedOnDcFlushCommand) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &productHelper = this->neoDevice->getProductHelper();
    if (productHelper.isL3FlushAfterPostSyncSupported()) {
        GTEST_SKIP();
    }

    auto mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);
    mockBaseCmdList->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto originalEvent = createTestEvent(false, true, false, false, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, true, false, false, false);
    auto newEventAddress = mutatedEvent->getGpuAddress(this->device);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    ASSERT_EQ(1u, mutableCommandList->mutablePipeControlCmds.size());
    auto mockMutablePipeControl = static_cast<MockMutablePipeControlHw<FamilyType> *>(mutableCommandList->mutablePipeControlCmds[0].get());
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(mockMutablePipeControl->pipeControl);

    auto postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl);
    EXPECT_EQ(originalEventAddress, postSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl);
    EXPECT_EQ(newEventAddress, postSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListOnDcFlushPlatformAndSignalEventWithTimestampSignalScopeSelectedWhenAppendingWithSignalEventThenMutationIsPerformedOnRegisterCommand) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto &productHelper = this->neoDevice->getProductHelper();
    if (productHelper.isL3FlushAfterPostSyncSupported()) {
        GTEST_SKIP();
    }
    auto mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);
    mockBaseCmdList->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto originalEvent = createTestEvent(false, true, true, false, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, true, true, false, false);
    auto newEventAddress = mutatedEvent->getGpuAddress(this->device);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(0u, mutableCommandList->mutableStoreRegMemCmds.size());

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());
    EXPECT_EQ(signalEvents[0]->getStoreRegMemList().size(), mutableCommandList->mutableStoreRegMemCmds.size());

    MI_STORE_REGISTER_MEM *storeRegMemCmd = nullptr;
    for (auto &mutableStoreRegMem : mutableCommandList->mutableStoreRegMemCmds) {
        auto mockMutableStoreRegMem = static_cast<MockMutableStoreRegisterMemHw<FamilyType> *>(mutableStoreRegMem.get());
        storeRegMemCmd = reinterpret_cast<MI_STORE_REGISTER_MEM *>(mockMutableStoreRegMem->storeRegMem);
        if (storeRegMemCmd->getMemoryAddress() == originalEventAddress) {
            break;
        }
    }
    ASSERT_NE(nullptr, storeRegMemCmd);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(newEventAddress, storeRegMemCmd->getMemoryAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularEventWhenMutateIntoEventThenDataIsUpdatedAndCommandChanged) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto signalEvent = createTestEvent(false, false, false, false, false);
    auto signalEventHandle = signalEvent->toHandle();
    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, signalEventHandle, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);

    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularTimestampEventWhenMutateIntoEventThenDataIsUpdatedAndCommandChanged) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = createTestEvent(false, false, true, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, true, false, false);
    auto newEventHandle = newEvent->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);

    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularEventAndPrefetchEnabledWhenMutatedIntoDifferentEventThenDataIsUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.EnableMemoryPrefetch.set(1);

    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularEventWhenNoopAndMutateIntoEventThenDataIsUpdatedAndCommandNoopedAndRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t semWaitNoop[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopEventHandle = nullptr;

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);

    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &noopEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoop, sizeof(MI_SEMAPHORE_WAIT)));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenAsyncMutationAndKernelWithWaitRegularEventWhenMutateIntoOtherEventThenDataIsUpdatedAndAsyncPatchlistDispatched) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr size_t semWaitSize = sizeof(MI_SEMAPHORE_WAIT);
    alignas(uint32_t) uint8_t semWaitData[semWaitSize] = {0};
    uint32_t *semWaitPtr = reinterpret_cast<uint32_t *>(semWaitData);

    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    mutableCommandList->getBase()->setupPatchPreambleEnabled(true);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    EXPECT_TRUE(waitEventVar->getDesc().asyncMutation);
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto expectedAsyncPatchListGpuDst = mutableSemWait->getGpuDestinationAddress();
    auto expectedAsyncPatchListHostSrc = mutableSemWait->getCommandView();

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(expectedAsyncPatchListHostSrc);
    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    auto &asyncPatchContainer = mutableCommandList->getBase()->getAsyncPatchContainer();
    ASSERT_EQ(1u, asyncPatchContainer.size());
    EXPECT_EQ(expectedAsyncPatchListGpuDst, asyncPatchContainer[0].gpuDestinationAddress);
    EXPECT_EQ(expectedAsyncPatchListHostSrc, asyncPatchContainer[0].hostSourceAddress);

    size_t expectedPatchSize = NEO::EncodeDataMemory<FamilyType>::getCommandSizeForEncode(sizeof(MI_SEMAPHORE_WAIT));
    EXPECT_EQ(expectedPatchSize, mutableCommandList->getBase()->getAsyncPatchlistPatchSize());

    ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    WhiteBox<L0::CommandQueue> *commandQueue = whiteboxCast(CommandQueue::create(device,
                                                                                 neoDevice->getDefaultEngine().commandStreamReceiver,
                                                                                 &queueDesc,
                                                                                 false,
                                                                                 false,
                                                                                 false,
                                                                                 result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto mutableCommandListHandle = mutableCommandList->toHandle();

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();
    L0::CommandListExecutionInternalOptions internalOptions = {};
    result = commandQueue->executeCommandLists(1, &mutableCommandListHandle, nullptr, internalOptions);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), usedSpaceBefore), usedSpaceAfter - usedSpaceBefore));

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, sdiCmds.size());

    size_t patchedSize = 0;
    for (auto &sdiCmds : sdiCmds) {
        auto sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmds);
        if (sdiCmd->getAddress() == expectedAsyncPatchListGpuDst) {
            if (sdiCmd->getStoreQword()) {
                expectedAsyncPatchListGpuDst += sizeof(uint64_t);
                patchedSize += sizeof(uint64_t);
                *semWaitPtr = sdiCmd->getDataDword0();
                semWaitPtr++;
                *semWaitPtr = sdiCmd->getDataDword1();
                semWaitPtr++;
            } else {
                expectedAsyncPatchListGpuDst += sizeof(uint32_t);
                patchedSize += sizeof(uint32_t);
                *semWaitPtr = sdiCmd->getDataDword0();
                semWaitPtr++;
            }
            if (patchedSize >= semWaitSize) {
                break;
            }
        }
    }
    semWaitPtr = reinterpret_cast<uint32_t *>(semWaitData);
    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitPtr);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(waitAddress, UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    EXPECT_EQ(0u, mutableCommandList->getBase()->getAsyncPatchlistPatchSize());

    commandQueue->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithTwoWaitRegularEventWhenNoopFirstAndMutateSecondThenDataIsUpdatedAndCommandsAreNoopedAndUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t semWaitNoop[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, true, false, false);
    auto eventHandle = event->toHandle();
    auto event2 = createTestEvent(false, false, true, false, false);
    auto eventHandle2 = event2->toHandle();
    auto newEvent = createTestEvent(false, false, true, false, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopEventHandle = nullptr;

    ze_event_handle_t appendEvents[] = {eventHandle, eventHandle2};
    ze_event_handle_t mutateEvents[] = {noopEventHandle, newEventHandle};
    ze_event_handle_t restoreFirstEvents[] = {eventHandle, newEventHandle};

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 2, appendEvents, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);
    EXPECT_EQ(2u, eventAllocationIt->refCount); // two wait events from the same pool added

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(2u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    auto waitEvent2Var = waitEvents[1];
    ASSERT_EQ(1u, waitEvent2Var->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);

    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    auto mutableSemWait2 = waitEvent2Var->getSemWaitList()[0];
    auto mockMutableSemWait2 = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait2);
    auto semWait2Cmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait2->semWait);

    auto waitAddress2 = event2->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress2, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWait2Cmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 2, mutateEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);
    EXPECT_EQ(1u, eventAllocationIt->refCount); // one event was nooped, one was updated with event from the same pool

    // semaphore wait command for event is nooped
    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoop, sizeof(MI_SEMAPHORE_WAIT)));

    // semaphore wait command for event2 is updated
    waitAddress2 = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress2, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWait2Cmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 2, restoreFirstEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);
    EXPECT_EQ(2u, eventAllocationIt->refCount); // first event was restored, second event remains the same as after mutation

    // semaphore wait command for first event was restored
    waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    // semaphore wait command for mutated event remains the same
    EXPECT_EQ(waitAddress2, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWait2Cmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoWaitEventFlagSelectedWhenAppendingKernelAndMutatingWaitEventThenErrorIsReturned) {
    auto originalEvent = createTestEvent(false, false, false, false, false);
    auto originalHandle = originalEvent->toHandle();
    auto mutatedEvent = createTestEvent(false, false, false, false, false);
    auto mutatedHandle = mutatedEvent->toHandle();

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &originalHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &mutatedHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenNotSupportedKernelLaunchModeWhenMutationPointActiveThenErrorCodeReturned) {
    mutableCommandIdDesc.flags = 0;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    this->testLaunchParams.isBuiltInKernel = true;
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    this->testLaunchParams.isBuiltInKernel = false;
    this->testLaunchParams.isIndirect = true;
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenNotSupportedKernelLaunchModeWhenMutationPointNotActiveThenSuccessCodeReturned) {
    this->testLaunchParams.isBuiltInKernel = true;
    auto result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    this->testLaunchParams.isBuiltInKernel = false;
    this->testLaunchParams.isIndirect = true;
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenNotSupportedKernelFlagsWhenAppendingKernelThenErrorCodeReturned) {
    mutableCommandIdDesc.flags = 0;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE, result);

    // immutable kernels (without get next command id) are allowed to be appended, as mcl does not support implicit args, so the kernel will not be mutated
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenCounterBasedEventWhenAppendLaunchKernelThenGetDeviceCounterAllocIsAddedForResidency) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    createTestEvent(true, false, false, false, false);
    ze_event_handle_t hEvent = this->eventHandles[0];

    MockGraphicsAllocation counterDeviceAlloc(this->device->getRootDeviceIndex(), nullptr, 0x1);

    this->events[0]->getInOrderExecEventHelper().assignData(1, 0, 1, 1, &counterDeviceAlloc, &counterDeviceAlloc, 1, 0, nullptr, 0, 0, false, true);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &hEvent, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    const auto &addedAllocations = whiteBoxAllocations.addedAllocations;
    EXPECT_FALSE(addedAllocations.empty());
    auto it = std::find_if(addedAllocations.begin(), addedAllocations.end(), [&counterDeviceAlloc](const L0::MCL::AllocationReference &ref) {
        return ref.allocation == &counterDeviceAlloc;
    });
    EXPECT_NE(it, addedAllocations.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenCounterBasedEventFromPeerDeviceWhenAppendLaunchKernelThenGetPeerDeviceCounterAllocIsAddedForResidency) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    createTestEvent(true, false, false, false, false);
    ze_event_handle_t hEvent = this->eventHandles[0];

    uint32_t peerDeviceIndex = this->device->getRootDeviceIndex() + 1;
    MockGraphicsAllocation peerCounterDeviceAlloc(peerDeviceIndex, reinterpret_cast<void *>(0x1234), 0x0u);

    this->events[0]->getInOrderExecEventHelper().assignData(1, 0, 1, 1, &peerCounterDeviceAlloc, &peerCounterDeviceAlloc, 1, 0, nullptr, 0, 0, false, true);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &hEvent, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    const auto &addedAllocations = whiteBoxAllocations.addedAllocations;
    EXPECT_FALSE(addedAllocations.empty());
    auto it = std::find_if(addedAllocations.begin(), addedAllocations.end(), [&peerCounterDeviceAlloc](const L0::MCL::AllocationReference &ref) {
        return ref.allocation->getGpuAddress() == peerCounterDeviceAlloc.getGpuAddress();
    });
    EXPECT_NE(it, addedAllocations.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithCbSignalEventAndMutateItThenExpectNewEventGetCmdListInOrderExecInfo) {
    auto mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    createTestEvent(true, false, false, false, false);
    auto newEvent = createTestEvent(true, false, false, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockBaseCmdList->inOrderExecInfo->getBaseDeviceAddress(), newEvent->getInOrderExecEventHelper().getBaseDeviceAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithSignalEventCbTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(true, false, true, false, false);
    auto newEvent = createTestEvent(true, false, true, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockBaseCmdListHw->inOrderExecInfo->getBaseDeviceAddress(), newEvent->getInOrderExecEventHelper().getBaseDeviceAddress());

    baseGpuVa = newEvent->getGpuAddress(this->device);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventCbSignalScopeTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(true, true, true, false, false);
    auto newEvent = createTestEvent(true, true, true, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockBaseCmdListHw->inOrderExecInfo->getBaseDeviceAddress(), newEvent->getInOrderExecEventHelper().getBaseDeviceAddress());

    baseGpuVa = newEvent->getGpuAddress(this->device);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventCbSignalScopeAndMutateItThenExpectPatchNewEventAddress) {
    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(true, true, false, false, false);
    auto newEvent = createTestEvent(true, true, false, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(nullptr, event->getAllocation(this->device));

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(nullptr, newEvent->getAllocation(this->device));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithSignalEventRegularAndMutateItThenExpectPatchNewEventAddress) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, false, false, false, false);
    auto newEvent = createTestEvent(false, false, false, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);
    auto completionGpuVa = event->getCompletionFieldGpuAddress(this->device);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    MI_STORE_DATA_IMM *sdiCmd = nullptr;
    MI_SEMAPHORE_WAIT *semCmd = nullptr;
    if (mockBaseCmdListHw->duplicatedInOrderCounterStorageEnabled == false) {
        ASSERT_EQ(1u, signalEvents[0]->getStoreDataImmList().size());
        auto mutableSdi = signalEvents[0]->getStoreDataImmList()[0];
        auto mockMutableSdi = static_cast<MockMutableStoreDataImmHw<FamilyType> *>(mutableSdi);
        sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(mockMutableSdi->storeDataImm);
        EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());

        ASSERT_EQ(1u, signalEvents[0]->getSemWaitList().size());
        auto mutableSem = signalEvents[0]->getSemWaitList()[0];
        auto mockMutableSem = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSem);
        semCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSem->semWait);
        EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    }

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = newEvent->getGpuAddress(this->device);
    completionGpuVa = newEvent->getCompletionFieldGpuAddress(this->device);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    if (mockBaseCmdListHw->duplicatedInOrderCounterStorageEnabled == false) {
        EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());
        EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    }
}

} // namespace ult
} // namespace L0
