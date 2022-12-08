/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "test_traits_common.h"

namespace L0 {
namespace ult {

using CommandListTests = Test<DeviceFixture>;
HWCMDTEST_F(IGFX_XE_HP_CORE, CommandListTests, whenCommandListIsCreatedThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPc = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPc);
    auto cmdPc = genCmdCast<PIPE_CONTROL *>(*itorPc);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*cmdPc));
    EXPECT_TRUE(cmdPc->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(cmdPc->getCommandStreamerStallEnable());

    auto itor = find<STATE_BASE_ADDRESS *>(itorPc, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    if (device->getNEODevice()->getDeviceInfo().imageSupport) {
        auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
        EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
        EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    }

    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());

    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

HWTEST2_F(CommandListTests, whenCommandListIsCreatedAndProgramExtendedPipeControlPriorToNonPipelinedStateCommandIsEnabledThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed, IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPc = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPc);
    auto cmdPc = genCmdCast<PIPE_CONTROL *>(*itorPc);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*cmdPc));
    EXPECT_TRUE(cmdPc->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(cmdPc->getCommandStreamerStallEnable());

    if constexpr (TestTraits<gfxCoreFamily>::isPipeControlExtendedPriorToNonPipelinedStateCommandSupported) {
        EXPECT_TRUE(cmdPc->getAmfsFlushEnable());
        EXPECT_TRUE(cmdPc->getInstructionCacheInvalidateEnable());
        EXPECT_TRUE(cmdPc->getConstantCacheInvalidationEnable());
        EXPECT_TRUE(cmdPc->getStateCacheInvalidationEnable());

        if constexpr (TestTraits<gfxCoreFamily>::isUnTypedDataPortCacheFlushSupported) {
            EXPECT_TRUE(cmdPc->getUnTypedDataPortCacheFlush());
        }
    }

    auto itor = find<STATE_BASE_ADDRESS *>(itorPc, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    if (device->getNEODevice()->getDeviceInfo().imageSupport) {
        auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
        EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
        EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    }
    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());

    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

using MultiTileCommandListTests = Test<MultiTileCommandListFixture<false, false, false>>;
HWCMDTEST_F(IGFX_XE_HP_CORE, MultiTileCommandListTests, givenPartitionedCommandListWhenCommandListIsCreatedThenStateBaseAddressCmdWithMultiPartialAndAtomicsCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    EXPECT_EQ(2u, commandList->partitionCount);
    auto &commandContainer = commandList->commandContainer;

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorSba = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorSba);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itorSba);

    EXPECT_FALSE(cmdSba->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

using CommandListTestsReserveSize = Test<DeviceFixture>;
HWTEST2_F(CommandListTestsReserveSize, givenCommandListWhenGetReserveSshSizeThen4PagesReturned, IsAtLeastXeHpCore) {
    L0::CommandListCoreFamily<gfxCoreFamily> commandList(1u);

    EXPECT_EQ(commandList.getReserveSshSize(), 4 * MemoryConstants::pageSize);
}

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
HWTEST2_F(CommandListAppendLaunchKernel, givenVariousKernelsWhenUpdateStreamPropertiesIsCalledThenRequiredStateFinalStateAndCommandsToPatchAreCorrectlySet, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    DebugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);

    Mock<::L0::Kernel> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::Kernel> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(-1, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(-1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());

    auto &hwInfoConfig = *NEO::HwInfoConfig::get(device->getHwInfo().platform.eProductFamily);
    int32_t expectedDispatchAllWalkerEnable = hwInfoConfig.isComputeDispatchAllWalkerEnableInCfeStateRequired(device->getHwInfo()) ? 0 : -1;

    pCommandList->updateStreamProperties(defaultKernel, false);

    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(cooperativeKernel, true);
    pCommandList->updateStreamProperties(cooperativeKernel, true);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 1 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(defaultKernel, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 0 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 1 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    size_t expectedCommandsToPatch = expectedDispatchAllWalkerEnable != -1 ? 1 : 0;
    EXPECT_EQ(expectedCommandsToPatch, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(cooperativeKernel, true);
    pCommandList->updateStreamProperties(defaultKernel, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true);
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    expectedCommandsToPatch = expectedCommandsToPatch != 0 ? 2 : 0;
    EXPECT_EQ(expectedCommandsToPatch, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(defaultKernel, false);
    pCommandList->updateStreamProperties(defaultKernel, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 0 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 1 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    expectedCommandsToPatch = expectedCommandsToPatch != 0 ? 1 : 0;
    EXPECT_EQ(expectedCommandsToPatch, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    EXPECT_EQ(-1, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(-1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenVariousKernelsAndPatchingDisallowedWhenUpdateStreamPropertiesIsCalledThenCommandsToPatchAreEmpty, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    Mock<::L0::Kernel> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::Kernel> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList->updateStreamProperties(defaultKernel, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    DebugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);
    pCommandList->updateStreamProperties(defaultKernel, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true);

    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(device->getHwInfo().platform.eProductFamily);
    size_t expectedCmdsToPatch = hwInfoConfig.isComputeDispatchAllWalkerEnableInCfeStateRequired(device->getHwInfo()) ? 1 : 0;

    EXPECT_EQ(expectedCmdsToPatch, pCommandList->commandsToPatch.size());
    pCommandList->reset();
}

struct AppendKernelTestInput {
    DriverHandle *driver = nullptr;
    L0::Context *context = nullptr;
    L0::Device *device = nullptr;

    ze_event_pool_flags_t eventPoolFlags = 0;

    uint32_t packetOffsetMul = 1;

    bool useFirstEventPacketAddress = false;
};

template <int32_t compactL3FlushEventPacket, uint32_t multiTile>
struct CommandListAppendLaunchKernelCompactL3FlushEventFixture : public ModuleFixture {
    void setUp() {
        DebugManager.flags.CompactL3FlushEventPacket.set(compactL3FlushEventPacket);
        DebugManager.flags.SignalAllEventPackets.set(0);
        if constexpr (multiTile == 1) {
            DebugManager.flags.CreateMultipleSubDevices.set(2);
            DebugManager.flags.EnableImplicitScaling.set(1);
            arg.workloadPartition = true;
            arg.expectDcFlush = 2; // DC Flush multi-tile platforms require DC Flush + x-tile sync after implicit scaling COMPUTE_WALKER
            input.packetOffsetMul = 2;
        } else {
            arg.expectDcFlush = 1;
        }
        ModuleFixture::setUp();

        input.driver = driverHandle.get();
        input.context = context;
        input.device = device;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendLaunchKernelAndL3Flush(AppendKernelTestInput &input, TestExpectedValues &arg) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
        using OPERATION = typename POSTSYNC_DATA::OPERATION;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        Mock<::L0::Kernel> kernel;
        auto module = std::unique_ptr<Module>(new Mock<Module>(input.device, nullptr));
        kernel.module = module.get();

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = input.eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

        uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);

        ze_group_count_t groupCount{1, 1, 1};
        CmdListKernelLaunchParams launchParams = {};
        result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, event->toHandle(), 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
        EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
            commandList->commandContainer.getCommandStream()->getUsed()));

        auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
        EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

        uint64_t l3FlushPostSyncAddress = event->getGpuAddress(input.device) + input.packetOffsetMul * event->getSinglePacketSize();
        if (input.useFirstEventPacketAddress) {
            l3FlushPostSyncAddress = event->getGpuAddress(input.device);
        }
        if (event->isUsingContextEndOffset()) {
            l3FlushPostSyncAddress += event->getContextEndOffset();
        }

        auto itorPipeControls = findAll<PIPE_CONTROL *>(firstWalker, cmdList.end());

        uint32_t postSyncPipeControls = 0;
        uint32_t dcFlushFound = 0;
        for (auto it : itorPipeControls) {
            auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
            if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                postSyncPipeControls++;
                EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                if (arg.workloadPartition) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }
            }
            if (cmd->getDcFlushEnable()) {
                dcFlushFound++;
            }
        }
        EXPECT_EQ(arg.expectedPostSyncPipeControls, postSyncPipeControls);
        EXPECT_EQ(arg.expectDcFlush, dcFlushFound);

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        if constexpr (multiTile == 1) {
            EXPECT_EQ(3u, itorStoreDataImm.size());
        } else {
            EXPECT_EQ(0u, itorStoreDataImm.size());
        }
    }

    DebugManagerStateRestore restorer;

    AppendKernelTestInput input = {};
    TestExpectedValues arg = {};
};

using CommandListAppendLaunchKernelCompactL3FlushDisabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<0, 0>>;

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushDisabledTest,
          givenAppendKernelWithSignalScopeTimestampEventWhenComputeWalkerTimestampPostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushDisabledTest,
          givenAppendKernelWithSignalScopeImmediateEventWhenComputeWalkerImmediatePostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    auto &l0GfxCoreHelper = input.device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = l0GfxCoreHelper.multiTileCapablePlatform() ? 3 : 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = 0;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelCompactL3FlushEnabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<1, 0>>;

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushEnabledTest,
          givenAppendKernelWithSignalScopeTimestampEventWhenRegisterTimestampPostsyncUsedThenExpectNoComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 1;
    arg.expectedPostSyncPipeControls = 0;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushEnabledTest,
          givenAppendKernelWithSignalScopeImmediateEventWhenL3ImmediatePostsyncUsedThenExpectPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<0, 1>>;

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest,
          givenAppendMultiTileKernelWithSignalScopeTimestampEventWhenComputeWalkerTimestampPostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 4;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest,
          givenAppendMultiTileKernelWithSignalScopeImmediateEventWhenComputeWalkerImmediatePostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 4;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = 0;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<1, 1>>;

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest,
          givenAppendMultiTileKernelWithSignalScopeTimestampEventWhenRegisterTimestampPostsyncUsedThenExpectNoComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 0;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest,
          givenAppendMultiTileKernelWithSignalScopeImmediateEventWhenL3ImmediatePostsyncUsedThenExpectPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListTests, GivenCopyCommandListWhenSettingRemainingEventPacketsThenExpectMiDwordFlushCommandsProgrammingPackets, IsAtLeastXeHpCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;

    NEO::DebugManager.flags.UseDynamicEventPacketsCount.set(1);
    NEO::DebugManager.flags.SignalAllEventPackets.set(1);
    NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.set(0);
    NEO::DebugManager.flags.CompactL3FlushEventPacket.set(0);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdStream = commandList->commandContainer.getCommandStream();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event.get());

    uint32_t packetUsed = event->getPacketsInUse();
    uint32_t remainingPackets = event->getMaxPacketsCount() - packetUsed;

    size_t sizeBefore = cmdStream->getUsed();
    commandList->setRemainingEventPackets(event.get(), Event::STATE_SIGNALED);
    size_t sizeAfter = cmdStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), sizeBefore),
        (sizeAfter - sizeBefore)));

    uint32_t expectedMiFlushCount = remainingPackets;
    if (NEO::EncodeMiFlushDW<FamilyType>::getMiFlushDwWaSize() > 0) {
        expectedMiFlushCount *= 2;
    }

    auto miFlushList = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(expectedMiFlushCount, static_cast<uint32_t>(miFlushList.size()));

    uint64_t gpuAddress = event->getGpuAddress(device);
    gpuAddress += (packetUsed * event->getSinglePacketSize());
    if (event->isUsingContextEndOffset()) {
        gpuAddress += event->getContextEndOffset();
    }

    for (uint32_t i = 0; i < expectedMiFlushCount; i++) {
        if ((expectedMiFlushCount == 2 * remainingPackets) && (i % 2 == 0)) {
            continue;
        }
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*miFlushList[i]);
        EXPECT_EQ(gpuAddress, cmd->getDestinationAddress());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, cmd->getPostSyncOperation());

        gpuAddress += event->getSinglePacketSize();
    }
}

