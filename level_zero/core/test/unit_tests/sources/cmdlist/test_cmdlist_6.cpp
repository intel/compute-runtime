/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using MultiTileImmediateCommandListTest = Test<MultiTileCommandListFixture<true, false, false>>;

HWTEST2_F(MultiTileImmediateCommandListTest, GivenMultiTileDeviceWhenCreatingImmediateCommandListThenExpectPartitionCountMatchTileCount, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, commandList->partitionCount);
}

using MultiTileImmediateInternalCommandListTest = Test<MultiTileCommandListFixture<true, true, false>>;

HWTEST2_F(MultiTileImmediateInternalCommandListTest, GivenMultiTileDeviceWhenCreatingInternalImmediateCommandListThenExpectPartitionCountEqualOne, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using MultiTileCopyEngineCommandListTest = Test<MultiTileCommandListFixture<false, false, true>>;

HWTEST2_F(MultiTileCopyEngineCommandListTest, GivenMultiTileDeviceWhenCreatingCopyEngineCommandListThenExpectPartitionCountEqualOne, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using CommandListExecuteImmediate = Test<DeviceFixture>;
HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenRequiredStreamStateIsCorrectlyReported, IsAtLeastSkl) {
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    auto &currentCsrStreamProperties = commandListImmediate.csr->getStreamProperties();

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::RoundRobin;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);

    int expectedDisableOverdispatch = hwInfoConfig.isDisableOverdispatchAvailable(*defaultHwInfo);
    bool expectedIsCoherencyRequired = hwHelper.forceNonGpuCoherencyWA(true);
    int expectedLargeGrfMode = hwInfoConfig.isGrfNumReportedWithScm() ? 1 : -1;
    int expectedThreadArbitrationPolicy = hwInfoConfig.isThreadArbitrationPolicyReportedWithScm() ? NEO::ThreadArbitrationPolicy::RoundRobin : -1;
    EXPECT_EQ(1, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(expectedDisableOverdispatch, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedIsCoherencyRequired, currentCsrStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(expectedLargeGrfMode, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(expectedThreadArbitrationPolicy, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::AgeBased;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);

    expectedLargeGrfMode = hwInfoConfig.isGrfNumReportedWithScm() ? 0 : -1;
    expectedThreadArbitrationPolicy = hwInfoConfig.isThreadArbitrationPolicyReportedWithScm() ? NEO::ThreadArbitrationPolicy::AgeBased : -1;
    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(0, currentCsrStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(expectedLargeGrfMode, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(expectedThreadArbitrationPolicy, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenContainsAnyKernelFlagIsReset, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    commandListImmediate.containsAnyKernel = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);
    EXPECT_FALSE(commandListImmediate.containsAnyKernel);
}

using CommandListTest = Test<DeviceFixture>;
using IsDcFlushSupportedPlatform = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_XE_HP_CORE>;

HWTEST2_F(CommandListTest, givenCopyCommandListWhenRequiredFlushOperationThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationThenExpectPipeControlWithDcFlush, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(sizeof(PIPE_CONTROL), usedAfter - usedBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));
    auto pipeControl = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(pipeControl, cmdList.end());
    auto cmdPipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControl);
    EXPECT_TRUE(cmdPipeControl->getDcFlushEnable());
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenNoRequiredFlushOperationThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(false, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationAndNoSignalScopeEventThenExpectPipeControlWithDcFlush, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    ze_result_t result;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t eventHandle = event->toHandle();

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, eventHandle);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(sizeof(PIPE_CONTROL), usedAfter - usedBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));
    auto pipeControl = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(pipeControl, cmdList.end());
    auto cmdPipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControl);
    EXPECT_TRUE(cmdPipeControl->getDcFlushEnable());
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationAndSignalScopeEventThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    ze_result_t result;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t eventHandle = event->toHandle();

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, eventHandle);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenImmediateCommandListWhenAppendMemoryRangesBarrierUsingFlushTaskThenExpectCorrectExecuteCall, IsAtLeastSkl) {
    ze_result_t result;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.isFlushTaskSubmissionEnabled = true;
    cmdList.cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    result = cmdList.appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                               ranges, nullptr, 0,
                                               nullptr);
    EXPECT_EQ(0u, cmdList.executeCommandListImmediateCalledCount);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListTest, givenImmediateCommandListWhenAppendMemoryRangesBarrierNotUsingFlushTaskThenExpectCorrectExecuteCall, IsAtLeastSkl) {
    ze_result_t result;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.isFlushTaskSubmissionEnabled = false;
    cmdList.cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    result = cmdList.appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                               ranges, nullptr, 0,
                                               nullptr);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateCalledCount);
    EXPECT_EQ(0u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
