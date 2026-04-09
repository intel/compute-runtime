/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {
namespace ult {
struct InOrderIpcTests : public InOrderCmdListFixture {
    void SetUp() override {
        InOrderCmdListFixture::SetUp();

        static_cast<NEO::MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->storeIpcAllocations = true;
        setOpaqueHandleSupport(true);
    }

    void assignInternalHandle(GraphicsAllocation *allocation) {
        auto memoryAllocation = static_cast<MemoryAllocation *>(allocation);
        if (memoryAllocation && memoryAllocation->internalHandle == 0) {
            memoryAllocation->internalHandle = ++internalHandleCounter;
        }
    }

    void enableEventSharing(InOrderFixtureMockEvent &event) {
        event.isSharableCounterBased = true;

        if (event.getInOrderExecEventHelper().isDataAssigned()) {
            assignInternalHandle(event.getInOrderExecEventHelper().getDeviceCounterAllocation());
            assignInternalHandle(event.getInOrderExecEventHelper().getHostCounterAllocation());

            auto &inOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(event.inOrderExecHelper);
            auto &sharableEventDataHelper = static_cast<WhiteboxSharableEventDataHelper &>(inOrderEventHelper.sharableEventDataHelper);
            assignInternalHandle(sharableEventDataHelper.allocation);
        }
    }

    void setOpaqueHandleSupport(bool enable) {
        context->settings.useOpaqueHandle = enable ? (OpaqueHandlingType::nthandle | OpaqueHandlingType::pidfd) : OpaqueHandlingType::none;

        auto defaultContext = Context::fromHandle(Context::fromHandle(device->getDriverHandle()->getDefaultContext()));
        defaultContext->settings.useOpaqueHandle = context->settings.useOpaqueHandle;
    }

    uint64_t internalHandleCounter = 0;
};

HWTEST_F(InOrderIpcTests, givenInvalidCbEventWhenOpenIpcCalledThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto nonTsEvent = createEvents<FamilyType>(1, false);
    auto tsEvent = createEvents<FamilyType>(1, true);

    ze_ipc_event_counter_based_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    enableEventSharing(*events[0]);
    enableEventSharing(*events[1]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    enableEventSharing(*events[0]);
    enableEventSharing(*events[1]);

    auto mockMemoryManager = static_cast<NEO::MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_EQ(events[0]->getInOrderExecEventHelper().isHostStorageDuplicated() ? 3u : 2u, mockMemoryManager->registerIpcExportedAllocationCalled);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    EXPECT_EQ(events[0]->getInOrderExecEventHelper().isHostStorageDuplicated() ? 6u : 4u, mockMemoryManager->registerIpcExportedAllocationCalled);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetIpcHandle(events[1]->toHandle(), &zexIpcData));

    auto &inOrderExecHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->getInOrderExecEventHelper());
    inOrderExecHelper.fromExternalMemory = true;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    inOrderExecHelper.fromExternalMemory = false;

    events[0]->makeCounterBasedImplicitlyDisabled(nonTsEvent->getAllocation());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));
}

HWTEST_F(InOrderIpcTests, givenCbEventWhenCreatingFromApiThenOpenIpcHandle) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE;
    ze_event_handle_t ipcEvent = nullptr;
    ze_event_handle_t nonIpcEvent = nullptr;
    ze_event_handle_t timestampIpcEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &nonIpcEvent));

    counterBasedDesc.flags |= ZEX_COUNTER_BASED_EVENT_FLAG_IPC;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &ipcEvent));

    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IPC | ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &timestampIpcEvent));

    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IPC | ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_MAPPED_TIMESTAMP;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &timestampIpcEvent));

    ze_ipc_event_counter_based_handle_t zexIpcData = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nonIpcEvent, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, ipcEvent, 0, nullptr, launchParams);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetIpcHandle(nonIpcEvent, &zexIpcData));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(ipcEvent, &zexIpcData));

    zeEventDestroy(ipcEvent);
    zeEventDestroy(nonIpcEvent);
}

HWTEST_F(InOrderIpcTests, givenIncorrectInternalHandleWhenGetIsCalledThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    ze_ipc_event_counter_based_handle_t zeIpcData = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->getInOrderExecEventHelper().getDeviceCounterAllocation());
    deviceAlloc->internalHandle = std::numeric_limits<uint64_t>::max();

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    if (events[0]->getInOrderExecEventHelper().isHostStorageDuplicated()) {
        deviceAlloc->internalHandle = 1;
        static_cast<MemoryAllocation *>(events[0]->getInOrderExecEventHelper().getHostCounterAllocation())->internalHandle = std::numeric_limits<uint64_t>::max();
        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));
    }
}

