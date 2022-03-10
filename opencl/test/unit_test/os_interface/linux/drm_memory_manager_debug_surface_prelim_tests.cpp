/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"

#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_prelim.h"
#include "opencl/test/unit_test/os_interface/linux/drm_memory_manager_prelim_fixtures.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock_memory_info.h"

#include "gtest/gtest.h"

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateDebugSurfaceWithUnalignedSizeCalledThenNullptrReturned) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize + 101, NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA, false, false, 0b1011};
    auto debugSurface = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    EXPECT_EQ(nullptr, debugSurface);
}

TEST_F(DrmMemoryManagerLocalMemoryPrelimTest, givenCreateDebugSurfaceAndAlignedMallocFailedThenNullptrReturned) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA, false, false, 0b1011};
    memoryManager->alignedMallocShouldFail = true;
    auto debugSurface = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    memoryManager->alignedMallocShouldFail = false;
    EXPECT_EQ(nullptr, debugSurface);
}

TEST_F(DrmMemoryManagerLocalMemoryWithCustomPrelimMockTest, givenCreateDebugSurfaceAndAllocUserptrFailedThenNullptrReturned) {
    mock->ioctl_res = -1;
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA, false, false, 0b1011};
    auto debugSurface = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    mock->ioctl_res = 0;
    EXPECT_EQ(1, mock->ioctl_cnt.gemUserptr);
    EXPECT_EQ(nullptr, debugSurface);
}

TEST_F(DrmMemoryManagerLocalMemoryWithCustomPrelimMockTest, givenCreateDebugSurfaceSuccessThenCorrectMultiHostAllocationReturned) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA, false, false, 0b1011};
    auto debugSurface = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties));

    EXPECT_NE(nullptr, debugSurface);

    EXPECT_EQ(MemoryPool::System4KBPages, debugSurface->getMemoryPool());
    EXPECT_EQ(3u, debugSurface->getNumGmms());
    EXPECT_EQ(3, mock->ioctl_cnt.gemUserptr);

    EXPECT_NE(nullptr, debugSurface->getUnderlyingBuffer());
    EXPECT_EQ(MemoryConstants::pageSize, debugSurface->getUnderlyingBufferSize());
    EXPECT_EQ(3 * MemoryConstants::pageSize, memoryManager->alignedMallocSizeRequired);

    auto gpuAddress = debugSurface->getGpuAddress();
    auto gfxPartition = memoryManager->getGfxPartition(0);
    EXPECT_NE(reinterpret_cast<uint64_t>(debugSurface->getUnderlyingBuffer()), gpuAddress);
    EXPECT_GE(GmmHelper::decanonize(gpuAddress), gfxPartition->getHeapBase(HeapIndex::HEAP_STANDARD));
    EXPECT_LT(GmmHelper::decanonize(gpuAddress), gfxPartition->getHeapLimit(HeapIndex::HEAP_STANDARD));

    auto &storageInfo = debugSurface->storageInfo;
    auto &bos = debugSurface->getBOs();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
    EXPECT_NE(nullptr, bos[1]);
    EXPECT_EQ(gpuAddress, bos[1]->peekAddress());
    EXPECT_EQ(nullptr, bos[2]);
    EXPECT_NE(nullptr, bos[3]);
    EXPECT_EQ(gpuAddress, bos[3]->peekAddress());

    EXPECT_TRUE(debugSurface->isFlushL3Required());
    EXPECT_TRUE(debugSurface->isUncacheable());
    EXPECT_EQ(debugSurface->getNumGmms(), storageInfo.getNumBanks());
    EXPECT_EQ(0b1011u, storageInfo.memoryBanks.to_ulong());
    EXPECT_EQ(0b1011u, storageInfo.pageTablesVisibility.to_ulong());
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_FALSE(storageInfo.multiStorage);
    EXPECT_FALSE(storageInfo.readOnlyMultiStorage);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_TRUE(storageInfo.cpuVisibleSegment);
    EXPECT_TRUE(storageInfo.isLockable);

    memoryManager->freeGraphicsMemory(debugSurface);
}
