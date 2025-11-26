/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/pool_info.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "gtest/gtest.h"

#include <array>
using namespace NEO;

using UnifiedMemoryPoolingStaticTest = ::testing::Test;
TEST_F(UnifiedMemoryPoolingStaticTest, givenUsmAllocPoolWhenCallingStaticMethodsThenReturnCorrectValues) {
    EXPECT_EQ(0.08, UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(0.02, UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(InternalMemoryType::hostUnifiedMemory));
    EXPECT_EQ(0.00, UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(InternalMemoryType::sharedUnifiedMemory));

    EXPECT_TRUE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::chunkAlignment / 2));
    EXPECT_TRUE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::chunkAlignment));
    EXPECT_TRUE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::chunkAlignment * 2));
    EXPECT_TRUE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::poolAlignment));
    EXPECT_FALSE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::poolAlignment * 2));

    const RootDeviceIndicesContainer rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> deviceBitfields;
    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(UsmMemAllocPool::flagsAreAllowed(unifiedMemoryProperties));
    unifiedMemoryProperties.allocationFlags.allFlags = 1u;
    unifiedMemoryProperties.allocationFlags.allAllocFlags = 0u;
    EXPECT_FALSE(UsmMemAllocPool::flagsAreAllowed(unifiedMemoryProperties));
    unifiedMemoryProperties.allocationFlags.allFlags = 0u;
    unifiedMemoryProperties.allocationFlags.allAllocFlags = 1u;
    EXPECT_FALSE(UsmMemAllocPool::flagsAreAllowed(unifiedMemoryProperties));
    unifiedMemoryProperties.allocationFlags.allFlags = 0u;
    unifiedMemoryProperties.allocationFlags.allAllocFlags = 0u;
    unifiedMemoryProperties.allocationFlags.hostptr = 0x1u;
    EXPECT_FALSE(UsmMemAllocPool::flagsAreAllowed(unifiedMemoryProperties));
}

using UnifiedMemoryPoolingTest = Test<SVMMemoryAllocatorFixture<true>>;

TEST_F(UnifiedMemoryPoolingTest, givenUsmAllocPoolWhenCallingIsInitializedThenReturnCorrectValue) {
    MockUsmMemAllocPool usmMemAllocPool;
    EXPECT_FALSE(usmMemAllocPool.isInitialized());
    EXPECT_EQ(0u, usmMemAllocPool.getPoolAddress());

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(usmMemAllocPool.initialize(svmManager.get(), unifiedMemoryProperties, 1 * MemoryConstants::megaByte, 0u, 1 * MemoryConstants::megaByte));
    EXPECT_TRUE(usmMemAllocPool.isInitialized());
    EXPECT_EQ(castToUint64(usmMemAllocPool.pool), usmMemAllocPool.getPoolAddress());

    usmMemAllocPool.cleanup();
    EXPECT_FALSE(usmMemAllocPool.isInitialized());
    EXPECT_EQ(0u, usmMemAllocPool.getPoolAddress());
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(reinterpret_cast<void *>(0x1), true));
}