HWTEST_F(InOrderIpcTests, givenCounterOffsetWhenOpenIsCalledThenPassCorrectData) {
    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(2, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    enableEventSharing(*events[1]);
    auto &inOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[1]->inOrderExecHelper);
    auto &sharableEventDataHelper = static_cast<WhiteboxSharableEventDataHelper &>(inOrderEventHelper.sharableEventDataHelper);

    auto communicationAllocation = sharableEventDataHelper.getAllocation();
    EXPECT_NE(0u, sharableEventDataHelper.allocationOffset);

    auto eventDataPtr = inOrderEventHelper.getInOrderExecEventDataPtr();

    eventDataPtr->counterOffset = 0x100;
    eventDataPtr->devicePartitions = 2;
    eventDataPtr->hostPartitions = 3;

    static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get())->numDevicePartitionsToWait = 2;
    static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get())->numHostPartitionsToWait = 3;
    static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get())->initializeAllocationsFromHost();
    auto deviceAlloc = static_cast<MemoryAllocation *>(inOrderEventHelper.getDeviceCounterAllocation());
    auto hostAlloc = static_cast<MemoryAllocation *>(inOrderEventHelper.getHostCounterAllocation());
    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[1]->toHandle(), &zeIpcData));

    IpcCounterBasedEventData &ipcData = *reinterpret_cast<IpcCounterBasedEventData *>(zeIpcData.data);

    EXPECT_EQ(0u, ipcData.oneWayAllocCounterHandle);
    EXPECT_EQ(0u, ipcData.oneWayCounterValue);
    EXPECT_EQ(0u, ipcData.oneWayPartitionCount);

    EXPECT_TRUE(static_cast<MemoryAllocation *>(communicationAllocation)->internalHandle == ipcData.communicationAllocHandle);
    EXPECT_TRUE(sharableEventDataHelper.allocationOffset == ipcData.allocOffset);
    EXPECT_TRUE(events[1]->counterBasedFlags == ipcData.counterBasedFlags);
    EXPECT_TRUE(events[1]->signalScope == ipcData.signalScopeFlags);
    EXPECT_TRUE(events[1]->waitScope == ipcData.waitScopeFlags);

    EXPECT_EQ(eventDataPtr->deviceAllocIpcHandle, deviceAlloc->internalHandle);
    EXPECT_TRUE(events[1]->getInOrderExecEventHelper().isHostStorageDuplicated() ? hostAlloc->internalHandle : 0u == eventDataPtr->hostAllocIpcHandle);

    EXPECT_TRUE(events[1]->getInOrderExecBaseSignalValue() == eventDataPtr->counterValue);
    EXPECT_TRUE(events[1]->getInOrderAllocationOffset() == eventDataPtr->counterOffset);

    auto expectedOffset = static_cast<uint32_t>(events[1]->getInOrderExecEventHelper().getBaseDeviceAddress() - events[1]->getInOrderExecEventHelper().getDeviceCounterAllocation()->getGpuAddress());
    EXPECT_NE(0u, expectedOffset);

    EXPECT_TRUE(expectedOffset == eventDataPtr->deviceIpcAllocOffset);

    if (events[1]->getInOrderExecEventHelper().isHostStorageDuplicated()) {
        auto expectedHostOffset = ptrDiff(events[1]->getInOrderExecEventHelper().getBaseHostCpuAddress(), events[1]->getInOrderExecEventHelper().getHostCounterAllocation()->getUnderlyingBuffer());
        EXPECT_NE(0u, expectedHostOffset);
        EXPECT_EQ(expectedHostOffset, eventDataPtr->hostIpcAllocOffset);
    } else {
        EXPECT_EQ(eventDataPtr->hostIpcAllocOffset, 0u);
    }

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenCounterOffsetWhenSignalingThenRefreshIpcData) {
    NEO::TagAllocatorBase *deviceAllocator = device->getDeviceInOrderCounterAllocator();
    NEO::TagAllocatorBase *hostAllocator = device->getHostInOrderCounterAllocator();

    deviceAllocator->getTag();
    hostAllocator->getTag();

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto allocator = device->getDeviceInOrderCounterAllocator();
    while (allocator->getGfxAllocations().size() == 1) {
        allocator->getTag();
    }
    allocator->getTag();

    if (immCmdList1->inOrderExecInfo->isHostStorageDuplicated()) {
        while (hostAllocator->getGfxAllocations().size() == 1) {
            hostAllocator->getTag();
        }
        hostAllocator->getTag();
    }

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(2, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    enableEventSharing(*events[1]);
    assignInternalHandle(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation());
    assignInternalHandle(immCmdList1->inOrderExecInfo->getHostCounterAllocation());

    EXPECT_NE(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList2->inOrderExecInfo->getDeviceCounterAllocation());

    auto &inOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[1]->inOrderExecHelper);

    auto eventDataPtr = inOrderEventHelper.getInOrderExecEventDataPtr();

    static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get())->initializeAllocationsFromHost();

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[1]->toHandle(), &zeIpcData));

    {
        auto deviceAlloc = static_cast<MemoryAllocation *>(inOrderEventHelper.getDeviceCounterAllocation());
        EXPECT_EQ(deviceAlloc, immCmdList2->inOrderExecInfo->getDeviceCounterAllocation());
        auto hostAlloc = static_cast<MemoryAllocation *>(inOrderEventHelper.getHostCounterAllocation());

        EXPECT_EQ(eventDataPtr->deviceAllocIpcHandle, deviceAlloc->internalHandle);
        EXPECT_TRUE(events[1]->getInOrderExecEventHelper().isHostStorageDuplicated() ? hostAlloc->internalHandle : 0u == eventDataPtr->hostAllocIpcHandle);

        auto expectedOffset = static_cast<uint32_t>(events[1]->getInOrderExecEventHelper().getBaseDeviceAddress() - events[1]->getInOrderExecEventHelper().getDeviceCounterAllocation()->getGpuAddress());
        EXPECT_NE(0u, expectedOffset);

        EXPECT_TRUE(expectedOffset == eventDataPtr->deviceIpcAllocOffset);

        if (events[1]->getInOrderExecEventHelper().isHostStorageDuplicated()) {
            auto expectedHostOffset = ptrDiff(events[1]->getInOrderExecEventHelper().getBaseHostCpuAddress(), events[1]->getInOrderExecEventHelper().getHostCounterAllocation()->getUnderlyingBuffer());
            EXPECT_NE(0u, expectedHostOffset);
            EXPECT_EQ(expectedHostOffset, eventDataPtr->hostIpcAllocOffset);
        } else {
            EXPECT_EQ(eventDataPtr->hostIpcAllocOffset, 0u);
        }
    }

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    {
        auto deviceAlloc = static_cast<MemoryAllocation *>(inOrderEventHelper.getDeviceCounterAllocation());
        EXPECT_EQ(deviceAlloc, immCmdList1->inOrderExecInfo->getDeviceCounterAllocation());
        auto hostAlloc = static_cast<MemoryAllocation *>(inOrderEventHelper.getHostCounterAllocation());

        EXPECT_EQ(eventDataPtr->deviceAllocIpcHandle, deviceAlloc->internalHandle);
        EXPECT_TRUE(events[1]->getInOrderExecEventHelper().isHostStorageDuplicated() ? hostAlloc->internalHandle : 0u == eventDataPtr->hostAllocIpcHandle);

        auto expectedOffset = static_cast<uint32_t>(events[1]->getInOrderExecEventHelper().getBaseDeviceAddress() - events[1]->getInOrderExecEventHelper().getDeviceCounterAllocation()->getGpuAddress());
        EXPECT_NE(0u, expectedOffset);

        EXPECT_TRUE(expectedOffset == eventDataPtr->deviceIpcAllocOffset);

        if (events[1]->getInOrderExecEventHelper().isHostStorageDuplicated()) {
            auto expectedHostOffset = ptrDiff(events[1]->getInOrderExecEventHelper().getBaseHostCpuAddress(), events[1]->getInOrderExecEventHelper().getHostCounterAllocation()->getUnderlyingBuffer());
            EXPECT_NE(0u, expectedHostOffset);
            EXPECT_EQ(expectedHostOffset, eventDataPtr->hostIpcAllocOffset);
        } else {
            EXPECT_EQ(eventDataPtr->hostIpcAllocOffset, 0u);
        }
    }

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenNonOpaqueHandleAndCounterOffsetWhenOpenIsCalledThenPassCorrectData) {
    setOpaqueHandleSupport(false);

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(2, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    enableEventSharing(*events[1]);
    auto &inOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[1]->inOrderExecHelper);
    auto &sharableEventDataHelper = static_cast<WhiteboxSharableEventDataHelper &>(inOrderEventHelper.sharableEventDataHelper);

    EXPECT_NE(0u, sharableEventDataHelper.allocationOffset);

    auto eventDataPtr = inOrderEventHelper.getInOrderExecEventDataPtr();

    eventDataPtr->counterOffset = 0x100;
    eventDataPtr->devicePartitions = 2;
    eventDataPtr->hostPartitions = 3;

    static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get())->numDevicePartitionsToWait = 2;
    static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get())->numHostPartitionsToWait = 3;
    static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get())->initializeAllocationsFromHost();
    auto deviceAlloc = static_cast<MemoryAllocation *>(inOrderEventHelper.getDeviceCounterAllocation());

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    if (events[1]->getInOrderExecEventHelper().isHostStorageDuplicated()) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeEventCounterBasedGetIpcHandle(events[1]->toHandle(), &zeIpcData));
    } else {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[1]->toHandle(), &zeIpcData));

        IpcCounterBasedEventData &ipcData = *reinterpret_cast<IpcCounterBasedEventData *>(zeIpcData.data);

        auto expectedOffset = static_cast<uint32_t>(events[1]->getInOrderExecEventHelper().getBaseDeviceAddress() - events[1]->getInOrderExecEventHelper().getDeviceCounterAllocation()->getGpuAddress());
        EXPECT_NE(0u, expectedOffset);

        EXPECT_EQ(deviceAlloc->internalHandle, ipcData.oneWayAllocCounterHandle);
        EXPECT_EQ(eventDataPtr->counterValue, ipcData.oneWayCounterValue);
        EXPECT_EQ(eventDataPtr->devicePartitions, ipcData.oneWayPartitionCount);

        EXPECT_EQ(0u, ipcData.communicationAllocHandle);
        EXPECT_EQ(expectedOffset, ipcData.allocOffset);
        EXPECT_TRUE(events[1]->counterBasedFlags == ipcData.counterBasedFlags);
        EXPECT_TRUE(events[1]->signalScope == ipcData.signalScopeFlags);
        EXPECT_TRUE(events[1]->waitScope == ipcData.waitScopeFlags);
    }
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenNonOpaqueIpcHandleWhenCreatingNewEventThenSetCorrectData) {
    setOpaqueHandleSupport(false);

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(2, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[1]);

    auto &inOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[1]->getInOrderExecEventHelper());
    auto &sharableEventDataHelper = static_cast<WhiteboxSharableEventDataHelper &>(inOrderEventHelper.sharableEventDataHelper);

    EXPECT_NE(0u, sharableEventDataHelper.allocationOffset);

    auto eventDataPtr = inOrderEventHelper.getInOrderExecEventDataPtr();

    eventDataPtr->counterOffset = 0x100;
    auto eventInOrderInfo = static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get());
    eventInOrderInfo->numDevicePartitionsToWait = 2;
    eventInOrderInfo->numHostPartitionsToWait = 3;
    eventInOrderInfo->initializeAllocationsFromHost();

    eventDataPtr->devicePartitions = 2;
    eventDataPtr->hostPartitions = eventInOrderInfo->isHostStorageDuplicated() ? 3 : 2;

    ze_ipc_event_counter_based_handle_t zeIpcData = {};

    if (events[1]->getInOrderExecEventHelper().isHostStorageDuplicated()) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeEventCounterBasedGetIpcHandle(events[1]->toHandle(), &zeIpcData));
    } else {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[1]->toHandle(), &zeIpcData));

        ze_event_handle_t newEvent = nullptr;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &newEvent));

        EXPECT_NE(nullptr, newEvent);

        auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));
        auto &newInOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(newEventMock->getInOrderExecEventHelper());

        auto expectedDeviceOffset = inOrderEventHelper.getBaseDeviceAddress() - inOrderEventHelper.getDeviceCounterAllocation()->getGpuAddress();

        EXPECT_EQ(newInOrderEventHelper.getDeviceCounterAllocation()->getGpuAddress() + expectedDeviceOffset, newInOrderEventHelper.getBaseDeviceAddress());
        EXPECT_EQ(eventInOrderInfo->getNumDevicePartitionsToWait(), newInOrderEventHelper.getEventData()->devicePartitions);

        EXPECT_TRUE(newInOrderEventHelper.isFromExternalMemory());

        EXPECT_EQ(newInOrderEventHelper.getDeviceCounterAllocation(), newInOrderEventHelper.getHostCounterAllocation());
        EXPECT_EQ(ptrOffset(newInOrderEventHelper.getHostCounterAllocation()->getUnderlyingBuffer(), expectedDeviceOffset), newInOrderEventHelper.getBaseHostCpuAddress());
        EXPECT_EQ(newInOrderEventHelper.getBaseDeviceAddress(), newInOrderEventHelper.getBaseHostGpuAddress());

        EXPECT_TRUE(newEventMock->isFromIpcPool);
        EXPECT_EQ(newEventMock->signalScope, events[1]->signalScope);
        EXPECT_EQ(newEventMock->waitScope, events[1]->waitScope);
        EXPECT_EQ(newEventMock->getInOrderExecBaseSignalValue(), events[1]->getInOrderExecBaseSignalValue());

        EXPECT_EQ(0u, newEventMock->getInOrderAllocationOffset());

        zeEventCounterBasedCloseIpcHandle(newEvent);
    }
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenIpcHandleWhenCreatingNewEventThenSetCorrectData) {
    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(2, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[1]);

    auto &inOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[1]->getInOrderExecEventHelper());
    auto &sharableEventDataHelper = static_cast<WhiteboxSharableEventDataHelper &>(inOrderEventHelper.sharableEventDataHelper);

    EXPECT_NE(0u, sharableEventDataHelper.allocationOffset);

    auto eventDataPtr = inOrderEventHelper.getInOrderExecEventDataPtr();

    eventDataPtr->counterOffset = 0x100;
    auto eventInOrderInfo = static_cast<WhiteboxInOrderExecInfo *>(inOrderEventHelper.inOrderExecInfo.get());
    eventInOrderInfo->numDevicePartitionsToWait = 2;
    eventInOrderInfo->numHostPartitionsToWait = 3;
    eventInOrderInfo->initializeAllocationsFromHost();

    eventDataPtr->devicePartitions = 2;
    eventDataPtr->hostPartitions = eventInOrderInfo->isHostStorageDuplicated() ? 3 : 2;

    ze_ipc_event_counter_based_handle_t zeIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[1]->toHandle(), &zeIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &newEvent));

    EXPECT_NE(nullptr, newEvent);

    auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));
    auto &newInOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(newEventMock->getInOrderExecEventHelper());

    auto expectedDeviceOffset = inOrderEventHelper.getBaseDeviceAddress() - inOrderEventHelper.getDeviceCounterAllocation()->getGpuAddress();

    EXPECT_EQ(newInOrderEventHelper.getDeviceCounterAllocation()->getGpuAddress() + expectedDeviceOffset, newInOrderEventHelper.getBaseDeviceAddress());
    EXPECT_EQ(eventInOrderInfo->getNumDevicePartitionsToWait(), newInOrderEventHelper.getEventData()->devicePartitions);
    EXPECT_EQ(eventInOrderInfo->isHostStorageDuplicated() ? eventInOrderInfo->getNumHostPartitionsToWait() : eventInOrderInfo->getNumDevicePartitionsToWait(), newInOrderEventHelper.getEventData()->hostPartitions);

    EXPECT_TRUE(newInOrderEventHelper.isFromExternalMemory());
    EXPECT_EQ(eventInOrderInfo->isHostStorageDuplicated(), newInOrderEventHelper.isHostStorageDuplicated());

    if (eventInOrderInfo->isHostStorageDuplicated()) {
        EXPECT_NE(newInOrderEventHelper.getDeviceCounterAllocation(), newInOrderEventHelper.getHostCounterAllocation());

        auto expectedHostOffset = ptrDiff(inOrderEventHelper.getBaseHostCpuAddress(), inOrderEventHelper.getHostCounterAllocation()->getUnderlyingBuffer());
        EXPECT_EQ(ptrOffset(newInOrderEventHelper.getHostCounterAllocation()->getUnderlyingBuffer(), expectedHostOffset), newInOrderEventHelper.getBaseHostCpuAddress());
        EXPECT_EQ(newInOrderEventHelper.getHostCounterAllocation()->getGpuAddress() + expectedHostOffset, newInOrderEventHelper.getBaseHostGpuAddress());
    } else {
        EXPECT_EQ(newInOrderEventHelper.getDeviceCounterAllocation(), newInOrderEventHelper.getHostCounterAllocation());
        EXPECT_EQ(ptrOffset(newInOrderEventHelper.getHostCounterAllocation()->getUnderlyingBuffer(), expectedDeviceOffset), newInOrderEventHelper.getBaseHostCpuAddress());
        EXPECT_EQ(newInOrderEventHelper.getBaseDeviceAddress(), newInOrderEventHelper.getBaseHostGpuAddress());
    }

    EXPECT_TRUE(newEventMock->isFromIpcPool);
    EXPECT_EQ(newEventMock->signalScope, events[1]->signalScope);
    EXPECT_EQ(newEventMock->waitScope, events[1]->waitScope);
    EXPECT_EQ(newEventMock->getInOrderExecBaseSignalValue(), events[1]->getInOrderExecBaseSignalValue());

    EXPECT_EQ(eventDataPtr->counterOffset, newEventMock->getInOrderAllocationOffset());

    zeEventCounterBasedCloseIpcHandle(newEvent);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenInvalidInternalHandleWhenOpenCalledThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->getInOrderExecEventHelper().getDeviceCounterAllocation());
    deviceAlloc->internalHandle = NEO::MockMemoryManager::invalidSharedHandle;

    ze_ipc_event_counter_based_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    if (events[0]->getInOrderExecEventHelper().isHostStorageDuplicated()) {
        deviceAlloc->internalHandle = 1;
        static_cast<MemoryAllocation *>(events[0]->getInOrderExecEventHelper().getHostCounterAllocation())->internalHandle = NEO::MockMemoryManager::invalidSharedHandle;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));

        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));
    }
}

