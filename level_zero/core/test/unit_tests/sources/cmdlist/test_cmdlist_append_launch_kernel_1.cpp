/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandListAppendLaunchKernel = Test<ModuleFixture>;

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithIndirectAllocationsAllowedThenCommandListReturnsExpectedIndirectAllocationsAllowed) {
    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    kernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
    kernel->unifiedMemoryControls.indirectHostAllocationsAllowed = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);
    EXPECT_TRUE(kernel->hasIndirectAllocationsAllowed());

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_TRUE(commandList->hasIndirectAllocationsAllowed());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithIndirectAllocationsNotAllowedThenCommandListReturnsExpectedIndirectAllocationsAllowed) {
    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = false;
    kernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = false;
    kernel->unifiedMemoryControls.indirectHostAllocationsAllowed = false;

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_FALSE(commandList->hasIndirectAllocationsAllowed());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithOldestFirstThreadArbitrationPolicySetUsingSchedulingHintExtensionThenCorrectInternalPolicyIsReturned) {
    createKernel();
    ze_scheduling_hint_exp_desc_t pHint{};
    pHint.flags = ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST;
    kernel->setSchedulingHintExp(&pHint);
    ASSERT_EQ(kernel->getSchedulingHintExp(), NEO::ThreadArbitrationPolicy::AgeBased);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithRRThreadArbitrationPolicySetUsingSchedulingHintExtensionThenCorrectInternalPolicyIsReturned) {
    createKernel();
    ze_scheduling_hint_exp_desc_t pHint{};
    pHint.flags = ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN;
    kernel->setSchedulingHintExp(&pHint);
    ASSERT_EQ(kernel->getSchedulingHintExp(), NEO::ThreadArbitrationPolicy::RoundRobin);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithStallRRThreadArbitrationPolicySetUsingSchedulingHintExtensionThenCorrectInternalPolicyIsReturned) {
    createKernel();
    ze_scheduling_hint_exp_desc_t pHint{};
    pHint.flags = ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN;
    kernel->setSchedulingHintExp(&pHint);
    ASSERT_EQ(kernel->getSchedulingHintExp(), NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithThreadArbitrationPolicySetUsingSchedulingHintExtensionTheSameFlagIsUsedToSetCmdListThreadArbitrationPolicy) {
    createKernel();
    ze_scheduling_hint_exp_desc_t *pHint = new ze_scheduling_hint_exp_desc_t;
    pHint->pNext = nullptr;
    pHint->flags = ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN;
    kernel->setSchedulingHintExp(pHint);

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, commandList->getFinalStreamState().stateComputeMode.threadArbitrationPolicy.value);
    delete (pHint);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithThreadArbitrationPolicySetUsingSchedulingHintExtensionAndOverrideThreadArbitrationPolicyThenTheLatterIsUsedToSetCmdListThreadArbitrationPolicy) {
    createKernel();
    ze_scheduling_hint_exp_desc_t *pHint = new ze_scheduling_hint_exp_desc_t;
    pHint->pNext = nullptr;
    pHint->flags = ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN;
    kernel->setSchedulingHintExp(pHint);

    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideThreadArbitrationPolicy.set(0);

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, commandList->getFinalStreamState().stateComputeMode.threadArbitrationPolicy.value);
    delete (pHint);
}

