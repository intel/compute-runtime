/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {
struct InOrderIpcTests : public InOrderCmdListFixture {
    void enableEventSharing(FixtureMockEvent &event) {
        event.isSharableCouterBased = true;

        if (event.inOrderExecInfo.get()) {
            if (event.inOrderExecInfo->getDeviceCounterAllocation()) {
                static_cast<MemoryAllocation *>(event.inOrderExecInfo->getDeviceCounterAllocation())->internalHandle = 1;
            }
            if (event.inOrderExecInfo->getHostCounterAllocation()) {
                static_cast<MemoryAllocation *>(event.inOrderExecInfo->getHostCounterAllocation())->internalHandle = 2;
            }
        }
    }
};

HWTEST2_F(InOrderIpcTests, givenInvalidCbEventWhenOpenIpcCalledThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto nonTsEvent = createEvents<FamilyType>(1, false);
    auto tsEvent = createEvents<FamilyType>(1, true);

    IpcCounterBasedEventData ipcData = {};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, events[0]->getCounterBasedIpcHandle(ipcData));

    enableEventSharing(*events[0]);
    enableEventSharing(*events[1]);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, events[0]->getCounterBasedIpcHandle(ipcData));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);

    enableEventSharing(*events[0]);
    enableEventSharing(*events[1]);

    auto mockMemoryManager = static_cast<NEO::MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_EQ(0u, mockMemoryManager->registerIpcExportedAllocationCalled);

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->getCounterBasedIpcHandle(ipcData));

    EXPECT_EQ(events[0]->inOrderExecInfo->isHostStorageDuplicated() ? 2u : 1u, mockMemoryManager->registerIpcExportedAllocationCalled);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, events[1]->getCounterBasedIpcHandle(ipcData));

    events[0]->makeCounterBasedImplicitlyDisabled(nonTsEvent->getAllocation());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, events[0]->getCounterBasedIpcHandle(ipcData));
}

HWTEST2_F(InOrderIpcTests, givenIncorrectInternalHandleWhenGetIsCalledThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    IpcCounterBasedEventData ipcData = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    deviceAlloc->internalHandle = std::numeric_limits<uint64_t>::max();

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, events[0]->getCounterBasedIpcHandle(ipcData));

    if (events[0]->inOrderExecInfo->isHostStorageDuplicated()) {
        deviceAlloc->internalHandle = 1;
        static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation())->internalHandle = std::numeric_limits<uint64_t>::max();
        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, events[0]->getCounterBasedIpcHandle(ipcData));
    }
}

HWTEST2_F(InOrderIpcTests, givenCounterOffsetWhenOpenIsCalledThenPassCorrectData, MatchAny) {
    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    enableEventSharing(*events[0]);

    events[0]->inOrderAllocationOffset = 0x100;

    static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get())->numDevicePartitionsToWait = 2;
    static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get())->numHostPartitionsToWait = 3;

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    auto hostAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation());

    IpcCounterBasedEventData ipcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->getCounterBasedIpcHandle(ipcData));

    EXPECT_TRUE(deviceAlloc->internalHandle == ipcData.deviceHandle);
    EXPECT_TRUE(events[0]->inOrderExecInfo->isHostStorageDuplicated() ? hostAlloc->internalHandle : 0u == ipcData.hostHandle);
    EXPECT_TRUE(events[0]->counterBasedFlags == ipcData.counterBasedFlags);
    EXPECT_TRUE(device->getRootDeviceIndex() == ipcData.rootDeviceIndex);
    EXPECT_TRUE(events[0]->inOrderExecSignalValue == ipcData.counterValue);
    EXPECT_TRUE(events[0]->signalScope == ipcData.signalScopeFlags);
    EXPECT_TRUE(events[0]->waitScope == ipcData.waitScopeFlags);

    auto expectedOffset = static_cast<uint32_t>(events[0]->inOrderExecInfo->getBaseDeviceAddress() - events[0]->inOrderExecInfo->getDeviceCounterAllocation()->getGpuAddress());
    EXPECT_NE(0u, expectedOffset);
    expectedOffset += events[0]->inOrderAllocationOffset;

    EXPECT_TRUE(expectedOffset == ipcData.counterOffset);
    EXPECT_TRUE(events[0]->inOrderExecInfo->getNumDevicePartitionsToWait() == ipcData.devicePartitions);
    EXPECT_TRUE(events[0]->inOrderExecInfo->isHostStorageDuplicated() ? events[0]->inOrderExecInfo->getNumHostPartitionsToWait() : events[0]->inOrderExecInfo->getNumDevicePartitionsToWait() == ipcData.hostPartitions);
}