HWTEST_F(InOrderIpcTests, givenTbxModeWhenOpenIsCalledThenSetAllocationParams) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    ze_ipc_event_counter_based_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));

    EXPECT_TRUE(newEventMock->getInOrderExecEventHelper().getDeviceCounterAllocation()->getAubInfo().writeMemoryOnly);

    if (newEventMock->getInOrderExecEventHelper().isHostStorageDuplicated()) {
        EXPECT_TRUE(newEventMock->getInOrderExecEventHelper().getHostCounterAllocation()->getAubInfo().writeMemoryOnly);
    }

    zeEventCounterBasedCloseIpcHandle(newEvent);
}

HWTEST_F(InOrderIpcTests, givenOpaqueIpcHandleWhenOpeningThenCorrectMemoryTypeIsSetBasedOnHostAccess) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    ze_event_handle_t newEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &newEvent));
    EXPECT_NE(nullptr, newEvent);

    auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));
    auto &newHelper = newEventMock->getInOrderExecEventHelper();
    auto svmManager = device->getDriverHandle()->getSvmAllocsManager();

    auto deviceAllocData = svmManager->getSVMAlloc(reinterpret_cast<void *>(newHelper.getDeviceCounterAllocation()->getGpuAddress()));
    ASSERT_NE(nullptr, deviceAllocData);

    if (newHelper.isHostStorageDuplicated()) {
        EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, deviceAllocData->memoryType);

        auto hostAllocData = svmManager->getSVMAlloc(reinterpret_cast<void *>(newHelper.getHostCounterAllocation()->getGpuAddress()));
        ASSERT_NE(nullptr, hostAllocData);
        EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, hostAllocData->memoryType);
    } else {
        EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, deviceAllocData->memoryType);
    }

    zeEventCounterBasedCloseIpcHandle(newEvent);
}

