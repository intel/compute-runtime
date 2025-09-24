/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
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

    EXPECT_TRUE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::chunkAlignment));
    EXPECT_TRUE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::chunkAlignment * 2));
    EXPECT_FALSE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::chunkAlignment / 2));
    EXPECT_FALSE(UsmMemAllocPool::alignmentIsAllowed(UsmMemAllocPool::poolAlignment + UsmMemAllocPool::chunkAlignment));

    const RootDeviceIndicesContainer rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> deviceBitfields;
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(UsmMemAllocPool::flagsAreAllowed(unifiedMemoryProperties));
    unifiedMemoryProperties.allocationFlags.allFlags = 1u;
    unifiedMemoryProperties.allocationFlags.allAllocFlags = 0u;
    EXPECT_FALSE(UsmMemAllocPool::flagsAreAllowed(unifiedMemoryProperties));
    unifiedMemoryProperties.allocationFlags.allFlags = 0u;
    unifiedMemoryProperties.allocationFlags.allAllocFlags = 1u;
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

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(usmMemAllocPool.initialize(svmManager.get(), unifiedMemoryProperties, 1 * MemoryConstants::megaByte, 0u, 1 * MemoryConstants::megaByte));
    EXPECT_TRUE(usmMemAllocPool.isInitialized());
    EXPECT_EQ(castToUint64(usmMemAllocPool.pool), usmMemAllocPool.getPoolAddress());

    usmMemAllocPool.cleanup();
    EXPECT_FALSE(usmMemAllocPool.isInitialized());
    EXPECT_EQ(0u, usmMemAllocPool.getPoolAddress());
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
        svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        static_cast<MockMemoryManager *>(device->getMemoryManager())->failInDevicePoolWithError = failAllocation;

        poolMemoryProperties = std::make_unique<SVMAllocsManager::UnifiedMemoryProperties>(poolMemoryType, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
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
    std::unique_ptr<SVMAllocsManager::UnifiedMemoryProperties> poolMemoryProperties;
    constexpr static auto poolAllocationThreshold = 1 * MemoryConstants::megaByte;
};

using InitializedHostUnifiedMemoryPoolingTest = InitializedUnifiedMemoryPoolingTest<InternalMemoryType::hostUnifiedMemory, false>;
TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenDifferentAllocationSizesWhenCallingCanBePooledThenCorrectValueIsReturned) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
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
    constexpr auto notAllowedAlignment = UsmMemAllocPool::chunkAlignment / 2;
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
    EXPECT_FALSE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment / 2));
    EXPECT_TRUE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment));
    EXPECT_FALSE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment + UsmMemAllocPool::chunkAlignment / 2));
    EXPECT_TRUE(usmMemAllocPool.alignmentIsAllowed(UsmMemAllocPool::chunkAlignment * 2));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenPoolableAllocationWhenUsingPoolThenAllocationIsPooledUnlessPoolIsFull) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
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
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, 0u, rootDeviceIndices, deviceBitfields);
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

    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
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
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = poolAllocationThreshold;
    EXPECT_EQ(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties));
    const auto bogusPtr = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(bogusPtr, true));
    EXPECT_EQ(0u, usmMemAllocPool.getPooledAllocationSize(bogusPtr));
    EXPECT_EQ(nullptr, usmMemAllocPool.getPooledAllocationBasePtr(bogusPtr));
    EXPECT_EQ(0u, usmMemAllocPool.getOffsetInPool(bogusPtr));
}

using UnifiedMemoryPoolingManagerStaticTest = ::testing::Test;
TEST_F(UnifiedMemoryPoolingManagerStaticTest, givenUsmMemAllocPoolsManagerWhenCallingCanBePooledThenCorrectValueIsReturned) {
    const RootDeviceIndicesContainer rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> deviceBitfields;
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(MockUsmMemAllocPoolsManager::canBePooled(UsmMemAllocPoolsManager::maxPoolableSize, unifiedMemoryProperties));
    EXPECT_FALSE(MockUsmMemAllocPoolsManager::canBePooled(UsmMemAllocPoolsManager::maxPoolableSize + 1, unifiedMemoryProperties));

    unifiedMemoryProperties.alignment = UsmMemAllocPool::chunkAlignment / 2;
    EXPECT_FALSE(MockUsmMemAllocPoolsManager::canBePooled(UsmMemAllocPoolsManager::maxPoolableSize, unifiedMemoryProperties));

    unifiedMemoryProperties.alignment = UsmMemAllocPool::chunkAlignment;
    unifiedMemoryProperties.allocationFlags.allFlags = 1u;
    EXPECT_FALSE(MockUsmMemAllocPoolsManager::canBePooled(UsmMemAllocPoolsManager::maxPoolableSize, unifiedMemoryProperties));

    unifiedMemoryProperties.allocationFlags.allFlags = 0u;
    unifiedMemoryProperties.allocationFlags.allAllocFlags = 1u;
    EXPECT_FALSE(MockUsmMemAllocPoolsManager::canBePooled(UsmMemAllocPoolsManager::maxPoolableSize, unifiedMemoryProperties));
}

