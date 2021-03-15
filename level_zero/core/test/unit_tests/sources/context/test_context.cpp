/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using ContextGetStatusTest = Test<DeviceFixture>;
TEST_F(ContextGetStatusTest, givenCallToContextGetStatusThenCorrectErrorCodeIsReturnedWhenResourcesHaveBeenReleased) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc;
    ze_result_t res = driverHandle->createContext(&desc, &hContext);
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

    ze_result_t res = driverHandle->createContext(&desc, &hContext);
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

    ze_result_t res = driverHandle->createContext(&desc, &hContext);
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

    ze_result_t res = driverHandle->createContext(&desc, &hContext);
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

    ze_result_t res = driverHandle->createContext(&desc, &hContext);
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

    ze_result_t res = driverHandle->createContext(&desc, &hContext);
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
    ze_result_t res = driverHandle->createContext(&desc, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_image_handle_t image = {};
    ze_image_desc_t imageDesc = {};

    res = contextImp->createImage(device, &imageDesc, &image);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, image);

    Image::fromHandle(image)->destroy();

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

} // namespace ult
} // namespace L0