HWTEST_F(InOrderIpcTests, givenNonOpaqueIpcHandleWhenOpeningThenHostAccessMemoryTypeIsSet) {
    setOpaqueHandleSupport(false);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    ze_ipc_event_counter_based_handle_t zeIpcData = {};

    if (events[0]->getInOrderExecEventHelper().isHostStorageDuplicated()) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));
    } else {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

        ze_event_handle_t newEvent = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &newEvent));
        EXPECT_NE(nullptr, newEvent);

        auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));
        auto &newHelper = newEventMock->getInOrderExecEventHelper();
        auto svmManager = device->getDriverHandle()->getSvmAllocsManager();

        auto deviceAllocData = svmManager->getSVMAlloc(reinterpret_cast<void *>(newHelper.getDeviceCounterAllocation()->getGpuAddress()));
        ASSERT_NE(nullptr, deviceAllocData);
        EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, deviceAllocData->memoryType);

        zeEventCounterBasedCloseIpcHandle(newEvent);
    }
}

HWTEST_F(InOrderIpcTests, givenIncorrectParamsWhenUsingIpcApisThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    ze_ipc_event_counter_based_handle_t zexIpcData = {};

    ze_event_handle_t nullEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetIpcHandle(nullEvent, &zexIpcData));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), nullptr));

    events[0]->makeCounterBasedInitiallyDisabled(pool->getAllocation());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_context_handle_t nullContext = nullptr;
    ze_event_handle_t newEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedOpenIpcHandle(nullContext, zexIpcData, &newEvent));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zexIpcData, nullptr));
}