HWTEST2_F(InOrderIpcTests, givenIpcHandleWhenCreatingNewEventThenSetCorrectData, MatchAny) {
    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);
    events[0]->inOrderAllocationOffset = 0x100;
    auto event0InOrderInfo = static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get());
    event0InOrderInfo->numDevicePartitionsToWait = 2;
    event0InOrderInfo->numHostPartitionsToWait = 3;

    IpcCounterBasedEventData ipcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->getCounterBasedIpcHandle(ipcData));

    ze_event_handle_t newEvent = nullptr;
    auto deviceH = device->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->openCounterBasedIpcHandle(ipcData, &newEvent, driverHandle.get(), context, 1, &deviceH));

    EXPECT_NE(nullptr, newEvent);

    auto newEventMock = static_cast<FixtureMockEvent *>(Event::fromHandle(newEvent));
    auto inOrderInfo = newEventMock->inOrderExecInfo.get();

    EXPECT_FALSE(inOrderInfo->isRegularCmdList());
    EXPECT_EQ(inOrderInfo->getDeviceCounterAllocation()->getGpuAddress(), inOrderInfo->getBaseDeviceAddress());
    EXPECT_EQ(event0InOrderInfo->getNumDevicePartitionsToWait(), inOrderInfo->getNumDevicePartitionsToWait());
    EXPECT_EQ(event0InOrderInfo->isHostStorageDuplicated() ? event0InOrderInfo->getNumHostPartitionsToWait() : event0InOrderInfo->getNumDevicePartitionsToWait(), inOrderInfo->getNumHostPartitionsToWait());

    EXPECT_NE(nullptr, inOrderInfo->getExternalDeviceAllocation());
    EXPECT_NE(nullptr, inOrderInfo->getExternalHostAllocation());

    if (event0InOrderInfo->isHostStorageDuplicated()) {
        EXPECT_NE(inOrderInfo->getExternalDeviceAllocation(), inOrderInfo->getExternalHostAllocation());
        EXPECT_TRUE(inOrderInfo->isHostStorageDuplicated());
    } else {
        EXPECT_EQ(inOrderInfo->getExternalDeviceAllocation(), inOrderInfo->getExternalHostAllocation());
        EXPECT_FALSE(inOrderInfo->isHostStorageDuplicated());
    }

    EXPECT_TRUE(newEventMock->isFromIpcPool);
    EXPECT_EQ(newEventMock->signalScope, events[0]->signalScope);
    EXPECT_EQ(newEventMock->waitScope, events[0]->waitScope);
    EXPECT_EQ(newEventMock->inOrderExecSignalValue, events[0]->inOrderExecSignalValue);

    auto expectedOffset = static_cast<uint32_t>(event0InOrderInfo->getBaseDeviceAddress() - event0InOrderInfo->getDeviceCounterAllocation()->getGpuAddress()) + events[0]->inOrderAllocationOffset;
    EXPECT_EQ(expectedOffset, newEventMock->inOrderAllocationOffset);

    zeEventDestroy(newEvent);
}

HWTEST2_F(InOrderIpcTests, givenInvalidInternalHandleWhenOpenCalledThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    deviceAlloc->internalHandle = NEO::MockMemoryManager::invalidSharedHandle;

    IpcCounterBasedEventData ipcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->getCounterBasedIpcHandle(ipcData));

    ze_event_handle_t newEvent = nullptr;
    auto deviceH = device->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, events[0]->openCounterBasedIpcHandle(ipcData, &newEvent, driverHandle.get(), context, 1, &deviceH));

    if (events[0]->inOrderExecInfo->isHostStorageDuplicated()) {
        deviceAlloc->internalHandle = 1;
        static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation())->internalHandle = NEO::MockMemoryManager::invalidSharedHandle;

        EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->getCounterBasedIpcHandle(ipcData));

        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, events[0]->openCounterBasedIpcHandle(ipcData, &newEvent, driverHandle.get(), context, 1, &deviceH));
    }
}

HWTEST2_F(InOrderIpcTests, givenTbxModeWhenOpenIsCalledThenSetAllocationParams, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    IpcCounterBasedEventData ipcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->getCounterBasedIpcHandle(ipcData));

    ze_event_handle_t newEvent = nullptr;
    auto deviceH = device->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->openCounterBasedIpcHandle(ipcData, &newEvent, driverHandle.get(), context, 1, &deviceH));

    auto newEventMock = static_cast<FixtureMockEvent *>(Event::fromHandle(newEvent));

    EXPECT_TRUE(newEventMock->inOrderExecInfo->getExternalDeviceAllocation()->getAubInfo().writeMemoryOnly);

    if (newEventMock->inOrderExecInfo->isHostStorageDuplicated()) {
        EXPECT_TRUE(newEventMock->inOrderExecInfo->getExternalHostAllocation()->getAubInfo().writeMemoryOnly);
    }

    zeEventDestroy(newEvent);
}

HWTEST2_F(InOrderIpcTests, givenIpcImportedEventWhenSignalingThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    IpcCounterBasedEventData ipcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->getCounterBasedIpcHandle(ipcData));

    ze_event_handle_t newEvent = nullptr;
    auto deviceH = device->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->openCounterBasedIpcHandle(ipcData, &newEvent, driverHandle.get(), context, 1, &deviceH));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, newEvent, 0, nullptr, launchParams, false));

    zeEventDestroy(newEvent);
}

} // namespace ult
} // namespace L0