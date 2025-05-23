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
    UsmMemAllocPool usmMemAllocPool;
    EXPECT_FALSE(usmMemAllocPool.isInitialized());
    EXPECT_EQ(0u, usmMemAllocPool.getPoolAddress());

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    auto mockPool = reinterpret_cast<MockUsmMemAllocPool *>(&usmMemAllocPool);
    EXPECT_TRUE(usmMemAllocPool.initialize(svmManager.get(), unifiedMemoryProperties, 1 * MemoryConstants::megaByte, 0u, 1 * MemoryConstants::megaByte));
    EXPECT_TRUE(usmMemAllocPool.isInitialized());
    EXPECT_EQ(castToUint64(mockPool->pool), usmMemAllocPool.getPoolAddress());

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

class UnifiedMemoryPoolingManagerTest : public SVMMemoryAllocatorFixture<true>, public ::testing::TestWithParam<std::tuple<InternalMemoryType, bool>> {
  public:
    void SetUp() override {
        REQUIRE_64BIT_OR_SKIP();
        SVMMemoryAllocatorFixture::setUp();
        poolMemoryType = std::get<0>(GetParam());
        failAllocation = std::get<1>(GetParam());

        deviceFactory = std::unique_ptr<UltDeviceFactory>(new UltDeviceFactory(1, 1));
        device = deviceFactory->rootDevices[0];

        RootDeviceIndicesContainer rootDeviceIndicesPool;
        rootDeviceIndicesPool.pushUnique(device->getRootDeviceIndex());
        std::map<uint32_t, DeviceBitfield> deviceBitfieldsPool;
        deviceBitfieldsPool.emplace(device->getRootDeviceIndex(), device->getDeviceBitfield());
        usmMemAllocPoolsManager.reset(new MockUsmMemAllocPoolsManager(device->getMemoryManager(),
                                                                      rootDeviceIndicesPool,
                                                                      deviceBitfieldsPool,
                                                                      device,
                                                                      poolMemoryType));
        ASSERT_NE(nullptr, usmMemAllocPoolsManager);
        EXPECT_FALSE(usmMemAllocPoolsManager->isInitialized());
        poolInfo0To4Kb = usmMemAllocPoolsManager->poolInfos[0];
        poolInfo4KbTo64Kb = usmMemAllocPoolsManager->poolInfos[1];
        poolInfo64KbTo2Mb = usmMemAllocPoolsManager->poolInfos[2];
        poolInfo2MbTo16Mb = usmMemAllocPoolsManager->poolInfos[3];
        poolInfo16MbTo64Mb = usmMemAllocPoolsManager->poolInfos[4];
        poolInfo64MbTo256Mb = usmMemAllocPoolsManager->poolInfos[5];
        svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
        mockMemoryManager->failInDevicePoolWithError = failAllocation;
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
    bool failAllocation;
    uint64_t nextMockGraphicsAddress = alignUp(std::numeric_limits<uint64_t>::max() - MemoryConstants::teraByte, MemoryConstants::pageSize2M);
    UsmMemAllocPoolsManager::PoolInfo poolInfo0To4Kb;
    UsmMemAllocPoolsManager::PoolInfo poolInfo4KbTo64Kb;
    UsmMemAllocPoolsManager::PoolInfo poolInfo64KbTo2Mb;
    UsmMemAllocPoolsManager::PoolInfo poolInfo2MbTo16Mb;
    UsmMemAllocPoolsManager::PoolInfo poolInfo16MbTo64Mb;
    UsmMemAllocPoolsManager::PoolInfo poolInfo64MbTo256Mb;
};

INSTANTIATE_TEST_SUITE_P(
    UnifiedMemoryPoolingManagerTestParameterized,
    UnifiedMemoryPoolingManagerTest,
    ::testing::Combine(
        ::testing::Values(InternalMemoryType::deviceUnifiedMemory, InternalMemoryType::hostUnifiedMemory),
        ::testing::Values(false)));

TEST_P(UnifiedMemoryPoolingManagerTest, givenNotInitializedPoolsManagerWhenUsingPoolThenMethodsSucceed) {
    void *ptr = reinterpret_cast<void *>(0x1u);
    const void *constPtr = const_cast<const void *>(ptr);
    EXPECT_FALSE(usmMemAllocPoolsManager->freeSVMAlloc(constPtr, true));
    EXPECT_EQ(nullptr, usmMemAllocPoolsManager->getPooledAllocationBasePtr(constPtr));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationSize(constPtr));
    EXPECT_FALSE(usmMemAllocPoolsManager->recycleSVMAlloc(ptr, true));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getOffsetInPool(constPtr));
    EXPECT_EQ(nullptr, usmMemAllocPoolsManager->getPoolContainingAlloc(constPtr));
    usmMemAllocPoolsManager->trim();

    auto mockDevice = reinterpret_cast<MockDevice *>(usmMemAllocPoolsManager->device);
    usmMemAllocPoolsManager->callBaseGetFreeMemory = true;
    mockDevice->deviceInfo.localMemSize = 4 * MemoryConstants::gigaByte;
    mockDevice->deviceInfo.globalMemSize = 8 * MemoryConstants::gigaByte;
    mockMemoryManager->localMemAllocsSize[mockDevice->getRootDeviceIndex()].store(1 * MemoryConstants::gigaByte);
    auto mutableHwInfo = mockDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    EXPECT_EQ(mutableHwInfo, &mockDevice->getHardwareInfo());
    mutableHwInfo->capabilityTable.isIntegratedDevice = false;
    EXPECT_EQ(3 * MemoryConstants::gigaByte, usmMemAllocPoolsManager->getFreeMemory());

    mutableHwInfo->capabilityTable.isIntegratedDevice = true;
    EXPECT_EQ(7 * MemoryConstants::gigaByte, usmMemAllocPoolsManager->getFreeMemory());

    usmMemAllocPoolsManager->cleanup();
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializationFailsForOneOfTheSmallPoolsWhenInitializingPoolsManagerThenPoolsAreCleanedUp) {
    mockMemoryManager->maxSuccessAllocatedGraphicsMemoryIndex = mockMemoryManager->successAllocatedGraphicsMemoryIndex + 2;
    EXPECT_FALSE(usmMemAllocPoolsManager->ensureInitialized(svmManager.get()));
    EXPECT_FALSE(usmMemAllocPoolsManager->isInitialized());
    ASSERT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo0To4Kb].size());
    EXPECT_FALSE(usmMemAllocPoolsManager->pools[poolInfo0To4Kb][0]->isInitialized());
    ASSERT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb].size());
    EXPECT_FALSE(usmMemAllocPoolsManager->pools[poolInfo4KbTo64Kb][0]->isInitialized());
    ASSERT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb].size());
    EXPECT_FALSE(usmMemAllocPoolsManager->pools[poolInfo64KbTo2Mb][0]->isInitialized());
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializedPoolsManagerWhenAllocatingNotGreaterThan2MBThenSmallPoolsAreUsed) {
    EXPECT_TRUE(usmMemAllocPoolsManager->ensureInitialized(svmManager.get()));
    EXPECT_TRUE(usmMemAllocPoolsManager->isInitialized());
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

    for (auto i = 0u; i < 7; ++i) { // use all memory in third pool
        auto ptr = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(2 * MemoryConstants::megaByte, *poolMemoryProperties.get());
        if (nullptr == ptr) {
            break;
        }
    }

    auto thirdPoolAlloc2MBOverCapacity = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(2 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_EQ(nullptr, thirdPoolAlloc2MBOverCapacity);

    usmMemAllocPoolsManager->cleanup();
}

TEST_P(UnifiedMemoryPoolingManagerTest, givenInitializedPoolsManagerWhenAllocatingGreaterThan2MBAndNotGreaterThan256BMThenBigPoolsAreUsed) {
    void *ptr = reinterpret_cast<void *>(0x1u);
    const void *constPtr = const_cast<const void *>(ptr);
    EXPECT_TRUE(usmMemAllocPoolsManager->ensureInitialized(svmManager.get()));
    EXPECT_TRUE(usmMemAllocPoolsManager->isInitialized());
    EXPECT_TRUE(usmMemAllocPoolsManager->ensureInitialized(svmManager.get()));

    EXPECT_EQ(0u, usmMemAllocPoolsManager->pools[poolInfo2MbTo16Mb].size());
    EXPECT_EQ(0u, usmMemAllocPoolsManager->pools[poolInfo16MbTo64Mb].size());
    EXPECT_EQ(0u, usmMemAllocPoolsManager->pools[poolInfo64MbTo256Mb].size());
    poolMemoryProperties->alignment = UsmMemAllocPool::chunkAlignment;
    const auto allocSize = 14 * MemoryConstants::megaByte;
    auto normalAlloc = createAlloc(allocSize, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, normalAlloc);

    size_t freeMemoryThreshold = static_cast<size_t>((allocSize + usmMemAllocPoolsManager->totalSize) / UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(poolMemoryType));
    const auto toleranceForFloatingPointArithmetic = static_cast<size_t>(1 / UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(poolMemoryType));
    usmMemAllocPoolsManager->mockFreeMemory = freeMemoryThreshold - toleranceForFloatingPointArithmetic;
    EXPECT_FALSE(usmMemAllocPoolsManager->recycleSVMAlloc(normalAlloc, true));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->pools[poolInfo2MbTo16Mb].size());

    const auto totalSizeStart = usmMemAllocPoolsManager->totalSize;
    usmMemAllocPoolsManager->mockFreeMemory = freeMemoryThreshold + toleranceForFloatingPointArithmetic;
    EXPECT_TRUE(usmMemAllocPoolsManager->recycleSVMAlloc(normalAlloc, true));
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo2MbTo16Mb].size());
    EXPECT_EQ(totalSizeStart + allocSize, usmMemAllocPoolsManager->totalSize);
    auto firstPool = reinterpret_cast<MockUsmMemAllocPool *>(usmMemAllocPoolsManager->pools[poolInfo2MbTo16Mb][0].get());
    EXPECT_EQ(2 * MemoryConstants::megaByte + 1, firstPool->minServicedSize);
    EXPECT_EQ(allocSize, firstPool->maxServicedSize);

    auto poolAlloc8MB = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(8 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, poolAlloc8MB);
    EXPECT_TRUE(firstPool->isInPool(poolAlloc8MB));
    EXPECT_EQ(8 * MemoryConstants::megaByte, usmMemAllocPoolsManager->getPooledAllocationSize(poolAlloc8MB));
    EXPECT_EQ(poolAlloc8MB, usmMemAllocPoolsManager->getPooledAllocationBasePtr(poolAlloc8MB));
    EXPECT_EQ(firstPool->getOffsetInPool(poolAlloc8MB), usmMemAllocPoolsManager->getOffsetInPool(poolAlloc8MB));

    auto allocationNotFitInPool = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(8 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_EQ(nullptr, allocationNotFitInPool);

    auto alloc64MB = createAlloc(64 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    freeMemoryThreshold = static_cast<size_t>((64 * MemoryConstants::megaByte + usmMemAllocPoolsManager->totalSize) / UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(poolMemoryType));
    usmMemAllocPoolsManager->mockFreeMemory = freeMemoryThreshold + toleranceForFloatingPointArithmetic;
    EXPECT_TRUE(usmMemAllocPoolsManager->recycleSVMAlloc(alloc64MB, true));
    auto poolAlloc64MB = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(64 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, poolAlloc64MB);
    auto secondPool = reinterpret_cast<MockUsmMemAllocPool *>(usmMemAllocPoolsManager->pools[poolInfo16MbTo64Mb][0].get());
    EXPECT_TRUE(secondPool->isInPool(poolAlloc64MB));
    EXPECT_EQ(64 * MemoryConstants::megaByte, usmMemAllocPoolsManager->getPooledAllocationSize(poolAlloc64MB));
    EXPECT_EQ(poolAlloc64MB, usmMemAllocPoolsManager->getPooledAllocationBasePtr(poolAlloc64MB));
    allocationNotFitInPool = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(64 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_EQ(nullptr, allocationNotFitInPool);
    EXPECT_EQ(nullptr, usmMemAllocPoolsManager->getPoolContainingAlloc(constPtr));

    auto alloc256MB = createAlloc(256 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    freeMemoryThreshold = static_cast<size_t>((256 * MemoryConstants::megaByte + usmMemAllocPoolsManager->totalSize) / UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(poolMemoryType));
    usmMemAllocPoolsManager->mockFreeMemory = freeMemoryThreshold + toleranceForFloatingPointArithmetic;
    EXPECT_TRUE(usmMemAllocPoolsManager->recycleSVMAlloc(alloc256MB, true));
    auto poolAlloc256MB = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(256 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, poolAlloc256MB);
    auto thirdPool = reinterpret_cast<MockUsmMemAllocPool *>(usmMemAllocPoolsManager->pools[poolInfo64MbTo256Mb][0].get());
    EXPECT_TRUE(thirdPool->isInPool(poolAlloc256MB));
    EXPECT_EQ(256 * MemoryConstants::megaByte, usmMemAllocPoolsManager->getPooledAllocationSize(poolAlloc256MB));
    EXPECT_EQ(poolAlloc256MB, usmMemAllocPoolsManager->getPooledAllocationBasePtr(poolAlloc256MB));
    allocationNotFitInPool = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(256 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    EXPECT_EQ(nullptr, allocationNotFitInPool);

    auto alloc256MBForTrim = createAlloc(256 * MemoryConstants::megaByte, *poolMemoryProperties.get());
    freeMemoryThreshold = static_cast<size_t>((256 * MemoryConstants::megaByte + usmMemAllocPoolsManager->totalSize) / UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(poolMemoryType));
    usmMemAllocPoolsManager->mockFreeMemory = freeMemoryThreshold + toleranceForFloatingPointArithmetic;
    EXPECT_TRUE(usmMemAllocPoolsManager->recycleSVMAlloc(alloc256MBForTrim, true));
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo2MbTo16Mb].size());
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo16MbTo64Mb].size());
    EXPECT_EQ(2u, usmMemAllocPoolsManager->pools[poolInfo64MbTo256Mb].size());
    usmMemAllocPoolsManager->trim();
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo2MbTo16Mb].size());
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo16MbTo64Mb].size());
    EXPECT_EQ(1u, usmMemAllocPoolsManager->pools[poolInfo64MbTo256Mb].size());

    auto smallAlloc = createAlloc(2 * MemoryConstants::megaByte - 1, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, smallAlloc);
    freeMemoryThreshold = static_cast<size_t>((2 * MemoryConstants::megaByte + usmMemAllocPoolsManager->totalSize) / UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(poolMemoryType));
    usmMemAllocPoolsManager->mockFreeMemory = freeMemoryThreshold + toleranceForFloatingPointArithmetic;
    EXPECT_FALSE(usmMemAllocPoolsManager->recycleSVMAlloc(smallAlloc, true));

    auto bigAlloc = createAlloc(256 * MemoryConstants::megaByte + 1, *poolMemoryProperties.get());
    EXPECT_NE(nullptr, bigAlloc);
    freeMemoryThreshold = static_cast<size_t>((256 * MemoryConstants::megaByte + 1 + usmMemAllocPoolsManager->totalSize) / UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(poolMemoryType));
    usmMemAllocPoolsManager->mockFreeMemory = freeMemoryThreshold + toleranceForFloatingPointArithmetic;
    EXPECT_FALSE(usmMemAllocPoolsManager->recycleSVMAlloc(bigAlloc, true));

    svmManager->freeSVMAlloc(smallAlloc);
    svmManager->freeSVMAlloc(bigAlloc);

    auto allocationOverMaxSize = usmMemAllocPoolsManager->createUnifiedMemoryAllocation(256 * MemoryConstants::megaByte + 1, *poolMemoryProperties.get());
    EXPECT_EQ(nullptr, allocationOverMaxSize);

    EXPECT_EQ(nullptr, usmMemAllocPoolsManager->getPoolContainingAlloc(constPtr));
    usmMemAllocPoolsManager->cleanup();
}