HWTEST_F(InOrderIpcTests, givenIpcHandleWhenOpenedThen2WayIpcSharingIsEnabledAndImportDataIsStored) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto &exporterHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->getInOrderExecEventHelper());
    auto exporterEventDataPtr = exporterHelper.getInOrderExecEventDataPtr();

    static_cast<WhiteboxInOrderExecInfo *>(exporterHelper.inOrderExecInfo.get())->initializeAllocationsFromHost();

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    EXPECT_NE(0u, exporterEventDataPtr->exporterProcessId);

    ze_event_handle_t newEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &newEvent));

    auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));
    auto &newHelper = static_cast<WhiteboxInOrderExecEventHelper &>(newEventMock->getInOrderExecEventHelper());

    EXPECT_TRUE(newHelper.twoWayIpcSharing);

    EXPECT_EQ(exporterEventDataPtr->deviceAllocIpcHandle, newHelper.imported2WayDeviceCounterHandle);
    EXPECT_EQ(exporterEventDataPtr->deviceIpcAllocOffset, newHelper.imported2WayCounterOffset);
    EXPECT_EQ(exporterEventDataPtr->exporterProcessId, newHelper.imported2WayExportedPid);

    EXPECT_FALSE(newHelper.is2WayIpcImportRefreshNeeded());

    zeEventCounterBasedCloseIpcHandle(newEvent);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWTEST_F(InOrderIpcTests, givenExporterSetsNewAllocDataWhenImporterQueriesStatusThenRefreshAllocations) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto &exporterHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->getInOrderExecEventHelper());

    static_cast<WhiteboxInOrderExecInfo *>(exporterHelper.inOrderExecInfo.get())->initializeAllocationsFromHost();

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    ze_event_handle_t newEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &newEvent));

    auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));
    auto &newHelper = static_cast<WhiteboxInOrderExecEventHelper &>(newEventMock->getInOrderExecEventHelper());

    EXPECT_FALSE(newHelper.is2WayIpcImportRefreshNeeded());

    auto oldBaseDeviceAddress = newHelper.getBaseDeviceAddress();

    auto eventDataPtr = newHelper.getInOrderExecEventDataPtr();
    eventDataPtr->deviceAllocIpcHandle = exporterHelper.getInOrderExecEventDataPtr()->deviceAllocIpcHandle;
    eventDataPtr->deviceIpcAllocOffset = exporterHelper.getInOrderExecEventDataPtr()->deviceIpcAllocOffset + 8;
    eventDataPtr->exporterProcessId = exporterHelper.getInOrderExecEventDataPtr()->exporterProcessId;

    EXPECT_TRUE(newHelper.is2WayIpcImportRefreshNeeded());

    newEventMock->refreshImported2WayIpcCbData();

    EXPECT_FALSE(newHelper.is2WayIpcImportRefreshNeeded());

    EXPECT_NE(oldBaseDeviceAddress, newHelper.getBaseDeviceAddress());

    EXPECT_EQ(eventDataPtr->deviceAllocIpcHandle, newHelper.imported2WayDeviceCounterHandle);
    EXPECT_EQ(eventDataPtr->deviceIpcAllocOffset, newHelper.imported2WayCounterOffset);
    EXPECT_EQ(eventDataPtr->exporterProcessId, newHelper.imported2WayExportedPid);

    zeEventCounterBasedCloseIpcHandle(newEvent);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWTEST_F(InOrderIpcTests, givenNon2WayIpcEventWhenCheckingRefreshNeededThenReturnFalse) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto &helper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->getInOrderExecEventHelper());

    EXPECT_FALSE(helper.twoWayIpcSharing);
    EXPECT_FALSE(helper.is2WayIpcImportRefreshNeeded());

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWTEST_F(InOrderIpcTests, givenExporterWhenGettingIpcHandleThenExporterDoesNotRefreshItself) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto &exporterHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->getInOrderExecEventHelper());

    static_cast<WhiteboxInOrderExecInfo *>(exporterHelper.inOrderExecInfo.get())->initializeAllocationsFromHost();

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    auto eventDataPtr = exporterHelper.getInOrderExecEventDataPtr();
    EXPECT_EQ(eventDataPtr->deviceAllocIpcHandle, exporterHelper.imported2WayDeviceCounterHandle);
    EXPECT_EQ(eventDataPtr->deviceIpcAllocOffset, exporterHelper.imported2WayCounterOffset);
    EXPECT_EQ(eventDataPtr->exporterProcessId, exporterHelper.imported2WayExportedPid);

    EXPECT_FALSE(exporterHelper.is2WayIpcImportRefreshNeeded());

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
}

