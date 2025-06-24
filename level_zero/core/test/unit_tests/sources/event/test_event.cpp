/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/mock_timestamp_packet.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/event_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"
#include "level_zero/driver_experimental/zex_api.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>

using namespace std::chrono_literals;

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern volatile TagAddressType *pauseAddress;
extern TaskCountType pauseValue;
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
    void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, AllocationProperties &properties, NEO::MultiGraphicsAllocation &multiGraphicsAllocation) override {
        return nullptr;
    };
    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
    void addAllocationToHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    void removeAllocationFromHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    AllocationStatus populateOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
    void cleanOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
    void freeGraphicsMemoryImpl(NEO::GraphicsAllocation *gfxAllocation) override{};
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override { return 0; };
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
    AddressRange reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override {
        return {};
    }
    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override {
        return {};
    }
    size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override {
        *heap = HeapIndex::heapStandard;
        return MemoryConstants::pageSize64k;
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override { return {}; }
    void freeCpuAddress(AddressRange addressRange) override{};
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const NEO::AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const NEO::AllocationData &allocationData) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    void unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) override { return; };
    void unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return; };
    bool mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };
    bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override{};
};

struct EventPoolFailTests : public ::testing::Test {
    void SetUp() override {

        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));

        prevMemoryManagerDriver = driverHandle->getMemoryManager();
        prevMemoryManagerExecEnv = neoDevice->executionEnvironment->memoryManager.release();

        currMemoryManager = new MemoryManagerEventPoolFailMock(*neoDevice->executionEnvironment);

        driverHandle->setMemoryManager(currMemoryManager);
        neoDevice->executionEnvironment->memoryManager.reset(currMemoryManager);

        device = driverHandle->devices[0];

        context = std::make_unique<ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->setMemoryManager(prevMemoryManagerDriver);

        neoDevice->executionEnvironment->memoryManager.release();
        neoDevice->executionEnvironment->memoryManager.reset(prevMemoryManagerExecEnv);

        delete currMemoryManager;
    }

    NEO::MemoryManager *prevMemoryManagerDriver = nullptr;
    NEO::MemoryManager *prevMemoryManagerExecEnv = nullptr;

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

TEST_F(EventPoolFailTests, givenEnabledTimestampPoolAllocatorWhenCreatingEventPoolAndAllocationFailsThenOutOfDeviceMemoryIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableTimestampPoolAllocator.set(1);

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
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

TEST_F(EventPoolCreate, givenInvalidPNextWhenCreatingPoolThenIgnore) {
    ze_base_desc_t baseDesc = {ZE_STRUCTURE_TYPE_FORCE_UINT32};

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        &baseDesc,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
}

TEST_F(EventPoolCreate, givenValidEventPoolWithFlagsWhenCallingGetFlagsThenCorrectFlagsAreReturned) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    std::vector<ze_event_pool_flags_t> testingFlags = {
        0,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        ZE_EVENT_POOL_FLAG_IPC,
        ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP};

    for (auto &flags : testingFlags) {
        ze_event_pool_flags_t eventPoolFlags = ZE_EVENT_POOL_FLAG_FORCE_UINT32;
        eventPoolDesc.flags = flags;
        ze_result_t result = ZE_RESULT_SUCCESS;
        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->getFlags(&eventPoolFlags));
        ASSERT_EQ(eventPoolDesc.flags, eventPoolFlags);
    }
}

TEST_F(EventPoolCreate, givenValidEventPoolWhenCallingGetContextThenCorrectContextIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_context_handle_t hContext;
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->getContextHandle(&hContext));
    ASSERT_EQ(hContext, context->toHandle());
}

HWTEST_F(EventPoolCreate, givenTimestampEventsThenEventSizeSufficientForAllKernelTimestamps) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    auto &hwInfo = device->getHwInfo();
    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    auto &gfxCoreHelper = device->getGfxCoreHelper();

    uint32_t maxPacketCount = EventPacketsCount::maxKernelSplit * NEO::TimestampPacketConstants::preferredPacketCount;
    if (l0GfxCoreHelper.useDynamicEventPacketsCount(hwInfo)) {
        maxPacketCount = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());
    }

    uint32_t packetsSize = maxPacketCount *
                           static_cast<uint32_t>(NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize());
    uint32_t kernelTimestampsSize = static_cast<uint32_t>(alignUp(packetsSize, gfxCoreHelper.getTimestampPacketAllocatorAlignment()));
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
    EXPECT_TRUE(eventPool->isEventPoolTimestampFlagSet());
}

TEST_F(EventPoolCreate, givenEventPoolCreatedWithNoTimestampFlagThenHasTimestampEventsReturnsFalse) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    EXPECT_FALSE(eventPool->isEventPoolTimestampFlagSet());
}

TEST_F(EventPoolCreate, givenEventPoolCreatedWithTimestampFlagAndOverrideTimestampEventsFlagThenHasTimestampEventsReturnsFalse) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideTimestampEvents.set(0);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    EXPECT_FALSE(eventPool->isEventPoolTimestampFlagSet());
}

TEST_F(EventPoolCreate, givenEventPoolCreatedWithoutTimestampFlagAndOverrideTimestampEventsFlagThenHasTimestampEventsReturnsTrue) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideTimestampEvents.set(1);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    EXPECT_TRUE(eventPool->isEventPoolTimestampFlagSet());
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

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);
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

    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    if (l0GfxCoreHelper.alwaysAllocateEventInLocalMem()) {
        EXPECT_EQ(NEO::AllocationType::gpuTimestampDeviceBuffer, eventPool->getAllocation().getAllocationType());
    } else {
        EXPECT_EQ(NEO::AllocationType::timestampPacketTagBuffer, eventPool->getAllocation().getAllocationType());
    }
    eventPool->destroy();
}

TEST_F(EventPoolCreate, GivenEnabledTimestampPoolAllocatorWhenCreatingEventPoolWithIpcFlagThenTimestampPoolAllocatorIsNotUsed) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableTimestampPoolAllocator.set(1);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    {
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_IPC;
        ze_result_t result = ZE_RESULT_SUCCESS;
        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        EXPECT_FALSE(driverHandle->devices[0]->getNEODevice()->getDeviceTimestampPoolAllocator().isPoolBuffer(eventPool->getAllocation().getDefaultGraphicsAllocation()));
    }
    {
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        ze_result_t result = ZE_RESULT_SUCCESS;
        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        EXPECT_TRUE(driverHandle->devices[0]->getNEODevice()->getDeviceTimestampPoolAllocator().isPoolBuffer(eventPool->getAllocation().getDefaultGraphicsAllocation()));
    }
}

struct EventPoolIpcMockGraphicsAllocation : public NEO::MockGraphicsAllocation {
    using NEO::MockGraphicsAllocation::MockGraphicsAllocation;

    int peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle) override {
        handle = peekInternalHandleValue;
        return peekInternalHandleRetCode;
    }

    int peekInternalHandleRetCode = 0;
    uint64_t peekInternalHandleValue = 0;
};
class MemoryManagerEventPoolIpcMock : public NEO::MockMemoryManager {
  public:
    MemoryManagerEventPoolIpcMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(executionEnvironment) {}
    void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, AllocationProperties &properties, NEO::MultiGraphicsAllocation &multiGraphicsAllocation) override {
        alloc = new EventPoolIpcMockGraphicsAllocation(&buffer, sizeof(buffer));
        alloc->setShareableHostMemory(true);
        multiGraphicsAllocation.addAllocation(alloc);
        return reinterpret_cast<void *>(alloc->getUnderlyingBuffer());
    }
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        if (callParentCreateGraphicsAllocationFromSharedHandle) {
            return NEO::MockMemoryManager::createGraphicsAllocationFromSharedHandle(osHandleData, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation, mapPointer);
        }
        alloc = new EventPoolIpcMockGraphicsAllocation(&buffer, sizeof(buffer));
        alloc->setShareableHostMemory(true);
        return alloc;
    }
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        if (callParentAllocateGraphicsMemoryWithProperties) {
            return NEO::MockMemoryManager::allocateGraphicsMemoryWithProperties(properties);
        }
        alloc = new EventPoolIpcMockGraphicsAllocation(&buffer, sizeof(buffer));
        alloc->setShareableHostMemory(true);
        return alloc;
    }
    char buffer[256];
    EventPoolIpcMockGraphicsAllocation *alloc = nullptr;
    bool callParentCreateGraphicsAllocationFromSharedHandle = true;
    bool callParentAllocateGraphicsMemoryWithProperties = true;
};

using EventPoolIPCHandleTests = Test<DeviceFixture>;

TEST_F(EventPoolIPCHandleTests, whenGettingIpcHandleForEventPoolThenHandleAndNumberOfEventsAreReturnedInHandle) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle);
    EXPECT_EQ(numEvents, ipcHandleData.numEvents);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, whenGettingIpcHandleForEventPoolThenHandleAndIsHostVisibleAreReturnedInHandle) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle);

    EXPECT_TRUE(ipcHandleData.isHostVisibleEventPoolAllocation);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, whenGettingIpcHandleForEventPoolThenIsImplicitScalingCapableReturnedInHandle) {
    uint32_t numEvents = 2;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle);

    EXPECT_EQ(ipcHandleData.numEvents, 2u);
    EXPECT_EQ(ipcHandleData.numDevices, 1u);
    EXPECT_EQ(ipcHandleData.isImplicitScalingCapable, device->isImplicitScalingCapable());
    EXPECT_EQ(ipcHandleData.isImplicitScalingCapable, eventPool->isImplicitScalingCapableFlagSet());

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, whenGettingIpcHandleForEventPoolThenHandleAndNumDevicesReturnedInHandle) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle);

    EXPECT_EQ(ipcHandleData.numDevices, 1u);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, whenGettingIpcHandleForEventPoolWithDeviceAllocThenHandleAndDeviceAllocAreReturnedInHandle) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
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

    EXPECT_TRUE(ipcHandleData.isDeviceEventPoolAllocation);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

using EventPoolCreateMultiDevice = Test<MultiDeviceFixture>;

TEST_F(EventPoolCreateMultiDevice, whenGettingIpcHandleForEventPoolWhenHostShareableMemoryIsFalseThenUnsuportedIsReturned) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    uint32_t deviceCount = 0;
    ze_result_t result = zeDeviceGet(driverHandle.get(), &deviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(deviceCount, numRootDevices);

    ze_device_handle_t *devices = new ze_device_handle_t[deviceCount];
    result = zeDeviceGet(driverHandle.get(), &deviceCount, devices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto eventPool = EventPool::create(driverHandle.get(), context, deviceCount, devices, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete[] devices;
}

TEST_F(EventPoolIPCHandleTests, whenOpeningIpcHandleForEventPoolThenEventPoolIsCreatedAndEventSizesAreTheSame) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
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

    auto ipcEventPool = L0::EventPool::fromHandle(ipcEventPoolHandle);

    EXPECT_EQ(ipcEventPool->getEventSize(), eventPool->getEventSize());
    EXPECT_EQ(numEvents, static_cast<uint32_t>(ipcEventPool->getNumEvents()));
    EXPECT_TRUE(ipcEventPool->isEventPoolTimestampFlagSet());

    res = ipcEventPool->closeIpcHandle();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, whenOpeningIpcHandleForEventPoolWithHostVisibleThenEventPoolIsCreatedAndIsHostVisibleIsSet) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
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

    EXPECT_EQ(ipcEventPool->isHostVisibleEventPoolAllocation, eventPool->isHostVisibleEventPoolAllocation);
    EXPECT_TRUE(ipcEventPool->isHostVisibleEventPoolAllocation);
    EXPECT_TRUE(ipcEventPool->isEventPoolKernelMappedTsFlagSet());

    res = ipcEventPool->closeIpcHandle();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests,
       GivenRemoteEventPoolHasTwoEventPacketsWhenContextWithSinglePacketOpensIpcEventPoolFromIpcHandleThenDiffrentMaxEventPacketsCauseInvalidArgumentError) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    uint32_t numEvents = 1;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    eventPool->eventPackets = 2;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    ::testing::internal::CaptureStderr();

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    std::string output = testing::internal::GetCapturedStderr();
    std::string expectedOutput("IPC handle max event packets 2 does not match context devices max event packet 1\n");
    EXPECT_EQ(expectedOutput, output);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, whenOpeningIpcHandleForEventPoolWithDeviceAllocThenEventPoolIsCreatedAsDeviceBufferAndDeviceAllocIsSet) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
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

    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    EXPECT_TRUE(ipcHandleData.isDeviceEventPoolAllocation);

    auto ipcEventPool = whiteboxCast(L0::EventPool::fromHandle(ipcEventPoolHandle));

    EXPECT_TRUE(ipcEventPool->isDeviceEventPoolAllocation);

    auto allocation = &ipcEventPool->getAllocation();

    EXPECT_EQ(allocation->getAllocationType(), NEO::AllocationType::gpuTimestampDeviceBuffer);

    EXPECT_EQ(ipcEventPool->getEventSize(), eventPool->getEventSize());
    EXPECT_EQ(numEvents, static_cast<uint32_t>(ipcEventPool->getNumEvents()));

    res = ipcEventPool->closeIpcHandle();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, GivenEventPoolWithIPCEventFlagAndDeviceMemoryThenShareableEventMemoryIsTrue) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC,
        1};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<EventPool> eventPool(whiteboxCast(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    EXPECT_TRUE(eventPool->isShareableEventMemory);
}

TEST_F(EventPoolIPCHandleTests, GivenEventPoolWithIPCEventFlagAndHostMemoryThenShareableEventMemoryIsFalse) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<EventPool> eventPool(whiteboxCast(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    uint32_t minAllocationSize = eventPool->getEventSize();
    EXPECT_GE(allocation->getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex())->getUnderlyingBufferSize(),
              minAllocationSize);
    EXPECT_FALSE(allocation->getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex())->isShareableHostMemory());
}

TEST_F(EventPoolIPCHandleTests, GivenIpcEventPoolWhenCreatingEventFromIpcPoolThenExpectIpcFlag) {
    uint32_t numEvents = 1;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    mockMemoryManager->callParentCreateGraphicsAllocationFromSharedHandle = false;
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);

    auto ipcEventPool = L0::EventPool::fromHandle(ipcEventPoolHandle);

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        0};

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(ipcEventPool, &eventDesc, device));
    ASSERT_NE(nullptr, event);
    EXPECT_TRUE(event->isFromIpcPool);

    res = ipcEventPool->closeIpcHandle();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    uint64_t *data = static_cast<uint64_t *>(event->getCompletionFieldHostAddress());
    for (uint32_t i = 0; i < event->getPacketsInUse(); i++) {
        *data = L0::Event::STATE_CLEARED;
        data = ptrOffset(data, event->getSinglePacketSize());
    }
    event->setIsCompleted();
    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    event->destroy();

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenIpcEventPoolWhenGettingIpcHandleAndFailingToGetThenCorrectErrorIsReturned) {
    uint32_t numEvents = 1;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    mockMemoryManager->callParentAllocateGraphicsMemoryWithProperties = false;
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    auto alloc = static_cast<EventPoolIpcMockGraphicsAllocation *>(eventPool->getAllocation().getGraphicsAllocation(device->getRootDeviceIndex()));
    EXPECT_EQ(mockMemoryManager->alloc, alloc);
    alloc->peekInternalHandleRetCode = -1;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

using EventPoolOpenIPCHandleFailTests = Test<DeviceFixture>;

TEST_F(EventPoolOpenIPCHandleFailTests, givenFailureToAllocateMemoryWhenOpeningIpcHandleForEventPoolThenOutOfDeviceMemoryIsReturned) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto originalMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
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
        EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);

        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(originalMemoryManager);
}

class MultiDeviceEventPoolOpenIpcHandleMemoryManager : public FailMemoryManager {
  public:
    MultiDeviceEventPoolOpenIpcHandleMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : FailMemoryManager(executionEnvironment) {}

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        return &mockAllocation0;
    }

    GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) override {
        NEO::GraphicsAllocation *mock1Alloc = nullptr;
        if (calls < maxCalls) {
            calls++;
            uint32_t rootDeviceIndex = properties.rootDeviceIndex;
            mock1Alloc = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
            if (mock1Alloc == nullptr) {
                mock1Alloc = new NEO::MockGraphicsAllocation(rootDeviceIndex, nullptr, 0);
                mockAllocations1.push_back(std::unique_ptr<NEO::GraphicsAllocation>(mock1Alloc));
            }
        }
        return mock1Alloc;
    }

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation) override {
        if (gfxAllocation == &mockAllocation0) {
            alloc0FreeCall++;
        }

        auto iter = std::find_if(mockAllocations1.begin(), mockAllocations1.end(), [&gfxAllocation](const auto &elem) {
            return elem.get() == gfxAllocation;
        });
        if (iter != mockAllocations1.end()) {
            alloc1FreeCall++;
        }
    }

    NEO::MockGraphicsAllocation mockAllocation0;
    std::vector<std::unique_ptr<NEO::GraphicsAllocation>> mockAllocations1;
    uint32_t calls = 0;
    uint32_t maxCalls = 0;
    uint32_t alloc0FreeCall = 0;
    uint32_t alloc1FreeCall = 0;
};

using MultiDeviceEventPoolOpenIpcHandleFailTests = Test<MultiDeviceFixture>;

TEST_F(MultiDeviceEventPoolOpenIpcHandleFailTests,
       givenFailureToAllocateMemoryWhenOpeningIpcHandleForEventPoolWithMultipleDevicesThenOutOfHostMemoryIsReturned) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = driverHandle->devices[0]->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(driverHandle->devices[0]->getNEODevice());
    auto originalMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 0, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    {
        NEO::MemoryManager *prevMemoryManager = nullptr;
        MultiDeviceEventPoolOpenIpcHandleMemoryManager *currMemoryManager = nullptr;

        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MultiDeviceEventPoolOpenIpcHandleMemoryManager(*neoDevice->executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);
        currMemoryManager->maxCalls = 1;
        ze_event_pool_handle_t ipcEventPoolHandle = {};
        res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
        EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);

        EXPECT_EQ(1u, currMemoryManager->alloc0FreeCall);
        EXPECT_EQ(1u, currMemoryManager->alloc1FreeCall);

        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(originalMemoryManager);
}

