/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>

using namespace std::chrono_literals;

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern volatile uint32_t *pauseAddress;
extern uint32_t pauseValue;
extern uint32_t pauseOffset;
extern std::function<void()> setupPauseAddress;
} // namespace CpuIntrinsicsTests

namespace L0 {
namespace ult {
using EventPoolCreate = Test<DeviceFixture>;
using EventCreate = Test<DeviceFixture>;

class MemoryManagerEventPoolFailMock : public NEO::MemoryManager {
  public:
    MemoryManagerEventPoolFailMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MemoryManager(executionEnvironment) {}
    void *createMultiGraphicsAllocationInSystemMemoryPool(std::vector<uint32_t> &rootDeviceIndices, AllocationProperties &properties, NEO::MultiGraphicsAllocation &multiGraphicsAllocation) override {
        return nullptr;
    };
    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override { return nullptr; }
    void addAllocationToHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    void removeAllocationFromHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    NEO::GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override { return nullptr; };
    AllocationStatus populateOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
    void cleanOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
    void freeGraphicsMemoryImpl(NEO::GraphicsAllocation *gfxAllocation) override{};
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override { return 0; };
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
    AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override {
        return {};
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData, bool useLocalMemory) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const NEO::AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const NEO::AllocationData &allocationData) override { return nullptr; };

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override{};
};

struct EventPoolFailTests : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerEventPoolFailMock(*neoDevice->executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);
        device = driverHandle->devices[0];

        context = std::make_unique<ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }
    NEO::MemoryManager *prevMemoryManager = nullptr;
    NEO::MemoryManager *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextImp> context;
};

TEST_F(EventPoolFailTests, whenCreatingEventPoolAndAllocationFailsThenOutOfDeviceMemoryIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    ze_event_pool_handle_t eventPool = {};
    ze_result_t res = context->createEventPool(&eventPoolDesc, 0, nullptr, &eventPool);
    EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

TEST_F(EventPoolCreate, GivenEventPoolThenAllocationContainsAtLeast16Bytes) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    uint32_t minAllocationSize = eventPool->getEventSize();
    EXPECT_GE(allocation->getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex())->getUnderlyingBufferSize(),
              minAllocationSize);
}

HWTEST_F(EventPoolCreate, givenTimestampEventsThenEventSizeSufficientForAllKernelTimestamps) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    uint32_t maxKernelSplit = 3;
    uint32_t packetsSize = maxKernelSplit * NEO::TimestampPacketSizeControl::preferredPacketCount *
                           static_cast<uint32_t>(NEO::TimestampPackets<typename FamilyType::TimestampPacketType>::getSinglePacketSize());
    uint32_t kernelTimestampsSize = static_cast<uint32_t>(alignUp(packetsSize, 4 * MemoryConstants::cacheLineSize));
    EXPECT_EQ(kernelTimestampsSize, eventPool->getEventSize());
}

TEST_F(EventPoolCreate, givenEventPoolCreatedWithTimestampFlagThenHasTimestampEventsReturnsTrue) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    EventPoolImp *eventPoolImp = static_cast<EventPoolImp *>(eventPool.get());
    EXPECT_TRUE(eventPoolImp->isEventPoolTimestampFlagSet());
}

TEST_F(EventPoolCreate, givenEventPoolCreatedWithNoTimestampFlagThenHasTimestampEventsReturnsFalse) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    EventPoolImp *eventPoolImp = static_cast<EventPoolImp *>(eventPool.get());
    EXPECT_FALSE(eventPoolImp->isEventPoolTimestampFlagSet());
}

TEST_F(EventPoolCreate, givenEventPoolCreatedWithTimestampFlagAndOverrideTimestampEventsFlagThenHasTimestampEventsReturnsFalse) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideTimestampEvents.set(0);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    EventPoolImp *eventPoolImp = static_cast<EventPoolImp *>(eventPool.get());
    EXPECT_FALSE(eventPoolImp->isEventPoolTimestampFlagSet());
}

TEST_F(EventPoolCreate, givenEventPoolCreatedWithoutTimestampFlagAndOverrideTimestampEventsFlagThenHasTimestampEventsReturnsTrue) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideTimestampEvents.set(1);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    EventPoolImp *eventPoolImp = static_cast<EventPoolImp *>(eventPool.get());
    EXPECT_TRUE(eventPoolImp->isEventPoolTimestampFlagSet());
}

TEST_F(EventPoolCreate, givenAnEventIsCreatedFromThisEventPoolThenEventContainsDeviceCommandStreamReceiver) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, event_object->csr);
}

TEST_F(EventPoolCreate, GivenNoDeviceThenEventPoolIsCreated) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        4};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    eventPool->destroy();
}

TEST_F(EventPoolCreate, GivenDeviceThenEventPoolIsCreated) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        4};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    eventPool->destroy();
}

using EventPoolIPCHandleTests = Test<DeviceFixture>;

