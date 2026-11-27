/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_allocations_list.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/mock_timestamp_packet.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/event/event_host_synchronize_wait.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture_extended.h"
#include "level_zero/core/test/unit_tests/fixtures/event_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"
#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern std::atomic<uint32_t> yieldCounter;
extern volatile TagAddressType *pauseAddress;
extern TaskCountType pauseValue;
extern uint32_t pauseOffset;
extern std::function<void()> setupPauseAddress;
extern std::atomic_uint32_t tpauseCounter;
extern std::atomic<uint64_t> lastTpauseCounter;
extern uint64_t rdtscRetValue;
extern std::function<void()> controlTpause;
} // namespace CpuIntrinsicsTests

namespace L0 {
namespace ult {

using EventSynchronizeTest = Test<EventFixture<1, 0>>;
using EventTests = Test<EventFixture<1, 0>>;

using EventPoolCreate = Test<DeviceFixture>;
using EventCreate = Test<DeviceFixture>;

class MemoryManagerEventPoolFailMock : public NEO::MemoryManager {
  public:
    MemoryManagerEventPoolFailMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MemoryManager(executionEnvironment) {}
    void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, const AllocationProperties &properties, NEO::MultiGraphicsAllocation &multiGraphicsAllocation) override {
        return nullptr;
    };
    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
    void addAllocationToHostPtrManager(NEO::GraphicsAllocation *memory) override {};
    void removeAllocationFromHostPtrManager(NEO::GraphicsAllocation *memory) override {};
    AllocationStatus populateOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
    void cleanOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override {};
    void freeGraphicsMemoryImpl(NEO::GraphicsAllocation *gfxAllocation) override {};
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override {};
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override { return 0; };
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    uint64_t getCurrentUsedLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    uint64_t getCurrentUsedSystemSharedMemorySize(uint32_t rootDeviceIndex) override { return 0; };
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
    AddressRange reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override {
        return {};
    }
    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override {
        return {};
    }
    size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override {
        return selectAlignmentAndHeap(0ull, size, heap);
    }
    size_t selectAlignmentAndHeap(const uint64_t requiredStartAddress, size_t size, HeapIndex *heap) override {
        // Always default to HEAP STANDARD and page size 64KB
        *heap = HeapIndex::heapStandard;
        size_t pageSizeAlignment = MemoryConstants::pageSize64k;

        // If the user provides a start address, we try to find the heap and page size alignment based on that address.
        if (requiredStartAddress != 0ull) {
            auto rootDeviceIndex = 0u;
            auto gfxPartition = getGfxPartition(rootDeviceIndex);
            if (gfxPartition->getHeapIndexAndPageSizeBasedOnAddress(requiredStartAddress, *heap, pageSizeAlignment)) {
                return pageSizeAlignment;
            }
        }
        return pageSizeAlignment;
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override {};
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override { return {}; }
    void freeCpuAddress(AddressRange addressRange) override {};
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
    bool unMapPhysicalDeviceMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) override { return false; };
    bool unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, bool isSvmReservation) override { return false; };
    bool mapPhysicalDeviceMemoryToVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, const MemoryFlags *memoryflags, size_t offset) override { return false; };
    bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, size_t offset, bool isSvmReservation) override { return false; };

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override {};
};

struct EventPoolFailTests : public ::testing::Test {
    void SetUp() override {

        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandle>();
        driverHandle->initialize(std::move(devices));

        prevMemoryManagerDriver = driverHandle->getMemoryManager();
        prevMemoryManagerExecEnv = neoDevice->executionEnvironment->memoryManager.release();

        currMemoryManager = new MemoryManagerEventPoolFailMock(*neoDevice->executionEnvironment);

        driverHandle->setMemoryManager(currMemoryManager);
        neoDevice->executionEnvironment->memoryManager.reset(currMemoryManager);

        device = driverHandle->devices[0];

        context = std::make_unique<Context>(driverHandle.get());
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

    std::unique_ptr<DriverHandle> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<Context> context;
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

TEST_F(EventPoolCreate, givenZeroEventCountWhenCreatingEventPoolThenInvalidSizeIsReturned) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        0,
        0};

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);
    EXPECT_EQ(nullptr, eventPool);
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

    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    auto &gfxCoreHelper = device->getGfxCoreHelper();

    uint32_t maxPacketCount = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());

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

    int peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle, void *reservedHandleData) override {
        handle = peekInternalHandleValue;
        if (reservedHandleData != nullptr) {
            memset(reservedHandleData, 0xab, reservedHandleDataSize);
        }
        return peekInternalHandleRetCode;
    }

    int peekInternalHandleRetCode = 0;
    uint64_t peekInternalHandleValue = 0;
    size_t reservedHandleDataSize = 32;
};
class MemoryManagerEventPoolIpcMock : public NEO::MockMemoryManager {
  public:
    MemoryManagerEventPoolIpcMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(executionEnvironment) {}
    void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, const AllocationProperties &properties, NEO::MultiGraphicsAllocation &multiGraphicsAllocation) override {
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

class MemoryManagerEventPoolIpcReleaseMock : public MemoryManagerEventPoolIpcMock {
  public:
    using MemoryManagerEventPoolIpcMock::MemoryManagerEventPoolIpcMock;

    void closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) override {
        closeInternalHandleCalled++;
        lastClosedHandle = handle;
        lastHandleId = handleId;
        lastGraphicsAllocation = graphicsAllocation;
    }

    uint32_t closeInternalHandleCalled = 0;
    uint64_t lastClosedHandle = 0;
    uint32_t lastHandleId = 0;
    GraphicsAllocation *lastGraphicsAllocation = nullptr;
};

class MemoryManagerEventPoolIpcReservedHandleMock : public MemoryManagerEventPoolIpcReleaseMock {
  public:
    using MemoryManagerEventPoolIpcReleaseMock::MemoryManagerEventPoolIpcReleaseMock;

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        createFromSharedHandleCalled++;
        if (failPrimaryImport && createFromSharedHandleCalled == 1) {
            return nullptr;
        }
        if (failFallbackImport && createFromSharedHandleCalled == 2) {
            return nullptr;
        }
        return MemoryManagerEventPoolIpcMock::createGraphicsAllocationFromSharedHandle(osHandleData, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation, mapPointer);
    }

    int getImportHandleFromReservedHandleData(void *reservedHandleData, uint32_t rootDeviceIndex) override {
        getImportHandleFromReservedHandleDataCalled++;
        lastReservedHandleData = reservedHandleData;
        lastRootDeviceIndex = rootDeviceIndex;
        return importHandleFromReservedHandleDataResult;
    }

    bool failPrimaryImport = false;
    bool failFallbackImport = false;
    uint32_t createFromSharedHandleCalled = 0;
    uint32_t getImportHandleFromReservedHandleDataCalled = 0;
    void *lastReservedHandleData = nullptr;
    uint32_t lastRootDeviceIndex = 0;
    int importHandleFromReservedHandleDataResult = -1;
};

class ContextWhiteboxForEventPoolIpcTesting : public ::L0::Context {
  public:
    ContextWhiteboxForEventPoolIpcTesting(L0::DriverHandle *driverHandle) : ::L0::Context(driverHandle) {}

    using ::L0::Context::releaseIpcEventPoolHandle;
    using ::L0::Context::trackIpcEventPoolHandle;
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

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle.val);
    EXPECT_EQ(numEvents, ipcHandleData.numEvents);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenTrackedIpcEventPoolHandleWhenTrackingSameHandleTwiceThenRefCountIsIncremented) {
    ContextWhiteboxForEventPoolIpcTesting contextWhitebox(driverHandle.get());
    NEO::MockGraphicsAllocation mockAllocation;
    constexpr uint64_t handle = 0x12345;

    EXPECT_TRUE(driverHandle->getIPCHandleMap().empty());

    contextWhitebox.trackIpcEventPoolHandle(handle, &mockAllocation);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    ASSERT_EQ(1u, ipcHandleMap.size());
    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());
    EXPECT_EQ(1u, handleIterator->second->refcnt);
    EXPECT_EQ(&mockAllocation, handleIterator->second->alloc);

    contextWhitebox.trackIpcEventPoolHandle(handle, &mockAllocation);

    EXPECT_EQ(1u, ipcHandleMap.size());
    handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());
    EXPECT_EQ(2u, handleIterator->second->refcnt);
    EXPECT_EQ(&mockAllocation, handleIterator->second->alloc);

    delete handleIterator->second;
    ipcHandleMap.clear();
}

TEST_F(EventPoolIPCHandleTests, givenTrackedIpcEventPoolHandleWhenReleasedToZeroThenHandleIsClosedRemovedAndMissingReleaseIsNoOp) {
    MemoryManagerEventPoolIpcReleaseMock mockMemoryManager(*neoDevice->executionEnvironment);
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle->memoryManager, &mockMemoryManager);

    ContextWhiteboxForEventPoolIpcTesting contextWhitebox(driverHandle.get());
    NEO::MockGraphicsAllocation mockAllocation;
    constexpr uint64_t handle = 0xABCDE;

    contextWhitebox.trackIpcEventPoolHandle(handle, &mockAllocation);
    contextWhitebox.trackIpcEventPoolHandle(handle, &mockAllocation);

    EXPECT_EQ(0u, mockMemoryManager.closeInternalHandleCalled);

    contextWhitebox.releaseIpcEventPoolHandle(handle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    ASSERT_EQ(1u, ipcHandleMap.size());
    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());
    EXPECT_EQ(1u, handleIterator->second->refcnt);
    EXPECT_EQ(0u, mockMemoryManager.closeInternalHandleCalled);

    contextWhitebox.releaseIpcEventPoolHandle(handle);

    EXPECT_TRUE(ipcHandleMap.empty());
    EXPECT_EQ(1u, mockMemoryManager.closeInternalHandleCalled);
    EXPECT_EQ(handle, mockMemoryManager.lastClosedHandle);
    EXPECT_EQ(0u, mockMemoryManager.lastHandleId);
    EXPECT_EQ(&mockAllocation, mockMemoryManager.lastGraphicsAllocation);

    contextWhitebox.releaseIpcEventPoolHandle(handle);
    EXPECT_TRUE(ipcHandleMap.empty());
    EXPECT_EQ(1u, mockMemoryManager.closeInternalHandleCalled);
}

TEST_F(EventPoolIPCHandleTests, givenGetIpcHandleInNonOpaqueModeWhenPuttingIpcHandleThenHandleIsReleasedFromMap) {
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

    uint8_t useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = OpaqueHandlingType::none;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, driverHandle->getIPCHandleMap().size());

    res = context->putIpcEventPoolHandle(ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    context->settings.useOpaqueHandle = useOpaque;

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenGetIpcHandleInOpaqueFdModeWhenPuttingIpcHandleThenHandleIsReleasedFromMap) {
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

    uint8_t useOpaque = context->settings.useOpaqueHandle;
    IpcHandleType handleType = context->settings.handleType;
    context->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    context->settings.handleType = IpcHandleType::fdHandle;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, driverHandle->getIPCHandleMap().size());

    res = context->putIpcEventPoolHandle(ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    context->settings.useOpaqueHandle = useOpaque;
    context->settings.handleType = handleType;

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenGetIpcHandleInOpaqueSocketsModeWithoutFdWhenPuttingIpcHandleThenHandleIsReleasedFromMap) {
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

    uint8_t useOpaque = context->settings.useOpaqueHandle;
    IpcHandleType handleType = context->settings.handleType;
    context->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    context->settings.handleType = IpcHandleType::ntHandle;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, driverHandle->getIPCHandleMap().size());

    res = context->putIpcEventPoolHandle(ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    context->settings.useOpaqueHandle = useOpaque;
    context->settings.handleType = handleType;

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenMultipleGetIpcHandleCallsWhenPuttingIpcHandleThenHandleIsReleasedOnLastPut) {
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

    uint8_t useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = OpaqueHandlingType::none;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->getIpcHandle(&ipcHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->getIpcHandle(&ipcHandle));
    EXPECT_EQ(1u, driverHandle->getIPCHandleMap().size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->putIpcEventPoolHandle(ipcHandle));
    EXPECT_EQ(1u, driverHandle->getIPCHandleMap().size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->putIpcEventPoolHandle(ipcHandle));
    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    context->settings.useOpaqueHandle = useOpaque;

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->destroy());
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenUnknownIpcHandleWhenPuttingIpcHandleThenSuccessIsReturned) {
    uint8_t useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = OpaqueHandlingType::none;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    ipcHandleData.handle.val = 0xdeadbeef;

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->putIpcEventPoolHandle(ipcHandle));
    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    context->settings.useOpaqueHandle = useOpaque;
}

TEST_F(EventPoolIPCHandleTests, givenGetIpcHandleWhenDestroyingEventPoolWithoutPutThenHandleIsReleasedFromMap) {
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
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->getIpcHandle(&ipcHandle));
    EXPECT_EQ(1u, driverHandle->getIPCHandleMap().size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->destroy());
    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenOpaqueSocketsFdModeWhenReleasingIpcEventPoolHandleThenHandleIsReleasedFromMap) {
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

    uint8_t useOpaque = context->settings.useOpaqueHandle;
    IpcHandleType handleType = context->settings.handleType;
    context->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    context->settings.handleType = IpcHandleType::fdHandle;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->getIpcHandle(&ipcHandle));
    EXPECT_EQ(1u, driverHandle->getIPCHandleMap().size());

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    context->releaseIpcEventPoolHandle(ipcHandleData.handle.val);
    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    context->settings.useOpaqueHandle = useOpaque;
    context->settings.handleType = handleType;

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->destroy());
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenOpaqueSocketsModeWithoutFdWhenReleasingIpcEventPoolHandleThenHandleIsReleasedFromMap) {
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

    uint8_t useOpaque = context->settings.useOpaqueHandle;
    IpcHandleType handleType = context->settings.handleType;
    context->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    context->settings.handleType = IpcHandleType::ntHandle;

    ze_ipc_event_pool_handle_t ipcHandle = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->getIpcHandle(&ipcHandle));
    EXPECT_EQ(1u, driverHandle->getIPCHandleMap().size());

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    context->releaseIpcEventPoolHandle(ipcHandleData.handle.val);
    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    context->settings.useOpaqueHandle = useOpaque;
    context->settings.handleType = handleType;

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->destroy());
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, givenExportedIpcHandleAndNullContextWhenDestroyingEventPoolThenNoReleaseIsAttempted) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    eventPool->hasExportedIpcHandle = true;
    eventPool->exportedIpcHandle = 0x1234;
    eventPool->context = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->destroy());
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

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle.val);

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

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle.val);

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

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle.val);

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

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t expectedHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(expectedHandle, ipcHandleData.handle.val);

    EXPECT_EQ(numEvents, ipcHandleData.numEvents);

    EXPECT_TRUE(ipcHandleData.isDeviceEventPoolAllocation);

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    delete mockMemoryManager;
    driverHandle->setMemoryManager(curMemoryManager);
}