TEST_F(UnifiedMemoryPoolingTest, givenUsmAllocPoolWhenCallingResidencyOperationsThenTrackResidencyCounts) {
    MockUsmMemAllocPool usmMemAllocPool;
    const auto poolBase = addrToPtr(0xFF00u);
    std::array<void *, 3> pooledPtrs = {poolBase, ptrOffset(poolBase, MemoryConstants::pageSize), ptrOffset(poolBase, 2 * MemoryConstants::pageSize)};

    usmMemAllocPool.allocations.insert(pooledPtrs[0], UsmMemAllocPool::AllocationInfo{
                                                          .address = castToUint64(pooledPtrs[0]),
                                                          .size = MemoryConstants::pageSize,
                                                          .requestedSize = MemoryConstants::pageSize,
                                                          .isResident = false,
                                                      });
    auto firstInfo = usmMemAllocPool.allocations.get(pooledPtrs[0]);
    ASSERT_NE(nullptr, firstInfo);
    EXPECT_EQ(pooledPtrs[0], addrToPtr(firstInfo->address));

    usmMemAllocPool.allocations.insert(pooledPtrs[1], UsmMemAllocPool::AllocationInfo{
                                                          .address = castToUint64(pooledPtrs[1]),
                                                          .size = MemoryConstants::pageSize,
                                                          .requestedSize = MemoryConstants::pageSize,
                                                          .isResident = false,
                                                      });
    auto secondInfo = usmMemAllocPool.allocations.get(pooledPtrs[1]);
    ASSERT_NE(nullptr, secondInfo);
    EXPECT_EQ(pooledPtrs[1], addrToPtr(secondInfo->address));

    using Op = UsmMemAllocPool::ResidencyOperationType;

    usmMemAllocPool.pool = pooledPtrs[0];
    usmMemAllocPool.poolInfo.poolSize = 3 * MemoryConstants::pageSize;
    usmMemAllocPool.poolEnd = ptrOffset(usmMemAllocPool.pool, usmMemAllocPool.poolInfo.poolSize);
    MockMemoryOperations mockMemoryOperationsHandler;
    usmMemAllocPool.memoryOperationsIface = &mockMemoryOperationsHandler;
    MockDevice mockDevice;
    usmMemAllocPool.device = &mockDevice;
    EXPECT_FALSE(usmMemAllocPool.isTrackingResidency());
    usmMemAllocPool.enableResidencyTracking();
    EXPECT_TRUE(usmMemAllocPool.trackResidency);
    EXPECT_TRUE(usmMemAllocPool.isTrackingResidency());
    MockGraphicsAllocation mockGfxAlloc;
    usmMemAllocPool.allocation = &mockGfxAlloc;
    EXPECT_TRUE(usmMemAllocPool.isInitialized());

    // ptr in pool but not allocated -> error
    const auto notAllocatedPtrInPool = pooledPtrs[2];
    EXPECT_TRUE(usmMemAllocPool.isInPool(notAllocatedPtrInPool));
    auto expectedMakeResidentCount = mockMemoryOperationsHandler.makeResidentCalledCount;
    auto expectedEvictCount = mockMemoryOperationsHandler.evictCalledCount;
    EXPECT_EQ(MemoryOperationsStatus::memoryNotFound, usmMemAllocPool.residencyOperation<Op::makeResident>(notAllocatedPtrInPool));
    EXPECT_EQ(MemoryOperationsStatus::memoryNotFound, usmMemAllocPool.residencyOperation<Op::evict>(notAllocatedPtrInPool));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);

    // ptr in pool, make resident -> make resident
    EXPECT_FALSE(firstInfo->isResident);
    EXPECT_FALSE(usmMemAllocPool.residencyCount);
    EXPECT_EQ(MemoryOperationsStatus::success, usmMemAllocPool.residencyOperation<Op::makeResident>(pooledPtrs[0]));
    EXPECT_EQ(++expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);
    EXPECT_TRUE(firstInfo->isResident);
    EXPECT_FALSE(secondInfo->isResident);
    EXPECT_EQ(1u, usmMemAllocPool.residencyCount);

    // ptr in pool already resident -> skip
    EXPECT_EQ(MemoryOperationsStatus::success, usmMemAllocPool.residencyOperation<Op::makeResident>(pooledPtrs[0]));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);
    EXPECT_TRUE(firstInfo->isResident);
    EXPECT_FALSE(secondInfo->isResident);
    EXPECT_EQ(1u, usmMemAllocPool.residencyCount);

    // second ptr in pool make resident -> skip
    EXPECT_EQ(MemoryOperationsStatus::success, usmMemAllocPool.residencyOperation<Op::makeResident>(pooledPtrs[1]));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);
    EXPECT_TRUE(firstInfo->isResident);
    EXPECT_TRUE(secondInfo->isResident);
    EXPECT_EQ(2u, usmMemAllocPool.residencyCount);

    // first ptr evict -> skip
    EXPECT_EQ(MemoryOperationsStatus::success, usmMemAllocPool.residencyOperation<Op::evict>(pooledPtrs[0]));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);
    EXPECT_FALSE(firstInfo->isResident);
    EXPECT_TRUE(secondInfo->isResident);
    EXPECT_EQ(1u, usmMemAllocPool.residencyCount);

    // second ptr evict -> evict
    EXPECT_EQ(MemoryOperationsStatus::success, usmMemAllocPool.residencyOperation<Op::evict>(pooledPtrs[1]));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(++expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);
    EXPECT_FALSE(firstInfo->isResident);
    EXPECT_FALSE(secondInfo->isResident);
    EXPECT_EQ(0u, usmMemAllocPool.residencyCount);

    // evict already evicted ptr -> skip
    EXPECT_EQ(MemoryOperationsStatus::success, usmMemAllocPool.residencyOperation<Op::evict>(pooledPtrs[1]));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);
    EXPECT_FALSE(firstInfo->isResident);
    EXPECT_FALSE(secondInfo->isResident);
    EXPECT_EQ(0u, usmMemAllocPool.residencyCount);

    class MockHeapAllocator : public HeapAllocator {
      public:
        MockHeapAllocator() : HeapAllocator(0, 0){};
        void free(uint64_t ptr, size_t size) override {}
    };
    usmMemAllocPool.chunkAllocator = std::make_unique<MockHeapAllocator>();
    MockSVMAllocsManager mockSvmManager(nullptr);
    usmMemAllocPool.svmMemoryManager = &mockSvmManager;

    usmMemAllocPool.allocations.insert(pooledPtrs[2], UsmMemAllocPool::AllocationInfo{
                                                          .address = castToUint64(pooledPtrs[2]),
                                                          .size = MemoryConstants::pageSize,
                                                          .requestedSize = MemoryConstants::pageSize,
                                                          .isResident = 0u,
                                                      });
    EXPECT_EQ(MemoryOperationsStatus::success, usmMemAllocPool.residencyOperation<Op::makeResident>(pooledPtrs[1]));
    EXPECT_EQ(MemoryOperationsStatus::success, usmMemAllocPool.residencyOperation<Op::makeResident>(pooledPtrs[2]));
    EXPECT_EQ(++expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);
    ASSERT_FALSE(firstInfo->isResident);
    ASSERT_TRUE(secondInfo->isResident);
    auto thirdInfo = usmMemAllocPool.allocations.get(pooledPtrs[2]);
    ASSERT_TRUE(thirdInfo->isResident);
    ASSERT_EQ(2u, usmMemAllocPool.residencyCount);

    // free not resident chunk -> skip
    EXPECT_TRUE(usmMemAllocPool.freeSVMAlloc(pooledPtrs[0], false));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);

    // free resident chunk while other chunk is resident -> skip
    EXPECT_TRUE(usmMemAllocPool.freeSVMAlloc(pooledPtrs[1], false));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);

    // free last resident chunk -> evict
    EXPECT_TRUE(usmMemAllocPool.freeSVMAlloc(pooledPtrs[2], false));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler.makeResidentCalledCount);
    EXPECT_EQ(++expectedEvictCount, mockMemoryOperationsHandler.evictCalledCount);

    EXPECT_EQ(0u, usmMemAllocPool.residencyCount);
}