TEST_F(EventPoolIPCHandleTests, whenGettingIpcHandleForEventPoolThenHandleAndNumberOfEventsAreReturnedInHandle) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    int handle = -1;
    memcpy_s(&handle, sizeof(int), ipcHandle.data, sizeof(int));
    EXPECT_NE(handle, -1);

    uint32_t expectedNumEvents = 0;
    memcpy_s(&expectedNumEvents, sizeof(expectedNumEvents), ipcHandle.data + sizeof(int), sizeof(expectedNumEvents));
    EXPECT_EQ(numEvents, expectedNumEvents);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolIPCHandleTests, whenOpeningIpcHandleForEventPoolThenEventPoolIsCreatedAndEventSizesAreTheSame) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    L0::EventPool *ipcEventPool = L0::EventPool::fromHandle(ipcEventPoolHandle);

    EXPECT_EQ(ipcEventPool->getEventSize(), eventPool->getEventSize());

    res = ipcEventPool->closeIpcHandle();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
}

using EventPoolOpenIPCHandleFailTests = Test<DeviceFixture>;

TEST_F(EventPoolOpenIPCHandleFailTests, givenFailureToAllocateMemoryWhenOpeningIpcHandleForEventPoolThenInvalidArgumentIsReturned) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    {
        NEO::MemoryManager *prevMemoryManager = nullptr;
        NEO::MemoryManager *currMemoryManager = nullptr;

        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new FailMemoryManager(*neoDevice->executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        ze_event_pool_handle_t ipcEventPoolHandle = {};
        res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
        EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);

        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
}

class MultiDeviceEventPoolOpenIPCHandleFailTestsMemoryManager : public FailMemoryManager {
  public:
    MultiDeviceEventPoolOpenIPCHandleFailTestsMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : FailMemoryManager(executionEnvironment) {}

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override {
        return &mockAllocation0;
    }

    GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) override {
        if (calls == 0) {
            calls++;
            return &mockAllocation1;
        }
        return nullptr;
    }

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation) override {
    }

    NEO::MockGraphicsAllocation mockAllocation0;
    NEO::MockGraphicsAllocation mockAllocation1;
    uint32_t calls = 0;
};

using MultiDeviceEventPoolOpenIPCHandleFailTests = Test<MultiDeviceFixture>;

TEST_F(MultiDeviceEventPoolOpenIPCHandleFailTests,
       givenFailureToAllocateMemoryWhenOpeningIpcHandleForEventPoolWithMultipleDevicesThenOutOfHostMemoryIsReturned) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        numEvents};

    auto deviceHandle = driverHandle->devices[0]->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    {
        NEO::MemoryManager *prevMemoryManager = nullptr;
        NEO::MemoryManager *currMemoryManager = nullptr;

        prevMemoryManager = driverHandle->getMemoryManager();
        NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(driverHandle->devices[0]->getNEODevice());
        currMemoryManager = new MultiDeviceEventPoolOpenIPCHandleFailTestsMemoryManager(*neoDevice->executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        ze_event_pool_handle_t ipcEventPoolHandle = {};
        res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
        EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);

        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolCreate, GivenNullptrDeviceAndNumberOfDevicesWhenCreatingEventPoolThenReturnError) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    ze_device_handle_t devices[] = {nullptr, device->toHandle()};
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 2, devices, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    ASSERT_EQ(nullptr, eventPool);
}

TEST_F(EventPoolCreate, GivenNullptrDeviceWithoutNumberOfDevicesWhenCreatingEventPoolThenEventPoolCreated) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    ze_device_handle_t devices[] = {nullptr, device->toHandle()};
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, devices, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
}

TEST_F(EventPoolCreate, whenHostVisibleFlagNotSetThenEventAllocationIsOnDevice) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        0u,
        4};

    ze_device_handle_t devices[] = {nullptr, device->toHandle()};

    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->recentlyPassedDeviceBitfield = systemMemoryBitfield;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, devices, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    EXPECT_EQ(NEO::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER, eventPool->getAllocation().getAllocationType());
    EXPECT_NE(systemMemoryBitfield, memoryManager->recentlyPassedDeviceBitfield);
    EXPECT_EQ(neoDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
}

TEST_F(EventPoolCreate, whenAllocationMemoryFailsThenEventAllocationIsNotCreated) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        0u,
        4};

    ze_device_handle_t devices[] = {nullptr, device->toHandle()};

    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, devices, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
    EXPECT_EQ(nullptr, eventPool);
}

TEST_F(EventCreate, givenAnEventCreatedThenTheEventHasTheDeviceCommandStreamReceiverSet) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};
    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    std::unique_ptr<L0::Event> event(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event);
    ASSERT_NE(nullptr, event.get()->csr);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, event.get()->csr);
}

TEST_F(EventCreate, givenEventWhenSignaledAndResetFromTheHostThenCorrectDataAndOffsetAreSet) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event);

    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    uint64_t gpuAddr = event->getGpuAddress(device);
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    event->hostSignal();
    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    event->reset();
    result = event->queryStatus();
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventCreate, givenAnEventCreateWithInvalidIndexUsingThisEventPoolThenErrorIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};
    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        2,
        ZE_EVENT_SCOPE_FLAG_DEVICE,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    ze_event_handle_t event = nullptr;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_result_t value = eventPool->createEvent(&eventDesc, &event);

    ASSERT_EQ(nullptr, event);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, value);
}

