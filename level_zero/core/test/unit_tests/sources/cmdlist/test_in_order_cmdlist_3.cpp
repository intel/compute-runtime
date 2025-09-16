/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {
namespace ult {
struct InOrderIpcTests : public InOrderCmdListFixture {
    void enableEventSharing(InOrderFixtureMockEvent &event) {
        event.isSharableCounterBased = true;

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

HWTEST_F(InOrderIpcTests, givenInvalidCbEventWhenOpenIpcCalledThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto nonTsEvent = createEvents<FamilyType>(1, false);
    auto tsEvent = createEvents<FamilyType>(1, true);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    enableEventSharing(*events[0]);
    enableEventSharing(*events[1]);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    enableEventSharing(*events[0]);
    enableEventSharing(*events[1]);

    auto mockMemoryManager = static_cast<NEO::MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_EQ(0u, mockMemoryManager->registerIpcExportedAllocationCalled);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    EXPECT_EQ(events[0]->inOrderExecInfo->isHostStorageDuplicated() ? 2u : 1u, mockMemoryManager->registerIpcExportedAllocationCalled);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[1]->toHandle(), &zexIpcData));

    events[0]->makeCounterBasedImplicitlyDisabled(nonTsEvent->getAllocation());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));
}

HWTEST_F(InOrderIpcTests, givenCbEventWhenCreatingFromApiThenOpenIpcHandle) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE;
    ze_event_handle_t ipcEvent = nullptr;
    ze_event_handle_t nonIpcEvent = nullptr;
    ze_event_handle_t timestampIpcEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &nonIpcEvent));

    counterBasedDesc.flags |= ZEX_COUNTER_BASED_EVENT_FLAG_IPC;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &ipcEvent));

    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IPC | ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &timestampIpcEvent));

    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IPC | ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_MAPPED_TIMESTAMP;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &timestampIpcEvent));

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nonIpcEvent, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, ipcEvent, 0, nullptr, launchParams);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zexCounterBasedEventGetIpcHandle(nonIpcEvent, &zexIpcData));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(ipcEvent, &zexIpcData));

    zeEventDestroy(ipcEvent);
    zeEventDestroy(nonIpcEvent);
}

HWTEST_F(InOrderIpcTests, givenIncorrectInternalHandleWhenGetIsCalledThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    deviceAlloc->internalHandle = std::numeric_limits<uint64_t>::max();

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    if (events[0]->inOrderExecInfo->isHostStorageDuplicated()) {
        deviceAlloc->internalHandle = 1;
        static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation())->internalHandle = std::numeric_limits<uint64_t>::max();
        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));
    }
}

HWTEST_F(InOrderIpcTests, givenCounterOffsetWhenOpenIsCalledThenPassCorrectData) {
    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    enableEventSharing(*events[0]);

    events[0]->inOrderAllocationOffset = 0x100;

    static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get())->numDevicePartitionsToWait = 2;
    static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get())->numHostPartitionsToWait = 3;
    static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get())->initializeAllocationsFromHost();
    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    auto hostAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation());

    zex_ipc_counter_based_event_handle_t zexIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    IpcCounterBasedEventData &ipcData = *reinterpret_cast<IpcCounterBasedEventData *>(zexIpcData.data);

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

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenIpcHandleWhenCreatingNewEventThenSetCorrectData) {
    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);
    events[0]->inOrderAllocationOffset = 0x100;
    auto event0InOrderInfo = static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get());
    event0InOrderInfo->numDevicePartitionsToWait = 2;
    event0InOrderInfo->numHostPartitionsToWait = 3;
    event0InOrderInfo->initializeAllocationsFromHost();

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    EXPECT_NE(nullptr, newEvent);

    auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));
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

    zexCounterBasedEventCloseIpcHandle(newEvent);

    completeHostAddress<FamilyType::gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>(immCmdList2.get());
}

HWTEST_F(InOrderIpcTests, givenInvalidInternalHandleWhenOpenCalledThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    deviceAlloc->internalHandle = NEO::MockMemoryManager::invalidSharedHandle;

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    if (events[0]->inOrderExecInfo->isHostStorageDuplicated()) {
        deviceAlloc->internalHandle = 1;
        static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation())->internalHandle = NEO::MockMemoryManager::invalidSharedHandle;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));
    }
}

HWTEST_F(InOrderIpcTests, givenTbxModeWhenOpenIsCalledThenSetAllocationParams) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    auto newEventMock = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(newEvent));

    EXPECT_TRUE(newEventMock->inOrderExecInfo->getExternalDeviceAllocation()->getAubInfo().writeMemoryOnly);

    if (newEventMock->inOrderExecInfo->isHostStorageDuplicated()) {
        EXPECT_TRUE(newEventMock->inOrderExecInfo->getExternalHostAllocation()->getAubInfo().writeMemoryOnly);
    }

    zexCounterBasedEventCloseIpcHandle(newEvent);
}

HWTEST_F(InOrderIpcTests, givenIpcImportedEventWhenSignalingThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, newEvent, 0, nullptr, launchParams));

    zexCounterBasedEventCloseIpcHandle(newEvent);
}

HWTEST_F(InOrderIpcTests, givenIncorrectParamsWhenUsingIpcApisThenReturnError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    ze_event_handle_t nullEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(nullEvent, &zexIpcData));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), nullptr));

    events[0]->makeCounterBasedInitiallyDisabled(pool->getAllocation());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_context_handle_t nullContext = nullptr;
    ze_event_handle_t newEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventOpenIpcHandle(nullContext, zexIpcData, &newEvent));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, nullptr));
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

HWCMDTEST_F(IGFX_XE_HP_CORE,
            InOrderRegularCmdListTests,
            givenPatchPreambleWhenExecutingInOrderCommandListThenPatchInOrderExecInfoSpace) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);
    mockCmdQHw->setPatchingPreamble(true, false);

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

} // namespace ult
} // namespace L0