TEST_F(EventPoolIPCHandleTests, whenGettingOpaqueEventPoolIpcHandleWithDeviceAllocThenReturnsHandleAndProcessID) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    // Enable opaque IPC handles
    context->settings.useOpaqueHandle = OpaqueHandlingType::pidfd;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *memManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(memManager);
    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    EXPECT_TRUE(eventPool->isDeviceEventPoolAllocation);

    auto allocation = &eventPool->getAllocation();

    EXPECT_EQ(allocation->getAllocationType(), NEO::AllocationType::gpuTimestampDeviceBuffer);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t badHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(badHandle, ipcData.handle.val);
    EXPECT_EQ(numEvents, ipcData.numEvents);
    EXPECT_EQ(NEO::SysCalls::getCurrentProcessId(), ipcData.processId);

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
    delete memManager;
    driverHandle->setMemoryManager(curMemManager);
}

TEST_F(EventPoolIPCHandleTests, whenGettingNonOpaqueIpcEventPoolHandleWithDeviceAllocThenReturnsHandleButNotProcessID) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    // Set to non-opaque IPC handles.
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = OpaqueHandlingType::none;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *memManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(memManager);
    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &poolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    EXPECT_TRUE(eventPool->isDeviceEventPoolAllocation);

    auto allocation = &eventPool->getAllocation();

    EXPECT_EQ(allocation->getAllocationType(), NEO::AllocationType::gpuTimestampDeviceBuffer);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t badHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(ipcData.handle.val, badHandle);
    EXPECT_EQ(ipcData.numEvents, numEvents);
    EXPECT_NE(ipcData.processId, NEO::SysCalls::getCurrentProcessId());
    EXPECT_EQ(ipcData.processId, 0u);

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
    delete memManager;
    driverHandle->setMemoryManager(curMemManager);
}

TEST_F(EventPoolIPCHandleTests, whenOpeningOpaqueIpcEventPoolHandleThenEventPoolIsCreatedAndProcessIDsAreTheSame) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    // Enable opaque IPC handles
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = OpaqueHandlingType::pidfd;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *memManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(memManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &poolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t badHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(ipcData.handle.val, badHandle);
    EXPECT_EQ(ipcData.numEvents, numEvents);
    EXPECT_EQ(ipcData.processId, NEO::SysCalls::getCurrentProcessId());

    ze_event_pool_handle_t ipcPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    auto ipcPool = L0::EventPool::fromHandle(ipcPoolHandle);

    EXPECT_EQ(ipcPool->getEventSize(), eventPool->getEventSize());
    EXPECT_EQ(static_cast<uint32_t>(ipcPool->getNumEvents()), numEvents);
    EXPECT_TRUE(ipcPool->isEventPoolTimestampFlagSet());

    EXPECT_EQ(ipcPool->closeIpcHandle(), ZE_RESULT_SUCCESS);

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
    delete memManager;
    driverHandle->setMemoryManager(curMemManager);
}

struct FabricAccessEventPoolIpcFixture {
    void setUp() {
        deviceFactory = std::make_unique<NEO::UltDeviceFactory>(1, 0);
        NEO::DeviceVector inputDevices;
        inputDevices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[0]));

        for (auto *devicePtr : driverHandle.devices) {
            delete devicePtr;
        }
        driverHandle.devices.clear();

        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle.initialize(std::move(inputDevices)));

        ze_context_handle_t hContext = nullptr;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle.createContext(&desc, 0u, nullptr, &hContext));
        context = Context::fromHandle(hContext);

        device = driverHandle.devices[0];
        neoDevice = device->getNEODevice();
    }

    void tearDown() {
        context->destroy();
    }

    std::unique_ptr<NEO::UltDeviceFactory> deviceFactory;
    Mock<DriverHandle> driverHandle;
    Context *context = nullptr;
    L0::Device *device = nullptr;
    NEO::Device *neoDevice = nullptr;
};

using EventPoolIpcFabricAccessTests = Test<FabricAccessEventPoolIpcFixture>;

TEST_F(EventPoolIpcFabricAccessTests, whenGettingOpaqueEventPoolIpcHandleAndFabricAccessIsSupportedThenReservedHandleDataIsCaptured) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    context->settings.useOpaqueHandle = OpaqueHandlingType::pidfd;
    driverHandle.isFabricAccessSupportedResult = true;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    MemoryManagerEventPoolIpcMock memManager(*neoDevice->getExecutionEnvironment());
    memManager.callParentAllocateGraphicsMemoryWithProperties = false;
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle.memoryManager, &memManager);
    auto eventPool = EventPool::create(&driverHandle, context, 1, &deviceHandle, &poolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    static const uint8_t emptyReservedHandleData[sizeof(ipcData.reservedHandleData)] = {0};
    EXPECT_NE(0, std::memcmp(ipcData.reservedHandleData, emptyReservedHandleData, sizeof(emptyReservedHandleData)));

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolIpcFabricAccessTests, whenGettingOpaqueEventPoolIpcHandleAndFabricAccessIsNotSupportedThenReservedHandleDataIsNotCaptured) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    context->settings.useOpaqueHandle = OpaqueHandlingType::pidfd;
    driverHandle.isFabricAccessSupportedResult = false;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    MemoryManagerEventPoolIpcMock memManager(*neoDevice->getExecutionEnvironment());
    memManager.callParentAllocateGraphicsMemoryWithProperties = false;
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle.memoryManager, &memManager);
    auto eventPool = EventPool::create(&driverHandle, context, 1, &deviceHandle, &poolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    static const uint8_t emptyReservedHandleData[sizeof(ipcData.reservedHandleData)] = {0};
    EXPECT_EQ(0, std::memcmp(ipcData.reservedHandleData, emptyReservedHandleData, sizeof(emptyReservedHandleData)));

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolIpcFabricAccessTests, whenOpeningOpaqueIpcEventPoolHandleAndPrimaryImportFailsThenReservedHandleDataFallbackIsUsed) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    context->settings.useOpaqueHandle = OpaqueHandlingType::pidfd;
    driverHandle.isFabricAccessSupportedResult = true;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    MemoryManagerEventPoolIpcReservedHandleMock memManager(*neoDevice->getExecutionEnvironment());
    memManager.callParentAllocateGraphicsMemoryWithProperties = false;
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle.memoryManager, &memManager);
    auto eventPool = EventPool::create(&driverHandle, context, 1, &deviceHandle, &poolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    memManager.failPrimaryImport = true;
    memManager.importHandleFromReservedHandleDataResult = 123;

    ze_event_pool_handle_t ipcPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_EQ(2u, memManager.createFromSharedHandleCalled);
    EXPECT_EQ(1u, memManager.getImportHandleFromReservedHandleDataCalled);
    EXPECT_NE(nullptr, memManager.lastReservedHandleData);

    auto ipcPool = L0::EventPool::fromHandle(ipcPoolHandle);
    EXPECT_EQ(ipcPool->closeIpcHandle(), ZE_RESULT_SUCCESS);

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolIpcFabricAccessTests, whenOpeningOpaqueIpcDeviceEventPoolHandleAndReservedHandleDataFallbackImportFailsThenFallbackFdIsClosed) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    context->settings.useOpaqueHandle = OpaqueHandlingType::pidfd;
    driverHandle.isFabricAccessSupportedResult = true;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    MemoryManagerEventPoolIpcReservedHandleMock memManager(*neoDevice->getExecutionEnvironment());
    memManager.callParentAllocateGraphicsMemoryWithProperties = false;
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle.memoryManager, &memManager);
    auto eventPool = EventPool::create(&driverHandle, context, 1, &deviceHandle, &poolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    ipcData.isHostVisibleEventPoolAllocation = false;

    memManager.failPrimaryImport = true;
    memManager.failFallbackImport = true;
    memManager.importHandleFromReservedHandleDataResult = 123;

    ze_event_pool_handle_t ipcPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcPoolHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);

    EXPECT_EQ(2u, memManager.createFromSharedHandleCalled);
    EXPECT_EQ(1u, memManager.getImportHandleFromReservedHandleDataCalled);
    EXPECT_EQ(1u, memManager.closeInternalHandleCalled);
    EXPECT_EQ(123u, memManager.lastClosedHandle);
    EXPECT_EQ(nullptr, memManager.lastGraphicsAllocation);

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolIpcFabricAccessTests, whenOpeningOpaqueIpcHostVisibleEventPoolHandleAndReservedHandleDataFallbackImportFailsThenFallbackFdIsNotClosedAgain) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    context->settings.useOpaqueHandle = OpaqueHandlingType::pidfd;
    driverHandle.isFabricAccessSupportedResult = true;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    MemoryManagerEventPoolIpcReservedHandleMock memManager(*neoDevice->getExecutionEnvironment());
    memManager.callParentAllocateGraphicsMemoryWithProperties = false;
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle.memoryManager, &memManager);
    auto eventPool = EventPool::create(&driverHandle, context, 1, &deviceHandle, &poolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    ipcData.isHostVisibleEventPoolAllocation = true;

    memManager.failPrimaryImport = true;
    memManager.failFallbackImport = true;
    memManager.importHandleFromReservedHandleDataResult = 123;

    ze_event_pool_handle_t ipcPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcPoolHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);

    EXPECT_EQ(2u, memManager.createFromSharedHandleCalled);
    EXPECT_EQ(1u, memManager.getImportHandleFromReservedHandleDataCalled);
    EXPECT_EQ(0u, memManager.closeInternalHandleCalled);

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolIpcFabricAccessTests, whenDestroyingImportedOpaqueEventPoolThenOpaqueHandleImportCacheRefIsReleased) {
    constexpr uint64_t cacheId = 0x1234;
    auto &cacheEntry = driverHandle.opaqueHandleImportCache[cacheId];
    cacheEntry.fd = -1;
    cacheEntry.refCount = 5;

    auto pool = new WhiteBox<::L0::EventPool>();
    pool->importedIpcCacheId = cacheId;
    pool->importedIpcDriverHandle = &driverHandle;

    delete pool;

    EXPECT_EQ(4u, driverHandle.opaqueHandleImportCache[cacheId].refCount);
}

TEST_F(EventPoolIpcFabricAccessTests, whenDestroyingImportedOpaqueEventPoolWithoutDriverHandleThenOpaqueHandleImportCacheRefIsNotReleased) {
    constexpr uint64_t cacheId = 0x1234;
    auto &cacheEntry = driverHandle.opaqueHandleImportCache[cacheId];
    cacheEntry.fd = -1;
    cacheEntry.refCount = 5;

    auto pool = new WhiteBox<::L0::EventPool>();
    pool->importedIpcCacheId = cacheId;
    pool->importedIpcDriverHandle = nullptr;

    delete pool;

    EXPECT_EQ(5u, driverHandle.opaqueHandleImportCache[cacheId].refCount);
}

TEST_F(EventPoolIPCHandleTests, whenOpeningOpaqueIpcEventPoolHandleWithipcSupportedAllocationByDefaultEnabledThenEventPoolIsCreatedAndProcessIDsAreTheSame) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableipcSupportedAllocationByDefault.set(true);

    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    // Enable opaque IPC handles
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = OpaqueHandlingType::pidfd;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *memManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(memManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &poolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t badHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(ipcData.handle.val, badHandle);
    EXPECT_EQ(ipcData.numEvents, numEvents);
    EXPECT_EQ(ipcData.processId, NEO::SysCalls::getCurrentProcessId());

    ze_event_pool_handle_t ipcPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    auto ipcPool = L0::EventPool::fromHandle(ipcPoolHandle);

    EXPECT_EQ(ipcPool->getEventSize(), eventPool->getEventSize());
    EXPECT_EQ(static_cast<uint32_t>(ipcPool->getNumEvents()), numEvents);
    EXPECT_TRUE(ipcPool->isEventPoolTimestampFlagSet());

    EXPECT_EQ(ipcPool->closeIpcHandle(), ZE_RESULT_SUCCESS);

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
    delete memManager;
    driverHandle->setMemoryManager(curMemManager);
}

TEST_F(EventPoolIPCHandleTests, whenOpeningNonOpaqueIpcEventPoolHandleThenEventPoolIsCreatedButProcessIDsNotSet) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        numEvents};

    // Set to non-opaque IPC handles.
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = OpaqueHandlingType::none;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto curMemManager = driverHandle->getMemoryManager();
    MemoryManagerEventPoolIpcMock *memManager = new MemoryManagerEventPoolIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(memManager);
    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &deviceHandle, &poolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ze_result_t res = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    auto &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    constexpr uint64_t badHandle = static_cast<uint64_t>(-1);
    EXPECT_NE(ipcData.handle.val, badHandle);
    EXPECT_EQ(ipcData.numEvents, numEvents);
    EXPECT_NE(ipcData.processId, NEO::SysCalls::getCurrentProcessId());
    EXPECT_EQ(ipcData.processId, 0u);

    ze_event_pool_handle_t ipcPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    auto ipcPool = L0::EventPool::fromHandle(ipcPoolHandle);

    EXPECT_EQ(ipcPool->getEventSize(), eventPool->getEventSize());
    EXPECT_EQ(static_cast<uint32_t>(ipcPool->getNumEvents()), numEvents);
    EXPECT_TRUE(ipcPool->isEventPoolTimestampFlagSet());

    EXPECT_EQ(ipcPool->closeIpcHandle(), ZE_RESULT_SUCCESS);

    EXPECT_EQ(eventPool->destroy(), ZE_RESULT_SUCCESS);
    delete memManager;
    driverHandle->setMemoryManager(curMemManager);
}

