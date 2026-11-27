/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture_extended.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/driver_experimental/zex_api.h"

#include <type_traits>
#include <variant>

namespace L0 {
namespace ult {

using InOrderCmdListTests = InOrderCmdListFixture;

HWTEST_F(InOrderCmdListTests, givenEmptyCounterBasedEventWhenPassedAsWaitEventToRegularCmdListThenSkipWait) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());

    auto eventHandle = events[0]->toHandle();
    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    const auto offset = cmdStream->getUsed();

    CmdListWaitEventParameters waitEventsParameters{
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = false,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
        .apiRequest = true};

    EXPECT_EQ(ZE_RESULT_SUCCESS, regularCmdList->appendWaitOnEvents(1, &eventHandle, waitEventsParameters));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    EXPECT_TRUE(findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end()).empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCmdsChainingFromAppendCopyWhenDispatchingKernelThenProgramSemaphoreOnce) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(1);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    bool heaplessEnabled = immCmdList->isHeaplessModeEnabled();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    auto offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

    void *alloc = allocDeviceMem(16 * MemoryConstants::kiloByte);

    auto findSemaphores = [&](size_t expectedNumSemaphores) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        auto semaphoresDiff = (cmds.size() > expectedNumSemaphores) ? (cmds.size() - expectedNumSemaphores) : (expectedNumSemaphores - cmds.size());
        EXPECT_LE(semaphoresDiff, 1u);
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    uint32_t numSemaphores = immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope())) ? 1 : 2;

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(alloc, alloc, 1, eventHandle, 0, nullptr, copyParams);
    findSemaphores(heaplessEnabled ? 1 : numSemaphores); // implicit dependency + optional chaining

    numSemaphores = immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope())) ? 1 : 0;

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(alloc, alloc, 1, nullptr, 0, nullptr, copyParams);
    findSemaphores(heaplessEnabled ? 1 : numSemaphores); // implicit dependency for Compact event or no semaphores for non-compact

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(alloc, &region, 1, 1, alloc, &region, 1, 1, eventHandle, 0, nullptr, copyParams);
    findSemaphores(heaplessEnabled ? 1 : 2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(alloc, &region, 1, 1, alloc, &region, 1, 1, nullptr, 0, nullptr, copyParams);
    findSemaphores(heaplessEnabled ? 1 : 0); // no implicit dependency

    context->freeMem(alloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCmdsChainingFromAppendCopyAndFlushRequiredWhenDispatchingKernelThenProgramSemaphoreOnce) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(1);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    bool heaplessEnabled = immCmdList->isHeaplessModeEnabled();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto eventHandle = events[0]->toHandle();

    auto offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    uint32_t copyData = 0;

    auto findSemaphores = [&](size_t expectedNumSemaphores) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
        auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        auto semaphoresDiff = (cmds.size() > expectedNumSemaphores) ? (cmds.size() - expectedNumSemaphores) : (expectedNumSemaphores - cmds.size());
        EXPECT_LE(semaphoresDiff, 1u);
    };
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto dcFlushRequired = immCmdList->getDcFlushRequired(true);

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, copyParams);
    auto nSemaphores = heaplessEnabled ? 1 : (dcFlushRequired ? 1 : 2);
    findSemaphores(nSemaphores); // implicit dependency + timestamp chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);
    nSemaphores = heaplessEnabled ? 1 : (dcFlushRequired ? 1 : 0);
    findSemaphores(nSemaphores); // implicit dependency or already waited on previous call

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, eventHandle, 0, nullptr, copyParams);
    findSemaphores(heaplessEnabled ? 1 : 2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, copyParams);
    findSemaphores(heaplessEnabled ? 1 : 0); // no implicit dependency
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenEventWithRequiredPipeControlWhenDispatchingCopyThenSignalInOrderAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    if (immCmdList->inOrderExecInfo->isAtomicDeviceSignalling()) {
        GTEST_SKIP();
    }

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    void *alloc = allocDeviceMem(16 * MemoryConstants::kiloByte);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(alloc, alloc, 1, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

    if (immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isFlushRequiredForSignal()))) {
        EXPECT_NE(cmdList.end(), sdiItor);
    } else {
        EXPECT_EQ(cmdList.end(), sdiItor);

        auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);
        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();

        using PostSyncType = std::decay_t<decltype(postSync)>;

        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(1u, postSync.getImmediateData());
        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
    }

    context->freeMem(alloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenEventWithRequiredPipeControlAndAllocFlushWhenDispatchingCopyThenSignalInOrderAllocation) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    uint32_t copyData = 0;

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, copyParams);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto gpuAddress = inOrderExecInfo->getBaseDeviceAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);
    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();
    EXPECT_EQ(gpuAddress, postSync.getDestinationAddress());
}

HWTEST2_F(InOrderCmdListTests, givenCmdsChainingWhenDispatchingKernelWithRelaxedOrderingThenProgramAllDependencies, IsXeHpcCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();
    size_t offset = 0;

    auto findConditionalBbStarts = [&](size_t expectedNumBbStarts) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());

        EXPECT_EQ(expectedNumBbStarts, cmds.size());
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findConditionalBbStarts(1); // chaining

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    findConditionalBbStarts(1); // implicit dependency
}

HWTEST2_F(InOrderCmdListTests, givenRelaxedOrderingEnabledWhenSignalEventCalledThenPassStallingCmdsInfo, IsXeHpcCore) {

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->recordFlushedBatchBuffer = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    auto verifyFlags = [&ultCsr](bool relaxedOrderingExpected, bool stallingCmdsExpected) {
        EXPECT_EQ(stallingCmdsExpected, ultCsr->recordedImmediateDispatchFlags.hasStallingCmds);
        EXPECT_EQ(stallingCmdsExpected, ultCsr->latestFlushedBatchBuffer.hasStallingCmds);

        EXPECT_EQ(relaxedOrderingExpected, ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    };

    auto immCmdList0 = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams); // NP state init

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(2, true);
    events[1]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    auto nonCbEvent = events[1]->toHandle();

    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = true,
    };
    immCmdList1->appendSignalEvent(events[0]->toHandle(), signalEventParameters);
    verifyFlags(false, false); // no dependencies

    signalEventParameters.relaxedOrderingDispatch = false,
    immCmdList2->appendSignalEvent(events[0]->toHandle(), signalEventParameters);
    verifyFlags(false, false); // no dependencies

    signalEventParameters.relaxedOrderingDispatch = true;
    immCmdList1->appendSignalEvent(events[0]->toHandle(), signalEventParameters);
    verifyFlags(true, false); // relaxed ordering with implicit dependency

    signalEventParameters.relaxedOrderingDispatch = true;
    immCmdList1->appendSignalEvent(nonCbEvent, signalEventParameters);
    verifyFlags(true, true); // relaxed ordering with implicit dependency

    immCmdList1->cmdQImmediate->unregisterCsrClient();
    immCmdList2->cmdQImmediate->unregisterCsrClient();

    signalEventParameters.relaxedOrderingDispatch = false,
    immCmdList1->appendSignalEvent(events[0]->toHandle(), signalEventParameters);
    verifyFlags(false, true); // relaxed ordering disabled == stalling semaphore
}

