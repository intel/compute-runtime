/*
 * Copyright (C) 2024-2025 Intel Corporation
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
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/driver_experimental/zex_api.h"

#include <type_traits>
#include <variant>

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
}

namespace L0 {
namespace ult {

using InOrderCmdListExtensionsTests = Test<ExtensionFixture>;

HWTEST_F(InOrderCmdListExtensionsTests, givenDriverHandleWhenAskingForExtensionsThenReturnCorrectVersions) {
    verifyExtensionDefinition(ZE_EVENT_POOL_COUNTER_BASED_EXP_NAME, ZE_EVENT_POOL_COUNTER_BASED_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZEX_COUNTER_BASED_EVENT_EXT_NAME, ZEX_COUNTER_BASED_EVENT_VERSION_1_0);
    verifyExtensionDefinition(ZE_INTEL_COMMAND_LIST_MEMORY_SYNC, ZE_INTEL_COMMAND_LIST_MEMORY_SYNC_EXP_VERSION_CURRENT);
}

using InOrderCmdListTests = InOrderCmdListFixture;

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

    zex_intel_event_sync_mode_exp_desc_t syncModeDesc = {ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC};
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
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;
    auto event1 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_TRUE(event1->isInterruptModeEnabled());
    EXPECT_FALSE(event1->isKmdWaitModeEnabled());

    eventDesc.index = 2;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT;
    auto event2 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_FALSE(event2->isInterruptModeEnabled());
    EXPECT_FALSE(event2->isKmdWaitModeEnabled());

    eventDesc.index = 3;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT | ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT;
    auto event3 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_TRUE(event3->isInterruptModeEnabled());
    EXPECT_TRUE(event3->isKmdWaitModeEnabled());

    eventDesc.index = 4;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;
    syncModeDesc.externalInterruptId = 123;
    auto event4 = DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));
    EXPECT_EQ(NEO::InterruptId::notUsed, event4->externalInterruptId);

    eventDesc.index = 5;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_EXTERNAL_INTERRUPT_WAIT;
    EXPECT_ANY_THROW(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    mockProductHelper->isInterruptSupportedResult = false;
    eventDesc.index = 6;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;
    auto event5 = Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue);
    EXPECT_EQ(event5, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
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

HWTEST_F(InOrderCmdListTests, givenNotSignaledInOrderEventWhenAddedToWaitListThenReturnError) {
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

    returnValue = immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &handle, launchParams);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
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

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->queryStatus());
}

HWTEST_F(InOrderCmdListTests, givenCmdListsWhenDispatchingThenUseInternalTaskCountForWaits) {
    auto immCmdList0 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    bool heapless = ultCsr->heaplessStateInitialized;
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
        auto hostAddress0 = static_cast<uint64_t *>(immCmdList0->inOrderExecInfo->getBaseHostAddress());
        auto hostAddress1 = static_cast<uint64_t *>(immCmdList1->inOrderExecInfo->getBaseHostAddress());

        *hostAddress0 = 1;
        *hostAddress1 = 1;
        CmdListMemoryCopyParams copyParams = {};
        immCmdList0->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, copyParams);

        auto expectedLatestTaskCount = immCmdList0->dcFlushSupport || (!heapless && immCmdList0->latestOperationHasOptimizedCbEvent) ? 1u : 2u;
        expectedLatestTaskCount += (heapless ? 1u : 0u);
        EXPECT_EQ(expectedLatestTaskCount, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(immCmdList0->dcFlushSupport || (!heapless && immCmdList0->latestOperationHasOptimizedCbEvent) ? 3u : 2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

        immCmdList1->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, copyParams);

        expectedLatestTaskCount = 2u;
        expectedLatestTaskCount += (heapless ? 1u : 0u);
        EXPECT_EQ(expectedLatestTaskCount, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(immCmdList0->dcFlushSupport || (!heapless && immCmdList0->latestOperationHasOptimizedCbEvent) ? 4u : 2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

        context->freeMem(deviceAlloc);
    }

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList0.get());
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList1.get());
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedEventsWhenHostWaitsAreCalledThenLatestWaitIsRecorded) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(2, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    auto inOrderExecInfo = events[1]->getInOrderExecInfo();
    *inOrderExecInfo->getBaseHostAddress() = 2u;

    auto status = events[1]->hostSynchronize(-1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);

    auto counterValue = events[1]->inOrderExecSignalValue;
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(counterValue));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(events[0]->inOrderExecSignalValue));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(counterValue + 1));

    // setting lower counter ignored
    inOrderExecInfo->setLastWaitedCounterValue(counterValue - 1);
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(counterValue));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(events[0]->inOrderExecSignalValue));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(counterValue + 1));

    status = events[0]->hostSynchronize(-1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(counterValue));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(counterValue + 1));

    // setting offset disables mechanism
    inOrderExecInfo->setAllocationOffset(4u);
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(0u));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(counterValue));

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
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

    events[0]->inOrderAllocationOffset = 123;

    uint64_t hostAddress = 0;
    if (events[0]->inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = castToUint64(ptrOffset(events[0]->inOrderExecInfo->getBaseHostAddress(), events[0]->inOrderAllocationOffset));
    } else {
        hostAddress = castToUint64(ptrOffset(events[0]->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer(), events[0]->inOrderAllocationOffset));
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->isUserFenceWaitSupported = true;
    ultCsr->waitUserFenceParams.forceRetStatusEnabled = true;
    ultCsr->waitUserFenceParams.forceRetStatusValue = false;
    EXPECT_EQ(0u, ultCsr->waitUserFenceParams.callCount);

    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(2));

    EXPECT_EQ(1u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(hostAddress, ultCsr->waitUserFenceParams.latestWaitedAddress);
    EXPECT_EQ(events[0]->inOrderExecSignalValue, ultCsr->waitUserFenceParams.latestWaitedValue);
    EXPECT_EQ(2, ultCsr->waitUserFenceParams.latestWaitedTimeout);

    ultCsr->waitUserFenceParams.forceRetStatusValue = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(3));

    EXPECT_EQ(2u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(hostAddress, ultCsr->waitUserFenceParams.latestWaitedAddress);
    EXPECT_EQ(events[0]->inOrderExecSignalValue, ultCsr->waitUserFenceParams.latestWaitedValue);
    EXPECT_EQ(3, ultCsr->waitUserFenceParams.latestWaitedTimeout);

    // already completed
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(3));
    EXPECT_EQ(2u, ultCsr->waitUserFenceParams.callCount);

    // non in-order event
    events[1]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[1]->hostSynchronize(2);
    EXPECT_EQ(2u, ultCsr->waitUserFenceParams.callCount);
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

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    regularCmdList->appendQueryKernelTimestamps(2, queryEvents, deviceMem, nullptr, nullptr, 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(chainingRequired ? 1u : 2u, semaphores.size());

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphores[0]);

    EXPECT_EQ(events[1]->getCompletionFieldGpuAddress(device), semaphoreCmd->getSemaphoreGraphicsAddress());

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

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCounterBasedTimestampEventWhenQueryingTimestampThenEnsureItsCompletion) {
    struct MyMockEvent : public L0::EventImp<uint64_t> {
        using BaseClass = L0::EventImp<uint64_t>;

        MyMockEvent(L0::EventPool *pool, L0::Device *device) : BaseClass::EventImp(0, device, false) {
            this->eventPool = pool;

            this->eventPoolAllocation = &pool->getAllocation();

            this->totalEventSize = 128;
            hostAddressFromPool = eventPoolAllocation->getGraphicsAllocation(0)->getUnderlyingBuffer();
            this->csrs[0] = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

            this->maxKernelCount = 1;
            this->maxPacketCount = 1;

            this->kernelEventCompletionData = std::make_unique<KernelEventCompletionData<uint64_t>[]>(1);
        }

        uint32_t assignKernelEventCompletionDataCalled = 0;
        uint32_t assignKernelEventCompletionDataFailCounter = 0;
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
        uint64_t getCompletionTimeout() const override { return std::numeric_limits<uint64_t>::max(); } // no timeout to avoid timing issues
    };

    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto event1 = std::make_unique<MyMockEvent>(eventPool.get(), device);
    auto event2 = std::make_unique<MyMockEvent>(eventPool.get(), device);
    auto event3 = std::make_unique<MyMockEvent>(eventPool.get(), device);

    event1->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event1->assignKernelEventCompletionDataFailCounter = 2;
    event1->setUsingContextEndOffset(true);
    event1->setEventTimestampFlag(true);
    event1->useContextEndForVerification = true;

    event2->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event2->assignKernelEventCompletionDataFailCounter = 2;
    event2->setUsingContextEndOffset(true);
    event2->setEventTimestampFlag(true);
    event2->useContextEndForVerification = false;

    event3->disableImplicitCounterBasedMode();
    event3->assignKernelEventCompletionDataFailCounter = 2;
    event3->setUsingContextEndOffset(true);
    event3->setEventTimestampFlag(true);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event1->toHandle(), 0, nullptr, launchParams);
    event1->hostEventSetValue(Event::STATE_CLEARED);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event2->toHandle(), 0, nullptr, launchParams);
    event2->hostEventSetValue(Event::STATE_CLEARED);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event3->toHandle(), 0, nullptr, launchParams);
    event3->hostEventSetValue(Event::STATE_CLEARED);

    event1->getInOrderExecInfo()->setLastWaitedCounterValue(2);
    event2->getInOrderExecInfo()->setLastWaitedCounterValue(2);
    event3->getInOrderExecInfo()->setLastWaitedCounterValue(3);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event1->queryStatus());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event2->queryStatus());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event3->queryStatus());

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

    firstQueue->executeCommandLists(1, &cmdlistHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(firstQueue->getCsr(), events[0]->csrs[0]);
    EXPECT_EQ(1u, events[1]->csrs.size());
    EXPECT_EQ(firstQueue->getCsr(), events[1]->csrs[0]);
    EXPECT_EQ(1u, events[2]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[2]->csrs[0]);

    secondQueue->executeCommandLists(1, &cmdlistHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(secondQueue->getCsr(), events[0]->csrs[0]);
    EXPECT_EQ(1u, events[1]->csrs.size());
    EXPECT_EQ(secondQueue->getCsr(), events[1]->csrs[0]);
    EXPECT_EQ(1u, events[2]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[2]->csrs[0]);

    cmdList->reset();
    EXPECT_EQ(0u, cmdList->interruptEvents.size());
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

    EXPECT_FALSE(ultCsr->waitUserFenceSupported());

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
    EXPECT_EQ(events[0]->getInOrderExecInfo()->getDeviceCounterAllocation(), ultCsr->waitUserFenceParams.latestAllocForInterruptWait);
    EXPECT_TRUE(ultCsr->waitUserFenceParams.userInterrupt);

    // Duplicated host storage
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[1]->hostSynchronize(2));

    EXPECT_EQ(2u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(events[1]->getInOrderExecInfo()->getHostCounterAllocation(), ultCsr->waitUserFenceParams.latestAllocForInterruptWait);
    EXPECT_TRUE(ultCsr->waitUserFenceParams.userInterrupt);

    // External host storage
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));
    *hostAddress = 0;

    uint64_t *gpuAddress = ptrOffset(hostAddress, 0x100);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, 1, &eventDesc, &handle));

    auto event2 = L0::Event::fromHandle(handle);
    event2->enableKmdWaitMode();
    event2->enableInterruptMode();

    event2->hostSynchronize(2);

    EXPECT_EQ(3u, ultCsr->waitUserFenceParams.callCount);
    EXPECT_EQ(event2->getInOrderExecInfo()->getExternalHostAllocation(), ultCsr->waitUserFenceParams.latestAllocForInterruptWait);
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
    EXPECT_EQ(events[0]->inOrderExecSignalValue, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(events[0]->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList->inOrderExecInfo->getDeviceCounterAllocation());
    EXPECT_EQ(events[0]->inOrderAllocationOffset, 0u);

    events[0]->inOrderAllocationOffset = 123;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, events[0]->reset());

    EXPECT_EQ(events[0]->inOrderExecSignalValue, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(events[0]->inOrderExecInfo.get(), immCmdList->inOrderExecInfo.get());
    EXPECT_EQ(events[0]->inOrderAllocationOffset, 123u);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, events[0]->hostSignal(false));
}

HWTEST_F(InOrderCmdListTests, whenCreatingInOrderExecInfoThenReuseDeviceAlloc) {
    auto tag = device->getDeviceInOrderCounterAllocator()->getTag();

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa1 = immCmdList1->inOrderExecInfo->getBaseDeviceAddress();

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa2 = immCmdList2->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(alignUp(gpuVa1 + (device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset() * 2), MemoryConstants::cacheLineSize), gpuVa2);

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
    auto gpuVa1 = immCmdList1->inOrderExecInfo->getBaseHostAddress();

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa2 = immCmdList2->inOrderExecInfo->getBaseHostAddress();

    EXPECT_NE(gpuVa1, gpuVa2);

    // allocation from the same allocator
    EXPECT_EQ(immCmdList1->inOrderExecInfo->getHostCounterAllocation(), tag->getBaseGraphicsAllocation()->getGraphicsAllocation(0));

    immCmdList1.reset();

    auto immCmdList3 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa3 = immCmdList3->inOrderExecInfo->getBaseHostAddress();

    EXPECT_EQ(gpuVa1, gpuVa3);

    immCmdList2.reset();

    auto immCmdList4 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto gpuVa4 = immCmdList4->inOrderExecInfo->getBaseHostAddress();

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
        hostAddress = static_cast<uint64_t *>(cmdList->inOrderExecInfo->getBaseHostAddress());
    }

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    auto nonWalkerSignallingSupported = cmdList->isInOrderNonWalkerSignalingRequired(events[0].get());

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->inOrderExecInfo.get() != nullptr);

    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(1));
    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->inOrderExecInfo.get() != nullptr);

    if (nonWalkerSignallingSupported) {
        *hostAddress = 1;
    } else {
        *reinterpret_cast<uint64_t *>(events[0]->getCompletionFieldHostAddress()) = Event::STATE_SIGNALED;
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));
    EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->inOrderExecInfo.get() != nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->reset());
    EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);
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
        EXPECT_EQ(events[0]->inOrderExecSignalValue, 1u);
        EXPECT_NE(events[0]->inOrderExecInfo.get(), nullptr);
        EXPECT_EQ(events[0]->inOrderAllocationOffset, counterOffset);
    } else {
        EXPECT_EQ(events[0]->inOrderExecSignalValue, 0u);
        EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);
        EXPECT_EQ(events[0]->inOrderAllocationOffset, 0u);
    }

    auto copyCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(true);

    uint32_t copyData = 0;
    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    copyCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, events[0]->toHandle(), 0, nullptr, copyParams);

    EXPECT_FALSE(events[0]->isCounterBased());
    EXPECT_EQ(events[0]->inOrderExecSignalValue, 0u);
    EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);
    EXPECT_EQ(events[0]->inOrderAllocationOffset, 0u);

    context->freeMem(deviceAlloc);
}

HWTEST_F(InOrderCmdListTests, givenInOrderModeWheUsingRegularEventAndImmediateCmdListThenSetInOrderParams) {
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_FALSE(events[0]->isCounterBased());

    if (cmdList->isInOrderNonWalkerSignalingRequired(events[0].get()) || cmdList->duplicatedInOrderCounterStorageEnabled) {
        EXPECT_EQ(events[0]->inOrderExecSignalValue, 1u);
        EXPECT_NE(events[0]->inOrderExecInfo.get(), nullptr);
    } else {
        EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);
    }

    auto tsEventPool = createEvents<FamilyType>(1, true);
    events[1]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    cmdList->appendBarrier(events[1]->toHandle(), 0, nullptr, false);
    EXPECT_EQ(events[1]->inOrderExecInfo.get() != nullptr, cmdList->duplicatedInOrderCounterStorageEnabled);
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

    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->inOrderExecInfo.get() != nullptr);

    immCmdList->inOrderExecInfo.reset();
    EXPECT_FALSE(immCmdList->isInOrderExecutionEnabled());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());
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

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    if (immCmdList->isQwordInOrderCounter()) {
        std::advance(itor, -2); // verify 2x LRI before semaphore
    }

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, immCmdList->isQwordInOrderCounter(), false));

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

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenOptimizedCbEventWhenSubmittingThenProgramPipeControlOrSemaphoreInBetweenDispatches) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(-1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();
    immCmdList->latestOperationHasOptimizedCbEvent = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    if (immCmdList->dcFlushSupport || !immCmdList->isHeaplessModeEnabled()) {
        auto itor = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
    } else {
        auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
    }

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderCmdListWhenSubmittingThenProgramPipeControlOrSemaphoreInBetweenDispatches) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(-1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();
    immCmdList->latestOperationHasOptimizedCbEvent = false;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    if (immCmdList->dcFlushSupport) {
        auto itor = find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
    } else {
        auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
    }

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWTEST_F(InOrderCmdListTests, givenDependencyFromDifferentRootDeviceWhenAppendCalledThenCreatePeerAllocation) {
    NEO::UltDeviceFactory deviceFactory{2, 0};

    NEO::DeviceVector devices;
    for (auto &dev : deviceFactory.rootDevices) {
        devices.push_back(std::unique_ptr<NEO::Device>(dev));
    }
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];

    auto ultCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(device1->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr1->storeMakeResidentAllocations = true;

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE;
    ze_event_handle_t eventH = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device0, &counterBasedDesc, &eventH));

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
        uint64_t *hostAddress = ptrOffset(cmdList->inOrderExecInfo->getBaseHostAddress(), cmdList->inOrderExecInfo->getAllocationOffset());
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

    if (immCmdList0->isQwordInOrderCounter()) {
        std::advance(itor, -2); // verify 2x LRI before semaphore
    }

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, immCmdList0->inOrderExecInfo->getBaseDeviceAddress(), immCmdList0->isQwordInOrderCounter(), false));

    EXPECT_EQ(0u, ultCsr1->makeResidentAllocations[immCmdList0->inOrderExecInfo->getDeviceCounterAllocation()]);

    auto peerData = static_cast<L0::DeviceImp *>(device1)->peerCounterAllocations.get(reinterpret_cast<void *>(immCmdList0->inOrderExecInfo->getBaseDeviceAddress()));
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

    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

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

        ASSERT_NE(cmdList.end(), itor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
        ASSERT_NE(nullptr, sdiCmd);

        if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
            EXPECT_EQ(reinterpret_cast<uint64_t>(immCmdList->inOrderExecInfo->getBaseHostAddress()), sdiCmd->getAddress());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
        }

        auto userInterruptCmd = genCmdCast<MI_USER_INTERRUPT *>(*(++itor));
        ASSERT_EQ(interruptExpected, nullptr != userInterruptCmd);

        auto allCmds = findAll<MI_USER_INTERRUPT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(interruptExpected ? 1u : 0u, allCmds.size());
    };

    // no signal Event
    immCmdList->appendBarrier(nullptr, 1, &eventHandle, false);
    validateInterrupt(false);

    // regular signal Event
    offset = cmdStream->getUsed();
    immCmdList->appendBarrier(events[1]->toHandle(), 1, &eventHandle, false);
    validateInterrupt(false);

    // signal Event with kmd wait mode
    offset = cmdStream->getUsed();
    events[1]->enableInterruptMode();
    immCmdList->appendBarrier(events[1]->toHandle(), 1, &eventHandle, false);
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
        auto hostAddress = immCmdList->inOrderExecInfo->getBaseHostAddress();
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

    bool dcFlushRequired = immCmdList->getDcFlushRequired(true);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
        EXPECT_EQ(0u, events[0]->counterBasedFlags);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
        EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE), events[0]->counterBasedFlags);
    }
    EXPECT_NE(dcFlushRequired, events[0]->isCounterBased());

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[1]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[1]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[1]->counterBasedFlags);
    EXPECT_FALSE(events[1]->isCounterBased());

    outOfOrderImmCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[2]->toHandle(), 0, nullptr, launchParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[2]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[2]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[2]->counterBasedFlags);
    EXPECT_FALSE(events[2]->isCounterBased());

    // Reuse on Regular = disable
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_FALSE(events[0]->isCounterBased());

    // Reuse on non-inOrder = disable
    events[0]->counterBasedMode = Event::CounterBasedMode::implicitlyEnabled;
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_EQ(dcFlushRequired, events[0]->isCounterBased());

    // Reuse on already disabled
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_EQ(dcFlushRequired, events[0]->isCounterBased());

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

HWTEST_F(InOrderCmdListTests, givenImplicitEventConvertionEnabledWhenUsingAppendResetThenImplicitlyDisable) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[0]->enableCounterBasedMode(false, eventPool->getCounterBasedFlags());

    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());
}

HWTEST_F(InOrderCmdListTests, givenImplicitEventConvertionEnabledWhenCallingAppendThenHandleInOrderExecInfo) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[0]->enableCounterBasedMode(false, eventPool->getCounterBasedFlags());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());

    events[0]->reset();
    EXPECT_EQ(0u, events[0]->inOrderExecSignalValue);
    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(2u, events[0]->inOrderExecSignalValue);
    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());

    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_EQ(0u, events[0]->inOrderExecSignalValue);
    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());
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

    auto offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    uint32_t copyData = 0;

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto findSemaphores = [&](size_t expectedNumSemaphores) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(expectedNumSemaphores, cmds.size());
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(1); // chaining
    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    findSemaphores(0); // no implicit dependency semaphore
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(3u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(4u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(5u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, copyParams);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(6u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(7u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, nullptr, 0, nullptr, copyParams);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(8u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(9u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), nullptr, 0, nullptr, false);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(10u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(11u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(12u, immCmdList->inOrderExecInfo->getCounterValue());

    context->freeMem(alloc);
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
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    bool dcFlushRequired = immCmdList->getDcFlushRequired(true);

    NEO::MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                               reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                               MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    AlignedAllocationData allocationData = {mockAllocation.gpuAddress, 0, &mockAllocation, false};

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, cooperativeParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), eventHandle, 0, nullptr, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    size_t rangeSizes = 1;
    const void **ranges = reinterpret_cast<const void **>(&copyData[0]);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, eventHandle, 0, nullptr);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    copyOnlyCmdList->appendMemoryCopyBlitRegion(&allocationData, &allocationData, region, region, {0, 0, 0}, 0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}, events[0].get(), 0, nullptr, copyParams, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }
    if (!events[0]->inOrderTimestampNode.empty()) {
        copyOnlyCmdList->inOrderExecInfo->pushTempTimestampNode(events[0]->inOrderTimestampNode[0], events[0]->inOrderExecSignalValue);
    }
    events[0]->inOrderTimestampNode.clear();
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, copyParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, eventHandle, 0, nullptr, copyParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    copyOnlyCmdList->appendBlitFill(alloc, &copyData, 1, 16, events[0].get(), 0, nullptr, copyParams);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendSignalEvent(eventHandle, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(copyData), eventHandle, 0, nullptr);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), copyData, 1, eventHandle, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {

        auto hostAddress = immCmdList->inOrderExecInfo->getBaseHostAddress();
        *hostAddress = immCmdList->inOrderExecInfo->getCounterValue();
    } else {
        auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
        *hostAddress = immCmdList->inOrderExecInfo->getCounterValue();
    }

    immCmdList->copyThroughLockedPtrEnabled = true;
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendMemoryCopy(alloc, &copyData, 1, eventHandle, 0, nullptr, copyParams);
    if (dcFlushRequired) {
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
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    NEO::MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                               reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                               MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    AlignedAllocationData allocationData = {mockAllocation.gpuAddress, 0, &mockAllocation, false};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams));

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, cooperativeParams));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), eventHandle, 0, nullptr, false));

    size_t rangeSizes = 1;
    const void **ranges = reinterpret_cast<const void **>(&copyData[0]);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, copyOnlyCmdList->appendMemoryCopyBlitRegion(&allocationData, &allocationData, region, region, {0, 0, 0}, 0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}, events[0].get(), 0, nullptr, copyParams, false));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, copyParams));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, eventHandle, 0, nullptr, copyParams));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, copyOnlyCmdList->appendBlitFill(alloc, &copyData, 1, 16, events[0].get(), 0, nullptr, copyParams));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendSignalEvent(eventHandle, false));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(copyData), eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendBarrier(eventHandle, 0, nullptr, false));

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

    void *alloc = allocDeviceMem(16384u);

    auto findSemaphores = [&](size_t expectedNumSemaphores) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        EXPECT_EQ(expectedNumSemaphores, cmds.size());
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
        EXPECT_EQ(expectedNumSemaphores, cmds.size());
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

    void *alloc = allocDeviceMem(16384u);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(alloc, alloc, 1, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

    if (immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope()))) {
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
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    uint32_t copyData = 0;

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, copyParams);

    auto dcFlushRequired = immCmdList->getDcFlushRequired(true);
    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto gpuAddress = inOrderExecInfo->getBaseDeviceAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto miAtomicItor = find<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
    if (device->getProductHelper().isDcFlushAllowed() && inOrderExecInfo->isHostStorageDuplicated()) {
        EXPECT_NE(cmdList.end(), miAtomicItor);
        auto miAtomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomicItor);
        EXPECT_EQ(gpuAddress, miAtomicCmd->getMemoryAddress());
    } else {
        EXPECT_EQ(cmdList.end(), miAtomicItor);
    }

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    if (immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope()))) {
        EXPECT_NE(cmdList.end(), sdiItor);
    } else {
        if (dcFlushRequired) {
            EXPECT_NE(cmdList.end(), sdiItor);
            if (inOrderExecInfo->isHostStorageDuplicated()) {
                gpuAddress = reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress());
            }
            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
            EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
        } else {
            EXPECT_EQ(cmdList.end(), sdiItor);
        }
        auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);
        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();
        if (dcFlushRequired) {
            EXPECT_NE(gpuAddress, postSync.getDestinationAddress());
        } else {
            EXPECT_EQ(gpuAddress, postSync.getDestinationAddress());
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCmdsChainingWhenDispatchingKernelWithRelaxedOrderingThenProgramAllDependencies) {
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

HWTEST2_F(InOrderCmdListTests, givenRelaxedOrderingEnabledWhenSignalEventCalledThenPassStallingCmdsInfo, IsAtLeastXeHpcCore) {

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

    immCmdList1->appendSignalEvent(events[0]->toHandle(), true);
    verifyFlags(false, false); // no dependencies

    immCmdList2->appendSignalEvent(events[0]->toHandle(), false);
    verifyFlags(false, false); // no dependencies

    immCmdList1->appendSignalEvent(events[0]->toHandle(), true);
    verifyFlags(true, false); // relaxed ordering with implicit dependency

    immCmdList1->appendSignalEvent(nonCbEvent, true);
    verifyFlags(true, true); // relaxed ordering with implicit dependency

    immCmdList1->cmdQImmediate->unregisterCsrClient();
    immCmdList2->cmdQImmediate->unregisterCsrClient();

    immCmdList1->appendSignalEvent(events[0]->toHandle(), false);
    verifyFlags(false, true); // relaxed ordering disabled == stalling semaphore
}

HWTEST2_F(InOrderCmdListTests, givenCounterHeuristicForRelaxedOrderingEnabledWhenSmallTaskIsFlushedThenIncrementCounter, IsAtLeastXeHpcCore) {
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

HWTEST2_F(InOrderCmdListTests, givenCounterHeuristicForRelaxedOrderingEnabledWhenAppendingThenEnableRelaxedOrderingCorrectly, IsAtLeastXeHpcCore) {
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

    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristicTreshold.set(1);

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

HWTEST2_F(InOrderCmdListTests, givenRelaxedOrderingWithCounterHeuristicWhenSubmisionSplitThenDontIncrementCounterTwice, IsAtLeastXeHpcCore) {
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);
    debugManager.flags.EnableInOrderRelaxedOrderingForEventsChaining.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    if (!immCmdList->skipInOrderNonWalkerSignalingAllowed(events[0].get())) {
        GTEST_SKIP(); // not supported
    }

    EXPECT_EQ(0u, immCmdList->relaxedOrderingCounter);

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);
    EXPECT_EQ(1u, immCmdList->relaxedOrderingCounter);

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);
    EXPECT_EQ(2u, immCmdList->relaxedOrderingCounter);

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);
    EXPECT_EQ(3u, immCmdList->relaxedOrderingCounter);
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
    auto heaplessStateInit = ultCsr->heaplessStateInitialized;

    if (heaplessStateInit) {
        EXPECT_NE(nullptr, immCmdList1->inOrderExecInfo->getHostCounterAllocation());
        EXPECT_NE(nullptr, immCmdList2->inOrderExecInfo->getHostCounterAllocation());
    } else {
        EXPECT_EQ(nullptr, immCmdList1->inOrderExecInfo->getHostCounterAllocation());
        EXPECT_EQ(nullptr, immCmdList2->inOrderExecInfo->getHostCounterAllocation());
    }

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams);

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getDeviceCounterAllocation()]);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams);

    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getDeviceCounterAllocation()]);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    if (immCmdList1->isQwordInOrderCounter()) {
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

    EXPECT_NE(nullptr, immCmdList1->inOrderExecInfo->getHostCounterAllocation());
    EXPECT_NE(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList1->inOrderExecInfo->getHostCounterAllocation());
    EXPECT_NE(nullptr, immCmdList2->inOrderExecInfo->getHostCounterAllocation());
    EXPECT_NE(immCmdList2->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList2->inOrderExecInfo->getHostCounterAllocation());

    EXPECT_EQ(AllocationType::timestampPacketTagBuffer, immCmdList1->inOrderExecInfo->getHostCounterAllocation()->getAllocationType());
    EXPECT_EQ(immCmdList1->inOrderExecInfo->getBaseHostAddress(), immCmdList1->inOrderExecInfo->getHostCounterAllocation()->getUnderlyingBuffer());
    EXPECT_FALSE(immCmdList1->inOrderExecInfo->getHostCounterAllocation()->isAllocatedInLocalMemoryPool());

    EXPECT_EQ(immCmdList1->inOrderExecInfo->getHostCounterAllocation(), immCmdList2->inOrderExecInfo->getHostCounterAllocation());

    auto hostAllocOffset = ptrDiff(immCmdList2->inOrderExecInfo->getBaseHostAddress(), immCmdList1->inOrderExecInfo->getBaseHostAddress());
    EXPECT_NE(0u, hostAllocOffset);

    EXPECT_EQ(AllocationType::timestampPacketTagBuffer, immCmdList2->inOrderExecInfo->getHostCounterAllocation()->getAllocationType());
    EXPECT_EQ(immCmdList2->inOrderExecInfo->getBaseHostAddress(), ptrOffset(immCmdList2->inOrderExecInfo->getHostCounterAllocation()->getUnderlyingBuffer(), hostAllocOffset));
    EXPECT_FALSE(immCmdList2->inOrderExecInfo->getHostCounterAllocation()->isAllocatedInLocalMemoryPool());

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams);

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getHostCounterAllocation()]);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams);

    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getHostCounterAllocation()]);
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

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList->inOrderExecInfo->getDeviceCounterAllocation()]);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[immCmdList->inOrderExecInfo->getDeviceCounterAllocation()]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenAddingRelaxedOrderingEventsThenConfigureRegistersFirst) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    immCmdList->addEventsToCmdList(0, nullptr, nullptr, true, true, true, false, false);

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

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenOptimizedCbEventWhenAppendNeedsRelaxedOrderingThenSubmitInOrderCounter) {
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
    immCmdList->latestOperationHasOptimizedCbEvent = true;

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

    bool isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

        auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();

        using PostSyncType = std::decay_t<decltype(postSync)>;

        if (!immCmdList->inOrderExecInfo->isAtomicDeviceSignalling()) {
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

            uint64_t address = pcCmd->getAddressHigh();
            address <<= 32;
            address |= pcCmd->getAddress();
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, address);
            EXPECT_EQ(2u, pcCmd->getImmediateData());

            auto releaseHelper = device->getNEODevice()->getReleaseHelper();
            const bool textureFlushRequired = releaseHelper && releaseHelper->isPostImageWriteFlushRequired() &&
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
        hostAddress = ptrOffset(immCmdList->inOrderExecInfo->getBaseHostAddress(), counterOffset);
    } else {
        hostAddress = static_cast<uint64_t *>(ptrOffset(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer(), counterOffset));
    }

    *hostAddress = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(1));

    *hostAddress = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));

    *hostAddress = 3;
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

    auto isaAllocation = kernel->getIsaAllocation();
    auto isaAddress = isaAllocation->getGpuAddress() + kernel->getIsaOffsetInParentAllocation();
    auto heap = immCmdList->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject);
    auto heapAddress = heap->getGraphicsAllocation()->getGpuAddress() + heap->getUsed();

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto firstPrefetchIterator = find<STATE_PREFETCH *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), firstPrefetchIterator);

    auto heapSize = alignUp(kernel->getIndirectSize(), MemoryConstants::cacheLineSize) / MemoryConstants::cacheLineSize;

    auto firstPrefetch = genCmdCast<STATE_PREFETCH *>(*firstPrefetchIterator);
    ASSERT_NE(nullptr, firstPrefetch);
    EXPECT_EQ(heapAddress, firstPrefetch->getAddress());
    EXPECT_EQ(heapSize, firstPrefetch->getPrefetchSize());

    EXPECT_FALSE(firstPrefetch->getKernelInstructionPrefetch());

    auto secondPrefetchIterator = find<STATE_PREFETCH *>(++firstPrefetchIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), secondPrefetchIterator);

    auto secondPrefetch = genCmdCast<STATE_PREFETCH *>(*secondPrefetchIterator);
    ASSERT_NE(nullptr, secondPrefetch);

    EXPECT_TRUE(secondPrefetch->getKernelInstructionPrefetch());
    EXPECT_EQ(isaAddress, secondPrefetch->getAddress());
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenAskingIfSkipInOrderNonWalkerSignallingAllowedThenReturnTrue, IsAtLeastXeHpcCore) {
    debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);
    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    EXPECT_TRUE(immCmdList->skipInOrderNonWalkerSignalingAllowed(events[0].get()));
}

HWTEST2_F(InOrderCmdListTests, givenRelaxedOrderingWhenProgrammingTimestampEventThenClearAndChainWithSyncAllocSignalingAsTwoSeparateSubmissions, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies,
                                   NEO::AppendOperations appendOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent, bool requireTaskCountUpdate,
                                   MutexLock *outerLock,
                                   std::unique_lock<std::mutex> *outerLockForIndirect) override {
            flushData.push_back(this->cmdListCurrentStartOffset);

            this->cmdListCurrentStartOffset = this->commandContainer.getCommandStream()->getUsed();

            return ZE_RESULT_SUCCESS;
        }

        std::vector<size_t> flushData; // start_offset
    };

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyMockCmdList>(false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    if (!immCmdList->skipInOrderNonWalkerSignalingAllowed(events[0].get())) {
        GTEST_SKIP(); // not supported
    }

    immCmdList->inOrderExecInfo->addCounterValue(1);

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));

    EXPECT_EQ(0u, immCmdList->flushData.size());

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    ASSERT_EQ(2u, immCmdList->flushData.size());
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());
    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), immCmdList->flushData[1]));

        if (immCmdList->isHeaplessModeEnabled()) {
            auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), sdiItor);

            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
            ASSERT_NE(nullptr, sdiCmd);

            EXPECT_EQ(events[0]->getCompletionFieldGpuAddress(device), sdiCmd->getAddress());
            EXPECT_EQ(0u, sdiCmd->getStoreQword());
            EXPECT_EQ(Event::STATE_CLEARED, sdiCmd->getDataDword0());

            auto sdiOffset = ptrDiff(sdiCmd, cmdStream->getCpuBase());
            EXPECT_TRUE(sdiOffset >= immCmdList->flushData[0]);
            EXPECT_TRUE(sdiOffset < immCmdList->flushData[1]);
        }

        auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto eventBaseGpuVa = events[0]->getPacketAddress(device);

        auto walker = genCmdCast<WalkerType *>(*walkerItor);
        auto &postSync = walker->getPostSync();
        using PostSyncType = std::decay_t<decltype(postSync)>;

        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
        EXPECT_EQ(eventBaseGpuVa, postSync.getDestinationAddress());

        auto walkerOffset = ptrDiff(walker, cmdStream->getCpuBase());
        EXPECT_TRUE(walkerOffset >= immCmdList->flushData[0]);
        EXPECT_TRUE(walkerOffset < immCmdList->flushData[1]);
    }

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), immCmdList->flushData[1]), (cmdStream->getUsed() - immCmdList->flushData[1])));

        // Relaxed Ordering registers
        auto lrrCmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_REG *>(*cmdList.begin());
        ASSERT_NE(nullptr, lrrCmd);

        EXPECT_EQ(RegisterOffsets::csGprR4, lrrCmd->getSourceRegisterAddress());
        EXPECT_EQ(RegisterOffsets::csGprR0, lrrCmd->getDestinationRegisterAddress());
        lrrCmd++;
        EXPECT_EQ(RegisterOffsets::csGprR4 + 4, lrrCmd->getSourceRegisterAddress());
        EXPECT_EQ(RegisterOffsets::csGprR0 + 4, lrrCmd->getDestinationRegisterAddress());

        lrrCmd++;

        auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

        EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyConditionalDataMemBbStart<FamilyType>(lrrCmd, 0, eventEndGpuVa, static_cast<uint64_t>(Event::STATE_CLEARED),
                                                                                               NEO::CompareOperation::equal, true, false, false));

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(ptrOffset(lrrCmd, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(false)));
        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(2u, sdiCmd->getDataDword0());
    }
}

HWTEST_F(InOrderCmdListTests, givenCbEventWhenAppendSignalEventCalledThenFallbackToAppendBarrier) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override {
            appendBarrierCalled++;
            return BaseClass::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);
        }

        uint32_t appendBarrierCalled = 0;
    };

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyMockCmdList>(false);

    auto eventPool = createEvents<FamilyType>(2, true);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    immCmdList->appendSignalEvent(events[0]->toHandle(), false);
    EXPECT_EQ(0u, immCmdList->appendBarrierCalled);

    immCmdList->appendSignalEvent(events[1]->toHandle(), false);
    EXPECT_EQ(1u, immCmdList->appendBarrierCalled);
}

HWTEST2_F(InOrderCmdListTests, givenRelaxedOrderingWhenProgrammingTimestampEventCbThenClearOnHstAndChainWithSyncAllocSignalingAsTwoSeparateSubmissions, IsAtLeastXeHpcCore) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;
        using BaseClass::BaseClass;

        bool handleCounterBasedEventOperations(L0::Event *signalEvent, bool skipAddingEventToResidency) override {
            auto ret = BaseClass::handleCounterBasedEventOperations(signalEvent, skipAddingEventToResidency);
            usedEvent = signalEvent;
            auto hostAddr = reinterpret_cast<uint32_t *>(usedEvent->getCompletionFieldHostAddress());
            *hostAddr = 0x123;

            return ret;
        }

        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies,
                                   NEO::AppendOperations appendOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent, bool requireTaskCountUpdate,
                                   MutexLock *outerLock,
                                   std::unique_lock<std::mutex> *outerLockForIndirect) override {
            auto hostAddr = reinterpret_cast<uint32_t *>(usedEvent->getCompletionFieldHostAddress());
            eventCompletionData.push_back(*hostAddr);

            if (eventCompletionData.size() == 1) {
                *hostAddr = 0x345;
            }

            this->cmdListCurrentStartOffset = this->commandContainer.getCommandStream()->getUsed();

            return ZE_RESULT_SUCCESS;
        }

        std::vector<uint32_t> eventCompletionData;
        L0::Event *usedEvent = nullptr;
    };

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyMockCmdList>(false);

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    if (!immCmdList->skipInOrderNonWalkerSignalingAllowed(events[0].get())) {
        GTEST_SKIP(); // not supported
    }

    immCmdList->inOrderExecInfo->addCounterValue(1);

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));

    EXPECT_EQ(0u, immCmdList->eventCompletionData.size());

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    ASSERT_EQ(2u, immCmdList->eventCompletionData.size());
    EXPECT_EQ(static_cast<uint32_t>(Event::STATE_CLEARED), immCmdList->eventCompletionData[0]);
    EXPECT_EQ(0x345u, immCmdList->eventCompletionData[1]);

    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    immCmdList->eventCompletionData.clear();

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);
    ASSERT_EQ(2u, immCmdList->eventCompletionData.size());
    EXPECT_NE(static_cast<uint32_t>(Event::STATE_CLEARED), immCmdList->eventCompletionData[0]);
}

HWTEST2_F(InOrderCmdListTests, givenRegularNonTimestampEventWhenSkipItsConvertedToCounterBasedThenDisableNonWalkerSignalingSkip, IsAtLeastXeHpcCore) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies,
                                   NEO::AppendOperations appendOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent, bool requireTaskCountUpdate,
                                   MutexLock *outerLock,
                                   std::unique_lock<std::mutex> *outerLockForIndirect) override {
            flushCounter++;

            this->cmdListCurrentStartOffset = this->commandContainer.getCommandStream()->getUsed();

            return ZE_RESULT_SUCCESS;
        }

        uint32_t flushCounter = 0;
    };

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyMockCmdList>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->signalScope = 0;

    if (!immCmdList->skipInOrderNonWalkerSignalingAllowed(events[0].get())) {
        GTEST_SKIP(); // not supported
    }

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    immCmdList->inOrderExecInfo->addCounterValue(1);

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));

    EXPECT_EQ(0u, immCmdList->flushCounter);

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    EXPECT_EQ(1u, immCmdList->flushCounter);
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenChainingWithRelaxedOrderingThenSignalAsSingleSubmission, IsAtLeastXeHpcCore) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies,
                                   NEO::AppendOperations appendOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent, bool requireTaskCountUpdate,
                                   MutexLock *outerLock,
                                   std::unique_lock<std::mutex> *outerLockForIndirect) override {
            flushCount++;

            return ZE_RESULT_SUCCESS;
        }

        uint32_t flushCount = 0;
    };

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.EnableInOrderRelaxedOrderingForEventsChaining.set(0);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyMockCmdList>(false);

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    immCmdList->inOrderExecInfo->addCounterValue(1);

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));

    EXPECT_EQ(0u, immCmdList->flushCount);

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    ASSERT_EQ(1u, immCmdList->flushCount);
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());
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

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
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

    if (immCmdList->dcFlushSupport || (!immCmdList->isHeaplessModeEnabled() && immCmdList->latestOperationHasOptimizedCbEvent)) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    events[0]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_TRUE(immCmdList->latestFlushIsHostVisible);

    immCmdList->hostSynchronize(0, false);

    if (!immCmdList->isHeaplessModeEnabled() && immCmdList->latestOperationHasOptimizedCbEvent) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(2u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    // handle post sync operations
    immCmdList->hostSynchronize(0, true);

    if (!immCmdList->isHeaplessModeEnabled() && immCmdList->latestOperationHasOptimizedCbEvent) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(3u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(2u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }
}

HWTEST_F(InOrderCmdListTests, givenEmptyTempAllocationsStorageWhenCallingSynchronizeThenUseInternalCounter) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_TRUE(immCmdList->latestFlushIsHostVisible);

    EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    immCmdList->hostSynchronize(0, true);

    if (!immCmdList->isHeaplessModeEnabled() && immCmdList->latestOperationHasOptimizedCbEvent) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    immCmdList->hostSynchronize(0, true);

    if (!immCmdList->isHeaplessModeEnabled() && immCmdList->latestOperationHasOptimizedCbEvent) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, InOrderCmdListTests, givenNonPostSyncWalkerWhenPatchingThenThrow) {
    InOrderPatchCommandHelpers::PatchCmd<FamilyType> incorrectCmd(nullptr, nullptr, nullptr, 1, NEO::InOrderPatchCommandHelpers::PatchCmdType::none, false, false);

    EXPECT_ANY_THROW(incorrectCmd.patch(1));

    InOrderPatchCommandHelpers::PatchCmd<FamilyType> walkerCmd(nullptr, nullptr, nullptr, 1, NEO::InOrderPatchCommandHelpers::PatchCmdType::walker, false, false);

    EXPECT_ANY_THROW(walkerCmd.patch(1));
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

    bool isCompactEvent0 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));
    bool isCompactEvent1 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[1]->isSignalScope()));
    bool isCompactEvent2 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[2]->isSignalScope()));

    EXPECT_TRUE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_EQ(isCompactEvent1, immCmdList->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_TRUE(immCmdList->isInOrderNonWalkerSignalingRequired(events[2].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(nullptr));

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    EXPECT_EQ(isCompactEvent0, immCmdList2->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_EQ(isCompactEvent1, immCmdList2->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_EQ(isCompactEvent2, immCmdList2->isInOrderNonWalkerSignalingRequired(events[2].get()));
    EXPECT_FALSE(immCmdList2->isInOrderNonWalkerSignalingRequired(nullptr));
}

HWTEST_F(InOrderCmdListTests, givenSignalAllPacketsSetWhenProgrammingRemainingPacketsThenSkip) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->signalAllEventPackets = true;

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
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    immCmdList->appendSignalEvent(events[0]->toHandle(), false);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t sdiSyncVa = 0;

    if (inOrderExecInfo->isHostStorageDuplicated()) {
        sdiSyncVa = reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress());
    } else {
        sdiSyncVa = inOrderExecInfo->getBaseDeviceAddress();
    }

    auto inOrderSyncVa = inOrderExecInfo->getBaseDeviceAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = cmdList.begin();
    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, inOrderSyncVa, immCmdList->isQwordInOrderCounter(), false));

    {

        auto rbeginItor = cmdList.rbegin();

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*rbeginItor);
        while (sdiCmd == nullptr) {
            sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++rbeginItor));
            if (rbeginItor == cmdList.rend()) {
                break;
            }
        }

        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(sdiSyncVa, sdiCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(2u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
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
    uint64_t sdiSyncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

    uint8_t ptr[64] = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    uint32_t inOrderCounter = 1;

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
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, inOrderCounter, inOrderSyncVa, immCmdList->isQwordInOrderCounter(), false));

        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();

        size_t rangeSizes = 1;
        const void **ranges = reinterpret_cast<const void **>(&ptr[0]);
        immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr);

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

        immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(ptr), nullptr, 0, nullptr);

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

    EXPECT_EQ(regularCmdList->inOrderExecInfo.get(), events[0]->inOrderExecInfo.get());

    uint32_t copyData = 0;
    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, events[1]->toHandle(), 0, nullptr, copyParams);

    EXPECT_EQ(regularCmdList->inOrderExecInfo.get(), events[1]->inOrderExecInfo.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderRegularCmdListWhenProgrammingNonKernelAppendThenWaitForDependencyAndSignalSyncAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    uint8_t ptr[64] = {};

    uint64_t inOrderSyncVa = regularCmdList->inOrderExecInfo->getBaseDeviceAddress();
    uint64_t sdiSyncVa = regularCmdList->inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(regularCmdList->inOrderExecInfo->getBaseHostAddress()) : regularCmdList->inOrderExecInfo->getBaseDeviceAddress();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

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

    {
        auto offset = cmdStream->getUsed();

        regularCmdList->appendEventReset(events[0]->toHandle());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), 2);
    }

    {
        auto offset = cmdStream->getUsed();

        size_t rangeSizes = 1;
        const void **ranges = reinterpret_cast<const void **>(&ptr[0]);
        regularCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 2, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), 3);
    }

    {
        auto offset = cmdStream->getUsed();

        regularCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(ptr), nullptr, 0, nullptr);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 3, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), 4);
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
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 4, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), 5);
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
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 5, inOrderSyncVa, regularCmdList->isQwordInOrderCounter(), false));
        verifySdi(cmdList.rbegin(), cmdList.rend(), 6);
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, InOrderCmdListTests, givenImmediateEventWhenWaitingFromRegularCmdListThenDontPatch) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);

    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size());

    EXPECT_EQ(NEO::InOrderPatchCommandHelpers::PatchCmdType::sdi, regularCmdList->inOrderPatchCmds[0].patchCmdType);

    EXPECT_EQ(immCmdList->inOrderExecInfo->isAtomicDeviceSignalling(), regularCmdList->inOrderPatchCmds[0].deviceAtomicSignaling);
    EXPECT_EQ(immCmdList->inOrderExecInfo->isHostStorageDuplicated(), regularCmdList->inOrderPatchCmds[0].duplicatedHostStorage);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);
    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), semaphoreCmd->getSemaphoreGraphicsAddress());

    auto walkerItor = find<WalkerType *>(semaphoreItor, cmdList.end());

    EXPECT_NE(cmdList.end(), walkerItor);
}

HWTEST2_F(InOrderCmdListTests, givenImmediateEventWhenWaitingFromRegularCmdListThenDontPatch, IsAtLeastXeCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);

    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size());

    EXPECT_EQ(NEO::InOrderPatchCommandHelpers::PatchCmdType::walker, regularCmdList->inOrderPatchCmds[0].patchCmdType);

    EXPECT_EQ(immCmdList->inOrderExecInfo->isAtomicDeviceSignalling(), regularCmdList->inOrderPatchCmds[0].deviceAtomicSignaling);
    EXPECT_EQ(immCmdList->inOrderExecInfo->isHostStorageDuplicated(), regularCmdList->inOrderPatchCmds[0].duplicatedHostStorage);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);
    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), semaphoreCmd->getSemaphoreGraphicsAddress());

    auto walkerItor = find<WalkerType *>(semaphoreItor, cmdList.end());

    EXPECT_NE(cmdList.end(), walkerItor);
}

HWTEST_F(InOrderCmdListTests, givenDebugFlagAndEventGeneratedByRegularCmdListWhenWaitingFromImmediateThenUseSubmissionCounter) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);

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

    uint64_t expectedCounterAppendValue = regularCmdList->inOrderExecInfo->getCounterValue();

    auto verifySemaphore = [&](uint64_t expectedValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, semaphoreCmd);

        if (semaphoreCmd->getSemaphoreGraphicsAddress() == immCmdList->inOrderExecInfo->getBaseDeviceAddress()) {
            // skip implicit dependency
            semaphoreItor++;
        } else if (immCmdList->isQwordInOrderCounter()) {
            std::advance(semaphoreItor, -2); // verify 2x LRI before semaphore
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreItor, expectedValue, regularCmdList->inOrderExecInfo->getBaseDeviceAddress(), immCmdList->isQwordInOrderCounter(), false));
    };

    // 0 Execute calls
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);

    // 1 Execute call
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr, nullptr);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);

    // 2 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr, nullptr);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue + expectedCounterAppendValue);

    // 3 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr, nullptr);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue + (expectedCounterAppendValue * 2));
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

        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, semaphoreCmd);

        if (semaphoreCmd->getSemaphoreGraphicsAddress() == immCmdList->inOrderExecInfo->getBaseDeviceAddress()) {
            // skip implicit dependency
            semaphoreItor++;
        } else if (immCmdList->isQwordInOrderCounter()) {
            std::advance(semaphoreItor, -2); // verify 2x LRI before semaphore
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreItor, expectedValue, regularCmdList->inOrderExecInfo->getBaseDeviceAddress(), immCmdList->isQwordInOrderCounter(), false));
    };

    // 0 Execute calls
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);

    // 1 Execute call
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr, nullptr);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);

    // 2 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr, nullptr);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    verifySemaphore(expectedCounterValue);

    // 3 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr, nullptr);
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
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
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

    alignedFree(alignedPtr);
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
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

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

    void *alloc = allocDeviceMem(16384u);

    immCmdList->appendMemoryCopy(alloc, alloc, 1, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    context->freeMem(alloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenAllocFlushRequiredWhenProgrammingComputeCopyThenSignalFromSdi) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto alignedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    immCmdList->appendMemoryCopy(alignedPtr, alignedPtr, 1, nullptr, 0, nullptr, copyParams);

    auto dcFlushRequired = immCmdList->getDcFlushRequired(true);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();

    if (dcFlushRequired) {
        EXPECT_EQ(0u, postSync.getDestinationAddress());
    } else {
        EXPECT_NE(0u, postSync.getDestinationAddress());
    }

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto gpuAddress = inOrderExecInfo->getBaseDeviceAddress();

    auto miAtomicItor = find<MI_ATOMIC *>(walkerItor, cmdList.end());
    if (device->getProductHelper().isDcFlushAllowed() && inOrderExecInfo->isHostStorageDuplicated()) {
        EXPECT_NE(cmdList.end(), miAtomicItor);
        auto miAtomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomicItor);
        EXPECT_EQ(gpuAddress, miAtomicCmd->getMemoryAddress());
    } else {
        EXPECT_EQ(cmdList.end(), miAtomicItor);
    }

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());
    if (dcFlushRequired) {
        EXPECT_NE(cmdList.end(), sdiItor);
        auto inOrderExecInfo = immCmdList->inOrderExecInfo;
        if (inOrderExecInfo->isHostStorageDuplicated()) {
            gpuAddress = reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress());
        }

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
        EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    } else {
        EXPECT_EQ(cmdList.end(), sdiItor);
    }

    alignedFree(alignedPtr);
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
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithSplitAndOutEventThenSignalInOrderAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

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
    EXPECT_EQ(immCmdList->getDcFlushRequired(true), pcCmd->getDcFlushEnable());
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pcCmd));
    EXPECT_TRUE(pcCmd->getUnTypedDataPortCacheFlush());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
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
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

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
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

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

    if (!immCmdList->inOrderAtomicSignalingEnabled) {
        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(1u, postSync.getImmediateData());
    }

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());

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
        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_NO_WRITE, postSync.getOperation());
        EXPECT_EQ(0u, postSync.getImmediateData());
    }

    auto l3FlushAfterPostSyncEnabled = this->neoDevice->getProductHelper().isL3FlushAfterPostSyncSupported(true);
    if (l3FlushAfterPostSyncEnabled) {
        EXPECT_NE(0u, postSync.getDestinationAddress());
    } else {
        EXPECT_EQ(0u, postSync.getDestinationAddress());
    }

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
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

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

    void *srcPtr = reinterpret_cast<void *>(0x1234);

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
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
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

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    if (immCmdList->isQwordInOrderCounter()) {
        std::advance(semaphoreItor, -2); // verify 2x LRI before semaphore
    }

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreItor, 2, inOrderExecInfo->getBaseDeviceAddress(), immCmdList->isQwordInOrderCounter(), false));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

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
    EXPECT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = regularCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

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
            expectedOffset = useZeroOffset ? 0 : device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset();
        }

        EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->getCounterValue());
        EXPECT_EQ(expectedOffset, immCmdList->inOrderExecInfo->getAllocationOffset());

        EXPECT_EQ(expectedCounter, events[0]->inOrderExecSignalValue);
        EXPECT_EQ(expectedOffset, events[0]->inOrderAllocationOffset);

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

    bool isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

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

            uint64_t address = pcCmd->getAddressHigh();
            address <<= 32;
            address |= pcCmd->getAddress();
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
        offset = device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset();
        if (isCompactEvent) {
            auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
            ASSERT_NE(cmdList.end(), pcItor);
            auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
            uint64_t address = pcCmd->getAddressHigh();
            address <<= 32;
            address |= pcCmd->getAddress();
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), address);
        } else {
            ASSERT_NE(cmdList.end(), semaphoreItor);

            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(std::numeric_limits<uint32_t>::max(), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(baseGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
            ASSERT_NE(nullptr, sdiCmd);

            EXPECT_EQ(baseGpuVa + offset, sdiCmd->getAddress());
            EXPECT_EQ(1u, sdiCmd->getDataDword0());
        }
    }

    EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(offset, immCmdList->inOrderExecInfo->getAllocationOffset());

    EXPECT_EQ(expectedCounter, events[0]->inOrderExecSignalValue);
    EXPECT_EQ(offset, events[0]->inOrderAllocationOffset);

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

    immCmdList2->appendBarrier(nullptr, 1, &eventHandle, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto inOrderExecInfo = immCmdList2->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList2->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
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

    immCmdList2->appendBarrier(nullptr, 1, &eventHandle, false);

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
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();
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

    for (auto &semaphore : semaphores) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);

        if (event->getInOrderExecInfo()->getBaseDeviceAddress() == semaphoreCmd->getSemaphoreGraphicsAddress()) {
            semaphoreFound = true;
        }
    }

    EXPECT_TRUE(semaphoreFound);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistThenInheritSignalSyncAllocation) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendBarrier(nullptr, 0, nullptr, false);
    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

    EXPECT_EQ(offset, cmdStream->getUsed());

    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
}

HWTEST_F(InOrderCmdListTests, givenRegularCmdListWhenProgrammingAppendBarrierWithoutWaitlistThenInheritSignalSyncAllocation) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto cmdStream = cmdList->getCmdContainer().getCommandStream();

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, cmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    cmdList->appendBarrier(nullptr, 0, nullptr, false);
    cmdList->appendBarrier(eventHandle, 0, nullptr, false);

    EXPECT_EQ(offset, cmdStream->getUsed());

    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
}

HWTEST_F(InOrderCmdListTests, givenEventCounterReusedFromPreviousAppendWhenHostSynchronizeThenFlushCaches) {
    if (!device->getProductHelper().isDcFlushAllowed()) {
        GTEST_SKIP();
    }
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(1u, cmdList->inOrderExecInfo->getCounterValue());

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();
    cmdList->appendBarrier(eventHandle, 0, nullptr, false);

    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_TRUE(ultCsr->flushTagUpdateCalled);
    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
}

HWTEST_F(InOrderCmdListTests, givenEventCounterNotReusedFromPreviousAppendWhenHostSynchronizeThenDontFlushCaches) {
    if (!device->getProductHelper().isDcFlushAllowed()) {
        GTEST_SKIP();
    }
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(1u, cmdList->inOrderExecInfo->getCounterValue());

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();
    cmdList->appendBarrier(eventHandle, 0, nullptr, false);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    EXPECT_EQ(2u, events[0]->inOrderExecSignalValue);
}

HWTEST_F(InOrderCmdListTests, givenTsCbEventWhenAppendNonKernelOperationOnNonHeaplessNonDcFlushPlatformThenWaitOnCounter) {
    if (device->getProductHelper().isDcFlushAllowed()) {
        GTEST_SKIP();
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    if (cmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    CpuIntrinsicsTests::pauseCounter = 0u;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    cmdList->appendBarrier(eventHandle, 0, nullptr, false);

    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    EXPECT_EQ(1u, CpuIntrinsicsTests::pauseCounter);
    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithDifferentEventsThenDontInherit) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(3, false);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    ze_event_handle_t waitlist[] = {events[0]->toHandle(), events[1]->toHandle()};

    immCmdList2->appendBarrier(events[2]->toHandle(), 2, waitlist, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor); // implicit dependency

    itor = find<MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor); // event0

    itor = find<MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    EXPECT_EQ(3u, events[2]->inOrderExecSignalValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAndTimestampEventThenSignalSyncAllocation) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();
    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAndRegularEventThenSignalSyncAllocation) {
    using MI_NOOP = typename FamilyType::MI_NOOP;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto cmd = cmdList.rbegin();
    MI_STORE_DATA_IMM *sdiCmd = nullptr;

    while (cmd != cmdList.rend()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*cmd);
        if (sdiCmd) {
            break;
        }

        if (genCmdCast<MI_NOOP *>(*cmd) || genCmdCast<MI_BATCH_BUFFER_END *>(*cmd)) {
            cmd++;
            continue;
        }

        ASSERT_TRUE(false);
    }

    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenCallingSyncThenHandleCompletion) {
    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    if (!immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    uint64_t *hostAddress = nullptr;
    GraphicsAllocation *expectedAlloc = nullptr;
    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        expectedAlloc = immCmdList->inOrderExecInfo->getHostCounterAllocation();
        hostAddress = ptrOffset(immCmdList->inOrderExecInfo->getBaseHostAddress(), counterOffset);
    } else {
        expectedAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
        hostAddress = static_cast<uint64_t *>(ptrOffset(expectedAlloc->getUnderlyingBuffer(), counterOffset));
    }

    *hostAddress = 0;

    GraphicsAllocation *downloadedAlloc = nullptr;
    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;
    bool forceFail = false;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        callCounter++;
        if (callCounter >= failCounter && !forceFail) {
            (*hostAddress)++;
        }
        downloadedAlloc = &graphicsAllocation;
    };

    // single check - not ready
    {
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
        EXPECT_EQ(downloadedAlloc, expectedAlloc);
        EXPECT_EQ(1u, callCounter);
        EXPECT_EQ(1u, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(0u, *hostAddress);
    }

    // timeout - not ready
    {
        forceFail = true;
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, expectedAlloc);
        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // gpu hang
    {
        ultCsr->forceReturnGpuHang = true;

        EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, expectedAlloc);

        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // success
    {
        ultCsr->checkGpuHangDetectedCalled = 0;
        ultCsr->forceReturnGpuHang = false;
        forceFail = false;
        callCounter = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(std::numeric_limits<uint64_t>::max(), false));
        EXPECT_EQ(downloadedAlloc, expectedAlloc);

        EXPECT_EQ(failCounter, callCounter);
        EXPECT_EQ(failCounter - 1, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(1u, *hostAddress);
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    *ultCsr->getTagAddress() = ultCsr->taskCount - 1;

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, true));

    *ultCsr->getTagAddress() = ultCsr->taskCount + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, true));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenDebugFlagSetWhenCallingSyncThenHandleCompletionOnHostAlloc) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    if (!immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto hostAlloc = immCmdList->inOrderExecInfo->getHostCounterAllocation();

    auto hostAddress = static_cast<uint64_t *>(ptrOffset(hostAlloc->getUnderlyingBuffer(), counterOffset));
    *hostAddress = 0;

    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;
    bool forceFail = false;

    GraphicsAllocation *downloadedAlloc = nullptr;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        callCounter++;
        if (callCounter >= failCounter && !forceFail) {
            (*hostAddress)++;
        }
        downloadedAlloc = &graphicsAllocation;
    };

    // single check - not ready
    {
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
        EXPECT_EQ(downloadedAlloc, hostAlloc);
        EXPECT_EQ(1u, callCounter);
        EXPECT_EQ(1u, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(0u, *hostAddress);
    }

    // timeout - not ready
    {
        forceFail = true;
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, hostAlloc);
        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // gpu hang
    {
        ultCsr->forceReturnGpuHang = true;

        EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, hostAlloc);
        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // success
    {
        ultCsr->checkGpuHangDetectedCalled = 0;
        ultCsr->forceReturnGpuHang = false;
        forceFail = false;
        callCounter = 0;
        EXPECT_EQ(downloadedAlloc, hostAlloc);
        EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(std::numeric_limits<uint64_t>::max(), false));

        EXPECT_EQ(failCounter, callCounter);
        EXPECT_EQ(failCounter - 1, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(1u, *hostAddress);
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    *ultCsr->getTagAddress() = ultCsr->taskCount - 1;

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, true));

    *ultCsr->getTagAddress() = ultCsr->taskCount + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, true));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenDoingCpuCopyThenSynchronize) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;
    if (!immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        callCounter++;
        if (callCounter >= failCounter) {
            (*hostAddress)++;
        }
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    events[0]->setIsCompleted();

    ultCsr->waitForCompletionWithTimeoutTaskCountCalled = 0;
    ultCsr->flushTagUpdateCalled = false;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 1, &eventHandle, copyParams);

    EXPECT_EQ(3u, callCounter);
    EXPECT_EQ(1u, *hostAddress);
    EXPECT_EQ(2u, ultCsr->checkGpuHangDetectedCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    context->freeMem(deviceAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenImmediateCmdListWhenDoingCpuCopyThenPassInfoToEvent) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());

    uint32_t hostCopyData = 0;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 3;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, copyParams);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_EQ(0u, events[0]->inOrderExecSignalValue);
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
    EXPECT_FALSE(events[0]->isAlreadyCompleted());

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, copyParams);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, copyParams);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_EQ(2u, events[0]->inOrderExecSignalValue);
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    context->freeMem(deviceAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenAubModeWhenSyncCalledAlwaysPollForCompletion) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::aub;
    auto eventPool = createEvents<FamilyType>(1, false);

    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        uint64_t *hostAddress = immCmdList->inOrderExecInfo->getBaseHostAddress();
        *hostAddress = 3;
    } else {
        auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
        auto hostAddress = static_cast<uint64_t *>(deviceAlloc->getUnderlyingBuffer());
        *hostAddress = 3;
    }

    *ultCsr->getTagAddress() = 3;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    immCmdList->hostSynchronize(0, false);

    auto expectPollForCompletion = (immCmdList->isHeaplessModeEnabled() || !immCmdList->latestOperationHasOptimizedCbEvent) ? 1u : 0u;
    EXPECT_EQ(expectPollForCompletion++, ultCsr->pollForAubCompletionCalled);

    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(expectPollForCompletion++, ultCsr->pollForAubCompletionCalled);

    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::hardwareWithAub;

    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(expectPollForCompletion, ultCsr->pollForAubCompletionCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenProfilingEventWhenDoingCpuCopyThenSetProfilingData) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto nonProfilingeventPool = createEvents<FamilyType>(1, false);
    auto profilingeventPool = createEvents<FamilyType>(1, true);

    auto eventHandle0 = events[0]->toHandle();
    auto eventHandle1 = events[1]->toHandle();

    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());

    uint32_t hostCopyData = 0;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 3;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle0, 0, nullptr, copyParams);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle1, 0, nullptr, copyParams);

    EXPECT_NE(nullptr, events[1]->inOrderExecInfo.get());
    EXPECT_TRUE(events[1]->isAlreadyCompleted());
    EXPECT_NE(L0::Event::STATE_CLEARED, *static_cast<uint32_t *>(events[1]->getHostAddress()));

    context->freeMem(deviceAlloc);
}

HWTEST_F(InOrderCmdListTests, givenEventCreatedFromPoolWhenItIsQueriedForAddressItReturnsProperAddressFromPool) {
    auto eventPool = createEvents<FamilyType>(1, false);
    uint64_t counterValue = 0;
    uint64_t address = 0;

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, &counterValue, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, nullptr, &address));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(nullptr, &counterValue, &address));

    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(Event::State::STATE_SIGNALED, counterValue);
    EXPECT_EQ(address, events[0]->getCompletionFieldGpuAddress(events[0]->peekEventPool()->getDevice()));
}
HWTEST_F(InOrderCmdListTests, givenEventCreatedFromPoolWithTimestampsWhenQueriedForAddressErrorIsReturned) {
    auto eventPool = createEvents<FamilyType>(1, true);
    uint64_t counterValue = 0;
    uint64_t address = 0;

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
}

HWTEST_F(InOrderCmdListTests, givenCorrectInputParamsWhenCreatingCbEventThenReturnSuccess) {
    uint64_t counterValue = 2;

    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue, &eventDesc, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue, nullptr, &handle));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate(context, nullptr, gpuAddress, hostAddress, counterValue, &eventDesc, &handle));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate(context, device, gpuAddress, &counterValue, counterValue, &eventDesc, &handle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, nullptr, counterValue, &eventDesc, &handle));
    auto eventObj = Event::fromHandle(handle);
    EXPECT_EQ(nullptr, eventObj->getInOrderExecInfo());
    zeEventDestroy(handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, nullptr, hostAddress, counterValue, &eventDesc, &handle));
    eventObj = Event::fromHandle(handle);
    EXPECT_EQ(nullptr, eventObj->getInOrderExecInfo());
    zeEventDestroy(handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, nullptr, nullptr, counterValue, &eventDesc, &handle));
    eventObj = Event::fromHandle(handle);
    EXPECT_EQ(nullptr, eventObj->getInOrderExecInfo());
    zeEventDestroy(handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue, &eventDesc, &handle));

    eventObj = Event::fromHandle(handle);

    ASSERT_NE(nullptr, eventObj);
    ASSERT_NE(nullptr, eventObj->getInOrderExecInfo().get());

    EXPECT_EQ(counterValue, eventObj->getInOrderExecInfo()->getCounterValue());
    EXPECT_EQ(hostAddress, eventObj->getInOrderExecInfo()->getBaseHostAddress());
    EXPECT_EQ(castToUint64(gpuAddress), eventObj->getInOrderExecInfo()->getBaseDeviceAddress());

    uint64_t address = 0;
    uint64_t value = 0;
    zexEventGetDeviceAddress(handle, &value, &address);

    EXPECT_EQ(address, eventObj->getInOrderExecInfo()->getBaseDeviceAddress());
    EXPECT_EQ(value, counterValue);

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenCorrectInputParamsWhenCreatingCbEvent2ThenReturnSuccess) {
    uint64_t counterValue = 2;

    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    zex_counter_based_event_external_sync_alloc_properties_t externalSyncAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES};
    externalSyncAllocProperties.completionValue = counterValue;
    externalSyncAllocProperties.deviceAddress = gpuAddress;
    externalSyncAllocProperties.hostAddress = hostAddress;

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSyncAllocProperties;
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, nullptr, &counterBasedDesc, &handle));
    externalSyncAllocProperties.hostAddress = &counterValue;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    externalSyncAllocProperties.hostAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    externalSyncAllocProperties.hostAddress = hostAddress;
    externalSyncAllocProperties.deviceAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    externalSyncAllocProperties.hostAddress = nullptr;
    externalSyncAllocProperties.deviceAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    counterBasedDesc.pNext = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));
    auto eventObj = Event::fromHandle(handle);

    zeEventDestroy(handle);

    counterBasedDesc.pNext = &externalSyncAllocProperties;
    externalSyncAllocProperties.hostAddress = hostAddress;
    externalSyncAllocProperties.deviceAddress = gpuAddress;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    eventObj = Event::fromHandle(handle);

    ASSERT_NE(nullptr, eventObj);
    ASSERT_NE(nullptr, eventObj->getInOrderExecInfo().get());

    EXPECT_EQ(counterValue, eventObj->getInOrderExecInfo()->getCounterValue());
    EXPECT_EQ(hostAddress, eventObj->getInOrderExecInfo()->getBaseHostAddress());
    EXPECT_EQ(castToUint64(gpuAddress), eventObj->getInOrderExecInfo()->getBaseDeviceAddress());
    EXPECT_EQ(nullptr, eventObj->getInOrderExecInfo()->getDeviceCounterAllocation());

    eventObj->resetCompletionStatus();
    eventObj->setIsCompleted();
    EXPECT_FALSE(eventObj->isAlreadyCompleted());

    uint64_t address = 0;
    uint64_t value = 0;
    zexEventGetDeviceAddress(handle, &value, &address);

    EXPECT_EQ(address, eventObj->getInOrderExecInfo()->getBaseDeviceAddress());
    EXPECT_EQ(value, counterValue);

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenUsmDeviceAllocationWhenCreatingCbEventFromExternalMemoryThenAssignGraphicsAllocation) {
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));
    auto deviceAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));

    zex_counter_based_event_external_sync_alloc_properties_t externalSyncAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES};
    externalSyncAllocProperties.completionValue = 2;
    externalSyncAllocProperties.deviceAddress = deviceAddress;
    externalSyncAllocProperties.hostAddress = hostAddress;

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSyncAllocProperties;

    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    auto eventObj = Event::fromHandle(handle);

    ASSERT_NE(nullptr, eventObj);
    ASSERT_NE(nullptr, eventObj->getInOrderExecInfo().get());

    EXPECT_EQ(castToUint64(deviceAddress), eventObj->getInOrderExecInfo()->getBaseDeviceAddress());
    EXPECT_NE(nullptr, eventObj->getInOrderExecInfo()->getDeviceCounterAllocation());

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
    context->freeMem(deviceAddress);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCreatingCounterBasedEventThenSetAllParams) {
    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t) * 2));

    zex_counter_based_event_external_storage_properties_t externalStorageAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES};
    externalStorageAllocProperties.completionValue = counterValue;
    externalStorageAllocProperties.deviceAddress = nullptr;
    externalStorageAllocProperties.incrementValue = incValue;

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalStorageAllocProperties;
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    externalStorageAllocProperties.deviceAddress = ptrOffset(devAddress, sizeof(uint64_t));
    externalStorageAllocProperties.incrementValue = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    externalStorageAllocProperties.incrementValue = incValue;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));
    auto eventObj = Event::fromHandle(handle);
    ASSERT_NE(nullptr, eventObj->getInOrderExecInfo());

    auto inOrderExecInfo = eventObj->getInOrderExecInfo();

    EXPECT_EQ(incValue, eventObj->getInOrderIncrementValue(1));
    EXPECT_EQ(counterValue, inOrderExecInfo->getCounterValue());
    EXPECT_EQ(castToUint64(externalStorageAllocProperties.deviceAddress), inOrderExecInfo->getBaseDeviceAddress());
    EXPECT_NE(nullptr, inOrderExecInfo->getDeviceCounterAllocation());
    SvmAllocationData *deviceAlloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(devAddress));
    auto offset = ptrDiff(devAddress, deviceAlloc->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress());
    auto lockedPtr = reinterpret_cast<uint64_t *>(ptrOffset(inOrderExecInfo->getDeviceCounterAllocation()->getLockedPtr(), sizeof(uint64_t) + offset));

    EXPECT_EQ(inOrderExecInfo->getBaseHostAddress(), lockedPtr);
    EXPECT_EQ(inOrderExecInfo->getExternalHostAllocation(), inOrderExecInfo->getDeviceCounterAllocation());

    zeEventDestroy(handle);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCreatingCounterBasedEventAndAdditionalTimestampNodeThenSetAdditionalParamsVectorCorrectly) {
    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto event = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    EXPECT_EQ(Event::State::HOST_CACHING_DISABLED_PERMANENT, event->isCompleted.load());
    event->isTimestampEvent = true;
    ASSERT_NE(nullptr, event->getInOrderExecInfo());

    auto node0 = device->getDeviceInOrderCounterAllocator()->getTag();

    event->resetAdditionalTimestampNode(node0, 1, false);

    auto node1 = device->getDeviceInOrderCounterAllocator()->getTag();
    event->resetAdditionalTimestampNode(node1, 1, false);
    EXPECT_EQ(2u, event->additionalTimestampNode.size());

    event->resetAdditionalTimestampNode(nullptr, 1, true);
    EXPECT_EQ(0u, event->additionalTimestampNode.size());

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCallingAppendThenDontResetInOrderExecInfo) {
    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t) * 2));

    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    auto handle = eventObj->toHandle();

    ASSERT_NE(nullptr, eventObj->getInOrderExecInfo());

    auto inOrderExecInfo = eventObj->getInOrderExecInfo();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    EXPECT_EQ(inOrderExecInfo, eventObj->getInOrderExecInfo());
    EXPECT_EQ(counterValue, eventObj->getInOrderExecInfo()->getCounterValue());
    EXPECT_EQ(counterValue, eventObj->getInOrderExecSignalValueWithSubmissionCounter());
    EXPECT_EQ(incValue, eventObj->getInOrderIncrementValue(1));

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCallingAppendThenDontPachDependencies) {
    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));

    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    auto handle = eventObj->toHandle();

    auto immCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &handle, launchParams);

    for (auto &cmd : immCmdList->inOrderPatchCmds) {
        EXPECT_NE(NEO::InOrderPatchCommandHelpers::PatchCmdType::lri64b, cmd.patchCmdType);
        EXPECT_NE(NEO::InOrderPatchCommandHelpers::PatchCmdType::semaphore, cmd.patchCmdType);
    }

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCallingAppendThenAggregateTimestampNodes) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());
    eventObj->setPacketsInUse(2);

    auto handle = eventObj->toHandle();

    auto immCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    ASSERT_EQ(1u, eventObj->inOrderTimestampNode.size());
    eventObj->setPacketsInUse(2);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    eventObj->setPacketsInUse(2);

    ASSERT_EQ(2u, eventObj->inOrderTimestampNode.size());

    auto node0 = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderTimestampNode[0]->getCpuBase());
    auto node1 = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderTimestampNode[1]->getCpuBase());

    TagSizeT packet00[4] = {1, 2, 3, 4};
    TagSizeT packet01[4] = {13, 14, 15, 16};

    TagSizeT packet10[4] = {5, 6, 7, 8};
    TagSizeT packet11[4] = {17, 18, 19, 20};

    node0->assignDataToAllTimestamps(0, packet00);
    node0->assignDataToAllTimestamps(1, packet01);
    node1->assignDataToAllTimestamps(0, packet10);
    node1->assignDataToAllTimestamps(1, packet11);

    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->queryKernelTimestamp(&result));

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();

    if (gfxCoreHelper.useOnlyGlobalTimestamps()) {
        EXPECT_EQ(2u, result.context.kernelStart);
    } else {
        EXPECT_EQ(1u, result.context.kernelStart);
    }
    EXPECT_EQ(2u, result.global.kernelStart);

    if (gfxCoreHelper.useOnlyGlobalTimestamps()) {
        EXPECT_EQ(20u, result.context.kernelEnd);
    } else {
        EXPECT_EQ(19u, result.context.kernelEnd);
    }
    EXPECT_EQ(20u, result.global.kernelEnd);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCallingAppendThenHandleResidency) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->storeMakeResidentAllocations = true;

    constexpr uint64_t counterValue = 4;
    constexpr uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    auto node0 = eventObj->inOrderTimestampNode[0];
    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[node0->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()]);

    while (device->getInOrderTimestampAllocator()->getGfxAllocations().size() == 1) {
        device->getInOrderTimestampAllocator()->getTag();
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    ASSERT_EQ(2u, eventObj->inOrderTimestampNode.size());

    auto node1 = eventObj->inOrderTimestampNode[1];

    EXPECT_NE(node0->getBaseGraphicsAllocation(), node1->getBaseGraphicsAllocation());

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[node1->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()]);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCallingAppendMoreThanCounterValueThenDontResetNodes) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    constexpr uint64_t counterValue = 4;
    constexpr uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    ASSERT_EQ(3u, eventObj->inOrderTimestampNode.size());

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCallingSplitAppendThenSetCorrectGpuVa) {
    using TagSizeT = typename FamilyType::TimestampPacketType;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    constexpr uint64_t counterValue = 4;
    constexpr uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();

    const size_t ptrBaseSize = 128;
    const size_t copyOffset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, copyOffset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - copyOffset, handle, 0, nullptr, copyParams);

    ASSERT_EQ(1u, eventObj->inOrderTimestampNode.size());
    auto expectedAddress0 = eventObj->inOrderTimestampNode[0]->getGpuAddress() + eventObj->getGlobalStartOffset();

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
        auto srmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

        EXPECT_EQ(expectedAddress0, srmCmd->getMemoryAddress());
    }

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - copyOffset, handle, 0, nullptr, copyParams);
    ASSERT_EQ(2u, eventObj->inOrderTimestampNode.size());
    auto expectedAddress1 = eventObj->inOrderTimestampNode[1]->getGpuAddress() + eventObj->getGlobalStartOffset();

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
        auto srmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

        EXPECT_EQ(expectedAddress1, srmCmd->getMemoryAddress());
    }

    context->freeMem(devAddress);
    alignedFree(alignedPtr);
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageWhenCallingAppendSignalInOrderDependencyCounterThenProgramAtomicOperation) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    constexpr uint64_t incValue = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1234;
    constexpr uint64_t counterValue = incValue * 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    immCmdList->inOrderAtomicSignalingEnabled = false;
    immCmdList->appendSignalInOrderDependencyCounter(eventObj.get(), false, false, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto it = find<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), it);

    auto miAtomic = genCmdCast<MI_ATOMIC *>(*it);
    EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_ADD, miAtomic->getAtomicOpcode());
    EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());
    EXPECT_EQ(getLowPart(incValue), miAtomic->getOperand1DataDword0());
    EXPECT_EQ(getHighPart(incValue), miAtomic->getOperand1DataDword1());

    EXPECT_EQ(castToUint64(devAddress), NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenCopyOnlyCmdListAndDebugFlagWhenCounterSignaledThenProgramMiFlush) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    immCmdList->appendSignalInOrderDependencyCounter(nullptr, false, false, false, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        if (immCmdList->useAdditionalBlitProperties) {
            auto it = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

            EXPECT_EQ(cmdList.end(), it);
        } else {
            auto it = cmdList.begin();
            if (BlitCommandsHelper<FamilyType>::isDummyBlitWaNeeded(immCmdList->dummyBlitWa)) {
                it++;
            }

            auto miFlush = genCmdCast<MI_FLUSH_DW *>(*it);
            EXPECT_NE(nullptr, miFlush);
        }
    }

    debugManager.flags.InOrderCopyMiFlushSync.set(0);

    offset = cmdStream->getUsed();
    immCmdList->appendSignalInOrderDependencyCounter(nullptr, false, false, false, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto it = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), it);
    }
}

HWTEST_F(InOrderCmdListTests, givenExternalSyncStorageAndCopyOnlyCmdListWhenCallingAppendMemoryCopyWithDisabledInOrderSignalingThenSignalAtomicStorage) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    constexpr uint64_t incValue = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1234;
    constexpr uint64_t counterValue = incValue * 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();
    uint32_t copyData = 0;
    copyParams.forceDisableCopyOnlyInOrderSignaling = true;

    {
        immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventObj->toHandle(), 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto it = find<MI_ATOMIC *>(cmdList.begin(), cmdList.end());

        if (immCmdList->useAdditionalBlitProperties) {
            EXPECT_EQ(cmdList.end(), it);
        } else {
            ASSERT_NE(cmdList.end(), it);

            auto miAtomic = genCmdCast<MI_ATOMIC *>(*it);
            EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_ADD, miAtomic->getAtomicOpcode());
            EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());
            EXPECT_EQ(getLowPart(incValue), miAtomic->getOperand1DataDword0());
            EXPECT_EQ(getHighPart(incValue), miAtomic->getOperand1DataDword1());

            EXPECT_EQ(castToUint64(devAddress), NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        }
    }

    offset = cmdStream->getUsed();

    {
        ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

        immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, eventObj->toHandle(), 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto it = find<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
        if (immCmdList->useAdditionalBlitProperties) {
            EXPECT_EQ(cmdList.end(), it);
        } else {
            ASSERT_NE(cmdList.end(), it);

            auto miAtomic = genCmdCast<MI_ATOMIC *>(*it);
            EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_ADD, miAtomic->getAtomicOpcode());
            EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());
            EXPECT_EQ(getLowPart(incValue), miAtomic->getOperand1DataDword0());
            EXPECT_EQ(getHighPart(incValue), miAtomic->getOperand1DataDword1());

            EXPECT_EQ(castToUint64(devAddress), NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        }
    }

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenTimestmapEnabledWhenCreatingStandaloneCbEventThenSetCorrectPacketSize) {
    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP;

    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    auto eventObj = Event::fromHandle(handle);

    using TagSizeT = typename FamilyType::TimestampPacketType;
    constexpr auto singlePacetSize = NEO::TimestampPackets<TagSizeT, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();

    EXPECT_EQ(singlePacetSize, eventObj->getSinglePacketSize());

    zeEventDestroy(handle);
}

HWTEST_F(InOrderCmdListTests, givenStandaloneEventWhenCallingSynchronizeThenReturnCorrectValue) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &handle));

    auto eventObj = Event::fromHandle(handle);

    EXPECT_EQ(ZE_RESULT_NOT_READY, eventObj->hostSynchronize(1));

    (*hostAddress)++;

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->hostSynchronize(1));

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenMitigateHostVisibleSignalWhenCallingSynchronizeOnCbEventThenFlushDcIfSupported) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.MitigateHostVisibleSignal.set(true);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));
    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);
    ze_event_desc_t eventDesc = {};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue, &eventDesc, &handle));
    auto eventObj = Event::fromHandle(handle);

    EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->hostSynchronize(-1));

    if (device->getProductHelper().isDcFlushAllowed() && !ultCsr->heaplessModeEnabled) {
        EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
        EXPECT_TRUE(ultCsr->flushTagUpdateCalled);
    } else {
        EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
        EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    }

    zeEventDestroy(handle);
    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedTimestampHostVisibleSignalWhenCallingSynchronizeOnCbEventThenFlushDcIfSupported) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP | ZEX_COUNTER_BASED_EVENT_FLAG_HOST_VISIBLE;
    counterBasedDesc.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    auto eventObj = Event::fromHandle(handle);
    *static_cast<Event::State *>(ptrOffset(eventObj->getHostAddress(), eventObj->getContextEndOffset())) = Event::State::STATE_SIGNALED;
    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getBaseHostAddress());
    *hostAddress = 1u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->hostSynchronize(-1));

    if (device->getProductHelper().isDcFlushAllowed() && !immCmdList->isHeaplessModeEnabled()) {
        EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
        EXPECT_TRUE(ultCsr->flushTagUpdateCalled);
    } else {
        EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
        EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    }

    zeEventDestroy(handle);
}

HWTEST_F(InOrderCmdListTests, givenStandaloneCbEventWhenPassingExternalInterruptIdThenAssign) {
    zex_intel_event_sync_mode_exp_desc_t syncModeDesc = {ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC};
    syncModeDesc.externalInterruptId = 123;

    auto mockProductHelper = new MockProductHelper;
    mockProductHelper->isInterruptSupportedResult = true;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(mockProductHelper);

    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;
    auto event1 = createStandaloneCbEvent(reinterpret_cast<const ze_base_desc_t *>(&syncModeDesc));
    EXPECT_EQ(NEO::InterruptId::notUsed, event1->externalInterruptId);
    EXPECT_FALSE(event1->isKmdWaitModeEnabled());

    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_EXTERNAL_INTERRUPT_WAIT;
    auto event2 = createStandaloneCbEvent(reinterpret_cast<const ze_base_desc_t *>(&syncModeDesc));
    EXPECT_EQ(syncModeDesc.externalInterruptId, event2->externalInterruptId);
    EXPECT_TRUE(event2->isKmdWaitModeEnabled());

    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_EXTERNAL_INTERRUPT_WAIT | ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT;
    auto event3 = createStandaloneCbEvent(reinterpret_cast<const ze_base_desc_t *>(&syncModeDesc));
    EXPECT_EQ(syncModeDesc.externalInterruptId, event3->externalInterruptId);
    EXPECT_TRUE(event3->isKmdWaitModeEnabled());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenStandaloneEventWhenCallingAppendThenSuccess) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;
    ze_event_handle_t eHandle3 = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle3));

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, copyParams);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, copyParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eHandle3, 0, nullptr, launchParams);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    zeEventDestroy(eHandle3);
    context->freeMem(hostAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenStandaloneEventAndKernelSplitWhenCallingAppendThenSuccess) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle2));

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eHandle1, 0, nullptr, copyParams);
    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 1, &eHandle2, copyParams);

    alignedFree(alignedPtr);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenStandaloneEventAndCopyOnlyCmdListWhenCallingAppendThenSuccess) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle2));

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, copyParams);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, copyParams);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedEventAndAdditionalTimestampNodeAndResetCalledThenVectorIsCleared) {
    auto eventPool = createEvents<FamilyType>(1, true);

    auto node0 = device->getDeviceInOrderCounterAllocator()->getTag();

    events[0]->resetAdditionalTimestampNode(node0, 1, false);
    EXPECT_EQ(1u, events[0]->additionalTimestampNode.size());

    events[0]->resetAdditionalTimestampNode(nullptr, 1, true);
    EXPECT_EQ(0u, events[0]->additionalTimestampNode.size());
}

HWTEST_F(InOrderCmdListTests, givenDebugFlagAndCounterBasedEventWhenAskingForEventAddressAndValueThenReturnCorrectValues) {
    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);

    auto eventPool = createEvents<FamilyType>(1, false);
    uint64_t counterValue = -1;
    uint64_t address = -1;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto deviceAlloc = cmdList->inOrderExecInfo->getDeviceCounterAllocation();

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(2u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress(), address);

    cmdList->close();

    ze_command_queue_desc_t desc = {};
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);

    auto cmdListHandle = cmdList->toHandle();
    mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(4u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress(), address);

    events[0]->inOrderAllocationOffset = 0x12300;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(4u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress() + events[0]->inOrderAllocationOffset, address);
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedEventWhenAskingForEventAddressAndValueThenReturnCorrectValues) {
    auto eventPool = createEvents<FamilyType>(1, false);
    uint64_t counterValue = -1;
    uint64_t address = -1;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto deviceAlloc = cmdList->inOrderExecInfo->getDeviceCounterAllocation();

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(2u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress(), address);

    cmdList->close();

    ze_command_queue_desc_t desc = {};
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);

    auto cmdListHandle = cmdList->toHandle();
    mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(2u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress(), address);

    events[0]->inOrderAllocationOffset = 0x12300;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(2u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress() + events[0]->inOrderAllocationOffset, address);
}

HWTEST_F(InOrderCmdListTests, givenIncorrectArgumentswhenCallingZexDeviceGetAggregatedCopyOffloadIncrementValueThenReturnError) {
    uint32_t incValue = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexDeviceGetAggregatedCopyOffloadIncrementValue(device->toHandle(), nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexDeviceGetAggregatedCopyOffloadIncrementValue(nullptr, &incValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, wWhenUsingImmediateCmdListThenDontAddCmdsToPatch) {
    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    uint32_t copyData = 0;

    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(0u, immCmdList->inOrderPatchCmds.size());
}

HWTEST_F(InOrderCmdListTests, givenRegularCmdListWhenResetCalledThenClearCmdsToPatch) {
    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_NE(0u, cmdList->inOrderPatchCmds.size());

    cmdList->reset();

    EXPECT_EQ(0u, cmdList->inOrderPatchCmds.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenGpuHangDetectedInCpuCopyPathThenReportError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto eventPool = createEvents<FamilyType>(1, false);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;

    ultCsr->forceReturnGpuHang = true;
    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;
    ultCsr->callBaseWaitForCompletionWithTimeout = false;

    auto status = immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, status);

    ultCsr->forceReturnGpuHang = false;

    *hostAddress = std::numeric_limits<uint64_t>::max();

    context->freeMem(deviceAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithoutEventThenAddBarrierAndSignalCounter) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto cmdItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pcCmd->getPostSyncOperation());

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));

    while (sdiCmd == nullptr && cmdItor != cmdList.end()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));
    }

    ASSERT_NE(nullptr, sdiCmd);
    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());

    alignedFree(alignedPtr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenDebugFlagSetWhenKernelSplitIsExpectedThenDontSplit) {
    debugManager.flags.ForceNonWalkerSplitMemoryCopy.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, walkers.size());

    alignedFree(alignedPtr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithEventThenSignalCounter) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto cmdItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_EQ(immCmdList->getDcFlushRequired(true), pcCmd->getDcFlushEnable());
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pcCmd));
    EXPECT_TRUE(pcCmd->getUnTypedDataPortCacheFlush());

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));

    while (sdiCmd == nullptr && cmdItor != cmdList.end()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));
    }

    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();
    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());

    alignedFree(alignedPtr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithProfilingEventThenSignalCounter) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto cmdItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pcCmd);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));

    while (sdiCmd == nullptr && cmdItor != cmdList.end()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));
    }

    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();
    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());

    alignedFree(alignedPtr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenImplicitScalingEnabledWhenAskingForExtensionsThenReturnSyncDispatchExtension) {
    uint32_t count = 0;
    ze_result_t res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_driver_extension_properties_t> extensionProperties(count);

    res = driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &param) {
        return (strcmp(param.name, ZE_SYNCHRONIZED_DISPATCH_EXP_NAME) == 0);
    });

    EXPECT_EQ(extensionProperties.end(), it);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeAndNoopWaitEventsAllowedWhenEventBoundToCmdListThenNoopSpaceForWaitCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    char noopedLriBuffer[sizeof(MI_LOAD_REGISTER_IMM)] = {};
    memset(noopedLriBuffer, 0, sizeof(MI_LOAD_REGISTER_IMM));
    char noopedSemWaitBuffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    memset(noopedSemWaitBuffer, 0, sizeof(MI_SEMAPHORE_WAIT));

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->allowCbWaitEventsNoopDispatch = true;

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    CommandToPatchContainer outCbWaitEventCmds;
    launchParams.outListCommands = &outCbWaitEventCmds;
    result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    size_t expectedLoadRegImmCount = FamilyType::isQwordInOrderCounter ? 2 : 0;

    size_t additionalPatchCmdsSize = regularCmdList->kernelMemoryPrefetchEnabled() ? 1 : 0;
    size_t expectedWaitCmds = 1 + expectedLoadRegImmCount + additionalPatchCmdsSize;
    ASSERT_EQ(expectedWaitCmds, outCbWaitEventCmds.size());

    size_t outCbWaitEventCmdsIndex = 0;
    for (; outCbWaitEventCmdsIndex < expectedLoadRegImmCount; outCbWaitEventCmdsIndex++) {
        auto &cmd = outCbWaitEventCmds[outCbWaitEventCmdsIndex + additionalPatchCmdsSize];

        EXPECT_EQ(CommandToPatch::CbWaitEventLoadRegisterImm, cmd.type);
        auto registerNumber = 0x2600 + (4 * outCbWaitEventCmdsIndex);
        EXPECT_EQ(registerNumber, cmd.offset);

        ASSERT_NE(nullptr, cmd.pDestination);
        auto memCmpRet = memcmp(cmd.pDestination, noopedLriBuffer, sizeof(MI_LOAD_REGISTER_IMM));
        EXPECT_EQ(0, memCmpRet);
    }

    auto &cmd = outCbWaitEventCmds[outCbWaitEventCmdsIndex + additionalPatchCmdsSize];

    EXPECT_EQ(CommandToPatch::CbWaitEventSemaphoreWait, cmd.type);

    ASSERT_NE(nullptr, cmd.pDestination);
    auto memCmpRet = memcmp(cmd.pDestination, noopedSemWaitBuffer, sizeof(MI_SEMAPHORE_WAIT));
    EXPECT_EQ(0, memCmpRet);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenAppendingKernelInCommandViewModeThenDoNotDispatchInOrderCommands) {
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    uint8_t computeWalkerHostBuffer[512];
    uint8_t payloadHostBuffer[256];

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.makeKernelCommandView = true;
    launchParams.cmdWalkerBuffer = computeWalkerHostBuffer;
    launchParams.hostPayloadBuffer = payloadHostBuffer;

    auto result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(0u, regularCmdList->inOrderPatchCmds.size());
}

} // namespace ult
} // namespace L0
