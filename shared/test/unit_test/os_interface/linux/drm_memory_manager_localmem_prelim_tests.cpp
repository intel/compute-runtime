/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_prelim_fixtures.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

TEST_F(DrmMemoryManagerLocalMemoryWithCustomPrelimMockTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnBufferObjectThenReturnPtr) {
    BufferObject bo(0, mock, 3, 1, 1024, 1);

    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager->lockBufferObject(&bo);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());

    memoryManager->unlockBufferObject(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenDrmMemoryManagerWithPrelimSupportWhenCreateBufferObjectInMemoryRegionIsCalledThenBufferObjectWithAGivenGpuAddressAndSizeIsCreatedAndAllocatedInASpecifiedMemoryRegion) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize64k;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(0u,
                                                                                            nullptr,
                                                                                            AllocationType::buffer,
                                                                                            gpuAddress,
                                                                                            size,
                                                                                            (1 << (MemoryBanks::getBankForLocalMemory(0) - 1)),
                                                                                            1,
                                                                                            -1,
                                                                                            false,
                                                                                            false));
    ASSERT_NE(nullptr, bo);

    EXPECT_EQ(1u, mock->ioctlCallsCount);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);
    EXPECT_EQ(size, createExt.size);

    const auto &regionParam = createExt.setParamExt.value();
    EXPECT_EQ(0u, regionParam.handle);
    EXPECT_EQ(1u, regionParam.size);
    EXPECT_EQ(DrmPrelimHelper::getMemoryRegionsParamFlag(), regionParam.param);

    const auto &memRegions = createExt.memoryRegions;
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, memRegions.at(0).memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, memRegions.at(0).memoryInstance);

    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(size, bo->peekSize());
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationThenImportAndExportIoctlAreUsed) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        auto mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[i]);

        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

        mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
        mock->engineInfoQueried = false;
        mock->queryEngineInfo();
        mock->ioctlCallsCount = 0;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();

        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    static_cast<DrmQueryMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>())->outputFd = 7;

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_NE(ptr, nullptr);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        if (i != 0) {
            EXPECT_EQ(static_cast<DrmQueryMock *>(executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Drm>())->inputFd, 7);
        }
        EXPECT_NE(static_cast<DrmAllocation *>(multiGraphics.getGraphicsAllocation(i))->getMmapPtr(), nullptr);
        EXPECT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndObtainFdFromHandleFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        auto mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[i]);

        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

        mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
        mock->engineInfoQueried = false;
        mock->queryEngineInfo();
        mock->ioctlCallsCount = 0;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();

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

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndImportFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        auto mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[i]);

        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

        mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
        mock->engineInfoQueried = false;
        mock->queryEngineInfo();
        mock->ioctlCallsCount = 0;
        mock->fdToHandleRetVal = -1;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();

        rootDeviceIndices.pushUnique(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

using DrmMemoryManagerUsmSharedHandlePrelimTest = DrmMemoryManagerLocalMemoryPrelimTest;

TEST_F(DrmMemoryManagerUsmSharedHandlePrelimTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndImportFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 1u;
    uint32_t rootDeviceIndex = 0u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    auto mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;
    mock->fdToHandleRetVal = -1;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createUSMHostAllocationFromSharedHandle(1, properties, nullptr, true);

    const char *systemErrorDescription = nullptr;
    executionEnvironment->getErrorDescription(&systemErrorDescription);

    char expectedErrorDescription[256];
    snprintf(expectedErrorDescription, 256, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", mock->fdToHandleRetVal, mock->getErrno(), strerror(mock->getErrno()));

    EXPECT_FALSE(strncmp(expectedErrorDescription, systemErrorDescription, 256));

    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMultiRootDeviceEnvironmentAndNoMemoryInfoWhenCreateMultiGraphicsAllocationThenOldPathIsUsed) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        auto mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[i]);

        mock->memoryInfo.reset(nullptr);
        mock->engineInfoQueried = false;
        mock->queryEngineInfo();
        mock->ioctlCallsCount = 0;
        mock->fdToHandleRetVal = -1;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();

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
        EXPECT_EQ(static_cast<DrmAllocation *>(multiGraphics.getGraphicsAllocation(i))->getMmapPtr(), nullptr);
        EXPECT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, whenCreateUnifiedMemoryAllocationThenGemCreateExtIsUsed) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest,
       whenCreateUnifiedMemoryAllocationWithChunkingThenGemCreateExtAndPreferredLocationAreUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(1);
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    mock->chunkingAvailable = true;
    mock->callBaseIsChunkingAvailable = true;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::chunkThreshold,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest,
       whenCreateUnifiedMemoryAllocationWithChunkingAndModeNotSetToSharedThenChunkingIsNotUsed) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    mock->chunkingAvailable = true;
    mock->callBaseIsChunkingAvailable = true;

    mock->chunkingMode = 0x02;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::chunkThreshold,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest,
       whenCreateUnifiedMemoryAllocationWithChunkingAndSizeLessThanMinimalThenChunkingIsNotUsed) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    mock->chunkingAvailable = true;
    mock->callBaseIsChunkingAvailable = true;

    mock->chunkingMode = 0x01;

    AllocationProperties gpuProperties{0u,
                                       mock->minimalChunkingSize / 2,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest,
       whenCreateUnifiedMemoryAllocationWithChunkingModeSetToSharedAndSizeGreaterThanMinimalThenChunkingIsUsed) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    mock->chunkingAvailable = true;
    mock->callBaseIsChunkingAvailable = true;

    mock->chunkingMode = 0x01;

    AllocationProperties gpuProperties{0u,
                                       mock->minimalChunkingSize * 2,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);
    EXPECT_TRUE(allocation->storageInfo.isChunked);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest,
       whenCreateUnifiedMemoryAllocationWithChunkingModeSetToSharedAndSizeGreaterThanMinimalWithDebuggingEnabledThenChunkingIsNotUsed) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    mock->chunkingAvailable = true;
    mock->callBaseIsChunkingAvailable = true;

    mock->chunkingMode = 0x01;

    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    AllocationProperties gpuProperties{0u,
                                       mock->minimalChunkingSize * 2,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest,
       whenCreateUnifiedMemoryAllocationWithChunkingAndNoEnableBOChunkingPreferredLocationHintSetThenGemCreateExtIsUsedWithoutPreferredLocation) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    mock->chunkingAvailable = true;
    mock->callBaseIsChunkingAvailable = true;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::chunkThreshold,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, whenCreateUnifiedMemoryAllocationWithMultiMemoryRegionsThenGemCreateExtIsUsedWithSingleLmemRegions) {
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 3;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;
    DeviceBitfield devices = 0b11;
    AllocationProperties gpuProperties{
        0u,
        true,
        MemoryConstants::megaByte,
        AllocationType::unifiedSharedMemory,
        false,
        devices};
    gpuProperties.alignment = MemoryConstants::pageSize64k;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[2].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenKMDSupportForCrossTileMigrationPolicyWhenCreateUnifiedMemoryAllocationWithMultiMemoryRegionsThenGemCreateExtIsUsedWithAllRegions) {
    DebugManagerStateRestore restorer;
    debugManager.flags.KMDSupportForCrossTileMigrationPolicy.set(1);

    std::vector<MemoryRegion> regionInfo(4);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(2, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 3;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;
    DeviceBitfield devices = 0b101;
    AllocationProperties gpuProperties{
        0u,
        true,
        MemoryConstants::pageSize64k,
        AllocationType::unifiedSharedMemory,
        false,
        devices};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 3u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);
    EXPECT_EQ(memRegions[2].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[2].memoryInstance, regionInfo[3].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationThenKmdMigratedAllocationIsCreated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;
    mock->setBindAvailable();

    executionEnvironment->calculateMaxOsContextCount();
    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_NE(ptr, nullptr);
    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    EXPECT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, MmapFailWhenCreateSharedUnifiedMemoryAllocationThenNullPtrReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;
    mock->setBindAvailable();

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationWithDeviceThenKmdMigratedAllocationIsCreated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;
    mock->setBindAvailable();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_NE(ptr, nullptr);
    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();

    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &vmAdvise = mock->context.receivedVmAdvise[0].value();
    EXPECT_EQ(static_cast<DrmAllocation *>(allocation)->getBO()->peekHandle(), static_cast<int>(vmAdvise.handle));
    EXPECT_EQ(DrmPrelimHelper::getVmAdviseSystemFlag(), vmAdvise.flags);

    const auto &memRegions = createExt.memoryRegions;
    EXPECT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenSetVmAdviseAtomicAttributeWhenCreatingKmdMigratedAllocationThenApplyVmAdviseAtomicCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    for (auto atomicAdvise : {-1, 0, 1, 2}) {
        debugManager.flags.SetVmAdviseAtomicAttribute.set(atomicAdvise);

        auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
        ASSERT_NE(ptr, nullptr);

        auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
        ASSERT_NE(allocation, nullptr);

        const auto &vmAdvise = mock->context.receivedVmAdvise[0].value();
        EXPECT_EQ(static_cast<DrmAllocation *>(allocation)->getBO()->peekHandle(), static_cast<int>(vmAdvise.handle));

        switch (atomicAdvise) {
        case 0:
            EXPECT_EQ(DrmPrelimHelper::getVmAdviseNoneFlag(), vmAdvise.flags);
            break;
        case 1:
            EXPECT_EQ(DrmPrelimHelper::getVmAdviseDeviceFlag(), vmAdvise.flags);
            break;
        default:
            EXPECT_EQ(DrmPrelimHelper::getVmAdviseSystemFlag(), vmAdvise.flags);
            break;
        }

        unifiedMemoryManager.freeSVMAlloc(ptr);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenSetVmAdviseDevicePreferredLocationWhenCreatingKmdMigratedAllocationThenApplyVmAdvisePreferredLocationCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    for (auto preferredLocation : {-1, 0, 1, 2}) {
        debugManager.flags.SetVmAdvisePreferredLocation.set(preferredLocation);

        auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
        ASSERT_NE(ptr, nullptr);

        auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
        ASSERT_NE(allocation, nullptr);

        if (mock->context.receivedVmAdvise[1] != std::nullopt) {

            const auto &vmAdvise = mock->context.receivedVmAdvise[1].value();
            EXPECT_EQ(static_cast<DrmAllocation *>(allocation)->getBO()->peekHandle(), static_cast<int>(vmAdvise.handle));

            EXPECT_EQ(DrmPrelimHelper::getPreferredLocationAdvise(), vmAdvise.flags);

            switch (preferredLocation) {
            case 0:
                EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, vmAdvise.memoryRegions.memoryClass);
                EXPECT_EQ(0u, vmAdvise.memoryRegions.memoryInstance);
                break;
            case 1:
            default:
                EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, vmAdvise.memoryRegions.memoryClass);
                EXPECT_EQ(0u, vmAdvise.memoryRegions.memoryInstance);
                break;
            }
        }

        unifiedMemoryManager.freeSVMAlloc(ptr);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenKmdMigratedSharedAllocationWhenCreatedInLocalMemory1OnlyThenApplyMemoryInstanceAndVmAdvisePreferredLocationCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideMultiStoragePlacement.set(0b10);
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 2;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    ASSERT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    ASSERT_NE(allocation, nullptr);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 0u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[2].region.memoryInstance);

    ASSERT_NE(mock->context.receivedVmAdvise[1], std::nullopt);

    const auto &vmAdvise = mock->context.receivedVmAdvise[1].value();
    EXPECT_EQ(static_cast<DrmAllocation *>(allocation)->getBO()->peekHandle(), static_cast<int>(vmAdvise.handle));

    EXPECT_EQ(DrmPrelimHelper::getPreferredLocationAdvise(), vmAdvise.flags);

    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, vmAdvise.memoryRegions.memoryClass);
    EXPECT_EQ(1u, vmAdvise.memoryRegions.memoryInstance);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateContextWithAccessCountersWhenCreatingKmdMigratedSharedAllocationThenDontSetPreferredLocation) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.CreateContextWithAccessCounters.set(1);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    ASSERT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    ASSERT_NE(allocation, nullptr);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 0u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    ASSERT_EQ(mock->context.receivedVmAdvise[1], std::nullopt);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateContextWithAccessCountersButOverridenWithSetVmAdvisePreferredLocationWhenCreatingKmdMigratedSharedAllocationThenSetPreferredLocation) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.CreateContextWithAccessCounters.set(1);
    debugManager.flags.SetVmAdvisePreferredLocation.set(1);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    ASSERT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    ASSERT_NE(allocation, nullptr);

    const auto &createExt = mock->context.receivedCreateGemExt.value();
    EXPECT_EQ(1u, createExt.handle);

    const auto &memRegions = createExt.memoryRegions;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 0u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    ASSERT_NE(mock->context.receivedVmAdvise[1], std::nullopt);

    const auto &vmAdvise = mock->context.receivedVmAdvise[1].value();
    EXPECT_EQ(static_cast<DrmAllocation *>(allocation)->getBO()->peekHandle(), static_cast<int>(vmAdvise.handle));

    EXPECT_EQ(DrmPrelimHelper::getPreferredLocationAdvise(), vmAdvise.flags);

    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, vmAdvise.memoryRegions.memoryClass);
    EXPECT_EQ(0u, vmAdvise.memoryRegions.memoryInstance);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenUseKmdMigrationAndUsmInitialPlacementSetToGpuWhenCreateUnifiedSharedMemoryWithOverridenMultiStoragePlacementThenKmdMigratedAllocationIsCreatedWithCorrectRegionsOrder) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.UsmInitialPlacement.set(1);
    debugManager.flags.OverrideMultiStoragePlacement.set(0x1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(5);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(2, 0)};
    regionInfo[4].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(3, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 4;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    const auto memRegions = mock->context.receivedCreateGemExt.value().memoryRegions;
    EXPECT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[0].memoryInstance, regionInfo[1].region.memoryInstance);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[1].memoryInstance, 1u);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenKMDSupportForCrossTileMigrationPolicyAndUsmInitialPlacementSetToGpuWhenCreateSharedUnifiedMemoryAllocationOnMultiTileArchitectureThenKmdMigratedAllocationIsCreatedWithCorrectRegionsOrder) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.KMDSupportForCrossTileMigrationPolicy.set(1);
    debugManager.flags.UsmInitialPlacement.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(5);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(2, 0)};
    regionInfo[4].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(3, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 4;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    const auto memRegions = mock->context.receivedCreateGemExt.value().memoryRegions;
    EXPECT_EQ(memRegions.size(), 5u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[0].memoryInstance, regionInfo[1].region.memoryInstance);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[2].region.memoryInstance);
    EXPECT_EQ(memRegions[2].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[2].memoryInstance, regionInfo[3].region.memoryInstance);
    EXPECT_EQ(memRegions[3].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[3].memoryInstance, regionInfo[4].region.memoryInstance);
    EXPECT_EQ(memRegions[4].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[4].memoryInstance, 1u);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