HWTEST2_F(InOrderCmdListTests, givenCounterHeuristicForRelaxedOrderingEnabledWhenSmallTaskIsFlushedThenIncrementCounter, IsXeHpcCore) {
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto queue = immCmdList->getCmdQImmediate(CopyOffloadModes::disabled);
    EXPECT_EQ(0u, queue->getTaskCount());
    EXPECT_EQ(0u, immCmdList->relaxedOrderingCounter);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(1u, immCmdList->relaxedOrderingCounter);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(2u, immCmdList->relaxedOrderingCounter);

    ultCsr->flushTagUpdate();
    EXPECT_NE(ultCsr->taskCount, queue->getTaskCount());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(3u, immCmdList->relaxedOrderingCounter);
    EXPECT_EQ(ultCsr->taskCount, queue->getTaskCount());
}

HWTEST2_F(InOrderCmdListTests, givenCounterHeuristicForRelaxedOrderingEnabledWhenAppendingThenEnableRelaxedOrderingCorrectly, IsXeHpcCore) {
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->recordFlushedBatchBuffer = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    auto verifyFlags = [&ultCsr](bool relaxedOrderingExpected, auto &cmdList, uint64_t expectedCounter) {
        EXPECT_EQ(expectedCounter, cmdList->relaxedOrderingCounter);
        EXPECT_EQ(relaxedOrderingExpected, ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    };

    auto immCmdList0 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto queue0 = immCmdList0->getCmdQImmediate(CopyOffloadModes::disabled);
    EXPECT_EQ(0u, queue0->getTaskCount());
    EXPECT_EQ(0u, immCmdList0->relaxedOrderingCounter);

    // First queue. Dont enable yet
    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(false, immCmdList0, 1);

    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(false, immCmdList0, 2);

    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(false, immCmdList0, 3);

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto queue1 = immCmdList1->getCmdQImmediate(CopyOffloadModes::disabled);
    EXPECT_EQ(0u, queue1->getTaskCount());
    EXPECT_EQ(0u, immCmdList1->relaxedOrderingCounter);

    // Reset to 0 - new queue
    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(false, immCmdList1, 0); // no dependencies

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(true, immCmdList1, 1);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(true, immCmdList1, 2);

    // Back to queue0. Reset to 0 - new queue
    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(true, immCmdList0, 0);

    EXPECT_TRUE(ultCsr->getDirectSubmissionRelaxedOrderingQueueDepth() > 1);

    for (uint32_t i = 0; i < ultCsr->getDirectSubmissionRelaxedOrderingQueueDepth(); i++) {
        immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        verifyFlags(true, immCmdList0, i + 1);
    }

    // Threshold reached
    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(false, immCmdList0, ultCsr->getDirectSubmissionRelaxedOrderingQueueDepth() + 1);

    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristicThreshold.set(1);

    // Back to queue1. Reset to 0 - new queue
    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(true, immCmdList1, 0);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(true, immCmdList1, 1);

    // Threshold reached
    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    verifyFlags(false, immCmdList1, 2);
}

HWTEST2_F(InOrderCmdListTests, givenCounterHeuristicForRelaxedOrderingEnabledWithFirstDeviceInitSubmissionWhenAppendingThenEnableRelaxedOrderingCorrectly, IsAtLeastXeHpcCore) {
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->recordFlushedBatchBuffer = true;

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useFirstSubmissionInitDevice = true;

    if (!device->getNEODevice()->isInitDeviceWithFirstSubmissionSupported(ultCsr->getType())) {
        GTEST_SKIP();
    }

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    EXPECT_EQ(0u, ultCsr->peekTaskCount());

    ultCsr->initializeDeviceWithFirstSubmission(*device->getNEODevice());
    EXPECT_EQ(1u, ultCsr->peekTaskCount());

    auto immCmdList0 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto queue0 = immCmdList0->getCmdQImmediate(CopyOffloadModes::disabled);
    EXPECT_EQ(0u, queue0->getTaskCount());
    EXPECT_EQ(0u, immCmdList0->relaxedOrderingCounter);

    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(1u, immCmdList0->relaxedOrderingCounter);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderEventModeWhenWaitingForEventFromPreviousAppendThenSkip) {

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), offset),
            cmdStream->getUsed() - offset));

        auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        if (immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get())) {
            EXPECT_EQ(cmdList.end(), itor); // already waited on previous call
        } else {
            ASSERT_NE(cmdList.end(), itor);

            itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

            EXPECT_EQ(cmdList.end(), itor);
        }
    }
}

HWTEST_F(InOrderCmdListTests, givenInOrderEventModeWhenSubmittingFromDifferentCmdListThenProgramSemaphoreForEvent) {
    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;
    ultCsr->residencyContainerDuplicateRemovalRequired = true;
    auto heaplessPrologProgrammed = ultCsr->heaplessPrologProgrammed;

    if (heaplessPrologProgrammed) {
        EXPECT_NE(nullptr, immCmdList1->inOrderExecInfo->getHostCounterAllocation(0));
        EXPECT_NE(nullptr, immCmdList2->inOrderExecInfo->getHostCounterAllocation(0));
    } else {
        EXPECT_EQ(nullptr, immCmdList1->inOrderExecInfo->getHostCounterAllocation(0));
        EXPECT_EQ(nullptr, immCmdList2->inOrderExecInfo->getHostCounterAllocation(0));
    }

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams);

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getDeviceCounterAllocation()]);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams);

    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getDeviceCounterAllocation()]);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    if (NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(immCmdList1->isQwordInOrderCounter(), device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport)) {
        std::advance(itor, -2); // verify 2x LRI before semaphore
    }

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, immCmdList1->inOrderExecInfo->getBaseDeviceAddress(), immCmdList1->isQwordInOrderCounter(), false));

    EXPECT_NE(immCmdList1->inOrderExecInfo->getBaseDeviceAddress(), immCmdList2->inOrderExecInfo->getBaseDeviceAddress());
}

HWTEST_F(InOrderCmdListTests, givenDebugFlagSetWhenDispatchingThenEnsureHostAllocationResidency) {
    NEO::debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto event0Handle = events[0]->toHandle();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    EXPECT_NE(nullptr, immCmdList1->inOrderExecInfo->getHostCounterAllocation(0));
    EXPECT_NE(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList1->inOrderExecInfo->getHostCounterAllocation(0));
    EXPECT_NE(nullptr, immCmdList2->inOrderExecInfo->getHostCounterAllocation(0));
    EXPECT_NE(immCmdList2->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList2->inOrderExecInfo->getHostCounterAllocation(0));

    EXPECT_EQ(AllocationType::timestampPacketTagBuffer, immCmdList1->inOrderExecInfo->getHostCounterAllocation(0)->getAllocationType());
    EXPECT_EQ(immCmdList1->inOrderExecInfo->getBaseHostAddress(0), immCmdList1->inOrderExecInfo->getHostCounterAllocation(0)->getUnderlyingBuffer());
    EXPECT_FALSE(immCmdList1->inOrderExecInfo->getHostCounterAllocation(0)->isAllocatedInLocalMemoryPool());

    EXPECT_EQ(immCmdList1->inOrderExecInfo->getHostCounterAllocation(0), immCmdList2->inOrderExecInfo->getHostCounterAllocation(0));

    auto hostAllocOffset = ptrDiff(immCmdList2->inOrderExecInfo->getBaseHostAddress(0), immCmdList1->inOrderExecInfo->getBaseHostAddress(0));
    EXPECT_NE(0u, hostAllocOffset);

    EXPECT_EQ(AllocationType::timestampPacketTagBuffer, immCmdList2->inOrderExecInfo->getHostCounterAllocation(0)->getAllocationType());
    EXPECT_EQ(immCmdList2->inOrderExecInfo->getBaseHostAddress(0), ptrOffset(immCmdList2->inOrderExecInfo->getHostCounterAllocation(0)->getUnderlyingBuffer(), hostAllocOffset));
    EXPECT_FALSE(immCmdList2->inOrderExecInfo->getHostCounterAllocation(0)->isAllocatedInLocalMemoryPool());

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams);

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getHostCounterAllocation(0)]);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams);

    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getHostCounterAllocation(0)]);
}

