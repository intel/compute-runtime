/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"

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

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> yieldCounter;
} // namespace CpuIntrinsicsTests

namespace L0 {
namespace ult {

using InOrderCmdListTests = InOrderCmdListFixture;

using InOrderCmdListExtensionsTests = Test<ExtensionFixture>;

HWTEST_F(InOrderCmdListExtensionsTests, givenDriverHandleWhenAskingForExtensionsThenReturnCorrectVersions) {
    verifyExtensionDefinition(ZE_EVENT_POOL_COUNTER_BASED_EXP_NAME, ZE_EVENT_POOL_COUNTER_BASED_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_INTEL_COMMAND_LIST_MEMORY_SYNC, ZE_INTEL_COMMAND_LIST_MEMORY_SYNC_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZEX_COUNTER_BASED_EVENT_EXT_NAME, ZEX_COUNTER_BASED_EVENT_VERSION_CURRENT);
}

HWTEST_F(InOrderCmdListTests, givenCmdListWhenAskingForQwordDataSizeThenReturnFalse) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    EXPECT_FALSE(immCmdList->isQwordInOrderCounter());
}

HWTEST_F(InOrderCmdListTests, givenInvalidPnextStructWhenCreatingEventThenIgnore) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ze_event_desc_t extStruct = {ZE_STRUCTURE_TYPE_FORCE_UINT32};
    ze_event_desc_t eventDesc = {};
    eventDesc.pNext = &extStruct;

    auto event0 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));

    EXPECT_NE(nullptr, event0.get());
}

HWTEST_F(InOrderCmdListTests, givenEventSyncModeDescPassedWhenCreatingEventThenEnableNewModes) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 7;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ze_event_sync_mode_desc_t syncModeDesc = {ZE_STRUCTURE_TYPE_EVENT_SYNC_MODE_DESC};
    ze_event_desc_t eventDesc = {};
    eventDesc.pNext = &syncModeDesc;

    auto mockProductHelper = new MockProductHelper;
    mockProductHelper->isInterruptSupportedResult = true;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(mockProductHelper);

    eventDesc.index = 0;
    syncModeDesc.syncModeFlags = 0;
    auto event0 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_FALSE(event0->isInterruptModeEnabled());
    EXPECT_FALSE(event0->isKmdWaitModeEnabled());

    eventDesc.index = 1;
    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_SIGNAL_INTERRUPT;
    auto event1 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_TRUE(event1->isInterruptModeEnabled());
    EXPECT_FALSE(event1->isKmdWaitModeEnabled());

    eventDesc.index = 2;
    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_LOW_POWER_WAIT;
    auto event2 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_FALSE(event2->isInterruptModeEnabled());
    EXPECT_FALSE(event2->isKmdWaitModeEnabled());

    eventDesc.index = 3;
    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_SIGNAL_INTERRUPT | ZE_EVENT_SYNC_MODE_FLAG_LOW_POWER_WAIT;
    auto event3 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_TRUE(event3->isInterruptModeEnabled());
    EXPECT_TRUE(event3->isKmdWaitModeEnabled());

    eventDesc.index = 4;
    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_SIGNAL_INTERRUPT;
    syncModeDesc.externalInterruptId = 123;
    auto event4 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_EQ(NEO::InterruptId::notUsed, event4->externalInterruptId);

    eventDesc.index = 5;
    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_EXTERNAL_INTERRUPT_WAIT;
    EXPECT_ANY_THROW(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    mockProductHelper->isInterruptSupportedResult = false;
    eventDesc.index = 6;
    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_SIGNAL_INTERRUPT;
    auto event5 = Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue);
    EXPECT_EQ(event5, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST_F(InOrderCmdListTests, givenDeprecatedEventSyncModeDescPassedWhenCreatingEventThenEnableInterruptMode) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    zex_intel_event_sync_mode_exp_desc_t syncModeDesc = {ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC};
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;
    ze_event_desc_t eventDesc = {};
    eventDesc.pNext = &syncModeDesc;

    auto mockProductHelper = new MockProductHelper;
    mockProductHelper->isInterruptSupportedResult = true;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(mockProductHelper);

    auto event0 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_TRUE(event0->isInterruptModeEnabled());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenQueueFlagWhenCreatingCmdListThenEnableRelaxedOrdering) {
    NEO::debugManager.flags.ForceInOrderImmediateCmdListExecution.set(-1);

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

    ze_command_list_handle_t cmdList;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));

    EXPECT_TRUE(static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(cmdList)->isInOrderExecutionEnabled());
    auto flags = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(cmdList)->getCmdListFlags();
    EXPECT_EQ(static_cast<ze_command_list_flags_t>(ZE_COMMAND_LIST_FLAG_IN_ORDER), flags);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(cmdList));
}

HWTEST_F(InOrderCmdListTests, givenWorkAppendedAfterSignalingEventWhenGettingSignalFlushStampThenFlushStampOfEventSubmissionIsReturned) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    ultCsr->incrementFlushStampOnFlush = true;
    auto eventPool = createEvents<FamilyType>(1, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams));
    const auto eventSubmissionFlushStamp = ultCsr->obtainCurrentFlushStamp();
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    EXPECT_NE(eventSubmissionFlushStamp, ultCsr->obtainCurrentFlushStamp());

    NEO::FlushStamp signalFlushStamp = 0;
    EXPECT_TRUE(events[0]->getSignalFlushStamp(ultCsr, signalFlushStamp));
    EXPECT_EQ(eventSubmissionFlushStamp, signalFlushStamp);
}

HWTEST_F(InOrderCmdListTests, givenNotSignaledInOrderEventWhenAddedToWaitListThenSkipWait) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.ForceInOrderEvents.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ze_event_desc_t eventDesc = {};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    eventDesc.index = 0;
    auto event = std::unique_ptr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_TRUE(event->isCounterBased());

    auto handle = event->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    const auto offset = cmdStream->getUsed();

    returnValue = immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &handle, launchParams);

    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    EXPECT_TRUE(findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end()).empty());
}

HWTEST_F(InOrderCmdListTests, givenIpcAndCounterBasedEventPoolFlagsWhenCreatingThenReturnError) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_IPC;
    eventPoolDesc.count = 1;

    ze_event_pool_counter_based_exp_desc_t counterBasedExtension = {ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC};
    counterBasedExtension.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
    eventPoolDesc.pNext = &counterBasedExtension;

    auto eventPool = EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue);

    EXPECT_EQ(nullptr, eventPool);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, returnValue);
}

HWTEST_F(InOrderCmdListTests, givenIncorrectFlagsWhenCreatingCounterBasedEventsThenReturnError) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_pool_counter_based_exp_desc_t counterBasedExtension = {ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC};
    eventPoolDesc.pNext = &counterBasedExtension;

    counterBasedExtension.flags = 0;
    auto eventPool = EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue);
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE), eventPool->getCounterBasedFlags());
    EXPECT_NE(nullptr, eventPool);
    eventPool->destroy();

    counterBasedExtension.flags = static_cast<uint32_t>(ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE) << 1;
    eventPool = EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue);
    EXPECT_EQ(nullptr, eventPool);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);

    counterBasedExtension.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE;
    eventPool = EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue);
    EXPECT_EQ(counterBasedExtension.flags, eventPool->getCounterBasedFlags());
    EXPECT_NE(nullptr, eventPool);
    eventPool->destroy();

    counterBasedExtension.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
    eventPool = EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue);
    EXPECT_EQ(counterBasedExtension.flags, eventPool->getCounterBasedFlags());
    EXPECT_NE(nullptr, eventPool);
    eventPool->destroy();
}

HWTEST_F(InOrderCmdListTests, givenIpcPoolEventWhenTryingToImplicitlyConverToCounterBasedEventThenDisallow) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    auto eventPoolForExport = std::unique_ptr<WhiteBox<EventPool>>(static_cast<WhiteBox<EventPool> *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
    auto eventPoolImported = std::unique_ptr<WhiteBox<EventPool>>(static_cast<WhiteBox<EventPool> *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));

    eventPoolForExport->isIpcPoolFlag = true;
    eventPoolImported->isImportedIpcPool = true;

    ze_event_desc_t eventDesc = {};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    DestroyableZeUniquePtr<InOrderFixtureMockEvent> event0(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolForExport.get(), &eventDesc, device, returnValue)));
    EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, event0->counterBasedMode);

    DestroyableZeUniquePtr<InOrderFixtureMockEvent> event1(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolImported.get(), &eventDesc, device, returnValue)));
    EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, event1->counterBasedMode);
}

HWTEST_F(InOrderCmdListTests, givenNotSignaledInOrderWhenWhenCallingQueryStatusThenReturnSuccess) {
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->enableCounterBasedMode(true, eventPool->getCounterBasedFlags());

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->queryStatus(0));
}

