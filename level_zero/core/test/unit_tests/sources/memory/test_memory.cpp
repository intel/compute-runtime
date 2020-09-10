/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using MemoryTest = Test<DeviceFixture>;

TEST_F(MemoryTest, givenDevicePointerThenDriverGetAllocPropertiesReturnsDeviceHandle) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = driverHandle->allocDeviceMem(device->toHandle(),
                                                      0u,
                                                      size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = driverHandle->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

using DeviceMemorySizeTest = Test<DeviceFixture>;

TEST_F(DeviceMemorySizeTest, givenSizeGreaterThanLimitThenDeviceAllocationFails) {
    size_t size = neoDevice->getHardwareCapabilities().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = driverHandle->allocDeviceMem(nullptr,
                                                      0u,
                                                      size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
}

TEST_F(MemoryTest, givenSharedPointerThenDriverGetAllocPropertiesReturnsDeviceHandle) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = driverHandle->allocSharedMem(device->toHandle(),
                                                      0u,
                                                      0u,
                                                      size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = driverHandle->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_SHARED);
    EXPECT_EQ(deviceHandle, device->toHandle());

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenHostPointerThenDriverGetAllocPropertiesReturnsNullDevice) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = driverHandle->allocHostMem(0u,
                                                    size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = driverHandle->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);
    EXPECT_EQ(deviceHandle, nullptr);

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenSystemAllocatedPointerThenDriverGetAllocPropertiesReturnsUnknownType) {
    size_t size = 10;
    int *ptr = new int[size];

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;
    ze_result_t result = driverHandle->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_UNKNOWN);

    delete[] ptr;
}

TEST_F(MemoryTest, givenSharedPointerAndDeviceHandleAsNullThenDriverReturnsSuccessAndReturnsPointerToSharedAllocation) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ASSERT_NE(nullptr, device->toHandle());
    ze_result_t result = driverHandle->allocSharedMem(nullptr,
                                                      0u,
                                                      0u,
                                                      size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenNoDeviceWhenAllocatingSharedMemoryThenDeviceInAllocationIsNullptr) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ASSERT_NE(nullptr, device->toHandle());
    ze_result_t result = driverHandle->allocSharedMem(nullptr,
                                                      0u,
                                                      0u,
                                                      size, alignment, &ptr);
    auto alloc = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(alloc->device, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

struct MemoryBitfieldTest : testing::Test {
    void SetUp() override {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        memoryManager = new NEO::MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        neoDevice = new NEO::RootDevice(executionEnvironment, 0);
        static_cast<RootDevice *>(neoDevice)->createDeviceImpl();
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
        memoryManager->recentlyPassedDeviceBitfield = {};

        ASSERT_NE(nullptr, driverHandle->devices[0]->toHandle());
        EXPECT_NE(neoDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
    }

    void TearDown() override {
        auto result = driverHandle->freeMem(ptr);
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    NEO::MockMemoryManager *memoryManager;
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;
};

TEST_F(MemoryBitfieldTest, givenDeviceWithValidBitfieldWhenAllocatingDeviceMemoryThenPassProperBitfield) {
    auto result = driverHandle->allocDeviceMem(device->toHandle(),
                                               0u,
                                               size, alignment, &ptr);
    EXPECT_EQ(neoDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
}
TEST(MemoryBitfieldTests, givenDeviceWithValidBitfieldWhenAllocatingSharedMemoryThenPassProperBitfield) {
    DebugManagerStateRestore restorer;
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (size_t i = 0; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
    }
    auto memoryManager = new NEO::MockMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    NEO::Device *neoDevice0 = new NEO::RootDevice(executionEnvironment, 0);
    static_cast<NEO::RootDevice *>(neoDevice0)->createDeviceImpl();
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    NEO::Device *neoDevice1 = new NEO::RootDevice(executionEnvironment, 1);
    static_cast<NEO::RootDevice *>(neoDevice1)->createDeviceImpl();
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice0));
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice1));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    memoryManager->recentlyPassedDeviceBitfield = {};
    ASSERT_NE(nullptr, driverHandle->devices[1]->toHandle());
    EXPECT_NE(neoDevice0->getDeviceBitfield(), neoDevice1->getDeviceBitfield());
    EXPECT_NE(neoDevice0->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
    auto result = driverHandle->allocSharedMem(nullptr,
                                               ZE_DEVICE_MEMORY_PROPERTY_FLAG_TBD,
                                               0u,
                                               size, alignment, &ptr);
    EXPECT_EQ(neoDevice0->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    memoryManager->recentlyPassedDeviceBitfield = {};
    EXPECT_NE(neoDevice1->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
    result = driverHandle->allocSharedMem(driverHandle->devices[1]->toHandle(),
                                          ZE_DEVICE_MEMORY_PROPERTY_FLAG_TBD,
                                          0u,
                                          size, alignment, &ptr);
    EXPECT_EQ(neoDevice1->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

class MockHostMemoryManager : public NEO::MockMemoryManager {
  public:
    MockHostMemoryManager(const NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)){};

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        allocateGraphicsMemoryWithPropertiesCount++;
        if (forceFailureInPrimaryAllocation) {
            return nullptr;
        }
        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties);
    }

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties,
                                                             const void *ptr) override {
        allocateGraphicsMemoryWithPropertiesCount++;
        if (forceFailureInAllocationWithHostPointer) {
            return nullptr;
        }
        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties, ptr);
    }

    uint32_t allocateGraphicsMemoryWithPropertiesCount = 0;
    bool forceFailureInPrimaryAllocation = false;
    bool forceFailureInAllocationWithHostPointer = false;
};