HWTEST_F(InOrderCmdListTests, givenInOrderEventModeWhenSubmittingThenClearEventCsrList) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    UltCommandStreamReceiver<FamilyType> tempCsr(*device->getNEODevice()->getExecutionEnvironment(), 0, 1);

    auto eventPool = createEvents<FamilyType>(1, false);

    events[0]->csrs.clear();
    events[0]->csrs.push_back(&tempCsr);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[0]->csrs[0]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenDispatchingThenHandleDependencyCounter) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    EXPECT_NE(nullptr, immCmdList->inOrderExecInfo.get());
    EXPECT_EQ(AllocationType::gpuTimestampDeviceBuffer, immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getAllocationType());

    EXPECT_EQ(0u, immCmdList->inOrderExecInfo->getCounterValue());

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;
    ultCsr->residencyContainerDuplicateRemovalRequired = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList->inOrderExecInfo->getDeviceCounterAllocation()]);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[immCmdList->inOrderExecInfo->getDeviceCounterAllocation()]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenMultipleCsrClientsWhenDispatchingInOrderThenDontSkipWalkerPostSync) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableWalkerPostSyncSkip.set(1);

    auto singleClientCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    ASSERT_TRUE(singleClientCmdList->isWalkerPostSyncSkipEnabled);

    singleClientCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, singleClientCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_TRUE(singleClientCmdList->isInOrderCounterSignalPending());

    auto multiClientCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    ASSERT_TRUE(multiClientCmdList->isWalkerPostSyncSkipEnabled);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(multiClientCmdList->getCsr(false));
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);
    ASSERT_GE(ultCsr->getNumClients(), 2u);

    multiClientCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, multiClientCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_FALSE(multiClientCmdList->isInOrderCounterSignalPending());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenAddingRelaxedOrderingEventsThenConfigureRegistersFirst) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = true,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    immCmdList->addEventsToCmdList(0, nullptr, waitEventsParameters);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto lrrCmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_REG *>(*cmdList.begin());
    ASSERT_NE(nullptr, lrrCmd);

    EXPECT_EQ(RegisterOffsets::csGprR4, lrrCmd->getSourceRegisterAddress());
    EXPECT_EQ(RegisterOffsets::csGprR0, lrrCmd->getDestinationRegisterAddress());
    lrrCmd++;
    EXPECT_EQ(RegisterOffsets::csGprR4 + 4, lrrCmd->getSourceRegisterAddress());
    EXPECT_EQ(RegisterOffsets::csGprR0 + 4, lrrCmd->getDestinationRegisterAddress());
}

HWTEST2_F(InOrderCmdListTests, givenCbEventWithProfilingWhenAppendNeedsRelaxedOrderingThenSubmitInOrderCounter, IsXeHpcCore) {
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList->latestOperationHasCbEventWithProfiling = true;

    auto value = immCmdList->inOrderExecInfo->getCounterValue();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getCounterValue(), value + 1 + !immCmdList->isHeaplessModeEnabled());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingWalkerThenSignalSyncAllocation) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    bool isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isFlushRequiredForSignal()));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

        auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();

        using PostSyncType = std::decay_t<decltype(postSync)>;
        if (immCmdList->isWalkerPostSyncSkipEnabled) {
            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_NO_WRITE, postSync.getOperation());
        } else if (!immCmdList->inOrderExecInfo->isAtomicDeviceSignalling()) {
            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(1u, postSync.getImmediateData());
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, postSync.getDestinationAddress());
        }
    }

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();
        using PostSyncType = std::decay_t<decltype(postSync)>;

        if (isCompactEvent) {
            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_NO_WRITE, postSync.getOperation());

            auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
            ASSERT_NE(cmdList.end(), pcItor);
            auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);

            uint64_t address = pcCmd->getAddress();
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, address);
            EXPECT_EQ(2u, pcCmd->getImmediateData());

            const auto &hwInfo = device->getNEODevice()->getHardwareInfo();
            const bool textureFlushRequired = hwInfo.caps.postImageWriteFlushRequired &&
                                              kernel->getImmutableData()->getKernelInfo()->kernelDescriptor.kernelAttributes.hasImageWriteArg;
            EXPECT_EQ(textureFlushRequired, pcCmd->getTextureCacheInvalidationEnable());
        } else {
            if (!immCmdList->inOrderExecInfo->isAtomicDeviceSignalling()) {
                EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
                EXPECT_EQ(2u, postSync.getImmediateData());
                EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, postSync.getDestinationAddress());
            }
        }
    }

    uint64_t *hostAddress = nullptr;
    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = ptrOffset(immCmdList->inOrderExecInfo->getBaseHostAddress(0), counterOffset);
    } else {
        hostAddress = static_cast<uint64_t *>(ptrOffset(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer(), counterOffset));
    }

    const uint64_t expectedEventValue = 2u;

    *hostAddress = expectedEventValue - 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(1));

    *hostAddress = expectedEventValue;
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));

    *hostAddress = expectedEventValue + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingTimestampEventThenClearAndChainWithSyncAllocSignaling) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    if (immCmdList->inOrderExecInfo->isAtomicDeviceSignalling()) {
        GTEST_SKIP();
    }
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    if (immCmdList->isHeaplessModeEnabled()) {
        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), sdiItor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(events[0]->getCompletionFieldGpuAddress(device), sdiCmd->getAddress());
        EXPECT_EQ(0u, sdiCmd->getStoreQword());
        EXPECT_EQ(Event::STATE_CLEARED, sdiCmd->getDataDword0());
    }

    auto eventBaseGpuVa = events[0]->getPacketAddress(device);

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);
    auto walker = genCmdCast<WalkerType *>(*walkerItor);

    auto &postSync = walker->getPostSync();
    using PostSyncType = std::decay_t<decltype(postSync)>;

    EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
    EXPECT_EQ(eventBaseGpuVa, postSync.getDestinationAddress());
    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++walker);
    ASSERT_EQ(nullptr, semaphoreCmd);
    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(walker);
    ASSERT_EQ(nullptr, sdiCmd);
}