using EventPoolCreateMultiDevice = Test<MultiDeviceFixture>;

TEST_F(EventPoolCreateMultiDevice, whenGettingIpcHandleForEventPoolWhenHostShareableMemoryIsFalseThenUnsupportedIsReturned) {
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
       GivenRemoteEventPoolHasTwoEventPacketsWhenContextWithSinglePacketOpensIpcEventPoolFromIpcHandleThenDifferentMaxEventPacketsCauseInvalidArgumentError) {
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

    StreamCapture capture;
    capture.captureStderr();

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    res = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    std::string output = capture.getCapturedStderr();
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

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
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
    MemoryManagerEventPoolIpcMock mockMemoryManager(*neoDevice->executionEnvironment);
    mockMemoryManager.callParentCreateGraphicsAllocationFromSharedHandle = false;
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle->memoryManager, &mockMemoryManager);
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

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(ipcEventPool, &eventDesc, device, result));
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
    result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    event->destroy();

    res = eventPool->destroy();
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
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
    MemoryManagerEventPoolIpcMock mockMemoryManager(*neoDevice->executionEnvironment);
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle->memoryManager, &mockMemoryManager);

    auto eventPool = EventPool::create(driverHandle.get(), context, 0, &deviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    MultiDeviceEventPoolOpenIpcHandleMemoryManager currMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(&currMemoryManager);
    currMemoryManager.maxCalls = 4;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);
    EXPECT_EQ(3u, currMemoryManager.calls);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    ASSERT_EQ(driverHandle->devices.size(), ipcEventPool->devices.size());
    for (uint32_t i = 0; i < driverHandle->devices.size(); i++) {
        EXPECT_EQ(driverHandle->devices[i], ipcEventPool->devices[i]);
    }

    ipcEventPool->closeIpcHandle();
    EXPECT_EQ(1u, currMemoryManager.alloc0FreeCall);
    EXPECT_EQ(3u, currMemoryManager.alloc1FreeCall);

    driverHandle->setMemoryManager(&mockMemoryManager);

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
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
    MemoryManagerEventPoolIpcMock mockMemoryManager(*neoDevice->executionEnvironment);
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle->memoryManager, &mockMemoryManager);

    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(), context, 2, devices.data(), &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    EXPECT_TRUE(ipcHandleData.isHostVisibleEventPoolAllocation);

    MultiDeviceEventPoolOpenIpcHandleMemoryManager currMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(&currMemoryManager);
    currMemoryManager.maxCalls = 4;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    EXPECT_EQ(ipcEventPool->isHostVisibleEventPoolAllocation, eventPool->isHostVisibleEventPoolAllocation);
    EXPECT_TRUE(ipcEventPool->isHostVisibleEventPoolAllocation);

    ipcEventPool->closeIpcHandle();

    driverHandle->setMemoryManager(&mockMemoryManager);

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
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
    MemoryManagerEventPoolIpcMock mockMemoryManager(*neoDevice->executionEnvironment);
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle->memoryManager, &mockMemoryManager);

    auto eventPool = EventPool::create(driverHandle.get(), context, 4, deviceHandles.data(), &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    EXPECT_EQ(ipcHandleData.numDevices, 4u);

    MultiDeviceEventPoolOpenIpcHandleMemoryManager currMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(&currMemoryManager);
    currMemoryManager.maxCalls = 4;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);
    EXPECT_EQ(3u, currMemoryManager.calls);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    for (uint32_t i = 0; i < 4u; i++) {
        EXPECT_EQ(driverHandle->devices[i], ipcEventPool->devices[i]);
    }
    EXPECT_EQ(ipcHandleData.numDevices, ipcEventPool->devices.size());

    ipcEventPool->closeIpcHandle();
    EXPECT_EQ(1u, currMemoryManager.alloc0FreeCall);
    EXPECT_EQ(3u, currMemoryManager.alloc1FreeCall);

    driverHandle->setMemoryManager(&mockMemoryManager);

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolCreateMultiDevice, GivenHostEventPoolCreatedWithSingleDeviceWhenOpeningIpcEventPoolUsingMultiDeviceContextThenFirstContextDeviceIsUsedAndHostStorageIsSharedWithOtherRootDevices) {
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
    MemoryManagerEventPoolIpcMock mockMemoryManager(*neoDevice->executionEnvironment);
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle->memoryManager, &mockMemoryManager);

    auto eventPool = EventPool::create(driverHandle.get(), context, 1, deviceHandles.data(), &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto &ipcHandleData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data);
    EXPECT_EQ(ipcHandleData.numDevices, 1u);
    ipcHandleData.isDeviceEventPoolAllocation = false;

    MultiDeviceEventPoolOpenIpcHandleMemoryManager currMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(&currMemoryManager);
    currMemoryManager.maxCalls = 4u;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);
    EXPECT_EQ(3u, currMemoryManager.calls);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    EXPECT_EQ(ipcHandleData.numDevices, ipcEventPool->devices.size());
    EXPECT_EQ(driverHandle->devices[0], ipcEventPool->devices[0]);

    ipcEventPool->closeIpcHandle();
    EXPECT_EQ(1u, currMemoryManager.alloc0FreeCall);
    EXPECT_EQ(3u, currMemoryManager.alloc1FreeCall);

    driverHandle->setMemoryManager(&mockMemoryManager);

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(EventPoolCreateMultiDevice, GivenContextWithOnlyHighestRootDeviceWhenOpeningIpcEventPoolThenAllocationIsStoredForThatRootDevice) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        1};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto highestDevice = driverHandle->devices[numRootDevices - 1];
    auto highestDeviceHandle = highestDevice->toHandle();
    NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(driverHandle->devices[0]->getNEODevice());
    MemoryManagerEventPoolIpcMock mockMemoryManager(*neoDevice->executionEnvironment);
    VariableBackup<NEO::MemoryManager *> memoryManagerBackup(&driverHandle->memoryManager, &mockMemoryManager);

    auto eventPool = EventPool::create(driverHandle.get(), context, 1, &highestDeviceHandle, &eventPoolDesc, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->getIpcHandle(&ipcHandle));

    ze_context_handle_t hImporterContext = nullptr;
    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ASSERT_EQ(ZE_RESULT_SUCCESS, driverHandle->createContext(&contextDesc, 1u, &highestDeviceHandle, &hImporterContext));
    auto importerContext = Context::fromHandle(hImporterContext);

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    result = importerContext->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ipcEventPoolHandle);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    EXPECT_EQ(highestDevice, ipcEventPool->devices[0]);
    auto importedAllocation = ipcEventPool->getAllocation().getGraphicsAllocation(highestDevice->getRootDeviceIndex());
    ASSERT_NE(nullptr, importedAllocation);
    EXPECT_EQ(highestDevice->getRootDeviceIndex(), importedAllocation->getRootDeviceIndex());

    ipcEventPool->closeIpcHandle();
    importerContext->destroy();

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventPool->destroy());
}

class EventPoolPeerImportMemoryManager : public FailMemoryManager {
  public:
    using FailMemoryManager::FailMemoryManager;

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        importedRootDeviceIndices.push_back(properties.rootDeviceIndex);
        importedHandles.push_back(static_cast<uint64_t>(osHandleData.handle));
        importedAsHostIpc.push_back(isHostIpcAllocation);
        if (properties.rootDeviceIndex == failingRootDeviceIndex) {
            return nullptr;
        }
        auto allocation = new NEO::MockGraphicsAllocation(properties.rootDeviceIndex, buffer, sizeof(buffer));
        allocation->internalHandle = exportedHandle;
        allocation->peekInternalHandleResult = peekInternalHandleResult;
        return allocation;
    }

    void closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) override {
        closedHandles.push_back(handle);
    }

    GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) override {
        existingStorageCalls++;
        return nullptr;
    }

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation) override {
        freeCalls++;
        delete gfxAllocation;
    }

    char buffer[256] = {};
    std::vector<uint32_t> importedRootDeviceIndices;
    std::vector<uint64_t> importedHandles;
    std::vector<bool> importedAsHostIpc;
    std::vector<uint64_t> closedHandles;
    uint64_t exportedHandle = 0x55;
    int peekInternalHandleResult = 0;
    uint32_t failingRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    uint32_t existingStorageCalls = 0;
    uint32_t freeCalls = 0;
};

struct EventPoolOpenIpcPeerAccessTest : public EventPoolCreateMultiDevice {
    void SetUp() override {
        EventPoolCreateMultiDevice::SetUp();

        neoDevice = static_cast<NEO::MockDevice *>(driverHandle->devices[0]->getNEODevice());
        originalMemoryManager = driverHandle->getMemoryManager();
        exporterMemoryManager = std::make_unique<MemoryManagerEventPoolIpcMock>(*neoDevice->executionEnvironment);
        exporterMemoryManager->callParentAllocateGraphicsMemoryWithProperties = false;
        driverHandle->setMemoryManager(exporterMemoryManager.get());

        ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, ZE_EVENT_POOL_FLAG_IPC, 1};
        auto exporterDeviceHandle = driverHandle->devices[2]->toHandle();
        ze_result_t result = ZE_RESULT_SUCCESS;
        exporterEventPool = EventPool::create(driverHandle.get(), context, 1, &exporterDeviceHandle, &eventPoolDesc, result);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, exporterEventPool);
        ASSERT_EQ(ZE_RESULT_SUCCESS, exporterEventPool->getIpcHandle(&ipcHandle));
        ASSERT_TRUE(reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle.data)->isDeviceEventPoolAllocation);

        importerMemoryManager = std::make_unique<EventPoolPeerImportMemoryManager>(*neoDevice->executionEnvironment);
        driverHandle->setMemoryManager(importerMemoryManager.get());
    }

    void TearDown() override {
        driverHandle->setMemoryManager(exporterMemoryManager.get());
        exporterEventPool->destroy();
        driverHandle->setMemoryManager(originalMemoryManager);
        EventPoolCreateMultiDevice::TearDown();
    }

    NEO::MockDevice *neoDevice = nullptr;
    NEO::MemoryManager *originalMemoryManager = nullptr;
    std::unique_ptr<MemoryManagerEventPoolIpcMock> exporterMemoryManager;
    std::unique_ptr<EventPoolPeerImportMemoryManager> importerMemoryManager;
    L0::EventPool *exporterEventPool = nullptr;
    ze_ipc_event_pool_handle_t ipcHandle = {};
};

TEST_F(EventPoolOpenIpcPeerAccessTest, GivenSingleDeviceDeviceEventPoolAndPeerAccessSupportedWhenOpeningIpcHandleInMultiDeviceContextThenPeerAllocationIsImportedForEachContextRootDevice) {
    NEO::debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(1);

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle));
    ASSERT_NE(nullptr, ipcEventPoolHandle);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    ASSERT_EQ(numRootDevices, importerMemoryManager->importedRootDeviceIndices.size());
    EXPECT_EQ(0u, importerMemoryManager->existingStorageCalls);
    for (uint32_t i = 1; i < numRootDevices; i++) {
        EXPECT_EQ(importerMemoryManager->exportedHandle, importerMemoryManager->importedHandles[i]);
        EXPECT_FALSE(importerMemoryManager->importedAsHostIpc[i]);
    }
    EXPECT_EQ(std::vector<uint64_t>(numRootDevices - 1, importerMemoryManager->exportedHandle), importerMemoryManager->closedHandles);
    auto firstRootDeviceIndex = driverHandle->devices[0]->getRootDeviceIndex();
    for (auto &device : driverHandle->devices) {
        auto rootDeviceIndex = device->getRootDeviceIndex();
        auto allocation = ipcEventPool->getAllocation().getGraphicsAllocation(rootDeviceIndex);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(rootDeviceIndex, allocation->getRootDeviceIndex());
        if (rootDeviceIndex != firstRootDeviceIndex) {
            EXPECT_EQ(std::numeric_limits<osHandle>::max(), allocation->peekSharedHandle());
        }
    }

    ipcEventPool->closeIpcHandle();
    EXPECT_EQ(numRootDevices, importerMemoryManager->freeCalls);
}

TEST_F(EventPoolOpenIpcPeerAccessTest, GivenSingleDeviceDeviceEventPoolAndPeerAccessNotSupportedWhenOpeningIpcHandleInMultiDeviceContextThenOnlyFirstContextDeviceGetsAllocation) {
    NEO::debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(0);

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle));
    ASSERT_NE(nullptr, ipcEventPoolHandle);

    auto ipcEventPool = whiteboxCast(EventPool::fromHandle(ipcEventPoolHandle));
    auto firstRootDeviceIndex = driverHandle->devices[0]->getRootDeviceIndex();
    ASSERT_EQ(1u, importerMemoryManager->importedRootDeviceIndices.size());
    EXPECT_EQ(firstRootDeviceIndex, importerMemoryManager->importedRootDeviceIndices[0]);
    EXPECT_EQ(0u, importerMemoryManager->existingStorageCalls);
    for (auto &device : driverHandle->devices) {
        auto rootDeviceIndex = device->getRootDeviceIndex();
        EXPECT_EQ(rootDeviceIndex == firstRootDeviceIndex, ipcEventPool->getAllocation().getGraphicsAllocation(rootDeviceIndex) != nullptr);
    }

    ipcEventPool->closeIpcHandle();
    EXPECT_EQ(1u, importerMemoryManager->freeCalls);
}

