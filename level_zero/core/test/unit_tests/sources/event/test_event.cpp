/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_memory_operations_handler.h"
#include "test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {
using EventPoolCreate = Test<DeviceFixture>;
using EventCreate = Test<DeviceFixture>;

TEST_F(EventPoolCreate, GivenEventPoolThenAllocationContainsAtLeast16Bytes) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    uint32_t minAllocationSize = eventPool->getEventSize();
    EXPECT_GE(allocation->getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex())->getUnderlyingBufferSize(),
              minAllocationSize);
}

TEST_F(EventPoolCreate, givenTimestampEventsThenEventSizeSufficientForAllKernelTimestamps) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);
    uint32_t packetsSize = NEO::TimestampPacketSizeControl::preferredPacketCount * sizeof(struct TimestampPacketStorage::Packet);
    uint32_t kernelTimestampsSize = static_cast<uint32_t>(alignUp(packetsSize, MemoryConstants::cacheLineSize));
    EXPECT_EQ(kernelTimestampsSize, eventPool->getEventSize());
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

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);
}

TEST_F(EventPoolCreate, GivenNoDeviceThenEventPoolIsCreated) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        4};

    auto eventPool = EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc);

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
    auto eventPool = EventPool::create(driverHandle.get(), 1, &deviceHandle, &eventPoolDesc);

    ASSERT_NE(nullptr, eventPool);
    eventPool->destroy();
}

TEST_F(EventPoolCreate, givenGetIpcHandleCalledReturnsNotSupported) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle;
    ze_result_t result = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(EventPoolCreate, givenCloseIpcHandleCalledReturnsNotSupported) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    ze_result_t result = eventPool->closeIpcHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(EventPoolCreate, GivenAtLeastOneValidDeviceHandleWhenCreatingEventPoolThenEventPoolCreated) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    ze_device_handle_t devices[] = {nullptr, device->toHandle()};
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), 2, devices, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);
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

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    std::unique_ptr<L0::Event> event(Event::create(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event);
    ASSERT_NE(nullptr, event.get()->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event.get()->csr);
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

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
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

        eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
        ASSERT_NE(nullptr, eventPool);
        event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::EventPool> eventPool = nullptr;
    std::unique_ptr<L0::Event> event;
};

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutZeroAndStateInitialHostSynchronizeReturnsNotReady) {
    ze_result_t result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithNonZeroTimeoutAndStateInitialHostSynchronizeReturnsNotReady) {
    ze_result_t result = event->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutZeroAndStateSignaledHostSynchronizeReturnsSuccess) {
    uint64_t *hostAddr = static_cast<uint64_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;
    ze_result_t result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(EventSynchronizeTest, givenCallToEventHostSynchronizeWithTimeoutNonZeroAndStateSignaledHostSynchronizeReturnsSuccess) {
    uint64_t *hostAddr = static_cast<uint64_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;
    ze_result_t result = event->hostSynchronize(10);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
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

    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);
    event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event);

    ze_result_t result = event->hostSynchronize(10);
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
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;
        eventDesc.wait = 0;

        eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
        ASSERT_NE(nullptr, eventPool);
        event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<L0::Event> event;
};

TEST_F(TimestampEventCreate, givenEventCreatedWithTimestampThenIsTimestampEventFlagSet) {
    EXPECT_TRUE(event->isTimestampEvent);
}

TEST_F(TimestampEventCreate, givenEventTimestampsCreatedWhenResetIsInvokeThenCorrectDataAreSet) {
    EXPECT_NE(nullptr, event->timestampsData);

    for (auto i = 0u; i < NEO::TimestampPacketSizeControl::preferredPacketCount; i++) {
        EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->timestampsData->getContextStartValue(i));
        EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->timestampsData->getGlobalStartValue(i));
        EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->timestampsData->getContextEndValue(i));
        EXPECT_EQ(static_cast<uint64_t>(Event::State::STATE_INITIAL), event->timestampsData->getGlobalEndValue(i));
    }

    EXPECT_EQ(0u, event->getPacketsInUse());
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

    EXPECT_EQ(NEO::GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, allocation->getAllocationType());
}

