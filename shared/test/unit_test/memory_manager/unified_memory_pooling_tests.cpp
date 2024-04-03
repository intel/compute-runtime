/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <array>
using namespace NEO;

using UnifiedMemoryPoolingTest = Test<SVMMemoryAllocatorFixture<true>>;
TEST_F(UnifiedMemoryPoolingTest, givenUsmAllocPoolWhenCallingIsInitializedThenReturnCorrectValue) {
    UsmMemAllocPool usmMemAllocPool;
    EXPECT_FALSE(usmMemAllocPool.isInitialized());

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    EXPECT_TRUE(usmMemAllocPool.initialize(svmManager.get(), unifiedMemoryProperties, 1 * MemoryConstants::megaByte));
    EXPECT_TRUE(usmMemAllocPool.isInitialized());

    usmMemAllocPool.cleanup();
    EXPECT_FALSE(usmMemAllocPool.isInitialized());
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(reinterpret_cast<void *>(0x1), true));
}

template <InternalMemoryType poolMemoryType, bool failAllocation>
class InitializedUnifiedMemoryPoolingTest : public UnifiedMemoryPoolingTest {
  public:
    void SetUp() override {
        UnifiedMemoryPoolingTest::setUp();
        EXPECT_FALSE(usmMemAllocPool.isInitialized());

        deviceFactory = std::unique_ptr<UltDeviceFactory>(new UltDeviceFactory(1, 1));
        device = deviceFactory->rootDevices[0];
        svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
        static_cast<MockMemoryManager *>(device->getMemoryManager())->failInDevicePoolWithError = failAllocation;

        poolMemoryProperties = std::make_unique<SVMAllocsManager::UnifiedMemoryProperties>(poolMemoryType, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
        poolMemoryProperties->device = device;
        ASSERT_EQ(!failAllocation, usmMemAllocPool.initialize(svmManager.get(), *poolMemoryProperties.get(), poolSize));
    }
    void TearDown() override {
        usmMemAllocPool.cleanup();
        UnifiedMemoryPoolingTest::tearDown();
    }

    const size_t poolSize = 2 * MemoryConstants::megaByte;
    MockUsmMemAllocPool usmMemAllocPool;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    Device *device;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    std::unique_ptr<SVMAllocsManager::UnifiedMemoryProperties> poolMemoryProperties;
};

using InitializedHostUnifiedMemoryPoolingTest = InitializedUnifiedMemoryPoolingTest<InternalMemoryType::hostUnifiedMemory, false>;
TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenDifferentAllocationSizesWhenCallingCanBePooledThenCorrectValueIsReturned) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold + 1, memoryProperties));

    memoryProperties.memoryType = InternalMemoryType::sharedUnifiedMemory;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold + 1, memoryProperties));

    memoryProperties.memoryType = InternalMemoryType::deviceUnifiedMemory;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold + 1, memoryProperties));

    memoryProperties.memoryType = InternalMemoryType::hostUnifiedMemory;
    memoryProperties.allocationFlags.allFlags = 1u;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold + 1, memoryProperties));
    memoryProperties.allocationFlags.allFlags = 0u;
    memoryProperties.allocationFlags.allAllocFlags = 1u;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold + 1, memoryProperties));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenVariousPointersWhenCallingIsInPoolThenCorrectValueIsReturned) {
    void *ptrBeforePool = reinterpret_cast<void *>(reinterpret_cast<size_t>(usmMemAllocPool.pool) - 1);
    void *lastPtrInPool = reinterpret_cast<void *>(reinterpret_cast<size_t>(usmMemAllocPool.poolEnd) - 1);

    EXPECT_FALSE(usmMemAllocPool.isInPool(ptrBeforePool));
    EXPECT_TRUE(usmMemAllocPool.isInPool(usmMemAllocPool.pool));
    EXPECT_TRUE(usmMemAllocPool.isInPool(lastPtrInPool));
    EXPECT_FALSE(usmMemAllocPool.isInPool(usmMemAllocPool.poolEnd));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenAlignmentsWhenCallingAlignmentIsAllowedThenCorrectValueIsReturned) {
    EXPECT_FALSE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment / 2));
    EXPECT_TRUE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment));
    EXPECT_FALSE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment + UsmMemAllocPool::chunkAlignment / 2));
    EXPECT_TRUE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment * 2));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenPoolableAllocationWhenUsingPoolThenAllocationIsPooledUnlessPoolIsFull) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = UsmMemAllocPool::allocationThreshold;
    const auto allocationSizeAboveThreshold = allocationSize + 1;
    EXPECT_EQ(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSizeAboveThreshold, memoryProperties));
    EXPECT_EQ(nullptr, usmMemAllocPool.allocations.get(reinterpret_cast<void *>(0x1)));

    auto allocFromPool = usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties);
    EXPECT_NE(nullptr, allocFromPool);
    EXPECT_TRUE(usmMemAllocPool.isInPool(allocFromPool));
    auto allocationInfo = usmMemAllocPool.allocations.get(allocFromPool);
    EXPECT_NE(nullptr, allocationInfo);
    EXPECT_EQ(allocationSize, allocationInfo->size);

    auto svmData = svmManager->getSVMAlloc(allocFromPool);
    auto poolSvmData = svmManager->getSVMAlloc(usmMemAllocPool.pool);
    EXPECT_EQ(svmData, poolSvmData);

    const auto allocationsToFillPool = poolSize / allocationSize;
    for (auto i = 1u; i < allocationsToFillPool; ++i) {
        // exhaust pool
        EXPECT_NE(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties));
    }

    EXPECT_EQ(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(1, memoryProperties));

    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(reinterpret_cast<void *>(0x1), true));
    EXPECT_TRUE(usmMemAllocPool.freeSVMAlloc(allocFromPool, true));
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(allocFromPool, true));
    EXPECT_EQ(nullptr, usmMemAllocPool.allocations.get(reinterpret_cast<void *>(0x1)));
    EXPECT_EQ(nullptr, usmMemAllocPool.allocations.extract(reinterpret_cast<void *>(0x1)));

    EXPECT_NE(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenVariousAlignmentsWhenUsingPoolThenAddressIsAligned) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, 0u, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = UsmMemAllocPool::allocationThreshold;

    std::array<size_t, 8> alignmentsToCheck = {UsmMemAllocPool::chunkAlignment,
                                               UsmMemAllocPool::chunkAlignment * 2,
                                               UsmMemAllocPool::chunkAlignment * 4,
                                               UsmMemAllocPool::chunkAlignment * 8,
                                               UsmMemAllocPool::chunkAlignment * 16,
                                               UsmMemAllocPool::chunkAlignment * 32,
                                               UsmMemAllocPool::chunkAlignment * 64,
                                               UsmMemAllocPool::chunkAlignment * 128};
    for (const auto &alignment : alignmentsToCheck) {
        if (alignment > UsmMemAllocPool::allocationThreshold) {
            break;
        }
        memoryProperties.alignment = alignment;
        auto allocFromPool = usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties);
        EXPECT_NE(nullptr, allocFromPool);
        EXPECT_TRUE(usmMemAllocPool.isInPool(allocFromPool));
        auto address = castToUint64(allocFromPool);
        EXPECT_EQ(0u, address % alignment);

        EXPECT_TRUE(usmMemAllocPool.freeSVMAlloc(allocFromPool, true));
    }
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenPoolableAllocationWhenGettingSizeAndBasePtrThenCorrectValuesAreReturned) {
    const auto bogusPtr = reinterpret_cast<void *>(0x1);
    EXPECT_EQ(nullptr, usmMemAllocPool.getPooledAllocationBasePtr(bogusPtr));
    EXPECT_EQ(0u, usmMemAllocPool.getPooledAllocationSize(bogusPtr));

    const auto ptrInPoolButNotAllocated = usmMemAllocPool.pool;
    EXPECT_EQ(nullptr, usmMemAllocPool.getPooledAllocationBasePtr(ptrInPoolButNotAllocated));
    EXPECT_EQ(0u, usmMemAllocPool.getPooledAllocationSize(ptrInPoolButNotAllocated));

    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto requestedAllocSize = 1 * MemoryConstants::kiloByte;
    EXPECT_GT(usmMemAllocPool.allocationThreshold, requestedAllocSize + usmMemAllocPool.chunkAlignment);

    // we want an allocation from the middle of the pool for testing
    auto unusedAlloc = usmMemAllocPool.createUnifiedMemoryAllocation(requestedAllocSize, memoryProperties);
    EXPECT_NE(nullptr, unusedAlloc);
    EXPECT_TRUE(usmMemAllocPool.isInPool(unusedAlloc));
    auto allocFromPool = usmMemAllocPool.createUnifiedMemoryAllocation(requestedAllocSize, memoryProperties);
    EXPECT_NE(nullptr, allocFromPool);
    EXPECT_TRUE(usmMemAllocPool.isInPool(allocFromPool));
    auto allocInfo = usmMemAllocPool.allocations.get(allocFromPool);
    EXPECT_NE(nullptr, allocInfo);
    auto actualAllocSize = allocInfo->size;
    EXPECT_GE(actualAllocSize, requestedAllocSize);

    EXPECT_TRUE(usmMemAllocPool.freeSVMAlloc(unusedAlloc, true));

    auto offsetPointer = ptrOffset(allocFromPool, actualAllocSize - 1);
    auto pastEndPointer = ptrOffset(allocFromPool, actualAllocSize);

    EXPECT_TRUE(usmMemAllocPool.isInPool(offsetPointer));
    EXPECT_TRUE(usmMemAllocPool.isInPool(pastEndPointer));

    EXPECT_EQ(0u, usmMemAllocPool.getPooledAllocationSize(bogusPtr));
    EXPECT_EQ(0u, usmMemAllocPool.getPooledAllocationSize(usmMemAllocPool.pool));
    EXPECT_EQ(requestedAllocSize, usmMemAllocPool.getPooledAllocationSize(allocFromPool));
    EXPECT_EQ(requestedAllocSize, usmMemAllocPool.getPooledAllocationSize(offsetPointer));
    EXPECT_EQ(0u, usmMemAllocPool.getPooledAllocationSize(pastEndPointer));

    EXPECT_EQ(nullptr, usmMemAllocPool.getPooledAllocationBasePtr(bogusPtr));
    EXPECT_EQ(nullptr, usmMemAllocPool.getPooledAllocationBasePtr(usmMemAllocPool.pool));
    EXPECT_EQ(allocFromPool, usmMemAllocPool.getPooledAllocationBasePtr(allocFromPool));
    EXPECT_EQ(allocFromPool, usmMemAllocPool.getPooledAllocationBasePtr(offsetPointer));
    EXPECT_EQ(nullptr, usmMemAllocPool.getPooledAllocationBasePtr(pastEndPointer));
}

using InitializationFailedUnifiedMemoryPoolingTest = InitializedUnifiedMemoryPoolingTest<InternalMemoryType::hostUnifiedMemory, true>;
TEST_F(InitializationFailedUnifiedMemoryPoolingTest, givenNotInitializedPoolWhenUsingPoolThenMethodsSucceed) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = UsmMemAllocPool::allocationThreshold;
    EXPECT_EQ(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties));
    const auto bogusPtr = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(bogusPtr, true));
    EXPECT_EQ(0u, usmMemAllocPool.getPooledAllocationSize(bogusPtr));
    EXPECT_EQ(nullptr, usmMemAllocPool.getPooledAllocationBasePtr(bogusPtr));
}
