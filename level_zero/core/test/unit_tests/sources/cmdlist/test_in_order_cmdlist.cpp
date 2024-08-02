/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

#include <type_traits>
#include <variant>

namespace L0 {
namespace ult {

using InOrderCmdListTests = InOrderCmdListFixture;

HWTEST2_F(InOrderCmdListTests, givenDriverHandleWhenAskingForExtensionsThenReturnCorrectVersions, IsAtLeastSkl) {
    uint32_t count = 0;
    ze_result_t res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_NE(0u, count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_driver_extension_properties_t> extensionProperties;
    extensionProperties.resize(count);

    res = driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &extension) { return (strcmp(extension.name, ZE_EVENT_POOL_COUNTER_BASED_EXP_NAME) == 0); });
    EXPECT_NE(it, extensionProperties.end());
    EXPECT_EQ((*it).version, ZE_EVENT_POOL_COUNTER_BASED_EXP_VERSION_CURRENT);

    it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &extension) { return (strcmp(extension.name, ZE_INTEL_COMMAND_LIST_MEMORY_SYNC) == 0); });
    EXPECT_NE(it, extensionProperties.end());
    EXPECT_EQ((*it).version, ZE_INTEL_COMMAND_LIST_MEMORY_SYNC_EXP_VERSION_CURRENT);

    it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &extension) { return (strcmp(extension.name, ZEX_INTEL_EVENT_SYNC_MODE_EXP_NAME) == 0); });
    EXPECT_NE(it, extensionProperties.end());
    EXPECT_EQ((*it).version, ZEX_INTEL_EVENT_SYNC_MODE_EXP_VERSION_CURRENT);
}

HWTEST2_F(InOrderCmdListTests, givenCmdListWhenAskingForQwordDataSizeThenReturnFalse, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_FALSE(immCmdList->isQwordInOrderCounter());
}

HWTEST2_F(InOrderCmdListTests, givenInvalidPnextStructWhenCreatingEventThenIgnore, IsAtLeastSkl) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ze_event_desc_t extStruct = {ZE_STRUCTURE_TYPE_FORCE_UINT32};
    ze_event_desc_t eventDesc = {};
    eventDesc.pNext = &extStruct;

    auto event0 = DestroyableZeUniquePtr<FixtureMockEvent>(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));

    EXPECT_NE(nullptr, event0.get());
}

HWTEST2_F(InOrderCmdListTests, givenEventSyncModeDescPassedWhenCreatingEventThenEnableNewModes, IsAtLeastSkl) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 6;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    zex_intel_event_sync_mode_exp_desc_t syncModeDesc = {ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC};
    ze_event_desc_t eventDesc = {};
    eventDesc.pNext = &syncModeDesc;

    eventDesc.index = 0;
    syncModeDesc.syncModeFlags = 0;
    auto event0 = DestroyableZeUniquePtr<FixtureMockEvent>(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));
    EXPECT_FALSE(event0->isInterruptModeEnabled());
    EXPECT_FALSE(event0->isKmdWaitModeEnabled());

    eventDesc.index = 1;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;
    auto event1 = DestroyableZeUniquePtr<FixtureMockEvent>(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));
    EXPECT_TRUE(event1->isInterruptModeEnabled());
    EXPECT_FALSE(event1->isKmdWaitModeEnabled());

    eventDesc.index = 2;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT;
    auto event2 = DestroyableZeUniquePtr<FixtureMockEvent>(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));
    EXPECT_FALSE(event2->isInterruptModeEnabled());
    EXPECT_TRUE(event2->isKmdWaitModeEnabled());

    eventDesc.index = 3;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT | ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT;
    auto event3 = DestroyableZeUniquePtr<FixtureMockEvent>(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));
    EXPECT_TRUE(event3->isInterruptModeEnabled());
    EXPECT_TRUE(event3->isKmdWaitModeEnabled());

    eventDesc.index = 4;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;
    syncModeDesc.externalInterruptId = 123;
    auto event4 = DestroyableZeUniquePtr<FixtureMockEvent>(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));
    EXPECT_EQ(NEO::InterruptId::notUsed, event4->externalInterruptId);

    eventDesc.index = 5;
    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_EXTERNAL_INTERRUPT_WAIT;
    EXPECT_ANY_THROW(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
}

HWTEST2_F(InOrderCmdListTests, givenQueueFlagWhenCreatingCmdListThenEnableRelaxedOrdering, IsAtLeastXeHpCore) {
    NEO::debugManager.flags.ForceInOrderImmediateCmdListExecution.set(-1);

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

    ze_command_list_handle_t cmdList;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));

    EXPECT_TRUE(static_cast<CommandListCoreFamilyImmediate<gfxCoreFamily> *>(cmdList)->isInOrderExecutionEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(cmdList));
}

HWTEST2_F(InOrderCmdListTests, givenNotSignaledInOrderEventWhenAddedToWaitListThenReturnError, IsAtLeastSkl) {
    debugManager.flags.ForceInOrderEvents.set(1);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ze_event_desc_t eventDesc = {};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    eventDesc.index = 0;
    auto event = std::unique_ptr<FixtureMockEvent>(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));
    EXPECT_TRUE(event->isCounterBased());

    auto handle = event->toHandle();

    returnValue = immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &handle, launchParams, false);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
}

HWTEST2_F(InOrderCmdListTests, givenIpcAndCounterBasedEventPoolFlagsWhenCreatingThenReturnError, IsAtLeastSkl) {
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

HWTEST2_F(InOrderCmdListTests, givenIncorrectFlagsWhenCreatingCounterBasedEventsThenReturnError, IsAtLeastSkl) {
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

HWTEST2_F(InOrderCmdListTests, givenIpcPoolEventWhenTryingToImplicitlyConverToCounterBasedEventThenDisallow, IsAtLeastSkl) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    auto eventPoolForExport = std::unique_ptr<WhiteBox<EventPool>>(static_cast<WhiteBox<EventPool> *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
    auto eventPoolImported = std::unique_ptr<WhiteBox<EventPool>>(static_cast<WhiteBox<EventPool> *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));

    eventPoolForExport->isIpcPoolFlag = true;
    eventPoolImported->isImportedIpcPool = true;

    ze_event_desc_t eventDesc = {};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    DestroyableZeUniquePtr<FixtureMockEvent> event0(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolForExport.get(), &eventDesc, device)));
    EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, event0->counterBasedMode);

    DestroyableZeUniquePtr<FixtureMockEvent> event1(static_cast<FixtureMockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolImported.get(), &eventDesc, device)));
    EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, event1->counterBasedMode);
}

HWTEST2_F(InOrderCmdListTests, givenNotSignaledInOrderWhenWhenCallingQueryStatusThenReturnSuccess, IsAtLeastSkl) {
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->enableCounterBasedMode(true, eventPool->getCounterBasedFlags());

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->queryStatus());
}

HWTEST2_F(InOrderCmdListTests, givenCmdListsWhenDispatchingThenUseInternalTaskCountForWaits, IsAtLeastSkl) {
    auto immCmdList0 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, immCmdList0->cmdQImmediate->getTaskCount());
    EXPECT_EQ(2u, immCmdList1->cmdQImmediate->getTaskCount());

    // explicit wait
    {
        immCmdList0->hostSynchronize(0);
        EXPECT_EQ(1u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

        immCmdList1->hostSynchronize(0);
        EXPECT_EQ(2u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
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

        immCmdList0->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, false, false);

        EXPECT_EQ(immCmdList0->dcFlushSupport ? 1u : 2u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(immCmdList0->dcFlushSupport ? 3u : 2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

        immCmdList1->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, false, false);
        EXPECT_EQ(2u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
        EXPECT_EQ(immCmdList0->dcFlushSupport ? 4u : 2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

        context->freeMem(deviceAlloc);
    }
}

HWTEST2_F(InOrderCmdListTests, givenCounterBasedEventsWhenHostWaitsAreCalledThenLatestWaitIsRecorded, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(2, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);

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
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenEventHostSyncCalledThenCallWaitUserFence, IsAtLeastXeHpCore) {
    NEO::debugManager.flags.WaitForUserFenceOnEventHostSynchronize.set(1);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(2, false);
    EXPECT_TRUE(events[0]->isKmdWaitModeEnabled());
    EXPECT_TRUE(events[0]->isInterruptModeEnabled());
    EXPECT_TRUE(events[1]->isKmdWaitModeEnabled());
    EXPECT_TRUE(events[1]->isInterruptModeEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(2));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    events[0]->inOrderAllocationOffset = 123;

    auto hostAddress = castToUint64(ptrOffset(events[0]->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer(), events[0]->inOrderAllocationOffset));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->waitUserFenecParams.forceRetStatusEnabled = true;
    ultCsr->waitUserFenecParams.forceRetStatusValue = false;
    EXPECT_EQ(0u, ultCsr->waitUserFenecParams.callCount);

    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(2));

    EXPECT_EQ(1u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(hostAddress, ultCsr->waitUserFenecParams.latestWaitedAddress);
    EXPECT_EQ(events[0]->inOrderExecSignalValue, ultCsr->waitUserFenecParams.latestWaitedValue);
    EXPECT_EQ(2, ultCsr->waitUserFenecParams.latestWaitedTimeout);

    ultCsr->waitUserFenecParams.forceRetStatusValue = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(3));

    EXPECT_EQ(2u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(hostAddress, ultCsr->waitUserFenecParams.latestWaitedAddress);
    EXPECT_EQ(events[0]->inOrderExecSignalValue, ultCsr->waitUserFenecParams.latestWaitedValue);
    EXPECT_EQ(3, ultCsr->waitUserFenecParams.latestWaitedTimeout);

    // already completed
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(3));
    EXPECT_EQ(2u, ultCsr->waitUserFenecParams.callCount);

    // non in-order event
    events[1]->makeCounterBasedInitiallyDisabled();
    events[1]->hostSynchronize(2);
    EXPECT_EQ(2u, ultCsr->waitUserFenecParams.callCount);
}

HWTEST2_F(InOrderCmdListTests, givenInterruptableEventsWhenExecutingOnDifferentCsrThenAssignItToEventOnExecute, IsAtLeastXeHpCore) {
    auto cmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto cmdlistHandle = cmdList->toHandle();

    auto eventPool = createEvents<FamilyType>(3, false);
    events[0]->enableKmdWaitMode();
    events[1]->enableKmdWaitMode();

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[2]->toHandle(), 0, nullptr, launchParams, false);
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

    auto firstQueue = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, csr1, &desc);
    firstQueue->initialize(false, false, false);

    auto csr2 = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    ASSERT_NE(nullptr, csr2);
    auto secondQueue = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, csr2, &desc);
    secondQueue->initialize(false, false, false);

    EXPECT_NE(firstQueue->getCsr(), secondQueue->getCsr());

    firstQueue->executeCommandLists(1, &cmdlistHandle, nullptr, false, nullptr);
    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(firstQueue->getCsr(), events[0]->csrs[0]);
    EXPECT_EQ(1u, events[1]->csrs.size());
    EXPECT_EQ(firstQueue->getCsr(), events[1]->csrs[0]);
    EXPECT_EQ(1u, events[2]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[2]->csrs[0]);

    secondQueue->executeCommandLists(1, &cmdlistHandle, nullptr, false, nullptr);
    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(secondQueue->getCsr(), events[0]->csrs[0]);
    EXPECT_EQ(1u, events[1]->csrs.size());
    EXPECT_EQ(secondQueue->getCsr(), events[1]->csrs[0]);
    EXPECT_EQ(1u, events[2]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[2]->csrs[0]);

    cmdList->reset();
    EXPECT_EQ(0u, cmdList->interruptEvents.size());
}

HWTEST2_F(InOrderCmdListTests, givenUserInterruptEventWhenWaitingThenWaitForUserFenceWithParams, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(2, false);
    events[0]->enableKmdWaitMode();
    events[0]->enableInterruptMode();

    events[1]->enableKmdWaitMode();
    events[1]->enableInterruptMode();
    events[1]->externalInterruptId = 0x123;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->waitUserFenecParams.forceRetStatusEnabled = true;

    EXPECT_EQ(0u, ultCsr->waitUserFenecParams.callCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(2));

    EXPECT_EQ(1u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(NEO::InterruptId::notUsed, ultCsr->waitUserFenecParams.externalInterruptId);
    EXPECT_TRUE(ultCsr->waitUserFenecParams.userInterrupt);

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[1]->hostSynchronize(2));

    EXPECT_EQ(2u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(events[1]->externalInterruptId, ultCsr->waitUserFenecParams.externalInterruptId);
    EXPECT_TRUE(ultCsr->waitUserFenecParams.userInterrupt);
}

HWTEST2_F(InOrderCmdListTests, givenUserInterruptEventWhenWaitingThenPassCorrectAllocation, IsAtLeastXeHpCore) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto singleStorageImmCmdList = createImmCmdList<gfxCoreFamily>();

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto duplicatedStorageImmCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(2, false);
    events[0]->enableKmdWaitMode();
    events[0]->enableInterruptMode();

    events[1]->enableKmdWaitMode();
    events[1]->enableInterruptMode();

    singleStorageImmCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    duplicatedStorageImmCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->waitUserFenecParams.forceRetStatusEnabled = true;

    EXPECT_EQ(0u, ultCsr->waitUserFenecParams.callCount);

    // Single counter storage
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(2));

    EXPECT_EQ(1u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(events[0]->getInOrderExecInfo()->getDeviceCounterAllocation(), ultCsr->waitUserFenecParams.latestAllocForInterruptWait);
    EXPECT_TRUE(ultCsr->waitUserFenecParams.userInterrupt);

    // Duplicated host storage
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[1]->hostSynchronize(2));

    EXPECT_EQ(2u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(events[1]->getInOrderExecInfo()->getHostCounterAllocation(), ultCsr->waitUserFenecParams.latestAllocForInterruptWait);
    EXPECT_TRUE(ultCsr->waitUserFenecParams.userInterrupt);

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

    EXPECT_EQ(3u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(event2->getInOrderExecInfo()->getExternalHostAllocation(), ultCsr->waitUserFenecParams.latestAllocForInterruptWait);
    EXPECT_TRUE(ultCsr->waitUserFenecParams.userInterrupt);

    event2->destroy();
    context->freeMem(hostAddress);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenHostResetOrSignalEventCalledThenReturnError, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(3, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

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

HWTEST2_F(InOrderCmdListTests, whenCreatingInOrderExecInfoThenReuseDeviceAlloc, IsAtLeastSkl) {
    auto tag = device->getDeviceInOrderCounterAllocator()->getTag();

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto gpuVa1 = immCmdList1->inOrderExecInfo->getBaseDeviceAddress();

    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();
    auto gpuVa2 = immCmdList2->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(alignUp(gpuVa1 + (device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset() * 2), MemoryConstants::cacheLineSize), gpuVa2);

    // allocation from the same allocator
    EXPECT_EQ(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation(), tag->getBaseGraphicsAllocation()->getGraphicsAllocation(0));

    immCmdList1.reset();

    auto immCmdList3 = createImmCmdList<gfxCoreFamily>();
    auto gpuVa3 = immCmdList3->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuVa1, gpuVa3);

    immCmdList2.reset();

    auto immCmdList4 = createImmCmdList<gfxCoreFamily>();
    auto gpuVa4 = immCmdList4->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuVa2, gpuVa4);

    tag->returnTag();
}

HWTEST2_F(InOrderCmdListTests, whenCreatingInOrderExecInfoThenReuseHostAlloc, IsAtLeastSkl) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto tag = device->getHostInOrderCounterAllocator()->getTag();

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto gpuVa1 = immCmdList1->inOrderExecInfo->getBaseHostAddress();

    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();
    auto gpuVa2 = immCmdList2->inOrderExecInfo->getBaseHostAddress();

    EXPECT_NE(gpuVa1, gpuVa2);

    // allocation from the same allocator
    EXPECT_EQ(immCmdList1->inOrderExecInfo->getHostCounterAllocation(), tag->getBaseGraphicsAllocation()->getGraphicsAllocation(0));

    immCmdList1.reset();

    auto immCmdList3 = createImmCmdList<gfxCoreFamily>();
    auto gpuVa3 = immCmdList3->inOrderExecInfo->getBaseHostAddress();

    EXPECT_EQ(gpuVa1, gpuVa3);

    immCmdList2.reset();

    auto immCmdList4 = createImmCmdList<gfxCoreFamily>();
    auto gpuVa4 = immCmdList4->inOrderExecInfo->getBaseHostAddress();

    EXPECT_EQ(gpuVa2, gpuVa4);

    tag->returnTag();
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventWhenAppendEventResetCalledThenReturnError, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(3, false);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendEventReset(events[0]->toHandle()));
}