using DrmMemoryManagerWithSingleSubDevicePrelimTest = DrmMemoryManagerWithSubDevicesPrelimTest<false>;
TEST_F(DrmMemoryManagerWithSingleSubDevicePrelimTest, givenUnifiedMemoryAllocationOnSubDevice0WhenCreatedWithInitialPlacementOnGpuThenCallMemoryPrefetch) {
    DeviceBitfield subDevices = 0b01;
    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       subDevices};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::GPU;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);
    ASSERT_NE(allocation, nullptr);

    EXPECT_EQ(mock->context.vmBindCalled, 1u);
    EXPECT_EQ(mock->context.vmPrefetchCalled, 1u);

    ASSERT_EQ(mock->context.receivedVmPrefetch.size(), 1u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[0].vmId, 1u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[0].region, static_cast<uint32_t>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE << 16 | 0u));

    memoryManager->freeGraphicsMemory(allocation);
}

using DrmMemoryManagerWithMultipleSubDevicesPrelimTest = DrmMemoryManagerWithSubDevicesPrelimTest<true>;
TEST_F(DrmMemoryManagerWithMultipleSubDevicesPrelimTest, givenUnifiedMemoryAllocationOnSubDevice1WhenCreatedWithInitialPlacementOnGpuThenCallMemoryPrefetch) {
    DeviceBitfield subDevices = 0b10;
    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       subDevices};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::GPU;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);
    ASSERT_NE(allocation, nullptr);

    EXPECT_EQ(mock->context.vmBindCalled, 1u);
    EXPECT_EQ(mock->context.vmPrefetchCalled, 1u);

    ASSERT_EQ(mock->context.receivedVmPrefetch.size(), 1u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[0].vmId, 2u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[0].region, static_cast<uint32_t>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE << 16 | 1u));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithMultipleSubDevicesPrelimTest, givenUnifiedMemoryAllocationOnMultipleSubDevicesWhenCreatedWithInitialPlacementOnGpuThenCallVmPrefetchCorrectly) {
    DeviceBitfield subDevices = 0b11;
    AllocationProperties gpuProperties{0u,
                                       2 * MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       subDevices};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::GPU;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);
    ASSERT_NE(allocation, nullptr);

    EXPECT_EQ(mock->context.vmBindCalled, 4u);
    EXPECT_EQ(mock->context.vmPrefetchCalled, 4u);

    ASSERT_EQ(mock->context.receivedVmPrefetch.size(), 4u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[0].vmId, 1u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[0].region, static_cast<uint32_t>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE << 16 | 0u));
    EXPECT_EQ(mock->context.receivedVmPrefetch[1].vmId, 2u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[1].region, static_cast<uint32_t>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE << 16 | 0u));
    EXPECT_EQ(mock->context.receivedVmPrefetch[2].vmId, 1u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[2].region, static_cast<uint32_t>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE << 16 | 1u));
    EXPECT_EQ(mock->context.receivedVmPrefetch[3].vmId, 2u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[3].region, static_cast<uint32_t>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE << 16 | 1u));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithMultipleSubDevicesPrelimTest, givenCreateKmdMigratedSharedAllocationWithMultipleBOsUnsetWhenCreatedWithInitialPlacementOnGpuThenCallVmPrefetchCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateKmdMigratedSharedAllocationWithMultipleBOs.set(0);

    DeviceBitfield subDevices = 0b11;
    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       subDevices};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::GPU;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);
    ASSERT_NE(allocation, nullptr);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    EXPECT_EQ(mock->context.vmPrefetchCalled, 1u);

    ASSERT_EQ(mock->context.receivedVmPrefetch.size(), 1u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[0].vmId, 1u);
    EXPECT_EQ(mock->context.receivedVmPrefetch[0].region, static_cast<uint32_t>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE << 16 | 0u));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, whenVmAdviseIoctlFailsThenCreateSharedUnifiedMemoryAllocationReturnsNullptr) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->context.vmAdviseReturn = -1;

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_EQ(ptr, nullptr);
    EXPECT_TRUE(mock->context.receivedVmAdvise[0]);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationFailsThenNullptrReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->context.mmapOffsetReturn = -1;

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationIsCalledThenKmdMigratedAllocationWithMultipleBOsIsCreated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 2;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    auto size = 2 * MemoryConstants::megaByte;
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(size, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_EQ(ptr, alignUp(drmAllocation->getMmapPtr(), MemoryConstants::pageSize2M));
    EXPECT_EQ(size, drmAllocation->getMmapSize() - MemoryConstants::pageSize2M);

    auto &bos = drmAllocation->getBOs();
    EXPECT_EQ(2u, bos.size());

    auto gpuAddress = castToUint64(ptr);
    for (auto bo : bos) {
        EXPECT_NE(0, bo->getHandle());
        EXPECT_EQ(gpuAddress, bo->peekAddress());
        EXPECT_EQ(size / 2, bo->peekSize());
        gpuAddress += size / 2;
    }

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateKmdMigratedSharedAllocationWithMultipleBOsClearedWhenCreateSharedUnifiedMemoryAllocationIsCalledThenKmdMigratedAllocationWithSingleBOsIsCreated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.CreateKmdMigratedSharedAllocationWithMultipleBOs.set(0);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 2;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    auto size = 2 * MemoryConstants::megaByte;
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(size, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_EQ(ptr, alignUp(drmAllocation->getMmapPtr(), MemoryConstants::pageSize2M));
    EXPECT_EQ(size, drmAllocation->getMmapSize() - MemoryConstants::pageSize2M);

    auto &bos = drmAllocation->getBOs();
    EXPECT_EQ(1u, bos.size());

    EXPECT_NE(0, bos[0]->getHandle());
    EXPECT_EQ(castToUint64(ptr), bos[0]->peekAddress());
    EXPECT_EQ(size, bos[0]->peekSize());

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenNoMemoryInfoWhenCreateUnifiedMemoryAllocationThenNullptr) {
    mock->memoryInfo.reset(nullptr);
    mock->ioctlCallsCount = 0;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMmapFailWhenCreateUnifiedMemoryAllocationThenNullptr) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->context.mmapOffsetReturn = -1;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenGemCreateExtFailWhenCreateUnifiedMemoryAllocationThenNullptr) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->context.gemCreateExtReturn = -1;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMemoryInfoWhenAllocateWithAlignmentThenGemCreateExtIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(allocation->getMmapPtr(), nullptr);
    EXPECT_NE(allocation->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);
    EXPECT_EQ(1u, mock->context.receivedCreateGemExt.value().handle);

    memoryManager->freeGraphicsMemory(allocation);
}