HWTEST_F(InOrderIpcTests, givenImportedEventWhenHostSynchronizeThenImplicitlyRefreshAllocations) {
    NEO::TagAllocatorBase *deviceAllocator = device->getDeviceInOrderCounterAllocator();
    NEO::TagAllocatorBase *hostAllocator = device->getHostInOrderCounterAllocator();

    deviceAllocator->getTag();
    hostAllocator->getTag();

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto allocator = device->getDeviceInOrderCounterAllocator();
    while (allocator->getGfxAllocations().size() == 1) {
        allocator->getTag();
    }
    allocator->getTag();

    if (immCmdList1->inOrderExecInfo->isHostStorageDuplicated()) {
        while (hostAllocator->getGfxAllocations().size() == 1) {
            hostAllocator->getTag();
        }
        hostAllocator->getTag();
    }

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    EXPECT_NE(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList2->inOrderExecInfo->getDeviceCounterAllocation());

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    enableEventSharing(*events[0]);
    assignInternalHandle(immCmdList2->inOrderExecInfo->getDeviceCounterAllocation());
    assignInternalHandle(immCmdList2->inOrderExecInfo->getHostCounterAllocation());

    auto &exporterHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->inOrderExecHelper);
    static_cast<WhiteboxInOrderExecInfo *>(exporterHelper.inOrderExecInfo.get())->initializeAllocationsFromHost();

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    ze_event_handle_t importedEventHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &importedEventHandle));

    auto importedEvent = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(importedEventHandle));
    auto &importedHelper = static_cast<WhiteboxInOrderExecEventHelper &>(importedEvent->getInOrderExecEventHelper());

    EXPECT_TRUE(importedHelper.twoWayIpcSharing);
    EXPECT_FALSE(importedHelper.is2WayIpcImportRefreshNeeded());

    auto cachedHandleBefore = importedHelper.imported2WayDeviceCounterHandle;

    // simulate that the open happened in a different process
    constexpr unsigned int fakeImporterPid = 99999;
    importedHelper.imported2WayExportedPid = fakeImporterPid;

    // re-signal original event on cmdlist2 (different allocations) -> updates shared InOrderExecEventData
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto exporterEventData = exporterHelper.getInOrderExecEventDataPtr();

    // shared data was updated with new allocation info
    EXPECT_NE(cachedHandleBefore, exporterEventData->deviceAllocIpcHandle);
    EXPECT_NE(fakeImporterPid, exporterEventData->exporterProcessId);

    // importer should detect a refresh is needed
    EXPECT_TRUE(importedHelper.is2WayIpcImportRefreshNeeded());

    // complete the counter so hostSynchronize returns
    auto *hostAddr = importedHelper.getBaseHostCpuAddress();
    *hostAddr = std::numeric_limits<uint64_t>::max();

    // hostSynchronize triggers implicit refresh via handlePreQueryStatusOperationsAndCheckCompletion -> refreshImported2WayIpcCbData
    EXPECT_EQ(ZE_RESULT_SUCCESS, importedEvent->hostSynchronize(0));

    // verify refresh happened: cached values updated to match current shared data
    EXPECT_FALSE(importedHelper.is2WayIpcImportRefreshNeeded());
    EXPECT_EQ(exporterEventData->deviceAllocIpcHandle, importedHelper.imported2WayDeviceCounterHandle);
    EXPECT_EQ(exporterEventData->deviceIpcAllocOffset, importedHelper.imported2WayCounterOffset);
    EXPECT_EQ(exporterEventData->exporterProcessId, importedHelper.imported2WayExportedPid);
    EXPECT_NE(fakeImporterPid, importedHelper.imported2WayExportedPid);

    zeEventCounterBasedCloseIpcHandle(importedEventHandle);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList1.get());
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenImportedEventWhenAppendWaitOnEventsThenImplicitlyRefreshAllocations) {
    NEO::TagAllocatorBase *deviceAllocator = device->getDeviceInOrderCounterAllocator();
    NEO::TagAllocatorBase *hostAllocator = device->getHostInOrderCounterAllocator();

    deviceAllocator->getTag();
    hostAllocator->getTag();

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto allocator = device->getDeviceInOrderCounterAllocator();
    while (allocator->getGfxAllocations().size() == 1) {
        allocator->getTag();
    }
    allocator->getTag();

    if (immCmdList1->inOrderExecInfo->isHostStorageDuplicated()) {
        while (hostAllocator->getGfxAllocations().size() == 1) {
            hostAllocator->getTag();
        }
        hostAllocator->getTag();
    }

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    EXPECT_NE(immCmdList1->inOrderExecInfo->getDeviceCounterAllocation(), immCmdList2->inOrderExecInfo->getDeviceCounterAllocation());

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    enableEventSharing(*events[0]);
    assignInternalHandle(immCmdList2->inOrderExecInfo->getDeviceCounterAllocation());
    assignInternalHandle(immCmdList2->inOrderExecInfo->getHostCounterAllocation());

    auto &exporterHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->inOrderExecHelper);
    static_cast<WhiteboxInOrderExecInfo *>(exporterHelper.inOrderExecInfo.get())->initializeAllocationsFromHost();

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    ze_event_handle_t importedEventHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &importedEventHandle));

    auto importedEvent = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(importedEventHandle));
    auto &importedHelper = static_cast<WhiteboxInOrderExecEventHelper &>(importedEvent->getInOrderExecEventHelper());

    EXPECT_TRUE(importedHelper.twoWayIpcSharing);
    EXPECT_FALSE(importedHelper.is2WayIpcImportRefreshNeeded());

    auto cachedHandleBefore = importedHelper.imported2WayDeviceCounterHandle;

    // simulate that the open happened in a different process
    constexpr unsigned int fakeImporterPid = 99999;
    importedHelper.imported2WayExportedPid = fakeImporterPid;

    // re-signal original event on cmdlist2 (different allocations) -> updates shared InOrderExecEventData
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto exporterEventData = exporterHelper.getInOrderExecEventDataPtr();

    EXPECT_NE(cachedHandleBefore, exporterEventData->deviceAllocIpcHandle);
    EXPECT_TRUE(importedHelper.is2WayIpcImportRefreshNeeded());

    auto baseDeviceAddressBefore = importedHelper.getBaseDeviceAddress();

    // use a third cmdlist for waiting (to avoid isCbEventBoundToCmdList skip)
    auto immCmdList3 = createImmCmdList<FamilyType::gfxCoreFamily>();

    // appendWaitOnEvents triggers implicit refresh via event->refreshImported2WayIpcCbData()
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList3->appendWaitOnEvents(1, &importedEventHandle, nullptr, false, true, true, false, false, false));

    // verify refresh happened
    EXPECT_FALSE(importedHelper.is2WayIpcImportRefreshNeeded());
    EXPECT_EQ(exporterEventData->deviceAllocIpcHandle, importedHelper.imported2WayDeviceCounterHandle);
    EXPECT_EQ(exporterEventData->deviceIpcAllocOffset, importedHelper.imported2WayCounterOffset);
    EXPECT_EQ(exporterEventData->exporterProcessId, importedHelper.imported2WayExportedPid);
    EXPECT_NE(fakeImporterPid, importedHelper.imported2WayExportedPid);
    EXPECT_NE(baseDeviceAddressBefore, importedHelper.getBaseDeviceAddress());

    zeEventCounterBasedCloseIpcHandle(importedEventHandle);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList1.get());
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList3.get());
}