HWTEST_F(InOrderCmdListTests, givenCmdListsWhenDispatchingThenUseInternalTaskCountForWaits) {
    auto immCmdList0 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    bool heapless = ultCsr->heaplessPrologProgrammed;
    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u + (heapless ? 1u : 0u), immCmdList0->cmdQImmediate->getTaskCount());
    EXPECT_EQ(2u + (heapless ? 1u : 0u), immCmdList1->cmdQImmediate->getTaskCount());

    // explicit wait
    {
        immCmdList0->hostSynchronize(0);
        EXPECT_EQ(heapless ? 2u : 1u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

        immCmdList1->hostSynchronize(0);
        EXPECT_EQ(heapless ? 3u : 2u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    }

    // implicit wait
    {
        immCmdList0->copyThroughLockedPtrEnabled = true;
        immCmdList1->copyThroughLockedPtrEnabled = true;

        void *deviceAlloc = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);

        uint32_t hostCopyData = 0;
        auto hostAddress0 = static_cast<uint64_t *>(immCmdList0->inOrderExecInfo->getBaseHostAddress(0));
        auto hostAddress1 = static_cast<uint64_t *>(immCmdList1->inOrderExecInfo->getBaseHostAddress(0));

        *hostAddress0 = 1;
        *hostAddress1 = 1;
        CmdListMemoryCopyParams copyParams = {};
        immCmdList0->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, copyParams);

        auto expectedLatestTaskCount = immCmdList0->dcFlushSupport || (!heapless && immCmdList0->latestOperationHasCbEventWithProfiling) ? 1u : 2u;
        expectedLatestTaskCount += (heapless ? 1u : 0u);
        EXPECT_EQ(expectedLatestTaskCount, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(immCmdList0->dcFlushSupport || (!heapless && immCmdList0->latestOperationHasCbEventWithProfiling) ? 3u : 2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

        immCmdList1->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, copyParams);

        expectedLatestTaskCount = 2u;
        expectedLatestTaskCount += (heapless ? 1u : 0u);
        EXPECT_EQ(expectedLatestTaskCount, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(immCmdList0->dcFlushSupport || (!heapless && immCmdList0->latestOperationHasCbEventWithProfiling) ? 4u : 2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

        context->freeMem(deviceAlloc);
    }

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList0.get());
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList1.get());
}

struct HeapfullCbProfilingEventMatch {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAtLeastXeHpcCore::isMatched<productFamily>() && HeapfulSupportedMatch::isMatched<productFamily>();
    }
};

HWTEST2_F(InOrderCmdListTests, givenCmdListsWhenDispatchingFillThenMarkCmdChainingProperly, HeapfullCbProfilingEventMatch) {
    auto immCmdList0 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto event = createEvents<FamilyType>(1, true);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;
    CmdListMemoryCopyParams copyParams = {};
    immCmdList0->appendMemoryFill(deviceAlloc, &hostCopyData, 4, 4, events[0].get(), 0, nullptr, copyParams);

    EXPECT_TRUE(immCmdList0->latestOperationHasCbEventWithProfiling);
    EXPECT_FALSE(immCmdList0->latestOperationRequiredNonWalkerInOrderCmdsChaining);

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenCmdListsWhenDispatchingMemcpyThenMarkCmdChainingProperly, HeapfullCbProfilingEventMatch) {
    auto immCmdList0 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto event = createEvents<FamilyType>(1, true);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;
    CmdListMemoryCopyParams copyParams = {};
    immCmdList0->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, events[0].get(), 0, nullptr, copyParams);

    EXPECT_TRUE(immCmdList0->latestOperationHasCbEventWithProfiling);
    EXPECT_FALSE(immCmdList0->latestOperationRequiredNonWalkerInOrderCmdsChaining);

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, GivenCounterBasedEventWithSignalWithUserInterruptWhenSignalingViaAnyPathThenExactlyOneUserInterruptIsEmitted, IsAtLeastXeCore) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto eventPool = createEvents<FamilyType>(1, false);
    auto *event = events[0].get();
    event->setSignalWithUserInterrupt(true);

    auto *cmdStream = cmdList->getCmdContainer().getCommandStream();

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, 256, 256, &deviceAlloc));
    uint32_t hostData = 0;
    CmdListMemoryCopyParams copyParams = {};
    CmdListWaitEventParameters waitEventsParameters = {
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

    auto countInterrupts = [&](std::function<void()> appendOp) -> size_t {
        auto offset = cmdStream->getUsed();
        appendOp();
        GenCmdList genCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
        return findAll<MI_USER_INTERRUPT *>(genCmdList.begin(), genCmdList.end()).size();
    };

    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams); }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters, signalEventParameters); }));
    EXPECT_EQ(1u, countInterrupts([&]() {
                  CmdListSignalEventParameters signalEventParameters = {
                      .relaxedOrderingDispatch = false,
                  };
                  cmdList->appendSignalEvent(event->toHandle(), signalEventParameters);
              }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendMemoryFill(deviceAlloc, &hostData, sizeof(hostData), sizeof(hostData), event->toHandle(), 0, nullptr, copyParams); }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendMemoryCopy(deviceAlloc, &hostData, sizeof(hostData), event->toHandle(), 0, nullptr, copyParams); }));

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, GivenTimestampCounterBasedEventWithSignalWithUserInterruptWhenSignalingViaAnyPathThenExactlyOneUserInterruptIsEmitted, IsAtLeastXeCore) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto eventPool = createEvents<FamilyType>(1, true);
    auto *event = events[0].get();
    event->setSignalWithUserInterrupt(true);

    auto *cmdStream = cmdList->getCmdContainer().getCommandStream();

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, 256, 256, &deviceAlloc));
    uint32_t hostData = 0;
    CmdListMemoryCopyParams copyParams = {};
    CmdListWaitEventParameters waitEventsParameters = {
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

    auto countInterrupts = [&](std::function<void()> appendOp) -> size_t {
        auto offset = cmdStream->getUsed();
        appendOp();
        GenCmdList genCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
        return findAll<MI_USER_INTERRUPT *>(genCmdList.begin(), genCmdList.end()).size();
    };

    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams); }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters, signalEventParameters); }));
    EXPECT_EQ(1u, countInterrupts([&]() {
                  CmdListSignalEventParameters signalEventParameters = {
                      .relaxedOrderingDispatch = false,
                  };
                  cmdList->appendSignalEvent(event->toHandle(), signalEventParameters);
              }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendMemoryFill(deviceAlloc, &hostData, sizeof(hostData), sizeof(hostData), event->toHandle(), 0, nullptr, copyParams); }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendMemoryCopy(deviceAlloc, &hostData, sizeof(hostData), event->toHandle(), 0, nullptr, copyParams); }));

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, GivenCompactedCounterBasedEventWithSignalWithUserInterruptWhenSignalingViaAnyPathThenExactlyOneUserInterruptIsEmitted, IsAtLeastXeCore) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.CompactL3FlushEventPacket.set(1);

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto eventPool = createEvents<FamilyType>(1, true);
    auto *event = events[0].get();
    event->setSignalWithUserInterrupt(true);

    auto *cmdStream = cmdList->getCmdContainer().getCommandStream();

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, 256, 256, &deviceAlloc));
    uint32_t hostData = 0;
    CmdListMemoryCopyParams copyParams = {};
    CmdListWaitEventParameters waitEventsParameters = {
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
    auto countInterrupts = [&](std::function<void()> appendOp) -> size_t {
        auto offset = cmdStream->getUsed();
        appendOp();
        GenCmdList genCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
        return findAll<MI_USER_INTERRUPT *>(genCmdList.begin(), genCmdList.end()).size();
    };

    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams); }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters, signalEventParameters); }));
    EXPECT_EQ(1u, countInterrupts([&]() {
                  CmdListSignalEventParameters signalEventParameters = {
                      .relaxedOrderingDispatch = false,
                  };
                  cmdList->appendSignalEvent(event->toHandle(), signalEventParameters);
              }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendMemoryFill(deviceAlloc, &hostData, sizeof(hostData), sizeof(hostData), event->toHandle(), 0, nullptr, copyParams); }));
    EXPECT_EQ(1u, countInterrupts([&]() { cmdList->appendMemoryCopy(deviceAlloc, &hostData, sizeof(hostData), event->toHandle(), 0, nullptr, copyParams); }));

    context->freeMem(deviceAlloc);
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedEventsWhenHostWaitsAreCalledThenLatestWaitIsRecorded) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(2, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    auto &inOrderExecInfo = immCmdList->inOrderExecInfo;
    *inOrderExecInfo->getBaseHostAddress(0) = 2u;

    auto status = events[1]->hostSynchronize(-1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);

    auto counterValue = events[1]->inOrderExecHelper.getEventData()->counterValue;
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(counterValue, 0));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(events[0]->inOrderExecHelper.getEventData()->counterValue, 0));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(counterValue + 1, 0));

    // setting lower counter ignored
    inOrderExecInfo->setLastWaitedCounterValue(counterValue - 1, 0);
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(counterValue, 0));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(events[0]->inOrderExecHelper.getEventData()->counterValue, 0));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(counterValue + 1, 0));

    status = events[0]->hostSynchronize(-1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(counterValue, 0));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(counterValue + 1, 0));

    inOrderExecInfo->setAllocationOffset(4u);
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(0u, 0));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(counterValue, 0));

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWTEST_F(InOrderCmdListTests, givenTbxModeWhenHostSynchronizeIsCalledThenPublishWaitedCounterAfterDownloadingAllocations) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto &inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    bool counterPublishedOnDownload = true;
    ultCsr->onDownloadAllocations = [&]() {
        counterPublishedOnDownload = inOrderExecInfo->isCounterAlreadyDone(inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset());
    };

    immCmdList->isTbxMode = true;
    auto downloadCallsBefore = ultCsr->downloadAllocationsCalledCount.load();

    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset()));

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(1, true));

    EXPECT_EQ(downloadCallsBefore + 1, ultCsr->downloadAllocationsCalledCount.load());
    EXPECT_FALSE(counterPublishedOnDownload);
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset()));

    ultCsr->onDownloadAllocations = nullptr;
}