template <uint32_t multiTile, uint32_t limitEventPacketes>
struct CommandListSignalAllEventPacketFixture : public ModuleFixture {
    void setUp() {
        NEO::DebugManager.flags.UseDynamicEventPacketsCount.set(1);
        NEO::DebugManager.flags.SignalAllEventPackets.set(1);

        if constexpr (limitEventPacketes == 1) {
            NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.set(1);
            NEO::DebugManager.flags.CompactL3FlushEventPacket.set(1);
        } else {
            NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.set(0);
            NEO::DebugManager.flags.CompactL3FlushEventPacket.set(0);
        }
        if constexpr (multiTile == 1) {
            DebugManager.flags.CreateMultipleSubDevices.set(2);
            DebugManager.flags.EnableImplicitScaling.set(1);
        }
        ModuleFixture::setUp();

        this->module = std::make_unique<Mock<Module>>(device, nullptr);
        this->kernel.module = this->module.get();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendKernel(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
        using OPERATION = typename POSTSYNC_DATA::OPERATION;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event.get());

        ze_group_count_t groupCount{1, 1, 1};
        CmdListKernelLaunchParams launchParams = {};
        size_t sizeBefore = cmdStream->getUsed();
        result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, event->toHandle(), 0, nullptr, launchParams);
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];

        uint32_t expectedWalkerPostSyncOp = 3;
        if (multiTile == 0 && eventPoolFlags == 0 && !l0GfxCoreHelper.multiTileCapablePlatform()) {
            expectedWalkerPostSyncOp = 1;
        }
        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());

        uint32_t extraCleanupStoreDataImm = 0;
        if (multiTile == 1 && NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired()) {
            extraCleanupStoreDataImm = 3;
        }

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(firstWalker, cmdList.end());
        if constexpr (limitEventPacketes == 1) {
            ASSERT_EQ(extraCleanupStoreDataImm, itorStoreDataImm.size());
        } else {
            uint32_t packetUsed = event->getPacketsInUse();
            uint32_t remainingPackets = event->getMaxPacketsCount() - packetUsed;
            remainingPackets /= commandList->partitionCount;
            ASSERT_EQ(remainingPackets + extraCleanupStoreDataImm, static_cast<uint32_t>(itorStoreDataImm.size()));

            uint64_t gpuAddress = event->getGpuAddress(device);
            gpuAddress += (packetUsed * event->getSinglePacketSize());
            if (event->isUsingContextEndOffset()) {
                gpuAddress += event->getContextEndOffset();
            }

            for (uint32_t i = extraCleanupStoreDataImm; i < itorStoreDataImm.size(); i++) {
                auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                EXPECT_EQ(gpuAddress, cmd->getAddress());
                EXPECT_FALSE(cmd->getStoreQword());
                EXPECT_EQ(0u, cmd->getDataDword0());
                if constexpr (multiTile == 1) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }
                gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendSignalEvent(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event.get());

        size_t sizeBefore = cmdStream->getUsed();
        result = commandList->appendSignalEvent(event->toHandle());
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

        uint32_t extraSignalStoreDataImm = 0;
        if (eventPoolFlags == 0) {
            extraSignalStoreDataImm = 1; // used packet reset for "non-TS, non-signal scope on DC Flush platforms" events performed by SDI command, other resets are via PIPE_CONTROL w/postsync
        }

        if constexpr (limitEventPacketes == 1) {
            ASSERT_EQ(extraSignalStoreDataImm, itorStoreDataImm.size());
        } else {
            uint32_t packetUsed = event->getPacketsInUse();
            uint32_t remainingPackets = event->getMaxPacketsCount() - packetUsed;
            remainingPackets /= commandList->partitionCount;
            ASSERT_EQ(remainingPackets + extraSignalStoreDataImm, static_cast<uint32_t>(itorStoreDataImm.size()));

            uint64_t gpuAddress = event->getGpuAddress(device);
            gpuAddress += (packetUsed * event->getSinglePacketSize());
            if (event->isUsingContextEndOffset()) {
                gpuAddress += event->getContextEndOffset();
            }

            for (uint32_t i = extraSignalStoreDataImm; i < itorStoreDataImm.size(); i++) {
                auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                EXPECT_EQ(gpuAddress, cmd->getAddress());
                EXPECT_FALSE(cmd->getStoreQword());
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
                if constexpr (multiTile == 1) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }
                gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendSignalEventForProfiling() {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event.get());

        commandList->setupTimestampEventForMultiTile(event.get());
        size_t sizeBefore = cmdStream->getUsed();
        commandList->appendEventForProfiling(event.get(), false);
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

        if constexpr (limitEventPacketes == 1) {
            constexpr uint32_t expectedStoreDataImm = 0;
            ASSERT_EQ(expectedStoreDataImm, itorStoreDataImm.size());
        } else {
            uint32_t packetUsed = event->getPacketsInUse();
            uint32_t remainingPackets = event->getMaxPacketsCount() - packetUsed;
            remainingPackets /= commandList->partitionCount;
            ASSERT_EQ(remainingPackets, static_cast<uint32_t>(itorStoreDataImm.size()));

            uint64_t gpuAddress = event->getGpuAddress(device);
            gpuAddress += (packetUsed * event->getSinglePacketSize());
            if (event->isUsingContextEndOffset()) {
                gpuAddress += event->getContextEndOffset();
            }

            for (uint32_t i = 0; i < remainingPackets; i++) {
                auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                EXPECT_EQ(gpuAddress, cmd->getAddress());
                EXPECT_FALSE(cmd->getStoreQword());
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
                if constexpr (multiTile == 1) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }
                gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendSignalEventImmediate() {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = 0;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event.get());

        size_t sizeBefore = cmdStream->getUsed();
        commandList->appendSignalEventPostWalker(event.get());
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

        if constexpr (limitEventPacketes == 1) {
            constexpr uint32_t expectedStoreDataImm = 0;
            ASSERT_EQ(expectedStoreDataImm, itorStoreDataImm.size());
        } else {
            uint32_t packetUsed = event->getPacketsInUse();
            uint32_t remainingPackets = event->getMaxPacketsCount() - packetUsed;
            remainingPackets /= commandList->partitionCount;
            ASSERT_EQ(remainingPackets, static_cast<uint32_t>(itorStoreDataImm.size()));

            uint64_t gpuAddress = event->getGpuAddress(device);
            gpuAddress += (packetUsed * event->getSinglePacketSize());
            if (event->isUsingContextEndOffset()) {
                gpuAddress += event->getContextEndOffset();
            }

            for (uint32_t i = 0; i < remainingPackets; i++) {
                auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                EXPECT_EQ(gpuAddress, cmd->getAddress());
                EXPECT_FALSE(cmd->getStoreQword());
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
                if constexpr (multiTile == 1) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }
                gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendWaitEvent(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        using COMPARE_OPERATION = typename FamilyType::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event.get());

        size_t sizeBefore = cmdStream->getUsed();
        auto eventHandle = event->toHandle();
        result = commandList->appendWaitOnEvents(1, &eventHandle, false);
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        auto itorSemWait = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        if constexpr (limitEventPacketes == 1 && multiTile == 0) {
            ASSERT_EQ(1u, itorSemWait.size());
        } else {
            uint32_t allPackets = event->getMaxPacketsCount();
            ASSERT_EQ(allPackets, static_cast<uint32_t>(itorSemWait.size()));

            uint64_t gpuAddress = event->getGpuAddress(device);
            if (event->isUsingContextEndOffset()) {
                gpuAddress += event->getContextEndOffset();
            }

            for (uint32_t i = 0; i < allPackets; i++) {
                auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itorSemWait[i]);
                EXPECT_EQ(gpuAddress, cmd->getSemaphoreGraphicsAddress());
                EXPECT_EQ(COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, cmd->getCompareOperation());
                EXPECT_EQ(Event::STATE_CLEARED, cmd->getSemaphoreDataDword());
                gpuAddress += event->getSinglePacketSize();
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendResetEvent(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event.get());

        if (this->alignEventPacketsForReset) {
            event->setPacketsInUse(commandList->partitionCount);
        }

        size_t sizeBefore = cmdStream->getUsed();
        result = commandList->appendEventReset(event->toHandle());
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

        uint32_t extraCleanupStoreDataImm = 0;
        if constexpr (multiTile == 1) {
            // multi-tile barrier self-cleanup
            extraCleanupStoreDataImm = 2;
        }

        if constexpr (limitEventPacketes == 1) { // single packet for single tile, two packets for two tiles
            uint32_t expectedStoreDataImm = 0;   // single packet will be reset by PC or SDI - assume here PC is used for timestamp event
            if constexpr (multiTile == 1) {
                expectedStoreDataImm = 1; // single SDI to reset second packet
            }
            if (eventPoolFlags == 0) {
                expectedStoreDataImm++; // but for immediate events, SDI is used instead PC, then add 1 here
            }
            ASSERT_EQ(expectedStoreDataImm + extraCleanupStoreDataImm, itorStoreDataImm.size());
        } else {
            // TS events reset uses getMaxPacketsCount(), no need to reset not used packets
            if (eventPoolFlags == ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP) {
                uint64_t gpuAddress = event->getGpuAddress(device);
                gpuAddress += event->getContextEndOffset();
                // last packet is reset by PIPE_CONTROL w/ post sync
                uint32_t expectedStoreDataImm = event->getMaxPacketsCount() - 1;

                ASSERT_EQ(expectedStoreDataImm + extraCleanupStoreDataImm, static_cast<uint32_t>(itorStoreDataImm.size()));
                for (uint32_t i = 0; i < expectedStoreDataImm; i++) {
                    auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                    EXPECT_EQ(gpuAddress, cmd->getAddress());
                    EXPECT_FALSE(cmd->getStoreQword());
                    EXPECT_EQ(Event::STATE_CLEARED, cmd->getDataDword0());
                    gpuAddress += event->getSinglePacketSize();
                }
            } else {
                uint32_t packetUsed = event->getPacketsInUse();
                uint32_t remainingResetSdiCommands = event->getMaxPacketsCount() - packetUsed;

                uint32_t packetOffsetFactor = 1;
                uint32_t usePacketSignalStoreDataImm = 1; // single SDI to reset single packet in single tile
                bool usePartitioningWrite = false;
                if (this->alignEventPacketsForReset) {
                    remainingResetSdiCommands /= commandList->partitionCount;
                    packetOffsetFactor = commandList->partitionCount;

                    if constexpr (multiTile == 1) {
                        usePacketSignalStoreDataImm++; // and two SDI to reset two packets in multi tile
                        usePartitioningWrite = true;   // only when number of not used packets is aligned to partition count, multi-tile reset can be split to both tiles
                    }
                }

                ASSERT_EQ(remainingResetSdiCommands + usePacketSignalStoreDataImm + extraCleanupStoreDataImm, static_cast<uint32_t>(itorStoreDataImm.size()));

                uint64_t gpuAddress = event->getGpuAddress(device);
                gpuAddress += (packetUsed * event->getSinglePacketSize());
                if (event->isUsingContextEndOffset()) {
                    gpuAddress += event->getContextEndOffset();
                }

                for (uint32_t i = usePacketSignalStoreDataImm; i < itorStoreDataImm.size() - extraCleanupStoreDataImm; i++) {
                    auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                    EXPECT_EQ(gpuAddress, cmd->getAddress());
                    EXPECT_FALSE(cmd->getStoreQword());
                    EXPECT_EQ(Event::STATE_CLEARED, cmd->getDataDword0());
                    if (usePartitioningWrite) {
                        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                    } else {
                        EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                    }
                    gpuAddress += (event->getSinglePacketSize() * packetOffsetFactor);
                }
            }
        }
    }

    DebugManagerStateRestore restorer;

    Mock<::L0::Kernel> kernel;
    std::unique_ptr<Mock<Module>> module;
    bool alignEventPacketsForReset = true;
};

using CommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<0, 0>>;
HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventImmediate<gfxCoreFamily>();
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

using MultiTileCommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<1, 0>>;
HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventImmediate<gfxCoreFamily>();
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeHpCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeHpCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<gfxCoreFamily>(0);
}

using CommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<0, 1>>;
HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventImmediate<gfxCoreFamily>();
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

using MultiTileCommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<1, 1>>;
HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatchedAsDefaultActiveSinglePacket, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatchedAsDefaultActiveSinglePacket, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventImmediate<gfxCoreFamily>();
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeHpCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeHpCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<gfxCoreFamily>(0);
}
} // namespace ult
} // namespace L0