HWTEST2_F(InOrderCmdListTests, givenRegularEventWithTemporaryInOrderDataAssignmentWhenCallingSynchronizeOrResetThenUnset, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->makeCounterBasedImplicitlyDisabled();

    auto nonWalkerSignallingSupported = immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

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

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->inOrderExecInfo.get() != nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->reset());
    EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWheUsingRegularEventThenSetInOrderParamsOnlyWhenChainingIsRequired, IsAtLeastSkl) {
    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_FALSE(events[0]->isCounterBased());

    if (immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get())) {
        EXPECT_EQ(events[0]->inOrderExecSignalValue, 1u);
        EXPECT_NE(events[0]->inOrderExecInfo.get(), nullptr);
        EXPECT_EQ(events[0]->inOrderAllocationOffset, counterOffset);
    } else {
        EXPECT_EQ(events[0]->inOrderExecSignalValue, 0u);
        EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);
        EXPECT_EQ(events[0]->inOrderAllocationOffset, 0u);
    }

    auto copyImmCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    uint32_t copyData = 0;
    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    copyImmCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, events[0]->toHandle(), 0, nullptr, false, false);

    EXPECT_FALSE(events[0]->isCounterBased());
    EXPECT_EQ(events[0]->inOrderExecSignalValue, 0u);
    EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);
    EXPECT_EQ(events[0]->inOrderAllocationOffset, 0u);

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenRegularEventWithInOrderExecInfoWhenReusedOnRegularCmdListThenUnsetInOrderData, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled();

    auto nonWalkerSignallingSupported = immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get());

    EXPECT_TRUE(immCmdList->isInOrderExecutionEnabled());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(nonWalkerSignallingSupported, events[0]->inOrderExecInfo.get() != nullptr);

    immCmdList->inOrderExecInfo.reset();
    EXPECT_FALSE(immCmdList->isInOrderExecutionEnabled());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetAndSingleTileCmdListWhenAskingForAtomicSignallingThenReturnTrue, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_FALSE(immCmdList->inOrderAtomicSignalingEnabled);
    EXPECT_EQ(1u, immCmdList->getInOrderIncrementValue());

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    EXPECT_TRUE(immCmdList2->inOrderAtomicSignalingEnabled);
    EXPECT_EQ(1u, immCmdList2->getInOrderIncrementValue());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenSubmittingThenProgramSemaphoreForPreviousDispatch, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

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
}

HWTEST2_F(InOrderCmdListTests, givenTimestmapEventWhenProgrammingBarrierThenDontAddPipeControl, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

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

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenDispatchingStoreDataImmThenProgramUserInterrupt, IsAtLeastSkl) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    debugManager.flags.ProgramUserInterruptOnResolvedDependency.set(1);

    auto eventPool = createEvents<FamilyType>(2, false);
    auto eventHandle = events[0]->toHandle();
    events[0]->makeCounterBasedInitiallyDisabled();

    EXPECT_FALSE(events[1]->isKmdWaitModeEnabled());
    EXPECT_FALSE(events[1]->isInterruptModeEnabled());

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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

        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());

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

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenWaitingForEventFromPreviousAppendThenSkip, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

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

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenWaitingForEventFromPreviousAppendOnRegularCmdListThenSkip, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

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

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenWaitingForRegularEventFromPreviousAppendThenSkip, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled();
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    immCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, eventHandle, 0, nullptr, false, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, nullptr, 1, &eventHandle, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor); // implicit dependency

    itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderCmdListWhenWaitingOnHostThenDontProgramSemaphoreAfterWait, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    *hostAddress = 3;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    immCmdList->hostSynchronize(1, false);

    auto offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventModeWhenSubmittingThenProgramSemaphoreOnlyForExternalEvent, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint32_t counterOffset = 64;
    uint32_t counterOffset2 = counterOffset + 32;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);
    immCmdList2->inOrderExecInfo->setAllocationOffset(counterOffset2);

    auto eventPool = createEvents<FamilyType>(2, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();
    auto event1Handle = events[1]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, event1Handle, 0, nullptr, launchParams, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    ze_event_handle_t waitlist[] = {event0Handle, event1Handle};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 2, waitlist, launchParams, false);

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
}

HWTEST2_F(InOrderCmdListTests, givenImplicitEventConvertionEnabledWhenUsingImmediateCmdListThenConvertEventToCounterBased, IsAtLeastSkl) {
    debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.set(0);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto outOfOrderImmCmdList = createImmCmdList<gfxCoreFamily>();
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    outOfOrderImmCmdList->inOrderExecInfo.reset();

    auto eventPool = createEvents<FamilyType>(3, false);
    events[0]->makeCounterBasedInitiallyDisabled();
    events[1]->makeCounterBasedInitiallyDisabled();
    events[2]->makeCounterBasedInitiallyDisabled();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_FALSE(events[0]->isCounterBased());

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[1]->counterBasedMode);
    EXPECT_EQ(0u, events[1]->counterBasedFlags);
    EXPECT_FALSE(events[1]->isCounterBased());

    debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.set(-1);

    bool dcFlushRequired = immCmdList->getDcFlushRequired(true);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
        EXPECT_EQ(0u, events[0]->counterBasedFlags);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
        EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE), events[0]->counterBasedFlags);
    }
    EXPECT_NE(dcFlushRequired, events[0]->isCounterBased());

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);

    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[1]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[1]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[1]->counterBasedFlags);
    EXPECT_FALSE(events[1]->isCounterBased());

    outOfOrderImmCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[2]->toHandle(), 0, nullptr, launchParams, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[2]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[2]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[2]->counterBasedFlags);
    EXPECT_FALSE(events[2]->isCounterBased());

    // Reuse on Regular = disable
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_FALSE(events[0]->isCounterBased());

    // Reuse on non-inOrder = disable
    events[0]->counterBasedMode = Event::CounterBasedMode::implicitlyEnabled;
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_EQ(dcFlushRequired, events[0]->isCounterBased());

    // Reuse on already disabled
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    }
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
    EXPECT_EQ(dcFlushRequired, events[0]->isCounterBased());

    // On explicitly enabled
    events[0]->counterBasedMode = Event::CounterBasedMode::explicitlyEnabled;
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_EQ(Event::CounterBasedMode::explicitlyEnabled, events[0]->counterBasedMode);
    EXPECT_TRUE(events[0]->isCounterBased());
}

HWTEST2_F(InOrderCmdListTests, givenImplicitEventConvertionEnabledWhenUsingAppendResetThenImplicitlyDisable, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled();
    events[0]->enableCounterBasedMode(false, eventPool->getCounterBasedFlags());

    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_EQ(Event::CounterBasedMode::implicitlyDisabled, events[0]->counterBasedMode);
    EXPECT_EQ(0u, events[0]->counterBasedFlags);
}

HWTEST2_F(InOrderCmdListTests, givenImplicitEventConvertionEnabledWhenCallingAppendThenHandleInOrderExecInfo, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled();
    events[0]->enableCounterBasedMode(false, eventPool->getCounterBasedFlags());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());

    events[0]->reset();
    EXPECT_EQ(0u, events[0]->inOrderExecSignalValue);
    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(2u, events[0]->inOrderExecSignalValue);
    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());

    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_EQ(0u, events[0]->inOrderExecSignalValue);
    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());
}

HWTEST2_F(InOrderCmdListTests, givenCmdsChainingWhenDispatchingKernelThenProgramSemaphoreOnce, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled();

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

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(1); // chaining
    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    findSemaphores(0); // no implicit dependency semaphore
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(3u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(4u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(5u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(6u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(7u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, nullptr, 0, nullptr, false);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(8u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(9u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), nullptr, 0, nullptr, false);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(10u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining
    EXPECT_EQ(11u, immCmdList->inOrderExecInfo->getCounterValue());

    offset = cmdStream->getUsed();

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    findSemaphores(0); // no implicit dependency
    EXPECT_EQ(12u, immCmdList->inOrderExecInfo->getCounterValue());

    context->freeMem(alloc);
}

HWTEST2_F(InOrderCmdListTests, givenImmediateCmdListWhenDispatchingWithRegularEventThenSwitchToCounterBased, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto copyOnlyCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

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

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, cooperativeParams, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), eventHandle, 0, nullptr, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    size_t rangeSizes = 1;
    const void **ranges = reinterpret_cast<const void **>(&copyData[0]);
    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, eventHandle, 0, nullptr);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled();
    copyOnlyCmdList->appendMemoryCopyBlitRegion(&allocationData, &allocationData, region, region, {0, 0, 0}, 0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}, events[0].get(), 0, nullptr, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, false, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, eventHandle, 0, nullptr, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled();
    copyOnlyCmdList->appendBlitFill(alloc, &copyData, 1, 16, events[0].get(), 0, nullptr, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendSignalEvent(eventHandle);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(copyData), eventHandle, 0, nullptr);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), copyData, 1, eventHandle, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    *hostAddress = immCmdList->inOrderExecInfo->getCounterValue();

    immCmdList->copyThroughLockedPtrEnabled = true;
    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendMemoryCopy(alloc, &copyData, 1, eventHandle, 0, nullptr, false, false);
    if (dcFlushRequired) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    context->freeMem(alloc);
}

HWTEST2_F(InOrderCmdListTests, givenCounterBasedEventWithIncorrectFlagsWhenPassingAsSignalEventThenReturnError, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();

    events[0]->counterBasedFlags = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));

    events[0]->counterBasedFlags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));

    events[0]->counterBasedFlags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));

    events[0]->counterBasedFlags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));
}

HWTEST2_F(InOrderCmdListTests, givenNonInOrderCmdListWhenPassingCounterBasedEventThenReturnError, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo.reset();
    EXPECT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto copyOnlyCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();
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

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false));

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, cooperativeParams, false));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), eventHandle, 0, nullptr, false));

    size_t rangeSizes = 1;
    const void **ranges = reinterpret_cast<const void **>(&copyData[0]);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, copyOnlyCmdList->appendMemoryCopyBlitRegion(&allocationData, &allocationData, region, region, {0, 0, 0}, 0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}, events[0].get(), 0, nullptr, false));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, false, false));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, eventHandle, 0, nullptr, false));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, copyOnlyCmdList->appendBlitFill(alloc, &copyData, 1, 16, events[0].get(), 0, nullptr, false));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendSignalEvent(eventHandle));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(copyData), eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendBarrier(eventHandle, 0, nullptr, false));

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), copyData, 1, eventHandle, false));

    immCmdList->copyThroughLockedPtrEnabled = true;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendMemoryCopy(alloc, &copyData, 1, eventHandle, 0, nullptr, false, false));

    context->freeMem(alloc);
}

HWTEST2_F(InOrderCmdListTests, givenCmdsChainingFromAppendCopyWhenDispatchingKernelThenProgramSemaphoreOnce, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled();

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

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    uint32_t numSemaphores = immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope())) ? 1 : 2;

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(alloc, alloc, 1, eventHandle, 0, nullptr, false, false);
    findSemaphores(numSemaphores); // implicit dependency + optional chaining

    numSemaphores = immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope())) ? 1 : 0;

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(alloc, alloc, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(numSemaphores); // implicit dependency for Compact event or no semaphores for non-compact

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(alloc, &region, 1, 1, alloc, &region, 1, 1, eventHandle, 0, nullptr, false, false);
    findSemaphores(2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(alloc, &region, 1, 1, alloc, &region, 1, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(0); // no implicit dependency

    context->freeMem(alloc);
}

HWTEST2_F(InOrderCmdListTests, givenCmdsChainingFromAppendCopyAndFlushRequiredWhenDispatchingKernelThenProgramSemaphoreOnce, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled();
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
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto dcFlushRequired = immCmdList->getDcFlushRequired(true);

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, false, false);
    findSemaphores(dcFlushRequired ? 1 : 2); // implicit dependency + timestamp chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(dcFlushRequired ? 1 : 0); // implicit dependency or already waited on previous call

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, eventHandle, 0, nullptr, false, false);
    findSemaphores(2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(0); // no implicit dependency
}

HWTEST2_F(InOrderCmdListTests, givenEventWithRequiredPipeControlWhenDispatchingCopyThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using WalkerVariant = typename FamilyType::WalkerVariant;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    void *alloc = allocDeviceMem(16384u);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(alloc, alloc, 1, eventHandle, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

    if (immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope()))) {
        EXPECT_NE(cmdList.end(), sdiItor);
    } else {
        EXPECT_EQ(cmdList.end(), sdiItor);

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);
        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([&immCmdList](auto &&walker) {
            auto &postSync = walker->getPostSync();

            using PostSyncType = std::decay_t<decltype(postSync)>;

            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(1u, postSync.getImmediateData());
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
        },
                   walkerVariant);
    }

    context->freeMem(alloc);
}

HWTEST2_F(InOrderCmdListTests, givenEventWithRequiredPipeControlAndAllocFlushWhenDispatchingCopyThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using WalkerVariant = typename FamilyType::WalkerVariant;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    uint32_t copyData = 0;

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, false, false);

    auto dcFlushRequired = immCmdList->getDcFlushRequired(true);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    if (immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope()))) {
        EXPECT_NE(cmdList.end(), sdiItor);
    } else {
        if (dcFlushRequired) {
            EXPECT_NE(cmdList.end(), sdiItor);

            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());

        } else {
            EXPECT_EQ(cmdList.end(), sdiItor);
        }

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([&immCmdList, &dcFlushRequired](auto &&walker) {
            auto &postSync = walker->getPostSync();
            if (dcFlushRequired) {
                EXPECT_NE(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
            } else {
                EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
            }
        },
                   walkerVariant);
    }
}

HWTEST2_F(InOrderCmdListTests, givenCmdsChainingWhenDispatchingKernelWithRelaxedOrderingThenProgramAllDependencies, IsAtLeastXeHpCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();
    size_t offset = 0;

    auto findConditionalBbStarts = [&](size_t expectedNumBbStarts) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());

        EXPECT_EQ(expectedNumBbStarts, cmds.size());
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findConditionalBbStarts(1); // chaining

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    findConditionalBbStarts(1); // implicit dependency
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventModeWhenWaitingForEventFromPreviousAppendThenSkip, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams, false);

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

HWTEST2_F(InOrderCmdListTests, givenInOrderEventModeWhenSubmittingFromDifferentCmdListThenProgramSemaphoreForEvent, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    EXPECT_EQ(nullptr, immCmdList1->inOrderExecInfo->getHostCounterAllocation());
    EXPECT_EQ(nullptr, immCmdList2->inOrderExecInfo->getHostCounterAllocation());

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getDeviceCounterAllocation()]);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams, false);

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

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenDispatchingThenEnsureHostAllocationResidency, IsAtLeastSkl) {
    NEO::debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto event0Handle = events[0]->toHandle();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    EXPECT_NE(nullptr, immCmdList1->inOrderExecInfo->getHostCounterAllocation());
    EXPECT_NE(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList1->inOrderExecInfo->getHostCounterAllocation());
    EXPECT_NE(nullptr, immCmdList2->inOrderExecInfo->getHostCounterAllocation());
    EXPECT_NE(immCmdList2->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList2->inOrderExecInfo->getHostCounterAllocation());

    EXPECT_EQ(AllocationType::bufferHostMemory, immCmdList1->inOrderExecInfo->getHostCounterAllocation()->getAllocationType());
    EXPECT_EQ(immCmdList1->inOrderExecInfo->getBaseHostAddress(), immCmdList1->inOrderExecInfo->getHostCounterAllocation()->getUnderlyingBuffer());
    EXPECT_FALSE(immCmdList1->inOrderExecInfo->getHostCounterAllocation()->isAllocatedInLocalMemoryPool());

    EXPECT_EQ(immCmdList1->inOrderExecInfo->getHostCounterAllocation(), immCmdList2->inOrderExecInfo->getHostCounterAllocation());

    auto hostAllocOffset = ptrDiff(immCmdList2->inOrderExecInfo->getBaseHostAddress(), immCmdList1->inOrderExecInfo->getBaseHostAddress());
    EXPECT_NE(0u, hostAllocOffset);

    EXPECT_EQ(AllocationType::bufferHostMemory, immCmdList2->inOrderExecInfo->getHostCounterAllocation()->getAllocationType());
    EXPECT_EQ(immCmdList2->inOrderExecInfo->getBaseHostAddress(), ptrOffset(immCmdList2->inOrderExecInfo->getHostCounterAllocation()->getUnderlyingBuffer(), hostAllocOffset));
    EXPECT_FALSE(immCmdList2->inOrderExecInfo->getHostCounterAllocation()->isAllocatedInLocalMemoryPool());

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getHostCounterAllocation()]);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams, false);

    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[immCmdList1->inOrderExecInfo->getHostCounterAllocation()]);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventModeWhenSubmittingThenClearEventCsrList, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    UltCommandStreamReceiver<FamilyType> tempCsr(*device->getNEODevice()->getExecutionEnvironment(), 0, 1);

    auto eventPool = createEvents<FamilyType>(1, false);

    events[0]->csrs.clear();
    events[0]->csrs.push_back(&tempCsr);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[0]->csrs[0]);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenDispatchingThenHandleDependencyCounter, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_NE(nullptr, immCmdList->inOrderExecInfo.get());
    EXPECT_EQ(AllocationType::timestampPacketTagBuffer, immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getAllocationType());

    EXPECT_EQ(0u, immCmdList->inOrderExecInfo->getCounterValue());

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[immCmdList->inOrderExecInfo->getDeviceCounterAllocation()]);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[immCmdList->inOrderExecInfo->getDeviceCounterAllocation()]);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenAddingRelaxedOrderingEventsThenConfigureRegistersFirst, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->addEventsToCmdList(0, nullptr, nullptr, true, true, true, false);

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

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingWalkerThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    bool isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([&immCmdList, counterOffset](auto &&walker) {
            auto &postSync = walker->getPostSync();

            using PostSyncType = std::decay_t<decltype(postSync)>;

            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(1u, postSync.getImmediateData());
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, postSync.getDestinationAddress());
        },
                   walkerVariant);
    }

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([&cmdList, &immCmdList, &walkerItor, isCompactEvent, eventEndGpuVa, counterOffset](auto &&walker) {
            auto &postSync = walker->getPostSync();
            using PostSyncType = std::decay_t<decltype(postSync)>;

            if (isCompactEvent) {
                EXPECT_EQ(PostSyncType::OPERATION::OPERATION_NO_WRITE, postSync.getOperation());

                auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
                ASSERT_NE(cmdList.end(), pcItor);

                auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(pcItor, cmdList.end());
                ASSERT_NE(cmdList.end(), semaphoreItor);

                auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
                ASSERT_NE(nullptr, semaphoreCmd);

                EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
                EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
                EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());

                auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
                ASSERT_NE(nullptr, sdiCmd);

                EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, sdiCmd->getAddress());
                EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
                EXPECT_EQ(2u, sdiCmd->getDataDword0());
            } else {
                EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
                EXPECT_EQ(2u, postSync.getImmediateData());
                EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress() + counterOffset, postSync.getDestinationAddress());
            }
        },
                   walkerVariant);
    }

    auto hostAddress = static_cast<uint64_t *>(ptrOffset(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer(), counterOffset));

    *hostAddress = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(1));

    *hostAddress = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));

    *hostAddress = 3;
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingTimestampEventThenClearAndChainWithSyncAllocSignaling, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerVariant = typename FamilyType::WalkerVariant;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(events[0]->getCompletionFieldGpuAddress(device), sdiCmd->getAddress());
    EXPECT_EQ(0u, sdiCmd->getStoreQword());
    EXPECT_EQ(Event::STATE_CLEARED, sdiCmd->getDataDword0());

    auto eventBaseGpuVa = events[0]->getPacketAddress(device);
    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(sdiItor, cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
    std::visit([eventBaseGpuVa, eventEndGpuVa, &immCmdList, &sdiCmd](auto &&walker) {
        auto &postSync = walker->getPostSync();
        using PostSyncType = std::decay_t<decltype(postSync)>;

        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
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
    },
               walkerVariant);
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenAskingIfSkipInOrderNonWalkerSignallingAllowedThenReturnTrue, IsAtLeastXeHpcCore) {
    debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);
    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_TRUE(immCmdList->skipInOrderNonWalkerSignalingAllowed(events[0].get()));
}

