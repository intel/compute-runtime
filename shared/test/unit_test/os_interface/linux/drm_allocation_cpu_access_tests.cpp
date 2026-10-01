/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/test/unit_test/os_interface/linux/drm_memory_manager_local_memory_fixture.h"

#include "gtest/gtest.h"

#include <array>
#include <cstring>

namespace NEO {

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

struct DrmMemoryManagerToTestMemsetAllocation : public DrmMemoryManager {
    using DrmMemoryManager::allocateGraphicsMemoryInDevicePool;
    DrmMemoryManagerToTestMemsetAllocation(ExecutionEnvironment &executionEnvironment, bool localMemoryEnabled, size_t lockableLocalMemorySize)
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
    bool memsetAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, int value, size_t sizeToSet, DeviceBitfield handleMask) override {
        memsetAllocationBanksCalled++;
        return DrmMemoryManager::memsetAllocationBanks(graphicsAllocation, destinationOffset, value, sizeToSet, handleMask);
    }
    std::array<std::unique_ptr<uint8_t[]>, 4> lockedLocalMemory;
    uint32_t deviceIndex = 3;
    size_t lockedLocalMemorySize = 0;
    uint32_t memsetAllocationBanksCalled = 0;
};

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    auto ptr = memoryManager->lockBufferObject(nullptr);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockBufferObject(nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = destinationAllocationSize;
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationFailsToLockResourceThenItReturnsFalse) {
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationWithCpuPtrThenAllocationIsFilledWithCorrectData) {
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnBufferObjectThenReturnPtr) {
    BufferObject bo(0, mock, 3, 1, 1024, 1);

    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager->lockBufferObject(&bo);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());

    memoryManager->unlockBufferObject(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryThenCallIoctlGemMapOffsetAndReturnLockedPtr, IsAtMostXeCore) {
    mockExp->ioctlExpected.gemCreateExt = 1;
    mockExp->ioctlExpected.gemWait = 1;
    mockExp->ioctlExpected.gemClose = 1;
    mockExp->ioctlExpected.gemMmapOffset = 1;
    mockExp->memoryInfo.reset(new MockMemoryInfo(*mockExp));

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::internalHeap;
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_NE(nullptr, drmAllocation->getBO()->peekLockedAddress());

    EXPECT_EQ(static_cast<uint32_t>(drmAllocation->getBO()->peekHandle()), mockExp->mmapOffsetHandle);
    EXPECT_EQ(0u, mockExp->mmapOffsetPad);
    EXPECT_EQ(0u, mockExp->mmapOffsetExpected);
    EXPECT_EQ(4u, mockExp->mmapOffsetFlags);

    memoryManager->unlockResource(allocation);
    EXPECT_EQ(nullptr, drmAllocation->getBO()->peekLockedAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnMmapThenReturnNullPtr, IsAtMostXeCore) {
    mockExp->ioctlExpected.gemMmapOffset = 2;
    DrmMockCustom::IoctlResExt ioctlResExtMmapFailure = {mockExp->ioctlCnt.total, -1};
    mockExp->ioctlResExt = &ioctlResExtMmapFailure;

    BufferObject bo(0, mockExp, 3, 1, 0, 0);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mockExp->ioctlResExt = &mockExp->none;
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnIoctlMmapFunctionOffsetThenReturnNullPtr, IsAtMostXeCore) {
    mockExp->ioctlExpected.gemMmapOffset = 2;
    mockExp->returnIoctlExtraErrorValue = true;
    mockExp->failOnMmapOffset = true;

    BufferObject bo(0, mockExp, 3, 1, 0, 0);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mockExp->ioctlResExt = &mockExp->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButBufferObjectIsNullThenReturnNullPtr) {
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t allocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = allocationSize + offset;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.memsetAllocation(allocation, offset, value, allocationSize);
    EXPECT_TRUE(ret);

    auto ptr = ptrOffset(drmMemoryManager.lockedLocalMemory[0].get(), offset);
    for (size_t i = 0; i < allocationSize; i++) {
        EXPECT_EQ(value, static_cast<uint8_t *>(ptr)[i]);
    }

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationFailsToLockResourceThenItReturnsFalse) {
    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, 0);
    size_t allocationSize = MemoryConstants::pageSize;
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.memsetAllocation(allocation, 0, value, allocationSize);
    EXPECT_FALSE(ret);

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationWithCpuPtrThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t allocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = allocationSize + offset;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, false, 0);
    int value = 0x42;

    auto allocation = drmMemoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, destinationAllocationSize, AllocationType::kernelIsa, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.memsetAllocation(allocation, offset, value, allocationSize);
    EXPECT_TRUE(ret);

    auto ptr = ptrOffset(allocation->getUnderlyingBuffer(), offset);
    for (size_t i = 0; i < allocationSize; i++) {
        EXPECT_EQ(value, static_cast<uint8_t *>(ptr)[i]);
    }

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenCopyMemoryToMultiTileAllocationThenCallCopyMemoryToAllocationBanks) {
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

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationOnAllMemoryBanksReturnsSuccessThenAllocationIsFilledWithCorrectData) {
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

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetMultiTileAllocationThenCallMemsetAllocationBanks) {
    size_t allocationSize = MemoryConstants::pageSize;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, allocationSize);
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::constantSurface;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    char data = 0;
    allocation->setCpuPtrAndGpuAddress(&data, allocation->getGpuAddress());

    auto ret = drmMemoryManager.memsetAllocation(allocation, 0, value, allocationSize);
    EXPECT_TRUE(ret);
    EXPECT_EQ(1u, drmMemoryManager.memsetAllocationBanksCalled);
    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationOnAllMemoryBanksReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t allocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = allocationSize + offset;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.memsetAllocation(allocation, offset, value, allocationSize);
    EXPECT_TRUE(ret);

    for (auto index = 0u; index < 3; index++) {
        auto ptr = ptrOffset(drmMemoryManager.lockedLocalMemory[index].get(), offset);
        for (size_t i = 0; i < allocationSize; i++) {
            EXPECT_EQ(value, static_cast<uint8_t *>(ptr)[i]);
        }
    }

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationOnSelectiveMemoryBanksThenOnlySelectedBanksAreFilled) {
    size_t offset = 3;
    size_t allocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = allocationSize + offset;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    DeviceBitfield memsetBanks = 0b0101;

    auto ret = drmMemoryManager.memsetAllocationBanks(allocation, offset, value, allocationSize, memsetBanks);
    EXPECT_TRUE(ret);

    for (auto lockedIndex = 0u; lockedIndex < 2u; lockedIndex++) {
        ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[lockedIndex]);
        auto ptr = ptrOffset(drmMemoryManager.lockedLocalMemory[lockedIndex].get(), offset);
        for (size_t i = 0; i < allocationSize; i++) {
            EXPECT_EQ(value, static_cast<uint8_t *>(ptr)[i]);
        }
    }

    // Banks 1 and 3 were never locked
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[2]);
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[3]);

    drmMemoryManager.freeGraphicsMemory(allocation);
}

} // namespace NEO