template <InternalMemoryType poolMemoryType, bool failAllocation>
class InitializedUnifiedMemoryPoolingTest : public UnifiedMemoryPoolingTest {
  public:
    void SetUp() override {
        UnifiedMemoryPoolingTest::setUp();
        EXPECT_FALSE(usmMemAllocPool.isInitialized());

        deviceFactory = std::unique_ptr<UltDeviceFactory>(new UltDeviceFactory(1, 1));
        device = deviceFactory->rootDevices[0];
        svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        static_cast<MockMemoryManager *>(device->getMemoryManager())->failInDevicePoolWithError = failAllocation;

        poolMemoryProperties = std::make_unique<UnifiedMemoryProperties>(poolMemoryType, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
        poolMemoryProperties->device = device;
        ASSERT_EQ(!failAllocation, usmMemAllocPool.initialize(svmManager.get(), *poolMemoryProperties.get(), poolSize, 0u, poolAllocationThreshold));
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
    std::unique_ptr<UnifiedMemoryProperties> poolMemoryProperties;
    constexpr static auto poolAllocationThreshold = 1 * MemoryConstants::megaByte;
};

using InitializedDeviceUnifiedMemoryPoolingTest = InitializedUnifiedMemoryPoolingTest<InternalMemoryType::deviceUnifiedMemory, false>;
TEST_F(InitializedDeviceUnifiedMemoryPoolingTest, givenDevicePoolWhenInitializedThenFieldsRequiredForResidencyOperationsSet) {
    EXPECT_EQ(device, usmMemAllocPool.device);
    auto poolGfxAllocation = svmManager->getSVMAlloc(usmMemAllocPool.pool)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_EQ(poolGfxAllocation, usmMemAllocPool.allocation);
    EXPECT_EQ(device->getRootDeviceEnvironment().memoryOperationsInterface.get(), usmMemAllocPool.memoryOperationsIface);
}

using InitializedHostUnifiedMemoryPoolingTest = InitializedUnifiedMemoryPoolingTest<InternalMemoryType::hostUnifiedMemory, false>;
TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenDifferentAllocationSizesWhenCallingCanBePooledThenCorrectValueIsReturned) {
    UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(usmMemAllocPool.canBePooled(poolAllocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold + 1, memoryProperties));

    memoryProperties.memoryType = InternalMemoryType::sharedUnifiedMemory;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold + 1, memoryProperties));

    memoryProperties.memoryType = InternalMemoryType::deviceUnifiedMemory;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold + 1, memoryProperties));

    memoryProperties.memoryType = InternalMemoryType::hostUnifiedMemory;
    memoryProperties.allocationFlags.allFlags = 1u;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold + 1, memoryProperties));

    memoryProperties.allocationFlags.allFlags = 0u;
    memoryProperties.allocationFlags.allAllocFlags = 1u;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold + 1, memoryProperties));

    memoryProperties.allocationFlags.allAllocFlags = 0u;
    constexpr auto notAllowedAlignment = UsmMemAllocPool::poolAlignment * 2;
    memoryProperties.alignment = notAllowedAlignment;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(poolAllocationThreshold + 1, memoryProperties));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenVariousPointersWhenCallingIsInPoolAndGetOffsetInPoolThenCorrectValuesAreReturned) {
    void *ptrBeforePool = reinterpret_cast<void *>(reinterpret_cast<size_t>(usmMemAllocPool.pool) - 1);
    void *lastPtrInPool = reinterpret_cast<void *>(reinterpret_cast<size_t>(usmMemAllocPool.poolEnd) - 1);

    EXPECT_FALSE(usmMemAllocPool.isInPool(ptrBeforePool));
    EXPECT_EQ(0u, usmMemAllocPool.getOffsetInPool(ptrBeforePool));

    EXPECT_TRUE(usmMemAllocPool.isInPool(usmMemAllocPool.pool));
    EXPECT_EQ(0u, usmMemAllocPool.getOffsetInPool(usmMemAllocPool.pool));

    EXPECT_TRUE(usmMemAllocPool.isInPool(lastPtrInPool));
    EXPECT_EQ(ptrDiff(lastPtrInPool, usmMemAllocPool.pool), usmMemAllocPool.getOffsetInPool(lastPtrInPool));

    EXPECT_FALSE(usmMemAllocPool.isInPool(usmMemAllocPool.poolEnd));
    EXPECT_EQ(0u, usmMemAllocPool.getOffsetInPool(usmMemAllocPool.poolEnd));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenAlignmentsWhenCallingAlignmentIsAllowedThenCorrectValueIsReturned) {
    EXPECT_TRUE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment / 2));
    EXPECT_TRUE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment));
    EXPECT_TRUE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment * 2));
    EXPECT_TRUE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::poolAlignment));
    EXPECT_FALSE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::poolAlignment * 2));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenPoolableAllocationWhenUsingPoolThenAllocationIsPooledUnlessPoolIsFull) {
    UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = poolAllocationThreshold;
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
    UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, 0u, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = poolAllocationThreshold;

    std::array<size_t, 8> alignmentsToCheck = {UsmMemAllocPool::chunkAlignment,
                                               UsmMemAllocPool::chunkAlignment * 2,
                                               UsmMemAllocPool::chunkAlignment * 4,
                                               UsmMemAllocPool::chunkAlignment * 8,
                                               UsmMemAllocPool::chunkAlignment * 16,
                                               UsmMemAllocPool::chunkAlignment * 32,
                                               UsmMemAllocPool::chunkAlignment * 64,
                                               UsmMemAllocPool::chunkAlignment * 128};
    for (const auto &alignment : alignmentsToCheck) {
        if (alignment > poolAllocationThreshold) {
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

    UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto requestedAllocSize = 1 * MemoryConstants::kiloByte;
    EXPECT_GT(poolAllocationThreshold, requestedAllocSize + usmMemAllocPool.chunkAlignment);

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
    UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = poolAllocationThreshold;
    EXPECT_EQ(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties));
    const auto bogusPtr = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(bogusPtr, true));
    EXPECT_EQ(0u, usmMemAllocPool.getPooledAllocationSize(bogusPtr));
    EXPECT_EQ(nullptr, usmMemAllocPool.getPooledAllocationBasePtr(bogusPtr));
    EXPECT_EQ(0u, usmMemAllocPool.getOffsetInPool(bogusPtr));
}