HWTEST2_F(InOrderCmdListTests, givenRelaxedOrderingWhenProgrammingTimestampEventThenClearAndChainWithSyncAllocSignalingAsTwoSeparateSubmissions, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerVariant = typename FamilyType::WalkerVariant;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool kernelOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent) override {
            flushData.push_back(this->cmdListCurrentStartOffset);

            this->cmdListCurrentStartOffset = this->commandContainer.getCommandStream()->getUsed();

            return ZE_RESULT_SUCCESS;
        }

        std::vector<size_t> flushData; // start_offset
    };

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdListImpl<gfxCoreFamily, MyMockCmdList>(false);

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

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(sdiItor, cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto eventBaseGpuVa = events[0]->getPacketAddress(device);

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([eventBaseGpuVa, &cmdStream, &immCmdList](auto &&walker) {
            auto &postSync = walker->getPostSync();
            using PostSyncType = std::decay_t<decltype(postSync)>;

            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
            EXPECT_EQ(eventBaseGpuVa, postSync.getDestinationAddress());

            auto walkerOffset = ptrDiff(walker, cmdStream->getCpuBase());
            EXPECT_TRUE(walkerOffset >= immCmdList->flushData[0]);
            EXPECT_TRUE(walkerOffset < immCmdList->flushData[1]);
        },
                   walkerVariant);
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

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenChainingWithRelaxedOrderingThenSignalAsSingleSubmission, IsAtLeastXeHpcCore) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool kernelOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent) override {
            flushCount++;

            return ZE_RESULT_SUCCESS;
        }

        uint32_t flushCount = 0;
    };

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.EnableInOrderRelaxedOrderingForEventsChaining.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdListImpl<gfxCoreFamily, MyMockCmdList>(false);

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    immCmdList->inOrderExecInfo->addCounterValue(1);

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));

    EXPECT_EQ(0u, immCmdList->flushCount);

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    ASSERT_EQ(1u, immCmdList->flushCount);
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingRegularEventThenClearAndChainWithSyncAllocSignaling, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerVariant = typename FamilyType::WalkerVariant;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->signalScope = 0;
    events[0]->makeCounterBasedImplicitlyDisabled();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(events[0]->getCompletionFieldGpuAddress(device), sdiCmd->getAddress());
    EXPECT_EQ(0u, sdiCmd->getStoreQword());
    EXPECT_EQ(Event::STATE_CLEARED, sdiCmd->getDataDword0());

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(sdiItor, cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto eventBaseGpuVa = events[0]->getPacketAddress(device);
    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
    std::visit([eventBaseGpuVa, eventEndGpuVa, &sdiCmd, &immCmdList](auto &&walker) {
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
    },
               walkerVariant);
}

HWTEST2_F(InOrderCmdListTests, givenHostVisibleEventOnLatestFlushWhenCallingSynchronizeThenUseInOrderSync, IsAtLeastSkl) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    EXPECT_FALSE(immCmdList->latestFlushIsHostVisible);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_EQ(immCmdList->dcFlushSupport ? false : true, immCmdList->latestFlushIsHostVisible);

    EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    immCmdList->hostSynchronize(0, false);

    if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    events[0]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_TRUE(immCmdList->latestFlushIsHostVisible);

    immCmdList->hostSynchronize(0, false);

    if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(2u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }

    // handle post sync operations
    immCmdList->hostSynchronize(0, true);

    if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    } else {
        EXPECT_EQ(2u, immCmdList->synchronizeInOrderExecutionCalled);
        EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    }
}

HWTEST2_F(InOrderCmdListTests, givenEmptyTempAllocationsStorageWhenCallingSynchronizeThenUseInternalCounter, IsAtLeastSkl) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_TRUE(immCmdList->latestFlushIsHostVisible);

    EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    immCmdList->hostSynchronize(0, true);

    EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    immCmdList->hostSynchronize(0, true);

    EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
}

using NonPostSyncWalkerMatcher = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_GEN12LP_CORE>;

HWTEST2_F(InOrderCmdListTests, givenNonPostSyncWalkerWhenPatchingThenThrow, NonPostSyncWalkerMatcher) {
    InOrderPatchCommandHelpers::PatchCmd<FamilyType> incorrectCmd(nullptr, nullptr, nullptr, 1, NEO::InOrderPatchCommandHelpers::PatchCmdType::none, false, false);

    EXPECT_ANY_THROW(incorrectCmd.patch(1));

    InOrderPatchCommandHelpers::PatchCmd<FamilyType> walkerCmd(nullptr, nullptr, nullptr, 1, NEO::InOrderPatchCommandHelpers::PatchCmdType::walker, false, false);

    EXPECT_ANY_THROW(walkerCmd.patch(1));
}

HWTEST2_F(InOrderCmdListTests, givenNonPostSyncWalkerWhenAskingForNonWalkerSignalingRequiredThenReturnFalse, NonPostSyncWalkerMatcher) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool1 = createEvents<FamilyType>(1, true);
    auto eventPool2 = createEvents<FamilyType>(1, false);
    auto eventPool3 = createEvents<FamilyType>(1, false);
    events[2]->makeCounterBasedInitiallyDisabled();

    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[2].get()));
}

HWTEST2_F(InOrderCmdListTests, givenMultipleAllocationsForWriteWhenAskingForNonWalkerSignalingRequiredThenReturnTrue, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool0 = createEvents<FamilyType>(1, true);
    auto eventPool1 = createEvents<FamilyType>(1, false);
    auto eventPool2 = createEvents<FamilyType>(1, false);
    events[2]->makeCounterBasedInitiallyDisabled();

    bool isCompactEvent0 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));
    bool isCompactEvent1 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[1]->isSignalScope()));
    bool isCompactEvent2 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[2]->isSignalScope()));

    EXPECT_TRUE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_EQ(isCompactEvent1, immCmdList->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_TRUE(immCmdList->isInOrderNonWalkerSignalingRequired(events[2].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(nullptr));

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    EXPECT_EQ(isCompactEvent0, immCmdList2->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_EQ(isCompactEvent1, immCmdList2->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_EQ(isCompactEvent2, immCmdList2->isInOrderNonWalkerSignalingRequired(events[2].get()));
    EXPECT_FALSE(immCmdList2->isInOrderNonWalkerSignalingRequired(nullptr));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingWalkerThenProgramPipeControlWithSignalAllocation, NonPostSyncWalkerMatcher) {
    using WALKER = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(64);
    immCmdList->inOrderExecInfo->addCounterValue(123);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pcCmd->getPostSyncOperation());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t expectedAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress() + immCmdList->inOrderExecInfo->getAllocationOffset();

    EXPECT_EQ(expectedAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(immCmdList->inOrderExecInfo->getCounterValue(), sdiCmd->getDataDword0());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitThenProgramPcAndSignalAlloc, NonPostSyncWalkerMatcher) {
    using WALKER = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
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

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, false, false);
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

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendSignalEventThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendSignalEvent(events[0]->toHandle());

    uint64_t inOrderSyncVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

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

        EXPECT_EQ(inOrderSyncVa, sdiCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(2u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
    }
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingNonKernelAppendThenWaitForDependencyAndSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->makeCounterBasedInitiallyDisabled();

    uint64_t inOrderSyncVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    uint8_t ptr[64] = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    uint32_t inOrderCounter = 1;

    auto verifySdi = [&inOrderSyncVa, &immCmdList](GenCmdList::reverse_iterator rIterator, GenCmdList::reverse_iterator rEnd, uint64_t signalValue) {
        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*rIterator);
        while (sdiCmd == nullptr) {
            sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++rIterator));
            if (rIterator == rEnd) {
                break;
            }
        }

        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(inOrderSyncVa, sdiCmd->getAddress());
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

HWTEST2_F(InOrderCmdListTests, givenInOrderRegularCmdListWhenProgrammingAppendWithSignalEventThenAssignInOrderInfo, IsAtLeastSkl) {
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(2, false);

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(regularCmdList->inOrderExecInfo.get(), events[0]->inOrderExecInfo.get());

    uint32_t copyData = 0;
    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, events[1]->toHandle(), 0, nullptr, false, false);

    EXPECT_EQ(regularCmdList->inOrderExecInfo.get(), events[1]->inOrderExecInfo.get());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderRegularCmdListWhenProgrammingNonKernelAppendThenWaitForDependencyAndSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->makeCounterBasedInitiallyDisabled();

    uint8_t ptr[64] = {};

    uint64_t inOrderSyncVa = regularCmdList->inOrderExecInfo->getBaseDeviceAddress();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto verifySdi = [&inOrderSyncVa, &regularCmdList](GenCmdList::reverse_iterator rIterator, GenCmdList::reverse_iterator rEnd, uint64_t signalValue) {
        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*rIterator);
        while (sdiCmd == nullptr) {
            sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++rIterator));
            if (rIterator == rEnd) {
                break;
            }
        }

        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(inOrderSyncVa, sdiCmd->getAddress());
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

HWTEST2_F(InOrderCmdListTests, givenImmediateEventWhenWaitingFromRegularCmdListThenDontPatch, IsAtLeastSkl) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size());

    if (NonPostSyncWalkerMatcher::isMatched<productFamily>()) {
        EXPECT_EQ(NEO::InOrderPatchCommandHelpers::PatchCmdType::sdi, regularCmdList->inOrderPatchCmds[0].patchCmdType);
    } else {
        EXPECT_EQ(NEO::InOrderPatchCommandHelpers::PatchCmdType::walker, regularCmdList->inOrderPatchCmds[0].patchCmdType);
    }

    EXPECT_EQ(immCmdList->inOrderExecInfo->isAtomicDeviceSignalling(), regularCmdList->inOrderPatchCmds[0].deviceAtomicSignaling);
    EXPECT_EQ(immCmdList->inOrderExecInfo->isHostStorageDuplicated(), regularCmdList->inOrderPatchCmds[0].duplicatedHostStorage);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);
    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), semaphoreCmd->getSemaphoreGraphicsAddress());

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(semaphoreItor, cmdList.end());

    EXPECT_NE(cmdList.end(), walkerItor);
}

HWTEST2_F(InOrderCmdListTests, givenEventGeneratedByRegularCmdListWhenWaitingFromImmediateThenUseSubmissionCounter, IsAtLeastSkl) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto regularCmdListHandle = regularCmdList->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    uint64_t expectedCounterValue = regularCmdList->inOrderExecInfo->getCounterValue();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    verifySemaphore(expectedCounterValue);

    // 1 Execute call
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    verifySemaphore(expectedCounterValue);

    // 2 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    verifySemaphore(expectedCounterValue + expectedCounterAppendValue);

    // 3 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false, nullptr);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    verifySemaphore(expectedCounterValue + (expectedCounterAppendValue * 2));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitThenDontSignalFromWalker, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

    uint32_t walkersFound = 0;
    while (cmdList.end() != walkerItor) {
        walkersFound++;

        WalkerVariant walkerCmd = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);

        std::visit([](auto &&walker) {
            using WalkerType = std::decay_t<decltype(*walker)>;
            using PostSyncType = typename WalkerType::PostSyncType;

            auto &postSync = walker->getPostSync();
            EXPECT_EQ(PostSyncType::OPERATION_NO_WRITE, postSync.getOperation());
        },
                   walkerCmd);

        walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(++walkerItor, cmdList.end());
    }

    EXPECT_TRUE(walkersFound > 1);

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingCopyThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;

    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), copyItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(copyItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t syncVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingComputeCopyThenDontSingalFromSdi, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    void *alloc = allocDeviceMem(16384u);

    immCmdList->appendMemoryCopy(alloc, alloc, 1, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
    std::visit([&immCmdList](auto &&walker) {
        auto &postSync = walker->getPostSync();
        using PostSyncType = std::decay_t<decltype(postSync)>;

        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
    },
               walkerVariant);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    context->freeMem(alloc);
}

HWTEST2_F(InOrderCmdListTests, givenAlocFlushRequiredhenProgrammingComputeCopyThenSingalFromSdi, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto alignedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    immCmdList->appendMemoryCopy(alignedPtr, alignedPtr, 1, nullptr, 0, nullptr, false, false);

    auto dcFlushRequired = immCmdList->getDcFlushRequired(true);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
    std::visit([&dcFlushRequired](auto &&walker) {
        auto &postSync = walker->getPostSync();

        if (dcFlushRequired) {
            EXPECT_EQ(0u, postSync.getDestinationAddress());
        } else {
            EXPECT_NE(0u, postSync.getDestinationAddress());
        }
    },
               walkerVariant);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());

    if (dcFlushRequired) {
        EXPECT_NE(cmdList.end(), sdiItor);
        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    } else {
        EXPECT_EQ(cmdList.end(), sdiItor);
    }

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingFillThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto fillItor = findBltFillCmd<FamilyType>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), fillItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(fillItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t syncVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithSplitAndOutEventThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, (size / 2) + 1, events[0]->toHandle(), 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), walkerItor);

    auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);

    while (PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE == pcCmd->getPostSyncOperation()) {
        pcItor = find<PIPE_CONTROL *>(++pcItor, cmdList.end());
        ASSERT_NE(cmdList.end(), pcItor);

        pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
        ASSERT_NE(nullptr, pcCmd);
    }

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    uint64_t syncVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithSplitAndWithoutOutEventThenAddPipeControlSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, (size / 2) + 1, nullptr, 0, nullptr, false);

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

    uint64_t syncVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithoutSplitThenSignalByWalker, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using WalkerVariant = typename FamilyType::WalkerVariant;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
    std::visit([&immCmdList](auto &&walker) {
        auto &postSync = walker->getPostSync();
        using PostSyncType = std::decay_t<decltype(postSync)>;

        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(1u, postSync.getImmediateData());
        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
    },
               walkerVariant);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    context->freeMem(data);
}

HWTEST2_F(InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingCopyRegionThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), copyItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(copyItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t syncVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendWaitOnEventsThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

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

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreItor, 2, immCmdList->inOrderExecInfo->getBaseDeviceAddress(), immCmdList->isQwordInOrderCounter(), false));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(3u, sdiCmd->getDataDword0());
}