HWTEST_F(InOrderCmdListTests, givenInOrderImmediateCommandListWhenHostSynchronizeCalledWithZeroTimeoutThenStillBlockOnCounterMiss) {
    VariableBackup<NEO::WaitUtils::WaitpkgUse> waitpkgUseBackup(&NEO::WaitUtils::waitpkgUse, NEO::WaitUtils::WaitpkgUse::noUse);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList->latestFlushIsHostVisible = true;

    auto &inOrderExecInfo = immCmdList->inOrderExecInfo;
    *ptrOffset(inOrderExecInfo->getBaseHostAddress(0), inOrderExecInfo->getAllocationOffset()) = 0;

    ASSERT_FALSE(inOrderExecInfo->isCounterAlreadyDone(inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset()));

    auto yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
    EXPECT_LT(yieldCountBefore, CpuIntrinsicsTests::yieldCounter.load());

    yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(1, false));
    EXPECT_LT(yieldCountBefore, CpuIntrinsicsTests::yieldCounter.load());

    inOrderExecInfo->setLastWaitedCounterValue(inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenDebugFlagSetWhenEventHostSyncCalledThenCallWaitUserFence) {
    NEO::debugManager.flags.WaitForUserFenceOnEventHostSynchronize.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(2, false);
    EXPECT_TRUE(events[0]->isKmdWaitModeEnabled());
    EXPECT_TRUE(events[0]->isInterruptModeEnabled());
    EXPECT_TRUE(events[1]->isKmdWaitModeEnabled());
    EXPECT_TRUE(events[1]->isInterruptModeEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(2));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->inOrderExecHelper).getInOrderExecEventDataPtr()->counterOffset = 123;

    uint64_t hostAddress = 0;
    if (events[0]->inOrderExecHelper.isHostStorageDuplicated()) {
        hostAddress = castToUint64(ptrOffset(events[0]->inOrderExecHelper.getBaseHostCpuAddress(), events[0]->inOrderExecHelper.getEventData()->counterOffset));
    } else {
        hostAddress = castToUint64(ptrOffset(events[0]->inOrderExecHelper.getDeviceCounterAllocation()->getUnderlyingBuffer(), events[0]->inOrderExecHelper.getEventData()->counterOffset));
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->isUserFenceWaitSupported = true;
    ultCsr->waitUserFenceParams.forceRetStatusEnabled = true;
    ultCsr->waitUserFenceParams.forceRetStatusValue = false;
    EXPECT_EQ(0u, ultCsr->waitUserFenceParams.callCount);

    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(2));

    EXPECT_LE(1u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(hostAddress, ultCsr->waitUserFenceParams.latestWaitedAddress);
    EXPECT_EQ(events[0]->inOrderExecHelper.getEventData()->counterValue, ultCsr->waitUserFenceParams.latestWaitedValue);
    EXPECT_EQ(2, ultCsr->waitUserFenceParams.latestWaitedTimeout);

    ultCsr->waitUserFenceParams.forceRetStatusValue = true;
    auto callCountBefore = ultCsr->waitUserFenceParams.callCount;

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(3));

    EXPECT_EQ(callCountBefore + 1, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(hostAddress, ultCsr->waitUserFenceParams.latestWaitedAddress);
    EXPECT_EQ(events[0]->getInOrderExecBaseSignalValue(), ultCsr->waitUserFenceParams.latestWaitedValue);
    EXPECT_EQ(3, ultCsr->waitUserFenceParams.latestWaitedTimeout);

    callCountBefore = ultCsr->waitUserFenceParams.callCount;

    // already completed
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(3));
    EXPECT_EQ(callCountBefore, ultCsr->waitUserFenceParams.callCount);

    // non in-order event
    events[1]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[1]->hostSynchronize(2);
    EXPECT_EQ(callCountBefore, ultCsr->waitUserFenceParams.callCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenRegularCmdListWhenAppendQueryKernelTimestampsCalledThenSynchronizeCounterBasedEvents) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(2, true);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    auto deviceMem = allocDeviceMem(128);

    ze_event_handle_t queryEvents[2] = {events[0]->toHandle(), events[1]->toHandle()};

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    bool chainingRequired = regularCmdList->latestOperationRequiredNonWalkerInOrderCmdsChaining;
    const bool cbProfilingEvent = regularCmdList->latestOperationHasCbEventWithProfiling;
    const bool counterSignalPending = regularCmdList->isInOrderCounterSignalPending();
    const bool barrierRequired = counterSignalPending || cbProfilingEvent;

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();
    CmdListWaitEventParameters waitEventsParameters{
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = false,
        .waitForImplicitInOrderDependency = false,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
        .apiRequest = false};
    regularCmdList->appendQueryKernelTimestamps(2, queryEvents, deviceMem, nullptr, nullptr, 0, nullptr, waitEventsParameters);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ((chainingRequired || barrierRequired) ? 1u : 2u, semaphores.size());

    if (counterSignalPending && !cbProfilingEvent) {
        auto barrier = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), barrier);
    }

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphores[0]);

    EXPECT_EQ(events[1]->getCompletionFieldGpuAddress(device), NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd));

    context->freeMem(deviceMem);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenRegularCmdListWhenAppendCbTimestampEventWithSkipAddToResidencyFlagThenEventAllocationNotAddedToResidency) {
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, true);

    launchParams.omitAddingEventResidency = true;
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto eventAllocation = events[0]->getAllocation(this->device);

    auto &cmdlistResidency = regularCmdList->getCmdContainer().getResidencyContainer();
    auto eventAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), eventAllocation);
    EXPECT_EQ(eventAllocationIt, cmdlistResidency.end());
}

struct TimestampCompletionMockEvent : public L0::EventImp<uint64_t> {
    using BaseClass = L0::EventImp<uint64_t>;

    TimestampCompletionMockEvent(L0::EventPool *pool, L0::Device *device, bool tbxMode) : BaseClass::EventImp(0, device, tbxMode) {
        this->eventPool = pool;

        this->eventPoolAllocation = &pool->getAllocation();

        this->totalEventSize = 128;
        hostAddressFromPool = eventPoolAllocation->getGraphicsAllocation(0)->getUnderlyingBuffer();
        this->csrs[0] = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

        this->maxPacketCount = 1;
    }

    uint32_t assignKernelEventCompletionDataCalled = 0;
    uint32_t assignKernelEventCompletionDataFailCounter = 0;
    mutable uint32_t getCompletionTimeoutCalled = 0;
    uint64_t completionTimeout = std::numeric_limits<uint64_t>::max();
    const uint64_t notReadyData = Event::STATE_CLEARED;
    bool useContextEndForVerification = true;

    void assignKernelEventCompletionData(void *address) override {
        auto offset = useContextEndForVerification ? NEO::TimestampPackets<uint64_t, 1>::getContextEndOffset() : NEO::TimestampPackets<uint64_t, 1>::getGlobalEndOffset();
        auto completionAddress = reinterpret_cast<uint64_t *>(ptrOffset(getHostAddress(), offset));
        assignKernelEventCompletionDataCalled++;
        if (assignKernelEventCompletionDataCalled <= assignKernelEventCompletionDataFailCounter) {
            *completionAddress = notReadyData;
        } else {
            *completionAddress = 0x123;
        }

        L0::EventImp<uint64_t>::assignKernelEventCompletionData(address);
    }