class EventSynchronizeTest : public Test<DeviceFixture> {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;
        eventDesc.wait = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);
        event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::EventPool> eventPool = nullptr;
    std::unique_ptr<L0::Event> event;
};

TEST_F(EventSynchronizeTest, GivenGpuHangWhenHostSynchronizeIsCalledThenDeviceLostIsReturned) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = true;

    event->csr = csr.get();
    event->gpuHangCheckPeriod = 0ms;

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    auto result = event->hostSynchronize(timeout);

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(EventSynchronizeTest, GivenNoGpuHangAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenResultNotReadyIsReturnedDueToTimeout) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = false;

    event->csr = csr.get();
    event->gpuHangCheckPeriod = 0ms;

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = event->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, GivenLongPeriodOfGpuCheckAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenResultNotReadyIsReturnedDueToTimeout) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    event->csr = csr.get();
    event->gpuHangCheckPeriod = 50000000ms;

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = event->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutZeroAndStateInitialHostSynchronizeReturnsNotReady) {
    ze_result_t result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithNonZeroTimeoutAndStateInitialHostSynchronizeReturnsNotReady) {
    ze_result_t result = event->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutZeroAndStateSignaledHostSynchronizeReturnsSuccess) {
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;
    ze_result_t result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutNonZeroAndStateSignaledHostSynchronizeReturnsSuccess) {
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;
    ze_result_t result = event->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventSynchronizeTest, givenInfiniteTimeoutWhenWaitingForNonTimestampEventCompletionThenReturnOnlyAfterAllEventPacketsAreCompleted) {
    constexpr uint32_t packetsInUse = 2;
    event->setPacketsInUse(packetsInUse);

    const size_t eventPacketSize = event->getSinglePacketSize();
    const size_t eventCompletionOffset = event->getContextStartOffset();

    VariableBackup<volatile uint32_t *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<uint32_t> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = static_cast<uint32_t *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));

    uint32_t *hostAddr = static_cast<uint32_t *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    for (uint32_t i = 0; i < packetsInUse; i++) {
        *hostAddr = Event::STATE_CLEARED;
        hostAddr = ptrOffset(hostAddr, eventPacketSize);
    }

    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (CpuIntrinsicsTests::pauseCounter > 10) {
            volatile uint32_t *nextPacket = CpuIntrinsicsTests::pauseAddress;
            for (uint32_t i = 0; i < packetsInUse; i++) {
                *nextPacket = Event::STATE_SIGNALED;
                nextPacket = ptrOffset(nextPacket, eventPacketSize);
            }
        }
    };

    constexpr uint64_t infiniteTimeout = std::numeric_limits<std::uint64_t>::max();
    ze_result_t result = event->hostSynchronize(infiniteTimeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventSynchronizeTest, givenInfiniteTimeoutWhenWaitingForTimestampEventCompletionThenReturnOnlyAfterAllEventPacketsAreCompleted) {
    constexpr uint32_t packetsInUse = 2;
    event->setPacketsInUse(packetsInUse);
    event->setEventTimestampFlag(true);

    const size_t eventPacketSize = event->getSinglePacketSize();
    const size_t eventCompletionOffset = event->getContextEndOffset();

    VariableBackup<volatile uint32_t *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<uint32_t> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = static_cast<uint32_t *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));

    uint32_t *hostAddr = static_cast<uint32_t *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    for (uint32_t i = 0; i < packetsInUse; i++) {
        *hostAddr = Event::STATE_CLEARED;
        hostAddr = ptrOffset(hostAddr, eventPacketSize);
    }

    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (CpuIntrinsicsTests::pauseCounter > 10) {
            volatile uint32_t *nextPacket = CpuIntrinsicsTests::pauseAddress;
            for (uint32_t i = 0; i < packetsInUse; i++) {
                *nextPacket = Event::STATE_SIGNALED;
                nextPacket = ptrOffset(nextPacket, eventPacketSize);
            }
        }
    };

    constexpr uint64_t infiniteTimeout = std::numeric_limits<std::uint64_t>::max();
    ze_result_t result = event->hostSynchronize(infiniteTimeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using EventPoolIPCEventResetTests = Test<DeviceFixture>;

TEST_F(EventPoolIPCEventResetTests, whenOpeningIpcHandleForEventPoolCreateWithIpcFlagThenEventsInNewPoolAreNotReset) {
    std::unique_ptr<L0::EventPoolImp> eventPool = nullptr;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool = std::unique_ptr<L0::EventPoolImp>(static_cast<L0::EventPoolImp *>(L0::EventPool::create(driverHandle.get(),
                                                                                                        context,
                                                                                                        0,
                                                                                                        nullptr,
                                                                                                        &eventPoolDesc,
                                                                                                        result)));
    EXPECT_NE(nullptr, eventPool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::unique_ptr<L0::EventImp<uint32_t>> event0;
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    event0 = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(),
                                                                                                                       &eventDesc,
                                                                                                                       device)));
    EXPECT_NE(nullptr, event0);

    uint32_t *hostAddr = static_cast<uint32_t *>(event0->getHostAddress());
    EXPECT_EQ(*hostAddr, Event::STATE_INITIAL);

    // change state
    event0->hostSignal();
    hostAddr = static_cast<uint32_t *>(event0->getHostAddress());
    EXPECT_EQ(*hostAddr, Event::STATE_SIGNALED);

    // create an event from the pool with the same index as event0, but this time, since isImportedIpcPool is true, no reset should happen
    eventPool->isImportedIpcPool = true;
    std::unique_ptr<L0::EventImp<uint32_t>> event1;
    event1 = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(),
                                                                                                                       &eventDesc,
                                                                                                                       device)));
    EXPECT_NE(nullptr, event1);

    uint32_t *hostAddr1 = static_cast<uint32_t *>(event1->getHostAddress());
    EXPECT_EQ(*hostAddr1, Event::STATE_SIGNALED);

    // create another event from the pool with the same index, but this time, since isImportedIpcPool is false, reset should happen
    eventPool->isImportedIpcPool = false;
    std::unique_ptr<L0::EventImp<uint32_t>> event2;
    event2 = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(),
                                                                                                                       &eventDesc,
                                                                                                                       device)));
    EXPECT_NE(nullptr, event2);

    uint32_t *hostAddr2 = static_cast<uint32_t *>(event2->getHostAddress());
    EXPECT_EQ(*hostAddr2, Event::STATE_INITIAL);
}

