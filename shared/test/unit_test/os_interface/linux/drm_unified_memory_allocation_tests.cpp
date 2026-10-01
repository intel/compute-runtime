/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_properties.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/unit_test/os_interface/linux/drm_memory_manager_local_memory_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

TEST_F(DrmMemoryManagerLocalMemoryTest, givenVmAdviseAtomicAttributeNotSupportedWhenAllocatingUnifiedMemoryInDevicePoolThenNullptrIsReturned) {
    ioctlHelper->vmAdviseAtomicAttribute = 0u;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, whenCreateUnifiedMemoryAllocationThenGemCreateExtIsUsed) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
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

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto &memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationThenKmdMigratedAllocationIsCreated) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;
    mock->setBindAvailable();

    executionEnvironment->calculateMaxOsContextCount();
    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_NE(ptr, nullptr);
    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto &memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    EXPECT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, MmapFailWhenCreateSharedUnifiedMemoryAllocationThenNullPtrReturned) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;
    mock->setBindAvailable();

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationFailsThenNullptrReturned) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    ioctlHelper->retrieveMmapOffsetResult = false;

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMmapOffsetFailureWhenCreateUnifiedMemoryAllocationThenNullptr) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    ioctlHelper->retrieveMmapOffsetResult = false;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenGemCreateExtFailWhenCreateUnifiedMemoryAllocationThenNullptr) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    ioctlHelper->createGemExtResult = -1;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllocationWithUnifiedMemoryAllocationWithoutUseMmapObjectThenReturnNullptr) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    allocData.allFlags = 0;
    allocData.size = 18 * alignmentSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.useMmapObject = false;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllocationWithUnifiedMemoryAllocationWithoutMemoryInfoSetThenReturnNullptr) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    allocData.allFlags = 0;
    allocData.size = 18 * alignmentSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.useMmapObject = true;

    mock->memoryInfo.reset();

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, whenCreateUnifiedMemoryAllocationWithMultiMemoryRegionsThenGemCreateExtIsUsedWithSingleLmemRegions) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 3;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;
    DeviceBitfield devices = 0b11;
    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    AllocationProperties gpuProperties{
        0u,
        true,
        MemoryConstants::megaByte,
        AllocationType::unifiedSharedMemory,
        false,
        devices};
    gpuProperties.alignment = alignmentSize;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto &memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[2].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseKmdMigrationAndUsmInitialPlacementSetToGpuWhenCreateUnifiedSharedMemoryWithOverriddenMultiStoragePlacementThenKmdMigratedAllocationIsCreatedWithCorrectRegionsOrder) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
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

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    EXPECT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[0].memoryInstance, regionInfo[1].region.memoryInstance);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[1].memoryInstance, 1u);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationIsCalledThenKmdMigratedAllocationWithMultipleBOsIsCreated) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
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

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    auto size = 2 * MemoryConstants::megaByte;
    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateKmdMigratedSharedAllocationWithMultipleBOsClearedWhenCreateSharedUnifiedMemoryAllocationIsCalledThenKmdMigratedAllocationWithSingleBOsIsCreated) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
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

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    auto size = 2 * MemoryConstants::megaByte;
    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenKMDSupportForCrossTileMigrationPolicyWhenCreateUnifiedMemoryAllocationWithMultiMemoryRegionsThenGemCreateExtIsUsedWithAllRegions) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
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

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto &memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    ASSERT_EQ(memRegions.size(), 3u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);
    EXPECT_EQ(memRegions[2].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[2].memoryInstance, regionInfo[3].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenKMDSupportForCrossTileMigrationPolicyAndUsmInitialPlacementSetToGpuWhenCreateSharedUnifiedMemoryAllocationOnMultiTileArchitectureThenKmdMigratedAllocationIsCreatedWithCorrectRegionsOrder) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
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

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
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

} // namespace NEO