    uint64_t getCompletionTimeout() const override {
        getCompletionTimeoutCalled++;
        return completionTimeout;
    }
};

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCounterBasedTimestampEventWhenQueryingTimestampThenEnsureItsCompletion) {
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto event1 = std::make_unique<TimestampCompletionMockEvent>(eventPool.get(), device, false);
    auto event2 = std::make_unique<TimestampCompletionMockEvent>(eventPool.get(), device, false);
    auto event3 = std::make_unique<TimestampCompletionMockEvent>(eventPool.get(), device, false);

    event1->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event1->getInOrderExecEventHelper().initializeLocalTempStorage();
    event1->assignKernelEventCompletionDataFailCounter = 2;
    event1->setEventTimestampFlag(true);
    event1->useContextEndForVerification = true;

    event2->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event2->getInOrderExecEventHelper().initializeLocalTempStorage();
    event2->assignKernelEventCompletionDataFailCounter = 2;
    event2->setEventTimestampFlag(true);
    event2->useContextEndForVerification = false;

    event3->disableImplicitCounterBasedMode();
    event3->assignKernelEventCompletionDataFailCounter = 2;
    event3->setEventTimestampFlag(true);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event1->toHandle(), 0, nullptr, launchParams);
    event1->hostEventSetValue(Event::STATE_CLEARED);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event2->toHandle(), 0, nullptr, launchParams);
    event2->hostEventSetValue(Event::STATE_CLEARED);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event3->toHandle(), 0, nullptr, launchParams);
    event3->hostEventSetValue(Event::STATE_CLEARED);

    event1->getInOrderExecEventHelper().setLastWaitedCounterValue(2, 0);
    event2->getInOrderExecEventHelper().setLastWaitedCounterValue(2, 0);
    event3->getInOrderExecEventHelper().setLastWaitedCounterValue(3, 0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event1->queryStatus(0));
    EXPECT_EQ(ZE_RESULT_SUCCESS, event2->queryStatus(0));
    EXPECT_EQ(ZE_RESULT_SUCCESS, event3->queryStatus(0));

    ze_kernel_timestamp_result_t kernelTimestamps = {};

    event1->assignKernelEventCompletionDataCalled = 0;
    event2->assignKernelEventCompletionDataCalled = 0;
    event3->assignKernelEventCompletionDataCalled = 0;
    event1->queryKernelTimestamp(&kernelTimestamps);
    event2->queryKernelTimestamp(&kernelTimestamps);
    event3->queryKernelTimestamp(&kernelTimestamps);

    EXPECT_EQ(event1->assignKernelEventCompletionDataFailCounter + 1, event1->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(event2->assignKernelEventCompletionDataFailCounter + 1, event2->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(event3->assignKernelEventCompletionDataFailCounter + 1, event3->assignKernelEventCompletionDataCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCounterBasedTimestampEventWithNotCompletedCounterWhenQueryingTimestampThenCheckTimestampOnceWithoutWaiting) {
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto event = std::make_unique<TimestampCompletionMockEvent>(eventPool.get(), device, false);

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->setEventTimestampFlag(true);
    event->assignKernelEventCompletionDataFailCounter = std::numeric_limits<uint32_t>::max();
    event->completionTimeout = 0;

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    event->hostEventSetValue(Event::STATE_CLEARED);
    ASSERT_TRUE(event->hasInOrderTimestampNode());

    ze_kernel_timestamp_result_t kernelTimestamps = {};

    event->assignKernelEventCompletionDataCalled = 0;
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->queryKernelTimestamp(&kernelTimestamps));
    EXPECT_EQ(2u, event->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);

    event->assignKernelEventCompletionDataCalled = 0;
    event->assignKernelEventCompletionDataFailCounter = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryKernelTimestamp(&kernelTimestamps));
    EXPECT_EQ(2u, event->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenTbxModeAndCounterBasedTimestampEventWithNotCompletedCounterWhenQueryingTimestampThenDownloadAllocationsAndReturnNotReadyWithoutWaiting) {
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(cmdList->getCsr(false));

    auto eventPool = createEvents<FamilyType>(1, true);
    auto event = std::make_unique<TimestampCompletionMockEvent>(eventPool.get(), device, true);

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->setEventTimestampFlag(true);
    event->assignKernelEventCompletionDataFailCounter = std::numeric_limits<uint32_t>::max();
    event->completionTimeout = 0;

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    event->hostEventSetValue(Event::STATE_CLEARED);
    ASSERT_TRUE(event->hasInOrderTimestampNode());

    ze_kernel_timestamp_result_t kernelTimestamps = {};

    ultCsr->downloadAllocationCalled = false;
    event->assignKernelEventCompletionDataCalled = 0;
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->queryKernelTimestamp(&kernelTimestamps));
    EXPECT_TRUE(ultCsr->downloadAllocationCalled);
    EXPECT_EQ(2u, event->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCounterBasedEventWithProfilingAndNotCompletedCounterWhenQueryingStatusThenCheckTimestampOnceWithoutWaiting) {
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto event = std::make_unique<TimestampCompletionMockEvent>(eventPool.get(), device, false);

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->setEventTimestampFlag(true);
    event->assignKernelEventCompletionDataFailCounter = std::numeric_limits<uint32_t>::max();
    event->completionTimeout = 0;

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    event->hostEventSetValue(Event::STATE_CLEARED);
    event->setCbEventWithProfiling(true);

    event->assignKernelEventCompletionDataCalled = 0;
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->queryStatus(0));
    EXPECT_EQ(1u, event->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);

    event->assignKernelEventCompletionDataCalled = 0;
    event->assignKernelEventCompletionDataFailCounter = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus(0));
    EXPECT_EQ(1u, event->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCounterBasedEventWithoutTimestampAndNotCompletedCounterWhenQueryingStatusThenReturnNotReadyWithoutWaiting) {
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto event = std::make_unique<TimestampCompletionMockEvent>(eventPool.get(), device, false);

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->completionTimeout = 0;

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    auto hostCounterAddress = cmdList->inOrderExecInfo->isHostStorageDuplicated() ? cmdList->inOrderExecInfo->getBaseHostAddress(0) : static_cast<uint64_t *>(cmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    *hostCounterAddress = 0;

    EXPECT_EQ(ZE_RESULT_NOT_READY, event->queryStatus(0));
    EXPECT_EQ(0u, event->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);

    *hostCounterAddress = event->getInOrderExecBaseSignalValue();

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus(0));
    EXPECT_EQ(0u, event->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenRegularTimestampEventWithNotCompletedCounterWhenQueryingThenReturnNotReadyWithoutWaiting) {
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto event = std::make_unique<TimestampCompletionMockEvent>(eventPool.get(), device, false);

    event->disableImplicitCounterBasedMode();
    event->setEventTimestampFlag(true);
    event->assignKernelEventCompletionDataFailCounter = std::numeric_limits<uint32_t>::max();
    event->completionTimeout = 0;

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    event->hostEventSetValue(Event::STATE_CLEARED);
    ASSERT_FALSE(event->isCounterBased());
    auto hostCounterAddress = cmdList->inOrderExecInfo->isHostStorageDuplicated() ? cmdList->inOrderExecInfo->getBaseHostAddress(0) : static_cast<uint64_t *>(cmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    *hostCounterAddress = 0;

    ze_kernel_timestamp_result_t kernelTimestamps = {};

    event->assignKernelEventCompletionDataCalled = 0;
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->queryStatus(0));
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->queryKernelTimestamp(&kernelTimestamps));
    EXPECT_EQ(0u, event->assignKernelEventCompletionDataCalled);
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);

    *hostCounterAddress = event->getInOrderExecBaseSignalValue();

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus(0));
    EXPECT_EQ(0u, event->getCompletionTimeoutCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInterruptableEventsWhenExecutingOnDifferentCsrThenAssignItToEventOnExecute) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto cmdlistHandle = cmdList->toHandle();

    auto eventPool = createEvents<FamilyType>(3, false);
    events[0]->enableKmdWaitMode();
    events[1]->enableKmdWaitMode();

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[2]->toHandle(), 0, nullptr, launchParams);
    cmdList->close();

    ASSERT_EQ(2u, cmdList->interruptEvents.size());
    EXPECT_EQ(events[0].get(), cmdList->interruptEvents[0]);
    EXPECT_EQ(events[1].get(), cmdList->interruptEvents[1]);

    ze_command_queue_desc_t desc = {};

    NEO::CommandStreamReceiver *csr1 = nullptr;
    for (auto &it : device->getNEODevice()->getAllEngines()) {
        if (it.osContext->isLowPriority() && NEO::EngineHelpers::isComputeEngine(it.osContext->getEngineType())) {
            csr1 = it.commandStreamReceiver;
            break;
        }
    }

    ASSERT_NE(nullptr, csr1);

    auto firstQueue = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr1, &desc);
    firstQueue->initialize(false, false, false);

    auto csr2 = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    ASSERT_NE(nullptr, csr2);
    auto secondQueue = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr2, &desc);
    secondQueue->initialize(false, false, false);

    EXPECT_NE(firstQueue->getCsr(), secondQueue->getCsr());

    CommandListExecutionInternalOptions internalOptions = {};
    firstQueue->executeCommandLists(1, &cmdlistHandle, nullptr, internalOptions);
    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(firstQueue->getCsr(), events[0]->csrs[0]);
    EXPECT_EQ(1u, events[1]->csrs.size());
    EXPECT_EQ(firstQueue->getCsr(), events[1]->csrs[0]);
    EXPECT_EQ(1u, events[2]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[2]->csrs[0]);

    secondQueue->executeCommandLists(1, &cmdlistHandle, nullptr, internalOptions);
    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(secondQueue->getCsr(), events[0]->csrs[0]);
    EXPECT_EQ(1u, events[1]->csrs.size());
    EXPECT_EQ(secondQueue->getCsr(), events[1]->csrs[0]);
    EXPECT_EQ(1u, events[2]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[2]->csrs[0]);

    cmdList->reset();
    EXPECT_EQ(0u, cmdList->interruptEvents.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenRegularCommandListWithLinuxUserFenceKmdWaitEventWhenExecutingOnSecondaryCsrThenAssignThatCsrToEvent) {
    DebugManagerStateRestore restore;
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto cmdlistHandle = cmdList->toHandle();

    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    defaultCsr->isWaitUserFenceNotEqualSupportedValue = true;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(false);
    auto eventWithoutKmdWait = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_FALSE(eventWithoutKmdWait->isLinuxUserFenceKmdWaitEnabled());
    ASSERT_FALSE(eventWithoutKmdWait->isSignalWithUserInterrupt());
    auto eventWithoutKmdWaitWhiteBox = whiteboxCast(eventWithoutKmdWait.get());

    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    eventDesc.index = 1;
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_FALSE(event->isCounterBased());
    ASSERT_TRUE(event->isLinuxUserFenceKmdWaitEnabled());
    ASSERT_FALSE(event->isSignalWithUserInterrupt());
    auto eventWhiteBox = whiteboxCast(event.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventWithoutKmdWait->toHandle(), 0, nullptr, launchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    ASSERT_EQ(1u, cmdList->interruptEvents.size());
    EXPECT_EQ(event.get(), cmdList->interruptEvents[0]);

    NEO::CommandStreamReceiver *secondaryCsr = nullptr;
    for (auto &engine : device->getNEODevice()->getAllEngines()) {
        if (engine.osContext->isLowPriority() && NEO::EngineHelpers::isComputeEngine(engine.osContext->getEngineType())) {
            secondaryCsr = engine.commandStreamReceiver;
            break;
        }
    }
    ASSERT_NE(nullptr, secondaryCsr);
    ASSERT_NE(defaultCsr, secondaryCsr);

    ze_command_queue_desc_t desc = {};
    auto queue = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, secondaryCsr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, queue->initialize(false, false, false));
    CommandListExecutionInternalOptions internalOptions = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, queue->executeCommandLists(1, &cmdlistHandle, nullptr, internalOptions));

    ASSERT_EQ(1u, eventWhiteBox->csrs.size());
    EXPECT_EQ(secondaryCsr, eventWhiteBox->csrs[0]);
    ASSERT_EQ(1u, eventWithoutKmdWaitWhiteBox->csrs.size());
    EXPECT_EQ(defaultCsr, eventWithoutKmdWaitWhiteBox->csrs[0]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenUserInterruptEventWhenWaitingThenWaitForUserFenceWithParams) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(2, false);
    events[0]->enableKmdWaitMode();
    events[0]->enableInterruptMode();

    events[1]->enableKmdWaitMode();
    events[1]->enableInterruptMode();
    events[1]->externalInterruptId = 0x123;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->isUserFenceWaitSupported = true;
    ultCsr->waitUserFenceParams.forceRetStatusEnabled = true;

    EXPECT_EQ(0u, ultCsr->waitUserFenceParams.callCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(2));

    EXPECT_EQ(1u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(NEO::InterruptId::notUsed, ultCsr->waitUserFenceParams.externalInterruptId);
    EXPECT_TRUE(ultCsr->waitUserFenceParams.userInterrupt);

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[1]->hostSynchronize(2));

    EXPECT_EQ(2u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(events[1]->externalInterruptId, ultCsr->waitUserFenceParams.externalInterruptId);
    EXPECT_TRUE(ultCsr->waitUserFenceParams.userInterrupt);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenUserInterruptEventAndTbxModeWhenWaitingThenDontWaitForUserFence) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->waitUserFenceParams.forceRetStatusEnabled = true;
    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    EXPECT_FALSE(ultCsr->waitUserFenceSupported(nullptr));

    auto eventPool = createEvents<FamilyType>(2, false);
    events[0]->enableKmdWaitMode();
    events[0]->enableInterruptMode();

    events[1]->enableKmdWaitMode();
    events[1]->enableInterruptMode();
    events[1]->externalInterruptId = 0x123;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    events[0]->hostSynchronize(2);
    events[1]->hostSynchronize(2);
    EXPECT_EQ(0u, ultCsr->waitUserFenceParams.callCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenUserInterruptEventWhenWaitingThenPassCorrectAllocation) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto singleStorageImmCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto duplicatedStorageImmCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(2, false);
    events[0]->enableKmdWaitMode();
    events[0]->enableInterruptMode();

    events[1]->enableKmdWaitMode();
    events[1]->enableInterruptMode();

    singleStorageImmCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    duplicatedStorageImmCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->isUserFenceWaitSupported = true;
    ultCsr->waitUserFenceParams.forceRetStatusEnabled = true;

    EXPECT_EQ(0u, ultCsr->waitUserFenceParams.callCount);

    // Single counter storage
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(2));

    EXPECT_EQ(1u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(events[0]->getInOrderExecEventHelper().getDeviceCounterAllocation(), ultCsr->waitUserFenceParams.latestAllocForInterruptWait);
    EXPECT_TRUE(ultCsr->waitUserFenceParams.userInterrupt);

    // Duplicated host storage
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[1]->hostSynchronize(2));

    EXPECT_EQ(2u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(events[1]->getInOrderExecEventHelper().getHostCounterAllocation(), ultCsr->waitUserFenceParams.latestAllocForInterruptWait);
    EXPECT_TRUE(ultCsr->waitUserFenceParams.userInterrupt);

    // External host storage
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));
    *hostAddress = 0;

    uint64_t *gpuAddress = ptrOffset(hostAddress, 0x100);

    ze_event_handle_t handle = nullptr;

    ze_event_counter_based_external_sync_allocation_desc_t externalSync = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
    externalSync.deviceAddress = gpuAddress;
    externalSync.hostAddress = hostAddress;
    externalSync.completionValue = 1;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSync;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    auto event2 = L0::Event::fromHandle(handle);
    event2->enableKmdWaitMode();
    event2->enableInterruptMode();

    event2->hostSynchronize(2);

    EXPECT_EQ(3u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(event2->getInOrderExecEventHelper().getHostCounterAllocation(), ultCsr->waitUserFenceParams.latestAllocForInterruptWait);
    EXPECT_TRUE(ultCsr->waitUserFenceParams.userInterrupt);

    event2->destroy();
    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenInOrderModeWhenHostResetOrSignalEventCalledThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(3, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_TRUE(MemoryConstants::pageSize64k >= immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBufferSize());

    EXPECT_TRUE(events[0]->isCounterBased());
    EXPECT_EQ(events[0]->getInOrderExecBaseSignalValue(), immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(events[0]->getInOrderExecEventHelper().getDeviceCounterAllocation(), immCmdList->inOrderExecInfo->getDeviceCounterAllocation());
    EXPECT_EQ(events[0]->getInOrderAllocationOffset(), 0u);

    static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->inOrderExecHelper).getInOrderExecEventDataPtr()->counterOffset = 123;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, events[0]->reset());

    EXPECT_EQ(events[0]->getInOrderExecBaseSignalValue(), immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(events[0]->getInOrderExecEventHelper().getBaseDeviceAddress(), immCmdList->inOrderExecInfo->getBaseDeviceAddress());
    EXPECT_EQ(events[0]->getInOrderAllocationOffset(), 123u);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, events[0]->hostSignal(false));
}