struct AllocHostMemoryTest : public ::testing::Test {
    void SetUp() override {
        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(),
                                                                                                                                executionEnvironment, i)));
        }

        memoryManager = new MockHostMemoryManager(*devices[0].get()->getExecutionEnvironment());
        devices[0].get()->getExecutionEnvironment()->memoryManager.reset(memoryManager);

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    MockHostMemoryManager *memoryManager = nullptr;
    const uint32_t numRootDevices = 2u;
};

TEST_F(AllocHostMemoryTest,
       whenCallingAllocHostMemThenAllocateGraphicsMemoryWithPropertiesIsCalledTheNumberOfTimesOfRootDevices) {
    void *ptr = nullptr;

    memoryManager->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_result_t result = driverHandle->allocHostMem(0u,
                                                    4096u, 0u, &ptr);
    EXPECT_EQ(memoryManager->allocateGraphicsMemoryWithPropertiesCount, numRootDevices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    driverHandle->freeMem(ptr);
}

TEST_F(AllocHostMemoryTest,
       whenCallingAllocHostMemAndFailingOnCreatingGraphicsAllocationThenNullIsReturned) {

    memoryManager->forceFailureInPrimaryAllocation = true;

    void *ptr = nullptr;

    memoryManager->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_result_t result = driverHandle->allocHostMem(0u,
                                                    4096u, 0u, &ptr);
    EXPECT_EQ(memoryManager->allocateGraphicsMemoryWithPropertiesCount, 1u);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(AllocHostMemoryTest,
       whenCallingAllocHostMemAndFailingOnCreatingGraphicsAllocationWithHostPointerThenNullIsReturned) {

    memoryManager->forceFailureInAllocationWithHostPointer = true;

    void *ptr = nullptr;

    memoryManager->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_result_t result = driverHandle->allocHostMem(0u,
                                                    4096u, 0u, &ptr);
    EXPECT_EQ(memoryManager->allocateGraphicsMemoryWithPropertiesCount, numRootDevices);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(nullptr, ptr);
}

using ContextMemoryTest = Test<ContextFixture>;

TEST_F(ContextMemoryTest, whenAllocatingSharedAllocationFromContextThenAllocationSucceeds) {
    size_t size = 10u;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 0u,
                                                 0u,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenAllocatingHostAllocationFromContextThenAllocationSucceeds) {
    size_t size = 10u;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocHostMem(0u,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenAllocatingDeviceAllocationFromContextThenAllocationSucceeds) {
    size_t size = 10u;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 0u,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenRetrievingAddressRangeForDeviceAllocationThenRangeIsCorrect) {
    size_t allocSize = 4096u;
    size_t alignment = 1u;
    void *allocPtr = nullptr;

    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 0u,
                                                 allocSize, alignment, &allocPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocPtr);

    void *base = nullptr;
    size_t size = 0u;
    void *pPtr = reinterpret_cast<void *>(reinterpret_cast<uint64_t>(allocPtr) + 77);
    result = context->getMemAddressRange(pPtr, &base, &size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(base, allocPtr);
    EXPECT_GE(size, allocSize);

    result = context->freeMem(allocPtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, givenSystemAllocatedPointerThenGetAllocPropertiesReturnsUnknownType) {
    size_t size = 10;
    int *ptr = new int[size];

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;
    ze_result_t result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_UNKNOWN);

    delete[] ptr;
}

} // namespace ult
} // namespace L0
