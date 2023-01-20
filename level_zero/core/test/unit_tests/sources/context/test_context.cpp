/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using MultiDeviceContextTests = Test<MultiDeviceFixture>;

TEST_F(MultiDeviceContextTests,
       whenCreatingContextWithZeroNumDevicesThenAllDevicesAreAssociatedWithTheContext) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    for (size_t i = 0; i < driverHandle->devices.size(); i++) {
        EXPECT_NE(contextImp->getDevices().find(driverHandle->devices[i]->getRootDeviceIndex()), contextImp->getDevices().end());
    }

    res = L0::Context::fromHandle(hContext)->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MultiDeviceContextTests,
       whenCreatingContextWithNonZeroNumDevicesThenOnlySpecifiedDeviceAndItsSubDevicesAreAssociatedWithTheContext) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_device_handle_t device0 = driverHandle->devices[0]->toHandle();
    DeviceImp *deviceImp0 = static_cast<DeviceImp *>(device0);
    uint32_t subDeviceCount0 = 0;
    ze_result_t res = deviceImp0->getSubDevices(&subDeviceCount0, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(subDeviceCount0, numSubDevices);
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount0);
    res = deviceImp0->getSubDevices(&subDeviceCount0, subDevices0.data());
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    ze_device_handle_t device1 = driverHandle->devices[1]->toHandle();
    DeviceImp *deviceImp1 = static_cast<DeviceImp *>(device1);
    uint32_t subDeviceCount1 = 0;
    res = deviceImp1->getSubDevices(&subDeviceCount1, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(subDeviceCount1, numSubDevices);
    std::vector<ze_device_handle_t> subDevices1(subDeviceCount1);
    res = deviceImp1->getSubDevices(&subDeviceCount1, subDevices1.data());
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    uint32_t subSubDeviceCount1 = 0;
    res = static_cast<DeviceImp *>(subDevices1[0])->getSubDevices(&subSubDeviceCount1, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    res = driverHandle->createContext(&desc, 1u, &device1, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    uint32_t expectedDeviceCountInContext = 1;
    EXPECT_EQ(contextImp->getDevices().size(), expectedDeviceCountInContext);

    EXPECT_FALSE(contextImp->isDeviceDefinedForThisContext(L0::Device::fromHandle(device0)));
    for (auto subDevice : subDevices0) {
        EXPECT_FALSE(contextImp->isDeviceDefinedForThisContext(L0::Device::fromHandle(subDevice)));
    }

    EXPECT_TRUE(contextImp->isDeviceDefinedForThisContext(L0::Device::fromHandle(device1)));
    for (auto subDevice : subDevices1) {
        auto l0SubDevice = static_cast<DeviceImp *>(subDevice);
        EXPECT_TRUE(contextImp->isDeviceDefinedForThisContext(l0SubDevice));

        for (auto &subSubDevice : l0SubDevice->subDevices) {
            EXPECT_TRUE(contextImp->isDeviceDefinedForThisContext(L0::Device::fromHandle(subSubDevice)));
        }
    }

    res = L0::Context::fromHandle(hContext)->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MultiDeviceContextTests,
       whenAllocatingDeviceMemoryWithDeviceNotDefinedForContextThenDeviceLostIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_device_handle_t device = driverHandle->devices[1]->toHandle();

    ze_result_t res = driverHandle->createContext(&desc, 1u, &device, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    size_t size = 4096;
    void *ptr = nullptr;
    res = contextImp->allocDeviceMem(driverHandle->devices[0]->toHandle(), &deviceDesc, size, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);

    res = L0::Context::fromHandle(hContext)->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MultiDeviceContextTests,
       whenAllocatingSharedMemoryWithDeviceNotDefinedForContextThenDeviceLostIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_device_handle_t device = driverHandle->devices[1]->toHandle();

    ze_result_t res = driverHandle->createContext(&desc, 1u, &device, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    size_t size = 4096;
    void *ptr = nullptr;
    res = contextImp->allocSharedMem(driverHandle->devices[0]->toHandle(), &deviceDesc, &hostDesc, size, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);

    res = L0::Context::fromHandle(hContext)->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

struct SVMAllocsManagerContextMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerContextMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager, false) {}
    void *createHostUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties) override {
        EXPECT_EQ(expectedRootDeviceIndexes.size(), memoryProperties.rootDeviceIndices.size());
        EXPECT_NE(std::find(memoryProperties.rootDeviceIndices.begin(), memoryProperties.rootDeviceIndices.end(), expectedRootDeviceIndexes[0]),
                  memoryProperties.rootDeviceIndices.end());
        EXPECT_NE(std::find(memoryProperties.rootDeviceIndices.begin(), memoryProperties.rootDeviceIndices.end(), expectedRootDeviceIndexes[1]),
                  memoryProperties.rootDeviceIndices.end());
        return NEO::SVMAllocsManager::createHostUnifiedMemoryAllocation(size, memoryProperties);
    }

    std::vector<uint32_t> expectedRootDeviceIndexes;
};

struct ContextHostAllocTests : public ::testing::Test {
    void SetUp() override {

        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        auto executionEnvironment = new NEO::ExecutionEnvironment;
        auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
        driverHandle = std::make_unique<DriverHandleImp>();
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerContextMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        zeDevices.resize(numberOfDevicesInContext);
        driverHandle->getDevice(&numberOfDevicesInContext, zeDevices.data());

        for (uint32_t i = 0; i < numberOfDevicesInContext; i++) {
            L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>(L0::Device::fromHandle(zeDevices[i]));
            currSvmAllocsManager->expectedRootDeviceIndexes.push_back(deviceImp->getRootDeviceIndex());
        }
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }

    DebugManagerStateRestore restorer;
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    SVMAllocsManagerContextMock *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    std::vector<ze_device_handle_t> zeDevices;
    const uint32_t numRootDevices = 4u;
    uint32_t numberOfDevicesInContext = 2u;
};

TEST_F(ContextHostAllocTests,
       whenAllocatingHostMemoryOnlyIndexesOfDevicesWithinTheContextAreUsed) {
    L0::ContextImp *context = nullptr;
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc,
                                                  numberOfDevicesInContext,
                                                  zeDevices.data(),
                                                  &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    void *hostPtr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    size_t size = 1024;
    res = context->allocHostMem(&hostDesc, size, 0u, &hostPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, hostPtr);

    res = context->freeMem(hostPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    context->destroy();
}

using ContextGetStatusTest = Test<DeviceFixture>;
TEST_F(ContextGetStatusTest, givenCallToContextGetStatusThenCorrectErrorCodeIsReturnedWhenResourcesHaveBeenReleased) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Context *context = L0::Context::fromHandle(hContext);

    res = context->getStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (auto device : driverHandle->devices) {
        L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
        deviceImp->releaseResources();
    }

    res = context->getStatus();
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);
    context->destroy();
}

using ContextPowerSavingHintTest = Test<DeviceFixture>;
TEST_F(ContextPowerSavingHintTest, givenCallToContextCreateWithPowerHintDescThenPowerHintSetInDriverHandle) {
    ze_context_handle_t hContext;
    ze_context_desc_t ctxtDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ze_context_power_saving_hint_exp_desc_t powerHintContext = {};
    powerHintContext.stype = ZE_STRUCTURE_TYPE_POWER_SAVING_HINT_EXP_DESC;
    powerHintContext.hint = 1;
    ctxtDesc.pNext = &powerHintContext;
    ze_result_t res = driverHandle->createContext(&ctxtDesc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(powerHintContext.hint, driverHandle->powerHint);
    L0::Context *context = L0::Context::fromHandle(hContext);
    context->destroy();
}

TEST_F(ContextPowerSavingHintTest, givenCallToContextCreateWithPowerHintMinimumThenPowerHintSetInDriverHandle) {
    ze_context_handle_t hContext;
    ze_context_desc_t ctxtDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ze_context_power_saving_hint_exp_desc_t powerHintContext = {};
    powerHintContext.stype = ZE_STRUCTURE_TYPE_POWER_SAVING_HINT_EXP_DESC;
    powerHintContext.hint = ZE_POWER_SAVING_HINT_TYPE_MIN;
    ctxtDesc.pNext = &powerHintContext;
    ze_result_t res = driverHandle->createContext(&ctxtDesc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(powerHintContext.hint, driverHandle->powerHint);
    L0::Context *context = L0::Context::fromHandle(hContext);
    context->destroy();
}

TEST_F(ContextPowerSavingHintTest, givenCallToContextCreateWithPowerHintMaximumThenPowerHintSetInDriverHandle) {
    ze_context_handle_t hContext;
    ze_context_desc_t ctxtDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ze_context_power_saving_hint_exp_desc_t powerHintContext = {};
    powerHintContext.stype = ZE_STRUCTURE_TYPE_POWER_SAVING_HINT_EXP_DESC;
    powerHintContext.hint = ZE_POWER_SAVING_HINT_TYPE_MAX;
    ctxtDesc.pNext = &powerHintContext;
    ze_result_t res = driverHandle->createContext(&ctxtDesc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(powerHintContext.hint, driverHandle->powerHint);
    L0::Context *context = L0::Context::fromHandle(hContext);
    context->destroy();
}

TEST_F(ContextPowerSavingHintTest, givenCallToContextCreateWithPowerHintGreaterThanMaxHintThenErrorIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t ctxtDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_context_power_saving_hint_exp_desc_t powerHintContext = {};
    powerHintContext.stype = ZE_STRUCTURE_TYPE_POWER_SAVING_HINT_EXP_DESC;
    powerHintContext.hint = ZE_POWER_SAVING_HINT_TYPE_MAX + 1;
    ctxtDesc.pNext = &powerHintContext;
    ze_result_t res = driverHandle->createContext(&ctxtDesc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, res);
}

TEST_F(ContextPowerSavingHintTest, givenCallToContextCreateWithoutPowerHintDescThenPowerHintIsNotSetInDriverHandle) {
    ze_context_handle_t hContext;
    ze_context_desc_t ctxtDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_scheduling_hint_exp_desc_t invalidExpContext = {};
    invalidExpContext.stype = ZE_STRUCTURE_TYPE_SCHEDULING_HINT_EXP_DESC;
    ctxtDesc.pNext = &invalidExpContext;
    ze_result_t res = driverHandle->createContext(&ctxtDesc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0, driverHandle->powerHint);
    L0::Context *context = L0::Context::fromHandle(hContext);
    context->destroy();
}

using ContextTest = Test<DeviceFixture>;

TEST_F(ContextTest, whenCreatingAndDestroyingContextThenSuccessIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = L0::Context::fromHandle(hContext)->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using ContextMakeMemoryResidentTests = Test<HostPointerManagerFixure>;

TEST_F(ContextMakeMemoryResidentTests,
       givenUknownPointerPassedToMakeMemoryResidentThenInvalidArgumentIsReturned) {
    const size_t size = 4096;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    delete[] ptr;
}

TEST_F(ContextMakeMemoryResidentTests,
       givenValidPointerPassedToMakeMemoryResidentThenSuccessIsReturned) {
    const size_t size = 4096;
    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t res = context->allocSharedMem(device->toHandle(),
                                              &deviceDesc,
                                              &hostDesc,
                                              size,
                                              0,
                                              &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;

    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    context->freeMem(ptr);
}

TEST_F(ContextMakeMemoryResidentTests,
       whenMakingASharedMemoryResidentThenIsAddedToVectorOfResidentAllocationsAndThenRemovedWhenEvicted) {
    const size_t size = 4096;
    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t res = context->allocSharedMem(device->toHandle(),
                                              &deviceDesc,
                                              &hostDesc,
                                              size,
                                              0,
                                              &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t finalSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize, finalSize);

    context->freeMem(ptr);
}

TEST_F(ContextMakeMemoryResidentTests,
       whenMakingASharedMemoryResidentThenIsAddedToVectorOfResidentAllocationsAndThenRemovedWhenFreed) {
    const size_t size = 4096;
    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t res = context->allocSharedMem(device->toHandle(),
                                              &deviceDesc,
                                              &hostDesc,
                                              size,
                                              0,
                                              &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t finalSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize, finalSize);
}

TEST_F(ContextMakeMemoryResidentTests,
       whenMakingADeviceMemoryResidentThenIsNotAddedToVectorOfResidentAllocations) {
    const size_t size = 4096;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                              &deviceDesc,
                                              size,
                                              0,
                                              &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize, currentSize);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    context->freeMem(ptr);
}

TEST_F(ContextMakeMemoryResidentTests,
       whenMakingASharedMemoryResidentButMemoryInterfaceFailsThenIsNotAddedToVectorOfResidentAllocations) {
    const size_t size = 4096;
    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t res = context->allocSharedMem(device->toHandle(),
                                              &deviceDesc,
                                              &hostDesc,
                                              size,
                                              0,
                                              &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::FAILED;

    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize, currentSize);

    context->freeMem(ptr);
}

struct ContextMakeMemoryResidentAndMigrationTests : public ContextMakeMemoryResidentTests {
    struct MockResidentTestsPageFaultManager : public MockPageFaultManager {
        void moveAllocationToGpuDomain(void *ptr) override {
            moveAllocationToGpuDomainCalledTimes++;
            migratedAddress.push_back(ptr);
        }
        void moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager) override {
            moveAllocationsWithinUMAllocsManagerToGpuDomainCalled++;
        }
        uint32_t moveAllocationToGpuDomainCalledTimes = 0;
        uint32_t moveAllocationsWithinUMAllocsManagerToGpuDomainCalled = 0;
        std::vector<void *> migratedAddress;
    };

    void SetUp() override {
        ContextMakeMemoryResidentTests::SetUp();
        mockMemoryManager = std::make_unique<MockMemoryManager>();
        mockPageFaultManager = new MockResidentTestsPageFaultManager;
        svmManager = std::make_unique<MockSVMAllocsManager>(mockMemoryManager.get(), false);

        mockMemoryManager->pageFaultManager.reset(mockPageFaultManager);
        memoryManager = device->getDriverHandle()->getMemoryManager();
        device->getDriverHandle()->setMemoryManager(mockMemoryManager.get());

        ze_host_mem_alloc_desc_t hostDesc = {};
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_result_t res = context->allocSharedMem(device->toHandle(),
                                                  &deviceDesc,
                                                  &hostDesc,
                                                  size,
                                                  0,
                                                  &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }

    void TearDown() override {
        device->getDriverHandle()->setMemoryManager(memoryManager);
        ContextMakeMemoryResidentTests::TearDown();
    }

    const size_t size = 4096;
    void *ptr = nullptr;

    std::unique_ptr<MockMemoryManager> mockMemoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    MockResidentTestsPageFaultManager *mockPageFaultManager = nullptr;
    NEO::MemoryManager *memoryManager = nullptr;
};

HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingCommandListsWithMigrationThenMemoryFromMakeResidentIsMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;

    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.createKernelArgsBufferAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    EXPECT_NE(nullptr, commandQueue);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 0u);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::Copy,
                                                                     0u,
                                                                     returnValue));
    auto commandListHandle = commandList->toHandle();
    res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 1u);
    EXPECT_EQ(mockPageFaultManager->migratedAddress[0], ptr);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    context->freeMem(ptr);
}
HWTEST2_F(ContextMakeMemoryResidentAndMigrationTests,
          whenExecutingKernelWithIndirectAccessThenSharedAllocationsAreMigrated, IsAtLeastSkl) {

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;

    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.createKernelArgsBufferAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    EXPECT_NE(nullptr, commandQueue);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 0u);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    commandList->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
    commandList->indirectAllocationsAllowed = true;
    commandList->close();

    auto commandListHandle = commandList->toHandle();
    res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true);

    EXPECT_EQ(mockPageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomainCalled, 1u);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    context->freeMem(ptr);
}
HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingCommandListsWithNoMigrationThenMemoryFromMakeResidentIsNotMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;

    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.createKernelArgsBufferAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    EXPECT_NE(nullptr, commandQueue);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 0u);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::Copy,
                                                                     0u,
                                                                     returnValue));
    auto commandListHandle = commandList->toHandle();
    res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 0u);
    EXPECT_EQ(mockPageFaultManager->migratedAddress.empty(), true);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    context->freeMem(ptr);
}

HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingImmediateCommandListsHavingSharedAllocationWithMigrationThenMemoryFromMakeResidentIsMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;

    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t result = ZE_RESULT_SUCCESS;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4090u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    int one = 1;
    result = commandList0->appendMemoryFill(dstBuffer, reinterpret_cast<void *>(&one), sizeof(one), 4090u,
                                            nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 2u);

    if (mockPageFaultManager->migratedAddress[0] != ptr) {
        EXPECT_EQ(mockPageFaultManager->migratedAddress[1], ptr);
    }

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    context->freeMem(ptr);
    context->freeMem(dstBuffer);
}

HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingImmediateCommandListsHavingHostAllocationWithMigrationThenMemoryFromMakeResidentIsMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;

    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t result = ZE_RESULT_SUCCESS;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    DebugManagerStateRestore restore;
    DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    auto sharedPtr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, device);
    EXPECT_NE(nullptr, sharedPtr);

    auto allocation = svmManager->getSVMAlloc(sharedPtr);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);

    auto &commandContainer = commandList0->commandContainer;
    commandContainer.addToResidencyContainer(gpuAllocation);
    commandContainer.addToResidencyContainer(allocation->cpuAllocation);

    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, 4096u, 0u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    int one = 1;
    result = commandList0->appendMemoryFill(dstBuffer, reinterpret_cast<void *>(&one), sizeof(one), 4090u,
                                            nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 3u);

    context->freeMem(ptr);
    svmManager->freeSVMAlloc(sharedPtr);
    context->freeMem(dstBuffer);
}