HWTEST2_F(InOrderCmdListTests, givenIoqAndPrefetchEnabledWhenKernelIsAppendedThenPrefetchIsBeforeIt, IsAtLeastXeHpcCore) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    NEO::debugManager.flags.EnableMemoryPrefetch.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto gmmHelper = device->getNEODevice()->getGmmHelper();

    auto isaAllocation = kernel->getIsaAllocation();
    auto isaAddress = gmmHelper->decanonize(isaAllocation->getGpuAddress()) + kernel->getIsaOffsetInParentAllocation();
    auto heap = immCmdList->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject);
    auto heapAddress = gmmHelper->decanonize(heap->getGraphicsAllocation()->getGpuAddress()) + heap->getUsed();

    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
    auto isaPrefetchSize = device->getProductHelper().getIsaPrefetchSize(kernel->getImmutableData()->getIsaSizeWithoutPadding());
    auto iohAdditionalPrefetchCount = NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForAdditionalMemoryPrefetch(*heap->getGraphicsAllocation(), kernel->getIndirectSize(), heap->getUsed(), rootDeviceEnvironment) / sizeof(STATE_PREFETCH);
    auto isaAdditionalPrefetchCount = NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForAdditionalMemoryPrefetch(*isaAllocation, isaPrefetchSize, kernel->getIsaOffsetInParentAllocation(), rootDeviceEnvironment) / sizeof(STATE_PREFETCH);

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto firstPrefetchIterator = find<STATE_PREFETCH *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), firstPrefetchIterator);

    for (size_t i = 0; i < iohAdditionalPrefetchCount; i++, firstPrefetchIterator++) {
        auto iohAdditionalPrefetch = genCmdCast<STATE_PREFETCH *>(*firstPrefetchIterator);
        ASSERT_NE(nullptr, iohAdditionalPrefetch);
        EXPECT_EQ(alignDown(heapAddress, MemoryConstants::pageSize), iohAdditionalPrefetch->getAddress());
        EXPECT_FALSE(iohAdditionalPrefetch->getKernelInstructionPrefetch());
    }

    auto heapSize = alignUp(kernel->getIndirectSize(), MemoryConstants::cacheLineSize) / MemoryConstants::cacheLineSize;

    auto firstPrefetch = genCmdCast<STATE_PREFETCH *>(*firstPrefetchIterator);
    ASSERT_NE(nullptr, firstPrefetch);
    EXPECT_EQ(heapAddress, firstPrefetch->getAddress());
    EXPECT_EQ(heapSize, firstPrefetch->getPrefetchSize());

    EXPECT_FALSE(firstPrefetch->getKernelInstructionPrefetch());

    auto secondPrefetchIterator = find<STATE_PREFETCH *>(++firstPrefetchIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), secondPrefetchIterator);

    for (size_t i = 0; i < isaAdditionalPrefetchCount; i++, secondPrefetchIterator++) {
        auto isaAdditionalPrefetch = genCmdCast<STATE_PREFETCH *>(*secondPrefetchIterator);
        ASSERT_NE(nullptr, isaAdditionalPrefetch);
        EXPECT_EQ(alignDown(isaAddress, MemoryConstants::pageSize), isaAdditionalPrefetch->getAddress());
        EXPECT_FALSE(isaAdditionalPrefetch->getKernelInstructionPrefetch());
    }

    auto secondPrefetch = genCmdCast<STATE_PREFETCH *>(*secondPrefetchIterator);
    ASSERT_NE(nullptr, secondPrefetch);

    EXPECT_TRUE(secondPrefetch->getKernelInstructionPrefetch());
    EXPECT_EQ(isaAddress, secondPrefetch->getAddress());
}

HWTEST_F(InOrderCmdListTests, givenCbEventWhenAppendSignalEventCalledThenFallbackToAppendBarrier) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                  CmdListWaitEventParameters &waitEventsParameters, CmdListSignalEventParameters &signalEventParameters) override {
            appendBarrierCalled++;
            return BaseClass::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents, waitEventsParameters, signalEventParameters);
        }

        uint32_t appendBarrierCalled = 0;
    };

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyMockCmdList>(false);

    auto eventPool = createEvents<FamilyType>(2, true);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList->appendSignalEvent(events[0]->toHandle(), signalEventParameters);
    EXPECT_EQ(0u, immCmdList->appendBarrierCalled);

    auto initialTaskCount = immCmdList->cmdQImmediate->getTaskCount();

    signalEventParameters.relaxedOrderingDispatch = false;
    immCmdList->appendSignalEvent(events[1]->toHandle(), signalEventParameters);
    EXPECT_EQ(1u, immCmdList->appendBarrierCalled);
    EXPECT_EQ(initialTaskCount + 1, immCmdList->cmdQImmediate->getTaskCount());
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenAppendSignalThenDoNotSkipInOrderBarrier) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    uint32_t incValue = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetAggregatedCopyOffloadIncrementValue(device->toHandle(), &incValue));
    EXPECT_GT(incValue, 0u);

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto event = createAggregatedEvent(incValue * 2, incValue, devAddress);
    auto &inOrderExecHelper = event->getInOrderExecEventHelper();
    auto *eventData = inOrderExecHelper.getEventData();

    EXPECT_TRUE(Event::isAggregatedEvent(event.get()));
    EXPECT_EQ(inOrderExecHelper.getIncrementValue(), incValue);
    EXPECT_EQ(eventData->counterValue, incValue * 2);
    EXPECT_FALSE(cmdList->isSkippingInOrderBarrierAllowed(event->toHandle(), 0, nullptr, false));

    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    cmdList->appendSignalEvent(event->toHandle(), signalEventParameters);

    EXPECT_EQ(inOrderExecHelper.getIncrementValue(), incValue);
    EXPECT_EQ(eventData->counterValue, incValue * 2);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenAppendSignalOnImmediateCmdListThenDoNotSkipInOrderBarrier) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    uint32_t incValue = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetAggregatedCopyOffloadIncrementValue(device->toHandle(), &incValue));
    EXPECT_GT(incValue, 0u);

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto event = createAggregatedEvent(incValue * 2, incValue, devAddress);
    auto &inOrderExecHelper = event->getInOrderExecEventHelper();
    auto *eventData = inOrderExecHelper.getEventData();

    EXPECT_TRUE(Event::isAggregatedEvent(event.get()));
    EXPECT_EQ(inOrderExecHelper.getIncrementValue(), incValue);
    EXPECT_EQ(eventData->counterValue, incValue * 2);
    EXPECT_FALSE(immCmdList->isSkippingInOrderBarrierAllowed(event->toHandle(), 0, nullptr, false));
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList->appendSignalEvent(event->toHandle(), signalEventParameters);

    EXPECT_EQ(inOrderExecHelper.getIncrementValue(), incValue);
    EXPECT_EQ(eventData->counterValue, incValue * 2);

    context->freeMem(devAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingRegularEventThenClearAndChainWithSyncAllocSignaling) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->signalScope = 0;
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    if (immCmdList->inOrderExecInfo->isAtomicDeviceSignalling()) {
        GTEST_SKIP();
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(events[0]->getCompletionFieldGpuAddress(device), sdiCmd->getAddress());
    EXPECT_EQ(0u, sdiCmd->getStoreQword());
    EXPECT_EQ(Event::STATE_CLEARED, sdiCmd->getDataDword0());

    auto walkerItor = find<WalkerType *>(sdiItor, cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto eventBaseGpuVa = events[0]->getPacketAddress(device);
    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();
    using PostSyncType = std::decay_t<decltype(postSync)>;

    EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
    EXPECT_EQ(eventBaseGpuVa, postSync.getDestinationAddress());

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++walker);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semaphoreCmd));
    EXPECT_EQ(eventEndGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd));
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());

    sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
}