HWTEST_F(CommandListAppendLaunchKernel, givenNotEnoughSpaceInCommandStreamWhenAppendingKernelThenBbEndIsAddedAndNewCmdBufferAllocated) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    createKernel();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    auto &commandContainer = commandList->commandContainer;
    const auto stream = commandContainer.getCommandStream();
    const auto streamCpu = stream->getCpuBase();

    Vec3<size_t> groupCount{1, 1, 1};
    auto sizeLeftInStream = sizeof(MI_BATCH_BUFFER_END);
    auto available = stream->getAvailableSpace();
    stream->getSpace(available - sizeLeftInStream);
    auto bbEndPosition = stream->getSpace(0);

    const uint32_t threadGroupDimensions[3] = {1, 1, 1};

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs{
        0,
        device->getNEODevice(),
        kernel.get(),
        threadGroupDimensions,
        PreemptionMode::MidBatch,
        0,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false};
    NEO::EncodeDispatchKernel<FamilyType>::encode(commandContainer, dispatchKernelArgs);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, 0u);

    const auto streamCpu2 = stream->getCpuBase();

    EXPECT_NE(nullptr, streamCpu2);
    EXPECT_NE(streamCpu, streamCpu2);

    EXPECT_EQ(2u, commandContainer.getCmdBufferAllocations().size());

    GenCmdList cmdList;
    FamilyType::PARSE::parseCommandBuffer(cmdList, bbEndPosition, 2 * sizeof(MI_BATCH_BUFFER_END));
    auto itor = find<MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenFunctionWhenBindingTablePrefetchAllowedThenProgramBindingTableEntryCount) {
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    createKernel();

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    auto commandStream = commandList->commandContainer.getCommandStream();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, commandStream->getCpuBase(), commandStream->getUsed()));

    auto itorMIDL = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorMIDL, cmdList.end());

    auto cmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(*itorMIDL);
    ASSERT_NE(cmd, nullptr);

    auto dsh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    auto idd = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(dsh->getCpuBase(), cmd->getInterfaceDescriptorDataStartAddress()));

    if (NEO::EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
        uint32_t numArgs = kernel->kernelImmData->getDescriptor().payloadMappings.bindingTable.numEntries;
        EXPECT_EQ(numArgs, idd->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, idd->getBindingTableEntryCount());
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfUsedWhenAppendedToCommandListThenKernelIsStored) {
    createKernel();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_group_count_t groupCount{1, 1, 1};

    EXPECT_TRUE(kernel->kernelImmData->getDescriptor().kernelAttributes.flags.usesPrintf);

    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, commandList->getPrintfFunctionContainer().size());
    EXPECT_EQ(kernel.get(), commandList->getPrintfFunctionContainer()[0]);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfUsedWhenAppendedToCommandListMultipleTimesThenKernelIsStoredOnce) {
    createKernel();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_group_count_t groupCount{1, 1, 1};

    EXPECT_TRUE(kernel->kernelImmData->getDescriptor().kernelAttributes.flags.usesPrintf);

    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, commandList->getPrintfFunctionContainer().size());
    EXPECT_EQ(kernel.get(), commandList->getPrintfFunctionContainer()[0]);

    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, commandList->getPrintfFunctionContainer().size());
}

HWTEST_F(CommandListAppendLaunchKernel, WhenAppendingMultipleTimesThenSshIsNotDepletedButReallocated) {
    createKernel();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_group_count_t groupCount{1, 1, 1};

    auto kernelSshSize = kernel->getSurfaceStateHeapDataSize();
    auto ssh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    auto sshHeapSize = ssh->getMaxAvailableSpace();
    auto initialAllocation = ssh->getGraphicsAllocation();
    EXPECT_NE(nullptr, initialAllocation);
    const_cast<KernelDescriptor::AddressingMode &>(kernel->getKernelDescriptor().kernelAttributes.bufferAddressingMode) = KernelDescriptor::BindfulAndStateless;
    for (size_t i = 0; i < sshHeapSize / kernelSshSize + 1; i++) {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }

    auto reallocatedAllocation = ssh->getGraphicsAllocation();
    EXPECT_NE(nullptr, reallocatedAllocation);
    EXPECT_NE(initialAllocation, reallocatedAllocation);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenEventsWhenAppendingKernelThenPostSyncToEventIsGenerated) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(
        kernel.toHandle(), &groupCount, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_FALSE(cmd->getDcFlushEnable());
            auto gpuAddress = event->getGpuAddress(device);
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            postSyncFound = true;
        }
    }
    EXPECT_TRUE(postSyncFound);

    {
        auto itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                                   std::end(commandList->commandContainer.getResidencyContainer()),
                                   &event->getAllocation(device));
        EXPECT_NE(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));
    }
}

using TimestampEventSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandListAppendLaunchKernel, givenTimestampEventsWhenAppendingKernelThenSRMAndPCEncoded, TimestampEventSupport) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(
        kernel.toHandle(), &groupCount, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(REG_GLOBAL_TIMESTAMP_LDW, cmd->getSourceRegisterAddress());
    }
    itor++;

    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, cmd->getSourceRegisterAddress());
    }
    itor++;

    itor = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    itor++;

    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
    itor++;

    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(REG_GLOBAL_TIMESTAMP_LDW, cmd->getSourceRegisterAddress());
    }
    itor++;

    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, cmd->getSourceRegisterAddress());
    }
    itor++;

    auto numPCs = findAll<PIPE_CONTROL *>(itor, cmdList.end());
    //we should not have PC when signal scope is device
    ASSERT_EQ(0u, numPCs.size());

    {
        auto itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                                   std::end(commandList->commandContainer.getResidencyContainer()),
                                   &event->getAllocation(device));
        EXPECT_NE(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));
    }
}

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelLaunchWithTSEventAndScopeFlagHostThenPCWithDCFlushEncoded, TimestampEventSupport) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        ZE_EVENT_SCOPE_FLAG_HOST,
        ZE_EVENT_SCOPE_FLAG_HOST};

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(
        kernel.toHandle(), &groupCount, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());

    PIPE_CONTROL *cmd = genCmdCast<PIPE_CONTROL *>(*itorPC[itorPC.size() - 1]);
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(cmd->getDcFlushEnable());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenForcePipeControlPriorToWalkerKeyThenAdditionalPCIsAdded, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    Mock<::L0::Kernel> kernel;
    ze_result_t result;
    std::unique_ptr<L0::CommandList> commandListBase(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceBefore = commandListBase->commandContainer.getCommandStream()->getUsed();

    ze_group_count_t groupCount{1, 1, 1};
    result = commandListBase->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandListBase->commandContainer.getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdListBase;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdListBase, ptrOffset(commandListBase->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdListBase.begin(), cmdListBase.end());

    size_t numberOfPCsBase = itorPC.size();

    DebugManagerStateRestore restorer;
    DebugManager.flags.ForcePipeControlPriorToWalker.set(1);

    std::unique_ptr<L0::CommandList> commandListWithDebugKey(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    usedSpaceBefore = commandListWithDebugKey->commandContainer.getCommandStream()->getUsed();

    result = commandListWithDebugKey->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    usedSpaceAfter = commandListWithDebugKey->commandContainer.getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdListBaseWithDebugKey;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdListBaseWithDebugKey, ptrOffset(commandListWithDebugKey->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    itorPC = findAll<PIPE_CONTROL *>(cmdListBaseWithDebugKey.begin(), cmdListBaseWithDebugKey.end());

    size_t numberOfPCsWithDebugKey = itorPC.size();

    EXPECT_EQ(numberOfPCsWithDebugKey, numberOfPCsBase + 1);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenForcePipeControlPriorToWalkerKeyAndNoSpaceThenNewBatchBufferAllocationIsUsed, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForcePipeControlPriorToWalker.set(1);

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    Mock<::L0::Kernel> kernel;
    ze_result_t result;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto firstBatchBufferAllocation = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    auto useSize = commandList->commandContainer.getCommandStream()->getAvailableSpace();
    useSize -= sizeof(PIPE_CONTROL);
    commandList->commandContainer.getCommandStream()->getSpace(useSize);

    ze_group_count_t groupCount{1, 1, 1};
    result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondBatchBufferAllocation = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelThenKernelIsExecutedOnImmediateCmdQ, IsAtLeastSkl) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    result = commandList0->appendLaunchKernel(
        kernel->toHandle(),
        &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelWithInvalidEventThenInvalidArgumentErrorIsReturned, IsAtLeastSkl) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    result = commandList0->appendLaunchKernel(
        kernel->toHandle(),
        &groupCount, nullptr, 1, nullptr);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelIndirectThenKernelIsExecutedOnImmediateCmdQ, IsAtLeastSkl) {
    createKernel();
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    result = commandList0->appendLaunchKernelIndirect(
        kernel->toHandle(),
        &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelIndirectWithInvalidEventThenInvalidArgumentErrorIsReturned, IsAtLeastSkl) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    result = commandList0->appendLaunchKernelIndirect(
        kernel->toHandle(),
        &groupCount, nullptr, 1, nullptr);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

using SupportedPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_DG1>;
HWTEST2_F(CommandListAppendLaunchKernel, givenCommandListWhenAppendLaunchKernelSeveralTimesThenAlwaysFirstEventPacketIsUsed, SupportedPlatforms) {
    createKernel();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        ZE_EVENT_SCOPE_FLAG_HOST,
        ZE_EVENT_SCOPE_FLAG_HOST};

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    EXPECT_EQ(1u, event->getPacketsInUse());
    ze_group_count_t groupCount{1, 1, 1};
    for (uint32_t i = 0; i < NEO::TimestampPacketSizeControl::preferredPacketCount + 4; i++) {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, event->toHandle(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    EXPECT_EQ(1u, event->getPacketsInUse());
}

struct CommandListAppendLaunchKernelWithImplicitArgs : CommandListAppendLaunchKernel {

    template <typename FamilyType>
    uint64_t getIndirectHeapOffsetForImplicitArgsBuffer(const Mock<::L0::Kernel> &kernel) {
        if (FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
            auto implicitArgsProgrammingSize = ImplicitArgsHelper::getSizeForImplicitArgsPatching(kernel.pImplicitArgs.get(), kernel.getKernelDescriptor(), neoDevice->getHardwareInfo());
            return implicitArgsProgrammingSize - sizeof(ImplicitArgs);
        } else {
            return 0u;
        }
    }
};

HWTEST_F(CommandListAppendLaunchKernelWithImplicitArgs, givenIndirectDispatchWithImplicitArgsWhenAppendingThenMiMathCommandsForWorkGroupCountAndGlobalWorkSizeAndWorkDimAreProgrammed) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    kernel.immutableData.crossThreadDataSize = sizeof(uint64_t);
    kernel.pImplicitArgs.reset(new ImplicitArgs());
    UnitTestHelper<FamilyType>::adjustKernelDescriptorForImplicitArgs(*kernel.immutableData.kernelDescriptor);

    kernel.setGroupSize(1, 1, 1);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     static_cast<ze_group_count_t *>(alloc),
                                                     nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto heap = commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT);
    uint64_t pImplicitArgsGPUVA = heap->getGraphicsAllocation()->getGpuAddress() + getIndirectHeapOffsetForImplicitArgsBuffer<FamilyType>(kernel);

    auto workDimStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    workDimStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R0);
    workDimStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA);

    auto groupCountXStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountXStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMX);
    groupCountXStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountX));

    auto groupCountYStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountYStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMY);
    groupCountYStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountY));

    auto groupCountZStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountZStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMZ);
    groupCountZStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountZ));

    auto globalSizeXStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeXStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R1);
    globalSizeXStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeX));

    auto globalSizeYStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeYStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R1);
    globalSizeYStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeY));

    auto globalSizeZStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeZStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R1);
    globalSizeZStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeZ));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountXStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountXStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountYStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountYStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountZStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountZStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeXStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeXStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeYStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeYStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeZStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeZStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    auto cmd2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    auto memoryMaskCmd = FamilyType::cmdInitLoadRegisterImm;
    memoryMaskCmd.setDataDword(0xFF00FFFF);

    EXPECT_EQ(cmd2->getDataDword(), memoryMaskCmd.getDataDword());

    itor++; //MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    cmd2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    auto offsetCmd = FamilyType::cmdInitLoadRegisterImm;
    offsetCmd.setDataDword(0x0000FFFF);

    EXPECT_EQ(cmd2->getDataDword(), offsetCmd.getDataDword());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor++; //MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor++; //MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor++; //MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), workDimStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), workDimStoreRegisterMemCmd.getMemoryAddress());

    context->freeMem(alloc);
}