TEST_F(ContextTest, whenGettingDriverThenDriverIsRetrievedSuccessfully) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));
    L0::DriverHandle *driverHandleFromContext = contextImp->getDriverHandle();
    EXPECT_EQ(driverHandleFromContext, driverHandle.get());

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemInterfacesThenSuccessIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT((int)driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size(), 0);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingQueryVirtualMemPageSizeCorrectAlignmentIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    size_t size = 1024;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pagesize, MemoryConstants::pageSize64k);

    size = MemoryConstants::pageSize2Mb - 1000;
    pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pagesize, MemoryConstants::pageSize2Mb / 2);

    size = MemoryConstants::pageSize2Mb + 1000;
    pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pagesize, MemoryConstants::pageSize2Mb);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingPhysicalMemInterfacesThenUnsupportedIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_physical_mem_desc_t descMem = {};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingMappingVirtualInterfacesThenUnsupportedIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT((int)driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size(), 0);

    ze_physical_mem_desc_t descMem = {};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    ze_memory_access_attribute_t access = {};
    size_t offset = 0;
    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->setVirtualMemAccessAttribute(ptr, pagesize, access);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t outAccess = {};
    size_t outSize = 0;
    res = contextImp->getVirtualMemAccessAttribute(ptr, pagesize, &outAccess, &outSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pagesize, outSize);

    res = contextImp->unMapVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemorySetAttributeWithValidEnumerationsThenSuccessisReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT((int)driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size(), 0);

    std::vector<ze_memory_access_attribute_t> memoryAccessFlags = {
        ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE, ZE_MEMORY_ACCESS_ATTRIBUTE_READONLY,
        ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};
    ze_memory_access_attribute_t previousAccess = {};
    size_t outSize = 0;
    for (auto accessFlags : memoryAccessFlags) {
        previousAccess = access;
        res = contextImp->setVirtualMemAccessAttribute(ptr, pagesize, accessFlags);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->getVirtualMemAccessAttribute(ptr, pagesize, &access, &outSize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(outSize, pagesize);
        EXPECT_NE(previousAccess, access);
        EXPECT_EQ(accessFlags, access);
    }

    res = contextImp->getVirtualMemAccessAttribute(ptr, pagesize, &access, &outSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemorySetAttributeWithInvalidValuesThenFailureReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT((int)driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size(), 0);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_FORCE_UINT32};
    res = contextImp->setVirtualMemAccessAttribute(ptr, pagesize, access);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, res);

    res = contextImp->setVirtualMemAccessAttribute(pStart, pagesize, access);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemoryGetAttributeWithInvalidValuesThenFailureReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT((int)driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size(), 0);

    size_t outSize = 0;
    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};
    res = contextImp->getVirtualMemAccessAttribute(pStart, pagesize, &access, &outSize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemoryFreeWithInvalidValuesThenFailuresReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    size_t invalidSize = 10u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT((int)driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size(), 0);

    res = contextImp->freeVirtualMem(ptr, invalidSize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->freeVirtualMem(pStart, pagesize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemoryReservationWithInvalidArgumentsThenFailureReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 64u;
    void *ptr = nullptr;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, res);
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    pStart = reinterpret_cast<void *>(0x1234);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

class ReserveMemoryManagerMock : public NEO::MemoryManager {
  public:
    ReserveMemoryManagerMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MemoryManager(executionEnvironment) {}
    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) override { return nullptr; }
    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) override { return nullptr; }
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
    AddressRange reserveGpuAddress(const void *requiredStartAddress, size_t size, RootDeviceIndicesContainer rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override {
        if (failReserveGpuAddress) {
            return {};
        }
        return AddressRange{reinterpret_cast<uint64_t>(requiredStartAddress), size};
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

    bool failReserveGpuAddress = true;
};

TEST_F(ContextTest, whenCallingVirtualMemReserveWithPStartWithSuccessfulAllocationThenSuccessReturned) {
    ze_context_handle_t hContext{};
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = reinterpret_cast<void *>(0x1234);
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;

    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto reserveMemoryManager = std::make_unique<ReserveMemoryManagerMock>(*neoDevice->executionEnvironment);
    auto memoryManager = driverHandle->getMemoryManager();
    reserveMemoryManager->failReserveGpuAddress = false;
    driverHandle->setMemoryManager(reserveMemoryManager.get());
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(reserveMemoryManager->getVirtualMemoryReservationMap().size(), 0u);
    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    driverHandle->setMemoryManager(memoryManager);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemoryReservationWhenOutOfMemoryThenOutOfMemoryReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 0u;
    void *ptr = nullptr;
    size_t pagesize = 0u;

    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    NEO::MemoryManager *failingReserveMemoryManager = new ReserveMemoryManagerMock(*neoDevice->executionEnvironment);
    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(failingReserveMemoryManager);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);
    pStart = reinterpret_cast<void *>(0x1234);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);
    driverHandle->setMemoryManager(memoryManager);
    delete failingReserveMemoryManager;

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using IsAtMostProductDG1 = IsAtMostProduct<IGFX_DG1>;

HWTEST2_F(ContextTest, WhenCreatingImageThenSuccessIsReturned, IsAtMostProductDG1) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_image_handle_t image = {};
    ze_image_desc_t imageDesc = {};
    imageDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    res = contextImp->createImage(device, &imageDesc, &image);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, image);

    Image::fromHandle(image)->destroy();

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

} // namespace ult
} // namespace L0