TEST_F(EventPoolOpenIpcPeerAccessTest, GivenSingleDeviceDeviceEventPoolAndPeerImportFailureWhenOpeningIpcHandleInMultiDeviceContextThenErrorIsReturnedAndImportedAllocationsAreFreed) {
    NEO::debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(1);
    importerMemoryManager->failingRootDeviceIndex = driverHandle->devices[2]->getRootDeviceIndex();

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle));
    EXPECT_EQ(nullptr, ipcEventPoolHandle);
    EXPECT_EQ(3u, importerMemoryManager->importedRootDeviceIndices.size());
    EXPECT_EQ(2u, importerMemoryManager->closedHandles.size());
    EXPECT_EQ(2u, importerMemoryManager->freeCalls);
}

TEST_F(EventPoolOpenIpcPeerAccessTest, GivenSingleDeviceDeviceEventPoolAndExportFromImportedAllocationFailsWhenOpeningIpcHandleInMultiDeviceContextThenErrorIsReturnedAndImportedAllocationIsFreed) {
    NEO::debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(1);
    importerMemoryManager->peekInternalHandleResult = -1;

    ze_event_pool_handle_t ipcEventPoolHandle = {};
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, context->openEventPoolIpcHandle(ipcHandle, &ipcEventPoolHandle));
    EXPECT_EQ(nullptr, ipcEventPoolHandle);
    EXPECT_EQ(1u, importerMemoryManager->importedRootDeviceIndices.size());
    EXPECT_EQ(0u, importerMemoryManager->closedHandles.size());
    EXPECT_EQ(1u, importerMemoryManager->freeCalls);
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

    std::unique_ptr<Event> event(static_cast<Event *>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
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

    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    auto event = std::unique_ptr<L0::Event>(l0GfxCoreHelper.createEvent(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(nullptr, event);

    if (eventPool->isImplicitScalingCapableFlagSet()) {
        EXPECT_TRUE(event->isEventTimestampFlagSet());
    } else {
        EXPECT_FALSE(event->isEventTimestampFlagSet());
    }

    uint32_t *eventCompletionMemory = reinterpret_cast<uint32_t *>(event->getCompletionFieldHostAddress());
    uint32_t maxPacketsCount = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());

    for (uint32_t i = 0; i < maxPacketsCount; i++) {
        EXPECT_EQ(Event::STATE_INITIAL, *eventCompletionMemory);
        eventCompletionMemory = ptrOffset(eventCompletionMemory, event->getSinglePacketSize());
    }

    result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    uint64_t gpuAddr = event->getGpuAddress(device);
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    event->hostSignal(false);
    result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(gpuAddr, event->getPacketAddress(device));

    event->reset();
    result = event->queryStatus(0);
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
            events.emplace_back(static_cast<Event *>(getHelper<L0GfxCoreHelper>().createEvent(eventPools[i].get(), &eventDesc, device, result)));
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

    ASSERT_GE(driverHandle->devices.size(), 2u);

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    auto neoDevice0 = device0->getNEODevice();
    auto neoDevice1 = device1->getNEODevice();

    VariableBackup<std::unique_ptr<ApiGfxCoreHelper>> backupApiGfxCoreHelper0(&neoDevice0->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper, std::make_unique<MockL0GfxCoreHelperAlwaysAllocateEventInLocalMemHw<FamilyType>>());
    VariableBackup<std::unique_ptr<ApiGfxCoreHelper>> backupApiGfxCoreHelper1(&neoDevice1->getExecutionEnvironment()->rootDeviceEnvironments[1]->apiGfxCoreHelper, std::make_unique<MockL0GfxCoreHelperAlwaysAllocateEventInLocalMemHw<FamilyType>>());

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
            eventsDevice0.emplace_back(static_cast<Event *>(l0GfxCoreHelper.createEvent(eventPoolsDevice0[i].get(), &eventDesc, device0, result)));
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
            eventsDevice1.emplace_back(static_cast<Event *>(l0GfxCoreHelper.createEvent(eventPoolsDevice1[i].get(), &eventDesc, device1, result)));
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
}

using EventSynchronizeTimestampTest = Test<EventFixture<1, 1>>;
using EventUsedPacketSignalSynchronizeTest = Test<EventCompactL3FlushFixture<1, 0, -1>>;

HWTEST_F(EventSynchronizeTest, GivenGpuHangWhenHostSynchronizeIsCalledThenDeviceLostIsReturned) {
    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isGpuHangDetectedReturnValue = true;

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    auto result = event->hostSynchronize(timeout);

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

HWTEST_F(EventSynchronizeTest, GivenHangHappenedBeforePeriodicHangCheckAndForceGpuStatusCheckDuringHostSynchronizeThenHangIsDetected) {
    NEO::debugManager.flags.ForceGpuStatusCheckOnSuccessfulEventHostSynchronize.set(1);

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isGpuHangDetectedReturnValue = true;
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

HWTEST_F(EventSynchronizeTest, GivenCompletedEventWithoutCmdQueueWhenHostSynchronizeIsCalledThenNoCsrClientIsUnregistered) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    event->csrs[0] = &csr;
    *static_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(0));
    EXPECT_EQ(0u, csr.unregisterClientCalled);
}

HWTEST_F(EventSynchronizeTest, GivenCompletedEventWithCmdQueueWhenHostSynchronizeIsCalledThenCmdQueueIsUnregisteredOnce) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    Mock<CommandQueue> cmdQueue(device, &csr);
    csr.registerClient(&cmdQueue);
    auto numClients = csr.getNumClients();

    event->csrs[0] = &csr;
    event->setLatestUsedCmdQueue(&cmdQueue);
    *static_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(0));
    EXPECT_EQ(1u, csr.unregisterClientCalled);
    EXPECT_EQ(numClients - 1, csr.getNumClients());
}

TEST_F(EventSynchronizeTest, GivenNoGpuHangAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenResultNotReadyIsReturnedDueToTimeout) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = false;

    event->csrs[0] = csr.get();

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = event->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, GivenShortTimeoutAndNotReadyEventWhenHostSynchronizeIsCalledThenTPauseIsNotCalled) {
    int64_t eventHostSyncThreshold = WaitUtils::defaultWaitPkgThresholdForEventHostSyncInMicroSeconds;
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    VariableBackup<int64_t> backupWaitpkgThresholdForEventHostSync(
        &WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds,
        eventHostSyncThreshold);

    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = false;

    event->csrs[0] = csr.get();

    constexpr uint64_t timeoutNanoseconds = 1;
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);
    auto result = event->hostSynchronize(timeoutNanoseconds);
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);
    CpuIntrinsicsTests::tpauseCounter = 0;

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, GivenUllsLightThresholdAndShortTimeoutWhenHostSynchronizeIsCalledThenTPauseIsNotCalled) {
    int64_t ullsLightThreshold = WaitUtils::defaultWaitPkgThresholdForUllsLightInMicroSeconds;
    int64_t eventHostSyncThreshold = WaitUtils::defaultWaitPkgThresholdForEventHostSyncInMicroSeconds;
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    VariableBackup<int64_t> backupWaitpkgThreshold(&WaitUtils::waitPkgThresholdInMicroSeconds, ullsLightThreshold);
    VariableBackup<int64_t> backupWaitpkgThresholdForEventHostSync(
        &WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds,
        eventHostSyncThreshold);

    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = false;

    event->csrs[0] = csr.get();

    constexpr uint64_t timeoutNanoseconds = 1;
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);
    auto result = event->hostSynchronize(timeoutNanoseconds);
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);
    CpuIntrinsicsTests::tpauseCounter = 0;

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

HWTEST_F(EventSynchronizeTest, GivenNotReadyEventAndInfiniteTimeoutWhenHostSynchronizeIsCalledThenTPauseIsCalled) {
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    VariableBackup<int64_t> backupWaitpkgThresholdForEventHostSync(&WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds, 0);
    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    VariableBackup<std::function<void()>> backupControlTpause(&CpuIntrinsicsTests::controlTpause);

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isGpuHangDetectedReturnValue = false;

    CpuIntrinsicsTests::tpauseCounter = 0u;

    auto *hostAddr = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), event->getContextStartOffset()));
    *hostAddr = Event::STATE_CLEARED;

    const auto testStartTime = std::chrono::high_resolution_clock::now();
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = hostAddr;
    CpuIntrinsicsTests::setupPauseAddress = [hostAddr, testStartTime]() {
        if (std::chrono::high_resolution_clock::now() - testStartTime > 10ms) {
            *hostAddr = Event::STATE_SIGNALED;
        }
    };
    CpuIntrinsicsTests::controlTpause = [hostAddr]() {
        *hostAddr = Event::STATE_SIGNALED;
    };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, CpuIntrinsicsTests::tpauseCounter.load());
    CpuIntrinsicsTests::tpauseCounter = 0u;
}

HWTEST_F(EventSynchronizeTest, GivenNotReadyEventAndInfiniteTimeoutWhenHostSynchronizeIsCalledThenTPauseUsesEventHostSyncCounterValue) {
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    VariableBackup<int64_t> backupWaitpkgThresholdForEventHostSync(&WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds, 0);
    uint64_t ullsLightCounterValue = WaitUtils::defaultCounterValueForUllsLight;
    VariableBackup<uint64_t> backupWaitpkgCounterValue(&WaitUtils::waitpkgCounterValue, ullsLightCounterValue);
    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    VariableBackup<std::function<void()>> backupControlTpause(&CpuIntrinsicsTests::controlTpause);

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isGpuHangDetectedReturnValue = false;

    CpuIntrinsicsTests::tpauseCounter = 0u;
    CpuIntrinsicsTests::lastTpauseCounter = 0u;

    auto *hostAddr = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), event->getContextStartOffset()));
    *hostAddr = Event::STATE_CLEARED;

    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = hostAddr;
    CpuIntrinsicsTests::setupPauseAddress = []() {};
    CpuIntrinsicsTests::controlTpause = [hostAddr]() {
        *hostAddr = Event::STATE_SIGNALED;
    };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, CpuIntrinsicsTests::tpauseCounter.load());
    EXPECT_EQ(CpuIntrinsicsTests::rdtscRetValue + WaitUtils::defaultCounterValueForEventHostSync, CpuIntrinsicsTests::lastTpauseCounter.load());
    CpuIntrinsicsTests::tpauseCounter = 0u;
    CpuIntrinsicsTests::lastTpauseCounter = 0u;
}

HWTEST_F(EventSynchronizeTest, GivenGpuHangAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenCsrHangCheckPeriodIsRespected) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isGpuHangDetectedReturnValue = true;
    csr.gpuHangCheckPeriodUs = std::numeric_limits<int64_t>::max();

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = event->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    csr.gpuHangCheckPeriodUs = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, event->hostSynchronize(timeoutNanoseconds));
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutZeroAndStateInitialHostSynchronizeReturnsNotReady) {
    ze_result_t result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, GivenNotReadyEventWhenHostSynchronizeWithZeroTimeoutThenCoreIsNotYielded) {
    const auto yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(0));
    EXPECT_EQ(yieldCountBefore, CpuIntrinsicsTests::yieldCounter);
}

TEST_F(EventSynchronizeTest, GivenNotReadyEventWhenHostSynchronizeWithTimeoutThenCoreIsYieldedBetweenPolls) {
    const auto yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(10));
    EXPECT_GT(CpuIntrinsicsTests::yieldCounter.load(), yieldCountBefore);
}

TEST_F(EventSynchronizeTest, GivenNotReadyCounterBasedEventWhenHostSynchronizeWithTimeoutThenCoreIsYieldedBetweenPolls) {
    event->enableCounterBasedMode(true, 0u);
    uint64_t counterValue = 0;
    event->inOrderExecHelper.initializeLocalTempStorage();
    event->inOrderExecHelper.assignData(1, 0, 1, 1, event->getAllocation(device), event->getAllocation(device), 0, 0, &counterValue, 0, 0, false, false);

    const auto yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(10));
    EXPECT_GT(CpuIntrinsicsTests::yieldCounter.load(), yieldCountBefore);
}

TEST_F(EventSynchronizeTest, GivenNotReadyCounterBasedEventWhenHostSynchronizeWithZeroTimeoutThenCoreIsNotYielded) {
    event->enableCounterBasedMode(true, 0u);
    uint64_t counterValue = 0;
    event->inOrderExecHelper.initializeLocalTempStorage();
    event->inOrderExecHelper.assignData(1, 0, 1, 1, event->getAllocation(device), event->getAllocation(device), 0, 0, &counterValue, 0, 0, false, false);

    const auto yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(0));
    EXPECT_EQ(yieldCountBefore, CpuIntrinsicsTests::yieldCounter);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithNonZeroTimeoutAndStateInitialHostSynchronizeReturnsNotReady) {
    ze_result_t result = event->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, GivenEventHostSynchronizeWaitStrategyDebugFlagsWhenDefaultsAreUsedThenKmdWaitStrategyAndDefaultTimingsAreSet) {
    EXPECT_EQ(3, NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.get());
    EXPECT_FALSE(NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.get());
    EXPECT_TRUE(NEO::debugManager.flags.EventHostSynchronizeWindowsDiscreteKmdWait.get());
    EXPECT_EQ(-1, NEO::debugManager.flags.EventHostSynchronizeWindowsDiscreteKmdWaitUlls.get());
    EXPECT_EQ(5000, NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.get());
    EXPECT_EQ(750, NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.get());
    EXPECT_EQ(50, NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.get());
    EXPECT_EQ(20000, NEO::debugManager.flags.EventHostSynchronizeWaitStrategyMinTimeoutMicroseconds.get());
    EXPECT_EQ(12000, NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.get());
    EXPECT_EQ(750000, NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWaitTimeoutNanoseconds.get());
}