class UnifiedMemoryPoolingManagerTest : public SVMMemoryAllocatorFixture<true>, public ::testing::TestWithParam<std::tuple<InternalMemoryType>> {
  public:
    void SetUp() override {
        REQUIRE_64BIT_OR_SKIP();
        SVMMemoryAllocatorFixture::setUp();
        poolMemoryType = std::get<0>(GetParam());

        deviceFactory = std::unique_ptr<UltDeviceFactory>(new UltDeviceFactory(1, 1));
        device = deviceFactory->rootDevices[0];

        usmMemAllocPoolsManager.reset(new MockUsmMemAllocPoolsManager(poolMemoryType,
                                                                      rootDeviceIndices,
                                                                      deviceBitfields,
                                                                      poolMemoryType == InternalMemoryType::deviceUnifiedMemory ? device : nullptr));
        ASSERT_NE(nullptr, usmMemAllocPoolsManager);
        EXPECT_FALSE(usmMemAllocPoolsManager->isInitialized());
        poolInfo0To4Kb = usmMemAllocPoolsManager->poolInfos[0];
        poolInfo4KbTo64Kb = usmMemAllocPoolsManager->poolInfos[1];
        poolInfo64KbTo2Mb = usmMemAllocPoolsManager->poolInfos[2];
        svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
        if (InternalMemoryType::deviceUnifiedMemory == poolMemoryType) {
            mockMemoryManager->localMemorySupported[mockRootDeviceIndex] = true;
        }

        poolMemoryProperties = std::make_unique<SVMAllocsManager::UnifiedMemoryProperties>(poolMemoryType, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
        poolMemoryProperties->device = poolMemoryType == InternalMemoryType::deviceUnifiedMemory ? device : nullptr;
    }
    void TearDown() override {
        SVMMemoryAllocatorFixture::tearDown();
    }

    void *createAlloc(size_t size, SVMAllocsManager::UnifiedMemoryProperties &unifiedMemoryProperties) {
        void *ptr = nullptr;
        auto mockGa = std::make_unique<MockGraphicsAllocation>(mockRootDeviceIndex, nullptr, size);
        mockGa->gpuAddress = nextMockGraphicsAddress;
        mockGa->cpuPtr = reinterpret_cast<void *>(nextMockGraphicsAddress);
        if (InternalMemoryType::deviceUnifiedMemory == poolMemoryType) {
            mockGa->setAllocationType(AllocationType::svmGpu);
            mockMemoryManager->mockGa = mockGa.release();
            mockMemoryManager->returnMockGAFromDevicePool = true;
            ptr = svmManager->createUnifiedMemoryAllocation(size, unifiedMemoryProperties);
            mockMemoryManager->returnMockGAFromDevicePool = false;
        }
        if (InternalMemoryType::hostUnifiedMemory == poolMemoryType) {
            mockGa->setAllocationType(AllocationType::svmCpu);
            mockMemoryManager->mockGa = mockGa.release();
            mockMemoryManager->returnMockGAFromHostPool = true;
            ptr = svmManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
            mockMemoryManager->returnMockGAFromHostPool = false;
        }
        EXPECT_NE(nullptr, ptr);
        nextMockGraphicsAddress = alignUp(nextMockGraphicsAddress + size + 1, MemoryConstants::pageSize2M);
        return ptr;
    }
    const size_t poolSize = 2 * MemoryConstants::megaByte;
    std::unique_ptr<MockUsmMemAllocPoolsManager> usmMemAllocPoolsManager;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    Device *device;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    std::unique_ptr<SVMAllocsManager::UnifiedMemoryProperties> poolMemoryProperties;
    MockMemoryManager *mockMemoryManager;
    InternalMemoryType poolMemoryType;
    uint64_t nextMockGraphicsAddress = alignUp(std::numeric_limits<uint64_t>::max() - MemoryConstants::teraByte, MemoryConstants::pageSize2M);

    UsmMemAllocPoolsManager::PoolInfo poolInfo0To4Kb;
    UsmMemAllocPoolsManager::PoolInfo poolInfo4KbTo64Kb;
    UsmMemAllocPoolsManager::PoolInfo poolInfo64KbTo2Mb;
};

INSTANTIATE_TEST_SUITE_P(
    UnifiedMemoryPoolingManagerTestParameterized,
    UnifiedMemoryPoolingManagerTest,
    ::testing::Combine(
        ::testing::Values(InternalMemoryType::deviceUnifiedMemory, InternalMemoryType::hostUnifiedMemory)));

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializationFailsForOneOfTheSmallPoolsWhenInitializingPoolsManagerThenPoolsAreCleanedUp) {
    mockMemoryManager->maxSuccessAllocatedGraphicsMemoryIndex = mockMemoryManager->successAllocatedGraphicsMemoryIndex + 2;
    EXPECT_FALSE(usmMemAllocPoolsManager->initialize(svmManager.get()));
    EXPECT_FALSE(usmMemAllocPoolsManager->isInitialized());
    EXPECT_EQ(0u, usmMemAllocPoolsManager->pools[poolInfo0To4Kb].size());
    EXPECT_EQ(0u, usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb].size());
    EXPECT_EQ(0u, usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb].size());
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

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializedPoolsManagerWhenCallingMethodsWithNotAllocatedPointersThenReturnCorrectValues) {
    ASSERT_TRUE(usmMemAllocPoolsManager->initialize(svmManager.get()));
    ASSERT_TRUE(usmMemAllocPoolsManager->isInitialized());

    void *ptrOutsidePools = addrToPtr(0x1);
    EXPECT_FALSE(usmMemAllocPoolsManager->freeSVMAlloc(ptrOutsidePools, true));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationSize(ptrOutsidePools));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationBasePtr(ptrOutsidePools));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getOffsetInPool(ptrOutsidePools));

    void *notAllocatedPtrInPoolAddressSpace = addrToPtr(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->getPoolAddress());
    EXPECT_FALSE(usmMemAllocPoolsManager->freeSVMAlloc(notAllocatedPtrInPoolAddressSpace, true));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationSize(notAllocatedPtrInPoolAddressSpace));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationBasePtr(notAllocatedPtrInPoolAddressSpace));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getOffsetInPool(notAllocatedPtrInPoolAddressSpace));

    usmMemAllocPoolsManager->cleanup();
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializedPoolsManagerWhenAllocatingNotGreaterThan2MBThenPoolsAreUsed) {
    ASSERT_TRUE(usmMemAllocPoolsManager->initialize(svmManager.get()));
    ASSERT_TRUE(usmMemAllocPoolsManager->isInitialized());
    ASSERT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo0To4Kb].size());
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->isInitialized());
    ASSERT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb].size());
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->isInitialized());
    ASSERT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb].size());
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->isInitialized());
    EXPECT_EQ(20 * MemoryConstants::megaByte, usmMemAllocPoolsManager->totalSize);

    EXPECT_EQ(2 * MemoryConstants::megaByte, usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->getPoolSize());
    EXPECT_EQ(2 * MemoryConstants::megaByte, usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->getPoolSize());
    EXPECT_EQ(16 * MemoryConstants::megaByte, usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->getPoolSize());

    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->isEmpty());
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->isEmpty());
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->isEmpty());

    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->sizeIsAllowed(1));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->sizeIsAllowed(4 * MemoryConstants::kiloByte));
    EXPECT_FALSE(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->sizeIsAllowed(4 * MemoryConstants::kiloByte + 1));

    EXPECT_FALSE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->sizeIsAllowed(4 * MemoryConstants::kiloByte));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->sizeIsAllowed(4 * MemoryConstants::kiloByte + 1));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->sizeIsAllowed(64 * MemoryConstants::kiloByte));
    EXPECT_FALSE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->sizeIsAllowed(64 * MemoryConstants::kiloByte + 1));

    EXPECT_FALSE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->sizeIsAllowed(64 * MemoryConstants::kiloByte));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->sizeIsAllowed(64 * MemoryConstants::kiloByte + 1));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->sizeIsAllowed(2 * MemoryConstants::megaByte));
    EXPECT_FALSE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->sizeIsAllowed(2 * MemoryConstants::megaByte + 1));

    auto firstPoolAlloc1B = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(1u, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, firstPoolAlloc1B);
    EXPECT_EQ(firstPoolAlloc1B, usmMemAllocPoolsManager->getPooledAllocationBasePtr(firstPoolAlloc1B));
    EXPECT_EQ(1u, usmMemAllocPoolsManager->getPooledAllocationSize(firstPoolAlloc1B));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->isInPool(firstPoolAlloc1B));
    EXPECT_EQ(20 * MemoryConstants::megaByte, usmMemAllocPoolsManager->totalSize);
    EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(firstPoolAlloc1B, true));

    auto firstPoolAlloc4KB = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(4 * MemoryConstants::kiloByte, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, firstPoolAlloc4KB);
    EXPECT_EQ(firstPoolAlloc4KB, usmMemAllocPoolsManager->getPooledAllocationBasePtr(firstPoolAlloc4KB));
    EXPECT_EQ(4 * MemoryConstants::kiloByte, usmMemAllocPoolsManager->getPooledAllocationSize(firstPoolAlloc4KB));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->isInPool(firstPoolAlloc4KB));
    EXPECT_EQ(20 * MemoryConstants::megaByte, usmMemAllocPoolsManager->totalSize);
    EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(firstPoolAlloc4KB, true));

    auto secondPoolAlloc4KB1B = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(4 * MemoryConstants::kiloByte + 1, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, secondPoolAlloc4KB1B);
    EXPECT_EQ(secondPoolAlloc4KB1B, usmMemAllocPoolsManager->getPooledAllocationBasePtr(secondPoolAlloc4KB1B));
    EXPECT_EQ(4 * MemoryConstants::kiloByte + 1, usmMemAllocPoolsManager->getPooledAllocationSize(secondPoolAlloc4KB1B));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->isInPool(secondPoolAlloc4KB1B));
    EXPECT_EQ(20 * MemoryConstants::megaByte, usmMemAllocPoolsManager->totalSize);
    EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(secondPoolAlloc4KB1B, true));

    auto secondPoolAlloc64KB = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(64 * MemoryConstants::kiloByte, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, secondPoolAlloc64KB);
    EXPECT_EQ(secondPoolAlloc64KB, usmMemAllocPoolsManager->getPooledAllocationBasePtr(secondPoolAlloc64KB));
    EXPECT_EQ(64 * MemoryConstants::kiloByte, usmMemAllocPoolsManager->getPooledAllocationSize(secondPoolAlloc64KB));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->isInPool(secondPoolAlloc64KB));
    EXPECT_EQ(20 * MemoryConstants::megaByte, usmMemAllocPoolsManager->totalSize);
    EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(secondPoolAlloc64KB, true));

    auto thirdPoolAlloc64KB1B = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(64 * MemoryConstants::kiloByte + 1, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, thirdPoolAlloc64KB1B);
    EXPECT_EQ(thirdPoolAlloc64KB1B, usmMemAllocPoolsManager->getPooledAllocationBasePtr(thirdPoolAlloc64KB1B));
    EXPECT_EQ(64 * MemoryConstants::kiloByte + 1, usmMemAllocPoolsManager->getPooledAllocationSize(thirdPoolAlloc64KB1B));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->isInPool(thirdPoolAlloc64KB1B));
    EXPECT_EQ(20 * MemoryConstants::megaByte, usmMemAllocPoolsManager->totalSize);
    EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(thirdPoolAlloc64KB1B, true));

    auto thirdPoolAlloc2MB = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(2 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, thirdPoolAlloc2MB);
    EXPECT_EQ(thirdPoolAlloc2MB, usmMemAllocPoolsManager->getPooledAllocationBasePtr(thirdPoolAlloc2MB));
    EXPECT_EQ(2 * MemoryConstants::megaByte, usmMemAllocPoolsManager->getPooledAllocationSize(thirdPoolAlloc2MB));
    EXPECT_TRUE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->isInPool(thirdPoolAlloc2MB));
    EXPECT_EQ(20 * MemoryConstants::megaByte, usmMemAllocPoolsManager->totalSize);

    usmMemAllocPoolsManager->canAddPools = false;
    std::vector<void *> ptrsToFree;
    for (auto i = 0u; i < 9; ++i) { // use all memory in third pool
        auto ptr = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(2 * MemoryConstants::megaByte, *poolMemoryProperties.get());
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
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb].size());
    usmMemAllocPoolsManager->canAddPools = true;

    auto thirdPoolAlloc2MBOverCapacity = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(2 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, thirdPoolAlloc2MBOverCapacity);
    EXPECT_EQ(36 * MemoryConstants::megaByte, usmMemAllocPoolsManager->totalSize);
    ASSERT_EQ(2u, usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb].size());
    auto &newPool = usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][1];
    EXPECT_NE(nullptr, newPool->getPooledAllocationBasePtr(thirdPoolAlloc2MBOverCapacity));

    ptrsToFree.push_back(thirdPoolAlloc2MB);
    ptrsToFree.push_back(thirdPoolAlloc2MBOverCapacity);

    for (auto ptr : ptrsToFree) {
        EXPECT_TRUE(usmMemAllocPoolsManager->freeSVMAlloc(ptr, true));
    }
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb].size());

    usmMemAllocPoolsManager->cleanup();
}