HWTEST_F(InOrderCmdListTests, whenCreatingInOrderExecInfoThenReuseDeviceAlloc) {
    auto tag = device->getDeviceInOrderCounterAllocator()->getTag();

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa1 = immCmdList1->inOrderExecInfo->getBaseDeviceAddress();

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa2 = immCmdList2->inOrderExecInfo->getBaseDeviceAddress();

    auto offset = alignUp(device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset(), MemoryConstants::cacheLineSize * 4) * 2;

    EXPECT_EQ(alignUp(gpuVa1 + offset, MemoryConstants::cacheLineSize), gpuVa2);

    // allocation from the same allocator
    EXPECT_EQ(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation(), tag->getBaseGraphicsAllocation()->getGraphicsAllocation(0));

    immCmdList1.reset();

    auto immCmdList3 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa3 = immCmdList3->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuVa1, gpuVa3);

    immCmdList2.reset();

    auto immCmdList4 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa4 = immCmdList4->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuVa2, gpuVa4);

    tag->returnTag();
}

HWTEST_F(InOrderCmdListTests, whenCreatingInOrderExecInfoThenReuseHostAlloc) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto tag = device->getHostInOrderCounterAllocator()->getTag();

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa1 = immCmdList1->inOrderExecInfo->getBaseHostAddress(0);

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa2 = immCmdList2->inOrderExecInfo->getBaseHostAddress(0);

    EXPECT_NE(gpuVa1, gpuVa2);

    // allocation from the same allocator
    EXPECT_EQ(immCmdList1->inOrderExecInfo->getHostCounterAllocation(0), tag->getBaseGraphicsAllocation()->getGraphicsAllocation(0));

    immCmdList1.reset();

    auto immCmdList3 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa3 = immCmdList3->inOrderExecInfo->getBaseHostAddress(0);

    EXPECT_EQ(gpuVa1, gpuVa3);

    immCmdList2.reset();

    auto immCmdList4 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa4 = immCmdList4->inOrderExecInfo->getBaseHostAddress(0);

    EXPECT_EQ(gpuVa2, gpuVa4);

    tag->returnTag();
}