class UnifiedMemoryPoolingManagerTest : public SVMMemoryAllocatorFixture<true>, public ::testing::TestWithParam<std::tuple<InternalMemoryType>> {
  public:
    void SetUp() override {
        REQUIRE_64BIT_OR_SKIP();
        SVMMemoryAllocatorFixture::setUp();
        poolMemoryType = std::get<0>(GetParam());
        ASSERT_TRUE(InternalMemoryType::deviceUnifiedMemory == poolMemoryType ||
                    InternalMemoryType::hostUnifiedMemory == poolMemoryType);

        deviceFactory = std::unique_ptr<UltDeviceFactory>(new UltDeviceFactory(1, 1));
        device = deviceFactory->rootDevices[0];

        usmMemAllocPoolsManager.reset(new MockUsmMemAllocPoolsManager(poolMemoryType,
                                                                      rootDeviceIndices,
                                                                      deviceBitfields,
                                                                      isDevicePool() ? device : nullptr));
        ASSERT_NE(nullptr, usmMemAllocPoolsManager);
        EXPECT_FALSE(usmMemAllocPoolsManager->isInitialized());
        svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
        if (isDevicePool()) {
            mockMemoryManager->localMemorySupported[mockRootDeviceIndex] = true;
        }

        poolMemoryProperties = std::make_unique<UnifiedMemoryProperties>(poolMemoryType, MemoryConstants::preferredAlignment, rootDeviceIndices, deviceBitfields);
        poolMemoryProperties->device = isDevicePool() ? device : nullptr;
    }
    void TearDown() override {
        SVMMemoryAllocatorFixture::tearDown();
    }

