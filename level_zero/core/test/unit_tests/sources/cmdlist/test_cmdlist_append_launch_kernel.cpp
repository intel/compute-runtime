/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen9/reg_configs.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
namespace L0 {
namespace ult {

struct DualStorageModuleFixture : public ModuleFixture {
    void SetUp() override {
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);
        ModuleFixture::SetUp();
    }
    void TearDown() override {
        ModuleFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};
using CommandListAppendLaunchKernel = Test<ModuleFixture>;
using CommandListDualStroage = Test<DualStorageModuleFixture>;

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithIndirectAllocationsAllowedThenCommandListReturnsExpectedIndirectAllocationsAllowed) {
    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    kernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
    kernel->unifiedMemoryControls.indirectHostAllocationsAllowed = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);
    EXPECT_TRUE(kernel->hasIndirectAllocationsAllowed());

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_FALSE(commandList->hasIndirectAllocationsAllowed());
}

HWTEST_F(CommandListAppendLaunchKernel, givenNotEnoughSpaceInCommandStreamWhenAppendingKernelThenBbEndIsAddedAndNewCmdBufferAllocated) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    createKernel();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));

    auto &commandContainer = commandList->commandContainer;
    const auto stream = commandContainer.getCommandStream();
    const auto streamCpu = stream->getCpuBase();

    auto available = stream->getAvailableSpace();
    stream->getSpace(available - sizeof(MI_BATCH_BUFFER_END) - 16);
    auto bbEndPosition = stream->getSpace(0);

    ze_group_count_t groupCount{1, 1, 1};
    commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ze_group_count_t groupCount{1, 1, 1};

    auto kernelSshSize = kernel->getSurfaceStateHeapDataSize();
    auto ssh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    auto sshHeapSize = ssh->getMaxAvailableSpace();
    auto initialAllocation = ssh->getGraphicsAllocation();
    EXPECT_NE(nullptr, initialAllocation);

    for (size_t i = 0; i < sshHeapSize / kernelSshSize + 1; i++) {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }

    auto reallocatedAllocation = ssh->getGraphicsAllocation();
    EXPECT_NE(nullptr, reallocatedAllocation);
    EXPECT_NE(initialAllocation, reallocatedAllocation);
}

using SklPlusMatcher = IsAtLeastProduct<IGFX_SKYLAKE>;
HWTEST2_F(CommandListAppendLaunchKernel, WhenAppendingFunctionThenUsedCmdBufferSizeDoesNotExceedEstimate, SklPlusMatcher) {
    createKernel();
    ze_group_count_t groupCount{1, 1, 1};

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto sizeBefore = commandList->commandContainer.getCommandStream()->getUsed();

    auto result = commandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto sizeAfter = commandList->commandContainer.getCommandStream()->getUsed();
    auto estimate = NEO::EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(device->getNEODevice());

    EXPECT_LE(sizeAfter - sizeBefore, estimate);

    sizeBefore = commandList->commandContainer.getCommandStream()->getUsed();

    result = commandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, true, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    sizeAfter = commandList->commandContainer.getCommandStream()->getUsed();
    estimate = NEO::EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(device->getNEODevice());

    EXPECT_LE(sizeAfter - sizeBefore, estimate);
    EXPECT_LE(sizeAfter - sizeBefore, estimate);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenEventsWhenAppendingKernelThenPostSyncToEventIsGenerated) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<Event>(Event::create(eventPool.get(), &eventDesc, device));

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
            auto gpuAddress = event->getGpuAddress();
            EXPECT_EQ(cmd->getAddressHigh(), gpuAddress >> 32u);
            EXPECT_EQ(cmd->getAddress(), uint32_t(gpuAddress));
            postSyncFound = true;
        }
    }
    EXPECT_TRUE(postSyncFound);

    {
        auto itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                                   std::end(commandList->commandContainer.getResidencyContainer()),
                                   &event->getAllocation());
        EXPECT_NE(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));
    }
}

using TimestampEventSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandListAppendLaunchKernel, givenTimestampEventsWhenAppendingKernelThenSRMAndPCEncoded, TimestampEventSupport) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<Event>(Event::create(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(
        kernel.toHandle(), &groupCount, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(REG_GLOBAL_TIMESTAMP_LDW, cmd->getRegisterAddress());
    }
    itor++;

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, cmd->getRegisterAddress());
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

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(REG_GLOBAL_TIMESTAMP_LDW, cmd->getRegisterAddress());
    }
    itor++;

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, cmd->getRegisterAddress());
    }

    {
        auto itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                                   std::end(commandList->commandContainer.getResidencyContainer()),
                                   &event->getAllocation());
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
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
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

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<Event>(Event::create(eventPool.get(), &eventDesc, device));

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

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelThenKernelIsExecutedOnImmediateCmdQ, SklPlusMatcher) {
    createKernel();

    Mock<CommandQueue> cmdQueue;

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    EXPECT_CALL(cmdQueue, executeCommandLists).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(cmdQueue, synchronize).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    ze_group_count_t groupCount{1, 1, 1};

    auto result = commandList->appendLaunchKernel(
        kernel->toHandle(),
        &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->cmdQImmediate = nullptr;
}

HWTEST_F(CommandListAppendLaunchKernel, givenIndirectDispatchWhenAppendingThenWorkGroupCountAndGlobalWorkSizeIsSetInCrossThreadData) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    Mock<::L0::Kernel> kernel;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = 2;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));

    void *alloc = nullptr;
    auto result = device->getDriverHandle()->allocDeviceMem(device->toHandle(), 0u, 16384u, 4096u, &alloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     static_cast<ze_group_count_t *>(alloc),
                                                     nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    device->getDriverHandle()->freeMem(alloc);
}

HWTEST_F(CommandListDualStroage, givenIndirectDispatchWithSharedDualStorageMemoryWhenAppendingThenWorkGroupCountAndGlobalWorkSizeIsSetInCrossThreadData) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    Mock<::L0::Kernel> kernel;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = 2;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));

    void *alloc = nullptr;
    auto result = device->getDriverHandle()->allocSharedMem(device->toHandle(), 0u, 0u, 16384u, 4096u, &alloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t *pThreadGroupDimensions = static_cast<ze_group_count_t *>(ptrOffset(alloc, sizeof(ze_group_count_t)));

    pThreadGroupDimensions->groupCountX = 3;
    pThreadGroupDimensions->groupCountY = 4;
    pThreadGroupDimensions->groupCountZ = 5;

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     pThreadGroupDimensions,
                                                     nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(pThreadGroupDimensions);
    ASSERT_NE(nullptr, allocData->cpuAllocation);
    auto gpuAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAllocation);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    uint32_t regAddress = 0;
    uint64_t gpuAddress = 0;
    auto expectedXAddress = reinterpret_cast<uint64_t>(ptrOffset(pThreadGroupDimensions, offsetof(ze_group_count_t, groupCountX)));
    auto expectedYAddress = reinterpret_cast<uint64_t>(ptrOffset(pThreadGroupDimensions, offsetof(ze_group_count_t, groupCountY)));
    auto expectedZAddress = reinterpret_cast<uint64_t>(ptrOffset(pThreadGroupDimensions, offsetof(ze_group_count_t, groupCountZ)));

    auto itor = find<MI_LOAD_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(GPUGPU_DISPATCHDIMX, regAddress);
    EXPECT_EQ(expectedXAddress, gpuAddress);

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(GPUGPU_DISPATCHDIMY, regAddress);
    EXPECT_EQ(expectedYAddress, gpuAddress);

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(GPUGPU_DISPATCHDIMZ, regAddress);
    EXPECT_EQ(expectedZAddress, gpuAddress);

    MI_STORE_REGISTER_MEM *cmd2 = nullptr;
    // Find group count cmds
    do {
        itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
        cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    } while (itor != cmdList.end() && cmd2->getRegisterAddress() != GPUGPU_DISPATCHDIMX);
    EXPECT_NE(cmdList.end(), itor);

    // Find workgroup size cmds
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(CS_GPR_R1, cmd2->getRegisterAddress());

    device->getDriverHandle()->freeMem(alloc);
}

HWTEST_F(CommandListAppendLaunchKernel, givenCommandListWhenResetCalledThenStateIsCleaned) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    createKernel();

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)));
    ASSERT_NE(nullptr, commandList);
    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());

    auto commandListControl = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)));
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
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));

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

using CommandListArbitrationPolicyTest = Test<ModuleFixture>;