static uint32_t munmapCalledCount = 0u;

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenAlignmentAndSizeWhenMmapReturnsUnalignedPointerThenCreateAllocWithAlignmentUnmapTwoUnalignedPart) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        if (addr == 0) {
            return reinterpret_cast<void *>(0x12345678);
        } else {
            return addr;
        }
    };

    memoryManager->munmapFunction = [](void *addr, size_t len) throw() {
        munmapCalledCount++;
        return 0;
    };

    munmapCalledCount = 0;
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, MemoryConstants::pageSize64k, MemoryConstants::pageSize64k, 0u);
    EXPECT_NE(allocation->getDefaultGmm(), nullptr);

    EXPECT_EQ(alignUp(reinterpret_cast<void *>(0x12345678), MemoryConstants::pageSize64k), allocation->getMmapPtr());
    EXPECT_EQ(1u, munmapCalledCount);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(3u, munmapCalledCount);
    munmapCalledCount = 0u;

    memoryManager->mmapFunction = SysCalls::mmap;
    memoryManager->munmapFunction = SysCalls::munmap;
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenAlignmentAndSizeWhenMmapReturnsAlignedThenCreateAllocWithAlignmentUnmapOneUnalignedPart) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        if (addr == 0) {
            return reinterpret_cast<void *>(0x10000000);
        } else {
            return addr;
        }
    };

    memoryManager->munmapFunction = [](void *addr, size_t len) throw() {
        munmapCalledCount++;
        return 0;
    };

    munmapCalledCount = 0u;
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, MemoryConstants::pageSize64k, MemoryConstants::pageSize64k, 0u);

    EXPECT_EQ(reinterpret_cast<void *>(0x10000000), allocation->getMmapPtr());
    EXPECT_EQ(1u, munmapCalledCount);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(2u, munmapCalledCount);
    munmapCalledCount = 0u;

    memoryManager->mmapFunction = SysCalls::mmap;
    memoryManager->munmapFunction = SysCalls::munmap;
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenInvalidCacheRegionWhenMmapReturnsUnalignedPointerThenReleaseUnalignedPartsEarly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        if (addr == 0) {
            return reinterpret_cast<void *>(0x12345678);
        } else {
            return addr;
        }
    };

    memoryManager->munmapFunction = [](void *addr, size_t len) throw() {
        munmapCalledCount++;
        return 0;
    };
    munmapCalledCount = 0;
    allocationData.cacheRegion = static_cast<uint32_t>(CacheRegion::none);
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, MemoryConstants::pageSize64k, MemoryConstants::pageSize64k, 0u);

    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(2u, munmapCalledCount);
    munmapCalledCount = 0u;

    memoryManager->mmapFunction = SysCalls::mmap;
    memoryManager->munmapFunction = SysCalls::munmap;
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMemoryInfoAndFailedMmapOffsetWhenAllocateWithAlignmentThenNullptr) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->context.mmapOffsetReturn = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(allocation, nullptr);
    mock->context.mmapOffsetReturn = 0;
}
TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMemoryInfoAndDisabledMmapBOCreationtWhenAllocateWithAlignmentThenUserptrIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(0);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->context.mmapOffsetReturn = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    EXPECT_NE(allocation, nullptr);
    EXPECT_EQ(static_cast<int>(mock->returnHandle), allocation->getBO()->peekHandle() + 1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMemoryInfoAndNotUseObjectMmapPropertyWhenAllocateWithAlignmentThenUserptrIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(0);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->context.mmapOffsetReturn = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;
    allocationData.useMmapObject = false;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    EXPECT_NE(allocation, nullptr);
    EXPECT_EQ(static_cast<int>(mock->returnHandle), allocation->getBO()->peekHandle() + 1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMemoryInfoAndFailedGemCreateExtWhenAllocateWithAlignmentThenNullptr) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->context.gemCreateExtReturn = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(allocation, nullptr);
}

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferAndBufferCompressedThenCreateGmmWithCorrectAuxFlags, IsAtMostXeHpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::buffer,
                                    deviceBitfield);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;

    allocData.flags.preferCompressed = CompressionSelector::preferCompressedAllocation(properties);
    auto buffer = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(0u, buffer->getDefaultGmm()->resourceParams.Flags.Info.RenderCompressed);

    allocData.flags.preferCompressed = true;
    auto bufferCompressed = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, bufferCompressed);
    EXPECT_EQ(1u, bufferCompressed->getDefaultGmm()->resourceParams.Flags.Info.RenderCompressed);

    memoryManager->freeGraphicsMemory(buffer);
    memoryManager->freeGraphicsMemory(bufferCompressed);
}

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenEnableStatelessCompressionWhenGraphicsAllocationCanBeAccessedStatelesslyThenPreferCompressed, IsAtMostXeHpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    debugManager.flags.EnableStatelessCompression.set(1);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.rootDeviceIndex = rootDeviceIndex;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;

    for (auto allocationType : {AllocationType::globalSurface, AllocationType::constantSurface}) {
        DeviceBitfield deviceBitfield{0x0};
        AllocationProperties properties(0, MemoryConstants::pageSize, allocationType, deviceBitfield);

        allocData.flags.preferCompressed = true;
        auto buffer = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        ASSERT_NE(nullptr, buffer);
        EXPECT_EQ(1u, buffer->getDefaultGmm()->resourceParams.Flags.Info.RenderCompressed);

        memoryManager->freeGraphicsMemory(buffer);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenChunkSizeBasedColouringPolicyWhenAllocatingInDevicePoolOnAllMemoryBanksThenDivideAllocationIntoEqualBufferObjectsWithGivenChunkSize) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.colouringPolicy = ColouringPolicy::chunkSizeBased;
    allocData.storageInfo.colouringGranularity = 256 * MemoryConstants::kiloByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(allocation->storageInfo.colouringPolicy, ColouringPolicy::chunkSizeBased);
    EXPECT_EQ(allocation->storageInfo.colouringGranularity, 256 * MemoryConstants::kiloByte);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto numHandles = static_cast<uint32_t>(alignUp(allocData.size, allocation->storageInfo.colouringGranularity) / allocation->storageInfo.colouringGranularity);
    EXPECT_EQ(numHandles, allocation->getNumGmms());
    EXPECT_EQ(numHandles, drmAllocation->getBOs().size());

    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < numHandles; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boAddress, bo->peekAddress());
        EXPECT_EQ(boSize, bo->peekSize());
        EXPECT_EQ(boSize, 256 * MemoryConstants::kiloByte);
        boAddress += boSize;
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenMappingBasedColouringPolicyWhenAllocatingInDevicePoolOnAllMemoryBanksThenSetBindAddressesToBufferObjects) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.colouringPolicy = ColouringPolicy::mappingBased;
    allocData.storageInfo.colouringGranularity = 64 * MemoryConstants::kiloByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(allocation->storageInfo.colouringPolicy, ColouringPolicy::mappingBased);
    EXPECT_EQ(allocation->storageInfo.colouringGranularity, 64 * MemoryConstants::kiloByte);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto numHandles = 4u;
    EXPECT_EQ(numHandles, allocation->getNumGmms());
    EXPECT_EQ(numHandles, drmAllocation->getBOs().size());

    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < numHandles; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boSize, bo->peekSize());

        auto addresses = bo->getColourAddresses();
        if (handleId < 2) {
            EXPECT_EQ(addresses.size(), 5u);
            EXPECT_EQ(boSize, 5 * MemoryConstants::pageSize64k);
        } else {
            EXPECT_EQ(addresses.size(), 4u);
            EXPECT_EQ(boSize, 4 * MemoryConstants::pageSize64k);
        }
        for (auto i = 0u; i < addresses.size(); i++) {
            EXPECT_EQ(addresses[i], boAddress + 4 * i * allocation->storageInfo.colouringGranularity + handleId * allocation->storageInfo.colouringGranularity);
        }
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCpuAccessRequiredWhenAllocatingInDevicePoolThenAllocationIsLocked) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.requiresCpuAccess = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_NE(nullptr, allocation->getLockedPtr());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_NE(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCpuAccessRequiredWhenAllocatingInDevicePoolAndMMAPFailThenAllocationIsNullptr) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.requiresCpuAccess = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    auto allocation1 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation1);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);

    status = MemoryManager::AllocationStatus::Success;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.requiresCpuAccess = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::writeCombined;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation2 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation2);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenWriteCombinedAllocationWhenAllocatingInDevicePoolThenAllocationIsLockedAndLockedPtrIsUsedAsGpuAddress) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData{};
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::writeCombined;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size + MemoryConstants::pageSize64k, 2 * MemoryConstants::megaByte) + 2 * MemoryConstants::megaByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_NE(nullptr, allocation->getLockedPtr());

    EXPECT_EQ(allocation->getLockedPtr(), allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getLockedPtr(), reinterpret_cast<void *>(allocation->getGpuAddress()));
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(0u, allocation->getReservedAddressSize());

    auto cpuAddress = allocation->getLockedPtr();
    auto alignedCpuAddress = alignDown(cpuAddress, 2 * MemoryConstants::megaByte);
    auto offset = ptrDiff(cpuAddress, alignedCpuAddress);
    EXPECT_EQ(offset, allocation->getAllocationOffset());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(reinterpret_cast<uint64_t>(cpuAddress), bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenSupportedTypeWhenAllocatingInDevicePoolThenSuccessStatusAndNonNullPtrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = MemoryConstants::pageSize;
    imgDesc.imageHeight = MemoryConstants::pageSize;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    bool resource48Bit[] = {true, false};
    AllocationType supportedTypes[] = {AllocationType::buffer,
                                       AllocationType::image,
                                       AllocationType::commandBuffer,
                                       AllocationType::linearStream,
                                       AllocationType::indirectObjectHeap,
                                       AllocationType::timestampPacketTagBuffer,
                                       AllocationType::internalHeap,
                                       AllocationType::kernelIsa,
                                       AllocationType::kernelIsaInternal,
                                       AllocationType::debugModuleArea,
                                       AllocationType::svmGpu};
    for (auto res48bit : resource48Bit) {
        for (auto supportedType : supportedTypes) {
            allocData.type = supportedType;
            allocData.imgInfo = (AllocationType::image == supportedType) ? &imgInfo : nullptr;
            allocData.hostPtr = (AllocationType::svmGpu == supportedType) ? ::alignedMalloc(allocData.size, 4096) : nullptr;

            switch (supportedType) {
            case AllocationType::image:
            case AllocationType::indirectObjectHeap:
            case AllocationType::internalHeap:
            case AllocationType::kernelIsa:
            case AllocationType::kernelIsaInternal:
                allocData.flags.resource48Bit = true;
                break;
            default:
                allocData.flags.resource48Bit = res48bit;
            }

            auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
            ASSERT_NE(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

            auto gpuAddress = allocation->getGpuAddress();
            auto gmmHelper = device->getGmmHelper();
            if (allocation->getAllocationType() == AllocationType::svmGpu) {
                if (!memoryManager->isLimitedRange(0)) {
                    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapSvm)), gpuAddress);
                    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapSvm)), gpuAddress);
                }
            } else if (memoryManager->heapAssigners[rootDeviceIndex]->useInternal32BitHeap(allocation->getAllocationType())) {
                EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
                EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
            } else {
                const bool prefer2MBAlignment = allocation->getUnderlyingBufferSize() >= 2 * MemoryConstants::megaByte;
                const bool prefer57bitAddressing = memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended) > 0 && !allocData.flags.resource48Bit;

                auto heap = HeapIndex::heapStandard64KB;
                if (prefer57bitAddressing) {
                    heap = HeapIndex::heapExtended;
                } else if (prefer2MBAlignment) {
                    heap = HeapIndex::heapStandard2MB;
                }

                EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(heap)), gpuAddress);
                EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(heap)), gpuAddress);
            }

            memoryManager->freeGraphicsMemory(allocation);
            if (AllocationType::svmGpu == supportedType) {
                ::alignedFree(const_cast<void *>(allocData.hostPtr));
            }
        }
    }
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    auto ptr = memoryManager->lockBufferObject(nullptr);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockBufferObject(nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenPrintBOCreateDestroyResultFlagSetWhileCreatingBufferObjectInMemoryRegionThenDebugInformationIsPrinted) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PrintBOCreateDestroyResult.set(true);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize64k;
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();

    StreamCapture capture;
    capture.captureStdout();
    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(0u,
                                                                                            nullptr,
                                                                                            AllocationType::buffer,
                                                                                            gpuAddress,
                                                                                            size,
                                                                                            (1 << (MemoryBanks::getBankForLocalMemory(0) - 1)),
                                                                                            1,
                                                                                            -1,
                                                                                            false,
                                                                                            false));
    EXPECT_NE(nullptr, bo);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 65536, param: 0x1000000010001, memory class: 1, memory instance: 256 }\nGEM_CREATE_EXT has returned: 0 BO-1 with size: 65536\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenPrintBOCreateDestroyResultFlagWhenCreatingSharedUnifiedAllocationThenPrintIoctlResult) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.PrintBOCreateDestroyResult.set(true);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->engineInfoQueried = false;
    mock->queryEngineInfo();
    mock->ioctlCallsCount = 0;
    mock->setBindAvailable();

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    StreamCapture capture;
    capture.captureStdout();
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_NE(nullptr, ptr);
    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(nullptr, allocation);

    unifiedMemoryManager.freeSVMAlloc(ptr);

    std::string output = capture.getCapturedStdout();
    auto idx = output.find("Performing GEM_CREATE_EXT with { size: 2097152, param: 0x1000000010001, memory class: 0, memory instance: 1, memory class: 1, memory instance: 256 }\n\
GEM_CREATE_EXT has returned: 0 BO-1 with size: 2097152\n");

    EXPECT_EQ(0u, idx);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenDebugModuleAreaTypeWhenAllocatingByDrmAndOsAgnosticMemoryManagersThenBothAllocateFromTheSameHeap) {
    auto gfxPartitionDrm = new MockGfxPartition();
    gfxPartitionDrm->callHeapAllocate = false;
    auto gfxPartitionRestore = memoryManager->gfxPartitions[rootDeviceIndex].release();
    memoryManager->gfxPartitions[rootDeviceIndex].reset(gfxPartitionDrm);

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize64k,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         device->getDeviceBitfield()};

    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    auto drmHeapIndex = gfxPartitionDrm->heapAllocateIndex;

    MockMemoryManager osAgnosticMemoryManager(false, localMemoryEnabled, *executionEnvironment);
    auto gfxPartitionOsAgnostic = new MockGfxPartition();
    gfxPartitionOsAgnostic->callHeapAllocate = false;
    auto gfxPartitionRestore2 = osAgnosticMemoryManager.gfxPartitions[rootDeviceIndex].release();
    osAgnosticMemoryManager.gfxPartitions[rootDeviceIndex].reset(gfxPartitionOsAgnostic);

    auto moduleDebugArea2 = osAgnosticMemoryManager.allocateGraphicsMemoryWithProperties(properties);
    auto osAgnosticHeapIndex = gfxPartitionOsAgnostic->heapAllocateIndex;

    EXPECT_EQ(osAgnosticHeapIndex, drmHeapIndex);

    memoryManager->freeGraphicsMemory(moduleDebugArea);
    osAgnosticMemoryManager.freeGraphicsMemory(moduleDebugArea2);

    osAgnosticMemoryManager.gfxPartitions[rootDeviceIndex].reset(gfxPartitionRestore2);
    memoryManager->gfxPartitions[rootDeviceIndex].reset(gfxPartitionRestore);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenSipKernelTypeWhenAllocatingByDrmAndOsAgnosticMemoryManagersThenBothAllocateFromTheSameHeap) {
    auto gfxPartitionDrm = new MockGfxPartition();
    gfxPartitionDrm->callHeapAllocate = false;

    memoryManager->gfxPartitions[rootDeviceIndex].reset(gfxPartitionDrm);

    AllocationProperties properties = {rootDeviceIndex, 0x1000, NEO::AllocationType::kernelIsaInternal, device->getDeviceBitfield()};
    properties.flags.use32BitFrontWindow = true;

    auto sipKernel = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    auto drmHeapIndex = gfxPartitionDrm->heapAllocateIndex;

    MockMemoryManager osAgnosticMemoryManager(false, localMemoryEnabled, *executionEnvironment);

    auto gfxPartitionOsAgnostic = new MockGfxPartition();
    gfxPartitionOsAgnostic->callHeapAllocate = false;
    osAgnosticMemoryManager.gfxPartitions[rootDeviceIndex].reset(gfxPartitionOsAgnostic);

    auto sipKernel2 = osAgnosticMemoryManager.allocateGraphicsMemoryWithProperties(properties);
    auto osAgnosticHeapIndex = gfxPartitionOsAgnostic->heapAllocateIndex;

    EXPECT_EQ(osAgnosticHeapIndex, drmHeapIndex);

    memoryManager->freeGraphicsMemory(sipKernel);
    osAgnosticMemoryManager.freeGraphicsMemory(sipKernel2);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenDebugModuleAreaTypeWhenAllocatingThenBothUseFrontWindowInternalDeviceHeaps) {
    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize64k,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         device->getDeviceBitfield()};
    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto gpuAddress = moduleDebugArea->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_LE(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapInternalDeviceFrontWindow)), gpuAddress);
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow)), moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(MemoryPool::localMemory, moduleDebugArea->getMemoryPool());

    memoryManager->freeGraphicsMemory(moduleDebugArea);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenSipKernelTypeWhenAllocatingThenFrontWindowInternalDeviceHeapIsUsed) {
    const auto allocType = AllocationType::kernelIsaInternal;
    AllocationProperties properties = {rootDeviceIndex, 0x1000, allocType, device->getDeviceBitfield()};
    properties.flags.use32BitFrontWindow = true;

    auto sipAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto gpuAddress = sipAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_LE(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapInternalDeviceFrontWindow)), gpuAddress);
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow)), sipAllocation->getGpuBaseAddress());
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), sipAllocation->getGpuBaseAddress());
    EXPECT_EQ(MemoryPool::localMemory, sipAllocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(sipAllocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenDebugVariableSetWhenAllocatingThenForceHeapAlignment) {
    debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(static_cast<int32_t>(MemoryConstants::megaByte * 2));

    const auto allocType = AllocationType::kernelIsaInternal;
    AllocationProperties properties = {rootDeviceIndex, 0x1000, allocType, device->getDeviceBitfield()};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::megaByte * 2));
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