TEST(EventHostSynchronizeWaitTest, givenFiniteTimeoutWhenCalculatingKmdWaitTimeoutThenFinalPollingWindowIsReserved) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategyMinTimeoutMicroseconds.set(20000);

    EXPECT_EQ(70000000u, EventHostSynchronize::getKmdWaitTimeout(100000000u, 10000000u));
    EXPECT_EQ(0u, EventHostSynchronize::getKmdWaitTimeout(32999999u, 12000000u));
    EXPECT_EQ(0u, EventHostSynchronize::getKmdWaitTimeout(30000000u, 10000000u));
    EXPECT_EQ(0u, EventHostSynchronize::getKmdWaitTimeout(10000000u, 10000000u));
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), EventHostSynchronize::getKmdWaitTimeout(std::numeric_limits<uint64_t>::max(), 10000000u));
}

TEST(EventHostSynchronizeWaitTest, givenDisabledFinalPollingWindowWhenCalculatingKmdWaitTimeoutThenAllRemainingTimeIsReturned) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategyMinTimeoutMicroseconds.set(0);

    EXPECT_EQ(90000000u, EventHostSynchronize::getKmdWaitTimeout(100000000u, 10000000u));
}

HWTEST_F(EventSynchronizeTest, GivenWaitControllerWhenStrategiesAndInputsAreChangedThenExpectedWaitActionsAreReturned) {
    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &rootDeviceEnvironment = *neoDevice->executionEnvironment->rootDeviceEnvironments[0];

    auto setWaitStrategy = [](int32_t strategy, int64_t initialPollUs, int64_t pollUs, int64_t sleepUs) {
        NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(strategy);
        NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(initialPollUs);
        NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(pollUs);
        NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(sleepUs);
        NEO::debugManager.flags.EventHostSynchronizeWaitStrategyMinTimeoutMicroseconds.set(20);
    };

    auto resetAcLineStatus = [&csr]() {
        csr.getAcLineConnectedCallBase = false;
        csr.getAcLineConnectedCalled = 0u;
        csr.getAcLineConnectedLastUpdateStatus = false;
        csr.getAcLineConnectedReturnValue = false;
    };

    {
        setWaitStrategy(2, 0, 0, 10);
        rootDeviceEnvironment.osInterface.reset();
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto waitAction = waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0);

        EXPECT_EQ(0, waitAction.sleepUs);
        EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
    }

    {
        setWaitStrategy(2, 0, 0, 10);
        rootDeviceEnvironment.osInterface = std::make_unique<NEO::OSInterface>();
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto waitAction = waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0);

        EXPECT_EQ(0, waitAction.sleepUs);
        EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
    }

    {
        setWaitStrategy(2, 0, 0, 10);
        rootDeviceEnvironment.osInterface = std::make_unique<NEO::OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto waitAction = waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0);

        EXPECT_EQ(0, waitAction.sleepUs);
        EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
    }

    rootDeviceEnvironment.osInterface = std::make_unique<NEO::OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    {
        setWaitStrategy(1, 10, 5, 3);
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto waitAction = waitController.getAction(15, std::numeric_limits<uint64_t>::max(), 15000);

        EXPECT_EQ(3, waitAction.sleepUs);
        EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
    }

    {
        setWaitStrategy(1, 10, 5, 3);
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto waitAction = waitController.getAction(14, std::numeric_limits<uint64_t>::max(), 14000);

        EXPECT_EQ(0, waitAction.sleepUs);
        EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
    }

    {
        setWaitStrategy(2, 0, 0, 10);
        resetAcLineStatus();
        csr.getAcLineConnectedReturnValue = false;
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto firstWaitAction = waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0);
        csr.getAcLineConnectedReturnValue = true;
        auto secondWaitAction = waitController.getAction(10, std::numeric_limits<uint64_t>::max(), 10000);

        EXPECT_EQ(10, firstWaitAction.sleepUs);
        EXPECT_EQ(10, secondWaitAction.sleepUs);
        EXPECT_EQ(1u, csr.getAcLineConnectedCalled);
        EXPECT_TRUE(csr.getAcLineConnectedLastUpdateStatus);
    }

    {
        setWaitStrategy(2, 0, 0, 10);
        resetAcLineStatus();
        csr.getAcLineConnectedReturnValue = true;
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto waitAction = waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0);

        EXPECT_EQ(0, waitAction.sleepUs);
        EXPECT_EQ(1u, csr.getAcLineConnectedCalled);
    }

    {
        setWaitStrategy(3, 0, 0, 10);
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto waitAction = waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0);

        EXPECT_EQ(0, waitAction.sleepUs);
        EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
    }

    {
        setWaitStrategy(2, 0, 0, 10);
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto infiniteTimeout = waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0);
        auto shortFiniteTimeout = waitController.getAction(10, 20000, 10000);
        auto longFiniteTimeout = waitController.getAction(10, 40000, 10000);
        auto nearDeadline = waitController.getAction(10, 100000, 80000);
        auto deadlineReached = waitController.getAction(10, 100000, 100000);

        EXPECT_EQ(10, infiniteTimeout.sleepUs);
        EXPECT_EQ(0, shortFiniteTimeout.sleepUs);
        EXPECT_EQ(10, longFiniteTimeout.sleepUs);
        EXPECT_EQ(0, nearDeadline.sleepUs);
        EXPECT_EQ(0, deadlineReached.sleepUs);
    }

    {
        setWaitStrategy(2, 0, 0, 10);
        NEO::debugManager.flags.EventHostSynchronizeWaitStrategyMinTimeoutMicroseconds.set(0);
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        auto finiteTimeout = waitController.getAction(0, 1, 0);
        auto deadlineReached = waitController.getAction(10, 100000, 100000);

        EXPECT_EQ(10, finiteTimeout.sleepUs);
        EXPECT_EQ(0, deadlineReached.sleepUs);
    }

    {
        setWaitStrategy(2, 0, -1, 10);
        resetAcLineStatus();
        L0::EventHostSynchronize::WaitController waitController(csr);

        EXPECT_EQ(0, waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0).sleepUs);

        NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(0);
        NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(0);
        EXPECT_EQ(0, waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0).sleepUs);

        NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(std::numeric_limits<int64_t>::max());
        NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
        EXPECT_EQ(0, waitController.getAction(0, std::numeric_limits<uint64_t>::max(), 0).sleepUs);
        EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
    }
}

HWTEST_F(EventSynchronizeTest, GivenZeroTimeoutWhenEventHostSynchronizeWaitStrategyIsEnabledThenAcLineStatusIsNotQueried) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(2);
    NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getAcLineConnectedCallBase = false;

    event->hostSynchronize(0);
    EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
}

HWTEST_F(EventSynchronizeTest, GivenDisabledEventHostSynchronizeWaitStrategyWhenHostSynchronizeIsCalledThenAcLineStatusIsNotQueried) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(0);
    NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(1);
    NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getAcLineConnectedCallBase = false;

    event->hostSynchronize(10);
    EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
}

HWTEST_F(EventSynchronizeTest, GivenFiniteTimeoutAndBatteryOnlyEventHostSynchronizeWaitStrategyAndWddmWhenHostSynchronizeIsCalledThenAcLineStatusIsNotQueried) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(2);
    NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getAcLineConnectedCallBase = false;
    csr.getAcLineConnectedReturnValue = false;

    event->hostSynchronize(10);
    EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
}

HWTEST_F(EventSynchronizeTest, GivenBatteryOnlyEventHostSynchronizeWaitStrategyAndDrmWhenHostSynchronizeIsCalledThenAcLineStatusIsNotQueried) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(2);
    NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getAcLineConnectedCallBase = false;

    event->hostSynchronize(10);
    EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
}

HWTEST_F(EventSynchronizeTest, GivenDrmAndKmdWaitStrategyWhenSynchronizingRegularEventThenUserFenceWaitsOnEventPacketWithNotEqual) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);
    constexpr int64_t expectedTimeout = 123456789;
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWaitTimeoutNanoseconds.set(expectedTimeout);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.isWaitUserFenceNotEqualSupportedValue = true;
    event->setLinuxUserFenceKmdWaitEnabled(true);
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = true;

    auto eventAddress = static_cast<uint32_t *>(event->getCompletionFieldHostAddress());
    *eventAddress = Event::STATE_CLEARED;
    csr.waitUserFenceParams.onWait = [&]() { *eventAddress = Event::STATE_SIGNALED; };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCount);
    EXPECT_EQ(1u, csr.waitUserFenceParams.callCountWithOperation);
    EXPECT_EQ(NEO::UserFenceWaitOperation::notEqual, csr.waitUserFenceParams.latestWaitOperation);
    EXPECT_EQ(NEO::UserFenceValueWidth::u32, csr.waitUserFenceParams.latestWaitValueWidth);
    EXPECT_EQ(castToUint64(eventAddress), csr.waitUserFenceParams.latestWaitedAddress);
    EXPECT_EQ(static_cast<uint64_t>(Event::STATE_CLEARED), csr.waitUserFenceParams.latestWaitedValue);
    EXPECT_EQ(expectedTimeout, csr.waitUserFenceParams.latestWaitedTimeout);
    EXPECT_FALSE(csr.waitUserFenceParams.userInterrupt);
    EXPECT_EQ(event->getAllocation(device), csr.waitUserFenceParams.latestAllocForInterruptWait);
}

namespace {
struct EventCompletingAfterPolling : L0::EventImp<uint32_t> {
    EventCompletingAfterPolling(L0::Device *device, NEO::CommandStreamReceiver *csr) : L0::EventImp<uint32_t>(0, device, false) {
        this->setCsr(csr, true);
        this->setWindowsDiscreteKmdWaitEnabled(true);
    }

    using L0::EventImp<uint32_t>::queryStatus;
    ze_result_t queryStatus(int64_t timeSinceWait, bool blockOnMiss) override {
        return ++this->queryCount == 3 ? ZE_RESULT_SUCCESS : ZE_RESULT_NOT_READY;
    }

    uint32_t queryCount = 0;
};

void setUpWindowsDiscreteKmdWait(int64_t kmdWaitTimeoutNanoseconds) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizeWindowsDiscreteKmdWaitTimeoutNanoseconds.set(kmdWaitTimeoutNanoseconds);
}
} // namespace

HWTEST_F(EventSynchronizeTest, GivenWorkFlushedAfterEventWhenSynchronizingThenKmdWaiterForEventSubmissionIsCreatedOnceAndWaitedOnWithTimeout) {
    DebugManagerStateRestore restore;
    setUpWindowsDiscreteKmdWait(2000000);
    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    event->setWindowsDiscreteKmdWaitEnabled(true);
    const auto signalTaskCount = csr.peekTaskCount();
    event->setCleanupTaskCount(&csr, signalTaskCount);
    event->setSignalFlushStamp(5);
    csr.taskCount++;

    auto eventAddress = static_cast<uint32_t *>(event->getCompletionFieldHostAddress());
    *eventAddress = Event::STATE_CLEARED;
    csr.kmdWaiterParams.onWait = [&]() {
        if (csr.kmdWaiterParams.waitCount == 2u) {
            *eventAddress = Event::STATE_SIGNALED;
        }
    };

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, csr.kmdWaiterParams.createCount);
    EXPECT_EQ(5u, csr.kmdWaiterParams.latestFlushStamp);
    EXPECT_EQ(signalTaskCount, csr.kmdWaiterParams.latestTaskCount);
    EXPECT_EQ(2u, csr.kmdWaiterParams.waitCount);
    EXPECT_EQ(2000000u, csr.kmdWaiterParams.latestWaitTimeout);
}

HWTEST_F(EventSynchronizeTest, GivenMissingSignalFlushStampNoKmdWaitBudgetOrNoWaiterWhenSynchronizingEventThenEventIsPolled) {
    DebugManagerStateRestore restore;
    setUpWindowsDiscreteKmdWait(2000000);
    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();

    EventCompletingAfterPolling eventNotSignaledByItsCsr(device, &csr);
    eventNotSignaledByItsCsr.setSignalFlushStamp(5);
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventNotSignaledByItsCsr.hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(3u, eventNotSignaledByItsCsr.queryCount);

    EventCompletingAfterPolling eventWithoutFlushStamp(device, &csr);
    eventWithoutFlushStamp.setCleanupTaskCount(&csr, csr.peekTaskCount());
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventWithoutFlushStamp.hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(3u, eventWithoutFlushStamp.queryCount);

    event->setWindowsDiscreteKmdWaitEnabled(true);
    event->setCleanupTaskCount(&csr, csr.peekTaskCount());
    event->setSignalFlushStamp(5);
    *static_cast<uint32_t *>(event->getCompletionFieldHostAddress()) = Event::STATE_CLEARED;
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(1));
    EXPECT_EQ(0u, csr.kmdWaiterParams.createCount);

    csr.kmdWaiterParams.createWaiter = false;
    EventCompletingAfterPolling eventWithoutWaiter(device, &csr);
    eventWithoutWaiter.setCleanupTaskCount(&csr, csr.peekTaskCount());
    eventWithoutWaiter.setSignalFlushStamp(5);
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventWithoutWaiter.hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(3u, eventWithoutWaiter.queryCount);
    EXPECT_EQ(1u, csr.kmdWaiterParams.createCount);
    EXPECT_EQ(0u, csr.kmdWaiterParams.waitCount);
}

HWTEST_F(EventSynchronizeTest, GivenMonitoredFenceReachedBeforeEventOrFailedKmdWaitWhenSynchronizingThenRemainingWaitIsPolled) {
    DebugManagerStateRestore restore;
    setUpWindowsDiscreteKmdWait(2000000);
    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();

    for (const auto waitStatus : {std::optional<NEO::WaitStatus>(NEO::WaitStatus::ready), std::optional<NEO::WaitStatus>()}) {
        csr.kmdWaiterParams.waitStatus = waitStatus;
        csr.kmdWaiterParams.waitCount = 0;
        EventCompletingAfterPolling pollingEvent(device, &csr);
        pollingEvent.setCleanupTaskCount(&csr, csr.peekTaskCount());
        pollingEvent.setSignalFlushStamp(5);

        EXPECT_EQ(ZE_RESULT_SUCCESS, pollingEvent.hostSynchronize(std::numeric_limits<uint64_t>::max()));
        EXPECT_EQ(3u, pollingEvent.queryCount);
        EXPECT_EQ(1u, csr.kmdWaiterParams.waitCount);
    }
}