HWTEST2_F(InOrderCmdListTests, givenRegularInOrderCmdListWhenProgrammingAppendWaitOnEventsThenDontSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled();

    auto eventHandle = events[0]->toHandle();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

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

    uint64_t syncVa = regularCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(3u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingCounterWithOverflowThenHandleOffsetCorrectly, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->addCounterValue(std::numeric_limits<uint32_t>::max() - 1);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    bool useZeroOffset = false;
    uint64_t expectedCounter = 1;
    uint32_t expectedOffset = 0;

    for (uint32_t i = 0; i < 10; i++) {
        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

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
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingCounterWithOverflowThenHandleItCorrectly, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerVariant = typename FamilyType::WalkerVariant;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->addCounterValue(std::numeric_limits<uint32_t>::max() - 1);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    bool isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    auto eventHandle = events[0]->toHandle();

    uint64_t baseGpuVa = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), walkerItor);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());

    uint64_t expectedCounter = 1;
    uint32_t offset = 0;

    if (immCmdList->isQwordInOrderCounter()) {
        expectedCounter = std::numeric_limits<uint32_t>::max();

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([isCompactEvent, &semaphoreItor, &immCmdList, &cmdList, expectedCounter](auto &&walker) {
            auto &postSync = walker->getPostSync();
            using PostSyncType = std::decay_t<decltype(postSync)>;

            if (isCompactEvent) {
                EXPECT_NE(cmdList.end(), semaphoreItor);

                auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
                ASSERT_NE(cmdList.end(), sdiItor);

                auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
                ASSERT_NE(nullptr, sdiCmd);

                EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
                EXPECT_EQ(getLowPart(expectedCounter), sdiCmd->getDataDword0());
                EXPECT_EQ(getHighPart(expectedCounter), sdiCmd->getDataDword1());

                EXPECT_EQ(PostSyncType::OPERATION::OPERATION_NO_WRITE, postSync.getOperation());
            } else {
                EXPECT_EQ(cmdList.end(), semaphoreItor);

                EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
                EXPECT_EQ(expectedCounter, postSync.getImmediateData());
                EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
            }
        },
                   walkerVariant);

    } else {
        ASSERT_NE(cmdList.end(), semaphoreItor);

        if (isCompactEvent) {
            // commands chaining
            semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
            ASSERT_NE(cmdList.end(), semaphoreItor);
        }

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(baseGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
        ASSERT_NE(nullptr, sdiCmd);

        offset = device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset();

        EXPECT_EQ(baseGpuVa + offset, sdiCmd->getAddress());
        EXPECT_EQ(1u, sdiCmd->getDataDword0());
    }

    EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(offset, immCmdList->inOrderExecInfo->getAllocationOffset());

    EXPECT_EQ(expectedCounter, events[0]->inOrderExecSignalValue);
    EXPECT_EQ(offset, events[0]->inOrderAllocationOffset);
}

HWTEST2_F(InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingBarrierThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList1 = createCopyOnlyImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    uint32_t copyData = 0;

    immCmdList1->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, false, false);

    auto offset = cmdStream->getUsed();

    immCmdList2->appendBarrier(nullptr, 1, &eventHandle, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(immCmdList2->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList2->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithWaitlistThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

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

    EXPECT_EQ(immCmdList2->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList2->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenStandaloneCbEventWhenDispatchingThenProgramCorrectly, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto event = createStandaloneCbEvent(nullptr);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventHandle = event->toHandle();

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false));

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

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistThenInheritSignalSyncAllocation, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendBarrier(nullptr, 0, nullptr, false);
    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

    EXPECT_EQ(offset, cmdStream->getUsed());

    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
}

HWTEST2_F(InOrderCmdListTests, givenRegularCmdListWhenProgrammingAppendBarrierWithoutWaitlistThenInheritSignalSyncAllocation, IsAtLeastSkl) {
    auto cmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = cmdList->getCmdContainer().getCommandStream();

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, cmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    cmdList->appendBarrier(nullptr, 0, nullptr, false);
    cmdList->appendBarrier(eventHandle, 0, nullptr, false);

    EXPECT_EQ(offset, cmdStream->getUsed());

    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithDifferentEventsThenDontInherit, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(3, false);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

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

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAndTimestampEventThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

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

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAndRegularEventThenSignalSyncAllocation, IsAtLeastSkl) {
    using MI_NOOP = typename FamilyType::MI_NOOP;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled();

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

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenCallingSyncThenHandleCompletion, IsAtLeastXeHpCore) {
    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
    auto hostAddress = static_cast<uint64_t *>(ptrOffset(deviceAlloc->getUnderlyingBuffer(), counterOffset));
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
        EXPECT_EQ(downloadedAlloc, deviceAlloc);
        EXPECT_EQ(1u, callCounter);
        EXPECT_EQ(1u, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(0u, *hostAddress);
    }

    // timeout - not ready
    {
        forceFail = true;
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, deviceAlloc);
        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // gpu hang
    {
        ultCsr->forceReturnGpuHang = true;

        EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, deviceAlloc);

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
        EXPECT_EQ(downloadedAlloc, deviceAlloc);

        EXPECT_EQ(failCounter, callCounter);
        EXPECT_EQ(failCounter - 1, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(1u, *hostAddress);
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    *ultCsr->getTagAddress() = ultCsr->taskCount - 1;

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, true));

    *ultCsr->getTagAddress() = ultCsr->taskCount + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, true));
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenCallingSyncThenHandleCompletionOnHostAlloc, IsAtLeastXeHpCore) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

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

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    *ultCsr->getTagAddress() = ultCsr->taskCount - 1;

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, true));

    *ultCsr->getTagAddress() = ultCsr->taskCount + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, true));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenDoingCpuCopyThenSynchronize, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    *hostAddress = 0;

    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        callCounter++;
        if (callCounter >= failCounter) {
            (*hostAddress)++;
        }
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    events[0]->setIsCompleted();

    ultCsr->waitForCompletionWithTimeoutTaskCountCalled = 0;
    ultCsr->flushTagUpdateCalled = false;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 1, &eventHandle, false, false);

    EXPECT_EQ(3u, callCounter);
    EXPECT_EQ(1u, *hostAddress);
    EXPECT_EQ(2u, ultCsr->checkGpuHangDetectedCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenImmediateCmdListWhenDoingCpuCopyThenPassInfoToEvent, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(nullptr, events[0]->inOrderExecInfo.get());

    uint32_t hostCopyData = 0;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    *hostAddress = 3;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, false, false);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_EQ(0u, events[0]->inOrderExecSignalValue);
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
    EXPECT_FALSE(events[0]->isAlreadyCompleted());

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, false, false);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, false, false);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_EQ(2u, events[0]->inOrderExecSignalValue);
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenProfilingEventWhenDoingCpuCopyThenSetProfilingData, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
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

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    *hostAddress = 3;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle0, 0, nullptr, false, false);

    EXPECT_NE(nullptr, events[0]->inOrderExecInfo.get());
    EXPECT_TRUE(events[0]->isAlreadyCompleted());
    EXPECT_EQ(L0::Event::STATE_CLEARED, *static_cast<uint32_t *>(events[0]->getHostAddress()));

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle1, 0, nullptr, false, false);

    EXPECT_NE(nullptr, events[1]->inOrderExecInfo.get());
    EXPECT_TRUE(events[1]->isAlreadyCompleted());
    EXPECT_NE(L0::Event::STATE_CLEARED, *static_cast<uint32_t *>(events[1]->getHostAddress()));

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenEventCreatedFromPoolWhenItIsQueriedForAddressItReturnsProperAddressFromPool, IsAtLeastSkl) {
    auto eventPool = createEvents<FamilyType>(1, false);
    uint64_t counterValue = 0;
    uint64_t address = 0;

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, &counterValue, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, nullptr, &address));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(nullptr, &counterValue, &address));

    events[0]->makeCounterBasedImplicitlyDisabled();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(Event::State::STATE_SIGNALED, counterValue);
    EXPECT_EQ(address, events[0]->getCompletionFieldGpuAddress(events[0]->peekEventPool()->getDevice()));
}
HWTEST2_F(InOrderCmdListTests, givenEventCreatedFromPoolWithTimestampsWhenQueriedForAddressErrorIsReturned, IsAtLeastSkl) {
    auto eventPool = createEvents<FamilyType>(1, true);
    uint64_t counterValue = 0;
    uint64_t address = 0;

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
}

HWTEST2_F(InOrderCmdListTests, givenCorrectInputParamsWhenCreatingCbEventThenReturnSuccess, IsAtLeastSkl) {
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

    uint64_t addresss = 0;
    uint64_t value = 0;
    zexEventGetDeviceAddress(handle, &value, &addresss);

    EXPECT_EQ(addresss, eventObj->getInOrderExecInfo()->getBaseDeviceAddress());
    EXPECT_EQ(value, counterValue);

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
}

HWTEST2_F(InOrderCmdListTests, givenStandaloneEventWhenCallingSynchronizeThenReturnCorrectValue, IsAtLeastSkl) {
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

HWTEST2_F(InOrderCmdListTests, givenStandaloneCbEventWhenPassingExternalInterruptIdThenAssign, IsAtLeastSkl) {
    zex_intel_event_sync_mode_exp_desc_t syncModeDesc = {ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC};
    syncModeDesc.externalInterruptId = 123;

    ze_event_desc_t eventDesc = {};
    eventDesc.pNext = &syncModeDesc;

    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;
    auto event1 = createStandaloneCbEvent(reinterpret_cast<const ze_base_desc_t *>(&syncModeDesc));
    EXPECT_EQ(NEO::InterruptId::notUsed, event1->externalInterruptId);

    syncModeDesc.syncModeFlags = ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_EXTERNAL_INTERRUPT_WAIT;
    auto event2 = createStandaloneCbEvent(reinterpret_cast<const ze_base_desc_t *>(&syncModeDesc));
    EXPECT_EQ(syncModeDesc.externalInterruptId, event2->externalInterruptId);
}

HWTEST2_F(InOrderCmdListTests, givenStandaloneEventWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
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

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, false);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eHandle3, 0, nullptr, launchParams, false);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    zeEventDestroy(eHandle3);
    context->freeMem(hostAddress);
}

HWTEST2_F(InOrderCmdListTests, givenStandaloneEventAndKernelSplitWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
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

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eHandle1, 0, nullptr, false, false);
    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 1, &eHandle2, false, false);

    alignedFree(alignedPtr);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWTEST2_F(InOrderCmdListTests, givenStandaloneEventAndCopyOnlyCmdListWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
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

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, false);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, false);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWTEST2_F(InOrderCmdListTests, givenCounterBasedEventWhenAskingForEventAddressAndValueThenReturnCorrectValues, IsAtLeastSkl) {
    auto eventPool = createEvents<FamilyType>(1, false);
    uint64_t counterValue = -1;
    uint64_t address = -1;

    auto cmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto deviceAlloc = cmdList->inOrderExecInfo->getDeviceCounterAllocation();

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(2u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress(), address);

    cmdList->close();

    ze_command_queue_desc_t desc = {};
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);

    auto cmdListHandle = cmdList->toHandle();
    mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(4u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress(), address);

    events[0]->inOrderAllocationOffset = 0x12300;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexEventGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(4u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress() + events[0]->inOrderAllocationOffset, address);
}

HWTEST2_F(InOrderCmdListTests, wWhenUsingImmediateCmdListThenDontAddCmdsToPatch, IsAtLeastXeHpCore) {
    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    uint32_t copyData = 0;

    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    EXPECT_EQ(0u, immCmdList->inOrderPatchCmds.size());
}

HWTEST2_F(InOrderCmdListTests, givenRegularCmdListWhenResetCalledThenClearCmdsToPatch, IsAtLeastSkl) {
    auto cmdList = createRegularCmdList<gfxCoreFamily>(false);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_NE(0u, cmdList->inOrderPatchCmds.size());

    cmdList->reset();

    EXPECT_EQ(0u, cmdList->inOrderPatchCmds.size());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenGpuHangDetectedInCpuCopyPathThenReportError, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto eventPool = createEvents<FamilyType>(1, false);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;

    ultCsr->forceReturnGpuHang = true;

    auto status = immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, false, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, status);

    ultCsr->forceReturnGpuHang = false;

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithoutEventThenAddBarrierAndSignalCounter, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, false, false);

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

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithEventThenSignalCounter, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eventHandle, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto cmdItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pcCmd);

    while (PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE == pcCmd->getPostSyncOperation()) {
        cmdItor = find<PIPE_CONTROL *>(++cmdItor, cmdList.end());
        ASSERT_NE(cmdList.end(), cmdItor);

        pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pcCmd);
    }

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));

    while (sdiCmd == nullptr && cmdItor != cmdList.end()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));
    }

    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderCmdListTests, givenImplicitScalingEnabledWhenAskingForExtensionsThenReturnSyncDispatchExtension, IsAtLeastXeHpCore) {
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

struct MultiTileInOrderCmdListTests : public InOrderCmdListTests {
    void SetUp() override {
        NEO::debugManager.flags.CreateMultipleSubDevices.set(partitionCount);
        NEO::debugManager.flags.EnableImplicitScaling.set(4);

        InOrderCmdListTests::SetUp();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createMultiTileImmCmdList() {
        auto cmdList = createImmCmdList<gfxCoreFamily>();

        cmdList->partitionCount = partitionCount;

        return cmdList;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<gfxCoreFamily>>> createMultiTileRegularCmdList(bool copyOnly) {
        auto cmdList = createRegularCmdList<gfxCoreFamily>(copyOnly);

        cmdList->partitionCount = partitionCount;

        return cmdList;
    }

    const uint32_t partitionCount = 2;
};

HWTEST2_F(MultiTileInOrderCmdListTests, givenStandaloneEventWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
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

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, false);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eHandle3, 0, nullptr, launchParams, false);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    zeEventDestroy(eHandle3);
    context->freeMem(hostAddress);
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenStandaloneEventAndKernelSplitWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
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

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eHandle1, 0, nullptr, false, false);
    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 1, &eHandle2, false, false);

    alignedFree(alignedPtr);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenImplicitScalingEnabledWhenAskingForExtensionsThenReturnSyncDispatchExtension, IsAtLeastXeHpCore) {
    uint32_t count = 0;
    ze_result_t res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_driver_extension_properties_t> extensionProperties(count);

    res = driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &param) {
        return (strcmp(param.name, ZE_SYNCHRONIZED_DISPATCH_EXP_NAME) == 0);
    });

    if (device->getL0GfxCoreHelper().synchronizedDispatchSupported()) {
        EXPECT_NE(extensionProperties.end(), it);
    } else {
        EXPECT_EQ(extensionProperties.end(), it);
    }
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenStandaloneEventAndCopyOnlyCmdListWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
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

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, false);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, false);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenDebugFlagSetWhenAskingForAtomicSignallingThenReturnTrue, IsAtLeastXeHpCore) {
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    EXPECT_FALSE(immCmdList->inOrderAtomicSignalingEnabled);
    EXPECT_EQ(1u, immCmdList->getInOrderIncrementValue());

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    auto immCmdList2 = createMultiTileImmCmdList<gfxCoreFamily>();

    EXPECT_TRUE(immCmdList2->inOrderAtomicSignalingEnabled);
    EXPECT_EQ(partitionCount, immCmdList2->getInOrderIncrementValue());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenAtomicSignallingEnabledWhenSignallingCounterThenUseMiAtomicCmd, IsAtLeastXeHpCore) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    EXPECT_EQ(0u, immCmdList->inOrderExecInfo->getCounterValue());

    auto handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(partitionCount, immCmdList->inOrderExecInfo->getCounterValue());

    size_t offset = cmdStream->getUsed();

    immCmdList->appendWaitOnEvents(1, &handle, nullptr, false, false, true, false, false);

    EXPECT_EQ(partitionCount * 2, immCmdList->inOrderExecInfo->getCounterValue());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto miAtomics = findAll<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, miAtomics.size());

    auto atomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomics[0]);
    ASSERT_NE(nullptr, atomicCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*atomicCmd));
    EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_INCREMENT, atomicCmd->getAtomicOpcode());
    EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, atomicCmd->getDataSize());
    EXPECT_EQ(0u, atomicCmd->getReturnDataControl());
    EXPECT_EQ(0u, atomicCmd->getCsStall());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenDuplicatedCounterStorageAndAtomicSignallingEnabledWhenSignallingCounterThenUseMiAtomicAndSdiCmd, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    EXPECT_EQ(0u, immCmdList->inOrderExecInfo->getCounterValue());

    auto handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(partitionCount, immCmdList->inOrderExecInfo->getCounterValue());

    size_t offset = cmdStream->getUsed();

    immCmdList->appendWaitOnEvents(1, &handle, nullptr, false, false, true, false, false);

    EXPECT_EQ(partitionCount * 2, immCmdList->inOrderExecInfo->getCounterValue());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto miAtomics = findAll<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, miAtomics.size());

    auto atomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomics[0]);
    ASSERT_NE(nullptr, atomicCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*atomicCmd));
    EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_INCREMENT, atomicCmd->getAtomicOpcode());
    EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, atomicCmd->getDataSize());
    EXPECT_EQ(0u, atomicCmd->getReturnDataControl());
    EXPECT_EQ(0u, atomicCmd->getCsStall());

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++miAtomics[0]));
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(partitionCount * 2, sdiCmd->getDataDword0());
    EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenDuplicatedCounterStorageAndWithoutAtomicSignallingEnabledWhenSignallingCounterThenUseTwoSdiCmds, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    EXPECT_EQ(0u, immCmdList->inOrderExecInfo->getCounterValue());

    auto handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    size_t offset = cmdStream->getUsed();

    immCmdList->appendWaitOnEvents(1, &handle, nullptr, false, false, true, false, false);

    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->getCounterValue());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(2u, sdiCmds.size());

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(sdiCmds[0]));
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());

    sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(sdiCmds[1]));
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenAtomicSignallingEnabledWhenWaitingForDependencyThenUseOnlyOneSemaphore, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    auto immCmdList1 = createMultiTileImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto handle = events[0]->toHandle();

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(partitionCount, immCmdList1->inOrderExecInfo->getCounterValue());

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    size_t offset = cmdStream->getUsed();

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &handle, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u + (ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired() ? 1 : 0), semaphores.size());

    auto itor = cmdList.begin();

    // implicit dependency
    auto gpuAddress = immCmdList2->inOrderExecInfo->getBaseDeviceAddress();

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, partitionCount, gpuAddress, immCmdList2->isQwordInOrderCounter(), false));

    // event
    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, partitionCount, events[0]->inOrderExecInfo->getBaseDeviceAddress(), immCmdList2->isQwordInOrderCounter(), false));
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingWaitOnEventsThenHandleAllEventPackets, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    size_t offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        if (isCompactEvent) {
            ASSERT_NE(cmdList.end(), semaphoreItor);
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);

            ASSERT_NE(nullptr, semaphoreCmd);

            auto gpuAddress = events[0]->getCompletionFieldGpuAddress(device);

            while (gpuAddress != semaphoreCmd->getSemaphoreGraphicsAddress()) {
                semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
                ASSERT_NE(cmdList.end(), semaphoreItor);

                semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
                ASSERT_NE(nullptr, semaphoreCmd);
            }

            EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress, semaphoreCmd->getSemaphoreGraphicsAddress());

            semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress + events[0]->getSinglePacketSize(), semaphoreCmd->getSemaphoreGraphicsAddress());
        }
    }

    offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        if (immCmdList->isQwordInOrderCounter()) {
            std::advance(itor, 2);
        }

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);

        if (isCompactEvent) {
            ASSERT_EQ(nullptr, semaphoreCmd); // already waited on previous call
        } else {
            ASSERT_NE(nullptr, semaphoreCmd);

            if (immCmdList->isQwordInOrderCounter()) {
                std::advance(itor, -2);
            }

            auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

            ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, gpuAddress, immCmdList->isQwordInOrderCounter(), false));
            ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, gpuAddress + device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset(), immCmdList->isQwordInOrderCounter(), false));
        }
    }
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenSignalingSyncAllocationThenEnablePartitionOffset, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendSignalInOrderDependencyCounter(nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*cmdList.begin());
    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenCallingSyncThenHandleCompletion, IsAtLeastXeHpCore) {
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    auto hostAddress0 = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    auto hostAddress1 = ptrOffset(hostAddress0, device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());

    *hostAddress0 = 0;
    *hostAddress1 = 0;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    *hostAddress0 = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    *hostAddress0 = 0;
    *hostAddress1 = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    *hostAddress0 = 1;
    *hostAddress1 = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(0));

    *hostAddress0 = 3;
    *hostAddress1 = 3;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(0));
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingTimestampEventThenHandleChaining, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->signalScope = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      cmdStream->getCpuBase(),
                                                      cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
    ASSERT_NE(nullptr, semaphoreCmd);

    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    if (eventEndGpuVa != semaphoreCmd->getSemaphoreGraphicsAddress()) {
        semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);

        semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
        ASSERT_NE(nullptr, semaphoreCmd);
    }

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + events[0]->getSinglePacketSize(), semaphoreCmd->getSemaphoreGraphicsAddress());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingTimestampEventThenHandlePacketsChaining, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->signalScope = 0;

    immCmdList->signalAllEventPackets = true;
    events[0]->maxPacketCount = 4;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      cmdStream->getCpuBase(),
                                                      cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
    ASSERT_NE(nullptr, semaphoreCmd);

    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    if (eventEndGpuVa != semaphoreCmd->getSemaphoreGraphicsAddress()) {
        semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);

        semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
        ASSERT_NE(nullptr, semaphoreCmd);
    }

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    auto offset = events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    offset += events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    offset += events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());
}