TEST_F(EventPoolCreateMultiDevice, GivenContextCreatedWithAllDriverDevicesWhenOpeningIpcEventPoolUsingContextThenExpectAllContextDevicesStoredInIpcEventPool) {
    uint32_t numEvents = 1;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto deviceHandle = driverHandle->devices[0]->toHandle();
    NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(driverHandle->devices[0]->getNEODevice());
    auto originalMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);

    auto eventPool = EventPool::create(driverHandle.get(), context, 0, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto prevMemoryManager = driverHandle->getMemoryManager();
    auto currMemoryManager = new MultiDeviceEventPoolOpenIpcHandleMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(currMemoryManager);
    currMemoryManager->maxCalls = 4;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);
    EXPECT_EQ(3u, currMemoryManager->calls);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    ASSERT_EQ(driverHandle->devices.size(), ipcEventPool->devices.size());
    for (uint32_t i = 0; i < driverHandle->devices.size(); i++) {
        EXPECT_EQ(driverHandle->devices[i], ipcEventPool->devices[i]);
    }

    ipcEventPool->closeIpcHandle();
    EXPECT_EQ(1u, currMemoryManager->alloc0FreeCall);
    EXPECT_EQ(3u, currMemoryManager->alloc1FreeCall);

    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    delete mockMemoryManager;
    driverHandle->setMemoryManager(originalMemoryManager);
}

TEST_F(EventPoolCreateMultiDevice, GivenIPCEventPoolCreatedWithMultiDeviceThenHostVisibleIsSetInEventPoolDuringOpenIPCEventHandle) {
    uint32_t numEvents = 1;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::vector<ze_device_handle_t> devices;
    devices.push_back(driverHandle->devices[0]->toHandle());
    devices.push_back(driverHandle->devices[1]->toHandle());
    NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(driverHandle->devices[0]->getNEODevice());
    auto originalMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);

    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(), context, 2, devices.data(), &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    EXPECT_TRUE(ipcHandleData.isHostVisibleEventPoolAllocation);

    auto prevMemoryManager = driverHandle->getMemoryManager();
    auto currMemoryManager = new MultiDeviceEventPoolOpenIpcHandleMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(currMemoryManager);
    currMemoryManager->maxCalls = 4;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    EXPECT_EQ(ipcEventPool->isHostVisibleEventPoolAllocation, eventPool->isHostVisibleEventPoolAllocation);
    EXPECT_TRUE(ipcEventPool->isHostVisibleEventPoolAllocation);

    ipcEventPool->closeIpcHandle();

    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    delete mockMemoryManager;
    driverHandle->setMemoryManager(originalMemoryManager);
}

TEST_F(EventPoolCreateMultiDevice, GivenContextCreatedWithAllDriverDevicesWhenOpeningIpcEventPoolUsingContextThenExpectNumDevicesSameForEventPoolOpenedAsCreate) {
    uint32_t numEvents = 1;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::vector<ze_device_handle_t> deviceHandles;
    deviceHandles.push_back(driverHandle->devices[0]->toHandle());
    deviceHandles.push_back(driverHandle->devices[1]->toHandle());
    deviceHandles.push_back(driverHandle->devices[2]->toHandle());
    deviceHandles.push_back(driverHandle->devices[3]->toHandle());
    NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(driverHandle->devices[0]->getNEODevice());
    auto originalMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);

    auto eventPool = EventPool::create(driverHandle.get(), context, 4, deviceHandles.data(), &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    EXPECT_EQ(ipcHandleData.numDevices, 4u);

    auto prevMemoryManager = driverHandle->getMemoryManager();
    auto currMemoryManager = new MultiDeviceEventPoolOpenIpcHandleMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(currMemoryManager);
    currMemoryManager->maxCalls = 4;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);
    EXPECT_EQ(3u, currMemoryManager->calls);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    for (uint32_t i = 0; i < 4u; i++) {
        EXPECT_EQ(driverHandle->devices[i], ipcEventPool->devices[i]);
    }
    EXPECT_EQ(ipcHandleData.numDevices, ipcEventPool->devices.size());

    ipcEventPool->closeIpcHandle();
    EXPECT_EQ(1u, currMemoryManager->alloc0FreeCall);
    EXPECT_EQ(3u, currMemoryManager->alloc1FreeCall);

    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    delete mockMemoryManager;
    driverHandle->setMemoryManager(originalMemoryManager);
}

TEST_F(EventPoolCreateMultiDevice, GivenContextCreatedWithSingleDeviceWhenOpeningIpcEventPoolUsingContextThenExpectOnlyThatDeviceHandleToBeUsed) {
    uint32_t numEvents = 1;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::vector<ze_device_handle_t> deviceHandles;
    deviceHandles.push_back(driverHandle->devices[2]->toHandle());
    NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(driverHandle->devices[0]->getNEODevice());
    auto originalMemoryManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *mockMemoryManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(mockMemoryManager);

    auto eventPool = EventPool::create(driverHandle.get(), context, 1, deviceHandles.data(), &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle.data);
    EXPECT_EQ(ipcHandleData.numDevices, 1u);

    auto prevMemoryManager = driverHandle->getMemoryManager();
    auto currMemoryManager = new MultiDeviceEventPoolOpenIpcHandleMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(currMemoryManager);
    currMemoryManager->maxCalls = 0u;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);
    EXPECT_EQ(0u, currMemoryManager->calls);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    EXPECT_EQ(ipcHandleData.numDevices, ipcEventPool->devices.size());
    EXPECT_EQ(deviceHandles[0], ipcEventPool->devices[0]->toHandle());

    ipcEventPool->closeIpcHandle();
    EXPECT_EQ(1u, currMemoryManager->alloc0FreeCall);
    EXPECT_EQ(0u, currMemoryManager->alloc1FreeCall);

    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    delete mockMemoryManager;
    driverHandle->setMemoryManager(originalMemoryManager);
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

    EXPECT_EQ(NEO::AllocationType::gpuTimestampDeviceBuffer, eventPool->getAllocation().getAllocationType());
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

    std::unique_ptr<Event> event(static_cast<Event *>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device)));
    ASSERT_NE(nullptr, event);
    ASSERT_NE(nullptr, event->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, event->csrs[0]);
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

    auto &hwInfo = device->getHwInfo();
    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    auto event = std::unique_ptr<L0::Event>(l0GfxCoreHelper.createEvent(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event);

    if (eventPool->isImplicitScalingCapableFlagSet()) {
        EXPECT_TRUE(event->isUsingContextEndOffset());
    } else {
        EXPECT_FALSE(event->isUsingContextEndOffset());
    }

    uint32_t *eventCompletionMemory = reinterpret_cast<uint32_t *>(event->getCompletionFieldHostAddress());
    uint32_t maxPacketsCount = EventPacketsCount::maxKernelSplit * NEO::TimestampPacketConstants::preferredPacketCount;
    if (l0GfxCoreHelper.useDynamicEventPacketsCount(hwInfo)) {
        maxPacketsCount = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());
    }

    for (uint32_t i = 0; i < maxPacketsCount; i++) {
        EXPECT_EQ(Event::STATE_INITIAL, *eventCompletionMemory);
        eventCompletionMemory = ptrOffset(eventCompletionMemory, event->getSinglePacketSize());
    }

    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    uint64_t gpuAddr = event->getGpuAddress(device);
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    event->hostSignal(false);
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

TEST_F(EventCreate, givenEventWhenCallingGetEventPoolThenCorrectEventPoolIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_event_handle_t hEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &hEvent));
    ASSERT_NE(nullptr, hEvent);

    ze_event_pool_handle_t hEventPool;
    auto event = Event::fromHandle(hEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->getEventPool(&hEventPool));
    EXPECT_EQ(eventPool->toHandle(), hEventPool);

    event->destroy();
}

TEST_F(EventCreate, givenEventWhenCallingGetSignalScopelThenCorrectScopeIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    std::vector<ze_event_scope_flags_t> testingFlags = {
        0,
        ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
        ZE_EVENT_SCOPE_FLAG_DEVICE,
        ZE_EVENT_SCOPE_FLAG_HOST};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    for (auto &flags : testingFlags) {
        eventDesc.signal = flags;

        ze_event_handle_t hEvent = nullptr;
        ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &hEvent));
        ASSERT_NE(nullptr, hEvent);

        ze_event_scope_flags_t signalScopeFlags = ZE_EVENT_SCOPE_FLAG_FORCE_UINT32;
        auto event = Event::fromHandle(hEvent);
        EXPECT_EQ(ZE_RESULT_SUCCESS, event->getSignalScope(&signalScopeFlags));
        EXPECT_EQ(eventDesc.signal, signalScopeFlags);

        event->destroy();
    }
}

TEST_F(EventCreate, givenEventWhenCallingGetWaitScopelThenCorrectScopeIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    std::vector<ze_event_scope_flags_t> testingFlags = {
        0,
        ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
        ZE_EVENT_SCOPE_FLAG_DEVICE,
        ZE_EVENT_SCOPE_FLAG_HOST};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    for (auto &flags : testingFlags) {
        eventDesc.wait = flags;

        ze_event_handle_t hEvent = nullptr;
        ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &hEvent));
        ASSERT_NE(nullptr, hEvent);

        ze_event_scope_flags_t waitScopeFlags = ZE_EVENT_SCOPE_FLAG_FORCE_UINT32;
        auto event = Event::fromHandle(hEvent);
        EXPECT_EQ(ZE_RESULT_SUCCESS, event->getWaitScope(&waitScopeFlags));
        EXPECT_EQ(eventDesc.wait, waitScopeFlags);

        event->destroy();
    }
}

HWTEST2_F(EventCreate, givenPlatformSupportMultTileWhenDebugKeyIsSetToNotUseContextEndThenDoNotUseContextEndOffset, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseContextEndOffsetForEventCompletion.set(0);

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        0,
        1};
    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        0};

    ze_event_handle_t eventHandle = nullptr;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_result_t value = eventPool->createEvent(&eventDesc, &eventHandle);
    ASSERT_NE(nullptr, eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, value);

    auto event = Event::fromHandle(eventHandle);
    EXPECT_FALSE(event->isEventTimestampFlagSet());
    EXPECT_FALSE(event->isUsingContextEndOffset());

    event->destroy();
}

HWTEST2_F(EventCreate, givenPlatformNotSupportsMultTileWhenDebugKeyIsSetToUseContextEndThenUseContextEndOffset, IsNotXeHpcCore) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseContextEndOffsetForEventCompletion.set(1);

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        0,
        1};
    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        0};

    ze_event_handle_t eventHandle = nullptr;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_result_t value = eventPool->createEvent(&eventDesc, &eventHandle);
    ASSERT_NE(nullptr, eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, value);

    auto event = Event::fromHandle(eventHandle);
    EXPECT_FALSE(event->isEventTimestampFlagSet());
    EXPECT_TRUE(event->isUsingContextEndOffset());

    event->destroy();
}

template <typename GfxFamily>
struct MockL0GfxCoreHelperAlwaysAllocateEventInLocalMemHw : L0::L0GfxCoreHelperHw<GfxFamily> {
    bool alwaysAllocateEventInLocalMem() const override { return true; }
};

HWTEST_F(EventCreate, GivenEnabledTimestampPoolAllocatorAndForcedEventAllocateInLocalMemoryWhenCreatingMultipleEventPoolsForSingleDeviceThenEventsUseSharedAllocationAndHaveUniqueAddresses) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableTimestampPoolAllocator.set(1);

    MockL0GfxCoreHelperAlwaysAllocateEventInLocalMemHw<FamilyType> mockL0GfxCoreHelper{};
    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelper));
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);

    ASSERT_TRUE(device->getNEODevice()->getDeviceTimestampPoolAllocator().isEnabled());

    ze_device_handle_t devices[] = {device->toHandle()};

    std::vector<std::unique_ptr<L0::EventPool>> eventPools;
    std::vector<std::unique_ptr<Event>> events;
    std::set<uint64_t> gpuAddresses;

    constexpr size_t numEventPools = 5;
    constexpr size_t numEventsInPool = 2;
    constexpr size_t numEvents = numEventPools * numEventsInPool;

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        numEventsInPool};

    ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    ze_result_t result = ZE_RESULT_SUCCESS;

    for (size_t i = 0; i < numEventPools; i++) {
        eventPools.emplace_back(EventPool::create(driverHandle.get(), context, 1, devices, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, eventPools[i]);

        for (size_t j = 0; j < numEventsInPool; j++) {
            eventDesc.index = static_cast<uint32_t>(j);
            events.emplace_back(static_cast<Event *>(getHelper<L0GfxCoreHelper>().createEvent(eventPools[i].get(), &eventDesc, device)));
            EXPECT_NE(nullptr, events.back());
        }
    }

    const auto expectedSharedAllocation = events[0]->getAllocation(device);
    EXPECT_TRUE(device->getNEODevice()->getDeviceTimestampPoolAllocator().isPoolBuffer(expectedSharedAllocation));

    for (auto &event : events) {
        EXPECT_EQ(expectedSharedAllocation, event->getAllocation(device));

        uint64_t gpuAddress = event->getGpuAddress(device);
        auto [iterator, wasInserted] = gpuAddresses.insert(gpuAddress);
        EXPECT_TRUE(wasInserted) << "Duplicate GPU address found: " << std::hex << "0x" << gpuAddress;
    }

    EXPECT_EQ(numEvents, gpuAddresses.size());

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);
    l0GfxCoreHelperBackup.release();
}

HWTEST_F(EventPoolCreateMultiDevice, GivenEnabledTimestampPoolAllocatorAndForcedLocalMemoryWhenCreatingEventPoolsForTwoDevicesThenEventsShareAllocationWithinDeviceButNotBetweenDevices) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableTimestampPoolAllocator.set(1);

    MockL0GfxCoreHelperAlwaysAllocateEventInLocalMemHw<FamilyType> mockL0GfxCoreHelper0{};
    MockL0GfxCoreHelperAlwaysAllocateEventInLocalMemHw<FamilyType> mockL0GfxCoreHelper1{};

    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup0(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelper0));
    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup1(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelper1));

    ASSERT_GE(driverHandle->devices.size(), 2u);

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    auto neoDevice0 = device0->getNEODevice();
    auto neoDevice1 = device1->getNEODevice();

    ASSERT_TRUE(neoDevice0->getDeviceTimestampPoolAllocator().isEnabled());
    ASSERT_TRUE(neoDevice1->getDeviceTimestampPoolAllocator().isEnabled());

    neoDevice0->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup0);
    neoDevice1->getExecutionEnvironment()->rootDeviceEnvironments[1]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup1);

    std::vector<std::unique_ptr<L0::EventPool>> eventPoolsDevice0;
    std::vector<std::unique_ptr<L0::EventPool>> eventPoolsDevice1;
    std::vector<std::unique_ptr<Event>> eventsDevice0;
    std::vector<std::unique_ptr<Event>> eventsDevice1;
    std::set<uint64_t> gpuAddressesDevice0;
    std::set<uint64_t> gpuAddressesDevice1;

    constexpr size_t numEventPools = 3;
    constexpr size_t numEventsInPool = 2;
    constexpr size_t numEvents = numEventPools * numEventsInPool;

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        numEventsInPool};

    ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto &l0GfxCoreHelper = neoDevice0->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    // Create events for device0
    ze_device_handle_t devices0[] = {device0->toHandle()};
    for (size_t i = 0; i < numEventPools; i++) {
        eventPoolsDevice0.emplace_back(EventPool::create(driverHandle.get(), context, 1, devices0, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, eventPoolsDevice0[i]);

        for (size_t j = 0; j < numEventsInPool; j++) {
            eventDesc.index = static_cast<uint32_t>(j);
            eventsDevice0.emplace_back(static_cast<Event *>(l0GfxCoreHelper.createEvent(eventPoolsDevice0[i].get(), &eventDesc, device0)));
            EXPECT_NE(nullptr, eventsDevice0.back());
        }
    }

    // Create events for device1
    ze_device_handle_t devices1[] = {device1->toHandle()};
    for (size_t i = 0; i < numEventPools; i++) {
        eventPoolsDevice1.emplace_back(EventPool::create(driverHandle.get(), context, 1, devices1, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, eventPoolsDevice1[i]);

        for (size_t j = 0; j < numEventsInPool; j++) {
            eventDesc.index = static_cast<uint32_t>(j);
            eventsDevice1.emplace_back(static_cast<Event *>(l0GfxCoreHelper.createEvent(eventPoolsDevice1[i].get(), &eventDesc, device1)));
            EXPECT_NE(nullptr, eventsDevice1.back());
        }
    }

    // Verify allocations and GPU addresses for device0
    const auto expectedSharedAllocationDevice0 = eventsDevice0[0]->getAllocation(device0);
    EXPECT_TRUE(neoDevice0->getDeviceTimestampPoolAllocator().isPoolBuffer(expectedSharedAllocationDevice0));
    EXPECT_FALSE(neoDevice1->getDeviceTimestampPoolAllocator().isPoolBuffer(expectedSharedAllocationDevice0));

    for (auto &event : eventsDevice0) {
        EXPECT_EQ(expectedSharedAllocationDevice0, event->getAllocation(device0));

        uint64_t gpuAddress = event->getGpuAddress(device0);
        auto [iterator, wasInserted] = gpuAddressesDevice0.insert(gpuAddress);
        EXPECT_TRUE(wasInserted) << "Duplicate GPU address found for device0: " << std::hex << "0x" << gpuAddress;
    }

    // Verify allocations and GPU addresses for device1
    const auto expectedSharedAllocationDevice1 = eventsDevice1[0]->getAllocation(device1);
    EXPECT_TRUE(neoDevice1->getDeviceTimestampPoolAllocator().isPoolBuffer(expectedSharedAllocationDevice1));
    EXPECT_FALSE(neoDevice0->getDeviceTimestampPoolAllocator().isPoolBuffer(expectedSharedAllocationDevice1));

    for (auto &event : eventsDevice1) {
        EXPECT_EQ(expectedSharedAllocationDevice1, event->getAllocation(device1));

        uint64_t gpuAddress = event->getGpuAddress(device1);
        auto [iterator, wasInserted] = gpuAddressesDevice1.insert(gpuAddress);
        EXPECT_TRUE(wasInserted) << "Duplicate GPU address found for device1: " << std::hex << "0x" << gpuAddress;
    }

    EXPECT_NE(expectedSharedAllocationDevice0, expectedSharedAllocationDevice1);
    EXPECT_EQ(numEvents, gpuAddressesDevice0.size());
    EXPECT_EQ(numEvents, gpuAddressesDevice1.size());

    neoDevice0->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup0);
    neoDevice1->getExecutionEnvironment()->rootDeviceEnvironments[1]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup1);
    l0GfxCoreHelperBackup0.release();
    l0GfxCoreHelperBackup1.release();
}

