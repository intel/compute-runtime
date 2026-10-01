/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/unit_test/os_interface/linux/drm_memory_manager_local_memory_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationThenImportAndExportIoctlAreUsed) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);
        rootDeviceIndices.pushUnique(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    getDrm(0)->outputFd = 7;

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_NE(ptr, nullptr);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        if (i != 0) {
            EXPECT_EQ(getDrm(i)->inputFd, 7);
        }
        ASSERT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        EXPECT_NE(static_cast<DrmAllocation *>(multiGraphics.getGraphicsAllocation(i))->getMmapPtr(), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndImportFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true)->fdToHandleRetVal = -1;
        rootDeviceIndices.pushUnique(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndNoMemoryInfoWhenCreateMultiGraphicsAllocationThenOldPathIsUsed) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
        createDrmForRootDevice(i, false)->fdToHandleRetVal = -1;
        rootDeviceIndices.pushUnique(i);
    }
    auto isLocalMemorySupported = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>().getEnableLocalMemory(hwInfo);
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(isLocalMemorySupported, false, false, *executionEnvironment);

    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        EXPECT_EQ(memoryManager->localMemBanksCount[i], (isLocalMemorySupported ? 1u : 0u));
    }

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_NE(ptr, nullptr);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        ASSERT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        EXPECT_EQ(static_cast<DrmAllocation *>(multiGraphics.getGraphicsAllocation(i))->getMmapPtr(), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenOsHandleWhenCreateIsCalledWithBufferHostMemoryAllocationTypeThenGraphicsAllocationIsReturned) {
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(mock->inputFd, static_cast<int>(osHandleData.handle));
    EXPECT_EQ(mock->setTilingHandle, 0u);

    auto bo = static_cast<DrmAllocation *>(graphicsAllocation)->getBO();
    EXPECT_EQ(bo->peekHandle(), static_cast<int>(mock->outputHandle));
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenSharedHandleImportFailureWhenCreatingUsmHostAllocationFromSharedHandleThenNullptrIsReturnedAndErrorDescriptionIsSet) {
    mock->fdToHandleRetVal = -1;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createUSMHostAllocationFromSharedHandle(1, properties, nullptr, true);
    EXPECT_EQ(ptr, nullptr);

    const char *systemErrorDescription = nullptr;
    executionEnvironment->getErrorDescription(&systemErrorDescription);

    char expectedErrorDescription[256];
    snprintf(expectedErrorDescription, 256, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", mock->fdToHandleRetVal, mock->getErrno(), strerror(mock->getErrno()));
    EXPECT_FALSE(strncmp(expectedErrorDescription, systemErrorDescription, 256));
}

using DrmMemoryManagerUsmSharedHandleTest = DrmMemoryManagerLocalMemoryTest;

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenMemoryInfoWhenCreateUSMHostAllocationFromSharedHandleSucceedsThenAllocationIsReturnedAndRegisteredInSystemMemoryForProperAccounting) {
    setUpSingleDeviceMemoryRegion(*mock);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(1, properties, nullptr, true);
    ASSERT_NE(allocation, nullptr);

    auto &sysMemAllocs = memoryManager->getSysMemAllocs();
    auto it = std::find(sysMemAllocs.begin(), sysMemAllocs.end(), allocation);
    EXPECT_NE(it, sysMemAllocs.end());
    EXPECT_GE(memoryManager->getUsedSystemMemorySize(), allocation->getUnderlyingBufferSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndMappedPtrWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAndSharedHandleIsNonShared) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;
    void *testMappedPtr = reinterpret_cast<void *>(0x12345000);

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, testMappedPtr, false, true);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), sysMemBefore);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndMappedPtrWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosedAndSharedHandleIsPreserved) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    int testFd = 123;
    void *testMappedPtr = reinterpret_cast<void *>(0x12345000);

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto registerBefore = memoryManager->registerSysMemAllocCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, testMappedPtr, false, false);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
    EXPECT_EQ(static_cast<unsigned int>(testFd), allocation->peekSharedHandle());
    EXPECT_EQ(memoryManager->getUsedSystemMemorySize(), sysMemBefore);
    EXPECT_EQ(registerBefore, memoryManager->registerSysMemAllocCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndNoBooMmapWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAndRegistered) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), sysMemBefore);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndNoBooMmapWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosedAndSharedHandlePreserved) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto registerBefore = memoryManager->registerSysMemAllocCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, false);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
    EXPECT_EQ(static_cast<unsigned int>(testFd), allocation->peekSharedHandle());
    EXPECT_EQ(memoryManager->getUsedSystemMemorySize(), sysMemBefore);
    EXPECT_EQ(registerBefore, memoryManager->registerSysMemAllocCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndNewBoCreationWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAfterLseek) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), sysMemBefore);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndNewBoCreationWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosed) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, false);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndReuseSharedAllocationWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAndBoIsReused) {
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd1 = 123;
    int testFd2 = 456;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto allocation1 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd1, properties, nullptr, false, false);
    ASSERT_NE(allocation1, nullptr);

    auto sysMemAfterFirst = memoryManager->getUsedSystemMemorySize();

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    auto allocation2 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd2, properties, nullptr, true, true);

    ASSERT_NE(allocation2, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd2, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation2->peekSharedHandle());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), sysMemAfterFirst);

    auto bo1 = static_cast<DrmAllocation *>(allocation1)->getBO();
    auto bo2 = static_cast<DrmAllocation *>(allocation2)->getBO();
    EXPECT_EQ(bo1, bo2);

    memoryManager->freeGraphicsMemory(allocation1);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndReuseSharedAllocationWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosedAndBoIsReused) {
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd1 = 123;
    int testFd2 = 456;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto allocation1 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd1, properties, nullptr, false, false);
    ASSERT_NE(allocation1, nullptr);

    auto sysMemAfterFirst = memoryManager->getUsedSystemMemorySize();

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    auto registerBefore = memoryManager->registerSysMemAllocCalled;

    auto allocation2 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd2, properties, nullptr, true, false);

    ASSERT_NE(allocation2, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
    EXPECT_EQ(static_cast<unsigned int>(testFd2), allocation2->peekSharedHandle());
    EXPECT_EQ(memoryManager->getUsedSystemMemorySize(), sysMemAfterFirst);
    EXPECT_EQ(registerBefore, memoryManager->registerSysMemAllocCalled);

    memoryManager->freeGraphicsMemory(allocation1);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenDefaultConsumeFdWhenCreateUSMHostAllocationFromSharedHandleCalledThenConsumeFdIsFalse) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
    EXPECT_EQ(static_cast<unsigned int>(testFd), allocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndIoctlFailureWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAndNullptrReturned) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->fdToHandleRetVal = -1;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndIoctlFailureWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosedAndNullptrReturned) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->fdToHandleRetVal = -1;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, false);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndMappedPtrWhenRegisterSysMemAllocFailsThenNullptrReturnedAndBoUnreferenced) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;
    void *testMappedPtr = reinterpret_cast<void *>(0x12345000);

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;
    memoryManager->failRegisterSysMemAlloc = true;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto unreferenceBefore = memoryManager->unreferenceCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, testMappedPtr, false, true);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_GT(memoryManager->unreferenceCalled, unreferenceBefore);
    EXPECT_EQ(sysMemBefore, memoryManager->getUsedSystemMemorySize());
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndNoBooMmapWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;
    memoryManager->failRegisterSysMemAlloc = true;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto unregisterBefore = memoryManager->unregisterAllocationCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(sysMemBefore, memoryManager->getUsedSystemMemorySize());
    EXPECT_EQ(unregisterBefore, memoryManager->unregisterAllocationCalled);
    EXPECT_EQ(0u, memoryManager->callsToCloseSharedHandle);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenNewBoCreationWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    memoryManager->failRegisterSysMemAlloc = true;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto unregisterBefore = memoryManager->unregisterAllocationCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(sysMemBefore, memoryManager->getUsedSystemMemorySize());
    EXPECT_EQ(unregisterBefore, memoryManager->unregisterAllocationCalled);
    EXPECT_EQ(0u, memoryManager->callsToCloseSharedHandle);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndReuseSharedAllocationWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd1 = 123;
    int testFd2 = 456;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto allocation1 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd1, properties, nullptr, false, false);
    ASSERT_NE(allocation1, nullptr);

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    memoryManager->failRegisterSysMemAlloc = true;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto unregisterBefore = memoryManager->unregisterAllocationCalled;

    auto allocation2 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd2, properties, nullptr, true, true);

    EXPECT_EQ(allocation2, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd2, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(sysMemBefore, memoryManager->getUsedSystemMemorySize());
    EXPECT_EQ(unregisterBefore, memoryManager->unregisterAllocationCalled);
    EXPECT_EQ(0u, memoryManager->callsToCloseSharedHandle);

    memoryManager->freeGraphicsMemory(allocation1);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenIsHostIpcAllocationWhenCreateGraphicsAllocationFromSharedHandleCalledThenConsumeFdIsTrue) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    TestedDrmMemoryManager::OsHandleData osHandleData{static_cast<uint64_t>(testFd)};

    mock->outputHandle = 100;

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(
        osHandleData, properties, false, true, false, nullptr);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentWhenCreateMultiGraphicsAllocationThenPrimeFdIsClosedAfterImport) {
    uint32_t rootDevicesNumber = 2u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);
        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    getDrm(0)->outputFd = 7;
    VariableBackup<uint32_t> closeFuncCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeFuncArgBackup(&SysCalls::closeFuncArgPassed, 0);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});
    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(7, SysCalls::closeFuncArgPassed);

    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentWhenCreateMultiGraphicsAllocationSucceedsThenInternalHandleCacheIsCleared) {
    uint32_t rootDevicesNumber = 2u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);
        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    auto drmMock0 = getDrm(0);
    drmMock0->outputFd = 7;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});
    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);
    ASSERT_NE(ptr, nullptr);

    auto primaryAlloc = multiGraphics.getDefaultGraphicsAllocation();
    ASSERT_NE(primaryAlloc, nullptr);

    drmMock0->outputFd = 42;

    uint64_t handleAfterSuccess = 0;
    auto ret = primaryAlloc->peekInternalHandle(memoryManager, handleAfterSuccess, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(42u, handleAfterSuccess);

    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentWhenCreateMultiGraphicsAllocationThenSecondaryAllocationIsRegisteredAndHasNoSharedHandle) {
    uint32_t rootDevicesNumber = 2u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);
        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    getDrm(0)->outputFd = 7;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});
    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);
    ASSERT_NE(ptr, nullptr);

    auto secondaryAlloc = multiGraphics.getGraphicsAllocation(1);
    ASSERT_NE(secondaryAlloc, nullptr);
    EXPECT_EQ(Sharing::nonSharedResource, secondaryAlloc->peekSharedHandle());
    EXPECT_GE(memoryManager->getUsedSystemMemorySize(), sysMemBefore + 2 * size);

    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentWhenCreateMultiGraphicsAllocationFailsOnSecondaryDeviceThenInternalHandleCacheIsClearedAndFreshFdObtained) {
    uint32_t rootDevicesNumber = 2u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        auto drm = createDrmForRootDevice(i, true);
        if (i == 1) {
            drm->fdToHandleRetVal = -1;
        }
        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    auto drmMock0 = getDrm(0);
    drmMock0->outputFd = 7;

    size_t size = 4096u;
    AllocationProperties properties(0u, true, size, AllocationType::bufferHostMemory, false, {});
    properties.flags.forceSystemMemory = true;

    auto primaryAlloc = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    ASSERT_NE(primaryAlloc, nullptr);
    ASSERT_NE(static_cast<DrmAllocation *>(primaryAlloc)->getMmapPtr(), nullptr);

    multiGraphics.addAllocation(primaryAlloc);

    AllocationProperties secondaryProps(1u, true, size, AllocationType::bufferHostMemory, false, {});
    secondaryProps.flags.forceSystemMemory = true;
    secondaryProps.flags.isUSMHostAllocation = true;
    secondaryProps.flags.allocateMemory = false;

    auto result = memoryManager->createGraphicsAllocationFromExistingStorage(
        secondaryProps, reinterpret_cast<void *>(primaryAlloc->getUnderlyingBuffer()), multiGraphics);
    EXPECT_EQ(result, nullptr);

    drmMock0->outputFd = 42;

    uint64_t handleAfterFailure = 0;
    auto ret = primaryAlloc->peekInternalHandle(memoryManager, handleAfterFailure, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(42u, handleAfterFailure);

    memoryManager->freeGraphicsMemory(primaryAlloc);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndObtainFdFromHandleFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);

        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    memoryManager->failOnObtainFdFromHandle = true;
    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);
    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenUncacheableFlagWhenCreatingAllocationFromMultipleSharedHandlesThenGmmUsageTypeIsUncached) {
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::buffer, false, device->getDeviceBitfield());
    properties.flags.uncacheable = true;

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    auto expectedUsageType = CacheSettingsHelper::getGmmUsageType(AllocationType::buffer, true, productHelper, gmmHelper->getHardwareInfo());

    for (uint32_t i = 0; i < handles.size(); i++) {
        auto gmm = graphicsAllocation->getGmm(i);
        ASSERT_NE(nullptr, gmm);
        EXPECT_EQ(expectedUsageType, gmm->getResourceUsageType());
        EXPECT_TRUE(CacheSettingsHelper::isUncachedType(gmm->getResourceUsageType()));
    }

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenSettingNumHandlesThenTheyAreRetrievedCorrectly) {
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    uint32_t numHandlesExpected = 8;
    graphicsAllocation->setNumHandles(numHandlesExpected);
    EXPECT_EQ(graphicsAllocation->getNumHandles(), numHandlesExpected);

    graphicsAllocation->setNumHandles(static_cast<uint32_t>(handles.size()));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesWithSharingResourcesThenDifferentAllocationsAreReturned) {
    mock->ioctlExpected.primeFdToHandle = 4;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;

    auto graphicsAllocationFromReferencedHandle = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle);

    auto graphicsAllocationFromReferencedHandle2 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle2);

    DrmAllocation *drmAllocationFromReferencedHandle = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle);
    auto boFromReferencedHandle = drmAllocationFromReferencedHandle->getBO();
    EXPECT_EQ(boFromReferencedHandle->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle->peekAddress());

    DrmAllocation *drmAllocationFromReferencedHandle2 = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle2);
    auto boFromReferencedHandle2 = drmAllocationFromReferencedHandle2->getBO();
    EXPECT_EQ(boFromReferencedHandle2->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle2->peekAddress());

    EXPECT_EQ(boFromReferencedHandle->peekAddress(), boFromReferencedHandle2->peekAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle2);
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesWithSharingResourcesWithBasePointerThenUnderlyingPointerIsTheSame) {
    mock->ioctlExpected.primeFdToHandle = 4;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;

    auto graphicsAllocationFromReferencedHandle = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle);

    auto graphicsAllocationFromReferencedHandle2 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, reinterpret_cast<void *>(graphicsAllocationFromReferencedHandle->getGpuAddress()));
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle2);

    DrmAllocation *drmAllocationFromReferencedHandle = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle);
    auto boFromReferencedHandle = drmAllocationFromReferencedHandle->getBO();
    EXPECT_EQ(boFromReferencedHandle->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle->peekAddress());

    DrmAllocation *drmAllocationFromReferencedHandle2 = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle2);
    auto boFromReferencedHandle2 = drmAllocationFromReferencedHandle2->getBO();
    EXPECT_EQ(boFromReferencedHandle2->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle2->peekAddress());

    EXPECT_EQ(boFromReferencedHandle->peekAddress(), boFromReferencedHandle2->peekAddress());

    EXPECT_EQ(graphicsAllocationFromReferencedHandle->getGpuAddress(), graphicsAllocationFromReferencedHandle2->getGpuAddress());
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle2);
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesWithNoSharingResourcesThenDifferentAllocationsAreReturned) {
    mock->ioctlExpected.primeFdToHandle = 4;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;

    auto graphicsAllocationFromReferencedHandle = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle);

    auto graphicsAllocationFromReferencedHandle2 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle2);

    DrmAllocation *drmAllocationFromReferencedHandle = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle);
    auto boFromReferencedHandle = drmAllocationFromReferencedHandle->getBO();
    EXPECT_EQ(boFromReferencedHandle->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle->peekAddress());

    DrmAllocation *drmAllocationFromReferencedHandle2 = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle2);
    auto boFromReferencedHandle2 = drmAllocationFromReferencedHandle2->getBO();
    EXPECT_EQ(boFromReferencedHandle2->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle2->peekAddress());

    EXPECT_NE(boFromReferencedHandle->peekAddress(), boFromReferencedHandle2->peekAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle2);
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesAndFindAndReferenceSharedBufferObjectReturnsNonNullThenAllocationSucceeds) {
    mock->ioctlExpected.primeFdToHandle = 4;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());

    auto graphicsAllocationFromReferencedHandle = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle);

    DrmAllocation *drmAllocationFromReferencedHandle = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle);
    auto boFromReferencedHandle = drmAllocationFromReferencedHandle->getBO();
    EXPECT_EQ(boFromReferencedHandle->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle->peekAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesWithOneHandleThenAllocationSucceedsAndGpuAddressIsFromTheExpectedHeap) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<NEO::osHandle> handles{6};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    const bool prefer57bitAddressing = memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended) > 0;
    const auto expectedHeap = prefer57bitAddressing ? HeapIndex::heapExtended : HeapIndex::heapStandard2MB;

    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();

    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(expectedHeap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(expectedHeap)), gpuAddress);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesAndIoctlFailsThenNullIsReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 0;
    mock->ioctlExpected.gemClose = 0;
    mock->failOnPrimeFdToHandle = true;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    EXPECT_EQ(nullptr, graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesThenAllocationSucceeds) {
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWithoutMemoryInfoThenDrmAllocationWithoutMappingIsReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    mock->memoryInfo.reset();

    TestedDrmMemoryManager::OsHandleData osHandleData{99u};
    this->mock->outputHandle = osHandleData.handle;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    EXPECT_EQ(static_cast<int>(osHandleData.handle), static_cast<DrmAllocation *>(graphicsAllocation)->getBO()->peekHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenOsHandleWhenCreatingAllocationFromSharedHandleThenPrimeGemWaitGemCloseAndMmapOffsetIoctlsAreUsed, IsAtMostXeCore) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, whenPrintBOCreateDestroyResultFlagIsSetAndCallToCreateSharedAllocationForHostAllocationThenExpectedMessageIsPrinted, IsAtMostXeCore) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOCreateDestroyResult.set(true);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    StreamCapture capture;
    capture.captureStdout();
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    std::stringstream expectedOutput;
    expectedOutput << "Created BO-0 range: ";
    expectedOutput << std::hex << graphicsAllocation->getGpuAddress();
    expectedOutput << " - ";
    expectedOutput << std::hex << ptrOffset(graphicsAllocation->getGpuAddress(), MemoryConstants::pageSize);
    expectedOutput << ", size: 4096 from PRIME_FD_TO_HANDLE\nCalling gem close on handle: BO-0\n";

    memoryManager->freeGraphicsMemory(graphicsAllocation);

    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(expectedOutput.str(), output);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledForAnExistingAllocationGraphicsAllocationIsReturned, IsAtMostXeCore) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation2);

    EXPECT_NE(nullptr, graphicsAllocation2->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation2->getUnderlyingBufferSize());

    DrmAllocation *drmAllocation2 = static_cast<DrmAllocation *>(graphicsAllocation2);
    auto bo2 = drmAllocation2->getBO();
    EXPECT_EQ(bo2->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo2->peekAddress());
    EXPECT_EQ(2u, bo2->getRefCount());
    EXPECT_EQ(size, bo2->peekSize());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledAndMmapOffsetIoctlFailsThenNullAllocationIsReturned, IsAtMostXeCore) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemMmapOffset = 2;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    mock->returnIoctlExtraErrorValue = true;
    mock->failOnMmapOffset = true;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    EXPECT_EQ(nullptr, graphicsAllocation);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerAndUseMmapObjectSetToFalseThenDrmAllocationWithoutMappingIsReturned) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{99u};
    this->mock->outputHandle = osHandleData.handle;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});
    properties.useMmapObject = false;

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    EXPECT_EQ(static_cast<int>(osHandleData.handle), static_cast<DrmAllocation *>(graphicsAllocation)->getBO()->peekHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, MmapFailWhenUSMHostAllocationFromSharedHandleThenNullPtrReturned) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);

    ASSERT_EQ(nullptr, graphicsAllocation);
}

} // namespace NEO