using EventAubCsrTest = Test<DeviceFixture>;

HWTEST_F(EventAubCsrTest, givenCallToEventHostSynchronizeWithAubModeCsrReturnsSuccess) {
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];
    int32_t tag;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    neoDevice->resetCommandStreamReceiver(aubCsr);

    std::unique_ptr<L0::EventPool> eventPool = nullptr;
    std::unique_ptr<L0::Event> event;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event);

    result = event->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct EventCreateAllocationResidencyTest : public ::testing::Test {
    void SetUp() override {
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        mockMemoryOperationsHandler = new NEO::MockMemoryOperationsHandlerTests;
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(
            mockMemoryOperationsHandler);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() override {
    }

    NEO::MockMemoryOperationsHandlerTests *mockMemoryOperationsHandler;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
};

class TimestampEventCreate : public Test<DeviceFixture> {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;
        eventDesc.wait = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);
        event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device)));
        ASSERT_NE(nullptr, event);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<L0::EventImp<uint32_t>> event;
};

TEST_F(TimestampEventCreate, givenEventCreatedWithTimestampThenIsTimestampEventFlagSet) {
    EXPECT_TRUE(event->isEventTimestampFlagSet());
}

TEST_F(TimestampEventCreate, givenEventTimestampsCreatedWhenResetIsInvokeThenCorrectDataAreSet) {
    EXPECT_NE(nullptr, event->kernelEventCompletionData);
    for (auto j = 0u; j < EventPacketsCount::maxKernelSplit; j++) {
        for (auto i = 0u; i < NEO::TimestampPacketSizeControl::preferredPacketCount; i++) {
            EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData[j].getContextStartValue(i));
            EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData[j].getGlobalStartValue(i));
            EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData[j].getContextEndValue(i));
            EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData[j].getGlobalEndValue(i));
        }
        EXPECT_EQ(1u, event->kernelEventCompletionData[j].getPacketsUsed());
    }

    EXPECT_EQ(1u, event->kernelCount);
}

TEST_F(TimestampEventCreate, givenSingleTimestampEventThenAllocationSizeCreatedForAllTimestamps) {
    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    uint32_t minTimestampEventAllocation = eventPool->getEventSize();
    EXPECT_GE(allocation->getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex())->getUnderlyingBufferSize(),
              minTimestampEventAllocation);
}

TEST_F(TimestampEventCreate, givenTimestampEventThenAllocationsIsOfPacketTagBufferType) {
    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(NEO::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, allocation->getAllocationType());
}

TEST_F(TimestampEventCreate, givenEventTimestampWhenPacketCountIsSetThenCorrectOffsetIsReturned) {
    EXPECT_EQ(1u, event->getPacketsInUse());
    auto gpuAddr = event->getGpuAddress(device);
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    event->setPacketsInUse(4u);
    EXPECT_EQ(4u, event->getPacketsInUse());

    gpuAddr += (4u * event->getSinglePacketSize());

    event->kernelCount = 2;
    event->setPacketsInUse(2u);
    EXPECT_EQ(6u, event->getPacketsInUse());
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    gpuAddr += (2u * event->getSinglePacketSize());
    event->kernelCount = 3;
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));
    EXPECT_EQ(7u, event->getPacketsInUse());
}

TEST_F(TimestampEventCreate, givenEventWhenSignaledAndResetFromTheHostThenCorrectDataAreSet) {
    EXPECT_NE(nullptr, event->kernelEventCompletionData);
    event->hostSignal();
    ze_result_t result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    event->reset();
    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    for (auto j = 0u; j < EventPacketsCount::maxKernelSplit; j++) {
        for (auto i = 0u; i < NEO::TimestampPacketSizeControl::preferredPacketCount; i++) {
            EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData[j].getContextStartValue(i));
            EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData[j].getGlobalStartValue(i));
            EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData[j].getContextEndValue(i));
            EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData[j].getGlobalEndValue(i));
        }
        EXPECT_EQ(1u, event->kernelEventCompletionData[j].getPacketsUsed());
    }
    EXPECT_EQ(1u, event->kernelCount);
}