using EventSynchronizeTest = Test<EventFixture<1, 0>>;
using EventUsedPacketSignalSynchronizeTest = Test<EventUsedPacketSignalFixture<1, 0, 0, -1>>;

TEST_F(EventSynchronizeTest, GivenGpuHangWhenHostSynchronizeIsCalledThenDeviceLostIsReturned) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = true;

    event->csrs[0] = csr.get();
    event->gpuHangCheckPeriod = 0ms;

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    auto result = event->hostSynchronize(timeout);

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(EventSynchronizeTest, GivenHangHappenedBeforePeriodicHangCheckAndForceGpuStatusCheckDuringHostSynchronizeThenHangIsDetected) {
    NEO::debugManager.flags.ForceGpuStatusCheckOnSuccessfulEventHostSynchronize.set(1);

    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = true;

    event->csrs[0] = csr.get();
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;

    auto result = event->hostSynchronize(0);

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(EventSynchronizeTest, GivenEventCompletedAndForceGpuStatusCheckThenHostSynchronizeReturnsSuccess) {
    NEO::debugManager.flags.ForceGpuStatusCheckOnSuccessfulEventHostSynchronize.set(1);

    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    event->csrs[0] = csr.get();
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;

    auto result = event->hostSynchronize(0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventSynchronizeTest, GivenNoGpuHangAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenResultNotReadyIsReturnedDueToTimeout) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = false;

    event->csrs[0] = csr.get();
    event->gpuHangCheckPeriod = 0ms;

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = event->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, GivenLongPeriodOfGpuCheckAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenResultNotReadyIsReturnedDueToTimeout) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    event->csrs[0] = csr.get();
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

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutNonZeroAndOverrideTimeoutSetAndStateInitialThenHostSynchronizeReturnsNotReady) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideEventSynchronizeTimeout.set(100);
    std::chrono::high_resolution_clock::time_point waitStartTime, currentTime;
    waitStartTime = std::chrono::high_resolution_clock::now();
    ze_result_t result = event->hostSynchronize(0);
    currentTime = std::chrono::high_resolution_clock::now();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    uint64_t timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - waitStartTime).count();
    uint64_t minTimeoutCheck = 80;
    EXPECT_GT(timeDiff, minTimeoutCheck);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutZeroWhenStateSignaledThenHostSynchronizeReturnsSuccess) {
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;

    event->setUsingContextEndOffset(false);
    ze_result_t result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutNonZeroWhenStateSignaledThenHostSynchronizeReturnsSuccess) {
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;

    event->setUsingContextEndOffset(false);
    ze_result_t result = event->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutZeroWhenOffsetEventStateSignaledThenHostSynchronizeReturnsSuccess) {
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    hostAddr = ptrOffset(hostAddr, event->getContextEndOffset());
    *hostAddr = Event::STATE_SIGNALED;

    event->setUsingContextEndOffset(true);
    ze_result_t result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventUsedPacketSignalSynchronizeTest, givenInfiniteTimeoutWhenWaitingForNonTimestampEventCompletionThenReturnOnlyAfterAllEventPacketsAreCompleted) {
    constexpr uint32_t packetsInUse = 2;
    event->setPacketsInUse(packetsInUse);
    event->setUsingContextEndOffset(false);

    const size_t eventPacketSize = event->getSinglePacketSize();
    const size_t eventCompletionOffset = event->getContextStartOffset();

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));

    uint64_t *hostAddr = static_cast<uint64_t *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    for (uint32_t i = 0; i < packetsInUse; i++) {
        *hostAddr = Event::STATE_CLEARED;
        hostAddr = ptrOffset(hostAddr, eventPacketSize);
    }

    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (CpuIntrinsicsTests::pauseCounter > 10) {
            volatile TagAddressType *nextPacket = CpuIntrinsicsTests::pauseAddress;
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

TEST_F(EventUsedPacketSignalSynchronizeTest, givenInfiniteTimeoutWhenWaitingForOffsetedNonTimestampEventCompletionThenReturnOnlyAfterAllEventPacketsAreCompleted) {
    constexpr uint32_t packetsInUse = 2;
    event->setPacketsInUse(packetsInUse);
    event->setUsingContextEndOffset(true);

    const size_t eventPacketSize = event->getSinglePacketSize();
    const size_t eventCompletionOffset = event->getContextEndOffset();

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));

    uint64_t *hostAddr = static_cast<uint64_t *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    for (uint32_t i = 0; i < packetsInUse; i++) {
        *hostAddr = Event::STATE_CLEARED;
        hostAddr = ptrOffset(hostAddr, eventPacketSize);
    }

    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (CpuIntrinsicsTests::pauseCounter > 10) {
            volatile TagAddressType *nextPacket = CpuIntrinsicsTests::pauseAddress;
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

TEST_F(EventUsedPacketSignalSynchronizeTest, givenInfiniteTimeoutWhenWaitingForTimestampEventCompletionThenReturnOnlyAfterAllEventPacketsAreCompleted) {
    constexpr uint32_t packetsInUse = 2;
    event->setPacketsInUse(packetsInUse);
    event->setEventTimestampFlag(true);

    const size_t eventPacketSize = event->getSinglePacketSize();
    const size_t eventCompletionOffset = event->getContextEndOffset();

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));

    uint64_t *hostAddr = static_cast<uint64_t *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    for (uint32_t i = 0; i < packetsInUse; i++) {
        *hostAddr = Event::STATE_CLEARED;
        hostAddr = ptrOffset(hostAddr, eventPacketSize);
    }

    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (CpuIntrinsicsTests::pauseCounter > 10) {
            volatile TagAddressType *nextPacket = CpuIntrinsicsTests::pauseAddress;
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
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<EventPool> eventPool = std::unique_ptr<EventPool>(whiteboxCast(L0::EventPool::create(driverHandle.get(),
                                                                                                         context,
                                                                                                         0,
                                                                                                         nullptr,
                                                                                                         &eventPoolDesc,
                                                                                                         result)));
    EXPECT_NE(nullptr, eventPool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::unique_ptr<L0::Event> event0;
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    event0 = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(),
                                                                                 &eventDesc,
                                                                                 device));
    EXPECT_NE(nullptr, event0);

    uint32_t *hostAddr = static_cast<uint32_t *>(event0->getCompletionFieldHostAddress());
    EXPECT_EQ(*hostAddr, Event::STATE_INITIAL);

    // change state
    event0->hostSignal(false);
    EXPECT_EQ(*hostAddr, Event::STATE_SIGNALED);

    // create an event from the pool with the same index as event0, but this time, since isImportedIpcPool is true, no reset should happen
    eventPool->isImportedIpcPool = true;
    std::unique_ptr<L0::Event> event1;
    event1 = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(),
                                                                                 &eventDesc,
                                                                                 device));
    EXPECT_NE(nullptr, event1);

    uint32_t *hostAddr1 = static_cast<uint32_t *>(event1->getCompletionFieldHostAddress());
    EXPECT_EQ(*hostAddr1, Event::STATE_SIGNALED);

    // create another event from the pool with the same index, but this time, since isImportedIpcPool is false, reset should happen
    eventPool->isImportedIpcPool = false;
    std::unique_ptr<L0::Event> event2;
    event2 = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(),
                                                                                 &eventDesc,
                                                                                 device));
    EXPECT_NE(nullptr, event2);

    uint32_t *hostAddr2 = static_cast<uint32_t *>(event2->getCompletionFieldHostAddress());
    EXPECT_EQ(*hostAddr2, Event::STATE_INITIAL);
}

using EventAubCsrTest = Test<DeviceFixture>;

HWTEST_F(EventAubCsrTest, givenCallToEventHostSynchronizeWithAubModeCsrReturnsSuccess) {
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;

    EnvironmentWithCsrWrapper environment;
    environment.setCsrType<MockCsrAub<FamilyType>>();

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

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
    event = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
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

struct TimestampEventCreateMultiKernelFixture : public EventFixture<1, 1> {
    void setUp() {
        debugManager.flags.UsePipeControlMultiKernelEventSync.set(0);
        debugManager.flags.SignalAllEventPackets.set(0);
        EventFixture<1, 1>::setUp();
    }

    DebugManagerStateRestore restorer;
};

using TimestampEventCreate = Test<EventFixture<1, 1>>;
using TimestampEventCreateMultiKernel = Test<TimestampEventCreateMultiKernelFixture>;
using TimestampEventUsedPacketSignalCreate = Test<EventUsedPacketSignalFixture<1, 1, 0, -1>>;

TEST_F(TimestampEventCreate, givenEventCreatedWithTimestampThenIsTimestampEventFlagSet) {
    EXPECT_TRUE(event->isEventTimestampFlagSet());
}

TEST_F(TimestampEventCreate, givenEventTimestampsCreatedWhenResetIsInvokeThenCorrectDataAreSet) {
    auto &hwInfo = device->getHwInfo();
    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t maxKernelCount = EventPacketsCount::maxKernelSplit;
    if (l0GfxCoreHelper.useDynamicEventPacketsCount(hwInfo)) {
        maxKernelCount = l0GfxCoreHelper.getEventMaxKernelCount(hwInfo);
    }

    EXPECT_NE(nullptr, event->kernelEventCompletionData);
    for (auto j = 0u; j < maxKernelCount; j++) {
        for (auto i = 0u; i < NEO::TimestampPacketConstants::preferredPacketCount; i++) {
            EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData[j].getContextStartValue(i));
            EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData[j].getGlobalStartValue(i));
            EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData[j].getContextEndValue(i));
            EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData[j].getGlobalEndValue(i));
        }
        EXPECT_EQ(1u, event->kernelEventCompletionData[j].getPacketsUsed());
    }

    EXPECT_EQ(1u, event->getKernelCount());
}

TEST_F(TimestampEventCreate, givenSingleTimestampEventThenAllocationSizeCreatedForAllTimestamps) {
    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    uint32_t minTimestampEventAllocation = eventPool->getEventSize();
    EXPECT_GE(allocation->getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex())->getUnderlyingBufferSize(),
              minTimestampEventAllocation);
}

TEST_F(TimestampEventCreate, givenTimestampEventThenAllocationsIsDependentIfAllocationOnLocalMemAllowed) {
    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    if (l0GfxCoreHelper.alwaysAllocateEventInLocalMem()) {
        EXPECT_EQ(NEO::AllocationType::gpuTimestampDeviceBuffer, allocation->getAllocationType());
    } else {
        EXPECT_EQ(NEO::AllocationType::timestampPacketTagBuffer, allocation->getAllocationType());
    }
}

HWTEST2_F(TimestampEventCreateMultiKernel, givenEventTimestampWhenPacketCountIsSetThenCorrectOffsetIsReturned, IsAtLeastXeCore) {
    EXPECT_EQ(1u, event->getPacketsInUse());
    auto gpuAddr = event->getGpuAddress(device);
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    event->setPacketsInUse(4u);
    EXPECT_EQ(4u, event->getPacketsInUse());

    gpuAddr += (4u * event->getSinglePacketSize());

    event->increaseKernelCount();
    event->setPacketsInUse(2u);
    EXPECT_EQ(6u, event->getPacketsInUse());
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    gpuAddr += (2u * event->getSinglePacketSize());
    event->increaseKernelCount();
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));
    EXPECT_EQ(7u, event->getPacketsInUse());
}

TEST_F(TimestampEventCreate, givenEventWhenSignaledAndResetFromTheHostThenCorrectDataAreSet) {
    EXPECT_NE(nullptr, event->kernelEventCompletionData);
    event->hostSignal(false);
    ze_result_t result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    event->reset();
    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    for (auto j = 0u; j < event->getKernelCount(); j++) {
        for (auto i = 0u; i < NEO::TimestampPacketConstants::preferredPacketCount; i++) {
            EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData[j].getContextStartValue(i));
            EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData[j].getGlobalStartValue(i));
            EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData[j].getContextEndValue(i));
            EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData[j].getGlobalEndValue(i));
        }
        EXPECT_EQ(1u, event->kernelEventCompletionData[j].getPacketsUsed());
    }
    EXPECT_EQ(1u, event->getKernelCount());
}

TEST_F(TimestampEventCreate, givenpCountZeroCallingQueryTimestampExpThenpCountSetProperly) {
    uint32_t pCount = 0;
    auto result = event->queryTimestampsExp(device, &pCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, pCount);
}

TEST_F(TimestampEventUsedPacketSignalCreate, givenpCountLargerThanSupportedWhenCallingQueryTimestampExpThenpCountSetProperly) {
    uint32_t pCount = 10;
    event->setPacketsInUse(2u);
    auto result = event->queryTimestampsExp(device, &pCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, pCount);
}

TEST_F(TimestampEventCreate, givenEventWithStaticPartitionOffThenQueryTimestampExpReturnsUnsupported) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableStaticPartitioning.set(0);

    uint32_t pCount = 0;
    auto result = event->queryTimestampsExp(device, &pCount, nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

using TimestampDeviceEventCreate = Test<EventFixture<0, 1>>;

TEST_F(TimestampDeviceEventCreate, givenTimestampDeviceEventThenAllocationsIsOfGpuDeviceTimestampType) {
    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(NEO::AllocationType::gpuTimestampDeviceBuffer, allocation->getAllocationType());
}

using EventQueryTimestampExpWithRootDeviceAndSubDevices = Test<MultiDeviceFixture>;

TEST_F(EventQueryTimestampExpWithRootDeviceAndSubDevices, givenEventWhenQuerytimestampExpWithRootDeviceAndSubDevicesThenReturnsCorrectValuesReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.SignalAllEventPackets.set(0);

    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<EventImp<uint32_t>> eventRoot;
    std::unique_ptr<EventImp<uint32_t>> eventSub0;
    std::unique_ptr<EventImp<uint32_t>> eventSub1;
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

    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 1, &rootDeviceHandle, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    eventRoot = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, L0::Device::fromHandle(rootDeviceHandle))));
    ASSERT_NE(nullptr, eventRoot);

    typename MockTimestampPackets32::Packet packetData[2];
    eventRoot->setPacketsInUse(2u);

    packetData[0].contextStart = 1u;
    packetData[0].contextEnd = 2u;
    packetData[0].globalStart = 3u;
    packetData[0].globalEnd = 4u;

    packetData[1].contextStart = 5u;
    packetData[1].contextEnd = 6u;
    packetData[1].globalStart = 7u;
    packetData[1].globalEnd = 8u;

    eventRoot->hostAddressFromPool = packetData;

    ze_kernel_timestamp_result_t results[2];
    uint32_t numPackets = 2;

    for (uint32_t packetId = 0; packetId < numPackets; packetId++) {
        eventRoot->kernelEventCompletionData[0].assignDataToAllTimestamps(packetId, eventRoot->hostAddressFromPool);
        eventRoot->hostAddressFromPool = ptrOffset(eventRoot->hostAddressFromPool, NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
    }

    uint32_t pCount = 0;
    result = eventRoot->queryTimestampsExp(L0::Device::fromHandle(rootDeviceHandle), &pCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, pCount);
    result = eventRoot->queryTimestampsExp(L0::Device::fromHandle(rootDeviceHandle), &pCount, results);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    for (uint32_t i = 0; i < pCount; i++) {
        EXPECT_EQ(packetData[i].contextStart, results[i].context.kernelStart);
        EXPECT_EQ(packetData[i].contextEnd, results[i].context.kernelEnd);
        EXPECT_EQ(packetData[i].globalStart, results[i].global.kernelStart);
        EXPECT_EQ(packetData[i].globalEnd, results[i].global.kernelEnd);
    }

    auto subdevice = L0::Device::fromHandle(subDeviceHandle[0]);
    eventSub0 = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, subdevice)));
    ASSERT_NE(nullptr, eventSub0);

    numPackets = 1;
    eventSub0->setPacketsInUse(1u);
    eventSub0->hostAddressFromPool = packetData;

    for (uint32_t packetId = 0; packetId < numPackets; packetId++) {
        eventSub0->kernelEventCompletionData[0].assignDataToAllTimestamps(packetId, eventSub0->hostAddressFromPool);
        eventSub0->hostAddressFromPool = ptrOffset(eventSub0->hostAddressFromPool, NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
    }

    pCount = 0;
    result = eventSub0->queryTimestampsExp(subdevice, &pCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, pCount);
    result = eventSub0->queryTimestampsExp(subdevice, &pCount, results);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (uint32_t i = 0; i < pCount; i++) {
        EXPECT_EQ(packetData[i].contextStart, results[i].context.kernelStart);
        EXPECT_EQ(packetData[i].contextEnd, results[i].context.kernelEnd);
        EXPECT_EQ(packetData[i].globalStart, results[i].global.kernelStart);
        EXPECT_EQ(packetData[i].globalEnd, results[i].global.kernelEnd);
    }

    subdevice = L0::Device::fromHandle(subDeviceHandle[1]);
    eventSub1 = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, subdevice)));
    ASSERT_NE(nullptr, eventSub1);

    numPackets = 1;
    eventSub1->setPacketsInUse(1u);
    eventSub1->hostAddressFromPool = packetData;

    for (uint32_t packetId = 0; packetId < numPackets; packetId++) {
        eventSub1->kernelEventCompletionData[0].assignDataToAllTimestamps(packetId, eventSub1->hostAddressFromPool);
        eventSub1->hostAddressFromPool = ptrOffset(eventSub1->hostAddressFromPool, NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
    }

    pCount = 0;
    result = eventSub1->queryTimestampsExp(subdevice, &pCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, pCount);
    result = eventSub1->queryTimestampsExp(subdevice, &pCount, results);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (uint32_t i = 0; i < pCount; i++) {
        EXPECT_EQ(packetData[i].contextStart, results[i].context.kernelStart);
        EXPECT_EQ(packetData[i].contextEnd, results[i].context.kernelEnd);
        EXPECT_EQ(packetData[i].globalStart, results[i].global.kernelStart);
        EXPECT_EQ(packetData[i].globalEnd, results[i].global.kernelEnd);
    }
}

using EventqueryKernelTimestampsExt = Test<EventUsedPacketSignalFixture<1, 1, 0, -1>>;

