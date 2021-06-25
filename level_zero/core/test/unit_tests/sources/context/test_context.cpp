/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using MultiDeviceContextTests = Test<MultiDeviceFixture>;

TEST_F(MultiDeviceContextTests,
       whenCreatingContextWithZeroNumDevicesThenAllDevicesAreAssociatedWithTheContext) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    for (size_t i = 0; i < driverHandle->devices.size(); i++) {
        EXPECT_NE(contextImp->getDevices().find(driverHandle->devices[i]->toHandle()), contextImp->getDevices().end());
    }

    res = L0::Context::fromHandle(hContext)->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MultiDeviceContextTests,
       whenCreatingContextWithNonZeroNumDevicesThenOnlySpecifiedDeviceAndItsSubDevicesAreAssociatedWithTheContext) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;

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

    res = driverHandle->createContext(&desc, 1u, &device1, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    uint32_t expectedDeviceCountInContext = 1 + subDeviceCount1;
    EXPECT_EQ(contextImp->getDevices().size(), expectedDeviceCountInContext);

    EXPECT_FALSE(contextImp->isDeviceDefinedForThisContext(L0::Device::fromHandle(device0)));
    for (auto subDevice : subDevices0) {
        EXPECT_FALSE(contextImp->isDeviceDefinedForThisContext(L0::Device::fromHandle(subDevice)));
    }

    EXPECT_TRUE(contextImp->isDeviceDefinedForThisContext(L0::Device::fromHandle(device1)));
    for (auto subDevice : subDevices1) {
        EXPECT_TRUE(contextImp->isDeviceDefinedForThisContext(L0::Device::fromHandle(subDevice)));
    }

    res = L0::Context::fromHandle(hContext)->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MultiDeviceContextTests,
       whenAllocatingDeviceMemoryWithDeviceNotDefinedForContextThenDeviceLostIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;

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
    ze_context_desc_t desc;

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
        EXPECT_NE(memoryProperties.rootDeviceIndices.find(expectedRootDeviceIndexes[0]),
                  memoryProperties.rootDeviceIndices.end());
        EXPECT_NE(memoryProperties.rootDeviceIndices.find(expectedRootDeviceIndexes[1]),
                  memoryProperties.rootDeviceIndices.end());
        return NEO::SVMAllocsManager::createHostUnifiedMemoryAllocation(size, memoryProperties);
    }

    std::vector<uint32_t> expectedRootDeviceIndexes;
};

struct ContextHostAllocTests : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
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
    ze_context_desc_t desc;
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
    ze_context_desc_t desc;
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

using ContextTest = Test<DeviceFixture>;

TEST_F(ContextTest, whenCreatingAndDestroyingContextThenSuccessIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;

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

    EXPECT_CALL(*mockMemoryInterface, makeResident)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_CALL(*mockMemoryInterface, evict)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    context->freeMem(ptr);
}

TEST_F(ContextMakeMemoryResidentTests,
       whenMakingASharedMemoryResidentThenIsAddedToVectorOfResidentAllocations) {
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

    EXPECT_CALL(*mockMemoryInterface, makeResident)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    EXPECT_CALL(*mockMemoryInterface, evict)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t finalSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize, finalSize);

    context->freeMem(ptr);
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

    EXPECT_CALL(*mockMemoryInterface, makeResident)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize, currentSize);

    EXPECT_CALL(*mockMemoryInterface, evict)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
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

    EXPECT_CALL(*mockMemoryInterface, makeResident)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::FAILED));
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
            migratedAddress = ptr;
        }
        uint32_t moveAllocationToGpuDomainCalledTimes = 0;
        void *migratedAddress = nullptr;
    };

    void SetUp() override {
        ContextMakeMemoryResidentTests::SetUp();
        mockMemoryManager = std::make_unique<MockMemoryManager>();
        mockPageFaultManager = new MockResidentTestsPageFaultManager;
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
    MockResidentTestsPageFaultManager *mockPageFaultManager = nullptr;
    NEO::MemoryManager *memoryManager = nullptr;
};

HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingCommandListsWithMigrationThenMemoryFromMakeResidentIsMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    EXPECT_CALL(*mockMemoryInterface, makeResident)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
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
    EXPECT_EQ(mockPageFaultManager->migratedAddress, ptr);

    EXPECT_CALL(*mockMemoryInterface, evict)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    context->freeMem(ptr);
}

HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingCommandListsWithNoMigrationThenMemoryFromMakeResidentIsNotMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    EXPECT_CALL(*mockMemoryInterface, makeResident)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
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
    EXPECT_EQ(mockPageFaultManager->migratedAddress, nullptr);

    EXPECT_CALL(*mockMemoryInterface, evict)
        .WillRepeatedly(testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    context->freeMem(ptr);
}

TEST_F(ContextTest, whenGettingDriverThenDriverIsRetrievedSuccessfully) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));
    L0::DriverHandle *driverHandleFromContext = contextImp->getDriverHandle();
    EXPECT_EQ(driverHandleFromContext, driverHandle.get());

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemInterfacesThenUnsupportedIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 0u;
    void *ptr = nullptr;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingPhysicalMemInterfacesThenUnsupportedIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;

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
    ze_context_desc_t desc;

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_physical_mem_desc_t descMem = {};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    ze_memory_access_attribute_t access = {};
    size_t offset = 0;
    void *ptr = nullptr;
    size_t size = 0;
    res = contextImp->mapVirtualMem(ptr, size, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->setVirtualMemAccessAttribute(ptr, size, access);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    ze_memory_access_attribute_t outAccess = {};
    size_t outSize = 0;
    res = contextImp->getVirtualMemAccessAttribute(ptr, size, &outAccess, &outSize);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->unMapVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using IsAtMostProductDG1 = IsAtMostProduct<IGFX_DG1>;

HWTEST2_F(ContextTest, WhenCreatingImageThenSuccessIsReturned, IsAtMostProductDG1) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;
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
