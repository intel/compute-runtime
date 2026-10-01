/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/unit_test/os_interface/linux/drm_memory_manager_local_memory_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

TEST_F(DrmMemoryManagerLocalMemoryTest, givenBufferObjectCreatedInMemoryRegionWhenCreatingMultiHostDebugSurfaceAllocationThenAllocationIsReturned) {

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMmapFailureWhenCreatingMultiHostDebugSurfaceAllocationThenNullptrIsReturned) {
    memoryManager->mmapFunction = [](void *addr, size_t len, int prot, int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    EXPECT_EQ(nullptr, memoryManager->createMultiHostDebugSurfaceAllocation(allocationData));
    memoryManager->mmapFunction = SysCalls::mmap;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithBufferObjectCreationFailureThenNullptrReturned) {
    class MockDrmMemoryManagerWithBOFailure : public TestedDrmMemoryManager {
      public:
        MockDrmMemoryManagerWithBOFailure(ExecutionEnvironment &executionEnvironment) : TestedDrmMemoryManager(executionEnvironment) {}

        BufferObject *createBufferObjectInMemoryRegion(uint32_t rootDeviceIndex, Gmm *gmm, AllocationType allocationType,
                                                       uint64_t gpuAddress, size_t size, DeviceBitfield memoryBanks,
                                                       size_t maxOsContextCount, int32_t pairHandle, bool isSystemMemoryPool,
                                                       bool isUSMHostAllocation) override {
            if (shouldBOCreationFail) {
                return nullptr; // Simulate buffer object creation failure
            }
            return DrmMemoryManager::createBufferObjectInMemoryRegion(rootDeviceIndex, gmm, allocationType, gpuAddress,
                                                                      size, memoryBanks, maxOsContextCount, pairHandle,
                                                                      isSystemMemoryPool, isUSMHostAllocation);
        }

        bool shouldBOCreationFail = false;
    };

    MockDrmMemoryManagerWithBOFailure mockMemoryManager(*executionEnvironment);
    mockMemoryManager.shouldBOCreationFail = true;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    auto allocation = mockMemoryManager.createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithRetrieveMmapOffsetFailureThenNullptrReturned) {
    ioctlHelper->retrieveMmapOffsetResult = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithComplexMemoryBankPatternThenCorrectBankMappingIsUsed) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b10101011; // Banks 0, 1, 3, 5, 7 (include bank 0 for getBO() compatibility)
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    // Check that allocation has correct number of GMMs (based on num banks)
    auto numBanks = allocation->getNumGmms();
    EXPECT_GT(numBanks, 0u);

    // Check that buffer objects are created
    const auto &bufferObjects = allocation->getBOs();
    EXPECT_GE(bufferObjects.size(), numBanks); // At least as many as GMMs

    // Check that some buffer objects are valid
    uint32_t validBufferObjectCount = 0;
    for (const auto &bo : bufferObjects) {
        if (bo != nullptr) {
            validBufferObjectCount++;
        }
    }
    EXPECT_GT(validBufferObjectCount, 0u); // At least some valid buffer objects

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithBufferObjectCreationFailureInSecondTileThenNullptrReturned) {
    class MockDrmMemoryManagerWithSelectiveBOFailure : public TestedDrmMemoryManager {
      public:
        MockDrmMemoryManagerWithSelectiveBOFailure(ExecutionEnvironment &executionEnvironment) : TestedDrmMemoryManager(executionEnvironment) {}

        BufferObject *createBufferObjectInMemoryRegion(uint32_t rootDeviceIndex, Gmm *gmm, AllocationType allocationType,
                                                       uint64_t gpuAddress, size_t size, DeviceBitfield memoryBanks,
                                                       size_t maxOsContextCount, int32_t pairHandle, bool isSystemMemoryPool,
                                                       bool isUSMHostAllocation) override {
            if (creationCount == 1) { // Fail on second tile
                return nullptr;
            }
            creationCount++;
            return DrmMemoryManager::createBufferObjectInMemoryRegion(rootDeviceIndex, gmm, allocationType, gpuAddress,
                                                                      size, memoryBanks, maxOsContextCount, pairHandle,
                                                                      isSystemMemoryPool, isUSMHostAllocation);
        }

        uint32_t creationCount = 0;
    };

    MockDrmMemoryManagerWithSelectiveBOFailure mockMemoryManager(*executionEnvironment);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two tiles
    allocationData.gpuAddress = 0x1000;

    auto allocation = mockMemoryManager.createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithMultipleMemoryBanksThenCorrectBankMappingIsUsed) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1011; // Banks 0, 1 and 3 (include bank 0 for getBO() compatibility)
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    // Should have 3 buffer objects for the 3 active banks
    EXPECT_EQ(3u, allocation->getNumGmms());

    // Check that buffer objects are created
    const auto &bufferObjects = allocation->getBOs();
    EXPECT_GE(bufferObjects.size(), 3u); // At least 3 buffer objects

    // Check that some buffer objects are valid
    uint32_t validBufferObjectCount = 0;
    for (const auto &bo : bufferObjects) {
        if (bo != nullptr) {
            validBufferObjectCount++;
        }
    }
    EXPECT_GE(validBufferObjectCount, 3u); // At least 3 valid buffer objects

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithSingleBankThenCorrectMmapSizeIsSet) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1; // Single bank
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    // For single bank, host size should equal allocation size
    EXPECT_EQ(MemoryConstants::pageSize, allocation->getMmapSize());
    EXPECT_NE(nullptr, allocation->getMmapPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithMultipleBanksThenCorrectMmapSizeIsSet) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two banks
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    // For two banks, host size should be twice the allocation size
    EXPECT_EQ(2 * MemoryConstants::pageSize, allocation->getMmapSize());
    EXPECT_NE(nullptr, allocation->getMmapPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCopyDebugSurfaceToMultiTileAllocationThenCallCopyMemoryToAllocation) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two banks
    allocationData.gpuAddress = 0x1000;
    std::vector<uint8_t> dataToCopy(allocationData.size, 1u);

    AllocationType debugSurfaces[] = {AllocationType::debugContextSaveArea, AllocationType::debugSbaTrackingBuffer};

    for (auto type : debugSurfaces) {
        allocationData.type = type;
        auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
        EXPECT_NE(nullptr, allocation);

        // For two banks, host size should be twice the allocation size
        EXPECT_EQ(2 * MemoryConstants::pageSize, allocation->getMmapSize());
        EXPECT_NE(nullptr, allocation->getMmapPtr());

        auto ret = memoryManager->copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
        EXPECT_TRUE(ret);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenMemsetMultiTileDebugSurfaceAllocationThenCallMemsetAllocation) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two banks
    allocationData.gpuAddress = 0x1000;
    int value = 0x42;

    AllocationType debugSurfaces[] = {AllocationType::debugContextSaveArea, AllocationType::debugSbaTrackingBuffer};

    for (auto type : debugSurfaces) {
        allocationData.type = type;
        auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
        EXPECT_NE(nullptr, allocation);

        // For two banks, host size should be twice the allocation size
        EXPECT_EQ(2 * MemoryConstants::pageSize, allocation->getMmapSize());
        EXPECT_NE(nullptr, allocation->getMmapPtr());

        auto ret = memoryManager->memsetAllocation(allocation, 0, value, allocationData.size);
        EXPECT_TRUE(ret);

        auto ptr = static_cast<uint8_t *>(allocation->getMmapPtr());
        for (size_t i = 0; i < allocationData.size; i++) {
            EXPECT_EQ(value, ptr[i]) << "Bank 0, byte at index " << i << " mismatch";
        }
        for (size_t i = 0; i < allocationData.size; i++) {
            EXPECT_EQ(value, ptr[allocationData.size + i]) << "Bank 1, byte at index " << i << " mismatch";
        }

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMoreThanOneSubDevicesWhenAllocatingDebugSbaTrackingBufferWithGpuVaThenAllTilesAreHandled) {
    // device creation resets MultiTileArchInfo; redeclare to exercise the real multi-tile path
    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 4;

    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::debugSbaTrackingBuffer, false, false, 0b1011};
    debugSurfaceProperties.gpuAddress = 0x12340000;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenOneSubDeviceWhenAllocatingDebugSbaTrackingBufferWithGpuVaThenTheSingleTileIsHandled) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::debugSbaTrackingBuffer, false, false, 0b1};
    debugSurfaceProperties.gpuAddress = 0x12340000;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithValidInputThenSuccessfulAllocationReturned) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(AllocationType::debugContextSaveArea, allocation->getAllocationType());
    EXPECT_EQ(MemoryConstants::pageSize, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->isFlushL3Required());
    EXPECT_TRUE(allocation->isUncacheable());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithZeroGpuAddressThenAddressIsAcquired) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0; // Zero address

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_NE(nullptr, allocation->getReservedAddressPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithStorageInfoThenStorageInfoIsSet) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1011; // Banks 0, 1 and 3 (include bank 0 for getBO() compatibility)
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(allocationData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocationData.storageInfo.getNumBanks(), allocation->storageInfo.getNumBanks());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateDebugSurfaceWithUnalignedSizeCalledThenNullptrReturned) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize + 101, NEO::AllocationType::debugContextSaveArea, false, false, 0b1011};
    auto debugSurface = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    EXPECT_EQ(nullptr, debugSurface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithUnalignedSizeThenNullptrReturned) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize + 101;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithNonZeroGpuAddressThenProvidedAddressIsUsed) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11;
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    ASSERT_NE(nullptr, allocation);
    auto gmmHelper = memoryManager->getGmmHelper(0);
    EXPECT_EQ(gmmHelper->canonize(0x1000), allocation->getGpuAddress());
    EXPECT_EQ(nullptr, allocation->getReservedAddressPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithOsContextThenOsContextIsSet) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11;
    allocationData.gpuAddress = 0x1000;

    auto osContext = static_cast<OsContextLinux *>(device->getDefaultEngine().osContext);
    allocationData.osContext = osContext;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(osContext, allocation->getOsContext());

    memoryManager->freeGraphicsMemory(allocation);
}

} // namespace NEO