HWTEST_F(EventSynchronizeTest, GivenGpuHangReportedByKmdWaitWhenSynchronizingEventThenDeviceLostIsReturned) {
    DebugManagerStateRestore restore;
    setUpWindowsDiscreteKmdWait(2000000);
    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.kmdWaiterParams.waitStatus = NEO::WaitStatus::gpuHang;
    event->setWindowsDiscreteKmdWaitEnabled(true);
    event->setCleanupTaskCount(&csr, csr.peekTaskCount());
    event->setSignalFlushStamp(5);
    *static_cast<uint32_t *>(event->getCompletionFieldHostAddress()) = Event::STATE_CLEARED;

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, csr.kmdWaiterParams.waitCount);
}

namespace {
struct MockMetricNotification : public MetricCollectorEventNotify {
    Event::State state = Event::STATE_INITIAL;
    uint32_t getNotificationStateCalled = 0u;
    uint32_t signalAtCall = 0u;
    Event::State getNotificationState() override {
        getNotificationStateCalled++;
        if (signalAtCall != 0u && getNotificationStateCalled >= signalAtCall) {
            state = Event::STATE_SIGNALED;
        }
        return state;
    }
};
} // namespace

HWTEST_F(EventSynchronizeTest, GivenDrmAndKmdWaitStrategyWhenSynchronizingMetricNotificationEventThenUserFenceWaitIsNotUsedAndPollingIsUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    event->setLinuxUserFenceKmdWaitEnabled(true);
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = true;

    // Metric notification events are signaled by the host driver polling the OA/perf FD, not by a GPU write.
    // A blocking KMD user-fence wait would deadlock (nothing wakes it), so such events must fall back to polling.
    MockMetricNotification metricNotification;
    metricNotification.signalAtCall = 2u;
    event->setMetricNotification(&metricNotification);

    auto eventAddress = static_cast<uint32_t *>(event->getCompletionFieldHostAddress());
    *eventAddress = Event::STATE_CLEARED;

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    event->setMetricNotification(nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCount);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCountWithOperation);
    EXPECT_NE(0u, metricNotification.getNotificationStateCalled);
}

HWTEST_F(EventSynchronizeTest, GivenDrmAndKmdWaitStrategyAndSignalAllPacketsWhenRemainingPacketWaitReturnsNotReadyThenHostSynchronizeFallsBackToPolling) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    std::vector<uint64_t> packetStorage;
    auto maxPackets = event->getMaxPacketsCount();
    if (maxPackets <= 1u) {
        maxPackets = 2u;
        event->maxPacketCount = maxPackets;
        packetStorage.resize(alignUp(maxPackets * event->getSinglePacketSize() + sizeof(NEO::TimeStampData), sizeof(uint64_t)) / sizeof(uint64_t));
        event->hostAddressFromPool = packetStorage.data();
    }

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.isWaitUserFenceNotEqualSupportedValue = true;
    event->setLinuxUserFenceKmdWaitEnabled(true);
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = false;

    for (uint32_t packetId = 0; packetId < maxPackets; packetId++) {
        auto packetAddress = static_cast<uint32_t *>(ptrOffset(event->getHostAddress(), packetId * event->getSinglePacketSize() + event->getCompletionFieldOffset()));
        *packetAddress = Event::STATE_CLEARED;
    }

    auto firstPacketAddress = static_cast<uint32_t *>(event->getCompletionFieldHostAddress());
    *firstPacketAddress = Event::STATE_SIGNALED;
    csr.waitUserFenceParams.onWait = [&]() {
        for (uint32_t packetId = 0; packetId < maxPackets; packetId++) {
            auto packetAddress = static_cast<uint32_t *>(ptrOffset(event->getHostAddress(), packetId * event->getSinglePacketSize() + event->getCompletionFieldOffset()));
            *packetAddress = Event::STATE_SIGNALED;
        }
    };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, csr.waitUserFenceParams.callCountWithOperation);
    EXPECT_EQ(NEO::UserFenceWaitOperation::notEqual, csr.waitUserFenceParams.latestWaitOperation);
    EXPECT_NE(castToUint64(firstPacketAddress), csr.waitUserFenceParams.latestWaitedAddress);
}

HWTEST_F(EventSynchronizeTest, GivenDrmAndKmdWaitStrategyWhenUserFenceWaitReturnsNotReadyThenHostSynchronizeFallsBackToPolling) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.isWaitUserFenceNotEqualSupportedValue = true;
    event->setLinuxUserFenceKmdWaitEnabled(true);
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = false;

    auto eventAddress = static_cast<uint32_t *>(event->getCompletionFieldHostAddress());
    *eventAddress = Event::STATE_CLEARED;
    csr.waitUserFenceParams.onWait = [&]() { *eventAddress = Event::STATE_SIGNALED; };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, csr.waitUserFenceParams.callCountWithOperation);
    EXPECT_EQ(NEO::UserFenceWaitOperation::notEqual, csr.waitUserFenceParams.latestWaitOperation);
}

HWTEST_F(EventSynchronizeTimestampTest, GivenDrmAndKmdWaitStrategyWhenSynchronizingHeapfullProfilingEventThenUserFenceWaitsOnTimestampNode) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.isWaitUserFenceNotEqualSupportedValue = true;
    event->setLinuxUserFenceKmdWaitEnabled(true);
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = true;

    using TimestampPackets = NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>;
    MockTagAllocator<TimestampPackets> timestampAllocator(0, neoDevice->getMemoryManager());
    auto timestampNode = timestampAllocator.getTag();
    timestampNode->initialize();

    event->resetInOrderTimestampNode(timestampNode, 1);
    event->setCbEventWithProfiling(true);

    uint64_t counterStorage = 1;
    NEO::MockGraphicsAllocation counterAllocation(&counterStorage, sizeof(counterStorage));
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->getInOrderExecEventHelper().assignData(1u, 0u, 1u, 1u, &counterAllocation, &counterAllocation, counterAllocation.getGpuAddress(), counterAllocation.getGpuAddress(), &counterStorage, 0u, 0u, false, false);

    auto timestampCompletionAddress = static_cast<uint32_t *>(ptrOffset(timestampNode->getCpuBase(), event->getCompletionFieldOffset()));
    *timestampCompletionAddress = Event::STATE_CLEARED;
    csr.waitUserFenceParams.onWait = [&]() { *timestampCompletionAddress = Event::STATE_SIGNALED; };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, csr.waitUserFenceParams.callCountWithOperation);
    EXPECT_EQ(NEO::UserFenceWaitOperation::notEqual, csr.waitUserFenceParams.latestWaitOperation);
    EXPECT_EQ(NEO::UserFenceValueWidth::u32, csr.waitUserFenceParams.latestWaitValueWidth);
    EXPECT_EQ(castToUint64(timestampCompletionAddress), csr.waitUserFenceParams.latestWaitedAddress);
    EXPECT_EQ(timestampNode->getBaseGraphicsAllocation()->getGraphicsAllocation(device->getRootDeviceIndex()), csr.waitUserFenceParams.latestAllocForInterruptWait);
}

HWTEST_F(EventSynchronizeTimestampTest, GivenDrmAndKmdWaitStrategyWhenTimestampNodeUserFenceWaitReturnsNotReadyThenHostSynchronizeFallsBackToPolling) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.isWaitUserFenceNotEqualSupportedValue = true;
    event->setLinuxUserFenceKmdWaitEnabled(true);
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = false;

    using TimestampPackets = NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>;
    MockTagAllocator<TimestampPackets> timestampAllocator(0, neoDevice->getMemoryManager());
    auto timestampNode = timestampAllocator.getTag();
    timestampNode->initialize();

    event->resetInOrderTimestampNode(timestampNode, 1);
    event->setCbEventWithProfiling(true);

    uint64_t counterStorage = 1;
    NEO::MockGraphicsAllocation counterAllocation(&counterStorage, sizeof(counterStorage));
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->getInOrderExecEventHelper().assignData(1u, 0u, 1u, 1u, &counterAllocation, &counterAllocation, counterAllocation.getGpuAddress(), counterAllocation.getGpuAddress(), &counterStorage, 0u, 0u, false, false);

    auto timestampCompletionAddress = static_cast<uint32_t *>(ptrOffset(timestampNode->getCpuBase(), event->getCompletionFieldOffset()));
    *timestampCompletionAddress = Event::STATE_CLEARED;
    csr.waitUserFenceParams.onWait = [&]() { *timestampCompletionAddress = Event::STATE_SIGNALED; };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, csr.waitUserFenceParams.callCountWithOperation);
    EXPECT_EQ(NEO::UserFenceWaitOperation::notEqual, csr.waitUserFenceParams.latestWaitOperation);
    EXPECT_EQ(castToUint64(timestampCompletionAddress), csr.waitUserFenceParams.latestWaitedAddress);
}

TEST_F(EventSynchronizeTimestampTest, GivenCbEventWithProfilingAndPatchPreambleCounterNotReachedWhenHostSynchronizeThenNotReadyUntilPatchPreambleCounterIsReached) {
    using TimestampPackets = NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>;
    MockTagAllocator<TimestampPackets> timestampAllocator(0, neoDevice->getMemoryManager());
    auto timestampNode = timestampAllocator.getTag();
    timestampNode->initialize();

    event->resetInOrderTimestampNode(timestampNode, 1);
    event->setCbEventWithProfiling(true);

    uint64_t counterStorage = 1;
    NEO::MockGraphicsAllocation counterAllocation(&counterStorage, sizeof(counterStorage));
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->getInOrderExecEventHelper().assignData(1u, 0u, 1u, 1u, &counterAllocation, &counterAllocation, counterAllocation.getGpuAddress(), counterAllocation.getGpuAddress(), &counterStorage, 0u, 0u, false, false);

    constexpr uint64_t patchPreambleCounter = 2;
    uint64_t patchPreambleStorage = 0;
    NEO::MockGraphicsAllocation patchPreambleAllocation(&patchPreambleStorage, sizeof(patchPreambleStorage));
    event->getInOrderExecEventHelper().assignPatchPreambleData(patchPreambleCounter, &patchPreambleStorage, patchPreambleAllocation.getGpuAddress(), &patchPreambleAllocation,
                                                               patchPreambleAllocation.getGpuAddress(), &patchPreambleAllocation);
    ASSERT_TRUE(event->isActiveExternalCbEvent());

    auto timestampCompletionAddress = static_cast<uint32_t *>(ptrOffset(timestampNode->getCpuBase(), event->getCompletionFieldOffset()));
    *timestampCompletionAddress = Event::STATE_SIGNALED;

    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(0));

    patchPreambleStorage = patchPreambleCounter;
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(0));
}

HWTEST_F(EventSynchronizeTimestampTest, GivenTbxModeAndCbEventWithProfilingWhenHostSynchronizeThenPatchPreambleCounterIsReadAfterDownload) {
    using TimestampPackets = NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>;
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    ze_result_t result = ZE_RESULT_SUCCESS;
    eventDesc.index = 1;
    auto tbxEvent = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device, result)));
    ASSERT_NE(nullptr, tbxEvent);

    MockTagAllocator<TimestampPackets> timestampAllocator(0, neoDevice->getMemoryManager());
    auto timestampNode = timestampAllocator.getTag();
    timestampNode->initialize();

    tbxEvent->resetInOrderTimestampNode(timestampNode, 1);
    tbxEvent->setCbEventWithProfiling(true);

    uint64_t counterStorage = 1;
    NEO::MockGraphicsAllocation counterAllocation(&counterStorage, sizeof(counterStorage));
    tbxEvent->getInOrderExecEventHelper().initializeLocalTempStorage();
    tbxEvent->getInOrderExecEventHelper().assignData(1u, 0u, 1u, 1u, &counterAllocation, &counterAllocation, counterAllocation.getGpuAddress(), counterAllocation.getGpuAddress(), &counterStorage, 0u, 0u, false, false);

    constexpr uint64_t patchPreambleCounter = 2;
    uint64_t patchPreambleStorage = 0;
    NEO::MockGraphicsAllocation patchPreambleAllocation(&patchPreambleStorage, sizeof(patchPreambleStorage));
    patchPreambleAllocation.updateTaskCount(1u, ultCsr.getOsContext().getContextId());
    tbxEvent->getInOrderExecEventHelper().assignPatchPreambleData(patchPreambleCounter, &patchPreambleStorage, patchPreambleAllocation.getGpuAddress(), &patchPreambleAllocation,
                                                                  patchPreambleAllocation.getGpuAddress(), &patchPreambleAllocation);
    ASSERT_TRUE(tbxEvent->isActiveExternalCbEvent());

    auto timestampCompletionAddress = static_cast<uint32_t *>(ptrOffset(timestampNode->getCpuBase(), tbxEvent->getCompletionFieldOffset()));
    *timestampCompletionAddress = Event::STATE_SIGNALED;

    ultCsr.initializeResources(device->getDevicePreemptionMode());
    uint32_t patchPreambleDownloads = 0;
    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation, uint64_t, size_t)>> backupCsrDownloadImpl(&ultCsr.downloadAllocationImpl);
    ultCsr.downloadAllocationImpl = [&](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        if (&gfxAllocation == &patchPreambleAllocation) {
            patchPreambleStorage = patchPreambleCounter;
            patchPreambleDownloads++;
        }
    };

    EXPECT_EQ(ZE_RESULT_SUCCESS, tbxEvent->hostSynchronize(0));
    EXPECT_EQ(1u, patchPreambleDownloads);
}

