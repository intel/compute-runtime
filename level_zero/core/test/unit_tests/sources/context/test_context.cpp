/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/common/ult_helpers_l0.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "gtest/gtest.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamily;

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
       whenPeerAllocationForReservedAllocationIsRequestedThenPeerAllocationIsAddedToDeviceMapAndRemovedWhenAllocationIsFreed) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    driverHandle->devices[1]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[1]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(driverHandle->devices[0], size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, 0, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(driverHandle->devices[0], &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;

    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    EXPECT_EQ(driverHandle->getSvmAllocsManager()->allocationsCounter.load(), allocData->getAllocId());
    auto peerAlloc = driverHandle->getPeerAllocation(driverHandle->devices[1], allocData, ptr, &peerGpuAddress, nullptr);
    EXPECT_NE(peerAlloc, nullptr);

    DeviceImp *deviceImp1 = static_cast<DeviceImp *>(driverHandle->devices[1]);

    {
        auto iter = deviceImp1->peerAllocations.allocations.find(ptr);
        EXPECT_NE(iter, deviceImp1->peerAllocations.allocations.end());
    }

    res = contextImp->unMapVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    {
        auto iter = deviceImp1->peerAllocations.allocations.find(ptr);
        EXPECT_EQ(iter, deviceImp1->peerAllocations.allocations.end());
    }

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MultiDeviceContextTests,
       GivenDeviceMemoryWhenMakeResidentCalledOnPeerDeviceThenSuccessReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    driverHandle->devices[1]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[1]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    const size_t size = 4096;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    res = context->allocDeviceMem(driverHandle->devices[0],
                                  &deviceDesc,
                                  size,
                                  0,
                                  &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->makeMemoryResident(driverHandle->devices[1], ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MultiDeviceContextTests,
       GivenInvalidDeviceMemoryWhenMakeResidentCalledOnPeerDeviceThenSuccessReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    driverHandle->devices[1]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[1]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    const size_t size = 4096;
    void *ptr = reinterpret_cast<void *>(0x1234);

    res = contextImp->makeMemoryResident(driverHandle->devices[1], ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MultiDeviceContextTests,
       whenMappingReservedMemoryOnPhysicalMemoryOnMultiDeviceThenSuccessReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    driverHandle->devices[1]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[1]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(driverHandle->devices[0], size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize * 2, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, 0, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(driverHandle->devices[0], &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ze_physical_mem_handle_t secondHalfMem = {};
    res = contextImp->createPhysicalMem(driverHandle->devices[1], &descMem, &secondHalfMem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;

    std::vector<ze_memory_access_attribute_t> memoryAccessFlags = {
        ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE, ZE_MEMORY_ACCESS_ATTRIBUTE_READONLY,
        ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};

    void *offsetAddr =
        reinterpret_cast<void *>(reinterpret_cast<uint64_t>(ptr) + pagesize);

    for (auto accessFlags : memoryAccessFlags) {
        res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, accessFlags);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->mapVirtualMem(offsetAddr, pagesize, secondHalfMem, offset, accessFlags);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->setVirtualMemAccessAttribute(ptr, pagesize, access);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->setVirtualMemAccessAttribute(offsetAddr, pagesize, access);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ze_memory_access_attribute_t outAccess = {};
        size_t outSize = 0;
        res = contextImp->getVirtualMemAccessAttribute(ptr, pagesize, &outAccess, &outSize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(pagesize * 2, outSize);
        res = contextImp->getVirtualMemAccessAttribute(offsetAddr, pagesize, &outAccess, &outSize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(pagesize * 2, outSize);

        res = contextImp->unMapVirtualMem(ptr, pagesize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->unMapVirtualMem(offsetAddr, pagesize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroyPhysicalMem(secondHalfMem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize * 2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
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
    SVMAllocsManagerContextMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
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

        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        auto executionEnvironment = new NEO::ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (size_t i = 0; i < numRootDevices; ++i) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
        }
        auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
        driverHandle = std::make_unique<DriverHandleImp>();
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerContextMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;
        L0UltHelper::initUsmAllocPools(driverHandle.get());

        zeDevices.resize(numberOfDevicesInContext);
        driverHandle->getDevice(&numberOfDevicesInContext, zeDevices.data());

        for (uint32_t i = 0; i < numberOfDevicesInContext; i++) {
            L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>(L0::Device::fromHandle(zeDevices[i]));
            currSvmAllocsManager->expectedRootDeviceIndexes.push_back(deviceImp->getRootDeviceIndex());
        }
    }

    void TearDown() override {
        L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
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

    driverHandle->getSvmAllocsManager()->cleanupUSMAllocCaches();

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

TEST_F(ContextPowerSavingHintTest, givenOsContextPowerHintMaxAndZePowerSavingHintTypeMaxThenTheyAreEqualAndBothAre100) {
    EXPECT_EQ(NEO::OsContext::getUmdPowerHintMax(), ZE_POWER_SAVING_HINT_TYPE_MAX);
    EXPECT_EQ(NEO::OsContext::getUmdPowerHintMax(), 100u);
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

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;

    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::success;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    context->freeMem(ptr);
}

TEST_F(ContextMakeMemoryResidentTests,
       givenValidAllocationwithLockedWhenCallingMakeMemoryResidentThenInvalidArgumentIsReturned) {
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
    auto allocation = driverHandleImp->getDriverSystemMemoryAllocation(ptr, size, neoDevice->getRootDeviceIndex(), nullptr);
    allocation->setLockedMemory(true);

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

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

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::success;
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

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;
    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize, currentSize);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::success;
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

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::failed;

    res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize, currentSize);

    context->freeMem(ptr);
}

TEST_F(ContextMakeMemoryResidentTests, givenDeviceUnifiedMemoryAndLocalOnlyAllocationModeThenCallMakeMemoryResidentImmediately) {
    const size_t size = 4096;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};

    auto *driverHandleImp{static_cast<DriverHandleImp *>(hostDriverHandle.get())};
    driverHandleImp->memoryManager->usmDeviceAllocationMode = NEO::LocalMemAllocationMode::localOnly;
    static_cast<MockMemoryManager *>(driverHandleImp->memoryManager)->returnFakeAllocation = true;
    hostDriverHandle->svmAllocsManager->cleanupUSMAllocCaches();

    EXPECT_EQ(0U, mockMemoryInterface->makeResidentCalled);
    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;
    ze_result_t res1 = context->allocDeviceMem(device->toHandle(),
                                               &deviceDesc,
                                               size,
                                               0,
                                               &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res1);

    auto allocData{driverHandleImp->svmAllocsManager->getSVMAlloc(ptr)};
    EXPECT_NE(allocData, nullptr);
    const bool lmemAllocationModeSupported{allocData->gpuAllocations.getDefaultGraphicsAllocation()->storageInfo.localOnlyRequired};
    EXPECT_EQ(mockMemoryInterface->makeResidentCalled, (lmemAllocationModeSupported ? 1U : 0U));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::outOfMemory;
    ze_result_t res2 = context->allocDeviceMem(device->toHandle(),
                                               &deviceDesc,
                                               size,
                                               0,
                                               &ptr);
    EXPECT_EQ(res2, (lmemAllocationModeSupported ? ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY : ZE_RESULT_SUCCESS));
    EXPECT_EQ(mockMemoryInterface->makeResidentCalled, (lmemAllocationModeSupported ? 2U : 0U));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));
}

TEST_F(ContextMakeMemoryResidentTests, givenNonDeviceUnifiedMemoryWhenAllocDeviceMemCalledThenMakeMemoryResidentIsNotImmediatelyCalled) {
    const size_t size = 4096;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};

    auto *driverHandleImp{static_cast<DriverHandleImp *>(hostDriverHandle.get())};
    driverHandleImp->memoryManager->usmDeviceAllocationMode = NEO::LocalMemAllocationMode::localOnly;
    static_cast<MockMemoryManager *>(driverHandleImp->memoryManager)->returnFakeAllocation = true;

    auto *origSvmAllocsManager{driverHandleImp->svmAllocsManager};
    auto fakeAllocationAddr{reinterpret_cast<void *>(0x1234)};

    MockGraphicsAllocation mockUnifiedAllocation{};
    SvmAllocationData allocData(0U);
    allocData.gpuAllocations.addAllocation(&mockUnifiedAllocation);
    allocData.memoryType = InternalMemoryType::notSpecified;

    MockSVMAllocsManager mockSvmAllocsManager{driverHandleImp->memoryManager};
    mockSvmAllocsManager.createUnifiedMemoryAllocationCallBase = false;
    mockSvmAllocsManager.createUnifiedMemoryAllocationReturnValue = fakeAllocationAddr;
    mockSvmAllocsManager.insertSVMAlloc(fakeAllocationAddr, allocData);
    driverHandleImp->svmAllocsManager = &mockSvmAllocsManager;

    EXPECT_EQ(0U, mockMemoryInterface->makeResidentCalled);
    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;
    ze_result_t res1 = context->allocDeviceMem(device->toHandle(),
                                               &deviceDesc,
                                               size,
                                               0,
                                               &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res1);
    EXPECT_EQ(mockMemoryInterface->makeResidentCalled, 0U);
    driverHandleImp->svmAllocsManager = origSvmAllocsManager;
}

struct ContextMakeMemoryResidentAndMigrationTests : public ContextMakeMemoryResidentTests {
    struct MockResidentTestsPageFaultManager : public MockPageFaultManager {
        void moveAllocationToGpuDomain(void *ptr) override {
            moveAllocationToGpuDomainCalledTimes++;
            migratedAddress = ptr;
        }
        void moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager) override {
            moveAllocationsWithinUMAllocsManagerToGpuDomainCalled++;
        }
        uint32_t moveAllocationToGpuDomainCalledTimes = 0;
        uint32_t moveAllocationsWithinUMAllocsManagerToGpuDomainCalled = 0;
        void *migratedAddress = nullptr;
    };

    void SetUp() override {
        ContextMakeMemoryResidentTests::SetUp();
        mockMemoryManager = std::make_unique<MockMemoryManager>();
        mockPageFaultManager = new MockResidentTestsPageFaultManager;
        svmManager = std::make_unique<MockSVMAllocsManager>(mockMemoryManager.get());

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

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;

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
                                                          false,
                                                          returnValue);
    EXPECT_NE(nullptr, commandQueue);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 0u);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::copy,
                                                                     0u,
                                                                     returnValue, false));
    auto commandListHandle = commandList->toHandle();
    commandList->close();
    res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 1u);
    EXPECT_EQ(mockPageFaultManager->migratedAddress, ptr);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::success;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    context->freeMem(ptr);
}
HWTEST_F(ContextMakeMemoryResidentAndMigrationTests, whenExecutingKernelWithIndirectAccessThenSharedAllocationsAreMigrated) {

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;

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
                                                          false,
                                                          returnValue);
    EXPECT_NE(nullptr, commandQueue);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 0u);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    commandList->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
    commandList->indirectAllocationsAllowed = true;
    commandList->close();

    auto commandListHandle = commandList->toHandle();
    res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true, nullptr, nullptr);

    EXPECT_EQ(mockPageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomainCalled, 1u);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::success;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    context->freeMem(ptr);
}
HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingCommandListsWithNoMigrationThenMemoryFromMakeResidentIsNotMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;

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
                                                          false,
                                                          returnValue);
    EXPECT_NE(nullptr, commandQueue);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 0u);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::copy,
                                                                     0u,
                                                                     returnValue, false));
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 0u);
    EXPECT_EQ(mockPageFaultManager->migratedAddress, nullptr);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::success;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    context->freeMem(ptr);
}

HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingImmediateCommandListsHavingSharedAllocationWithMigrationThenMemoryFromMakeResidentIsMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;

    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t result = ZE_RESULT_SUCCESS;

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4090u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    CmdListMemoryCopyParams copyParams = {};
    int one = 1;
    result = commandList0->appendMemoryFill(dstBuffer, reinterpret_cast<void *>(&one), sizeof(one), 4090u,
                                            nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 1u);
    EXPECT_EQ(mockPageFaultManager->migratedAddress, ptr);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::success;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    context->freeMem(ptr);
    context->freeMem(dstBuffer);
}

HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         GivenImmediateCommandListWhenExecutingRegularCommandListsHavingSharedAllocationWithMigrationOnImmediateThenMemoryFromMakeResidentIsMovedToGpuOnce) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;

    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t result = ZE_RESULT_SUCCESS;

    std::unique_ptr<L0::CommandList> commandListImmediate(CommandList::createImmediate(productFamily,
                                                                                       device,
                                                                                       &desc,
                                                                                       false,
                                                                                       NEO::EngineGroupType::compute,
                                                                                       result));
    ASSERT_NE(nullptr, commandListImmediate);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4090u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily,
                                                                            device,
                                                                            NEO::EngineGroupType::compute,
                                                                            0u,
                                                                            result, false));
    CmdListMemoryCopyParams copyParams = {};
    int one = 1;
    result = commandListRegular->appendMemoryFill(dstBuffer, reinterpret_cast<void *>(&one), sizeof(one), 4090u,
                                                  nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    commandListRegular->close();

    auto cmdListHandle = commandListRegular->toHandle();
    result = commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalledTimes, 1u);
    EXPECT_EQ(mockPageFaultManager->migratedAddress, ptr);

    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::success;
    res = context->evictMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    context->freeMem(ptr);
    context->freeMem(dstBuffer);
}