TEST_F(TimestampEventCreate, givenpCountZeroCallingQueryTimestampExpThenpCountSetProperly) {
    uint32_t pCount = 0;
    auto result = event->queryTimestampsExp(device, &pCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, pCount);
}

TEST_F(TimestampEventCreate, givenpCountLargerThanSupportedWhenCallingQueryTimestampExpThenpCountSetProperly) {
    uint32_t pCount = 10;
    event->setPacketsInUse(2u);
    auto result = event->queryTimestampsExp(device, &pCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, pCount);
}

TEST_F(TimestampEventCreate, givenEventWithStaticPartitionOffThenQueryTimestampExpReturnsUnsupported) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.EnableStaticPartitioning.set(0);

    uint32_t pCount = 0;
    auto result = event->queryTimestampsExp(device, &pCount, nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

class TimestampDeviceEventCreate : public Test<DeviceFixture> {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;
        eventDesc.wait = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);
        event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device)));
        ASSERT_NE(nullptr, event);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<L0::EventImp<uint32_t>> event;
};

TEST_F(TimestampDeviceEventCreate, givenTimestampDeviceEventThenAllocationsIsOfGpuDeviceTimestampType) {
    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(NEO::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER, allocation->getAllocationType());
}

using EventQueryTimestampExpWithSubDevice = Test<MultiDeviceFixture>;

TEST_F(EventQueryTimestampExpWithSubDevice, givenEventWhenQuerytimestampExpWithSubDeviceThenReturnsCorrectValueReturned) {
    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<L0::EventImp<uint32_t>> event;
    uint32_t deviceCount = 1;
    ze_device_handle_t rootDeviceHandle;

    ze_result_t result = zeDeviceGet(driverHandle.get(), &deviceCount, &rootDeviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    deviceCount = 0;
    result = zeDeviceGetSubDevices(rootDeviceHandle, &deviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(deviceCount >= 2);

    auto subDeviceHandle = std::make_unique<ze_device_handle_t[]>(deviceCount);
    result = zeDeviceGetSubDevices(rootDeviceHandle, &deviceCount, subDeviceHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;

    auto subDeviceId = 0u;
    auto subdevice = L0::Device::fromHandle(subDeviceHandle[subDeviceId]);
    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 1, &subDeviceHandle[0], &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, subdevice)));
    ASSERT_NE(nullptr, event);

    class MockTimestampPackets32 : public TimestampPackets<uint32_t> {
      public:
        using typename TimestampPackets<uint32_t>::Packet;
    };

    typename MockTimestampPackets32::Packet packetData[2];
    event->setPacketsInUse(2u);

    packetData[0].contextStart = 1u;
    packetData[0].contextEnd = 2u;
    packetData[0].globalStart = 3u;
    packetData[0].globalEnd = 4u;

    packetData[1].contextStart = 5u;
    packetData[1].contextEnd = 6u;
    packetData[1].globalStart = 7u;
    packetData[1].globalEnd = 8u;

    event->hostAddress = packetData;

    ze_kernel_timestamp_result_t results[2];
    uint32_t numPackets = 2;

    for (uint32_t packetId = 0; packetId < numPackets; packetId++) {
        event->kernelEventCompletionData[0].assignDataToAllTimestamps(packetId, event->hostAddress);
        event->hostAddress = ptrOffset(event->hostAddress, NEO::TimestampPackets<uint32_t>::getSinglePacketSize());
    }
    uint32_t pCount = 0;
    result = event->queryTimestampsExp(subdevice, &pCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, pCount);
    result = event->queryTimestampsExp(subdevice, &pCount, results);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (uint32_t i = 0; i < pCount; i++) {
        EXPECT_EQ(packetData[subDeviceId].contextStart, results[i].context.kernelStart);
        EXPECT_EQ(packetData[subDeviceId].contextEnd, results[i].context.kernelEnd);
        EXPECT_EQ(packetData[subDeviceId].globalStart, results[i].global.kernelStart);
        EXPECT_EQ(packetData[subDeviceId].globalEnd, results[i].global.kernelEnd);
    }
}

HWCMDTEST_F(IGFX_GEN9_CORE, TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet) {
    class MockTimestampPackets32 : public TimestampPackets<uint32_t> {
      public:
        using typename TimestampPackets<uint32_t>::Packet;
    };

    typename MockTimestampPackets32::Packet data = {};
    data.contextStart = 1u;
    data.contextEnd = 2u;
    data.globalStart = 3u;
    data.globalEnd = 4u;

    event->hostAddress = &data;
    ze_kernel_timestamp_result_t result = {};

    event->queryKernelTimestamp(&result);
    EXPECT_EQ(data.contextStart, result.context.kernelStart);
    EXPECT_EQ(data.contextEnd, result.context.kernelEnd);
    EXPECT_EQ(data.globalStart, result.global.kernelStart);
    EXPECT_EQ(data.globalEnd, result.global.kernelEnd);
}

