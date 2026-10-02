/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_properties.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_prelim_fixtures.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenDrmMemoryManagerWithPrelimSupportWhenCreateBufferObjectInMemoryRegionIsCalledThenBufferObjectWithAGivenGpuAddressAndSizeIsCreatedAndAllocatedInASpecifiedMemoryRegion, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest,
          whenCreateUnifiedMemoryAllocationWithChunkingThenGemCreateExtAndPreferredLocationAreUsed, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest,
          whenCreateUnifiedMemoryAllocationWithChunkingAndModeNotSetToSharedThenChunkingIsNotUsed, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest,
          whenCreateUnifiedMemoryAllocationWithChunkingAndSizeLessThanMinimalThenChunkingIsNotUsed, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest,
          whenCreateUnifiedMemoryAllocationWithChunkingModeSetToSharedAndSizeGreaterThanMinimalThenChunkingIsUsed, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest,
          whenCreateUnifiedMemoryAllocationWithChunkingModeSetToSharedAndSizeGreaterThanMinimalWithDebuggingEnabledThenChunkingIsNotUsed, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest,
          whenCreateUnifiedMemoryAllocationWithChunkingAndNoEnableBOChunkingPreferredLocationHintSetThenGemCreateExtIsUsedWithoutPreferredLocation, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationWithDeviceThenKmdMigratedAllocationIsCreated, IsAtMostXeCore) {
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

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenSetVmAdviseAtomicAttributeWhenCreatingKmdMigratedAllocationThenApplyVmAdviseAtomicCorrectly, IsAtMostXeCore) {
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

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenSetVmAdviseDevicePreferredLocationWhenCreatingKmdMigratedAllocationThenApplyVmAdvisePreferredLocationCorrectly, IsAtMostXeCore) {
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

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenKmdMigratedSharedAllocationWhenCreatedInLocalMemory1OnlyThenApplyMemoryInstanceAndVmAdvisePreferredLocationCorrectly, IsAtMostXeCore) {
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

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateContextWithAccessCountersWhenCreatingKmdMigratedSharedAllocationThenDontSetPreferredLocation, IsAtMostXeCore) {
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

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateContextWithAccessCountersButOverriddenWithSetVmAdvisePreferredLocationWhenCreatingKmdMigratedSharedAllocationThenSetPreferredLocation, IsAtMostXeCore) {
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

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

using DrmMemoryManagerWithSingleSubDevicePrelimTest = DrmMemoryManagerWithSubDevicesPrelimTest<false>;
HWTEST2_F(DrmMemoryManagerWithSingleSubDevicePrelimTest, givenUnifiedMemoryAllocationOnSubDevice0WhenCreatedWithInitialPlacementOnGpuThenCallMemoryPrefetch, IsAtMostXeCore) {
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
HWTEST2_F(DrmMemoryManagerWithMultipleSubDevicesPrelimTest, givenUnifiedMemoryAllocationOnSubDevice1WhenCreatedWithInitialPlacementOnGpuThenCallMemoryPrefetch, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerWithMultipleSubDevicesPrelimTest, givenUnifiedMemoryAllocationOnMultipleSubDevicesWhenCreatedWithInitialPlacementOnGpuThenCallVmPrefetchCorrectly, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerWithMultipleSubDevicesPrelimTest, givenCreateKmdMigratedSharedAllocationWithMultipleBOsUnsetWhenCreatedWithInitialPlacementOnGpuThenCallVmPrefetchCorrectly, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, whenVmAdviseIoctlFailsThenCreateSharedUnifiedMemoryAllocationReturnsNullptr, IsAtMostXeCore) {
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

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_EQ(ptr, nullptr);
    EXPECT_TRUE(mock->context.receivedVmAdvise[0]);
}

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenPrintBOCreateDestroyResultFlagSetWhileCreatingBufferObjectInMemoryRegionThenDebugInformationIsPrinted, IsAtMostXeCore) {
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

HWTEST2_F(DrmMemoryManagerLocalMemoryPrelimTest, givenPrintBOCreateDestroyResultFlagWhenCreatingSharedUnifiedAllocationThenPrintIoctlResult, IsAtMostXeCore) {
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

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

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

HWTEST2_TEMPLATED_F(DrmCommandStreamEnhancedPrelimTest, givenUseVmBindSetWhenFlushThenAllocIsBoundAndNotPassedToExec, IsAtMostXeCore) {
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
