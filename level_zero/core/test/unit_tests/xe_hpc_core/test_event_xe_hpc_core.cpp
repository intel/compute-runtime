/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace NEO {
class ExecutionEnvironment;
} // namespace NEO

namespace L0 {
namespace ult {

class MemoryManagerEventPoolIPCMockXeHpc : public NEO::MockMemoryManager {
  public:
    MemoryManagerEventPoolIPCMockXeHpc(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(executionEnvironment) {}
    void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, AllocationProperties &properties, NEO::MultiGraphicsAllocation &multiGraphicsAllocation) override {
        alloc = new NEO::MockGraphicsAllocation(&buffer, sizeof(buffer));
        alloc->setShareableHostMemory(true);
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
    MemoryManagerEventPoolIPCMockXeHpc *mockMemoryManager = new MemoryManagerEventPoolIPCMockXeHpc(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    EXPECT_TRUE(eventPool->isDeviceEventPoolAllocation);

    auto allocation = &eventPool->getAllocation();

    EXPECT_EQ(allocation->getAllocationType(), NEO::AllocationType::gpuTimestampDeviceBuffer);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle);
    EXPECT_EQ(numEvents, ipcHandleData.numEvents);
    EXPECT_EQ(0u, ipcHandleData.rootDeviceIndex);
    EXPECT_EQ(1u, ipcHandleData.numDevices);
    EXPECT_TRUE(ipcHandleData.isDeviceEventPoolAllocation);
    EXPECT_TRUE(ipcHandleData.isHostVisibleEventPoolAllocation);
    EXPECT_EQ(ipcHandleData.isImplicitScalingCapable, device->isImplicitScalingCapable());
    EXPECT_EQ(ipcHandleData.isImplicitScalingCapable, eventPool->isImplicitScalingCapableFlagSet());

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
    MemoryManagerEventPoolIPCMockXeHpc *mockMemoryManager = new MemoryManagerEventPoolIPCMockXeHpc(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto ipcEventPool = whiteboxCast(L0::EventPool::fromHandle(ipcEventPoolHandle));

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