HWTEST_F(InOrderCmdListTests, givenInOrderEventWhenAppendEventResetCalledThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(3, false);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendEventReset(events[0]->toHandle()));
}

HWTEST_F(InOrderCmdListTests, givenRegularEventWithTemporaryInOrderDataAssignmentWhenCallingSynchronizeOrResetThenUnset) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto hostAddress = static_cast<uint64_t *>(cmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());

    if (cmdList->isHeaplessModeEnabled() && cmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = static_cast<uint64_t *>(cmdList->inOrderExecInfo->getBaseHostAddress(0));
    }

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    auto nonWalkerSignallingSupported = cmdList->isInOrderNonWalkerSignalingRequired(events[0].get());

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->getInOrderExecEventHelper().isDataAssigned());

    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(1));
    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->getInOrderExecEventHelper().isDataAssigned());

    if (nonWalkerSignallingSupported) {
        *hostAddress = 1;
    } else {
        *reinterpret_cast<uint64_t *>(events[0]->getCompletionFieldHostAddress()) = Event::STATE_SIGNALED;
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));
    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->getInOrderExecEventHelper().isDataAssigned());

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->reset());
    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());
}

HWTEST_F(InOrderCmdListTests, givenInOrderModeWheUsingRegularEventThenSetInOrderParamsOnlyWhenChainingIsRequired) {
    uint32_t counterOffset = 64;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    cmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_FALSE(events[0]->isCounterBased());

    if (cmdList->isInOrderNonWalkerSignalingRequired(events[0].get())) {
        EXPECT_EQ(events[0]->getInOrderExecBaseSignalValue(), 1u);
        EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());
        EXPECT_EQ(events[0]->getInOrderAllocationOffset(), counterOffset);
    } else {
        EXPECT_EQ(events[0]->getInOrderExecBaseSignalValue(), 0u);
        EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());
        EXPECT_EQ(events[0]->getInOrderAllocationOffset(), 0u);
    }

    auto copyCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(true);

    uint32_t copyData = 0;
    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    copyCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, events[0]->toHandle(), 0, nullptr, copyParams);

    EXPECT_FALSE(events[0]->isCounterBased());
    EXPECT_EQ(events[0]->getInOrderExecBaseSignalValue(), 0u);
    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_EQ(events[0]->getInOrderAllocationOffset(), 0u);

    context->freeMem(deviceAlloc);
}

HWTEST_F(InOrderCmdListTests, givenInOrderModeWheUsingRegularEventAndImmediateCmdListThenSetInOrderParams) {
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    if (cmdList->isInOrderNonWalkerSignalingRequired(events[0].get()) || cmdList->duplicatedInOrderCounterStorageEnabled) {
        EXPECT_EQ(events[0]->getInOrderExecBaseSignalValue(), 1u);
        EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    } else {
        EXPECT_LE(events[0]->getInOrderExecBaseSignalValue(), 1u);
    }

    auto tsEventPool = createEvents<FamilyType>(1, true);
    events[1]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());
    CmdListWaitEventParameters waitEventsParameters = {
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
    cmdList->appendBarrier(events[1]->toHandle(), 0, nullptr, waitEventsParameters, signalEventParameters);
    EXPECT_LE(events[1]->getInOrderExecBaseSignalValue(), 2u);
}

HWTEST_F(InOrderCmdListTests, givenRegularEventWithInOrderExecInfoWhenReusedOnRegularCmdListThenUnsetInOrderData) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    auto nonWalkerSignallingSupported = immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get());
    if (!nonWalkerSignallingSupported) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(immCmdList->isInOrderExecutionEnabled());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->getInOrderExecEventHelper().isDataAssigned());

    immCmdList->inOrderExecInfo.reset();
    EXPECT_FALSE(immCmdList->isInOrderExecutionEnabled());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());
}

HWTEST_F(InOrderCmdListTests, givenDebugFlagSetAndSingleTileCmdListWhenAskingForAtomicSignallingThenReturnTrue) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto &compilerProductHelper = device->getNEODevice()->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);

    if (heaplessEnabled) {
        EXPECT_TRUE(immCmdList->inOrderAtomicSignalingEnabled);
    } else {
        EXPECT_FALSE(immCmdList->inOrderAtomicSignalingEnabled);
    }

    EXPECT_EQ(1u, immCmdList->getInOrderIncrementValue());

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    EXPECT_TRUE(immCmdList2->inOrderAtomicSignalingEnabled);
    EXPECT_EQ(1u, immCmdList2->getInOrderIncrementValue());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenSubmittingThenProgramSemaphoreForPreviousDispatch) {
    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    const bool counterSignalPending = immCmdList->isInOrderCounterSignalPending();

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    if (counterSignalPending) {
        auto barrierItor = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), barrierItor);
    } else {
        auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        if (NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(immCmdList->isQwordInOrderCounter(), device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport)) {
            std::advance(itor, -2); // verify 2x LRI before semaphore
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, immCmdList->isQwordInOrderCounter(), false));
    }

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenResolveDependenciesViaPipeControlsForInOrderModeWhenSubmittingThenProgramPipeControlInBetweenDispatches) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCbEventWithProfilingWhenSubmittingThenProgramPipeControlInBetweenDispatches) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(-1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();
    immCmdList->latestOperationHasCbEventWithProfiling = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderCmdListWhenSubmittingThenProgramPipeControlInBetweenDispatches) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(-1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();
    immCmdList->latestOperationHasCbEventWithProfiling = false;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenMultiplePartitionsWhenResolvingInOrderDependencyThenUseSemaphoreInsteadOfPipeControl) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(-1);

    uint32_t counterOffset = 64;

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);
    regularCmdList->partitionCount = 2;

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto expectedWaitValue = regularCmdList->inOrderExecInfo->getCounterValue();
    auto offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    if (NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(regularCmdList->isQwordInOrderCounter(), device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport)) {
        std::advance(itor, -2); // verify 2x LRI before semaphore
    }

    EXPECT_TRUE(verifyInOrderDependency<FamilyType>(itor, expectedWaitValue, regularCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, regularCmdList->isQwordInOrderCounter(), false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInterleavedCsrSubmissionWhenResolvingInOrderDependencyThenUseSemaphoreInsteadOfPipeControl) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(-1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));

    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_FALSE(immCmdList->isInOrderCounterSignalPending());

    auto offset = cmdStream->getUsed();

    ultCsr->taskCount = immCmdList->cmdQImmediate->getTaskCount() + 1;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto semaphoreItor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), semaphoreItor);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCbEventWithProfilingAndInterleavedCsrSubmissionWhenResolvingInOrderDependencyThenStillUsePipeControl) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(-1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    if (immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));

    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_FALSE(immCmdList->isInOrderCounterSignalPending());

    auto offset = cmdStream->getUsed();

    ultCsr->taskCount = immCmdList->cmdQImmediate->getTaskCount() + 1;

    immCmdList->latestOperationHasCbEventWithProfiling = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto barrierItor = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), barrierItor);

    auto semaphoreItor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), semaphoreItor);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenPostSyncSkippedOnPreviousOpWhenTransitioningToMultiClientThenResolveDependencyViaBarrierInsteadOfSemaphore) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableWalkerPostSyncSkip.set(1);
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(0);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);
    ASSERT_TRUE(immCmdList->isWalkerPostSyncSkipEnabled);

    if (!immCmdList->dcFlushSupport) {
        GTEST_SKIP();
    }

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());
    ASSERT_TRUE(immCmdList->isInOrderCounterSignalPending());

    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);
    ASSERT_GE(ultCsr->getNumClients(), 2u);

    ultCsr->taskCount = immCmdList->cmdQImmediate->getTaskCount() + 1;

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto barrierItor = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), barrierItor);
    auto semaphoreItor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), semaphoreItor);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWTEST_F(InOrderCmdListTests, givenDependencyFromDifferentRootDeviceWhenAppendCalledThenCreatePeerAllocation) {
    NEO::UltDeviceFactory deviceFactory{2, 0};

    NEO::DeviceVector devices;
    for (auto &dev : deviceFactory.rootDevices) {
        devices.push_back(std::unique_ptr<NEO::Device>(dev));
    }
    auto driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
    driverHandle->initialize(std::move(devices));
    setupFabricDriverModels(*driverHandle);
    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];

    auto ultCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(device1->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr1->storeMakeResidentAllocations = true;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE;
    ze_event_handle_t eventH = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device0, &counterBasedDesc, &eventH));

    auto createCmdList = [&](L0::Device *inputDevice) {
        auto cmdList = makeZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();

        auto csr = inputDevice->getNEODevice()->getDefaultEngine().commandStreamReceiver;

        ze_command_queue_desc_t desc = {};
        desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

        mockCmdQs.emplace_back(std::make_unique<Mock<CommandQueue>>(inputDevice, csr, &desc));

        cmdList->cmdQImmediate = mockCmdQs[createdCmdLists].get();
        cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
        cmdList->initialize(inputDevice, NEO::EngineGroupType::renderCompute, 0u);
        cmdList->commandContainer.setImmediateCmdListCsr(csr);
        cmdList->enableInOrderExecution();
        uint64_t *hostAddress = ptrOffset(cmdList->inOrderExecInfo->getBaseHostAddress(0), cmdList->inOrderExecInfo->getAllocationOffset());
        for (uint32_t i = 0; i < cmdList->inOrderExecInfo->getNumHostPartitionsToWait(); i++) {
            *hostAddress = std::numeric_limits<uint64_t>::max();
            hostAddress = ptrOffset(hostAddress, device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
        }

        createdCmdLists++;

        return cmdList;
    };

    auto immCmdList0 = createCmdList(device0);
    auto immCmdList1 = createCmdList(device1);

    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, eventH, 0, nullptr, launchParams);

    auto &cmdContainer1 = immCmdList1->getCmdContainer();
    auto cmdStream = cmdContainer1.getCommandStream();
    auto offset = cmdStream->getUsed();

    zeCommandListAppendWaitOnEvents(immCmdList1->toHandle(), 1, &eventH);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    if (NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(immCmdList0->isQwordInOrderCounter(), device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport)) {
        std::advance(itor, -2); // verify 2x LRI before semaphore
    }

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, immCmdList0->inOrderExecInfo->getBaseDeviceAddress(), immCmdList0->isQwordInOrderCounter(), false));

    EXPECT_EQ(0u, ultCsr1->makeResidentAllocations[immCmdList0->inOrderExecInfo->getDeviceCounterAllocation()]);

    auto peerData = static_cast<L0::Device *>(device1)->peerCounterAllocations.get(reinterpret_cast<void *>(immCmdList0->inOrderExecInfo->getBaseDeviceAddress()));
    ASSERT_NE(nullptr, peerData);

    auto peerAlloc = peerData->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(immCmdList0->inOrderExecInfo->getDeviceCounterAllocation(), peerAlloc);

    EXPECT_EQ(1u, ultCsr1->makeResidentAllocations[peerAlloc]);

    zeEventDestroy(eventH);
}