using InOrderRegularCmdListTests = InOrderCmdListFixture;
HWTEST_F(InOrderRegularCmdListTests, givenInOrderCmdListWhenQueryingRequiredSizeThenExpectCorrectValues) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto deviceRequiredSize = regularCmdList->getInOrderExecDeviceRequiredSize();
    EXPECT_EQ(sizeof(uint64_t), deviceRequiredSize);
    auto deviceNodeAddress = regularCmdList->getInOrderExecDeviceGpuAddress();
    EXPECT_NE(0u, deviceNodeAddress);
    auto hostRequiredSize = regularCmdList->getInOrderExecHostRequiredSize();
    EXPECT_EQ(0u, hostRequiredSize);
    auto hostNodeAddress = regularCmdList->getInOrderExecHostGpuAddress();
    EXPECT_EQ(0u, hostNodeAddress);

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    hostRequiredSize = regularCmdList->getInOrderExecHostRequiredSize();
    EXPECT_EQ(sizeof(uint64_t), hostRequiredSize);
    hostNodeAddress = regularCmdList->getInOrderExecHostGpuAddress();
    EXPECT_NE(0u, hostNodeAddress);
}

HWTEST_F(InOrderRegularCmdListTests, givenInOrderCmdListWhenClearingInOrderExecCounterAllocationThenResetHostCounterStorage) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto *hostAddress = regularCmdList->inOrderExecInfo->getBaseHostAddress();
    ASSERT_NE(nullptr, hostAddress);

    *hostAddress = 0x1234;
    regularCmdList->clearInOrderExecCounterAllocation();

    EXPECT_EQ(regularCmdList->inOrderExecInfo->getInitialCounterValue(), *hostAddress);
}

HWTEST_F(InOrderRegularCmdListTests, givenOutOfOrderCmdListWhenClearingInOrderExecCounterAllocationThenDoNothing) {
    auto cmdList = makeZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->initialize(device, EngineGroupType::renderCompute, 0u));
    EXPECT_FALSE(cmdList->isInOrderExecutionEnabled());
    EXPECT_EQ(nullptr, cmdList->inOrderExecInfo.get());

    cmdList->clearInOrderExecCounterAllocation();

    EXPECT_EQ(nullptr, cmdList->inOrderExecInfo.get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            InOrderRegularCmdListTests,
            givenPatchPreambleWhenExecutingInOrderCommandListThenPatchInOrderExecInfoSpace) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);
    mockCmdQHw->setPatchingPreamble(true);

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->close();

    auto deviceNodeAddress = regularCmdList->getInOrderExecDeviceGpuAddress();

    void *queueCpuBase = mockCmdQHw->commandStream.getCpuBase();
    auto usedSpaceBefore = mockCmdQHw->commandStream.getUsed();
    auto commandListHandle = regularCmdList->toHandle();
    auto returnValue = mockCmdQHw->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = mockCmdQHw->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(queueCpuBase, usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));
    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_LT(1u, sdiCmds.size());

    auto storeDataDeviceInOrderNoop = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmds[0]);
    EXPECT_EQ(deviceNodeAddress, storeDataDeviceInOrderNoop->getAddress());
    EXPECT_TRUE(storeDataDeviceInOrderNoop->getStoreQword());
    EXPECT_EQ(0u, storeDataDeviceInOrderNoop->getDataDword0());
    EXPECT_EQ(0u, storeDataDeviceInOrderNoop->getDataDword1());

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);
    cmdList.clear();

    regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->close();

    deviceNodeAddress = regularCmdList->getInOrderExecDeviceGpuAddress();
    auto hostNodeAddress = regularCmdList->getInOrderExecHostGpuAddress();

    usedSpaceBefore = mockCmdQHw->commandStream.getUsed();
    commandListHandle = regularCmdList->toHandle();
    returnValue = mockCmdQHw->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    usedSpaceAfter = mockCmdQHw->commandStream.getUsed();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(queueCpuBase, usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));
    sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_LT(2u, sdiCmds.size());

    storeDataDeviceInOrderNoop = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmds[0]);
    EXPECT_EQ(deviceNodeAddress, storeDataDeviceInOrderNoop->getAddress());
    EXPECT_TRUE(storeDataDeviceInOrderNoop->getStoreQword());
    EXPECT_EQ(0u, storeDataDeviceInOrderNoop->getDataDword0());
    EXPECT_EQ(0u, storeDataDeviceInOrderNoop->getDataDword1());

    auto storeDataHostInOrderNoop = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmds[1]);
    EXPECT_EQ(hostNodeAddress, storeDataHostInOrderNoop->getAddress());
    EXPECT_TRUE(storeDataHostInOrderNoop->getStoreQword());
    EXPECT_EQ(0u, storeDataHostInOrderNoop->getDataDword0());
    EXPECT_EQ(0u, storeDataHostInOrderNoop->getDataDword1());
}