HWTEST_F(InOrderCmdListTests, givenHostVisibleEventOnLatestFlushWhenCallingSynchronizeThenUseInOrderSync) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    EXPECT_FALSE(immCmdList->latestFlushIsHostVisible);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(immCmdList->dcFlushSupport ? false : true, immCmdList->latestFlushIsHostVisible);

    EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    immCmdList->hostSynchronize(0, false);

    if (immCmdList->dcFlushSupport || immCmdList->latestOperationHasCbEventWithProfiling) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    auto currSyncInOrderCalls = immCmdList->synchronizeInOrderExecutionCalled;
    immCmdList->hostSynchronize(0, false);
    EXPECT_EQ(currSyncInOrderCalls, immCmdList->synchronizeInOrderExecutionCalled);

    events[0]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(!immCmdList->dcFlushSupport || immCmdList->isHeaplessModeEnabled(), immCmdList->latestFlushIsHostVisible);

    immCmdList->hostSynchronize(0, false);

    if (!immCmdList->latestFlushIsHostVisible || immCmdList->latestOperationHasCbEventWithProfiling) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(3u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(2u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    // handle post sync operations
    immCmdList->hostSynchronize(0, true);

    if (!immCmdList->latestFlushIsHostVisible || immCmdList->latestOperationHasCbEventWithProfiling) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(4u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(3u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(2u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }
}

HWTEST_F(InOrderCmdListTests, givenPendingInOrderCounterSignalWhenCallingSynchronizeThenUseTaskCountWait) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    immCmdList->latestFlushIsHostVisible = true;
    immCmdList->inOrderExecInfo->addCounterValue(immCmdList->getInOrderIncrementValue());
    ASSERT_TRUE(immCmdList->isInOrderCounterSignalPending());

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
}

HWTEST_F(InOrderCmdListTests, givenEmptyTempAllocationsStorageWhenCallingSynchronizeThenUseInternalCounter) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(!immCmdList->dcFlushSupport || immCmdList->isHeaplessModeEnabled(), immCmdList->latestFlushIsHostVisible);
    EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    immCmdList->hostSynchronize(0, true);

    if (!immCmdList->latestFlushIsHostVisible || immCmdList->latestOperationHasCbEventWithProfiling) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    immCmdList->hostSynchronize(0, true);

    if (!immCmdList->latestFlushIsHostVisible || immCmdList->latestOperationHasCbEventWithProfiling) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }
}

HWTEST_F(InOrderCmdListTests, givenGpuHangWhenCallingSynchronizeThenDontMarkCounterValueAsWaited) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    // host visible flush makes the in order counter wait eligible
    immCmdList->dcFlushSupport = false;

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    ASSERT_TRUE(immCmdList->latestFlushIsHostVisible);
    const bool inOrderWaitPath = !immCmdList->isInOrderCounterSignalPending();

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;

    auto *hostAddress = ptrOffset(inOrderExecInfo->getBaseHostAddress(0), inOrderExecInfo->getAllocationOffset());
    for (uint32_t i = 0; i < inOrderExecInfo->getNumHostPartitionsToWait(); i++) {
        *hostAddress = 0;
        hostAddress = ptrOffset(hostAddress, device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
    }

    ultCsr->forceReturnGpuHang = true;
    ultCsr->callBaseWaitForCompletionWithTimeout = false;
    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, immCmdList->hostSynchronize(10, false));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset()));

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, immCmdList->hostSynchronize(10, false));

    // a memoized counter value would make this call skip the wait and report success
    if (inOrderWaitPath) {
        EXPECT_EQ(2u, immCmdList->synchronizeInOrderExecutionCalled);
    } else {
        EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    ultCsr->forceReturnGpuHang = false;
    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;

    // counter is not memoized as waited, so let the cmd list destructor's in order wait complete
    hostAddress = ptrOffset(inOrderExecInfo->getBaseHostAddress(0), inOrderExecInfo->getAllocationOffset());
    for (uint32_t i = 0; i < inOrderExecInfo->getNumHostPartitionsToWait(); i++) {
        *hostAddress = std::numeric_limits<uint64_t>::max();
        hostAddress = ptrOffset(hostAddress, device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, InOrderCmdListTests, givenNonPostSyncWalkerWhenAskingForNonWalkerSignalingRequiredThenReturnFalse) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool1 = createEvents<FamilyType>(1, true);
    auto eventPool2 = createEvents<FamilyType>(1, false);
    auto eventPool3 = createEvents<FamilyType>(1, false);
    events[2]->makeCounterBasedInitiallyDisabled(eventPool3->getAllocation());

    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[2].get()));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenMultipleAllocationsForWriteWhenAskingForNonWalkerSignalingRequiredThenReturnTrue) {

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool0 = createEvents<FamilyType>(1, true);
    auto eventPool1 = createEvents<FamilyType>(1, false);
    auto eventPool2 = createEvents<FamilyType>(1, false);
    events[2]->makeCounterBasedInitiallyDisabled(eventPool2->getAllocation());

    bool isCompactEvent0 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isFlushRequiredForSignal()));
    bool isCompactEvent1 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[1]->isFlushRequiredForSignal()));
    bool isCompactEvent2 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[2]->isFlushRequiredForSignal()));

    EXPECT_TRUE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_EQ(isCompactEvent1, immCmdList->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_TRUE(immCmdList->isInOrderNonWalkerSignalingRequired(events[2].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(nullptr));

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    const bool heaplessWithDcFlush = immCmdList2->isHeaplessModeEnabled() && immCmdList2->dcFlushSupport;
    EXPECT_EQ(isCompactEvent0 || heaplessWithDcFlush, immCmdList2->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_EQ(isCompactEvent1, immCmdList2->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_EQ(isCompactEvent2, immCmdList2->isInOrderNonWalkerSignalingRequired(events[2].get()));
    EXPECT_FALSE(immCmdList2->isInOrderNonWalkerSignalingRequired(nullptr));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenDuplicatedCounterStorageAndHeaplessWithDcFlushWhenAskingForNonWalkerSignalingRequiredForCounterBasedProfilingEventThenReturnTrue) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->heaplessModeEnabled = true;
    immCmdList->dcFlushSupport = true;
    immCmdList->compactL3FlushEventPacket = false;

    auto profilingEventPool = createEvents<FamilyType>(1, true);
    auto counterEventPool = createEvents<FamilyType>(1, false);

    ASSERT_TRUE(immCmdList->duplicatedInOrderCounterStorageEnabled);

    EXPECT_TRUE(events[0]->isCounterBased());
    EXPECT_TRUE(events[0]->isEventTimestampFlagSet());
    EXPECT_TRUE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));

    EXPECT_TRUE(events[1]->isCounterBased());
    EXPECT_FALSE(events[1]->isEventTimestampFlagSet());
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[1].get()));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenDuplicatedCounterStorageAndHeaplessWithoutDcFlushWhenAskingForNonWalkerSignalingRequiredForCounterBasedProfilingEventThenReturnFalse) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->heaplessModeEnabled = true;
    immCmdList->dcFlushSupport = false;
    immCmdList->compactL3FlushEventPacket = false;

    auto profilingEventPool = createEvents<FamilyType>(1, true);

    ASSERT_TRUE(immCmdList->duplicatedInOrderCounterStorageEnabled);

    EXPECT_TRUE(events[0]->isCounterBased());
    EXPECT_TRUE(events[0]->isEventTimestampFlagSet());
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenDuplicatedCounterStorageAndHeapfulWithDcFlushWhenAskingForNonWalkerSignalingRequiredForCounterBasedProfilingEventThenReturnFalse) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->heaplessModeEnabled = false;
    immCmdList->dcFlushSupport = true;
    immCmdList->compactL3FlushEventPacket = false;

    auto profilingEventPool = createEvents<FamilyType>(1, true);

    ASSERT_TRUE(immCmdList->duplicatedInOrderCounterStorageEnabled);

    EXPECT_TRUE(events[0]->isCounterBased());
    EXPECT_TRUE(events[0]->isEventTimestampFlagSet());
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
}