TEST_F(EventqueryKernelTimestampsExt, givenpCountLargerThanSupportedWhenCallingQueryKernelTimestampsExtThenpCountSetProperly) {
    uint32_t pCount = 10;
    event->setPacketsInUse(2u);

    std::vector<ze_kernel_timestamp_result_t> kernelTsBuffer(2);
    ze_event_query_kernel_timestamps_results_ext_properties_t results{};
    results.pKernelTimestampsBuffer = kernelTsBuffer.data();
    results.pSynchronizedTimestampsBuffer = nullptr;
    event->hostSignal(false);

    auto result = event->queryKernelTimestampsExt(device, &pCount, &results);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, pCount);
}

TEST_F(EventqueryKernelTimestampsExt, givenEventWithStaticPartitionOffThenQueryKernelTimestampsExtReturnsUnsupported) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableStaticPartitioning.set(0);

    event->hasKernelMappedTsCapability = true;

    std::vector<ze_kernel_timestamp_result_t> kernelTsBuffer(2);
    ze_event_query_kernel_timestamps_results_ext_properties_t results{};
    results.pKernelTimestampsBuffer = kernelTsBuffer.data();
    results.pSynchronizedTimestampsBuffer = nullptr;
    event->hostSignal(false);

    uint32_t pCount = 10;
    auto result = event->queryKernelTimestampsExt(device, &pCount, &results);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(EventqueryKernelTimestampsExt, givenEventStatusNotReadyThenQueryKernelTimestampsExtReturnsNotReady) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableStaticPartitioning.set(0);

    event->hasKernelMappedTsCapability = true;

    std::vector<ze_kernel_timestamp_result_t> kernelTsBuffer(2);
    ze_event_query_kernel_timestamps_results_ext_properties_t results{};
    results.pKernelTimestampsBuffer = kernelTsBuffer.data();
    results.pSynchronizedTimestampsBuffer = nullptr;

    uint32_t pCount = 10;
    auto result = event->queryKernelTimestampsExt(device, &pCount, &results);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventqueryKernelTimestampsExt, givenEventWithMappedTimestampCapabilityWhenQueryKernelTimestampsExtIsCalledCorrectValuesAreReturned) {

    struct MappedTimeStampData {
        typename MockTimestampPackets32::Packet packetData[3];
        NEO::TimeStampData referenceTs{};
    } mappedTimeStampData;

    auto &hwInfo = device->getNEODevice()->getHardwareInfo();
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.kernelTimestampValidBits = 32;
    event->setPacketsInUse(3u);
    event->hasKernelMappedTsCapability = true;
    const auto deviceTsFrequency = device->getNEODevice()->getDeviceInfo().profilingTimerResolution;
    const int64_t gpuReferenceTimeInNs = 2000;
    const int64_t cpuReferenceTimeInNs = 3000;
    const auto maxKernelTsValue = maxNBitValue(hwInfo.capabilityTable.kernelTimestampValidBits);

    auto timeToTimeStamp = [&](uint32_t timeInNs) {
        return static_cast<uint32_t>(timeInNs / deviceTsFrequency);
    };

    mappedTimeStampData.packetData[0].contextStart = 50u;
    mappedTimeStampData.packetData[0].contextEnd = 100u;
    mappedTimeStampData.packetData[0].globalStart = timeToTimeStamp(4000u);
    mappedTimeStampData.packetData[0].globalEnd = timeToTimeStamp(5000u);

    // Device Ts overflow case
    mappedTimeStampData.packetData[1].contextStart = 20u;
    mappedTimeStampData.packetData[1].contextEnd = 30u;
    mappedTimeStampData.packetData[1].globalStart = timeToTimeStamp(500u);
    mappedTimeStampData.packetData[1].globalEnd = timeToTimeStamp(1500u);

    mappedTimeStampData.packetData[2].contextStart = 20u;
    mappedTimeStampData.packetData[2].contextEnd = 30u;
    mappedTimeStampData.packetData[2].globalStart = timeToTimeStamp(5000u);
    mappedTimeStampData.packetData[2].globalEnd = timeToTimeStamp(500u);

    event->hostAddressFromPool = &mappedTimeStampData;
    event->maxPacketCount = 3;
    uint32_t count = 0;

    NEO::TimeStampData *referenceTs = event->peekReferenceTs();
    referenceTs->cpuTimeinNS = cpuReferenceTimeInNs;
    referenceTs->gpuTimeStamp = static_cast<uint64_t>(gpuReferenceTimeInNs / deviceTsFrequency);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryKernelTimestampsExt(device, &count, nullptr));
    EXPECT_EQ(count, 3u);

    std::vector<ze_kernel_timestamp_result_t> kernelTsBuffer(count);
    std::vector<ze_synchronized_timestamp_result_ext_t> synchronizedTsBuffer(count);

    ze_event_query_kernel_timestamps_results_ext_properties_t results{};
    results.pKernelTimestampsBuffer = kernelTsBuffer.data();
    results.pSynchronizedTimestampsBuffer = synchronizedTsBuffer.data();

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryKernelTimestampsExt(device, &count, &results));
    uint64_t errorOffset = 5;
    // Packet 1
    auto expectedGlobalStart = (cpuReferenceTimeInNs - gpuReferenceTimeInNs) + 4000u;
    auto expectedGlobalEnd = (cpuReferenceTimeInNs - gpuReferenceTimeInNs) + 5000u;
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[0].global.kernelStart, expectedGlobalStart - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[0].global.kernelStart, expectedGlobalStart + errorOffset);
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[0].global.kernelEnd, expectedGlobalEnd - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[0].global.kernelEnd, expectedGlobalEnd + errorOffset);

    auto expectedContextStart = expectedGlobalStart;
    auto expectedContextEnd = expectedContextStart + (mappedTimeStampData.packetData[0].contextEnd - mappedTimeStampData.packetData[0].contextStart) * deviceTsFrequency;
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[0].context.kernelStart, expectedContextStart - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[0].context.kernelStart, expectedContextStart + errorOffset);
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[0].context.kernelEnd, expectedContextEnd - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[0].context.kernelEnd, expectedContextEnd + errorOffset);

    // Packet 2
    expectedGlobalStart = (cpuReferenceTimeInNs - gpuReferenceTimeInNs) + 500u +
                          static_cast<uint64_t>(maxNBitValue(hwInfo.capabilityTable.kernelTimestampValidBits) * deviceTsFrequency);
    expectedGlobalEnd = expectedGlobalStart + (1500 - 500);
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[1].global.kernelStart, expectedGlobalStart - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[1].global.kernelStart, expectedGlobalStart + errorOffset);
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[1].global.kernelEnd, expectedGlobalEnd - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[1].global.kernelEnd, expectedGlobalEnd + errorOffset);

    expectedContextStart = expectedGlobalStart;
    expectedContextEnd = expectedContextStart + (mappedTimeStampData.packetData[1].contextEnd - mappedTimeStampData.packetData[1].contextStart) * deviceTsFrequency;
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[1].context.kernelStart, expectedContextStart - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[1].context.kernelStart, expectedContextStart + errorOffset);
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[1].context.kernelEnd, expectedContextEnd - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[1].context.kernelEnd, expectedContextEnd + errorOffset);

    // Packet 3
    expectedGlobalStart = (cpuReferenceTimeInNs - gpuReferenceTimeInNs) + 5000u;
    expectedGlobalEnd = expectedGlobalStart + (static_cast<uint64_t>(maxKernelTsValue * deviceTsFrequency) - 5000u + 500u);
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[2].global.kernelStart, expectedGlobalStart - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[2].global.kernelStart, expectedGlobalStart + errorOffset);
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[2].global.kernelEnd, expectedGlobalEnd - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[2].global.kernelEnd, expectedGlobalEnd + errorOffset);

    expectedContextStart = expectedGlobalStart;
    expectedContextEnd = expectedContextStart + (mappedTimeStampData.packetData[2].contextEnd - mappedTimeStampData.packetData[1].contextStart) * deviceTsFrequency;
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[2].context.kernelStart, expectedContextStart - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[2].context.kernelStart, expectedContextStart + errorOffset);
    EXPECT_GE(results.pSynchronizedTimestampsBuffer[2].context.kernelEnd, expectedContextEnd - errorOffset);
    EXPECT_LE(results.pSynchronizedTimestampsBuffer[2].context.kernelEnd, expectedContextEnd + errorOffset);
}

using HostMappedEventTests = Test<DeviceFixture>;
HWTEST_F(HostMappedEventTests, givenMappedEventsWhenSettingRefereshTimestampThenCorrectRefreshIntervalIsCalculated) {

    ze_event_pool_desc_t eventPoolDesc = {};
    const auto deviceTsFrequency = device->getNEODevice()->getDeviceInfo().profilingTimerResolution;
    const auto kernelTsValidBits = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.kernelTimestampValidBits;
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    uint64_t expectedTsRefreshIntervalInNanoSec = 0u;
    if (kernelTsValidBits >= 64) {
        expectedTsRefreshIntervalInNanoSec = maxNBitValue(kernelTsValidBits) / 2;
    } else {
        expectedTsRefreshIntervalInNanoSec = static_cast<uint64_t>((maxNBitValue(kernelTsValidBits) * deviceTsFrequency) / 2);
    }

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    auto event = std::unique_ptr<EventImp<uint64_t>>(static_cast<EventImp<uint64_t> *>(L0::Event::create<uint64_t>(eventPool.get(), &eventDesc, device)));
    ASSERT_NE(nullptr, event);

    // Reset before setting
    NEO::TimeStampData *resetReferenceTs = event->peekReferenceTs();
    resetReferenceTs->cpuTimeinNS = std::numeric_limits<uint64_t>::max();
    resetReferenceTs->gpuTimeStamp = std::numeric_limits<uint64_t>::max();

    event->setReferenceTs(expectedTsRefreshIntervalInNanoSec + 1);
    EXPECT_NE(resetReferenceTs->cpuTimeinNS, std::numeric_limits<uint64_t>::max());
    EXPECT_NE(resetReferenceTs->gpuTimeStamp, std::numeric_limits<uint64_t>::max());
}

HWTEST_F(HostMappedEventTests, givenEventTimestampRefreshIntervalInMilliSecIsSetThenCorrectRefreshIntervalIsCalculated) {

    const uint32_t refereshIntervalMillisec = 10;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EventTimestampRefreshIntervalInMilliSec.set(refereshIntervalMillisec);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    uint64_t expectedTsRefreshIntervalInNanoSec = refereshIntervalMillisec * 1000000;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    auto event = std::unique_ptr<EventImp<uint64_t>>(static_cast<EventImp<uint64_t> *>(L0::Event::create<uint64_t>(eventPool.get(), &eventDesc, device)));
    ASSERT_NE(nullptr, event);

    // Reset before setting
    NEO::TimeStampData *resetReferenceTs = event->peekReferenceTs();
    resetReferenceTs->cpuTimeinNS = 0;
    resetReferenceTs->gpuTimeStamp = 0;

    event->setReferenceTs(expectedTsRefreshIntervalInNanoSec + 1);
    EXPECT_NE(resetReferenceTs->cpuTimeinNS, 0u);
    EXPECT_NE(resetReferenceTs->gpuTimeStamp, 0u);
}

HWTEST_F(HostMappedEventTests, givenEventTimestampRefreshIntervalInMilliSecIsSetThenRefreshIntervalIsNotCalculatedIfCpuTimeLessThanInterval) {

    const uint32_t refereshIntervalMillisec = 10;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EventTimestampRefreshIntervalInMilliSec.set(refereshIntervalMillisec);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    uint64_t expectedTsRefreshIntervalInNanoSec = refereshIntervalMillisec * 1000000;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    auto event = std::unique_ptr<EventImp<uint64_t>>(static_cast<EventImp<uint64_t> *>(L0::Event::create<uint64_t>(eventPool.get(), &eventDesc, device)));
    ASSERT_NE(nullptr, event);

    // Reset before setting
    NEO::TimeStampData *resetReferenceTs = event->peekReferenceTs();
    resetReferenceTs->cpuTimeinNS = 1;
    resetReferenceTs->gpuTimeStamp = 1;

    event->setReferenceTs(expectedTsRefreshIntervalInNanoSec - 2);
    EXPECT_EQ(resetReferenceTs->cpuTimeinNS, 1u);
    EXPECT_EQ(resetReferenceTs->gpuTimeStamp, 1u);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet) {
    typename MockTimestampPackets32::Packet data = {};
    data.contextStart = 1u;
    data.contextEnd = 2u;
    data.globalStart = 3u;
    data.globalEnd = 4u;

    event->hostAddressFromPool = &data;
    ze_kernel_timestamp_result_t result = {};

    event->queryKernelTimestamp(&result);
    EXPECT_EQ(data.contextStart, result.context.kernelStart);
    EXPECT_EQ(data.contextEnd, result.context.kernelEnd);
    EXPECT_EQ(data.globalStart, result.global.kernelStart);
    EXPECT_EQ(data.globalEnd, result.global.kernelEnd);
}

HWTEST_F(TimestampEventCreate, givenFlagPrintCalculatedTimestampsWhenCallQueryKernelTimestampThenProperLogIsPrinted) {
    debugManager.flags.PrintCalculatedTimestamps.set(1);
    typename MockTimestampPackets32::Packet data = {};
    data.contextStart = 1u;
    data.contextEnd = 2u;
    data.globalStart = 3u;
    data.globalEnd = 4u;

    event->hostAddressFromPool = &data;
    ze_kernel_timestamp_result_t result = {};
    StreamCapture capture;
    capture.captureStdout();
    event->queryKernelTimestamp(&result);
    std::string output = capture.getCapturedStdout();
    std::stringstream expected;
    expected << "globalStartTS: " << result.global.kernelStart << ", "
             << "globalEndTS: " << result.global.kernelEnd << ", "
             << "contextStartTS: " << result.context.kernelStart << ", "
             << "contextEndTS: " << result.context.kernelEnd << std::endl;

    EXPECT_EQ(0, output.compare(expected.str().c_str()));
}

TEST_F(TimestampEventUsedPacketSignalCreate, givenFlagPrintTimestampPacketContentsWhenMultiPacketAndCallQueryKernelTimestampThenProperLogIsPrinted) {
    debugManager.flags.PrintTimestampPacketContents.set(1);
    typename MockTimestampPackets32::Packet packetData[2];

    packetData[0].contextStart = 1u;
    packetData[0].contextEnd = 2u;
    packetData[0].globalStart = 3u;
    packetData[0].globalEnd = 4u;

    packetData[1].contextStart = 5u;
    packetData[1].contextEnd = 6u;
    packetData[1].globalStart = 7u;
    packetData[1].globalEnd = 8u;

    event->hostAddressFromPool = packetData;

    ze_kernel_timestamp_result_t results;
    auto packedCount = 2u;
    event->setPacketsInUse(packedCount);

    StreamCapture capture;
    capture.captureStdout();
    event->queryKernelTimestamp(&results);
    std::string output = capture.getCapturedStdout();
    std::stringstream expected;

    for (uint32_t i = 0; i < packedCount; i++) {
        expected << "kernel id: 0, "
                 << "packet: " << i << ", "
                 << "globalStartTS: " << packetData[i].globalStart << ", "
                 << "globalEndTS: " << packetData[i].globalEnd << ", "
                 << "contextStartTS: " << packetData[i].contextStart << ", "
                 << "contextEndTS: " << packetData[i].contextEnd << std::endl;
    }
    EXPECT_EQ(0, output.compare(expected.str().c_str()));
}

TEST_F(TimestampEventUsedPacketSignalCreate, givenEventWithBlitAdditionalPropertiesWhenCallingCalulateProfilingDataThenCorrectDataSet) {
    typename MockTimestampPackets32::Packet packetData[3];
    uint32_t deviceCount = 1;
    ze_device_handle_t rootDeviceHandle;

    ze_result_t result = zeDeviceGet(driverHandle.get(), &deviceCount, &rootDeviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, neoDevice->getMemoryManager());
    MockTagAllocator<NEO::TimestampPackets<uint32_t, 2>> blitTagAllocator(0, neoDevice->getMemoryManager());
    auto event = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, L0::Device::fromHandle(rootDeviceHandle))));
    ASSERT_NE(nullptr, event);

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(deviceTagAllocator.getTag(), nullptr, *neoDevice, 1, false, false);

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->updateInOrderExecState(inOrderExecInfo, 1, 0);

    event->resetAdditionalTimestampNode(blitTagAllocator.getTag(), 1, false);

    event->setPacketsInUse(2u);

    packetData[0].contextStart = 1u;
    packetData[0].contextEnd = 2u;
    packetData[0].globalStart = 3u;
    packetData[0].globalEnd = 4u;

    packetData[1].contextStart = 5u;
    packetData[1].contextEnd = 6u;
    packetData[1].globalStart = 7u;
    packetData[1].globalEnd = 8u;

    packetData[2].contextStart = 9u;
    packetData[2].contextEnd = 10u;
    packetData[2].globalStart = 11u;
    packetData[2].globalEnd = 12u;

    event->hostAddressFromPool = packetData;
    uint32_t pCount = 3;

    for (uint32_t packetId = 0; packetId < pCount; packetId++) {
        event->kernelEventCompletionData[0].assignDataToAllTimestamps(packetId, event->hostAddressFromPool);
        event->hostAddressFromPool = ptrOffset(event->hostAddressFromPool, NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
    }
    result = event->calculateProfilingData();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(packetData[0].contextStart, event->contextStartTS);
    EXPECT_EQ(packetData[2].contextEnd, event->contextEndTS);
    EXPECT_EQ(packetData[0].globalStart, event->globalStartTS);
    EXPECT_EQ(packetData[2].globalEnd, event->globalEndTS);
}