HWTEST_F(EventSynchronizeTimestampTest, GivenTbxModeAndStandaloneCbTimestampEventWithProfilingWhenHostSynchronizeThenTimestampNodeIsDownloaded) {
    using TimestampPacketType = typename FamilyType::TimestampPacketType;
    using TimestampPackets = NEO::TimestampPackets<TimestampPacketType, NEO::TimestampPacketConstants::preferredPacketCount>;
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP;
    ze_event_handle_t handle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    auto standaloneEvent = Event::fromHandle(handle);

    MockTagAllocator<TimestampPackets> timestampAllocator(0, neoDevice->getMemoryManager());
    auto timestampNode = timestampAllocator.getTag();
    timestampNode->initialize();
    standaloneEvent->resetInOrderTimestampNode(timestampNode, 1);
    standaloneEvent->setCbEventWithProfiling(true);

    uint64_t counterStorage = 1;
    NEO::MockGraphicsAllocation counterAllocation(&counterStorage, sizeof(counterStorage));
    standaloneEvent->getInOrderExecEventHelper().assignData(1u, 0u, 1u, 1u, &counterAllocation, &counterAllocation, counterAllocation.getGpuAddress(), counterAllocation.getGpuAddress(), &counterStorage, 0u, 0u, false, false);

    auto nodeAllocation = standaloneEvent->getAllocation(device);
    ASSERT_NE(nullptr, nodeAllocation);
    nodeAllocation->updateTaskCount(1u, ultCsr.getOsContext().getContextId());
    auto timestampCompletionAddress = static_cast<TimestampPacketType *>(ptrOffset(timestampNode->getCpuBase(), standaloneEvent->getCompletionFieldOffset()));

    ultCsr.initializeResources(device->getDevicePreemptionMode());
    uint32_t nodeDownloads = 0;
    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation, uint64_t, size_t)>> backupCsrDownloadImpl(&ultCsr.downloadAllocationImpl);
    ultCsr.downloadAllocationImpl = [&](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        if (&gfxAllocation == nodeAllocation) {
            *timestampCompletionAddress = static_cast<TimestampPacketType>(Event::STATE_SIGNALED);
            nodeDownloads++;
        }
    };

    EXPECT_EQ(ZE_RESULT_SUCCESS, standaloneEvent->hostSynchronize(0));
    EXPECT_EQ(1u, nodeDownloads);

    zeEventDestroy(handle);
}

TEST_F(EventSynchronizeTimestampTest, GivenCompletedCbEventWithProfilingWhenHostSynchronizeThenSuccessIsReturnedWithoutReadingTimestamp) {
    using TimestampPackets = NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>;
    MockTagAllocator<TimestampPackets> timestampAllocator(0, neoDevice->getMemoryManager());
    auto timestampNode = timestampAllocator.getTag();
    timestampNode->initialize();

    event->resetInOrderTimestampNode(timestampNode, 1);
    event->setCbEventWithProfiling(true);
    event->setIsCompleted();

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(0));
}

TEST_F(EventSynchronizeTimestampTest, GivenCounterBasedTimestampEventBackedByNodeWithZeroTotalEventSizeWhenHostSignalThenProfilingCanBeRetrieved) {
    using TimestampPackets = NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>;
    MockTagAllocator<TimestampPackets> timestampAllocator(0, neoDevice->getMemoryManager());
    auto timestampNode = timestampAllocator.getTag();
    timestampNode->initialize();

    event->totalEventSize = 0;
    event->maxPacketCount = 1;
    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->resetInOrderTimestampNode(timestampNode, 1);
    event->resetPacketsUsedCount();

    ASSERT_TRUE(event->isCounterBased());
    ASSERT_TRUE(event->isEventTimestampFlagSet());
    ASSERT_NE(nullptr, event->getAllocation(device));

    event->setGpuStartTimestamp();
    event->setGpuEndTimestamp();

    ASSERT_NE(0u, event->gpuStartTimestamp);
    ASSERT_NE(0u, event->gpuEndTimestamp);

    const auto expectedStart = static_cast<uint32_t>(event->gpuStartTimestamp);
    const auto expectedEnd = static_cast<uint32_t>(event->gpuEndTimestamp);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSignal(true));

    ze_kernel_timestamp_result_t result = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, event->queryKernelTimestamp(&result));

    EXPECT_EQ(expectedStart, result.context.kernelStart);
    EXPECT_EQ(expectedEnd, result.context.kernelEnd);
    EXPECT_EQ(expectedStart, result.global.kernelStart);
    EXPECT_EQ(expectedEnd, result.global.kernelEnd);
}

TEST_F(EventSynchronizeTimestampTest, GivenCounterBasedTimestampEventWithoutTimestampNodeAndWithEventPoolAllocationWhenHostSignalThenTimestampsAreWrittenToEventPoolMemory) {
    using TimestampPackets = NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>;

    event->enableCounterBasedMode(false, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);

    ASSERT_TRUE(event->isCounterBased());
    ASSERT_FALSE(event->hasInOrderTimestampNode());
    ASSERT_NE(nullptr, event->getAllocation(device));

    event->setGpuStartTimestamp();
    event->setGpuEndTimestamp();
    ASSERT_NE(0u, event->gpuStartTimestamp);
    ASSERT_NE(0u, event->gpuEndTimestamp);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSignal(true));

    auto timestampData = static_cast<TimestampPackets *>(event->getHostAddress());
    EXPECT_EQ(static_cast<uint32_t>(event->gpuStartTimestamp), timestampData->getGlobalStartValue(0));
    EXPECT_EQ(static_cast<uint32_t>(event->gpuEndTimestamp), timestampData->getGlobalEndValue(0));
}

HWTEST_F(EventSynchronizeTest, GivenDrmAndKmdWaitStrategyWhenSynchronizingCounterBasedEventThenBoundedUserFenceWaitIsUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);
    constexpr int64_t expectedTimeout = 123456789;
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWaitTimeoutNanoseconds.set(expectedTimeout);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.isWaitUserFenceNotEqualSupportedValue = true;
    event->setLinuxUserFenceKmdWaitEnabled(true);
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = true;

    uint64_t counterStorage = 0;
    NEO::MockGraphicsAllocation counterAllocation(&counterStorage, sizeof(counterStorage));
    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->getInOrderExecEventHelper().assignData(1u, 0u, 1u, 1u, &counterAllocation, &counterAllocation, counterAllocation.getGpuAddress(), counterAllocation.getGpuAddress(), &counterStorage, 0u, 0u, false, false);
    csr.waitUserFenceParams.onWait = [&]() { counterStorage = 1; };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, csr.waitUserFenceParams.callCount);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCountWithOperation);
    EXPECT_EQ(castToUint64(&counterStorage), csr.waitUserFenceParams.latestWaitedAddress);
    EXPECT_EQ(1u, csr.waitUserFenceParams.latestWaitedValue);
    EXPECT_EQ(expectedTimeout, csr.waitUserFenceParams.latestWaitedTimeout);
    EXPECT_FALSE(csr.waitUserFenceParams.userInterrupt);
    EXPECT_EQ(&counterAllocation, csr.waitUserFenceParams.latestAllocForInterruptWait);
}

HWTEST_F(EventSynchronizeTest, GivenDrmAndKmdWaitStrategyWhenFenceWaitIsActiveThenAdditionalUserFenceWaitIsNotUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.isWaitUserFenceNotEqualSupportedValue = true;
    event->setLinuxUserFenceKmdWaitEnabled(true);
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = true;

    uint64_t counterStorage = 0;
    NEO::MockGraphicsAllocation counterAllocation(&counterStorage, sizeof(counterStorage));
    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->enableKmdWaitMode();
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->getInOrderExecEventHelper().assignData(1u, 0u, 1u, 1u, &counterAllocation, &counterAllocation, counterAllocation.getGpuAddress(), counterAllocation.getGpuAddress(), &counterStorage, 0u, 0u, false, false);
    csr.waitUserFenceParams.onWait = [&]() { counterStorage = 1; };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, csr.waitUserFenceParams.callCount);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCountWithOperation);
}

HWTEST_F(EventSynchronizeTest, GivenEventHostSynchronizeWaitStrategyAndWddmOnLinuxWhenHostSynchronizeIsCalledThenAcLineStatusIsNotQueried) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(2);
    NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    reinterpret_cast<NEO::MockRootDeviceEnvironment *>(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get())->isWddmOnLinuxEnable = true;

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getAcLineConnectedCallBase = false;

    event->hostSynchronize(10);
    EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
}

HWTEST_F(EventSynchronizeTest, GivenInitialPollNotElapsedWhenHostSynchronizeIsCalledThenAcLineStatusIsNotQueried) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(2);
    NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(1000000);
    NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(1);
    NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getAcLineConnectedCallBase = false;

    event->hostSynchronize(10);
    EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
}

HWTEST_F(EventSynchronizeTest, GivenEventHostSynchronizeWaitStrategyAndOverflowingCycleThenAcLineStatusIsNotQueried) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(2);
    NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(std::numeric_limits<int64_t>::max());
    NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getAcLineConnectedCallBase = false;

    event->hostSynchronize(10);
    EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
}

HWTEST_F(EventSynchronizeTest, GivenFenceWaitWhenEventHostSynchronizeWaitStrategyIsEnabledThenAcLineStatusIsNotQueried) {
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(2);
    NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.set(1);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getAcLineConnectedCallBase = false;
    csr.isUserFenceWaitSupported = true;
    csr.waitUserFenceParams.forceRetStatusEnabled = true;
    csr.waitUserFenceParams.forceRetStatusValue = false;
    event->enableCounterBasedMode(true, 0u);
    event->enableKmdWaitMode();
    uint64_t counterValue = 0;
    event->inOrderExecHelper.initializeLocalTempStorage();
    event->inOrderExecHelper.assignData(1, 0, 1, 1, event->getAllocation(device), event->getAllocation(device), 0, 0, &counterValue, 0, 0, false, false);

    event->hostSynchronize(10);
    EXPECT_EQ(0u, csr.getAcLineConnectedCalled);
    EXPECT_NE(0u, csr.waitUserFenceParams.callCount);
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

    ze_result_t result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutNonZeroWhenStateSignaledThenHostSynchronizeReturnsSuccess) {
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;

    ze_result_t result = event->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventUsedPacketSignalSynchronizeTest, givenInfiniteTimeoutWhenWaitingForNonTimestampEventCompletionThenReturnOnlyAfterAllEventPacketsAreCompleted) {
    constexpr uint32_t packetsInUse = 2;
    event->setPacketsInUse(packetsInUse);

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
        if (CpuIntrinsicsTests::pauseCounter > 1) {
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
        if (CpuIntrinsicsTests::pauseCounter > 1) {
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
                                                                                 device, result));
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
                                                                                 device, result));
    EXPECT_NE(nullptr, event1);

    uint32_t *hostAddr1 = static_cast<uint32_t *>(event1->getCompletionFieldHostAddress());
    EXPECT_EQ(*hostAddr1, Event::STATE_SIGNALED);

    // create another event from the pool with the same index, but this time, since isImportedIpcPool is false, reset should happen
    eventPool->isImportedIpcPool = false;
    std::unique_ptr<L0::Event> event2;
    event2 = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(),
                                                                                 &eventDesc,
                                                                                 device, result));
    EXPECT_NE(nullptr, event2);

    uint32_t *hostAddr2 = static_cast<uint32_t *>(event2->getCompletionFieldHostAddress());
    EXPECT_EQ(*hostAddr2, Event::STATE_INITIAL);
}

using EventAubCsrTest = Test<DeviceFixture>;

HWTEST_F(EventAubCsrTest, givenCallToEventHostSynchronizeWithAubModeCsrReturnsSuccess) {
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;

    EnvironmentWithCsrWrapper environment;
    environment.setCsrType<MockCsrAub<FamilyType>>();

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
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
    event = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));
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
        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() override {
    }

    NEO::MockMemoryOperationsHandlerTests *mockMemoryOperationsHandler;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
};

using TimestampEventCreate = Test<EventFixture<1, 1>>;
using TimestampEventUsedPacketSignalCreate = Test<EventCompactL3FlushFixture<1, 1, -1>>;

TEST_F(TimestampEventCreate, givenEventCreatedWithTimestampThenIsTimestampEventFlagSet) {
    EXPECT_TRUE(event->isEventTimestampFlagSet());
}

TEST_F(TimestampEventCreate, givenEventTimestampsCreatedWhenResetIsInvokeThenCorrectDataAreSet) {
    for (auto i = 0u; i < NEO::TimestampPacketConstants::preferredPacketCount; i++) {
        EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData.getContextStartValue(i));
        EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData.getGlobalStartValue(i));
        EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData.getContextEndValue(i));
        EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->kernelEventCompletionData.getGlobalEndValue(i));
    }

    EXPECT_EQ(1u, event->kernelEventCompletionData.getPacketsUsed());
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

TEST_F(TimestampEventCreate, givenEventWhenSignaledAndResetFromTheHostThenCorrectDataAreSet) {
    event->hostSignal(false);
    ze_result_t result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    event->reset();
    result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    for (auto i = 0u; i < NEO::TimestampPacketConstants::preferredPacketCount; i++) {
        EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData.getContextStartValue(i));
        EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData.getGlobalStartValue(i));
        EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData.getContextEndValue(i));
        EXPECT_EQ(Event::State::STATE_INITIAL, event->kernelEventCompletionData.getGlobalEndValue(i));
    }
    EXPECT_EQ(1u, event->kernelEventCompletionData.getPacketsUsed());
}

TEST_F(TimestampEventCreate, givenPacketsInUseExceedingCompletionDataCapacityWhenHostSignalThenInvariantIsCaught) {
    // hostEventSetValueTimestamps() feeds assignKernelEventCompletionData() a stack buffer sized for exactly
    // preferredPacketCount packets, so a larger packet count must abort rather than walk off the end of it.
    event->setPacketsInUse(NEO::TimestampPacketConstants::preferredPacketCount + 1);

    EXPECT_THROW(event->hostSignal(false), std::exception);
}