HWTEST_F(InOrderCmdListTests, givenSignalAllPacketsSetWhenProgrammingRemainingPacketsThenSkip) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->maxPacketCount = 2;
    events[0]->setPacketsInUse(1);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    immCmdList->dispatchEventRemainingPacketsPostSyncOperation(events[0].get(), false);
    immCmdList->dispatchEventRemainingPacketsPostSyncOperation(events[0].get(), true);

    EXPECT_EQ(offset, cmdStream->getUsed());

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    offset = cmdStream->getUsed();

    immCmdList->dispatchEventRemainingPacketsPostSyncOperation(events[0].get(), false);
    immCmdList->dispatchEventRemainingPacketsPostSyncOperation(events[0].get(), true);

    EXPECT_NE(offset, cmdStream->getUsed());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingWalkerThenProgramPipeControlWithSignalAllocation) {
    using WALKER = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(64);
    immCmdList->inOrderExecInfo->addCounterValue(123);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_EQ(immCmdList->getDcFlushRequired(true), pcCmd->getDcFlushEnable());

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pcCmd->getPostSyncOperation());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t expectedAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress() + immCmdList->inOrderExecInfo->getAllocationOffset();

    EXPECT_EQ(expectedAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(immCmdList->inOrderExecInfo->getCounterValue(), sdiCmd->getDataDword0());

    auto pipeControls = findAll<PIPE_CONTROL *>(walkerItor, sdiItor);
    EXPECT_EQ(1u, pipeControls.size());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingWalkerWithEventThenDontProgramPipeControl) {
    using WALKER = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    uint64_t expectedAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress() + immCmdList->inOrderExecInfo->getAllocationOffset();

    while (expectedAddress != sdiCmd->getAddress()) {
        sdiItor = find<MI_STORE_DATA_IMM *>(++sdiItor, cmdList.end());
        if (sdiItor == cmdList.end()) {
            ASSERT_TRUE(false);
        }
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    }

    auto pipeControls = findAll<PIPE_CONTROL *>(walkerItor, sdiItor);
    EXPECT_EQ(1u, pipeControls.size());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitThenProgramPcAndSignalAlloc) {
    using WALKER = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(64);
    immCmdList->inOrderExecInfo->addCounterValue(123);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 256;
    const size_t offset = 1;

    void *hostAlloc = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, ptrBaseSize, MemoryConstants::cacheLineSize, &hostAlloc);

    ASSERT_NE(nullptr, hostAlloc);

    auto unalignedPtr = ptrOffset(hostAlloc, offset);
    CmdListMemoryCopyParams copyParams = {};
    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto lastWalkerItor = reverseFind<WALKER *>(cmdList.rbegin(), cmdList.rend());
    ASSERT_NE(cmdList.rend(), lastWalkerItor);

    auto pcItor = reverseFind<PIPE_CONTROL *>(cmdList.rbegin(), lastWalkerItor);
    ASSERT_NE(lastWalkerItor, pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pcCmd->getPostSyncOperation());

    auto sdiItor = reverseFind<MI_STORE_DATA_IMM *>(cmdList.rbegin(), pcItor);
    ASSERT_NE(pcItor, sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t expectedAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress() + immCmdList->inOrderExecInfo->getAllocationOffset();

    EXPECT_EQ(expectedAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(immCmdList->inOrderExecInfo->getCounterValue(), sdiCmd->getDataDword0());

    context->freeMem(hostAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendSignalEventThenSignalSyncAllocation) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    const bool counterSignalPending = immCmdList->isInOrderCounterSignalPending();

    auto offset = cmdStream->getUsed();
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList->appendSignalEvent(events[0]->toHandle(), signalEventParameters);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto inOrderSyncVa = inOrderExecInfo->getBaseDeviceAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = cmdList.begin();
    const uint64_t expectedDependencyValue = 1u;
    if (counterSignalPending) {
        ASSERT_NE(nullptr, genCmdCast<typename FamilyType::StallingBarrierType *>(*itor));
    } else {
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, expectedDependencyValue, inOrderSyncVa, immCmdList->isQwordInOrderCounter(), false));
    }

    {
        const uint64_t expectedSignalValue = 2u;

        auto pcCmd = findInOrderCounterSignalPipeControl<FamilyType>(cmdList, inOrderSyncVa);
        ASSERT_NE(nullptr, pcCmd);

        EXPECT_EQ(expectedSignalValue, pcCmd->getImmediateData());
        EXPECT_EQ(immCmdList->getDcFlushRequired(events[0]->isSignalScope()), pcCmd->getDcFlushEnable());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingNonKernelAppendThenWaitForDependencyAndSignalSyncAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t inOrderSyncVa = inOrderExecInfo->getBaseDeviceAddress();
    uint64_t sdiSyncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    uint8_t ptr[64] = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    const bool counterSignalPending = immCmdList->isInOrderCounterSignalPending();

    uint32_t inOrderCounter = 1u;

    auto verifySdi = [&sdiSyncVa, &immCmdList](GenCmdList::reverse_iterator rIterator, GenCmdList::reverse_iterator rEnd, uint64_t signalValue) {
        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*rIterator);
        while (sdiCmd == nullptr) {
            sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++rIterator));
            if (rIterator == rEnd) {
                break;
            }
        }

        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(sdiSyncVa, sdiCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(getLowPart(signalValue), sdiCmd->getDataDword0());
        EXPECT_EQ(getHighPart(signalValue), sdiCmd->getDataDword1());
    };

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendEventReset(events[0]->toHandle());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        if (counterSignalPending) {
            ASSERT_NE(nullptr, genCmdCast<typename FamilyType::StallingBarrierType *>(*itor));
        } else {
            ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, immCmdList->isQwordInOrderCounter(), false));
        }

        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();

        size_t rangeSizes = 1;
        const void **ranges = reinterpret_cast<const void **>(&ptr[0]);
        CmdListWaitEventParameters waitEventsParameters = {
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };
        immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr, waitEventsParameters);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, immCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();
        CmdListWaitEventParameters waitEventsParameters = {
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };
        immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(ptr), nullptr, 0, nullptr, waitEventsParameters);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, immCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }
}

HWTEST_F(InOrderCmdListTests, givenInOrderRegularCmdListWhenProgrammingAppendWithSignalEventThenAssignInOrderInfo) {
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(2, false);

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());

    uint32_t copyData = 0;
    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, events[1]->toHandle(), 0, nullptr, copyParams);

    EXPECT_TRUE(events[1]->getInOrderExecEventHelper().isDataAssigned());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderRegularCmdListWhenProgrammingNonKernelAppendThenWaitForDependencyAndSignalSyncAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    uint8_t ptr[64] = {};

    uint64_t inOrderSyncVa = regularCmdList->inOrderExecInfo->getBaseDeviceAddress();
    uint64_t sdiSyncVa = regularCmdList->inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(regularCmdList->inOrderExecInfo->getBaseHostAddress(0)) : regularCmdList->inOrderExecInfo->getBaseDeviceAddress();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    const bool counterSignalPending = regularCmdList->isInOrderCounterSignalPending();

    auto verifySdi = [&sdiSyncVa, &regularCmdList](GenCmdList::reverse_iterator rIterator, GenCmdList::reverse_iterator rEnd, uint64_t signalValue) {
        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*rIterator);
        while (sdiCmd == nullptr) {
            sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++rIterator));
            if (rIterator == rEnd) {
                break;
            }
        }

        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(sdiSyncVa, sdiCmd->getAddress());
        EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(getLowPart(signalValue), sdiCmd->getDataDword0());
        EXPECT_EQ(getHighPart(signalValue), sdiCmd->getDataDword1());
    };

    uint64_t inOrderCounter = 1u;

    {
        auto offset = cmdStream->getUsed();

        regularCmdList->appendEventReset(events[0]->toHandle());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        if (counterSignalPending) {
            auto stallingBarrier = genCmdCast<typename FamilyType::StallingBarrierType *>(*itor);
            auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itor);
            ASSERT_TRUE(stallingBarrier != nullptr || pipeControl != nullptr);
            if (pipeControl != nullptr) {
                EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pipeControl->getPostSyncOperation());
            }
        } else {
            ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        }
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();

        size_t rangeSizes = 1;
        const void **ranges = reinterpret_cast<const void **>(&ptr[0]);
        CmdListWaitEventParameters waitEventsParameters = {
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };
        regularCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr, waitEventsParameters);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();
        CmdListWaitEventParameters waitEventsParameters = {
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };
        regularCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(ptr), nullptr, 0, nullptr, waitEventsParameters);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();

        zex_wait_on_mem_desc_t desc;
        desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
        regularCmdList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, 1, nullptr, false);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();

        zex_write_to_mem_desc_t desc = {};
        uint64_t data = 0xabc;
        regularCmdList->appendWriteToMemory(reinterpret_cast<void *>(&desc), ptr, data);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }
}