TEST_F(TimestampEventCreate, givenEventWhenQueryingTimestampExpThenCorrectDataSet) {
    class MockTimestampPackets32 : public TimestampPackets<uint32_t> {
      public:
        using typename TimestampPackets<uint32_t>::Packet;
    };

    typename MockTimestampPackets32::Packet packetData[2];
    event->setPacketsInUse(2u);

    packetData[0].contextStart = 1u;
    packetData[0].contextEnd = 2u;
    packetData[0].globalStart = 3u;
    packetData[0].globalEnd = 4u;

    packetData[1].contextStart = 5u;
    packetData[1].contextEnd = 6u;
    packetData[1].globalStart = 7u;
    packetData[1].globalEnd = 8u;

    event->hostAddress = packetData;

    ze_kernel_timestamp_result_t results[2];
    uint32_t pCount = 2;

    for (uint32_t packetId = 0; packetId < pCount; packetId++) {
        event->kernelEventCompletionData[0].assignDataToAllTimestamps(packetId, event->hostAddress);
        event->hostAddress = ptrOffset(event->hostAddress, NEO::TimestampPackets<uint32_t>::getSinglePacketSize());
    }

    auto result = event->queryTimestampsExp(device, &pCount, results);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (uint32_t i = 0; i < pCount; i++) {
        EXPECT_EQ(packetData[i].contextStart, results[i].context.kernelStart);
        EXPECT_EQ(packetData[i].contextEnd, results[i].context.kernelEnd);
        EXPECT_EQ(packetData[i].globalStart, results[i].global.kernelStart);
        EXPECT_EQ(packetData[i].globalEnd, results[i].global.kernelEnd);
    }
}

HWTEST_EXCLUDE_PRODUCT(TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet, IGFX_GEN12LP_CORE);

TEST_F(TimestampEventCreate, givenEventWhenQueryKernelTimestampThenNotReadyReturned) {
    struct MockEventQuery : public EventImp<uint32_t> {
        MockEventQuery(L0::EventPool *eventPool, int index, L0::Device *device) : EventImp(eventPool, index, device) {}

        ze_result_t queryStatus() override {
            return ZE_RESULT_NOT_READY;
        }
    };

    auto mockEvent = std::make_unique<MockEventQuery>(eventPool.get(), 1u, device);

    ze_kernel_timestamp_result_t resultTimestamp = {};

    auto result = mockEvent->queryKernelTimestamp(&resultTimestamp);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    EXPECT_EQ(0u, resultTimestamp.context.kernelStart);
    EXPECT_EQ(0u, resultTimestamp.context.kernelEnd);
    EXPECT_EQ(0u, resultTimestamp.global.kernelStart);
    EXPECT_EQ(0u, resultTimestamp.global.kernelEnd);
}

using EventPoolCreateMultiDevice = Test<MultiDeviceFixture>;

TEST_F(EventPoolCreateMultiDevice, whenCreatingEventPoolWithMultipleDevicesThenEventPoolCreateSucceeds) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 32;

    uint32_t deviceCount = 0;
    ze_result_t result = zeDeviceGet(driverHandle.get(), &deviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(deviceCount, numRootDevices);

    ze_device_handle_t *devices = new ze_device_handle_t[deviceCount];
    result = zeDeviceGet(driverHandle.get(), &deviceCount, devices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(),
                                                               context,
                                                               deviceCount,
                                                               devices,
                                                               &eventPoolDesc,
                                                               result));
    EXPECT_NE(nullptr, eventPool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto allocation = &eventPool->getAllocation();
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocation->getGraphicsAllocations().size(), numRootDevices);

    delete[] devices;
}

TEST_F(EventPoolCreateMultiDevice, whenCreatingEventPoolWithMultipleDevicesThenDontDuplicateRootDeviceIndices) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 32;

    uint32_t deviceCount = 1;
    ze_device_handle_t rootDeviceHandle;

    ze_result_t result = zeDeviceGet(driverHandle.get(), &deviceCount, &rootDeviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    deviceCount = 0;
    result = zeDeviceGetSubDevices(rootDeviceHandle, &deviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(deviceCount >= 2);

    auto subDeviceHandle = std::make_unique<ze_device_handle_t[]>(deviceCount);
    result = zeDeviceGetSubDevices(rootDeviceHandle, &deviceCount, subDeviceHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(),
                                                               context,
                                                               deviceCount,
                                                               subDeviceHandle.get(),
                                                               &eventPoolDesc,
                                                               result));
    EXPECT_NE(nullptr, eventPool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto allocation = &eventPool->getAllocation();
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocation->getGraphicsAllocations().size(), 1u);
}

TEST_F(EventPoolCreateMultiDevice, whenCreatingEventPoolWithNoDevicesThenEventPoolCreateSucceedsAndAllDeviceAreUsed) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 32;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(),
                                                               context,
                                                               0,
                                                               nullptr,
                                                               &eventPoolDesc,
                                                               result));
    EXPECT_NE(nullptr, eventPool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto allocation = &eventPool->getAllocation();
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocation->getGraphicsAllocations().size(), numRootDevices);
}