HWTEST_F(ContextMakeMemoryResidentAndMigrationTests,
         whenExecutingImmediateCommandListsHavingHostAllocationWithMigrationThenMemoryFromMakeResidentIsMovedToGpu) {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(hostDriverHandle.get());
    size_t previousSize = driverHandleImp->sharedMakeResidentAllocations.size();

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::success;

    ze_result_t res = context->makeMemoryResident(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t currentSize = driverHandleImp->sharedMakeResidentAllocations.size();
    EXPECT_EQ(previousSize + 1, currentSize);

    const ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t result = ZE_RESULT_SUCCESS;

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    DebugManagerStateRestore restore;
    debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto sharedPtr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, device);
    EXPECT_NE(nullptr, sharedPtr);

    auto allocation = svmManager->getSVMAlloc(sharedPtr);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);

    auto &commandContainer = commandList0->getCmdContainer();
    commandContainer.addToResidencyContainer(gpuAllocation);
    commandContainer.addToResidencyContainer(allocation->cpuAllocation);

    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, 4096u, 0u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    CmdListMemoryCopyParams copyParams = {};
    int one = 1;
    result = commandList0->appendMemoryFill(dstBuffer, reinterpret_cast<void *>(&one), sizeof(one), 4090u,
                                            nullptr, 0, nullptr, copyParams);
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
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

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

    size = MemoryConstants::pageSize2M - 1000;
    pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pagesize, MemoryConstants::pageSize64k);

    size = MemoryConstants::pageSize2M + 1000;
    pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pagesize, MemoryConstants::pageSize64k);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingPhysicalMemInterfacesThenSuccessIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    size_t size = 1024;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, 0, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingPhysicalMemCreateWithInvalisSizeThenUnsupportedSizeReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    size_t size = 1024;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_DEVICE, 10};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, res);

    descMem.flags = ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_HOST;
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingDestroyPhysicalMemWithIncorrectPointerThenMemoryNotFreed) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    size_t size = 1024;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, 0, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getPhysicalMemoryAllocationMap().size()), 0);

    res = contextImp->destroyPhysicalMem(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getPhysicalMemoryAllocationMap().size()), 0);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(static_cast<int>(driverHandle->getMemoryManager()->getPhysicalMemoryAllocationMap().size()), 0);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingMappingVirtualInterfacesOnPhysicalDeviceMemoryThenSuccessIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_DEVICE, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;

    std::vector<ze_memory_access_attribute_t> memoryAccessFlags = {
        ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE, ZE_MEMORY_ACCESS_ATTRIBUTE_READONLY,
        ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};

    for (auto accessFlags : memoryAccessFlags) {
        res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, accessFlags);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->unMapVirtualMem(ptr, pagesize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }

    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->setVirtualMemAccessAttribute(ptr, pagesize, access);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t outAccess = {};
    size_t outSize = 0;
    res = contextImp->getVirtualMemAccessAttribute(ptr, pagesize, &outAccess, &outSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pagesize, outSize);

    res = contextImp->unMapVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingMappingVirtualInterfacesOnPhysicalDeviceMemoryThenMakeResidentIsCalledWithForcePagingFenceTrue) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    NEO::MockMemoryOperations *mockMemoryInterface = static_cast<NEO::MockMemoryOperations *>(
        device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.get());

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_DEVICE, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    size_t offset = 0;

    std::vector<ze_memory_access_attribute_t> memoryAccessFlags = {
        ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE, ZE_MEMORY_ACCESS_ATTRIBUTE_READONLY,
        ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};

    for (auto accessFlags : memoryAccessFlags) {
        EXPECT_FALSE(mockMemoryInterface->makeResidentForcePagingFenceValue);
        res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, accessFlags);
        EXPECT_TRUE(mockMemoryInterface->makeResidentForcePagingFenceValue);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        res = contextImp->unMapVirtualMem(ptr, pagesize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        mockMemoryInterface->makeResidentForcePagingFenceValue = false;
    }

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingMappingVirtualInterfacesOnPhysicalHostMemoryThenSuccessIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_HOST, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;

    std::vector<ze_memory_access_attribute_t> memoryAccessFlags = {
        ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE, ZE_MEMORY_ACCESS_ATTRIBUTE_READONLY,
        ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};

    for (auto accessFlags : memoryAccessFlags) {
        res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, accessFlags);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->unMapVirtualMem(ptr, pagesize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }

    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->setVirtualMemAccessAttribute(ptr, pagesize, access);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t outAccess = {};
    size_t outSize = 0;
    res = contextImp->getVirtualMemAccessAttribute(ptr, pagesize, &outAccess, &outSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pagesize, outSize);

    res = contextImp->unMapVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

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
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

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
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

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
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

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
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    res = contextImp->freeVirtualMem(ptr, invalidSize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->freeVirtualMem(pStart, pagesize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    const auto maxCpuVa = NEO::CpuInfo::getInstance().getVirtualAddressSize() == 57u ? maxNBitValue(56) : maxNBitValue(47);
    pStart = reinterpret_cast<void *>(maxCpuVa + 0x1234);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    res = contextImp->freeVirtualMem(ptr, invalidSize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

class ReserveMemoryManagerMock : public NEO::MemoryManager {
  public:
    ReserveMemoryManagerMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MemoryManager(executionEnvironment) {}
    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
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
        if (failReserveGpuAddress) {
            return {};
        }
        return AddressRange{requiredStartAddress, size};
    }
    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override {
        if (failReserveGpuAddress) {
            return {};
        }
        return AddressRange{requiredStartAddress, size};
    }
    size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override {
        *heap = HeapIndex::heapStandard;
        return MemoryConstants::pageSize64k;
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override {
        if (failReserveCpuAddress) {
            return {};
        }
        return AddressRange{requiredStartAddress, size};
    }
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
    bool mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override {
        if (failMapVirtualMemory) {
            return false;
        } else {
            return true;
        }
    };
    bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override {
        if (failMapVirtualMemory) {
            return false;
        } else {
            return true;
        }
    };

    NEO::GraphicsAllocation *allocatePhysicalGraphicsMemory(const AllocationProperties &properties) override {

        if (failAllocatePhysicalGraphicsMemory) {
            return nullptr;
        }
        mockAllocation.reset(new NEO::MockGraphicsAllocation(const_cast<void *>(buffer), size));
        return mockAllocation.get();
    }

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override{};
    bool peek32bit() override {
        return this->is32bit;
    }

    bool failReserveGpuAddress = true;
    bool failReserveCpuAddress = true;
    bool failMapVirtualMemory = true;
    bool failAllocatePhysicalGraphicsMemory = true;
    bool is32bit = false;
    void *buffer = nullptr;
    size_t size = 0;
    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
};

HWTEST2_F(ContextTest, whenCallingVirtualMemReserveWithPStartInSvmRangeWithSuccessfulAllocationThenSuccessReturned, IsNotMTL) {
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
    reserveMemoryManager->failReserveCpuAddress = false;
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

TEST_F(ContextTest, whenCallingVirtualMemReserveWithPStartAboveSvmRangeWithSuccessfulAllocationThenSuccessReturned) {
    ze_context_handle_t hContext{};
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    const auto maxCpuVa = NEO::CpuInfo::getInstance().getVirtualAddressSize() == 57u ? maxNBitValue(56) : maxNBitValue(47);
    void *pStart = reinterpret_cast<void *>(maxCpuVa + 0x1234);
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

TEST_F(ContextTest, whenUsingOffsetsIntoReservedVirtualMemoryThenMappingIsSuccessful) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize * 2, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, 0, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;

    std::vector<ze_memory_access_attribute_t> memoryAccessFlags = {
        ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE, ZE_MEMORY_ACCESS_ATTRIBUTE_READONLY,
        ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};

    void *offsetAddr =
        reinterpret_cast<void *>(reinterpret_cast<uint64_t>(ptr) + pagesize);

    for (auto accessFlags : memoryAccessFlags) {
        res = contextImp->mapVirtualMem(offsetAddr, pagesize, mem, offset, accessFlags);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->setVirtualMemAccessAttribute(offsetAddr, pagesize, access);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ze_memory_access_attribute_t outAccess = {};
        size_t outSize = 0;
        res = contextImp->getVirtualMemAccessAttribute(offsetAddr, pagesize, &outAccess, &outSize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(pagesize * 2, outSize);

        res = contextImp->unMapVirtualMem(offsetAddr, pagesize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize * 2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenUsingOffsetsIntoReservedVirtualMemoryWithMultiplePhysicalMemoryThenMappingIsSuccessful) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize * 2, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, 0, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ze_physical_mem_handle_t secondHalfMem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &secondHalfMem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;

    std::vector<ze_memory_access_attribute_t> memoryAccessFlags = {
        ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE, ZE_MEMORY_ACCESS_ATTRIBUTE_READONLY,
        ZE_MEMORY_ACCESS_ATTRIBUTE_NONE};

    void *offsetAddr =
        reinterpret_cast<void *>(reinterpret_cast<uint64_t>(ptr) + pagesize);

    for (auto accessFlags : memoryAccessFlags) {
        res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, accessFlags);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->mapVirtualMem(offsetAddr, pagesize, secondHalfMem, offset, accessFlags);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->setVirtualMemAccessAttribute(ptr, pagesize, access);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->setVirtualMemAccessAttribute(offsetAddr, pagesize, access);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ze_memory_access_attribute_t outAccess = {};
        size_t outSize = 0;
        res = contextImp->getVirtualMemAccessAttribute(ptr, pagesize, &outAccess, &outSize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(pagesize * 2, outSize);
        res = contextImp->getVirtualMemAccessAttribute(offsetAddr, pagesize, &outAccess, &outSize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(pagesize * 2, outSize);

        res = contextImp->unMapVirtualMem(ptr, pagesize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        res = contextImp->unMapVirtualMem(offsetAddr, pagesize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroyPhysicalMem(secondHalfMem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize * 2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST2_F(ContextTest, whenCallingVirtualMemoryReservationWhenOutOfMemoryThenOutOfMemoryReturned, IsNotMTL) {
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
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, res);
    pStart = reinterpret_cast<void *>(0x1234);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, res);
    const auto maxCpuVa = NEO::CpuInfo::getInstance().getVirtualAddressSize() == 57u ? maxNBitValue(56) : maxNBitValue(47);
    pStart = reinterpret_cast<void *>(maxCpuVa + 0x1234);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);
    driverHandle->setMemoryManager(memoryManager);
    delete failingReserveMemoryManager;

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemoryReservationWithInvalidArgumentsThenUnsupportedSizeReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 64u;
    void *ptr = nullptr;
    size_t pagesize = 0u;

    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    NEO::MemoryManager *failingReserveMemoryManager = new ReserveMemoryManagerMock(*neoDevice->executionEnvironment);
    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(failingReserveMemoryManager);
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, res);
    const auto maxCpuVa = NEO::CpuInfo::getInstance().getVirtualAddressSize() == 57u ? maxNBitValue(56) : maxNBitValue(47);
    pStart = reinterpret_cast<void *>(maxCpuVa + 0x1234);
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, res);
    driverHandle->setMemoryManager(memoryManager);
    delete failingReserveMemoryManager;

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemoryReservationWithInvalidMultiPageSizeInArgumentsThenUnsupportedSizeReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 64u;
    void *ptr = nullptr;
    size_t pagesize = 0u;

    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    NEO::MemoryManager *failingReserveMemoryManager = new ReserveMemoryManagerMock(*neoDevice->executionEnvironment);
    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(failingReserveMemoryManager);

    size = pagesize * 3 + 10;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, res);

    size = pagesize * 2 + 1;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, res);

    driverHandle->setMemoryManager(memoryManager);
    delete failingReserveMemoryManager;

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemoryReservationWithValidMultiPageSizeInArgumentsThenSuccessReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 64u;
    void *ptr = nullptr;
    size_t pagesize = 0u;

    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);

    size = pagesize * 3;

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingVirtualMemoryReservationWithOverlappingReservationRangeThenSuccessReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 0;
    void *ptr = nullptr;
    size_t pagesize = 0u;

    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    size = 16 * pagesize;

    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    void *newPStart = addrToPtr(castToUint64(ptr) + pagesize);
    void *newPtr = nullptr;
    res = contextImp->reserveVirtualMem(newPStart, pagesize, &newPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, newPtr);

    EXPECT_NE(ptr, newPtr);

    res = contextImp->freeVirtualMem(newPtr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

class MockCpuInfoOverrideVirtualAddressSize {
  public:
    class MockCpuInfo : public CpuInfo {
      public:
        using CpuInfo::cpuFlags;
        using CpuInfo::virtualAddressSize;
    } *mockCpuInfo = reinterpret_cast<MockCpuInfo *>(const_cast<CpuInfo *>(&CpuInfo::getInstance()));

    MockCpuInfoOverrideVirtualAddressSize(uint32_t newCpuVirtualAddressSize) {
        virtualAddressSizeSave = mockCpuInfo->getVirtualAddressSize();
        mockCpuInfo->virtualAddressSize = newCpuVirtualAddressSize;
    }

    ~MockCpuInfoOverrideVirtualAddressSize() {
        mockCpuInfo->virtualAddressSize = virtualAddressSizeSave;
    }

    uint32_t virtualAddressSizeSave = 0;
};

HWTEST2_F(ContextTest, Given32BitCpuAddressWidthWhenCallingVirtualMemoryReservationCorrectAllocationMethodIsSelected, IsNotMTL) {
    MockCpuInfoOverrideVirtualAddressSize overrideCpuInfo(32);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = addrToPtr(0x1234);
    size_t size = MemoryConstants::pageSize2M;
    void *ptr = nullptr;

    auto reserveMemoryManager = new ReserveMemoryManagerMock(*neoDevice->executionEnvironment);
    reserveMemoryManager->is32bit = true;
    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(reserveMemoryManager);

    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, res);
    reserveMemoryManager->failReserveCpuAddress = false;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    pStart = addrToPtr(maxNBitValue(32) + 0x1234);

    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);
    reserveMemoryManager->failReserveGpuAddress = false;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->setMemoryManager(memoryManager);
    delete reserveMemoryManager;

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST2_F(ContextTest, Given48BitCpuAddressWidthWhenCallingVirtualMemoryReservationCorrectAllocationMethodIsSelected, IsNotMTL) {
    MockCpuInfoOverrideVirtualAddressSize overrideCpuInfo(48);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = addrToPtr(0x1234);
    size_t size = MemoryConstants::pageSize2M;
    void *ptr = nullptr;

    auto reserveMemoryManager = new ReserveMemoryManagerMock(*neoDevice->executionEnvironment);
    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(reserveMemoryManager);

    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, res);
    reserveMemoryManager->failReserveCpuAddress = false;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    pStart = addrToPtr(maxNBitValue(47) + 0x1234);

    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);
    reserveMemoryManager->failReserveGpuAddress = false;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->setMemoryManager(memoryManager);
    delete reserveMemoryManager;

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST2_F(ContextTest, Given57BitCpuAddressWidthWhenCallingVirtualMemoryReservationCorrectAllocationMethodIsSelected, IsNotMTL) {
    MockCpuInfoOverrideVirtualAddressSize overrideCpuInfo(57);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = addrToPtr(0x1234);
    size_t size = MemoryConstants::pageSize2M;
    void *ptr = nullptr;

    auto reserveMemoryManager = new ReserveMemoryManagerMock(*neoDevice->executionEnvironment);
    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(reserveMemoryManager);

    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, res);
    reserveMemoryManager->failReserveCpuAddress = false;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    pStart = addrToPtr(maxNBitValue(56) + 0x1234);

    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);
    reserveMemoryManager->failReserveGpuAddress = false;
    res = contextImp->reserveVirtualMem(pStart, size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->freeVirtualMem(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->setMemoryManager(memoryManager);
    delete reserveMemoryManager;

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingPhysicalMemoryAllocateWhenOutOfMemoryThenOutofMemoryReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    size_t size = 0u;
    size_t pagesize = 0u;

    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    NEO::MemoryManager *failingReserveMemoryManager = new ReserveMemoryManagerMock(*neoDevice->executionEnvironment);
    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(failingReserveMemoryManager);
    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, 0, pagesize};
    ze_physical_mem_handle_t mem = {};
    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);
    driverHandle->setMemoryManager(memoryManager);
    delete failingReserveMemoryManager;

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingMapVirtualDeviceMemoryWithFailedMapThenOutOfMemoryreturned) {
    ze_context_handle_t hContext;
    std::unique_ptr<MockMemoryManager> mockMemoryManager;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    mockMemoryManager = std::make_unique<MockMemoryManager>();

    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(mockMemoryManager.get());

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_DEVICE, pagesize};
    ze_physical_mem_handle_t mem = {};

    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;
    mockMemoryManager->failMapPhysicalToVirtualMemory = true;
    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->setMemoryManager(memoryManager);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingMapVirtualHostMemoryWithFailedMapThenOutOfMemoryreturned) {
    ze_context_handle_t hContext;
    std::unique_ptr<MockMemoryManager> mockMemoryManager;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    mockMemoryManager = std::make_unique<MockMemoryManager>();

    auto memoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(mockMemoryManager.get());

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_HOST, pagesize};
    ze_physical_mem_handle_t mem = {};

    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;
    mockMemoryManager->failMapPhysicalToVirtualMemory = true;
    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->setMemoryManager(memoryManager);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextTest, whenCallingMapVirtualMemoryWithInvalidValuesThenFailureReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    void *pStart = 0x0;
    size_t size = 4096u;
    void *ptr = nullptr;
    size_t pagesize = 0u;
    res = contextImp->queryVirtualMemPageSize(device, size, &pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = contextImp->reserveVirtualMem(pStart, pagesize * 2, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(static_cast<int>(driverHandle->getMemoryManager()->getVirtualMemoryReservationMap().size()), 0);

    ze_physical_mem_desc_t descMem = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC, nullptr, 0, pagesize};
    ze_physical_mem_handle_t mem = {};
    ze_physical_mem_handle_t invalidMem = {};

    res = contextImp->createPhysicalMem(device, &descMem, &mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_memory_access_attribute_t access = {ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE};
    size_t offset = 0;

    res = contextImp->mapVirtualMem(ptr, pagesize, invalidMem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->mapVirtualMem(nullptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->mapVirtualMem(ptr, pagesize * 4, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    void *offsetAddr =
        reinterpret_cast<void *>(reinterpret_cast<uint64_t>(ptr) + pagesize);
    res = contextImp->mapVirtualMem(offsetAddr, pagesize * 2, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->unMapVirtualMem(nullptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->unMapVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->mapVirtualMem(ptr, 0u, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT, res);

    access = ZE_MEMORY_ACCESS_ATTRIBUTE_FORCE_UINT32;
    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, res);

    access = ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE;
    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->mapVirtualMem(ptr, pagesize, mem, offset, access);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    res = contextImp->unMapVirtualMem(ptr, pagesize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->destroyPhysicalMem(mem);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = contextImp->freeVirtualMem(ptr, pagesize * 2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

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

HWTEST_F(ContextTest, givenBindlessModeDisabledWhenMakeImageResidentAndEvictThenImageImplicitArgsAllocationIsNotMadeResidentAndEvicted) {
    if (!device->getNEODevice()->getRootDeviceEnvironment().getReleaseHelper() ||
        !device->getNEODevice()->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.UseBindlessMode.set(0);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto mockMemoryOperationsInterface = new NEO::MockMemoryOperations();
    mockMemoryOperationsInterface->captureGfxAllocationsForMakeResident = true;
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(mockMemoryOperationsInterface);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_image_handle_t image = {};
    ze_image_desc_t imageDesc = {};
    imageDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    res = contextImp->createImage(device, &imageDesc, &image);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, image);

    {
        contextImp->makeImageResident(device, image);
        EXPECT_EQ(1, mockMemoryOperationsInterface->makeResidentCalledCount);
        auto allocIter = std::find(mockMemoryOperationsInterface->gfxAllocationsForMakeResident.begin(),
                                   mockMemoryOperationsInterface->gfxAllocationsForMakeResident.end(),
                                   Image::fromHandle(image)->getImplicitArgsAllocation());
        EXPECT_EQ(mockMemoryOperationsInterface->gfxAllocationsForMakeResident.end(), allocIter);
    }

    {
        contextImp->evictImage(device, image);
        EXPECT_EQ(1, mockMemoryOperationsInterface->evictCalledCount);
        auto allocIter = std::find(mockMemoryOperationsInterface->gfxAllocationsForMakeResident.begin(),
                                   mockMemoryOperationsInterface->gfxAllocationsForMakeResident.end(),
                                   Image::fromHandle(image)->getImplicitArgsAllocation());
        EXPECT_EQ(mockMemoryOperationsInterface->gfxAllocationsForMakeResident.end(), allocIter);
    }

    Image::fromHandle(image)->destroy();

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST_F(ContextTest, givenBindlessImageWhenMakeImageResidentAndEvictThenImageImplicitArgsAllocationIsMadeResidentAndEvicted) {
    if (!device->getNEODevice()->getRootDeviceEnvironment().getReleaseHelper() ||
        !device->getNEODevice()->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.UseBindlessMode.set(1);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto mockMemoryOperationsInterface = new NEO::MockMemoryOperations();
    mockMemoryOperationsInterface->captureGfxAllocationsForMakeResident = true;
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(mockMemoryOperationsInterface);
    auto bindlessHelper = new MockBindlesHeapsHelper(device->getNEODevice(),
                                                     device->getNEODevice()->getNumGenericSubDevices() > 1);
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);
    // Reset ResidentCalledCount to 0 since bindlessHeapsHelper constructor changes the value.
    mockMemoryOperationsInterface->makeResidentCalledCount = 0;

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_image_handle_t image = {};
    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    ze_image_desc_t imageDesc = {};
    imageDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    imageDesc.pNext = &bindlessExtDesc;

    res = contextImp->createImage(device, &imageDesc, &image);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, image);

    {
        contextImp->makeImageResident(device, image);
        EXPECT_EQ(2, mockMemoryOperationsInterface->makeResidentCalledCount);
        auto allocIter = std::find(mockMemoryOperationsInterface->gfxAllocationsForMakeResident.begin(),
                                   mockMemoryOperationsInterface->gfxAllocationsForMakeResident.end(),
                                   Image::fromHandle(image)->getImplicitArgsAllocation());
        EXPECT_NE(mockMemoryOperationsInterface->gfxAllocationsForMakeResident.end(), allocIter);
    }

    {
        contextImp->evictImage(device, image);
        EXPECT_EQ(2, mockMemoryOperationsInterface->evictCalledCount);
        auto allocIter = std::find(mockMemoryOperationsInterface->gfxAllocationsForMakeResident.begin(),
                                   mockMemoryOperationsInterface->gfxAllocationsForMakeResident.end(),
                                   Image::fromHandle(image)->getImplicitArgsAllocation());
        EXPECT_EQ(mockMemoryOperationsInterface->gfxAllocationsForMakeResident.end(), allocIter);
    }

    Image::fromHandle(image)->destroy();

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST_F(ContextTest, givenMakeImageResidentThenMakeImageResidentIsCalledWithForcePagingFenceTrue) {
    if (!device->getNEODevice()->getRootDeviceEnvironment().getReleaseHelper() ||
        !device->getNEODevice()->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    class MockMemoryOperationsForcePagingFenceCheck : public NEO::MockMemoryOperations {
      public:
        MockMemoryOperationsForcePagingFenceCheck() {}

        MemoryOperationsStatus makeResident(NEO::Device *neoDevice, ArrayRef<NEO::GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, bool forcePagingFence) override {
            makeResidentCalledCount++;
            forcePagingFencePassed = forcePagingFence;
            return MemoryOperationsStatus::success;
        }
        bool forcePagingFencePassed = false;
    };

    auto mockMemoryOperationsInterface = new MockMemoryOperationsForcePagingFenceCheck();
    mockMemoryOperationsInterface->captureGfxAllocationsForMakeResident = true;
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(mockMemoryOperationsInterface);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_image_handle_t image = {};
    ze_image_desc_t imageDesc = {};
    imageDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    res = contextImp->createImage(device, &imageDesc, &image);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, image);

    contextImp->makeImageResident(device, image);
    EXPECT_EQ(1, mockMemoryOperationsInterface->makeResidentCalledCount);
    EXPECT_TRUE(mockMemoryOperationsInterface->forcePagingFencePassed);

    Image::fromHandle(image)->destroy();

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

} // namespace ult
} // namespace L0