HWTEST_F(CommandListArbitrationPolicyTest, whenCreatingCommandListThenDefaultThreadArbitrationPolicyIsUsed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily,
                                                                                          device,
                                                                                          NEO::EngineGroupType::RenderCompute,
                                                                                          returnValue)));
    EXPECT_NE(nullptr, commandList);
    EXPECT_NE(nullptr, commandList->commandContainer.getCommandStream());

    GenCmdList parsedCommandList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        parsedCommandList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommandList.begin(), parsedCommandList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                      cmd->getDataDword());
        }
    }
}

HWTEST_F(CommandListArbitrationPolicyTest, whenCreatingCommandListThenChosenThreadArbitrationPolicyIsUsed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideThreadArbitrationPolicy.set(0);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily,
                                                                                          device,
                                                                                          NEO::EngineGroupType::RenderCompute,
                                                                                          returnValue)));
    EXPECT_NE(nullptr, commandList);
    EXPECT_NE(nullptr, commandList->commandContainer.getCommandStream());

    GenCmdList parsedCommandList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        parsedCommandList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommandList.begin(), parsedCommandList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::AgeBased),
                      cmd->getDataDword());
        }
    }
}

HWTEST_F(CommandListArbitrationPolicyTest, whenCommandListIsResetThenOriginalThreadArbitrationPolicyIsKept) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily,
                                                                                          device,
                                                                                          NEO::EngineGroupType::RenderCompute,
                                                                                          returnValue)));
    EXPECT_NE(nullptr, commandList);
    EXPECT_NE(nullptr, commandList->commandContainer.getCommandStream());

    bool found;
    uint64_t originalThreadArbitrationPolicy = std::numeric_limits<uint64_t>::max();
    {
        GenCmdList parsedCommandList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            parsedCommandList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
            commandList->commandContainer.getCommandStream()->getUsed()));
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

        auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommandList.begin(), parsedCommandList.end());
        EXPECT_GE(2u, miLoadImm.size());

        for (auto it : miLoadImm) {
            auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
            if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
                EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                          cmd->getDataDword());
                originalThreadArbitrationPolicy = cmd->getDataDword();
                found = false;
            }
        }
    }

    commandList->reset();

    {
        GenCmdList parsedCommandList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            parsedCommandList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
            commandList->commandContainer.getCommandStream()->getUsed()));
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

        auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommandList.begin(), parsedCommandList.end());
        EXPECT_GE(2u, miLoadImm.size());

        uint64_t newThreadArbitrationPolicy = std::numeric_limits<uint64_t>::max();
        for (auto it : miLoadImm) {
            auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
            if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
                EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                          cmd->getDataDword());
                newThreadArbitrationPolicy = cmd->getDataDword();
                EXPECT_EQ(originalThreadArbitrationPolicy, newThreadArbitrationPolicy);
            }
        }
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenSingleValidWaitEventsAddsSemaphoreToCommandStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    Mock<::L0::Kernel> kernel;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    std::unique_ptr<EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    std::unique_ptr<Event> event(Event::create(eventPool.get(), &eventDesc, device));
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
        EXPECT_EQ(cmd->getSemaphoreGraphicsAddress() & device->getHwInfo().capabilityTable.gpuAddressSpace, event->getGpuAddress() & device->getHwInfo().capabilityTable.gpuAddressSpace);
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenMultipleValidWaitEventsAddsSemaphoreCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    Mock<::L0::Kernel> kernel;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc1 = {};
    eventDesc1.index = 0;

    ze_event_desc_t eventDesc2 = {};
    eventDesc2.index = 1;

    std::unique_ptr<EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    std::unique_ptr<Event> event1(Event::create(eventPool.get(), &eventDesc1, device));
    std::unique_ptr<Event> event2(Event::create(eventPool.get(), &eventDesc2, device));
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
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    const ze_kernel_handle_t launchFn = kernel->toHandle();
    uint32_t *numLaunchArgs;
    auto result = device->getDriverHandle()->allocDeviceMem(
        device->toHandle(), 0u, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
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
    device->getDriverHandle()->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenAppendLaunchMultipleKernelsThenUsesMathAndWalker) {
    createKernel();

    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_MATH = typename FamilyType::MI_MATH;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    const ze_kernel_handle_t launchFn[3] = {kernel->toHandle(), kernel->toHandle(), kernel->toHandle()};
    uint32_t *numLaunchArgs;
    const uint32_t numKernels = 3;
    auto result = device->getDriverHandle()->allocDeviceMem(
        device->toHandle(), 0u, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
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
    device->getDriverHandle()->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

} // namespace ult
} // namespace L0