HWTEST2_F(TimestampEventCreateMultiKernel, givenFlagPrintTimestampPacketContentsWhenMultiKernelsAndMultiPacketsAndCallQueryKernelTimestampThenProperLogIsPrinted, IsAtLeastXeCore) {
    debugManager.flags.PrintTimestampPacketContents.set(1);
    typename MockTimestampPackets32::Packet packetData[4];

    event->hostAddressFromPool = packetData;

    // 1st kernel 1st packet
    packetData[0].contextStart = 1;
    packetData[0].contextEnd = 2;
    packetData[0].globalStart = 3;
    packetData[0].globalEnd = 4;

    // 1st kernel 2nd packet
    packetData[1].contextStart = 5;
    packetData[1].contextEnd = 6;
    packetData[1].globalStart = 7;
    packetData[1].globalEnd = 8;

    // 2nd kernel 1st packet
    packetData[2].contextStart = 9;
    packetData[2].contextEnd = 10;
    packetData[2].globalStart = 11;
    packetData[2].globalEnd = 12;

    // 2nd kernel 2st packet
    packetData[3].contextStart = 13;
    packetData[3].contextEnd = 14;
    packetData[3].globalStart = 15;
    packetData[3].globalEnd = 16;

    auto packedCount = 2u;
    auto kernelCount = 2u;

    // set packet for first kernel
    event->setPacketsInUse(packedCount);
    event->setKernelCount(kernelCount);
    // set packet for second kernel
    event->setPacketsInUse(packedCount);

    ze_kernel_timestamp_result_t results;
    StreamCapture capture;
    capture.captureStdout();
    event->queryKernelTimestamp(&results);
    std::string output = capture.getCapturedStdout();
    std::stringstream expected;
    auto i = 0u;
    for (uint32_t kernelId = 0u; kernelId < kernelCount; kernelId++) {
        for (uint32_t packet = 0; packet < packedCount; packet++) {
            expected << "kernel id: " << kernelId << ", "
                     << "packet: " << packet << ", "
                     << "globalStartTS: " << packetData[i].globalStart << ", "
                     << "globalEndTS: " << packetData[i].globalEnd << ", "
                     << "contextStartTS: " << packetData[i].contextStart << ", "
                     << "contextEndTS: " << packetData[i].contextEnd << std::endl;
            i++;
        }
    }
    EXPECT_EQ(0, output.compare(expected.str().c_str()));
}

TEST_F(TimestampEventUsedPacketSignalCreate, givenEventWhenQueryingTimestampExpThenCorrectDataSet) {
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

    event->hostAddressFromPool = packetData;

    ze_kernel_timestamp_result_t results[2];
    uint32_t pCount = 2;

    for (uint32_t packetId = 0; packetId < pCount; packetId++) {
        event->kernelEventCompletionData[0].assignDataToAllTimestamps(packetId, event->hostAddressFromPool);
        event->hostAddressFromPool = ptrOffset(event->hostAddressFromPool, NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
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

HWTEST2_F(TimestampEventCreateMultiKernel, givenTimeStampEventUsedOnTwoKernelsWhenL3FlushSetOnFirstKernelThenDoNotUseSecondPacketOfFirstKernel, IsAtLeastXeCore) {
    typename MockTimestampPackets32::Packet packetData[4];

    event->hostAddressFromPool = packetData;

    constexpr uint32_t kernelStartValue = 5u;
    constexpr uint32_t kernelEndValue = 10u;

    constexpr uint32_t waStartValue = 2u;
    constexpr uint32_t waEndValue = 15u;

    // 1st kernel 1st packet
    packetData[0].contextStart = kernelStartValue;
    packetData[0].contextEnd = kernelEndValue;
    packetData[0].globalStart = kernelStartValue;
    packetData[0].globalEnd = kernelEndValue;

    // 1st kernel 2nd packet for L3 Flush
    packetData[1].contextStart = waStartValue;
    packetData[1].contextEnd = waEndValue;
    packetData[1].globalStart = waStartValue;
    packetData[1].globalEnd = waEndValue;

    // 2nd kernel 1st packet
    packetData[2].contextStart = kernelStartValue;
    packetData[2].contextEnd = kernelEndValue;
    packetData[2].globalStart = kernelStartValue;
    packetData[2].globalEnd = kernelEndValue;

    event->setPacketsInUse(2u);
    event->setL3FlushForCurrentKernel();

    event->increaseKernelCount();
    EXPECT_EQ(1u, event->getPacketsUsedInLastKernel());

    ze_kernel_timestamp_result_t results;
    event->queryKernelTimestamp(&results);

    EXPECT_EQ(static_cast<uint64_t>(kernelStartValue), results.context.kernelStart);
    EXPECT_EQ(static_cast<uint64_t>(kernelStartValue), results.global.kernelStart);
    EXPECT_EQ(static_cast<uint64_t>(kernelEndValue), results.context.kernelEnd);
    EXPECT_EQ(static_cast<uint64_t>(kernelEndValue), results.global.kernelEnd);
}

HWTEST2_F(TimestampEventCreateMultiKernel, givenTimeStampEventUsedOnTwoKernelsWhenL3FlushSetOnSecondKernelThenDoNotUseSecondPacketOfSecondKernel, IsAtLeastXeCore) {
    typename MockTimestampPackets32::Packet packetData[4];

    event->hostAddressFromPool = packetData;

    constexpr uint32_t kernelStartValue = 5u;
    constexpr uint32_t kernelEndValue = 10u;

    constexpr uint32_t waStartValue = 2u;
    constexpr uint32_t waEndValue = 15u;

    // 1st kernel 1st packet
    packetData[0].contextStart = kernelStartValue;
    packetData[0].contextEnd = kernelEndValue;
    packetData[0].globalStart = kernelStartValue;
    packetData[0].globalEnd = kernelEndValue;

    // 2nd kernel 1st packet
    packetData[1].contextStart = kernelStartValue;
    packetData[1].contextEnd = kernelEndValue;
    packetData[1].globalStart = kernelStartValue;
    packetData[1].globalEnd = kernelEndValue;

    // 2nd kernel 2nd packet for L3 Flush
    packetData[2].contextStart = waStartValue;
    packetData[2].contextEnd = waEndValue;
    packetData[2].globalStart = waStartValue;
    packetData[2].globalEnd = waEndValue;

    EXPECT_EQ(1u, event->getPacketsUsedInLastKernel());

    event->increaseKernelCount();
    event->setPacketsInUse(2u);
    event->setL3FlushForCurrentKernel();

    ze_kernel_timestamp_result_t results;
    event->queryKernelTimestamp(&results);

    EXPECT_EQ(static_cast<uint64_t>(kernelStartValue), results.context.kernelStart);
    EXPECT_EQ(static_cast<uint64_t>(kernelStartValue), results.global.kernelStart);
    EXPECT_EQ(static_cast<uint64_t>(kernelEndValue), results.context.kernelEnd);
    EXPECT_EQ(static_cast<uint64_t>(kernelEndValue), results.global.kernelEnd);
}

HWTEST2_F(TimestampEventCreateMultiKernel, givenOverflowingTimeStampDataOnTwoKernelsWhenQueryKernelTimestampIsCalledOverflowIsObserved, IsAtLeastXeCore) {
    typename MockTimestampPackets32::Packet packetData[4] = {};
    event->hostAddressFromPool = packetData;

    uint32_t maxTimeStampValue = std::numeric_limits<uint32_t>::max();

    // 1st kernel 1st packet (overflowing context timestamp)
    packetData[0].contextStart = maxTimeStampValue - 1;
    packetData[0].contextEnd = maxTimeStampValue + 1;
    packetData[0].globalStart = maxTimeStampValue - 2;
    packetData[0].globalEnd = maxTimeStampValue - 1;

    // 2nd kernel 1st packet (overflowing global timestamp)
    packetData[1].contextStart = maxTimeStampValue - 2;
    packetData[1].contextEnd = maxTimeStampValue - 1;
    packetData[1].globalStart = maxTimeStampValue - 1;
    packetData[1].globalEnd = maxTimeStampValue + 1;

    // 2nd kernel 2nd packet (overflowing context timestamp)
    memcpy(&packetData[2], &packetData[0], sizeof(MockTimestampPackets32::Packet));
    packetData[2].contextStart = maxTimeStampValue;
    packetData[2].contextEnd = maxTimeStampValue + 2;

    EXPECT_EQ(1u, event->getPacketsUsedInLastKernel());

    event->increaseKernelCount();
    event->setPacketsInUse(2u);

    ze_kernel_timestamp_result_t results;
    event->queryKernelTimestamp(&results);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    if (gfxCoreHelper.useOnlyGlobalTimestamps() == false) {
        EXPECT_EQ(static_cast<uint64_t>(maxTimeStampValue - 2), results.context.kernelStart);
        EXPECT_EQ(static_cast<uint64_t>(maxTimeStampValue + 2), results.context.kernelEnd);
    }

    EXPECT_EQ(static_cast<uint64_t>(maxTimeStampValue - 2), results.global.kernelStart);
    EXPECT_EQ(static_cast<uint64_t>(maxTimeStampValue + 1), results.global.kernelEnd);
}

HWTEST_EXCLUDE_PRODUCT(TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet, IGFX_GEN12LP_CORE);

TEST_F(TimestampEventCreate, givenEventWhenQueryKernelTimestampThenNotReadyReturned) {
    struct MockEventQuery : public L0::EventImp<uint32_t> {
        MockEventQuery(int index, L0::Device *device) : EventImp(index, device, false) {}

        ze_result_t queryStatus() override {
            return ZE_RESULT_NOT_READY;
        }
    };

    auto mockEvent = std::make_unique<MockEventQuery>(1u, device);

    ze_kernel_timestamp_result_t resultTimestamp = {};

    auto result = mockEvent->queryKernelTimestamp(&resultTimestamp);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    EXPECT_EQ(0u, resultTimestamp.context.kernelStart);
    EXPECT_EQ(0u, resultTimestamp.context.kernelEnd);
    EXPECT_EQ(0u, resultTimestamp.global.kernelStart);
    EXPECT_EQ(0u, resultTimestamp.global.kernelEnd);
}

using EventPoolCreateMultiDeviceFlatHierarchy = Test<MultiDeviceFixtureFlatHierarchy>;

TEST_F(EventPoolCreateMultiDeviceFlatHierarchy, givenFlatHierarchyWhenCallZeGetDevicesThenSubDevicesAreReturnedAsSeparateDevices) {
    uint32_t deviceCount = 0;
    ze_result_t result = zeDeviceGet(driverHandle.get(), &deviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(deviceCount, numRootDevices * numSubDevices);

    ze_device_handle_t *devices = new ze_device_handle_t[deviceCount];
    result = zeDeviceGet(driverHandle.get(), &deviceCount, devices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t i = 0u;
    for (const auto device : driverHandle->devices) {
        auto deviceImpl = static_cast<DeviceImp *>(device);
        for (const auto subdevice : deviceImpl->subDevices) {
            EXPECT_EQ(devices[i], subdevice);
            i++;
        }
    }

    static_cast<DeviceImp *>(driverHandle->devices[1])->numSubDevices = 0;
    driverHandle->setupDevicesToExpose();
    uint32_t deviceCount2 = 0;
    result = zeDeviceGet(driverHandle.get(), &deviceCount2, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(deviceCount2, (numRootDevices - 1) * numSubDevices + 1);
    ze_device_handle_t *devices2 = new ze_device_handle_t[deviceCount2];

    result = zeDeviceGet(driverHandle.get(), &deviceCount2, devices2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    i = 0u;
    for (const auto device : driverHandle->devices) {
        auto deviceImpl = static_cast<DeviceImp *>(device);
        if (deviceImpl->numSubDevices > 0) {
            for (const auto subdevice : deviceImpl->subDevices) {
                EXPECT_EQ(devices2[i], subdevice);
                i++;
            }
        } else {
            EXPECT_EQ(devices2[i], device);
            i++;
        }
    }

    static_cast<DeviceImp *>(driverHandle->devices[1])->numSubDevices = numSubDevices;
    delete[] devices2;
    delete[] devices;
}

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

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
            UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);
            UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);
        }
        executionEnvironment->calculateMaxOsContextCount();

        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (uint32_t i = 0; i < numRootDevices; i++) {
            neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, i);
            devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockEventPoolCreateMemoryManager = true;

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

    auto eventPool = new L0::EventPool(&eventPoolDesc);
    EXPECT_NE(nullptr, eventPool);

    result = eventPool->initialize(driverHandle.get(), context, numRootDevices, devices);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);

    delete eventPool;
    delete[] devices;
}

using EventTests = Test<EventFixture<1, 0>>;
using EventUsedPacketSignalTests = Test<EventUsedPacketSignalFixture<1, 0, 0, -1>>;

TEST_F(EventTests, WhenQueryingStatusThenSuccessIsReturned) {
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    ASSERT_NE(event, nullptr);

    auto result = event->hostSignal(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);

    event->destroy();
}

TEST_F(EventTests, givenForceHostSignalScopeSetToOneWhenCreateEventWithoutScopeThenHostScopeSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForceHostSignalScope.set(1);

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    ASSERT_NE(event, nullptr);

    EXPECT_TRUE(event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));

    event->destroy();
}

TEST_F(EventTests, givenForceHostSignalScopeSetToZeroWhenCreateEventWithHostScopeThenHostScopeUnset) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForceHostSignalScope.set(0);

    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    ASSERT_NE(event, nullptr);

    EXPECT_FALSE(event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));

    event->destroy();
}

TEST_F(EventTests, givenAbortHostSyncOnNonHostVisibleEventWhenWaitForNonHostVisibleEventThenAbort) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.AbortHostSyncOnNonHostVisibleEvent.set(true);

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    ASSERT_NE(event, nullptr);

    EXPECT_ANY_THROW(event->hostSynchronize(std::numeric_limits<uint64_t>::max()));

    event->destroy();
}

TEST_F(EventTests, GivenResetWhenQueryingStatusThenNotReadyIsReturned) {
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    ASSERT_NE(event, nullptr);

    auto result = event->hostSignal(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    event->setUsingContextEndOffset(true);

    result = event->reset();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);

    event->destroy();
}

TEST_F(EventTests, WhenDestroyingAnEventThenSuccessIsReturned) {
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
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

    auto event0 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc0, device));
    ASSERT_NE(event0, nullptr);

    auto event1 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc1, device));
    ASSERT_NE(event1, nullptr);

    EXPECT_NE(event0->hostAddressFromPool, event1->hostAddressFromPool);
    EXPECT_NE(event0->getGpuAddress(device), event1->getGpuAddress(device));

    event0->destroy();
    event1->destroy();
}

TEST_F(EventTests, givenRegularEventUseMultiplePacketsWhenHostSignalThenExpectAllPacketsAreSignaled) {
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;

    uint32_t packetsUsed = std::min(4u, eventPool->getEventMaxPackets());

    eventPool->setEventSize(static_cast<uint32_t>(alignUp(packetsUsed * device->getGfxCoreHelper().getSingleTimestampPacketSize(), 64)));

    auto event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(),
                                                                                                                           &eventDesc,
                                                                                                                           device)));
    ASSERT_NE(event, nullptr);

    uint32_t *hostAddr = static_cast<uint32_t *>(event->getCompletionFieldHostAddress());
    EXPECT_EQ(*hostAddr, Event::STATE_INITIAL);
    EXPECT_EQ(1u, event->getPacketsInUse());

    event->setPacketsInUse(packetsUsed);
    event->setEventTimestampFlag(false);
    event->hostSignal(false);
    for (uint32_t i = 0; i < packetsUsed; i++) {
        EXPECT_EQ(Event::STATE_SIGNALED, *hostAddr);
        hostAddr = ptrOffset(hostAddr, event->getSinglePacketSize());
    }
}

TEST_F(EventUsedPacketSignalTests, givenEventUseMultiplePacketsWhenHostSignalThenExpectAllPacketsAreSignaled) {
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;

    uint32_t packetsUsed = std::min(4u, eventPool->getEventMaxPackets());

    eventPool->setEventSize(static_cast<uint32_t>(alignUp(packetsUsed * device->getGfxCoreHelper().getSingleTimestampPacketSize(), 64)));

    auto event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(),
                                                                                                                           &eventDesc,
                                                                                                                           device)));
    ASSERT_NE(event, nullptr);

    size_t eventOffset = event->getCompletionFieldOffset();

    uint32_t *hostAddr = static_cast<uint32_t *>(ptrOffset(event->getHostAddress(), eventOffset));

    EXPECT_EQ(Event::STATE_INITIAL, *hostAddr);
    EXPECT_EQ(1u, event->getPacketsInUse());

    event->setPacketsInUse(packetsUsed);
    event->setEventTimestampFlag(false);

    event->hostSignal(false);
    for (uint32_t i = 0; i < packetsUsed; i++) {
        EXPECT_EQ(Event::STATE_SIGNALED, *hostAddr);
        hostAddr = ptrOffset(hostAddr, event->getSinglePacketSize());
    }
}

using EventUsedPacketSignalNoCompactionTests = Test<EventUsedPacketSignalFixture<1, 0, 0, 0>>;
HWTEST2_F(EventUsedPacketSignalNoCompactionTests, WhenSettingL3FlushOnEventThenSetOnParticularKernel, IsAtLeastXeCore) {
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    ASSERT_NE(event, nullptr);
    EXPECT_FALSE(event->getL3FlushForCurrenKernel());

    event->setL3FlushForCurrentKernel();
    EXPECT_TRUE(event->getL3FlushForCurrenKernel());

    event->increaseKernelCount();
    EXPECT_EQ(2u, event->getKernelCount());

    EXPECT_FALSE(event->getL3FlushForCurrenKernel());

    event->setL3FlushForCurrentKernel();
    EXPECT_TRUE(event->getL3FlushForCurrenKernel());

    event->reset();
    EXPECT_FALSE(event->getL3FlushForCurrenKernel());

    constexpr size_t expectedL3FlushOnKernelCount = 0;
    EXPECT_EQ(expectedL3FlushOnKernelCount, event->l3FlushAppliedOnKernel.count());

    event->destroy();
}

struct EventSizeFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        hDevice = device->toHandle();
    }

    void tearDown() {
        eventObj0.reset(nullptr);
        eventObj1.reset(nullptr);
        eventPool.reset(nullptr);
        DeviceFixture::tearDown();
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
    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<L0::Event> eventObj0;
    std::unique_ptr<L0::Event> eventObj1;
};

using EventSizeTests = Test<EventSizeFixture>;