using DrmMemoryManagerFailInjectionPrelimTest = Test<DrmMemoryManagerFixturePrelim>;

HWTEST_TEMPLATED_F(DrmMemoryManagerFailInjectionPrelimTest, givenEnabledLocalMemoryWhenNewFailsThenAllocateInDevicePoolReturnsStatusErrorAndNullallocation) {
    mock->ioctlExpected.total = -1; // don't care
    class MockGfxPartition : public GfxPartition {
      public:
        MockGfxPartition() : GfxPartition(reservedCpuAddressRange) {
            init(defaultHwInfo->capabilityTable.gpuAddressSpace, getSizeToReserve(), 0, 1, false, 0u, defaultHwInfo->capabilityTable.gpuAddressSpace + 1);
        }
        ~MockGfxPartition() override {
            for (const auto &heap : heaps) {
                auto mockHeap = static_cast<const MockHeap *>(&heap);
                if (defaultHwInfo->capabilityTable.gpuAddressSpace != MemoryConstants::max36BitAddress && mockHeap->getSize() > 0) {
                    EXPECT_EQ(0u, mockHeap->alloc->getUsedSize());
                }
            }
        }
        struct MockHeap : Heap {
            using Heap::alloc;
        };
        OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    };
    TestedDrmMemoryManager testedMemoryManager(true, false, true, *executionEnvironment);
    testedMemoryManager.overrideGfxPartition(new MockGfxPartition);

    InjectedFunction method = [&](size_t failureIndex) {
        MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
        AllocationData allocData;
        allocData.allFlags = 0;
        allocData.size = MemoryConstants::pageSize;
        allocData.flags.allocateMemory = true;
        allocData.type = AllocationType::buffer;
        allocData.rootDeviceIndex = rootDeviceIndex;

        auto allocation = testedMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);

        if (MemoryManagement::nonfailingAllocation != failureIndex) {
            EXPECT_EQ(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
        } else {
            EXPECT_NE(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
            testedMemoryManager.freeGraphicsMemory(allocation);
        }
    };

    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    injectFailures(method);
}

using DrmMemoryManagerCopyMemoryToAllocationPrelimTest = DrmMemoryManagerLocalMemoryPrelimTest;

struct DrmMemoryManagerToTestCopyMemoryToAllocation : public DrmMemoryManager {
    using DrmMemoryManager::allocateGraphicsMemoryInDevicePool;
    DrmMemoryManagerToTestCopyMemoryToAllocation(ExecutionEnvironment &executionEnvironment, bool localMemoryEnabled, size_t lockableLocalMemorySize)
        : DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        std::fill(this->localMemorySupported.begin(), this->localMemorySupported.end(), localMemoryEnabled);
        lockedLocalMemorySize = lockableLocalMemorySize;
    }
    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
        if (lockedLocalMemorySize > 0) {
            lockedLocalMemory[0].reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory[0].get();
        }
        return nullptr;
    }
    void *lockBufferObject(BufferObject *bo) override {
        if (lockedLocalMemorySize > 0) {
            deviceIndex = (deviceIndex < 3) ? deviceIndex + 1u : 0u;
            lockedLocalMemory[deviceIndex].reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory[deviceIndex].get();
        }
        return nullptr;
    }
    void unlockBufferObject(BufferObject *bo) override {
    }
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
    }
    bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) override {
        copyMemoryToAllocationBanksCalled++;
        return DrmMemoryManager::copyMemoryToAllocationBanks(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, handleMask);
    }
    std::array<std::unique_ptr<uint8_t[]>, 4> lockedLocalMemory;
    uint32_t deviceIndex = 3;
    size_t lockedLocalMemorySize = 0;
    uint32_t copyMemoryToAllocationBanksCalled = 0;
};

