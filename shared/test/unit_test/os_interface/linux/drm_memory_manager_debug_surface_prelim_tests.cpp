/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture_prelim.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_prelim_fixtures.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"

#include "gtest/gtest.h"

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateDebugSurfaceWithUnalignedSizeCalledThenNullptrReturned) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize + 101, NEO::AllocationType::debugContextSaveArea, false, false, 0b1011};
    auto debugSurface = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    EXPECT_EQ(nullptr, debugSurface);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateMultiHostDebugSurfaceAllocationWithUnalignedSizeThenNullptrReturned) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize + 101; // Unaligned size
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateMultiHostDebugSurfaceAllocationWithAlignedSizeThenLocalMemoryPoolSelected) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two tiles
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->isFlushL3Required());
    EXPECT_TRUE(allocation->isUncacheable());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateMultiHostDebugSurfaceAllocationWithZeroGpuAddressThenGpuRangeIsAcquired) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two tiles
    allocationData.gpuAddress = 0;                 // Zero GPU address

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_NE(nullptr, allocation->getReservedAddressPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateMultiHostDebugSurfaceAllocationWithNonZeroGpuAddressThenProvidedAddressIsUsed) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two tiles
    allocationData.gpuAddress = 0x1000;            // Non-zero GPU address

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    auto gmmHelper = memoryManager->getGmmHelper(0);
    EXPECT_EQ(gmmHelper->canonize(0x1000), allocation->getGpuAddress());
    EXPECT_EQ(nullptr, allocation->getReservedAddressPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateMultiHostDebugSurfaceAllocationWithMultipleTilesThenAllTilesAreHandled) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two tiles
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(2u, allocation->getNumGmms());

    // Check that buffer objects are created for tiles
    const auto &bufferObjects = allocation->getBOs();
    EXPECT_GE(bufferObjects.size(), 2u); // At least 2 buffer objects

    // Check that some buffer objects are valid (not all may be used)
    bool foundValidBufferObjects = false;
    for (const auto &bo : bufferObjects) {
        if (bo != nullptr) {
            foundValidBufferObjects = true;
            break;
        }
    }
    EXPECT_TRUE(foundValidBufferObjects);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateMultiHostDebugSurfaceAllocationWithOsContextThenOsContextIsSet) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two tiles
    allocationData.gpuAddress = 0x1000;

    auto osContext = static_cast<OsContextLinux *>(device->getDefaultEngine().osContext);
    allocationData.osContext = osContext;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(osContext, allocation->getOsContext());

    memoryManager->freeGraphicsMemory(allocation);
}