HWTEST_F(EventSizeTests, whenCreatingEventPoolThenUseCorrectSizeAndAlignment) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool.reset(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &hwInfo = device->getHwInfo();

    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t packetCount = EventPacketsCount::eventPackets;
    if (l0GfxCoreHelper.useDynamicEventPacketsCount(hwInfo)) {
        packetCount = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());
    }

    auto expectedAlignment = static_cast<uint32_t>(gfxCoreHelper.getTimestampPacketAllocatorAlignment());
    auto singlePacketSize = TimestampPackets<typename FamilyType::TimestampPacketType, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();
    auto expectedSize = static_cast<uint32_t>(alignUp(packetCount * singlePacketSize, expectedAlignment));

    EXPECT_EQ(expectedSize, eventPool->getEventSize());

    createEvents();

    constexpr size_t timestampPacketTypeSize = sizeof(typename FamilyType::TimestampPacketType);

    EXPECT_EQ(timestampPacketTypeSize / 4, eventObj0->getTimestampSizeInDw());
    EXPECT_EQ(timestampPacketTypeSize / 4, eventObj1->getTimestampSizeInDw());

    EXPECT_EQ(0u, eventObj0->getContextStartOffset());
    EXPECT_EQ(timestampPacketTypeSize, eventObj0->getGlobalStartOffset());
    EXPECT_EQ(timestampPacketTypeSize * 2, eventObj0->getContextEndOffset());
    EXPECT_EQ(timestampPacketTypeSize * 3, eventObj0->getGlobalEndOffset());

    EXPECT_EQ(l0GfxCoreHelper.getImmediateWritePostSyncOffset(), eventObj0->getSinglePacketSize());

    auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
    EXPECT_EQ(expectedSize, hostPtrDiff);
}

HWTEST_F(EventSizeTests, givenDebugFlagwhenCreatingEventPoolThenUseCorrectSizeAndAlignment) {
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &hwInfo = device->getHwInfo();
    auto expectedAlignment = static_cast<uint32_t>(gfxCoreHelper.getTimestampPacketAllocatorAlignment());

    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t packetCount = EventPacketsCount::eventPackets;
    if (l0GfxCoreHelper.useDynamicEventPacketsCount(hwInfo)) {
        packetCount = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());
    }

    {
        debugManager.flags.OverrideTimestampPacketSize.set(4);

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool.reset(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto singlePacketSize = TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();

        auto expectedSize = static_cast<uint32_t>(alignUp(packetCount * singlePacketSize, expectedAlignment));

        EXPECT_EQ(expectedSize, eventPool->getEventSize());

        createEvents();

        EXPECT_EQ(1u, eventObj0->getTimestampSizeInDw());
        EXPECT_EQ(1u, eventObj1->getTimestampSizeInDw());

        auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
        EXPECT_EQ(expectedSize, hostPtrDiff);
    }

    {
        debugManager.flags.OverrideTimestampPacketSize.set(8);

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool.reset(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto singlePacketSize = TimestampPackets<uint64_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();

        auto expectedSize = static_cast<uint32_t>(alignUp(packetCount * singlePacketSize, expectedAlignment));

        EXPECT_EQ(expectedSize, eventPool->getEventSize());

        createEvents();

        EXPECT_EQ(2u, eventObj0->getTimestampSizeInDw());
        EXPECT_EQ(2u, eventObj1->getTimestampSizeInDw());

        auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
        EXPECT_EQ(expectedSize, hostPtrDiff);
    }

    {
        debugManager.flags.OverrideTimestampPacketSize.set(12);
        ze_result_t result = ZE_RESULT_SUCCESS;
        EXPECT_ANY_THROW(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
        EXPECT_ANY_THROW(createEvents());
    }
}

HWTEST_F(EventTests, whenCreatingNonTimestampEventsThenPacketsSizeIsQword) {
    DebugManagerStateRestore restore;

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    const ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};

    std::unique_ptr<L0::EventPool> timestampPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    eventPoolDesc.flags = 0;
    std::unique_ptr<L0::EventPool> regularPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    ze_event_handle_t timestampEventHandle = nullptr;
    ze_event_handle_t regularEventHandle = nullptr;

    timestampPool->createEvent(&eventDesc, &timestampEventHandle);
    regularPool->createEvent(&eventDesc, &regularEventHandle);

    auto timestampEvent = Event::fromHandle(timestampEventHandle);
    auto regularEvent = Event::fromHandle(regularEventHandle);

    auto timestampSinglePacketSize = NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    EXPECT_EQ(timestampSinglePacketSize, timestampEvent->getSinglePacketSize());
    EXPECT_EQ(device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset(), regularEvent->getSinglePacketSize());

    timestampEvent->destroy();
    regularEvent->destroy();
}

HWTEST_F(EventTests, GivenEventWhenHostSynchronizeCalledThenExpectDownloadEventAllocationOnlyWhenEventWasUsedOnGpu) {
    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    constexpr uint32_t iterations = 5;

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));

    ASSERT_NE(event, nullptr);
    ASSERT_NE(nullptr, event->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, event->csrs[0]);
    event->setUsingContextEndOffset(false);

    size_t eventCompletionOffset = event->getContextStartOffset();
    if (event->isUsingContextEndOffset()) {
        eventCompletionOffset = event->getContextEndOffset();
    }
    TagAddressType *eventAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    *eventAddress = Event::STATE_INITIAL;

    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = eventAddress;

    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (CpuIntrinsicsTests::pauseCounter >= iterations) {
            volatile TagAddressType *packet = CpuIntrinsicsTests::pauseAddress;
            *packet = Event::STATE_SIGNALED;
        }
    };

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0]);
    ultCsr->initializeResources(false, device->getDevicePreemptionMode());
    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto eventAllocation = event->getAllocation(device);
    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    auto result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t downloadedAllocations = downloadAllocationTrack[eventAllocation];
    EXPECT_EQ(0u, downloadedAllocations);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);

    event->reset();
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = eventAddress;
    *eventAddress = Event::STATE_INITIAL;
    ultCsr->downloadAllocationsCalledCount = 0;

    eventAllocation->updateTaskCount(0u, ultCsr->getOsContext().getContextId());

    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    downloadedAllocations = downloadAllocationTrack[eventAllocation];
    EXPECT_EQ(iterations + 1u, downloadedAllocations);
    EXPECT_EQ(2u, ultCsr->downloadAllocationsCalledCount);

    event->destroy();
}

struct EventContextGroupFixture : public EventFixture<1, 0> {
    using BaseClass = EventFixture<1, 0>;
    void setUp() {
        HardwareInfo hwInfo = *defaultHwInfo;
        if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
            skipped = true;
            GTEST_SKIP();
        }

        debugManager.flags.ContextGroupSize.set(2);
        BaseClass::setUp();
    }

    void tearDown() {
        if (!skipped) {
            BaseClass::tearDown();
        }
    }

    DebugManagerStateRestore restore;
    bool skipped = false;
};

using EventContextGroupTests = Test<EventContextGroupFixture>;

HWTEST_F(EventContextGroupTests, givenSecondaryCsrWhenDownloadingAllocationThenUseCorrectCsr) {
    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));

    size_t eventCompletionOffset = event->getContextStartOffset();
    if (event->isUsingContextEndOffset()) {
        eventCompletionOffset = event->getContextEndOffset();
    }
    TagAddressType *eventAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    *eventAddress = Event::STATE_INITIAL;

    auto ultCsr = new UltCommandStreamReceiver<FamilyType>(*neoDevice->getExecutionEnvironment(), 0, 1);
    neoDevice->secondaryCsrs.clear();
    neoDevice->secondaryCsrs.push_back(std::unique_ptr<UltCommandStreamReceiver<FamilyType>>(ultCsr));

    OsContext osContext(0, static_cast<uint32_t>(neoDevice->getAllEngines().size()), EngineDescriptorHelper::getDefaultDescriptor());

    ultCsr->setupContext(osContext);
    ultCsr->initializeResources(false, device->getDevicePreemptionMode());

    uint32_t downloadCounter = 0;
    ultCsr->downloadAllocationImpl = [&downloadCounter](GraphicsAllocation &gfxAllocation) {
        downloadCounter++;
    };

    auto eventAllocation = event->getAllocation(device);
    ultCsr->makeResident(*eventAllocation);

    event->hostSynchronize(1);

    EXPECT_EQ(1u, downloadCounter);

    event->destroy();
}

HWTEST_F(EventTests, GivenEventUsedOnNonDefaultCsrWhenHostSynchronizeCalledThenAllocationIsDownloaded) {
    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));

    ASSERT_NE(event, nullptr);
    ASSERT_NE(nullptr, event->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, event->csrs[0]);
    event->setUsingContextEndOffset(false);

    size_t eventCompletionOffset = event->getContextStartOffset();
    if (event->isUsingContextEndOffset()) {
        eventCompletionOffset = event->getContextEndOffset();
    }
    TagAddressType *eventAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    *eventAddress = Event::STATE_SIGNALED;

    EXPECT_LT(1u, neoDevice->getAllEngines().size());

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(neoDevice->getAllEngines()[1].commandStreamReceiver);
    ultCsr->initializeResources(false, device->getDevicePreemptionMode());
    EXPECT_NE(event->csrs[0], ultCsr);

    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto eventAllocation = event->getAllocation(device);
    constexpr uint64_t timeout = 0;
    auto result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t downloadedAllocations = downloadAllocationTrack[eventAllocation];
    EXPECT_EQ(0u, downloadedAllocations);
    EXPECT_EQ(0u, ultCsr->downloadAllocationsCalledCount);

    *eventAddress = Event::STATE_SIGNALED;

    ultCsr->downloadAllocationsCalledCount = 0;

    eventAllocation->updateTaskCount(0u, ultCsr->getOsContext().getContextId());

    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    downloadedAllocations = downloadAllocationTrack[eventAllocation];
    EXPECT_EQ(1u, downloadedAllocations);

    event->destroy();
}

HWTEST_F(EventTests, givenRegularEventWhenCallingResetAdditionalTimestampNodeMultipleTimesWithTagAllocatorThenTagAddedToAdditionalTimestampNodeVectorOnce) {

    MockTagAllocator<DeviceAllocNodeType<true>> eventTagAllocator(0, neoDevice->getMemoryManager());
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device)));
    ASSERT_NE(event, nullptr);

    event->resetAdditionalTimestampNode(eventTagAllocator.getTag(), 1, false);

    event->resetAdditionalTimestampNode(eventTagAllocator.getTag(), 1, false);
    EXPECT_EQ(1u, event->additionalTimestampNode.size());
}

HWTEST_F(EventTests, givenRegularEventWhenCallingResetAdditionalTimestampNodeWithNullptrThenVectorCleared) {

    MockTagAllocator<DeviceAllocNodeType<true>> eventTagAllocator(0, neoDevice->getMemoryManager());
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device)));
    ASSERT_NE(event, nullptr);

    event->resetAdditionalTimestampNode(eventTagAllocator.getTag(), 1, false);
    EXPECT_EQ(1u, event->additionalTimestampNode.size());

    event->resetAdditionalTimestampNode(nullptr, 0, false);
    EXPECT_EQ(0u, event->additionalTimestampNode.size());
}

HWTEST_F(EventTests, givenInOrderEventWhenHostSynchronizeIsCalledThenAllocationIsDonwloadedOnlyAfterEventWasUsedOnGpu) {
    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, neoDevice->getMemoryManager());

    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device)));

    ASSERT_NE(event, nullptr);

    TagAddressType *eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0]);
    ultCsr->initializeResources(false, device->getDevicePreemptionMode());

    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto mockNode = tagAllocator.getTag();
    auto syncAllocation = mockNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(mockNode, nullptr, *neoDevice, 1, false, false);
    *inOrderExecInfo->getBaseHostAddress() = 1;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->updateInOrderExecState(inOrderExecInfo, 1, 0);

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    auto result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, downloadAllocationTrack[syncAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);

    auto event2 = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device)));

    event2->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event2->updateInOrderExecState(inOrderExecInfo, 1, 0);
    syncAllocation->updateTaskCount(0u, ultCsr->getOsContext().getContextId());
    ultCsr->downloadAllocationsCalledCount = 0;
    eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    result = event2->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(0u, downloadAllocationTrack[syncAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);
}

HWTEST_F(EventTests, givenStandaloneCbEventAndTbxModeWhenSynchronizingThenHandleCorrectly) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    uint64_t counterValue = 2;

    ze_host_mem_alloc_desc_t desc = {};
    void *ptr = nullptr;
    context->allocHostMem(&desc, sizeof(uint64_t), 1, &ptr);

    uint64_t *hostAddress = static_cast<uint64_t *>(ptr);

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue, &eventDesc, &handle));

    auto eventObj = Event::fromHandle(handle);

    ASSERT_NE(event, nullptr);
    auto result = eventObj->hostSynchronize(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zeEventDestroy(handle);
    context->freeMem(hostAddress);
}

HWTEST_F(EventTests, givenOsAgnosticContextWhenAllocatingInterruptThenReturnError) {
    uint32_t interruptId = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexIntelAllocateNetworkInterrupt(nullptr, interruptId));

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zexIntelAllocateNetworkInterrupt(context, interruptId));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexIntelReleaseNetworkInterrupt(nullptr, interruptId));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexIntelReleaseNetworkInterrupt(context, interruptId));
}

HWTEST_F(EventTests, givenInOrderEventWithHostAllocWhenHostSynchronizeIsCalledThenAllocationIsDonwloadedOnlyAfterEventWasUsedOnGpu) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, neoDevice->getMemoryManager());
    MockTagAllocator<DeviceAllocNodeType<true>> hostTagAllocator(0, neoDevice->getMemoryManager());

    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device)));

    ASSERT_NE(event, nullptr);

    TagAddressType *eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0]);
    ultCsr->initializeResources(false, device->getDevicePreemptionMode());
    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto deviceMockNode = deviceTagAllocator.getTag();
    auto hostMockNode = hostTagAllocator.getTag();
    auto deviceSyncAllocation = deviceMockNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    auto hostSyncAllocation = hostMockNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(deviceMockNode, hostMockNode, *neoDevice, 1, false, false);
    *inOrderExecInfo->getBaseHostAddress() = 1;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->updateInOrderExecState(inOrderExecInfo, 1, 0);

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    auto result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, downloadAllocationTrack[deviceSyncAllocation]);
    EXPECT_EQ(0u, downloadAllocationTrack[hostSyncAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);

    auto event2 = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device)));

    event2->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event2->updateInOrderExecState(inOrderExecInfo, 1, 0);
    hostSyncAllocation->updateTaskCount(0u, ultCsr->getOsContext().getContextId());
    ultCsr->downloadAllocationsCalledCount = 0;
    eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    result = event2->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, downloadAllocationTrack[deviceSyncAllocation]);
    EXPECT_NE(0u, downloadAllocationTrack[hostSyncAllocation]);
    EXPECT_NE(0u, downloadAllocationTrack[hostSyncAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);
}

HWTEST_F(EventTests, GivenEventIsReadyToDownloadAllAlocationsWhenDownloadAllocationNotRequiredThenDontDownloadAllocations) {
    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::hardware;

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));

    size_t offset = event->getCompletionFieldOffset();
    void *completionAddress = ptrOffset(event->hostAddressFromPool, offset);
    size_t packets = event->getPacketsInUse();
    uint64_t signaledValue = Event::STATE_SIGNALED;
    for (size_t i = 0; i < packets; i++) {
        memcpy(completionAddress, &signaledValue, sizeof(uint64_t));
        completionAddress = ptrOffset(completionAddress, event->getSinglePacketSize());
    }

    auto status = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
    EXPECT_EQ(0u, static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0])->downloadAllocationsCalledCount);
    event->destroy();
}