TEST_F(TimestampEventCreate, givenPacketsInUseAtCompletionDataCapacityWhenHostSignalThenAllPacketsAreSignaled) {
    event->setPacketsInUse(NEO::TimestampPacketConstants::preferredPacketCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSignal(false));

    for (auto i = 0u; i < NEO::TimestampPacketConstants::preferredPacketCount; i++) {
        EXPECT_EQ(Event::State::STATE_SIGNALED, event->kernelEventCompletionData.getContextStartValue(i));
        EXPECT_EQ(Event::State::STATE_SIGNALED, event->kernelEventCompletionData.getGlobalStartValue(i));
        EXPECT_EQ(Event::State::STATE_SIGNALED, event->kernelEventCompletionData.getContextEndValue(i));
        EXPECT_EQ(Event::State::STATE_SIGNALED, event->kernelEventCompletionData.getGlobalEndValue(i));
    }
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
    eventRoot = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, L0::Device::fromHandle(rootDeviceHandle), result)));
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
        eventRoot->kernelEventCompletionData.assignDataToAllTimestamps(packetId, eventRoot->hostAddressFromPool);
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
    eventSub0 = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, subdevice, result)));
    ASSERT_NE(nullptr, eventSub0);

    numPackets = 1;
    eventSub0->setPacketsInUse(1u);
    eventSub0->hostAddressFromPool = packetData;

    for (uint32_t packetId = 0; packetId < numPackets; packetId++) {
        eventSub0->kernelEventCompletionData.assignDataToAllTimestamps(packetId, eventSub0->hostAddressFromPool);
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
    eventSub1 = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, subdevice, result)));
    ASSERT_NE(nullptr, eventSub1);

    numPackets = 1;
    eventSub1->setPacketsInUse(1u);
    eventSub1->hostAddressFromPool = packetData;

    for (uint32_t packetId = 0; packetId < numPackets; packetId++) {
        eventSub1->kernelEventCompletionData.assignDataToAllTimestamps(packetId, eventSub1->hostAddressFromPool);
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

using EventMultiRootDeviceTimestampTest = Test<MultiDeviceFixture>;

HWTEST_F(EventMultiRootDeviceTimestampTest, givenCounterBasedTimestampEventWithTimestampNodeFromOtherRootDeviceWhenHostSignalThenKernelTimestampIsReady) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    uint32_t deviceCount = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeDeviceGet(driverHandle.get(), &deviceCount, nullptr));
    ASSERT_LE(2u, deviceCount);
    std::vector<ze_device_handle_t> devices(deviceCount);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeDeviceGet(driverHandle.get(), &deviceCount, devices.data()));
    auto eventDevice = Device::fromHandle(devices[0]);
    auto cmdListDevice = Device::fromHandle(devices[1]);
    eventDevice->getNEODevice()->getRootDeviceEnvironmentRef().osTime.reset(new NEO::MockOSTimeWithConstTimestamp());

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    std::unique_ptr<EventImp<TagSizeT>> event(static_cast<EventImp<TagSizeT> *>(L0::Event::create<TagSizeT>(eventPool.get(), &eventDesc, eventDevice, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    event->enableCounterBasedMode(false, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->resetInOrderTimestampNode(cmdListDevice->getInOrderTimestampAllocator()->getTag(), 1);

    ASSERT_TRUE(event->isCounterBased());
    ASSERT_EQ(nullptr, event->getAllocation(eventDevice));

    event->setGpuStartTimestamp();
    event->setGpuEndTimestamp();
    ASSERT_NE(0u, event->gpuStartTimestamp);
    ASSERT_NE(0u, event->gpuEndTimestamp);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSignal(true));

    ze_kernel_timestamp_result_t timestamp = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, event->queryKernelTimestamp(&timestamp));
    EXPECT_EQ(static_cast<TagSizeT>(event->gpuStartTimestamp), timestamp.global.kernelStart);
    EXPECT_EQ(static_cast<TagSizeT>(event->gpuEndTimestamp), timestamp.global.kernelEnd);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event.release()->destroy());
}

using EventqueryKernelTimestampsExt = Test<EventCompactL3FlushFixture<1, 1, -1>>;

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
    auto event = std::unique_ptr<EventImp<uint64_t>>(static_cast<EventImp<uint64_t> *>(L0::Event::create<uint64_t>(eventPool.get(), &eventDesc, device, result)));
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
    auto event = std::unique_ptr<EventImp<uint64_t>>(static_cast<EventImp<uint64_t> *>(L0::Event::create<uint64_t>(eventPool.get(), &eventDesc, device, result)));
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
    auto event = std::unique_ptr<EventImp<uint64_t>>(static_cast<EventImp<uint64_t> *>(L0::Event::create<uint64_t>(eventPool.get(), &eventDesc, device, result)));
    ASSERT_NE(nullptr, event);

    // Reset before setting
    NEO::TimeStampData *resetReferenceTs = event->peekReferenceTs();
    resetReferenceTs->cpuTimeinNS = 1;
    resetReferenceTs->gpuTimeStamp = 1;

    event->setReferenceTs(expectedTsRefreshIntervalInNanoSec - 2);
    EXPECT_EQ(resetReferenceTs->cpuTimeinNS, 1u);
    EXPECT_EQ(resetReferenceTs->gpuTimeStamp, 1u);
}

HWTEST_F(HostMappedEventTests, givenMappedTimestampEventWhenResettingAllPacketsThenReferenceTimestampIsNotOverwritten) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    auto event = std::unique_ptr<EventImp<uint64_t>>(static_cast<EventImp<uint64_t> *>(L0::Event::create<uint64_t>(eventPool.get(), &eventDesc, device, result)));
    ASSERT_NE(nullptr, event);
    ASSERT_LT(event->getMaxPacketsCount(), NEO::TimestampPacketConstants::preferredPacketCount);

    NEO::TimeStampData *referenceTs = event->peekReferenceTs();
    referenceTs->cpuTimeinNS = 0;
    referenceTs->gpuTimeStamp = 0;

    event->resetDeviceCompletionData(true);

    EXPECT_EQ(0u, referenceTs->cpuTimeinNS);
    EXPECT_EQ(0u, referenceTs->gpuTimeStamp);
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

HWTEST_F(TimestampEventCreate, givenOverflowingTimeStampDataOnMultiplePacketsWhenQueryKernelTimestampIsCalledOverflowIsObserved) {
    typename MockTimestampPackets32::Packet packetData[3] = {};
    event->hostAddressFromPool = packetData;

    // 1st packet - both timestamps overflowing, starts the overflow tracking
    packetData[0].contextStart = 100u;
    packetData[0].contextEnd = 50u;
    packetData[0].globalStart = 1000u;
    packetData[0].globalEnd = 500u;

    // 2nd packet - both timestamps overflowing again, tracked while already overflowed
    packetData[1].contextStart = 200u;
    packetData[1].contextEnd = 60u;
    packetData[1].globalStart = 2000u;
    packetData[1].globalEnd = 600u;

    // 3rd packet - not overflowing, must not extend the end timestamps
    packetData[2].contextStart = 10u;
    packetData[2].contextEnd = 300u;
    packetData[2].globalStart = 20u;
    packetData[2].globalEnd = 3000u;

    event->setPacketsInUse(3u);

    ze_kernel_timestamp_result_t results = {};
    event->queryKernelTimestamp(&results);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    if (gfxCoreHelper.useOnlyGlobalTimestamps() == false) {
        EXPECT_EQ(10u, results.context.kernelStart);
        EXPECT_EQ(60u, results.context.kernelEnd);
    }

    EXPECT_EQ(20u, results.global.kernelStart);
    EXPECT_EQ(600u, results.global.kernelEnd);
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
        expected << "packet: " << i << ", "
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
    auto event = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, L0::Device::fromHandle(rootDeviceHandle), result)));
    ASSERT_NE(nullptr, event);

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(deviceTagAllocator.getTag(), nullptr, *neoDevice, 1, false);

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
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
        event->kernelEventCompletionData.assignDataToAllTimestamps(packetId, event->hostAddressFromPool);
        event->hostAddressFromPool = ptrOffset(event->hostAddressFromPool, NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
    }
    result = event->calculateProfilingData();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(packetData[0].contextStart, event->contextStartTS);
    EXPECT_EQ(packetData[2].contextEnd, event->contextEndTS);
    EXPECT_EQ(packetData[0].globalStart, event->globalStartTS);
    EXPECT_EQ(packetData[2].globalEnd, event->globalEndTS);
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
        event->kernelEventCompletionData.assignDataToAllTimestamps(packetId, event->hostAddressFromPool);
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

TEST_F(TimestampEventUsedPacketSignalCreate, givenNonAggregatedStorageEventWhenCallingQueryTimestampsExpThenAllPacketsAreReported) {
    ASSERT_FALSE(Event::isAggregatedEvent(event.get()));

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
    for (uint32_t packetId = 0; packetId < 2u; packetId++) {
        event->kernelEventCompletionData.assignDataToAllTimestamps(packetId, event->hostAddressFromPool);
        event->hostAddressFromPool = ptrOffset(event->hostAddressFromPool, NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
    }

    // Size query reports the full packet count (not 1 like it would for aggregated storage event).
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryTimestampsExp(device, &count, nullptr));
    EXPECT_EQ(2u, count);

    ze_kernel_timestamp_result_t results[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryTimestampsExp(device, &count, results));
    EXPECT_EQ(2u, count);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(packetData[i].contextStart, results[i].context.kernelStart);
        EXPECT_EQ(packetData[i].contextEnd, results[i].context.kernelEnd);
        EXPECT_EQ(packetData[i].globalStart, results[i].global.kernelStart);
        EXPECT_EQ(packetData[i].globalEnd, results[i].global.kernelEnd);
    }
}

HWTEST_EXCLUDE_PRODUCT(TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet, IGFX_GEN12LP_CORE);

TEST_F(TimestampEventCreate, givenEventWhenQueryKernelTimestampThenNotReadyReturned) {
    struct MockEventQuery : public L0::EventImp<uint32_t> {
        MockEventQuery(int index, L0::Device *device) : EventImp(index, device, false) {}

        using EventImp<uint32_t>::queryStatus;
        ze_result_t queryStatus(int64_t timeSinceWait, bool blockOnMiss) override {
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
        auto deviceImpl = static_cast<Device *>(device);
        for (const auto subdevice : deviceImpl->subDevices) {
            EXPECT_EQ(devices[i], subdevice);
            i++;
        }
    }

    static_cast<Device *>(driverHandle->devices[1])->numSubDevices = 0;
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
        auto deviceImpl = static_cast<Device *>(device);
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

    static_cast<Device *>(driverHandle->devices[1])->numSubDevices = numSubDevices;
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

TEST_F(EventPoolCreateSingleDevice, whenCreatingEventPoolAndPlatformHasDcFlushThenEventPoolIsUncacheable) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.count = 1;

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
    auto mockAllocation = static_cast<MemoryAllocation *>(allocation->getGraphicsAllocations()[0]);
    EXPECT_EQ(mockAllocation->uncacheable, device->getProductHelper().isDcFlushAllowed());
}

struct EventPoolCreateNegativeTest : public ::testing::Test {
    void SetUp() override {

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }
        executionEnvironment->calculateMaxOsContextCount();

        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (uint32_t i = 0; i < numRootDevices; i++) {
            neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, i);
            devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        driverHandle->initialize(std::move(devices));
        static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockEventPoolCreateMemoryManager = true;

        device = driverHandle->devices[0];

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = Context::fromHandle(hContext);
    }
    void TearDown() override {
        context->destroy();
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t numRootDevices = 2u;
    L0::Context *context = nullptr;
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

using EventUsedPacketSignalTests = Test<EventCompactL3FlushFixture<1, 0, -1>>;

TEST_F(EventTests, WhenQueryingStatusThenSuccessIsReturned) {
    ze_result_t returnValue;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, returnValue));
    ASSERT_NE(event, nullptr);

    auto result = event->hostSignal(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);

    event->destroy();
}

TEST_F(EventTests, givenForceHostSignalScopeSetToOneWhenCreateEventWithoutScopeThenHostScopeSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForceHostSignalScope.set(1);

    ze_result_t result;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(event, nullptr);

    EXPECT_TRUE(event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));

    event->destroy();
}

TEST_F(EventTests, givenForceHostSignalScopeSetToZeroWhenCreateEventWithHostScopeThenHostScopeUnset) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForceHostSignalScope.set(0);

    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    ze_result_t result;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(event, nullptr);

    EXPECT_FALSE(event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));

    event->destroy();
}

TEST_F(EventTests, givenAbortHostSyncOnNonHostVisibleEventWhenWaitForNonHostVisibleEventThenAbort) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.AbortHostSyncOnNonHostVisibleEvent.set(true);
    ze_result_t result;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(event, nullptr);

    EXPECT_ANY_THROW(event->hostSynchronize(std::numeric_limits<uint64_t>::max()));

    event->destroy();
}

TEST_F(EventTests, GivenResetWhenQueryingStatusThenNotReadyIsReturned) {
    ze_result_t returnValue;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, returnValue));
    ASSERT_NE(event, nullptr);

    auto result = event->hostSignal(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = event->reset();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_NOT_READY);

    event->destroy();
}

TEST_F(EventTests, WhenDestroyingAnEventThenSuccessIsReturned) {
    ze_result_t result;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(event, nullptr);

    result = event->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventTests, givenTwoEventsCreatedThenTheyHaveDifferentAddresses) {
    ze_event_desc_t eventDesc0 = {};
    eventDesc0.index = 0;
    eventDesc.index = 0;

    ze_event_desc_t eventDesc1 = {};
    eventDesc1.index = 1;
    eventDesc.index = 1;
    ze_result_t result;
    auto event0 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc0, device, result));
    ASSERT_NE(event0, nullptr);

    auto event1 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc1, device, result));
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
    ze_result_t result;
    auto event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(),
                                                                                                                           &eventDesc,
                                                                                                                           device, result)));
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
    ze_result_t result;
    auto event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(),
                                                                                                                           &eventDesc,
                                                                                                                           device, result)));
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

} // namespace ult
} // namespace L0