HWTEST2_F(MultiTileInOrderCmdListTests, whenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    ASSERT_EQ(4u, regularCmdList->inOrderPatchCmds.size()); // Walker + 2x Semaphore + Walker

    WalkerVariant walkerVariantFromContainer1 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(regularCmdList->inOrderPatchCmds[0].cmd1);
    WalkerVariant walkerVariantFromContainer2 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(regularCmdList->inOrderPatchCmds[3].cmd1);
    std::visit([](auto &&walker1, auto &&walker2) {
        ASSERT_NE(nullptr, walker1);
        ASSERT_NE(nullptr, walker2);
    },
               walkerVariantFromContainer1, walkerVariantFromContainer2);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    WalkerVariant walkerVariantFromParser1 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itor);

    itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(++itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    WalkerVariant walkerVariantFromParser2 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itor);

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());

    std::visit([&](auto &&walkerFromParser1, auto &&walkerFromParser2, auto &&walkerFromContainer1, auto &&walkerFromContainer2) {
        auto verifyPatching = [&](uint64_t executionCounter) {
            auto appendValue = regularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

            EXPECT_EQ(1u + appendValue, walkerFromContainer1->getPostSync().getImmediateData());
            EXPECT_EQ(1u + appendValue, walkerFromParser1->getPostSync().getImmediateData());

            EXPECT_EQ(2u + appendValue, walkerFromContainer2->getPostSync().getImmediateData());
            EXPECT_EQ(2u + appendValue, walkerFromParser2->getPostSync().getImmediateData());
        };

        regularCmdList->close();

        auto handle = regularCmdList->toHandle();

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(0);

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(1);

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(2);
    },
               walkerVariantFromParser1, walkerVariantFromParser2, walkerVariantFromContainer1, walkerVariantFromContainer2);
}

struct BcsSplitInOrderCmdListTests : public InOrderCmdListTests {
    void SetUp() override {
        NEO::debugManager.flags.SplitBcsCopy.set(1);
        NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

        hwInfoBackup = std::make_unique<VariableBackup<HardwareInfo>>(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        defaultHwInfo->featureTable.ftrBcsInfo = 0b111111111;

        InOrderCmdListTests::SetUp();
    }

    bool verifySplit(uint64_t expectedTaskCount) {
        auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

        for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
            if (static_cast<CommandQueueImp *>(bcsSplit.cmdQs[0])->getTaskCount() != expectedTaskCount) {
                return false;
            }
        }

        return true;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createBcsSplitImmCmdList() {
        auto cmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

        auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

        ze_command_queue_desc_t desc = {};
        desc.ordinal = static_cast<uint32_t>(device->getNEODevice()->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

        cmdList->isBcsSplitNeeded = bcsSplit.setupDevice(device->getHwInfo().platform.eProductFamily, false, &desc, cmdList->getCsr(false));
        cmdList->isFlushTaskSubmissionEnabled = false;

        return cmdList;
    }

    template <typename FamilyType, GFXCORE_FAMILY gfxCoreFamily>
    void verifySplitCmds(LinearStream &cmdStream, size_t streamOffset, L0::Device *device, uint64_t submissionId, WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> &immCmdList,
                         uint64_t externalDependencyGpuVa);

    std::unique_ptr<VariableBackup<HardwareInfo>> hwInfoBackup;
    const uint32_t numLinkCopyEngines = 4;
};

template <typename FamilyType, GFXCORE_FAMILY gfxCoreFamily>
void BcsSplitInOrderCmdListTests::verifySplitCmds(LinearStream &cmdStream, size_t streamOffset, L0::Device *device, uint64_t submissionId,
                                                  WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> &immCmdList, uint64_t externalDependencyGpuVa) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;
    auto counterGpuAddress = immCmdList.inOrderExecInfo->getBaseDeviceAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), streamOffset), (cmdStream.getUsed() - streamOffset)));

    auto itor = cmdList.begin();

    for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
        auto beginItor = itor;

        auto signalSubCopyEventGpuVa = bcsSplit.events.subcopy[i + (submissionId * numLinkCopyEngines)]->getCompletionFieldGpuAddress(device);

        size_t numExpectedSemaphores = 0;

        if (submissionId > 0) {
            numExpectedSemaphores++;
            itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            if (immCmdList.isQwordInOrderCounter()) {
                std::advance(itor, -2); // verify 2x LRI before semaphore
            }

            ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, submissionId, counterGpuAddress, immCmdList.isQwordInOrderCounter(), true));
        }

        if (externalDependencyGpuVa > 0) {
            numExpectedSemaphores++;
            itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
            ASSERT_NE(cmdList.end(), itor);
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(externalDependencyGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
        }

        itor = find<XY_COPY_BLT *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
        ASSERT_NE(nullptr, genCmdCast<XY_COPY_BLT *>(*itor));

        auto flushDwItor = find<MI_FLUSH_DW *>(++itor, cmdList.end());
        ASSERT_NE(cmdList.end(), flushDwItor);

        auto signalSubCopyEvent = genCmdCast<MI_FLUSH_DW *>(*flushDwItor);
        ASSERT_NE(nullptr, signalSubCopyEvent);

        while (signalSubCopyEvent->getDestinationAddress() != signalSubCopyEventGpuVa) {
            flushDwItor = find<MI_FLUSH_DW *>(++flushDwItor, cmdList.end());
            ASSERT_NE(cmdList.end(), flushDwItor);

            signalSubCopyEvent = genCmdCast<MI_FLUSH_DW *>(*flushDwItor);
            ASSERT_NE(nullptr, signalSubCopyEvent);
        }

        itor = ++flushDwItor;

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(beginItor, itor);
        EXPECT_EQ(numExpectedSemaphores, semaphoreCmds.size());
    }

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());

    if (submissionId > 0) {
        ASSERT_NE(cmdList.end(), semaphoreItor);
        if (immCmdList.isQwordInOrderCounter()) {
            std::advance(semaphoreItor, -2); // verify 2x LRI before semaphore
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreItor, submissionId, counterGpuAddress, immCmdList.isQwordInOrderCounter(), true));
    }

    for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
        auto subCopyEventSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, subCopyEventSemaphore);

        EXPECT_EQ(bcsSplit.events.subcopy[i + (submissionId * numLinkCopyEngines)]->getCompletionFieldGpuAddress(device), subCopyEventSemaphore->getSemaphoreGraphicsAddress());

        itor = ++semaphoreItor;
    }

    ASSERT_NE(nullptr, genCmdCast<MI_FLUSH_DW *>(*itor)); // marker event

    auto implicitCounterSdi = genCmdCast<MI_STORE_DATA_IMM *>(*(++itor));
    ASSERT_NE(nullptr, implicitCounterSdi);

    EXPECT_EQ(counterGpuAddress, implicitCounterSdi->getAddress());
    EXPECT_EQ(submissionId + 1, implicitCounterSdi->getDataDword0());

    EXPECT_EQ(submissionId + 1, immCmdList.inOrderExecInfo->getCounterValue());

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(++itor, cmdList.end());
    EXPECT_EQ(0u, sdiCmds.size());
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenDispatchingCopyThenHandleInOrderSignaling, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    EXPECT_TRUE(verifySplit(0));

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    EXPECT_TRUE(verifySplit(1));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

    for (auto &event : bcsSplit.events.barrier) {
        EXPECT_FALSE(event->isCounterBased());
    }
    for (auto &event : bcsSplit.events.subcopy) {
        EXPECT_FALSE(event->isCounterBased());
    }
    for (auto &event : bcsSplit.events.marker) {
        EXPECT_FALSE(event->isCounterBased());
    }
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyAfterBarrierWithoutImplicitDependenciesThenHandleCorrectInOrderSignaling, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    *immCmdList->getCsr(false)->getBarrierCountTagAddress() = 0u;
    immCmdList->getCsr(false)->getNextBarrierCount();

    size_t offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    // no implicit dependencies
    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 0, *immCmdList, 0);
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyAfterBarrierWithImplicitDependenciesThenHandleCorrectInOrderSignaling, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    *immCmdList->getCsr(false)->getBarrierCountTagAddress() = 0u;
    immCmdList->getCsr(false)->getNextBarrierCount();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    size_t offset = cmdStream->getUsed();

    *immCmdList->getCsr(false)->getBarrierCountTagAddress() = 0u;
    immCmdList->getCsr(false)->getNextBarrierCount();
    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    // implicit dependencies
    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 1, *immCmdList, 0);
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyWithEventDependencyThenRequiredSemaphores, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled();
    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    size_t offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 1, &eventHandle, false, false);

    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 1, *immCmdList, events[0]->getCompletionFieldGpuAddress(device));
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenDispatchingCopyRegionThenHandleInOrderSignaling, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    EXPECT_TRUE(verifySplit(0));

    ze_copy_region_t region = {0, 0, 0, copySize, 1, 1};

    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);

    EXPECT_TRUE(verifySplit(1));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenImmediateCmdListWhenDispatchingWithRegularEventThenSwitchToCounterBased, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    uint32_t copyData[64] = {};

    events[0]->makeCounterBasedInitiallyDisabled();
    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, eventHandle, 0, nullptr, false, false);

    if (immCmdList->getDcFlushRequired(true)) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    EXPECT_TRUE(verifySplit(1));
}

using InOrderRegularCmdListTests = InOrderCmdListTests;

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderFlagWhenCreatingCmdListThenEnableInOrderMode, IsAtLeastSkl) {
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

    ze_command_list_handle_t cmdList;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    EXPECT_TRUE(static_cast<CommandListCoreFamily<gfxCoreFamily> *>(cmdList)->isInOrderExecutionEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(cmdList));
}

HWTEST2_F(InOrderRegularCmdListTests, whenUsingRegularCmdListThenAddCmdsToPatch, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    uint32_t copyData = 0;

    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    EXPECT_EQ(1u, regularCmdList->inOrderPatchCmds.size()); // SDI

    auto sdiFromContainer1 = genCmdCast<MI_STORE_DATA_IMM *>(regularCmdList->inOrderPatchCmds[0].cmd1);
    ASSERT_NE(nullptr, sdiFromContainer1);
    MI_STORE_DATA_IMM *sdiFromParser1 = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        sdiFromParser1 = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
    }

    offset = cmdStream->getUsed();
    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);
    ASSERT_EQ(3u, regularCmdList->inOrderPatchCmds.size()); // SDI + Semaphore/2xLRI + SDI

    MI_SEMAPHORE_WAIT *semaphoreFromParser2 = nullptr;
    MI_SEMAPHORE_WAIT *semaphoreFromContainer2 = nullptr;

    MI_LOAD_REGISTER_IMM *firstLriFromContainer2 = nullptr;
    MI_LOAD_REGISTER_IMM *secondLriFromContainer2 = nullptr;

    MI_LOAD_REGISTER_IMM *firstLriFromParser2 = nullptr;
    MI_LOAD_REGISTER_IMM *secondLriFromParser2 = nullptr;

    if (regularCmdList->isQwordInOrderCounter()) {
        firstLriFromContainer2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(regularCmdList->inOrderPatchCmds[1].cmd1);
        ASSERT_NE(nullptr, firstLriFromContainer2);
        secondLriFromContainer2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(regularCmdList->inOrderPatchCmds[1].cmd2);
        ASSERT_NE(nullptr, secondLriFromContainer2);
    } else {
        semaphoreFromContainer2 = genCmdCast<MI_SEMAPHORE_WAIT *>(regularCmdList->inOrderPatchCmds[1].cmd1);
        EXPECT_EQ(nullptr, regularCmdList->inOrderPatchCmds[1].cmd2);
        ASSERT_NE(nullptr, semaphoreFromContainer2);
    }

    auto sdiFromContainer2 = genCmdCast<MI_STORE_DATA_IMM *>(regularCmdList->inOrderPatchCmds[2].cmd1);
    ASSERT_NE(nullptr, sdiFromContainer2);
    MI_STORE_DATA_IMM *sdiFromParser2 = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();

        if (regularCmdList->isQwordInOrderCounter()) {
            itor = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            firstLriFromParser2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
            ASSERT_NE(nullptr, firstLriFromParser2);
            secondLriFromParser2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*(++itor));
            ASSERT_NE(nullptr, secondLriFromParser2);
        } else {
            auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            semaphoreFromParser2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            ASSERT_NE(nullptr, semaphoreFromParser2);
        }

        auto sdiItor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), sdiItor);

        sdiFromParser2 = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    }

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = regularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

        EXPECT_EQ(getLowPart(1u + appendValue), sdiFromContainer1->getDataDword0());
        EXPECT_EQ(getLowPart(1u + appendValue), sdiFromParser1->getDataDword0());

        if (regularCmdList->isQwordInOrderCounter()) {
            EXPECT_EQ(getHighPart(1u + appendValue), sdiFromContainer1->getDataDword1());
            EXPECT_EQ(getHighPart(1u + appendValue), sdiFromParser1->getDataDword1());

            EXPECT_TRUE(sdiFromContainer1->getStoreQword());
            EXPECT_TRUE(sdiFromParser1->getStoreQword());

            EXPECT_EQ(getLowPart(1u + appendValue), firstLriFromContainer2->getDataDword());
            EXPECT_EQ(getLowPart(1u + appendValue), firstLriFromParser2->getDataDword());

            EXPECT_EQ(getHighPart(1u + appendValue), secondLriFromContainer2->getDataDword());
            EXPECT_EQ(getHighPart(1u + appendValue), secondLriFromParser2->getDataDword());
        } else {
            EXPECT_FALSE(sdiFromContainer1->getStoreQword());
            EXPECT_FALSE(sdiFromParser1->getStoreQword());

            EXPECT_EQ(1u + appendValue, semaphoreFromContainer2->getSemaphoreDataDword());
            EXPECT_EQ(1u + appendValue, semaphoreFromParser2->getSemaphoreDataDword());
        }

        EXPECT_EQ(getLowPart(2u + appendValue), sdiFromContainer2->getDataDword0());
        EXPECT_EQ(getLowPart(2u + appendValue), sdiFromParser2->getDataDword0());

        if (regularCmdList->isQwordInOrderCounter()) {
            EXPECT_EQ(getHighPart(2u + appendValue), sdiFromContainer2->getDataDword1());
            EXPECT_EQ(getHighPart(2u + appendValue), sdiFromParser2->getDataDword1());

            EXPECT_TRUE(sdiFromContainer2->getStoreQword());
            EXPECT_TRUE(sdiFromParser2->getStoreQword());
        } else {
            EXPECT_FALSE(sdiFromContainer2->getStoreQword());
            EXPECT_FALSE(sdiFromParser2->getStoreQword());
        }
    };

    regularCmdList->close();

    auto handle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
    verifyPatching(2);

    if (regularCmdList->isQwordInOrderCounter()) {
        regularCmdList->inOrderExecInfo->addRegularCmdListSubmissionCounter(static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 3);
        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);

        verifyPatching(regularCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter() - 1);
    }
}