HWTEST_F(EventTests, GivenNotReadyEventBecomesReadyWhenDownloadAllocationRequiredThenDownloadAllocationsOnce) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    CommandStreamReceiverType tbxCsrTypes[] = {CommandStreamReceiverType::tbx, CommandStreamReceiverType::tbxWithAub};

    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(neoDevice->getUltCommandStreamReceiver<FamilyType>());
    for (auto csrType : tbxCsrTypes) {
        ultCsr.commandStreamReceiverType = csrType;
        auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));

        auto status = event->queryStatus();
        EXPECT_EQ(ZE_RESULT_NOT_READY, status);
        EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);

        size_t offset = event->getCompletionFieldOffset();
        void *completionAddress = ptrOffset(event->hostAddressFromPool, offset);
        size_t packets = event->getPacketsInUse();
        uint64_t signaledValue = Event::STATE_SIGNALED;
        for (size_t i = 0; i < packets; i++) {
            memcpy(completionAddress, &signaledValue, sizeof(uint64_t));
            completionAddress = ptrOffset(completionAddress, event->getSinglePacketSize());
        }

        status = event->queryStatus();
        EXPECT_EQ(ZE_RESULT_SUCCESS, status);
        EXPECT_EQ(1u, ultCsr.downloadAllocationsCalledCount);

        status = event->queryStatus();
        EXPECT_EQ(ZE_RESULT_SUCCESS, status);
        EXPECT_EQ(1u, ultCsr.downloadAllocationsCalledCount);

        event->destroy();
        ultCsr.downloadAllocationsCalledCount = 0;
    }
}
HWTEST_F(EventTests, GivenCsrTbxModeWhenEventCreatedAndSignaledThenEventAllocationIsWritten) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();
    auto mockMemIface = static_cast<NEO::MockMemoryOperations *>(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.get());

    constexpr uint32_t expectedBanks = std::numeric_limits<uint32_t>::max();

    mockMemIface->captureGfxAllocationsForMakeResident = true;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;

    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    EXPECT_EQ(0u, ultCsr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(0u, ultCsr.writeMemoryParams.chunkWriteCallCount);

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    auto eventAllocation = event->getAllocation(device);
    auto offsetInSharedAlloc = event->getOffsetInSharedAlloc();

    EXPECT_TRUE(eventAllocation->getAubInfo().writeMemoryOnly);

    auto eventAllocItor = std::find(mockMemIface->gfxAllocationsForMakeResident.begin(),
                                    mockMemIface->gfxAllocationsForMakeResident.end(),
                                    eventAllocation);
    EXPECT_EQ(mockMemIface->gfxAllocationsForMakeResident.end(), eventAllocItor);

    uint32_t expectedCallCount = std::min(static_cast<uint32_t>(eventPool->getEventSize() / sizeof(uint64_t)), uint32_t(16));

    EXPECT_EQ(2u, ultCsr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(1u, ultCsr.writeMemoryParams.chunkWriteCallCount);
    EXPECT_EQ(eventAllocation, ultCsr.writeMemoryParams.latestGfxAllocation);
    EXPECT_TRUE(ultCsr.writeMemoryParams.latestChunkedMode);
    EXPECT_EQ(sizeof(uint64_t) * expectedCallCount, ultCsr.writeMemoryParams.latestChunkSize);
    EXPECT_EQ(0u + offsetInSharedAlloc, ultCsr.writeMemoryParams.latestGpuVaChunkOffset);
    EXPECT_FALSE(eventAllocation->isTbxWritable(expectedBanks));

    auto status = event->hostSignal(false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);

    EXPECT_EQ(3u, ultCsr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(2u, ultCsr.writeMemoryParams.chunkWriteCallCount);
    EXPECT_EQ(eventAllocation, ultCsr.writeMemoryParams.latestGfxAllocation);
    EXPECT_TRUE(ultCsr.writeMemoryParams.latestChunkedMode);
    EXPECT_EQ(event->getSinglePacketSize(), ultCsr.writeMemoryParams.latestChunkSize);
    EXPECT_EQ(0u + offsetInSharedAlloc, ultCsr.writeMemoryParams.latestGpuVaChunkOffset);

    EXPECT_FALSE(eventAllocation->isTbxWritable(expectedBanks));

    std::bitset<32> singleBitMask;
    for (uint32_t i = 0; i < 32; i++) {
        singleBitMask.reset();
        singleBitMask.set(i, true);
        uint32_t bit = static_cast<uint32_t>(singleBitMask.to_ulong());
        EXPECT_FALSE(eventAllocation->isTbxWritable(bit));
    }

    event->reset();
    EXPECT_EQ(0, mockMemIface->makeResidentCalledCount);

    EXPECT_EQ(4u, ultCsr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(3u, ultCsr.writeMemoryParams.chunkWriteCallCount);
    EXPECT_EQ(eventAllocation, ultCsr.writeMemoryParams.latestGfxAllocation);
    EXPECT_TRUE(ultCsr.writeMemoryParams.latestChunkedMode);
    EXPECT_EQ(event->getSinglePacketSize(), ultCsr.writeMemoryParams.latestChunkSize);
    EXPECT_EQ(0u + offsetInSharedAlloc, ultCsr.writeMemoryParams.latestGpuVaChunkOffset);

    EXPECT_FALSE(eventAllocation->isTbxWritable(expectedBanks));

    size_t offset = event->getCompletionFieldOffset();
    void *completionAddress = ptrOffset(event->hostAddressFromPool, offset);
    size_t packets = event->getPacketsInUse();
    uint64_t signaledValue = Event::STATE_SIGNALED;
    for (size_t i = 0; i < packets; i++) {
        memcpy(completionAddress, &signaledValue, sizeof(uint64_t));
        completionAddress = ptrOffset(completionAddress, event->getSinglePacketSize());
    }

    status = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
    EXPECT_EQ(1u, ultCsr.downloadAllocationsCalledCount);

    event->destroy();
}

template <typename TagSizeT>
struct MockEventCompletion : public L0::EventImp<TagSizeT> {
    using BaseClass = L0::EventImp<TagSizeT>;
    using BaseClass::gpuEndTimestamp;
    using BaseClass::gpuStartTimestamp;
    using BaseClass::hostAddressFromPool;

    MockEventCompletion(MultiGraphicsAllocation *alloc, uint32_t eventSize, uint32_t maxKernelCount, uint32_t maxPacketsCount, int index, L0::Device *device) : BaseClass::EventImp(index, device, false) {
        auto neoDevice = device->getNEODevice();
        auto &hwInfo = neoDevice->getHardwareInfo();
        this->signalAllEventPackets = L0GfxCoreHelper::useSignalAllEventPackets(hwInfo);

        this->eventPoolAllocation = alloc;

        uint64_t baseHostAddr = reinterpret_cast<uint64_t>(alloc->getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex())->getUnderlyingBuffer());
        this->totalEventSize = eventSize;
        this->eventPoolOffset = index * this->totalEventSize;
        hostAddressFromPool = reinterpret_cast<void *>(baseHostAddr + this->eventPoolOffset);
        this->csrs[0] = neoDevice->getDefaultEngine().commandStreamReceiver;

        this->maxKernelCount = maxKernelCount;
        this->maxPacketCount = maxPacketsCount;

        this->kernelEventCompletionData = std::make_unique<KernelEventCompletionData<TagSizeT>[]>(this->maxKernelCount);
    }

    void assignKernelEventCompletionData(void *address) override {
        assignKernelEventCompletionDataCounter++;
    }

    ze_result_t hostEventSetValue(Event::State eventState) override {
        if (shouldHostEventSetValueFail) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return BaseClass::hostEventSetValue(eventState);
    }

    ze_result_t hostSynchronize(uint64_t timeout) override {
        hostSynchronizeCalled++;
        return BaseClass::hostSynchronize(timeout);
    }

    ze_result_t queryStatus() override {
        if (failOnNextQueryStatus) {
            failOnNextQueryStatus = false;
            return ZE_RESULT_NOT_READY;
        }

        return BaseClass::queryStatus();
    }

    bool shouldHostEventSetValueFail = false;
    bool failOnNextQueryStatus = false;
    uint32_t assignKernelEventCompletionDataCounter = 0u;
    uint32_t hostSynchronizeCalled = 0;
};

TEST_F(EventTests, WhenQueryingStatusAfterHostSignalThenDontAccessMemoryAndReturnSuccess) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    auto result = event->hostSignal(false);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 0u);
}

TEST_F(EventTests, givenDebugFlagSetWhenCallingResetThenSynchronizeBeforeReset) {
    debugManager.flags.SynchronizeEventBeforeReset.set(1);

    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    event->failOnNextQueryStatus = true;

    *reinterpret_cast<uint32_t *>(event->hostAddressFromPool) = Event::STATE_SIGNALED;

    StreamCapture capture;
    capture.captureStdout();

    EXPECT_EQ(0u, event->hostSynchronizeCalled);

    event->reset();

    EXPECT_EQ(1u, event->hostSynchronizeCalled);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("");
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(EventTests, givenDebugFlagSetWhenCallingResetThenPrintLogAndSynchronizeBeforeReset) {
    debugManager.flags.SynchronizeEventBeforeReset.set(2);

    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    *reinterpret_cast<uint32_t *>(event->hostAddressFromPool) = Event::STATE_SIGNALED;

    {
        event->failOnNextQueryStatus = false;

        StreamCapture capture;
        capture.captureStdout();

        EXPECT_EQ(0u, event->hostSynchronizeCalled);

        event->reset();

        EXPECT_EQ(1u, event->hostSynchronizeCalled);

        std::string output = capture.getCapturedStdout();
        std::string expectedOutput("");
        EXPECT_EQ(expectedOutput, output);
    }

    {
        event->failOnNextQueryStatus = true;

        StreamCapture capture;
        capture.captureStdout();

        EXPECT_EQ(1u, event->hostSynchronizeCalled);

        event->reset();

        EXPECT_EQ(2u, event->hostSynchronizeCalled);

        std::string output = capture.getCapturedStdout();
        char expectedStr[128] = {};
        snprintf(expectedStr, 128, "\nzeEventHostReset: Event %p not ready. Calling zeEventHostSynchronize.", event.get());

        EXPECT_EQ(std::string(expectedStr), output);
    }
}

TEST_F(EventTests, whenAppendAdditionalCsrThenStoreUniqueCsr) {
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    auto csr2 = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    EXPECT_EQ(event->csrs.size(), 1u);

    event->appendAdditionalCsr(csr1.get());
    EXPECT_EQ(event->csrs.size(), 2u);

    event->appendAdditionalCsr(csr2.get());
    EXPECT_EQ(event->csrs.size(), 3u);

    event->appendAdditionalCsr(csr1.get());
    EXPECT_EQ(event->csrs.size(), 3u);

    event->destroy();
}

TEST_F(EventTests, WhenQueryingStatusAfterHostSignalThatFailedThenAccessMemoryAndReturnSuccess) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    event->shouldHostEventSetValueFail = true;
    event->hostSignal(false);
    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 1u);
}

HWTEST_F(EventTests, givenQwordPacketSizeWhenSignalingThenCopyQword) {
    using TimestampPacketType = typename FamilyType::TimestampPacketType;

    auto event = std::make_unique<MockEventCompletion<TimestampPacketType>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);

    auto completionAddress = static_cast<uint64_t *>(event->getCompletionFieldHostAddress());

    {
        event->setSinglePacketSize(sizeof(uint64_t));
        *completionAddress = std::numeric_limits<uint64_t>::max();

        event->hostSignal(false);

        EXPECT_EQ(static_cast<uint64_t>(Event::STATE_SIGNALED), *completionAddress);

        EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
    }

    {
        event->setSinglePacketSize(NEO::TimestampPackets<TimestampPacketType, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());

        *completionAddress = std::numeric_limits<uint64_t>::max();

        event->hostSignal(false);

        if (sizeof(TimestampPacketType) == sizeof(uint32_t)) {
            uint64_t expectedValue = static_cast<uint64_t>(Event::STATE_SIGNALED);

            EXPECT_EQ(expectedValue, *completionAddress);
            EXPECT_EQ(static_cast<uint32_t>(Event::STATE_SIGNALED), *static_cast<uint32_t *>(event->getCompletionFieldHostAddress()));
        } else if (sizeof(TimestampPacketType) == sizeof(uint64_t)) {
            EXPECT_EQ(static_cast<uint64_t>(Event::STATE_SIGNALED), *completionAddress);
        } else {
            ASSERT_TRUE(false);
        }

        EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
    }
}

TEST_F(EventTests, WhenQueryingStatusThenAccessMemoryOnce) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 1u);
}

TEST_F(EventTests, WhenQueryingStatusAfterResetThenAccessMemory) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->reset(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 2u);
}

TEST_F(EventTests, WhenResetEventThenZeroCpuTimestamps) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    event->gpuStartTimestamp = 10u;
    event->gpuEndTimestamp = 20u;
    EXPECT_EQ(event->reset(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->gpuStartTimestamp, 0u);
    EXPECT_EQ(event->gpuEndTimestamp, 0u);
}

TEST_F(EventTests, WhenEventResetIsCalledThenKernelCountAndPacketsUsedHaveNotBeenReset) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    event->gpuStartTimestamp = 10u;
    event->gpuEndTimestamp = 20u;
    event->zeroKernelCount();

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->reset());

    EXPECT_EQ(0u, event->getKernelCount());
    EXPECT_EQ(0u, event->getPacketsInUse());
    EXPECT_EQ(event->gpuStartTimestamp, 0u);
    EXPECT_EQ(event->gpuEndTimestamp, 0u);
}

TEST_F(EventTests, GivenResetAllPacketsWhenResetPacketsThenOneKernelCountAndOnePacketUsed) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    event->gpuStartTimestamp = 10u;
    event->gpuEndTimestamp = 20u;
    event->zeroKernelCount();
    auto resetAllPackets = true;
    event->resetPackets(resetAllPackets);

    EXPECT_EQ(1u, event->getKernelCount());
    EXPECT_EQ(1u, event->getPacketsInUse());
    EXPECT_EQ(event->gpuStartTimestamp, 0u);
    EXPECT_EQ(event->gpuEndTimestamp, 0u);
}

TEST_F(EventTests, GivenResetAllPacketsFalseWhenResetPacketsThenKernelCountAndPacketsUsedHaveNotBeenReset) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    event->gpuStartTimestamp = 10u;
    event->gpuEndTimestamp = 20u;
    event->zeroKernelCount();
    auto resetAllPackets = false;
    event->resetPackets(resetAllPackets);

    EXPECT_EQ(0u, event->getKernelCount());
    EXPECT_EQ(0u, event->getPacketsInUse());
    EXPECT_EQ(event->gpuStartTimestamp, 0u);
    EXPECT_EQ(event->gpuEndTimestamp, 0u);
}

TEST_F(EventTests, WhenSetNewKernelCountThenKernelCountGetReturnsCorrectValue) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    event->zeroKernelCount();
    EXPECT_EQ(0u, event->getKernelCount());

    event->setKernelCount(1);
    EXPECT_EQ(1u, event->getKernelCount());
}

TEST_F(EventTests, givenCallToEventQueryStatusWithKernelPointerReturnsCounter) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    Mock<Module> mockModule(this->device, nullptr);
    std::shared_ptr<Mock<KernelImp>> mockKernel{new Mock<KernelImp>{}};
    mockKernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel->module = &mockModule;

    event->setKernelForPrintf(std::weak_ptr<Kernel>{mockKernel});
    event->setKernelWithPrintfDeviceMutex(&static_cast<DeviceImp *>(this->device)->printfKernelMutex);
    EXPECT_FALSE(event->getKernelForPrintf().expired());
    EXPECT_NE(nullptr, event->getKernelWithPrintfDeviceMutex());

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    event->hostSynchronize(timeout);
    EXPECT_EQ(1u, mockKernel->printPrintfOutputCalledTimes);
}

TEST_F(EventTests, givenCallToEventQueryStatusWithNullKernelPointerReturnsCounter) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getMaxKernelCount(), eventPool->getEventMaxPackets(), 1u, device);
    Mock<Module> mockModule(this->device, nullptr);
    std::shared_ptr<Mock<KernelImp>> mockKernel{new Mock<KernelImp>{}};
    mockKernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel->module = &mockModule;

    event->resetKernelForPrintf();
    EXPECT_TRUE(event->getKernelForPrintf().expired());

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    event->hostSynchronize(timeout);
    EXPECT_EQ(0u, mockKernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(nullptr, event->getKernelWithPrintfDeviceMutex());
}

HWTEST_F(EventTests, givenSignalAllPacketsValueWhenGettingEventPacketToWaitThenReturnCorrectValue) {
    event->maxPacketCount = 4;

    event->signalAllEventPackets = true;
    EXPECT_EQ(4u, event->getPacketsToWait());

    event->signalAllEventPackets = false;
    EXPECT_EQ(event->getPacketsInUse(), event->getPacketsToWait());
}

TEST_F(EventSynchronizeTest, whenEventSetCsrThenCorrectCsrSet) {
    auto defaultCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    const auto mockCsr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    EXPECT_EQ(event->csrs[0], defaultCsr);
    event->setCsr(mockCsr.get(), false);
    EXPECT_EQ(event->csrs[0], mockCsr.get());

    event->reset();
    EXPECT_EQ(event->csrs[0], defaultCsr);
}

template <int32_t multiTile, int32_t signalRemainingPackets>
struct EventDynamicPacketUseFixture : public DeviceFixture {
    void setUp() {
        NEO::debugManager.flags.UseDynamicEventPacketsCount.set(1);
        if constexpr (multiTile == 1) {
            debugManager.flags.CreateMultipleSubDevices.set(2);
            debugManager.flags.EnableImplicitScaling.set(1);
        }
        NEO::debugManager.flags.SignalAllEventPackets.set(signalRemainingPackets);
        if constexpr (signalRemainingPackets == 1) {
            NEO::debugManager.flags.UsePipeControlMultiKernelEventSync.set(0);
            NEO::debugManager.flags.CompactL3FlushEventPacket.set(0);
        }
        DeviceFixture::setUp();
    }

    void testAllDevices() {
        auto &hwInfo = device->getHwInfo();
        auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        ze_event_pool_desc_t eventPoolDesc = {
            ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
            nullptr,
            0,
            1};

        ze_result_t result = ZE_RESULT_SUCCESS;
        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        auto expectedMaxKernelCount = driverHandle->getEventMaxKernelCount(0, nullptr);
        EXPECT_EQ(expectedMaxKernelCount, eventPool->getMaxKernelCount());

        auto eventPoolMaxPackets = eventPool->getEventMaxPackets();
        auto expectedPoolMaxPackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());
        if constexpr (multiTile == 1) {
            expectedPoolMaxPackets *= 2;
        }
        EXPECT_EQ(expectedPoolMaxPackets, eventPoolMaxPackets);

        auto eventSize = eventPool->getEventSize();
        auto expectedEventSize = static_cast<uint32_t>(alignUp(expectedPoolMaxPackets * gfxCoreHelper.getSingleTimestampPacketSize(), gfxCoreHelper.getTimestampPacketAllocatorAlignment()));
        EXPECT_EQ(expectedEventSize, eventSize);

        ze_event_desc_t eventDesc = {
            ZE_STRUCTURE_TYPE_EVENT_DESC,
            nullptr,
            0,
            ZE_EVENT_SCOPE_FLAG_DEVICE,
            ZE_EVENT_SCOPE_FLAG_DEVICE};