HWTEST_F(InOrderIpcTests, givenUnsignaledSharedEventWhenExporterSignalsThenImporterRefreshesAllocations) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    auto &exporterHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->getInOrderExecEventHelper());
    auto &sharableEventDataHelper = static_cast<WhiteboxSharableEventDataHelper &>(exporterHelper.sharableEventDataHelper);

    events[0]->isSharableCounterBased = true;
    assignInternalHandle(immCmdList->inOrderExecInfo->getDeviceCounterAllocation());
    assignInternalHandle(immCmdList->inOrderExecInfo->getHostCounterAllocation());
    assignInternalHandle(sharableEventDataHelper.allocation);

    // get IPC handle - exports allocs normally
    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    // open IPC handle - should succeed creating event with null allocs (zero handles in shared data)
    ze_event_handle_t importedEventHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &importedEventHandle));

    auto importedEvent = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(importedEventHandle));
    auto &importedHelper = static_cast<WhiteboxInOrderExecEventHelper &>(importedEvent->getInOrderExecEventHelper());

    EXPECT_TRUE(importedHelper.twoWayIpcSharing);
    EXPECT_EQ(nullptr, importedHelper.getDeviceCounterAllocation());
    EXPECT_EQ(nullptr, importedHelper.getHostCounterAllocation());
    EXPECT_EQ(0u, importedHelper.getBaseDeviceAddress());
    EXPECT_EQ(0u, importedHelper.getBaseHostGpuAddress());
    EXPECT_EQ(nullptr, importedHelper.getBaseHostCpuAddress());
    EXPECT_FALSE(importedHelper.isHostStorageDuplicated());

    // no refresh needed yet - shared data still zeroed
    EXPECT_FALSE(importedHelper.is2WayIpcImportRefreshNeeded());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto exporterEventDataPtr = exporterHelper.getInOrderExecEventDataPtr();

    // importer should detect refresh is needed (shared data now has non-zero handle)
    EXPECT_TRUE(importedHelper.is2WayIpcImportRefreshNeeded());

    // appendWaitOnEvents triggers implicit refresh via refreshImported2WayIpcCbData
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList2->appendWaitOnEvents(1, &importedEventHandle, nullptr, false, true, true, false, false, false));

    // verify refresh happened: allocs populated, hostStorageDuplicated correct
    EXPECT_FALSE(importedHelper.is2WayIpcImportRefreshNeeded());
    EXPECT_NE(nullptr, importedHelper.getDeviceCounterAllocation());
    EXPECT_NE(nullptr, importedHelper.getHostCounterAllocation());
    EXPECT_NE(0u, importedHelper.getBaseDeviceAddress());

    EXPECT_EQ(exporterHelper.isHostStorageDuplicated(), importedHelper.isHostStorageDuplicated());
    EXPECT_EQ(exporterHelper.hostStorageDuplicated, importedHelper.isHostStorageDuplicated());

    EXPECT_EQ(exporterEventDataPtr->deviceAllocIpcHandle, importedHelper.imported2WayDeviceCounterHandle);
    EXPECT_EQ(exporterEventDataPtr->deviceIpcAllocOffset, importedHelper.imported2WayCounterOffset);
    EXPECT_EQ(exporterEventDataPtr->exporterProcessId, importedHelper.imported2WayExportedPid);

    zeEventCounterBasedCloseIpcHandle(importedEventHandle);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenUnsignaledSharedEventWhenImporterSignalsThenExporterRefreshesAllocations) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    auto &exporterHelper = static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->getInOrderExecEventHelper());
    auto &sharableEventDataHelper = static_cast<WhiteboxSharableEventDataHelper &>(exporterHelper.sharableEventDataHelper);
    auto exporterEventDataPtr = exporterHelper.getInOrderExecEventDataPtr();

    events[0]->isSharableCounterBased = true;
    assignInternalHandle(immCmdList->inOrderExecInfo->getDeviceCounterAllocation());
    assignInternalHandle(immCmdList->inOrderExecInfo->getHostCounterAllocation());
    assignInternalHandle(sharableEventDataHelper.allocation);

    // get IPC handle - exports allocs normally
    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    // open IPC handle - succeeds with null allocs (zero handles in shared data)
    ze_event_handle_t importedEventHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedOpenIpcHandle(context->toHandle(), zeIpcData, &importedEventHandle));

    auto importedEvent = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(importedEventHandle));
    auto &importedHelper = static_cast<WhiteboxInOrderExecEventHelper &>(importedEvent->getInOrderExecEventHelper());

    EXPECT_TRUE(importedHelper.twoWayIpcSharing);
    EXPECT_EQ(nullptr, importedHelper.getDeviceCounterAllocation());
    EXPECT_EQ(nullptr, importedHelper.getHostCounterAllocation());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, importedEventHandle, 0, nullptr, launchParams);

    // importer should detect refresh needed (shared data now has non-zero handles)
    EXPECT_TRUE(exporterHelper.is2WayIpcImportRefreshNeeded());

    // appendWaitOnEvents on imported event triggers refresh
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto exporterEventHandle = events[0]->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList2->appendWaitOnEvents(1, &exporterEventHandle, nullptr, false, true, true, false, false, false));

    // verify exporter refresh happened
    EXPECT_FALSE(exporterHelper.is2WayIpcImportRefreshNeeded());
    EXPECT_NE(nullptr, exporterHelper.getDeviceCounterAllocation());
    EXPECT_NE(nullptr, exporterHelper.getHostCounterAllocation());
    EXPECT_NE(0u, exporterHelper.getBaseDeviceAddress());
    EXPECT_EQ(exporterEventDataPtr->deviceAllocIpcHandle, importedHelper.imported2WayDeviceCounterHandle);
    EXPECT_EQ(exporterEventDataPtr->deviceIpcAllocOffset, importedHelper.imported2WayCounterOffset);
    EXPECT_EQ(exporterEventDataPtr->exporterProcessId, importedHelper.imported2WayExportedPid);

    EXPECT_EQ(exporterHelper.isHostStorageDuplicated(), importedHelper.isHostStorageDuplicated());
    EXPECT_EQ(exporterHelper.hostStorageDuplicated, importedHelper.isHostStorageDuplicated());

    zeEventCounterBasedCloseIpcHandle(importedEventHandle);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList.get());
    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenSocketHandleSharingNotSupportedWhenGettingCounterBasedIpcHandleThenHandlesAreNotTracked) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    EXPECT_FALSE(Context::fromHandle(device->getDriverHandle()->getDefaultContext())->isSocketHandleSharingSupported());
    EXPECT_TRUE(events[0]->exportedIpcServerHandles.empty());

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    EXPECT_TRUE(events[0]->exportedIpcServerHandles.empty());
}

HWTEST_F(InOrderIpcTests, givenSocketHandleSharingSupportedWhenGettingCounterBasedIpcHandleThenHandlesAreTracked) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto defaultContext = Context::fromHandle(device->getDriverHandle()->getDefaultContext());
    defaultContext->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    defaultContext->settings.handleType = IpcHandleType::fdHandle;
    EXPECT_TRUE(defaultContext->isSocketHandleSharingSupported());

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    EXPECT_FALSE(events[0]->exportedIpcServerHandles.empty());
}

HWTEST_F(InOrderIpcTests, givenEventWithTrackedIpcHandlesWhenDestroyCalledThenHandlesAreUnregistered) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto defaultContext = Context::fromHandle(device->getDriverHandle()->getDefaultContext());
    defaultContext->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    defaultContext->settings.handleType = IpcHandleType::fdHandle;

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    EXPECT_FALSE(events[0]->exportedIpcServerHandles.empty());

    events[0].release()->destroy();
}

HWTEST_F(InOrderIpcTests, givenEventWithTrackedIpcHandlesWhenUpdateInOrderExecStateWithDifferentAddressThenOldHandlesAreCleared) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(2, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto defaultContext = Context::fromHandle(device->getDriverHandle()->getDefaultContext());
    defaultContext->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    defaultContext->settings.handleType = IpcHandleType::fdHandle;

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    auto handleCountAfterFullExport = events[0]->exportedIpcServerHandles.size();
    EXPECT_GT(handleCountAfterFullExport, 0u);

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    auto newInOrderExecInfo = immCmdList2->inOrderExecInfo;
    EXPECT_NE(newInOrderExecInfo->getBaseDeviceAddress(), events[0]->getInOrderExecEventHelper().getBaseDeviceAddress());

    events[0]->updateInOrderExecState(newInOrderExecInfo, 1, 0);

    EXPECT_LT(events[0]->exportedIpcServerHandles.size(), handleCountAfterFullExport);
}

} // namespace ult
} // namespace L0