HWTEST2_F(InOrderRegularCmdListTests, givenCrossRegularCmdListDependenciesWhenExecutingThenDontPatchWhenExecutedOnlyOnce, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList1 = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList2 = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    regularCmdList1->close();

    uint64_t baseEventWaitValue = 3;

    auto implicitCounterGpuVa = regularCmdList2->inOrderExecInfo->getBaseDeviceAddress();
    auto externalCounterGpuVa = regularCmdList1->inOrderExecInfo->getBaseDeviceAddress();

    auto cmdStream2 = regularCmdList2->getCmdContainer().getCommandStream();

    size_t offset2 = cmdStream2->getUsed();

    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    regularCmdList2->close();

    size_t sizeToParse2 = cmdStream2->getUsed();

    auto verifyPatching = [&](uint64_t expectedImplicitDependencyValue, uint64_t expectedExplicitDependencyValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream2->getCpuBase(), offset2), (sizeToParse2 - offset2)));

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, semaphoreCmds.size());

        if (regularCmdList1->isQwordInOrderCounter()) {
            // verify 2x LRI before semaphore
            std::advance(semaphoreCmds[0], -2);
            std::advance(semaphoreCmds[1], -2);
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreCmds[0], expectedImplicitDependencyValue, implicitCounterGpuVa, regularCmdList1->isQwordInOrderCounter(), false));
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreCmds[1], expectedExplicitDependencyValue, externalCounterGpuVa, regularCmdList1->isQwordInOrderCounter(), false));
    };

    auto cmdListHandle1 = regularCmdList1->toHandle();
    auto cmdListHandle2 = regularCmdList2->toHandle();

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(5, baseEventWaitValue);

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(7, baseEventWaitValue);
}

HWTEST2_F(InOrderRegularCmdListTests, givenCrossRegularCmdListDependenciesWhenExecutingThenPatchWhenExecutedMultipleTimes, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList1 = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList2 = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    regularCmdList1->close();

    uint64_t baseEventWaitValue = 3;

    auto implicitCounterGpuVa = regularCmdList2->inOrderExecInfo->getBaseDeviceAddress();
    auto externalCounterGpuVa = regularCmdList1->inOrderExecInfo->getBaseDeviceAddress();

    auto cmdListHandle1 = regularCmdList1->toHandle();
    auto cmdListHandle2 = regularCmdList2->toHandle();

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);

    auto cmdStream2 = regularCmdList2->getCmdContainer().getCommandStream();

    size_t offset2 = cmdStream2->getUsed();
    size_t sizeToParse2 = 0;

    auto verifyPatching = [&](uint64_t expectedImplicitDependencyValue, uint64_t expectedExplicitDependencyValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream2->getCpuBase(), offset2), (sizeToParse2 - offset2)));

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, semaphoreCmds.size());

        if (regularCmdList1->isQwordInOrderCounter()) {
            // verify 2x LRI before semaphore
            std::advance(semaphoreCmds[0], -2);
            std::advance(semaphoreCmds[1], -2);
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreCmds[0], expectedImplicitDependencyValue, implicitCounterGpuVa, regularCmdList1->isQwordInOrderCounter(), false));
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreCmds[1], expectedExplicitDependencyValue, externalCounterGpuVa, regularCmdList1->isQwordInOrderCounter(), false));
    };

    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    regularCmdList2->close();

    sizeToParse2 = cmdStream2->getUsed();

    verifyPatching(1, baseEventWaitValue);

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(1, baseEventWaitValue + (2 * regularCmdList1->inOrderExecInfo->getCounterValue()));

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(5, baseEventWaitValue + (2 * regularCmdList1->inOrderExecInfo->getCounterValue()));

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(7, baseEventWaitValue + (3 * regularCmdList1->inOrderExecInfo->getCounterValue()));
}

HWTEST2_F(InOrderRegularCmdListTests, givenDebugFlagSetWhenUsingRegularCmdListThenDontAddCmdsToPatch, IsAtLeastXeHpCore) {
    debugManager.flags.EnableInOrderRegularCmdListPatching.set(0);

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    uint32_t copyData = 0;

    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    EXPECT_EQ(0u, regularCmdList->inOrderPatchCmds.size());
}

HWTEST2_F(InOrderRegularCmdListTests, whenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    ASSERT_EQ(3u, regularCmdList->inOrderPatchCmds.size()); // Walker + Semaphore + Walker

    WalkerVariant walkerVariantFromContainer1 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(regularCmdList->inOrderPatchCmds[0].cmd1);
    WalkerVariant walkerVariantFromContainer2 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(regularCmdList->inOrderPatchCmds[2].cmd1);
    std::visit([](auto &&walker1, auto &&walker2) {
        ASSERT_NE(nullptr, walker1);
        ASSERT_NE(nullptr, walker2);
    },
               walkerVariantFromContainer1, walkerVariantFromContainer2);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    WalkerVariant walkerVariantFromParser1 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itor);

    itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(++itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    WalkerVariant walkerVariantFromParser2 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itor);

    std::visit([&](auto &&walkerFromParser1, auto &&walkerFromParser2, auto &&walkerFromContainer1, auto &&walkerFromContainer2) {
        auto verifyPatching = [&](uint64_t executionCounter) {
            auto appendValue = regularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

            EXPECT_EQ(1u + appendValue, walkerFromContainer1->getPostSync().getImmediateData());
            EXPECT_EQ(1u + appendValue, walkerFromParser1->getPostSync().getImmediateData());

            EXPECT_EQ(2u + appendValue, walkerFromContainer2->getPostSync().getImmediateData());
            EXPECT_EQ(2u + appendValue, walkerFromParser2->getPostSync().getImmediateData());
        };

        regularCmdList->close();

        auto handle = regularCmdList->toHandle();

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(0);

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(1);

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(2);
    },
               walkerVariantFromParser1, walkerVariantFromParser2, walkerVariantFromContainer1, walkerVariantFromContainer2);

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());
}

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenProgramPipeControlsToHandleDependencies, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerVariant = typename FamilyType::WalkerVariant;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->getCounterValue());
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(1u, regularCmdList->inOrderExecInfo->getCounterValue());

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([&regularCmdList](auto &&walker) {
            auto &postSync = walker->getPostSync();
            using PostSyncType = std::decay_t<decltype(postSync)>;

            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(1u, postSync.getImmediateData());
            EXPECT_EQ(regularCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
        },
                   walkerVariant);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), sdiItor);
    }

    offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));
        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), semaphoreItor);

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([&regularCmdList](auto &&walker) {
            auto &postSync = walker->getPostSync();
            using PostSyncType = std::decay_t<decltype(postSync)>;

            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(2u, postSync.getImmediateData());
            EXPECT_EQ(regularCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
        },
                   walkerVariant);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), sdiItor);
    }

    regularCmdList->inOrderExecInfo->setAllocationOffset(123);
    auto hostAddr = static_cast<uint64_t *>(regularCmdList->inOrderExecInfo->getBaseHostAddress());
    *hostAddr = 0x1234;
    regularCmdList->latestOperationRequiredNonWalkerInOrderCmdsChaining = true;

    auto originalInOrderExecInfo = regularCmdList->inOrderExecInfo;

    regularCmdList->reset();
    EXPECT_NE(originalInOrderExecInfo.get(), regularCmdList->inOrderExecInfo.get());
    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->getAllocationOffset());
    hostAddr = static_cast<uint64_t *>(regularCmdList->inOrderExecInfo->getBaseHostAddress());
    EXPECT_EQ(0u, *hostAddr);
    EXPECT_FALSE(regularCmdList->latestOperationRequiredNonWalkerInOrderCmdsChaining);
}

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenUpdateCounterAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->makeCounterBasedInitiallyDisabled();

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCopyOnlyCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto copyOnlyCmdStream = regularCopyOnlyCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_NE(nullptr, regularCmdList->inOrderExecInfo.get());

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

    regularCmdList->appendMemoryCopyRegion(data, &region, 1, 1, data, &region, 1, 1, nullptr, 0, nullptr, false, false);

    regularCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    regularCmdList->appendSignalEvent(eventHandle);

    regularCmdList->appendBarrier(nullptr, 1, &eventHandle, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(2u, sdiCmds.size());
    }

    offset = copyOnlyCmdStream->getUsed();
    regularCopyOnlyCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(copyOnlyCmdStream->getCpuBase(), offset),
                                                          (copyOnlyCmdStream->getUsed() - offset)));

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);
    }

    context->freeMem(data);
}

using InOrderRegularCopyOnlyCmdListTests = InOrderCmdListTests;

HWTEST2_F(InOrderRegularCopyOnlyCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenDontProgramBarriers, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    auto alignedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    regularCmdList->appendMemoryCopy(alignedPtr, alignedPtr, MemoryConstants::cacheLineSize, nullptr, 0, nullptr, false, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

        ASSERT_NE(nullptr, sdiCmd);

        auto gpuAddress = regularCmdList->inOrderExecInfo->getBaseDeviceAddress();

        EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
        EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(1u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
    }

    offset = cmdStream->getUsed();

    regularCmdList->appendMemoryCopy(alignedPtr, alignedPtr, MemoryConstants::cacheLineSize, nullptr, 0, nullptr, false, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        if (regularCmdList->isQwordInOrderCounter()) {
            std::advance(itor, 2); // 2x LRI before semaphore
        }
        EXPECT_NE(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*itor));

        itor++;
        auto copyCmd = genCmdCast<XY_COPY_BLT *>(*itor);

        EXPECT_NE(nullptr, copyCmd);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

        ASSERT_NE(nullptr, sdiCmd);

        auto gpuAddress = regularCmdList->inOrderExecInfo->getBaseDeviceAddress();

        EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
        EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(2u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
    }

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderRegularCmdListTests, givenNonInOrderRegularCmdListWhenPassingCounterBasedEventToWaitThenPatchOnExecute, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);
    auto inOrderRegularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->inOrderExecInfo.reset();

    inOrderRegularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size());

    MI_SEMAPHORE_WAIT *semaphoreFromParser2 = nullptr;
    MI_SEMAPHORE_WAIT *semaphoreFromContainer2 = nullptr;

    MI_LOAD_REGISTER_IMM *firstLriFromContainer2 = nullptr;
    MI_LOAD_REGISTER_IMM *secondLriFromContainer2 = nullptr;

    MI_LOAD_REGISTER_IMM *firstLriFromParser2 = nullptr;
    MI_LOAD_REGISTER_IMM *secondLriFromParser2 = nullptr;

    if (regularCmdList->isQwordInOrderCounter()) {
        firstLriFromContainer2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(regularCmdList->inOrderPatchCmds[0].cmd1);
        ASSERT_NE(nullptr, firstLriFromContainer2);
        secondLriFromContainer2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(regularCmdList->inOrderPatchCmds[0].cmd2);
        ASSERT_NE(nullptr, secondLriFromContainer2);
    } else {
        semaphoreFromContainer2 = genCmdCast<MI_SEMAPHORE_WAIT *>(regularCmdList->inOrderPatchCmds[0].cmd1);
        EXPECT_EQ(nullptr, regularCmdList->inOrderPatchCmds[0].cmd2);
        ASSERT_NE(nullptr, semaphoreFromContainer2);
    }

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();

        if (regularCmdList->isQwordInOrderCounter()) {
            itor = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            firstLriFromParser2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
            ASSERT_NE(nullptr, firstLriFromParser2);
            secondLriFromParser2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*(++itor));
            ASSERT_NE(nullptr, secondLriFromParser2);
        } else {
            auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            semaphoreFromParser2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            ASSERT_NE(nullptr, semaphoreFromParser2);
        }
    }

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = inOrderRegularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

        if (regularCmdList->isQwordInOrderCounter()) {
            EXPECT_EQ(getLowPart(1u + appendValue), firstLriFromContainer2->getDataDword());
            EXPECT_EQ(getLowPart(1u + appendValue), firstLriFromParser2->getDataDword());

            EXPECT_EQ(getHighPart(1u + appendValue), secondLriFromContainer2->getDataDword());
            EXPECT_EQ(getHighPart(1u + appendValue), secondLriFromParser2->getDataDword());
        } else {
            EXPECT_EQ(1u + appendValue, semaphoreFromContainer2->getSemaphoreDataDword());
            EXPECT_EQ(1u + appendValue, semaphoreFromParser2->getSemaphoreDataDword());
        }
    };

    regularCmdList->close();
    inOrderRegularCmdList->close();

    auto inOrderRegularCmdListHandle = inOrderRegularCmdList->toHandle();
    auto regularHandle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &inOrderRegularCmdListHandle, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &regularHandle, nullptr, false, nullptr);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &inOrderRegularCmdListHandle, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &regularHandle, nullptr, false, nullptr);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &inOrderRegularCmdListHandle, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &regularHandle, nullptr, false, nullptr);
    verifyPatching(2);

    mockCmdQHw->executeCommandLists(1, &regularHandle, nullptr, false, nullptr);
    verifyPatching(2);
}

HWTEST2_F(InOrderRegularCmdListTests, givenAddedCmdForPatchWhenUpdateNewInOrderInfoThenNewInfoIsSet, IsAtLeastXeHpCore) {
    auto semaphoreCmd = FamilyType::cmdInitMiSemaphoreWait;

    auto inOrderRegularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto &inOrderExecInfo = inOrderRegularCmdList->inOrderExecInfo;
    inOrderExecInfo->addRegularCmdListSubmissionCounter(4);
    inOrderExecInfo->addCounterValue(1);

    auto inOrderRegularCmdList2 = createRegularCmdList<gfxCoreFamily>(false);
    auto &inOrderExecInfo2 = inOrderRegularCmdList2->inOrderExecInfo;
    inOrderExecInfo2->addRegularCmdListSubmissionCounter(6);
    inOrderExecInfo2->addCounterValue(1);

    inOrderRegularCmdList->addCmdForPatching(&inOrderExecInfo, &semaphoreCmd, nullptr, 1, NEO::InOrderPatchCommandHelpers::PatchCmdType::semaphore);

    ASSERT_EQ(1u, inOrderRegularCmdList->inOrderPatchCmds.size());

    inOrderRegularCmdList->disablePatching(0);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(0u, semaphoreCmd.getSemaphoreDataDword());

    inOrderRegularCmdList->enablePatching(0);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(4u, semaphoreCmd.getSemaphoreDataDword());

    inOrderRegularCmdList->updateInOrderExecInfo(0, &inOrderExecInfo2, false);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(6u, semaphoreCmd.getSemaphoreDataDword());

    inOrderExecInfo->addRegularCmdListSubmissionCounter(1);
    inOrderRegularCmdList->updateInOrderExecInfo(0, &inOrderExecInfo, true);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(6u, semaphoreCmd.getSemaphoreDataDword());

    inOrderRegularCmdList->enablePatching(0);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(5u, semaphoreCmd.getSemaphoreDataDword());
}

struct StandaloneInOrderTimestampAllocationTests : public InOrderCmdListTests {
    void SetUp() override {
        NEO::debugManager.flags.StandaloneInOrderTimestampAllocationEnabled.set(1);
        InOrderCmdListTests::SetUp();
    }
};

