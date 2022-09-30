/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
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

using AppendMemoryCopyXeHpAndLater = Test<DeviceFixture>;
using MultiTileAppendMemoryCopyXeHpAndLater = Test<ImplicitScalingRootDevice>;

HWTEST2_F(AppendMemoryCopyXeHpAndLater,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernels,
          IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename GfxFamily::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1231);
    void *dstPtr = reinterpret_cast<void *>(0x200002345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    uint64_t firstKernelEventAddress = event->getGpuAddress(device);
    uint64_t secondKernelEventAddress = event->getGpuAddress(device) + event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = event->getGpuAddress(device) + 2 * event->getSinglePacketSize();

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100002345, event->toHandle(), 0, nullptr);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(3u, event->getPacketsInUse());
    EXPECT_EQ(3u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];
    auto thirdWalker = itorWalkers[2];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*thirdWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(thirdKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLater,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernels,
          IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename GfxFamily::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    EXPECT_EQ(2u, commandList.partitionCount);
    void *srcPtr = reinterpret_cast<void *>(0x1231);
    void *dstPtr = reinterpret_cast<void *>(0x200002345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    uint64_t firstKernelEventAddress = event->getGpuAddress(device);
    uint64_t secondKernelEventAddress = event->getGpuAddress(device) + 2 * event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = event->getGpuAddress(device) + 4 * event->getSinglePacketSize();

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100002345, event->toHandle(), 0, nullptr);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(6u, event->getPacketsInUse());
    EXPECT_EQ(3u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];
    auto thirdWalker = itorWalkers[2];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*thirdWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(thirdKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
}

HWTEST2_F(AppendMemoryCopyXeHpAndLater,
          givenCommandListAndEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernelsAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename GfxFamily::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1231);
    void *dstPtr = reinterpret_cast<void *>(0x200002345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    uint64_t firstKernelEventAddress = event->getGpuAddress(device);
    uint64_t secondKernelEventAddress = event->getGpuAddress(device) + event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = event->getGpuAddress(device) + 2 * event->getSinglePacketSize();

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100002345, event->toHandle(), 0, nullptr);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(4u, event->getPacketsInUse());
    EXPECT_EQ(3u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];
    auto thirdWalker = itorWalkers[2];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*thirdWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(thirdKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    uint64_t l3FlushPostSyncAddress = thirdKernelEventAddress + event->getSinglePacketSize();
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
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }
    EXPECT_EQ(1u, postSyncPipeControls);
    EXPECT_EQ(1u, dcFlushFound);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLater,
          givenMultiTileCommandListAndEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernelsAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    EXPECT_EQ(2u, commandList.partitionCount);
    auto &commandContainer = commandList.commandContainer;

    void *srcPtr = reinterpret_cast<void *>(0x1231);
    void *dstPtr = reinterpret_cast<void *>(0x200002345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    uint64_t firstKernelEventAddress = event->getGpuAddress(device);
    uint64_t secondKernelEventAddress = event->getGpuAddress(device) + 2 * event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = event->getGpuAddress(device) + 4 * event->getSinglePacketSize();

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100002345, event->toHandle(), 0, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(8u, event->getPacketsInUse());
    EXPECT_EQ(3u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];
    auto thirdWalker = itorWalkers[2];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*thirdWalker);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(thirdKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    uint64_t l3FlushPostSyncAddress = thirdKernelEventAddress + 2 * event->getSinglePacketSize();
    if (event->isUsingContextEndOffset()) {
        l3FlushPostSyncAddress += event->getContextEndOffset();
    }

    auto itorPipeControls = findAll<PIPE_CONTROL *>(thirdWalker, cmdList.end());

    uint32_t postSyncPipeControls = 0;
    uint32_t dcFlushFound = 0;

    for (auto it : itorPipeControls) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncPipeControls++;
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }

    constexpr uint32_t expectedDcFlush = 2; //dc flush for last cross-tile sync and separately for signal scope event after last kernel split
    EXPECT_EQ(1u, postSyncPipeControls);
    EXPECT_EQ(expectedDcFlush, dcFlushFound);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLater,
          givenCommandListWhenMemoryCopyWithSignalEventScopeSetToSubDeviceThenB2BPipeControlIsAddedWithDcFlushWithPostSyncForLastPC, IsXeHpOrXeHpgCore) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_SUBDEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, event.get(), 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalker = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);

    auto pipeControls = findAll<PIPE_CONTROL *>(itorWalker, cmdList.end());
    uint32_t postSyncFound = 0;
    uint32_t dcFlushFound = 0;
    ASSERT_NE(0u, pipeControls.size());
    for (auto it : pipeControls) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA &&
            cmd->getImmediateData() == Event::STATE_SIGNALED) {
            postSyncFound++;
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }

    constexpr uint32_t expectedDcFlushFound = 1u;

    EXPECT_EQ(1u, postSyncFound);
    EXPECT_EQ(expectedDcFlushFound, dcFlushFound);
}

} // namespace ult
} // namespace L0