TEST_F(DrmMemoryManagerCopyMemoryToAllocationPrelimTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[0].get(), offset), dataToCopy.data(), dataToCopy.size()));

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerCopyMemoryToAllocationPrelimTest, givenDrmMemoryManagerWhenCopyMemoryToMultiTileAllocationThenCallCopyMemoryToAllocationBanks) {
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::constantSurface;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    char data = 0;
    allocation->setCpuPtrAndGpuAddress(&data, allocation->getGpuAddress());

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);
    EXPECT_EQ(1u, drmMemoryManager.copyMemoryToAllocationBanksCalled);
    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerCopyMemoryToAllocationPrelimTest, givenDrmMemoryManagerWhenCopyDebugSurfaceToMultiTileAllocationThenCallCopyMemoryToAllocation) {
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationType debugSurfaces[] = {AllocationType::debugContextSaveArea, AllocationType::debugSbaTrackingBuffer};

    for (auto type : debugSurfaces) {
        AllocationProperties debugSurfaceProperties{0, true, sourceAllocationSize, type, false, false, 0b11};
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
        ASSERT_NE(nullptr, allocation);

        auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
        EXPECT_TRUE(ret);
        EXPECT_EQ(0u, drmMemoryManager.copyMemoryToAllocationBanksCalled);
        drmMemoryManager.freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerCopyMemoryToAllocationPrelimTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationFailsToLockResourceThenItReturnsFalse) {
    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, 0);
    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_FALSE(ret);

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerCopyMemoryToAllocationPrelimTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationWithCpuPtrThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, false, 0);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    auto allocation = drmMemoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, destinationAllocationSize, AllocationType::kernelIsa, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(ptrOffset(allocation->getUnderlyingBuffer(), offset), dataToCopy.data(), dataToCopy.size()));

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerCopyMemoryToAllocationPrelimTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationOnAllMemoryBanksReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    for (auto index = 0u; index < 3; index++) {
        EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[index].get(), offset), dataToCopy.data(), dataToCopy.size()));
    }

    drmMemoryManager.freeGraphicsMemory(allocation);
}