    void *createAlloc(size_t size, UnifiedMemoryProperties &unifiedMemoryProperties) {
        void *ptr = nullptr;
        auto mockGa = std::make_unique<MockGraphicsAllocation>(mockRootDeviceIndex, nullptr, size);
        mockGa->gpuAddress = nextMockGraphicsAddress;
        mockGa->cpuPtr = reinterpret_cast<void *>(nextMockGraphicsAddress);
        if (isDevicePool()) {
            mockGa->setAllocationType(AllocationType::buffer);
            mockMemoryManager->mockGa = mockGa.release();
            mockMemoryManager->returnMockGAFromDevicePool = true;
            ptr = svmManager->createUnifiedMemoryAllocation(size, unifiedMemoryProperties);
            mockMemoryManager->returnMockGAFromDevicePool = false;
        } else {
            mockGa->setAllocationType(AllocationType::bufferHostMemory);
            mockMemoryManager->mockGa = mockGa.release();
            mockMemoryManager->returnMockGAFromHostPool = true;
            ptr = svmManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
            mockMemoryManager->returnMockGAFromHostPool = false;
        }
        EXPECT_NE(nullptr, ptr);
        nextMockGraphicsAddress = alignUp(nextMockGraphicsAddress + size + 1, MemoryConstants::pageSize2M);
        return ptr;
    }
    bool isDevicePool() {
        return InternalMemoryType::deviceUnifiedMemory == poolMemoryType;
    }
    const size_t poolSize = 2 * MemoryConstants::megaByte;
    std::unique_ptr<MockUsmMemAllocPoolsManager> usmMemAllocPoolsManager;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    Device *device;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    std::unique_ptr<UnifiedMemoryProperties> poolMemoryProperties;
    MockMemoryManager *mockMemoryManager;
    InternalMemoryType poolMemoryType;
    uint64_t nextMockGraphicsAddress = alignUp(std::numeric_limits<uint64_t>::max() - MemoryConstants::teraByte, MemoryConstants::pageSize2M);
};