        std::unique_ptr<L0::Event> event(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));

        EXPECT_EQ(expectedPoolMaxPackets, event->getMaxPacketsCount());

        uint32_t maxKernels = l0GfxCoreHelper.getEventMaxKernelCount(hwInfo);
        EXPECT_EQ(expectedMaxKernelCount, maxKernels);
        EXPECT_EQ(maxKernels, event->getMaxKernelCount());
    }

    void testSingleDevice() {
        ze_result_t result = ZE_RESULT_SUCCESS;

        device->getNEODevice()->getExecutionEnvironment()->setDeviceHierarchyMode(DeviceHierarchyMode::composite);

        auto &hwInfo = device->getHwInfo();
        auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        ze_event_pool_desc_t eventPoolDesc = {
            ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
            nullptr,
            0,
            1};

        std::vector<ze_device_handle_t> deviceHandles;

        L0::Device *eventDevice = device;
        if constexpr (multiTile == 1) {
            uint32_t count = 2;
            ze_device_handle_t subDevices[2];
            result = device->getSubDevices(&count, subDevices);
            ASSERT_EQ(ZE_RESULT_SUCCESS, result);
            deviceHandles.push_back(subDevices[0]);
            eventDevice = Device::fromHandle(subDevices[0]);
        } else {
            deviceHandles.push_back(device->toHandle());
        }

        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 1, deviceHandles.data(), &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        auto expectedMaxKernelCount = driverHandle->getEventMaxKernelCount(1, deviceHandles.data());
        EXPECT_EQ(expectedMaxKernelCount, eventPool->getMaxKernelCount());

        auto eventPoolMaxPackets = eventPool->getEventMaxPackets();
        auto expectedPoolMaxPackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());

        EXPECT_EQ(expectedPoolMaxPackets, eventPoolMaxPackets);

        auto eventSize = eventPool->getEventSize();
        auto expectedEventSize = static_cast<uint32_t>(alignUp(expectedPoolMaxPackets * gfxCoreHelper.getSingleTimestampPacketSize(), gfxCoreHelper.getTimestampPacketAllocatorAlignment()));
        EXPECT_EQ(expectedEventSize, eventSize);

        ze_event_desc_t eventDesc = {
            ZE_STRUCTURE_TYPE_EVENT_DESC,
            nullptr,
            0,
            ZE_EVENT_SCOPE_FLAG_DEVICE,
            ZE_EVENT_SCOPE_FLAG_DEVICE};

        std::unique_ptr<L0::Event> event(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, eventDevice));

        EXPECT_EQ(expectedPoolMaxPackets, event->getMaxPacketsCount());

        uint32_t maxKernels = l0GfxCoreHelper.getEventMaxKernelCount(hwInfo);
        EXPECT_EQ(expectedMaxKernelCount, maxKernels);
        EXPECT_EQ(maxKernels, event->getMaxKernelCount());
    }

    void testSignalAllPackets(uint32_t eventValueAfterSignal, uint32_t queryRetAfterPartialReset, ze_event_pool_flags_t flags, bool signalAll) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

        ze_event_pool_desc_t eventPoolDesc = {
            ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
            nullptr,
            flags,
            1};

        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        auto eventPoolMaxPackets = eventPool->getEventMaxPackets();
        auto expectedPoolMaxPackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());

        EXPECT_EQ(expectedPoolMaxPackets, eventPoolMaxPackets);

        ze_event_desc_t eventDesc = {
            ZE_STRUCTURE_TYPE_EVENT_DESC,
            nullptr,
            0,
            ZE_EVENT_SCOPE_FLAG_DEVICE,
            ZE_EVENT_SCOPE_FLAG_DEVICE};

        std::unique_ptr<L0::Event> event(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
        EXPECT_EQ(expectedPoolMaxPackets, event->getMaxPacketsCount());

        // natively one packet is in use
        uint32_t usedPackets = event->getPacketsInUse();

        uint32_t maxPackets = event->getMaxPacketsCount();
        size_t packetSize = event->getSinglePacketSize();
        void *eventHostAddress = event->getHostAddress();
        uint32_t remainingPackets = maxPackets - usedPackets;
        void *remainingPacketsAddress = ptrOffset(eventHostAddress, (usedPackets * packetSize));
        if (event->isUsingContextEndOffset()) {
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, event->getContextEndOffset());
        }

        for (uint32_t i = 0; i < remainingPackets; i++) {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(remainingPacketsAddress);
            EXPECT_EQ(Event::STATE_INITIAL, *completionField);
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, packetSize);
        }

        result = event->queryStatus();
        EXPECT_EQ(ZE_RESULT_NOT_READY, result);
        event->resetCompletionStatus();

        event->hostSignal(false);

        remainingPacketsAddress = ptrOffset(eventHostAddress, (usedPackets * packetSize));
        if (event->isUsingContextEndOffset()) {
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, event->getContextEndOffset());
        }

        for (uint32_t i = 0; i < remainingPackets; i++) {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(remainingPacketsAddress);
            EXPECT_EQ(eventValueAfterSignal, *completionField);
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, packetSize);
        }

        result = event->queryStatus();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        event->resetCompletionStatus();

        remainingPacketsAddress = ptrOffset(eventHostAddress, (usedPackets * packetSize));
        if (event->isUsingContextEndOffset()) {
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, event->getContextEndOffset());
        }

        {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(remainingPacketsAddress);
            *completionField = Event::STATE_CLEARED;

            result = event->queryStatus();
            EXPECT_EQ(queryRetAfterPartialReset, result);
            event->resetCompletionStatus();

            *completionField = Event::STATE_SIGNALED;

            result = event->queryStatus();
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
            event->resetCompletionStatus();
        }

        event->reset();

        uint32_t eventValueAfterReset = Event::STATE_CLEARED;
        for (uint32_t i = 0; i < remainingPackets; i++) {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(remainingPacketsAddress);

            // manually signaled free packet will not be cleared when signal all is not active
            if (!signalAll && i == 0) {
                eventValueAfterReset = Event::STATE_SIGNALED;
            } else {
                eventValueAfterReset = Event::STATE_CLEARED;
            }
            EXPECT_EQ(eventValueAfterReset, *completionField);
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, packetSize);
        }

        result = event->queryStatus();
        EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    }

    DebugManagerStateRestore restorer;
};

using EventDynamicPacketUseTest = Test<EventDynamicPacketUseFixture<0, 0>>;
HWTEST_F(EventDynamicPacketUseTest, givenDynamicPacketEstimationWhenGettingMaxPacketFromAllDevicesThenMaxPossibleSelected) {
    testAllDevices();
}

HWTEST_F(EventDynamicPacketUseTest, givenDynamicPacketEstimationWhenGettingMaxPacketFromSingleDeviceThenMaxFromThisDeviceSelected) {
    testSingleDevice();
}

using EventMultiTileDynamicPacketUseTest = Test<EventDynamicPacketUseFixture<1, 0>>;
HWTEST2_F(EventMultiTileDynamicPacketUseTest, givenDynamicPacketEstimationWhenGettingMaxPacketFromAllDevicesThenMaxPossibleSelected, IsAtLeastXeCore) {
    testAllDevices();
}

HWTEST2_F(EventMultiTileDynamicPacketUseTest, givenEventUsedCreatedOnSubDeviceButUsedOnDifferentSubdeviceWhenQueryingThenDownload, IsAtLeastXeCore) {
    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    auto rootDevice = static_cast<MockDeviceImp *>(device);

    ASSERT_TRUE(rootDevice->subDevices.size() > 1);

    auto subDevice0 = rootDevice->subDevices[0];
    auto subDevice1 = rootDevice->subDevices[1];

    auto rootCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(rootDevice->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto ultCsr0 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice0->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto ultCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice1->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto ultCsr2 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice1->getNEODevice()->getInternalEngine().commandStreamReceiver);

    rootCsr->initializeResources(false, device->getDevicePreemptionMode());
    ultCsr0->initializeResources(false, device->getDevicePreemptionMode());
    ultCsr1->initializeResources(false, device->getDevicePreemptionMode());

    rootCsr->commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr0->commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr1->commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr2->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    ultCsr2->resourcesInitialized = false;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, subDevice1));

    size_t eventCompletionOffset = event->getContextStartOffset();
    if (event->isUsingContextEndOffset()) {
        eventCompletionOffset = event->getContextEndOffset();
    }
    TagAddressType *eventAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    *eventAddress = Event::STATE_INITIAL;

    uint32_t rootDownloadCounter = 0;
    uint32_t downloadCounter0 = 0;
    uint32_t downloadCounter1 = 0;
    uint32_t downloadCounter2 = 0;

    rootCsr->downloadAllocationImpl = [&rootDownloadCounter](GraphicsAllocation &gfxAllocation) {
        rootDownloadCounter++;
    };
    ultCsr0->downloadAllocationImpl = [&downloadCounter0](GraphicsAllocation &gfxAllocation) {
        downloadCounter0++;
    };
    ultCsr1->downloadAllocationImpl = [&downloadCounter1](GraphicsAllocation &gfxAllocation) {
        downloadCounter1++;
    };
    ultCsr2->downloadAllocationImpl = [&downloadCounter2](GraphicsAllocation &gfxAllocation) {
        downloadCounter2++;
    };

    auto eventAllocation = event->getAllocation(device);
    ultCsr0->makeResident(*eventAllocation);
    ultCsr2->makeResident(*eventAllocation);
    rootCsr->makeResident(*eventAllocation);

    auto hostAddress = static_cast<uint64_t *>(event->getCompletionFieldHostAddress());
    *hostAddress = Event::STATE_SIGNALED;

    event->hostSynchronize(1);

    EXPECT_EQ(1u, rootCsr->downloadAllocationsCalledCount);
    EXPECT_FALSE(rootCsr->latestDownloadAllocationsBlocking);
    EXPECT_EQ(0u, rootDownloadCounter);

    EXPECT_EQ(1u, ultCsr0->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr0->latestDownloadAllocationsBlocking);
    EXPECT_EQ(1u, downloadCounter0);

    EXPECT_EQ(1u, ultCsr1->downloadAllocationsCalledCount);
    EXPECT_TRUE(ultCsr1->latestDownloadAllocationsBlocking);
    EXPECT_EQ(0u, downloadCounter1);

    EXPECT_EQ(0u, ultCsr2->downloadAllocationsCalledCount);
    EXPECT_EQ(0u, downloadCounter2);

    event->destroy();
}

HWTEST2_F(EventMultiTileDynamicPacketUseTest, givenEventCounterBasedUsedCreatedOnSubDeviceButUsedOnDifferentSubdeviceWhenQueryingThenDownload, IsAtLeastXeCore) {
    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    auto rootDevice = static_cast<MockDeviceImp *>(device);

    ASSERT_TRUE(rootDevice->subDevices.size() > 1);

    auto subDevice0 = rootDevice->subDevices[0];
    auto subDevice1 = rootDevice->subDevices[1];

    auto ultCsr0 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice0->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto ultCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice1->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr0->initializeResources(false, device->getDevicePreemptionMode());
    ultCsr1->initializeResources(false, device->getDevicePreemptionMode());

    ultCsr0->commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr1->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 3;
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto event0 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, subDevice1));
    auto event1 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, subDevice1));
    auto event2 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, subDevice1));
    event0->eventPoolAllocation = nullptr;
    event1->eventPoolAllocation = nullptr;
    event2->eventPoolAllocation = nullptr;

    auto inOrderExecInfo0 = NEO::InOrderExecInfo::create(device->getDeviceInOrderCounterAllocator()->getTag(), nullptr, *device->getNEODevice(), 1, false);
    inOrderExecInfo0->setLastWaitedCounterValue(1);
    event0->updateInOrderExecState(inOrderExecInfo0, 1, 0);

    uint64_t counter = 2;
    auto inOrderExecInfo1 = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), nullptr, 0x1, nullptr, &counter, 1, 1, 1);
    inOrderExecInfo1->setLastWaitedCounterValue(1);
    event1->updateInOrderExecState(inOrderExecInfo1, 1, 0);

    MockGraphicsAllocation mockAlloc(rootDeviceIndex, nullptr, 1);
    auto inOrderExecInfo2 = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), &mockAlloc, 0x1, &mockAlloc, &counter, 1, 1, 1);
    event2->updateInOrderExecState(inOrderExecInfo2, 1, 0);

    ultCsr0->makeResident(*inOrderExecInfo0->getDeviceCounterAllocation());

    event0->hostSynchronize(1);

    EXPECT_EQ(1u, ultCsr0->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr0->latestDownloadAllocationsBlocking);

    EXPECT_EQ(1u, ultCsr1->downloadAllocationsCalledCount);
    EXPECT_TRUE(ultCsr1->latestDownloadAllocationsBlocking);

    event1->hostSynchronize(1);

    EXPECT_EQ(2u, ultCsr0->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr0->latestDownloadAllocationsBlocking);

    EXPECT_EQ(3u, ultCsr1->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr1->latestDownloadAllocationsBlocking);

    event2->hostSynchronize(1);

    EXPECT_EQ(2u, ultCsr0->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr0->latestDownloadAllocationsBlocking);

    EXPECT_EQ(4u, ultCsr1->downloadAllocationsCalledCount);
    EXPECT_TRUE(ultCsr1->latestDownloadAllocationsBlocking);

    event0->destroy();
    event1->destroy();
    event2->destroy();
}

HWTEST2_F(EventMultiTileDynamicPacketUseTest, givenDynamicPacketEstimationWhenGettingMaxPacketFromSingleOneTileDeviceThenMaxFromThisDeviceSelected, IsAtLeastXeCore) {
    testSingleDevice();
}

using EventSignalAllPacketsTest = Test<EventDynamicPacketUseFixture<0, 1>>;
HWTEST2_F(EventSignalAllPacketsTest, givenDynamicPacketEstimationWhenImmediateEventSignalMaxPacketThenAllPacketCompletionSignaled, IsAtLeastXeCore) {
    testSignalAllPackets(Event::STATE_SIGNALED, ZE_RESULT_NOT_READY, 0, true);
}

HWTEST2_F(EventSignalAllPacketsTest, givenDynamicPacketEstimationWhenTimestampEventSignalMaxPacketThenAllPacketCompletionSignaled, IsAtLeastXeCore) {
    testSignalAllPackets(Event::STATE_SIGNALED, ZE_RESULT_NOT_READY, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, true);
}

using EventSignalUsedPacketsTest = Test<EventDynamicPacketUseFixture<0, 0>>;
HWTEST2_F(EventSignalUsedPacketsTest, givenDynamicPacketEstimationWhenImmediateSignalUsedPacketThenUsedPacketCompletionSignaled, IsAtLeastXeCore) {
    testSignalAllPackets(Event::STATE_CLEARED, ZE_RESULT_SUCCESS, 0, false);
}

HWTEST2_F(EventSignalUsedPacketsTest, givenDynamicPacketEstimationWhenTimestampSignalUsedPacketThenUsedPacketCompletionSignaled, IsAtLeastXeCore) {
    testSignalAllPackets(Event::STATE_CLEARED, ZE_RESULT_SUCCESS, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, false);
}

void testQueryAllPackets(L0::Event *event, bool singlePacket) {
    auto result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    uint32_t usedPackets = event->getPacketsInUse();

    uint32_t maxPackets = event->getMaxPacketsCount();
    size_t packetSize = event->getSinglePacketSize();
    void *firstPacketAddress = event->getCompletionFieldHostAddress();
    void *eventHostAddress = firstPacketAddress;
    for (uint32_t i = 0; i < usedPackets; i++) {
        uint32_t *completionField = reinterpret_cast<uint32_t *>(eventHostAddress);
        EXPECT_EQ(Event::STATE_INITIAL, *completionField);
        *completionField = Event::STATE_SIGNALED;
        eventHostAddress = ptrOffset(eventHostAddress, packetSize);
    }

    if (singlePacket) {
        EXPECT_EQ(maxPackets, usedPackets);
    } else {
        result = event->queryStatus();
        EXPECT_EQ(ZE_RESULT_NOT_READY, result);

        ASSERT_LT(usedPackets, maxPackets);

        uint32_t remainingPackets = maxPackets - usedPackets;
        for (uint32_t i = 0; i < remainingPackets; i++) {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(eventHostAddress);
            EXPECT_EQ(Event::STATE_INITIAL, *completionField);
            *completionField = Event::STATE_SIGNALED;
            eventHostAddress = ptrOffset(eventHostAddress, packetSize);
        }
    }

    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = event->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventHostAddress = firstPacketAddress;
    for (uint32_t i = 0; i < maxPackets; i++) {
        uint32_t *completionField = reinterpret_cast<uint32_t *>(eventHostAddress);
        EXPECT_EQ(Event::STATE_INITIAL, *completionField);
        eventHostAddress = ptrOffset(eventHostAddress, packetSize);
    }

    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    result = event->hostSignal(false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventHostAddress = firstPacketAddress;
    for (uint32_t i = 0; i < maxPackets; i++) {
        uint32_t *completionField = reinterpret_cast<uint32_t *>(eventHostAddress);
        EXPECT_EQ(Event::STATE_SIGNALED, *completionField);
        eventHostAddress = ptrOffset(eventHostAddress, packetSize);
    }

    result = event->queryStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using TimestampEventAllPacketSignalMultiPacketUseTest = Test<EventUsedPacketSignalFixture<1, 1, 1, 0>>;
HWTEST2_F(TimestampEventAllPacketSignalMultiPacketUseTest,
          givenSignalAllEventPacketWhenQueryingAndSignalingTimestampEventThenUseEventMaxPackets,
          IsAtLeastXeCore) {
    testQueryAllPackets(event.get(), false);
}

using ImmediateEventAllPacketSignalMultiPacketUseTest = Test<EventUsedPacketSignalFixture<1, 0, 1, 0>>;
HWTEST2_F(ImmediateEventAllPacketSignalMultiPacketUseTest,
          givenSignalAllEventPacketWhenQueryingAndSignalingImmediateEventThenUseEventMaxPackets,
          IsAtLeastXeCore) {
    testQueryAllPackets(event.get(), false);
}

using TimestampEventAllPacketSignalSinglePacketUseTest = Test<EventUsedPacketSignalFixture<1, 1, 1, 1>>;
TEST_F(TimestampEventAllPacketSignalSinglePacketUseTest, givenSignalAllEventPacketWhenQueryingAndSignalingTimestampEventThenUseEventMaxPackets) {
    testQueryAllPackets(event.get(), true);
}

using ImmediateEventAllPacketSignalSinglePacketUseTest = Test<EventUsedPacketSignalFixture<1, 0, 1, 1>>;
TEST_F(ImmediateEventAllPacketSignalSinglePacketUseTest, givenSignalAllEventPacketWhenQueryingAndSignalingImmediateEventThenUseEventMaxPackets) {
    testQueryAllPackets(event.get(), true);
}

struct LocalMemoryEnabledDeviceFixture : public DeviceFixture {
    void setUp() {
        debugManager.flags.EnableLocalMemory.set(1);
        DeviceFixture::setUp();
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore restore;
};

using EventTimestampTest = Test<LocalMemoryEnabledDeviceFixture>;
HWTEST2_F(EventTimestampTest, givenAppendMemoryCopyIsCalledWhenCpuCopyIsUsedAndCopyTimeIsLessThanDeviceTimestampResolutionThenReturnTimstampDifferenceAsOne, IsXeHpcCore) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    neoDevice->setOSTime(new NEO::MockOSTimeWithConstTimestamp());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device));
    constexpr uint32_t copySize = 1024;
    auto hostPtr = new char[copySize];
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *devicePtr;
    context->allocDeviceMem(device->toHandle(), &deviceDesc, copySize, 1u, &devicePtr);
    CmdListMemoryCopyParams copyParams = {};
    cmdList.appendMemoryCopy(devicePtr, hostPtr, copySize, event->toHandle(), 0, nullptr, copyParams);

    ze_kernel_timestamp_result_t result = {};
    event->queryKernelTimestamp(&result);
    EXPECT_EQ(result.context.kernelEnd - result.context.kernelStart, 1u);

    delete[] hostPtr;
    context->freeMem(devicePtr);
}

TEST_F(EventTests, givenNullDescriptorWhenCreatingCbEvent2ThenEventWithNoProfilingAndSignalScopeHostAndDeviceScopeWaitIsCreated) {
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, nullptr, &handle));

    auto eventObj = Event::fromHandle(handle);
    EXPECT_TRUE(eventObj->isCounterBasedExplicitlyEnabled());
    EXPECT_FALSE(eventObj->isIpcImported());
    EXPECT_FALSE(eventObj->isEventTimestampFlagSet());
    EXPECT_TRUE(eventObj->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));
    EXPECT_TRUE(eventObj->isWaitScope(ZE_EVENT_SCOPE_FLAG_DEVICE));
    EXPECT_EQ(static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE), eventObj->getCounterBasedFlags());
    zeEventDestroy(handle);
}

} // namespace ult
} // namespace L0