typedef Test<DrmMemoryManagerFixturePrelim> DrmMemoryManagerTestPrelim;

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, whenSettingNumHandlesThenTheyAreRetrievedCorrectly) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, whenCreatingAllocationFromMultipleSharedHandlesWithSharingResourcesThenDifferentAllocationsAreReturned) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, whenCreatingAllocationFromMultipleSharedHandlesWithSharingResourcesWithBasePointerThenUnderlyingPointerIsTheSame) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, whenCreatingAllocationFromMultipleSharedHandlesWithNoSharingResourcesThenDifferentAllocationsAreReturned) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, whenCreatingAllocationFromMultipleSharedHandlesAndFindAndReferenceSharedBufferObjectReturnsNonNullThenAllocationSucceeds) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, whenCreatingAllocationFromMultipleSharedHandlesWithOneHandleThenAllocationSucceedsAndGpuAddressIsFromTheExpectedHeap) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, whenCreatingAllocationFromMultipleSharedHandlesAndIoctlFailsThenNullIsReturned) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, whenCreatingAllocationFromMultipleSharedHandlesThenAllocationSucceeds) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnMmapFunctionThenReturnNullPtr) {
    mock->ioctlExpected.gemMmapOffset = 2;
    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    BufferObject bo(0, mock, 3, 1, 0, 1);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mock->ioctlResExt = &mock->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnIoctlMmapOffsetThenReturnNullPtr) {
    mock->ioctlExpected.gemMmapOffset = 2;
    mock->failOnMmapOffset = true;

    BufferObject bo(0, mock, 3, 1, 0, 1);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mock->ioctlResExt = &mock->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledThenGraphicsAllocationIsReturned2) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim,
                   whenPrintBOCreateDestroyResultFlagIsSetAndCallToCreateSharedAllocationForHostAllocationThenExpectedMessageIsPrinted) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledWithBufferHostMemoryAllocationTypeThenGraphicsAllocationIsReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
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

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledForAnExistingAllocationGraphicsAllocationIsReturned) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledAndMmapOffsetIoctlFailsThenNullAllocationIsReturned) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerAndUseMmapObjectSetToFalseThenDrmAllocationWithoutMappingIsReturned) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerWithoutMemoryInfoThenDrmAllocationWithoutMappingIsReturned) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, MmapFailWhenUSMHostAllocationFromSharedHandleThenNullPtrReturned) {
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButBufferObjectIsNullThenReturnNullPtr) {
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledForMemoryInfoThenReturnMemoryRegionSize) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmQueryMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    drm->memoryInfo.reset(new MockMemoryInfo(*drm));
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    auto deviceMask = std::max(static_cast<uint32_t>(maxNBitValue(hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount)), 1u);

    EXPECT_EQ(memoryInfo->getMemoryRegionSize(deviceMask), memoryManager.getLocalMemorySize(0u, deviceMask));
    EXPECT_EQ(memoryInfo->getMemoryRegionSize(1), memoryManager.getLocalMemorySize(0u, 1));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledThenReturnMemoryRegionSizeCorrectly) {
    auto hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &sysInfo = hwInfo.gtSystemInfo;

    sysInfo.MultiTileArchInfo.IsValid = true;
    sysInfo.MultiTileArchInfo.TileCount = 4;

    MockExecutionEnvironment executionEnvironment(&hwInfo, true, 4u);
    for (auto i = 0u; i < 4u; i++) {
        auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment.rootDeviceEnvironments[i]);
        ASSERT_NE(nullptr, drm);

        executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[i]->osInterface->setDriverModel(std::move(drm));
    }
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto drm = executionEnvironment.rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmQueryMock>();
        drm->memoryInfoQueried = false;
        drm->queryMemoryInfo();

        auto memoryInfo = drm->getMemoryInfo();
        ASSERT_NE(nullptr, memoryInfo);

        uint64_t expectedSize = 0;
        for (uint32_t i = 0; i < sysInfo.MultiTileArchInfo.TileCount; i++) {
            expectedSize += memoryInfo->getMemoryRegionSize(1 << i);
        }

        auto deviceMask = static_cast<uint32_t>(maxNBitValue(sysInfo.MultiTileArchInfo.TileCount));
        EXPECT_EQ(expectedSize, memoryManager.getLocalMemorySize(i, deviceMask));
        EXPECT_EQ(memoryInfo->getMemoryRegionSize(1), memoryManager.getLocalMemorySize(i, 1));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestPrelim, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledButMemoryInfoIsNotAvailableThenSizeZeroIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmQueryMock(*executionEnvironment.rootDeviceEnvironments[0]);
    drm->memoryInfo.reset();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    EXPECT_EQ(0u, memoryManager.getLocalMemorySize(0u, 0xF));
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenGraphicsAllocationInDevicePoolIsAllocatedForImage1DWhenTheSizeReturnedFromGmmIsUnalignedThenCreateBufferObjectWithSizeAlignedTo64KB) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image1D;
    imgDesc.imageWidth = 100;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::image;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);

    auto gpuAddress = allocation->getGpuAddress();
    auto sizeAlignedTo64KB = alignUp(allocData.imgInfo->size, MemoryConstants::pageSize64k);
    EXPECT_NE(0u, gpuAddress);
    EXPECT_EQ(sizeAlignedTo64KB, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAlignedTo64KB, allocation->getReservedAddressSize());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAlignedTo64KB, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenAllocationWithUnifiedMemoryAllocationWithoutUseMmapObjectThenReturnNullptr) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.useMmapObject = false;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenAllocationWithUnifiedMemoryAllocationWithoutMemoryInfoSetThenReturnNullptr) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.useMmapObject = true;

    mock->memoryInfo.reset();

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