HWTEST_F(InOrderCmdListTests, givenTimestmapEventWhenProgrammingBarrierThenDontAddPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->signalScope = 0;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    CmdListWaitEventParameters waitEventsParameters = {
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

    immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParameters, signalEventParameters);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(InOrderCmdListTests, givenDebugFlagSetWhenDispatchingStoreDataImmThenProgramUserInterrupt) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    debugManager.flags.ProgramUserInterruptOnResolvedDependency.set(1);

    auto eventPool = createEvents<FamilyType>(2, false);
    auto eventHandle = events[0]->toHandle();
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    EXPECT_FALSE(events[1]->isKmdWaitModeEnabled());
    EXPECT_FALSE(events[1]->isInterruptModeEnabled());

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    auto offset = cmdStream->getUsed();

    auto validateInterrupt = [&](bool interruptExpected) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), offset),
            cmdStream->getUsed() - offset));

        auto itor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

        if (itor != cmdList.end()) {
            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
            ASSERT_NE(nullptr, sdiCmd);

            if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(immCmdList->inOrderExecInfo->getBaseHostAddress(0)), sdiCmd->getAddress());
            } else {
                EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
            }

            auto userInterruptCmd = genCmdCast<MI_USER_INTERRUPT *>(*(++itor));
            ASSERT_EQ(interruptExpected, nullptr != userInterruptCmd);
        } else {
            EXPECT_NE(nullptr, findInOrderCounterSignalPipeControl<FamilyType>(cmdList, immCmdList->inOrderExecInfo->getBaseDeviceAddress()));
        }

        auto allCmds = findAll<MI_USER_INTERRUPT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(interruptExpected ? 1u : 0u, allCmds.size());
    };

    // no signal Event
    CmdListWaitEventParameters waitEventsParameters = {
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
    immCmdList->appendBarrier(nullptr, 1, &eventHandle, waitEventsParameters, signalEventParameters);
    validateInterrupt(false);

    // regular signal Event
    offset = cmdStream->getUsed();
    immCmdList->appendBarrier(events[1]->toHandle(), 1, &eventHandle, waitEventsParameters, signalEventParameters);
    validateInterrupt(false);

    // signal Event with kmd wait mode
    offset = cmdStream->getUsed();
    events[1]->enableInterruptMode();
    immCmdList->appendBarrier(events[1]->toHandle(), 1, &eventHandle, waitEventsParameters, signalEventParameters);
    validateInterrupt(true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenWaitingForEventFromPreviousAppendThenSkip) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    if (immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get())) {
        EXPECT_EQ(cmdList.end(), itor); // already waited on previous call
    } else {
        ASSERT_NE(cmdList.end(), itor); // implicit dependency

        itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

        EXPECT_EQ(cmdList.end(), itor);
    }
}

HWTEST_F(InOrderCmdListTests, givenInOrderModeWhenWaitingForEventFromPreviousAppendOnRegularCmdListThenSkip) {

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    if (regularCmdList->isInOrderNonWalkerSignalingRequired(events[0].get())) {
        EXPECT_EQ(cmdList.end(), itor); // already waited on previous call
    } else {
        ASSERT_NE(cmdList.end(), itor); // implicit dependency

        itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

        EXPECT_EQ(cmdList.end(), itor);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenWaitingForRegularEventFromPreviousAppendThenSkip) {
    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    immCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, eventHandle, 0, nullptr, copyParams);

    auto offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, nullptr, 1, &eventHandle, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor); // implicit dependency

    itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);

    context->freeMem(deviceAlloc);
}

HWTEST_F(InOrderCmdListTests, givenInOrderCmdListWhenWaitingOnHostThenDontProgramSemaphoreAfterWait) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        auto hostAddress = immCmdList->inOrderExecInfo->getBaseHostAddress(0);
        *hostAddress = 3;
    } else {
        auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
        *hostAddress = 3;
    }

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    immCmdList->hostSynchronize(1, false);

    auto offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderEventModeWhenSubmittingThenProgramSemaphoreOnlyForExternalEvent) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint32_t counterOffset = 64;
    uint32_t counterOffset2 = counterOffset + 32;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);
    immCmdList2->inOrderExecInfo->setAllocationOffset(counterOffset2);

    auto eventPool = createEvents<FamilyType>(2, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();
    auto event1Handle = events[1]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, event1Handle, 0, nullptr, launchParams);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    ze_event_handle_t waitlist[] = {event0Handle, event1Handle};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 2, waitlist, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor);

    itor++; // skip implicit dependency

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, immCmdList2->inOrderExecInfo->getBaseDeviceAddress() + counterOffset2, immCmdList->isQwordInOrderCounter(), false));

    itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderCmdListTests, givenImplicitEventConvertionEnabledWhenUsingImmediateCmdListThenConvertEventToCounterBased) {
    debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.set(0);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto outOfOrderImmCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    outOfOrderImmCmdList->inOrderExecInfo.reset();

    auto eventPool = createEvents<FamilyType>(3, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[1]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[2]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_FALSE(events[0]->isCounterBased());

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[1]->counterBasedMode);
    EXPECT_EQ(0u, events[1]->counterBasedFlags);
    EXPECT_FALSE(events[1]->isCounterBased());

    debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.set(-1);

    bool promotionDisabled = !immCmdList->implicitCounterBasedEventPromotionAllowed;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
        EXPECT_EQ(0u, events[0]->counterBasedFlags);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
        EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE), events[0]->counterBasedFlags);
    }
    EXPECT_NE(promotionDisabled, events[0]->isCounterBased());

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[1]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[1]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[1]->counterBasedFlags);
    EXPECT_FALSE(events[1]->isCounterBased());

    outOfOrderImmCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[2]->toHandle(), 0, nullptr, launchParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[2]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[2]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[2]->counterBasedFlags);
    EXPECT_FALSE(events[2]->isCounterBased());

    // Reuse on Regular = disable
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_FALSE(events[0]->isCounterBased());

    // Reuse on non-inOrder = disable
    events[0]->counterBasedMode = Event::CounterBasedMode::implicitlyEnabled;
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_EQ(promotionDisabled, events[0]->isCounterBased());

    // Reuse on already disabled
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_EQ(promotionDisabled, events[0]->isCounterBased());

    // On explicitly enabled
    events[0]->counterBasedMode = Event::CounterBasedMode::explicitlyEnabled;
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(Event::CounterBasedMode::explicitlyEnabled, events[0]->counterBasedMode);
    EXPECT_TRUE(events[0]->isCounterBased());
}

HWTEST_F(InOrderCmdListTests, givenWaitEventWhenUsedOnRegularCmdListThenDisableImplicitConversion) {
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_FALSE(events[0]->isCounterBased());
}

