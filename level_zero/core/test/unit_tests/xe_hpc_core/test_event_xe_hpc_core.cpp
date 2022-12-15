/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

class MemoryManagerEventPoolIPCMock : public NEO::MockMemoryManager {
  public:
    MemoryManagerEventPoolIPCMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(executionEnvironment) {}
    void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, AllocationProperties &properties, NEO::MultiGraphicsAllocation &multiGraphicsAllocation) override {
        alloc = new NEO::MockGraphicsAllocation(&buffer, sizeof(buffer));
        alloc->isShareableHostMemory = true;
        multiGraphicsAllocation.addAllocation(alloc);
        return reinterpret_cast<void *>(alloc->getUnderlyingBuffer());
    };
    char buffer[64];
    NEO::MockGraphicsAllocation *alloc;
};

using EventPoolIPCHandleHpcCoreTests = Test<DeviceFixture>;

HWTEST2_F(EventPoolIPCHandleHpcCoreTests, whenGettingIpcHandleForEventPoolWithDeviceAllocThenHandleDeviceAllocAndIsHostVisibleAreReturnedInHandle, IsXeHpcCore) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIPCMock *mockMemoryManager = new MemoryManagerEventPoolIPCMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    EXPECT_TRUE(eventPool->isDeviceEventPoolAllocation);

    auto allocation = &eventPool->getAllocation();

    EXPECT_EQ(allocation->getAllocationType(), NEO::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    int handle = -1;
    memcpy_s(&handle, sizeof(int), ipcHandle.data, sizeof(int));
    EXPECT_NE(handle, -1);

    uint32_t expectedNumEvents = 0;
    memcpy_s(&expectedNumEvents, sizeof(expectedNumEvents), ipcHandle.data + sizeof(int), sizeof(expectedNumEvents));
    EXPECT_EQ(numEvents, expectedNumEvents);

    uint32_t rootDeviceIndex = 0;
    memcpy_s(&rootDeviceIndex, sizeof(rootDeviceIndex), ipcHandle.data + sizeof(int) + sizeof(size_t), sizeof(rootDeviceIndex));
    EXPECT_EQ(0u, rootDeviceIndex);

    bool deviceAlloc = 0;
    memcpy_s(&deviceAlloc, sizeof(deviceAlloc), ipcHandle.data + sizeof(int) + sizeof(size_t) + sizeof(rootDeviceIndex), sizeof(deviceAlloc));
    EXPECT_TRUE(deviceAlloc);

    bool isHostVisible = 0;
    memcpy_s(&isHostVisible, sizeof(isHostVisible), ipcHandle.data + sizeof(int) + sizeof(size_t) + sizeof(rootDeviceIndex) + sizeof(bool), sizeof(isHostVisible));
    EXPECT_TRUE(isHostVisible);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

HWTEST2_F(EventPoolIPCHandleHpcCoreTests, whenOpeningIpcHandleForEventPoolWithHostVisibleThenEventPoolIsCreatedAsDeviceAndIsHostVisibleIsSet, IsXeHpcCore) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIPCMock *mockMemoryManager = new MemoryManagerEventPoolIPCMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto ipcEventPool = static_cast<L0::EventPoolImp *>(L0::EventPool::fromHandle(ipcEventPoolHandle));

    EXPECT_EQ(ipcEventPool->isDeviceEventPoolAllocation, eventPool->isDeviceEventPoolAllocation);
    EXPECT_TRUE(ipcEventPool->isDeviceEventPoolAllocation);

    EXPECT_EQ(ipcEventPool->isHostVisibleEventPoolAllocation, eventPool->isHostVisibleEventPoolAllocation);
    EXPECT_TRUE(ipcEventPool->isHostVisibleEventPoolAllocation);

    res = ipcEventPool->closeIpcHandle();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

} // namespace ult
} // namespace L0