HWTEST2_F(StandaloneInOrderTimestampAllocationTests, givenTimestampEventWhenDispatchingThenAssignNewNode, IsAtLeastSkl) {
    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    auto cmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_EQ(nullptr, events[0]->inOrderTimestampNode);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    EXPECT_NE(nullptr, events[0]->inOrderTimestampNode);

    // keep node0 ownership for testing
    auto node0 = events[0]->inOrderTimestampNode;
    events[0]->inOrderTimestampNode = nullptr;

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    EXPECT_NE(nullptr, events[0]->inOrderTimestampNode);
    EXPECT_NE(node0, events[0]->inOrderTimestampNode);

    auto node1 = events[0]->inOrderTimestampNode;

    // node1 moved to reusable list
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    EXPECT_NE(nullptr, events[0]->inOrderTimestampNode);
    EXPECT_NE(node1->getGpuAddress(), events[0]->inOrderTimestampNode->getGpuAddress());

    auto node2 = events[0]->inOrderTimestampNode;

    auto hostAddress = cmdList->inOrderExecInfo->getBaseHostAddress();
    *hostAddress = 3;

    // return node1 to pool
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    // node1 reused
    EXPECT_EQ(node1->getGpuAddress(), events[0]->inOrderTimestampNode->getGpuAddress());

    // reuse node2 - counter already waited
    *hostAddress = 2;

    cmdList->inOrderExecInfo->releaseNotUsedTempTimestampNodes(false);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    EXPECT_EQ(node2->getGpuAddress(), events[0]->inOrderTimestampNode->getGpuAddress());

    events[0]->unsetInOrderExecInfo();
    EXPECT_EQ(nullptr, events[0]->inOrderTimestampNode);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    // mark as not ready, to make sure that destructor will release everything anyway
    *hostAddress = 0;
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeAndNoopWaitEventsAllowedWhenEventBoundToCmdListThenNoopSpaceForWaitCommands, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    char noopedLriBuffer[sizeof(MI_LOAD_REGISTER_IMM)] = {};
    memset(noopedLriBuffer, 0, sizeof(MI_LOAD_REGISTER_IMM));
    char noopedSemWaitBuffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    memset(noopedSemWaitBuffer, 0, sizeof(MI_SEMAPHORE_WAIT));

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->allowCbWaitEventsNoopDispatch = true;

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    CommandToPatchContainer outCbWaitEventCmds;
    launchParams.outListCommands = &outCbWaitEventCmds;
    result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    size_t expectedLoadRegImmCount = FamilyType::isQwordInOrderCounter ? 2 : 0;

    size_t expectedWaitCmds = 1 + expectedLoadRegImmCount;
    ASSERT_EQ(expectedWaitCmds, outCbWaitEventCmds.size());

    size_t outCbWaitEventCmdsIndex = 0;
    for (; outCbWaitEventCmdsIndex < expectedLoadRegImmCount; outCbWaitEventCmdsIndex++) {
        EXPECT_EQ(CommandToPatch::CbWaitEventLoadRegisterImm, outCbWaitEventCmds[outCbWaitEventCmdsIndex].type);
        auto registerNumber = 0x2600 + (4 * outCbWaitEventCmdsIndex);
        EXPECT_EQ(registerNumber, outCbWaitEventCmds[outCbWaitEventCmdsIndex].offset);

        ASSERT_NE(nullptr, outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination);
        auto memCmpRet = memcmp(outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination, noopedLriBuffer, sizeof(MI_LOAD_REGISTER_IMM));
        EXPECT_EQ(0, memCmpRet);
    }

    EXPECT_EQ(CommandToPatch::CbWaitEventSemaphoreWait, outCbWaitEventCmds[outCbWaitEventCmdsIndex].type);

    ASSERT_NE(nullptr, outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination);
    auto memCmpRet = memcmp(outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination, noopedSemWaitBuffer, sizeof(MI_SEMAPHORE_WAIT));
    EXPECT_EQ(0, memCmpRet);
}

using SynchronizedDispatchTests = InOrderCmdListFixture;

struct MultiTileSynchronizedDispatchTests : public MultiTileInOrderCmdListTests {
    void SetUp() override {
        NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(1);
        MultiTileInOrderCmdListTests::SetUp();
    }
};

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenSyncDispatchExtensionWhenCreatingRegularCmdListThenEnableSyncDispatchMode, IsAtLeastSkl) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(-1);

    ze_base_desc_t unknownDesc = {ZE_STRUCTURE_TYPE_FORCE_UINT32};

    ze_synchronized_dispatch_exp_desc_t syncDispatchDesc = {};
    syncDispatchDesc.stype = ZE_STRUCTURE_TYPE_SYNCHRONIZED_DISPATCH_EXP_DESC;

    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    zex_command_list_handle_t hCmdList;

    // pNext == nullptr
    auto result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // pNext == unknown type
    cmdListDesc.pNext = &unknownDesc;
    result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // limited dispatch mode
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_DISABLED_EXP_FLAG;
    cmdListDesc.pNext = &syncDispatchDesc;
    result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::limited, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // full dispatch mode
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG;
    cmdListDesc.pNext = &syncDispatchDesc;
    result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenSyncDispatchExtensionWhenCreatingImmediateCmdListThenEnableSyncDispatchMode, IsAtLeastSkl) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(-1);

    ze_base_desc_t unknownDesc = {ZE_STRUCTURE_TYPE_FORCE_UINT32};

    ze_synchronized_dispatch_exp_desc_t syncDispatchDesc = {};
    syncDispatchDesc.stype = ZE_STRUCTURE_TYPE_SYNCHRONIZED_DISPATCH_EXP_DESC;

    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    zex_command_list_handle_t hCmdList;

    // pNext == nullptr
    auto result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // pNext == unknown type
    queueDesc.pNext = &unknownDesc;
    result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // limited dispatch mode
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_DISABLED_EXP_FLAG;
    queueDesc.pNext = &syncDispatchDesc;
    result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::limited, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // full dispatch mode
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG;
    queueDesc.pNext = &syncDispatchDesc;
    result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);
}

HWTEST2_F(SynchronizedDispatchTests, givenSingleTileSyncDispatchQueueWhenCreatingThenDontAssignQueueId, IsAtLeastSkl) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(1);

    auto regularCmdList0 = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList1 = createRegularCmdList<gfxCoreFamily>(false);
    auto immCmdList0 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), regularCmdList0->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, regularCmdList0->synchronizedDispatchMode);

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), regularCmdList1->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, regularCmdList1->synchronizedDispatchMode);

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), immCmdList0->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, immCmdList0->synchronizedDispatchMode);

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), immCmdList1->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, immCmdList1->synchronizedDispatchMode);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenDebugFlagSetWhenCreatingCmdListThenEnableSynchronizedDispatch, IsAtLeastSkl) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(-1);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    auto regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, immCmdList->synchronizedDispatchMode);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, regularCmdList->synchronizedDispatchMode);

    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(0);

    immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, immCmdList->synchronizedDispatchMode);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, regularCmdList->synchronizedDispatchMode);

    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(1);

    immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList->synchronizedDispatchMode);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, regularCmdList->synchronizedDispatchMode);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenMultiTileSyncDispatchQueueWhenCreatingThenAssignQueueId, IsAtLeastSkl) {
    auto regularCmdList0 = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList1 = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    auto immCmdList0 = createMultiTileImmCmdList<gfxCoreFamily>();
    auto immCmdList1 = createMultiTileImmCmdList<gfxCoreFamily>();

    auto limitedRegularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    limitedRegularCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    auto limitedImmCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    limitedImmCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    EXPECT_EQ(0u, regularCmdList0->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, regularCmdList0->synchronizedDispatchMode);

    EXPECT_EQ(1u, regularCmdList1->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, regularCmdList1->synchronizedDispatchMode);

    EXPECT_EQ(2u, immCmdList0->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList0->synchronizedDispatchMode);

    EXPECT_EQ(3u, immCmdList1->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList1->synchronizedDispatchMode);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenMultiTileSyncDispatchQueueWhenCreatingThenDontAssignQueueIdForLimitedMode, IsAtLeastSkl) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(0);

    auto mockDevice = static_cast<MockDeviceImp *>(device);

    constexpr uint32_t limitedQueueId = std::numeric_limits<uint32_t>::max();

    EXPECT_EQ(nullptr, mockDevice->syncDispatchTokenAllocation);

    auto limitedRegularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    limitedRegularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::limited);

    EXPECT_NE(nullptr, mockDevice->syncDispatchTokenAllocation);

    auto limitedImmCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    limitedImmCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::limited);

    EXPECT_EQ(limitedQueueId, limitedRegularCmdList->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::limited, limitedRegularCmdList->synchronizedDispatchMode);

    EXPECT_EQ(limitedQueueId, limitedImmCmdList->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::limited, limitedImmCmdList->synchronizedDispatchMode);

    auto regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::full);
    EXPECT_EQ(0u, regularCmdList->syncDispatchQueueId);

    regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::full);
    EXPECT_EQ(1u, regularCmdList->syncDispatchQueueId);

    limitedImmCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    limitedImmCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::limited);

    EXPECT_EQ(limitedQueueId, limitedRegularCmdList->syncDispatchQueueId);

    regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::full);
    EXPECT_EQ(2u, regularCmdList->syncDispatchQueueId);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenSyncDispatchEnabledWhenAllocatingQueueIdThenEnsureTokenAllocation, IsAtLeastSkl) {
    auto mockDevice = static_cast<MockDeviceImp *>(device);

    EXPECT_EQ(nullptr, mockDevice->syncDispatchTokenAllocation);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList->synchronizedDispatchMode);

    auto syncAllocation = mockDevice->syncDispatchTokenAllocation;
    EXPECT_NE(nullptr, syncAllocation);

    EXPECT_EQ(syncAllocation->getAllocationType(), NEO::AllocationType::syncDispatchToken);

    immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList->synchronizedDispatchMode);

    EXPECT_EQ(mockDevice->syncDispatchTokenAllocation, syncAllocation);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenSyncDispatchWhenAppendingThenHandleResidency, IsAtLeastSkl) {
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[device->getSyncDispatchTokenAllocation()]);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[device->getSyncDispatchTokenAllocation()]);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenDefaultCmdListWhenCooperativeDispatchEnableThenEnableSyncDispatchMode, IsAtLeastSkl) {
    debugManager.flags.ForceSynchronizedDispatchMode.set(-1);
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(immCmdList->synchronizedDispatchMode, NEO::SynchronizedDispatchMode::disabled);

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);

    if (device->getL0GfxCoreHelper().implicitSynchronizedDispatchForCooperativeKernelsAllowed()) {
        EXPECT_EQ(immCmdList->synchronizedDispatchMode, NEO::SynchronizedDispatchMode::full);
    } else {
        EXPECT_EQ(immCmdList->synchronizedDispatchMode, NEO::SynchronizedDispatchMode::disabled);
    }

    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;
    device->ensureSyncDispatchTokenAllocation();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_EQ(immCmdList->synchronizedDispatchMode, NEO::SynchronizedDispatchMode::limited);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenLimitedSyncDispatchWhenAppendingThenProgramTokenCheck, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>;
    class MyCmdList : public BaseClass {
      public:
        void appendSynchronizedDispatchInitializationSection() override {
            initCalled++;
            BaseClass::appendSynchronizedDispatchInitializationSection();
        }

        void appendSynchronizedDispatchCleanupSection() override {
            cleanupCalled++;
            BaseClass::appendSynchronizedDispatchCleanupSection();
        }

        uint32_t initCalled = 0;
        uint32_t cleanupCalled = 0;
    };

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto immCmdList = createImmCmdListImpl<gfxCoreFamily, MyCmdList>(false);
    immCmdList->partitionCount = partitionCount;
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint32_t expectedInitCalls = 1;
    uint32_t expectedCleanupCalls = 1;

    auto verifyTokenCheck = [&](uint32_t numDependencies) {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), semaphore);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        for (uint32_t i = 0; i < numDependencies; i++) {
            for (uint32_t j = 1; j < partitionCount; j++) {
                semaphore++;
                semaphore = find<MI_SEMAPHORE_WAIT *>(semaphore, cmdList.end());
                EXPECT_NE(cmdList.end(), semaphore);
            }
            semaphore++;
        }

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        EXPECT_NE(nullptr, semaphoreCmd);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        EXPECT_EQ(0u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(device->getSyncDispatchTokenAllocation()->getGpuAddress() + sizeof(uint32_t), semaphoreCmd->getSemaphoreGraphicsAddress());
        EXPECT_EQ(COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());

        EXPECT_EQ(expectedInitCalls++, immCmdList->initCalled);
        EXPECT_EQ(expectedCleanupCalls++, immCmdList->cleanupCalled);

        return !::testing::Test::HasFailure();
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCheck(0));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), nullptr, 0, nullptr, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    const ze_kernel_handle_t launchKernels = kernel->toHandle();
    immCmdList->appendLaunchMultipleKernelsIndirect(1, &launchKernels, reinterpret_cast<const uint32_t *>(alloc), &groupCount, nullptr, 0, nullptr, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    size_t rangeSizes = 1;
    const void **ranges = const_cast<const void **>(&alloc);
    immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(alloc, alloc, 1, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(alloc, &region, 1, 1, alloc, &region, 1, 1, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(alloc, alloc, 2, 2, nullptr, 0, nullptr, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(alloc), nullptr, 0, nullptr);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    auto handle = events[0]->toHandle();
    events[0]->unsetCmdQueue();
    immCmdList->appendBarrier(nullptr, 1, &handle, false);
    EXPECT_TRUE(verifyTokenCheck(2));

    context->freeMem(alloc);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenFullSyncDispatchWhenAppendingThenProgramTokenAcquire, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::full;
    immCmdList->syncDispatchQueueId = 0x1234;

    const uint32_t queueId = immCmdList->syncDispatchQueueId + 1;
    const uint64_t queueIdToken = static_cast<uint64_t>(queueId) << 32;
    const uint64_t tokenInitialValue = queueIdToken + partitionCount;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint64_t syncAllocGpuVa = device->getSyncDispatchTokenAllocation()->getGpuAddress();

    auto verifyTokenAcquisition = [&](bool hasDependencySemaphore) {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto itor = cmdList.begin();
        if (hasDependencySemaphore) {
            for (uint32_t i = 0; i < partitionCount; i++) {
                itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
                EXPECT_NE(cmdList.end(), itor);
                itor++;
            }
        }

        // Primary-secondaty path selection
        void *primaryTileSectionSkipVa = *itor;

        // Primary Tile section
        auto miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(
            ptrOffset(primaryTileSectionSkipVa, NEO::EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(false)));
        void *loopBackToAcquireVa = miPredicate;

        if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
            return false;
        }

        auto miAtomic = reinterpret_cast<MI_ATOMIC *>(++miPredicate);
        EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_1, miAtomic->getDwordLength());
        EXPECT_EQ(1u, miAtomic->getInlineData());

        EXPECT_EQ(0u, miAtomic->getOperand1DataDword0());
        EXPECT_EQ(0u, miAtomic->getOperand1DataDword1());

        EXPECT_EQ(getLowPart(tokenInitialValue), miAtomic->getOperand2DataDword0());
        EXPECT_EQ(getHighPart(tokenInitialValue), miAtomic->getOperand2DataDword1());

        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_CMP_WR, miAtomic->getAtomicOpcode());
        EXPECT_EQ(MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());

        EXPECT_EQ(syncAllocGpuVa, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));

        if (::testing::Test::HasFailure()) {
            return false;
        }

        void *jumpToEndSectionFromPrimaryTile = ++miAtomic;

        auto semaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(
            ptrOffset(jumpToEndSectionFromPrimaryTile, NEO::EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(false)));

        EXPECT_EQ(0u, semaphore->getSemaphoreDataDword());
        EXPECT_EQ(syncAllocGpuVa + sizeof(uint32_t), semaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphore->getCompareOperation());

        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(++semaphore);
        EXPECT_EQ(castToUint64(loopBackToAcquireVa), bbStart->getBatchBufferStartAddress());

        if (::testing::Test::HasFailure()) {
            return false;
        }

        uint64_t workPartitionGpuVa = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation()->getGpuAddress();

        // Secondary Tile section
        miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++bbStart);
        if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
            return false;
        }

        // Primary Tile section skip - patching
        if (!RelaxedOrderingCommandsHelper::verifyConditionalDataMemBbStart<FamilyType>(primaryTileSectionSkipVa, castToUint64(miPredicate), workPartitionGpuVa, 0, NEO::CompareOperation::notEqual, false, false, false)) {
            return false;
        }

        semaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(++miPredicate);
        EXPECT_EQ(queueId, semaphore->getSemaphoreDataDword());
        EXPECT_EQ(syncAllocGpuVa + sizeof(uint32_t), semaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphore->getCompareOperation());

        // End section
        miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++semaphore);
        if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
            return false;
        }

        // Jump to end from Primary Tile section - patching
        if (!RelaxedOrderingCommandsHelper::verifyConditionalDataMemBbStart<FamilyType>(jumpToEndSectionFromPrimaryTile, castToUint64(miPredicate), syncAllocGpuVa + sizeof(uint32_t), queueId, NEO::CompareOperation::equal, false, false, false)) {
            return false;
        }

        return true;
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenAcquisition(false));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenAcquisition(true));
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenFullSyncDispatchWhenAppendingThenProgramTokenCleanup, IsAtLeastSkl) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::full;
    immCmdList->syncDispatchQueueId = 0x1234;

    const uint32_t queueId = immCmdList->syncDispatchQueueId + 1;
    const uint64_t queueIdToken = static_cast<uint64_t>(queueId) << 32;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint64_t syncAllocGpuVa = device->getSyncDispatchTokenAllocation()->getGpuAddress();

    auto verifyTokenCleanup = [&]() {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

        EXPECT_NE(cmdList.end(), itor);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        MI_ATOMIC *miAtomic = nullptr;
        bool atomicFound = false;

        while (itor != cmdList.end()) {
            itor = find<MI_ATOMIC *>(itor, cmdList.end());
            EXPECT_NE(cmdList.end(), itor);
            if (::testing::Test::HasFailure()) {
                return false;
            }

            miAtomic = genCmdCast<MI_ATOMIC *>(*itor);

            if (syncAllocGpuVa == NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic)) {
                atomicFound = true;
                break;
            }
            itor++;
        }

        EXPECT_TRUE(atomicFound);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_0, miAtomic->getDwordLength());
        EXPECT_EQ(0u, miAtomic->getInlineData());

        EXPECT_EQ(0u, miAtomic->getOperand1DataDword0());
        EXPECT_EQ(0u, miAtomic->getOperand1DataDword1());
        EXPECT_EQ(0u, miAtomic->getOperand2DataDword0());
        EXPECT_EQ(0u, miAtomic->getOperand2DataDword1());

        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_DECREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());

        if (::testing::Test::HasFailure()) {
            return false;
        }

        miAtomic++;

        EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_1, miAtomic->getDwordLength());
        EXPECT_EQ(1u, miAtomic->getInlineData());

        EXPECT_EQ(getLowPart(queueIdToken), miAtomic->getOperand1DataDword0());
        EXPECT_EQ(getHighPart(queueIdToken), miAtomic->getOperand1DataDword1());
        EXPECT_EQ(0u, miAtomic->getOperand2DataDword0());
        EXPECT_EQ(0u, miAtomic->getOperand2DataDword1());

        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_CMP_WR, miAtomic->getAtomicOpcode());
        EXPECT_EQ(MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());

        EXPECT_EQ(syncAllocGpuVa, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));

        return !::testing::Test::HasFailure();
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCleanup());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCleanup());
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenLimitedSyncDispatchWhenAppendingThenDontProgramTokenCleanup, IsAtLeastSkl) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint64_t syncAllocGpuVa = device->getSyncDispatchTokenAllocation()->getGpuAddress();

    auto verifyTokenCleanup = [&]() {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

        EXPECT_NE(cmdList.end(), itor);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto atomics = findAll<MI_ATOMIC *>(itor, cmdList.end());
        for (auto &atomic : atomics) {
            auto miAtomic = genCmdCast<MI_ATOMIC *>(*atomic);
            EXPECT_NE(nullptr, miAtomic);
            EXPECT_NE(syncAllocGpuVa, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        }

        return !::testing::Test::HasFailure();
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCleanup());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCleanup());
}