HWTEST_F(InOrderCmdListTests, givenEventUsedOnImmediateThenSynchronizedAndResetWhenUsedAsWaitOnRegularBarrierThenAppendSucceeds) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(2, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[1]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto eventHandle = events[0]->toHandle();
    CmdListWaitEventParameters waitEventsParameters = {
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
    auto result = immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParameters, signalEventParameters);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (events[0]->isCounterBased()) {
        completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
    } else {
        *reinterpret_cast<uint64_t *>(events[0]->getCompletionFieldHostAddress()) = Event::STATE_SIGNALED;
    }

    result = events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = events[0]->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEventHandle = events[1]->toHandle();
    ze_event_handle_t waitEventHandle = events[0]->toHandle();
    result = regularCmdList->appendBarrier(signalEventHandle, 1, &waitEventHandle, waitEventsParameters, signalEventParameters);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(InOrderCmdListTests, givenImplicitEventConvertionEnabledWhenUsingAppendResetThenImplicitlyDisable) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[0]->enableCounterBasedMode(false, eventPool->getCounterBasedFlags());

    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());
}

HWTEST_F(InOrderCmdListTests, givenImplicitEventConvertionEnabledWhenCallingAppendThenHandleInOrderExecInfo) {
    debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[0]->enableCounterBasedMode(false, eventPool->getCounterBasedFlags());

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams));

    EXPECT_EQ(1u, events[0]->getInOrderExecBaseSignalValue());
    EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());

    events[0]->reset();
    EXPECT_EQ(0u, events[0]->getInOrderExecBaseSignalValue());
    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams));

    EXPECT_GE(events[0]->getInOrderExecBaseSignalValue(), 1u);
    EXPECT_LE(events[0]->getInOrderExecBaseSignalValue(), 2u);
    EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_EQ(0u, events[0]->getInOrderExecBaseSignalValue());
    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCmdsChainingWhenDispatchingKernelThenProgramSemaphoreOnce) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    if (immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    uint32_t expectedCounterValue = 0;
    auto expectCounterUpdate = [&]() {
        expectedCounterValue++;
        EXPECT_EQ(expectedCounterValue, immCmdList->inOrderExecInfo->getCounterValue());
    };

    auto offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    uint32_t copyData = 0;

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16 * MemoryConstants::kiloByte, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto findSemaphores = [&](size_t expectedNumSemaphores) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        auto semaphoresDiff = (cmds.size() > expectedNumSemaphores) ? (cmds.size() - expectedNumSemaphores) : (expectedNumSemaphores - cmds.size());
        EXPECT_LE(semaphoresDiff, 1u);
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(1); // chaining
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    findSemaphores(0); // no implicit dependency semaphore
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);
    findSemaphores(0); // no implicit dependency
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, copyParams);
    findSemaphores(0); // no implicit dependency
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, nullptr, 0, nullptr, copyParams);
    findSemaphores(0); // no implicit dependency
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), nullptr, 0, nullptr, false);
    findSemaphores(0); // no implicit dependency
    expectCounterUpdate();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    expectCounterUpdate();

    offset = cmdStream->getUsed();

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    findSemaphores(0); // no implicit dependency
    expectCounterUpdate();

    context->freeMem(alloc);
}

HWTEST_F(InOrderCmdListTests, givenDebugFlagSetWhenAppendCalledThenDontProgramImplicitSemaphores) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    NEO::debugManager.flags.SkipImplicitInOrderDependencies.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto cmdList = HardwareParse::parseCommandBuffer<FamilyType>(*cmdStream, offset);

    auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, cmds.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenImmediateCmdListWhenDispatchingWithRegularEventThenSwitchToCounterBased) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto copyOnlyCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    uint32_t copyData[64] = {};

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16 * MemoryConstants::kiloByte, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    bool promotionDisabled = !immCmdList->implicitCounterBasedEventPromotionAllowed;

    NEO::MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                               reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                               MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    AlignedAllocationData allocationData = {nullptr, mockAllocation.gpuAddress, 0, &mockAllocation, false};

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, cooperativeParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), eventHandle, 0, nullptr, false);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    size_t rangeSizes = 1;
    const void **ranges = reinterpret_cast<const void **>(&copyData[0]);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, eventHandle, 0, nullptr, waitEventsParameters);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    copyOnlyCmdList->appendMemoryCopyBlitRegion(&allocationData, &allocationData, region, region, {0, 0, 0}, 0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}, events[0].get(), 0, nullptr, copyParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }
    if (events[0]->inOrderExecHelper.hasTimestampNodes()) {
        copyOnlyCmdList->inOrderExecInfo->pushTempTimestampNode(events[0]->inOrderExecHelper.getTimestampNode(0), events[0]->inOrderExecHelper.getEventData()->counterValue, 0);
    }
    static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->inOrderExecHelper).timestampNodes.clear();
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, copyParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, eventHandle, 0, nullptr, copyParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    copyOnlyCmdList->appendBlitFill(alloc, &copyData, 1, 16, events[0].get(), 0, nullptr, copyParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList->appendSignalEvent(eventHandle, signalEventParameters);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    CmdListWaitEventParameters waitEventsParametersForGlobalTs = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(copyData), eventHandle, 0, nullptr, waitEventsParametersForGlobalTs);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), copyData, 1, eventHandle, false);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {

        auto hostAddress = immCmdList->inOrderExecInfo->getBaseHostAddress(0);
        *hostAddress = immCmdList->inOrderExecInfo->getCounterValue();
    } else {
        auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
        *hostAddress = immCmdList->inOrderExecInfo->getCounterValue();
    }

    immCmdList->copyThroughLockedPtrEnabled = true;
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendMemoryCopy(alloc, &copyData, 1, eventHandle, 0, nullptr, copyParams);
    if (promotionDisabled) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    context->freeMem(alloc);
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedEventWithIncorrectFlagsWhenPassingAsSignalEventThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();

    events[0]->counterBasedFlags = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));

    events[0]->counterBasedFlags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));

    events[0]->counterBasedFlags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));

    events[0]->counterBasedFlags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenNonInOrderCmdListWhenPassingCounterBasedEventThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo.reset();
    EXPECT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto copyOnlyCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();
    copyOnlyCmdList->inOrderExecInfo.reset();
    EXPECT_FALSE(copyOnlyCmdList->isInOrderExecutionEnabled());

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    uint32_t copyData[64] = {};

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16 * MemoryConstants::kiloByte, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    NEO::MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                               reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                               MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    AlignedAllocationData allocationData = {nullptr, mockAllocation.gpuAddress, 0, &mockAllocation, false};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, cooperativeParams));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), eventHandle, 0, nullptr, false));

    size_t rangeSizes = 1;
    const void **ranges = reinterpret_cast<const void **>(&copyData[0]);
    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, eventHandle, 0, nullptr, waitEventsParameters));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, copyOnlyCmdList->appendMemoryCopyBlitRegion(&allocationData, &allocationData, region, region, {0, 0, 0}, 0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}, events[0].get(), 0, nullptr, copyParams));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, copyParams));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, eventHandle, 0, nullptr, copyParams));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, copyOnlyCmdList->appendBlitFill(alloc, &copyData, 1, 16, events[0].get(), 0, nullptr, copyParams));

    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendSignalEvent(eventHandle, signalEventParameters));
    CmdListWaitEventParameters waitEventsParametersForGlobalTs = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(copyData), eventHandle, 0, nullptr, waitEventsParametersForGlobalTs));
    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters));

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), copyData, 1, eventHandle, false));

    immCmdList->copyThroughLockedPtrEnabled = true;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryCopy(alloc, &copyData, 1, eventHandle, 0, nullptr, copyParams));

    {
        auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
        ze_image_region_t imgRegion = {1, 1, 1, 1, 1, 1};
        ze_image_desc_t zeDesc = {};
        zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
        zeDesc.type = ZE_IMAGE_TYPE_3D;
        zeDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
        zeDesc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
        zeDesc.width = 11;
        zeDesc.height = 13;
        zeDesc.depth = 17;
        zeDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
        zeDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
        zeDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
        zeDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;
        image->initialize(device, &zeDesc);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, copyOnlyCmdList->appendImageCopyFromMemoryExt(image->toHandle(), &copyData, &imgRegion, 0, 0, eventHandle, 0, nullptr, copyParams));
    }

    context->freeMem(alloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenNonInOrderCmdListWithCounterBasedEventWhenAppendMemoryCopyThenReturnInvalidArgument) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo.reset();
    EXPECT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    void *deviceAlloc = nullptr;
    void *hostAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, 256u, 256u, &deviceAlloc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 256u, 0u, &hostAlloc));

    CmdListMemoryCopyParams copyParams = {};
    immCmdList->copyThroughLockedPtrEnabled = true;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryCopy(deviceAlloc, hostAlloc, 256u, eventHandle, 0, nullptr, copyParams));

    context->freeMem(deviceAlloc);
    context->freeMem(hostAlloc);
}

HWTEST_F(InOrderCmdListTests, givenEmptyCounterBasedEventWhenPassedAsWaitEventToImmediateCmdListThenSkipWait) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());

    auto eventHandle = events[0]->toHandle();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    const auto offset = cmdStream->getUsed();

    CmdListWaitEventParameters waitEventsParameters{
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = false,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
        .apiRequest = true};

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendWaitOnEvents(1, &eventHandle, waitEventsParameters));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    EXPECT_TRUE(findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end()).empty());
}

} // namespace ult
} // namespace L0