using AllocationInfoLogging = DrmMemoryManagerLocalMemoryPrelimTest;

TEST(AllocationInfoLogging, givenDrmGraphicsAllocationWithMultipleBOsWhenGettingImplementationSpecificAllocationInfoThenReturnInfoStringWithAllHandles) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    BufferObject bo0(0, &drm, 3, 0, 0, 1), bo1(0, &drm, 3, 1, 0, 1),
        bo2(0, &drm, 3, 2, 0, 1), bo3(0, &drm, 3, 3, 0, 1);
    BufferObjects bos{&bo0, &bo1, &bo2, &bo3};
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, bos, nullptr, 0u, 0u, MemoryPool::localMemory);

    EXPECT_STREQ(drmAllocation.getAllocationInfoString().c_str(), " Handle: 0 Handle: 1 Handle: 2 Handle: 3");
}

class DrmMemoryManagerAllocation57BitTest : public DrmMemoryManagerLocalMemoryPrelimTest,
                                            public ::testing::WithParamInterface<AllocationType> {
  public:
    void SetUp() override { DrmMemoryManagerLocalMemoryPrelimTest::SetUp(); }
    void TearDown() override { DrmMemoryManagerLocalMemoryPrelimTest::TearDown(); }
};

TEST_P(DrmMemoryManagerAllocation57BitTest, givenAllocationTypeHaveToBeAllocatedAs57BitThenHeapExtendedIsUsed) {
    if (0 == memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, GetParam(), mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);

    auto gpuAddress = allocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExtended)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended)), gpuAddress);

    memoryManager->freeGraphicsMemory(allocation);
}