HWTEST_F(InOrderCmdListTests, givenEventGeneratedByRegularCmdListWhenWaitingFromImmediateThenDontUseSubmissionCounter) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto regularCmdListHandle = regularCmdList->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    uint64_t expectedCounterValue = regularCmdList->inOrderExecInfo->getCounterValue();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    regularCmdList->close();

    auto verifySemaphore = [&](uint64_t expectedValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        const bool lriRequired = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(immCmdList->isQwordInOrderCounter(), immCmdList->getDevice()->getDeviceInfo().semaphore64bCmdSupport);

        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, semaphoreCmd);

        if (NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd) == immCmdList->inOrderExecInfo->getBaseDeviceAddress()) {
            // skip implicit dependency
            semaphoreItor++;
        } else if (lriRequired) {
            std::advance(semaphoreItor, -2); // verify 2x LRI before semaphore
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreItor, expectedValue, regularCmdList->inOrderExecInfo->getBaseDeviceAddress(), immCmdList->isQwordInOrderCounter(), false));
    };

    // 0 Execute calls
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);

    // 1 Execute call
    offset = cmdStream->getUsed();
    CommandListExecutionInternalOptions internalOptions = {};
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, internalOptions);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);

    // 2 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, internalOptions);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);

    // 3 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, internalOptions);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitThenDontSignalFromWalker) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = reinterpret_cast<void *>(0x100000);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

    uint32_t walkersFound = 0;
    while (cmdList.end() != walkerItor) {
        walkersFound++;

        auto walker = genCmdCast<WalkerType *>(*walkerItor);
        auto &postSync = walker->getPostSync();
        EXPECT_EQ(PostSyncType::OPERATION_NO_WRITE, postSync.getOperation());

        walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(++walkerItor, cmdList.end());
    }

    EXPECT_TRUE(walkersFound > 1);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingCopyThenSignalInOrderAllocation) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = false;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;

    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), copyItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(copyItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingComputeCopyThenDontSingalFromSdi) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    void *alloc = allocDeviceMem(16 * MemoryConstants::kiloByte);

    immCmdList->appendMemoryCopy(alloc, alloc, 1, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();

    if (immCmdList->isWalkerPostSyncSkipEnabled) {
        EXPECT_EQ(0u, postSync.getDestinationAddress());
    } else {
        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
    }

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    context->freeMem(alloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenWalkerPostSyncSkipEnabledWhenAppendKernelWithEventThenSignalWalker) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    debugManager.flags.EnableWalkerPostSyncSkip.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();
    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenWalkerPostSyncSkipEnabledWhenAppendKernelWithoutEventThenDontSignalWalker) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    debugManager.flags.EnableWalkerPostSyncSkip.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();
    EXPECT_EQ(0u, postSync.getDestinationAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenAllocFlushRequiredWhenProgrammingComputeCopyThenSignalFromSdi) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto alignedPtr = reinterpret_cast<void *>(0x100000);

    immCmdList->appendMemoryCopy(alignedPtr, alignedPtr, 1, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();

    if (immCmdList->isWalkerPostSyncSkipEnabled) {
        EXPECT_EQ(0u, postSync.getDestinationAddress());
    } else {
        EXPECT_NE(0u, postSync.getDestinationAddress());
    }

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto gpuAddress = inOrderExecInfo->getBaseDeviceAddress();

    auto miAtomicItor = find<MI_ATOMIC *>(walkerItor, cmdList.end());
    if (device->getProductHelper().isDcFlushAllowed() && inOrderExecInfo->isHostStorageDuplicated() && !immCmdList->isWalkerPostSyncSkipEnabled) {
        EXPECT_NE(cmdList.end(), miAtomicItor);
        auto miAtomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomicItor);
        EXPECT_EQ(gpuAddress, miAtomicCmd->getMemoryAddress());
    } else {
        EXPECT_EQ(cmdList.end(), miAtomicItor);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingFillThenSignalInOrderAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = false;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, copyParams);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto fillItor = findBltFillCmd<FamilyType>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), fillItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(fillItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithSplitAndOutProfilingEventThenSignalInOrderAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, (size / 2) + 1, events[0]->toHandle(), 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), walkerItor);

    auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithSplitAndWithoutOutEventThenAddPipeControlSignalInOrderAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, (size / 2) + 1, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), walkerItor);

    auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithoutSplitThenSignalByWalker) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocDeviceMem(size);

    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();
    using PostSyncType = std::decay_t<decltype(postSync)>;
    if (immCmdList->isWalkerPostSyncSkipEnabled) {
        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_NO_WRITE, postSync.getOperation());
    } else if (!immCmdList->inOrderAtomicSignalingEnabled) {
        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(1u, postSync.getImmediateData());
        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
    }

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    context->freeMem(data);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithoutSplitAndWithEventThenDoNotSignalByWalker) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->dcFlushSupport = true;
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocDeviceMem(size);
    auto eventPool = createEvents<FamilyType>(1, true);
    immCmdList->appendMemoryFill(data, data, 1, size, events[0]->toHandle(), 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();
    using PostSyncType = std::decay_t<decltype(postSync)>;

    if (!immCmdList->inOrderAtomicSignalingEnabled) {
        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
    }
    EXPECT_NE(0u, postSync.getDestinationAddress());
    context->freeMem(data);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingCopyRegionThenSignalInOrderAllocation) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = false;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, copyParams);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), copyItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(copyItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingImageCopyFromMemoryExtThenSignalInOrderAllocation) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyBlockCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = false;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;
    image->initialize(device, &desc);

    const size_t imageMemSize = desc.width * desc.height * desc.depth * 4u;
    ze_host_mem_alloc_desc_t hostDesc = {};
    void *srcPtr = nullptr;
    context->allocHostMem(&hostDesc, imageMemSize, 1, &srcPtr);

    immCmdList->appendImageCopyFromMemoryExt(image->toHandle(), srcPtr, nullptr, 0, 0, nullptr, 0, nullptr, copyParams);
    auto offset = cmdStream->getUsed();
    immCmdList->appendImageCopyFromMemoryExt(image->toHandle(), srcPtr, nullptr, 0, 0, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), copyItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(copyItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(srcPtr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendWaitOnEventsThenSignalSyncAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    zeCommandListAppendWaitOnEvents(immCmdList->toHandle(), 1, &eventHandle);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    const uint64_t expectedDependencyValue = 2u;

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    // implicit dependency: either semaphore wait on in-order counter or a stalling barrier
    auto itor = cmdList.begin();
    if (genCmdCast<typename FamilyType::StallingBarrierType *>(*itor)) {
        itor++;
    } else {
        const bool lriRequired = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(immCmdList->isQwordInOrderCounter(), immCmdList->getDevice()->getDeviceInfo().semaphore64bCmdSupport);
        if (lriRequired) {
            std::advance(semaphoreItor, -2);
        }
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreItor, expectedDependencyValue, inOrderExecInfo->getBaseDeviceAddress(), immCmdList->isQwordInOrderCounter(), false));
    }

    auto sdiItor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(3u, sdiCmd->getDataDword0());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenRegularInOrderCmdListWhenProgrammingAppendWaitOnEventsThenDontSignalSyncAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto eventHandle = events[0]->toHandle();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    zeCommandListAppendWaitOnEvents(regularCmdList->toHandle(), 1, &eventHandle);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), semaphoreItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = regularCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(3u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingCounterWithOverflowThenHandleOffsetCorrectly) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->addCounterValue(std::numeric_limits<uint32_t>::max() - 1);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    bool useZeroOffset = false;
    uint64_t expectedCounter = 1;
    uint32_t expectedOffset = 0;

    for (uint32_t i = 0; i < 10; i++) {
        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

        if (immCmdList->isQwordInOrderCounter()) {
            expectedCounter += static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) - 1;
        } else {
            expectedCounter = 1;
            expectedOffset = useZeroOffset ? 0 : alignUp(device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset(), MemoryConstants::cacheLineSize * 4);
        }

        EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->getCounterValue());
        EXPECT_EQ(expectedOffset, immCmdList->inOrderExecInfo->getAllocationOffset());

        EXPECT_EQ(expectedCounter, events[0]->getInOrderExecBaseSignalValue());
        EXPECT_EQ(expectedOffset, events[0]->getInOrderAllocationOffset());

        immCmdList->inOrderExecInfo->addCounterValue(std::numeric_limits<uint32_t>::max() - 2);

        useZeroOffset = !useZeroOffset;
    }

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingCounterWithOverflowThenHandleItCorrectly) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->addCounterValue(std::numeric_limits<uint32_t>::max() - 1);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    bool isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isFlushRequiredForSignal()));

    auto eventHandle = events[0]->toHandle();

    uint64_t baseGpuVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), walkerItor);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());

    uint64_t expectedCounter = 1;
    uint32_t offset = 0;

    if (immCmdList->isQwordInOrderCounter()) {
        expectedCounter = std::numeric_limits<uint32_t>::max();

        auto walker = genCmdCast<WalkerType *>(*walkerItor);
        auto &postSync = walker->getPostSync();
        using PostSyncType = std::decay_t<decltype(postSync)>;

        if (isCompactEvent) {

            auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
            ASSERT_NE(cmdList.end(), pcItor);
            auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);

            uint64_t address = pcCmd->getAddress();
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), address);
            EXPECT_EQ(expectedCounter, pcCmd->getImmediateData());

            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_NO_WRITE, postSync.getOperation());
        } else {
            EXPECT_EQ(cmdList.end(), semaphoreItor);

            if (!immCmdList->inOrderAtomicSignalingEnabled) {
                EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
                EXPECT_EQ(expectedCounter, postSync.getImmediateData());
            }
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
        }
    } else {
        offset = alignUp(device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset(), MemoryConstants::cacheLineSize * 4);
        if (isCompactEvent) {
            auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
            ASSERT_NE(cmdList.end(), pcItor);
            auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
            uint64_t address = pcCmd->getAddress();
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), address);
        } else {
            ASSERT_NE(cmdList.end(), semaphoreItor);

            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(std::numeric_limits<uint32_t>::max(), NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semaphoreCmd));
            EXPECT_EQ(baseGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd));

            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
            ASSERT_NE(nullptr, sdiCmd);

            EXPECT_EQ(baseGpuVa + offset, sdiCmd->getAddress());
            EXPECT_EQ(1u, sdiCmd->getDataDword0());
        }
    }

    EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(offset, immCmdList->inOrderExecInfo->getAllocationOffset());

    EXPECT_EQ(expectedCounter, events[0]->getInOrderExecBaseSignalValue());
    EXPECT_EQ(offset, events[0]->getInOrderAllocationOffset());

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingBarrierThenSignalInOrderAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList1 = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    uint32_t copyData = 0;

    immCmdList1->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, copyParams);

    auto offset = cmdStream->getUsed();

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList2->appendBarrier(nullptr, 1, &eventHandle, waitEventsParametersForBarrier, signalEventParameters);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto inOrderExecInfo = immCmdList2->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList2->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingBarrierWithHostVisibleSignalEventThenSignalCounterWithoutPipeControl) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = false;

    // A host scope signal event on a platform requiring DC flush makes the barrier a stalling
    // operation, which is the case that used to program PIPE_CONTROL on a copy engine.
    immCmdList->dcFlushSupport = true;

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();
    ASSERT_TRUE(immCmdList->getDcFlushRequired(events[0]->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST)));

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    // Copy engines do not implement PIPE_CONTROL, so the counter write would never land and
    // any host wait on this command list would never complete.
    EXPECT_TRUE(findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end()).empty());

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    const uint64_t syncVa = inOrderExecInfo->getBaseDeviceAddress() + inOrderExecInfo->getAllocationOffset();
    const uint64_t signalValue = inOrderExecInfo->getCounterValue();

    bool counterSignalled = false;
    for (auto &it : findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end())) {
        auto flushCmd = genCmdCast<MI_FLUSH_DW *>(*it);
        if ((flushCmd->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) &&
            (flushCmd->getDestinationAddress() == syncVa) &&
            (flushCmd->getImmediateData() == signalValue)) {
            counterSignalled = true;
        }
    }
    EXPECT_TRUE(counterSignalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithWaitlistThenSignalSyncAllocation) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList2->appendBarrier(nullptr, 1, &eventHandle, waitEventsParametersForBarrier, signalEventParameters);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto pcItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), pcItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    auto inOrderExecInfo = immCmdList2->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();
    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList2->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST_F(InOrderCmdListTests, givenStandaloneCbEventWhenDispatchingThenProgramCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto event = createStandaloneCbEvent(nullptr);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventHandle = event->toHandle();

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    bool semaphoreFound = false;

    ASSERT_FALSE(semaphores.empty());

    for (auto &semaphore : semaphores) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);

        ASSERT_TRUE(semaphoreCmd != nullptr);
        if (event->getInOrderExecEventHelper().getBaseDeviceAddress() == NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd)) {
            semaphoreFound = true;
        }
    }

    EXPECT_TRUE(semaphoreFound);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAfterKernelWithEventThenInheritSignalSyncAllocation) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto eventPool = createEvents<FamilyType>(2, false);

    events[1]->signalScope = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventHandle = events[1]->toHandle();

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };

    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false};
    immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);
    EXPECT_EQ(nullptr, events[1]->getLatestUsedCmdQueue());
    immCmdList->latestOperationHasCbEventWithProfiling = true;
    EXPECT_FALSE(events[1]->cbEventWithProfiling);
    immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);
    EXPECT_EQ(immCmdList->cmdQImmediate, events[1]->getLatestUsedCmdQueue());
    EXPECT_FALSE(events[1]->cbEventWithProfiling);

    EXPECT_LT(offset, cmdStream->getUsed());

    EXPECT_EQ(2u, events[1]->getInOrderExecBaseSignalValue());

    offset = cmdStream->getUsed();

    events[1]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);
    immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

    if (immCmdList->dcFlushSupport) {
        EXPECT_NE(offset, cmdStream->getUsed());
        EXPECT_EQ(3u, events[1]->getInOrderExecBaseSignalValue());
    } else {
        EXPECT_EQ(offset, cmdStream->getUsed());
        EXPECT_EQ(2u, events[1]->getInOrderExecBaseSignalValue());
    }
}

} // namespace ult
} // namespace L0