HWTEST_F(CommandListAppendLaunchKernel, givenIndirectDispatchWhenAppendingThenWorkGroupCountAndGlobalWorkSizeAndWorkDimIsSetInCrossThreadData) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    Mock<::L0::Kernel> kernel;
    kernel.groupSize[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.workDim = 4;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     static_cast<ze_group_count_t *>(alloc),
                                                     nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    kernel.groupSize[2] = 2;
    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     static_cast<ze_group_count_t *>(alloc),
                                                     nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor++; //MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor++; //MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor++; //MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end()); //kernel with groupSize[2] = 2
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    context->freeMem(alloc);
}

HWTEST_F(CommandListAppendLaunchKernel, givenCommandListWhenResetCalledThenStateIsCleaned) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    createKernel();

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);
    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());

    auto commandListControl = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandListControl);
    ASSERT_NE(nullptr, commandListControl->commandContainer.getCommandStream());

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(
        kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->reset();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(device, commandList->device);
    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    ASSERT_GE(commandListControl->commandContainer.getCmdBufferAllocations()[0]->getUnderlyingBufferSize(), commandList->commandContainer.getCmdBufferAllocations()[0]->getUnderlyingBufferSize());
    ASSERT_EQ(commandListControl->commandContainer.getResidencyContainer().size(),
              commandList->commandContainer.getResidencyContainer().size());
    ASSERT_EQ(commandListControl->commandContainer.getDeallocationContainer().size(),
              commandList->commandContainer.getDeallocationContainer().size());
    ASSERT_EQ(commandListControl->getPrintfFunctionContainer().size(),
              commandList->getPrintfFunctionContainer().size());
    ASSERT_EQ(commandListControl->commandContainer.getCommandStream()->getUsed(), commandList->commandContainer.getCommandStream()->getUsed());
    ASSERT_EQ(commandListControl->commandContainer.slmSize, commandList->commandContainer.slmSize);

    for (uint32_t i = 0; i < NEO::HeapType::NUM_TYPES; i++) {
        auto heapType = static_cast<NEO::HeapType>(i);

        ASSERT_NE(nullptr, commandListControl->commandContainer.getIndirectHeapAllocation(heapType));
        ASSERT_NE(nullptr, commandList->commandContainer.getIndirectHeapAllocation(heapType));
        ASSERT_EQ(commandListControl->commandContainer.getIndirectHeapAllocation(heapType)->getUnderlyingBufferSize(),
                  commandList->commandContainer.getIndirectHeapAllocation(heapType)->getUnderlyingBufferSize());

        ASSERT_NE(nullptr, commandListControl->commandContainer.getIndirectHeap(heapType));
        ASSERT_NE(nullptr, commandList->commandContainer.getIndirectHeap(heapType));
        ASSERT_EQ(commandListControl->commandContainer.getIndirectHeap(heapType)->getUsed(),
                  commandList->commandContainer.getIndirectHeap(heapType)->getUsed());

        ASSERT_EQ(commandListControl->commandContainer.isHeapDirty(heapType), commandList->commandContainer.isHeapDirty(heapType));
    }

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListAppendLaunchKernel, WhenAddingKernelsThenResidencyContainerDoesNotContainDuplicatesAfterClosingCommandList) {
    Mock<::L0::Kernel> kernel;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    ze_group_count_t groupCount{1, 1, 1};
    for (int i = 0; i < 4; ++i) {
        auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    commandList->close();

    uint32_t it = 0;
    const auto &residencyCont = commandList->commandContainer.getResidencyContainer();
    for (auto alloc : residencyCont) {
        auto occurences = std::count(residencyCont.begin(), residencyCont.end(), alloc);
        EXPECT_EQ(1U, static_cast<uint32_t>(occurences)) << it;
        ++it;
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenSingleValidWaitEventsThenAddSemaphoreToCommandStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    Mock<::L0::Kernel> kernel;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    std::unique_ptr<EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    std::unique_ptr<Event> event(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t hEventHandle = event->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 1, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
        EXPECT_EQ(cmd->getCompareOperation(),
                  MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(static_cast<uint32_t>(-1), cmd->getSemaphoreDataDword());
        EXPECT_EQ(cmd->getSemaphoreGraphicsAddress() & device->getHwInfo().capabilityTable.gpuAddressSpace, event->getGpuAddress(device) & device->getHwInfo().capabilityTable.gpuAddressSpace);
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenMultipleValidWaitEventsThenAddSemaphoreCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    Mock<::L0::Kernel> kernel;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc1 = {};
    eventDesc1.index = 0;

    ze_event_desc_t eventDesc2 = {};
    eventDesc2.index = 1;

    std::unique_ptr<EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    std::unique_ptr<Event> event1(Event::create<uint32_t>(eventPool.get(), &eventDesc1, device));
    std::unique_ptr<Event> event2(Event::create<uint32_t>(eventPool.get(), &eventDesc2, device));
    ze_event_handle_t hEventHandle1 = event1->toHandle();
    ze_event_handle_t hEventHandle2 = event2->toHandle();

    ze_event_handle_t waitEvents[2];
    waitEvents[0] = hEventHandle1;
    waitEvents[1] = hEventHandle2;

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 2, waitEvents);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto itor = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_FALSE(itor.empty());
    ASSERT_EQ(2, static_cast<int>(itor.size()));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenAppendLaunchMultipleKernelsIndirectThenEnablesPredicate) {
    createKernel();

    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    const ze_kernel_handle_t launchFn = kernel->toHandle();
    uint32_t *numLaunchArgs;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(
        device->toHandle(), &deviceDesc, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
    result = commandList->appendLaunchMultipleKernelsIndirect(1, &launchFn, numLaunchArgs, nullptr, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    *numLaunchArgs = 0;
    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));
    auto itorWalker = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);

    auto cmd = genCmdCast<GPGPU_WALKER *>(*itorWalker);
    EXPECT_TRUE(cmd->getPredicateEnable());
    context->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenAppendLaunchMultipleKernelsThenUsesMathAndWalker) {
    createKernel();

    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_MATH = typename FamilyType::MI_MATH;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    const ze_kernel_handle_t launchFn[3] = {kernel->toHandle(), kernel->toHandle(), kernel->toHandle()};
    uint32_t *numLaunchArgs;
    const uint32_t numKernels = 3;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(
        device->toHandle(), &deviceDesc, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
    result = commandList->appendLaunchMultipleKernelsIndirect(numKernels, launchFn, numLaunchArgs, nullptr, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    *numLaunchArgs = 2;
    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = cmdList.begin();

    for (uint32_t i = 0; i < numKernels; i++) {
        itor = find<MI_MATH *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<GPGPU_WALKER *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
    }

    itor = find<MI_MATH *>(itor, cmdList.end());
    ASSERT_EQ(cmdList.end(), itor);
    context->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

HWTEST_F(CommandListAppendLaunchKernel, givenInvalidEventListWhenAppendLaunchCooperativeKernelIsCalledThenErrorIsReturned) {
    createKernel();

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    returnValue = commandList->appendLaunchCooperativeKernel(kernel->toHandle(), &groupCount, nullptr, 1, nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelUsingSyncBufferWhenAppendLaunchCooperativeKernelIsCalledThenCorrectValueIsReturned, IsAtLeastSkl) {
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.usesSyncBuffer = true;
    kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
    bool isCooperative = true;
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    auto engineGroupType = NEO::EngineGroupType::Compute;
    if (hwHelper.isCooperativeEngineSupported(*defaultHwInfo)) {
        engineGroupType = hwHelper.getEngineGroupType(aub_stream::EngineType::ENGINE_CCS, EngineUsage::Cooperative, *defaultHwInfo);
    }
    pCommandList->initialize(device, engineGroupType, 0u);
    auto result = pCommandList->appendLaunchCooperativeKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, engineGroupType, 0u);
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    {
        VariableBackup<uint32_t> usesSyncBuffer{&kernelAttributes.flags.packed};
        usesSyncBuffer = false;
        pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    {
        VariableBackup<uint32_t> groupCountX{&groupCount.groupCountX};
        uint32_t maximalNumberOfWorkgroupsAllowed;
        kernel.suggestMaxCooperativeGroupCount(&maximalNumberOfWorkgroupsAllowed, engineGroupType, false);
        groupCountX = maximalNumberOfWorkgroupsAllowed + 1;
        pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        pCommandList->initialize(device, engineGroupType, 0u);
        result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
    {
        VariableBackup<bool> cooperative{&isCooperative};
        cooperative = false;
        result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
}

HWTEST2_F(CommandListAppendLaunchKernel, whenUpdateStreamPropertiesIsCalledThenRequiredStateAndFinalStateAreCorrectlySet, IsAtLeastSkl) {
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(-1, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(-1, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);

    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    int32_t expectedDisableOverdispatch = hwInfoConfig.isDisableOverdispatchAvailable(*defaultHwInfo);

    pCommandList->updateStreamProperties(kernel, false, false);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);

    pCommandList->updateStreamProperties(kernel, false, false);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenCooperativeKernelWhenAppendLaunchCooperativeKernelIsCalledThenCommandListTypeIsProperlySet, IsAtLeastSkl) {
    createKernel();
    kernel->setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    bool isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_FALSE(pCommandList->containsCooperativeKernelsFlag);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenAnyCooperativeKernelAndMixingAllowedWhenAppendLaunchCooperativeKernelIsCalledThenCommandListTypeIsProperlySet, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    createKernel();
    kernel->setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);

    bool isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_FALSE(pCommandList->containsCooperativeKernelsFlag);

    isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);

    isCooperative = false;
    result = pCommandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenCooperativeAndNonCooperativeKernelsAndAllowMixingWhenAppendLaunchCooperativeKernelIsCalledThenReturnSuccess, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    bool isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    isCooperative = false;
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