TEST_F(TimestampEventCreate, givenEventTimestampWhenPacketCountIsIncreasedThenCorrectOffsetIsReturned) {
    EXPECT_EQ(0u, event->getPacketsInUse());
    auto gpuAddr = event->getGpuAddress();
    EXPECT_EQ(gpuAddr, event->getTimestampPacketAddress());

    event->increasePacketsInUse();
    EXPECT_EQ(1u, event->getPacketsInUse());

    gpuAddr += sizeof(TimestampPacketStorage::Packet);
    EXPECT_EQ(gpuAddr, event->getTimestampPacketAddress());
}

HWCMDTEST_F(IGFX_GEN9_CORE, TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet) {
    TimestampPacketStorage::Packet data = {};
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

HWCMDTEST_EXCLUDE_ADDITIONAL_FAMILY(TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet, IGFX_TIGERLAKE_LP);
HWCMDTEST_EXCLUDE_ADDITIONAL_FAMILY(TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet, IGFX_DG1);

TEST_F(TimestampEventCreate, givenEventWhenQueryKernelTimestampThenNotReadyReturned) {
    struct MockEventQuery : public EventImp {
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
                                                               deviceCount,
                                                               devices,
                                                               &eventPoolDesc));
    EXPECT_NE(nullptr, eventPool);

    auto allocation = &eventPool->getAllocation();
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocation->getGraphicsAllocations().size(), numRootDevices);

    delete[] devices;
}

TEST_F(EventPoolCreateMultiDevice, whenCreatingEventPoolWithNoDevicesThenEventPoolCreatedWithOneDevice) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 32;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(),
                                                               0,
                                                               nullptr,
                                                               &eventPoolDesc));
    EXPECT_NE(nullptr, eventPool);

    auto allocation = &eventPool->getAllocation();
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocation->getGraphicsAllocations().size(), 1u);
}

struct EventPoolCreateNegativeTest : public ::testing::Test {
    void SetUp() override {
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
    }
    void TearDown() override {
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t numRootDevices = 2u;
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
                                                               deviceCount,
                                                               devices,
                                                               &eventPoolDesc));
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

    auto eventPool = new L0::EventPoolImp(driverHandle.get(), numRootDevices,
                                          devices, eventPoolDesc.count,
                                          eventPoolDesc.flags);
    EXPECT_NE(nullptr, eventPool);

    result = eventPool->initialize(driverHandle.get(), numRootDevices, devices,
                                   eventPoolDesc.count);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);

    delete eventPool;
    delete[] devices;
}

class EventTests : public DeviceFixture,
                   public testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();

        auto hDevice = device->toHandle();
        eventPool = whitebox_cast(EventPool::create(device->getDriverHandle(), 1, &hDevice, &eventPoolDesc));
    }

    void TearDown() override {
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

TEST_F(EventTests, WhenQueryingStatusThenSuccessIsReturned) {
    auto event = whitebox_cast(Event::create(eventPool, &eventDesc, device));
    ASSERT_NE(event, nullptr);

    auto result = event->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);

    event->destroy();
}

TEST_F(EventTests, GivenResetWhenQueryingStatusThenNotReadyIsReturned) {
    auto event = whitebox_cast(Event::create(eventPool, &eventDesc, device));
    ASSERT_NE(event, nullptr);

    auto result = event->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = event->reset();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);

    event->destroy();
}

TEST_F(EventTests, WhenDestroyingAnEventThenSuccessIsReturned) {
    auto event = whitebox_cast(Event::create(eventPool, &eventDesc, device));
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

    auto event0 = whitebox_cast(Event::create(eventPool, &eventDesc0, device));
    ASSERT_NE(event0, nullptr);

    auto event1 = whitebox_cast(Event::create(eventPool, &eventDesc1, device));
    ASSERT_NE(event1, nullptr);

    EXPECT_NE(event0->hostAddress, event1->hostAddress);
    EXPECT_NE(event0->gpuAddress, event1->gpuAddress);

    event0->destroy();
    event1->destroy();
}

} // namespace ult
} // namespace L0