static const AllocationType allocation57Bit[] = {
    AllocationType::buffer,
    AllocationType::constantSurface,
    AllocationType::globalSurface,
    AllocationType::pipe,
    AllocationType::printfSurface,
    AllocationType::privateSurface,
    AllocationType::sharedBuffer,
};

INSTANTIATE_TEST_SUITE_P(Drm57Bit,
                         DrmMemoryManagerAllocation57BitTest,
                         ::testing::ValuesIn(allocation57Bit));

struct DrmCommandStreamEnhancedPrelimTest : public DrmCommandStreamEnhancedTemplate<DrmMockCustomPrelim> {
    void SetUp() override {
        debugManager.flags.UseVmBind.set(1u);
        DrmCommandStreamEnhancedTemplate::SetUp();
    }
    void TearDown() override {
        DrmCommandStreamEnhancedTemplate::TearDown();
        dbgState.reset();
    }

    template <typename FamilyType>
    void setUpT() {
        DrmCommandStreamEnhancedTemplate<DrmMockCustomPrelim>::setUpT<FamilyType>();

        this->commandBuffer = this->mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{this->csr->getRootDeviceIndex(), MemoryConstants::pageSize});
        LinearStream cs(commandBuffer);
        CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
        EncodeNoop<FamilyType>::alignToCacheLine(cs);
        this->batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
        this->allocation = this->mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
        this->csr->makeResident(*this->allocation);
    }

    DebugManagerStateRestore restorer;
    GraphicsAllocation *commandBuffer;
    GraphicsAllocation *allocation;
    BatchBuffer batchBuffer;
};

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedPrelimTest, givenUseVmBindSetWhenFlushThenAllocIsBoundAndNotPassedToExec) {
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    const auto execObjectRequirements = [allocation = this->allocation](const auto &execObject) {
        auto mockExecObject = static_cast<const MockExecObject &>(execObject);
        return (mockExecObject.getHandle() == 0 &&
                mockExecObject.getOffset() == static_cast<DrmAllocation *>(allocation)->getBO()->peekAddress());
    };

    auto &residency = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->execObjectsStorage;
    EXPECT_TRUE(std::find_if(residency.begin(), residency.end(), execObjectRequirements) == residency.end());
    EXPECT_EQ(residency.size(), 1u);

    residency.clear();

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}