INSTANTIATE_TEST_SUITE_P(
    UnifiedMemoryPoolingManagerTestParameterized,
    UnifiedMemoryPoolingManagerTest,
    ::testing::Combine(
        ::testing::Values(InternalMemoryType::deviceUnifiedMemory, InternalMemoryType::hostUnifiedMemory)));

TEST_P(UnifiedMemoryPoolingManagerTest, givenUsmMemAllocPoolsManagerWhenCallingCanBePooledThenCorrectValueIsReturned) {
    const RootDeviceIndicesContainer rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> deviceBitfields;
    UnifiedMemoryProperties unifiedMemoryProperties(poolMemoryType, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    size_t maxPoolableSize = 0u;
    if (this->isDevicePool()) {
        maxPoolableSize = PoolInfo::getMaxPoolableSize(device->getGfxCoreHelper());
    } else {
        maxPoolableSize = PoolInfo::getHostMaxPoolableSize();
    }
    EXPECT_TRUE(usmMemAllocPoolsManager->canBePooled(maxPoolableSize, unifiedMemoryProperties));
    EXPECT_FALSE(usmMemAllocPoolsManager->canBePooled(maxPoolableSize + 1, unifiedMemoryProperties));

    unifiedMemoryProperties.alignment = UsmMemAllocPool::poolAlignment * 2;
    EXPECT_FALSE(usmMemAllocPoolsManager->canBePooled(maxPoolableSize, unifiedMemoryProperties));

    unifiedMemoryProperties.alignment = UsmMemAllocPool::chunkAlignment;
    unifiedMemoryProperties.allocationFlags.allFlags = 1u;
    EXPECT_FALSE(usmMemAllocPoolsManager->canBePooled(maxPoolableSize, unifiedMemoryProperties));

    unifiedMemoryProperties.allocationFlags.allFlags = 0u;
    unifiedMemoryProperties.allocationFlags.allAllocFlags = 1u;
    EXPECT_FALSE(usmMemAllocPoolsManager->canBePooled(maxPoolableSize, unifiedMemoryProperties));
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializationFailsForOneOfTheSmallPoolsWhenInitializingPoolsManagerThenPoolsAreCleanedUp) {
    mockMemoryManager->maxSuccessAllocatedGraphicsMemoryIndex = mockMemoryManager->successAllocatedGraphicsMemoryIndex + 2;
    EXPECT_FALSE(usmMemAllocPoolsManager->initialize(svmManager.get()));
    EXPECT_FALSE(usmMemAllocPoolsManager->isInitialized());
    for (auto poolInfo : usmMemAllocPoolsManager->getPoolInfos()) {
        EXPECT_EQ(0u, usmMemAllocPoolsManager->pools[poolInfo].size());
    }
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenTrackResidencySetWhenInitializingThanSetTrackingResidencyForPools) {
    usmMemAllocPoolsManager->enableResidencyTracking();
    EXPECT_TRUE(usmMemAllocPoolsManager->trackResidency);
    ASSERT_TRUE(usmMemAllocPoolsManager->initialize(svmManager.get()));
    ASSERT_TRUE(usmMemAllocPoolsManager->isInitialized());
    for (auto &[_, bucket] : usmMemAllocPoolsManager->pools) {
        for (const auto &pool : bucket) {
            EXPECT_TRUE(reinterpret_cast<MockUsmMemAllocPool *>(pool.get())->trackResidency);
        }
    }

    usmMemAllocPoolsManager->cleanup();
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializedPoolsManagerWhenAllocatingGreaterThan2MBOrWrongAlignmentOrWrongFlagsThenDoNotPool) {
    ASSERT_TRUE(usmMemAllocPoolsManager->initialize(svmManager.get()));
    ASSERT_TRUE(usmMemAllocPoolsManager->isInitialized());
    auto allocOverLimit = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(2 * MemoryConstants::megaByte + 1u, *poolMemoryProperties.get());
    EXPECT_EQ(nullptr, allocOverLimit);

    poolMemoryProperties->allocationFlags.allFlags = 1u;
    auto allocWithExtraFlags = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(1u, *poolMemoryProperties.get());
    EXPECT_EQ(nullptr, allocWithExtraFlags);

    poolMemoryProperties->allocationFlags.allFlags = 0u;
    poolMemoryProperties->alignment = 4 * MemoryConstants::megaByte;
    auto allocWithWrongAlignment = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(1u, *poolMemoryProperties.get());
    EXPECT_EQ(nullptr, allocWithWrongAlignment);

    usmMemAllocPoolsManager->cleanup();
}

TEST_P(UnifiedMemoryPoolingManagerTest, whenGetPoolInfosCalledThenCorrectInfoIsReturned) {
    auto &poolInfos = usmMemAllocPoolsManager->getPoolInfos();
    EXPECT_EQ(3u, poolInfos.size());

    EXPECT_EQ(2 * MemoryConstants::kiloByte, poolInfos[0].poolSize);
    EXPECT_EQ(0u, poolInfos[0].minServicedSize);
    EXPECT_EQ(256u, poolInfos[0].maxServicedSize);

    EXPECT_EQ(8 * MemoryConstants::kiloByte, poolInfos[1].poolSize);
    EXPECT_EQ(256u + 1, poolInfos[1].minServicedSize);
    EXPECT_EQ(1 * MemoryConstants::kiloByte, poolInfos[1].maxServicedSize);

    EXPECT_EQ(2 * MemoryConstants::pageSize, poolInfos[2].poolSize);
    EXPECT_EQ(1 * MemoryConstants::kiloByte + 1, poolInfos[2].minServicedSize);
    EXPECT_EQ(MemoryConstants::pageSize, poolInfos[2].maxServicedSize);
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializedPoolsManagerWhenCallingMethodsWithNotAllocatedPointersThenReturnCorrectValues) {
    ASSERT_TRUE(usmMemAllocPoolsManager->initialize(svmManager.get()));
    ASSERT_TRUE(usmMemAllocPoolsManager->isInitialized());

    void *ptrOutsidePools = addrToPtr(0x1);
    EXPECT_FALSE(usmMemAllocPoolsManager->freeSVMAlloc(ptrOutsidePools, true));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationSize(ptrOutsidePools));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationBasePtr(ptrOutsidePools));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getOffsetInPool(ptrOutsidePools));

    void *notAllocatedPtrInPoolAddressSpace = addrToPtr(usmMemAllocPoolsManager->pools[PoolInfo::getPoolInfos(device->getGfxCoreHelper())[0]][0]->getPoolAddress());
    EXPECT_FALSE(usmMemAllocPoolsManager->freeSVMAlloc(notAllocatedPtrInPoolAddressSpace, true));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationSize(notAllocatedPtrInPoolAddressSpace));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationBasePtr(notAllocatedPtrInPoolAddressSpace));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getOffsetInPool(notAllocatedPtrInPoolAddressSpace));

    usmMemAllocPoolsManager->cleanup();
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializedPoolsManagerWhenAllocatingNotGreaterThan2MBThenPoolsAreUsed) {
    ASSERT_TRUE(usmMemAllocPoolsManager->initialize(svmManager.get()));
    ASSERT_TRUE(usmMemAllocPoolsManager->isInitialized());

    size_t totalSize = 0u;
    for (auto &poolInfo : usmMemAllocPoolsManager->getPoolInfos()) {
        totalSize += poolInfo.poolSize;
    }
    EXPECT_EQ(totalSize, usmMemAllocPoolsManager->totalSize);

    for (auto &poolInfo : usmMemAllocPoolsManager->getPoolInfos()) {
        auto &pool = usmMemAllocPoolsManager->pools[poolInfo][0];

        ASSERT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo].size());
        ASSERT_TRUE(pool->isInitialized());
        EXPECT_EQ(poolInfo.poolSize, pool->getPoolSize());
        EXPECT_TRUE(pool->isEmpty());

        EXPECT_FALSE(pool->sizeIsAllowed(poolInfo.minServicedSize - 1));
        EXPECT_TRUE(pool->sizeIsAllowed(poolInfo.minServicedSize));
        EXPECT_TRUE(pool->sizeIsAllowed(poolInfo.maxServicedSize));
        EXPECT_FALSE(pool->sizeIsAllowed(poolInfo.maxServicedSize + 1));
    }

    for (auto &poolInfo : usmMemAllocPoolsManager->getPoolInfos()) {
        size_t minServicedSize = poolInfo.minServicedSize ? poolInfo.minServicedSize : 1;

        auto poolAllocMinSize = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(minServicedSize, *poolMemoryProperties.get());
        EXPECT_NE(nullptr, poolAllocMinSize);
        EXPECT_EQ(poolAllocMinSize, usmMemAllocPoolsManager->getPooledAllocationBasePtr(poolAllocMinSize));
        EXPECT_EQ(minServicedSize, usmMemAllocPoolsManager->getPooledAllocationSize(poolAllocMinSize));
        EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo][0]->isInPool(poolAllocMinSize));
        EXPECT_EQ(totalSize, usmMemAllocPoolsManager->totalSize);
        EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(poolAllocMinSize, true));

        auto poolAllocMaxSize = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(poolInfo.maxServicedSize, *poolMemoryProperties.get());
        EXPECT_NE(nullptr, poolAllocMaxSize);
        EXPECT_EQ(poolAllocMaxSize, usmMemAllocPoolsManager->getPooledAllocationBasePtr(poolAllocMaxSize));
        EXPECT_EQ(poolInfo.maxServicedSize, usmMemAllocPoolsManager->getPooledAllocationSize(poolAllocMaxSize));
        EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo][0]->isInPool(poolAllocMaxSize));
        EXPECT_EQ(totalSize, usmMemAllocPoolsManager->totalSize);
        EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(poolAllocMaxSize, true));
    }

    usmMemAllocPoolsManager->canAddPools = false;
    std::vector<void *> ptrsToFree;
    auto thirdPoolInfo = usmMemAllocPoolsManager->getPoolInfos()[2];
    auto allocationsToOverfillThirdPool = thirdPoolInfo.poolSize / thirdPoolInfo.maxServicedSize + 1;
    for (auto i = 0u; i < allocationsToOverfillThirdPool; ++i) {
        auto ptr = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(thirdPoolInfo.maxServicedSize, *poolMemoryProperties.get());
        if (nullptr == ptr) {
            break;
        }
        const auto address = castToUint64(ptr);
        const auto offset = usmMemAllocPoolsManager->getOffsetInPool(ptr);
        const auto pool = usmMemAllocPoolsManager->getPoolContainingAlloc(ptr);
        const auto poolAddress = pool->getPoolAddress();
        EXPECT_EQ(ptrOffset(poolAddress, offset), address);

        ptrsToFree.push_back(ptr);
    }
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[thirdPoolInfo].size());
    usmMemAllocPoolsManager->canAddPools = true;

    auto thirdPoolAllocOverCapacity = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(thirdPoolInfo.maxServicedSize, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, thirdPoolAllocOverCapacity);
    EXPECT_EQ(totalSize + thirdPoolInfo.poolSize, usmMemAllocPoolsManager->totalSize);
    ASSERT_EQ(2u, usmMemAllocPoolsManager->pools[thirdPoolInfo].size());
    auto &newPool = usmMemAllocPoolsManager->pools[thirdPoolInfo][1];
    EXPECT_NE(nullptr, newPool->getPooledAllocationBasePtr(thirdPoolAllocOverCapacity));

    ptrsToFree.push_back(thirdPoolAllocOverCapacity);

    for (auto ptr : ptrsToFree) {
        EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(ptr, true));
    }
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[thirdPoolInfo].size());

    usmMemAllocPoolsManager->cleanup();
}