using EventPoolCreateSingleDevice = Test<DeviceFixture>;

TEST_F(EventPoolCreateSingleDevice, whenCreatingEventPoolWithNoDevicesThenEventPoolCreateSucceedsAndSingleDeviceIsUsed) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 32;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(),
                                                               context,
                                                               0,
                                                               nullptr,
                                                               &eventPoolDesc,
                                                               result));
    EXPECT_NE(nullptr, eventPool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto allocation = &eventPool->getAllocation();
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocation->getGraphicsAllocations().size(), 1u);
}

struct EventPoolCreateNegativeTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (uint32_t i = 0; i < numRootDevices; i++) {
            neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, i);
            devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->isMockEventPoolCreateMemoryManager = true;

        device = driverHandle->devices[0];

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = static_cast<ContextImp *>(Context::fromHandle(hContext));
    }
    void TearDown() override {
        context->destroy();
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t numRootDevices = 2u;
    L0::ContextImp *context = nullptr;
};

TEST_F(EventPoolCreateNegativeTest, whenCreatingEventPoolButMemoryManagerFailsThenErrorIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 32;

    uint32_t deviceCount = 0;
    ze_result_t result = zeDeviceGet(driverHandle.get(), &deviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(deviceCount, numRootDevices);

    ze_device_handle_t *devices = new ze_device_handle_t[deviceCount];
    result = zeDeviceGet(driverHandle.get(), &deviceCount, devices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(),
                                                               context,
                                                               deviceCount,
                                                               devices,
                                                               &eventPoolDesc,
                                                               result));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
    EXPECT_EQ(nullptr, eventPool);
    delete[] devices;
}

TEST_F(EventPoolCreateNegativeTest, whenInitializingEventPoolButMemoryManagerFailsThenErrorIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 32;

    uint32_t deviceCount = 0;
    ze_result_t result = zeDeviceGet(driverHandle.get(), &deviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(deviceCount, numRootDevices);

    ze_device_handle_t *devices = new ze_device_handle_t[deviceCount];
    result = zeDeviceGet(driverHandle.get(), &deviceCount, devices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto eventPool = new L0::EventPoolImp(&eventPoolDesc);
    EXPECT_NE(nullptr, eventPool);

    result = eventPool->initialize(driverHandle.get(), context, numRootDevices, devices);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);

    delete eventPool;
    delete[] devices;
}

class EventFixture : public DeviceFixture {
  public:
    void SetUp() {
        DeviceFixture::SetUp();

        auto hDevice = device->toHandle();
        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool = whitebox_cast(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
    }

    void TearDown() {
        eventPool->destroy();

        DeviceFixture::TearDown();
    }

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        4};

    ze_event_desc_t eventDesc = {};
    EventPool *eventPool;
};

using EventTests = Test<EventFixture>;

TEST_F(EventTests, WhenQueryingStatusThenSuccessIsReturned) {
    auto event = whitebox_cast(Event::create<uint32_t>(eventPool, &eventDesc, device));
    ASSERT_NE(event, nullptr);

    auto result = event->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);

    event->destroy();
}

TEST_F(EventTests, GivenResetWhenQueryingStatusThenNotReadyIsReturned) {
    auto event = whitebox_cast(Event::create<uint32_t>(eventPool, &eventDesc, device));
    ASSERT_NE(event, nullptr);

    auto result = event->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = event->reset();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);

    event->destroy();
}

TEST_F(EventTests, WhenDestroyingAnEventThenSuccessIsReturned) {
    auto event = whitebox_cast(Event::create<uint32_t>(eventPool, &eventDesc, device));
    ASSERT_NE(event, nullptr);

    auto result = event->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventTests, givenTwoEventsCreatedThenTheyHaveDifferentAddresses) {
    ze_event_desc_t eventDesc0 = {};
    eventDesc0.index = 0;
    eventDesc.index = 0;

    ze_event_desc_t eventDesc1 = {};
    eventDesc1.index = 1;
    eventDesc.index = 1;

    auto event0 = whitebox_cast(Event::create<uint32_t>(eventPool, &eventDesc0, device));
    ASSERT_NE(event0, nullptr);

    auto event1 = whitebox_cast(Event::create<uint32_t>(eventPool, &eventDesc1, device));
    ASSERT_NE(event1, nullptr);

    EXPECT_NE(event0->hostAddress, event1->hostAddress);
    EXPECT_NE(event0->getGpuAddress(device), event1->getGpuAddress(device));

    event0->destroy();
    event1->destroy();
}