struct CopyOffloadInOrderTests : public InOrderCmdListTests {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        backupHwInfo = std::make_unique<VariableBackup<NEO::HardwareInfo>>(defaultHwInfo.get());

        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        defaultHwInfo->featureTable.ftrBcsInfo = 0b111;

        InOrderCmdListTests::SetUp();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createImmCmdListWithOffload() {
        return createImmCmdListImpl<gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>(true);
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createMultiTileImmCmdListWithOffload(uint32_t partitionCount) {
        auto cmdList = createImmCmdListWithOffload<gfxCoreFamily>();
        cmdList->partitionCount = partitionCount;
        return cmdList;
    }

    uint32_t copyData1 = 0;
    uint32_t copyData2 = 0;
    std::unique_ptr<VariableBackup<NEO::HardwareInfo>> backupHwInfo;
};

HWTEST2_F(CopyOffloadInOrderTests, givenDebugFlagSetWhenCreatingCmdListThenEnableCopyOffload, IsAtLeastXeHpCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(1);

    ze_command_list_handle_t cmdListHandle;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_TRUE(cmdList->copyOperationOffloadEnabled);
        EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload);

        auto queue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload);
        EXPECT_EQ(cmdQueueDesc.priority, queue->desc.priority);
        EXPECT_EQ(cmdQueueDesc.mode, queue->desc.mode);
        EXPECT_TRUE(queue->peekIsCopyOnlyCommandQueue());
        EXPECT_TRUE(NEO::EngineHelpers::isBcs(queue->getCsr()->getOsContext().getEngineType()));

        zeCommandListDestroy(cmdListHandle);
    }

    {
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_TRUE(cmdList->copyOperationOffloadEnabled);
        EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload);

        auto queue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload);
        EXPECT_EQ(cmdQueueDesc.priority, queue->desc.priority);
        EXPECT_EQ(cmdQueueDesc.mode, queue->desc.mode);
        EXPECT_TRUE(queue->peekIsCopyOnlyCommandQueue());
        EXPECT_TRUE(NEO::EngineHelpers::isBcs(queue->getCsr()->getOsContext().getEngineType()));

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    }

    {
        cmdQueueDesc.flags = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_FALSE(cmdList->copyOperationOffloadEnabled);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    }

    {
        cmdQueueDesc.ordinal = static_cast<DeviceImp *>(device)->getCopyEngineOrdinal();

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_FALSE(cmdList->copyOperationOffloadEnabled);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.ordinal = 0;
    }

    {
        NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_FALSE(cmdList->copyOperationOffloadEnabled);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenQueueDescriptorWhenCreatingCmdListThenEnableCopyOffload, IsAtLeastXeHpCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

    ze_command_list_handle_t cmdListHandle;

    zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadDesc = {ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES};
    copyOffloadDesc.copyOffloadEnabled = true;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    cmdQueueDesc.pNext = &copyOffloadDesc;

    {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_TRUE(cmdList->copyOperationOffloadEnabled);
        EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);
    }

    {
        copyOffloadDesc.copyOffloadEnabled = false;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_FALSE(cmdList->copyOperationOffloadEnabled);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOffloadEnabledWhenProgrammingHwCmdsThenUserCopyCommands, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();
    EXPECT_FALSE(immCmdList->isCopyOnly());

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, false, false);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), copyItor);
    }

    {
        auto offset = cmdStream->getUsed();

        ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
        immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, nullptr, 0, nullptr, false, false);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), copyItor);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenProfilingEventWhenAppendingThenUseBcsCommands, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle, 0, nullptr, false, false);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, eventHandle, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto pipeControls = findAll<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipeControls.size());

    auto miFlushCmds = findAll<typename FamilyType::MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, miFlushCmds.size());
}

HWTEST2_F(CopyOffloadInOrderTests, givenAtomicSignalingModeWhenUpdatingCounterThenUseCorrectHwCommands, IsAtLeastXeHpCore) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr uint32_t partitionCount = 4;

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto gmmHelper = device->getNEODevice()->getGmmHelper();

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

        auto immCmdList = createMultiTileImmCmdListWithOffload<gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, false, false);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miAtomics = findAll<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(1u, miAtomics.size());

        auto atomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomics[0]);
        ASSERT_NE(nullptr, atomicCmd);

        auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

        EXPECT_EQ(gpuAddress, gmmHelper->canonize(NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*atomicCmd)));
        EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_ADD, atomicCmd->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, atomicCmd->getDataSize());
        EXPECT_EQ(getLowPart(partitionCount), atomicCmd->getOperand1DataDword0());
        EXPECT_EQ(getHighPart(partitionCount), atomicCmd->getOperand1DataDword1());
    }

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(0);

        auto immCmdList = createMultiTileImmCmdListWithOffload<gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, false, false);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miStoreDws = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(partitionCount, miStoreDws.size());

        for (uint32_t i = 0; i < partitionCount; i++) {

            auto storeDw = genCmdCast<MI_STORE_DATA_IMM *>(*miStoreDws[i]);
            ASSERT_NE(nullptr, storeDw);

            auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress() + (i * device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
            EXPECT_EQ(gpuAddress, gmmHelper->canonize(storeDw->getAddress()));
            EXPECT_EQ(1u, storeDw->getDataDword0());
        }
    }

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
        debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

        auto immCmdList = createMultiTileImmCmdListWithOffload<gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, false, false);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miStoreDws = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(partitionCount * 2, miStoreDws.size());

        for (uint32_t i = 0; i < partitionCount; i++) {

            auto storeDw = genCmdCast<MI_STORE_DATA_IMM *>(*miStoreDws[i + partitionCount]);
            ASSERT_NE(nullptr, storeDw);

            auto gpuAddress = immCmdList->inOrderExecInfo->getBaseHostGpuAddress() + (i * device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
            EXPECT_EQ(gpuAddress, storeDw->getAddress());
            EXPECT_EQ(1u, storeDw->getDataDword0());
        }
    }
}
HWTEST2_F(CopyOffloadInOrderTests, givenDeviceToHostCopyWhenProgrammingThenAddFence, IsAtLeastXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    using MI_MEM_FENCE = typename GfxFamily::MI_MEM_FENCE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename GfxFamily::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device));

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    constexpr size_t allocSize = 1;
    void *hostBuffer = nullptr;
    void *deviceBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocHostMem(&hostDesc, allocSize, allocSize, &hostBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocSize, allocSize, &deviceBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_copy_region_t dstRegion = {0, 0, 0, 1, 1, 1};
    ze_copy_region_t srcRegion = {0, 0, 0, 1, 1, 1};

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, false, false);

    bool expected = device->getProductHelper().isDeviceToHostCopySignalingFenceRequired();

    GenCmdList genCmdList;
    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    itor = find<MI_MEM_FENCE *>(itor, genCmdList.end());
    EXPECT_EQ(expected, genCmdList.end() != itor);

    context->freeMem(hostBuffer);
    context->freeMem(deviceBuffer);
}

HWTEST2_F(CopyOffloadInOrderTests, whenDispatchingSelectCorrectQueueAndCsr, IsAtLeastXeHpcCore) {
    auto regularEventsPool = createEvents<FamilyType>(1, false);

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto regularCsr = static_cast<CommandQueueImp *>(immCmdList->cmdQImmediate)->getCsr();
    auto copyCsr = static_cast<CommandQueueImp *>(immCmdList->cmdQImmediateCopyOffload)->getCsr();

    EXPECT_EQ(0u, regularCsr->peekTaskCount());
    EXPECT_EQ(0u, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(0u, copyCsr->peekTaskCount());
    EXPECT_EQ(0u, immCmdList->cmdQImmediateCopyOffload->getTaskCount());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0].get(), 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, regularCsr->peekTaskCount());
    EXPECT_EQ(1u, immCmdList->cmdQImmediate->getTaskCount());

    EXPECT_EQ(0u, copyCsr->peekTaskCount());
    EXPECT_EQ(0u, immCmdList->cmdQImmediateCopyOffload->getTaskCount());

    EXPECT_EQ(regularCsr, events[0]->csrs[0]);
    EXPECT_EQ(immCmdList->cmdQImmediate, events[0]->latestUsedCmdQueue);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, false, false);

    EXPECT_EQ(1u, regularCsr->peekTaskCount());
    EXPECT_EQ(1u, immCmdList->cmdQImmediate->getTaskCount());

    EXPECT_EQ(1u, copyCsr->peekTaskCount());
    EXPECT_EQ(1u, immCmdList->cmdQImmediateCopyOffload->getTaskCount());

    EXPECT_EQ(copyCsr, events[0]->csrs[0]);
    EXPECT_EQ(immCmdList->cmdQImmediateCopyOffload, events[0]->latestUsedCmdQueue);
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOperationWithHostVisibleEventThenMarkAsNotHostVisibleSubmission, IsAtLeastXeHpcCore) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device));

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, hostVisibleEvent.get(), 0, nullptr, launchParams, false);

    EXPECT_TRUE(immCmdList->latestFlushIsHostVisible);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, hostVisibleEvent.get(), 0, nullptr, false, false);

    EXPECT_EQ(!immCmdList->dcFlushSupport, immCmdList->latestFlushIsHostVisible);
}

HWTEST2_F(CopyOffloadInOrderTests, givenRelaxedOrderingEnabledWhenDispatchingThenUseCorrectCsr, IsAtLeastXeHpcCore) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
      public:
        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool kernelOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent) override {
            latestRelaxedOrderingMode = hasRelaxedOrderingDependencies;

            return ZE_RESULT_SUCCESS;
        }

        bool latestRelaxedOrderingMode = false;
    };

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    auto immCmdList = createImmCmdListImpl<gfxCoreFamily, MyMockCmdList>(true);

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto mainQueueDirectSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*mainQueueCsr);
    auto offloadDirectSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*copyQueueCsr);

    mainQueueCsr->directSubmission.reset(mainQueueDirectSubmission);
    copyQueueCsr->blitterDirectSubmission.reset(offloadDirectSubmission);

    int client1, client2;

    // first dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    // compute CSR
    mainQueueCsr->registerClient(&client1);
    mainQueueCsr->registerClient(&client2);

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));
    EXPECT_FALSE(immCmdList->isRelaxedOrderingDispatchAllowed(0, true));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(immCmdList->latestRelaxedOrderingMode);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, false, false);
    EXPECT_FALSE(immCmdList->latestRelaxedOrderingMode);

    // offload CSR
    mainQueueCsr->unregisterClient(&client1);
    mainQueueCsr->unregisterClient(&client2);
    copyQueueCsr->registerClient(&client1);
    copyQueueCsr->registerClient(&client2);

    EXPECT_FALSE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));
    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, true));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_FALSE(immCmdList->latestRelaxedOrderingMode);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(immCmdList->latestRelaxedOrderingMode);
}

HWTEST2_F(CopyOffloadInOrderTests, givenInOrderModeWhenCallingSyncThenHandleCompletionOnCorrectCsr, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
    auto hostAddress = static_cast<uint64_t *>(deviceAlloc->getUnderlyingBuffer());
    *hostAddress = 0;

    GraphicsAllocation *mainCsrDownloadedAlloc = nullptr;
    uint32_t mainCsrCallCounter = 0;

    GraphicsAllocation *offloadCsrDownloadedAlloc = nullptr;
    uint32_t offloadCsrCallCounter = 0;

    mainQueueCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        mainCsrCallCounter++;

        mainCsrDownloadedAlloc = &graphicsAllocation;
    };

    offloadCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        offloadCsrCallCounter++;

        offloadCsrDownloadedAlloc = &graphicsAllocation;
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    immCmdList->hostSynchronize(0, false);
    EXPECT_EQ(mainCsrDownloadedAlloc, deviceAlloc);
    EXPECT_EQ(offloadCsrDownloadedAlloc, nullptr);
    EXPECT_EQ(1u, mainCsrCallCounter);
    EXPECT_EQ(0u, offloadCsrCallCounter);
    EXPECT_EQ(1u, mainQueueCsr->checkGpuHangDetectedCalled);
    EXPECT_EQ(0u, offloadCsr->checkGpuHangDetectedCalled);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, false, false);

    EXPECT_EQ(0u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    immCmdList->hostSynchronize(0, false);

    if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(0u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
        EXPECT_EQ(1u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    } else {
        EXPECT_EQ(mainCsrDownloadedAlloc, deviceAlloc);
        EXPECT_EQ(offloadCsrDownloadedAlloc, deviceAlloc);
        EXPECT_EQ(1u, mainCsrCallCounter);
        EXPECT_EQ(1u, offloadCsrCallCounter);
        EXPECT_EQ(1u, mainQueueCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(1u, offloadCsr->checkGpuHangDetectedCalled);

        EXPECT_EQ(0u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
        EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenTbxModeWhenSyncCalledAlwaysDownloadAllocationsFromBothCsrs, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();
    immCmdList->isTbxMode = true;

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
    auto hostAddress = static_cast<uint64_t *>(deviceAlloc->getUnderlyingBuffer());
    *hostAddress = 2;
    *mainQueueCsr->getTagAddress() = 2;
    *offloadCsr->getTagAddress() = 2;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(0u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(0u, offloadCsr->downloadAllocationsCalledCount);

    immCmdList->hostSynchronize(0, false);

    EXPECT_EQ(1u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(1u, offloadCsr->downloadAllocationsCalledCount);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, false, false);

    immCmdList->hostSynchronize(0, false);

    EXPECT_EQ(2u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(2u, offloadCsr->downloadAllocationsCalledCount);
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonInOrderModeWaitWhenCallingSyncThenHandleCompletionOnCorrectCsr, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    *mainQueueCsr->getTagAddress() = 2;
    *offloadCsr->getTagAddress() = 2;

    auto mockAlloc = new MockGraphicsAllocation();

    auto internalAllocStorage = mainQueueCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(std::unique_ptr<MockGraphicsAllocation>(mockAlloc)), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
    auto hostAddress = static_cast<uint64_t *>(deviceAlloc->getUnderlyingBuffer());
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    immCmdList->hostSynchronize(0, true);
    EXPECT_EQ(1u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, false, false);

    immCmdList->hostSynchronize(0, true);
    EXPECT_EQ(1u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(1u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonInOrderModeWaitWhenCallingSyncThenHandleCompletionAndTempAllocations, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    *mainQueueCsr->getTagAddress() = 4;
    *offloadCsr->getTagAddress() = 4;

    auto mainInternalStorage = mainQueueCsr->getInternalAllocationStorage();

    auto offloadInternalStorage = offloadCsr->getInternalAllocationStorage();

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
    auto hostAddress = static_cast<uint64_t *>(deviceAlloc->getUnderlyingBuffer());
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);
    offloadInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);

    // only main is completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, false, false);

    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty()); // temp allocation created on offload csr

    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);

    // both completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    auto mockAlloc = new MockGraphicsAllocation();
    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::unique_ptr<MockGraphicsAllocation>(mockAlloc)), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    // only copy completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_FALSE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    mockAlloc->updateTaskCount(1, mainQueueCsr->getOsContext().getContextId());

    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    // stored only in copy storage
    offloadInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());
}

HWTEST2_F(CopyOffloadInOrderTests, givenInterruptEventWhenDispatchingTheProgramUserInterrupt, IsAtLeastXeHpcCore) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->enableInterruptMode();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0]->toHandle(), 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto itor = find<MI_USER_INTERRUPT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenAppendingKernelInCommandViewModeThenDoNotDispatchInOrderCommands, IsAtLeastXeHpCore) {
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    uint8_t computeWalkerHostBuffer[512];
    uint8_t payloadHostBuffer[256];

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.makeKernelCommandView = true;
    launchParams.cmdWalkerBuffer = computeWalkerHostBuffer;
    launchParams.hostPayloadBuffer = payloadHostBuffer;

    auto result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(0u, regularCmdList->inOrderPatchCmds.size());
}

} // namespace ult
} // namespace L0