TEST_F(EventTests, givenRegularEventUseMultiplePacketsWhenHostSignalThenExpectAllPacketsAreSignaled) {
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool,
                                                                                                                           &eventDesc,
                                                                                                                           device)));
    ASSERT_NE(event, nullptr);

    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    EXPECT_EQ(*hostAddr, Event::STATE_INITIAL);
    EXPECT_EQ(1u, event->getPacketsInUse());

    constexpr uint32_t packetsUsed = 4u;
    event->setPacketsInUse(packetsUsed);
    event->setEventTimestampFlag(false);
    event->hostSignal();
    for (uint32_t i = 0; i < packetsUsed; i++) {
        EXPECT_EQ(Event::STATE_SIGNALED, *hostAddr);
        hostAddr = ptrOffset(hostAddr, event->getSinglePacketSize());
    }
}

struct EventSizeFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();
        hDevice = device->toHandle();
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }

    void createEvents() {
        ze_event_handle_t hEvent0 = 0;
        ze_event_handle_t hEvent1 = 0;
        ze_event_desc_t eventDesc0 = {};
        ze_event_desc_t eventDesc1 = {};

        eventDesc0.index = 0;
        eventDesc1.index = 1;

        auto result = eventPool->createEvent(&eventDesc0, &hEvent0);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        result = eventPool->createEvent(&eventDesc1, &hEvent1);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        eventObj0.reset(L0::Event::fromHandle(hEvent0));
        eventObj1.reset(L0::Event::fromHandle(hEvent1));
    }

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        4};

    DebugManagerStateRestore restore;
    ze_device_handle_t hDevice = 0;
    std::unique_ptr<EventPoolImp> eventPool;
    std::unique_ptr<L0::Event> eventObj0;
    std::unique_ptr<L0::Event> eventObj1;
};

using EventSizeTests = Test<EventSizeFixture>;

HWTEST_F(EventSizeTests, whenCreatingEventPoolThenUseCorrectSizeAndAlignment) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool.reset(static_cast<EventPoolImp *>(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &hwHelper = device->getHwHelper();

    auto expectedAlignment = static_cast<uint32_t>(hwHelper.getTimestampPacketAllocatorAlignment());
    auto singlePacketSize = TimestampPackets<typename FamilyType::TimestampPacketType>::getSinglePacketSize();
    auto expectedSize = static_cast<uint32_t>(alignUp(EventPacketsCount::eventPackets * singlePacketSize, expectedAlignment));

    EXPECT_EQ(expectedSize, eventPool->getEventSize());

    createEvents();

    constexpr size_t timestampPacketTypeSize = sizeof(typename FamilyType::TimestampPacketType);

    EXPECT_EQ(timestampPacketTypeSize / 4, eventObj0->getTimestampSizeInDw());
    EXPECT_EQ(timestampPacketTypeSize / 4, eventObj1->getTimestampSizeInDw());

    EXPECT_EQ(0u, eventObj0->getContextStartOffset());
    EXPECT_EQ(timestampPacketTypeSize, eventObj0->getGlobalStartOffset());
    EXPECT_EQ(timestampPacketTypeSize * 2, eventObj0->getContextEndOffset());
    EXPECT_EQ(timestampPacketTypeSize * 3, eventObj0->getGlobalEndOffset());

    EXPECT_EQ(timestampPacketTypeSize * 4, eventObj0->getSinglePacketSize());

    auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
    EXPECT_EQ(expectedSize, hostPtrDiff);
}

HWTEST_F(EventSizeTests, givenDebugFlagwhenCreatingEventPoolThenUseCorrectSizeAndAlignment) {

    auto &hwHelper = device->getHwHelper();
    auto expectedAlignment = static_cast<uint32_t>(hwHelper.getTimestampPacketAllocatorAlignment());

    {
        DebugManager.flags.OverrideTimestampPacketSize.set(4);

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool.reset(static_cast<EventPoolImp *>(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result)));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto singlePacketSize = TimestampPackets<uint32_t>::getSinglePacketSize();

        auto expectedSize = static_cast<uint32_t>(alignUp(EventPacketsCount::eventPackets * singlePacketSize, expectedAlignment));

        EXPECT_EQ(expectedSize, eventPool->getEventSize());

        createEvents();

        EXPECT_EQ(1u, eventObj0->getTimestampSizeInDw());
        EXPECT_EQ(1u, eventObj1->getTimestampSizeInDw());

        auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
        EXPECT_EQ(expectedSize, hostPtrDiff);
    }

    {
        DebugManager.flags.OverrideTimestampPacketSize.set(8);

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool.reset(static_cast<EventPoolImp *>(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result)));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto singlePacketSize = TimestampPackets<uint64_t>::getSinglePacketSize();

        auto expectedSize = static_cast<uint32_t>(alignUp(EventPacketsCount::eventPackets * singlePacketSize, expectedAlignment));

        EXPECT_EQ(expectedSize, eventPool->getEventSize());

        createEvents();

        EXPECT_EQ(2u, eventObj0->getTimestampSizeInDw());
        EXPECT_EQ(2u, eventObj1->getTimestampSizeInDw());

        auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
        EXPECT_EQ(expectedSize, hostPtrDiff);
    }

    {
        DebugManager.flags.OverrideTimestampPacketSize.set(12);
        ze_result_t result = ZE_RESULT_SUCCESS;
        EXPECT_ANY_THROW(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
        EXPECT_ANY_THROW(createEvents());
    }
}

} // namespace ult
} // namespace L0
